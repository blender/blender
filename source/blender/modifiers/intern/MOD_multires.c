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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_modifier.h"
#include "BKE_paint.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subdiv_mesh.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"

typedef struct MultiresRuntimeData {
  /* Cached subdivision surface descriptor, with topology and settings. */
  struct Subdiv *subdiv;
} MultiresRuntimeData;

static void initData(ModifierData *md)
{
  MultiresModifierData *mmd = (MultiresModifierData *)md;

  mmd->lvl = 0;
  mmd->sculptlvl = 0;
  mmd->renderlvl = 0;
  mmd->totlvl = 0;
  mmd->uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_CORNERS;
  mmd->quality = 3;
  mmd->flags |= eMultiresModifierFlag_UseCrease;
}

static void copyData(const ModifierData *md_src, ModifierData *md_dst, const int flag)
{
  modifier_copyData_generic(md_src, md_dst, flag);
}

static void freeRuntimeData(void *runtime_data_v)
{
  if (runtime_data_v == NULL) {
    return;
  }
  MultiresRuntimeData *runtime_data = (MultiresRuntimeData *)runtime_data_v;
  if (runtime_data->subdiv != NULL) {
    BKE_subdiv_free(runtime_data->subdiv);
  }
  MEM_freeN(runtime_data);
}

static void freeData(ModifierData *md)
{
  MultiresModifierData *mmd = (MultiresModifierData *)md;
  freeRuntimeData(mmd->modifier.runtime);
}

static MultiresRuntimeData *multires_ensure_runtime(MultiresModifierData *mmd)
{
  MultiresRuntimeData *runtime_data = (MultiresRuntimeData *)mmd->modifier.runtime;
  if (runtime_data == NULL) {
    runtime_data = MEM_callocN(sizeof(*runtime_data), "subsurf runtime");
    mmd->modifier.runtime = runtime_data;
  }
  return runtime_data;
}

/* Main goal of this function is to give usable subdivision surface descriptor
 * which matches settings and topology. */
static Subdiv *subdiv_descriptor_ensure(MultiresModifierData *mmd,
                                        const SubdivSettings *subdiv_settings,
                                        const Mesh *mesh)
{
  MultiresRuntimeData *runtime_data = (MultiresRuntimeData *)mmd->modifier.runtime;
  Subdiv *subdiv = BKE_subdiv_update_from_mesh(runtime_data->subdiv, subdiv_settings, mesh);
  runtime_data->subdiv = subdiv;
  return subdiv;
}

/* Subdivide into fully qualified mesh. */

static Mesh *multires_as_mesh(MultiresModifierData *mmd,
                              const ModifierEvalContext *ctx,
                              Mesh *mesh,
                              Subdiv *subdiv)
{
  Mesh *result = mesh;
  const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER);
  const bool ignore_simplify = (ctx->flag & MOD_APPLY_IGNORE_SIMPLIFY);
  const Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Object *object = ctx->object;
  SubdivToMeshSettings mesh_settings;
  BKE_multires_subdiv_mesh_settings_init(
      &mesh_settings, scene, object, mmd, use_render_params, ignore_simplify);
  if (mesh_settings.resolution < 3) {
    return result;
  }
  BKE_subdiv_displacement_attach_from_multires(subdiv, mesh, mmd);
  result = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh);
  return result;
}

/* Subdivide into CCG. */

static void multires_ccg_settings_init(SubdivToCCGSettings *settings,
                                       const MultiresModifierData *mmd,
                                       const ModifierEvalContext *ctx,
                                       Mesh *mesh)
{
  const bool has_mask = CustomData_has_layer(&mesh->ldata, CD_GRID_PAINT_MASK);
  const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER);
  const bool ignore_simplify = (ctx->flag & MOD_APPLY_IGNORE_SIMPLIFY);
  const Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Object *object = ctx->object;
  const int level = multires_get_level(scene, object, mmd, use_render_params, ignore_simplify);
  settings->resolution = (1 << level) + 1;
  settings->need_normal = true;
  settings->need_mask = has_mask;
}

static Mesh *multires_as_ccg(MultiresModifierData *mmd,
                             const ModifierEvalContext *ctx,
                             Mesh *mesh,
                             Subdiv *subdiv)
{
  Mesh *result = mesh;
  SubdivToCCGSettings ccg_settings;
  multires_ccg_settings_init(&ccg_settings, mmd, ctx, mesh);
  if (ccg_settings.resolution < 3) {
    return result;
  }
  BKE_subdiv_displacement_attach_from_multires(subdiv, mesh, mmd);
  result = BKE_subdiv_to_ccg_mesh(subdiv, &ccg_settings, mesh);
  return result;
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result = mesh;
  MultiresModifierData *mmd = (MultiresModifierData *)md;
  SubdivSettings subdiv_settings;
  BKE_multires_subdiv_settings_init(&subdiv_settings, mmd);
  if (subdiv_settings.level == 0) {
    return result;
  }
  BKE_subdiv_settings_validate_for_mesh(&subdiv_settings, mesh);
  MultiresRuntimeData *runtime_data = multires_ensure_runtime(mmd);
  Subdiv *subdiv = subdiv_descriptor_ensure(mmd, &subdiv_settings, mesh);
  if (subdiv == NULL) {
    /* Happens on bad topology, ut also on empty input mesh. */
    return result;
  }
  /* NOTE: Orco needs final coordinates on CPU side, which are expected to be
   * accessible via MVert. For this reason we do not evaluate multires to
   * grids when orco is requested. */
  const bool for_orco = (ctx->flag & MOD_APPLY_ORCO) != 0;
  if ((ctx->object->mode & OB_MODE_SCULPT) && !for_orco) {
    /* NOTE: CCG takes ownership over Subdiv. */
    result = multires_as_ccg(mmd, ctx, mesh, subdiv);
    result->runtime.subdiv_ccg_tot_level = mmd->totlvl;
    /* TODO(sergey): Usually it is sculpt stroke's update variants which
     * takes care of this, but is possible that we need this before the
     * stroke: i.e. when exiting blender right after stroke is done.
     * Annoying and not so much black-boxed as far as sculpting goes, and
     * surely there is a better way of solving this. */
    if (ctx->object->sculpt != NULL) {
      SculptSession *sculpt_session = ctx->object->sculpt;
      sculpt_session->subdiv_ccg = result->runtime.subdiv_ccg;
      sculpt_session->multires = mmd;
      sculpt_session->totvert = mesh->totvert;
      sculpt_session->totpoly = mesh->totpoly;
      sculpt_session->mvert = NULL;
      sculpt_session->mpoly = NULL;
      sculpt_session->mloop = NULL;
    }
    /* NOTE: CCG becomes an owner of Subdiv descriptor, so can not share
     * this pointer. Not sure if it's needed, but might have a second look
     * on the ownership model here. */
    runtime_data->subdiv = NULL;
    // BKE_subdiv_stats_print(&subdiv->stats);
  }
  else {
    result = multires_as_mesh(mmd, ctx, mesh, subdiv);
    // BKE_subdiv_stats_print(&subdiv->stats);
    if (subdiv != runtime_data->subdiv) {
      BKE_subdiv_free(subdiv);
    }
  }
  return result;
}

ModifierTypeInfo modifierType_Multires = {
    /* name */ "Multires",
    /* structName */ "MultiresModifierData",
    /* structSize */ sizeof(MultiresModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_RequiresOriginalData,

    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ NULL,
    /* freeData */ freeData,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ freeRuntimeData,
};
