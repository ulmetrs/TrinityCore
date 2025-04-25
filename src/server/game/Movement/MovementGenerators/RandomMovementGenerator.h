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

#ifndef TRINITY_RANDOMMOTIONGENERATOR_H
#define TRINITY_RANDOMMOTIONGENERATOR_H

#include "MovementGenerator.h"
#include "Position.h"
#include "Timer.h"
#include "Movement/Spline/MoveSplineInitArgs.h"

#define MIN_WANDER_DISTANCE 1.0f
#define NUM_WANDER_POINTS 12

class PathGenerator;

template<class T>
class RandomMovementGenerator : public MovementGeneratorMedium<T, RandomMovementGenerator<T>>
{
    public:
        explicit RandomMovementGenerator(float distance = 0.0f);

        MovementGeneratorType GetMovementGeneratorType() const override;

        void Pause(uint32 timer = 0) override;
        void Resume(uint32 overrideTimer = 0) override;

        void DoInitialize(T*);
        void DoReset(T*);
        bool DoUpdate(T*, uint32);
        void DoDeactivate(T*);
        void DoFinalize(T*, bool, bool);

        void UnitSpeedChanged() override { RandomMovementGenerator<T>::AddFlag(MOVEMENTGENERATOR_FLAG_SPEED_UPDATE_PENDING); }

    private:
        void SetRandomLocation(T*);

        Position _reference;
        TimeTracker _timer;
        float _wanderDistance;
        uint8 _wanderSteps;
        uint8 _angleIndex;
        std::vector<float> _angles;
        uint8 _pathIndex;
        std::vector<Movement::PointsArray> _paths;
        std::unique_ptr<PathGenerator> _pathGenerator;
};

#endif
