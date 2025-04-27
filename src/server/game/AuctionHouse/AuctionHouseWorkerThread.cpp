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
#include "World.h"

#define MAX_AUCTIONS_PER_PAGE 50

AuctionHouseWorkerThread::AuctionHouseWorkerThread(SignalQueue<std::unique_ptr<AuctionSearcherRequest>>* requestQueue,
                                                   SignalQueue<std::unique_ptr<AuctionSearcherResponse>>* responseQueue)
    : requestQueue_(requestQueue), responseQueue_(responseQueue) {
    workerThread_ = std::jthread([this](std::stop_token stop) { Run(stop); });
}

void AuctionHouseWorkerThread::AddAuctionSearchUpdateToQueue(std::shared_ptr<AuctionSearcherUpdate> const update) {
    updateQueue_.send(update, workerThread_.get_stop_token());
}

void AuctionHouseWorkerThread::Run(std::stop_token stop) {
    TC_LOG_DEBUG("auctionHouse", "AuctionHouseWorkerThread Running");
    while (!stop.stop_requested()) {
        bool processed = false;

        // Check for an update
        if (auto update = updateQueue_.try_receive()) {
            ProcessSearchUpdate(*update);
            processed = true;
        }

        // Check for a request
        if (auto request = requestQueue_->try_receive()) {
            ProcessSearchRequest(std::move(*request));
            processed = true;
        }

        // If no work was processed, sleep briefly to avoid busy-waiting
        if (!processed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void AuctionHouseWorkerThread::ProcessSearchUpdate(std::shared_ptr<AuctionSearcherUpdate> const& update) {
    switch (update->updateType) {
        case AuctionSearcherUpdate::Type::ADD:
            SearchUpdateAdd(*std::static_pointer_cast<AuctionSearchAdd>(update).get());
            break;
        case AuctionSearcherUpdate::Type::REMOVE:
            SearchUpdateRemove(*std::static_pointer_cast<AuctionSearchRemove>(update).get());
            break;
        case AuctionSearcherUpdate::Type::UPDATE_BID:
            SearchUpdateBid(*std::static_pointer_cast<AuctionSearchUpdateBid>(update).get());
            break;
        default:
            break;
    }
}

void AuctionHouseWorkerThread::ProcessSearchRequest(std::unique_ptr<AuctionSearcherRequest> request) {
    switch (request->requestType) {
        case AuctionSearcherRequest::Type::LIST:
            SearchListRequest(*static_cast<AuctionSearchListRequest*>(request.get()));
            break;
        case AuctionSearcherRequest::Type::OWNER_LIST:
            SearchOwnerListRequest(*static_cast<AuctionSearchOwnerListRequest*>(request.get()));
            break;
        case AuctionSearcherRequest::Type::BIDDER_LIST:
            SearchBidderListRequest(*static_cast<AuctionSearchBidderListRequest*>(request.get()));
            break;
        default:
            break;
    }
}

void AuctionHouseWorkerThread::SearchUpdateAdd(AuctionSearchAdd const& auctionAdd)
{
    TC_LOG_DEBUG("auctionHouse", "SearchUpdateAdd Called");
    SearchableAuctionEntriesMap& searchableAuctionMap = GetSearchableAuctionMap(auctionAdd.listFaction);
    searchableAuctionMap.insert(std::make_pair(auctionAdd.searchableAuctionEntry->Id, auctionAdd.searchableAuctionEntry));
}

void AuctionHouseWorkerThread::SearchUpdateRemove(AuctionSearchRemove const& auctionRemove)
{
    TC_LOG_DEBUG("auctionHouse", "SearchUpdateRemove Called");
    SearchableAuctionEntriesMap& searchableAuctionMap = GetSearchableAuctionMap(auctionRemove.listFaction);
    searchableAuctionMap.erase(auctionRemove.auctionId);
}

void AuctionHouseWorkerThread::SearchUpdateBid(AuctionSearchUpdateBid const& auctionUpdateBid)
{
    TC_LOG_DEBUG("auctionHouse", "SearchUpdateBid Called");
    SearchableAuctionEntriesMap const& searchableAuctionMap = GetSearchableAuctionMap(auctionUpdateBid.listFaction);
    SearchableAuctionEntriesMap::const_iterator itr = searchableAuctionMap.find(auctionUpdateBid.auctionId);
    if (itr != searchableAuctionMap.end())
    {
        itr->second->bid = auctionUpdateBid.bid;
        itr->second->bidderGuid = auctionUpdateBid.bidderGuid;
    }
}

void AuctionHouseWorkerThread::SearchListRequest(AuctionSearchListRequest const& searchListRequest)
{
    TC_LOG_DEBUG("auctionHouse", "SearchListRequest Called");
    SearchableAuctionEntriesMap const& searchableAuctionMap = GetSearchableAuctionMap(searchListRequest.listFaction);
    uint32 count = 0, totalCount = 0;

    auto searchResponse = std::make_unique<AuctionSearcherResponse>();
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

    TC_LOG_DEBUG("auctionHouse", "Queueing Response");

    responseQueue_->send(std::move(searchResponse));
}

void AuctionHouseWorkerThread::SearchOwnerListRequest(AuctionSearchOwnerListRequest const& searchOwnerListRequest)
{
    TC_LOG_DEBUG("auctionHouse", "SearchOwnerListRequest Called");
    SearchableAuctionEntriesMap const& searchableAuctionMap = GetSearchableAuctionMap(searchOwnerListRequest.listFaction);

    auto searchResponse = std::make_unique<AuctionSearcherResponse>();
    searchResponse->playerGuid = searchOwnerListRequest.ownerGuid;
    searchResponse->packet.Initialize(SMSG_AUCTION_OWNER_LIST_RESULT, (4 + 4 + 4));
    searchResponse->packet << (uint32)0;

    uint32 count = 0;
    uint32 totalcount = 0;

    for (auto const& pair : searchableAuctionMap)
    {
        if (pair.second->ownerGuid != searchOwnerListRequest.ownerGuid)
            continue;

        std::shared_ptr<SearchableAuctionEntry> const& auctionEntry = pair.second;
        auctionEntry->BuildAuctionInfo(searchResponse->packet);
        ++count;
        ++totalcount;
    }

    searchResponse->packet.put<uint32>(0, count);
    searchResponse->packet << (uint32)totalcount;
    searchResponse->packet << (uint32)sWorld->getIntConfig(CONFIG_AUCTION_SEARCH_DELAY);

    TC_LOG_DEBUG("auctionHouse", "Queueing Response");
    responseQueue_->send(std::move(searchResponse));
}

void AuctionHouseWorkerThread::SearchBidderListRequest(AuctionSearchBidderListRequest const& searchBidderListRequest)
{
    TC_LOG_DEBUG("auctionHouse", "SearchBidderListRequest Called");
    SearchableAuctionEntriesMap const& searchableAuctionMap = GetSearchableAuctionMap(searchBidderListRequest.listFaction);

    auto searchResponse = std::make_unique<AuctionSearcherResponse>();
    searchResponse->playerGuid = searchBidderListRequest.ownerGuid;
    searchResponse->packet.Initialize(SMSG_AUCTION_BIDDER_LIST_RESULT, (4 + 4 + 4));
    searchResponse->packet << (uint32)0;                                     //add 0 as count

    uint32 count = 0;
    uint32 totalcount = 0;

    for (uint32 const auctionId : searchBidderListRequest.outbiddedAuctionIds)
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
        if (pair.second->bidderGuid != searchBidderListRequest.ownerGuid)
            continue;

        std::shared_ptr<SearchableAuctionEntry> const& auctionEntry = pair.second;
        auctionEntry->BuildAuctionInfo(searchResponse->packet);
        ++count;
        ++totalcount;
    }

    searchResponse->packet.put<uint32>(0, count);
    searchResponse->packet << totalcount;
    searchResponse->packet << (uint32)sWorld->getIntConfig(CONFIG_AUCTION_SEARCH_DELAY);

    TC_LOG_DEBUG("auctionHouse", "Queueing Response");
    responseQueue_->send(std::move(searchResponse));
}

void AuctionHouseWorkerThread::BuildListAuctionItems(AuctionSearchListRequest const& searchRequest, SortableAuctionEntriesList& auctionEntries, SearchableAuctionEntriesMap const& auctionMap) const
{
    TC_LOG_DEBUG("auctionHouse", "BuildListAuctionItems Called");
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