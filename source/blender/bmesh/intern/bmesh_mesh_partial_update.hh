/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "BLI_bit_span.hh"
#include "BLI_compiler_attrs.h"
#include "BLI_vector.hh"

#include "bmesh_class.hh"

namespace blender {

/**
 * Parameters used to determine which kinds of data needs to be generated.
 */
struct BMPartialUpdate_Params {
  bool do_normals;
  bool do_tessellate;
};

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
struct BMPartialUpdate {
  Vector<BMVert *> verts;
  Vector<BMFace *> faces;

  /** Store the parameters used in creation so invalid use can be asserted. */
  BMPartialUpdate_Params params = {};
};

/**
 * All Tagged & Connected, see: #BM_mesh_partial_create_from_verts
 * Operate on everything that's tagged as well as connected geometry.
 */
[[nodiscard]] BMPartialUpdate *BM_mesh_partial_create_from_verts(
    BMesh &bm, const BMPartialUpdate_Params &params, BitSpan verts_mask, int verts_mask_count);

/**
 * All Connected, operate on all faces that have both tagged and un-tagged vertices.
 *
 * Reduces computations when transforming isolated regions.
 */
[[nodiscard]] BMPartialUpdate *BM_mesh_partial_create_from_verts_group_single(
    BMesh &bm, const BMPartialUpdate_Params &params, BitSpan verts_mask, int verts_mask_count);

/**
 * All Connected, operate on all faces that have vertices in the same group.
 *
 * Reduces computations when transforming isolated regions.
 *
 * This is a version of #BM_mesh_partial_create_from_verts_group_single
 * that handles multiple groups instead of a bitmap mask.
 *
 * This is needed for example when transform has mirror enabled,
 * since one side needs to have a different group to the other since a face that has vertices
 * attached to both won't have an affine transformation.
 *
 * \param verts_group: Vertex aligned array of groups.
 * Values are used as follows:
 * - >0: Each face is grouped with other faces of the same group.
 * -  0: Not in a group (don't handle these).
 * - -1: Don't use grouping logic (include any face that contains a vertex with this group).
 * \param verts_group_count: The number of non-zero values in `verts_groups`.
 */
[[nodiscard]] BMPartialUpdate *BM_mesh_partial_create_from_verts_group_multi(
    BMesh &bm, const BMPartialUpdate_Params &params, Span<int> verts_group, int verts_group_count);

void BM_mesh_partial_destroy(BMPartialUpdate *bmpinfo) ATTR_NONNULL(1);

}  // namespace blender
