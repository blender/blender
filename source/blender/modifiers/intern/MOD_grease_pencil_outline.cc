/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BKE_attribute.hh"
#include "BKE_material.hh"
#include "BLI_array_utils.hh"
#include "BLI_index_range.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_virtual_array.hh"

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_grease_pencil.hh"

#include "GEO_resample_curves.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_ui_common.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *omd = reinterpret_cast<GreasePencilOutlineModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(omd, modifier));

  MEMCPY_STRUCT_AFTER(omd, DNA_struct_default_get(GreasePencilOutlineModifierData), modifier);
  modifier::greasepencil::init_influence_data(&omd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *omd = reinterpret_cast<const GreasePencilOutlineModifierData *>(md);
  auto *tmmd = reinterpret_cast<GreasePencilOutlineModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tmmd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&omd->influence, &tmmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *omd = reinterpret_cast<GreasePencilOutlineModifierData *>(md);
  modifier::greasepencil::free_influence_data(&omd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *omd = reinterpret_cast<GreasePencilOutlineModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&omd->influence, ob, walk, user_data);
  walk(user_data, ob, (ID **)&omd->outline_material, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&omd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  auto *omd = reinterpret_cast<GreasePencilOutlineModifierData *>(md);
  if (ctx->scene->camera) {
    DEG_add_object_relation(
        ctx->node, ctx->scene->camera, DEG_OB_COMP_TRANSFORM, "Grease Pencil Outline Modifier");
    DEG_add_object_relation(
        ctx->node, ctx->scene->camera, DEG_OB_COMP_PARAMETERS, "Grease Pencil Outline Modifier");
  }
  if (omd->object != nullptr) {
    DEG_add_object_relation(
        ctx->node, omd->object, DEG_OB_COMP_TRANSFORM, "Grease Pencil Outline Modifier");
  }
  DEG_add_object_relation(
      ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Grease Pencil Outline Modifier");
}

/**
 * Rearrange curve buffers by moving points from the start to the back of each stroke.
 * \note This is an optional feature. The offset is determine by the closest point to an object.
 * \param curve_offsets: Offset of each curve, indicating the point that becomes the new start.
 */
static bke::CurvesGeometry reorder_cyclic_curve_points(const bke::CurvesGeometry &src_curves,
                                                       const IndexMask &curve_selection,
                                                       const Span<int> curve_offsets)
{
  BLI_assert(curve_offsets.size() == src_curves.curves_num());

  OffsetIndices<int> src_offsets = src_curves.points_by_curve();
  bke::AttributeAccessor src_attributes = src_curves.attributes();

  Array<int> indices(src_curves.points_num());
  curve_selection.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    const IndexRange points = src_offsets[curve_i];
    const int point_num = points.size();
    const int point_start = points.start();
    MutableSpan<int> point_indices = indices.as_mutable_span().slice(points);
    if (points.size() < 2) {
      array_utils::fill_index_range(point_indices, point_start);
      return;
    }
    /* Offset can be negative or larger than the buffer. Use modulo to get an
     * equivalent offset within buffer size to simplify copying. */
    const int offset_raw = curve_offsets[curve_i];
    const int offset = offset_raw >= 0 ? offset_raw % points.size() :
                                         points.size() - ((-offset_raw) % points.size());
    BLI_assert(0 <= offset && offset < points.size());
    if (offset == 0) {
      array_utils::fill_index_range(point_indices, point_start);
      return;
    }

    const int point_middle = point_start + offset;
    array_utils::fill_index_range(point_indices.take_front(point_num - offset), point_middle);
    array_utils::fill_index_range(point_indices.take_back(offset), point_start);
  });

  /* Have to make a copy of the input geometry, gather_attributes does not work in-place when the
   * source indices are not ordered. */
  bke::CurvesGeometry dst_curves(src_curves);
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  bke::gather_attributes(
      src_attributes, bke::AttrDomain::Point, bke::AttrDomain::Point, {}, indices, dst_attributes);

  return dst_curves;
}

static int find_closest_point(const Span<float3> positions, const float3 &target)
{
  if (positions.is_empty()) {
    return 0;
  }

  int closest_i = 0;
  float min_dist_squared = math::distance_squared(positions.first(), target);
  for (const int i : positions.index_range().drop_front(1)) {
    const float dist_squared = math::distance_squared(positions[i], target);
    if (dist_squared < min_dist_squared) {
      closest_i = i;
      min_dist_squared = dist_squared;
    }
  }
  return closest_i;
}

static void modify_drawing(const GreasePencilOutlineModifierData &omd,
                           const ModifierEvalContext &ctx,
                           bke::greasepencil::Drawing &drawing,
                           const float4x4 &viewmat)
{
  modifier::greasepencil::ensure_no_bezier_curves(drawing);

  if (drawing.strokes().curve_num == 0) {
    return;
  }

  /* Selected source curves. */
  IndexMaskMemory curve_mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, drawing.strokes(), omd.influence, curve_mask_memory);

  /* Unit object scale is applied to the stroke radius. */
  const float object_scale = math::length(
      math::transform_direction(ctx.object->object_to_world(), float3(M_SQRT1_3)));
  /* Legacy thickness setting is diameter in pixels, divide by 2000 to get radius. */
  const float radius = math::max(omd.thickness * object_scale, 1.0f) *
                       bke::greasepencil::LEGACY_RADIUS_CONVERSION_FACTOR;
  /* Offset the strokes by the radius so the outside aligns with the input stroke. */
  const float outline_offset = (omd.flag & MOD_GREASE_PENCIL_OUTLINE_KEEP_SHAPE) != 0 ? -radius :
                                                                                        0.0f;
  const int mat_nr = (omd.outline_material ?
                          BKE_object_material_index_get(ctx.object, omd.outline_material) :
                          -1);

  bke::CurvesGeometry curves = ed::greasepencil::create_curves_outline(
      drawing, curves_mask, viewmat, omd.subdiv, radius, outline_offset, mat_nr);

  /* Cyclic curve reordering feature. */
  if (omd.object) {
    const OffsetIndices points_by_curve = curves.points_by_curve();

    /* Computes the offset of the closest point to the object from the curve start. */
    Array<int> offset_by_curve(curves.curves_num());
    for (const int i : curves.curves_range()) {
      const IndexRange points = points_by_curve[i];
      /* Closest point index is already relative to the point range and can be used as offset. */
      offset_by_curve[i] = find_closest_point(curves.positions().slice(points), omd.object->loc);
    }

    curves = reorder_cyclic_curve_points(curves, curves.curves_range(), offset_by_curve);
  }

  /* Resampling feature. */
  if (omd.sample_length > 0.0f) {
    VArray<float> sample_lengths = VArray<float>::from_single(omd.sample_length,
                                                              curves.curves_num());
    curves = geometry::resample_to_length(curves, curves.curves_range(), sample_lengths);
  }

  drawing.strokes_for_write() = std::move(curves);
  drawing.tag_topology_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  using bke::greasepencil::Drawing;
  using bke::greasepencil::Layer;
  using modifier::greasepencil::LayerDrawingInfo;

  const auto &omd = *reinterpret_cast<const GreasePencilOutlineModifierData *>(md);

  const Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  if (!scene->camera) {
    return;
  }
  const float4x4 viewinv = scene->camera->world_to_object();

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, omd.influence, mask_memory);

  const Vector<LayerDrawingInfo> drawings = modifier::greasepencil::get_drawing_infos_by_layer(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(drawings, [&](const LayerDrawingInfo &info) {
    const Layer &layer = grease_pencil.layer(info.layer_index);
    const float4x4 viewmat = viewinv * layer.to_world_space(*ctx->object);
    modify_drawing(omd, *ctx, *info.drawing, viewmat);
  });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "thickness", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "use_keep_shape", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "subdivision", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "sample_length", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "outline_material", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  Scene *scene = CTX_data_scene(C);
  if (scene->camera == nullptr) {
    layout->label(RPT_("Outline requires an active camera"), ICON_ERROR);
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
  modifier_panel_register(region_type, eModifierType_GreasePencilOutline, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *omd = reinterpret_cast<const GreasePencilOutlineModifierData *>(md);

  BLO_write_struct(writer, GreasePencilOutlineModifierData, omd);
  modifier::greasepencil::write_influence_data(writer, &omd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *omd = reinterpret_cast<GreasePencilOutlineModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &omd->influence);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilOutline = {
    /*idname*/ "GreasePencilOutline",
    /*name*/ N_("Outline"),
    /*struct_name*/ "GreasePencilOutlineModifierData",
    /*struct_size*/ sizeof(GreasePencilOutlineModifierData),
    /*srna*/ &RNA_GreasePencilOutlineModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_OUTLINE,

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
