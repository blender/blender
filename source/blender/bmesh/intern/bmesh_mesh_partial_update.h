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
 */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "BLI_compiler_attrs.h"

/**
 * Parameters used to determine which kinds of data needs to be generated.
 */
typedef struct BMPartialUpdate_Params {
  bool do_normals;
  bool do_tessellate;
} BMPartialUpdate_Params;

/**
 * Cached data to speed up partial updates.
 *
 * Hints:
 *
 * - Avoid creating this data for single updates,
 *   it should be created and reused across multiple updates to gain a significant benefit
 *   (while transforming geometry for example).
 *
 * - Partial normal updates use face & loop indices,
 *   setting them to dirty values between updates will slow down normal recalculation.
 */
typedef struct BMPartialUpdate {
  BMVert **verts;
  BMFace **faces;
  int verts_len, verts_len_alloc;
  int faces_len, faces_len_alloc;

  /** Store the parameters used in creation so invalid use can be asserted. */
  BMPartialUpdate_Params params;
} BMPartialUpdate;

BMPartialUpdate *BM_mesh_partial_create_from_verts(BMesh *bm,
                                                   const BMPartialUpdate_Params *params,
                                                   const int verts_len,
                                                   bool (*filter_fn)(BMVert *, void *user_data),
                                                   void *user_data)
    ATTR_NONNULL(1, 2, 4) ATTR_WARN_UNUSED_RESULT;

void BM_mesh_partial_destroy(BMPartialUpdate *bmpinfo) ATTR_NONNULL(1);
