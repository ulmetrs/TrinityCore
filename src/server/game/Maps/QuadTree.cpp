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

#include "QuadTree.h"

template <typename T>
struct QuadTree<T>::QuadNode
{
    float minX, minY, maxX, maxY;
    std::vector<Node> objects;
    std::unique_ptr<QuadNode> children[4];
    int depth;
    int maxObjects, maxDepth;

    QuadNode(float minX_, float minY_, float maxX_, float maxY_, int depth_, int maxObjects_, int maxDepth_)
        : minX(minX_), minY(minY_), maxX(maxX_), maxY(maxY_), depth(depth_), maxObjects(maxObjects_), maxDepth(maxDepth_)
    {}

    bool IsLeaf() const { return children[0] == nullptr; }

    void Subdivide()
    {
        float midX = (minX + maxX) * 0.5f;
        float midY = (minY + maxY) * 0.5f;
        children[0] = std::make_unique<QuadNode>(minX, minY, midX, midY, depth+1, maxObjects, maxDepth); // SW
        children[1] = std::make_unique<QuadNode>(midX, minY, maxX, midY, depth+1, maxObjects, maxDepth); // SE
        children[2] = std::make_unique<QuadNode>(minX, midY, midX, maxY, depth+1, maxObjects, maxDepth); // NW
        children[3] = std::make_unique<QuadNode>(midX, midY, maxX, maxY, depth+1, maxObjects, maxDepth); // NE
    }

    int GetChildIndex(float x, float y) const
    {
        float midX = (minX + maxX) * 0.5f;
        float midY = (minY + maxY) * 0.5f;
        if (x < midX)
            return (y < midY) ? 0 : 2;
        else
            return (y < midY) ? 1 : 3;
    }
};

template <typename T>
QuadTree<T>::QuadTree(float minX_, float minY_, float maxX_, float maxY_, int maxDepth_, int maxObjects_)
    : minX(minX_), minY(minY_), maxX(maxX_), maxY(maxY_), maxDepth(maxDepth_), maxObjects(maxObjects_)
{
    root = std::make_unique<QuadNode>(minX, minY, maxX, maxY, 0, maxObjects, maxDepth);
}

template <typename T>
QuadTree<T>::~QuadTree() = default;

template <typename T>
void QuadTree<T>::Insert(float x, float y, T* object)
{
    QuadNode* node = root.get();
    while (!node->IsLeaf())
        node = node->children[node->GetChildIndex(x, y)].get();

    node->objects.emplace_back(x, y, object);

    if (node->objects.size() > node->maxObjects && node->depth < node->maxDepth)
    {
        node->Subdivide();
        for (const Node& n : node->objects)
        {
            int idx = node->GetChildIndex(n.x, n.y);
            node->children[idx]->objects.emplace_back(n.x, n.y, n.object);
        }
        node->objects.clear();
    }
}

template <typename T>
void QuadTree<T>::Remove(float x, float y, T* object)
{
    QuadNode* node = root.get();
    while (!node->IsLeaf())
        node = node->children[node->GetChildIndex(x, y)].get();

    auto it = std::remove_if(node->objects.begin(), node->objects.end(),
        [object](const Node& n) { return n.object == object; });
    node->objects.erase(it, node->objects.end());
}

template <typename T>
void QuadTree<T>::Clear()
{
    root = std::make_unique<QuadNode>(minX, minY, maxX, maxY, 0, maxObjects, maxDepth);
}

template <typename T>
template <typename Func>
void QuadTree<T>::QueryRange(float qMinX, float qMinY, float qMaxX, float qMaxY, Func&& visitor) const
{
    std::function<void(const QuadNode*)> query = [&](const QuadNode* node)
    {
        if (node->maxX < qMinX || node->minX > qMaxX || node->maxY < qMinY || node->minY > qMaxY)
            return;
        for (const Node& n : node->objects)
        {
            if (n.x >= qMinX && n.x <= qMaxX && n.y >= qMinY && n.y <= qMaxY)
                visitor(n.object);
        }
        if (!node->IsLeaf())
            for (const auto& child : node->children)
                query(child.get());
    };
    query(root.get());
}

template <typename T>
template <typename Func>
void QuadTree<T>::QueryCircle(float centerX, float centerY, float radius, Func&& visitor) const
{
    float radiusSq = radius * radius;
    // Use a bounding box to prune branches quickly
    float minX = centerX - radius;
    float maxX = centerX + radius;
    float minY = centerY - radius;
    float maxY = centerY + radius;

    std::function<void(const QuadNode*)> query = [&](const QuadNode* node)
    {
        // Prune if node is completely outside the bounding box
        if (node->maxX < minX || node->minX > maxX || node->maxY < minY || node->minY > maxY)
            return;
        for (const Node& n : node->objects)
        {
            float dx = n.x - centerX;
            float dy = n.y - centerY;
            if (dx * dx + dy * dy <= radiusSq)
                visitor(n.object);
        }
        if (!node->IsLeaf())
            for (const auto& child : node->children)
                query(child.get());
    };
    query(root.get());
}