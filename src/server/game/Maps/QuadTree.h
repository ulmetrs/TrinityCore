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

#include <vector>
#include <memory>
#include <algorithm>

template <typename T>
class QuadTree
{
public:
    struct Node
    {
        float x, y;
        T* object;
        Node(float x_, float y_, T* obj_) : x(x_), y(y_), object(obj_) {}
    };

    QuadTree(float minX, float minY, float maxX, float maxY, int maxDepth = 6, int maxObjects = 8);
    ~QuadTree();

    void Insert(float x, float y, T* object);
    void Remove(float x, float y, T* object);
    void Clear();
    template <typename Func>
    void QueryRange(float qMinX, float qMinY, float qMaxX, float qMaxY, Func&& visitor) const;
    template <typename Func>
    void QueryCircle(float centerX, float centerY, float radius, Func&& visitor) const;

private:
    struct QuadNode;
    std::unique_ptr<QuadNode> root;
    float minX, minY, maxX, maxY;
    int maxDepth, maxObjects;
};

#endif // TRINITY_QUADTREE_H