/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_math_matrix.hh"

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_query.hh"

#include "GEO_realize_instances.hh"

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
  auto *mmd = reinterpret_cast<GreasePencilMultiModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mmd, modifier));

  MEMCPY_STRUCT_AFTER(mmd, DNA_struct_default_get(GreasePencilMultiModifierData), modifier);
  modifier::greasepencil::init_influence_data(&mmd->influence, true);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *mmd = reinterpret_cast<const GreasePencilMultiModifierData *>(md);
  auto *tmmd = reinterpret_cast<GreasePencilMultiModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&mmd->influence, &tmmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilMultiModifierData *>(md);
  modifier::greasepencil::free_influence_data(&mmd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *mmd = reinterpret_cast<GreasePencilMultiModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&mmd->influence, ob, walk, user_data);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  auto *mmd = reinterpret_cast<GreasePencilMultiModifierData *>(md);
  if (mmd->duplications <= 1) {
    return true;
  }
  return false;
}

static bke::CurvesGeometry duplicate_strokes(const bke::CurvesGeometry &curves,
                                             const IndexMask curves_mask,
                                             const IndexMask unselected_mask,
                                             const int count,
                                             int &r_original_point_count,
                                             int &r_original_curve_count)
{
  bke::CurvesGeometry masked_curves = bke::curves_copy_curve_selection(curves, curves_mask, {});
  bke::CurvesGeometry unselected_curves = bke::curves_copy_curve_selection(
      curves, unselected_mask, {});

  r_original_point_count = masked_curves.points_num();
  r_original_curve_count = masked_curves.curves_num();

  Curves *masked_curves_id = bke::curves_new_nomain(masked_curves);
  Curves *unselected_curves_id = bke::curves_new_nomain(unselected_curves);

  bke::GeometrySet masked_geo = bke::GeometrySet::from_curves(masked_curves_id);
  bke::GeometrySet unselected_geo = bke::GeometrySet::from_curves(unselected_curves_id);

  std::unique_ptr<bke::Instances> instances = std::make_unique<bke::Instances>();
  const int masked_handle = instances->add_reference(bke::InstanceReference{masked_geo});
  const int unselected_handle = instances->add_reference(bke::InstanceReference{unselected_geo});

  for ([[maybe_unused]] const int i : IndexRange(count)) {
    instances->add_instance(masked_handle, float4x4::identity());
  }
  instances->add_instance(unselected_handle, float4x4::identity());

  geometry::RealizeInstancesOptions options;
  options.keep_original_ids = true;
  options.realize_instance_attributes = true;
  bke::GeometrySet result_geo = geometry::realize_instances(
                                    bke::GeometrySet::from_instances(instances.release()), options)
                                    .geometry;
  return std::move(result_geo.get_curves_for_write()->geometry.wrap());
}

static void generate_curves(GreasePencilMultiModifierData &mmd,
                            const ModifierEvalContext &ctx,
                            Drawing &drawing)
{
  bke::CurvesGeometry &curves = drawing.strokes_for_write();

  IndexMaskMemory mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, curves, mmd.influence, mask_memory);

  if (curves_mask.is_empty()) {
    return;
  }

  const IndexMask unselected_mask = curves_mask.complement(curves.curves_range(), mask_memory);

  int src_point_count, src_curve_count;
  curves = duplicate_strokes(
      curves, curves_mask, unselected_mask, mmd.duplications, src_point_count, src_curve_count);

  const float offset = math::length(math::to_scale(ctx.object->object_to_world())) * mmd.offset;
  const float distance = mmd.distance;
  const bool use_fading = (mmd.flag & MOD_GREASE_PENCIL_MULTIPLY_ENABLE_FADING) != 0;
  const float fading_thickness = mmd.fading_thickness;
  const float fading_opacity = mmd.fading_opacity;
  const float fading_center = mmd.fading_center;

  MutableSpan<float3> positions = curves.positions_for_write();
  const Span<float3> tangents = curves.evaluated_tangents();
  const Span<float3> normals = drawing.curve_plane_normals();

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> opacities = attributes.lookup_or_add_for_write_span<float>(
      "opacity", bke::AttrDomain::Point);
  bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_span<float>(
      "radius", bke::AttrDomain::Point);

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();

  Array<float3> pos_l(src_point_count);
  Array<float3> pos_r(src_point_count);

  int src_point_i = 0;
  for (const int src_curve_i : IndexRange(src_curve_count)) {
    for (const int point : points_by_curve[src_curve_i]) {
      const float3 miter = math::cross(normals[src_curve_i], tangents[point]) * distance;
      pos_l[src_point_i] = positions[point] + miter;
      pos_r[src_point_i] = positions[point] - miter;
      src_point_i++;
    }
  }

  const Span<float3> stroke_pos_l = pos_l.as_span();
  const Span<float3> stroke_pos_r = pos_r.as_span();

  for (const int i : IndexRange(mmd.duplications)) {
    using bke::attribute_math::mix2;
    const IndexRange stroke = IndexRange(src_point_count * i, src_point_count);
    MutableSpan<float3> instance_positions = positions.slice(stroke);
    MutableSpan<float> instance_radii = radii.span.slice(stroke);
    const float offset_fac = (mmd.duplications == 1) ?
                                 0.5f :
                                 (1.0f - (float(i) / float(mmd.duplications - 1)));
    const float fading_fac = fabsf(offset_fac - fading_center);
    const float thickness_factor = use_fading ? mix2(fading_fac, 1.0f, 1.0f - fading_thickness) :
                                                1.0f;
    threading::parallel_for(instance_positions.index_range(), 512, [&](const IndexRange range) {
      for (const int point : range) {
        const float fac = mix2(float(i) / float(mmd.duplications - 1), 1 + offset, offset);
        const int old_point = point % src_point_count;
        instance_positions[point] = mix2(fac, stroke_pos_l[old_point], stroke_pos_r[old_point]);
        instance_radii[point] *= thickness_factor;
      }
    });

    if (opacities) {
      MutableSpan<float> instance_opacity = opacities.span.slice(stroke);
      const float opacity_factor = use_fading ? mix2(fading_fac, 1.0f, 1.0f - fading_opacity) :
                                                1.0f;
      threading::parallel_for(instance_positions.index_range(), 512, [&](const IndexRange range) {
        for (const int point : range) {
          instance_opacity[point] *= opacity_factor;
        }
      });
    }
  }

  radii.finish();
  opacities.finish();

  drawing.tag_topology_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  auto *mmd = reinterpret_cast<GreasePencilMultiModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, mmd->influence, memory);
  const Vector<Drawing *> drawings = modifier::greasepencil::get_drawings_for_write(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(drawings,
                               [&](Drawing *drawing) { generate_curves(*mmd, *ctx, *drawing); });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "duplicates", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  uiLayout *col = &layout->column(false);
  col->active_set(RNA_int_get(ptr, "duplicates") > 0);
  col->prop(ptr, "distance", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "offset", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  PanelLayout fade_panel_layout = layout->panel_prop_with_bool_header(
      C, ptr, "open_fading_panel", ptr, "use_fade", IFACE_("Fade"));
  if (uiLayout *fade_panel = fade_panel_layout.body) {
    uiLayout *sub = &fade_panel->column(false);
    sub->active_set(RNA_boolean_get(ptr, "use_fade"));

    sub->prop(ptr, "fading_center", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    sub->prop(ptr, "fading_thickness", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
    sub->prop(ptr, "fading_opacity", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  }

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
  modifier_panel_register(region_type, eModifierType_GreasePencilMultiply, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *mmd = reinterpret_cast<const GreasePencilMultiModifierData *>(md);

  BLO_write_struct(writer, GreasePencilMultiModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilMultiModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilMultiply = {
    /*idname*/ "GreasePencilMultiply",
    /*name*/ N_("Multiple Strokes"),
    /*struct_name*/ "GreasePencilMultiModifierData",
    /*struct_size*/ sizeof(GreasePencilMultiModifierData),
    /*srna*/ &RNA_GreasePencilMultiplyModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_CURVE,

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
