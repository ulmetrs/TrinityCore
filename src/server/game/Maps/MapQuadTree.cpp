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

MapQuadTree::MapQuadTree(float minX, float minY, float maxX, float maxY)
    : playerTree(minX, minY, maxX, maxY),
      creatureTree(minX, minY, maxX, maxY),
      gameObjectTree(minX, minY, maxX, maxY),
      dynamicObjectTree(minX, minY, maxX, maxY),
      corpseTree(minX, minY, maxX, maxY)
{}

// Helper for static_assert false in templates:
template<class>
struct always_false : std::false_type {};

template<typename T>
void MapQuadTree::Insert(T* object) {
    if constexpr (std::is_same_v<T, Player>)
        playerTree.Insert(object->GetPositionX(), object->GetPositionY(), object);
    else if constexpr (std::is_same_v<T, Creature>)
        creatureTree.Insert(object->GetPositionX(), object->GetPositionY(), object);
    else if constexpr (std::is_same_v<T, GameObject>)
        gameObjectTree.Insert(object->GetPositionX(), object->GetPositionY(), object);
    else if constexpr (std::is_same_v<T, DynamicObject>)
        dynamicObjectTree.Insert(object->GetPositionX(), object->GetPositionY(), object);
    else if constexpr (std::is_same_v<T, Corpse>)
        corpseTree.Insert(object->GetPositionX(), object->GetPositionY(), object);
    else
        static_assert(always_false<T>::value, "Unsupported type for MapQuadTree::Insert");
}

template<typename T>
void MapQuadTree::Remove(T* object) {
    if constexpr (std::is_same_v<T, Player>)
        playerTree.Remove(object->GetPositionX(), object->GetPositionY(), object);
    else if constexpr (std::is_same_v<T, Creature>)
        creatureTree.Remove(object->GetPositionX(), object->GetPositionY(), object);
    else if constexpr (std::is_same_v<T, GameObject>)
        gameObjectTree.Remove(object->GetPositionX(), object->GetPositionY(), object);
    else if constexpr (std::is_same_v<T, DynamicObject>)
        dynamicObjectTree.Remove(object->GetPositionX(), object->GetPositionY(), object);
    else if constexpr (std::is_same_v<T, Corpse>)
        corpseTree.Remove(object->GetPositionX(), object->GetPositionY(), object);
    else
        static_assert(always_false<T>::value, "Unsupported type for MapQuadTree::Remove");
}

void MapQuadTree::Clear()
{
    playerTree.Clear();
    creatureTree.Clear();
    gameObjectTree.Clear();
    dynamicObjectTree.Clear();
    corpseTree.Clear();
}

template<typename T, typename Func>
void MapQuadTree::QueryRange(float minX, float minY, float maxX, float maxY, Func&& func) const
{
    if constexpr (std::is_same_v<T, Player>)
        playerTree.QueryRange(minX, minY, maxX, maxY, std::forward<Func>(func));
    else if constexpr (std::is_same_v<T, Creature>)
        creatureTree.QueryRange(minX, minY, maxX, maxY, std::forward<Func>(func));
    else if constexpr (std::is_same_v<T, GameObject>)
        gameObjectTree.QueryRange(minX, minY, maxX, maxY, std::forward<Func>(func));
    else if constexpr (std::is_same_v<T, DynamicObject>)
        dynamicObjectTree.QueryRange(minX, minY, maxX, maxY, std::forward<Func>(func));
    else if constexpr (std::is_same_v<T, Corpse>)
        corpseTree.QueryRange(minX, minY, maxX, maxY, std::forward<Func>(func));
    else
        static_assert(always_false<T>::value, "Unsupported type for MapQuadTree::QueryRange");
}

template<typename T, typename Func>
void MapQuadTree::QueryCircle(float centerX, float centerY, float radius, Func&& func) const
{
    if constexpr (std::is_same_v<T, Player>)
        playerTree.QueryCircle(centerX, centerY, radius, std::forward<Func>(func));
    else if constexpr (std::is_same_v<T, Creature>)
        creatureTree.QueryCircle(centerX, centerY, radius, std::forward<Func>(func));
    else if constexpr (std::is_same_v<T, GameObject>)
        gameObjectTree.QueryCircle(centerX, centerY, radius, std::forward<Func>(func));
    else if constexpr (std::is_same_v<T, DynamicObject>)
        dynamicObjectTree.QueryCircle(centerX, centerY, radius, std::forward<Func>(func));
    else if constexpr (std::is_same_v<T, Corpse>)
        corpseTree.QueryCircle(centerX, centerY, radius, std::forward<Func>(func));
    else
        static_assert(always_false<T>::value, "Unsupported type for MapQuadTree::QueryCircle");
}