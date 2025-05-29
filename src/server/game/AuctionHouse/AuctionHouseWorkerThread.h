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

#include "AuctionHouseDefines.h"
#include "MPSCQueue.h"
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

struct ListAuctionResponse
{
    ObjectGuid playerGuid;
    WorldPacket packet;
};

template<typename T>
class SignalQueue {
public:
    explicit SignalQueue() {}

    void send(T value, std::stop_token stop = {});
    std::optional<T> receive(std::stop_token stop = {});
    std::optional<T> try_receive();
    void close();

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable_any cv_;
};

typedef std::vector<AuctionSortInfo> AuctionSortOrderVector;
typedef std::unordered_map<uint32, std::shared_ptr<SearchableAuctionEntry>> SearchableAuctionEntriesMap;
typedef std::vector<SearchableAuctionEntry*> SortableAuctionEntriesList;

class AuctionSorter
{
public:
    AuctionSorter(AuctionSortOrderVector const* sort, int loc_idx) : _sort(sort), _loc_idx(loc_idx) {}
    bool operator()(SearchableAuctionEntry const* auc1, SearchableAuctionEntry const* auc2) const;

private:
    AuctionSortOrderVector const* _sort;
    int _loc_idx;
};

class AuctionHouseWorkerThread
{
public:
    AuctionHouseWorkerThread(SignalQueue<std::unique_ptr<AuctionMessage>>* requestQueue, MPSCQueue<ListAuctionResponse>* responseQueue);
    ~AuctionHouseWorkerThread();
    void QueueModifyAuctionsMessage(std::shared_ptr<AuctionMessage> message);

private:
    void Run(std::stop_token stop);
    void AddAuction(AddAuctionMessage const& message);
    void RemoveAuction(RemoveAuctionMessage const& message);
    void UpdateAuctionBid(UpdateAuctionBidMessage const& message);
    void ListAuctions(ListAuctionMessage const& message);
    void BuildListAuctionItems(ListAuctionMessage const& message, SortableAuctionEntriesList& auctionEntries, SearchableAuctionEntriesMap const& auctionMap) const;
    void ListBidderAuctions(ListBidderAuctionMessage const& message);
    void ListOwnerAuctions(ListOwnerAuctionMessage const& message);

    SignalQueue<std::shared_ptr<AuctionMessage>> _modifyQueue;
    SignalQueue<std::unique_ptr<AuctionMessage>>* _requestQueue;
    MPSCQueue<ListAuctionResponse>* _responseQueue;
    std::unordered_map<uint8, SearchableAuctionEntriesMap> _auctions;
    std::jthread _workerThread;
};

#endif // AUCTION_HOUSE_WORKER_THREAD_H