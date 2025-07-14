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

#ifndef TRINITY_GRIDNOTIFIERSIMPL_H
#define TRINITY_GRIDNOTIFIERSIMPL_H

#include "GridNotifiers.h"
#include "Corpse.h"
#include "CreatureAI.h"
#include "Player.h"
#include "SpellAuras.h"
#include "UpdateData.h"
#include "WorldPacket.h"
#include "WorldSession.h"

// SEARCHERS & LIST SEARCHERS & WORKERS

// WorldObject searchers & workers

template<class Check>
void Trinity::WorldObjectSearcher<Check>::operator()(Player* p)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_PLAYER))
        return;

    if (i_object)
        return;

    // @tswow-begin
    if (!p->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(p))
        i_object = p;
}

template<class Check>
void Trinity::WorldObjectSearcher<Check>::operator()(GameObject* g)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_GAMEOBJECT))
        return;

    if (i_object)
        return;

    // @tswow-begin
    if (!g->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(g))
        i_object = g;
}

template<class Check>
void Trinity::WorldObjectSearcher<Check>::operator()(Creature* c)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_CREATURE))
        return;

    if (i_object)
        return;

    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(c))
        i_object = c;
}

template<class Check>
void Trinity::WorldObjectSearcher<Check>::operator()(DynamicObject* d)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_DYNAMICOBJECT))
        return;

    if (i_object)
        return;

    // @tswow-begin
    if (!d->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(d))
        i_object = d;
}

template<class Check>
void Trinity::WorldObjectSearcher<Check>::operator()(Corpse* c)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_CORPSE))
        return;

    if (i_object)
        return;

    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(c))
        i_object = c;
}

template<class Check>
void Trinity::WorldObjectLastSearcher<Check>::operator()(Player* p)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_PLAYER))
        return;

    // @tswow-begin
    if (!p->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(p))
        i_object = p;
}

template<class Check>
void Trinity::WorldObjectLastSearcher<Check>::operator()(GameObject* g)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_GAMEOBJECT))
        return;

    // @tswow-begin
    if (!g->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(g))
        i_object = g;
}

template<class Check>
void Trinity::WorldObjectLastSearcher<Check>::operator()(Creature* c)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_CREATURE))
        return;

    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(c))
        i_object = c;
}

template<class Check>
void Trinity::WorldObjectLastSearcher<Check>::operator()(DynamicObject* d)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_DYNAMICOBJECT))
        return;

    // @tswow-begin
    if (!d->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(d))
        i_object = d;
}

template<class Check>
void Trinity::WorldObjectLastSearcher<Check>::operator()(Corpse* c)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_CORPSE))
        return;

    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(c))
        i_object = c;
}

template<class Check>
void Trinity::WorldObjectListSearcher<Check>::operator()(Player* p)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_PLAYER))
        return;

    if (i_check(p))
        Insert(p);
}

template<class Check>
void Trinity::WorldObjectListSearcher<Check>::operator()(Creature* c)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_CREATURE))
        return;

    if (i_check(c))
        Insert(c);
}

template<class Check>
void Trinity::WorldObjectListSearcher<Check>::operator()(Corpse* c)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_CORPSE))
        return;

    if (i_check(c))
        Insert(c);
}

template<class Check>
void Trinity::WorldObjectListSearcher<Check>::operator()(GameObject* g)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_GAMEOBJECT))
        return;

    if (i_check(g))
        Insert(g);
}

template<class Check>
void Trinity::WorldObjectListSearcher<Check>::operator()(DynamicObject* d)
{
    if (!(i_mapTypeMask & GRID_MAP_TYPE_MASK_DYNAMICOBJECT))
        return;

    if (i_check(d))
        Insert(d);
}

// Gameobject searchers

template<class Check>
void Trinity::GameObjectSearcher<Check>::operator()(GameObject* g)
{
    // already found
    if (i_object)
        return;

    // @tswow-begin
    if (!g->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(g))
        i_object = g;
}

template<class Check>
void Trinity::GameObjectLastSearcher<Check>::operator()(GameObject* g)
{
    // @tswow-begin
    if (!g->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(g))
        i_object = g;
}

template<class Check>
void Trinity::GameObjectListSearcher<Check>::operator()(GameObject* g)
{
    // @tswow-begin
    if (!g->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(g))
        Insert(g);
}

// Unit searchers

template<class Check>
void Trinity::UnitSearcher<Check>::operator()(Creature* c)
{
    // already found
    if (i_object)
        return;

    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(c))
        i_object = c;
}

template<class Check>
void Trinity::UnitSearcher<Check>::operator()(Player* p)
{
    // already found
    if (i_object)
        return;

    // @tswow-begin
    if (!p->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(p))
        i_object = p;
}

template<class Check>
void Trinity::UnitLastSearcher<Check>::operator()(Creature* c)
{
    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(c))
        i_object = c;
}

template<class Check>
void Trinity::UnitLastSearcher<Check>::operator()(Player* p)
{
    // @tswow-begin
    if (!p->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(p))
        i_object = p;
}

template<class Check>
void Trinity::UnitListSearcher<Check>::operator()(Player* p)
{
    // @tswow-begin
    if (!p->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(p))
        Insert(p);
}

template<class Check>
void Trinity::UnitListSearcher<Check>::operator()(Creature* c)
{
    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(c))
        Insert(c);
}

// Creature searchers

template<class Check>
void Trinity::CreatureSearcher<Check>::operator()(Creature* c)
{
    // already found
    if (i_object)
        return;

    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(c))
    {
        i_object = c;
        return;
    }
}

template<class Check>
void Trinity::CreatureLastSearcher<Check>::operator()(Creature* c)
{
    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(c))
        i_object = c;
}

template<class Check>
void Trinity::CreatureListSearcher<Check>::operator()(Creature* c)
{
    // @tswow-begin
    if (!c->InSamePhase(i_phaseMask, i_phase_id))
        return;
    // @tswow-end

    if (i_check(c))
        Insert(c);
}

template<class Check>
void Trinity::PlayerListSearcher<Check>::operator()(Player* p)
{
    // @tswow-begin
    if (p->InSamePhase(i_phaseMask, i_phase_id))
    // @tswow-end
        if (i_check(p))
            Insert(p);
}

template<class Check>
void Trinity::PlayerSearcher<Check>::operator()(Player* p)
{
    if (i_object)
        return;

    // @tswow-begin
    if (!p->InSamePhase(i_phaseMask, i_phase_id))
    // @tswow-end
        return;

    if (i_check(p))
        i_object = p;
}

template<class Check>
void Trinity::PlayerLastSearcher<Check>::operator()(Player* p)
{
    // @tswow-begin
    if (!p->InSamePhase(i_phaseMask, i_phase_id))
    // @tswow-end
        return;

    if (i_check(p))
        i_object = p;
}

template<class Builder>
void Trinity::LocalizedPacketDo<Builder>::operator()(Player* p)
{
    LocaleConstant loc_idx = p->GetSession()->GetSessionDbLocaleIndex();
    uint32 cache_idx = loc_idx+1;
    WorldPacket* data;

    // create if not cached yet
    if (i_data_cache.size() < cache_idx + 1 || !i_data_cache[cache_idx])
    {
        if (i_data_cache.size() < cache_idx + 1)
            i_data_cache.resize(cache_idx + 1);

        data = new WorldPacket();

        i_builder(*data, loc_idx);

        i_data_cache[cache_idx] = data;
    }
    else
        data = i_data_cache[cache_idx];

    p->SendDirectMessage(data);
}

template<class Builder>
void Trinity::LocalizedPacketListDo<Builder>::operator()(Player* p)
{
    LocaleConstant loc_idx = p->GetSession()->GetSessionDbLocaleIndex();
    uint32 cache_idx = loc_idx+1;
    WorldPacketList* data_list;

    // create if not cached yet
    if (i_data_cache.size() < cache_idx+1 || i_data_cache[cache_idx].empty())
    {
        if (i_data_cache.size() < cache_idx+1)
            i_data_cache.resize(cache_idx+1);

        data_list = &i_data_cache[cache_idx];

        i_builder(*data_list, loc_idx);
    }
    else
        data_list = &i_data_cache[cache_idx];

    for (size_t i = 0; i < data_list->size(); ++i)
        p->SendDirectMessage((*data_list)[i]);
}

#endif                                                      // TRINITY_GRIDNOTIFIERSIMPL_H
