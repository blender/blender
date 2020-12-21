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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

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
  int *loop_to_face_map;
  int *base_to_orig_vmap;
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
