/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_sys_types.h"

struct BMesh;
struct Mesh;
struct MultiresModifierData;

typedef struct MultiresUnsubdivideGrid {
  /* For sanity checks. */
  int grid_index;
  int grid_size;

  /** Grid coordinates in object space. */
  float (*grid_co)[3];

} MultiresUnsubdivideGrid;

typedef struct MultiresUnsubdivideContext {
  /* Input Mesh to un-subdivide. */
  struct Mesh *original_mesh;
  struct MDisps *original_mdisp;

  /** Number of subdivision in the grids of the input mesh. */
  int num_original_levels;

  /** Level 0 base mesh after applying the maximum amount of unsubdivisions. */
  struct Mesh *base_mesh;

  /** Limit on how many levels down the unsubdivide operation should create, if possible. */
  int max_new_levels;

  /** New levels that were created after unsubdividing. */
  int num_new_levels;

  /**
   * Number of subdivisions that should be applied to the base mesh.
   * (num_new_levels + num_original_levels).
   */
  int num_total_levels;

  /** Data for the new grids, indexed by base mesh loop index. */
  int num_grids;
  struct MultiresUnsubdivideGrid *base_mesh_grids;

  /* Private data. */
  struct BMesh *bm_original_mesh;
  blender::Array<int> loop_to_face_map;
  const int *base_to_orig_vmap;
} MultiresUnsubdivideContext;

/* --------------------------------------------------------------------
 * Construct/destruct reshape context.
 */

void multires_unsubdivide_context_init(MultiresUnsubdivideContext *context,
                                       struct Mesh *original_mesh,
                                       struct MultiresModifierData *mmd);
void multires_unsubdivide_context_free(MultiresUnsubdivideContext *context);

/* --------------------------------------------------------------------
 * Rebuild Lower Subdivisions.
 */

/* Rebuilds all subdivision to the level 0 base mesh. */
bool multires_unsubdivide_to_basemesh(MultiresUnsubdivideContext *context);
