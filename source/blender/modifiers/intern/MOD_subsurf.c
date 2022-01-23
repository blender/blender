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
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subdiv_deform.h"
#include "BKE_subdiv_mesh.h"
#include "BKE_subdiv_modifier.h"
#include "BKE_subsurf.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RE_engine.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "BLO_read_write.h"

#include "intern/CCGSubSurf.h"

static void initData(ModifierData *md)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(SubsurfModifierData), modifier);
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;
  if (smd->flags & eSubsurfModifierFlag_UseCustomNormals) {
    r_cddata_masks->lmask |= CD_MASK_NORMAL;
    r_cddata_masks->lmask |= CD_MASK_CUSTOMLOOPNORMAL;
  }
  if (smd->flags & eSubsurfModifierFlag_UseCrease) {
    r_cddata_masks->vmask |= CD_MASK_CREASE;
  }
}

static bool dependsOnNormals(ModifierData *md)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;
  if (smd->flags & eSubsurfModifierFlag_UseCustomNormals) {
    return true;
  }
  return false;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
  const SubsurfModifierData *smd = (const SubsurfModifierData *)md;
#endif
  SubsurfModifierData *tsmd = (SubsurfModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  tsmd->emCache = tsmd->mCache = NULL;
}

static void freeRuntimeData(void *runtime_data_v)
{
  if (runtime_data_v == NULL) {
    return;
  }
  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)runtime_data_v;
  if (runtime_data->subdiv != NULL) {
    BKE_subdiv_free(runtime_data->subdiv);
  }
  MEM_freeN(runtime_data);
}

static void freeData(ModifierData *md)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;

  if (smd->mCache) {
    ccgSubSurf_free(smd->mCache);
    smd->mCache = NULL;
  }
  if (smd->emCache) {
    ccgSubSurf_free(smd->emCache);
    smd->emCache = NULL;
  }
  freeRuntimeData(smd->modifier.runtime);
}

static bool isDisabled(const Scene *scene, ModifierData *md, bool useRenderParams)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;
  int levels = (useRenderParams) ? smd->renderLevels : smd->levels;

  return get_render_subsurf_level(&scene->r, levels, useRenderParams != 0) == 0;
}

static int subdiv_levels_for_modifier_get(const SubsurfModifierData *smd,
                                          const ModifierEvalContext *ctx)
{
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER);
  const int requested_levels = (use_render_params) ? smd->renderLevels : smd->levels;
  return get_render_subsurf_level(&scene->r, requested_levels, use_render_params);
}

/* Subdivide into fully qualified mesh. */

static void subdiv_mesh_settings_init(SubdivToMeshSettings *settings,
                                      const SubsurfModifierData *smd,
                                      const ModifierEvalContext *ctx)
{
  const int level = subdiv_levels_for_modifier_get(smd, ctx);
  settings->resolution = (1 << level) + 1;
  settings->use_optimal_display = (smd->flags & eSubsurfModifierFlag_ControlEdges) &&
                                  !(ctx->flag & MOD_APPLY_TO_BASE_MESH);
}

static Mesh *subdiv_as_mesh(SubsurfModifierData *smd,
                            const ModifierEvalContext *ctx,
                            Mesh *mesh,
                            Subdiv *subdiv)
{
  Mesh *result = mesh;
  SubdivToMeshSettings mesh_settings;
  subdiv_mesh_settings_init(&mesh_settings, smd, ctx);
  if (mesh_settings.resolution < 3) {
    return result;
  }
  result = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh);
  return result;
}

/* Subdivide into CCG. */

static void subdiv_ccg_settings_init(SubdivToCCGSettings *settings,
                                     const SubsurfModifierData *smd,
                                     const ModifierEvalContext *ctx)
{
  const int level = subdiv_levels_for_modifier_get(smd, ctx);
  settings->resolution = (1 << level) + 1;
  settings->need_normal = true;
  settings->need_mask = false;
}

static Mesh *subdiv_as_ccg(SubsurfModifierData *smd,
                           const ModifierEvalContext *ctx,
                           Mesh *mesh,
                           Subdiv *subdiv)
{
  Mesh *result = mesh;
  SubdivToCCGSettings ccg_settings;
  subdiv_ccg_settings_init(&ccg_settings, smd, ctx);
  if (ccg_settings.resolution < 3) {
    return result;
  }
  result = BKE_subdiv_to_ccg_mesh(subdiv, &ccg_settings, mesh);
  return result;
}

/* Cache settings for lazy CPU evaluation. */

static void subdiv_cache_cpu_evaluation_settings(const ModifierEvalContext *ctx,
                                                 Mesh *me,
                                                 SubsurfModifierData *smd)
{
  SubdivToMeshSettings mesh_settings;
  subdiv_mesh_settings_init(&mesh_settings, smd, ctx);
  me->runtime.subsurf_apply_render = (ctx->flag & MOD_APPLY_RENDER) != 0;
  me->runtime.subsurf_resolution = mesh_settings.resolution;
  me->runtime.subsurf_use_optimal_display = mesh_settings.use_optimal_display;
}

/* Modifier itself. */

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result = mesh;
#if !defined(WITH_OPENSUBDIV)
  BKE_modifier_set_error(ctx->object, md, "Disabled, built without OpenSubdiv");
  return result;
#endif
  SubsurfModifierData *smd = (SubsurfModifierData *)md;
  SubdivSettings subdiv_settings;
  BKE_subsurf_modifier_subdiv_settings_init(
      &subdiv_settings, smd, (ctx->flag & MOD_APPLY_RENDER) != 0);
  if (subdiv_settings.level == 0) {
    return result;
  }
  SubsurfRuntimeData *runtime_data = BKE_subsurf_modifier_ensure_runtime(smd);

  /* Delay evaluation to the draw code if possible, provided we do not have to apply the modifier.
   */
  if ((ctx->flag & MOD_APPLY_TO_BASE_MESH) == 0) {
    Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
    const bool is_render_mode = (ctx->flag & MOD_APPLY_RENDER) != 0;
    /* Same check as in `DRW_mesh_batch_cache_create_requested` to keep both code coherent. */
    const bool is_editmode = (mesh->edit_mesh != NULL) &&
                             (mesh->edit_mesh->mesh_eval_final != NULL);
    const int required_mode = BKE_subsurf_modifier_eval_required_mode(is_render_mode, is_editmode);
    if (BKE_subsurf_modifier_can_do_gpu_subdiv_ex(scene, ctx->object, smd, required_mode, false)) {
      subdiv_cache_cpu_evaluation_settings(ctx, mesh, smd);
      return result;
    }
  }

  Subdiv *subdiv = BKE_subsurf_modifier_subdiv_descriptor_ensure(
      smd, &subdiv_settings, mesh, false);
  if (subdiv == NULL) {
    /* Happens on bad topology, but also on empty input mesh. */
    return result;
  }
  const bool use_clnors = (smd->flags & eSubsurfModifierFlag_UseCustomNormals) &&
                          (mesh->flag & ME_AUTOSMOOTH) &&
                          CustomData_has_layer(&mesh->ldata, CD_CUSTOMLOOPNORMAL);
  if (use_clnors) {
    /* If custom normals are present and the option is turned on calculate the split
     * normals and clear flag so the normals get interpolated to the result mesh. */
    BKE_mesh_calc_normals_split(mesh);
    CustomData_clear_layer_flag(&mesh->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
  }
  /* TODO(sergey): Decide whether we ever want to use CCG for subsurf,
   * maybe when it is a last modifier in the stack? */
  if (true) {
    result = subdiv_as_mesh(smd, ctx, mesh, subdiv);
  }
  else {
    result = subdiv_as_ccg(smd, ctx, mesh, subdiv);
  }

  if (use_clnors) {
    float(*lnors)[3] = CustomData_get_layer(&result->ldata, CD_NORMAL);
    BLI_assert(lnors != NULL);
    BKE_mesh_set_custom_normals(result, lnors);
    CustomData_set_layer_flag(&mesh->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
    CustomData_set_layer_flag(&result->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
  }
  // BKE_subdiv_stats_print(&subdiv->stats);
  if (subdiv != runtime_data->subdiv) {
    BKE_subdiv_free(subdiv);
  }
  return result;
}

static void deformMatrices(ModifierData *md,
                           const ModifierEvalContext *ctx,
                           Mesh *mesh,
                           float (*vertex_cos)[3],
                           float (*deform_matrices)[3][3],
                           int num_verts)
{
#if !defined(WITH_OPENSUBDIV)
  BKE_modifier_set_error(ctx->object, md, "Disabled, built without OpenSubdiv");
  return;
#endif

  /* Subsurf does not require extra space mapping, keep matrices as is. */
  (void)deform_matrices;

  SubsurfModifierData *smd = (SubsurfModifierData *)md;
  SubdivSettings subdiv_settings;
  BKE_subsurf_modifier_subdiv_settings_init(
      &subdiv_settings, smd, (ctx->flag & MOD_APPLY_RENDER) != 0);
  if (subdiv_settings.level == 0) {
    return;
  }
  SubsurfRuntimeData *runtime_data = BKE_subsurf_modifier_ensure_runtime(smd);
  Subdiv *subdiv = BKE_subsurf_modifier_subdiv_descriptor_ensure(
      smd, &subdiv_settings, mesh, false);
  if (subdiv == NULL) {
    /* Happens on bad topology, but also on empty input mesh. */
    return;
  }
  BKE_subdiv_deform_coarse_vertices(subdiv, mesh, vertex_cos, num_verts);
  if (subdiv != runtime_data->subdiv) {
    BKE_subdiv_free(subdiv);
  }
}

#ifdef WITH_CYCLES
static bool get_show_adaptive_options(const bContext *C, Panel *panel)
{
  /* Don't show adaptive options if cycles isn't the active engine. */
  const struct RenderEngineType *engine_type = CTX_data_engine_type(C);
  if (!STREQ(engine_type->idname, "CYCLES")) {
    return false;
  }

  /* Only show adaptive options if this is the last modifier. */
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, NULL);
  ModifierData *md = ptr->data;
  if (md->next != NULL) {
    return false;
  }

  /* Don't show adaptive options if regular subdivision used. */
  if (!RNA_boolean_get(ptr, "use_limit_surface")) {
    return false;
  }

  /* Don't show adaptive options if the cycles experimental feature set is disabled. */
  Scene *scene = CTX_data_scene(C);
  if (!BKE_scene_uses_cycles_experimental_features(scene)) {
    return false;
  }

  return true;
}
#endif

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  /* Only test for adaptive subdivision if built with cycles. */
  bool show_adaptive_options = false;
  bool ob_use_adaptive_subdivision = false;
  PointerRNA cycles_ptr = {NULL};
  PointerRNA ob_cycles_ptr = {NULL};
#ifdef WITH_CYCLES
  PointerRNA scene_ptr;
  Scene *scene = CTX_data_scene(C);
  RNA_id_pointer_create(&scene->id, &scene_ptr);
  if (BKE_scene_uses_cycles(scene)) {
    cycles_ptr = RNA_pointer_get(&scene_ptr, "cycles");
    ob_cycles_ptr = RNA_pointer_get(&ob_ptr, "cycles");
    if (!RNA_pointer_is_null(&ob_cycles_ptr)) {
      ob_use_adaptive_subdivision = RNA_boolean_get(&ob_cycles_ptr, "use_adaptive_subdivision");
      show_adaptive_options = get_show_adaptive_options(C, panel);
    }
  }
#else
  UNUSED_VARS(C);
#endif

  uiItemR(layout, ptr, "subdivision_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  if (show_adaptive_options) {
    uiItemR(layout,
            &ob_cycles_ptr,
            "use_adaptive_subdivision",
            0,
            IFACE_("Adaptive Subdivision"),
            ICON_NONE);
  }
  if (ob_use_adaptive_subdivision && show_adaptive_options) {
    uiItemR(layout, &ob_cycles_ptr, "dicing_rate", 0, NULL, ICON_NONE);
    float render = MAX2(RNA_float_get(&cycles_ptr, "dicing_rate") *
                            RNA_float_get(&ob_cycles_ptr, "dicing_rate"),
                        0.1f);
    float preview = MAX2(RNA_float_get(&cycles_ptr, "preview_dicing_rate") *
                             RNA_float_get(&ob_cycles_ptr, "dicing_rate"),
                         0.1f);
    char output[256];
    BLI_snprintf(output,
                 sizeof(output),
                 TIP_("Final Scale: Render %.2f px, Viewport %.2f px"),
                 render,
                 preview);
    uiItemL(layout, output, ICON_NONE);

    uiItemS(layout);

    uiItemR(layout, ptr, "levels", 0, IFACE_("Levels Viewport"), ICON_NONE);
  }
  else {
    uiLayout *col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "levels", 0, IFACE_("Levels Viewport"), ICON_NONE);
    uiItemR(col, ptr, "render_levels", 0, IFACE_("Render"), ICON_NONE);
  }

  uiItemR(layout, ptr, "show_only_control_edges", 0, NULL, ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void advanced_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  bool ob_use_adaptive_subdivision = false;
  bool show_adaptive_options = false;
#ifdef WITH_CYCLES
  Scene *scene = CTX_data_scene(C);
  if (BKE_scene_uses_cycles(scene)) {
    PointerRNA ob_cycles_ptr = RNA_pointer_get(&ob_ptr, "cycles");
    if (!RNA_pointer_is_null(&ob_cycles_ptr)) {
      ob_use_adaptive_subdivision = RNA_boolean_get(&ob_cycles_ptr, "use_adaptive_subdivision");
      show_adaptive_options = get_show_adaptive_options(C, panel);
    }
  }
#else
  UNUSED_VARS(C);
#endif

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, !(show_adaptive_options && ob_use_adaptive_subdivision));
  uiItemR(layout, ptr, "use_limit_surface", 0, NULL, ICON_NONE);

  uiLayout *col = uiLayoutColumn(layout, true);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_limit_surface"));
  uiItemR(col, ptr, "quality", 0, NULL, ICON_NONE);

  uiItemR(layout, ptr, "uv_smooth", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "boundary_smooth", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_creases", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_custom_normals", 0, NULL, ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Subsurf, panel_draw);
  modifier_subpanel_register(
      region_type, "advanced", "Advanced", NULL, advanced_panel_draw, panel_type);
}

static void blendRead(BlendDataReader *UNUSED(reader), ModifierData *md)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;

  smd->emCache = smd->mCache = NULL;
}

ModifierTypeInfo modifierType_Subsurf = {
    /* name */ "Subdivision",
    /* structName */ "SubsurfModifierData",
    /* structSize */ sizeof(SubsurfModifierData),
    /* srna */ &RNA_SubsurfModifier,
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode |
        eModifierTypeFlag_AcceptsCVs,
    /* icon */ ICON_MOD_SUBSURF,

    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ deformMatrices,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyGeometrySet */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ freeRuntimeData,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ blendRead,
};
