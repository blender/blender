/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "bmesh_class.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BMPartialUpdate;

struct BMeshNormalsUpdate_Params {
  /**
   * When calculating tessellation as well as normals, tessellate & calculate face normals
   * for improved performance. See #BMeshCalcTessellation_Params
   */
  bool face_normals;
};

/**
 * \brief BMesh Compute Normals
 *
 * Updates the normals of a mesh.
 */
void BM_mesh_normals_update_ex(BMesh *bm, const struct BMeshNormalsUpdate_Params *param);
void BM_mesh_normals_update(BMesh *bm);
/**
 * A version of #BM_mesh_normals_update that updates a subset of geometry,
 * used to avoid the overhead of updating everything.
 */
void BM_mesh_normals_update_with_partial_ex(BMesh *bm,
                                            const struct BMPartialUpdate *bmpinfo,
                                            const struct BMeshNormalsUpdate_Params *param);
void BM_mesh_normals_update_with_partial(BMesh *bm, const struct BMPartialUpdate *bmpinfo);

/**
 * \brief BMesh Compute Normals from/to external data.
 *
 * Computes the vertex normals of a mesh into vnos,
 * using given vertex coordinates (vcos) and polygon normals (fnos).
 */
void BM_verts_calc_normal_vcos(BMesh *bm,
                               const float (*fnos)[3],
                               const float (*vcos)[3],
                               float (*vnos)[3]);
/**
 * \brief BMesh Compute Loop Normals from/to external data.
 *
 * Compute split normals, i.e. vertex normals associated with each poly (hence 'loop normals').
 * Useful to materialize sharp edges (or non-smooth faces) without actually modifying the geometry
 * (splitting edges).
 */
void BM_loops_calc_normal_vcos(BMesh *bm,
                               const float (*vcos)[3],
                               const float (*vnos)[3],
                               const float (*fnos)[3],
                               bool use_split_normals,
                               float split_angle,
                               float (*r_lnos)[3],
                               struct MLoopNorSpaceArray *r_lnors_spacearr,
                               short (*clnors_data)[2],
                               int cd_loop_clnors_offset,
                               bool do_rebuild);

/**
 * Check whether given loop is part of an unknown-so-far cyclic smooth fan, or not.
 * Needed because cyclic smooth fans have no obvious 'entry point',
 * and yet we need to walk them once, and only once.
 */
bool BM_loop_check_cyclic_smooth_fan(BMLoop *l_curr);
void BM_lnorspacearr_store(BMesh *bm, float (*r_lnors)[3]);
void BM_lnorspace_invalidate(BMesh *bm, bool do_invalidate_all);
void BM_lnorspace_rebuild(BMesh *bm, bool preserve_clnor);
/**
 * \warning This function sets #BM_ELEM_TAG on loops & edges via #bm_mesh_loops_calc_normals,
 * take care to run this before setting up tags.
 */
void BM_lnorspace_update(BMesh *bm);
void BM_normals_loops_edges_tag(BMesh *bm, bool do_edges);
#ifndef NDEBUG
void BM_lnorspace_err(BMesh *bm);
#endif

/* Loop Generics */
struct BMLoopNorEditDataArray *BM_loop_normal_editdata_array_init(BMesh *bm,
                                                                  bool do_all_loops_of_vert);
void BM_loop_normal_editdata_array_free(struct BMLoopNorEditDataArray *lnors_ed_arr);

/**
 * \warning This function sets #BM_ELEM_TAG on loops & edges via #bm_mesh_loops_calc_normals,
 * take care to run this before setting up tags.
 */
bool BM_custom_loop_normals_to_vector_layer(struct BMesh *bm);
void BM_custom_loop_normals_from_vector_layer(struct BMesh *bm, bool add_sharp_edges);

/**
 * Define sharp edges as needed to mimic 'autosmooth' from angle threshold.
 *
 * Used when defining an empty custom loop normals data layer,
 * to keep same shading as with auto-smooth!
 */
void BM_edges_sharp_from_angle_set(BMesh *bm, float split_angle);

#ifdef __cplusplus
}
#endif
