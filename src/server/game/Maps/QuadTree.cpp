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
#include "Object.h"
#include <algorithm> // for std::max

// QuadNode Implementation

// QuadNode Implementation

QuadNode::QuadNode(Bounds bounds, int depth_, int maxObjects_, int maxDepth_)
    : _bounds(bounds), depth(depth_), maxObjects(maxObjects_), maxDepth(maxDepth_)
{}

bool QuadNode::IsLeaf() const
{
    return children[0] == nullptr;
}

void QuadNode::Remove(WorldObject* obj)
{
    auto it = std::remove(objects.begin(), objects.end(), obj);
    objects.erase(it, objects.end());
    obj->SetQuadNode(nullptr);
}

void QuadNode::Subdivide()
{
    float midX = (_bounds.minX + _bounds.maxX) * 0.5f;
    float midY = (_bounds.minY + _bounds.maxY) * 0.5f;
    children[0] = std::make_unique<QuadNode>(Bounds{ _bounds.minX, _bounds.minY, midX, midY }, depth + 1, maxObjects, maxDepth); // SW
    children[1] = std::make_unique<QuadNode>(Bounds{ midX, _bounds.minY, _bounds.maxX, midY }, depth + 1, maxObjects, maxDepth); // SE
    children[2] = std::make_unique<QuadNode>(Bounds{ _bounds.minX, midY, midX, _bounds.maxY }, depth + 1, maxObjects, maxDepth); // NW
    children[3] = std::make_unique<QuadNode>(Bounds{ midX, midY, _bounds.maxX, _bounds.maxY }, depth + 1, maxObjects, maxDepth); // NE
    for (WorldObject* obj : objects)
    {
        int idx = GetChildIndex(obj->GetPositionX(), obj->GetPositionY());
        children[idx]->objects.push_back(obj);
        obj->SetQuadNode(children[idx].get());
    }
    objects.clear();
}

int QuadNode::GetChildIndex(float x, float y) const
{
    float midX = (_bounds.minX + _bounds.maxX) * 0.5f;
    float midY = (_bounds.minY + _bounds.maxY) * 0.5f;
    if (x < midX)
        return (y < midY) ? 0 : 2;
    else
        return (y < midY) ? 1 : 3;
}

// QuadTree Implementation

QuadTree::QuadTree(Bounds bounds, int maxObjects, float cellSize)
    : _bounds(bounds), _maxObjects(maxObjects)
{
    float width = _bounds.maxX - _bounds.minX;
    float height = _bounds.maxY - _bounds.minY;

    // 1. Find center
    float centerX = (_bounds.minX + _bounds.maxX) * 0.5f;
    float centerY = (_bounds.minY + _bounds.maxY) * 0.5f;

    // 2. Find the maximum side length
    float side = std::max(width, height);

    // 3. Expand both min/max to make square
    float halfSide = side * 0.5f;
    _bounds.minX = centerX - halfSide;
    _bounds.maxX = centerX + halfSide;
    _bounds.minY = centerY - halfSide;
    _bounds.maxY = centerY + halfSide;

    // 4. Compute depth so that smallest quadrant is <= cellSize
    _maxDepth = static_cast<int>(std::ceil(std::log2(side / cellSize)));

    // 5. Create the root node using the new square bounds and computed depth
    root = std::make_unique<QuadNode>(_bounds, 0, _maxObjects, _maxDepth);
}

QuadTree::~QuadTree() = default;

// Smart insert, only if needed and remove from previous node
void QuadTree::Insert(WorldObject* obj)
{
    float x = obj->GetPositionX();
    float y = obj->GetPositionY();
    QuadNode* node = root.get();
    while (!node->IsLeaf())
        node = node->children[node->GetChildIndex(x, y)].get();

    if (obj->GetQuadNode() == node)
        return;

    if (obj->GetQuadNode())
        obj->GetQuadNode()->Remove(obj);

    node->objects.push_back(obj);
    obj->SetQuadNode(node);

    if (node->objects.size() > node->maxObjects && node->depth < node->maxDepth)
        node->Subdivide();
}

void QuadTree::Clear()
{
    // Traverse all nodes and for each WorldObject* set _quadNode = nullptr
    std::function<void(QuadNode*)> clearNode = [&](QuadNode* node)
    {
        for (WorldObject* obj : node->objects)
            obj->SetQuadNode(nullptr);
        if (!node->IsLeaf())
            for (auto& child : node->children)
                clearNode(child.get());
    };
    clearNode(root.get());
    root = std::make_unique<QuadNode>(_bounds, 0, _maxObjects, _maxDepth);
}