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

#include "MapManager.h"
#include "InstanceSaveMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Transport.h"
#include "GridDefines.h"
#include "MapInstanced.h"
#include "MapPartitioned.h"
#include "InstanceScript.h"
#include "Config.h"
#include "World.h"
#include "Corpse.h"
#include "ObjectMgr.h"
#include "WorldPacket.h"
#include "Group.h"
#include "Player.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "TSProfile.h"
#include "ScriptMgr.h"
#include <numeric>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

MapManager* MapManager::instance()
{
    static MapManager instance;
    return &instance;
}

MapManager::MapManager() : _nextInstanceId(0), _scheduledScripts(0)
{
    i_timer.SetInterval(sWorld->getIntConfig(CONFIG_INTERVAL_MAPUPDATE));
}

MapManager::~MapManager() { }

void MapManager::LoadBaseMaps(std::set<uint32> const& mapIds)
{
    // Now, for each mapId with at least one grid, create the correct map type
    for (const auto& mapId : mapIds)
    {

        TC_LOG_INFO("server.loading", "Loading Base Map {}", mapId);
        CreateBaseMap(mapId);
    }
}

void MapManager::Initialize()
{
    int num_threads(sWorld->getIntConfig(CONFIG_NUMTHREADS));
    // Start mtmaps if needed.
    if (num_threads > 0)
        m_updater.activate(num_threads);
}

void MapManager::InitializeVisibilityDistanceInfo()
{
    for (auto& [_, mapPtr] : _baseMaps)
        mapPtr->InitVisibilityDistance();
}

uint32 MapManager::CalculatePartitionId(uint32 mapid, Position const& pos)
{
    Map* map = FindBaseMap(mapid);
    if (!map)
        return 0;

    MapPartitioned* mapPartitioned = map->ToMapPartitioned();
    if (!mapPartitioned)
        return 0;

    return mapPartitioned->CalculatePartitionId(pos);
}

void MapManager::VisualizePartitions(Unit* owner, Seconds duration)
{
    Map* map = FindBaseMap(owner->GetMapId());
    if (!map)
        return;

    MapPartitioned* mapPartitioned = map->ToMapPartitioned();
    if (!mapPartitioned)
        return;

    mapPartitioned->VisualizePartitions(owner, duration);
}

std::vector<uint32> MapManager::GetContinentPartitionIds(uint32 mapId)
{
    Map* map = FindBaseMap(mapId);
    if (!map)
        return {};

    MapPartitioned* mapPartitioned = map->ToMapPartitioned();
    if (!mapPartitioned)
        return {};

    return mapPartitioned->GetPartitionIds();
}

ChainedRange<Map::PlayerList> MapManager::GetContinentPlayers(uint32 mapId)
{
    Map* map = FindBaseMap(mapId);
    if (!map)
        return ChainedRange<Map::PlayerList>(std::vector<Map::PlayerList*>{});

    MapPartitioned* mapPartitioned = map->ToMapPartitioned();
    if (!mapPartitioned)
        return ChainedRange<Map::PlayerList>(std::vector<Map::PlayerList*>{});

    return mapPartitioned->GetAllPlayers();
}

Map* MapManager::CreateBaseMap(uint32 mapId)
{
    Map* map = FindBaseMap(mapId);
    if (map)
        return map;

    std::lock_guard<std::mutex> lock(_mapsLock);

    MapEntry const* entry = sMapStore.LookupEntry(mapId);
    ASSERT(entry);

    if (entry->Instanceable())
    {
        // If MapInstanced needs grids, add as a parameter. Otherwise, just pass mapId.
        map = new MapInstanced(mapId);
        std::unique_ptr<Map> ptr(map);
        _baseMaps[mapId] = std::move(ptr);
    }
    else
    {
        // Pass the grids to MapPartitioned constructor
        map = new MapPartitioned(mapId);
        std::unique_ptr<Map> ptr(map);
        _baseMaps[mapId] = std::move(ptr);

        MapPartitioned* mapPartitioned = map->ToMapPartitioned();

        // TODO move into constructor?
        // Create all partitions for this map before loading respawns and corpses
        for (auto& partitionEntry : mapPartitioned->GetPartitionEntries())
        {
            mapPartitioned->CreatePartition(mapId, partitionEntry.partitionId);
        }

        // Leave the ordering of this
        map->LoadRespawnTimes();
        map->LoadCorpseData();
        sScriptMgr->OnCreateMap(map);

        // Leave the ordering of this
        for (auto& [_, partitionPtr] : mapPartitioned->GetPartitions())
        {
            partitionPtr.get()->LoadRespawnTimes();
            partitionPtr.get()->LoadCorpseData();
            sScriptMgr->OnCreateMap(partitionPtr.get());
        }
    }

    return map;
}

// This is used for most of our uses cases, find the map if exists, create it if its not -
// player is required if the map is instanceable
Map* MapManager::CreateMap(uint32 id, Position const& pos, Player* player, uint32 loginInstanceId)
{
    ZoneScopedNC("MapManager::CreateMap", WORLD_UPDATE_COLOR)

    Map* map = CreateBaseMap(id);
    if (!map)
        return nullptr;

    MapInstanced* mapInstanced = map->ToMapInstanced();
    if (mapInstanced)
    {
        // For GameEventManager, Transports, when we spawn these in an instance map without a player they
        // go into the base map - Im guessing they update the spawn tables and get replicated for new instances
        if (!player)
            return map;

        // Additional Logic to check for existing instance
        return mapInstanced->CreateInstanceForPlayer(id, player, loginInstanceId);
    }

    MapPartitioned* mapPartitioned = map->ToMapPartitioned();
    if (!mapPartitioned)
        return nullptr;

    uint32 partitionId = mapPartitioned->CalculatePartitionId(pos);

    // Additional logic to check for existing partition
    return mapPartitioned->CreatePartition(id, partitionId);
}

// Use this for queries where we do not want to create the map directly
Map* MapManager::FindMap(uint32 mapid, Position const& pos, uint32 instanceId) const
{
    Map* map = FindBaseMap(mapid);
    if (!map)
        return nullptr;

    MapInstanced* mapInstanced = map->ToMapInstanced();
    if (mapInstanced)
    {
        // FindMap does not return the base map for instances
        return mapInstanced->FindInstance(instanceId);
    }

    MapPartitioned* mapPartitioned = map->ToMapPartitioned();
    if (!mapPartitioned)
        return nullptr;
        
    uint32 partitionId = mapPartitioned->CalculatePartitionId(pos);
    // For partitions the base map is the default/fallback partition, so return it
    if (partitionId == mapPartitioned->GetPartitionId())
        return mapPartitioned;

    return mapPartitioned->FindPartition(partitionId);
}

Map* MapManager::FindContinent(uint32 mapId) const
{
    Map* baseMap = FindBaseMap(mapId);
    if (!baseMap)
        return nullptr;

    MapPartitioned* mapPartitioned = baseMap->ToMapPartitioned();
    if (!mapPartitioned)
        return nullptr;

    return baseMap;
}

Map* MapManager::FindPartition(uint32 mapId, uint32 partitionId) const
{
    Map* baseMap = FindBaseMap(mapId);
    if (!baseMap)
        return nullptr;

    MapPartitioned* mapPartitioned = baseMap->ToMapPartitioned();
    if (!mapPartitioned)
        return nullptr;

    return mapPartitioned->FindPartition(partitionId);
}

Map::EnterState MapManager::PlayerCannotEnter(uint32 mapid, Player* player, bool loginCheck)
{
    MapEntry const* entry = sMapStore.LookupEntry(mapid);
    if (!entry)
        return Map::CANNOT_ENTER_NO_ENTRY;

    if (!entry->IsDungeon())
        return Map::CAN_ENTER;

    InstanceTemplate const* instance = sObjectMgr->GetInstanceTemplate(mapid);
    if (!instance)
        return Map::CANNOT_ENTER_UNINSTANCED_DUNGEON;

    Difficulty targetDifficulty, requestedDifficulty;
    targetDifficulty = requestedDifficulty = player->GetDifficulty(entry->IsRaid());
    // Get the highest available difficulty if current setting is higher than the instance allows
    MapDifficulty const* mapDiff = GetDownscaledMapDifficultyData(entry->ID, targetDifficulty);
    if (!mapDiff)
        return Map::CANNOT_ENTER_DIFFICULTY_UNAVAILABLE;

    //Bypass checks for GMs
    if (player->IsGameMaster())
        return Map::CAN_ENTER;

    char const* mapName = entry->MapName[player->GetSession()->GetSessionDbcLocale()];

    Group* group = player->GetGroup();
    if (entry->IsRaid()) // can only enter in a raid group
        if ((!group || !group->isRaidGroup()) && !sWorld->getBoolConfig(CONFIG_INSTANCE_IGNORE_RAID))
            return Map::CANNOT_ENTER_NOT_IN_RAID;

    if (!player->IsAlive())
    {
        if (player->HasCorpse())
        {
            // let enter in ghost mode in instance that connected to inner instance with corpse
            uint32 corpseMap = player->GetCorpseLocation().GetMapId();
            do
            {
                if (corpseMap == mapid)
                    break;

                InstanceTemplate const* corpseInstance = sObjectMgr->GetInstanceTemplate(corpseMap);
                corpseMap = corpseInstance ? corpseInstance->Parent : 0;
            } while (corpseMap);

            if (!corpseMap)
                return Map::CANNOT_ENTER_CORPSE_IN_DIFFERENT_INSTANCE;

            TC_LOG_DEBUG("maps", "MAP: Player '{}' has corpse in instance '{}' and can enter.", player->GetName(), mapName);
        }
        else
            TC_LOG_DEBUG("maps", "Map::CanPlayerEnter - player '{}' is dead but does not have a corpse!", player->GetName());
    }

    //Get instance where player's group is bound & its map
    if (!loginCheck && group)
    {
        InstanceGroupBind* boundInstance = group->GetBoundInstance(entry);
        if (boundInstance && boundInstance->save)
            if (Map* boundMap = FindMap(mapid, Position(), boundInstance->save->GetInstanceId()))
                if (Map::EnterState denyReason = boundMap->CannotEnter(player))
                    return denyReason;
    }

    // players are only allowed to enter 5 instances per hour
    if (entry->IsDungeon() && (!player->GetGroup() || (player->GetGroup() && !player->GetGroup()->isLFGGroup())))
    {
        uint32 instanceIdToCheck = 0;
        if (InstanceSave* save = player->GetInstanceSave(mapid, entry->IsRaid()))
            instanceIdToCheck = save->GetInstanceId();

        // instanceId can never be 0 - will not be found
        if (!player->GetSession()->UpdateAndCheckInstanceCount(instanceIdToCheck) && !player->isDead())
            return Map::CANNOT_ENTER_TOO_MANY_INSTANCES;
    }

    //Other requirements
    if (!player->Satisfy(sObjectMgr->GetAccessRequirement(mapid, targetDifficulty), mapid, true))
        return Map::CANNOT_ENTER_UNSPECIFIED_REASON;

    return Map::CAN_ENTER;
}

void MapManager::Update(uint32 diff)
{
    i_timer.Update(diff);
    if (!i_timer.Passed())
        return;

    // map updates can be scheduled to be run in parallel if the updater is activated
    for (auto& [id, mapPtr] : _baseMaps)
    {
        if (m_updater.activated())
            m_updater.schedule_update(*mapPtr, uint32(i_timer.GetCurrent()));
        else
            mapPtr->Update(uint32(i_timer.GetCurrent()));
        
        if (MapPartitioned* mapPartitioned = mapPtr->ToMapPartitioned())
        {
            for (auto& [_, partitionPtr] : mapPartitioned->GetPartitions())
            {
                if (m_updater.activated())
                    m_updater.schedule_update(*partitionPtr, uint32(i_timer.GetCurrent()));
                else
                    partitionPtr->Update(uint32(i_timer.GetCurrent()));
            }
        }

        // (Previously this was done in MapInstanced::Update, but I prefer not to tie up another thread as a scheduler, thats what this is for)
        if (MapInstanced* mapInstanced = mapPtr->ToMapInstanced())
        {
            auto& instances = mapInstanced->GetInstances();
            for (auto it = instances.begin(); it != instances.end(); /* no increment here */)
            {
                if (it->second->CanUnload(uint32(i_timer.GetCurrent())))
                {
                    mapInstanced->DestroyInstance(it); // iterator incremented
                }
                else
                {
                    if (m_updater.activated())
                        m_updater.schedule_update(*it->second, uint32(i_timer.GetCurrent()));
                    else
                        it->second->Update(uint32(i_timer.GetCurrent()));
                    ++it;
                }
            }
        }
    }

    if (m_updater.activated())
        m_updater.wait();

    // delayed map updates must be run synchronously
    for (auto& [id, mapPtr] : _baseMaps)
    {
        mapPtr->DelayedUpdate(uint32(i_timer.GetCurrent()));

        // For consistency with Update, we need to call DelayedUpdate on each partition
        if (MapPartitioned* mapPartitioned = mapPtr->ToMapPartitioned())
        {
            for (auto& [_, partitionPtr] : mapPartitioned->GetPartitions())
                partitionPtr->DelayedUpdate(uint32(i_timer.GetCurrent()));
        }
        if (MapInstanced* mapInstanced = mapPtr->ToMapInstanced())
        {
            for (auto& [_, instancePtr] : mapInstanced->GetInstances())
                instancePtr->DelayedUpdate(uint32(i_timer.GetCurrent()));
        }
    }

    i_timer.SetCurrent(0);
}

bool MapManager::ExistMapAndVMap(uint32 mapid, float x, float y)
{
    GridCoord p = Trinity::ComputeGridCoord(x, y);

    int gx = (MAX_NUMBER_OF_GRIDS - 1) - p.x_coord;
    int gy = (MAX_NUMBER_OF_GRIDS - 1) - p.y_coord;

    return Map::ExistMap(mapid, gx, gy) && Map::ExistVMap(mapid, gx, gy);
}

bool MapManager::IsValidMAP(uint32 mapid, bool startUp)
{
    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);

    if (startUp)
        return mEntry ? true : false;
    else
        return mEntry && (!mEntry->IsDungeon() || sObjectMgr->GetInstanceTemplate(mapid));

    /// @todo add check for battleground template
}

void MapManager::UnloadAll()
{
    // first unload base maps
    for (auto& [id, mapPtr] : _baseMaps)
        mapPtr->UnloadAll();

    // then delete them
    _baseMaps.clear();

    if (m_updater.activated())
        m_updater.deactivate();
}

uint32 MapManager::GetNumInstances()
{
    std::lock_guard<std::mutex> lock(_mapsLock);

    uint32 ret = 0;
    for (auto const& [_, map] : _baseMaps)
    {
        MapInstanced* mapInstanced = map->ToMapInstanced();
        if (!mapInstanced)
            continue;
        ret += mapInstanced->GetInstances().size();
    }
    return ret;
}

uint32 MapManager::GetNumPlayersInInstances()
{
    std::lock_guard<std::mutex> lock(_mapsLock);

    uint32 ret = 0;
    for (auto& [_, map] : _baseMaps)
    {
        MapInstanced* mapInstanced = map->ToMapInstanced();
        if (!mapInstanced)
            continue;
        MapInstanced::Instances& maps = mapInstanced->GetInstances();
        ret += std::accumulate(maps.begin(), maps.end(), 0u, [](uint32 total, MapInstanced::Instances::value_type const& value) { return total + value.second->GetPlayers().getSize(); });
    }
    return ret;
}

void MapManager::InitInstanceIds()
{
    _nextInstanceId = 1;

    QueryResult result = CharacterDatabase.Query("SELECT MAX(id) FROM instance");
    if (result)
    {
        uint32 maxId = (*result)[0].GetUInt32();

        // resize to maxId + 1, if we have instance with id n, there are n+1 elements
        _instanceIds.resize(maxId + 1);
    }
}

void MapManager::RegisterInstanceId(uint32 instanceId)
{
    // Allocation was done in InitInstanceIds()
    _instanceIds[instanceId] = true;

    // Instances are pulled in ascending order from db and _nextInstanceId is initialized with 1,
    // so if the instance id is used, increment
    if (_nextInstanceId == instanceId)
        ++_nextInstanceId;
}

uint32 MapManager::GenerateInstanceId()
{
    uint32 newInstanceId = _nextInstanceId;

    // find the lowest available id starting from the current _nextInstanceId
    while (_nextInstanceId < 0xFFFFFFFF && ++_nextInstanceId < _instanceIds.size() && _instanceIds[_nextInstanceId]);

    if (_nextInstanceId == 0xFFFFFFFF)
    {
        TC_LOG_ERROR("maps", "Instance ID overflow!! Can't continue, shutting down server. ");
        World::StopNow(ERROR_EXIT_CODE);
    }

    return newInstanceId;
}