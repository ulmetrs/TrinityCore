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

#include "AuctionHouseMgr.h"
#include "AuctionHouseSearcher.h"
#include "AuctionHouseWorkerThread.h"
#include "CharacterCache.h"
#include "Item.h"
#include "ObjectAccessor.h"
#include "World.h"

AuctionHouseSearcher::AuctionHouseSearcher() {
    for (uint32 i = 0; i < sWorld->getIntConfig(CONFIG_AUCTIONHOUSE_WORKERTHREADS); ++i) {
        workerThreads_.push_back(std::make_unique<AuctionHouseWorkerThread>(&requestQueue_, &responseQueue_));
    }
}

AuctionHouseSearcher::~AuctionHouseSearcher() {
    requestQueue_.close();
    responseQueue_.close();
}

void AuctionHouseSearcher::Update() {
    while (auto response = responseQueue_.try_receive()) {
        if (Player* player = ObjectAccessor::FindConnectedPlayer((*response)->playerGuid)) {
            player->GetSession()->SendPacket(&(*response)->packet);
        }
    }
}

void AuctionHouseSearcher::QueueSearchRequest(std::unique_ptr<AuctionSearcherRequest> searchRequestInfo) {
    requestQueue_.send(std::move(searchRequestInfo));
}

void AuctionHouseSearcher::AddAuction(AuctionEntry const* auctionEntry)
{
    Item* item = sAuctionMgr->GetAItem(auctionEntry->itemGUIDLow);
    if (!item)
        return;

    // SearchableAuctionEntry is a shared_ptr as it will be shared among all the worker threads and needs to be self-managed
    std::shared_ptr<SearchableAuctionEntry> searchableAuctionEntry = std::make_shared<SearchableAuctionEntry>();
    searchableAuctionEntry->Id = auctionEntry->Id;
    
    // Auction info
    ObjectGuid ownerGuid = ObjectGuid(HighGuid::Player, auctionEntry->owner);
    searchableAuctionEntry->ownerGuid = ownerGuid;
    sCharacterCache->GetCharacterNameByGuid(ownerGuid, searchableAuctionEntry->ownerName);
    searchableAuctionEntry->startbid = auctionEntry->startbid;
    searchableAuctionEntry->buyout = auctionEntry->buyout;
    searchableAuctionEntry->expire_time = auctionEntry->expire_time;
    searchableAuctionEntry->listFaction = auctionEntry->GetFactionId();
    searchableAuctionEntry->bid = auctionEntry->bid;
    ObjectGuid bidderGuid = ObjectGuid(HighGuid::Player, auctionEntry->bidder);
    searchableAuctionEntry->bidderGuid = bidderGuid;

    // Item info
    searchableAuctionEntry->item.entry = item->GetEntry();
    for (uint8 i = 0; i < MAX_INSPECTED_ENCHANTMENT_SLOT; ++i)
    {
        searchableAuctionEntry->item.enchants[i].id = item->GetEnchantmentId(EnchantmentSlot(i));
        searchableAuctionEntry->item.enchants[i].duration = item->GetEnchantmentDuration(EnchantmentSlot(i));
        searchableAuctionEntry->item.enchants[i].charges = item->GetEnchantmentCharges(EnchantmentSlot(i));
    }

    searchableAuctionEntry->item.randomPropertyId = item->GetItemRandomPropertyId();
    searchableAuctionEntry->item.suffixFactor = item->GetItemSuffixFactor();
    searchableAuctionEntry->item.count = item->GetCount();
    searchableAuctionEntry->item.spellCharges = item->GetSpellCharges();
    searchableAuctionEntry->item.itemTemplate = item->GetTemplate();

    searchableAuctionEntry->SetItemNames();

    NotifyAllWorkers(std::make_shared<AuctionSearchAdd>(searchableAuctionEntry));
}

void AuctionHouseSearcher::RemoveAuction(AuctionEntry const* auctionEntry)
{
    NotifyAllWorkers(std::make_shared<AuctionSearchRemove>(auctionEntry->Id, auctionEntry->GetFactionId()));
}

void AuctionHouseSearcher::UpdateBid(AuctionEntry const* auctionEntry)
{
    // Updating bids is a bit unique, we really only need to update a single worker as every worker thread contains
    // a map of shared pointers to the same SearchableAuctionEntry's, so updating one will update them all.
    ObjectGuid bidderGuid = ObjectGuid(HighGuid::Player, auctionEntry->bidder);
    NotifyOneWorker(std::make_shared<AuctionSearchUpdateBid>(auctionEntry->Id, auctionEntry->GetFactionId(), auctionEntry->bid, bidderGuid));
}

void AuctionHouseSearcher::NotifyAllWorkers(std::shared_ptr<AuctionSearcherUpdate> const update) {
    for (auto const& worker : workerThreads_) {
        worker->AddAuctionSearchUpdateToQueue(update);
    }
}

void AuctionHouseSearcher::NotifyOneWorker(std::shared_ptr<AuctionSearcherUpdate> const update) {
    workerThreads_.front()->AddAuctionSearchUpdateToQueue(update);
}