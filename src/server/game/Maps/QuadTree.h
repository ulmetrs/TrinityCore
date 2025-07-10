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

class WorldObject;

struct Bounds
{
    float minX, minY, maxX, maxY;
};

class QuadNode
{
    friend class QuadTree;
public:
    QuadNode(Bounds bounds, int depth, int maxObjects, int maxDepth);

    bool IsLeaf() const;
    void Remove(WorldObject* obj);
    void Subdivide();
    int GetChildIndex(float x, float y) const;

private:
    Bounds _bounds;
    std::vector<WorldObject*> objects;
    std::unique_ptr<QuadNode> children[4];
    int depth;
    int maxObjects, maxDepth;
};

class QuadTree
{
public:
    QuadTree(Bounds bounds, int maxObjects = 8, float cellSize = SIZE_OF_GRID_CELL);
    ~QuadTree();

    void Insert(WorldObject* object);
    void Clear();

    // Template methods must be defined in the header!
    template <typename Func>
    void QueryRange(float minX, float minY, float maxX, float maxY, Func&& visitor) const
    {
        std::function<void(const QuadNode*)> query = [&](const QuadNode* node)
        {
            if (node->_bounds.maxX < minX || node->_bounds.minX > maxX ||
                node->_bounds.maxY < minY || node->_bounds.minY > maxY)
                return;
            for (WorldObject* obj : node->objects)
            {
                float x = obj->GetPositionX();
                float y = obj->GetPositionY();
                if (x >= minX && x <= maxX && y >= minY && y <= maxY)
                    visitor(obj);
            }
            if (!node->IsLeaf())
            {
                for (const auto& child : node->children)
                    if (child)
                        query(child.get());
            }
        };
        query(root.get());
    }

    template <typename Func>
    void QueryCircle(float centerX, float centerY, float radius, Func&& visitor) const
    {
        float radiusSq = radius * radius;
        float minX = centerX - radius;
        float maxX = centerX + radius;
        float minY = centerY - radius;
        float maxY = centerY + radius;

        std::function<void(const QuadNode*)> query = [&](const QuadNode* node)
        {
            if (node->_bounds.maxX < minX || node->_bounds.minX > maxX ||
                node->_bounds.maxY < minY || node->_bounds.minY > maxY)
                return;
            for (WorldObject* obj : node->objects)
            {
                float x = obj->GetPositionX();
                float y = obj->GetPositionY();
                float dx = x - centerX;
                float dy = y - centerY;
                if (dx * dx + dy * dy <= radiusSq)
                    visitor(obj);
            }
            if (!node->IsLeaf())
            {
                for (const auto& child : node->children)
                    if (child)
                        query(child.get());
            }
        };
        query(root.get());
    }

private:
    std::unique_ptr<QuadNode> root;
    Bounds _bounds;
    int _maxObjects;
    int _maxDepth;
};

#endif // TRINITY_QUADTREE_H