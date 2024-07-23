/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_utildefines.h"

struct BMesh;
struct BMVert;
struct BMFace;
namespace blender::draw::pbvh {
struct PBVHBatches;
}

namespace blender::bke::pbvh {

class Node;
class Tree;

enum class Type {
  Mesh,
  Grids,
  BMesh,
};

}  // namespace blender::bke::pbvh

/* #PBVHNodeFlags is needed by `DRW_render.hh` and `draw_cache.cc`. */
enum PBVHNodeFlags {
  PBVH_Leaf = 1 << 0,

  PBVH_UpdateNormals = 1 << 1,
  PBVH_UpdateBB = 1 << 2,
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
  PBVH_TexLeaf = 1 << 16,
  /** Used internally by `pbvh_bmesh.cc`. */
  PBVH_TopologyUpdated = 1 << 17,
};
ENUM_OPERATORS(PBVHNodeFlags, PBVH_TopologyUpdated);

/* A few C++ methods to play nice with sets and maps. */
#define PBVH_REF_CXX_METHODS(Class) \
  bool operator==(const Class b) const \
  { \
    return i == b.i; \
  } \
  uint64_t hash() const \
  { \
    return i; \
  }

struct PBVHVertRef {
  intptr_t i;

  PBVH_REF_CXX_METHODS(PBVHVertRef)
};

#define PBVH_REF_NONE -1LL

void BKE_pbvh_draw_debug_cb(blender::bke::pbvh::Tree &pbvh,
                            void (*draw_fn)(blender::bke::pbvh::Node *node,
                                            void *user_data,
                                            const float bmin[3],
                                            const float bmax[3],
                                            PBVHNodeFlags flag),
                            void *user_data);
