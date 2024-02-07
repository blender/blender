/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <optional>

#include "BLI_array.hh"
#include "BLI_inplace_priority_queue.hh"
#include "BLI_math_matrix.h"
#include "BLI_span.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"

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

static void createTransCurvesVerts(bContext * /*C*/, TransInfo *t)
{
  MutableSpan<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  IndexMaskMemory memory;
  Array<IndexMask> selection_per_object(t->data_container_len);
  const bool use_proportional_edit = (t->flag & T_PROP_EDIT_ALL) != 0;
  const bool use_connected_only = (t->flag & T_PROP_CONNECTED) != 0;

  /* Count selected elements per object and create TransData structs. */
  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    Curves *curves_id = static_cast<Curves *>(tc.obedit->data);
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();

    if (use_proportional_edit) {
      tc.data_len = curves.point_num;
    }
    else {
      selection_per_object[i] = ed::curves::retrieve_selected_points(curves, memory);
      tc.data_len = selection_per_object[i].size();
    }

    if (tc.data_len > 0) {
      tc.data = MEM_cnew_array<TransData>(tc.data_len, __func__);
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
                                      float4x4(object->object_to_world),
                                      value_attribute,
                                      selection_per_object[i],
                                      use_proportional_edit,
                                      curves.curves_range(),
                                      use_connected_only,
                                      0 /* No data offset for curves. */);

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
      curves.tag_positions_changed();
      curves.calculate_bezier_auto_handles();
    }
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
  }
}

}  // namespace blender::ed::transform::curves

void curve_populate_trans_data_structs(TransDataContainer &tc,
                                       blender::bke::CurvesGeometry &curves,
                                       const blender::float4x4 &transform,
                                       std::optional<blender::MutableSpan<float>> value_attribute,
                                       const blender::IndexMask &selected_indices,
                                       const bool use_proportional_edit,
                                       const blender::IndexMask &affected_curves,
                                       bool use_connected_only,
                                       int trans_data_offset)
{
  using namespace blender;

  float mtx[3][3], smtx[3][3];
  copy_m3_m4(mtx, transform.ptr());
  pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

  MutableSpan<float3> positions = curves.positions_for_write();
  if (use_proportional_edit) {
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const VArray<bool> selection = *curves.attributes().lookup_or_default<bool>(
        ".selection", bke::AttrDomain::Point, true);
    affected_curves.foreach_segment(GrainSize(512), [&](const IndexMaskSegment segment) {
      Vector<float> closest_distances;
      for (const int curve_i : segment) {
        const IndexRange points = points_by_curve[curve_i];
        const bool has_any_selected = ed::curves::has_anything_selected(selection, points);
        if (!has_any_selected && use_connected_only) {
          for (const int point_i : points) {
            TransData &td = tc.data[point_i + trans_data_offset];
            td.flag |= TD_SKIP;
          }
          continue;
        }

        closest_distances.reinitialize(points.size());
        closest_distances.fill(std::numeric_limits<float>::max());

        for (const int i : IndexRange(points.size())) {
          const int point_i = points[i];
          TransData &td = tc.data[point_i + trans_data_offset];
          float3 *elem = &positions[point_i];

          copy_v3_v3(td.iloc, *elem);
          copy_v3_v3(td.center, td.iloc);
          td.loc = *elem;

          td.flag = 0;
          if (selection[point_i]) {
            closest_distances[i] = 0.0f;
            td.flag = TD_SELECTED;
          }

          if (value_attribute) {
            float *value = &((*value_attribute)[point_i]);
            td.val = value;
            td.ival = *value;
          }

          td.ext = nullptr;

          copy_m3_m3(td.smtx, smtx);
          copy_m3_m3(td.mtx, mtx);
        }

        if (use_connected_only) {
          blender::ed::transform::curves::calculate_curve_point_distances_for_proportional_editing(
              positions.slice(points), closest_distances.as_mutable_span());
          for (const int i : IndexRange(points.size())) {
            TransData &td = tc.data[points[i] + trans_data_offset];
            td.dist = closest_distances[i];
          }
        }
      }
    });
  }
  else {
    threading::parallel_for(selected_indices.index_range(), 1024, [&](const IndexRange range) {
      for (const int selection_i : range) {
        TransData *td = &tc.data[selection_i + trans_data_offset];
        const int point_i = selected_indices[selection_i];
        float3 *elem = &positions[point_i];

        copy_v3_v3(td->iloc, *elem);
        copy_v3_v3(td->center, td->iloc);
        td->loc = *elem;

        if (value_attribute) {
          float *value = &((*value_attribute)[point_i]);
          td->val = value;
          td->ival = *value;
        }

        td->flag = TD_SELECTED;
        td->ext = nullptr;

        copy_m3_m3(td->smtx, smtx);
        copy_m3_m3(td->mtx, mtx);
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
