/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BKE_attribute.hh"
#include "BKE_material.h"

#include "DNA_defaults.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_curves.hh"
#include "BKE_deform.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"
#include "BKE_shrinkwrap.hh"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_query.hh"

#include "GEO_smooth_curves.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "MOD_grease_pencil_util.hh"
#include "MOD_ui_common.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *smd = reinterpret_cast<GreasePencilShrinkwrapModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(GreasePencilShrinkwrapModifierData), modifier);
  modifier::greasepencil::init_influence_data(&smd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *smd = reinterpret_cast<const GreasePencilShrinkwrapModifierData *>(md);
  auto *tsmd = reinterpret_cast<GreasePencilShrinkwrapModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tsmd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&smd->influence, &tsmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *smd = reinterpret_cast<GreasePencilShrinkwrapModifierData *>(md);
  modifier::greasepencil::free_influence_data(&smd->influence);

  if (smd->cache_data) {
    BKE_shrinkwrap_free_tree(smd->cache_data);
    MEM_SAFE_FREE(smd->cache_data);
  }
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *smd = reinterpret_cast<GreasePencilShrinkwrapModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&smd->influence, ob, walk, user_data);
  walk(user_data, ob, (ID **)&smd->target, IDWALK_CB_NOP);
  walk(user_data, ob, (ID **)&smd->aux_target, IDWALK_CB_NOP);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  auto *smd = reinterpret_cast<GreasePencilShrinkwrapModifierData *>(md);

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the mesh is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  if (smd->target == nullptr || smd->target->type != OB_MESH) {
    return true;
  }
  if (smd->aux_target != nullptr && smd->aux_target->type != OB_MESH) {
    return true;
  }
  return false;
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  auto *smd = reinterpret_cast<GreasePencilShrinkwrapModifierData *>(md);
  CustomData_MeshMasks mask = {0};

  if (BKE_shrinkwrap_needs_normals(smd->shrink_type, smd->shrink_mode)) {
    mask.lmask |= CD_MASK_CUSTOMLOOPNORMAL;
  }

  if (smd->target != nullptr) {
    DEG_add_object_relation(
        ctx->node, smd->target, DEG_OB_COMP_TRANSFORM, "Grease Pencil Shrinkwrap Modifier");
    DEG_add_object_relation(
        ctx->node, smd->target, DEG_OB_COMP_GEOMETRY, "Grease Pencil Shrinkwrap Modifier");
    DEG_add_customdata_mask(ctx->node, smd->target, &mask);
    if (smd->shrink_type == MOD_SHRINKWRAP_TARGET_PROJECT) {
      DEG_add_special_eval_flag(ctx->node, &smd->target->id, DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY);
    }
  }
  if (smd->aux_target != nullptr) {
    DEG_add_object_relation(
        ctx->node, smd->aux_target, DEG_OB_COMP_TRANSFORM, "Grease Pencil Shrinkwrap Modifier");
    DEG_add_object_relation(
        ctx->node, smd->aux_target, DEG_OB_COMP_GEOMETRY, "Grease Pencil Shrinkwrap Modifier");
    DEG_add_customdata_mask(ctx->node, smd->aux_target, &mask);
    if (smd->shrink_type == MOD_SHRINKWRAP_TARGET_PROJECT) {
      DEG_add_special_eval_flag(
          ctx->node, &smd->aux_target->id, DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY);
    }
  }
  DEG_add_depends_on_transform_relation(ctx->node, "Grease Pencil Shrinkwrap Modifier");
}

static void modify_drawing(const GreasePencilShrinkwrapModifierData &smd,
                           const ModifierEvalContext &ctx,
                           bke::greasepencil::Drawing &drawing)
{
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const Span<MDeformVert> dverts = curves.deform_verts();
  const MutableSpan<float3> positions = curves.positions_for_write();
  const int defgrp_idx = BKE_object_defgroup_name_index(ctx.object,
                                                        smd.influence.vertex_group_name);

  /* Selected source curves. */
  IndexMaskMemory curve_mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, drawing.strokes(), smd.influence, curve_mask_memory);

  ShrinkwrapParams params;
  params.target = smd.target;
  params.aux_target = smd.aux_target;
  params.invert_vertex_weights = smd.influence.flag & GREASE_PENCIL_INFLUENCE_INVERT_VERTEX_GROUP;
  params.keep_distance = smd.keep_dist;
  params.shrink_type = smd.shrink_type;
  params.shrink_options = smd.shrink_opts;
  params.shrink_mode = smd.shrink_mode;
  params.projection_limit = smd.proj_limit;
  params.projection_axis = smd.proj_axis;
  params.subsurf_levels = smd.subsurf_levels;

  curves_mask.foreach_index([&](const int curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    const Span<MDeformVert> curve_dverts = dverts.is_empty() ? dverts : dverts.slice(points);
    const MutableSpan<float3> curve_positions = positions.slice(points);

    shrinkwrapParams_deform(
        params, *ctx.object, *smd.cache_data, curve_dverts, defgrp_idx, curve_positions);
  });

  /* Optional smoothing after shrinkwrap. */
  const VArray<bool> point_selection = VArray<bool>::ForSingle(true, curves.points_num());
  const bool smooth_ends = false;
  const bool keep_shape = true;
  geometry::smooth_curve_attribute(curves_mask,
                                   points_by_curve,
                                   point_selection,
                                   curves.cyclic(),
                                   smd.smooth_step,
                                   smd.smooth_factor,
                                   smooth_ends,
                                   keep_shape,
                                   positions);

  drawing.tag_positions_changed();
}

static void ensure_shrinkwrap_cache_data(GreasePencilShrinkwrapModifierData &smd,
                                         const ModifierEvalContext &ctx)
{
  if (smd.cache_data) {
    BKE_shrinkwrap_free_tree(smd.cache_data);
    MEM_SAFE_FREE(smd.cache_data);
  }
  Object *target_ob = DEG_get_evaluated_object(ctx.depsgraph, smd.target);
  Mesh *target_mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(target_ob);

  smd.cache_data = static_cast<ShrinkwrapTreeData *>(
      MEM_callocN(sizeof(ShrinkwrapTreeData), __func__));
  const bool tree_ok = BKE_shrinkwrap_init_tree(
      smd.cache_data, target_mesh, smd.shrink_type, smd.shrink_mode, false);
  if (!tree_ok) {
    MEM_SAFE_FREE(smd.cache_data);
  }
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  using bke::greasepencil::Drawing;
  using bke::greasepencil::Layer;
  using modifier::greasepencil::LayerDrawingInfo;

  auto &smd = *reinterpret_cast<GreasePencilShrinkwrapModifierData *>(md);
  BLI_assert(smd.target != nullptr);
  if (smd.target == ctx->object || smd.aux_target == ctx->object) {
    return;
  }
  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int frame = grease_pencil.runtime->eval_frame;

  ensure_shrinkwrap_cache_data(smd, *ctx);

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, smd.influence, mask_memory);

  const Vector<Drawing *> drawings = modifier::greasepencil::get_drawings_for_write(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(drawings,
                               [&](Drawing *drawing) { modify_drawing(smd, *ctx, *drawing); });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  static const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);
  int wrap_method = RNA_enum_get(ptr, "wrap_method");
  uiLayout *col, *row;

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "wrap_method", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (ELEM(wrap_method,
           MOD_SHRINKWRAP_PROJECT,
           MOD_SHRINKWRAP_NEAREST_SURFACE,
           MOD_SHRINKWRAP_TARGET_PROJECT))
  {
    uiItemR(layout, ptr, "wrap_mode", UI_ITEM_NONE, nullptr, ICON_NONE);
  }

  if (wrap_method == MOD_SHRINKWRAP_PROJECT) {
    uiItemR(layout, ptr, "project_limit", UI_ITEM_NONE, IFACE_("Limit"), ICON_NONE);
    uiItemR(layout, ptr, "subsurf_levels", UI_ITEM_NONE, nullptr, ICON_NONE);

    col = uiLayoutColumn(layout, false);
    row = uiLayoutRowWithHeading(col, true, IFACE_("Axis"));
    uiItemR(row, ptr, "use_project_x", toggles_flag, nullptr, ICON_NONE);
    uiItemR(row, ptr, "use_project_y", toggles_flag, nullptr, ICON_NONE);
    uiItemR(row, ptr, "use_project_z", toggles_flag, nullptr, ICON_NONE);

    uiItemR(col, ptr, "use_negative_direction", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "use_positive_direction", UI_ITEM_NONE, nullptr, ICON_NONE);

    uiItemR(layout, ptr, "cull_face", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
    col = uiLayoutColumn(layout, false);
    uiLayoutSetActive(col,
                      RNA_boolean_get(ptr, "use_negative_direction") &&
                          RNA_enum_get(ptr, "cull_face") != 0);
    uiItemR(col, ptr, "use_invert_cull", UI_ITEM_NONE, nullptr, ICON_NONE);
  }

  uiItemR(layout, ptr, "target", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (wrap_method == MOD_SHRINKWRAP_PROJECT) {
    uiItemR(layout, ptr, "auxiliary_target", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  uiItemR(layout, ptr, "offset", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "smooth_factor", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "smooth_step", UI_ITEM_NONE, IFACE_("Repeat"), ICON_NONE);

  if (uiLayout *influence_panel = uiLayoutPanelProp(
          C, layout, ptr, "open_influence_panel", "Influence"))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_vertex_group_settings(C, influence_panel, ptr);
  }

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilShrinkwrap, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *smd = reinterpret_cast<const GreasePencilShrinkwrapModifierData *>(md);

  BLO_write_struct(writer, GreasePencilShrinkwrapModifierData, smd);
  modifier::greasepencil::write_influence_data(writer, &smd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *smd = reinterpret_cast<GreasePencilShrinkwrapModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &smd->influence);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilShrinkwrap = {
    /*idname*/ "GreasePencilShrinkwrap",
    /*name*/ N_("Shrinkwrap"),
    /*struct_name*/ "GreasePencilShrinkwrapModifierData",
    /*struct_size*/ sizeof(GreasePencilShrinkwrapModifierData),
    /*srna*/ &RNA_GreasePencilShrinkwrapModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_SHRINKWRAP,

    /*copy_data*/ blender::copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ blender::modify_geometry_set,

    /*init_data*/ blender::init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ blender::free_data,
    /*is_disabled*/ blender::is_disabled,
    /*update_depsgraph*/ blender::update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ blender::foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ blender::panel_register,
    /*blend_write*/ blender::blend_write,
    /*blend_read*/ blender::blend_read,
};
