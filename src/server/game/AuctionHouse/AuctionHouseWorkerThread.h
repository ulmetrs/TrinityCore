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
    AuctionHouseWorkerThread(SignalQueue<std::unique_ptr<AuctionMessage>>* messageQueue,
        SignalQueue<std::unique_ptr<ListAuctionMessageResponse>>* responseQueue);
    ~AuctionHouseWorkerThread();
    void AddAuctionMessageToQueue(std::unique_ptr<AuctionMessage> message);

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

    SearchableAuctionEntriesMap& GetSearchableAuctionMap(uint8 faction) { return searchableAuctionMap_[faction]; }

    SearchableAuctionEntriesMap searchableAuctionMap_[AUCTION_FACTION_MAX];
    std::jthread workerThread_;
    SignalQueue<std::unique_ptr<AuctionMessage>>* messageQueue_;
    SignalQueue<std::unique_ptr<ListAuctionMessageResponse>>* responseQueue_;
};

#endif // AUCTION_HOUSE_WORKER_THREAD_H