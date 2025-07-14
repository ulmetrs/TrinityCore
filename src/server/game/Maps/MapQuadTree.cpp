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

MapQuadTree::MapQuadTree(Bounds bounds)
    : playerTree(bounds),
    gameObjectTree(bounds),
    gridCreatureTree(bounds),
    worldCreatureTree(bounds),
    gridDynamicObjectTree(bounds),
    worldDynamicObjectTree(bounds),
    gridCorpseTree(bounds),
    worldCorpseTree(bounds)
{}

// Explicit template instantiation for common function pointer and lambda types
template void MapQuadTree::QueryCircle<float(*)(WorldObject*)>(uint32_t, float, float, float, float(*&&)(WorldObject*)) const;
template void MapQuadTree::QueryRange<float(*)(WorldObject*)>(uint32_t, float, float, float, float, float(*&&)(WorldObject*)) const;
template void MapQuadTree::QueryAll<float(*)(WorldObject*)>(uint32_t, float(*&&)(WorldObject*)) const;