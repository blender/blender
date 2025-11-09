/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_array_utils.hh"
#include "BLI_index_range.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_ui_common.hh"

#include "GEO_join_geometries.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *dmd = reinterpret_cast<GreasePencilDashModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(dmd, modifier));

  MEMCPY_STRUCT_AFTER(dmd, DNA_struct_default_get(GreasePencilDashModifierData), modifier);
  modifier::greasepencil::init_influence_data(&dmd->influence, false);

  GreasePencilDashModifierSegment *ds = DNA_struct_default_alloc(GreasePencilDashModifierSegment);
  STRNCPY_UTF8(ds->name, DATA_("Segment"));
  dmd->segments_array = ds;
  dmd->segments_num = 1;
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *dmd = reinterpret_cast<const GreasePencilDashModifierData *>(md);
  auto *tmmd = reinterpret_cast<GreasePencilDashModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tmmd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&dmd->influence, &tmmd->influence, flag);

  tmmd->segments_array = static_cast<GreasePencilDashModifierSegment *>(
      MEM_dupallocN(dmd->segments_array));
}

static void free_data(ModifierData *md)
{
  auto *dmd = reinterpret_cast<GreasePencilDashModifierData *>(md);
  modifier::greasepencil::free_influence_data(&dmd->influence);

  MEM_SAFE_FREE(dmd->segments_array);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *dmd = reinterpret_cast<GreasePencilDashModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&dmd->influence, ob, walk, user_data);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  const auto *dmd = reinterpret_cast<GreasePencilDashModifierData *>(md);
  /* Enable if at least one segment has non-zero length. */
  for (const GreasePencilDashModifierSegment &dash_segment : dmd->segments()) {
    if (dash_segment.dash + dash_segment.gap - 1 > 0) {
      return false;
    }
  }
  return true;
}

static int floored_modulo(const int a, const int b)
{
  return a - math::floor(float(a) / float(b)) * b;
}

/* Combined segment info used by all strokes. */
struct PatternInfo {
  int offset = 0;
  int length = 0;
  Array<IndexRange> segments;
  Array<bool> cyclic;
  Array<int> material;
  Array<float> radius;
  Array<float> opacity;
};

static PatternInfo get_pattern_info(const GreasePencilDashModifierData &dmd)
{
  PatternInfo info;
  for (const GreasePencilDashModifierSegment &dash_segment : dmd.segments()) {
    info.length += dash_segment.dash + dash_segment.gap;
  }

  info.segments.reinitialize(dmd.segments().size());
  info.cyclic.reinitialize(dmd.segments().size());
  info.material.reinitialize(dmd.segments().size());
  info.radius.reinitialize(dmd.segments().size());
  info.opacity.reinitialize(dmd.segments().size());
  info.offset = floored_modulo(dmd.dash_offset, info.length);

  /* Store segments as ranges. */
  IndexRange dash_range(0);
  IndexRange gap_range(0);
  for (const int i : dmd.segments().index_range()) {
    const GreasePencilDashModifierSegment &dash_segment = dmd.segments()[i];
    dash_range = gap_range.after(dash_segment.dash);
    gap_range = dash_range.after(dash_segment.gap);
    info.segments[i] = dash_range;
    info.cyclic[i] = dash_segment.flag & MOD_GREASE_PENCIL_DASH_USE_CYCLIC;
    info.material[i] = dash_segment.mat_nr;
    info.radius[i] = dash_segment.radius;
    info.opacity[i] = dash_segment.opacity;
  }
  return info;
}

/* Returns the segment covering the given index, including repetitions. */
static int find_dash_segment(const PatternInfo &pattern_info, const int index)
{
  const int repeat = index / pattern_info.length;
  const int segments_num = pattern_info.segments.size();

  const int local_index = index - repeat * pattern_info.length;
  for (const int i : pattern_info.segments.index_range().drop_back(1)) {
    const IndexRange segment = pattern_info.segments[i];
    const IndexRange next_segment = pattern_info.segments[i + 1];
    if (local_index >= segment.start() && local_index < next_segment.start()) {
      return i + repeat * segments_num;
    }
  }
  return segments_num - 1 + repeat * segments_num;
}

/**
 * Iterate over all dash curves.
 * \param fn: Function taking an index range of source points describing new curves.
 * \note Point range can be larger than the source point range in case of cyclic curves.
 */
static void foreach_dash(const PatternInfo &pattern_info,
                         const IndexRange src_points,
                         const bool cyclic,
                         FunctionRef<void(IndexRange, bool, int, float, float)> fn)
{
  const int points_num = src_points.size();
  const int segments_num = pattern_info.segments.size();

  const int first_segment = find_dash_segment(pattern_info, pattern_info.offset);
  const int last_segment = find_dash_segment(pattern_info, pattern_info.offset + points_num - 1);
  BLI_assert(first_segment < segments_num);
  BLI_assert(last_segment >= first_segment);

  const IndexRange all_segments = IndexRange(first_segment, last_segment - first_segment + 1);
  for (const int i : all_segments) {
    const int repeat = i / segments_num;
    const int segment_index = i - repeat * segments_num;
    const IndexRange range = pattern_info.segments[segment_index].shift(repeat *
                                                                        pattern_info.length);

    const int64_t point_shift = src_points.start() - pattern_info.offset;
    const int64_t min_point = src_points.start();
    const int64_t max_point = cyclic ? src_points.one_after_last() : src_points.last();
    const int64_t start = std::clamp(range.start() + point_shift, min_point, max_point);
    const int64_t end = std::clamp(range.one_after_last() + point_shift, min_point, max_point + 1);

    IndexRange points(start, end - start);
    if (!points.is_empty()) {
      fn(points,
         pattern_info.cyclic[segment_index],
         pattern_info.material[segment_index],
         pattern_info.radius[segment_index],
         pattern_info.opacity[segment_index]);
    }
  }
}

static bke::CurvesGeometry create_dashes(const PatternInfo &pattern_info,
                                         const bke::CurvesGeometry &src_curves,
                                         const IndexMask &curves_mask)
{
  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  const VArray<bool> src_cyclic = src_curves.cyclic();
  const VArray<int> src_material = *src_attributes.lookup_or_default(
      "material_index", bke::AttrDomain::Curve, 0);
  const VArray<float> src_radius = *src_attributes.lookup_or_default<float>(
      "radius", bke::AttrDomain::Point, 0.01f);
  const VArray<float> src_opacity = *src_attributes.lookup_or_default<float>(
      "opacity", bke::AttrDomain::Point, 1.0f);

  /* Count new curves and points. */
  int dst_point_num = 0;
  int dst_curve_num = 0;
  curves_mask.foreach_index([&](const int64_t src_curve_i) {
    const IndexRange src_points = src_curves.points_by_curve()[src_curve_i];

    foreach_dash(pattern_info,
                 src_points,
                 src_cyclic[src_curve_i],
                 [&](const IndexRange copy_points,
                     bool /*cyclic*/,
                     int /*material*/,
                     float /*radius*/,
                     float /*opacity*/) {
                   dst_point_num += copy_points.size();
                   dst_curve_num += 1;
                 });
  });

  bke::CurvesGeometry dst_curves(dst_point_num, dst_curve_num);
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  bke::SpanAttributeWriter<bool> dst_cyclic = dst_attributes.lookup_or_add_for_write_span<bool>(
      "cyclic", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter<int> dst_material = dst_attributes.lookup_or_add_for_write_span<int>(
      "material_index", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter<float> dst_radius = dst_attributes.lookup_or_add_for_write_span<float>(
      "radius", bke::AttrDomain::Point);
  bke::SpanAttributeWriter<float> dst_opacity = dst_attributes.lookup_or_add_for_write_span<float>(
      "opacity", bke::AttrDomain::Point);
  /* Map each destination point and curve to its source. */
  Array<int> src_point_indices(dst_point_num);
  Array<int> src_curve_indices(dst_curve_num);

  {
    /* Start at curve offset and add points for each dash. */
    IndexRange dst_point_range(0);
    int dst_curve_i = 0;
    auto add_dash_curve = [&](const int src_curve,
                              const IndexRange src_points,
                              const IndexRange copy_points,
                              bool cyclic,
                              int material,
                              float radius,
                              float opacity) {
      dst_point_range = dst_point_range.after(copy_points.size());
      dst_curves.offsets_for_write()[dst_curve_i] = dst_point_range.start();

      if (src_points.contains(copy_points.last())) {
        array_utils::fill_index_range(src_point_indices.as_mutable_span().slice(dst_point_range),
                                      int(copy_points.start()));
      }
      else {
        /* Cyclic curve. */
        array_utils::fill_index_range(
            src_point_indices.as_mutable_span().slice(dst_point_range.drop_back(1)),
            int(copy_points.start()));
        src_point_indices[dst_point_range.last()] = src_points.first();
      }
      src_curve_indices[dst_curve_i] = src_curve;
      dst_cyclic.span[dst_curve_i] = cyclic;
      dst_material.span[dst_curve_i] = material >= 0 ? material : src_material[src_curve];
      for (const int i : dst_point_range) {
        dst_radius.span[i] = src_radius[src_point_indices[i]] * radius;
      }
      if (dst_opacity) {
        for (const int i : dst_point_range) {
          dst_opacity.span[i] = src_opacity[src_point_indices[i]] * opacity;
        }
      }

      ++dst_curve_i;
    };

    curves_mask.foreach_index([&](const int64_t src_curve_i) {
      const IndexRange src_points = src_curves.points_by_curve()[src_curve_i];
      foreach_dash(pattern_info,
                   src_points,
                   src_cyclic[src_curve_i],
                   [&](const IndexRange copy_points,
                       bool cyclic,
                       int material,
                       float radius,
                       float opacity) {
                     add_dash_curve(
                         src_curve_i, src_points, copy_points, cyclic, material, radius, opacity);
                   });
    });
    if (dst_curve_i > 0) {
      /* Last offset entry is total point count. */
      dst_curves.offsets_for_write()[dst_curve_i] = dst_point_range.one_after_last();
    }
  }

  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Point,
                         bke::AttrDomain::Point,
                         bke::attribute_filter_from_skip_ref({"radius", "opacity"}),
                         src_point_indices,
                         dst_attributes);
  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Curve,
                         bke::AttrDomain::Curve,
                         bke::attribute_filter_from_skip_ref({"cyclic", "material_index"}),
                         src_curve_indices,
                         dst_attributes);

  dst_cyclic.finish();
  dst_material.finish();
  dst_radius.finish();
  dst_opacity.finish();
  dst_curves.update_curve_types();

  return dst_curves;
}

static void modify_drawing(const GreasePencilDashModifierData &dmd,
                           const ModifierEvalContext &ctx,
                           const PatternInfo &pattern_info,
                           bke::greasepencil::Drawing &drawing)
{
  modifier::greasepencil::ensure_no_bezier_curves(drawing);
  const bke::CurvesGeometry &src_curves = drawing.strokes();
  if (src_curves.curve_num == 0) {
    return;
  }
  /* Selected source curves. */
  IndexMaskMemory curve_mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, src_curves, dmd.influence, curve_mask_memory);
  const IndexMask unselected_mask = curves_mask.complement(src_curves.curves_range(),
                                                           curve_mask_memory);
  bke::CurvesGeometry unselected_curves = bke::curves_copy_curve_selection(
      src_curves, unselected_mask, {});

  bke::CurvesGeometry dashed_curves = create_dashes(pattern_info, src_curves, curves_mask);

  Curves *masked_curves_id = bke::curves_new_nomain(dashed_curves);
  Curves *unselected_curves_id = bke::curves_new_nomain(unselected_curves);
  bke::GeometrySet masked_geo = bke::GeometrySet::from_curves(masked_curves_id);
  bke::GeometrySet unselected_geo = bke::GeometrySet::from_curves(unselected_curves_id);
  bke::GeometrySet joined_geo = geometry::join_geometries({unselected_geo, masked_geo}, {});

  if (!joined_geo.has_curves()) {
    drawing.strokes_for_write() = {};
  }
  else {
    drawing.strokes_for_write() = std::move(joined_geo.get_curves_for_write()->geometry.wrap());
  }
  drawing.tag_topology_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  using bke::greasepencil::Drawing;

  auto *dmd = reinterpret_cast<GreasePencilDashModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int frame = grease_pencil.runtime->eval_frame;

  const PatternInfo pattern_info = get_pattern_info(*dmd);

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, dmd->influence, mask_memory);

  const Vector<Drawing *> drawings = modifier::greasepencil::get_drawings_for_write(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(
      drawings, [&](Drawing *drawing) { modify_drawing(*dmd, *ctx, pattern_info, *drawing); });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);
  auto *dmd = static_cast<GreasePencilDashModifierData *>(ptr->data);

  layout->use_property_split_set(true);

  layout->prop(ptr, "dash_offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  uiLayout *row = &layout->row(false);
  row->use_property_split_set(false);

  uiTemplateList(row,
                 (bContext *)C,
                 "MOD_UL_grease_pencil_dash_modifier_segments",
                 "",
                 ptr,
                 "segments",
                 ptr,
                 "segment_active_index",
                 nullptr,
                 3,
                 10,
                 0,
                 1,
                 UI_TEMPLATE_LIST_FLAG_NONE);

  uiLayout *col = &row->column(false);
  uiLayout *sub = &col->column(true);
  sub->op("OBJECT_OT_grease_pencil_dash_modifier_segment_add", "", ICON_ADD);
  sub->op("OBJECT_OT_grease_pencil_dash_modifier_segment_remove", "", ICON_REMOVE);
  col->separator();
  sub = &col->column(true);
  PointerRNA op_ptr = sub->op(
      "OBJECT_OT_grease_pencil_dash_modifier_segment_move", "", ICON_TRIA_UP);
  RNA_enum_set(&op_ptr, "type", /* blender::ed::object::DashSegmentMoveDirection::Up */ -1);
  op_ptr = sub->op("OBJECT_OT_grease_pencil_dash_modifier_segment_move", "", ICON_TRIA_DOWN);
  RNA_enum_set(&op_ptr, "type", /* blender::ed::object::DashSegmentMoveDirection::Down */ 1);

  if (dmd->segment_active_index >= 0 && dmd->segment_active_index < dmd->segments_num) {
    PointerRNA ds_ptr = RNA_pointer_create_discrete(ptr->owner_id,
                                                    &RNA_GreasePencilDashModifierSegment,
                                                    &dmd->segments()[dmd->segment_active_index]);

    sub = &layout->column(true);
    sub->prop(&ds_ptr, "dash", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    sub->prop(&ds_ptr, "gap", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    sub = &layout->column(false);
    sub->prop(&ds_ptr, "radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    sub->prop(&ds_ptr, "opacity", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    sub->prop(&ds_ptr, "material_index", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    sub->prop(&ds_ptr, "use_cyclic", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (uiLayout *influence_panel = layout->panel_prop(
          C, ptr, "open_influence_panel", IFACE_("Influence")))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
  }

  modifier_error_message_draw(layout, ptr);
}

static void segment_list_item_draw(uiList * /*ui_list*/,
                                   const bContext * /*C*/,
                                   uiLayout *layout,
                                   PointerRNA * /*idataptr*/,
                                   PointerRNA *itemptr,
                                   int /*icon*/,
                                   PointerRNA * /*active_dataptr*/,
                                   const char * /*active_propname*/,
                                   int /*index*/,
                                   int /*flt_flag*/)
{
  uiLayout *row = &layout->row(true);
  row->prop(itemptr, "name", UI_ITEM_R_NO_BG, "", ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilDash, panel_draw);

  uiListType *list_type = MEM_callocN<uiListType>("Grease Pencil Dash modifier segments");
  STRNCPY(list_type->idname, "MOD_UL_grease_pencil_dash_modifier_segments");
  list_type->draw_item = segment_list_item_draw;
  WM_uilisttype_add(list_type);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *dmd = reinterpret_cast<const GreasePencilDashModifierData *>(md);

  BLO_write_struct(writer, GreasePencilDashModifierData, dmd);
  modifier::greasepencil::write_influence_data(writer, &dmd->influence);

  BLO_write_struct_array(
      writer, GreasePencilDashModifierSegment, dmd->segments_num, dmd->segments_array);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *dmd = reinterpret_cast<GreasePencilDashModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &dmd->influence);

  BLO_read_struct_array(
      reader, GreasePencilDashModifierSegment, dmd->segments_num, &dmd->segments_array);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilDash = {
    /*idname*/ "GreasePencilDash",
    /*name*/ N_("Dot Dash"),
    /*struct_name*/ "GreasePencilDashModifierData",
    /*struct_size*/ sizeof(GreasePencilDashModifierData),
    /*srna*/ &RNA_GreasePencilDashModifierData,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_DASH,

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

blender::Span<GreasePencilDashModifierSegment> GreasePencilDashModifierData::segments() const
{
  return {this->segments_array, this->segments_num};
}

blender::MutableSpan<GreasePencilDashModifierSegment> GreasePencilDashModifierData::segments()
{
  return {this->segments_array, this->segments_num};
}
