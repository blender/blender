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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "multires_reshape.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_task.h"

#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_multires.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subdiv_eval.h"
#include "BKE_subdiv_foreach.h"
#include "BKE_subdiv_mesh.h"

#include "DEG_depsgraph_query.h"

/* ================================================================================================
 * Construct/destruct reshape context.
 */

/* Create subdivision surface descriptor which is configured for surface evaluation at a given
 * multires modifier. */
Subdiv *multires_reshape_create_subdiv(Depsgraph *depsgraph,
                                       /*const*/ Object *object,
                                       const MultiresModifierData *mmd)
{
  Mesh *base_mesh;

  if (depsgraph != NULL) {
    Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
    Object *object_eval = DEG_get_evaluated_object(depsgraph, object);
    base_mesh = mesh_get_eval_deform(depsgraph, scene_eval, object_eval, &CD_MASK_BAREMESH);
  }
  else {
    base_mesh = (Mesh *)object->data;
  }

  SubdivSettings subdiv_settings;
  BKE_multires_subdiv_settings_init(&subdiv_settings, mmd);
  Subdiv *subdiv = BKE_subdiv_new_from_mesh(&subdiv_settings, base_mesh);
  if (!BKE_subdiv_eval_begin_from_mesh(subdiv, base_mesh, NULL)) {
    BKE_subdiv_free(subdiv);
    return NULL;
  }
  return subdiv;
}

static void context_zero(MultiresReshapeContext *reshape_context)
{
  memset(reshape_context, 0, sizeof(*reshape_context));
}

static void context_init_lookup(MultiresReshapeContext *reshape_context)
{
  const Mesh *base_mesh = reshape_context->base_mesh;
  const MPoly *mpoly = base_mesh->mpoly;
  const int num_faces = base_mesh->totpoly;

  reshape_context->face_start_grid_index = MEM_malloc_arrayN(
      sizeof(int), num_faces, "face_start_grid_index");
  int num_grids = 0;
  int num_ptex_faces = 0;
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    const int num_corners = mpoly[face_index].totloop;
    reshape_context->face_start_grid_index[face_index] = num_grids;
    num_grids += num_corners;
    num_ptex_faces += (num_corners == 4) ? 1 : num_corners;
  }

  reshape_context->grid_to_face_index = MEM_malloc_arrayN(
      sizeof(int), num_grids, "grid_to_face_index");
  reshape_context->ptex_start_grid_index = MEM_malloc_arrayN(
      sizeof(int), num_ptex_faces, "ptex_start_grid_index");
  for (int face_index = 0, grid_index = 0, ptex_index = 0; face_index < num_faces; ++face_index) {
    const int num_corners = mpoly[face_index].totloop;
    const int num_face_ptex_faces = (num_corners == 4) ? 1 : num_corners;
    for (int i = 0; i < num_face_ptex_faces; ++i) {
      reshape_context->ptex_start_grid_index[ptex_index + i] = grid_index + i;
    }
    for (int corner = 0; corner < num_corners; ++corner, ++grid_index) {
      reshape_context->grid_to_face_index[grid_index] = face_index;
    }
    ptex_index += num_face_ptex_faces;
  }

  /* Store number of grids, which will be used for sanity checks. */
  reshape_context->num_grids = num_grids;
}

static void context_init_grid_pointers(MultiresReshapeContext *reshape_context)
{
  Mesh *base_mesh = reshape_context->base_mesh;
  reshape_context->mdisps = CustomData_get_layer(&base_mesh->ldata, CD_MDISPS);
  reshape_context->grid_paint_masks = CustomData_get_layer(&base_mesh->ldata, CD_GRID_PAINT_MASK);
}

static void context_init_commoon(MultiresReshapeContext *reshape_context)
{
  BLI_assert(reshape_context->subdiv != NULL);
  BLI_assert(reshape_context->base_mesh != NULL);

  reshape_context->face_ptex_offset = BKE_subdiv_face_ptex_offset_get(reshape_context->subdiv);

  context_init_lookup(reshape_context);
  context_init_grid_pointers(reshape_context);
}

static bool context_is_valid(MultiresReshapeContext *reshape_context)
{
  if (reshape_context->mdisps == NULL) {
    /* Multires displacement has been removed before current changes were applies. */
    return false;
  }
  return true;
}

static bool context_verify_or_free(MultiresReshapeContext *reshape_context)
{
  const bool is_valid = context_is_valid(reshape_context);
  if (!is_valid) {
    multires_reshape_context_free(reshape_context);
  }
  return is_valid;
}

bool multires_reshape_context_create_from_object(MultiresReshapeContext *reshape_context,
                                                 Depsgraph *depsgraph,
                                                 Object *object,
                                                 MultiresModifierData *mmd)
{
  context_zero(reshape_context);

  const bool use_render_params = false;
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Mesh *base_mesh = (Mesh *)object->data;

  reshape_context->depsgraph = depsgraph;
  reshape_context->object = object;
  reshape_context->mmd = mmd;

  reshape_context->base_mesh = base_mesh;

  reshape_context->subdiv = multires_reshape_create_subdiv(depsgraph, object, mmd);
  reshape_context->need_free_subdiv = true;

  reshape_context->reshape.level = multires_get_level(
      scene_eval, object, mmd, use_render_params, true);
  reshape_context->reshape.grid_size = BKE_subdiv_grid_size_from_level(
      reshape_context->reshape.level);

  reshape_context->top.level = mmd->totlvl;
  reshape_context->top.grid_size = BKE_subdiv_grid_size_from_level(reshape_context->top.level);

  context_init_commoon(reshape_context);

  return context_verify_or_free(reshape_context);
}

bool multires_reshape_context_create_from_ccg(MultiresReshapeContext *reshape_context,
                                              SubdivCCG *subdiv_ccg,
                                              Mesh *base_mesh,
                                              int top_level)
{
  context_zero(reshape_context);

  reshape_context->base_mesh = base_mesh;

  reshape_context->subdiv = subdiv_ccg->subdiv;
  reshape_context->need_free_subdiv = false;

  reshape_context->reshape.level = subdiv_ccg->level;
  reshape_context->reshape.grid_size = BKE_subdiv_grid_size_from_level(
      reshape_context->reshape.level);

  reshape_context->top.level = top_level;
  reshape_context->top.grid_size = BKE_subdiv_grid_size_from_level(reshape_context->top.level);

  context_init_commoon(reshape_context);

  return context_verify_or_free(reshape_context);
}

bool multires_reshape_context_create_from_subdivide(MultiresReshapeContext *reshape_context,
                                                    struct Object *object,
                                                    struct MultiresModifierData *mmd,
                                                    int top_level)
{
  context_zero(reshape_context);

  Mesh *base_mesh = (Mesh *)object->data;

  reshape_context->mmd = mmd;
  reshape_context->base_mesh = base_mesh;

  reshape_context->subdiv = multires_reshape_create_subdiv(NULL, object, mmd);
  reshape_context->need_free_subdiv = true;

  reshape_context->reshape.level = mmd->totlvl;
  reshape_context->reshape.grid_size = BKE_subdiv_grid_size_from_level(
      reshape_context->reshape.level);

  reshape_context->top.level = top_level;
  reshape_context->top.grid_size = BKE_subdiv_grid_size_from_level(reshape_context->top.level);

  context_init_commoon(reshape_context);

  return context_verify_or_free(reshape_context);
}

void multires_reshape_free_original_grids(MultiresReshapeContext *reshape_context)
{
  MDisps *orig_mdisps = reshape_context->orig.mdisps;
  GridPaintMask *orig_grid_paint_masks = reshape_context->orig.grid_paint_masks;

  if (orig_mdisps == NULL && orig_grid_paint_masks == NULL) {
    return;
  }

  const int num_grids = reshape_context->num_grids;
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    if (orig_mdisps != NULL) {
      MDisps *orig_grid = &orig_mdisps[grid_index];
      MEM_SAFE_FREE(orig_grid->disps);
    }
    if (orig_grid_paint_masks != NULL) {
      GridPaintMask *orig_paint_mask_grid = &orig_grid_paint_masks[grid_index];
      MEM_SAFE_FREE(orig_paint_mask_grid->data);
    }
  }

  MEM_SAFE_FREE(orig_mdisps);
  MEM_SAFE_FREE(orig_grid_paint_masks);

  reshape_context->orig.mdisps = NULL;
  reshape_context->orig.grid_paint_masks = NULL;
}

void multires_reshape_context_free(MultiresReshapeContext *reshape_context)
{
  if (reshape_context->need_free_subdiv) {
    BKE_subdiv_free(reshape_context->subdiv);
  }

  multires_reshape_free_original_grids(reshape_context);

  MEM_freeN(reshape_context->face_start_grid_index);
  MEM_freeN(reshape_context->ptex_start_grid_index);
  MEM_freeN(reshape_context->grid_to_face_index);
}

/* ================================================================================================
 * Helper accessors.
 */

/* For the given grid index get index of face it was created for. */
int multires_reshape_grid_to_face_index(const MultiresReshapeContext *reshape_context,
                                        int grid_index)
{
  BLI_assert(grid_index >= 0);
  BLI_assert(grid_index < reshape_context->num_grids);

  /* TODO(sergey): Optimization: when SubdivCCG is known we can calculate face index using
   * SubdivCCG::grid_faces and SubdivCCG::faces, saving memory used by grid_to_face_index. */

  return reshape_context->grid_to_face_index[grid_index];
}

/* For the given grid index get corner of a face it was created for. */
int multires_reshape_grid_to_corner(const MultiresReshapeContext *reshape_context, int grid_index)
{
  BLI_assert(grid_index >= 0);
  BLI_assert(grid_index < reshape_context->num_grids);

  /* TODO(sergey): Optimization: when SubdivCCG is known we can calculate face index using
   * SubdivCCG::grid_faces and SubdivCCG::faces, saving memory used by grid_to_face_index. */

  const int face_index = multires_reshape_grid_to_face_index(reshape_context, grid_index);
  return grid_index - reshape_context->face_start_grid_index[face_index];
}

bool multires_reshape_is_quad_face(const MultiresReshapeContext *reshape_context, int face_index)
{
  const MPoly *base_poly = &reshape_context->base_mesh->mpoly[face_index];
  return (base_poly->totloop == 4);
}

/* For the given grid index get index of corresponding ptex face. */
int multires_reshape_grid_to_ptex_index(const MultiresReshapeContext *reshape_context,
                                        int grid_index)
{
  const int face_index = multires_reshape_grid_to_face_index(reshape_context, grid_index);
  const int corner = multires_reshape_grid_to_corner(reshape_context, grid_index);
  const bool is_quad = multires_reshape_is_quad_face(reshape_context, face_index);
  return reshape_context->face_ptex_offset[face_index] + (is_quad ? 0 : corner);
}

/* Convert normalized coordinate within a grid to a normalized coordinate within a ptex face. */
PTexCoord multires_reshape_grid_coord_to_ptex(const MultiresReshapeContext *reshape_context,
                                              const GridCoord *grid_coord)
{
  PTexCoord ptex_coord;

  ptex_coord.ptex_face_index = multires_reshape_grid_to_ptex_index(reshape_context,
                                                                   grid_coord->grid_index);

  float corner_u, corner_v;
  BKE_subdiv_grid_uv_to_ptex_face_uv(grid_coord->u, grid_coord->v, &corner_u, &corner_v);

  const int face_index = multires_reshape_grid_to_face_index(reshape_context,
                                                             grid_coord->grid_index);
  const int corner = multires_reshape_grid_to_corner(reshape_context, grid_coord->grid_index);
  if (multires_reshape_is_quad_face(reshape_context, face_index)) {
    float grid_u, grid_v;
    BKE_subdiv_ptex_face_uv_to_grid_uv(corner_u, corner_v, &grid_u, &grid_v);
    BKE_subdiv_rotate_grid_to_quad(corner, grid_u, grid_v, &ptex_coord.u, &ptex_coord.v);
  }
  else {
    ptex_coord.u = corner_u;
    ptex_coord.v = corner_v;
  }

  return ptex_coord;
}

/* Convert a normalized coordinate within a ptex face to a normalized coordinate within a grid. */
GridCoord multires_reshape_ptex_coord_to_grid(const MultiresReshapeContext *reshape_context,
                                              const PTexCoord *ptex_coord)
{
  GridCoord grid_coord;

  const int start_grid_index = reshape_context->ptex_start_grid_index[ptex_coord->ptex_face_index];
  const int face_index = reshape_context->grid_to_face_index[start_grid_index];

  int corner_delta;
  if (multires_reshape_is_quad_face(reshape_context, face_index)) {
    corner_delta = BKE_subdiv_rotate_quad_to_corner(
        ptex_coord->u, ptex_coord->v, &grid_coord.u, &grid_coord.v);
  }
  else {
    corner_delta = 0;
    grid_coord.u = ptex_coord->u;
    grid_coord.v = ptex_coord->v;
  }
  grid_coord.grid_index = start_grid_index + corner_delta;

  BKE_subdiv_ptex_face_uv_to_grid_uv(grid_coord.u, grid_coord.v, &grid_coord.u, &grid_coord.v);

  return grid_coord;
}

void multires_reshape_tangent_matrix_for_corner(const MultiresReshapeContext *reshape_context,
                                                const int face_index,
                                                const int corner,
                                                const float dPdu[3],
                                                const float dPdv[3],
                                                float r_tangent_matrix[3][3])
{
  /* For a quad faces we would need to flip the tangent, since they will use
   * use different coordinates within displacement grid compared to the ptex face. */
  const bool is_quad = multires_reshape_is_quad_face(reshape_context, face_index);
  const int tangent_corner = is_quad ? corner : 0;
  BKE_multires_construct_tangent_matrix(r_tangent_matrix, dPdu, dPdv, tangent_corner);
}

ReshapeGridElement multires_reshape_grid_element_for_grid_coord(
    const MultiresReshapeContext *reshape_context, const GridCoord *grid_coord)
{
  ReshapeGridElement grid_element = {NULL, NULL};

  const int grid_size = reshape_context->top.grid_size;
  const int grid_x = lround(grid_coord->u * (grid_size - 1));
  const int grid_y = lround(grid_coord->v * (grid_size - 1));
  const int grid_element_index = grid_y * grid_size + grid_x;

  if (reshape_context->mdisps != NULL) {
    MDisps *displacement_grid = &reshape_context->mdisps[grid_coord->grid_index];
    grid_element.displacement = displacement_grid->disps[grid_element_index];
  }

  if (reshape_context->grid_paint_masks != NULL) {
    GridPaintMask *grid_paint_mask = &reshape_context->grid_paint_masks[grid_coord->grid_index];
    grid_element.mask = &grid_paint_mask->data[grid_element_index];
  }

  return grid_element;
}

ReshapeGridElement multires_reshape_grid_element_for_ptex_coord(
    const MultiresReshapeContext *reshape_context, const PTexCoord *ptex_coord)
{
  GridCoord grid_coord = multires_reshape_ptex_coord_to_grid(reshape_context, ptex_coord);
  return multires_reshape_grid_element_for_grid_coord(reshape_context, &grid_coord);
}

ReshapeConstGridElement multires_reshape_orig_grid_element_for_grid_coord(
    const MultiresReshapeContext *reshape_context, const GridCoord *grid_coord)
{
  ReshapeConstGridElement grid_element = {{0.0f, 0.0f, 0.0f}, 0.0f};

  const MDisps *mdisps = reshape_context->orig.mdisps;
  if (mdisps != NULL) {
    const MDisps *displacement_grid = &mdisps[grid_coord->grid_index];
    if (displacement_grid->disps != NULL) {
      const int grid_size = BKE_subdiv_grid_size_from_level(displacement_grid->level);
      const int grid_x = lround(grid_coord->u * (grid_size - 1));
      const int grid_y = lround(grid_coord->v * (grid_size - 1));
      const int grid_element_index = grid_y * grid_size + grid_x;
      copy_v3_v3(grid_element.displacement, displacement_grid->disps[grid_element_index]);
    }
  }

  const GridPaintMask *grid_paint_masks = reshape_context->orig.grid_paint_masks;
  if (grid_paint_masks != NULL) {
    const GridPaintMask *paint_mask_grid = &grid_paint_masks[grid_coord->grid_index];
    if (paint_mask_grid->data != NULL) {
      const int grid_size = BKE_subdiv_grid_size_from_level(paint_mask_grid->level);
      const int grid_x = lround(grid_coord->u * (grid_size - 1));
      const int grid_y = lround(grid_coord->v * (grid_size - 1));
      const int grid_element_index = grid_y * grid_size + grid_x;
      grid_element.mask = paint_mask_grid->data[grid_element_index];
    }
  }

  return grid_element;
}

/* ================================================================================================
 * Sample limit surface of the base mesh.
 */

void multires_reshape_evaluate_limit_at_grid(const MultiresReshapeContext *reshape_context,
                                             const GridCoord *grid_coord,
                                             float r_P[3],
                                             float r_tangent_matrix[3][3])
{
  float dPdu[3], dPdv[3];
  const PTexCoord ptex_coord = multires_reshape_grid_coord_to_ptex(reshape_context, grid_coord);
  Subdiv *subdiv = reshape_context->subdiv;
  BKE_subdiv_eval_limit_point_and_derivatives(
      subdiv, ptex_coord.ptex_face_index, ptex_coord.u, ptex_coord.v, r_P, dPdu, dPdv);

  const int face_index = multires_reshape_grid_to_face_index(reshape_context,
                                                             grid_coord->grid_index);
  const int corner = multires_reshape_grid_to_corner(reshape_context, grid_coord->grid_index);
  multires_reshape_tangent_matrix_for_corner(
      reshape_context, face_index, corner, dPdu, dPdv, r_tangent_matrix);
}

/* ================================================================================================
 * Custom data preparation.
 */

static void allocate_displacement_grid(MDisps *displacement_grid, const int level)
{
  const int grid_size = BKE_subdiv_grid_size_from_level(level);
  const int grid_area = grid_size * grid_size;
  float(*disps)[3] = MEM_calloc_arrayN(grid_area, 3 * sizeof(float), "multires disps");
  if (displacement_grid->disps != NULL) {
    MEM_freeN(displacement_grid->disps);
  }
  /* TODO(sergey): Preserve data on the old level. */
  displacement_grid->disps = disps;
  displacement_grid->totdisp = grid_area;
  displacement_grid->level = level;
}

static void ensure_displacement_grid(MDisps *displacement_grid, const int level)
{
  if (displacement_grid->disps != NULL && displacement_grid->level >= level) {
    return;
  }
  allocate_displacement_grid(displacement_grid, level);
}

static void ensure_displacement_grids(Mesh *mesh, const int grid_level)
{
  const int num_grids = mesh->totloop;
  MDisps *mdisps = CustomData_get_layer(&mesh->ldata, CD_MDISPS);
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    ensure_displacement_grid(&mdisps[grid_index], grid_level);
  }
}

static void ensure_mask_grids(Mesh *mesh, const int level)
{
  GridPaintMask *grid_paint_masks = CustomData_get_layer(&mesh->ldata, CD_GRID_PAINT_MASK);
  if (grid_paint_masks == NULL) {
    return;
  }
  const int num_grids = mesh->totloop;
  const int grid_size = BKE_subdiv_grid_size_from_level(level);
  const int grid_area = grid_size * grid_size;
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    GridPaintMask *grid_paint_mask = &grid_paint_masks[grid_index];
    if (grid_paint_mask->level >= level) {
      continue;
    }
    grid_paint_mask->level = level;
    if (grid_paint_mask->data) {
      MEM_freeN(grid_paint_mask->data);
    }
    /* TODO(sergey): Preserve data on the old level. */
    grid_paint_mask->data = MEM_calloc_arrayN(grid_area, sizeof(float), "gpm.data");
  }
}

void multires_reshape_ensure_grids(Mesh *mesh, const int level)
{
  ensure_displacement_grids(mesh, level);
  ensure_mask_grids(mesh, level);
}

/* ================================================================================================
 * Displacement, space conversion.
 */

void multires_reshape_store_original_grids(MultiresReshapeContext *reshape_context)
{
  const MDisps *mdisps = reshape_context->mdisps;
  const GridPaintMask *grid_paint_masks = reshape_context->grid_paint_masks;

  MDisps *orig_mdisps = MEM_dupallocN(mdisps);
  GridPaintMask *orig_grid_paint_masks = NULL;
  if (grid_paint_masks != NULL) {
    orig_grid_paint_masks = MEM_dupallocN(grid_paint_masks);
  }

  const int num_grids = reshape_context->num_grids;
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    MDisps *orig_grid = &orig_mdisps[grid_index];
    /* Ignore possibly invalid/non-allocated original grids. They will be replaced with 0 original
     * data when accessed during reshape process.
     * Reshape process will ensure all grids are on top level, but that happens on separate set of
     * grids which eventually replaces original one. */
    if (orig_grid->disps != NULL) {
      orig_grid->disps = MEM_dupallocN(orig_grid->disps);
    }
    if (orig_grid_paint_masks != NULL) {
      GridPaintMask *orig_paint_mask_grid = &orig_grid_paint_masks[grid_index];
      if (orig_paint_mask_grid->data != NULL) {
        orig_paint_mask_grid->data = MEM_dupallocN(orig_paint_mask_grid->data);
      }
    }
  }

  reshape_context->orig.mdisps = orig_mdisps;
  reshape_context->orig.grid_paint_masks = orig_grid_paint_masks;
}

typedef void (*ForeachGridCoordinateCallback)(const MultiresReshapeContext *reshape_context,
                                              const GridCoord *grid_coord,
                                              void *userdata_v);

typedef struct ForeachGridCoordinateTaskData {
  const MultiresReshapeContext *reshape_context;

  int grid_size;
  float grid_size_1_inv;

  ForeachGridCoordinateCallback callback;
  void *callback_userdata_v;
} ForeachGridCoordinateTaskData;

static void foreach_grid_face_coordinate_task(void *__restrict userdata_v,
                                              const int face_index,
                                              const TaskParallelTLS *__restrict UNUSED(tls))
{
  ForeachGridCoordinateTaskData *data = userdata_v;

  const MultiresReshapeContext *reshape_context = data->reshape_context;

  const Mesh *base_mesh = data->reshape_context->base_mesh;
  const MPoly *mpoly = base_mesh->mpoly;
  const int grid_size = data->grid_size;
  const float grid_size_1_inv = 1.0f / (((float)grid_size) - 1.0f);

  const int num_corners = mpoly[face_index].totloop;
  int grid_index = reshape_context->face_start_grid_index[face_index];
  for (int corner = 0; corner < num_corners; ++corner, ++grid_index) {
    for (int y = 0; y < grid_size; ++y) {
      const float v = (float)y * grid_size_1_inv;
      for (int x = 0; x < grid_size; ++x) {
        const float u = (float)x * grid_size_1_inv;

        GridCoord grid_coord;
        grid_coord.grid_index = grid_index;
        grid_coord.u = u;
        grid_coord.v = v;

        data->callback(data->reshape_context, &grid_coord, data->callback_userdata_v);
      }
    }
  }
}

/* Run given callback for every grid coordinate at a given level. */
static void foreach_grid_coordinate(const MultiresReshapeContext *reshape_context,
                                    const int level,
                                    ForeachGridCoordinateCallback callback,
                                    void *userdata_v)
{
  ForeachGridCoordinateTaskData data;
  data.reshape_context = reshape_context;
  data.grid_size = BKE_subdiv_grid_size_from_level(level);
  data.grid_size_1_inv = 1.0f / (((float)data.grid_size) - 1.0f);
  data.callback = callback;
  data.callback_userdata_v = userdata_v;

  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  parallel_range_settings.min_iter_per_thread = 1;

  const Mesh *base_mesh = reshape_context->base_mesh;
  const int num_faces = base_mesh->totpoly;
  BLI_task_parallel_range(
      0, num_faces, &data, foreach_grid_face_coordinate_task, &parallel_range_settings);
}

static void object_grid_element_to_tangent_displacement(
    const MultiresReshapeContext *reshape_context,
    const GridCoord *grid_coord,
    void *UNUSED(userdata_v))
{
  float P[3];
  float tangent_matrix[3][3];
  multires_reshape_evaluate_limit_at_grid(reshape_context, grid_coord, P, tangent_matrix);

  float inv_tangent_matrix[3][3];
  invert_m3_m3(inv_tangent_matrix, tangent_matrix);

  ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(reshape_context,
                                                                                 grid_coord);

  float D[3];
  sub_v3_v3v3(D, grid_element.displacement, P);

  float tangent_D[3];
  mul_v3_m3v3(tangent_D, inv_tangent_matrix, D);

  copy_v3_v3(grid_element.displacement, tangent_D);
}

void multires_reshape_object_grids_to_tangent_displacement(
    const MultiresReshapeContext *reshape_context)
{
  foreach_grid_coordinate(reshape_context,
                          reshape_context->top.level,
                          object_grid_element_to_tangent_displacement,
                          NULL);
}

/* ================================================================================================
 * MDISPS
 *
 * TODO(sergey): Make foreach_grid_coordinate more accessible and move this functionality to
 * own file. */

static void assign_final_coords_from_mdisps(const MultiresReshapeContext *reshape_context,
                                            const GridCoord *grid_coord,
                                            void *UNUSED(userdata_v))
{
  float P[3];
  float tangent_matrix[3][3];
  multires_reshape_evaluate_limit_at_grid(reshape_context, grid_coord, P, tangent_matrix);

  ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(reshape_context,
                                                                                 grid_coord);
  float D[3];
  mul_v3_m3v3(D, tangent_matrix, grid_element.displacement);

  add_v3_v3v3(grid_element.displacement, P, D);
}

void multires_reshape_assign_final_coords_from_mdisps(
    const MultiresReshapeContext *reshape_context)
{
  foreach_grid_coordinate(
      reshape_context, reshape_context->top.level, assign_final_coords_from_mdisps, NULL);
}

static void assign_final_coords_from_orig_mdisps(const MultiresReshapeContext *reshape_context,
                                                 const GridCoord *grid_coord,
                                                 void *UNUSED(userdata_v))
{
  float P[3];
  float tangent_matrix[3][3];
  multires_reshape_evaluate_limit_at_grid(reshape_context, grid_coord, P, tangent_matrix);

  const ReshapeConstGridElement orig_grid_element =
      multires_reshape_orig_grid_element_for_grid_coord(reshape_context, grid_coord);

  float D[3];
  mul_v3_m3v3(D, tangent_matrix, orig_grid_element.displacement);

  ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(reshape_context,
                                                                                 grid_coord);
  add_v3_v3v3(grid_element.displacement, P, D);
}

void multires_reshape_assign_final_coords_from_orig_mdisps(
    const MultiresReshapeContext *reshape_context)
{
  foreach_grid_coordinate(
      reshape_context, reshape_context->top.level, assign_final_coords_from_orig_mdisps, NULL);
}
