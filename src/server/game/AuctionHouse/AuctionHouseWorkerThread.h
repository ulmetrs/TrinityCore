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

#include "AuctionHouseCommon.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>
#include <thread>


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

    AuctionMessage(Type const _type, uint8 _houseId) : type(_type), houseId(_houseId) {}
    virtual ~AuctionMessage() = default;

    Type type;
    uint8 houseId;
};

struct AddAuctionMessage : AuctionMessage
{
    AddAuctionMessage(std::shared_ptr<SearchableAuctionEntry> _searchableAuctionEntry)
        : AuctionMessage(AuctionMessage::Type::Add, _searchableAuctionEntry->houseId), searchableAuctionEntry(_searchableAuctionEntry) {}

    std::shared_ptr<SearchableAuctionEntry> searchableAuctionEntry;
};

struct RemoveAuctionMessage : AuctionMessage
{
    RemoveAuctionMessage(uint32 _auctionId, uint8 _houseId)
        : AuctionMessage(AuctionMessage::Type::Remove, _houseId), auctionId(_auctionId) {}

    uint32 auctionId;
};

struct UpdateAuctionBidMessage : AuctionMessage
{
    UpdateAuctionBidMessage(uint32 _auctionId, uint8 _houseId, uint32 _bid, ObjectGuid _bidderGuid)
        : AuctionMessage(AuctionMessage::Type::UpdateBid, _houseId), auctionId(_auctionId), bid(_bid), bidderGuid(_bidderGuid) {}
    uint32 auctionId;
    uint32 bid;
    ObjectGuid bidderGuid;
};

struct ListAuctionMessage : AuctionMessage
{
    ListAuctionMessage(uint8 _houseId, AuctionHouseSearchInfo const&& _searchInfo, AuctionHousePlayerInfo const&& _playerInfo)
        : AuctionMessage(AuctionMessage::Type::List, _houseId), searchInfo(_searchInfo), playerInfo(_playerInfo) {}

    AuctionHouseSearchInfo searchInfo;
    AuctionHousePlayerInfo playerInfo;
};

struct ListOwnerAuctionMessage : AuctionMessage
{
    ListOwnerAuctionMessage(uint8 _houseId, ObjectGuid _ownerGuid)
        : AuctionMessage(AuctionMessage::Type::ListOwner, _houseId), ownerGuid(_ownerGuid) {}

    ObjectGuid ownerGuid;
};

struct ListBidderAuctionMessage : AuctionMessage
{
    ListBidderAuctionMessage(uint8 _houseId, std::vector<uint32> const&& _outbiddedAuctionIds, ObjectGuid _ownerGuid)
        : AuctionMessage(AuctionMessage::Type::ListBidder, _houseId), outbiddedAuctionIds(_outbiddedAuctionIds), ownerGuid(_ownerGuid) {}
    std::vector<uint32> outbiddedAuctionIds;
    ObjectGuid ownerGuid;
};

template<typename T>
class SignalQueue {
public:
    explicit SignalQueue(size_t capacity = 0) : capacity_(capacity) {}

    void send(T value, std::stop_token stop = {});
    std::optional<T> receive(std::stop_token stop = {});
    std::optional<T> try_receive();
    void close();

private:
    std::queue<T> queue_;
    size_t capacity_;
    std::mutex mutex_;
    std::condition_variable_any cv_;
};

class AuctionHouseWorkerThread
{
public:
    AuctionHouseWorkerThread(
        SignalQueue<std::unique_ptr<AuctionMessage>>* messageQueue,
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

    AuctionHouseObject* GetAuctionHouse(uint8 houseId) {
        auto it = auctionHouseMap_.find(houseId);
        return it != auctionHouseMap_.end() ? it->second.get() : nullptr;
    }

    SignalQueue<std::unique_ptr<AuctionMessage>>* messageQueue_;
    AuctionHouseMap& auctionHouseMap_;
    std::jthread workerThread_;
};

#endif // AUCTION_HOUSE_WORKER_THREAD_H