/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

struct BMCalcPathUVParams {
  uint use_topology_distance : 1;
  uint use_step_face : 1;
  int cd_loop_uv_offset;
  float aspect_y;
};

LinkNode *BM_mesh_calc_path_uv_vert(BMesh *bm,
                                    BMLoop *l_src,
                                    BMLoop *l_dst,
                                    const BMCalcPathUVParams *params,
                                    bool (*filter_fn)(BMLoop *, void *),
                                    void *user_data) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3, 5);

LinkNode *BM_mesh_calc_path_uv_edge(BMesh *bm,
                                    BMLoop *l_src,
                                    BMLoop *l_dst,
                                    const BMCalcPathUVParams *params,
                                    bool (*filter_fn)(BMLoop *, void *),
                                    void *user_data) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3, 5);

LinkNode *BM_mesh_calc_path_uv_face(BMesh *bm,
                                    BMFace *f_src,
                                    BMFace *f_dst,
                                    const BMCalcPathUVParams *params,
                                    bool (*filter_fn)(BMFace *, void *),
                                    void *user_data) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3, 5);
