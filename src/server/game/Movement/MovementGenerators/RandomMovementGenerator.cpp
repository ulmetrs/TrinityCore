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

#include "RandomMovementGenerator.h"
#include "Creature.h"
#include "G3DPosition.hpp"
#include "Map.h"
#include "MovementDefines.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "PathGenerator.h"
#include "Random.h"
#include <G3D/Vector3.h>

namespace
{
    constexpr float MIN_WANDER_DISTANCE = 1.0f;
    constexpr int NUM_WANDER_PATHS = 12;
}

template<class T>
RandomMovementGenerator<T>::RandomMovementGenerator(float distance) : _init(false), _maxWanderDistance(distance), _wanderSteps(0), _reference(), _pathIndex(0), _timer(0), _failedAttempts(0)
{
    this->Mode = MOTION_MODE_DEFAULT;
    this->Priority = MOTION_PRIORITY_NORMAL;
    this->Flags = MOVEMENTGENERATOR_FLAG_INITIALIZATION_PENDING;
    this->BaseUnitState = UNIT_STATE_ROAMING;
}

template RandomMovementGenerator<Creature>::RandomMovementGenerator(float/* distance*/);

template<class T>
MovementGeneratorType RandomMovementGenerator<T>::GetMovementGeneratorType() const
{
    return RANDOM_MOTION_TYPE;
}

template<class T>
void RandomMovementGenerator<T>::Pause(uint32 timer /*= 0*/)
{
    if (timer)
    {
        this->AddFlag(MOVEMENTGENERATOR_FLAG_TIMED_PAUSED);
        _timer.Reset(timer);
        this->RemoveFlag(MOVEMENTGENERATOR_FLAG_PAUSED);
    }
    else
    {
        this->AddFlag(MOVEMENTGENERATOR_FLAG_PAUSED);
        this->RemoveFlag(MOVEMENTGENERATOR_FLAG_TIMED_PAUSED);
    }
}

template<class T>
void RandomMovementGenerator<T>::Resume(uint32 overrideTimer /*= 0*/)
{
    if (overrideTimer)
        _timer.Reset(overrideTimer);

    this->RemoveFlag(MOVEMENTGENERATOR_FLAG_PAUSED);
}

template MovementGeneratorType RandomMovementGenerator<Creature>::GetMovementGeneratorType() const;

template<class T>
void RandomMovementGenerator<T>::DoInitialize(T*) { }

template<>
void RandomMovementGenerator<Creature>::DoInitialize(Creature* owner)
{
    RemoveFlag(MOVEMENTGENERATOR_FLAG_INITIALIZATION_PENDING | MOVEMENTGENERATOR_FLAG_TRANSITORY | MOVEMENTGENERATOR_FLAG_DEACTIVATED | MOVEMENTGENERATOR_FLAG_TIMED_PAUSED);
    AddFlag(MOVEMENTGENERATOR_FLAG_INITIALIZED);

    if (!owner || !owner->IsAlive())
        return;

    owner->StopMoving();
    ResetPaths();

    if (_maxWanderDistance <= MIN_WANDER_DISTANCE)
        _maxWanderDistance = std::max(MIN_WANDER_DISTANCE, owner->GetWanderDistance());

    // Retail seems to let a creature walk 2 up to 10 splines before triggering a pause
    _wanderSteps = urand(1, ((_maxWanderDistance <= MIN_WANDER_DISTANCE) ? 2 : 8));
    // Should we reset timer? _timer.Reset(0);

    // Only set this on first initialize
    if (!_init)
    {
        _init = true;
        _reference = owner->GetPosition();
    }
}

template<class T>
void RandomMovementGenerator<T>::DoReset(T*) { }

template<>
void RandomMovementGenerator<Creature>::DoReset(Creature* owner)
{
    RemoveFlag(MOVEMENTGENERATOR_FLAG_TRANSITORY | MOVEMENTGENERATOR_FLAG_DEACTIVATED);

    DoInitialize(owner);
}

template<class T>
void RandomMovementGenerator<T>::SetRandomLocation(T*) { }

template<>
void RandomMovementGenerator<Creature>::SetRandomLocation(Creature* owner)
{
    if (!owner)
        return;

    if (owner->HasUnitState(UNIT_STATE_NOT_MOVE | UNIT_STATE_LOST_CONTROL) || owner->IsMovementPreventedByCasting())
    {
        AddFlag(MOVEMENTGENERATOR_FLAG_INTERRUPTED);
        owner->StopMoving();
        ResetPaths();
        return;
    }

    // Create a path for caching
    if (_paths.size() < NUM_WANDER_PATHS)
    {
        Movement::PointsArray path;
        Position dest = owner->GetPosition();

        // The last path connects to the front of the first path
        // This is the only potential sharp angle, but the math required to make this not sharp
        // takes away from our performance optimizations.
        if (_paths.size() == NUM_WANDER_PATHS - 1)
        {
            G3D::Vector3& first = _paths.front().front();
            dest.Relocate(first.x, first.y, first.z);
        }
        else
        {
            float distanceFromSpawn = owner->GetPosition().GetExactDist(_reference);
            float angle = 0.0f;
            
            // The first path always needs to be random incase we need to recover from a failed path
            // (the angle constraints might get us stuck in a corner that we need to actually reverse to get out of)
            if (_paths.empty())
            {
                angle = frand(0, 2 * M_PI);
            }
            // If we are not near the wander boundary then pick a random 'forwardish' direction
            else if (distanceFromSpawn < 0.75f * _maxWanderDistance)
            {
                angle = frand(-0.5f * M_PI, 0.5f * M_PI);
            }
            // Else steer back towards the spawn point
            // If the turn angle is too sharp, clamp it to 0.75f * M_PI
            else
            {
                float currentOrientation = owner->GetOrientation();
                float dx = _reference.GetPositionX() - owner->GetPositionX();
                float dy = _reference.GetPositionY() - owner->GetPositionY();
                float angleToReference = std::atan2(dy, dx);
                float angleDiff = angleToReference - currentOrientation;
                while (angleDiff > M_PI) angleDiff -= 2 * M_PI;
                while (angleDiff < -M_PI) angleDiff += 2 * M_PI;
                float maxTurn = 0.75f * M_PI;
                if (angleDiff > maxTurn) angleDiff = maxTurn;
                if (angleDiff < -maxTurn) angleDiff = -maxTurn;
                angle = angleDiff;
            }

            // Pick a wander distance
            float distance = frand(MIN_WANDER_DISTANCE, _maxWanderDistance);
            //Move dest accounting for collisions
            owner->MovePositionToFirstCollision(dest, distance, angle);
        }

        // Check if the destination is in LOS
        if (!owner->IsWithinLOS(dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ()))
        {
            // We cannot path to the location, but rather than clearing the reseting the cache immediately try a few more times
            // just in case we pick a better random angle or the los changes
            _failedAttempts++;
            // We have gotten ourselves into a bad spot with the current cached path
            if (_failedAttempts >= 5)
                ResetPaths();

            _timer.Reset(200);
            return;
        }

        // Lazy load path generator
        if (!_pathGenerator)
        {
            _pathGenerator = std::make_unique<PathGenerator>(owner);
            _pathGenerator->SetPathLengthLimit(50.0f); // Bumped this up since epoch uses some long wander settings
        }

        bool result = _pathGenerator->CalculatePath(PositionToVector3(owner->GetPosition()), PositionToVector3(dest));
        // PATHFIND_FARFROMPOLY shouldn't be checked as creatures in water are most likely far from poly
        if (!result || (_pathGenerator->GetPathType() & PATHFIND_NOPATH)
                    || (_pathGenerator->GetPathType() & PATHFIND_SHORTCUT)
                    /*|| (_pathGenerator->GetPathType() & PATHFIND_FARFROMPOLY)*/)
        {
            // We cannot path to the location, but rather than clearing the reseting the cache immediately try a few more times
            // just in case we pick a better random angle or the los changes
            _failedAttempts++;
            // We have gotten ourselves into a bad spot with the current cached path
            if (_failedAttempts >= 5)
                ResetPaths();

            _timer.Reset(100);
            return;
        }

        _failedAttempts = 0;
        _paths.push_back(_pathGenerator->GetPath());
    }

    RemoveFlag(MOVEMENTGENERATOR_FLAG_TRANSITORY | MOVEMENTGENERATOR_FLAG_TIMED_PAUSED);

    owner->AddUnitState(UNIT_STATE_ROAMING_MOVE);

    bool walk = true;
    switch (owner->GetMovementTemplate().GetRandom())
    {
        case CreatureRandomMovementType::CanRun:
            walk = owner->IsWalking();
            break;
        case CreatureRandomMovementType::AlwaysRun:
            walk = false;
            break;
        default:
            break;
    }

    Movement::MoveSplineInit init(owner);
    init.MovebyPath(_paths[_pathIndex]);
    init.SetWalk(walk);
    init.Launch();

    ++_pathIndex;
    if (_pathIndex >= NUM_WANDER_PATHS)
        _pathIndex = 0;
    --_wanderSteps;

    // Call for creature group update
    owner->SignalFormationMovement();
}

template<class T>
void RandomMovementGenerator<T>::ResetPaths()
{
    _pathIndex = 0;
    _paths.clear();
    _pathGenerator = nullptr;
    _failedAttempts = 0;
}

template<class T>
bool RandomMovementGenerator<T>::DoUpdate(T*, uint32)
{
    return false;
}

template<>
bool RandomMovementGenerator<Creature>::DoUpdate(Creature* owner, uint32 diff)
{
    if (!owner || !owner->IsAlive())
        return true;

    if (HasFlag(MOVEMENTGENERATOR_FLAG_FINALIZED | MOVEMENTGENERATOR_FLAG_PAUSED))
        return true;

    if (owner->HasUnitState(UNIT_STATE_NOT_MOVE) || owner->IsMovementPreventedByCasting())
    {
        AddFlag(MOVEMENTGENERATOR_FLAG_INTERRUPTED);
        owner->StopMoving();
        ResetPaths();
        return true;
    }
    else
        RemoveFlag(MOVEMENTGENERATOR_FLAG_INTERRUPTED);

    _timer.Update(diff);

    // We have to make new splines on speed change
    if (HasFlag(MOVEMENTGENERATOR_FLAG_SPEED_UPDATE_PENDING) && !owner->movespline->Finalized())
    {
        ResetPaths();
        SetRandomLocation(owner);
    }
    else if (owner->movespline->Finalized())
    {
        if (!_wanderSteps)
        {
            // Retail seems to let a creature walk 2 up to 10 splines before triggering a pause
            _wanderSteps = urand(1, ((_maxWanderDistance <= MIN_WANDER_DISTANCE) ? 2 : 8));
            _timer.Reset(urand(6, 12) * IN_MILLISECONDS); // Retails seems to use rounded numbers so we do as well
        }
        if (_timer.Passed())
            SetRandomLocation(owner);
    }

    return true;
}

template<class T>
void RandomMovementGenerator<T>::DoDeactivate(T*) { }

template<>
void RandomMovementGenerator<Creature>::DoDeactivate(Creature* owner)
{
    AddFlag(MOVEMENTGENERATOR_FLAG_DEACTIVATED);
    owner->ClearUnitState(UNIT_STATE_ROAMING_MOVE);
}

template<class T>
void RandomMovementGenerator<T>::DoFinalize(T*, bool, bool) { }

template<>
void RandomMovementGenerator<Creature>::DoFinalize(Creature* owner, bool active, bool/* movementInform*/)
{
    AddFlag(MOVEMENTGENERATOR_FLAG_FINALIZED);
    if (active)
    {
        owner->ClearUnitState(UNIT_STATE_ROAMING_MOVE);
        owner->StopMoving();

        // TODO: Research if this modification is needed, which most likely isnt
        owner->SetWalk(false);
    }
}
