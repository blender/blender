/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#ifdef __cplusplus
extern "C" {
#endif

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
 * \param mem_arena: Avoids many small allocations & should be cleared after each use.
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

#ifdef __cplusplus
}
#endif
