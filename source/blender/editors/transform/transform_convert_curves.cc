/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <optional>

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_index_mask_expression.hh"
#include "BLI_inplace_priority_queue.hh"
#include "BLI_math_matrix.h"
#include "BLI_span.hh"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_geometry_set.hh"

#include "ED_curves.hh"

#include "MEM_guardedalloc.h"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

/* -------------------------------------------------------------------- */
/** \name Curve/Surfaces Transform Creation
 * \{ */

namespace blender::ed::transform::curves {

void create_aligned_handles_masks(const bke::CurvesGeometry &curves,
                                  const Span<IndexMask> points_to_transform_per_attr,
                                  const int curve_index,
                                  TransCustomData &custom_data)
{
  if (points_to_transform_per_attr.size() == 1) {
    return;
  }
  const VArraySpan<int8_t> handle_types_left = curves.handle_types_left();
  const VArraySpan<int8_t> handle_types_right = curves.handle_types_right();
  CurvesTransformData &transform_data = *static_cast<CurvesTransformData *>(custom_data.data);

  IndexMaskMemory memory;
  /* When control point is selected both handles are threaded as selected and transformed together.
   * So these will be excluded from alignment. */
  const IndexMask &selected_points = points_to_transform_per_attr[0];
  const IndexMask selected_left_handles = IndexMask::from_difference(
      points_to_transform_per_attr[1], selected_points, memory);
  index_mask::ExprBuilder builder;
  /* Left are excluded here to align only one handle when both are selected. */
  const IndexMask selected_right_handles = evaluate_expression(
      builder.subtract({&points_to_transform_per_attr[2]},
                       {&selected_left_handles, &selected_points}),
      memory);

  const IndexMask &affected_handles = IndexMask::from_union(
      selected_left_handles, selected_right_handles, memory);

  auto aligned_handles_to_selection = [&](const VArraySpan<int8_t> &handle_types) {
    return IndexMask::from_predicate(
        affected_handles, GrainSize(4096), memory, [&](const int64_t i) {
          return handle_types[i] == BEZIER_HANDLE_ALIGN;
        });
  };

  const IndexMask both_aligned = IndexMask::from_intersection(
      aligned_handles_to_selection(handle_types_left),
      aligned_handles_to_selection(handle_types_right),
      memory);

  transform_data.aligned_with_left[curve_index] = IndexMask::from_intersection(
      selected_left_handles, both_aligned, transform_data.memory);
  transform_data.aligned_with_right[curve_index] = IndexMask::from_intersection(
      selected_right_handles, both_aligned, transform_data.memory);
}

static void curve_connected_point_distances(const Span<float3> positions,
                                            MutableSpan<float> r_distances)
{
  BLI_assert(positions.size() == r_distances.size());
  Array<bool, 32> visited(positions.size(), false);

  InplacePriorityQueue<float, std::less<>> queue(r_distances);
  while (!queue.is_empty()) {
    int64_t index = queue.pop_index();
    if (visited[index]) {
      continue;
    }
    visited[index] = true;

    const int left_i = index - 1;
    if (left_i >= 0 && !visited[left_i]) {
      const float left_dist = r_distances[index] +
                              math::distance(positions[index], positions[left_i]);
      if (left_dist < r_distances[left_i]) {
        r_distances[left_i] = left_dist;
        queue.priority_increased(left_i);
      }
    }

    const int right_i = index + 1;
    if (right_i < positions.size() && !visited[right_i]) {
      const float right_dist = r_distances[index] +
                               math::distance(positions[index], positions[right_i]);
      if (right_dist < r_distances[right_i]) {
        r_distances[right_i] = right_dist;
        queue.priority_increased(right_i);
      }
    }
  }
}

static void cyclic_curve_connected_point_distances(const Span<float3> positions,
                                                   MutableSpan<float> r_distances)
{
  BLI_assert(positions.size() == r_distances.size());
  Array<bool, 32> visited(positions.size(), false);

  InplacePriorityQueue<float, std::less<>> queue(r_distances);
  while (!queue.is_empty()) {
    int64_t index = queue.pop_index();
    if (visited[index]) {
      continue;
    }
    visited[index] = true;

    const int left_i = math::mod_periodic<int>(index - 1, positions.size());
    const float left_dist = r_distances[index] +
                            math::distance(positions[index], positions[left_i]);
    if (left_dist < r_distances[left_i] && !visited[left_i]) {
      r_distances[left_i] = left_dist;
      queue.priority_increased(left_i);
    }

    const int right_i = math::mod_periodic<int>(index + 1, positions.size());
    const float right_dist = r_distances[index] +
                             math::distance(positions[index], positions[right_i]);
    if (right_dist < r_distances[right_i] && !visited[right_i]) {
      r_distances[right_i] = right_dist;
      queue.priority_increased(right_i);
    }
  }
}

static IndexMask handles_by_type(const IndexMask &handles,
                                 const VArray<int8_t> &types,
                                 const HandleType type,
                                 IndexMaskMemory &memory)
{
  if (const std::optional<int8_t> single_type = types.get_if_single()) {
    return HandleType(*single_type) == type ? handles : IndexMask();
  }
  const VArraySpan types_span = types;
  return IndexMask::from_predicate(
      handles, GrainSize(4096), memory, [&](const int64_t i) { return types_span[i] == type; });
}

static bool update_auto_handle_types(bke::CurvesGeometry &curves,
                                     const IndexMask &auto_handles,
                                     const IndexMask &auto_handles_opposite,
                                     const IndexMask &selected_handles,
                                     const IndexMask &selected_handles_opposite,
                                     const StringRef handle_type_name,
                                     IndexMaskMemory &memory)
{
  index_mask::ExprBuilder builder;
  const IndexMask convert_to_align = evaluate_expression(
      builder.merge({
          /* Selected BEZIER_HANDLE_AUTO handles from one side. */
          &builder.intersect({&selected_handles, &auto_handles}),
          /* Both sides are BEZIER_HANDLE_AUTO and opposite side is selected.
           * It ensures to convert both handles, when only one is transformed. */
          &builder.intersect({&selected_handles_opposite, &auto_handles_opposite, &auto_handles}),
      }),
      memory);
  if (convert_to_align.is_empty()) {
    return false;
  }
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter handle_types = attributes.lookup_or_add_for_write_span<int8_t>(
      handle_type_name, bke::AttrDomain::Point);
  index_mask::masked_fill(handle_types.span, int8_t(BEZIER_HANDLE_ALIGN), convert_to_align);
  handle_types.finish();
  return true;
}

static bool update_vector_handle_types(bke::CurvesGeometry &curves,
                                       const IndexMask &selected_handles,
                                       const StringRef handle_type_name)
{
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

  IndexMaskMemory memory;
  const IndexMask selected_vector = handles_by_type(
      selected_handles,
      *attributes.lookup_or_default<int8_t>(handle_type_name, bke::AttrDomain::Point, 0),
      BEZIER_HANDLE_VECTOR,
      memory);
  if (selected_vector.is_empty()) {
    return false;
  }

  bke::SpanAttributeWriter handle_types = attributes.lookup_or_add_for_write_span<int8_t>(
      handle_type_name, bke::AttrDomain::Point);
  index_mask::masked_fill(handle_types.span, int8_t(BEZIER_HANDLE_FREE), selected_vector);
  handle_types.finish();
  return true;
}

bool update_handle_types_for_transform(const eTfmMode mode,
                                       const std::array<IndexMask, 3> &selection_per_attribute,
                                       const IndexMask &bezier_points,
                                       bke::CurvesGeometry &curves)
{
  IndexMaskMemory memory;

  const IndexMask selected_left = IndexMask::from_difference(
      selection_per_attribute[1], selection_per_attribute[0], memory);
  const IndexMask selected_right = IndexMask::from_difference(
      selection_per_attribute[2], selection_per_attribute[0], memory);

  const IndexMask auto_left = handles_by_type(
      bezier_points, curves.handle_types_left(), BEZIER_HANDLE_AUTO, memory);
  const IndexMask auto_right = handles_by_type(
      bezier_points, curves.handle_types_right(), BEZIER_HANDLE_AUTO, memory);

  bool changed = false;

  if (ELEM(mode, TFM_ROTATION, TFM_RESIZE) && selection_per_attribute[0].size() == 1 &&
      selected_left.is_empty() && selected_right.is_empty())
  {
    const int64_t selected_point = selection_per_attribute[0].first();
    if (auto_left.contains(selected_point)) {
      curves.handle_types_left_for_write()[selected_point] = BEZIER_HANDLE_ALIGN;
      changed = true;
    }
    if (auto_right.contains(selected_point)) {
      curves.handle_types_right_for_write()[selected_point] = BEZIER_HANDLE_ALIGN;
      changed = true;
    }
  }
  else {
    changed |= update_auto_handle_types(
        curves, auto_left, auto_right, selected_left, selected_right, "handle_type_left", memory);
    changed |= update_auto_handle_types(
        curves, auto_right, auto_left, selected_right, selected_left, "handle_type_right", memory);

    changed |= update_vector_handle_types(curves, selected_left, "handle_type_left");
    changed |= update_vector_handle_types(curves, selected_right, "handle_type_right");
  }

  if (changed) {
    curves.tag_topology_changed();
  }

  return changed;
}

static MutableSpan<float3> append_positions_to_custom_data(const IndexMask selection,
                                                           Span<float3> positions,
                                                           TransCustomData &custom_data)
{
  CurvesTransformData &transform_data = *static_cast<CurvesTransformData *>(custom_data.data);
  transform_data.selection_by_layer.append(selection);
  const int data_offset = transform_data.layer_offsets.last();
  transform_data.layer_offsets.append(data_offset + selection.size());
  array_utils::gather(
      positions,
      selection,
      transform_data.positions.as_mutable_span().slice(data_offset, selection.size()));
  return transform_data.positions.as_mutable_span().slice(transform_data.layer_offsets.last(1),
                                                          selection.size());
}

static void createTransCurvesVerts(bContext *C, TransInfo *t)
{
  MutableSpan<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  Array<Vector<IndexMask>> points_to_transform_per_attribute(t->data_container_len);
  Array<IndexMask> bezier_curves(t->data_container_len);
  const bool use_proportional_edit = (t->flag & T_PROP_EDIT_ALL) != 0;
  const bool use_connected_only = (t->flag & T_PROP_CONNECTED) != 0;

  /* Evaluated depsgraph is necessary for taking into account deformation from modifiers. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* Count selected elements per object and create TransData structs. */
  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    Curves *curves_id = static_cast<Curves *>(tc.obedit->data);
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    CurvesTransformData *curves_transform_data = create_curves_transform_custom_data(
        tc.custom.type);
    Span<StringRef> selection_attribute_names = ed::curves::get_curves_selection_attribute_names(
        curves);
    std::array<IndexMask, 3> selection_per_attribute;

    bezier_curves[i] = bke::curves::indices_for_type(curves.curve_types(),
                                                     curves.curve_type_counts(),
                                                     CURVE_TYPE_BEZIER,
                                                     curves.curves_range(),
                                                     curves_transform_data->memory);

    const IndexMask bezier_points = bke::curves::curve_to_point_selection(
        curves.points_by_curve(), bezier_curves[i], curves_transform_data->memory);

    for (const int attribute_i : selection_attribute_names.index_range()) {
      const StringRef &selection_name = selection_attribute_names[attribute_i];
      selection_per_attribute[attribute_i] = ed::curves::retrieve_selected_points(
          curves, selection_name, bezier_points, curves_transform_data->memory);
    }

    /* Alter selection as in legacy curves bezt_select_to_transform_triple_flag(). */
    if (!bezier_points.is_empty()) {
      update_handle_types_for_transform(t->mode, selection_per_attribute, bezier_points, curves);

      index_mask::ExprBuilder builder;
      const index_mask::Expr &selected_bezier_points = builder.intersect(
          {&bezier_points, selection_per_attribute.data()});

      /* Select bezier handles that must be transformed because the control point is
       * selected. */
      selection_per_attribute[1] = evaluate_expression(
          builder.merge({&selection_per_attribute[1], &selected_bezier_points}),
          curves_transform_data->memory);
      selection_per_attribute[2] = evaluate_expression(
          builder.merge({&selection_per_attribute[2], &selected_bezier_points}),
          curves_transform_data->memory);
    }

    if (use_proportional_edit) {
      tc.data_len = curves.points_num() + 2 * bezier_points.size();
      points_to_transform_per_attribute[i].append(curves.points_range());

      if (selection_attribute_names.size() > 1) {
        points_to_transform_per_attribute[i].append(bezier_points);
        points_to_transform_per_attribute[i].append(bezier_points);
      }
    }
    else {
      tc.data_len = 0;
      for (const int selection_i : selection_attribute_names.index_range()) {
        points_to_transform_per_attribute[i].append(selection_per_attribute[selection_i]);
        tc.data_len += points_to_transform_per_attribute[i][selection_i].size();
      }
    }

    if (tc.data_len > 0) {
      tc.data = MEM_calloc_arrayN<TransData>(tc.data_len, __func__);
      curves_transform_data->positions.reinitialize(tc.data_len);
    }
    else {
      tc.custom.type.free_cb(t, &tc, &tc.custom.type);
    }
  }

  /* Populate TransData structs. */
  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    if (tc.data_len == 0) {
      continue;
    }
    Object *object = tc.obedit;
    Curves *curves_id = static_cast<Curves *>(object->data);
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(*depsgraph, *object);

    std::optional<MutableSpan<float>> value_attribute;
    bke::SpanAttributeWriter<float> attribute_writer;
    if (t->mode == TFM_CURVE_SHRINKFATTEN) {
      bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
      attribute_writer = attributes.lookup_or_add_for_write_span<float>(
          "radius",
          bke::AttrDomain::Point,
          bke::AttributeInitVArray(VArray<float>::from_single(0.01f, curves.points_num())));
      value_attribute = attribute_writer.span;
    }
    else if (t->mode == TFM_TILT) {
      bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
      attribute_writer = attributes.lookup_or_add_for_write_span<float>("tilt",
                                                                        bke::AttrDomain::Point);
      value_attribute = attribute_writer.span;
    }

    CurvesTransformData &transform_data = *static_cast<CurvesTransformData *>(tc.custom.type.data);
    transform_data.aligned_with_left.reinitialize(1);
    transform_data.aligned_with_right.reinitialize(1);

    curve_populate_trans_data_structs(*t,
                                      tc,
                                      curves,
                                      object->object_to_world(),
                                      deformation,
                                      value_attribute,
                                      points_to_transform_per_attribute[i],
                                      curves.curves_range(),
                                      use_connected_only,
                                      bezier_curves[i]);
    create_aligned_handles_masks(curves, points_to_transform_per_attribute[i], 0, tc.custom.type);

    /* TODO: This is wrong. The attribute writer should live at least as long as the span. */
    attribute_writer.finish();
  }
}

void calculate_aligned_handles(const TransCustomData &custom_data,
                               bke::CurvesGeometry &curves,
                               const int curve_index)
{
  if (ed::curves::get_curves_selection_attribute_names(curves).size() == 1) {
    return;
  }
  const CurvesTransformData &transform_data = *static_cast<const CurvesTransformData *>(
      custom_data.data);

  const Span<float3> positions = curves.positions();
  MutableSpan<float3> handle_positions_left = curves.handle_positions_left_for_write();
  MutableSpan<float3> handle_positions_right = curves.handle_positions_right_for_write();

  bke::curves::bezier::calculate_aligned_handles(transform_data.aligned_with_left[curve_index],
                                                 positions,
                                                 handle_positions_left,
                                                 handle_positions_right);
  bke::curves::bezier::calculate_aligned_handles(transform_data.aligned_with_right[curve_index],
                                                 positions,
                                                 handle_positions_right,
                                                 handle_positions_left);
}

static void recalcData_curves(TransInfo *t)
{
  if (t->state != TRANS_CANCEL) {
    transform_snap_project_individual_apply(t);
  }

  const Span<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  for (const TransDataContainer &tc : trans_data_contrainers) {
    Curves *curves_id = static_cast<Curves *>(tc.obedit->data);
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    if (t->mode == TFM_CURVE_SHRINKFATTEN) {
      curves.tag_radii_changed();
    }
    else if (t->mode == TFM_TILT) {
      curves.tag_normals_changed();
    }
    else {
      const Vector<MutableSpan<float3>> positions_per_selection_attr =
          ed::curves::get_curves_positions_for_write(curves);
      for (const int i : positions_per_selection_attr.index_range()) {
        copy_positions_from_curves_transform_custom_data(
            tc.custom.type, i, positions_per_selection_attr[i]);
      }
      curves.tag_positions_changed();
      curves.calculate_bezier_auto_handles();
      calculate_aligned_handles(tc.custom.type, curves, 0);
    }
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
  }
}

static OffsetIndices<int> recent_position_offsets(TransCustomData &custom_data, int num)
{
  const CurvesTransformData &transform_data = *static_cast<CurvesTransformData *>(
      custom_data.data);
  return OffsetIndices(transform_data.layer_offsets.as_span().slice(
      transform_data.layer_offsets.size() - num - 1, num + 1));
}

/**
 * Creates map of indices to `tc.data` representing the curves.
 * For bezier curves it uses the layout `[L0, P0, R0, L1, P1, R1, L2, P2, R2]`,
 * where `[P0, P1, P2]`, `[L0, L1, L2]` and `[R0, R1, R2]` are positions,
 *  left handles and right handles respectively.
 * Other curve types just use the positions `[P0, P1, ..., Pn]` of the control points directly.
 */
static void fill_map(const CurveType curve_type,
                     const IndexRange curve_points,
                     const OffsetIndices<int> position_offsets_in_td,
                     const int handles_offset,
                     MutableSpan<int> map)
{
  const int position_index = curve_points.start() + position_offsets_in_td[0].start();
  if (curve_type == CURVE_TYPE_BEZIER) {
    const int left_handle_index = handles_offset + position_offsets_in_td[1].start();
    const int right_handle_index = handles_offset + position_offsets_in_td[2].start();
    std::array<int, 3> first_per_attr = {left_handle_index, position_index, right_handle_index};
    threading::parallel_for(curve_points.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        for (const int attr : IndexRange(3)) {
          map[i * 3 + attr] = first_per_attr[attr] + i;
        }
      }
    });
  }
  else {
    array_utils::fill_index_range(map, position_index);
  }
}

CurvesTransformData *create_curves_transform_custom_data(TransCustomData &custom_data)
{
  CurvesTransformData *transform_data = MEM_new<CurvesTransformData>(__func__);
  transform_data->layer_offsets.append(0);
  custom_data.data = transform_data;
  custom_data.free_cb = [](TransInfo *, TransDataContainer *, TransCustomData *custom_data) {
    CurvesTransformData *data = static_cast<CurvesTransformData *>(custom_data->data);
    MEM_delete(data);
    custom_data->data = nullptr;
  };
  return transform_data;
}

void copy_positions_from_curves_transform_custom_data(const TransCustomData &custom_data,
                                                      const int layer,
                                                      MutableSpan<float3> positions_dst)
{
  const CurvesTransformData &transform_data = *static_cast<CurvesTransformData *>(
      custom_data.data);
  const IndexMask &selection = transform_data.selection_by_layer[layer];
  OffsetIndices<int> offsets{transform_data.layer_offsets};
  Span<float3> positions = transform_data.positions.as_span().slice(offsets[layer]);

  array_utils::scatter(positions, selection, positions_dst);
}

void curve_populate_trans_data_structs(const TransInfo &t,
                                       TransDataContainer &tc,
                                       bke::CurvesGeometry &curves,
                                       const float4x4 &transform,
                                       const bke::crazyspace::GeometryDeformation &deformation,
                                       std::optional<MutableSpan<float>> value_attribute,
                                       const Span<IndexMask> points_to_transform_per_attr,
                                       const IndexMask &affected_curves,
                                       bool use_connected_only,
                                       const IndexMask &bezier_curves,
                                       void *extra)
{
  const std::array<Span<float3>, 3> src_positions_per_selection_attr = {
      curves.positions(),
      curves.handle_positions_left().value_or(Span<float3>()),
      curves.handle_positions_right().value_or(Span<float3>())};
  const View3D *v3d = static_cast<const View3D *>(t.view);
  const bool hide_handles = (v3d != nullptr) ? (v3d->overlay.handle_display == CURVE_HANDLE_NONE) :
                                               false;
  const bool use_individual_origin = (t.around == V3D_AROUND_LOCAL_ORIGINS);
  const Span<float3> point_positions = curves.positions();
  const VArray<bool> cyclic = curves.cyclic();
  const VArray<bool> point_selection = *curves.attributes().lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Point, true);
  const VArray<int8_t> curve_types = curves.curve_types();

  std::array<MutableSpan<float3>, 3> positions_per_selection_attr;
  for (const int selection_i : points_to_transform_per_attr.index_range()) {
    positions_per_selection_attr[selection_i] = append_positions_to_custom_data(
        points_to_transform_per_attr[selection_i],
        src_positions_per_selection_attr[selection_i],
        tc.custom.type);
  }

  MutableSpan<TransData> all_tc_data = MutableSpan(tc.data, tc.data_len);
  OffsetIndices<int> position_offsets_in_td = recent_position_offsets(
      tc.custom.type, points_to_transform_per_attr.size());

  Vector<VArray<bool>> selection_attrs;
  Span<StringRef> selection_attribute_names = ed::curves::get_curves_selection_attribute_names(
      curves);
  for (const StringRef selection_name : selection_attribute_names) {
    const VArray<bool> selection_attr = *curves.attributes().lookup_or_default<bool>(
        selection_name, bke::AttrDomain::Point, true);
    selection_attrs.append(selection_attr);
  }

  const float3x3 mtx_base = transform.view<3, 3>();
  const float3x3 smtx_base = math::pseudo_invert(mtx_base);

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  Array<float3> mean_center_point_per_curve(curves.curves_num(), float3(0));
  if (use_individual_origin) {
    affected_curves.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
      const IndexRange points = points_by_curve[curve_i];
      IndexMaskMemory memory;
      const IndexMask selection =
          IndexMask::from_bools(point_selection, memory).slice_content(points);
      if (selection.is_empty()) {
        /* For proportional editing around individual origins, unselected points will not use the
         * TransData center (instead the closest point found is used, see logic in #set_prop_dist /
         * #prop_dist_loc_get). */
        return;
      }
      float3 center(0.0f);
      selection.foreach_index([&](const int64_t point_i) { center += point_positions[point_i]; });
      center /= selection.size();
      mean_center_point_per_curve[curve_i] = center;
    });
  }

  const Array<int> point_to_curve_map = curves.point_to_curve_map();
  for (const int selection_i : position_offsets_in_td.index_range()) {
    if (position_offsets_in_td[selection_i].is_empty()) {
      continue;
    }
    MutableSpan<TransData> tc_data = all_tc_data.slice(position_offsets_in_td[selection_i]);
    MutableSpan<float3> positions = positions_per_selection_attr[selection_i];
    const IndexMask points_to_transform = points_to_transform_per_attr[selection_i];
    const VArray<bool> selection = selection_attrs[selection_i];

    points_to_transform.foreach_index(
        GrainSize(1024), [&](const int64_t domain_i, const int64_t transform_i) {
          const int curve_i = point_to_curve_map[domain_i];

          TransData &td = tc_data[transform_i];
          float3 *elem = &positions[transform_i];

          float3 center;
          const bool use_local_center = hide_handles || use_individual_origin ||
                                        point_selection[domain_i];
          const bool use_mean_center = use_individual_origin &&
                                       !(curve_types[curve_i] == CURVE_TYPE_BEZIER);
          if (use_mean_center) {
            center = mean_center_point_per_curve[curve_i];
          }
          else if (use_local_center) {
            center = point_positions[domain_i];
          }
          else {
            center = *elem;
          }

          copy_v3_v3(td.iloc, *elem);
          copy_v3_v3(td.center, center);
          td.loc = *elem;

          td.flag = 0;
          if (selection[domain_i]) {
            td.flag = TD_SELECTED;
          }

          td.extra = extra;

          /* Set #TransData.val to nullptr for handles since those values are only tweaked on
           * control points. Logic in e.g. #initCurveShrinkFatten() also relies on this. */
          if (value_attribute && (selection_i == 0)) {
            float *value = &((*value_attribute)[domain_i]);
            td.val = value;
            td.ival = *value;
          }
          else {
            td.val = nullptr;
          }

          if (deformation.deform_mats.is_empty()) {
            copy_m3_m3(td.smtx, smtx_base.ptr());
            copy_m3_m3(td.mtx, mtx_base.ptr());
          }
          else {
            const float3x3 mtx = deformation.deform_mats[domain_i] * mtx_base;
            const float3x3 smtx = math::pseudo_invert(mtx);
            copy_m3_m3(td.smtx, smtx.ptr());
            copy_m3_m3(td.mtx, mtx.ptr());
          }
        });
  }
  if (points_to_transform_per_attr.size() > 1 && points_to_transform_per_attr.first().is_empty()) {
    auto update_handle_center = [&](const int handle_selection_attr,
                                    const int opposite_handle_selection_attr) {
      const IndexMask &handles_to_transform = points_to_transform_per_attr[handle_selection_attr];
      const IndexMask &opposite_handles_to_transform =
          points_to_transform_per_attr[opposite_handle_selection_attr];

      if (handles_to_transform.size() == 1 && opposite_handles_to_transform.size() <= 1) {
        MutableSpan<TransData> tc_data = all_tc_data.slice(
            position_offsets_in_td[handle_selection_attr]);
        copy_v3_v3(tc_data[0].center, point_positions[handles_to_transform.first()]);
      }
    };
    update_handle_center(1, 2);
    update_handle_center(2, 1);
  }

  if (use_connected_only) {
    Array<int> curves_offsets_in_td_buffer(curves.curves_num() + 1, 0);
    affected_curves.foreach_index(GrainSize(512), [&](const int64_t curve) {
      curves_offsets_in_td_buffer[curve] =
          points_to_transform_per_attr[0].slice_content(points_by_curve[curve]).size();
    });
    offset_indices::accumulate_counts_to_offsets(curves_offsets_in_td_buffer);
    const OffsetIndices<int> curves_offsets_in_td(curves_offsets_in_td_buffer);

    Array<int> bezier_offsets_in_td(curves.curves_num() + 1, 0);
    offset_indices::copy_group_sizes(points_by_curve, bezier_curves, bezier_offsets_in_td);
    offset_indices::accumulate_counts_to_offsets(bezier_offsets_in_td);

    affected_curves.foreach_segment(GrainSize(512), [&](const IndexMaskSegment segment) {
      Array<int> map;
      Array<float> closest_distances;
      Array<float3> mapped_curve_positions;

      for (const int curve_i : segment) {
        const int selection_attrs_num = curve_types[curve_i] == CURVE_TYPE_BEZIER ? 3 : 1;
        const IndexRange curve_points = points_by_curve[curve_i];
        const IndexRange editable_curve_points = curves_offsets_in_td[curve_i];
        const int total_curve_points = selection_attrs_num * editable_curve_points.size();
        map.reinitialize(total_curve_points);
        closest_distances.reinitialize(total_curve_points);
        closest_distances.fill(std::numeric_limits<float>::max());
        mapped_curve_positions.reinitialize(total_curve_points);

        fill_map(CurveType(curve_types[curve_i]),
                 editable_curve_points,
                 position_offsets_in_td,
                 bezier_offsets_in_td[curve_i],
                 map);

        bool has_any_selected = false;
        for (const int selection_attr_i : IndexRange(selection_attrs_num)) {
          has_any_selected = has_any_selected ||
                             ed::curves::has_anything_selected(selection_attrs[selection_attr_i],
                                                               curve_points);
        }
        if (!has_any_selected) {
          for (const int i : map) {
            TransData &td = all_tc_data[i];
            td.flag |= TD_SKIP;
          }
          continue;
        }

        for (const int i : closest_distances.index_range()) {
          TransData &td = all_tc_data[map[i]];
          mapped_curve_positions[i] = td.loc;
          if (td.flag & TD_SELECTED) {
            closest_distances[i] = 0.0f;
          }
        }

        if (cyclic[curve_i]) {
          cyclic_curve_connected_point_distances(mapped_curve_positions.as_span(),
                                                 closest_distances.as_mutable_span());
        }
        else {
          curve_connected_point_distances(mapped_curve_positions.as_span(),
                                          closest_distances.as_mutable_span());
        }

        for (const int i : closest_distances.index_range()) {
          TransData &td = all_tc_data[map[i]];
          td.dist = closest_distances[i];
        }
      }
    });
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_Curves = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*create_trans_data*/ createTransCurvesVerts,
    /*recalc_data*/ recalcData_curves,
    /*special_aftertrans_update*/ nullptr,
};

}  // namespace blender::ed::transform::curves
