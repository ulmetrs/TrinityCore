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
#include "AuctionHouseCommon.h"
#include "GameTime.h"
#include "World.h"
#include "WorldPacket.h"

template<typename T>
void SignalQueue<T>::send(T value, std::stop_token stop) {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, stop, [this] {
        return queue_.size() < capacity_ || capacity_ == 0;
    });
    if (stop.stop_requested()) return;
    queue_.push(std::move(value));
    lock.unlock();
    cv_.notify_one();
}

template<typename T>
std::optional<T> SignalQueue<T>::receive(std::stop_token stop) {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, stop, [this] { return !queue_.empty(); });
    if (stop.stop_requested() || queue_.empty()) return std::nullopt;
    T value = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    cv_.notify_one();
    return value;
}

template<typename T>
std::optional<T> SignalQueue<T>::try_receive() {
    std::unique_lock lock(mutex_);
    if (queue_.empty()) return std::nullopt;
    T value = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    cv_.notify_one();
    return value;
}

template<typename T>
void SignalQueue<T>::close() {
    std::unique_lock lock(mutex_);
    while (!queue_.empty()) queue_.pop();
    cv_.notify_all();
}

// Explicit template instantiations
template class SignalQueue<std::unique_ptr<AuctionMessage>>;

AuctionHouseWorkerThread::AuctionHouseWorkerThread(
    SignalQueue<std::unique_ptr<AuctionMessage>>* messageQueue,
    AuctionHouseMap& auctionHouseMap)
    : messageQueue_(messageQueue), auctionHouseMap_(auctionHouseMap)
{
    workerThread_ = std::jthread([this](std::stop_token stop) { Run(stop); });
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
            ProcessMessage(std::move(*message));
        }
    }
}

void AuctionHouseWorkerThread::ProcessMessage(std::unique_ptr<AuctionMessage> message)
{
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

void AuctionHouseWorkerThread::AddAuction(AddAuctionMessage const& message)
{
    AuctionHouseObject* auctionHouse = GetAuctionHouse(message.houseId);
    SearchableAuctionEntriesMap& searchableAuctionMap = auctionHouse->GetSearchableAuctionMap();
    std::unique_lock<std::shared_mutex> lock(auctionHouse->GetMapMutex());
    searchableAuctionMap.insert(std::make_pair(message.searchableAuctionEntry->Id, message.searchableAuctionEntry));
}

void AuctionHouseWorkerThread::RemoveAuction(RemoveAuctionMessage const& message)
{
    AuctionHouseObject* auctionHouse = GetAuctionHouse(message.houseId);
    SearchableAuctionEntriesMap& searchableAuctionMap = auctionHouse->GetSearchableAuctionMap();
    std::unique_lock<std::shared_mutex> lock(auctionHouse->GetMapMutex());
    searchableAuctionMap.erase(message.auctionId);
}

void AuctionHouseWorkerThread::UpdateAuctionBid(UpdateAuctionBidMessage const& message)
{
    AuctionHouseObject* auctionHouse = GetAuctionHouse(message.houseId);
    SearchableAuctionEntriesMap& searchableAuctionMap = auctionHouse->GetSearchableAuctionMap();
    std::unique_lock<std::shared_mutex> lock(auctionHouse->GetMapMutex());
    SearchableAuctionEntriesMap::const_iterator itr = searchableAuctionMap.find(message.auctionId);
    if (itr != searchableAuctionMap.end())
    {

        itr->second->bid = message.bid;
        itr->second->bidderGuid = message.bidderGuid;
    }
}

void AuctionHouseWorkerThread::ListAuctions(ListAuctionMessage const& message)
{
    AuctionHouseObject* auctionHouse = GetAuctionHouse(message.houseId);
    SearchableAuctionEntriesMap const& searchableAuctionMap = auctionHouse->GetSearchableAuctionMap();
    std::shared_lock<std::shared_mutex> lock(auctionHouse->GetMapMutex());
    uint32 count = 0, totalCount = 0;

    WorldPacket packet;
    packet.Initialize(SMSG_AUCTION_LIST_RESULT, (4 + 4 + 4));
    packet << (uint32)0;

    if (!message.searchInfo.getAll)
    {
        SortableAuctionEntriesList auctionEntries;
        BuildListAuctionItems(message, auctionEntries, searchableAuctionMap);

        if (!message.searchInfo.sorting.empty() && auctionEntries.size() > MAX_AUCTIONS_PER_PAGE)
        {
            AuctionSorter sorter(&message.searchInfo.sorting, message.playerInfo.loc_idx);
            std::sort(auctionEntries.begin(), auctionEntries.end(), sorter);
        }

        SortableAuctionEntriesList::const_iterator itr = auctionEntries.begin();
        if (message.searchInfo.listfrom)
        {
            if (message.searchInfo.listfrom > auctionEntries.size())
                itr = auctionEntries.end();
            else
                itr += message.searchInfo.listfrom;
        }

        for (; itr != auctionEntries.end(); ++itr)
        {
            (*itr)->BuildAuctionInfo(packet);
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
            Aentry->BuildAuctionInfo(packet);
            if (count >= MAX_GETALL_RETURN)
                break;
        }
        totalCount = searchableAuctionMap.size();
    }

    packet.put<uint32>(0, count);
    packet << totalCount;
    packet << (uint32)sWorld->getIntConfig(CONFIG_AUCTION_SEARCH_DELAY);

    if (Player* player = ObjectAccessor::FindConnectedPlayer(message.playerInfo.playerGuid))
    {
        player->GetSession()->SendPacket(&packet);
    }
}

void AuctionHouseWorkerThread::BuildListAuctionItems(ListAuctionMessage const& message, SortableAuctionEntriesList& auctionEntries, SearchableAuctionEntriesMap const& auctionMap) const
{
    // pussywizard: optimization, this is a simplified case for the default search state (no filters)
    if (message.searchInfo.itemClass == 0xffffffff && message.searchInfo.itemSubClass == 0xffffffff
        && message.searchInfo.inventoryType == 0xffffffff && message.searchInfo.quality == 0xffffffff
        && message.searchInfo.levelmin == 0x00 && message.searchInfo.levelmax == 0x00
        && message.searchInfo.usable == 0x00 && message.searchInfo.wsearchedname.empty())
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

        if (message.searchInfo.itemClass != 0xffffffff && proto->Class != message.searchInfo.itemClass)
            continue;

        if (message.searchInfo.itemSubClass != 0xffffffff && proto->SubClass != message.searchInfo.itemSubClass)
            continue;

        if (message.searchInfo.inventoryType != 0xffffffff && proto->InventoryType != message.searchInfo.inventoryType)
        {
            // xinef: exception, robes are counted as chests
            if (message.searchInfo.inventoryType != INVTYPE_CHEST || proto->InventoryType != INVTYPE_ROBE)
                continue;
        }

        if (message.searchInfo.quality != 0xffffffff && proto->Quality < message.searchInfo.quality)
            continue;

        if (message.searchInfo.levelmin != 0x00 && (proto->RequiredLevel < message.searchInfo.levelmin
            || (message.searchInfo.levelmax != 0x00 && proto->RequiredLevel > message.searchInfo.levelmax)))
            continue;

        if (message.searchInfo.usable != 0x00)
        {
            if (!message.playerInfo.usablePlayerInfo.value().PlayerCanUseItem(proto))
                continue;
        }

        // Allow search by suffix (ie: of the Monkey) or partial name (ie: Monkey)
        // No need to do any of this if no search term was entered
        if (!message.searchInfo.wsearchedname.empty())
        {
            if (Aitem.itemName[message.playerInfo.loc_idx].find(message.searchInfo.wsearchedname) == std::wstring::npos)
                continue;
        }

        auctionEntries.push_back(Aentry.get());
    }
}

void AuctionHouseWorkerThread::ListBidderAuctions(ListBidderAuctionMessage const& message)
{
    AuctionHouseObject* auctionHouse = GetAuctionHouse(message.houseId);
    SearchableAuctionEntriesMap const& searchableAuctionMap = auctionHouse->GetSearchableAuctionMap();
    std::shared_lock<std::shared_mutex> lock(auctionHouse->GetMapMutex());

    WorldPacket packet;
    packet.Initialize(SMSG_AUCTION_BIDDER_LIST_RESULT, (4 + 4 + 4));
    packet << (uint32)0;                                     //add 0 as count

    uint32 count = 0;
    uint32 totalcount = 0;

    for (uint32 const auctionId : message.outbiddedAuctionIds)
    {
        SearchableAuctionEntriesMap::const_iterator itr = searchableAuctionMap.find(auctionId);
        if (itr == searchableAuctionMap.end())
            continue;

        std::shared_ptr<SearchableAuctionEntry> const& auctionEntry = itr->second;
        auctionEntry->BuildAuctionInfo(packet);
        ++count;
        ++totalcount;
    }

    for (auto const& pair : searchableAuctionMap)
    {
        if (pair.second->bidderGuid != message.ownerGuid)
            continue;

        std::shared_ptr<SearchableAuctionEntry> const& auctionEntry = pair.second;
        auctionEntry->BuildAuctionInfo(packet);
        ++count;
        ++totalcount;
    }

    packet.put<uint32>(0, count);
    packet << totalcount;
    packet << (uint32)sWorld->getIntConfig(CONFIG_AUCTION_SEARCH_DELAY);

    if (Player* player = ObjectAccessor::FindConnectedPlayer(message.ownerGuid))
    {
        player->GetSession()->SendPacket(&packet);
    }
}

void AuctionHouseWorkerThread::ListOwnerAuctions(ListOwnerAuctionMessage const& message)
{
    AuctionHouseObject* auctionHouse = GetAuctionHouse(message.houseId);
    SearchableAuctionEntriesMap const& searchableAuctionMap = auctionHouse->GetSearchableAuctionMap();
    std::shared_lock<std::shared_mutex> lock(auctionHouse->GetMapMutex());

    WorldPacket packet;
    packet.Initialize(SMSG_AUCTION_OWNER_LIST_RESULT, (4 + 4 + 4));
    packet << (uint32)0;

    uint32 count = 0;
    uint32 totalcount = 0;

    for (auto const& pair : searchableAuctionMap)
    {
        if (pair.second->ownerGuid != message.ownerGuid)
            continue;

        std::shared_ptr<SearchableAuctionEntry> const& auctionEntry = pair.second;
        auctionEntry->BuildAuctionInfo(packet);
        ++count;
        ++totalcount;
    }

    packet.put<uint32>(0, count);
    packet << (uint32)totalcount;
    packet << (uint32)sWorld->getIntConfig(CONFIG_AUCTION_SEARCH_DELAY);

    if (Player* player = ObjectAccessor::FindConnectedPlayer(message.ownerGuid))
    {
        player->GetSession()->SendPacket(&packet);
    }
}