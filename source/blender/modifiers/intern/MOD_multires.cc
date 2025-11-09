/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstddef>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_paint.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subdiv_deform.hh"
#include "BKE_subdiv_mesh.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_types.hh" /* For subdivide operator UI. */

#include "DEG_depsgraph_query.hh"

#include "MOD_ui_common.hh"

struct MultiresRuntimeData {
  /* Cached subdivision surface descriptor, with topology and settings. */
  blender::bke::subdiv::Subdiv *subdiv;
};

static void init_data(ModifierData *md)
{
  MultiresModifierData *mmd = reinterpret_cast<MultiresModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mmd, modifier));

  MEMCPY_STRUCT_AFTER(mmd, DNA_struct_default_get(MultiresModifierData), modifier);

  /* Open subdivision panels by default. */
  md->ui_expand_flag = UI_PANEL_DATA_EXPAND_ROOT | UI_SUBPANEL_DATA_EXPAND_1;
}

static void copy_data(const ModifierData *md_src, ModifierData *md_dst, const int flag)
{
  BKE_modifier_copydata_generic(md_src, md_dst, flag);
}

static void free_runtime_data(void *runtime_data_v)
{
  if (runtime_data_v == nullptr) {
    return;
  }
  MultiresRuntimeData *runtime_data = static_cast<MultiresRuntimeData *>(runtime_data_v);
  if (runtime_data->subdiv != nullptr) {
    blender::bke::subdiv::free(runtime_data->subdiv);
  }
  MEM_freeN(runtime_data);
}

static void free_data(ModifierData *md)
{
  MultiresModifierData *mmd = reinterpret_cast<MultiresModifierData *>(md);
  free_runtime_data(mmd->modifier.runtime);
}

static MultiresRuntimeData *multires_ensure_runtime(MultiresModifierData *mmd)
{
  MultiresRuntimeData *runtime_data = static_cast<MultiresRuntimeData *>(mmd->modifier.runtime);
  if (runtime_data == nullptr) {
    runtime_data = MEM_callocN<MultiresRuntimeData>(__func__);
    mmd->modifier.runtime = runtime_data;
  }
  return runtime_data;
}

/* Main goal of this function is to give usable subdivision surface descriptor
 * which matches settings and topology. */
static blender::bke::subdiv::Subdiv *subdiv_descriptor_ensure(
    MultiresModifierData *mmd,
    const blender::bke::subdiv::Settings *subdiv_settings,
    const Mesh *mesh)
{
  MultiresRuntimeData *runtime_data = static_cast<MultiresRuntimeData *>(mmd->modifier.runtime);
  blender::bke::subdiv::Subdiv *subdiv = blender::bke::subdiv::update_from_mesh(
      runtime_data->subdiv, subdiv_settings, mesh);
  runtime_data->subdiv = subdiv;
  return subdiv;
}

/* Subdivide into fully qualified mesh. */

static Mesh *multires_as_mesh(const MultiresModifierData *mmd,
                              const ModifierEvalContext *ctx,
                              Mesh *mesh,
                              blender::bke::subdiv::Subdiv *subdiv)
{
  Mesh *result = mesh;
  const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER);
  const bool ignore_simplify = (ctx->flag & MOD_APPLY_IGNORE_SIMPLIFY);
  const bool ignore_control_edges = (ctx->flag & MOD_APPLY_TO_ORIGINAL);
  const Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  Object *object = ctx->object;
  blender::bke::subdiv::ToMeshSettings mesh_settings;
  BKE_multires_subdiv_mesh_settings_init(&mesh_settings,
                                         scene,
                                         object,
                                         mmd,
                                         use_render_params,
                                         ignore_simplify,
                                         ignore_control_edges);
  if (mesh_settings.resolution < 3) {
    return result;
  }
  blender::bke::subdiv::displacement_attach_from_multires(subdiv, mesh, mmd);
  result = blender::bke::subdiv::subdiv_to_mesh(subdiv, &mesh_settings, mesh);
  return result;
}

/* Subdivide into CCG. */

static void multires_ccg_settings_init(SubdivToCCGSettings *settings,
                                       const MultiresModifierData *mmd,
                                       const ModifierEvalContext *ctx,
                                       const Mesh *mesh)
{
  const bool has_mask = CustomData_has_layer(&mesh->corner_data, CD_GRID_PAINT_MASK);
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
                             blender::bke::subdiv::Subdiv *subdiv)
{
  Mesh *result = mesh;
  SubdivToCCGSettings ccg_settings;
  multires_ccg_settings_init(&ccg_settings, mmd, ctx, mesh);
  if (ccg_settings.resolution < 3) {
    return result;
  }
  blender::bke::subdiv::displacement_attach_from_multires(subdiv, mesh, mmd);
  result = BKE_subdiv_to_ccg_mesh(*subdiv, ccg_settings, *mesh);

  /* NOTE: CCG becomes an owner of Subdiv descriptor, so can not share
   * this pointer. Not sure if it's needed, but might have a second look
   * on the ownership model here. */
  MultiresRuntimeData *runtime_data = static_cast<MultiresRuntimeData *>(mmd->modifier.runtime);
  runtime_data->subdiv = nullptr;

  return result;
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  using namespace blender;
  Mesh *result = mesh;
#if !defined(WITH_OPENSUBDIV)
  BKE_modifier_set_error(ctx->object, md, "Disabled, built without OpenSubdiv");
  return result;
#endif
  MultiresModifierData *mmd = reinterpret_cast<MultiresModifierData *>(md);
  blender::bke::subdiv::Settings subdiv_settings;
  BKE_multires_subdiv_settings_init(&subdiv_settings, mmd);
  if (subdiv_settings.level == 0) {
    return result;
  }
  MultiresRuntimeData *runtime_data = multires_ensure_runtime(mmd);
  blender::bke::subdiv::Subdiv *subdiv = subdiv_descriptor_ensure(mmd, &subdiv_settings, mesh);
  if (subdiv == nullptr) {
    /* Happens on bad topology, also on empty input mesh. */
    return result;
  }
  const bool use_clnors = mmd->flags & eMultiresModifierFlag_UseCustomNormals &&
                          mesh->normals_domain() == blender::bke::MeshNormalDomain::Corner;
  /* NOTE: Orco needs final coordinates on CPU side, which are expected to be
   * accessible via mesh vertices. For this reason we do not evaluate multires to
   * grids when orco is requested. */
  const bool for_orco = (ctx->flag & MOD_APPLY_ORCO) != 0;
  /* Needed when rendering or baking will in sculpt mode. */
  const bool for_render = (ctx->flag & MOD_APPLY_RENDER) != 0;

  const bool sculpt_base_mesh = mmd->flags & eMultiresModifierFlag_UseSculptBaseMesh;

  if ((ctx->object->mode & OB_MODE_SCULPT) && !for_orco && !for_render && !sculpt_base_mesh) {
    /* NOTE: CCG takes ownership over Subdiv. */
    result = multires_as_ccg(mmd, ctx, mesh, subdiv);
    result->runtime->subdiv_ccg_tot_level = mmd->totlvl;
    /* TODO(sergey): Usually it is sculpt stroke's update variants which
     * takes care of this, but is possible that we need this before the
     * stroke: i.e. when exiting blender right after stroke is done.
     * Annoying and not so much black-boxed as far as sculpting goes, and
     * surely there is a better way of solving this. */
    if (ctx->object->sculpt != nullptr) {
      SculptSession *sculpt_session = ctx->object->sculpt;
      sculpt_session->subdiv_ccg = result->runtime->subdiv_ccg.get();
      sculpt_session->multires.active = true;
      sculpt_session->multires.modifier = mmd;
      sculpt_session->multires.level = mmd->sculptlvl;
    }
    // blender::bke::subdiv::stats_print(&subdiv->stats);
  }
  else {
    if (use_clnors) {
      void *data = CustomData_add_layer(
          &mesh->corner_data, CD_NORMAL, CD_CONSTRUCT, mesh->corners_num);
      memcpy(data, mesh->corner_normals().data(), mesh->corner_normals().size_in_bytes());
    }

    result = multires_as_mesh(mmd, ctx, mesh, subdiv);

    if (use_clnors) {
      bke::mesh_set_custom_normals_normalized(
          *result,
          {static_cast<float3 *>(CustomData_get_layer_for_write(
               &result->corner_data, CD_NORMAL, result->corners_num)),
           result->corners_num});
      CustomData_free_layers(&result->corner_data, CD_NORMAL);
    }
    // blender::bke::subdiv::stats_print(&subdiv->stats);
    if (subdiv != runtime_data->subdiv) {
      blender::bke::subdiv::free(subdiv);
    }
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

  MultiresModifierData *mmd = reinterpret_cast<MultiresModifierData *>(md);

  blender::bke::subdiv::Settings subdiv_settings;
  BKE_multires_subdiv_settings_init(&subdiv_settings, mmd);
  if (subdiv_settings.level == 0) {
    return;
  }

  SubdivToCCGSettings ccg_settings;
  multires_ccg_settings_init(&ccg_settings, mmd, ctx, mesh);
  if (ccg_settings.resolution < 3) {
    return;
  }

  MultiresRuntimeData *runtime_data = multires_ensure_runtime(mmd);
  blender::bke::subdiv::Subdiv *subdiv = subdiv_descriptor_ensure(mmd, &subdiv_settings, mesh);
  if (subdiv == nullptr) {
    /* Happens on bad topology, also on empty input mesh. */
    return;
  }
  blender::bke::subdiv::displacement_attach_from_multires(subdiv, mesh, mmd);
  blender::bke::subdiv::deform_coarse_vertices(subdiv, mesh, positions);
  if (subdiv != runtime_data->subdiv) {
    blender::bke::subdiv::free(subdiv);
  }
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  col = &layout->column(true);
  col->prop(ptr, "levels", UI_ITEM_NONE, IFACE_("Levels Viewport"), ICON_NONE);
  col->prop(ptr, "sculpt_levels", UI_ITEM_NONE, IFACE_("Sculpt"), ICON_NONE);
  col->prop(ptr, "render_levels", UI_ITEM_NONE, IFACE_("Render"), ICON_NONE);

  const bool is_sculpt_mode = CTX_data_active_object(C)->mode & OB_MODE_SCULPT;
  uiBlock *block = panel->layout->block();
  UI_block_lock_set(block, !is_sculpt_mode, N_("Sculpt Base Mesh"));
  col->prop(ptr, "use_sculpt_base_mesh", UI_ITEM_NONE, IFACE_("Sculpt Base Mesh"), ICON_NONE);
  UI_block_lock_clear(block);

  layout->prop(ptr, "show_only_control_edges", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void subdivisions_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->enabled_set(RNA_enum_get(&ob_ptr, "mode") != OB_MODE_EDIT);

  MultiresModifierData *mmd = static_cast<MultiresModifierData *>(ptr->data);

  /**
   * Changing some of the properties can not be done once there is an
   * actual displacement stored for this multi-resolution modifier.
   * This check will disallow changes for those properties.
   * This check is a bit stupid but it should be sufficient for the usual
   * multi-resolution usage. It might become less strict and only disallow
   * modifications if there is CD_MDISPS layer, or if there is actual
   * non-zero displacement, but such checks will be too slow to be done
   * on every redraw.
   */

  PointerRNA op_ptr;
  op_ptr = layout->op("OBJECT_OT_multires_subdivide",
                      IFACE_("Subdivide"),
                      ICON_NONE,
                      blender::wm::OpCallContext::ExecDefault,
                      UI_ITEM_NONE);
  RNA_enum_set(&op_ptr, "mode", int8_t(MultiresSubdivideModeType::CatmullClark));
  RNA_string_set(&op_ptr, "modifier", reinterpret_cast<ModifierData *>(mmd)->name);

  row = &layout->row(false);
  op_ptr = row->op("OBJECT_OT_multires_subdivide",
                   IFACE_("Simple"),
                   ICON_NONE,
                   blender::wm::OpCallContext::ExecDefault,
                   UI_ITEM_NONE);
  RNA_enum_set(&op_ptr, "mode", int8_t(MultiresSubdivideModeType::Simple));
  RNA_string_set(&op_ptr, "modifier", reinterpret_cast<ModifierData *>(mmd)->name);
  op_ptr = row->op("OBJECT_OT_multires_subdivide",
                   IFACE_("Linear"),
                   ICON_NONE,
                   blender::wm::OpCallContext::ExecDefault,
                   UI_ITEM_NONE);
  RNA_enum_set(&op_ptr, "mode", int8_t(MultiresSubdivideModeType::Linear));
  RNA_string_set(&op_ptr, "modifier", reinterpret_cast<ModifierData *>(mmd)->name);

  layout->separator();

  layout->op("OBJECT_OT_multires_unsubdivide", IFACE_("Unsubdivide"), ICON_NONE);
  layout->op("OBJECT_OT_multires_higher_levels_delete", IFACE_("Delete Higher"), ICON_NONE);
}

static void shape_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->enabled_set(RNA_enum_get(&ob_ptr, "mode") != OB_MODE_EDIT);

  PointerRNA op_ptr;
  row = &layout->row(false);
  row->op("OBJECT_OT_multires_reshape", IFACE_("Reshape"), ICON_NONE);

  row = &layout->row(false);
  op_ptr = row->op("OBJECT_OT_multires_base_apply", IFACE_("Apply Base"), ICON_NONE);
  RNA_boolean_set(&op_ptr, "apply_heuristic", true);
  op_ptr = row->op("OBJECT_OT_multires_base_apply", IFACE_("Conform Base"), ICON_NONE);
  RNA_boolean_set(&op_ptr, "apply_heuristic", false);
}

static void generate_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col, *row;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  MultiresModifierData *mmd = static_cast<MultiresModifierData *>(ptr->data);

  bool is_external = RNA_boolean_get(ptr, "is_external");

  if (mmd->totlvl == 0) {
    layout->op("OBJECT_OT_multires_rebuild_subdiv", IFACE_("Rebuild Subdivisions"), ICON_NONE);
  }

  col = &layout->column(false);
  row = &col->row(false);
  if (is_external) {
    row->op("OBJECT_OT_multires_external_pack", IFACE_("Pack External"), ICON_NONE);
    col->use_property_split_set(true);
    row = &col->row(false);
    row->prop(ptr, "filepath", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else {
    col->op("OBJECT_OT_multires_external_save", IFACE_("Save External..."), ICON_NONE);
  }
}

static void advanced_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  bool has_displacement = RNA_int_get(ptr, "total_levels") != 0;

  layout->use_property_split_set(true);

  layout->active_set(!has_displacement);

  layout->prop(ptr, "quality", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col = &layout->column(false);
  col->active_set(true);
  col->prop(ptr, "uv_smooth", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "boundary_smooth", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->prop(ptr, "use_creases", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "use_custom_normals", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Multires, panel_draw);
  modifier_subpanel_register(
      region_type, "subdivide", "Subdivision", nullptr, subdivisions_panel_draw, panel_type);
  modifier_subpanel_register(region_type, "shape", "Shape", nullptr, shape_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "generate", "Generate", nullptr, generate_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "advanced", "Advanced", nullptr, advanced_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_Multires = {
    /*idname*/ "Multires",
    /*name*/ N_("Multires"),
    /*struct_name*/ "MultiresModifierData",
    /*struct_size*/ sizeof(MultiresModifierData),
    /*srna*/ &RNA_MultiresModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_RequiresOriginalData,
    /*icon*/ ICON_MOD_MULTIRES,

    /*copy_data*/ copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ deform_matrices,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ free_data,
    /*is_disabled*/ nullptr,
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
