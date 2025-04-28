/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AuctionHouseWorkerThread.h"
#include "AuctionHouseMgr.h"
#include "AuctionSorter.h"
#include "GameTime.h"
#include "World.h"

#define MAX_AUCTIONS_PER_PAGE 50

AuctionHouseWorkerThread::AuctionHouseWorkerThread(
    SignalQueue<std::unique_ptr<AuctionMessage>>* messageQueue,
    SignalQueue<std::unique_ptr<ListAuctionMessageResponse>>* responseQueue,
    SearchableAuctionEntriesMap* searchableAuctionMap,
    std::shared_mutex* mapMutex)
    : messageQueue_(messageQueue), responseQueue_(responseQueue),
      searchableAuctionMap_(searchableAuctionMap), mapMutex_(mapMutex)
{
    TC_LOG_DEBUG("auctionHouse", "Creating AH WORKER {}", GameTime::GetGameTimeMS());
    workerThread_ = std::jthread([this](std::stop_token stop) { Run(stop); });
    TC_LOG_DEBUG("auctionHouse", "Finished Creating AH WORKER {}", GameTime::GetGameTimeMS());
}

AuctionHouseWorkerThread::~AuctionHouseWorkerThread()
{
    if (workerThread_.joinable())
    {
        workerThread_.join();
    }
}

void AuctionHouseWorkerThread::Run(std::stop_token stop)
{
    while (!stop.stop_requested())
    {
        if (auto message = messageQueue_->receive(stop))
        {
            TC_LOG_DEBUG("auctionHouse", "Receive Auction Message From Queue {}", GameTime::GetGameTimeMS());
            try
            {
                ProcessMessage(std::move(*message));
            }
            catch (const std::exception& e)
            {
                TC_LOG_ERROR("auctionHouse", "Exception in ProcessMessage: {}", e.what());
            }
        }
    }
}

void AuctionHouseWorkerThread::ProcessMessage(std::unique_ptr<AuctionMessage> message)
{
    TC_LOG_DEBUG("auctionHouse", "ProcessMessage {}", GameTime::GetGameTimeMS());
    switch (message->type)
    {
        case AuctionMessage::Type::Add:
            AddAuction(*static_cast<AddAuctionMessage*>(message.get()));
            break;
        case AuctionMessage::Type::Remove:
            RemoveAuction(*static_cast<RemoveAuctionMessage*>(message.get()));
            break;
        case AuctionMessage::Type::UpdateBid:
            UpdateAuctionBid(*static_cast<UpdateAuctionBidMessage*>(message.get()));
            break;
        case AuctionMessage::Type::List:
            ListAuctions(*static_cast<ListAuctionMessage*>(message.get()));
            break;
        case AuctionMessage::Type::ListOwner:
            ListOwnerAuctions(*static_cast<ListOwnerAuctionMessage*>(message.get()));
            break;
        case AuctionMessage::Type::ListBidder:
            ListBidderAuctions(*static_cast<ListBidderAuctionMessage*>(message.get()));
            break;
        default:
            break;
    }
}

void AuctionHouseWorkerThread::AddAuction(AddAuctionMessage const& auctionAdd)
{
    TC_LOG_DEBUG("auctionHouse", "AddAuction {}", GameTime::GetGameTimeMS());
    SearchableAuctionEntriesMap& searchableAuctionMap = GetSearchableAuctionMap(auctionAdd.listFaction);
    std::unique_lock<std::shared_mutex> lock(GetMapMutex(auctionAdd.listFaction));
    searchableAuctionMap.insert(std::make_pair(auctionAdd.searchableAuctionEntry->Id, auctionAdd.searchableAuctionEntry));
}

void AuctionHouseWorkerThread::RemoveAuction(RemoveAuctionMessage const& auctionRemove)
{
    TC_LOG_DEBUG("auctionHouse", "RemoveAuction {}", GameTime::GetGameTimeMS());
    SearchableAuctionEntriesMap& searchableAuctionMap = GetSearchableAuctionMap(auctionRemove.listFaction);
    std::unique_lock<std::shared_mutex> lock(GetMapMutex(auctionRemove.listFaction));
    searchableAuctionMap.erase(auctionRemove.auctionId);
}

void AuctionHouseWorkerThread::UpdateAuctionBid(UpdateAuctionBidMessage const& auctionUpdateBid)
{
    TC_LOG_DEBUG("auctionHouse", "UpdateAuctionBid {}", GameTime::GetGameTimeMS());
    SearchableAuctionEntriesMap& searchableAuctionMap = GetSearchableAuctionMap(auctionUpdateBid.listFaction);
    std::unique_lock<std::shared_mutex> lock(GetMapMutex(auctionUpdateBid.listFaction));
    SearchableAuctionEntriesMap::const_iterator itr = searchableAuctionMap.find(auctionUpdateBid.auctionId);
    if (itr != searchableAuctionMap.end())
    {
        itr->second->bid = auctionUpdateBid.bid;
        itr->second->bidderGuid = auctionUpdateBid.bidderGuid;
    }
}

void AuctionHouseWorkerThread::ListAuctions(ListAuctionMessage const& searchListRequest)
{
    TC_LOG_DEBUG("auctionHouse", "ListAuctions Called {}", GameTime::GetGameTimeMS());
    std::shared_lock<std::shared_mutex> lock(GetMapMutex(searchListRequest.listFaction));
    SearchableAuctionEntriesMap& searchableAuctionMap = GetSearchableAuctionMap(searchListRequest.listFaction);
    uint32 count = 0, totalCount = 0;

    auto searchResponse = std::make_unique<ListAuctionMessageResponse>();
    searchResponse->playerGuid = searchListRequest.playerInfo.playerGuid;
    searchResponse->packet.Initialize(SMSG_AUCTION_LIST_RESULT, (4 + 4 + 4));
    searchResponse->packet << (uint32)0;

    if (!searchListRequest.searchInfo.getAll)
    {
        SortableAuctionEntriesList auctionEntries;
        BuildListAuctionItems(searchListRequest, auctionEntries, searchableAuctionMap);

        if (!searchListRequest.searchInfo.sorting.empty() && auctionEntries.size() > MAX_AUCTIONS_PER_PAGE)
        {
            AuctionSorter sorter(&searchListRequest.searchInfo.sorting, searchListRequest.playerInfo.loc_idx);
            std::sort(auctionEntries.begin(), auctionEntries.end(), sorter);
        }

        SortableAuctionEntriesList::const_iterator itr = auctionEntries.begin();
        if (searchListRequest.searchInfo.listfrom)
        {
            if (searchListRequest.searchInfo.listfrom > auctionEntries.size())
                itr = auctionEntries.end();
            else
                itr += searchListRequest.searchInfo.listfrom;
        }

        for (; itr != auctionEntries.end(); ++itr)
        {
            (*itr)->BuildAuctionInfo(searchResponse->packet);
            if (++count >= MAX_AUCTIONS_PER_PAGE)
                break;
        }
        totalCount = auctionEntries.size();
    }
    else
    {
        for (auto const& pair : searchableAuctionMap)
        {
            std::shared_ptr<SearchableAuctionEntry> const& Aentry = pair.second;
            ++count;
            Aentry->BuildAuctionInfo(searchResponse->packet);
            if (count >= MAX_GETALL_RETURN)
                break;
        }
        totalCount = searchableAuctionMap.size();
    }

    searchResponse->packet.put<uint32>(0, count);
    searchResponse->packet << totalCount;
    searchResponse->packet << (uint32)sWorld->getIntConfig(CONFIG_AUCTION_SEARCH_DELAY);

    TC_LOG_DEBUG("auctionHouse", "SearchListRequest Queueing Response {}", GameTime::GetGameTimeMS());
    responseQueue_->send(std::move(searchResponse));
}

void AuctionHouseWorkerThread::BuildListAuctionItems(ListAuctionMessage const& searchRequest, SortableAuctionEntriesList& auctionEntries, SearchableAuctionEntriesMap const& auctionMap) const
{
    TC_LOG_DEBUG("auctionHouse", "BuildListAuctionItems Called {}", GameTime::GetGameTimeMS());
    // pussywizard: optimization, this is a simplified case for the default search state (no filters)
    if (searchRequest.searchInfo.itemClass == 0xffffffff && searchRequest.searchInfo.itemSubClass == 0xffffffff
        && searchRequest.searchInfo.inventoryType == 0xffffffff && searchRequest.searchInfo.quality == 0xffffffff
        && searchRequest.searchInfo.levelmin == 0x00 && searchRequest.searchInfo.levelmax == 0x00
        && searchRequest.searchInfo.usable == 0x00 && searchRequest.searchInfo.wsearchedname.empty())
    {
        for (auto const& pair : auctionMap)
            auctionEntries.push_back(pair.second.get());

        return;
    }

    for (auto const& pair : auctionMap)
    {
        std::shared_ptr<SearchableAuctionEntry> const& Aentry = pair.second;
        SearchableAuctionEntryItem const& Aitem = Aentry->item;
        ItemTemplate const* proto = Aitem.itemTemplate;

        if (searchRequest.searchInfo.itemClass != 0xffffffff && proto->Class != searchRequest.searchInfo.itemClass)
            continue;

        if (searchRequest.searchInfo.itemSubClass != 0xffffffff && proto->SubClass != searchRequest.searchInfo.itemSubClass)
            continue;

        if (searchRequest.searchInfo.inventoryType != 0xffffffff && proto->InventoryType != searchRequest.searchInfo.inventoryType)
        {
            // xinef: exception, robes are counted as chests
            if (searchRequest.searchInfo.inventoryType != INVTYPE_CHEST || proto->InventoryType != INVTYPE_ROBE)
                continue;
        }

        if (searchRequest.searchInfo.quality != 0xffffffff && proto->Quality < searchRequest.searchInfo.quality)
            continue;

        if (searchRequest.searchInfo.levelmin != 0x00 && (proto->RequiredLevel < searchRequest.searchInfo.levelmin
            || (searchRequest.searchInfo.levelmax != 0x00 && proto->RequiredLevel > searchRequest.searchInfo.levelmax)))
            continue;

        if (searchRequest.searchInfo.usable != 0x00)
        {
            if (!searchRequest.playerInfo.usablePlayerInfo.value().PlayerCanUseItem(proto))
                continue;
        }

        // Allow search by suffix (ie: of the Monkey) or partial name (ie: Monkey)
        // No need to do any of this if no search term was entered
        if (!searchRequest.searchInfo.wsearchedname.empty())
        {
            if (Aitem.itemName[searchRequest.playerInfo.loc_idx].find(searchRequest.searchInfo.wsearchedname) == std::wstring::npos)
                continue;
        }

        auctionEntries.push_back(Aentry.get());
    }
}

void AuctionHouseWorkerThread::ListBidderAuctions(ListBidderAuctionMessage const& message)
{
    TC_LOG_DEBUG("auctionHouse", "ListBidderAuctions Called {}", GameTime::GetGameTimeMS());
    SearchableAuctionEntriesMap const& searchableAuctionMap = GetSearchableAuctionMap(message.listFaction);
    std::shared_lock<std::shared_mutex> lock(GetMapMutex(message.listFaction));

    auto searchResponse = std::make_unique<ListAuctionMessageResponse>();
    searchResponse->playerGuid = message.ownerGuid;
    searchResponse->packet.Initialize(SMSG_AUCTION_BIDDER_LIST_RESULT, (4 + 4 + 4));
    searchResponse->packet << (uint32)0;                                     //add 0 as count

    uint32 count = 0;
    uint32 totalcount = 0;

    for (uint32 const auctionId : message.outbiddedAuctionIds)
    {
        SearchableAuctionEntriesMap::const_iterator itr = searchableAuctionMap.find(auctionId);
        if (itr == searchableAuctionMap.end())
            continue;

        std::shared_ptr<SearchableAuctionEntry> const& auctionEntry = itr->second;
        auctionEntry->BuildAuctionInfo(searchResponse->packet);
        ++count;
        ++totalcount;
    }

    for (auto const& pair : searchableAuctionMap)
    {
        if (pair.second->bidderGuid != message.ownerGuid)
            continue;

        std::shared_ptr<SearchableAuctionEntry> const& auctionEntry = pair.second;
        auctionEntry->BuildAuctionInfo(searchResponse->packet);
        ++count;
        ++totalcount;
    }

    searchResponse->packet.put<uint32>(0, count);
    searchResponse->packet << totalcount;
    searchResponse->packet << (uint32)sWorld->getIntConfig(CONFIG_AUCTION_SEARCH_DELAY);

    TC_LOG_DEBUG("auctionHouse", "ListBidderAuctions Queueing Response {}", GameTime::GetGameTimeMS());
    responseQueue_->send(std::move(searchResponse));
}

void AuctionHouseWorkerThread::ListOwnerAuctions(ListOwnerAuctionMessage const& message)
{
    TC_LOG_DEBUG("auctionHouse", "ListOwnerAuctions Called {}", GameTime::GetGameTimeMS());
    SearchableAuctionEntriesMap const& searchableAuctionMap = GetSearchableAuctionMap(message.listFaction);
    std::shared_lock<std::shared_mutex> lock(GetMapMutex(message.listFaction));

    auto searchResponse = std::make_unique<ListAuctionMessageResponse>();
    searchResponse->playerGuid = message.ownerGuid;
    searchResponse->packet.Initialize(SMSG_AUCTION_OWNER_LIST_RESULT, (4 + 4 + 4));
    searchResponse->packet << (uint32)0;

    uint32 count = 0;
    uint32 totalcount = 0;

    for (auto const& pair : searchableAuctionMap)
    {
        if (pair.second->ownerGuid != message.ownerGuid)
            continue;

        std::shared_ptr<SearchableAuctionEntry> const& auctionEntry = pair.second;
        auctionEntry->BuildAuctionInfo(searchResponse->packet);
        ++count;
        ++totalcount;
    }

    searchResponse->packet.put<uint32>(0, count);
    searchResponse->packet << (uint32)totalcount;
    searchResponse->packet << (uint32)sWorld->getIntConfig(CONFIG_AUCTION_SEARCH_DELAY);

    TC_LOG_DEBUG("auctionHouse", "ListOwnerAuctions Queueing Response {}", GameTime::GetGameTimeMS());
    responseQueue_->send(std::move(searchResponse));
}