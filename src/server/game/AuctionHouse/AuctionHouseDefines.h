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

#ifndef AUCTION_HOUSE_DEFINES_H
#define AUCTION_HOUSE_DEFINES_H

#include "Common.h"
#include "ObjectGuid.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define MIN_AUCTION_TIME (12*HOUR)
#define MAX_AUCTION_ITEMS 160
#define MAX_GETALL_RETURN 55000
#define MAX_AUCTIONS_PER_PAGE 50

struct AuctionHouseEntry;
struct ItemTemplate;
class Item;
class Player;
class WorldPacket;

enum AuctionError : uint8
{
    ERR_AUCTION_OK                  = 0,
    ERR_AUCTION_INVENTORY           = 1,
    ERR_AUCTION_DATABASE_ERROR      = 2,
    ERR_AUCTION_NOT_ENOUGHT_MONEY   = 3,
    ERR_AUCTION_ITEM_NOT_FOUND      = 4,
    ERR_AUCTION_HIGHER_BID          = 5,
    ERR_AUCTION_BID_INCREMENT       = 7,
    ERR_AUCTION_BID_OWN             = 10,
    ERR_AUCTION_RESTRICTED_ACCOUNT  = 13
};

enum AuctionAction : uint8
{
    AUCTION_SELL_ITEM   = 0,
    AUCTION_CANCEL      = 1,
    AUCTION_PLACE_BID   = 2
};

enum MailAuctionAnswers
{
    AUCTION_OUTBIDDED           = 0,
    AUCTION_WON                 = 1,
    AUCTION_SUCCESSFUL          = 2,
    AUCTION_EXPIRED             = 3,
    AUCTION_CANCELLED_TO_BIDDER = 4,
    AUCTION_CANCELED            = 5,
    AUCTION_SALE_PENDING        = 6
};

enum AuctionHouseId
{
    AUCTIONHOUSE_ALLIANCE       = 2,
    AUCTIONHOUSE_HORDE          = 6,
    AUCTIONHOUSE_NEUTRAL        = 7
};

enum AuctionEntryFlag : uint8
{
    AUCTION_ENTRY_FLAG_NONE         = 0x0,
    AUCTION_ENTRY_FLAG_GM_LOG_BUYER = 0x1  // write transaction to gm log file for buyer (optimization flag - avoids querying database for offline player permissions)
};

struct TC_GAME_API AuctionEntry
{
    uint32 Id;
    uint8 houseId;
    ObjectGuid::LowType itemGUIDLow;
    uint32 itemEntry;
    uint32 itemCount;
    ObjectGuid::LowType owner;
    uint32 startbid;                                        //maybe useless
    uint32 bid;
    uint32 buyout;
    time_t expire_time;
    ObjectGuid::LowType bidder;
    uint32 deposit;                                         //deposit can be calculated only when creating auction
    uint32 etime;
    std::unordered_set<ObjectGuid> bidders;
    AuctionHouseEntry const* auctionHouseEntry;             // in AuctionHouse.dbc
    AuctionEntryFlag Flags;

    // helpers
    uint32 GetAuctionCut() const;
    uint32 GetAuctionOutBid() const;
    void DeleteFromDB(CharacterDatabaseTransaction trans) const;
    void SaveToDB(CharacterDatabaseTransaction trans) const;
    void LoadFromDB(Field* fields);
    std::string BuildAuctionMailSubject(Item* item, MailAuctionAnswers response);
    static std::string BuildAuctionWonMailBody(ObjectGuid guid, uint32 bid, uint32 buyout);
    static std::string BuildAuctionSoldMailBody(ObjectGuid guid, uint32 bid, uint32 buyout, uint32 deposit, uint32 consignment);
    static std::string BuildAuctionInvoiceMailBody(ObjectGuid guid, uint32 bid, uint32 buyout, uint32 deposit, uint32 consignment, uint32 moneyDelay, uint32 eta);
    static uint32 CalculateAuctionOutBid(uint32 bid);
};

enum AuctionSortOrder
{
    AUCTION_SORT_MINLEVEL       = 0,
    AUCTION_SORT_RARITY         = 1,
    AUCTION_SORT_BUYOUT         = 2,
    AUCTION_SORT_TIMELEFT       = 3,
    AUCTION_SORT_UNK4           = 4,
    AUCTION_SORT_ITEM           = 5,
    AUCTION_SORT_MINBIDBUY      = 6,
    AUCTION_SORT_OWNER          = 7,
    AUCTION_SORT_BID            = 8,
    AUCTION_SORT_STACK          = 9,
    AUCTION_SORT_BUYOUT_2       = 10,
    AUCTION_SORT_MAX
};

struct AuctionEntryItemEnchants
{
    uint32 id;
    uint32 duration;
    uint32 charges;
};

struct SearchableAuctionEntryItem
{
    std::wstring itemName[TOTAL_LOCALES];
    uint32 entry;
    AuctionEntryItemEnchants enchants[MAX_INSPECTED_ENCHANTMENT_SLOT];
    int32 randomPropertyId;
    uint32 suffixFactor;
    uint32 count;
    int32 spellCharges;
    ItemTemplate const* itemTemplate;
};

struct SearchableAuctionEntry
{
    uint32 Id;
    uint8 houseId;
    ObjectGuid ownerGuid;
    std::string ownerName;
    uint32 buyout;
    time_t expire_time;
    uint32 startbid;
    uint32 bid;
    ObjectGuid bidderGuid;
    SearchableAuctionEntryItem item;

    void BuildAuctionInfo(WorldPacket& data) const;
    void SetItemNames();
    int CompareAuctionEntry(uint32 column, SearchableAuctionEntry const& auc, int loc_idx) const;
};

typedef std::unordered_map<uint32, uint16> AuctionPlayerSkills;
typedef std::unordered_set<uint32> AuctionPlayerSpells;

struct AuctionHouseUsablePlayerInfo
{
    uint32 classMask;
    uint32 raceMask;
    uint8 level;
    AuctionPlayerSkills skills;
    AuctionPlayerSpells spells;

    bool PlayerCanUseItem(ItemTemplate const* proto) const;
    uint16 GetSkillValue(uint32 skill) const;
    bool HasSpell(uint32 spell) const;
};

struct AuctionHousePlayerInfo
{
    ObjectGuid playerGuid;
    uint32 faction;
    int loc_idx;
    int locdbc_idx;
    std::optional<AuctionHouseUsablePlayerInfo> usablePlayerInfo;
};

struct AuctionSortInfo
{
    AuctionSortInfo() = default;
    AuctionSortOrder sortOrder{ AUCTION_SORT_MAX };
    bool isDesc{ true };
};

struct AuctionHouseSearchInfo
{
    std::wstring wsearchedname;
    uint32 listfrom;
    uint8 levelmin;
    uint8 levelmax;
    bool usable;
    uint32 inventoryType;
    uint32 itemClass;
    uint32 itemSubClass;
    uint32 quality;
    bool getAll;
    std::vector<AuctionSortInfo> sorting;
};

typedef std::map<uint32, AuctionEntry*> AuctionEntryMap;

class TC_GAME_API AuctionHouseObject
{
public:
    ~AuctionHouseObject()
    {
        for (AuctionEntryMap::iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end(); ++itr)
            delete itr->second;
    }
    uint32 Getcount() const { return AuctionsMap.size(); }
    AuctionEntryMap::iterator GetAuctionsBegin() { return AuctionsMap.begin(); }
    AuctionEntryMap::iterator GetAuctionsEnd() { return AuctionsMap.end(); }
    AuctionEntry* GetAuction(uint32 id) const
    {
        AuctionEntryMap::const_iterator itr = AuctionsMap.find(id);
        return itr != AuctionsMap.end() ? itr->second : nullptr;
    }
    void AddAuction(AuctionEntry* auction);
    bool RemoveAuction(AuctionEntry* auction);

private:
    AuctionEntryMap AuctionsMap;

};

typedef std::map<uint8, std::unique_ptr<AuctionHouseObject>> AuctionHouseMap;

#endif // AUCTION_HOUSE_DEFINES_H