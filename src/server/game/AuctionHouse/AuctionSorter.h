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

#ifndef AUCTION_SORTER_H
#define AUCTION_SORTER_H

#include "Common.h"
#include <vector>

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

struct AuctionSortInfo
{
    AuctionSortInfo() = default;
    AuctionSortOrder sortOrder{ AUCTION_SORT_MAX };
    bool isDesc{ true };
};

typedef std::vector<AuctionSortInfo> AuctionSortOrderVector;

// Forward declaration
class SearchableAuctionEntry;

class AuctionSorter
{
public:
    AuctionSorter(AuctionSortOrderVector const* sort, int loc_idx) : _sort(sort), _loc_idx(loc_idx) {}
    bool operator()(SearchableAuctionEntry const* auc1, SearchableAuctionEntry const* auc2) const;

private:
    AuctionSortOrderVector const* _sort;
    int _loc_idx;
};

#endif // AUCTION_SORTER_H