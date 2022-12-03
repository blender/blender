/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_bvhutils.h"
#include "BLI_bitmap.h"
#include "BLI_math_matrix.h"

struct Scene;
struct Mesh;
struct BassReliefModifierData;
struct Object;
struct MDeformVert;
struct ModifierEvalContext;
struct MPropCol;

#define MAX_BASSRELIEF_DEBUG_COLORS 7

#ifdef __cplusplus
extern "C" {
#endif

/* Information about a mesh and BVH tree. */
typedef struct BassReliefTreeData {
  Mesh *mesh;
  const MPoly *mpoly;

  BVHTree *bvh;
  BVHTreeFromMesh treeData;
  struct SpaceTransform transform;
  float keepDist;

  const float (*vert_normals)[3];
  float (*pnors)[3];
  float (*clnors)[3];
} BassReliefTreeData;

void bassReliefModifier_deform(struct BassReliefModifierData *smd,
                               const struct ModifierEvalContext *ctx,
                               struct Scene *scene,
                               struct Object *ob,
                               struct Mesh *mesh,
                               const struct MDeformVert *dvert,
                               const int defgrp_index,
                               float (*vertexCos)[3],
                               int numVerts,
                               struct MPropCol *debugColors[MAX_BASSRELIEF_DEBUG_COLORS]);

/*
 * NULL initializes to local data
 */
#define NULL_BassReliefCalcData \
  { \
    NULL, \
  }
#define NULL_BVHTreeFromMesh \
  { \
    NULL, \
  }
#define NULL_BVHTreeRayHit \
  { \
    NULL, \
  }
#define NULL_BVHTreeNearest \
  { \
    0, \
  }

#ifdef __cplusplus
}
#endif
