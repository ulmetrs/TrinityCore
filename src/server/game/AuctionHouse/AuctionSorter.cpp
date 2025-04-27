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

#include "AuctionSorter.h"
#include "AuctionHouseCommon.h"
#include "Item.h"

bool AuctionSorter::operator()(SearchableAuctionEntry const* auc1, SearchableAuctionEntry const* auc2) const
{
    if (_sort->empty()) return false;

    for (AuctionSortOrderVector::const_iterator itr = _sort->begin(); itr != _sort->end(); ++itr)
    {
        int res = auc1->CompareAuctionEntry(itr->sortOrder, *auc2, _loc_idx);
        if (res == 0) continue;
        return (res < 0) == itr->isDesc;
    }

    return false;
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
            if (itemProto1->Quality < itemProto2->Quality)
                return -1;
            else if (itemProto1->Quality > itemProto2->Quality)
                return +1;
            break;
        }
        case AUCTION_SORT_BUYOUT:
            if (buyout != auc.buyout)
            {
                if (buyout < auc.buyout)
                    return -1;
                else if (buyout > auc.buyout)
                    return +1;
            }
            else
            {
                if (bid < auc.bid)
                    return -1;
                else if (bid > auc.bid)
                    return +1;
            }
            break;
        case AUCTION_SORT_TIMELEFT:
            if (expire_time < auc.expire_time)
                return -1;
            else if (expire_time > auc.expire_time)
                return +1;
            break;
        case AUCTION_SORT_UNK4:
            if (bidderGuid.GetCounter() < auc.bidderGuid.GetCounter())
                return -1;
            else if (bidderGuid.GetCounter() > auc.bidderGuid.GetCounter())
                return +1;
            break;
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
            if (buyout != auc.buyout)
            {
                if (buyout > auc.buyout)
                    return -1;
                else if (buyout < auc.buyout)
                    return +1;
            }
            else
            {
                if (bid < auc.bid)
                    return -1;
                else if (bid > auc.bid)
                    return +1;
            }
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
            uint32 bid1 = bid ? bid : startbid;
            uint32 bid2 = auc.bid ? auc.bid : auc.startbid;
            if (bid1 > bid2)
                return -1;
            else if (bid1 < bid2)
                return +1;
            break;
        }
        case AUCTION_SORT_STACK:
        {
            if (item.count < auc.item.count)
                return -1;
            else if (item.count > auc.item.count)
                return +1;
            break;
        }
        case AUCTION_SORT_BUYOUT_2:
            if (buyout < auc.buyout)
                return -1;
            else if (buyout > auc.buyout)
                return +1;
            break;
        default:
            break;
    }
    return 0;
}