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

class AuctionHouseWorkerThread
{
public:
    AuctionHouseWorkerThread(SignalQueue<std::unique_ptr<AuctionSearcherRequest>>* requestQueue,
                            SignalQueue<std::unique_ptr<AuctionSearcherResponse>>* responseQueue);
    void AddAuctionSearchUpdateToQueue(std::shared_ptr<AuctionSearcherUpdate> const update);

private:
void Run(std::stop_token stop);
    void ProcessSearchUpdate(std::shared_ptr<AuctionSearcherUpdate> const& update);
    void ProcessSearchRequest(std::unique_ptr<AuctionSearcherRequest> request);

    void SearchUpdateAdd(AuctionSearchAdd const& auctionAdd);
    void SearchUpdateRemove(AuctionSearchRemove const& auctionRemove);
    void SearchUpdateBid(AuctionSearchUpdateBid const& auctionUpdateBid);
    void SearchListRequest(AuctionSearchListRequest const& searchListRequest);
    void SearchOwnerListRequest(AuctionSearchOwnerListRequest const& searchOwnerListRequest);
    void SearchBidderListRequest(AuctionSearchBidderListRequest const& searchBidderListRequest);
    void BuildListAuctionItems(AuctionSearchListRequest const& searchRequest, SortableAuctionEntriesList& auctionEntries, SearchableAuctionEntriesMap const& auctionMap) const;

    SearchableAuctionEntriesMap& GetSearchableAuctionMap(uint8 faction) { return _searchableAuctionMap[faction]; }

    SearchableAuctionEntriesMap _searchableAuctionMap[AUCTION_FACTION_MAX];
    std::jthread workerThread_;
    SignalQueue<std::unique_ptr<AuctionSearcherRequest>>* requestQueue_;
    SignalQueue<std::unique_ptr<AuctionSearcherResponse>>* responseQueue_;
    SignalQueue<std::shared_ptr<AuctionSearcherUpdate>> updateQueue_;
};

#endif // AUCTION_HOUSE_WORKER_THREAD_H