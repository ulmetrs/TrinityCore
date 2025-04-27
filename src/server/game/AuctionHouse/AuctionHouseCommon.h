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

#ifndef AUCTION_HOUSE_COMMON_H
#define AUCTION_HOUSE_COMMON_H

#include "Common.h"
#include "ObjectGuid.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ItemTemplate;
class WorldPacket;

enum AuctionHouseFaction : uint8
{
    AUCTION_FACTION_ALLIANCE = 0,
    AUCTION_FACTION_HORDE    = 1,
    AUCTION_FACTION_NEUTRAL  = 2,
    AUCTION_FACTION_MAX
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
    ObjectGuid ownerGuid;
    std::string ownerName;
    uint32 buyout;
    time_t expire_time;
    uint32 startbid;
    uint32 bid;
    ObjectGuid bidderGuid;
    AuctionHouseFaction listFaction;
    SearchableAuctionEntryItem item;

    void BuildAuctionInfo(WorldPacket& data) const;
    void SetItemNames();
    int CompareAuctionEntry(uint32 column, SearchableAuctionEntry const& auc, int loc_idx) const;
};

typedef std::unordered_map<uint32, uint16> AuctionPlayerSkills;
typedef std::unordered_set<uint32> AuctionPlayerSpells;

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

struct AuctionSortInfo
{
    AuctionSortInfo() = default;
    AuctionSortOrder sortOrder{ AUCTION_SORT_MAX };
    bool isDesc{ true };
};

typedef std::vector<AuctionSortInfo> AuctionSortOrderVector;

struct AuctionHousePlayerInfo
{
    ObjectGuid playerGuid;
    uint32 faction;
    int loc_idx;
    int locdbc_idx;
    std::optional<AuctionHouseUsablePlayerInfo> usablePlayerInfo;
};

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

struct AuctionSearcherRequest
{
    enum class Type : uint8
    {
        LIST,
        OWNER_LIST,
        BIDDER_LIST
    };

    AuctionSearcherRequest(Type const _requestType, AuctionHouseFaction _listFaction) : requestType(_requestType), listFaction(_listFaction) {}
    virtual ~AuctionSearcherRequest() = default;

    Type requestType;
    AuctionHouseFaction listFaction;
};

struct AuctionSearchListRequest : AuctionSearcherRequest
{
    AuctionSearchListRequest(AuctionHouseFaction _listFaction, AuctionHouseSearchInfo const&& _searchInfo, AuctionHousePlayerInfo const&& _playerInfo)
        : AuctionSearcherRequest(AuctionSearcherRequest::Type::LIST, _listFaction), searchInfo(_searchInfo), playerInfo(_playerInfo) {}

    AuctionHouseSearchInfo searchInfo;
    AuctionHousePlayerInfo playerInfo;
};

struct AuctionSearchOwnerListRequest : AuctionSearcherRequest
{
    AuctionSearchOwnerListRequest(AuctionHouseFaction _listFaction, ObjectGuid _ownerGuid)
        : AuctionSearcherRequest(AuctionSearcherRequest::Type::OWNER_LIST, _listFaction), ownerGuid(_ownerGuid) {}

    ObjectGuid ownerGuid;
};

struct AuctionSearchBidderListRequest : AuctionSearcherRequest
{
    AuctionSearchBidderListRequest(AuctionHouseFaction _listFaction, std::vector<uint32> const&& _outbiddedAuctionIds, ObjectGuid _ownerGuid)
        : AuctionSearcherRequest(AuctionSearcherRequest::Type::BIDDER_LIST, _listFaction), outbiddedAuctionIds(_outbiddedAuctionIds), ownerGuid(_ownerGuid) {}

    std::vector<uint32> outbiddedAuctionIds;
    ObjectGuid ownerGuid;
};

struct AuctionSearcherResponse
{
    ObjectGuid playerGuid;
    WorldPacket packet;
};

struct AuctionSearcherUpdate
{
    enum class Type : uint8
    {
        ADD,
        REMOVE,
        UPDATE_BID
    };

    AuctionSearcherUpdate(Type const _updateType, AuctionHouseFaction _listFaction) : updateType(_updateType), listFaction(_listFaction) {}
    virtual ~AuctionSearcherUpdate() = default;

    Type updateType;
    AuctionHouseFaction listFaction;
};

struct AuctionSearchAdd : AuctionSearcherUpdate
{
    AuctionSearchAdd(std::shared_ptr<SearchableAuctionEntry> _searchableAuctionEntry)
        : AuctionSearcherUpdate(AuctionSearcherUpdate::Type::ADD, _searchableAuctionEntry->listFaction), searchableAuctionEntry(_searchableAuctionEntry) {}

    std::shared_ptr<SearchableAuctionEntry> searchableAuctionEntry;
};

struct AuctionSearchRemove : AuctionSearcherUpdate
{
    AuctionSearchRemove(uint32 _auctionId, AuctionHouseFaction _listFaction)
        : AuctionSearcherUpdate(AuctionSearcherUpdate::Type::REMOVE, _listFaction), auctionId(_auctionId) {}

    uint32 auctionId;
};

struct AuctionSearchUpdateBid : AuctionSearcherUpdate
{
    AuctionSearchUpdateBid(uint32 _auctionId, AuctionHouseFaction _listFaction, uint32 _bid, ObjectGuid _bidderGuid)
        : AuctionSearcherUpdate(AuctionSearcherUpdate::Type::UPDATE_BID, _listFaction), auctionId(_auctionId), bid(_bid), bidderGuid(_bidderGuid) {}

    uint32 auctionId;
    uint32 bid;
    ObjectGuid bidderGuid;
};

typedef std::unordered_map<uint32, std::shared_ptr<SearchableAuctionEntry>> SearchableAuctionEntriesMap;
typedef std::vector<SearchableAuctionEntry*> SortableAuctionEntriesList;

#endif // AUCTION_HOUSE_COMMON_H