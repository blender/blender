/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.hh"

#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_colorband.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_query.hh"
#include "BKE_material.h"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_query.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.h"

#include "MOD_grease_pencil_util.hh"
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

namespace blender {

using bke::greasepencil::Drawing;

static void init_data(ModifierData *md)
{
  auto *tmd = reinterpret_cast<GreasePencilTintModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(tmd, modifier));

  MEMCPY_STRUCT_AFTER(tmd, DNA_struct_default_get(GreasePencilTintModifierData), modifier);
  modifier::greasepencil::init_influence_data(&tmd->influence, true);

  /* Add default color ramp. */
  tmd->color_ramp = BKE_colorband_add(false);
  if (tmd->color_ramp) {
    BKE_colorband_init(tmd->color_ramp, true);
    CBData *data = tmd->color_ramp->data;
    data[0].r = data[0].g = data[0].b = data[0].a = 1.0f;
    data[0].pos = 0.0f;
    data[1].r = data[1].g = data[1].b = 0.0f;
    data[1].a = 1.0f;
    data[1].pos = 1.0f;

    tmd->color_ramp->tot = 2;
  }
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *tmd = reinterpret_cast<const GreasePencilTintModifierData *>(md);
  auto *ttmd = reinterpret_cast<GreasePencilTintModifierData *>(target);

  modifier::greasepencil::free_influence_data(&ttmd->influence);
  MEM_SAFE_FREE(ttmd->color_ramp);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&tmd->influence, &ttmd->influence, flag);

  if (tmd->color_ramp) {
    ttmd->color_ramp = static_cast<ColorBand *>(MEM_dupallocN(tmd->color_ramp));
  }
}

static void free_data(ModifierData *md)
{
  auto *tmd = reinterpret_cast<GreasePencilTintModifierData *>(md);
  modifier::greasepencil::free_influence_data(&tmd->influence);

  MEM_SAFE_FREE(tmd->color_ramp);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *tmd = reinterpret_cast<GreasePencilTintModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&tmd->influence, ob, walk, user_data);
  walk(user_data, ob, (ID **)&tmd->object, IDWALK_CB_NOP);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  auto *tmd = reinterpret_cast<GreasePencilTintModifierData *>(md);
  if (tmd->tint_mode == MOD_GREASE_PENCIL_TINT_GRADIENT) {
    return tmd->object == nullptr;
  }
  return false;
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  auto *tmd = reinterpret_cast<GreasePencilTintModifierData *>(md);
  if (tmd->object != nullptr) {
    DEG_add_object_relation(
        ctx->node, tmd->object, DEG_OB_COMP_TRANSFORM, "Grease Pencil Tint Modifier");
  }
  DEG_add_object_relation(
      ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Grease Pencil Tint Modifier");
}

static ColorGeometry4f get_base_color(const ColorGeometry4f &input_color,
                                      const ColorGeometry4f &material_color)
{
  /* When input alpha is zero, replace with material color. */
  return (input_color.a == 0.0f && material_color.a > 0.0f) ? material_color : input_color;
}

static ColorGeometry4f apply_uniform_tint(const GreasePencilTintModifierData &tmd,
                                          const ColorGeometry4f &input_color,
                                          const float factor)
{
  const float3 rgb = math::interpolate(
      float3(input_color.r, input_color.g, input_color.b), float3(tmd.color), factor);
  /* Alpha is unchanged. */
  return ColorGeometry4f(rgb[0], rgb[1], rgb[2], input_color.a);
}

static ColorGeometry4f apply_gradient_tint(const GreasePencilTintModifierData &tmd,
                                           const float4x4 &matrix,
                                           const float3 &position,
                                           const ColorGeometry4f &input_color,
                                           const float factor)
{
  const float3 gradient_pos = math::transform_point(matrix, position);
  const float gradient_factor = std::clamp(
      math::safe_divide(math::length(gradient_pos), tmd.radius), 0.0f, 1.0f);

  float4 gradient_color;
  BKE_colorband_evaluate(tmd.color_ramp, gradient_factor, gradient_color);

  const float3 input_rgb = {input_color.r, input_color.g, input_color.b};
  /* GP2 compatibility: ignore vertex group factor and use the plain modifier setting for
   * RGB mixing. */
  const float3 rgb = math::interpolate(input_rgb, gradient_color.xyz(), tmd.factor);
  /* GP2 compatibility: use vertex group factor for alpha. */
  return ColorGeometry4f(rgb[0], rgb[1], rgb[2], factor);
}

static void modify_stroke_color(Object &ob,
                                const GreasePencilTintModifierData &tmd,
                                bke::CurvesGeometry &curves,
                                const IndexMask &curves_mask,
                                const MutableSpan<ColorGeometry4f> vertex_colors)
{
  const bool use_weight_as_factor = (tmd.flag & MOD_GREASE_PENCIL_TINT_USE_WEIGHT_AS_FACTOR);
  const bool invert_vertex_group = (tmd.influence.flag &
                                    GREASE_PENCIL_INFLUENCE_INVERT_VERTEX_GROUP);
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();

  bke::AttributeAccessor attributes = curves.attributes();
  const VArray<int> stroke_materials = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Curve, 0);
  const VArray<float> vgroup_weights = modifier::greasepencil::get_influence_vertex_weights(
      curves, tmd.influence);

  /* Common input color and base factor calculation. */
  auto get_material_color = [&](const int64_t curve_i) {
    const Material *ma = BKE_object_material_get(&ob, stroke_materials[curve_i] + 1);
    const MaterialGPencilStyle *gp_style = ma ? ma->gp_style : nullptr;
    return (gp_style ? ColorGeometry4f(gp_style->stroke_rgba) :
                       ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
  };

  auto get_point_factor = [&](const int64_t point_i) {
    if (use_weight_as_factor) {
      const float weight = vgroup_weights[point_i];
      return invert_vertex_group ? 1.0f - weight : weight;
    }
    return tmd.factor;
  };

  const GreasePencilTintModifierMode tint_mode = GreasePencilTintModifierMode(tmd.tint_mode);
  switch (tint_mode) {
    case MOD_GREASE_PENCIL_TINT_UNIFORM: {
      curves_mask.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
        const ColorGeometry4f material_color = get_material_color(curve_i);

        const IndexRange points = points_by_curve[curve_i];
        for (const int64_t point_i : points) {
          vertex_colors[point_i] = apply_uniform_tint(
              tmd,
              get_base_color(vertex_colors[point_i], material_color),
              get_point_factor(point_i));
        }
      });
      break;
    }
    case MOD_GREASE_PENCIL_TINT_GRADIENT: {
      if (tmd.object == nullptr) {
        return;
      }

      const OffsetIndices<int> points_by_curve = curves.points_by_curve();
      const Span<float3> positions = curves.positions();
      /* Transforms points to the gradient object space. */
      const float4x4 matrix = tmd.object->world_to_object() * ob.object_to_world();

      curves_mask.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
        const ColorGeometry4f material_color = get_material_color(curve_i);

        const IndexRange points = points_by_curve[curve_i];
        for (const int64_t point_i : points) {
          vertex_colors[point_i] = apply_gradient_tint(
              tmd,
              matrix,
              positions[point_i],
              get_base_color(vertex_colors[point_i], material_color),
              get_point_factor(point_i));
        }
      });
      break;
    }
  }
}

static void modify_fill_color(Object &ob,
                              const GreasePencilTintModifierData &tmd,
                              bke::CurvesGeometry &curves,
                              const IndexMask &curves_mask)
{
  const bool use_weight_as_factor = (tmd.flag & MOD_GREASE_PENCIL_TINT_USE_WEIGHT_AS_FACTOR);
  const bool invert_vertex_group = (tmd.influence.flag &
                                    GREASE_PENCIL_INFLUENCE_INVERT_VERTEX_GROUP);
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const GreasePencilTintModifierMode tint_mode = GreasePencilTintModifierMode(tmd.tint_mode);

  /* Check early before getting attribute writers. */
  if (tint_mode == MOD_GREASE_PENCIL_TINT_GRADIENT && tmd.object == nullptr) {
    return;
  }

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  /* Fill color per stroke. */
  bke::SpanAttributeWriter<ColorGeometry4f> fill_colors =
      attributes.lookup_or_add_for_write_span<ColorGeometry4f>(
          "fill_color",
          bke::AttrDomain::Curve,
          bke::AttributeInitVArray(VArray<ColorGeometry4f>::ForSingle(
              ColorGeometry4f(float4(0.0f)), curves.curves_num())));
  const VArray<int> stroke_materials = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Curve, 0);
  const VArray<float> vgroup_weights = modifier::greasepencil::get_influence_vertex_weights(
      curves, tmd.influence);

  /* Common input color and base factor calculation. */
  auto get_material_color = [&](const int64_t curve_i) {
    const Material *ma = BKE_object_material_get(&ob, stroke_materials[curve_i] + 1);
    const MaterialGPencilStyle *gp_style = ma ? ma->gp_style : nullptr;
    return (gp_style ? ColorGeometry4f(gp_style->fill_rgba) :
                       ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
  };

  auto get_curve_factor = [&](const int64_t curve_i) {
    if (use_weight_as_factor) {
      /* Use the first stroke point as vertex weight. */
      const IndexRange points = points_by_curve[curve_i];
      const float weight = points.is_empty() ? 1.0f : vgroup_weights[points.first()];
      return invert_vertex_group ? 1.0f - weight : weight;
    }
    return tmd.factor;
  };

  switch (tint_mode) {
    case MOD_GREASE_PENCIL_TINT_UNIFORM: {
      curves_mask.foreach_index(GrainSize(512), [&](int64_t curve_i) {
        const ColorGeometry4f material_color = get_material_color(curve_i);
        fill_colors.span[curve_i] = apply_uniform_tint(
            tmd,
            get_base_color(fill_colors.span[curve_i], material_color),
            get_curve_factor(curve_i));
      });
      break;
    }
    case MOD_GREASE_PENCIL_TINT_GRADIENT: {
      const OffsetIndices<int> points_by_curve = curves.points_by_curve();
      const Span<float3> positions = curves.positions();
      /* Transforms points to the gradient object space. */
      const float4x4 matrix = tmd.object->world_to_object() * ob.object_to_world();

      curves_mask.foreach_index(GrainSize(512), [&](int64_t curve_i) {
        const ColorGeometry4f material_color = get_material_color(curve_i);
        /* Use the first stroke point for gradient position. */
        const IndexRange points = points_by_curve[curve_i];
        const float3 pos = points.is_empty() ? float3(0.0f, 0.0f, 0.0f) :
                                               positions[points.first()];

        fill_colors.span[curve_i] = apply_gradient_tint(
            tmd,
            matrix,
            pos,
            get_base_color(fill_colors.span[curve_i], material_color),
            get_curve_factor(curve_i));
      });
      break;
    }
  }

  fill_colors.finish();
}

static void modify_opacity(const GreasePencilTintModifierData &tmd,
                           bke::CurvesGeometry &curves,
                           const IndexMask &curves_mask)
{
  /* Only when factor is greater than 1. */
  if (tmd.factor <= 1.0f) {
    return;
  }

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> opacities = attributes.lookup_or_add_for_write_span<float>(
      "opacity", bke::AttrDomain::Point);

  curves_mask.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    for (const int64_t point_i : points) {
      opacities.span[point_i] = std::clamp(
          opacities.span[point_i] + tmd.factor - 1.0f, 0.0f, 1.0f);
    }
  });

  opacities.finish();
}

static void modify_curves(ModifierData &md, const ModifierEvalContext &ctx, Drawing &drawing)
{
  auto &tmd = reinterpret_cast<GreasePencilTintModifierData &>(md);
  bke::CurvesGeometry &curves = drawing.strokes_for_write();

  IndexMaskMemory mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, curves, tmd.influence, mask_memory);

  /* Factor > 1.0 also affects the opacity of the stroke. */
  modify_opacity(tmd, curves, curves_mask);

  switch (tmd.color_mode) {
    case MOD_GREASE_PENCIL_COLOR_STROKE:
      modify_stroke_color(
          *ctx.object, tmd, curves, curves_mask, drawing.vertex_colors_for_write());
      break;
    case MOD_GREASE_PENCIL_COLOR_FILL:
      modify_fill_color(*ctx.object, tmd, curves, curves_mask);
      break;
    case MOD_GREASE_PENCIL_COLOR_BOTH:
      modify_stroke_color(
          *ctx.object, tmd, curves, curves_mask, drawing.vertex_colors_for_write());
      modify_fill_color(*ctx.object, tmd, curves, curves_mask);
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
  auto *tmd = reinterpret_cast<GreasePencilTintModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, tmd->influence, mask_memory);
  const Vector<Drawing *> drawings = modifier::greasepencil::get_drawings_for_write(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(drawings,
                               [&](Drawing *drawing) { modify_curves(*md, *ctx, *drawing); });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  const GreasePencilTintModifierMode tint_mode = GreasePencilTintModifierMode(
      RNA_enum_get(ptr, "tint_mode"));
  const bool use_weight_as_factor = RNA_boolean_get(ptr, "use_weight_as_factor");

  uiItemR(layout, ptr, "color_mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiLayout *row = uiLayoutRow(layout, true);
  uiLayoutSetActive(row, !use_weight_as_factor);
  uiItemR(row, ptr, "factor", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(row, ptr, "use_weight_as_factor", UI_ITEM_NONE, "", ICON_MOD_VERTEX_WEIGHT);

  uiItemR(layout, ptr, "tint_mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  switch (tint_mode) {
    case MOD_GREASE_PENCIL_TINT_UNIFORM:
      uiItemR(layout, ptr, "color", UI_ITEM_NONE, nullptr, ICON_NONE);
      break;
    case MOD_GREASE_PENCIL_TINT_GRADIENT:
      uiLayout *col = uiLayoutColumn(layout, false);
      uiLayoutSetPropSep(col, false);
      uiTemplateColorRamp(col, ptr, "color_ramp", true);
      uiItemS(layout);
      uiItemR(layout, ptr, "object", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, ptr, "radius", UI_ITEM_NONE, nullptr, ICON_NONE);
      break;
  }

  if (uiLayout *influence_panel = uiLayoutPanelProp(
          C, layout, ptr, "open_influence_panel", "Influence"))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_vertex_group_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_custom_curve_settings(C, influence_panel, ptr);
  }

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilTint, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *tmd = reinterpret_cast<const GreasePencilTintModifierData *>(md);

  BLO_write_struct(writer, GreasePencilTintModifierData, tmd);
  modifier::greasepencil::write_influence_data(writer, &tmd->influence);
  if (tmd->color_ramp) {
    BLO_write_struct(writer, ColorBand, tmd->color_ramp);
  }
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *tmd = reinterpret_cast<GreasePencilTintModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &tmd->influence);
  BLO_read_data_address(reader, &tmd->color_ramp);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilTint = {
    /*idname*/ "GreasePencilTint",
    /*name*/ N_("Tint"),
    /*struct_name*/ "GreasePencilTintModifierData",
    /*struct_size*/ sizeof(GreasePencilTintModifierData),
    /*srna*/ &RNA_GreasePencilTintModifier,
    /*type*/ ModifierTypeType::NonGeometrical,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_TINT,

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
