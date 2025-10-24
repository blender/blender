/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_math_matrix.hh"

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"

#include "BLO_read_write.hh"

#include "GEO_realize_instances.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_types.hh"

#include "RNA_prototypes.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_ui_common.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilMirrorModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mmd, modifier));

  MEMCPY_STRUCT_AFTER(mmd, DNA_struct_default_get(GreasePencilMirrorModifierData), modifier);
  modifier::greasepencil::init_influence_data(&mmd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *mmd = reinterpret_cast<const GreasePencilMirrorModifierData *>(md);
  auto *tmmd = reinterpret_cast<GreasePencilMirrorModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tmmd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&mmd->influence, &tmmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilMirrorModifierData *>(md);
  modifier::greasepencil::free_influence_data(&mmd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *mmd = reinterpret_cast<GreasePencilMirrorModifierData *>(md);
  walk(user_data, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
  modifier::greasepencil::foreach_influence_ID_link(&mmd->influence, ob, walk, user_data);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  auto *mmd = reinterpret_cast<GreasePencilMirrorModifierData *>(md);
  if (mmd->object != nullptr) {
    DEG_add_object_relation(
        ctx->node, mmd->object, DEG_OB_COMP_TRANSFORM, "Grease Pencil Mirror Modifier");
    DEG_add_depends_on_transform_relation(ctx->node, "Grease Pencil Mirror Modifier");
  }
}

static float4x4 get_mirror_matrix(const Object &ob,
                                  const GreasePencilMirrorModifierData &mmd,
                                  const bool mirror_x,
                                  const bool mirror_y,
                                  const bool mirror_z)
{
  float4x4 matrix = math::from_scale<float4x4, 3>(
      float3(mirror_x ? -1.0f : 1.0f, mirror_y ? -1.0f : 1.0f, mirror_z ? -1.0f : 1.0f));

  if (mmd.object) {
    /* Transforms from parent object space to target object space. */
    const float4x4 to_target = math::invert(mmd.object->object_to_world()) * ob.object_to_world();
    /* Mirror points in the target object space. */
    matrix = math::invert(to_target) * matrix * to_target;
  }
  return matrix;
}

static bke::CurvesGeometry create_mirror_copies(const Object &ob,
                                                const GreasePencilMirrorModifierData &mmd,
                                                const bke::CurvesGeometry &base_curves,
                                                const bke::CurvesGeometry &mirror_curves)
{
  const bool use_mirror_x = (mmd.flag & MOD_GREASE_PENCIL_MIRROR_AXIS_X);
  const bool use_mirror_y = (mmd.flag & MOD_GREASE_PENCIL_MIRROR_AXIS_Y);
  const bool use_mirror_z = (mmd.flag & MOD_GREASE_PENCIL_MIRROR_AXIS_Z);

  Curves *base_curves_id = bke::curves_new_nomain(base_curves);
  Curves *mirror_curves_id = bke::curves_new_nomain(mirror_curves);
  bke::GeometrySet base_geo = bke::GeometrySet::from_curves(base_curves_id);
  bke::GeometrySet mirror_geo = bke::GeometrySet::from_curves(mirror_curves_id);

  std::unique_ptr<bke::Instances> instances = std::make_unique<bke::Instances>();
  const int base_handle = instances->add_reference(bke::InstanceReference{base_geo});
  const int mirror_handle = instances->add_reference(bke::InstanceReference{mirror_geo});
  for (const int mirror_x : IndexRange(use_mirror_x ? 2 : 1)) {
    for (const int mirror_y : IndexRange(use_mirror_y ? 2 : 1)) {
      for (const int mirror_z : IndexRange(use_mirror_z ? 2 : 1)) {
        if (mirror_x == 0 && mirror_y == 0 && mirror_z == 0) {
          instances->add_instance(base_handle, float4x4::identity());
        }
        else {
          const float4x4 matrix = get_mirror_matrix(
              ob, mmd, bool(mirror_x), bool(mirror_y), bool(mirror_z));
          instances->add_instance(mirror_handle, matrix);
        }
      }
    }
  }

  geometry::RealizeInstancesOptions options;
  options.keep_original_ids = true;
  options.realize_instance_attributes = false;
  bke::GeometrySet result_geo = geometry::realize_instances(
                                    bke::GeometrySet::from_instances(instances.release()), options)
                                    .geometry;
  return std::move(result_geo.get_curves_for_write()->geometry.wrap());
}

static void modify_drawing(const GreasePencilMirrorModifierData &mmd,
                           const ModifierEvalContext &ctx,
                           bke::greasepencil::Drawing &drawing)
{
  const bool use_mirror_x = (mmd.flag & MOD_GREASE_PENCIL_MIRROR_AXIS_X);
  const bool use_mirror_y = (mmd.flag & MOD_GREASE_PENCIL_MIRROR_AXIS_Y);
  const bool use_mirror_z = (mmd.flag & MOD_GREASE_PENCIL_MIRROR_AXIS_Z);
  if (!use_mirror_x && !use_mirror_y && !use_mirror_z) {
    return;
  }

  const bke::CurvesGeometry &src_curves = drawing.strokes();
  if (src_curves.curve_num == 0) {
    return;
  }
  /* Selected source curves. */
  IndexMaskMemory curve_mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, src_curves, mmd.influence, curve_mask_memory);

  if (curves_mask.size() == src_curves.curve_num) {
    /* All geometry gets mirrored. */
    drawing.strokes_for_write() = create_mirror_copies(*ctx.object, mmd, src_curves, src_curves);
  }
  else {
    /* Create masked geometry, then mirror it. */
    bke::CurvesGeometry masked_curves = bke::curves_copy_curve_selection(
        src_curves, curves_mask, {});

    drawing.strokes_for_write() = create_mirror_copies(
        *ctx.object, mmd, src_curves, masked_curves);
  }

  drawing.tag_topology_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  using bke::greasepencil::Drawing;

  auto *mmd = reinterpret_cast<GreasePencilMirrorModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, mmd->influence, mask_memory);

  const Vector<Drawing *> drawings = modifier::greasepencil::get_drawings_for_write(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(drawings,
                               [&](Drawing *drawing) { modify_drawing(*mmd, *ctx, *drawing); });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);
  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  layout->use_property_split_set(true);

  uiLayout *row = &layout->row(true, IFACE_("Axis"));
  row->prop(ptr, "use_axis_x", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "use_axis_y", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "use_axis_z", toggles_flag, std::nullopt, ICON_NONE);

  layout->prop(ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (uiLayout *influence_panel = layout->panel_prop(
          C, ptr, "open_influence_panel", IFACE_("Influence")))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
  }

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilMirror, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *mmd = reinterpret_cast<const GreasePencilMirrorModifierData *>(md);

  BLO_write_struct(writer, GreasePencilMirrorModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilMirrorModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilMirror = {
    /*idname*/ "GreasePencilMirror",
    /*name*/ N_("Mirror"),
    /*struct_name*/ "GreasePencilMirrorModifierData",
    /*struct_size*/ sizeof(GreasePencilMirrorModifierData),
    /*srna*/ &RNA_GreasePencilMirrorModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_MIRROR,

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
    /*is_disabled*/ nullptr,
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
