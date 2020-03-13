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

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_vector.h"

#include "BKE_customdata.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_subdiv.h"

#include "DEG_depsgraph_query.h"

#include "multires_reshape.h"

/* ================================================================================================
 * Reshape from object.
 */

bool multiresModifier_reshapeFromVertcos(struct Depsgraph *depsgraph,
                                         struct Object *object,
                                         struct MultiresModifierData *mmd,
                                         const float (*vert_coords)[3],
                                         const int num_vert_coords)
{
  MultiresReshapeContext reshape_context;
  if (!multires_reshape_context_create_from_object(&reshape_context, depsgraph, object, mmd)) {
    return false;
  }
  multires_reshape_store_original_grids(&reshape_context);
  multires_reshape_ensure_grids(object->data, reshape_context.top.level);
  if (!multires_reshape_assign_final_coords_from_vertcos(
          &reshape_context, vert_coords, num_vert_coords)) {
    multires_reshape_context_free(&reshape_context);
    return false;
  }
  multires_reshape_smooth_object_grids_with_details(&reshape_context);
  multires_reshape_object_grids_to_tangent_displacement(&reshape_context);
  multires_reshape_context_free(&reshape_context);
  return true;
}

/* Returns truth on success, false otherwise.
 *
 * This function might fail in cases like source and destination not having
 * matched amount of vertices. */
bool multiresModifier_reshapeFromObject(struct Depsgraph *depsgraph,
                                        struct MultiresModifierData *mmd,
                                        struct Object *dst,
                                        struct Object *src)
{
  struct Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  struct Object *src_eval = DEG_get_evaluated_object(depsgraph, src);
  Mesh *src_mesh_eval = mesh_get_eval_final(depsgraph, scene_eval, src_eval, &CD_MASK_BAREMESH);

  int num_deformed_verts;
  float(*deformed_verts)[3] = BKE_mesh_vert_coords_alloc(src_mesh_eval, &num_deformed_verts);

  const bool result = multiresModifier_reshapeFromVertcos(
      depsgraph, dst, mmd, deformed_verts, num_deformed_verts);

  MEM_freeN(deformed_verts);

  return result;
}

/* ================================================================================================
 * Reshape from modifier.
 */

bool multiresModifier_reshapeFromDeformModifier(struct Depsgraph *depsgraph,
                                                struct Object *object,
                                                struct MultiresModifierData *mmd,
                                                struct ModifierData *deform_md)
{
  MultiresModifierData highest_mmd = *mmd;
  highest_mmd.sculptlvl = highest_mmd.totlvl;
  highest_mmd.lvl = highest_mmd.totlvl;
  highest_mmd.renderlvl = highest_mmd.totlvl;

  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);

  /* Create mesh for the multires, ignoring any further modifiers (leading
   * deformation modifiers will be applied though). */
  Mesh *multires_mesh = BKE_multires_create_mesh(depsgraph, scene_eval, &highest_mmd, object);
  int num_deformed_verts;
  float(*deformed_verts)[3] = BKE_mesh_vert_coords_alloc(multires_mesh, &num_deformed_verts);

  /* Apply deformation modifier on the multires, */
  const ModifierEvalContext modifier_ctx = {
      .depsgraph = depsgraph,
      .object = object,
      .flag = MOD_APPLY_USECACHE | MOD_APPLY_IGNORE_SIMPLIFY,
  };
  modwrap_deformVerts(
      deform_md, &modifier_ctx, multires_mesh, deformed_verts, multires_mesh->totvert);
  BKE_id_free(NULL, multires_mesh);

  /* Reshaping */
  bool result = multiresModifier_reshapeFromVertcos(
      depsgraph, object, &highest_mmd, deformed_verts, num_deformed_verts);

  /* Cleanup */
  MEM_freeN(deformed_verts);

  return result;
}

/* ================================================================================================
 * Reshape from grids.
 */

bool multiresModifier_reshapeFromCCG(const int tot_level,
                                     Mesh *coarse_mesh,
                                     struct SubdivCCG *subdiv_ccg)
{
  MultiresReshapeContext reshape_context;
  if (!multires_reshape_context_create_from_ccg(
          &reshape_context, subdiv_ccg, coarse_mesh, tot_level)) {
    return false;
  }

  multires_ensure_external_read(coarse_mesh, reshape_context.top.level);

  multires_reshape_store_original_grids(&reshape_context);
  multires_reshape_ensure_grids(coarse_mesh, reshape_context.top.level);
  if (!multires_reshape_assign_final_coords_from_ccg(&reshape_context, subdiv_ccg)) {
    multires_reshape_context_free(&reshape_context);
    return false;
  }
  multires_reshape_smooth_object_grids_with_details(&reshape_context);
  multires_reshape_object_grids_to_tangent_displacement(&reshape_context);
  multires_reshape_context_free(&reshape_context);
  return true;
}

/* ================================================================================================
 * Subdivision.
 */

void multiresModifier_subdivide(Object *object, MultiresModifierData *mmd)
{
  const int top_level = mmd->totlvl + 1;
  multiresModifier_subdivide_to_level(object, mmd, top_level);
}

void multiresModifier_subdivide_to_level(struct Object *object,
                                         struct MultiresModifierData *mmd,
                                         const int top_level)
{
  if (top_level <= mmd->totlvl) {
    return;
  }

  Mesh *coarse_mesh = object->data;
  MultiresReshapeContext reshape_context;

  /* There was no multires at all, all displacement is at 0. Can simply make sure all mdisps grids
   * are allocated at a proper level and return. */
  const bool has_mdisps = CustomData_has_layer(&coarse_mesh->ldata, CD_MDISPS);
  if (!has_mdisps) {
    CustomData_add_layer(&coarse_mesh->ldata, CD_MDISPS, CD_CALLOC, NULL, coarse_mesh->totloop);
  }
  if (!has_mdisps || top_level == 1) {
    multires_reshape_ensure_grids(coarse_mesh, top_level);
    multires_set_tot_level(object, mmd, top_level);
    return;
  }

  multires_flush_sculpt_updates(object);

  if (!multires_reshape_context_create_from_subdivide(&reshape_context, object, mmd, top_level)) {
    return;
  }
  multires_reshape_store_original_grids(&reshape_context);
  multires_reshape_ensure_grids(coarse_mesh, reshape_context.top.level);
  multires_reshape_assign_final_coords_from_orig_mdisps(&reshape_context);
  multires_reshape_smooth_object_grids(&reshape_context);
  multires_reshape_object_grids_to_tangent_displacement(&reshape_context);
  multires_reshape_context_free(&reshape_context);

  multires_set_tot_level(object, mmd, top_level);
}

/* ================================================================================================
 * Apply base.
 */

void multiresModifier_base_apply(struct Depsgraph *depsgraph,
                                 Object *object,
                                 MultiresModifierData *mmd)
{
  multires_force_sculpt_rebuild(object);

  MultiresReshapeContext reshape_context;
  if (!multires_reshape_context_create_from_object(&reshape_context, depsgraph, object, mmd)) {
    return;
  }

  multires_reshape_assign_final_coords_from_mdisps(&reshape_context);
  multires_reshape_apply_base_update_mesh_coords(&reshape_context);
  multires_reshape_apply_base_refit_base_mesh(&reshape_context);
  multires_reshape_apply_base_refine_subdiv(&reshape_context);
  multires_reshape_object_grids_to_tangent_displacement(&reshape_context);

  multires_reshape_context_free(&reshape_context);
}
