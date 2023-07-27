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

struct LinkNode *BM_mesh_calc_path_region_vert(BMesh *bm,
                                               BMElem *ele_src,
                                               BMElem *ele_dst,
                                               bool (*filter_fn)(BMVert *, void *user_data),
                                               void *user_data) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3);

struct LinkNode *BM_mesh_calc_path_region_edge(BMesh *bm,
                                               BMElem *ele_src,
                                               BMElem *ele_dst,
                                               bool (*filter_fn)(BMEdge *, void *user_data),
                                               void *user_data) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3);

struct LinkNode *BM_mesh_calc_path_region_face(BMesh *bm,
                                               BMElem *ele_src,
                                               BMElem *ele_dst,
                                               bool (*filter_fn)(BMFace *, void *user_data),
                                               void *user_data) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3);

#ifdef __cplusplus
}
#endif
