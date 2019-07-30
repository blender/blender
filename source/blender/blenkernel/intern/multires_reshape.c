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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "BKE_ccg.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subdiv_eval.h"
#include "BKE_subdiv_foreach.h"
#include "BKE_subdiv_mesh.h"

#include "DEG_depsgraph_query.h"

static void multires_reshape_init_mmd(MultiresModifierData *reshape_mmd,
                                      const MultiresModifierData *mmd)
{
  *reshape_mmd = *mmd;
}

static void multires_reshape_init_mmd_top_level(MultiresModifierData *reshape_mmd,
                                                const MultiresModifierData *mmd)
{
  *reshape_mmd = *mmd;
  reshape_mmd->lvl = reshape_mmd->totlvl;
}

/* =============================================================================
 * General reshape implementation, reused by all particular cases.
 */

typedef struct MultiresReshapeContext {
  Subdiv *subdiv;
  const Mesh *coarse_mesh;
  MDisps *mdisps;
  GridPaintMask *grid_paint_mask;
  int top_grid_size;
  int top_level;
  /* Indexed by coarse face index, returns first ptex face index corresponding
   * to that coarse face. */
  int *face_ptex_offset;
} MultiresReshapeContext;

static void multires_reshape_allocate_displacement_grid(MDisps *displacement_grid, const int level)
{
  const int grid_size = BKE_subdiv_grid_size_from_level(level);
  const int grid_area = grid_size * grid_size;
  float(*disps)[3] = MEM_calloc_arrayN(grid_area, 3 * sizeof(float), "multires disps");
  if (displacement_grid->disps != NULL) {
    MEM_freeN(displacement_grid->disps);
  }
  displacement_grid->disps = disps;
  displacement_grid->totdisp = grid_area;
  displacement_grid->level = level;
}

static void multires_reshape_ensure_displacement_grid(MDisps *displacement_grid, const int level)
{
  if (displacement_grid->disps != NULL && displacement_grid->level == level) {
    return;
  }
  multires_reshape_allocate_displacement_grid(displacement_grid, level);
}

static void multires_reshape_ensure_displacement_grids(Mesh *mesh, const int grid_level)
{
  const int num_grids = mesh->totloop;
  MDisps *mdisps = CustomData_get_layer(&mesh->ldata, CD_MDISPS);
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    multires_reshape_ensure_displacement_grid(&mdisps[grid_index], grid_level);
  }
}

static void multires_reshape_ensure_mask_grids(Mesh *mesh, const int grid_level)
{
  GridPaintMask *grid_paint_masks = CustomData_get_layer(&mesh->ldata, CD_GRID_PAINT_MASK);
  if (grid_paint_masks == NULL) {
    return;
  }
  const int num_grids = mesh->totloop;
  const int grid_size = BKE_subdiv_grid_size_from_level(grid_level);
  const int grid_area = grid_size * grid_size;
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    GridPaintMask *grid_paint_mask = &grid_paint_masks[grid_index];
    if (grid_paint_mask->level == grid_level) {
      continue;
    }
    grid_paint_mask->level = grid_level;
    if (grid_paint_mask->data) {
      MEM_freeN(grid_paint_mask->data);
    }
    grid_paint_mask->data = MEM_calloc_arrayN(grid_area, sizeof(float), "gpm.data");
  }
}

static void multires_reshape_ensure_grids(Mesh *mesh, const int grid_level)
{
  multires_reshape_ensure_displacement_grids(mesh, grid_level);
  multires_reshape_ensure_mask_grids(mesh, grid_level);
}

/* Convert normalized coordinate within a grid to a normalized coordinate within
 * a ptex face. */
static void multires_reshape_corner_coord_to_ptex(const MPoly *coarse_poly,
                                                  const int corner,
                                                  const float corner_u,
                                                  const float corner_v,
                                                  float *r_ptex_face_u,
                                                  float *r_ptex_face_v)
{
  if (coarse_poly->totloop == 4) {
    float grid_u, grid_v;
    BKE_subdiv_ptex_face_uv_to_grid_uv(corner_u, corner_v, &grid_u, &grid_v);
    BKE_subdiv_rotate_grid_to_quad(corner, grid_u, grid_v, r_ptex_face_u, r_ptex_face_v);
  }
  else {
    *r_ptex_face_u = corner_u;
    *r_ptex_face_v = corner_v;
  }
}

/* NOTE: The tangent vectors are measured in ptex face normalized coordinates,
 * which is different from grid tangent. */
static void multires_reshape_sample_surface(Subdiv *subdiv,
                                            const MPoly *coarse_poly,
                                            const int corner,
                                            const float corner_u,
                                            const float corner_v,
                                            const int ptex_face_index,
                                            float r_P[3],
                                            float r_dPdu[3],
                                            float r_dPdv[3])
{
  float ptex_face_u, ptex_face_v;
  multires_reshape_corner_coord_to_ptex(
      coarse_poly, corner, corner_u, corner_v, &ptex_face_u, &ptex_face_v);
  BKE_subdiv_eval_limit_point_and_derivatives(
      subdiv, ptex_face_index, ptex_face_u, ptex_face_v, r_P, r_dPdu, r_dPdv);
}

static void multires_reshape_tangent_matrix_for_corner(const MPoly *coarse_poly,
                                                       const int coarse_corner,
                                                       const float dPdu[3],
                                                       const float dPdv[3],
                                                       float r_tangent_matrix[3][3])
{
  /* For a quad faces we would need to flip the tangent, since they will use
   * use different coordinates within displacement grid comparent to ptex
   * face. */
  const bool is_quad = (coarse_poly->totloop == 4);
  const int tangent_corner = is_quad ? coarse_corner : 0;
  BKE_multires_construct_tangent_matrix(r_tangent_matrix, dPdu, dPdv, tangent_corner);
}

static void multires_reshape_vertex_from_final_data(MultiresReshapeContext *ctx,
                                                    const int ptex_face_index,
                                                    const float corner_u,
                                                    const float corner_v,
                                                    const int coarse_poly_index,
                                                    const int coarse_corner,
                                                    const float final_P[3],
                                                    const float final_mask)
{
  Subdiv *subdiv = ctx->subdiv;
  const int grid_size = ctx->top_grid_size;
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const MPoly *coarse_poly = &coarse_mpoly[coarse_poly_index];
  const int loop_index = coarse_poly->loopstart + coarse_corner;
  /* Evaluate limit surface. */
  float P[3], dPdu[3], dPdv[3];
  multires_reshape_sample_surface(
      subdiv, coarse_poly, coarse_corner, corner_u, corner_v, ptex_face_index, P, dPdu, dPdv);
  /* Construct tangent matrix which matches orientation of the current
   * displacement grid. */
  float tangent_matrix[3][3], inv_tangent_matrix[3][3];
  multires_reshape_tangent_matrix_for_corner(
      coarse_poly, coarse_corner, dPdu, dPdv, tangent_matrix);
  invert_m3_m3(inv_tangent_matrix, tangent_matrix);
  /* Convert object coordinate to a tangent space of displacement grid. */
  float D[3];
  sub_v3_v3v3(D, final_P, P);
  float tangent_D[3];
  mul_v3_m3v3(tangent_D, inv_tangent_matrix, D);
  /* Calculate index of element within the grid. */
  float grid_u, grid_v;
  BKE_subdiv_ptex_face_uv_to_grid_uv(corner_u, corner_v, &grid_u, &grid_v);
  const int grid_x = (grid_u * (grid_size - 1) + 0.5f);
  const int grid_y = (grid_v * (grid_size - 1) + 0.5f);
  const int index = grid_y * grid_size + grid_x;
  /* Write tangent displacement. */
  MDisps *displacement_grid = &ctx->mdisps[loop_index];
  copy_v3_v3(displacement_grid->disps[index], tangent_D);
  /* Write mask grid. */
  if (ctx->grid_paint_mask != NULL) {
    GridPaintMask *grid_paint_mask = &ctx->grid_paint_mask[loop_index];
    BLI_assert(grid_paint_mask->level == displacement_grid->level);
    grid_paint_mask->data[index] = final_mask;
  }
}

/* =============================================================================
 * Helpers to propagate displacement to higher levels.
 */

typedef struct MultiresPropagateData {
  /* Number of displacement grids. */
  int num_grids;
  /* Resolution level up to which displacement is known. */
  int reshape_level;
  /* Resolution up to which propagation is happening, affecting all the
   * levels in [reshape_level + 1, top_level]. */
  int top_level;
  /* Grid sizes at the corresponding levels. */
  int reshape_grid_size;
  int top_grid_size;
  /* Keys to access CCG at different levels. */
  CCGKey reshape_level_key;
  CCGKey top_level_key;
  /* Original grid data, before any updates for reshape.
   * Contains data at the reshape_level resolution level. */
  CCGElem **orig_grids_data;
  /* Custom data layers from a coarse mesh. */
  MDisps *mdisps;
  GridPaintMask *grid_paint_mask;
} MultiresPropagateData;

static CCGElem **allocate_grids(CCGKey *key, int num_grids)
{
  CCGElem **grids = MEM_calloc_arrayN(num_grids, sizeof(CCGElem *), "reshape grids*");
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    grids[grid_index] = MEM_calloc_arrayN(
        key->elem_size, key->grid_area, "reshape orig_grids_data elems");
  }
  return grids;
}

static void free_grids(CCGElem **grids, int num_grids)
{
  if (grids == NULL) {
    return;
  }
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    MEM_freeN(grids[grid_index]);
  }
  MEM_freeN(grids);
}

/* Initialize element sizes and offsets. */
static void multires_reshape_init_key_layers(CCGKey *key, const MultiresPropagateData *data)
{
  key->elem_size = 3 * sizeof(float);
  if (data->grid_paint_mask != NULL) {
    key->mask_offset = 3 * sizeof(float);
    key->elem_size += sizeof(float);
    key->has_mask = true;
  }
  else {
    key->mask_offset = -1;
    key->has_mask = false;
  }
  /* We never have normals in original grids. */
  key->normal_offset = -1;
  key->has_normals = false;
}

/* Initialize key used to access reshape grids at given level. */
static void multires_reshape_init_level_key(CCGKey *key,
                                            const MultiresPropagateData *data,
                                            const int level)
{
  key->level = level;
  /* Init layers. */
  multires_reshape_init_key_layers(key, data);
  /* By default, only 3 floats for coordinate, */
  key->grid_size = BKE_subdiv_grid_size_from_level(key->level);
  key->grid_area = key->grid_size * key->grid_size;
  key->grid_bytes = key->elem_size * key->grid_area;
}

static void multires_reshape_store_original_grids(MultiresPropagateData *data)
{
  const int num_grids = data->num_grids;
  /* Original data to be backed up. */
  const MDisps *mdisps = data->mdisps;
  const GridPaintMask *grid_paint_mask = data->grid_paint_mask;
  /* Allocate grids for backup. */
  CCGKey *orig_key = &data->reshape_level_key;
  CCGElem **orig_grids_data = allocate_grids(orig_key, num_grids);
  /* Fill in grids. */
  const int orig_grid_size = data->reshape_grid_size;
  const int top_grid_size = data->top_grid_size;
  const int skip = (top_grid_size - 1) / (orig_grid_size - 1);
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    CCGElem *orig_grid = orig_grids_data[grid_index];
    for (int y = 0; y < orig_grid_size; y++) {
      const int top_y = y * skip;
      for (int x = 0; x < orig_grid_size; x++) {
        const int top_x = x * skip;
        const int top_index = top_y * top_grid_size + top_x;
        memcpy(CCG_grid_elem_co(orig_key, orig_grid, x, y),
               mdisps[grid_index].disps[top_index],
               sizeof(float) * 3);
        if (orig_key->has_mask) {
          *CCG_grid_elem_mask(
              orig_key, orig_grid, x, y) = grid_paint_mask[grid_index].data[top_index];
        }
      }
    }
  }
  /* Store in the context. */
  data->orig_grids_data = orig_grids_data;
}

static void multires_reshape_propagate_prepare(MultiresPropagateData *data,
                                               Mesh *coarse_mesh,
                                               const int reshape_level,
                                               const int top_level)
{
  BLI_assert(reshape_level <= top_level);
  memset(data, 0, sizeof(*data));
  data->num_grids = coarse_mesh->totloop;
  data->reshape_level = reshape_level;
  data->top_level = top_level;
  if (reshape_level == top_level) {
    /* Nothing to do, reshape will happen on the whole grid content. */
    return;
  }
  data->mdisps = CustomData_get_layer(&coarse_mesh->ldata, CD_MDISPS);
  data->grid_paint_mask = CustomData_get_layer(&coarse_mesh->ldata, CD_GRID_PAINT_MASK);
  data->top_grid_size = BKE_subdiv_grid_size_from_level(top_level);
  data->reshape_grid_size = BKE_subdiv_grid_size_from_level(reshape_level);
  /* Initialize keys to access CCG at different levels. */
  multires_reshape_init_level_key(&data->reshape_level_key, data, data->reshape_level);
  multires_reshape_init_level_key(&data->top_level_key, data, data->top_level);
  /* Make a copy of grids before reshaping, so we can calculate deltas
   * later on. */
  multires_reshape_store_original_grids(data);
}

static void multires_reshape_propagate_prepare_from_mmd(MultiresPropagateData *data,
                                                        struct Depsgraph *depsgraph,
                                                        Object *object,
                                                        const MultiresModifierData *mmd,
                                                        const int top_level,
                                                        const bool use_render_params)
{
  /* TODO(sergey): Find mode reliable way of getting current level. */
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Mesh *mesh = object->data;
  const int level = multires_get_level(scene_eval, object, mmd, use_render_params, true);
  multires_reshape_propagate_prepare(data, mesh, level, top_level);
}

/* Calculate delta of changed reshape level data layers. Delta goes to a
 * grids at top level (meaning, the result grids are only partially filled
 * in). */
static void multires_reshape_calculate_delta(MultiresPropagateData *data,
                                             CCGElem **delta_grids_data)
{
  const int num_grids = data->num_grids;
  /* At this point those custom data layers has updated data for the
   * level we are propagating from. */
  const MDisps *mdisps = data->mdisps;
  const GridPaintMask *grid_paint_mask = data->grid_paint_mask;
  CCGKey *reshape_key = &data->reshape_level_key;
  CCGKey *delta_level_key = &data->top_level_key;
  /* Calculate delta. */
  const int top_grid_size = data->top_grid_size;
  const int reshape_grid_size = data->reshape_grid_size;
  const int delta_grid_size = data->top_grid_size;
  const int skip = (top_grid_size - 1) / (reshape_grid_size - 1);
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    /*const*/ CCGElem *orig_grid = data->orig_grids_data[grid_index];
    CCGElem *delta_grid = delta_grids_data[grid_index];
    for (int y = 0; y < reshape_grid_size; y++) {
      const int top_y = y * skip;
      for (int x = 0; x < reshape_grid_size; x++) {
        const int top_x = x * skip;
        const int top_index = top_y * delta_grid_size + top_x;
        sub_v3_v3v3(CCG_grid_elem_co(delta_level_key, delta_grid, top_x, top_y),
                    mdisps[grid_index].disps[top_index],
                    CCG_grid_elem_co(reshape_key, orig_grid, x, y));
        if (delta_level_key->has_mask) {
          const float old_mask_value = *CCG_grid_elem_mask(reshape_key, orig_grid, x, y);
          const float new_mask_value = grid_paint_mask[grid_index].data[top_index];
          *CCG_grid_elem_mask(delta_level_key, delta_grid, top_x, top_y) = new_mask_value -
                                                                           old_mask_value;
        }
      }
    }
  }
}

/* Makes it so delta is propagated onto all the higher levels, but is also
 * that this delta is smoothed in a way that it does not cause artifacts on
 * boundaries. */

typedef struct MultiresPropagateCornerData {
  float coord_delta[3];
  float mask_delta;
} MultiresPropagateCornerData;

BLI_INLINE void multires_reshape_propagate_init_patch_corners(
    MultiresPropagateData *data,
    CCGElem *delta_grid,
    const int patch_x,
    const int patch_y,
    MultiresPropagateCornerData r_corners[4])
{
  CCGKey *delta_level_key = &data->top_level_key;
  const int orig_grid_size = data->reshape_grid_size;
  const int top_grid_size = data->top_grid_size;
  const int skip = (top_grid_size - 1) / (orig_grid_size - 1);
  const int x = patch_x * skip;
  const int y = patch_y * skip;
  /* Store coordinate deltas. */
  copy_v3_v3(r_corners[0].coord_delta, CCG_grid_elem_co(delta_level_key, delta_grid, x, y));
  copy_v3_v3(r_corners[1].coord_delta, CCG_grid_elem_co(delta_level_key, delta_grid, x + skip, y));
  copy_v3_v3(r_corners[2].coord_delta, CCG_grid_elem_co(delta_level_key, delta_grid, x, y + skip));
  copy_v3_v3(r_corners[3].coord_delta,
             CCG_grid_elem_co(delta_level_key, delta_grid, x + skip, y + skip));
  if (delta_level_key->has_mask) {
    r_corners[0].mask_delta = *CCG_grid_elem_mask(delta_level_key, delta_grid, x, y);
    r_corners[1].mask_delta = *CCG_grid_elem_mask(delta_level_key, delta_grid, x + skip, y);
    r_corners[2].mask_delta = *CCG_grid_elem_mask(delta_level_key, delta_grid, x, y + skip);
    r_corners[3].mask_delta = *CCG_grid_elem_mask(delta_level_key, delta_grid, x + skip, y + skip);
  }
}

BLI_INLINE void multires_reshape_propagate_interpolate_coord(
    float delta[3], const MultiresPropagateCornerData corners[4], const float weights[4])
{
  interp_v3_v3v3v3v3(delta,
                     corners[0].coord_delta,
                     corners[1].coord_delta,
                     corners[2].coord_delta,
                     corners[3].coord_delta,
                     weights);
}

BLI_INLINE float multires_reshape_propagate_interpolate_mask(
    const MultiresPropagateCornerData corners[4], const float weights[4])
{
  return corners[0].mask_delta * weights[0] + corners[1].mask_delta * weights[1] +
         corners[2].mask_delta * weights[2] + corners[3].mask_delta * weights[3];
}

BLI_INLINE void multires_reshape_propagate_and_smooth_delta_grid_patch(MultiresPropagateData *data,
                                                                       CCGElem *delta_grid,
                                                                       const int patch_x,
                                                                       const int patch_y)
{
  CCGKey *delta_level_key = &data->top_level_key;
  const int orig_grid_size = data->reshape_grid_size;
  const int top_grid_size = data->top_grid_size;
  const int skip = (top_grid_size - 1) / (orig_grid_size - 1);
  const float skip_inv = 1.0f / (float)skip;
  MultiresPropagateCornerData corners[4];
  multires_reshape_propagate_init_patch_corners(data, delta_grid, patch_x, patch_y, corners);
  const int start_x = patch_x * skip;
  const int start_y = patch_y * skip;
  for (int y = 0; y <= skip; y++) {
    const float v = (float)y * skip_inv;
    const int final_y = start_y + y;
    for (int x = 0; x <= skip; x++) {
      const float u = (float)x * skip_inv;
      const int final_x = start_x + x;
      const float linear_weights[4] = {
          (1.0f - u) * (1.0f - v), u * (1.0f - v), (1.0f - u) * v, u * v};
      multires_reshape_propagate_interpolate_coord(
          CCG_grid_elem_co(delta_level_key, delta_grid, final_x, final_y),
          corners,
          linear_weights);
      if (delta_level_key->has_mask) {
        float *mask = CCG_grid_elem_mask(delta_level_key, delta_grid, final_x, final_y);
        *mask = multires_reshape_propagate_interpolate_mask(corners, linear_weights);
      }
    }
  }
}

BLI_INLINE void multires_reshape_propagate_and_smooth_delta_grid(MultiresPropagateData *data,
                                                                 CCGElem *delta_grid)
{
  const int orig_grid_size = data->reshape_grid_size;
  for (int patch_y = 0; patch_y < orig_grid_size - 1; patch_y++) {
    for (int patch_x = 0; patch_x < orig_grid_size - 1; patch_x++) {
      multires_reshape_propagate_and_smooth_delta_grid_patch(data, delta_grid, patch_x, patch_y);
    }
  }
}

/* Entry point to propagate+smooth. */
static void multires_reshape_propagate_and_smooth_delta(MultiresPropagateData *data,
                                                        CCGElem **delta_grids_data)
{
  const int num_grids = data->num_grids;
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    CCGElem *delta_grid = delta_grids_data[grid_index];
    multires_reshape_propagate_and_smooth_delta_grid(data, delta_grid);
  }
}

/* Apply smoothed deltas on the actual data layers. */
static void multires_reshape_propagate_apply_delta(MultiresPropagateData *data,
                                                   CCGElem **delta_grids_data)
{
  const int num_grids = data->num_grids;
  /* At this point those custom data layers has updated data for the
   * level we are propagating from. */
  MDisps *mdisps = data->mdisps;
  GridPaintMask *grid_paint_mask = data->grid_paint_mask;
  CCGKey *orig_key = &data->reshape_level_key;
  CCGKey *delta_level_key = &data->top_level_key;
  CCGElem **orig_grids_data = data->orig_grids_data;
  const int orig_grid_size = data->reshape_grid_size;
  const int top_grid_size = data->top_grid_size;
  const int skip = (top_grid_size - 1) / (orig_grid_size - 1);
  /* Restore grid values at the reshape level. Those values are to be changed
   * to the accommodate for the smooth delta. */
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    CCGElem *orig_grid = orig_grids_data[grid_index];
    for (int y = 0; y < orig_grid_size; y++) {
      const int top_y = y * skip;
      for (int x = 0; x < orig_grid_size; x++) {
        const int top_x = x * skip;
        const int top_index = top_y * top_grid_size + top_x;
        copy_v3_v3(mdisps[grid_index].disps[top_index],
                   CCG_grid_elem_co(orig_key, orig_grid, x, y));
        if (grid_paint_mask != NULL) {
          grid_paint_mask[grid_index].data[top_index] = *CCG_grid_elem_mask(
              orig_key, orig_grid, x, y);
        }
      }
    }
  }
  /* Add smoothed delta to all the levels. */
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    CCGElem *delta_grid = delta_grids_data[grid_index];
    for (int y = 0; y < top_grid_size; y++) {
      for (int x = 0; x < top_grid_size; x++) {
        const int top_index = y * top_grid_size + x;
        add_v3_v3(mdisps[grid_index].disps[top_index],
                  CCG_grid_elem_co(delta_level_key, delta_grid, x, y));
        if (delta_level_key->has_mask) {
          grid_paint_mask[grid_index].data[top_index] += *CCG_grid_elem_mask(
              delta_level_key, delta_grid, x, y);
        }
      }
    }
  }
}

static void multires_reshape_propagate(MultiresPropagateData *data)
{
  if (data->reshape_level == data->top_level) {
    return;
  }
  const int num_grids = data->num_grids;
  /* Calculate delta made at the reshape level. */
  CCGKey *delta_level_key = &data->top_level_key;
  CCGElem **delta_grids_data = allocate_grids(delta_level_key, num_grids);
  multires_reshape_calculate_delta(data, delta_grids_data);
  /* Propagate deltas to the higher levels. */
  multires_reshape_propagate_and_smooth_delta(data, delta_grids_data);
  /* Finally, apply smoothed deltas. */
  multires_reshape_propagate_apply_delta(data, delta_grids_data);
  /* Cleanup. */
  free_grids(delta_grids_data, num_grids);
}

static void multires_reshape_propagate_free(MultiresPropagateData *data)
{
  free_grids(data->orig_grids_data, data->num_grids);
}

/* =============================================================================
 * Reshape from deformed vertex coordinates.
 */

typedef struct MultiresReshapeFromDeformedVertsContext {
  MultiresReshapeContext reshape_ctx;
  const float (*deformed_verts)[3];
  int num_deformed_verts;
} MultiresReshapeFromDeformedVertsContext;

static bool multires_reshape_topology_info(const SubdivForeachContext *foreach_context,
                                           const int num_vertices,
                                           const int UNUSED(num_edges),
                                           const int UNUSED(num_loops),
                                           const int UNUSED(num_polygons))
{
  MultiresReshapeFromDeformedVertsContext *ctx = foreach_context->user_data;
  if (num_vertices != ctx->num_deformed_verts) {
    return false;
  }
  return true;
}

/* Will run reshaping for all grid elements which are adjacent to the given
 * one. This is the way to ensure continuity of displacement stored in the
 * grids across the inner boundaries of the grids. */
static void multires_reshape_neighour_boundary_vertices(MultiresReshapeContext *ctx,
                                                        const int UNUSED(ptex_face_index),
                                                        const float corner_u,
                                                        const float corner_v,
                                                        const int coarse_poly_index,
                                                        const int coarse_corner,
                                                        const float final_P[3],
                                                        const float final_mask)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const MPoly *coarse_poly = &coarse_mpoly[coarse_poly_index];
  const int num_corners = coarse_poly->totloop;
  const int start_ptex_face_index = ctx->face_ptex_offset[coarse_poly_index];
  const bool is_quad = (coarse_poly->totloop == 4);
  if (corner_u == 1.0f && corner_v == 1.0f) {
    for (int current_corner = 0; current_corner < num_corners; ++current_corner) {
      if (current_corner == coarse_corner) {
        continue;
      }
      const int current_ptex_face_index = is_quad ? start_ptex_face_index :
                                                    start_ptex_face_index + current_corner;
      multires_reshape_vertex_from_final_data(ctx,
                                              current_ptex_face_index,
                                              1.0f,
                                              1.0f,
                                              coarse_poly_index,
                                              current_corner,
                                              final_P,
                                              final_mask);
    }
  }
  else if (corner_u == 1.0f) {
    const float next_corner_index = (coarse_corner + 1) % num_corners;
    const float next_corner_u = corner_v;
    const float next_corner_v = 1.0f;
    const int next_ptex_face_index = is_quad ? start_ptex_face_index :
                                               start_ptex_face_index + next_corner_index;
    multires_reshape_vertex_from_final_data(ctx,
                                            next_ptex_face_index,
                                            next_corner_u,
                                            next_corner_v,
                                            coarse_poly_index,
                                            next_corner_index,
                                            final_P,
                                            final_mask);
  }
  else if (corner_v == 1.0f) {
    const float prev_corner_index = (coarse_corner + num_corners - 1) % num_corners;
    const float prev_corner_u = 1.0f;
    const float prev_corner_v = corner_u;
    const int prev_ptex_face_index = is_quad ? start_ptex_face_index :
                                               start_ptex_face_index + prev_corner_index;
    multires_reshape_vertex_from_final_data(ctx,
                                            prev_ptex_face_index,
                                            prev_corner_u,
                                            prev_corner_v,
                                            coarse_poly_index,
                                            prev_corner_index,
                                            final_P,
                                            final_mask);
  }
}

static void multires_reshape_vertex(MultiresReshapeFromDeformedVertsContext *ctx,
                                    const int ptex_face_index,
                                    const float u,
                                    const float v,
                                    const int coarse_poly_index,
                                    const int coarse_corner,
                                    const int subdiv_vertex_index)
{
  const float *final_P = ctx->deformed_verts[subdiv_vertex_index];
  const Mesh *coarse_mesh = ctx->reshape_ctx.coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const MPoly *coarse_poly = &coarse_mpoly[coarse_poly_index];
  const bool is_quad = (coarse_poly->totloop == 4);
  float corner_u, corner_v;
  int actual_coarse_corner;
  if (is_quad) {
    actual_coarse_corner = BKE_subdiv_rotate_quad_to_corner(u, v, &corner_u, &corner_v);
  }
  else {
    actual_coarse_corner = coarse_corner;
    corner_u = u;
    corner_v = v;
  }
  multires_reshape_vertex_from_final_data(&ctx->reshape_ctx,
                                          ptex_face_index,
                                          corner_u,
                                          corner_v,
                                          coarse_poly_index,
                                          actual_coarse_corner,
                                          final_P,
                                          0.0f);
  multires_reshape_neighour_boundary_vertices(&ctx->reshape_ctx,
                                              ptex_face_index,
                                              corner_u,
                                              corner_v,
                                              coarse_poly_index,
                                              actual_coarse_corner,
                                              final_P,
                                              0.0f);
}

static void multires_reshape_vertex_inner(const SubdivForeachContext *foreach_context,
                                          void *UNUSED(tls_v),
                                          const int ptex_face_index,
                                          const float u,
                                          const float v,
                                          const int coarse_poly_index,
                                          const int coarse_corner,
                                          const int subdiv_vertex_index)
{
  MultiresReshapeFromDeformedVertsContext *ctx = foreach_context->user_data;
  multires_reshape_vertex(
      ctx, ptex_face_index, u, v, coarse_poly_index, coarse_corner, subdiv_vertex_index);
}

static void multires_reshape_vertex_every_corner(
    const struct SubdivForeachContext *foreach_context,
    void *UNUSED(tls_v),
    const int ptex_face_index,
    const float u,
    const float v,
    const int UNUSED(coarse_vertex_index),
    const int coarse_poly_index,
    const int coarse_corner,
    const int subdiv_vertex_index)
{
  MultiresReshapeFromDeformedVertsContext *ctx = foreach_context->user_data;
  multires_reshape_vertex(
      ctx, ptex_face_index, u, v, coarse_poly_index, coarse_corner, subdiv_vertex_index);
}

static void multires_reshape_vertex_every_edge(const struct SubdivForeachContext *foreach_context,
                                               void *UNUSED(tls_v),
                                               const int ptex_face_index,
                                               const float u,
                                               const float v,
                                               const int UNUSED(coarse_edge_index),
                                               const int coarse_poly_index,
                                               const int coarse_corner,
                                               const int subdiv_vertex_index)
{
  MultiresReshapeFromDeformedVertsContext *ctx = foreach_context->user_data;
  multires_reshape_vertex(
      ctx, ptex_face_index, u, v, coarse_poly_index, coarse_corner, subdiv_vertex_index);
}

static Subdiv *multires_create_subdiv_for_reshape(struct Depsgraph *depsgraph,
                                                  /*const*/ Object *object,
                                                  const MultiresModifierData *mmd)
{
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *object_eval = DEG_get_evaluated_object(depsgraph, object);
  Mesh *deformed_mesh = mesh_get_eval_deform(
      depsgraph, scene_eval, object_eval, &CD_MASK_BAREMESH);
  SubdivSettings subdiv_settings;
  BKE_multires_subdiv_settings_init(&subdiv_settings, mmd);
  Subdiv *subdiv = BKE_subdiv_new_from_mesh(&subdiv_settings, deformed_mesh);
  if (!BKE_subdiv_eval_update_from_mesh(subdiv, deformed_mesh)) {
    BKE_subdiv_free(subdiv);
    return NULL;
  }
  return subdiv;
}

static bool multires_reshape_from_vertcos(struct Depsgraph *depsgraph,
                                          Object *object,
                                          const MultiresModifierData *mmd,
                                          const float (*deformed_verts)[3],
                                          const int num_deformed_verts,
                                          const bool use_render_params)
{
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Mesh *coarse_mesh = object->data;
  MDisps *mdisps = CustomData_get_layer(&coarse_mesh->ldata, CD_MDISPS);
  /* Pick maximum between multires level and displacement level.
   * This is because mesh can be used by objects with multires at different
   * levels.
   *
   * TODO(sergey): At this point it should be possible to always use
   * mdisps->level. */
  const int top_level = max_ii(mmd->totlvl, mdisps->level);
  /* Make sure displacement grids are ready. */
  multires_reshape_ensure_grids(coarse_mesh, top_level);
  /* Initialize subdivision surface. */
  Subdiv *subdiv = multires_create_subdiv_for_reshape(depsgraph, object, mmd);
  if (subdiv == NULL) {
    return false;
  }
  /* Construct context. */
  MultiresReshapeFromDeformedVertsContext reshape_deformed_verts_ctx = {
      .reshape_ctx =
          {
              .subdiv = subdiv,
              .coarse_mesh = coarse_mesh,
              .mdisps = mdisps,
              .grid_paint_mask = NULL,
              .top_grid_size = BKE_subdiv_grid_size_from_level(top_level),
              .top_level = top_level,
              .face_ptex_offset = BKE_subdiv_face_ptex_offset_get(subdiv),
          },
      .deformed_verts = deformed_verts,
      .num_deformed_verts = num_deformed_verts,
  };
  SubdivForeachContext foreach_context = {
      .topology_info = multires_reshape_topology_info,
      .vertex_inner = multires_reshape_vertex_inner,
      .vertex_every_edge = multires_reshape_vertex_every_edge,
      .vertex_every_corner = multires_reshape_vertex_every_corner,
      .user_data = &reshape_deformed_verts_ctx,
  };
  /* Initialize mesh rasterization settings. */
  SubdivToMeshSettings mesh_settings;
  BKE_multires_subdiv_mesh_settings_init(
      &mesh_settings, scene_eval, object, mmd, use_render_params, true);
  /* Initialize propagation to higher levels. */
  MultiresPropagateData propagate_data;
  multires_reshape_propagate_prepare_from_mmd(
      &propagate_data, depsgraph, object, mmd, top_level, use_render_params);
  /* Run all the callbacks. */
  BKE_subdiv_foreach_subdiv_geometry(subdiv, &foreach_context, &mesh_settings, coarse_mesh);
  BKE_subdiv_free(subdiv);
  /* Update higher levels if needed. */
  multires_reshape_propagate(&propagate_data);
  multires_reshape_propagate_free(&propagate_data);
  return true;
}

/* =============================================================================
 * Reshape from object.
 */

/* Returns truth on success, false otherwise.
 *
 * This function might fail in cases like source and destination not having
 * matched amount of vertices. */
bool multiresModifier_reshapeFromObject(struct Depsgraph *depsgraph,
                                        MultiresModifierData *mmd,
                                        Object *dst,
                                        Object *src)
{
  /* Would be cool to support this eventually, but it is very tricky to match
   * vertices order even for meshes, when mixing meshes and other objects it's
   * even more tricky. */
  if (src->type != OB_MESH) {
    return false;
  }
  MultiresModifierData reshape_mmd;
  multires_reshape_init_mmd(&reshape_mmd, mmd);
  /* Get evaluated vertices locations to reshape to. */
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *src_eval = DEG_get_evaluated_object(depsgraph, src);
  Mesh *src_mesh_eval = mesh_get_eval_final(depsgraph, scene_eval, src_eval, &CD_MASK_BAREMESH);
  int num_deformed_verts;
  float(*deformed_verts)[3] = BKE_mesh_vertexCos_get(src_mesh_eval, &num_deformed_verts);
  bool result = multires_reshape_from_vertcos(
      depsgraph, dst, &reshape_mmd, deformed_verts, num_deformed_verts, false);
  MEM_freeN(deformed_verts);
  return result;
}

/* =============================================================================
 * Reshape from modifier.
 */

bool multiresModifier_reshapeFromDeformModifier(struct Depsgraph *depsgraph,
                                                MultiresModifierData *mmd,
                                                Object *object,
                                                ModifierData *md)
{
  MultiresModifierData highest_mmd;
  /* It is possible that the current subdivision level of multires is lower
   * that it's maximum possible one (i.e., viewport is set to a lower level
   * for the performance purposes). But even then, we want all the multires
   * levels to be reshaped. Most accurate way to do so is to ignore all
   * simplifications and calculate deformation modifier for the highest
   * possible multires level.
   * Alternative would be propagate displacement from current level to a
   * higher ones, but that is likely to cause artifacts. */
  multires_reshape_init_mmd_top_level(&highest_mmd, mmd);
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  /* Perform sanity checks and early output. */
  if (multires_get_level(scene_eval, object, &highest_mmd, false, true) == 0) {
    return false;
  }
  /* Create mesh for the multires, ignoring any further modifiers (leading
   * deformation modifiers will be applied though). */
  Mesh *multires_mesh = BKE_multires_create_mesh(depsgraph, scene_eval, &highest_mmd, object);
  int num_deformed_verts;
  float(*deformed_verts)[3] = BKE_mesh_vertexCos_get(multires_mesh, &num_deformed_verts);
  /* Apply deformation modifier on the multires, */
  const ModifierEvalContext modifier_ctx = {
      .depsgraph = depsgraph,
      .object = object,
      .flag = MOD_APPLY_USECACHE | MOD_APPLY_IGNORE_SIMPLIFY,
  };
  modwrap_deformVerts(md, &modifier_ctx, multires_mesh, deformed_verts, multires_mesh->totvert);
  BKE_id_free(NULL, multires_mesh);
  /* Reshaping */
  bool result = multires_reshape_from_vertcos(
      depsgraph, object, &highest_mmd, deformed_verts, num_deformed_verts, false);
  /* Cleanup */
  MEM_freeN(deformed_verts);
  return result;
}

/* =============================================================================
 * Reshape from grids.
 */

typedef struct ReshapeFromCCGTaskData {
  MultiresReshapeContext reshape_ctx;
  const CCGKey *key;
  /*const*/ CCGElem **grids;
} ReshapeFromCCGTaskData;

static void reshape_from_ccg_task(void *__restrict userdata,
                                  const int coarse_poly_index,
                                  const TaskParallelTLS *__restrict UNUSED(tls))
{
  ReshapeFromCCGTaskData *data = userdata;
  const CCGKey *key = data->key;
  /*const*/ CCGElem **grids = data->grids;
  const Mesh *coarse_mesh = data->reshape_ctx.coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const MPoly *coarse_poly = &coarse_mpoly[coarse_poly_index];
  const int key_grid_size = key->grid_size;
  const int key_grid_size_1 = key_grid_size - 1;
  const int resolution = key_grid_size;
  const float resolution_1_inv = 1.0f / (float)(resolution - 1);
  const int start_ptex_face_index = data->reshape_ctx.face_ptex_offset[coarse_poly_index];
  const bool is_quad = (coarse_poly->totloop == 4);
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    for (int y = 0; y < resolution; y++) {
      const float corner_v = y * resolution_1_inv;
      for (int x = 0; x < resolution; x++) {
        const float corner_u = x * resolution_1_inv;
        /* Quad faces consists of a single ptex face. */
        const int ptex_face_index = is_quad ? start_ptex_face_index :
                                              start_ptex_face_index + corner;
        float grid_u, grid_v;
        BKE_subdiv_ptex_face_uv_to_grid_uv(corner_u, corner_v, &grid_u, &grid_v);
        /*const*/ CCGElem *grid = grids[coarse_poly->loopstart + corner];
        /*const*/ CCGElem *grid_element = CCG_grid_elem(
            key, grid, key_grid_size_1 * grid_u, key_grid_size_1 * grid_v);
        const float *final_P = CCG_elem_co(key, grid_element);
        float final_mask = 0.0f;
        if (key->has_mask) {
          final_mask = *CCG_elem_mask(key, grid_element);
        }
        multires_reshape_vertex_from_final_data(&data->reshape_ctx,
                                                ptex_face_index,
                                                corner_u,
                                                corner_v,
                                                coarse_poly_index,
                                                corner,
                                                final_P,
                                                final_mask);
      }
    }
  }
}

bool multiresModifier_reshapeFromCCG(const int tot_level, Mesh *coarse_mesh, SubdivCCG *subdiv_ccg)
{
  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
  /* Sanity checks. */
  if (coarse_mesh->totloop != subdiv_ccg->num_grids) {
    /* Grids are supposed to eb created for each face-cornder (aka loop). */
    return false;
  }
  MDisps *mdisps = CustomData_get_layer(&coarse_mesh->ldata, CD_MDISPS);
  if (mdisps == NULL) {
    /* Multires displacement has been removed before current changes were
     * applies to all the levels. */
    return false;
  }
  GridPaintMask *grid_paint_mask = CustomData_get_layer(&coarse_mesh->ldata, CD_GRID_PAINT_MASK);
  Subdiv *subdiv = subdiv_ccg->subdiv;
  /* Pick maximum between multires level and displacement level.
   * This is because mesh can be used by objects with multires at different
   * levels.
   *
   * TODO(sergey): At this point it should be possible to always use
   * mdisps->level. */
  const int top_level = max_ii(tot_level, mdisps->level);
  /* Make sure displacement grids are ready. */
  multires_reshape_ensure_grids(coarse_mesh, top_level);
  /* Construct context. */
  ReshapeFromCCGTaskData data = {
      .reshape_ctx =
          {
              .subdiv = subdiv,
              .coarse_mesh = coarse_mesh,
              .mdisps = mdisps,
              .grid_paint_mask = grid_paint_mask,
              .top_grid_size = BKE_subdiv_grid_size_from_level(top_level),
              .top_level = top_level,
              .face_ptex_offset = BKE_subdiv_face_ptex_offset_get(subdiv),
          },
      .key = &key,
      .grids = subdiv_ccg->grids,
  };
  /* Initialize propagation to higher levels. */
  MultiresPropagateData propagate_data;
  multires_reshape_propagate_prepare(&propagate_data, coarse_mesh, key.level, top_level);
  /* Threaded grids iteration. */
  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  BLI_task_parallel_range(
      0, coarse_mesh->totpoly, &data, reshape_from_ccg_task, &parallel_range_settings);
  /* Update higher levels if needed. */
  multires_reshape_propagate(&propagate_data);
  multires_reshape_propagate_free(&propagate_data);
  return true;
}
