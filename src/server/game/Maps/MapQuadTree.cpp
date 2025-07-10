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

#include "MapQuadTree.h"
#include "Player.h"
#include "Creature.h"
#include "GameObject.h"
#include "DynamicObject.h"
#include "Corpse.h"
#include "Entities/Object/Object.h"

MapQuadTree::MapQuadTree(float minX, float minY, float maxX, float maxY)
    : playerTree(minX, minY, maxX, maxY),
    gameObjectTree(minX, minY, maxX, maxY),
    gridCreatureTree(minX, minY, maxX, maxY),
    worldCreatureTree(minX, minY, maxX, maxY),
    gridDynamicObjectTree(minX, minY, maxX, maxY),
    worldDynamicObjectTree(minX, minY, maxX, maxY),
    gridCorpseTree(minX, minY, maxX, maxY),
    worldCorpseTree(minX, minY, maxX, maxY)
{}

template<class T>
struct always_false : std::false_type {};

template<typename T>
void MapQuadTree::Insert(T* object)
{
    if constexpr (std::is_same_v<T, Player>)
        playerTree.Insert(object);
    else if constexpr (std::is_same_v<T, GameObject>)
        gameObjectTree.Insert(object);
    else if constexpr (std::is_same_v<T, Creature>)
        (object->IsStoredInWorldObjectGridContainer() ? worldCreatureTree : gridCreatureTree).Insert(object);
    else if constexpr (std::is_same_v<T, DynamicObject>)
        (object->IsStoredInWorldObjectGridContainer() ? worldDynamicObjectTree : gridDynamicObjectTree).Insert(object);
    else if constexpr (std::is_same_v<T, Corpse>)
        (object->IsStoredInWorldObjectGridContainer() ? worldCorpseTree : gridCorpseTree).Insert(object);
    else
        static_assert(always_false<T>::value, "Unsupported type for MapQuadTree::Insert");
}

void MapQuadTree::Clear()
{
    playerTree.Clear();
    gameObjectTree.Clear();
    gridCreatureTree.Clear();
    worldCreatureTree.Clear();
    gridDynamicObjectTree.Clear();
    worldDynamicObjectTree.Clear();
    gridCorpseTree.Clear();
    worldCorpseTree.Clear();
}

template<typename Func>
void MapQuadTree::QueryRange(uint32_t mask, float minX, float minY, float maxX, float maxY, Func&& func) const
{
    if (mask & MAPQT_WORLD_PLAYER)
        playerTree.QueryRange(minX, minY, maxX, maxY, func);
    if (mask & MAPQT_GRID_GAMEOBJECT)
        gameObjectTree.QueryRange(minX, minY, maxX, maxY, func);
    if (mask & MAPQT_GRID_CREATURE)
        gridCreatureTree.QueryRange(minX, minY, maxX, maxY, func);
    if (mask & MAPQT_WORLD_CREATURE)
        worldCreatureTree.QueryRange(minX, minY, maxX, maxY, func);
    if (mask & MAPQT_GRID_DYNAMICOBJ)
        gridDynamicObjectTree.QueryRange(minX, minY, maxX, maxY, func);
    if (mask & MAPQT_WORLD_DYNAMICOBJ)
        worldDynamicObjectTree.QueryRange(minX, minY, maxX, maxY, func);
    if (mask & MAPQT_GRID_CORPSE)
        gridCorpseTree.QueryRange(minX, minY, maxX, maxY, func);
    if (mask & MAPQT_WORLD_CORPSE)
        worldCorpseTree.QueryRange(minX, minY, maxX, maxY, func);
}

template<typename Func>
void MapQuadTree::QueryCircle(uint32_t mask, float centerX, float centerY, float radius, Func&& func) const
{
    if (mask & MAPQT_WORLD_PLAYER)
        playerTree.QueryCircle(centerX, centerY, radius, func);
    if (mask & MAPQT_GRID_GAMEOBJECT)
        gameObjectTree.QueryCircle(centerX, centerY, radius, func);
    if (mask & MAPQT_GRID_CREATURE)
        gridCreatureTree.QueryCircle(centerX, centerY, radius, func);
    if (mask & MAPQT_WORLD_CREATURE)
        worldCreatureTree.QueryCircle(centerX, centerY, radius, func);
    if (mask & MAPQT_GRID_DYNAMICOBJ)
        gridDynamicObjectTree.QueryCircle(centerX, centerY, radius, func);
    if (mask & MAPQT_WORLD_DYNAMICOBJ)
        worldDynamicObjectTree.QueryCircle(centerX, centerY, radius, func);
    if (mask & MAPQT_GRID_CORPSE)
        gridCorpseTree.QueryCircle(centerX, centerY, radius, func);
    if (mask & MAPQT_WORLD_CORPSE)
        worldCorpseTree.QueryCircle(centerX, centerY, radius, func);
}

// Explicit template instantiation for common function pointer and lambda types
template void MapQuadTree::QueryRange<float(*)(WorldObject*)>(uint32_t, float, float, float, float, float(*&&)(WorldObject*)) const;
template void MapQuadTree::QueryCircle<float(*)(WorldObject*)>(uint32_t, float, float, float, float(*&&)(WorldObject*)) const;