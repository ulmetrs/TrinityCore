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

#include "Player.h"
#include "GameObject.h"
#include "Creature.h"
#include "DynamicObject.h"
#include "Corpse.h"
#include <cassert>

template<typename T>
void QuadNode<T>::Remove(T* obj)
{
    auto it = std::find(objects.begin(), objects.end(), obj);
    if (it != objects.end())
    {
        obj->SetQuadNode(nullptr);
        objects.erase(it);
    }
}

template<typename T>
void QuadNode<T>::Subdivide()
{
    float midX = (_bounds.minX + _bounds.maxX) * 0.5f;
    float midY = (_bounds.minY + _bounds.maxY) * 0.5f;

    // Create the four child nodes
    // NW
    children[0] = std::make_unique<QuadNode<T>>(Bounds{_bounds.minX, midY, midX, _bounds.maxY}, depth + 1, maxObjects, maxDepth);
    // NE
    children[1] = std::make_unique<QuadNode<T>>(Bounds{midX, midY, _bounds.maxX, _bounds.maxY}, depth + 1, maxObjects, maxDepth);
    // SW
    children[2] = std::make_unique<QuadNode<T>>(Bounds{_bounds.minX, _bounds.minY, midX, midY}, depth + 1, maxObjects, maxDepth);
    // SE
    children[3] = std::make_unique<QuadNode<T>>(Bounds{midX, _bounds.minY, _bounds.maxX, midY}, depth + 1, maxObjects, maxDepth);
}

template<typename T>
int QuadNode<T>::GetChildIndex(float x, float y) const
{
    float midX = (_bounds.minX + _bounds.maxX) * 0.5f;
    float midY = (_bounds.minY + _bounds.maxY) * 0.5f;
    if (x < midX)
        return (y < midY) ? 2 : 0; // SW : NW
    else
        return (y < midY) ? 3 : 1; // SE : NE
}

template<typename T>
QuadTree<T>::QuadTree(Bounds bounds, size_t maxObjects, float cellSize)
    : _bounds(bounds), _maxObjects(maxObjects)
{
    ASSERT(bounds.minX >= -MAP_HALFSIZE && bounds.minY >= -MAP_HALFSIZE);
    ASSERT(bounds.maxX <= MAP_HALFSIZE && bounds.maxY <= MAP_HALFSIZE);
    ASSERT(bounds.minX < bounds.maxX && bounds.minY < bounds.maxY);
    ASSERT(maxObjects > 0);
    ASSERT(cellSize > 0);

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
    _maxDepth = side <= 0 ? 0 : static_cast<int>(std::ceil(std::log2(side / cellSize)));

    // 5. Create the root node using the new square bounds and computed depth
    root = std::make_unique<QuadNode<T>>(_bounds, 0, _maxObjects, _maxDepth);
}

template<typename T>
void QuadTree<T>::Clear()
{
    std::function<void(QuadNode<T>*)> clearNode = [&](QuadNode<T>* node)
    {
        for (T* obj : node->objects)
        {
            obj->SetQuadNode(nullptr);
            obj->CleanupsBeforeDelete();
            delete obj;
        }
        node->objects.clear();
        if (!node->IsLeaf())
        {
            for (auto& child : node->children)
                if (child)
                    clearNode(child.get());
        }
    };
    clearNode(root.get());
    root = std::make_unique<QuadNode<T>>(_bounds, 0, _maxObjects, _maxDepth);
}

template<typename T>
bool QuadTree<T>::Insert(T* obj)
{
    QuadNode<T>* currentNode = static_cast<QuadNode<T>*>(obj->GetQuadNode());

    float x = obj->GetPositionX();
    float y = obj->GetPositionY();
    if (x < _bounds.minX || x > _bounds.maxX || y < _bounds.minY || y > _bounds.maxY)
    {
        if (currentNode)
            currentNode->Remove(obj);

        return false;
    }
    
    QuadNode<T>* node = root.get();

    // Traverse to the correct leaf node
    while (true)
    {
        if (node->IsLeaf())
        {
            // If already in this node, do nothing
            if (currentNode == node)
                return true;

            // Remove from previous node if needed
            if (currentNode)
                currentNode->Remove(obj);

            obj->SetQuadNode(node);
            node->objects.push_back(obj);

            if (node->objects.size() > node->maxObjects && node->depth < node->maxDepth)
            {
                node->Subdivide();

                // Re-insert objects into children
                auto objs = std::move(node->objects);
                node->objects.clear();
                for (T* o : objs)
                {
                    int idx = node->GetChildIndex(o->GetPositionX(), o->GetPositionY());
                    o->SetQuadNode(node->children[idx].get());
                    node->children[idx]->objects.push_back(o);
                }
            }

            return true;
        }
        else
        {
            int idx = node->GetChildIndex(obj->GetPositionX(), obj->GetPositionY());
            node = node->children[idx].get();
        }
    }
}

template<typename T>
template<typename Func>
void QuadTree<T>::QueryCircle(float centerX, float centerY, float radius, Func&& visitor) const
{
    float radiusSq = radius * radius;
    float minX = centerX - radius;
    float maxX = centerX + radius;
    float minY = centerY - radius;
    float maxY = centerY + radius;

    auto intersectsCircle = [&](const Bounds& b) -> bool {
        // Coarse AABB check first (cheap)
        if (b.maxX < minX || b.minX > maxX || b.maxY < minY || b.minY > maxY) return false;

        // Exact circle-rect: Clamp center to rect, check distSq <= radiusSq
        float closestX = std::max(b.minX, std::min(centerX, b.maxX));
        float closestY = std::max(b.minY, std::min(centerY, b.maxY));
        float dx = centerX - closestX;
        float dy = centerY - closestY;
        return (dx * dx + dy * dy) <= radiusSq;
    };

    std::stack<const QuadNode<T>*> nodeStack;
    nodeStack.push(root.get());

    while (!nodeStack.empty())
    {
        const QuadNode<T>* node = nodeStack.top();
        nodeStack.pop();

        if (!intersectsCircle(node->_bounds)) continue;

        if (node->IsLeaf())
        {
            for (T* obj : node->objects)
            {
                float x = obj->GetPositionX();
                float y = obj->GetPositionY();
                float dx = x - centerX;
                float dy = y - centerY;
                if (dx * dx + dy * dy <= radiusSq)
                    std::forward<Func>(visitor)(obj);
            }
        }

        if (!node->IsLeaf())
        {
            for (const auto& child : node->children)
            {
                if (child && intersectsCircle(child->_bounds))
                    nodeStack.push(child.get());
            }
        }
    }
}

template<typename T>
template<typename Func>
void QuadTree<T>::QueryRange(float minX, float minY, float maxX, float maxY, Func&& visitor) const
{
    std::stack<const QuadNode<T>*> nodeStack;
    nodeStack.push(root.get());

    while (!nodeStack.empty())
    {
        const QuadNode<T>* node = nodeStack.top();
        nodeStack.pop();

        if (node->_bounds.maxX < minX || node->_bounds.minX > maxX ||
            node->_bounds.maxY < minY || node->_bounds.minY > maxY)
            continue;

        if (node->IsLeaf())
        {
            for (T* obj : node->objects)
            {
                float x = obj->GetPositionX();
                float y = obj->GetPositionY();
                if (x >= minX && x <= maxX && y >= minY && y <= maxY)
                    std::forward<Func>(visitor)(obj);
            }
        }

        if (!node->IsLeaf())
        {
            for (const auto& child : node->children)
            {
                if (child &&
                    !(child->_bounds.maxX < minX || child->_bounds.minX > maxX ||
                      child->_bounds.maxY < minY || child->_bounds.minY > maxY))
                {
                    nodeStack.push(child.get());
                }
            }
        }
    }
}

template<typename T>
template<typename Func>
void QuadTree<T>::QueryAll(Func&& visitor) const
{
    std::stack<const QuadNode<T>*> nodeStack;
    nodeStack.push(root.get());
    while (!nodeStack.empty())
    {
        const QuadNode<T>* node = nodeStack.top();
        nodeStack.pop();

        for (T* obj : node->objects)
        {
            visitor(obj);
        }

        if (!node->IsLeaf())
        {
            for (const auto& child : node->children)
            {
                if (child)
                    nodeStack.push(child.get());
            }
        }
    }
}