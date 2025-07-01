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

#ifndef TRINITY_MAP_PARTITIONED_H
#define TRINITY_MAP_PARTITIONED_H

#include "Duration.h"
#include "Map.h"
#include "Position.h"
#include "UniqueTrackablePtr.h"
#include "Unit.h"

class TC_GAME_API MapPartitioned : public Map
{
    friend class MapManager;
    public:
        typedef std::vector<Position> PartitionPolygon;
        typedef std::vector<MapPartition> PartitionEntries;
        typedef std::unordered_map<uint32, Trinity::unique_trackable_ptr<Map>> Partitions;

        MapPartitioned(uint32 id);
        ~MapPartitioned() { }

        // DEBUG ONLY
        void VisualizePartitions(Unit* owner, Seconds duration);

        std::vector<uint32> GetPartitionIds() const;
        virtual ChainedRange<Map::PlayerList> GetAllPlayers() const override;

        // functions overwrite Map versions
        virtual void InitVisibilityDistance() override;
        void Update(uint32 diff) override;
        void DelayedUpdate(uint32 diff) override;
        void UnloadAll() override;

        uint32 CalculatePartitionId(Position const& pos) const;
        Map* CreatePartition(uint32 mapId, uint32 partitionId);
        Map* FindPartition(uint32 partitionId) const
        {
            auto it = _partitions.find(partitionId);
            return (it != _partitions.end()) ? it->second.get() : nullptr;
        }

        Partitions &GetPartitions() { return _partitions; }
        PartitionEntries &GetPartitionEntries() { return _partitionEntries; }
        MapPartition* GetPartitionEntry(uint32 partitionId)
        {
            for (auto& entry : _partitionEntries)
            {
                if (entry.partitionId == partitionId)
                    return &entry;
            }
            return nullptr;
        }
    private:
        static bool IsPointInPolygon(Position const& pos, PartitionPolygon const& polygon);

        Partitions _partitions; // The actual maps
        PartitionEntries _partitionEntries; // The database entries
};

#endif // TRINITY_MAP_PARTITIONED_H
