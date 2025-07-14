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

#include "CreatureGroups.h"
#include "Containers.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "MotionMaster.h"
#include "MovementGenerator.h"
#include "ObjectMgr.h"
#include <winscard.h>

#define MAX_DESYNC 5.0f

FormationMgr::FormationMgr()
{
}

FormationMgr::~FormationMgr()
{
}

FormationMgr* FormationMgr::instance()
{
    static FormationMgr instance;
    return &instance;
}

CreatureGroup* FormationMgr::GetCreatureGroup(ObjectGuid::LowType leaderSpawnId, Map* map)
{
    auto itr = map->CreatureGroupHolder.find(leaderSpawnId);
    if (itr != map->CreatureGroupHolder.end())
        return itr->second;
    return nullptr;
}

void FormationMgr::AddCreatureToGroup(ObjectGuid::LowType leaderSpawnId, Creature* creature)
{
    Map* map = creature->GetMap();

    auto itr = map->CreatureGroupHolder.find(leaderSpawnId);
    if (itr != map->CreatureGroupHolder.end())
    {
        //Add member to an existing group
        TC_LOG_DEBUG("entities.unit", "Group found: {}, inserting creature {}, Group InstanceID {}", leaderSpawnId, creature->GetGUID().ToString(), creature->GetInstanceId());

        // With dynamic spawn the creature may have just respawned
        // we need to find previous instance of creature and delete it from the formation, as it'll be invalidated
        auto bounds = Trinity::Containers::MapEqualRange(map->GetCreatureBySpawnIdStore(), creature->GetSpawnId());
        for (auto const& pair : bounds)
        {
            Creature* other = pair.second;
            if (other == creature)
                continue;

            if (itr->second->HasMember(other))
                itr->second->RemoveMember(other);
        }
    }
    else
    {
        //Create new group
        TC_LOG_DEBUG("entities.unit", "Group not found: {}. Creating new group.", leaderSpawnId);
        CreatureGroup* group = new CreatureGroup(leaderSpawnId);
        std::tie(itr, std::ignore) = map->CreatureGroupHolder.emplace(leaderSpawnId, group);
    }

    itr->second->AddMember(creature);
}

void FormationMgr::RemoveCreatureFromGroup(CreatureGroup* group, Creature* member)
{
    TC_LOG_DEBUG("entities.unit", "Deleting member pointer to GUID: {} from group {}", group->GetLeaderSpawnId(), member->GetSpawnId());
    group->RemoveMember(member);

    if (group->IsEmpty())
    {
        Map* map = member->GetMap();

        TC_LOG_DEBUG("entities.unit", "Deleting group with InstanceID {}", member->GetInstanceId());
        auto itr = map->CreatureGroupHolder.find(group->GetLeaderSpawnId());
        ASSERT(itr != map->CreatureGroupHolder.end(), "Not registered group %u in map %u", group->GetLeaderSpawnId(), map->GetId());
        map->CreatureGroupHolder.erase(itr);
        delete group;
    }
}

void FormationMgr::LoadCreatureFormations()
{
    uint32 oldMSTime = getMSTime();

    //Get group data
    QueryResult result = WorldDatabase.Query("SELECT leaderGUID, memberGUID, dist, angle, groupAI, point_1, point_2 FROM creature_formations ORDER BY leaderGUID");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">>  Loaded 0 creatures in formations. DB table `creature_formations` is empty!");
        return;
    }

    uint32 count = 0;
    std::unordered_set<ObjectGuid::LowType> leaderSpawnIds;
    do
    {
        Field* fields = result->Fetch();

        //Load group member data
        FormationInfo member;
        member.LeaderSpawnId              = fields[0].GetUInt32();
        ObjectGuid::LowType memberSpawnId = fields[1].GetUInt32();
        member.FollowDist                 = 0.f;
        member.FollowAngle                = 0.f;

        //If creature is group leader we may skip loading of dist/angle
        if (member.LeaderSpawnId != memberSpawnId)
        {
            member.FollowDist             = fields[2].GetFloat();
            member.FollowAngle            = fields[3].GetFloat() * float(M_PI) / 180.0f;
        }

        member.GroupAI                    = fields[4].GetUInt32();
        for (uint8 i = 0; i < 2; ++i)
            member.LeaderWaypointIDs[i]   = fields[5 + i].GetUInt16();

        // check data correctness
        {
            if (!sObjectMgr->GetCreatureData(member.LeaderSpawnId))
            {
                TC_LOG_ERROR("sql.sql", "creature_formations table leader guid {} incorrect (not exist)", member.LeaderSpawnId);
                continue;
            }

            if (!sObjectMgr->GetCreatureData(memberSpawnId))
            {
                TC_LOG_ERROR("sql.sql", "creature_formations table member guid {} incorrect (not exist)", memberSpawnId);
                continue;
            }

            leaderSpawnIds.insert(member.LeaderSpawnId);
        }

        _creatureGroupMap.emplace(memberSpawnId, std::move(member));
        ++count;
    } while (result->NextRow());

    for (ObjectGuid::LowType leaderSpawnId : leaderSpawnIds)
    {
        if (!_creatureGroupMap.count(leaderSpawnId))
        {
            TC_LOG_ERROR("sql.sql", "creature_formation contains leader spawn {} which is not included on its formation, removing", leaderSpawnId);
            for (auto itr = _creatureGroupMap.begin(); itr != _creatureGroupMap.end();)
            {
                if (itr->second.LeaderSpawnId == leaderSpawnId)
                {
                    itr = _creatureGroupMap.erase(itr);
                    continue;
                }

                ++itr;
            }
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} creatures in formations in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

FormationInfo* FormationMgr::GetFormationInfo(ObjectGuid::LowType spawnId)
{
    return Trinity::Containers::MapGetValuePtr(_creatureGroupMap, spawnId);
}

void FormationMgr::AddFormationMember(ObjectGuid::LowType spawnId, float followAng, float followDist, ObjectGuid::LowType leaderSpawnId, uint32 groupAI)
{
    FormationInfo member;
    member.LeaderSpawnId = leaderSpawnId;
    member.FollowDist    = followDist;
    member.FollowAngle   = followAng;
    member.GroupAI       = groupAI;
    for (uint8 i = 0; i < 2; ++i)
        member.LeaderWaypointIDs[i] = 0;

    _creatureGroupMap.emplace(spawnId, std::move(member));
}

CreatureGroup::CreatureGroup(ObjectGuid::LowType leaderSpawnId) : _leader(nullptr), _members(),
_leaderSpawnId(leaderSpawnId), _tempLeaderDefaultMovementType(IDLE_MOTION_TYPE), _engaging(false), _disengaging(false)
{
}

CreatureGroup::~CreatureGroup()
{
}

void CreatureGroup::AddMember(Creature* member)
{
    FormationInfo* formationInfo = ASSERT_NOTNULL(sFormationMgr->GetFormationInfo(member->GetSpawnId()));
    _members.emplace(member, formationInfo);
    member->SetFormation(this);

    // If the new member is not the default leader
    if (member->GetSpawnId() != _leaderSpawnId)
    {
        // The spawn point is selected on Creature.LoadFromDB, we will let the AI/Aggro system
        // determine the engagement state
        // No need to do anything else
        return;
    }

    // Just to make the logic legible, we are re-adding the default leader to the group
    Creature* defaultLeader = member;

    // The _leader is only null on initial creation, so this means the group is not yet formed
    if (!_leader)
    {
        _leader = defaultLeader;

        // The motion is already initialized to the default so simply return
        return;
    }
    
    // We must take back leadership

    // If the temporary leader has been set to waypoint moving and has a path, we need to copy the waypoint info back to the default leader
    if (_leader->GetDefaultMovementType() == WAYPOINT_MOTION_TYPE && _leader->GetWaypointPath())
    {
        // Copy the temp leaders waypoint info back to the default leader
        defaultLeader->UpdateCurrentWaypointInfo(_leader->GetCurrentWaypointInfo().first, _leader->GetCurrentWaypointInfo().second);

        // If we respawn while the temp leader is in combat, we need to set the default leader as engaged with the current target
        if (_leader->IsEngaged())
        {
            defaultLeader->EngageWithTarget(_leader->GetThreatManager().GetCurrentVictim());
            defaultLeader->SetHomePosition(_leader->GetHomePosition());
        }
        else
            defaultLeader->GetMotionMaster()->Initialize(); // Else initialize with the new waypoint info

        // Reset the temp leaders motion type (idle or random) and path id
        _leader->SetDefaultMovementType(_tempLeaderDefaultMovementType);
        _leader->LoadPath(_tempLeaderPathId);
        // Reset temp variables
        _tempLeaderDefaultMovementType = IDLE_MOTION_TYPE;
        _tempLeaderPathId = 0;

        // If the temp leader is not engaged, initialize the default motion
        if (!_leader->IsEngaged())
            _leader->GetMotionMaster()->Initialize();
    }

    // set new leader
    _leader = defaultLeader;
}

void CreatureGroup::RemoveMember(Creature* member)
{
    _members.erase(member);
    member->SetFormation(nullptr);
    // If group is unformed or member is not the leader we don't need to do anything else
    if (!_leader || member != _leader)
        return;

    // If we remove the leader we need to find a new leader
    Creature* newLeader = _members.empty() ? nullptr : _members.begin()->first;
    // No-one left mark group as unformed
    if (!newLeader)
    {
        _leader = nullptr;
        return;
    }
    
    if (_leader->GetDefaultMovementType() == WAYPOINT_MOTION_TYPE && _leader->GetWaypointPath())
    {
        // Store the temp leaders motion type so that we can restore it later
        _tempLeaderDefaultMovementType = newLeader->GetDefaultMovementType();
        _tempLeaderPathId = newLeader->GetWaypointPath();
        
        // Copy old leaders waypoint info
        newLeader->UpdateCurrentWaypointInfo(_leader->GetCurrentWaypointInfo().first, _leader->GetCurrentWaypointInfo().second);
        // Override temp leaders movement
        newLeader->SetDefaultMovementType(WAYPOINT_MOTION_TYPE);
        newLeader->LoadPath(_leader->GetWaypointPath());
        // Re-initialize the motion if not engaged
        if (!newLeader->IsEngaged())
            newLeader->GetMotionMaster()->Initialize();
    }
    else
        newLeader->GetMotionMaster()->Initialize(); 

    // set new leader
    _leader = newLeader;
}

void CreatureGroup::MemberEngagingTarget(Creature* member, Unit* target)
{
    // used to prevent recursive calls
    if (_engaging)
        return;

    uint8 groupAI = ASSERT_NOTNULL(sFormationMgr->GetFormationInfo(member->GetSpawnId()))->GroupAI;
    if (!groupAI)
        return;

    if (member == _leader)
    {
        if (!(groupAI & FLAG_MEMBERS_ASSIST_LEADER))
            return;
    }
    else if (!(groupAI & FLAG_LEADER_ASSISTS_MEMBER))
        return;

    _engaging = true;

    for (auto const& pair : _members)
    {
        Creature* other = pair.first;
        if (other == member)
            continue;

        if (!other->IsAlive())
            continue;

        if (((other != _leader && (groupAI & FLAG_MEMBERS_ASSIST_LEADER)) || (other == _leader && (groupAI & FLAG_LEADER_ASSISTS_MEMBER))) && other->IsValidAttackTarget(target))
            other->EngageWithTarget(target);
    }

    _engaging = false;
}

void CreatureGroup::MemberDisengaging(Creature* member)
{
    // used to prevent recursive calls
    if (_disengaging)
        return;

    uint8 groupAI = ASSERT_NOTNULL(sFormationMgr->GetFormationInfo(member->GetSpawnId()))->GroupAI;
    if (!groupAI)
        return;

    // we only disengage other members if disengaging member is alive
    if (!member->IsAlive())
        return;

    if (member == _leader)
    {
        if (!(groupAI & FLAG_MEMBERS_ASSIST_LEADER))
            return;
    }
    else if (!(groupAI & FLAG_LEADER_ASSISTS_MEMBER))
        return;

    _disengaging = true;

    for (auto const& pair : _members)
    {
        Creature* other = pair.first;
        if (other == member)
            continue;

        if (!other->IsAlive() || other->IsInEvadeMode())
            continue;

        if ((other != _leader && (groupAI & FLAG_MEMBERS_ASSIST_LEADER)) || (other == _leader && (groupAI & FLAG_LEADER_ASSISTS_MEMBER)))
        {
            if (CreatureAI* ai = other->AI())
                ai->EnterEvadeMode(CreatureAI::EVADE_REASON_OTHER);
        }
    }

    _disengaging = false;
}

void CreatureGroup::LeaderStartedMoving()
{
    if (!_leader)
        return;

    for (auto const& pair : _members)
    {
        Creature* member = pair.first;
        if (member == _leader || !member->IsAlive() || member->IsEngaged() || !(pair.second->GroupAI & FLAG_IDLE_IN_FORMATION))
            continue;

        float angle = pair.second->FollowAngle + float(M_PI); // for some reason, someone thought it was a great idea to invert relativ angles...
        float dist = pair.second->FollowDist;

        if (!member->HasUnitState(UNIT_STATE_FOLLOW_FORMATION))
            member->GetMotionMaster()->MoveFormation(_leader, dist, angle, pair.second->LeaderWaypointIDs[0], pair.second->LeaderWaypointIDs[1]);
    }
}

bool CreatureGroup::CanLeaderStartMoving() const
{
    for (std::unordered_map<Creature*, FormationInfo*>::value_type const& pair : _members)
    {
        if (pair.first != _leader && pair.first->IsAlive())
        {
            if (pair.first->IsEngaged() || pair.first->IsReturningHome())
                return false;
        }
    }

    return true;
}

Position CreatureGroup::GetRespawnPosition(Creature* member, Position const& spawnPoint) const
{
    if (!_leader)
        return spawnPoint;

    uint32 groupAI = ASSERT_NOTNULL(sFormationMgr->GetFormationInfo(member->GetSpawnId()))->GroupAI;
    if (!groupAI)
        return spawnPoint;

    if (groupAI & FLAG_IDLE_IN_FORMATION)
        return _leader->GetRandomNearPosition(3.0f);

    return spawnPoint;
}