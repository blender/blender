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

/**
 * Splits a face into many smaller faces defined by an edge-net.
 * handle customdata and degenerate cases.
 *
 * - Isolated holes or unsupported face configurations, will be ignored.
 * - Customdata calculations aren't efficient
 *   (need to calculate weights for each vert).
 */
bool BM_face_split_edgenet(BMesh *bm,
                           BMFace *f,
                           BMEdge **edge_net,
                           int edge_net_len,
                           BMFace ***r_face_arr,
                           int *r_face_arr_len);

/**
 * For when the edge-net has holes in it-this connects them.
 *
 * \param use_partial_connect: Support for handling islands connected by only a single edge,
 * \note that this is quite slow so avoid using where possible.
 * \param mem_arena: Avoids many small allocs & should be cleared after each use.
 * take care since \a edge_net_new is stored in \a r_edge_net_new.
 */
bool BM_face_split_edgenet_connect_islands(BMesh *bm,
                                           BMFace *f,
                                           BMEdge **edge_net_init,
                                           uint edge_net_init_len,
                                           bool use_partial_connect,
                                           struct MemArena *mem_arena,
                                           BMEdge ***r_edge_net_new,
                                           uint *r_edge_net_new_len) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3, 6, 7, 8);
