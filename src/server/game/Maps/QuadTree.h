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

#ifndef TRINITY_QUADTREE_H
#define TRINITY_QUADTREE_H

#include "GridDefines.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <cmath>

class Player;
class GameObject;
class Creature;
class DynamicObject;
class Corpse;

struct Bounds
{
    float minX, minY, maxX, maxY;
};

template<typename T>
class QuadTree;

template<typename T>
class QuadNode
{
    friend class QuadTree<T>;
public:
    QuadNode(Bounds bounds, int depth, int maxObjects, int maxDepth)
        : _bounds(bounds), depth(depth), maxObjects(maxObjects), maxDepth(maxDepth) {}

    bool IsLeaf() const { return !children[0]; }
    void Remove(T* obj);
    void Subdivide();
    int GetChildIndex(float x, float y) const;
    Bounds GetBounds() const { return _bounds; }
    int GetDepth() const { return depth; }

private:
    Bounds _bounds;
    std::vector<T*> objects;
    std::unique_ptr<QuadNode<T>> children[4];
    int depth;
    int maxObjects;
    int maxDepth;
};

template<typename T>
class QuadTree
{
public:
    QuadTree(Bounds bounds, int maxObjects = 8, float cellSize = SIZE_OF_GRID_CELL);
    void Clear();
    void Insert(T* obj);

    template<typename Func>
    void QueryRange(float minX, float minY, float maxX, float maxY, Func&& visitor) const;

    template<typename Func>
    void QueryCircle(float centerX, float centerY, float radius, Func&& visitor) const;

private:
    std::unique_ptr<QuadNode<T>> root;
    Bounds _bounds;
    int _maxObjects;
    int _maxDepth;
};

#include "QuadTree.tpp"

#endif // TRINITY_QUADTREE_H