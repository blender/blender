/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

struct BMCalcPathParams {
  uint use_topology_distance : 1;
  uint use_step_face : 1;
};

LinkNode *BM_mesh_calc_path_vert(BMesh *bm,
                                 BMVert *v_src,
                                 BMVert *v_dst,
                                 const BMCalcPathParams *params,
                                 bool (*filter_fn)(BMVert *, void *),
                                 void *user_data) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3, 5);

LinkNode *BM_mesh_calc_path_edge(BMesh *bm,
                                 BMEdge *e_src,
                                 BMEdge *e_dst,
                                 const BMCalcPathParams *params,
                                 bool (*filter_fn)(BMEdge *, void *),
                                 void *user_data) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3, 5);

LinkNode *BM_mesh_calc_path_face(BMesh *bm,
                                 BMFace *f_src,
                                 BMFace *f_dst,
                                 const BMCalcPathParams *params,
                                 bool (*filter_fn)(BMFace *, void *),
                                 void *user_data) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3, 5);
