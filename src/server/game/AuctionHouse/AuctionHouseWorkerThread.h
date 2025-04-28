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

#ifndef AUCTION_HOUSE_WORKER_THREAD_H
#define AUCTION_HOUSE_WORKER_THREAD_H

#include "SignalQueue.h"
#include "AuctionHouseCommon.h"
#include <memory>
#include <thread>
#include <unordered_map>
#include <shared_mutex>

struct AuctionMessage
{
    enum class Type : uint8
    {
        Add,
        Remove,
        UpdateBid,
        List,
        ListOwner,
        ListBidder
    };

    AuctionMessage(Type const _type, AuctionHouseId _houseId) : type(_type), houseId(_houseId) {}
    virtual ~AuctionMessage() = default;

    Type type;
    AuctionHouseId houseId;
};

struct AddAuctionMessage : AuctionMessage
{
    AddAuctionMessage(std::shared_ptr<SearchableAuctionEntry> _searchableAuctionEntry)
        : AuctionMessage(AuctionMessage::Type::Add, _searchableAuctionEntry->houseId), searchableAuctionEntry(_searchableAuctionEntry) {}

    std::shared_ptr<SearchableAuctionEntry> searchableAuctionEntry;
};

struct RemoveAuctionMessage : AuctionMessage
{
    RemoveAuctionMessage(uint32 _auctionId, AuctionHouseId _houseId)
        : AuctionMessage(AuctionMessage::Type::Remove, _houseId), auctionId(_auctionId) {}

    uint32 auctionId;
};

struct UpdateAuctionBidMessage : AuctionMessage
{
    UpdateAuctionBidMessage(uint32 _auctionId, AuctionHouseId _houseId, uint32 _bid, ObjectGuid _bidderGuid)
        : AuctionMessage(AuctionMessage::Type::UpdateBid, _houseId), auctionId(_auctionId), bid(_bid), bidderGuid(_bidderGuid) {}
    uint32 auctionId;
    uint32 bid;
    ObjectGuid bidderGuid;
};

struct ListAuctionMessage : AuctionMessage
{
    ListAuctionMessage(AuctionHouseId _houseId, AuctionHouseSearchInfo const&& _searchInfo, AuctionHousePlayerInfo const&& _playerInfo)
        : AuctionMessage(AuctionMessage::Type::List, _houseId), searchInfo(_searchInfo), playerInfo(_playerInfo) {}

    AuctionHouseSearchInfo searchInfo;
    AuctionHousePlayerInfo playerInfo;
};

struct ListOwnerAuctionMessage : AuctionMessage
{
    ListOwnerAuctionMessage(AuctionHouseId _houseId, ObjectGuid _ownerGuid)
        : AuctionMessage(AuctionMessage::Type::ListOwner, _houseId), ownerGuid(_ownerGuid) {}

    ObjectGuid ownerGuid;
};

struct ListBidderAuctionMessage : AuctionMessage
{
    ListBidderAuctionMessage(AuctionHouseId _houseId, std::vector<uint32> const&& _outbiddedAuctionIds, ObjectGuid _ownerGuid)
        : AuctionMessage(AuctionMessage::Type::ListBidder, _houseId), outbiddedAuctionIds(_outbiddedAuctionIds), ownerGuid(_ownerGuid) {}
    std::vector<uint32> outbiddedAuctionIds;
    ObjectGuid ownerGuid;
};

struct ListAuctionMessageResponse
{
    ObjectGuid playerGuid;
    WorldPacket packet;
};

class AuctionHouseWorkerThread
{
public:
    AuctionHouseWorkerThread(
        SignalQueue<std::unique_ptr<AuctionMessage>>* messageQueue,
        SignalQueue<std::unique_ptr<ListAuctionMessageResponse>>* responseQueue,
        AuctionHouseMap& auctionHouseMap);
    ~AuctionHouseWorkerThread();

private:
    void Run(std::stop_token stop);
    void ProcessMessage(std::unique_ptr<AuctionMessage> message);
    void AddAuction(AddAuctionMessage const& message);
    void RemoveAuction(RemoveAuctionMessage const& message);
    void UpdateAuctionBid(UpdateAuctionBidMessage const& message);
    void ListAuctions(ListAuctionMessage const& message);
    void BuildListAuctionItems(ListAuctionMessage const& message, SortableAuctionEntriesList& auctionEntries, SearchableAuctionEntriesMap const& auctionMap) const;
    void ListBidderAuctions(ListBidderAuctionMessage const& message);
    void ListOwnerAuctions(ListOwnerAuctionMessage const& message);

    AuctionHouseObject* GetAuctionHouse(AuctionHouseId houseId) {
        auto it = auctionHouseMap_.find(houseId);
        return it != auctionHouseMap_.end() ? it->second.get() : nullptr;
    }

    SignalQueue<std::unique_ptr<AuctionMessage>>* messageQueue_;
    SignalQueue<std::unique_ptr<ListAuctionMessageResponse>>* responseQueue_;
    AuctionHouseMap& auctionHouseMap_;
    std::jthread workerThread_;
};

#endif // AUCTION_HOUSE_WORKER_THREAD_H