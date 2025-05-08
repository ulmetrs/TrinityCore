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

#ifndef TRANSPORTS_H
#define TRANSPORTS_H

#include "GameObject.h"
#include "TransportMgr.h"
#include "VehicleDefines.h"

struct CreatureData;

// Transport is Type 15 MO Transport
// Elevator is Type 11 Transport
class TC_GAME_API GenericTransport : public GameObject, public TransportBase
{
    public:
    typedef std::set<WorldObject*> PassengerSet;

        GenericTransport() : m_movementStarted(0), m_stopped(false){}
        void AddPassenger(WorldObject* passenger);
        void RemovePassenger(WorldObject* passenger);
        void AddFollowerToTransport(Unit* passenger, Unit* follower);
        void RemoveFollowerFromTransport(Unit* passenger, Unit* follower);

        PassengerSet& GetPassengers() { return _passengers; }

        void UpdatePosition(float x, float y, float z, float o);

        /// This method transforms supplied transport offsets into global coordinates
        void CalculatePassengerPosition(float& x, float& y, float& z, float* o = nullptr) const override
        {
            TransportBase::CalculatePassengerPosition(x, y, z, o, GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
        }

        /// This method transforms supplied global coordinates into local offsets
        void CalculatePassengerOffset(float& x, float& y, float& z, float* o = nullptr) const override
        {
            TransportBase::CalculatePassengerOffset(x, y, z, o, GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
        }

        //! Needed when transport moves from inactive to active grid
        virtual void LoadStaticPassengers() {}

        //! Needed when transport enters inactive grid
        virtual void UnloadStaticPassengers() {}

    protected:
        void UpdatePassengerPositions(PassengerSet& passengers);

        // Used for ElevatorTransport
        uint32 m_movementStarted;
        bool m_stopped;

        PassengerSet _passengers;
        PassengerSet::iterator _passengerTeleportItr;
        PassengerSet _staticPassengers; // Used only for Transport, easier to port ...


};

class TC_GAME_API ElevatorTransport : public GenericTransport
{
    public:
        // Don't really need a new Create for ElevatorTransport since we already initialise m_GoValue in GameObject::Create
        //bool Create(uint32 dbGuid, uint32 guidlow, uint32 name_id, Map* map, uint32 phaseMask, Position const& pos, 
        //    const QuaternionData& rotation = QuaternionData(), uint32 animprogress = 255, GOState go_state = GO_STATE_READY) override;
        // Alternatively, instead of overriding Update, we can do like in TC master branch and implement m_goTypeImpl and call this Update()
        // in generic GameObject->Update() code
        void Update(const uint32 diff) override;
};

class TC_GAME_API Transport : public GenericTransport
{
        friend Transport* TransportMgr::CreateTransport(uint32, ObjectGuid::LowType, Map*); // Used for MO_TRANSPORT

        explicit Transport();
    public:

        ~Transport();

        bool Create(ObjectGuid::LowType guidlow, uint32 entry, uint32 mapid, float x, float y, float z, float ang, uint32 animprogress); // ?
        void CleanupsBeforeDelete(bool finalCleanup = true) override;

        void Update(uint32 diff) override;
        void DelayedUpdate(uint32 diff);

        void BuildUpdate(UpdateDataMapType& data_map) override;

        Creature* CreateNPCPassenger(ObjectGuid::LowType guid, CreatureData const* data);
        GameObject* CreateGOPassenger(ObjectGuid::LowType guid, GameObjectData const* data);

        /**
        * @fn bool Transport::SummonPassenger(uint64, Position const&, TempSummonType, SummonPropertiesEntry const*, uint32, Unit*, uint32, uint32)
        *
        * @brief Temporarily summons a creature as passenger on this transport.
        *
        * @param entry Id of the creature from creature_template table
        * @param pos Initial position of the creature (transport offsets)
        * @param summonType
        * @param properties
        * @param duration Determines how long the creauture will exist in world depending on @summonType (in milliseconds)
        * @param summoner Summoner of the creature (for AI purposes)
        * @param spellId
        * @param vehId If set, this value overrides vehicle id from creature_template that the creature will use
        *
        * @return Summoned creature.
        */
        TempSummon* SummonPassenger(uint32 entry, Position const& pos, TempSummonType summonType, SummonPropertiesEntry const* properties = nullptr, uint32 duration = 0, Unit* summoner = nullptr, uint32 spellId = 0, uint32 vehId = 0);

        uint32 GetTransportPeriod() const override { return GetUInt32Value(GAMEOBJECT_LEVEL); }
        void SetPeriod(uint32 period) { SetLevel(period); }
        //GetTimer() const { return GetGOValue()->Transport.PathProgress; } not used anywhere ??

        KeyFrameVec const& GetKeyFrames() const { return _transportInfo->keyFrames; }

        //! Needed when transport moves from inactive to active grid
        void LoadStaticPassengers() override;

        //! Needed when transport enters inactive grid
        void UnloadStaticPassengers() override;

        void EnableMovement(bool enabled);

        void SetDelayedAddModelToMap() { _delayedAddModel = true; }

        TransportTemplate const* GetTransportTemplate() const { return _transportInfo; }

        std::string GetDebugInfo() const override;

    private:
        void MoveToNextWaypoint();
        float CalculateSegmentPos(float perc);
        bool TeleportTransport(uint32 newMapid, float x, float y, float z, float o);
        void DelayedTeleportTransport();
        void DoEventIfAny(KeyFrame const& node, bool departure);

        //! Helpers to know if stop frame was reached
        bool IsMoving() const { return _isMoving; }
        void SetMoving(bool val) { _isMoving = val; }

        TransportTemplate const* _transportInfo;

        KeyFrameVec::const_iterator _currentFrame;
        KeyFrameVec::const_iterator _nextFrame;
        TimeTracker _positionChangeTimer;
        bool _isMoving;
        bool _pendingStop;

        //! These are needed to properly control events triggering only once for each frame
        bool _triggeredArrivalEvent;
        bool _triggeredDepartureEvent;

        bool _delayedAddModel;
        bool _delayedTeleport;
};

#endif
