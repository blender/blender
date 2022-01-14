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
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ReportList;

/**
 * Compute simplified tangent space normals, i.e.
 * tangent vector + sign of bi-tangent one, which combined with
 * split normals can be used to recreate the full tangent space.
 * NOTE: * The mesh should be made of only tris and quads!
 */
void BKE_mesh_calc_loop_tangent_single_ex(const struct MVert *mverts,
                                          int numVerts,
                                          const struct MLoop *mloops,
                                          float (*r_looptangent)[4],
                                          float (*loopnors)[3],
                                          const struct MLoopUV *loopuv,
                                          int numLoops,
                                          const struct MPoly *mpolys,
                                          int numPolys,
                                          struct ReportList *reports);
/**
 * Wrapper around BKE_mesh_calc_loop_tangent_single_ex, which takes care of most boiling code.
 * \note
 * - There must be a valid loop's CD_NORMALS available.
 * - The mesh should be made of only tris and quads!
 */
void BKE_mesh_calc_loop_tangent_single(struct Mesh *mesh,
                                       const char *uvmap,
                                       float (*r_looptangents)[4],
                                       struct ReportList *reports);

/**
 * See: #BKE_editmesh_loop_tangent_calc (matching logic).
 */
void BKE_mesh_calc_loop_tangent_ex(const struct MVert *mvert,
                                   const struct MPoly *mpoly,
                                   uint mpoly_len,
                                   const struct MLoop *mloop,
                                   const struct MLoopTri *looptri,
                                   uint looptri_len,

                                   struct CustomData *loopdata,
                                   bool calc_active_tangent,
                                   const char (*tangent_names)[64],
                                   int tangent_names_len,
                                   const float (*vert_normals)[3],
                                   const float (*poly_normals)[3],
                                   const float (*loop_normals)[3],
                                   const float (*vert_orco)[3],
                                   /* result */
                                   struct CustomData *loopdata_out,
                                   uint loopdata_out_len,
                                   short *tangent_mask_curr_p);

void BKE_mesh_calc_loop_tangents(struct Mesh *me_eval,
                                 bool calc_active_tangent,
                                 const char (*tangent_names)[MAX_NAME],
                                 int tangent_names_len);

/* Helpers */
void BKE_mesh_add_loop_tangent_named_layer_for_uv(struct CustomData *uv_data,
                                                  struct CustomData *tan_data,
                                                  int numLoopData,
                                                  const char *layer_name);

#define DM_TANGENT_MASK_ORCO (1 << 9)
/**
 * Here we get some useful information such as active uv layer name and
 * search if it is already in tangent_names.
 * Also, we calculate tangent_mask that works as a descriptor of tangents state.
 * If tangent_mask has changed, then recalculate tangents.
 */
void BKE_mesh_calc_loop_tangent_step_0(const struct CustomData *loopData,
                                       bool calc_active_tangent,
                                       const char (*tangent_names)[64],
                                       int tangent_names_count,
                                       bool *rcalc_act,
                                       bool *rcalc_ren,
                                       int *ract_uv_n,
                                       int *rren_uv_n,
                                       char *ract_uv_name,
                                       char *rren_uv_name,
                                       short *rtangent_mask);

#ifdef __cplusplus
}
#endif
