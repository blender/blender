//
// Copyright (c) 2009 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#ifndef DETOURDEBUGDRAW_H
#define DETOURDEBUGDRAW_H

#include "DetourStatNavMesh.h"
#include "DetourTileNavMesh.h"

void dtDebugDrawStatNavMeshPoly(const dtStatNavMesh* mesh, dtStatPolyRef ref, const float* col);
void dtDebugDrawStatNavMeshBVTree(const dtStatNavMesh* mesh);
void dtDebugDrawStatNavMesh(const dtStatNavMesh* mesh, bool drawClosedList = false);

void dtDebugDrawTiledNavMesh(const dtTiledNavMesh* mesh);
void dtDebugDrawTiledNavMeshPoly(const dtTiledNavMesh* mesh, dtTilePolyRef ref, const float* col);

#endif // DETOURDEBUGDRAW_H