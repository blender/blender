/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#ifdef __cplusplus
extern "C" {
#endif

enum {
  /** Vertices tags must match (special case). */
  VERT_RESTRICT_TAG = (1 << 0),
  /** Don't rotate out of degenerate state (needed for iterative rotation). */
  EDGE_RESTRICT_DEGENERATE = (1 << 1),
};

/**
 * \note This function sets the edge indices to invalid values.
 */
void BM_mesh_beautify_fill(BMesh *bm,
                           BMEdge **edge_array,
                           int edge_array_len,
                           short flag,
                           short method,
                           short oflag_edge,
                           short oflag_face);

/**
 * Assuming we have 2 triangles sharing an edge (2 - 4),
 * check if the edge running from (1 - 3) gives better results.
 *
 * \return (negative number means the edge can be rotated, lager == better).
 */
float BM_verts_calc_rotate_beauty(const BMVert *v1,
                                  const BMVert *v2,
                                  const BMVert *v3,
                                  const BMVert *v4,
                                  short flag,
                                  short method);

#ifdef __cplusplus
}
#endif
