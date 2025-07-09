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

class Player;
class Creature;
class GameObject;
class DynamicObject;
class Corpse;
class Map;
class SObjectMgr;

class MapQuadTree
{
public:
    MapQuadTree(float minX, float minY, float maxX, float maxY);
    template<typename T>
    void Insert(T* object);
    template<typename T>
    void Remove(T* object);
    void Clear();
    template<typename T, typename Func>
    void QueryRange(float minX, float minY, float maxX, float maxY, Func&& func) const;
    template<typename T, typename Func>
    void QueryCircle(float centerX, float centerY, float radius, Func&& func) const;

private:
    QuadTree<Player*> playerTree;
    QuadTree<Creature*> creatureTree;
    QuadTree<GameObject*> gameObjectTree;
    QuadTree<DynamicObject*> dynamicObjectTree;
    QuadTree<Corpse*> corpseTree;
};

#endif // TRINITY_MAP_QUADTREE_H