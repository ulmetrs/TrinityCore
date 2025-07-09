#ifndef QUAD_TREE_OBJECT_H
#define QUAD_TREE_OBJECT_H

#include "QuadTree.h"

template<class T>
class QuadTreeObject
{
    public:
        QuadTreeObject() : _tree(nullptr) { }
        virtual ~QuadTreeObject() { RemoveFromTree(); }

        bool IsInTree() const { return _tree != nullptr; }
        void AddToTree(QuadTree<T>* tree)
        {
            ASSERT(!IsInTree());
            _tree = tree;
            _tree->Insert(GetPositionX(), GetPositionY(), this);
        }
        void RemoveFromTree()
        {
            ASSERT(IsInTree());
            _tree->Remove(GetPositionX(), GetPositionY(), this);
            _tree = nullptr;
        }
    private:
        QuadTree<T>* _tree;
};

#endif
