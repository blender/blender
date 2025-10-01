/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "bmesh_class.hh"

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
void BM_mesh_normals_update_ex(BMesh *bm, const BMeshNormalsUpdate_Params *param);
void BM_mesh_normals_update(BMesh *bm);
/**
 * A version of #BM_mesh_normals_update that updates a subset of geometry,
 * used to avoid the overhead of updating everything.
 */
void BM_mesh_normals_update_with_partial_ex(BMesh *bm,
                                            const BMPartialUpdate *bmpinfo,
                                            const BMeshNormalsUpdate_Params *param);
void BM_mesh_normals_update_with_partial(BMesh *bm, const BMPartialUpdate *bmpinfo);

/**
 * \brief BMesh Compute Normals from/to external data.
 *
 * Computes the vertex normals of a mesh into vnos,
 * using given vertex coordinates (vcos) and polygon normals (fnos).
 */
void BM_verts_calc_normal_vcos(BMesh *bm,
                               blender::Span<blender::float3> fnos,
                               blender::Span<blender::float3> vcos,
                               blender::MutableSpan<blender::float3> vnos);
/**
 * \brief BMesh Compute Loop Normals from/to external data.
 *
 * Compute custom normals, i.e. vertex normals associated with each poly (hence 'loop normals').
 * Useful to materialize sharp edges (or non-smooth faces) without actually modifying the geometry
 * (splitting edges).
 */
void BM_loops_calc_normal_vcos(BMesh *bm,
                               blender::Span<blender::float3> vcos,
                               blender::Span<blender::float3> vnos,
                               blender::Span<blender::float3> fnos,
                               bool use_split_normals,
                               blender::MutableSpan<blender::float3> r_lnos,
                               MLoopNorSpaceArray *r_lnors_spacearr,
                               short (*clnors_data)[2],
                               int cd_loop_clnors_offset,
                               bool do_rebuild);

/**
 * Check whether given loop is part of an unknown-so-far cyclic smooth fan, or not.
 * Needed because cyclic smooth fans have no obvious 'entry point',
 * and yet we need to walk them once, and only once.
 */
bool BM_loop_check_cyclic_smooth_fan(BMLoop *l_curr);
void BM_lnorspacearr_store(BMesh *bm, blender::MutableSpan<blender::float3> r_lnors);
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

/**
 * Initialize loop data based on a type, overriding the #BMesh::selectmode of `bm`.
 * This can be useful if a single types selection is preferred,
 * instead of using mixed modes and the selection history.
 */
BMLoopNorEditDataArray *BM_loop_normal_editdata_array_init_with_htype(BMesh *bm,
                                                                      bool do_all_loops_of_vert,
                                                                      char htype_override);
BMLoopNorEditDataArray *BM_loop_normal_editdata_array_init(BMesh *bm, bool do_all_loops_of_vert);
void BM_loop_normal_editdata_array_free(BMLoopNorEditDataArray *lnors_ed_arr);

/**
 * \warning This function sets #BM_ELEM_TAG on loops & edges via #bm_mesh_loops_calc_normals,
 * take care to run this before setting up tags.
 */
bool BM_custom_loop_normals_to_vector_layer(BMesh *bm);
void BM_custom_loop_normals_from_vector_layer(BMesh *bm, bool add_sharp_edges);

/**
 * Define sharp edges as needed to mimic auto-smooth from angle threshold.
 *
 * Used when defining an empty custom loop normals data layer,
 * to keep same shading as with auto-smooth!
 */
void BM_edges_sharp_from_angle_set(BMesh *bm, float split_angle);
