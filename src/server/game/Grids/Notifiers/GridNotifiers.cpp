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

#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateData.h"
#include "Transport.h"
#include "ObjectAccessor.h"
#include "CellImpl.h"
// @tswow-begin
#include "TSUnit.h"
#include "TSCreature.h"
// @tswow-end

using namespace Trinity;

void VisibleNotifier::SendToSelf()
{
    // at this moment i_clientGUIDs have guids that not iterate at grid level checks
    // but exist one case when this possible and object not out of range: transports
    if (GenericTransport* transport = i_player.GetTransport())
    {
        for (GenericTransport::PassengerSet::iterator itr = transport->GetPassengers().begin(); itr != transport->GetPassengers().end(); ++itr)
        {
            if (vis_guids.find((*itr)->GetGUID()) != vis_guids.end())
            {
                vis_guids.erase((*itr)->GetGUID());

                switch ((*itr)->GetTypeId())
                {
                    case TYPEID_GAMEOBJECT:
                        i_player.UpdateVisibilityOf((*itr)->ToGameObject(), i_data, i_visibleNow);
                        break;
                    case TYPEID_PLAYER:
                        i_player.UpdateVisibilityOf((*itr)->ToPlayer(), i_data, i_visibleNow);
                        if (!(*itr)->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
                            (*itr)->ToPlayer()->UpdateVisibilityOf(&i_player);
                        break;
                    case TYPEID_UNIT:
                        i_player.UpdateVisibilityOf((*itr)->ToCreature(), i_data, i_visibleNow);
                        break;
                    case TYPEID_DYNAMICOBJECT:
                        i_player.UpdateVisibilityOf((*itr)->ToDynObject(), i_data, i_visibleNow);
                        break;
                    default:
                        break;
                }
            }
        }
    }

    for (auto it = vis_guids.begin(); it != vis_guids.end(); ++it)
    {
        i_player.m_clientGUIDs.erase(*it);
        i_data.AddOutOfRangeGUID(*it);

        if (it->IsPlayer())
        {
            Player* player = ObjectAccessor::FindPlayer(*it);
            if (player && !player->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
                player->UpdateVisibilityOf(&i_player);
        }
    }

    if (!i_data.HasData())
        return;

    WorldPacket packet;
    i_data.BuildPacket(&packet);
    i_player.SendDirectMessage(&packet);

    for (std::set<Unit*>::const_iterator it = i_visibleNow.begin(); it != i_visibleNow.end(); ++it)
        i_player.SendInitialVisiblePackets(*it);
}

void VisibleChangesNotifier::operator()(Player* p)
{
    if (p == &i_object)
        return;

    p->UpdateVisibilityOf(&i_object);

    if (p->HasSharedVision())
    {
        for (SharedVisionList::const_iterator i = p->GetSharedVisionList().begin();
             i != p->GetSharedVisionList().end(); ++i)
        {
            if ((*i)->m_seer == p)
                (*i)->UpdateVisibilityOf(&i_object);
        }
    }
}

void VisibleChangesNotifier::operator()(Creature* c)
{
    if (c->HasSharedVision())
    {
        for (SharedVisionList::const_iterator i = c->GetSharedVisionList().begin();
             i != c->GetSharedVisionList().end(); ++i)
        {
            if ((*i)->m_seer == c)
                (*i)->UpdateVisibilityOf(&i_object);
        }
    }
}

void VisibleChangesNotifier::operator()(DynamicObject* d)
{
    if (Unit* caster = d->GetCaster())
        if (Player* player = caster->ToPlayer())
            if (player->m_seer == d)
                player->UpdateVisibilityOf(&i_object);
}

inline void CreatureUnitRelocationWorker(Creature* c, Unit* u)
{
    if (!u->IsAlive() || !c->IsAlive() || c == u || u->IsInFlight())
        return;

    if (!c->HasUnitState(UNIT_STATE_SIGHTLESS))
    {
        // @tswow-begin
        FIRE_ID(c->GetCreatureTemplate()->events.id,Creature,OnMoveInLOS,TSCreature(c),TSUnit(u));
        // @tswow-end
        if (c->IsAIEnabled() && c->CanSeeOrDetect(u, false, true))
            c->AI()->MoveInLineOfSight_Safe(u);
        else
            if (u->GetTypeId() == TYPEID_PLAYER && u->HasStealthAura() && c->IsAIEnabled() && c->CanSeeOrDetect(u, false, true, true))
                c->AI()->TriggerAlert(u);
    }
}

void PlayerRelocationNotifier::operator()(Player* p)
{
    vis_guids.erase(p->GetGUID());

    i_player.UpdateVisibilityOf(p, i_data, i_visibleNow);

    if (p->m_seer->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
        return;

    p->UpdateVisibilityOf(&i_player);
}

void PlayerRelocationNotifier::operator()(Creature* c)
{
    bool relocated_for_ai = (&i_player == i_player.m_seer);

    vis_guids.erase(c->GetGUID());

    i_player.UpdateVisibilityOf(c, i_data, i_visibleNow);

    if (relocated_for_ai && !c->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
        CreatureUnitRelocationWorker(c, &i_player);
}

void CreatureRelocationNotifier::operator()(Player* p)
{
    if (!p->m_seer->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
        p->UpdateVisibilityOf(&i_creature);

    CreatureUnitRelocationWorker(&i_creature, p);
}

void CreatureRelocationNotifier::operator()(Creature* c)
{
    if (!i_creature.IsAlive())
        return;

    CreatureUnitRelocationWorker(&i_creature, c);

    if (!c->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
        CreatureUnitRelocationWorker(c, &i_creature);
}

void AIRelocationNotifier::operator()(Creature* c)
{
    CreatureUnitRelocationWorker(c, &i_unit);
    if (isCreature)
        CreatureUnitRelocationWorker((Creature*)&i_unit, c);
}

void MessageDistDeliverer::operator()(Player* p)
{
    // @tswow-begin
    if (!p->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (required3dDist)
    {
        if (p->GetExactDistSq(i_source) > i_distSq)
            return;
    }
    else
    {
        if (p->GetExactDist2dSq(i_source) > i_distSq)
            return;
    }

    // Send packet to all who are sharing the player's vision
    if (p->HasSharedVision())
    {
        for (SharedVisionList::const_iterator i = p->GetSharedVisionList().begin();
             i != p->GetSharedVisionList().end(); ++i)
        {
            if ((*i)->m_seer == p)
                SendPacket(*i);
        }
    }

    if (p->m_seer == p || p->GetVehicle())
        SendPacket(p);
}

void MessageDistDeliverer::operator()(Creature* c)
{
    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (required3dDist)
    {
        if (c->GetExactDistSq(i_source) > i_distSq)
            return;
    }
    else
    {
        if (c->GetExactDist2dSq(i_source) > i_distSq)
            return;
    }

    // Send packet to all who are sharing the creature's vision
    if (c->HasSharedVision())
    {
        for (SharedVisionList::const_iterator i = c->GetSharedVisionList().begin();
             i != c->GetSharedVisionList().end(); ++i)
        {
            if ((*i)->m_seer == c)
                SendPacket(*i);
        }
    }
}

void MessageDistDeliverer::operator()(DynamicObject* d)
{
    // @tswow-begin
    if (!d->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (required3dDist)
    {
        if (d->GetExactDistSq(i_source) > i_distSq)
            return;
    }
    else
    {
        if (d->GetExactDist2dSq(i_source) > i_distSq)
            return;
    }

    if (Unit* caster = d->GetCaster())
    {
        // Send packet back to the caster if the caster has vision of dynamic object
        Player* p = caster->ToPlayer();
        if (p && p->m_seer == d)
            SendPacket(p);
    }
}

void MessageDistDelivererToHostile::operator()(Player* p)
{
    // @tswow-begin
    if (!p->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (p->GetExactDist2dSq(i_source) > i_distSq)
        return;

    // Send packet to all who are sharing the player's vision
    if (p->HasSharedVision())
    {
        for (SharedVisionList::const_iterator i = p->GetSharedVisionList().begin();
             i != p->GetSharedVisionList().end(); ++i)
        {
            if ((*i)->m_seer == p)
                SendPacket(*i);
        }
    }

    if (p->m_seer == p || p->GetVehicle())
        SendPacket(p);
}

void MessageDistDelivererToHostile::operator()(Creature* c)
{
    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (c->GetExactDist2dSq(i_source) > i_distSq)
        return;

    // Send packet to all who are sharing the creature's vision
    if (c->HasSharedVision())
    {
        for (SharedVisionList::const_iterator i = c->GetSharedVisionList().begin();
             i != c->GetSharedVisionList().end(); ++i)
        {
            if ((*i)->m_seer == c)
                SendPacket(*i);
        }
    }
}

void MessageDistDelivererToHostile::operator()(DynamicObject* d)
{
    // @tswow-begin
    if (!d->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (d->GetExactDist2dSq(i_source) > i_distSq)
        return;

    if (Unit* caster = d->GetCaster())
    {
        // Send packet back to the caster if the caster has vision of dynamic object
        Player* p = caster->ToPlayer();
        if (p && p->m_seer == d)
            SendPacket(p);
    }
}

bool AnyDeadUnitObjectInRangeCheck::operator()(Player* u)
{
    return !u->IsAlive() && !u->HasAuraType(SPELL_AURA_GHOST) && i_searchObj->IsWithinDistInMap(u, i_range);
}

bool AnyDeadUnitObjectInRangeCheck::operator()(Corpse* u)
{
    return u->GetType() != CORPSE_BONES && i_searchObj->IsWithinDistInMap(u, i_range);
}

bool AnyDeadUnitObjectInRangeCheck::operator()(Creature* u)
{
    return !u->IsAlive() && i_searchObj->IsWithinDistInMap(u, i_range);
}

bool AnyDeadUnitSpellTargetInRangeCheck::operator()(Player* u)
{
    return AnyDeadUnitObjectInRangeCheck::operator()(u) && WorldObjectSpellTargetCheck::operator()(u);
}

bool AnyDeadUnitSpellTargetInRangeCheck::operator()(Corpse* u)
{
    return AnyDeadUnitObjectInRangeCheck::operator()(u) && WorldObjectSpellTargetCheck::operator()(u);
}

bool AnyDeadUnitSpellTargetInRangeCheck::operator()(Creature* u)
{
    return AnyDeadUnitObjectInRangeCheck::operator()(u) && WorldObjectSpellTargetCheck::operator()(u);
}
