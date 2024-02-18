/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_hash.h"
#include "BLI_math_matrix.hh"
#include "BLI_rand.h"

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.h"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "MOD_grease_pencil_util.hh"
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *omd = reinterpret_cast<GreasePencilOffsetModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(omd, modifier));

  MEMCPY_STRUCT_AFTER(omd, DNA_struct_default_get(GreasePencilOffsetModifierData), modifier);
  modifier::greasepencil::init_influence_data(&omd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *omd = reinterpret_cast<const GreasePencilOffsetModifierData *>(md);
  auto *tomd = reinterpret_cast<GreasePencilOffsetModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tomd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&omd->influence, &tomd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *omd = reinterpret_cast<GreasePencilOffsetModifierData *>(md);
  modifier::greasepencil::free_influence_data(&omd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *omd = reinterpret_cast<GreasePencilOffsetModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&omd->influence, ob, walk, user_data);
}

static void update_depsgraph(ModifierData * /*md*/, const ModifierUpdateDepsgraphContext *ctx)
{
  DEG_add_object_relation(
      ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Grease Pencil Offset Modifier");
}

static void apply_stroke_transform(const GreasePencilOffsetModifierData &omd,
                                   const VArray<float> &weights,
                                   const IndexRange &points,
                                   const float3 &loc_factor,
                                   const float3 &rot_factor,
                                   const float3 &scale_factor,
                                   const MutableSpan<float3> positions,
                                   const MutableSpan<float> radii)
{
  const bool has_global_offset = !(math::is_zero(float3(omd.loc)) &&
                                   math::is_zero(float3(omd.rot)) &&
                                   math::is_zero(float3(omd.scale)));
  const bool has_stroke_offset = !(math::is_zero(float3(omd.stroke_loc)) &&
                                   math::is_zero(float3(omd.stroke_rot)) &&
                                   math::is_zero(float3(omd.stroke_scale)));

  for (const int64_t i : points) {
    const float weight = weights[i];
    float3 &pos = positions[i];
    float &radius = radii[i];

    /* Add per-stroke offset. */
    if (has_stroke_offset) {
      const float4x4 matrix = math::from_loc_rot_scale<float4x4>(
          omd.stroke_loc * loc_factor * weight,
          omd.stroke_rot * rot_factor * weight,
          float3(1.0f) + omd.stroke_scale * scale_factor * weight);
      pos = math::transform_point(matrix, pos);
    }
    /* Add global offset. */
    if (has_global_offset) {
      const float3 scale = float3(1.0f) + float3(omd.scale) * weight;
      const float4x4 matrix = math::from_loc_rot_scale<float4x4>(
          float3(omd.loc) * weight, float3(omd.rot) * weight, scale);
      pos = math::transform_point(matrix, pos);

      /* Apply scale to thickness. */
      const float unit_scale = (math::abs(scale.x) + math::abs(scale.y) + math::abs(scale.z)) /
                               3.0f;
      radius *= unit_scale;
    }
  }
}

/** Randomized offset per stroke. */
static void modify_stroke_random(const Object &ob,
                                 const GreasePencilOffsetModifierData &omd,
                                 const IndexMask &curves_mask,
                                 bke::CurvesGeometry &curves)
{
  const bool use_uniform_scale = (omd.flag & MOD_GREASE_PENCIL_OFFSET_UNIFORM_RANDOM_SCALE);

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_span<float>(
      "radius", bke::AttrDomain::Point);
  const MutableSpan<float3> positions = curves.positions_for_write();
  const VArray<float> vgroup_weights = modifier::greasepencil::get_influence_vertex_weights(
      curves, omd.influence);

  /* Make sure different modifiers get different seeds. */
  const int seed = omd.seed + BLI_hash_string(ob.id.name + 2) + BLI_hash_string(omd.modifier.name);
  const float rand_offset = BLI_hash_int_01(seed);

  /* Generates a random number for loc/rot/scale channels, based on seed and a per-stroke random
   * value r. */
  auto get_random_channel = [&](const char channel, const double r) {
    float rand = fmodf(r * 2.0f - 1.0f + rand_offset, 1.0f);
    return fmodf(sin(rand * 12.9898f + channel * 78.233f) * 43758.5453f, 1.0f);
  };

  auto get_random_value = [&](const char channel, const int64_t curve_i) {
    const uint halton_primes[3] = {2, 3, 7};
    double halton_offset[3] = {0.0f, 0.0f, 0.0f};
    double r[3];
    /* To ensure a nice distribution, we use halton sequence and offset using the curve index. */
    BLI_halton_3d(halton_primes, halton_offset, curve_i, r);
    return get_random_channel(channel, r[0]);
  };

  auto get_random_vector = [&](const char channel, const int64_t curve_i) {
    const uint halton_primes[3] = {2, 3, 7};
    double halton_offset[3] = {0.0f, 0.0f, 0.0f};
    double r[3];
    /* To ensure a nice distribution, we use halton sequence and offset using the curve index. */
    BLI_halton_3d(halton_primes, halton_offset, curve_i, r);
    return float3(get_random_channel(channel, r[0]),
                  get_random_channel(channel, r[1]),
                  get_random_channel(channel, r[2]));
  };

  curves_mask.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    const IndexRange points = points_by_curve[curve_i];

    /* Randomness factors for loc/rot/scale per curve. */
    const float3 loc_factor = get_random_vector(0, curve_i);
    const float3 rot_factor = get_random_vector(1, curve_i);
    const float3 scale_factor = use_uniform_scale ? float3(get_random_value(2, curve_i)) :
                                                    get_random_vector(2, curve_i);

    apply_stroke_transform(
        omd, vgroup_weights, points, loc_factor, rot_factor, scale_factor, positions, radii.span);
  });

  radii.finish();
}

/* This is a very weird/broken formula, but kept for compatibility. */
static float get_factor_from_index(const GreasePencilOffsetModifierData &omd,
                                   const int size,
                                   const int index)
{
  const int step = max_ii(omd.stroke_step, 1);
  const int start_offset = omd.stroke_start_offset;
  return ((size - (index / step + start_offset % size) % size * step % size) - 1) / float(size);
}

/** Offset proportional to stroke index. */
static void modify_stroke_by_index(const GreasePencilOffsetModifierData &omd,
                                   const IndexMask &curves_mask,
                                   bke::CurvesGeometry &curves)
{
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_span<float>(
      "radius", bke::AttrDomain::Point);
  const MutableSpan<float3> positions = curves.positions_for_write();
  const VArray<float> vgroup_weights = modifier::greasepencil::get_influence_vertex_weights(
      curves, omd.influence);

  curves_mask.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    const float factor = get_factor_from_index(omd, curves.curves_num(), curve_i);
    apply_stroke_transform(omd,
                           vgroup_weights,
                           points,
                           float3(factor),
                           float3(factor),
                           float3(factor),
                           positions,
                           radii.span);
  });

  radii.finish();
}

/** Offset proportional to material index. */
static void modify_stroke_by_material(const Object &ob,
                                      const GreasePencilOffsetModifierData &omd,
                                      const IndexMask &curves_mask,
                                      bke::CurvesGeometry &curves)
{
  const short *totcolp = BKE_object_material_len_p(const_cast<Object *>(&ob));
  const short totcol = totcolp ? *totcolp : 0;

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_span<float>(
      "radius", bke::AttrDomain::Point);
  const MutableSpan<float3> positions = curves.positions_for_write();
  const VArray<float> vgroup_weights = modifier::greasepencil::get_influence_vertex_weights(
      curves, omd.influence);
  const VArray<int> stroke_materials = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Curve, 0);

  curves_mask.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    const float factor = get_factor_from_index(omd, totcol, stroke_materials[curve_i]);
    apply_stroke_transform(omd,
                           vgroup_weights,
                           points,
                           float3(factor),
                           float3(factor),
                           float3(factor),
                           positions,
                           radii.span);
  });

  radii.finish();
}

/** Offset proportional to layer index. */
static void modify_stroke_by_layer(const GreasePencilOffsetModifierData &omd,
                                   const int layer_index,
                                   const int layers_num,
                                   const IndexMask &curves_mask,
                                   bke::CurvesGeometry &curves)
{
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_span<float>(
      "radius", bke::AttrDomain::Point);
  const MutableSpan<float3> positions = curves.positions_for_write();
  const VArray<float> vgroup_weights = modifier::greasepencil::get_influence_vertex_weights(
      curves, omd.influence);

  const float factor = get_factor_from_index(omd, layers_num, layer_index);

  curves_mask.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    apply_stroke_transform(omd,
                           vgroup_weights,
                           points,
                           float3(factor),
                           float3(factor),
                           float3(factor),
                           positions,
                           radii.span);
  });

  radii.finish();
}

static void modify_drawing(const ModifierData &md,
                           const ModifierEvalContext &ctx,
                           bke::greasepencil::Drawing &drawing)
{
  const auto &omd = reinterpret_cast<const GreasePencilOffsetModifierData &>(md);

  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  IndexMaskMemory mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, curves, omd.influence, mask_memory);

  switch (omd.offset_mode) {
    case MOD_GREASE_PENCIL_OFFSET_RANDOM:
      modify_stroke_random(*ctx.object, omd, curves_mask, curves);
      break;
    case MOD_GREASE_PENCIL_OFFSET_MATERIAL:
      modify_stroke_by_material(*ctx.object, omd, curves_mask, curves);
      break;
    case MOD_GREASE_PENCIL_OFFSET_STROKE:
      modify_stroke_by_index(omd, curves_mask, curves);
      break;
    case MOD_GREASE_PENCIL_OFFSET_LAYER:
      BLI_assert_unreachable();
      break;
  }
}

static void modify_drawing_by_layer(const ModifierData &md,
                                    const ModifierEvalContext &ctx,
                                    bke::greasepencil::Drawing &drawing,
                                    int layer_index,
                                    int layers_num)
{
  const auto &omd = reinterpret_cast<const GreasePencilOffsetModifierData &>(md);

  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  IndexMaskMemory mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, curves, omd.influence, mask_memory);

  switch (omd.offset_mode) {
    case MOD_GREASE_PENCIL_OFFSET_LAYER:
      modify_stroke_by_layer(omd, layer_index, layers_num, curves_mask, curves);
      break;
    case MOD_GREASE_PENCIL_OFFSET_RANDOM:
    case MOD_GREASE_PENCIL_OFFSET_MATERIAL:
    case MOD_GREASE_PENCIL_OFFSET_STROKE:
      BLI_assert_unreachable();
      break;
  }
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  using bke::greasepencil::Drawing;
  using modifier::greasepencil::LayerDrawingInfo;

  auto *omd = reinterpret_cast<GreasePencilOffsetModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, omd->influence, mask_memory);

  if (omd->offset_mode == MOD_GREASE_PENCIL_OFFSET_LAYER) {
    const Vector<LayerDrawingInfo> drawings = modifier::greasepencil::get_drawing_infos_by_layer(
        grease_pencil, layer_mask, frame);
    threading::parallel_for_each(drawings, [&](const LayerDrawingInfo &info) {
      modify_drawing_by_layer(
          *md, *ctx, *info.drawing, info.layer_index, grease_pencil.layers().size());
    });
  }
  else {
    const Vector<Drawing *> drawings = modifier::greasepencil::get_drawings_for_write(
        grease_pencil, layer_mask, frame);
    threading::parallel_for_each(drawings,
                                 [&](Drawing *drawing) { modify_drawing(*md, *ctx, *drawing); });
  }
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);
  const auto offset_mode = GreasePencilOffsetModifierMode(RNA_enum_get(ptr, "offset_mode"));

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "location", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "rotation", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "scale", UI_ITEM_NONE, nullptr, ICON_NONE);

  LayoutPanelState *advanced_panel_state = BKE_panel_layout_panel_state_ensure(
      panel, "advanced", true);
  PointerRNA advanced_state_ptr = RNA_pointer_create(
      nullptr, &RNA_LayoutPanelState, advanced_panel_state);
  if (uiLayout *advanced_panel = uiLayoutPanelProp(
          C, layout, &advanced_state_ptr, "is_open", "Advanced"))
  {
    uiItemR(advanced_panel, ptr, "offset_mode", UI_ITEM_NONE, nullptr, ICON_NONE);

    uiItemR(advanced_panel, ptr, "stroke_location", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
    uiItemR(advanced_panel, ptr, "stroke_rotation", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
    uiItemR(advanced_panel, ptr, "stroke_scale", UI_ITEM_NONE, IFACE_("Scale"), ICON_NONE);

    uiLayout *col = uiLayoutColumn(advanced_panel, true);
    switch (offset_mode) {
      case MOD_GREASE_PENCIL_OFFSET_RANDOM:
        uiItemR(advanced_panel, ptr, "use_uniform_random_scale", UI_ITEM_NONE, nullptr, ICON_NONE);
        uiItemR(advanced_panel, ptr, "seed", UI_ITEM_NONE, nullptr, ICON_NONE);
        break;
      case MOD_GREASE_PENCIL_OFFSET_STROKE:
        uiItemR(col, ptr, "stroke_step", UI_ITEM_NONE, IFACE_("Stroke Step"), ICON_NONE);
        uiItemR(col, ptr, "stroke_start_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
        break;
      case MOD_GREASE_PENCIL_OFFSET_MATERIAL:
        uiItemR(col, ptr, "stroke_step", UI_ITEM_NONE, IFACE_("Material Step"), ICON_NONE);
        uiItemR(col, ptr, "stroke_start_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
        break;
      case MOD_GREASE_PENCIL_OFFSET_LAYER:
        uiItemR(col, ptr, "stroke_step", UI_ITEM_NONE, IFACE_("Layer Step"), ICON_NONE);
        uiItemR(col, ptr, "stroke_start_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
        break;
    }
  }

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
  modifier_panel_register(region_type, eModifierType_GreasePencilOffset, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *omd = reinterpret_cast<const GreasePencilOffsetModifierData *>(md);

  BLO_write_struct(writer, GreasePencilOffsetModifierData, omd);
  modifier::greasepencil::write_influence_data(writer, &omd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *omd = reinterpret_cast<GreasePencilOffsetModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &omd->influence);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilOffset = {
    /*idname*/ "GreasePencilOffset",
    /*name*/ N_("Offset"),
    /*struct_name*/ "GreasePencilOffsetModifierData",
    /*struct_size*/ sizeof(GreasePencilOffsetModifierData),
    /*srna*/ &RNA_GreasePencilOffsetModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_OFFSET,

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
