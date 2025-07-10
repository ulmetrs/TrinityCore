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
    MapQuadTree(float minX, float minY, float maxX, float maxY);
    void Clear();

    // Insert
    template<typename T>
    void Insert(T* object);
    void Insert(WorldObject* object);

    // Query (range or circle) using type mask
    template<typename Func>
    void QueryRange(uint32_t mask, float minX, float minY, float maxX, float maxY, Func&& func) const;
    template<typename Func>
    void QueryCircle(uint32_t mask, float centerX, float centerY, float radius, Func&& func) const;

private:
    QuadTree playerTree;               // always World
    QuadTree gameObjectTree;           // always Grid
    QuadTree gridCreatureTree;
    QuadTree worldCreatureTree;
    QuadTree gridDynamicObjectTree;
    QuadTree worldDynamicObjectTree;
    QuadTree gridCorpseTree;
    QuadTree worldCorpseTree;
};

#endif // TRINITY_MAP_QUADTREE_H