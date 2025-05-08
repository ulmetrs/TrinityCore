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

#ifndef _AUCTION_HOUSE_MGR_H
#define _AUCTION_HOUSE_MGR_H

#include "AuctionHouseWorkerThread.h"
#include "Define.h"
#include "ObjectGuid.h"
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

class TC_GAME_API AuctionHouseMgr
{
public:
    typedef std::unordered_map<ObjectGuid::LowType, Item*> ItemMap;
    typedef std::vector<AuctionEntry*> PlayerAuctions;
    typedef std::pair<PlayerAuctions*, uint32> AuctionPair;

    static AuctionHouseMgr* instance();
    static uint32 GetAuctionDeposit(AuctionHouseEntry const* entry, uint32 time, Item* pItem, uint32 count);
    static AuctionHouseEntry const* GetAuctionHouseEntryByFactionTemplateId(uint32 factionTemplateId);
    static AuctionHouseEntry const* GetAuctionHouseEntry(uint8 houseId);
    static uint8 GetAuctionHouseId(uint32 factionTemplateId);

    AuctionHouseObject* GetAuctionHouseByFactionTemplateId(uint32 factionTemplateId);
    AuctionHouseObject* GetAuctionHouse(uint8 houseId);

    Item* GetAItem(ObjectGuid::LowType id)
    {
        ItemMap::const_iterator itr = mAitems.find(id);
        if (itr != mAitems.end())
            return itr->second;

        return nullptr;
    }

    //auction messages
    void SendAuctionWonMail(AuctionEntry* auction, CharacterDatabaseTransaction trans);
    void SendAuctionSalePendingMail(AuctionEntry* auction, CharacterDatabaseTransaction trans);
    void SendAuctionSuccessfulMail(AuctionEntry* auction, CharacterDatabaseTransaction trans);
    void SendAuctionExpiredMail(AuctionEntry* auction, CharacterDatabaseTransaction trans);
    void SendAuctionOutbiddedMail(AuctionEntry* auction, uint32 newPrice, Player* newBidder, CharacterDatabaseTransaction trans);
    void SendAuctionCancelledToBidderMail(AuctionEntry* auction, CharacterDatabaseTransaction trans);

    //load first auction items, because of check if item exists, when loading
    void LoadAuctionItems();
    void LoadAuctions();
    void AddAItem(Item* it);
    bool RemoveAItem(ObjectGuid::LowType id, bool deleteItem = false, CharacterDatabaseTransaction* trans = nullptr);
    void AddAuction(AuctionEntry* auctionEntry);
    bool RemoveAuction(AuctionEntry* auctionEntry);
    void UpdateBid(AuctionEntry* auctionEntry);
    void QueueAuctionMessage(std::unique_ptr<AuctionMessage> message);
    void UpdateExpiredAuctions();
    bool PendingAuctionAdd(Player* player, AuctionEntry* aEntry);
    uint32 PendingAuctionCount(Player const* player) const;
    void PendingAuctionProcess(Player* player);
    void UpdatePendingAuctions();

private:
    AuctionHouseMgr();
    ~AuctionHouseMgr()
    {
        for (ItemMap::iterator itr = mAitems.begin(); itr != mAitems.end(); ++itr)
            delete itr->second;

        messageQueue_.close();
    }

    AuctionHouseMap auctionHouseMap_;
    std::map<ObjectGuid, AuctionPair> pendingAuctionMap;

    ItemMap mAitems;

    SignalQueue<std::unique_ptr<AuctionMessage>> messageQueue_;
    std::vector<std::unique_ptr<AuctionHouseWorkerThread>> workerThreads_;
};

#define sAuctionMgr AuctionHouseMgr::instance()

#endif