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
#include "AuctionHouseCommon.h"
#include "AuctionHouseWorkerThread.h"
#include "AuctionHouseBot.h"
#include "AccountMgr.h"
#include "Bag.h"
#include "Common.h"
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Item.h"
#include "Language.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldSession.h"
#include "WowTime.h"

enum eAuctionHouse
{
    AH_MINIMUM_DEPOSIT = 100
};

AuctionHouseMgr::AuctionHouseMgr()
{
    auctionHouseMap_[AUCTIONHOUSE_ALLIANCE] = std::make_unique<AuctionHouseObject>();
    auctionHouseMap_[AUCTIONHOUSE_HORDE]    = std::make_unique<AuctionHouseObject>();
    auctionHouseMap_[AUCTIONHOUSE_NEUTRAL]  = std::make_unique<AuctionHouseObject>();

    for (uint32 i = 0; i < sWorld->getIntConfig(CONFIG_AUCTION_WORKER_THREADS); ++i)
    {
        workerThreads_.push_back(std::make_unique<AuctionHouseWorkerThread>(&messageQueue_, auctionHouseMap_));
    }
}

AuctionHouseMgr* AuctionHouseMgr::instance()
{
    static AuctionHouseMgr instance;
    return &instance;
}

AuctionHouseObject* AuctionHouseMgr::GetAuctionHouseByFactionTemplateId(uint32 factionTemplateId)
{
    uint8 houseId = GetAuctionHouseId(factionTemplateId);
    return GetAuctionHouse(houseId);
}

AuctionHouseObject* AuctionHouseMgr::GetAuctionHouse(uint8 houseId)
{
    auto it = auctionHouseMap_.find(houseId);
    if (it != auctionHouseMap_.end())
        return it->second.get();
    return auctionHouseMap_[AUCTIONHOUSE_NEUTRAL].get();
}

uint32 AuctionHouseMgr::GetAuctionDeposit(AuctionHouseEntry const* entry, uint32 time, Item* pItem, uint32 count)
{
    uint32 MSV = pItem->GetTemplate()->SellPrice;

    if (MSV <= 0)
        return float(AH_MINIMUM_DEPOSIT) * sWorld->getRate(RATE_AUCTION_DEPOSIT);

    float multiplier = CalculatePct(float(entry->DepositRate), 3);
    uint32 timeHr = (((time / 60) / 60) / 12);
    uint32 deposit = uint32(MSV * multiplier * sWorld->getRate(RATE_AUCTION_DEPOSIT));
    float remainderbase = float(MSV * multiplier * sWorld->getRate(RATE_AUCTION_DEPOSIT)) - deposit;

    deposit *= timeHr * count;

    int i = count;
    while (i > 0 && (remainderbase * i) != uint32(remainderbase * i))
        i--;

    if (i)
        deposit += remainderbase * i * timeHr;

    TC_LOG_DEBUG("auctionHouse", "MSV:        {}", MSV);
    TC_LOG_DEBUG("auctionHouse", "Items:      {}", count);
    TC_LOG_DEBUG("auctionHouse", "Multiplier: {}", multiplier);
    TC_LOG_DEBUG("auctionHouse", "Deposit:    {}", deposit);
    TC_LOG_DEBUG("auctionHouse", "Deposit rm: {}", remainderbase * count);

    if (deposit < float(AH_MINIMUM_DEPOSIT) * sWorld->getRate(RATE_AUCTION_DEPOSIT))
        return float(AH_MINIMUM_DEPOSIT) * sWorld->getRate(RATE_AUCTION_DEPOSIT);
    else
        return deposit;
}

//does not clear ram
void AuctionHouseMgr::SendAuctionWonMail(AuctionEntry* auction, CharacterDatabaseTransaction trans)
{
    Item* pItem = GetAItem(auction->itemGUIDLow);
    if (!pItem)
        return;

    uint32 bidderAccId = 0;
    ObjectGuid bidderGuid(HighGuid::Player, auction->bidder);
    Player* bidder = ObjectAccessor::FindConnectedPlayer(bidderGuid);
    // data for gm.log
    std::string bidderName;
    bool logGmTrade = (auction->Flags & AUCTION_ENTRY_FLAG_GM_LOG_BUYER) != AUCTION_ENTRY_FLAG_NONE;

    if (bidder)
    {
        bidderAccId = bidder->GetSession()->GetAccountId();
        bidderName = bidder->GetName();
    }
    else
    {
        bidderAccId = sCharacterCache->GetCharacterAccountIdByGuid(bidderGuid);

        if (logGmTrade && !sCharacterCache->GetCharacterNameByGuid(bidderGuid, bidderName))
            bidderName = sObjectMgr->GetTrinityStringForDBCLocale(LANG_UNKNOWN);
    }

    if (logGmTrade)
    {
        ObjectGuid ownerGuid = ObjectGuid(HighGuid::Player, auction->owner);
        std::string ownerName;
        if (!sCharacterCache->GetCharacterNameByGuid(ownerGuid, ownerName))
            ownerName = sObjectMgr->GetTrinityStringForDBCLocale(LANG_UNKNOWN);

        uint32 ownerAccId = sCharacterCache->GetCharacterAccountIdByGuid(ownerGuid);

        sLog->OutCommand(bidderAccId, "GM {} (Account: {}) won item in auction: {} (Entry: {} Count: {}) and pay money: {}. Original owner {} (Account: {})",
            bidderName, bidderAccId, pItem->GetTemplate()->Name1, pItem->GetEntry(), pItem->GetCount(), auction->bid, ownerName, ownerAccId);
    }

    // receiver exist
    if ((bidder || bidderAccId) && !sAuctionBotConfig->IsBotChar(auction->bidder))
    {
        // set owner to bidder (to prevent delete item with sender char deleting)
        // owner in `data` will set at mail receive and item extracting
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_OWNER);
        stmt->setUInt32(0, auction->bidder);
        stmt->setUInt32(1, pItem->GetGUID().GetCounter());
        trans->Append(stmt);

        if (bidder)
        {
            bidder->GetSession()->SendAuctionBidderNotification(auction->houseId, auction->Id, bidderGuid, 0, 0, auction->itemEntry);
            // FIXME: for offline player need also
            bidder->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_WON_AUCTIONS, 1);
        }

        MailDraft(auction->BuildAuctionMailSubject(pItem, AUCTION_WON), AuctionEntry::BuildAuctionWonMailBody(ObjectGuid::Create<HighGuid::Player>(auction->owner), auction->bid, auction->buyout))
            .AddItem(pItem)
            .SendMailTo(trans, MailReceiver(bidder, auction->bidder), auction, MAIL_CHECK_MASK_COPIED);
    }
    else
    {
        // bidder doesn't exist, delete the item
        RemoveAItem(auction->itemGUIDLow, true, &trans);
    }
}

void AuctionHouseMgr::SendAuctionSalePendingMail(AuctionEntry* auction, CharacterDatabaseTransaction trans)
{
    Item* pItem = GetAItem(auction->itemGUIDLow);
    if (!pItem)
        return;

    ObjectGuid owner_guid(HighGuid::Player, auction->owner);
    Player* owner = ObjectAccessor::FindConnectedPlayer(owner_guid);
    uint32 owner_accId = sCharacterCache->GetCharacterAccountIdByGuid(owner_guid);
    // owner exist (online or offline)
    if ((owner || owner_accId) && !sAuctionBotConfig->IsBotChar(auction->owner))
    {
        WowTime eta = *GameTime::GetUtcWowTime();
        eta += Seconds(sWorld->getIntConfig(CONFIG_MAIL_DELIVERY_DELAY));
        if (owner)
            eta += owner->GetSession()->GetTimezoneOffset();

        MailDraft(auction->BuildAuctionMailSubject(pItem, AUCTION_SALE_PENDING),
            AuctionEntry::BuildAuctionInvoiceMailBody(ObjectGuid::Create<HighGuid::Player>(auction->bidder), auction->bid, auction->buyout, auction->deposit,
            auction->GetAuctionCut(), sWorld->getIntConfig(CONFIG_MAIL_DELIVERY_DELAY), eta.GetPackedTime()))
            .SendMailTo(trans, MailReceiver(owner, auction->owner), auction, MAIL_CHECK_MASK_COPIED);
    }
}

//call this method to send mail to auction owner, when auction is successful, it does not clear ram
void AuctionHouseMgr::SendAuctionSuccessfulMail(AuctionEntry* auction, CharacterDatabaseTransaction trans)
{
    Item* pItem = GetAItem(auction->itemGUIDLow);
    if (!pItem)
        return;

    ObjectGuid owner_guid(HighGuid::Player, auction->owner);
    Player* owner = ObjectAccessor::FindConnectedPlayer(owner_guid);
    uint32 owner_accId = sCharacterCache->GetCharacterAccountIdByGuid(owner_guid);
    // owner exist
    if ((owner || owner_accId) && !sAuctionBotConfig->IsBotChar(auction->owner))
    {
        uint32 profit = auction->bid + auction->deposit - auction->GetAuctionCut();

        //FIXME: what do if owner offline
        if (owner)
        {
            owner->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_EARNED_BY_AUCTIONS, profit);
            owner->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_AUCTION_SOLD, auction->bid);
            //send auction owner notification, bidder must be current!
            owner->GetSession()->SendAuctionOwnerNotification(auction);
        }

        MailDraft(auction->BuildAuctionMailSubject(pItem, AUCTION_SUCCESSFUL),
            AuctionEntry::BuildAuctionSoldMailBody(ObjectGuid::Create<HighGuid::Player>(auction->bidder), auction->bid, auction->buyout, auction->deposit, auction->GetAuctionCut()))
            .AddMoney(profit)
            .SendMailTo(trans, MailReceiver(owner, auction->owner), auction, MAIL_CHECK_MASK_COPIED, sWorld->getIntConfig(CONFIG_MAIL_DELIVERY_DELAY));
    }
}

//does not clear ram
void AuctionHouseMgr::SendAuctionExpiredMail(AuctionEntry* auction, CharacterDatabaseTransaction trans)
{
    //return an item in auction to its owner by mail
    Item* pItem = GetAItem(auction->itemGUIDLow);
    if (!pItem)
        return;

    ObjectGuid owner_guid(HighGuid::Player, auction->owner);
    Player* owner = ObjectAccessor::FindConnectedPlayer(owner_guid);
    uint32 owner_accId = sCharacterCache->GetCharacterAccountIdByGuid(owner_guid);
    // owner exist
    if ((owner || owner_accId) && !sAuctionBotConfig->IsBotChar(auction->owner))
    {
        if (owner)
            owner->GetSession()->SendAuctionOwnerNotification(auction);

        MailDraft(auction->BuildAuctionMailSubject(pItem, AUCTION_EXPIRED), "")
            .AddItem(pItem)
            .SendMailTo(trans, MailReceiver(owner, auction->owner), auction, MAIL_CHECK_MASK_COPIED, 0);
    }
    else
    {
        // owner doesn't exist, delete the item
        RemoveAItem(auction->itemGUIDLow, true, &trans);
    }
}

//this function sends mail to old bidder
void AuctionHouseMgr::SendAuctionOutbiddedMail(AuctionEntry* auction, uint32 newPrice, Player* newBidder, CharacterDatabaseTransaction trans)
{
    Item* pItem = GetAItem(auction->itemGUIDLow);
    if (!pItem)
        return;

    ObjectGuid oldBidder_guid(HighGuid::Player, auction->bidder);
    Player* oldBidder = ObjectAccessor::FindConnectedPlayer(oldBidder_guid);

    uint32 oldBidder_accId = 0;
    if (!oldBidder)
        oldBidder_accId = sCharacterCache->GetCharacterAccountIdByGuid(oldBidder_guid);

    // old bidder exist
    if ((oldBidder || oldBidder_accId) && !sAuctionBotConfig->IsBotChar(auction->bidder))
    {
        if (oldBidder && newBidder)
            oldBidder->GetSession()->SendAuctionBidderNotification(auction->houseId, auction->Id, newBidder->GetGUID(), newPrice, auction->GetAuctionOutBid(), auction->itemEntry);

        MailDraft(auction->BuildAuctionMailSubject(pItem, AUCTION_OUTBIDDED), "")
            .AddMoney(auction->bid)
            .SendMailTo(trans, MailReceiver(oldBidder, auction->bidder), auction, MAIL_CHECK_MASK_COPIED);
    }
}

//this function sends mail, when auction is cancelled to old bidder
void AuctionHouseMgr::SendAuctionCancelledToBidderMail(AuctionEntry* auction, CharacterDatabaseTransaction trans)
{
    Item* pItem = GetAItem(auction->itemGUIDLow);
    if (!pItem)
        return;

    ObjectGuid bidder_guid = ObjectGuid(HighGuid::Player, auction->bidder);
    Player* bidder = ObjectAccessor::FindConnectedPlayer(bidder_guid);

    uint32 bidder_accId = 0;
    if (!bidder)
        bidder_accId = sCharacterCache->GetCharacterAccountIdByGuid(bidder_guid);

    // bidder exist
    if ((bidder || bidder_accId) && !sAuctionBotConfig->IsBotChar(auction->bidder))
        MailDraft(auction->BuildAuctionMailSubject(pItem, AUCTION_CANCELLED_TO_BIDDER), "")
            .AddMoney(auction->bid)
            .SendMailTo(trans, MailReceiver(bidder, auction->bidder), auction, MAIL_CHECK_MASK_COPIED);
}

void AuctionHouseMgr::LoadAuctionItems()
{
    uint32 oldMSTime = getMSTime();

    // need to clear in case we are reloading
    if (!mAitems.empty())
    {
        for (ItemMap::iterator itr = mAitems.begin(); itr != mAitems.end(); ++itr)
            delete itr->second;

        mAitems.clear();
    }

    // data needs to be at first place for Item::LoadFromDB
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_AUCTION_ITEMS);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 auction items. DB table `auctionhouse` or `item_instance` is empty!");

        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        ObjectGuid::LowType item_guid = fields[11].GetUInt32();
        uint32 itemEntry    = fields[12].GetUInt32();

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
        if (!proto)
        {
            TC_LOG_ERROR("misc", "AuctionHouseMgr::LoadAuctionItems: Unknown item (GUID: {} item entry: #{}) in auction, skipped.", item_guid, itemEntry);
            continue;
        }

        Item* item = NewItemOrBag(proto);
        if (!item->LoadFromDB(item_guid, ObjectGuid::Empty, fields, itemEntry))
        {
            delete item;
            continue;
        }
        AddAItem(item);

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} auction items in {} ms", count, GetMSTimeDiffToNow(oldMSTime));

}

void AuctionHouseMgr::LoadAuctions()
{
    uint32 oldMSTime = getMSTime();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_AUCTIONS);
    PreparedQueryResult resultAuctions = CharacterDatabase.Query(stmt);

    if (!resultAuctions)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 auctions. DB table `auctionhouse` is empty.");

        return;
    }

    // parse bidder list
    std::unordered_map<uint32, std::unordered_set<ObjectGuid>> biddersByAuction;
    CharacterDatabasePreparedStatement* stmt2 = CharacterDatabase.GetPreparedStatement(CHAR_SEL_AUCTION_BIDDERS);

    uint32 countBidders = 0;
    if (PreparedQueryResult resultBidders = CharacterDatabase.Query(stmt2))
    {
        do
        {
            Field* fields = resultBidders->Fetch();
            biddersByAuction[fields[0].GetUInt32()].insert(ObjectGuid::Create<HighGuid::Player>(fields[1].GetUInt32()));
            ++countBidders;
        }
        while (resultBidders->NextRow());
    }

    // parse auctions from db
    uint32 countAuctions = 0;
    bool moveToNeutralAH = sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_AUCTION);
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    do
    {
        Field* fields = resultAuctions->Fetch();
        AuctionEntry* aItem = new AuctionEntry();
        aItem->LoadFromDB(fields);

        if (moveToNeutralAH)
        {
            aItem->houseId = AUCTIONHOUSE_NEUTRAL;
            aItem->auctionHouseEntry = AuctionHouseMgr::GetAuctionHouseEntry(aItem->houseId);
        }

        if (!AuctionHouseMgr::GetAuctionHouseEntry(aItem->houseId))
        {
            TC_LOG_ERROR("misc", "Auction {} has invalid house id {}", aItem->Id, aItem->houseId);
            aItem->DeleteFromDB(trans);
            delete aItem;
            continue;
        }

        // check if sold item exists for guid
        // and itemEntry in fact (GetAItem will fail if problematic in result check in AuctionHouseMgr::LoadAuctionItems)
        if (!GetAItem(aItem->itemGUIDLow))
        {
            TC_LOG_ERROR("misc", "Auction {} has not a existing item : {}", aItem->Id, aItem->itemGUIDLow);
            aItem->DeleteFromDB(trans);
            delete aItem;
            continue;
        }

        auto it = biddersByAuction.find(aItem->Id);
        if (it != biddersByAuction.end())
            aItem->bidders = std::move(it->second);

        AddAuction(aItem);
        ++countAuctions;
    } while (resultAuctions->NextRow());

    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_INFO("server.loading", ">> Loaded {} auctions with {} bidders in {} ms", countAuctions, countBidders, GetMSTimeDiffToNow(oldMSTime));
}

void AuctionHouseMgr::AddAItem(Item* it)
{
    ASSERT(it);
    ASSERT(mAitems.find(it->GetGUID().GetCounter()) == mAitems.end());
    mAitems[it->GetGUID().GetCounter()] = it;
}

bool AuctionHouseMgr::RemoveAItem(ObjectGuid::LowType id, bool deleteItem /*= false*/, CharacterDatabaseTransaction* trans /*= nullptr*/)
{
    ItemMap::iterator i = mAitems.find(id);
    if (i == mAitems.end())
        return false;

    if (deleteItem)
    {
        ASSERT(trans);
        i->second->FSetState(ITEM_REMOVED);
        i->second->SaveToDB(*trans);
    }

    mAitems.erase(i);
    return true;
}

void AuctionHouseMgr::AddAuction(AuctionEntry* auction)
{
    ASSERT(auction);

    Item* item = GetAItem(auction->itemGUIDLow);
    ASSERT(item);

    // Add the auction to the correct auction house synchronously
    AuctionHouseObject* auctionHouse = GetAuctionHouse(auction->houseId);
    auctionHouse->AddAuction(auction);
    sScriptMgr->OnAuctionAdd(auctionHouse, auction);

    // SearchableAuctionEntry is a shared_ptr as it will be shared among all the worker threads and needs to be self-managed
    std::shared_ptr<SearchableAuctionEntry> searchableAuctionEntry = std::make_shared<SearchableAuctionEntry>();
    searchableAuctionEntry->Id = auction->Id;
    searchableAuctionEntry->houseId = auction->houseId;

    // Auction info
    ObjectGuid ownerGuid = ObjectGuid(HighGuid::Player, auction->owner);
    searchableAuctionEntry->ownerGuid = ownerGuid;
    sCharacterCache->GetCharacterNameByGuid(ownerGuid, searchableAuctionEntry->ownerName);
    searchableAuctionEntry->startbid = auction->startbid;
    searchableAuctionEntry->buyout = auction->buyout;
    searchableAuctionEntry->expire_time = auction->expire_time;
    searchableAuctionEntry->bid = auction->bid;
    ObjectGuid bidderGuid = ObjectGuid(HighGuid::Player, auction->bidder);
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

    // Queue the searchable auction entry to be added asynchronously
    auto message = std::make_unique<AddAuctionMessage>(searchableAuctionEntry);
    messageQueue_.send(std::move(message));
}

bool AuctionHouseMgr::RemoveAuction(AuctionEntry* auction)
{
    AuctionHouseObject* auctionHouse = GetAuctionHouse(auction->houseId);
    bool wasInMap = auctionHouse->RemoveAuction(auction);
    sScriptMgr->OnAuctionRemove(auctionHouse, auction);

    // Queue the searchable auction entry to be removed asynchronously
    auto message = std::make_unique<RemoveAuctionMessage>(auction->Id, auction->houseId);
    messageQueue_.send(std::move(message));

    // we need to delete the entry, it is not referenced any more
    delete auction;
    return wasInMap;
}

void AuctionHouseMgr::UpdateBid(AuctionEntry* auction)
{
    // Queue the searchable auction entry to be removed asynchronously Note: the synchronous bid update is done in the handler
    ObjectGuid bidderGuid = ObjectGuid(HighGuid::Player, auction->bidder);
    auto message = std::make_unique<UpdateAuctionBidMessage>(auction->Id, auction->houseId, auction->bid, bidderGuid);
    messageQueue_.send(std::move(message));
}

void AuctionHouseMgr::QueueAuctionMessage(std::unique_ptr<AuctionMessage> message)
{
    messageQueue_.send(std::move(message));
}

void AuctionHouseMgr::UpdateExpiredAuctions()
{
    for (auto& pair : auctionHouseMap_)
    {
        AuctionHouseObject* auctionHouse = pair.second.get();

        // If storage is empty, no need to update. next == NULL in this case
        if (!auctionHouse || auctionHouse->Getcount() == 0)
            continue;

        time_t curTime = GameTime::GetGameTime();

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        for (AuctionEntryMap::iterator itr = auctionHouse->GetAuctionsBegin(); itr != auctionHouse->GetAuctionsEnd();)
        {
            // from auctionhousehandler.cpp, creates auction pointer & player pointer
            AuctionEntry* auction = itr->second;
            // Increment iterator due to AuctionEntry deletion
            ++itr;

            ///- filter auctions expired on next update
            if (auction->expire_time > curTime + 60)
                continue;

            ///- Either cancel the auction if there was no bidder
            if (auction->bidder == 0 && auction->bid == 0)
            {
                SendAuctionExpiredMail(auction, trans);
                sScriptMgr->OnAuctionExpire(auctionHouse, auction);
            }
            ///- Or perform the transaction
            else
            {
                //we should send an "item sold" message if the seller is online
                //we send the item to the winner
                //we send the money to the seller
                SendAuctionSuccessfulMail(auction, trans);
                SendAuctionWonMail(auction, trans);
                sScriptMgr->OnAuctionSuccessful(auctionHouse, auction);
            }

            ///- In any case clear the auction
            auction->DeleteFromDB(trans);

            RemoveAItem(auction->itemGUIDLow);
            RemoveAuction(auction);
        }

        // Run DB changes
        CharacterDatabase.CommitTransaction(trans);
    }
}

bool AuctionHouseMgr::PendingAuctionAdd(Player* player, AuctionEntry* aEntry)
{
    PlayerAuctions* thisAH;
    auto itr = pendingAuctionMap.find(player->GetGUID());
    if (itr != pendingAuctionMap.end())
    {
        thisAH = itr->second.first;

        // Get deposit so far
        uint32 totalDeposit = 0;
        for (AuctionEntry const* thisAuction : *thisAH)
            totalDeposit += thisAuction->deposit;

        // Add this deposit
        totalDeposit += aEntry->deposit;

        if (!player->HasEnoughMoney(totalDeposit))
            return false;
    }
    else
    {
        thisAH = new PlayerAuctions;
        pendingAuctionMap[player->GetGUID()] = AuctionPair(thisAH, 0);
    }
    thisAH->push_back(aEntry);
    return true;
}

uint32 AuctionHouseMgr::PendingAuctionCount(Player const* player) const
{
    auto const itr = pendingAuctionMap.find(player->GetGUID());
    if (itr != pendingAuctionMap.end())
        return itr->second.first->size();

    return 0;
}

void AuctionHouseMgr::PendingAuctionProcess(Player* player)
{
    auto iterMap = pendingAuctionMap.find(player->GetGUID());
    if (iterMap == pendingAuctionMap.end())
        return;

    PlayerAuctions* thisAH = iterMap->second.first;

    uint32 totaldeposit = 0;
    auto itrAH = thisAH->begin();
    for (; itrAH != thisAH->end(); ++itrAH)
    {
        AuctionEntry* AH = (*itrAH);
        if (!player->HasEnoughMoney(totaldeposit + AH->deposit))
            break;

        totaldeposit += AH->deposit;
    }

    // expire auctions we cannot afford
    if (itrAH != thisAH->end())
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        do
        {
            AuctionEntry* AH = (*itrAH);
            AH->expire_time = GameTime::GetGameTime();
            AH->DeleteFromDB(trans);
            AH->SaveToDB(trans);
            ++itrAH;
        } while (itrAH != thisAH->end());

        CharacterDatabase.CommitTransaction(trans);
    }

    pendingAuctionMap.erase(player->GetGUID());
    delete thisAH;
    player->ModifyMoney(-int32(totaldeposit));
}

void AuctionHouseMgr::UpdatePendingAuctions()
{
    for (auto itr = pendingAuctionMap.begin(); itr != pendingAuctionMap.end();)
    {
        ObjectGuid playerGUID = itr->first;
        if (Player* player = ObjectAccessor::FindConnectedPlayer(playerGUID))
        {
            // Check if there were auctions since last update process if not
            if (PendingAuctionCount(player) == itr->second.second)
            {
                ++itr;
                PendingAuctionProcess(player);
            }
            else
            {
                ++itr;
                pendingAuctionMap[playerGUID].second = PendingAuctionCount(player);
            }
        }
        else
        {
            // Expire any auctions that we couldn't get a deposit for
            TC_LOG_WARN("auctionHouse", "Player {} was offline, unable to retrieve deposit!", playerGUID.ToString());
            PlayerAuctions* thisAH = itr->second.first;
            ++itr;
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            for (auto AHitr = thisAH->begin(); AHitr != thisAH->end();)
            {
                AuctionEntry* AH = (*AHitr);
                ++AHitr;
                AH->expire_time = GameTime::GetGameTime();
                AH->DeleteFromDB(trans);
                AH->SaveToDB(trans);
            }
            CharacterDatabase.CommitTransaction(trans);
            pendingAuctionMap.erase(playerGUID);
            delete thisAH;
        }
    }
}

AuctionHouseEntry const* AuctionHouseMgr::GetAuctionHouseEntryByFactionTemplateId(uint32 factionTemplateId)
{
    uint8 houseId = GetAuctionHouseId(factionTemplateId);
    return GetAuctionHouseEntry(houseId);
}

AuctionHouseEntry const* AuctionHouseMgr::GetAuctionHouseEntry(uint8 houseId)
{
    return sAuctionHouseStore.LookupEntry(uint32(houseId));
}

uint8 AuctionHouseMgr::GetAuctionHouseId(uint32 factionTemplateId)
{
    if (sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_AUCTION))
        return AUCTIONHOUSE_ALLIANCE; // goblin auction house

    // FIXME: found way for proper auctionhouse selection by another way
    // AuctionHouse.dbc have faction field with _player_ factions associated with auction house races.
    // but no easy way convert creature faction to player race faction for specific city
    FactionTemplateEntry const* u_entry = sFactionTemplateStore.LookupEntry(factionTemplateId);
    if (!u_entry)
        return AUCTIONHOUSE_NEUTRAL; // goblin auction house
    else if (u_entry->FactionGroup & FACTION_MASK_ALLIANCE)
        return AUCTIONHOUSE_ALLIANCE; // human auction house
    else if (u_entry->FactionGroup & FACTION_MASK_HORDE)
        return AUCTIONHOUSE_HORDE; // orc auction house
    else
        return AUCTIONHOUSE_NEUTRAL; // goblin auction house
}