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

#ifndef TRINITY_MAP_QUADTREE_H
#define TRINITY_MAP_QUADTREE_H

#include "QuadTree.h"
#include <cstdint>

class Player;
class Creature;
class GameObject;
class DynamicObject;
class Corpse;
class WorldObject;

enum MapQuadTreeMask : uint32_t
{
    MAPQT_WORLD_PLAYER       = 0x01,
    MAPQT_GRID_GAMEOBJECT    = 0x02,
    MAPQT_GRID_CREATURE      = 0x04,
    MAPQT_WORLD_CREATURE     = 0x08,
    MAPQT_GRID_DYNAMICOBJ    = 0x10,
    MAPQT_WORLD_DYNAMICOBJ   = 0x20,
    MAPQT_GRID_CORPSE        = 0x40,
    MAPQT_WORLD_CORPSE       = 0x80,

    MAPQT_ALL_GRID = MAPQT_GRID_GAMEOBJECT | MAPQT_GRID_CREATURE | MAPQT_GRID_DYNAMICOBJ | MAPQT_GRID_CORPSE,
    MAPQT_ALL_WORLD = MAPQT_WORLD_PLAYER | MAPQT_WORLD_CREATURE | MAPQT_WORLD_DYNAMICOBJ | MAPQT_WORLD_CORPSE,
    MAPQT_ALL = MAPQT_ALL_GRID | MAPQT_ALL_WORLD
};

class MapQuadTree
{
public:
    MapQuadTree(Bounds bounds);

    template<class T>
    struct always_false : std::false_type {};

    template<typename T>
    void Insert(T* object)
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

    void Clear()
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
    void QueryRange(uint32_t mask, float minX, float minY, float maxX, float maxY, Func&& func) const
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
    void QueryCircle(uint32_t mask, float centerX, float centerY, float radius, Func&& func) const
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

private:
    QuadTree<Player> playerTree;               // always World
    QuadTree<GameObject> gameObjectTree;           // always Grid
    QuadTree<Creature> gridCreatureTree;
    QuadTree<Creature> worldCreatureTree;
    QuadTree<DynamicObject> gridDynamicObjectTree;
    QuadTree<DynamicObject> worldDynamicObjectTree;
    QuadTree<Corpse> gridCorpseTree;
    QuadTree<Corpse> worldCorpseTree;
};

#endif // TRINITY_MAP_QUADTREE_H