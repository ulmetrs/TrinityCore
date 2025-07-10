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

class WorldObject;

class QuadNode
{
    friend class QuadTree;
public:
    QuadNode(float minX, float minY, float maxX, float maxY, int depth, int maxObjects, int maxDepth);

    bool IsLeaf() const;
    void Remove(WorldObject* obj);
    void Subdivide();
    int GetChildIndex(float x, float y) const;

private:
    float minX, minY, maxX, maxY;
    std::vector<WorldObject*> objects;
    std::unique_ptr<QuadNode> children[4];
    int depth;
    int maxObjects, maxDepth;
};

class QuadTree
{
public:
    QuadTree(float minX, float minY, float maxX, float maxY, int maxObjects = 8, float cellSize = SIZE_OF_GRID_CELL);
    ~QuadTree();

    void Insert(WorldObject* object);
    void Clear();

    template <typename Func>
    void QueryRange(float qMinX, float qMinY, float qMaxX, float qMaxY, Func&& visitor) const
    {
        std::function<void(const QuadNode*)> query = [&](const QuadNode* node)
        {
            if (node->maxX < qMinX || node->minX > qMaxX || node->maxY < qMinY || node->minY > qMaxY)
                return;
            for (WorldObject* obj : node->objects)
            {
                if (obj->GetPositionX() >= qMinX && obj->GetPositionX() <= qMaxX &&
                    obj->GetPositionY() >= qMinY && obj->GetPositionY() <= qMaxY)
                    visitor(obj);
            }
            if (!node->IsLeaf())
                for (const auto& child : node->children)
                    query(child.get());
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
            if (node->maxX < minX || node->minX > maxX || node->maxY < minY || node->minY > maxY)
                return;
            for (WorldObject* obj : node->objects)
            {
                float dx = obj->GetPositionX() - centerX;
                float dy = obj->GetPositionY() - centerY;
                if (dx * dx + dy * dy <= radiusSq)
                    visitor(obj);
            }
            if (!node->IsLeaf())
                for (const auto& child : node->children)
                    query(child.get());
        };
        query(root.get());
    }

private:
    std::unique_ptr<QuadNode> root;
    float _minX, _minY, _maxX, _maxY;
    int _maxDepth;
    int _maxObjects;
};

#endif // TRINITY_QUADTREE_H