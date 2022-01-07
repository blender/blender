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
