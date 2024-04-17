/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_types.hh"
#include "BKE_scene.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subdiv_deform.hh"
#include "BKE_subdiv_mesh.hh"
#include "BKE_subdiv_modifier.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RE_engine.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "intern/CCGSubSurf.h"

static void init_data(ModifierData *md)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(SubsurfModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;
  if (smd->flags & eSubsurfModifierFlag_UseCustomNormals) {
    r_cddata_masks->lmask |= CD_MASK_CUSTOMLOOPNORMAL;
  }
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
  const SubsurfModifierData *smd = (const SubsurfModifierData *)md;
#endif
  SubsurfModifierData *tsmd = (SubsurfModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  tsmd->emCache = tsmd->mCache = nullptr;
}

static void free_runtime_data(void *runtime_data_v)
{
  if (runtime_data_v == nullptr) {
    return;
  }
  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)runtime_data_v;
  if (runtime_data->subdiv_cpu != nullptr) {
    BKE_subdiv_free(runtime_data->subdiv_cpu);
  }
  if (runtime_data->subdiv_gpu != nullptr) {
    BKE_subdiv_free(runtime_data->subdiv_gpu);
  }
  MEM_freeN(runtime_data);
}

static void free_data(ModifierData *md)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;

  if (smd->mCache) {
    ccgSubSurf_free(static_cast<CCGSubSurf *>(smd->mCache));
    smd->mCache = nullptr;
  }
  if (smd->emCache) {
    ccgSubSurf_free(static_cast<CCGSubSurf *>(smd->emCache));
    smd->emCache = nullptr;
  }
  free_runtime_data(smd->modifier.runtime);
}

static bool is_disabled(const Scene *scene, ModifierData *md, bool use_render_params)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;
  int levels = (use_render_params) ? smd->renderLevels : smd->levels;

  return get_render_subsurf_level(&scene->r, levels, use_render_params != 0) == 0;
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
  result = BKE_subdiv_to_ccg_mesh(*subdiv, ccg_settings, *mesh);
  return result;
}

/* Cache settings for lazy CPU evaluation. */

static void subdiv_cache_mesh_wrapper_settings(const ModifierEvalContext *ctx,
                                               Mesh *mesh,
                                               SubsurfModifierData *smd,
                                               SubsurfRuntimeData *runtime_data)
{
  SubdivToMeshSettings mesh_settings;
  subdiv_mesh_settings_init(&mesh_settings, smd, ctx);

  runtime_data->has_gpu_subdiv = true;
  runtime_data->resolution = mesh_settings.resolution;
  runtime_data->use_optimal_display = mesh_settings.use_optimal_display;
  runtime_data->use_loop_normals = (smd->flags & eSubsurfModifierFlag_UseCustomNormals);

  mesh->runtime->subsurf_runtime_data = runtime_data;
}

/* Modifier itself. */

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result = mesh;
#if !defined(WITH_OPENSUBDIV)
  BKE_modifier_set_error(ctx->object, md, "Disabled, built without OpenSubdiv");
  return result;
#endif
  SubsurfModifierData *smd = (SubsurfModifierData *)md;
  if (!BKE_subsurf_modifier_runtime_init(smd, (ctx->flag & MOD_APPLY_RENDER) != 0)) {
    return result;
  }

  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)smd->modifier.runtime;

  /* Decrement the recent usage counters. */
  if (runtime_data->used_cpu) {
    runtime_data->used_cpu--;
  }

  if (runtime_data->used_gpu) {
    runtime_data->used_gpu--;
  }

  /* Delay evaluation to the draw code if possible, provided we do not have to apply the modifier.
   */
  if ((ctx->flag & MOD_APPLY_TO_BASE_MESH) == 0) {
    Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
    const bool is_render_mode = (ctx->flag & MOD_APPLY_RENDER) != 0;
    /* Same check as in `DRW_mesh_batch_cache_create_requested` to keep both code coherent. The
     * difference is that here we do not check for the final edit mesh pointer as it is not yet
     * assigned at this stage of modifier stack evaluation. */
    const bool is_editmode = (mesh->runtime->edit_mesh != nullptr);
    const int required_mode = BKE_subsurf_modifier_eval_required_mode(is_render_mode, is_editmode);
    if (BKE_subsurf_modifier_can_do_gpu_subdiv(scene, ctx->object, mesh, smd, required_mode)) {
      subdiv_cache_mesh_wrapper_settings(ctx, mesh, smd, runtime_data);
      return result;
    }
  }

  Subdiv *subdiv = BKE_subsurf_modifier_subdiv_descriptor_ensure(runtime_data, mesh, false);
  if (subdiv == nullptr) {
    /* Happens on bad topology, but also on empty input mesh. */
    return result;
  }
  const bool use_clnors = BKE_subsurf_modifier_use_custom_loop_normals(smd, mesh);
  if (use_clnors) {
    void *data = CustomData_add_layer(
        &mesh->corner_data, CD_NORMAL, CD_CONSTRUCT, mesh->corners_num);
    memcpy(data, mesh->corner_normals().data(), mesh->corner_normals().size_in_bytes());
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
    BKE_mesh_set_custom_normals(result,
                                static_cast<float(*)[3]>(CustomData_get_layer_for_write(
                                    &result->corner_data, CD_NORMAL, result->corners_num)));
    CustomData_free_layers(&result->corner_data, CD_NORMAL, result->corners_num);
  }
  // BKE_subdiv_stats_print(&subdiv->stats);
  if (!ELEM(subdiv, runtime_data->subdiv_cpu, runtime_data->subdiv_gpu)) {
    BKE_subdiv_free(subdiv);
  }
  return result;
}

static void deform_matrices(ModifierData *md,
                            const ModifierEvalContext *ctx,
                            Mesh *mesh,
                            blender::MutableSpan<blender::float3> positions,
                            blender::MutableSpan<blender::float3x3> /*matrices*/)
{
#if !defined(WITH_OPENSUBDIV)
  BKE_modifier_set_error(ctx->object, md, "Disabled, built without OpenSubdiv");
  return;
#endif

  /* Subsurf does not require extra space mapping, keep matrices as is. */

  SubsurfModifierData *smd = (SubsurfModifierData *)md;
  if (!BKE_subsurf_modifier_runtime_init(smd, (ctx->flag & MOD_APPLY_RENDER) != 0)) {
    return;
  }
  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)smd->modifier.runtime;
  Subdiv *subdiv = BKE_subsurf_modifier_subdiv_descriptor_ensure(runtime_data, mesh, false);
  if (subdiv == nullptr) {
    /* Happens on bad topology, but also on empty input mesh. */
    return;
  }
  BKE_subdiv_deform_coarse_vertices(
      subdiv, mesh, reinterpret_cast<float(*)[3]>(positions.data()), positions.size());
  if (!ELEM(subdiv, runtime_data->subdiv_cpu, runtime_data->subdiv_gpu)) {
    BKE_subdiv_free(subdiv);
  }
}

#ifdef WITH_CYCLES
static bool get_show_adaptive_options(const bContext *C, Panel *panel)
{
  /* Don't show adaptive options if cycles isn't the active engine. */
  const RenderEngineType *engine_type = CTX_data_engine_type(C);
  if (!STREQ(engine_type->idname, "CYCLES")) {
    return false;
  }

  /* Only show adaptive options if this is the last modifier. */
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  ModifierData *md = static_cast<ModifierData *>(ptr->data);
  if (md->next != nullptr) {
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
  PointerRNA cycles_ptr = {nullptr};
  PointerRNA ob_cycles_ptr = {nullptr};
#ifdef WITH_CYCLES
  Scene *scene = CTX_data_scene(C);
  PointerRNA scene_ptr = RNA_id_pointer_create(&scene->id);
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

  uiItemR(layout, ptr, "subdivision_type", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  if (show_adaptive_options) {
    uiItemR(layout,
            &ob_cycles_ptr,
            "use_adaptive_subdivision",
            UI_ITEM_NONE,
            IFACE_("Adaptive Subdivision"),
            ICON_NONE);
  }
  if (ob_use_adaptive_subdivision && show_adaptive_options) {
    uiItemR(layout, &ob_cycles_ptr, "dicing_rate", UI_ITEM_NONE, nullptr, ICON_NONE);
    float render = std::max(RNA_float_get(&cycles_ptr, "dicing_rate") *
                                RNA_float_get(&ob_cycles_ptr, "dicing_rate"),
                            0.1f);
    float preview = std::max(RNA_float_get(&cycles_ptr, "preview_dicing_rate") *
                                 RNA_float_get(&ob_cycles_ptr, "dicing_rate"),
                             0.1f);
    char output[256];
    SNPRINTF(output, RPT_("Final Scale: Render %.2f px, Viewport %.2f px"), render, preview);
    uiItemL(layout, output, ICON_NONE);

    uiItemS(layout);

    uiItemR(layout, ptr, "levels", UI_ITEM_NONE, IFACE_("Levels Viewport"), ICON_NONE);
  }
  else {
    uiLayout *col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "levels", UI_ITEM_NONE, IFACE_("Levels Viewport"), ICON_NONE);
    uiItemR(col, ptr, "render_levels", UI_ITEM_NONE, IFACE_("Render"), ICON_NONE);
  }

  uiItemR(layout, ptr, "show_only_control_edges", UI_ITEM_NONE, nullptr, ICON_NONE);

  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  SubsurfModifierData *smd = static_cast<SubsurfModifierData *>(ptr->data);
  Object *ob = static_cast<Object *>(ob_ptr.data);
  const Mesh *mesh = static_cast<const Mesh *>(ob->data);
  if (BKE_subsurf_modifier_force_disable_gpu_evaluation_for_mesh(smd, mesh)) {
    uiItemL(
        layout, "Sharp edges or custom normals detected, disabling GPU subdivision", ICON_INFO);
  }
  else if (Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob)) {
    if (ModifierData *md_eval = BKE_modifiers_findby_name(ob_eval, smd->modifier.name)) {
      if (md_eval->type == eModifierType_Subsurf) {
        SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)md_eval->runtime;

        if (runtime_data && runtime_data->used_gpu) {
          if (runtime_data->used_cpu) {
            uiItemL(layout, "Using both CPU and GPU subdivision", ICON_INFO);
          }
        }
      }
    }
  }

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
  uiItemR(layout, ptr, "use_limit_surface", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiLayout *col = uiLayoutColumn(layout, true);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_limit_surface"));
  uiItemR(col, ptr, "quality", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "uv_smooth", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "boundary_smooth", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_creases", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_custom_normals", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Subsurf, panel_draw);
  modifier_subpanel_register(
      region_type, "advanced", "Advanced", nullptr, advanced_panel_draw, panel_type);
}

static void blend_read(BlendDataReader * /*reader*/, ModifierData *md)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;

  smd->emCache = smd->mCache = nullptr;
}

ModifierTypeInfo modifierType_Subsurf = {
    /*idname*/ "Subdivision",
    /*name*/ N_("Subdivision"),
    /*struct_name*/ "SubsurfModifierData",
    /*struct_size*/ sizeof(SubsurfModifierData),
    /*srna*/ &RNA_SubsurfModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode |
        eModifierTypeFlag_AcceptsCVs,
    /*icon*/ ICON_MOD_SUBSURF,

    /*copy_data*/ copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ deform_matrices,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ free_data,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ free_runtime_data,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ blend_read,
    /*foreach_cache*/ nullptr,
};
