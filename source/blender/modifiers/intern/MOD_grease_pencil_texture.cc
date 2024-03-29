/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BKE_attribute.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_span.hh"

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_query.hh"

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
  auto *tmd = reinterpret_cast<GreasePencilTextureModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(tmd, modifier));

  MEMCPY_STRUCT_AFTER(tmd, DNA_struct_default_get(GreasePencilTextureModifierData), modifier);
  modifier::greasepencil::init_influence_data(&tmd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *tmd = reinterpret_cast<const GreasePencilTextureModifierData *>(md);
  auto *tmmd = reinterpret_cast<GreasePencilTextureModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tmmd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&tmd->influence, &tmmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *tmd = reinterpret_cast<GreasePencilTextureModifierData *>(md);
  modifier::greasepencil::free_influence_data(&tmd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *tmd = reinterpret_cast<GreasePencilTextureModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&tmd->influence, ob, walk, user_data);
}

static void write_stroke_transforms(bke::greasepencil::Drawing &drawing,
                                    const IndexMask &curves_mask,
                                    const float offset,
                                    const float rotation,
                                    const float scale,
                                    const bool normalize_u)
{
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> u_translations = attributes.lookup_or_add_for_write_span<float>(
      "u_translation", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter<float> rotations = attributes.lookup_or_add_for_write_span<float>(
      "rotation", bke::AttrDomain::Point);
  bke::SpanAttributeWriter<float> u_scales = attributes.lookup_or_add_for_write_span<float>(
      "u_scale",
      bke::AttrDomain::Curve,
      bke::AttributeInitVArray(VArray<float>::ForSingle(1.0f, curves.curves_num())));

  curves.ensure_evaluated_lengths();

  curves_mask.foreach_index(GrainSize(512), [&](int64_t curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    const bool is_cyclic = cyclic[curve_i];
    const Span<float> lengths = curves.evaluated_lengths_for_curve(curve_i, is_cyclic);
    const float norm = normalize_u ? math::safe_rcp(lengths.last()) : 1.0f;

    u_translations.span[curve_i] += offset;
    u_scales.span[curve_i] *= scale * norm;
    for (const int point_i : points) {
      rotations.span[point_i] += rotation;
    }
  });

  u_translations.finish();
  u_scales.finish();
  rotations.finish();
}

static float2 rotate_by_angle(const float2 &p, const float angle)
{
  const float cos_angle = math::cos(angle);
  const float sin_angle = math::sin(angle);
  return float2(p.x * cos_angle - p.y * sin_angle, p.x * sin_angle + p.y * cos_angle);
}

/*
 * This gets the legacy stroke-space to layer-space matrix.
 */
static void get_legacy_stroke_matrix(const Span<float3> positions,
                                     float3x4 &stroke_to_layer,
                                     float4x3 &layer_to_stroke)
{
  using namespace blender;
  using namespace blender::math;

  if (positions.size() < 2) {
    stroke_to_layer = float3x4::identity();
    layer_to_stroke = float4x3::identity();
  }

  const float3 &pt0 = positions[0];
  const float3 &pt1 = positions[1];
  const float3 &pt3 = positions[int(positions.size() * 0.75f)];

  /* Local X axis (p0 -> p1) */
  const float3 local_x = normalize(pt1 - pt0);

  /* Point vector at 3/4 */
  const float3 local_3 = (positions.size() == 2) ? (pt3 * 0.001f) - pt0 : pt3 - pt0;

  /* Vector orthogonal to polygon plane. */
  const float3 normal = cross(local_x, local_3);

  /* Local Y axis (cross to normal/x axis). */
  const float3 local_y = normalize(cross(normal, local_x));

  /* Get layer space using first point as origin. */
  stroke_to_layer = float3x4(float4(local_x, 0), float4(local_y, 0), float4(pt0, 1));
  layer_to_stroke = math::transpose(float3x4(float4(local_x, -dot(pt0, local_x)),
                                             float4(local_y, -dot(pt0, local_y)),
                                             float4(0, 0, 0, 1)));
}

static void write_fill_transforms(bke::greasepencil::Drawing &drawing,
                                  const IndexMask &curves_mask,
                                  const float2 &offset,
                                  const float rotation,
                                  const float scale)
{
  /* Texture matrices are a combination of an unknown 3D transform into UV space, with a known 2D
   * transform on top.
   *
   * However, the modifier offset is not applied directly to the UV transform, since it emulates
   * legacy behavior of the GPv2 modifier, which applied translation first, before rotating about
   * (0.5, 0.5) and scaling. To achieve the same result as the legacy modifier, the actual offset
   * is calculated such that the result matches the GPv2 behavior.
   *
   * The canonical transform is
   *   uv = T + R / S * xy
   *
   * In terms of legacy variables TL, RL, SL the same transform is described as
   *   uv = (RL * (xy / 2 + TL) + 1/2) / SL
   *
   * where the 1/2 scaling factor and offset are the "bounds" transform and rotation center.
   *
   * Rearranging into canonical loc/rot/scale terms:
   *   uv = (RL * TL + 1/2) / SL + 1/2 * RL / SL * xy
   * <=>
   *    T = (RL * TL + 1/2) / SL
   *    R = RL
   *    S = 2*SL
   * <=>
   *    TL = 1/2 * R^T * (T * S - 1)
   *    RL = R
   *    SL = S/2
   */

  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  const Span<float3> positions = curves.positions();
  Array<float4x2> texture_matrices(drawing.texture_matrices());

  curves_mask.foreach_index(GrainSize(512), [&](int64_t curve_i) {
    const IndexRange points = curves.points_by_curve()[curve_i];
    float4x2 &texture_matrix = texture_matrices[curve_i];
    /* Factor out the stroke-to-layer transform part used by GPv2.
     * This may not be the same as the transform used by GPv3 for concave shapes due to a
     * simplistic normal calculation, but we want to achieve the same effect as GPv2 so have to use
     * the same matrix. */
    float3x4 stroke_to_layer;
    float4x3 layer_to_stroke;
    get_legacy_stroke_matrix(positions.slice(points), stroke_to_layer, layer_to_stroke);
    const float3x2 uv_matrix = texture_matrix * stroke_to_layer;
    const float2 uv_translation = uv_matrix[2];
    float2 inv_uv_scale;
    const float2 axis_u = math::normalize_and_get_length(uv_matrix[0], inv_uv_scale[0]);
    const float2 axis_v = math::normalize_and_get_length(uv_matrix[1], inv_uv_scale[1]);
    const float uv_rotation = math::atan2(axis_u[1], axis_u[0]);
    const float2 uv_scale = math::safe_rcp(inv_uv_scale);

    const float2 legacy_uv_translation = rotate_by_angle(0.5f * uv_scale * uv_translation - 0.5f,
                                                         -uv_rotation);
    const float legacy_uv_rotation = uv_rotation;
    const float2 legacy_uv_scale = 0.5f * uv_scale;

    const float2 legacy_uv_translation_new = legacy_uv_translation + offset;
    const float legacy_uv_rotation_new = legacy_uv_rotation + rotation;
    const float2 legacy_uv_scale_new = legacy_uv_scale * scale;

    const float2 uv_translation_new =
        (rotate_by_angle(legacy_uv_translation_new, legacy_uv_rotation_new) + 0.5f) *
        math::safe_rcp(legacy_uv_scale_new);
    const float uv_rotation_new = legacy_uv_rotation_new;
    const float2 uv_scale_new = 2.0f * legacy_uv_scale_new;

    const float cos_uv_rotation_new = math::cos(uv_rotation_new);
    const float sin_uv_rotation_new = math::sin(uv_rotation_new);
    const float2 inv_uv_scale_new = math::safe_rcp(uv_scale_new);
    const float3x2 uv_matrix_new = float3x2(
        inv_uv_scale_new[0] * float2(cos_uv_rotation_new, sin_uv_rotation_new),
        inv_uv_scale_new[1] * float2(-sin_uv_rotation_new, cos_uv_rotation_new),
        uv_translation_new);
    texture_matrix = uv_matrix_new * layer_to_stroke;
  });

  drawing.set_texture_matrices(texture_matrices, curves_mask);
}

static void modify_curves(const GreasePencilTextureModifierData &tmd,
                          const ModifierEvalContext &ctx,
                          bke::greasepencil::Drawing &drawing)
{
  IndexMaskMemory mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, drawing.strokes(), tmd.influence, mask_memory);

  const bool normalize_u = (tmd.fit_method == MOD_GREASE_PENCIL_TEXTURE_FIT_STROKE);
  switch (GreasePencilTextureModifierMode(tmd.mode)) {
    case MOD_GREASE_PENCIL_TEXTURE_STROKE:
      write_stroke_transforms(
          drawing, curves_mask, tmd.uv_offset, tmd.alignment_rotation, tmd.uv_scale, normalize_u);
      break;
    case MOD_GREASE_PENCIL_TEXTURE_FILL:
      write_fill_transforms(
          drawing, curves_mask, tmd.fill_offset, tmd.fill_rotation, tmd.fill_scale);
      break;
    case MOD_GREASE_PENCIL_TEXTURE_STROKE_AND_FILL:
      write_stroke_transforms(
          drawing, curves_mask, tmd.uv_offset, tmd.alignment_rotation, tmd.uv_scale, normalize_u);
      write_fill_transforms(
          drawing, curves_mask, tmd.fill_offset, tmd.fill_rotation, tmd.fill_scale);
      break;
  }
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  using bke::greasepencil::Drawing;
  using bke::greasepencil::Layer;

  const auto &tmd = *reinterpret_cast<const GreasePencilTextureModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, tmd.influence, mask_memory);
  const int frame = grease_pencil.runtime->eval_frame;
  const Vector<Drawing *> drawings = modifier::greasepencil::get_drawings_for_write(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(drawings,
                               [&](Drawing *drawing) { modify_curves(tmd, *ctx, *drawing); });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);
  const auto &tmd = *static_cast<GreasePencilTextureModifierData *>(ptr->data);
  const auto mode = GreasePencilTextureModifierMode(tmd.mode);
  uiLayout *col;

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (ELEM(mode, MOD_GREASE_PENCIL_TEXTURE_STROKE, MOD_GREASE_PENCIL_TEXTURE_STROKE_AND_FILL)) {
    col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "fit_method", UI_ITEM_NONE, IFACE_("Stroke Fit Method"), ICON_NONE);
    uiItemR(col, ptr, "uv_offset", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "alignment_rotation", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "uv_scale", UI_ITEM_NONE, IFACE_("Scale"), ICON_NONE);
  }

  if (mode == MOD_GREASE_PENCIL_TEXTURE_STROKE_AND_FILL) {
    uiItemS(layout);
  }

  if (ELEM(mode, MOD_GREASE_PENCIL_TEXTURE_FILL, MOD_GREASE_PENCIL_TEXTURE_STROKE_AND_FILL)) {
    col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "fill_rotation", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "fill_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
    uiItemR(col, ptr, "fill_scale", UI_ITEM_NONE, IFACE_("Scale"), ICON_NONE);
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
  modifier_panel_register(region_type, eModifierType_GreasePencilTexture, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *tmd = reinterpret_cast<const GreasePencilTextureModifierData *>(md);

  BLO_write_struct(writer, GreasePencilTextureModifierData, tmd);
  modifier::greasepencil::write_influence_data(writer, &tmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *tmd = reinterpret_cast<GreasePencilTextureModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &tmd->influence);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilTexture = {
    /*idname*/ "GreasePencilTexture",
    /*name*/ N_("TimeOffset"),
    /*struct_name*/ "GreasePencilTextureModifierData",
    /*struct_size*/ sizeof(GreasePencilTextureModifierData),
    /*srna*/ &RNA_GreasePencilTextureModifier,
    /*type*/ ModifierTypeType::NonGeometrical,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_UVPROJECT,

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
