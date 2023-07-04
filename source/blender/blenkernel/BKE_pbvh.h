/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief External data structures for PBVH. Does not
 *        include data structures internal to the draw code.
 */

#include "BLI_compiler_compat.h"
#include "BLI_utildefines.h"

struct PBVHNode;
struct PBVHBatches;
struct BMesh;

typedef enum {
  PBVH_FACES,
  PBVH_GRIDS,
  PBVH_BMESH,
} PBVHType;

/* PBVHNodeFlags is needed by DRW_render.h and draw_cache.c. */
typedef enum PBVHNodeFlags {
  PBVH_Leaf = 1 << 0,

  PBVH_UpdateNormals = 1 << 1,
  PBVH_UpdateBB = 1 << 2,
  PBVH_UpdateOriginalBB = 1 << 3,
  PBVH_UpdateDrawBuffers = 1 << 4,
  PBVH_UpdateRedraw = 1 << 5,
  PBVH_UpdateMask = 1 << 6,
  PBVH_UpdateVisibility = 1 << 8,

  PBVH_RebuildDrawBuffers = 1 << 9,
  PBVH_FullyHidden = 1 << 10,
  PBVH_FullyMasked = 1 << 11,
  PBVH_FullyUnmasked = 1 << 12,

  PBVH_UpdateTopology = 1 << 13,
  PBVH_UpdateColor = 1 << 14,

  PBVH_RebuildPixels = 1 << 15,
  PBVH_Delete = 1 << 16,
  PBVH_UpdateCurvatureDir = 1 << 17,
  PBVH_UpdateTris = 1 << 18,
  PBVH_RebuildNodeVerts = 1 << 19,

  /* Tri areas are not guaranteed to be up to date, tools should
   *  update all nodes on first step of brush.
   */
  PBVH_UpdateTriAreas = 1 << 20,
  PBVH_UpdateOtherVerts = 1 << 21,
  PBVH_TexLeaf = 1 << 22,
  PBVH_TopologyUpdated = 1 << 23, /* Used internally by dyntopo.c. */
} PBVHNodeFlags;
ENUM_OPERATORS(PBVHNodeFlags, PBVH_TopologyUpdated);

/* A few C++ methods for PBVHxxxRef structs to play nice with sets and maps. */
#ifdef __cplusplus
#  define PBVH_REF_CXX_METHODS(Class) \
    bool operator==(const Class b) const \
    { \
      return i == b.i; \
    } \
    uint64_t hash() const \
    { \
      return i; \
    }
#else
#  define PBVH_REF_CXX_METHODS(Class)
#endif

typedef struct PBVHVertRef {
  intptr_t i;

  PBVH_REF_CXX_METHODS(PBVHVertRef)
} PBVHVertRef;

/* NOTE: edges in PBVH_GRIDS are always pulled from the base mesh. */
typedef struct PBVHEdgeRef {
  intptr_t i;

  PBVH_REF_CXX_METHODS(PBVHVertRef)
} PBVHEdgeRef;

/* NOTE: faces in PBVH_GRIDS are always puled from the base mesh. */
typedef struct PBVHFaceRef {
  intptr_t i;

  PBVH_REF_CXX_METHODS(PBVHVertRef)
} PBVHFaceRef;

#define PBVH_REF_NONE -1LL

/* Public members of PBVH, used for inlined functions. */
struct PBVHPublic {
  PBVHType type;
  struct BMesh *bm;
};

typedef struct PBVH PBVH;
typedef struct PBVHNode PBVHNode;

BLI_INLINE PBVHType BKE_pbvh_type(const PBVH *pbvh)
{
  return ((const struct PBVHPublic *)pbvh)->type;
}

#ifdef __cplusplus
extern "C" {
#endif

/* Needed by eevee_materias.c. */
void BKE_pbvh_is_drawing_set(PBVH *pbvh, bool val);

/* Needed by basic_engine.c. */
void BKE_pbvh_draw_debug_cb(PBVH *pbvh,
                            void (*draw_fn)(PBVHNode *node,
                                            void *user_data,
                                            const float bmin[3],
                                            const float bmax[3],
                                            PBVHNodeFlags flag),
                            void *user_data);

#ifdef __cplusplus
}
#endif /* extern "C" */
