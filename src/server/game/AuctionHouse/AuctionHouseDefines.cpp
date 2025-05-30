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

#include "AuctionHouseDefines.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Util.h"
#include "WorldPacket.h"

uint32 AuctionEntry::GetAuctionCut() const
{
    int32 cut = int32(CalculatePct(bid, auctionHouseEntry->ConsignmentRate) * sWorld->getRate(RATE_AUCTION_CUT));
    return std::max(cut, 0);
}

uint32 AuctionEntry::GetAuctionOutBid() const
{
    return AuctionEntry::CalculateAuctionOutBid(bid);
}

void AuctionEntry::DeleteFromDB(CharacterDatabaseTransaction trans) const
{
    CharacterDatabasePreparedStatement* stmt;

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_AUCTION);
    stmt->setUInt32(0, Id);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_AUCTION_BIDDERS);
    stmt->setUInt32(0, Id);
    trans->Append(stmt);
}

void AuctionEntry::SaveToDB(CharacterDatabaseTransaction trans) const
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_AUCTION);
    stmt->setUInt32(0, Id);
    stmt->setUInt8(1, uint8(houseId));
    stmt->setUInt32(2, itemGUIDLow);
    stmt->setUInt32(3, owner);
    stmt->setUInt32(4, buyout);
    stmt->setUInt32(5, uint32(expire_time));
    stmt->setUInt32(6, bidder);
    stmt->setUInt32(7, bid);
    stmt->setUInt32(8, startbid);
    stmt->setUInt32(9, deposit);
    stmt->setUInt8(10, Flags);
    trans->Append(stmt);
}

void AuctionEntry::LoadFromDB(Field* fields)
{
    Id = fields[0].GetUInt32();
    houseId = fields[1].GetUInt8();
    itemGUIDLow = fields[2].GetUInt32();
    itemEntry = fields[3].GetUInt32();
    itemCount = fields[4].GetUInt32();
    owner = fields[5].GetUInt32();
    buyout = fields[6].GetUInt32();
    expire_time = fields[7].GetUInt32();
    bidder = fields[8].GetUInt32();
    bid = fields[9].GetUInt32();
    startbid = fields[10].GetUInt32();
    deposit = fields[11].GetUInt32();
    Flags = AuctionEntryFlag(fields[12].GetUInt8());
}

std::string AuctionEntry::BuildAuctionMailSubject(Item* item, MailAuctionAnswers response)
{
    return Trinity::StringFormat("{}:{}:{}:{}:{}", itemEntry, item ? item->GetItemRandomPropertyId() : 0, response, Id, itemCount);
}

std::string AuctionEntry::BuildAuctionWonMailBody(ObjectGuid guid, uint32 bid, uint32 buyout)
{
    return Trinity::StringFormat("{:X}:{}:{}", guid.GetRawValue(), bid, buyout);
}

std::string AuctionEntry::BuildAuctionSoldMailBody(ObjectGuid guid, uint32 bid, uint32 buyout, uint32 deposit, uint32 consignment)
{
    return Trinity::StringFormat("{:X}:{}:{}:{}:{}", guid.GetRawValue(), bid, buyout, deposit, consignment);
}

std::string AuctionEntry::BuildAuctionInvoiceMailBody(ObjectGuid guid, uint32 bid, uint32 buyout, uint32 deposit, uint32 consignment, uint32 moneyDelay, uint32 eta)
{
    return Trinity::StringFormat("{:X}:{}:{}:{}:{}:{}:{}", guid.GetRawValue(), bid, buyout, deposit, consignment, moneyDelay, eta);
}

// the sum of outbid is (1% from current bid)*5, if bid is very small, it is 1c
uint32 AuctionEntry::CalculateAuctionOutBid(uint32 bid)
{
    uint32 outbid = CalculatePct(bid, 5);
    return outbid ? outbid : 1;
}

bool AuctionHouseUsablePlayerInfo::PlayerCanUseItem(ItemTemplate const* proto) const
{
    uint32 itemSkill = proto->GetSkill();
    if (itemSkill != 0)
    {
        if (GetSkillValue(itemSkill) == 0)
            return false;
    }

    if ((proto->AllowableClass & classMask) == 0 || (proto->AllowableRace & raceMask) == 0)
        return false;

    if (proto->RequiredSkill != 0)
    {
        if (GetSkillValue(proto->RequiredSkill) == 0)
            return false;
        else if (GetSkillValue(proto->RequiredSkill) < proto->RequiredSkillRank)
            return false;
    }

    if (proto->RequiredSpell != 0 && !HasSpell(proto->RequiredSpell))
        return false;

    if (level < proto->RequiredLevel)
        return false;

    if (proto->Spells[0].SpellId)
    {
        // this check is for vanilla recipies. Spells are learned through individual learning spells instead of spell 483 and 55884
        SpellEntry const* spellEntry = sSpellStore.LookupEntry(proto->Spells[0].SpellId);
        if (spellEntry && spellEntry->Effect[0] == SPELL_EFFECT_LEARN_SPELL && spellEntry->EffectTriggerSpell[0])
            if (HasSpell(spellEntry->EffectTriggerSpell[0]))
                return false;

        // this check is for tbc/wotlk recipies. Spells are learned through 483 and 55884, the second spell in the item will be the actual spell learned.
        if (proto->Spells[0].SpellId == 483 || proto->Spells[0].SpellId == 55884)
            if (HasSpell(proto->Spells[1].SpellId))
                return false;
    }

    return true;
}

uint16 AuctionHouseUsablePlayerInfo::GetSkillValue(uint32 skill) const
{
    if (!skill)
        return 0;

    AuctionPlayerSkills::const_iterator itr = skills.find(skill);
    if (itr == skills.end())
        return 0;

    return itr->second;
}

bool AuctionHouseUsablePlayerInfo::HasSpell(uint32 spell) const
{
    AuctionPlayerSpells::const_iterator itr = spells.find(spell);
    return (itr != spells.end());
}

void SearchableAuctionEntry::BuildAuctionInfo(WorldPacket& data) const
{
    data << uint32(Id);
    data << uint32(item.entry);

    for (uint8 i = 0; i < MAX_INSPECTED_ENCHANTMENT_SLOT; ++i)
    {
        data << uint32(item.enchants[i].id);
        data << uint32(item.enchants[i].duration);
        data << uint32(item.enchants[i].charges);
    }

    data << int32(item.randomPropertyId);                           // Random item property id
    data << uint32(item.suffixFactor);                              // SuffixFactor
    data << uint32(item.count);                                     // item->count
    data << uint32(item.spellCharges);                              // item->charge FFFFFFF
    data << uint32(0);                                              // item->flags (client doesnt do anything with it)
    data << ownerGuid;                                              // Auction->owner
    data << uint32(startbid);                                       // Auction->startbid (not sure if useful)
    data << uint32(bid ? AuctionEntry::CalculateAuctionOutBid(bid) : 0);
    // Minimal outbid
    data << uint32(buyout);                                         // Auction->buyout
    data << uint32((expire_time - GameTime::GetGameTime()) * IN_MILLISECONDS); // time left
    data << bidderGuid;                                             // auction->bidder current
    data << uint32(bid);                                            // current bid
}

void SearchableAuctionEntry::SetItemNames()
{
    ItemTemplate const* proto = item.itemTemplate;
    ItemLocale const* il = sObjectMgr->GetItemLocale(proto->ItemId);

    for (uint32 locale = 0; locale < TOTAL_LOCALES; ++locale)
    {
        if (proto->Name1.empty())
            continue;

        std::string itemName = proto->Name1;

        // local name
        LocaleConstant locdbc_idx = sWorld->GetAvailableDbcLocale(static_cast<LocaleConstant>(locale));
        if (locdbc_idx >= LOCALE_enUS && il)
            ObjectMgr::GetLocaleString(il->Name, locale, itemName);

        // DO NOT use GetItemEnchantMod(proto->RandomProperty) as it may return a result
        //  that matches the search but it may not equal item->GetItemRandomPropertyId()
        //  used in BuildAuctionInfo() which then causes wrong items to be listed
        int32 propRefID = item.randomPropertyId;
        if (propRefID)
        {
            // Append the suffix to the name (ie: of the Monkey) if one exists
            // These are found in ItemRandomSuffix.dbc and ItemRandomProperties.dbc
            // even though the DBC name seems misleading
            std::array<char const*, 16> const* suffix = nullptr;

            if (propRefID < 0)
            {
                ItemRandomSuffixEntry const* itemRandEntry = sItemRandomSuffixStore.LookupEntry(-item.randomPropertyId);
                if (itemRandEntry)
                    suffix = &itemRandEntry->Name;
            }
            else
            {
                ItemRandomPropertiesEntry const* itemRandEntry = sItemRandomPropertiesStore.LookupEntry(item.randomPropertyId);
                if (itemRandEntry)
                    suffix = &itemRandEntry->Name;
            }

            // dbc local name
            if (suffix)
            {
                // Append the suffix (ie: of the Monkey) to the name using localization
                // or default enUS if localization is invalid
                itemName += ' ';
                itemName += (*suffix)[locdbc_idx >= 0 ? locdbc_idx : LOCALE_enUS];
            }
        }

        if (!Utf8toWStr(itemName, item.itemName[locale]))
            continue;

        wstrToLower(item.itemName[locale]);
    }
}

int SearchableAuctionEntry::CompareAuctionEntry(uint32 column, SearchableAuctionEntry const& auc, int loc_idx) const
{
    switch (column)
    {
        case AUCTION_SORT_MINLEVEL:
        {
            ItemTemplate const* itemProto1 = item.itemTemplate;
            ItemTemplate const* itemProto2 = auc.item.itemTemplate;
            if (itemProto1->RequiredLevel > itemProto2->RequiredLevel)
                return -1;
            else if (itemProto1->RequiredLevel < itemProto2->RequiredLevel)
                return +1;
            break;
        }
        case AUCTION_SORT_RARITY:
        {
            ItemTemplate const* itemProto1 = item.itemTemplate;
            ItemTemplate const* itemProto2 = auc.item.itemTemplate;
            if (itemProto1->Quality > itemProto2->Quality)
                return -1;
            else if (itemProto1->Quality < itemProto2->Quality)
                return +1;
            break;
        }
        case AUCTION_SORT_BUYOUT:
        {
            uint32 val1 = item.count > 1 ? buyout / item.count : buyout;
            uint32 val2 = auc.item.count > 1 ? auc.buyout / auc.item.count : auc.buyout;
            if (val1 > val2)
                return -1;
            else if (val1 < val2)
                return +1;
            break;
        }
        case AUCTION_SORT_TIMELEFT:
        {
            if (expire_time > auc.expire_time)
                return -1;
            else if (expire_time < auc.expire_time)
                return +1;
            break;
        }
        case AUCTION_SORT_UNK4:
        {
            if (bidderGuid.GetCounter() > auc.bidderGuid.GetCounter())
                return -1;
            else if (bidderGuid.GetCounter() < auc.bidderGuid.GetCounter())
                return +1;
            break;
        }
        case AUCTION_SORT_ITEM:
        {
            int comparison = item.itemName[loc_idx].compare(auc.item.itemName[loc_idx]);
            if (comparison > 0)
                return -1;
            else if (comparison < 0)
                return +1;
            break;
        }
        case AUCTION_SORT_MINBIDBUY:
        {
            uint32 val1 = buyout > bid ? buyout : bid;
            if (item.count > 1)
                val1 /= item.count;
            uint32 val2 = auc.buyout > auc.bid ? auc.buyout : auc.bid;
            if (auc.item.count > 1)
                val2 /= auc.item.count;
            if (val1 > val2)
                return -1;
            else if (val1 < val2)
                return +1;
            break;
        }
        case AUCTION_SORT_OWNER:
        {
            int comparison = ownerName.compare(auc.ownerName);
            if (comparison > 0)
                return -1;
            else if (comparison < 0)
                return +1;
            break;
        }
        case AUCTION_SORT_BID:
        {
            uint32 val1 = item.count > 1 ? bid / item.count : bid;
            uint32 val2 = auc.item.count > 1 ? auc.bid / auc.item.count : auc.bid;
            if (val1 > val2)
                return -1;
            else if (val1 < val2)
                return +1;
            break;
        }
        case AUCTION_SORT_STACK:
        {
            if (item.count > auc.item.count)
                return -1;
            else if (item.count < auc.item.count)
                return +1;
            break;
        }
        case AUCTION_SORT_BUYOUT_2:
        {
            uint32 val1 = item.count > 1 ? buyout / item.count : buyout;
            uint32 val2 = auc.item.count > 1 ? auc.buyout / auc.item.count : auc.buyout;
            if (val1 > val2)
                return -1;
            else if (val1 < val2)
                return +1;
            break;
        }
        default:
            break;
    }
    return 0;
}

void AuctionHouseObject::AddAuction(AuctionEntry* auction)
{
    AuctionsMap[auction->Id] = auction;
}

bool AuctionHouseObject::RemoveAuction(AuctionEntry* auction)
{
    return AuctionsMap.erase(auction->Id) ? true : false;
}