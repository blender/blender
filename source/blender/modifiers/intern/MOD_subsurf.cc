/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstddef>
#include <cstring>

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_types.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_scene.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subdiv_deform.hh"
#include "BKE_subdiv_mesh.hh"
#include "BKE_subdiv_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RE_engine.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

static void init_data(ModifierData *md)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(SubsurfModifierData), modifier);
}

static void free_runtime_data(void *runtime_data_v)
{
  if (runtime_data_v == nullptr) {
    return;
  }
  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)runtime_data_v;
  if (runtime_data->subdiv_cpu != nullptr) {
    blender::bke::subdiv::free(runtime_data->subdiv_cpu);
  }
  if (runtime_data->subdiv_gpu != nullptr) {
    blender::bke::subdiv::free(runtime_data->subdiv_gpu);
  }
  MEM_freeN(runtime_data);
}

static void free_data(ModifierData *md)
{
  SubsurfModifierData *smd = (SubsurfModifierData *)md;

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

static void subdiv_mesh_settings_init(blender::bke::subdiv::ToMeshSettings *settings,
                                      const SubsurfModifierData *smd,
                                      const ModifierEvalContext *ctx)
{
  const int level = subdiv_levels_for_modifier_get(smd, ctx);
  settings->resolution = (1 << level) + 1;
  settings->use_optimal_display = (smd->flags & eSubsurfModifierFlag_ControlEdges) &&
                                  !(ctx->flag & MOD_APPLY_TO_ORIGINAL);
}

static Mesh *subdiv_as_mesh(SubsurfModifierData *smd,
                            const ModifierEvalContext *ctx,
                            Mesh *mesh,
                            blender::bke::subdiv::Subdiv *subdiv)
{
  Mesh *result = mesh;
  blender::bke::subdiv::ToMeshSettings mesh_settings;
  subdiv_mesh_settings_init(&mesh_settings, smd, ctx);
  if (mesh_settings.resolution < 3) {
    return result;
  }
  result = blender::bke::subdiv::subdiv_to_mesh(subdiv, &mesh_settings, mesh);
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
                           blender::bke::subdiv::Subdiv *subdiv)
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
                                               SubsurfRuntimeData *runtime_data,
                                               const bool has_gpu_subdiv)
{
  blender::bke::subdiv::ToMeshSettings mesh_settings;
  subdiv_mesh_settings_init(&mesh_settings, smd, ctx);

  runtime_data->has_gpu_subdiv = has_gpu_subdiv;
  runtime_data->resolution = mesh_settings.resolution;
  runtime_data->use_optimal_display = mesh_settings.use_optimal_display;
  runtime_data->use_loop_normals = (smd->flags & eSubsurfModifierFlag_UseCustomNormals);

  mesh->runtime->subsurf_runtime_data = runtime_data;
}

static ModifierData *modifier_get_last_enabled_for_mode(const Scene *scene,
                                                        const Object *ob,
                                                        int required_mode)
{
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.last);

  while (md) {
    if (BKE_modifier_is_enabled(scene, md, required_mode)) {
      break;
    }

    md = md->prev;
  }

  return md;
}

/* Modifier itself. */

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  using namespace blender;
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
  if ((ctx->flag & MOD_APPLY_TO_ORIGINAL) == 0) {
    Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

    /* Same check as in `DRW_mesh_batch_cache_create_requested` to keep both code coherent. The
     * difference is that here we do not check for the final edit mesh pointer as it is not yet
     * assigned at this stage of modifier stack evaluation. */
    const bool is_render_mode = (ctx->flag & MOD_APPLY_RENDER) != 0;
    const bool is_editmode = (mesh->runtime->edit_mesh != nullptr);
    const int required_mode = BKE_subsurf_modifier_eval_required_mode(is_render_mode, is_editmode);

    /* Check if we are the last modifier in the stack. */
    ModifierData *md = modifier_get_last_enabled_for_mode(scene, ctx->object, required_mode);
    if (md == (const ModifierData *)smd) {
      const bool has_gpu_subdiv = BKE_subsurf_modifier_can_do_gpu_subdiv(smd, mesh);
      subdiv_cache_mesh_wrapper_settings(ctx, mesh, smd, runtime_data, has_gpu_subdiv);

      /* Delay for:
       * - Background mode: Not sure if we are going to use the tessellated mesh.
       * - Render: Engine might do its own subdivision and not need this.
       * - GPU subdivision support: Might only need to display and not access tessellated mesh.
       *
       * If we can't delay, we still create the wrapper so external renderers can get the base
       * mesh. But we tessellate immediately to take advantage of better parallellization
       * as part of multithreaded depsgraph evaluation. */
      const bool delay = G.background || is_render_mode || has_gpu_subdiv;
      if (!delay) {
        BKE_mesh_wrapper_ensure_subdivision(mesh);
      }

      return result;
    }
  }

  blender::bke::subdiv::Subdiv *subdiv = BKE_subsurf_modifier_subdiv_descriptor_ensure(
      runtime_data, mesh, false);
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
    bke::mesh_set_custom_normals_normalized(
        *result,
        {static_cast<float3 *>(
             CustomData_get_layer_for_write(&result->corner_data, CD_NORMAL, result->corners_num)),
         result->corners_num});
    CustomData_free_layers(&result->corner_data, CD_NORMAL);
  }
  // blender::bke::subdiv::stats_print(&subdiv->stats);
  if (!ELEM(subdiv, runtime_data->subdiv_cpu, runtime_data->subdiv_gpu)) {
    blender::bke::subdiv::free(subdiv);
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
  blender::bke::subdiv::Subdiv *subdiv = BKE_subsurf_modifier_subdiv_descriptor_ensure(
      runtime_data, mesh, false);
  if (subdiv == nullptr) {
    /* Happens on bad topology, but also on empty input mesh. */
    return;
  }
  blender::bke::subdiv::deform_coarse_vertices(subdiv, mesh, positions);
  if (!ELEM(subdiv, runtime_data->subdiv_cpu, runtime_data->subdiv_gpu)) {
    blender::bke::subdiv::free(subdiv);
  }
}

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

  return true;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->prop(ptr, "subdivision_type", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  layout->use_property_split_set(true);

  uiLayout *col = &layout->column(true);
  col->prop(ptr, "levels", UI_ITEM_NONE, IFACE_("Levels Viewport"), ICON_NONE);
  col->prop(ptr, "render_levels", UI_ITEM_NONE, IFACE_("Render"), ICON_NONE);

  layout->prop(ptr, "show_only_control_edges", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  SubsurfModifierData *smd = static_cast<SubsurfModifierData *>(ptr->data);
  Object *ob = static_cast<Object *>(ob_ptr.data);
  if (ob->type == OB_MESH && BKE_subsurf_modifier_force_disable_gpu_evaluation_for_mesh(
                                 smd, static_cast<const Mesh *>(ob->data)))
  {
    layout->label(RPT_("Sharp edges or custom normals detected, disabling GPU subdivision"),
                  ICON_INFO);
  }
  else if (Object *ob_eval = DEG_get_evaluated(depsgraph, ob)) {
    if (ModifierData *md_eval = BKE_modifiers_findby_name(ob_eval, smd->modifier.name)) {
      if (md_eval->type == eModifierType_Subsurf) {
        SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)md_eval->runtime;

        if (runtime_data && runtime_data->used_gpu) {
          if (runtime_data->used_cpu) {
            layout->label(RPT_("Using both CPU and GPU subdivision"), ICON_INFO);
          }
        }
      }
    }
  }

  if (get_show_adaptive_options(C, panel)) {
    PanelLayout adaptive_panel = layout->panel_prop_with_bool_header(
        C,
        ptr,
        "open_adaptive_subdivision_panel",
        ptr,
        "use_adaptive_subdivision",
        IFACE_("Adaptive Subdivision"));
    if (adaptive_panel.body) {
      Scene *scene = CTX_data_scene(C);
      PointerRNA scene_ptr = RNA_id_pointer_create(&scene->id);
      PointerRNA cycles_ptr = RNA_pointer_get(&scene_ptr, "cycles");
      const float render_rate = RNA_float_get(&cycles_ptr, "dicing_rate");
      const float preview_rate = RNA_float_get(&cycles_ptr, "preview_dicing_rate");
      std::string render_str, preview_str;

      adaptive_panel.body->active_set(smd->flags & eSubsurfModifierFlag_UseAdaptiveSubdivision);
      adaptive_panel.body->prop(ptr, "adaptive_space", UI_ITEM_NONE, IFACE_("Space"), ICON_NONE);
      if (smd->adaptive_space == SUBSURF_ADAPTIVE_SPACE_OBJECT) {
        adaptive_panel.body->prop(
            ptr, "adaptive_object_edge_length", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        preview_str = fmt::format("{:.5g}", preview_rate * smd->adaptive_object_edge_length);
        render_str = fmt::format("{:.5g}", render_rate * smd->adaptive_object_edge_length);
      }
      else {
        adaptive_panel.body->prop(
            ptr, "adaptive_pixel_size", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        preview_str = fmt::format("{:.2f} px",
                                  std::max(preview_rate * smd->adaptive_pixel_size, 0.1f));
        render_str = fmt::format("{:.2f} px",
                                 std::max(render_rate * smd->adaptive_pixel_size, 0.1f));
      }

      uiLayout *split = &adaptive_panel.body->split(0.4f, false);
      uiLayout *col = &split->column(true);
      col->alignment_set(blender::ui::LayoutAlign::Right);
      col->label(IFACE_("Viewport"), ICON_NONE);
      col->label(IFACE_("Render"), ICON_NONE);
      col = &split->column(true);
      col->label(preview_str, ICON_NONE);
      col->label(render_str, ICON_NONE);
    }
  }

  if (uiLayout *advanced_layout = layout->panel_prop(
          C, ptr, "open_advanced_panel", IFACE_("Advanced")))
  {
    advanced_layout->use_property_split_set(true);

    advanced_layout->prop(ptr, "use_limit_surface", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    uiLayout *col = &advanced_layout->column(true);
    col->active_set((smd->flags & eSubsurfModifierFlag_UseAdaptiveSubdivision) ||
                    RNA_boolean_get(ptr, "use_limit_surface"));
    col->prop(ptr, "quality", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    advanced_layout->prop(ptr, "uv_smooth", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    advanced_layout->prop(ptr, "boundary_smooth", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    advanced_layout->prop(ptr, "use_creases", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    advanced_layout->prop(ptr, "use_custom_normals", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Subsurf, panel_draw);
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

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ deform_matrices,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ nullptr,
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
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
