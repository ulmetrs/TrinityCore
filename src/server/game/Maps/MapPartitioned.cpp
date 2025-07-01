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

#include "MapPartitioned.h"
#include "DBCStores.h"
#include "Group.h"
#include "Log.h"
#include "MapManager.h"
#include "ObjectDefines.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "TSProfile.h"

MapPartitioned::MapPartitioned(uint32 id) : Map(id, 0)
{
    PartitionEntries const* entries = sObjectMgr->GetMapPartitions(id);

    if (entries && !entries->empty())
    {
        PartitionEntries sortedEntries = *entries;

        std::sort(sortedEntries.begin(), sortedEntries.end(), [](const MapPartition& a, const MapPartition& b) {
            return a.priority > b.priority;
        });

        _partitionEntries = std::move(sortedEntries);
    }
}

static const int8 BOUNDARY_VISUALIZE_STEP_SIZE = 5;
void MapPartitioned::VisualizePartitions(Unit* owner, Seconds duration)
{
    TC_LOG_DEBUG("visualize", "Visualizing partitions for map {}", owner->GetMap()->GetId());
    MapPartition* partition = GetPartitionEntry(owner->GetMap()->GetPartitionId());
    TC_LOG_DEBUG("visualize", "Got Partition Entry {} size {}", partition->partitionId, partition->polygon.size());
    if (!partition || partition->polygon.size() < 2)
        return;

    for (size_t i = 0; i < partition->polygon.size(); ++i)
    {
        const Position& start = partition->polygon[i];
        const Position& end = partition->polygon[(i + 1) % partition->polygon.size()]; // Wrap to first point

        float dx = end.GetPositionX() - start.GetPositionX();
        float dy = end.GetPositionY() - start.GetPositionY();
        float length = std::sqrt(dx * dx + dy * dy);

        if (length < 1e-3f)
            continue;

        float stepCount = std::floor(length / BOUNDARY_VISUALIZE_STEP_SIZE);
        float stepX = dx / length * BOUNDARY_VISUALIZE_STEP_SIZE;
        float stepY = dy / length * BOUNDARY_VISUALIZE_STEP_SIZE;

        TC_LOG_DEBUG("visualize", "For point {} summon {} waypoints", i, stepCount);
        for (int step = 0; step <= stepCount; ++step)
        {
            float x = start.GetPositionX() + step * stepX;
            float y = start.GetPositionY() + step * stepY;
            float z = owner->GetPositionZ();
            owner->SummonCreature(VISUAL_WAYPOINT, x, y, z, 0, TEMPSUMMON_TIMED_DESPAWN, duration);
        }
    }
}

std::vector<uint32> MapPartitioned::GetPartitionIds() const
{
    std::vector<uint32> ids;
    ids.reserve(1 + _partitions.size());
    ids.push_back(GetPartitionId());
    for (const auto& [partitionId, _] : _partitions)
        ids.push_back(partitionId);
    return ids;
}

ChainedRange<Map::PlayerList> MapPartitioned::GetAllPlayers() const
{
    std::vector<Map::PlayerList*> lists;
    lists.push_back(const_cast<Map::PlayerList*>(&GetPlayers()));
    for (const auto& [_, partitionPtr] : _partitions)
        lists.push_back(const_cast<Map::PlayerList*>(&partitionPtr->GetPlayers()));
    return ChainedRange<Map::PlayerList>(lists);
}

void MapPartitioned::InitVisibilityDistance()
{
    for (auto& [_, partitionPtr] : _partitions)
        partitionPtr->InitVisibilityDistance();

    Map::InitVisibilityDistance();
}

void MapPartitioned::Update(uint32 t)
{
    ZoneScopedNC("MapPartitioned::Update", WORLD_UPDATE_COLOR)

    Map::Update(t);
}

void MapPartitioned::DelayedUpdate(uint32 diff)
{
    ZoneScopedNC("MapPartitioned::DelayedUpdate", WORLD_UPDATE_COLOR)

    Map::DelayedUpdate(diff);
}

void MapPartitioned::UnloadAll()
{
    // Clear child maps
    for (auto& [_, partitionPtr] : _partitions)
        partitionPtr->UnloadAll();

    _partitions.clear();

    // Clear this base map as well
    Map::UnloadAll();

    sScriptMgr->OnDestroyMap(this);
}

bool MapPartitioned::IsPointInPolygon(Position const& pos, PartitionPolygon const& polygon)
{
    float x = pos.GetPositionX();
    float y = pos.GetPositionY();
    bool inside = false;
    size_t n = polygon.size();
    if (n < 3)
        return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = polygon[i].GetPositionX(), yi = polygon[i].GetPositionY();
        float xj = polygon[j].GetPositionX(), yj = polygon[j].GetPositionY();
        bool intersect = ((yi > y) != (yj > y)) &&
                         (x < (xj - xi) * (y - yi) / (yj - yi + 1e-12f) + xi);
        if (intersect)
            inside = !inside;
    }
    return inside;
}

uint32 MapPartitioned::CalculatePartitionId(Position const& pos) const
{
    for (const auto& partition : _partitionEntries)
    {
        if (IsPointInPolygon(pos, partition.polygon))
            return partition.partitionId;
    }

    return 0;
}

Map* MapPartitioned::CreatePartition(uint32 mapId, uint32 partitionId)
{
    ASSERT(GetId() == mapId);

    // The base map will be used as fallback for all partitions, this
    // just skips searching the partitions if the id matches
    if (GetPartitionId() == partitionId)
        return this;

    Map* partition = FindPartition(partitionId);
    if (partition)
        return partition;

    // make sure we have a valid map id
    MapEntry const* entry = sMapStore.LookupEntry(GetId());
    if (!entry)
    {
        TC_LOG_ERROR("maps", "CreatePartition: no entry for map {}", GetId());
        ABORT();
    }

    Map* map = new PartitionMap(GetId(), partitionId, this);
    ASSERT(map->IsWorldMap());

    Trinity::unique_trackable_ptr<Map>& ptr = _partitions[partitionId];
    ptr.reset(map);
    map->SetWeakPtr(ptr);

    return map;
}