/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_customdata_types.h"

#include "BLI_offset_indices.hh"
#include "BLI_sys_types.h"

struct ReportList;

/**
 * Compute simplified tangent space normals, i.e.
 * tangent vector + sign of bi-tangent one, which combined with
 * split normals can be used to recreate the full tangent space.
 *
 * \note The mesh should be made of only triangles and quads!
 */
void BKE_mesh_calc_loop_tangent_single_ex(const float (*vert_positions)[3],
                                          int numVerts,
                                          const int *corner_verts,
                                          float (*r_looptangent)[4],
                                          const float (*corner_normals)[3],
                                          const float (*loopuv)[2],
                                          int numLoops,
                                          blender::OffsetIndices<int> faces,
                                          ReportList *reports);

/**
 * Wrapper around BKE_mesh_calc_loop_tangent_single_ex, which takes care of most boilerplate code.
 * \note
 * - There must be a valid loop's CD_NORMALS available.
 * - The mesh should be made of only triangles and quads!
 */
void BKE_mesh_calc_loop_tangent_single(Mesh *mesh,
                                       const char *uvmap,
                                       float (*r_looptangents)[4],
                                       ReportList *reports);

/**
 * See: #BKE_editmesh_loop_tangent_calc (matching logic).
 */
void BKE_mesh_calc_loop_tangent_ex(const float (*vert_positions)[3],
                                   blender::OffsetIndices<int> faces,
                                   const int *corner_verts,
                                   const blender::int3 *corner_tris,
                                   const int *corner_tri_faces,
                                   uint corner_tris_len,
                                   const blender::Span<bool> sharp_faces,

                                   const CustomData *loopdata,
                                   bool calc_active_tangent,
                                   const char (*tangent_names)[MAX_CUSTOMDATA_LAYER_NAME],
                                   int tangent_names_len,
                                   const float (*vert_normals)[3],
                                   const float (*face_normals)[3],
                                   const float (*corner_normals)[3],
                                   const float (*vert_orco)[3],
                                   /* result */
                                   CustomData *loopdata_out,
                                   uint loopdata_out_len,
                                   short *tangent_mask_curr_p);

void BKE_mesh_calc_loop_tangents(Mesh *mesh_eval,
                                 bool calc_active_tangent,
                                 const char (*tangent_names)[MAX_CUSTOMDATA_LAYER_NAME],
                                 int tangent_names_len);

/* Helpers */
void BKE_mesh_add_loop_tangent_named_layer_for_uv(const CustomData *uv_data,
                                                  CustomData *tan_data,
                                                  int numLoopData,
                                                  const char *layer_name);

#define DM_TANGENT_MASK_ORCO (1 << 9)
/**
 * Here we get some useful information such as active uv layer name and
 * search if it is already in tangent_names.
 * Also, we calculate tangent_mask that works as a descriptor of tangents state.
 * If tangent_mask has changed, then recalculate tangents.
 */
void BKE_mesh_calc_loop_tangent_step_0(const CustomData *loopData,
                                       bool calc_active_tangent,
                                       const char (*tangent_names)[MAX_CUSTOMDATA_LAYER_NAME],
                                       int tangent_names_count,
                                       bool *rcalc_act,
                                       bool *rcalc_ren,
                                       int *ract_uv_n,
                                       int *rren_uv_n,
                                       char *ract_uv_name,
                                       char *rren_uv_name,
                                       short *rtangent_mask);
