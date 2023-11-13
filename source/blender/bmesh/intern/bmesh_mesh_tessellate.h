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

struct BMPartialUpdate;

struct BMeshCalcTessellation_Params {
  /**
   * When calculating normals as well as tessellation, calculate normals after tessellation
   * for improved performance. See #BMeshCalcTessellation_Params
   */
  bool face_normals;
};

void BM_mesh_calc_tessellation_ex(BMesh *bm,
                                  BMLoop *(*looptris)[3],
                                  const struct BMeshCalcTessellation_Params *params);
void BM_mesh_calc_tessellation(BMesh *bm, BMLoop *(*looptris)[3]);

/**
 * A version of #BM_mesh_calc_tessellation that avoids degenerate triangles.
 */
void BM_mesh_calc_tessellation_beauty(BMesh *bm, BMLoop *(*looptris)[3]);

void BM_mesh_calc_tessellation_with_partial_ex(BMesh *bm,
                                               BMLoop *(*looptris)[3],
                                               const struct BMPartialUpdate *bmpinfo,
                                               const struct BMeshCalcTessellation_Params *params);
void BM_mesh_calc_tessellation_with_partial(BMesh *bm,
                                            BMLoop *(*looptris)[3],
                                            const struct BMPartialUpdate *bmpinfo);

#ifdef __cplusplus
}
#endif
