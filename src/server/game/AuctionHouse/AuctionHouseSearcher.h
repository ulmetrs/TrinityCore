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

#ifndef AUCTION_HOUSE_SEARCHER_H
#define AUCTION_HOUSE_SEARCHER_H

#include "SignalQueue.h"
#include "AuctionHouseCommon.h"
#include <memory>
#include <vector>

class AuctionHouseWorkerThread;

class AuctionHouseSearcher
{
public:
    AuctionHouseSearcher();
    ~AuctionHouseSearcher();

    void Update();
    void QueueSearchRequest(std::unique_ptr<AuctionSearcherRequest> searchRequestInfo);
    void AddAuction(AuctionEntry const* auctionEntry);
    void RemoveAuction(AuctionEntry const* auctionEntry);
    void UpdateBid(AuctionEntry const* auctionEntry);

    void NotifyAllWorkers(std::shared_ptr<AuctionSearcherUpdate> const update);
    void NotifyOneWorker(std::shared_ptr<AuctionSearcherUpdate> const update);

private:
    SignalQueue<std::unique_ptr<AuctionSearcherRequest>> requestQueue_;
    SignalQueue<std::unique_ptr<AuctionSearcherResponse>> responseQueue_;
    std::vector<std::unique_ptr<AuctionHouseWorkerThread>> workerThreads_;
};

#endif // AUCTION_HOUSE_SEARCHER_H