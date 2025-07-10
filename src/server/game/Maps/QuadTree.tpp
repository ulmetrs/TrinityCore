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

#include <cassert>

template<typename T>
QuadTree<T>::QuadTree(Bounds bounds, int maxObjects, float cellSize)
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
    root = std::make_unique<QuadNode<T>>(_bounds, 0, _maxObjects, _maxDepth);
}

template<typename T>
void QuadTree<T>::Clear()
{
    std::function<void(QuadNode<T>*)> clearNode = [&](QuadNode<T>* node)
    {
        for (T* obj : node->objects)
            obj->SetQuadNode(nullptr);
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
void QuadTree<T>::Insert(T* obj)
{
    QuadNode<T>* currentNode = static_cast<QuadNode<T>*>(obj->GetQuadNode());
    QuadNode<T>* node = root.get();

    // Traverse to the correct leaf node
    while (true)
    {
        if (node->IsLeaf())
        {
            // If already in this node, do nothing
            if (currentNode == node)
                return;

            // Remove from previous node if needed
            if (currentNode)
                currentNode->Remove(obj);

            node->objects.push_back(obj);
            obj->SetQuadNode(node);

            if (node->objects.size() > static_cast<size_t>(node->maxObjects) && node->depth < node->maxDepth)
            {
                node->Subdivide();
                // Re-insert objects into children
                auto objs = std::move(node->objects);
                node->objects.clear();
                for (T* o : objs)
                {
                    int idx = node->GetChildIndex(o->GetPositionX(), o->GetPositionY());
                    node->children[idx]->objects.push_back(o);
                    o->SetQuadNode(node->children[idx].get());
                }
            }
            break;
        }
        else
        {
            int idx = node->GetChildIndex(obj->GetPositionX(), obj->GetPositionY());
            node = node->children[idx].get();
        }
    }
}

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
template<typename Func>
void QuadTree<T>::QueryRange(float minX, float minY, float maxX, float maxY, Func&& visitor) const
{
    std::function<void(const QuadNode<T>*)> query = [&](const QuadNode<T>* node)
    {
        if (node->_bounds.maxX < minX || node->_bounds.minX > maxX ||
            node->_bounds.maxY < minY || node->_bounds.minY > maxY)
            return;
        for (T* obj : node->objects)
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

template<typename T>
template<typename Func>
void QuadTree<T>::QueryCircle(float centerX, float centerY, float radius, Func&& visitor) const
{
    float radiusSq = radius * radius;
    float minX = centerX - radius;
    float maxX = centerX + radius;
    float minY = centerY - radius;
    float maxY = centerY + radius;

    std::function<void(const QuadNode<T>*)> query = [&](const QuadNode<T>* node)
    {
        if (node->_bounds.maxX < minX || node->_bounds.minX > maxX ||
            node->_bounds.maxY < minY || node->_bounds.minY > maxY)
            return;
        for (T* obj : node->objects)
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