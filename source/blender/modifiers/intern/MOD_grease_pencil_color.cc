/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"

#include "BLI_math_color.h"

#include "BKE_colortools.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_query.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

namespace blender {

using bke::greasepencil::Drawing;

static void init_data(ModifierData *md)
{
  auto *cmd = reinterpret_cast<GreasePencilColorModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(cmd, modifier));

  MEMCPY_STRUCT_AFTER(cmd, DNA_struct_default_get(GreasePencilColorModifierData), modifier);
  modifier::greasepencil::init_influence_data(&cmd->influence, true);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *cmd = reinterpret_cast<const GreasePencilColorModifierData *>(md);
  auto *tcmd = reinterpret_cast<GreasePencilColorModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tcmd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&cmd->influence, &tcmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *cmd = reinterpret_cast<GreasePencilColorModifierData *>(md);
  modifier::greasepencil::free_influence_data(&cmd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *cmd = reinterpret_cast<GreasePencilColorModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&cmd->influence, ob, walk, user_data);
}

static void apply_color_factor(ColorGeometry4f &color,
                               const ColorGeometry4f &material_color,
                               const float3 factor)
{
  float3 hsv;
  /* When input alpha is zero, replace with material color. */
  if (color.a == 0.0f && material_color.a > 0.0f) {
    color.a = 1.0f;
    rgb_to_hsv_v(material_color, hsv);
  }
  else {
    rgb_to_hsv_v(color, hsv);
  }
  hsv[0] = fractf(hsv[0] + factor[0] + 0.5f);
  hsv[1] = clamp_f(hsv[1] * factor[1], 0.0f, 1.0f);
  hsv[2] = hsv[2] * factor[2];
  hsv_to_rgb_v(hsv, color);
}

static void modify_stroke_color(Object &ob,
                                const GreasePencilColorModifierData &cmd,
                                bke::CurvesGeometry &curves,
                                const IndexMask &curves_mask,
                                const MutableSpan<ColorGeometry4f> vertex_colors)
{
  const bool use_curve = (cmd.influence.flag & GREASE_PENCIL_INFLUENCE_USE_CUSTOM_CURVE);
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();

  bke::AttributeAccessor attributes = curves.attributes();
  const VArray<int> stroke_materials = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Curve, 0);

  curves_mask.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    const Material *ma = BKE_object_material_get(&ob, stroke_materials[curve_i] + 1);
    const MaterialGPencilStyle *gp_style = ma ? ma->gp_style : nullptr;
    const ColorGeometry4f material_color = (gp_style ? ColorGeometry4f(gp_style->stroke_rgba) :
                                                       ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));

    const IndexRange points = points_by_curve[curve_i];
    for (const int64_t i : points.index_range()) {
      const int64_t point_i = points[i];
      float3 factor = cmd.hsv;
      if (use_curve) {
        const float curve_input = points.size() >= 2 ? (float(i) / float(points.size() - 1)) :
                                                       0.0f;
        const float curve_factor = BKE_curvemapping_evaluateF(
            cmd.influence.custom_curve, 0, curve_input);
        factor *= curve_factor;
      }

      apply_color_factor(vertex_colors[point_i], material_color, factor);
    }
  });
}

static void modify_fill_color(Object &ob,
                              const GreasePencilColorModifierData &cmd,
                              Drawing &drawing,
                              const IndexMask &curves_mask)
{
  const bke::CurvesGeometry &curves = drawing.strokes();
  const bke::AttributeAccessor attributes = curves.attributes();
  /* Fill color per stroke. */
  MutableSpan<ColorGeometry4f> fill_colors = drawing.fill_colors_for_write();
  const VArray<int> stroke_materials = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Curve, 0);

  curves_mask.foreach_index(GrainSize(512), [&](int64_t curve_i) {
    const Material *ma = BKE_object_material_get(&ob, stroke_materials[curve_i] + 1);
    const MaterialGPencilStyle *gp_style = ma ? ma->gp_style : nullptr;
    const ColorGeometry4f material_color = (gp_style ? ColorGeometry4f(gp_style->fill_rgba) :
                                                       ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));

    apply_color_factor(fill_colors[curve_i], material_color, cmd.hsv);
  });
}

static void modify_drawing(ModifierData &md, const ModifierEvalContext &ctx, Drawing &drawing)
{
  auto &cmd = reinterpret_cast<GreasePencilColorModifierData &>(md);

  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  IndexMaskMemory mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, curves, cmd.influence, mask_memory);

  switch (cmd.color_mode) {
    case MOD_GREASE_PENCIL_COLOR_STROKE:
      modify_stroke_color(
          *ctx.object, cmd, curves, curves_mask, drawing.vertex_colors_for_write());
      break;
    case MOD_GREASE_PENCIL_COLOR_FILL:
      modify_fill_color(*ctx.object, cmd, drawing, curves_mask);
      break;
    case MOD_GREASE_PENCIL_COLOR_BOTH:
      modify_stroke_color(
          *ctx.object, cmd, curves, curves_mask, drawing.vertex_colors_for_write());
      modify_fill_color(*ctx.object, cmd, drawing, curves_mask);
      break;
    case MOD_GREASE_PENCIL_COLOR_HARDNESS:
      BLI_assert_unreachable();
      break;
  }
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  auto *cmd = reinterpret_cast<GreasePencilColorModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, cmd->influence, mask_memory);
  const Vector<Drawing *> drawings = modifier::greasepencil::get_drawings_for_write(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(drawings,
                               [&](Drawing *drawing) { modify_drawing(*md, *ctx, *drawing); });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "color_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->prop(ptr, "hue", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  layout->prop(ptr, "saturation", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  layout->prop(ptr, "value", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);

  if (uiLayout *influence_panel = layout->panel_prop(
          C, ptr, "open_influence_panel", IFACE_("Influence")))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_custom_curve_settings(C, influence_panel, ptr);
  }

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilColor, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *cmd = reinterpret_cast<const GreasePencilColorModifierData *>(md);

  BLO_write_struct(writer, GreasePencilColorModifierData, cmd);
  modifier::greasepencil::write_influence_data(writer, &cmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *cmd = reinterpret_cast<GreasePencilColorModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &cmd->influence);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilColor = {
    /*idname*/ "GreasePencilColor",
    /*name*/ N_("Color"),
    /*struct_name*/ "GreasePencilColorModifierData",
    /*struct_size*/ sizeof(GreasePencilColorModifierData),
    /*srna*/ &RNA_GreasePencilColorModifier,
    /*type*/ ModifierTypeType::NonGeometrical,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_HUE_SATURATION,

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
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ blender::foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ blender::panel_register,
    /*blend_write*/ blender::blend_write,
    /*blend_read*/ blender::blend_read,
};
