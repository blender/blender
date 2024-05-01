/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <optional>

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_inplace_priority_queue.hh"
#include "BLI_math_matrix.h"
#include "BLI_span.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"

#include "ED_curves.hh"

#include "MEM_guardedalloc.h"

#include "transform.hh"
#include "transform_convert.hh"

/* -------------------------------------------------------------------- */
/** \name Curve/Surfaces Transform Creation
 * \{ */

namespace blender::ed::transform::curves {

static void calculate_curve_point_distances_for_proportional_editing(
    const Span<float3> positions, MutableSpan<float> r_distances)
{
  Array<bool, 32> visited(positions.size(), false);

  InplacePriorityQueue<float, std::less<float>> queue(r_distances);
  while (!queue.is_empty()) {
    int64_t index = queue.pop_index();
    if (visited[index]) {
      continue;
    }
    visited[index] = true;

    /* TODO(Falk): Handle cyclic curves here. */
    if (index > 0 && !visited[index - 1]) {
      int adjacent = index - 1;
      float dist = r_distances[index] + math::distance(positions[index], positions[adjacent]);
      if (dist < r_distances[adjacent]) {
        r_distances[adjacent] = dist;
        queue.priority_changed(adjacent);
      }
    }
    if (index < positions.size() - 1 && !visited[index + 1]) {
      int adjacent = index + 1;
      float dist = r_distances[index] + math::distance(positions[index], positions[adjacent]);
      if (dist < r_distances[adjacent]) {
        r_distances[adjacent] = dist;
        queue.priority_changed(adjacent);
      }
    }
  }
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

static void createTransCurvesVerts(bContext * /*C*/, TransInfo *t)
{
  MutableSpan<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  Array<Vector<IndexMask>> points_to_transform_per_attribute(t->data_container_len);
  Array<IndexMask> bezier_curves(t->data_container_len);
  const bool use_proportional_edit = (t->flag & T_PROP_EDIT_ALL) != 0;
  const bool use_connected_only = (t->flag & T_PROP_CONNECTED) != 0;

  Vector<int> must_be_selected;

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

    for (const int attribute_i : selection_attribute_names.index_range()) {
      const StringRef &selection_name = selection_attribute_names[attribute_i];
      selection_per_attribute[attribute_i] = ed::curves::retrieve_selected_points(
          curves, selection_name, curves_transform_data->memory);
    }

    bezier_curves[i] = bke::curves::indices_for_type(curves.curve_types(),
                                                     curves.curve_type_counts(),
                                                     CURVE_TYPE_BEZIER,
                                                     curves.curves_range(),
                                                     curves_transform_data->memory);
    /* Alter selection as in legacy curves bezt_select_to_transform_triple_flag(). */
    if (!bezier_curves[i].is_empty()) {
      const OffsetIndices<int> points_by_curve = curves.points_by_curve();
      const VArray<int8_t> handle_types_left = curves.handle_types_left();
      const VArray<int8_t> handle_types_right = curves.handle_types_right();

      must_be_selected.clear();
      bezier_curves[i].foreach_index([&](const int bezier_index) {
        for (const int point_i : points_by_curve[bezier_index]) {
          if (selection_per_attribute[0].contains(point_i)) {
            const HandleType type_left = HandleType(handle_types_left[point_i]);
            const HandleType type_right = HandleType(handle_types_right[point_i]);
            if (ELEM(type_left, BEZIER_HANDLE_AUTO, BEZIER_HANDLE_ALIGN) &&
                ELEM(type_right, BEZIER_HANDLE_AUTO, BEZIER_HANDLE_ALIGN))
            {
              must_be_selected.append(point_i);
            }
          }
        }
      });

      /* Select bezier handles that must be transformed if the main control point is selected. */
      IndexMask must_be_selected_mask = IndexMask::from_indices(must_be_selected.as_span(),
                                                                curves_transform_data->memory);
      if (must_be_selected.size()) {
        selection_per_attribute[1] = IndexMask::from_union(
            selection_per_attribute[1], must_be_selected_mask, curves_transform_data->memory);
        selection_per_attribute[2] = IndexMask::from_union(
            selection_per_attribute[2], must_be_selected_mask, curves_transform_data->memory);
      }
    }

    if (use_proportional_edit) {
      Array<int> bezier_point_offset_data(bezier_curves[i].size() + 1);
      OffsetIndices<int> bezier_offsets = offset_indices::gather_selected_offsets(
          curves.points_by_curve(), bezier_curves[i], bezier_point_offset_data);

      const int bezier_point_count = bezier_offsets.total_size();
      tc.data_len = curves.points_num() + 2 * bezier_point_count;
      points_to_transform_per_attribute[i].append(curves.points_range());

      if (bezier_point_count > 0) {
        Vector<index_mask::IndexMask::Initializer> bezier_point_ranges;
        OffsetIndices<int> points_by_curve = curves.points_by_curve();
        bezier_curves[i].foreach_index(GrainSize(512), [&](const int bezier_curve_i) {
          bezier_point_ranges.append(points_by_curve[bezier_curve_i]);
        });
        IndexMask bezier_points = IndexMask::from_initializers(bezier_point_ranges,
                                                               curves_transform_data->memory);
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
      tc.data = MEM_cnew_array<TransData>(tc.data_len, __func__);
      curves_transform_data->positions.reinitialize(tc.data_len);
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

    std::optional<MutableSpan<float>> value_attribute;
    bke::SpanAttributeWriter<float> attribute_writer;
    if (t->mode == TFM_CURVE_SHRINKFATTEN) {
      bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
      attribute_writer = attributes.lookup_or_add_for_write_span<float>(
          "radius",
          bke::AttrDomain::Point,
          bke::AttributeInitVArray(VArray<float>::ForSingle(0.01f, curves.points_num())));
      value_attribute = attribute_writer.span;
    }
    else if (t->mode == TFM_TILT) {
      bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
      attribute_writer = attributes.lookup_or_add_for_write_span<float>("tilt",
                                                                        bke::AttrDomain::Point);
      value_attribute = attribute_writer.span;
    }

    curve_populate_trans_data_structs(tc,
                                      curves,
                                      object->object_to_world(),
                                      value_attribute,
                                      points_to_transform_per_attribute[i],
                                      curves.curves_range(),
                                      use_connected_only,
                                      bezier_curves[i]);

    /* TODO: This is wrong. The attribute writer should live at least as long as the span. */
    attribute_writer.finish();
  }
}

static void recalcData_curves(TransInfo *t)
{
  const Span<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  for (const TransDataContainer &tc : trans_data_contrainers) {
    Curves *curves_id = static_cast<Curves *>(tc.obedit->data);
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    if (t->mode == TFM_CURVE_SHRINKFATTEN) {
      /* No cache to update currently. */
    }
    else if (t->mode == TFM_TILT) {
      curves.tag_normals_changed();
    }
    else {
      const std::array<MutableSpan<float3>, 3> positions_per_selection_attr = {
          curves.positions_for_write(),
          curves.handle_positions_left_for_write(),
          curves.handle_positions_right_for_write()};
      for (const int selection_i :
           ed::curves::get_curves_selection_attribute_names(curves).index_range())
      {
        copy_positions_from_curves_transform_custom_data(
            tc.custom.type, selection_i, positions_per_selection_attr[selection_i]);
      }
      curves.tag_positions_changed();
      curves.calculate_bezier_auto_handles();
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
 * Creates map of indices to `tc.data` representing curve in layout
 * [L0, P0, R0, L1, P1, R1, L2,P2, R2], where [P0, P1, P2], [L0, L1, L2] and [R0, R1, R2] are
 * positions, left handles and right handles respectively.
 */
static void fill_map(const CurveType curve_type,
                     const IndexRange curve_points,
                     const OffsetIndices<int> position_offsets_in_td,
                     const int handles_offset,
                     MutableSpan<int> map)
{
  const int attr_num = (curve_type == CURVE_TYPE_BEZIER) ? 3 : 1;
  const int left_handle_index = handles_offset + position_offsets_in_td[1].start();
  const int position_index = curve_points.start() + position_offsets_in_td[0].start();
  const int right_handle_index = handles_offset + position_offsets_in_td[2].start();

  std::array<int, 3> first_per_attr = {curve_type == CURVE_TYPE_BEZIER ? left_handle_index :
                                                                         position_index,
                                       /* Next two unused for non Bezier curves. */
                                       position_index,
                                       right_handle_index};

  threading::parallel_for(curve_points.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      for (const int attr : IndexRange(attr_num)) {
        map[i * attr_num + attr] = first_per_attr[attr] + i;
      }
    }
  });
}

}  // namespace blender::ed::transform::curves

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

void copy_positions_from_curves_transform_custom_data(
    const TransCustomData &custom_data,
    const int layer,
    blender::MutableSpan<blender::float3> positions_dst)
{
  using namespace blender;
  const CurvesTransformData &transform_data = *static_cast<CurvesTransformData *>(
      custom_data.data);
  const IndexMask &selection = transform_data.selection_by_layer[layer];
  OffsetIndices<int> offsets{transform_data.layer_offsets};
  Span<float3> positions = transform_data.positions.as_span().slice(offsets[layer]);

  array_utils::scatter(positions, selection, positions_dst);
}

void curve_populate_trans_data_structs(
    TransDataContainer &tc,
    blender::bke::CurvesGeometry &curves,
    const blender::float4x4 &transform,
    std::optional<blender::MutableSpan<float>> value_attribute,
    const blender::Span<blender::IndexMask> points_to_transform_per_attr,
    const blender::IndexMask &affected_curves,
    bool use_connected_only,
    const blender::IndexMask &bezier_curves)
{
  using namespace blender;
  const std::array<Span<float3>, 3> src_positions_per_selection_attr = {
      curves.positions(), curves.handle_positions_left(), curves.handle_positions_right()};
  std::array<MutableSpan<float3>, 3> positions_per_selection_attr;

  for (const int selection_i : points_to_transform_per_attr.index_range()) {
    positions_per_selection_attr[selection_i] =
        ed::transform::curves::append_positions_to_custom_data(
            points_to_transform_per_attr[selection_i],
            src_positions_per_selection_attr[selection_i],
            tc.custom.type);
  }

  float mtx[3][3], smtx[3][3];
  copy_m3_m4(mtx, transform.ptr());
  pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

  MutableSpan<TransData> all_tc_data = MutableSpan(tc.data, tc.data_len);
  OffsetIndices<int> position_offsets_in_td = ed::transform::curves::recent_position_offsets(
      tc.custom.type, points_to_transform_per_attr.size());

  Vector<VArray<bool>> selection_attrs;
  Span<StringRef> selection_attribute_names = ed::curves::get_curves_selection_attribute_names(
      curves);
  for (const StringRef selection_name : selection_attribute_names) {
    const VArray<bool> selection_attr = *curves.attributes().lookup_or_default<bool>(
        selection_name, bke::AttrDomain::Point, true);
    selection_attrs.append(selection_attr);
  }

  for (const int selection_i : position_offsets_in_td.index_range()) {
    if (position_offsets_in_td[selection_i].is_empty()) {
      continue;
    }
    MutableSpan<TransData> tc_data = all_tc_data.slice(position_offsets_in_td[selection_i]);
    MutableSpan<float3> positions = positions_per_selection_attr[selection_i];
    IndexMask points_to_transform = points_to_transform_per_attr[selection_i];
    VArray<bool> selection = selection_attrs[selection_i];

    threading::parallel_for(points_to_transform.index_range(), 1024, [&](const IndexRange range) {
      for (const int tranform_point_i : range) {
        const int point_in_domain_i = points_to_transform[tranform_point_i];
        TransData &td = tc_data[tranform_point_i];
        float3 *elem = &positions[tranform_point_i];

        copy_v3_v3(td.iloc, *elem);
        copy_v3_v3(td.center, td.iloc);
        td.loc = *elem;

        td.flag = 0;
        if (selection[point_in_domain_i]) {
          td.flag = TD_SELECTED;
        }

        if (value_attribute) {
          float *value = &((*value_attribute)[point_in_domain_i]);
          td.val = value;
          td.ival = *value;
        }
        td.ext = nullptr;

        copy_m3_m3(td.smtx, smtx);
        copy_m3_m3(td.mtx, mtx);
      }
    });
  }
  if (use_connected_only) {
    const VArray<int8_t> curve_types = curves.curve_types();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
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
        const int total_curve_points = selection_attrs_num * curve_points.size();
        map.reinitialize(total_curve_points);
        closest_distances.reinitialize(total_curve_points);
        closest_distances.fill(std::numeric_limits<float>::max());
        mapped_curve_positions.reinitialize(total_curve_points);

        ed::transform::curves::fill_map(CurveType(curve_types[curve_i]),
                                        curve_points,
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
        blender::ed::transform::curves::calculate_curve_point_distances_for_proportional_editing(
            mapped_curve_positions.as_span(), closest_distances.as_mutable_span());
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
    /*create_trans_data*/ blender::ed::transform::curves::createTransCurvesVerts,
    /*recalc_data*/ blender::ed::transform::curves::recalcData_curves,
    /*special_aftertrans_update*/ nullptr,
};
