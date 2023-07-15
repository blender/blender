/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_array.hh"
#include "BLI_inplace_priority_queue.hh"
#include "BLI_span.hh"

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
    Curves *curves_id = static_cast<Curves *>(tc.obedit->data);
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();

    float mtx[3][3], smtx[3][3];
    copy_m3_m4(mtx, tc.obedit->object_to_world);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    MutableSpan<float3> positions = curves.positions_for_write();
    if (use_proportional_edit) {
      const OffsetIndices<int> points_by_curve = curves.points_by_curve();
      const VArray<bool> selection = *curves.attributes().lookup_or_default<bool>(
          ".selection", ATTR_DOMAIN_POINT, true);
      threading::parallel_for(curves.curves_range(), 512, [&](const IndexRange range) {
        Vector<float> closest_distances;
        for (const int curve_i : range) {
          const IndexRange points = points_by_curve[curve_i];
          const bool has_any_selected = ed::curves::has_anything_selected(selection, points);
          if (!has_any_selected && use_connected_only) {
            for (const int point_i : points) {
              TransData &td = tc.data[point_i];
              td.flag |= TD_SKIP;
            }
            continue;
          }

          closest_distances.reinitialize(points.size());
          closest_distances.fill(std::numeric_limits<float>::max());

          for (const int i : IndexRange(points.size())) {
            const int point_i = points[i];
            TransData &td = tc.data[point_i];
            float3 *elem = &positions[point_i];

            copy_v3_v3(td.iloc, *elem);
            copy_v3_v3(td.center, td.iloc);
            td.loc = *elem;

            td.flag = 0;
            if (selection[point_i]) {
              closest_distances[i] = 0.0f;
              td.flag = TD_SELECTED;
            }

            td.ext = nullptr;

            copy_m3_m3(td.smtx, smtx);
            copy_m3_m3(td.mtx, mtx);
          }

          if (use_connected_only) {
            calculate_curve_point_distances_for_proportional_editing(
                positions.slice(points), closest_distances.as_mutable_span());
            for (const int i : IndexRange(points.size())) {
              TransData &td = tc.data[points[i]];
              td.dist = closest_distances[i];
            }
          }
        }
      });
    }
    else {
      const IndexMask selected_indices = selection_per_object[i];
      threading::parallel_for(selected_indices.index_range(), 1024, [&](const IndexRange range) {
        for (const int selection_i : range) {
          TransData *td = &tc.data[selection_i];
          float3 *elem = &positions[selected_indices[selection_i]];

          copy_v3_v3(td->iloc, *elem);
          copy_v3_v3(td->center, td->iloc);
          td->loc = *elem;

          td->flag = TD_SELECTED;
          td->ext = nullptr;

          copy_m3_m3(td->smtx, smtx);
          copy_m3_m3(td->mtx, mtx);
        }
      });
    }
  }
}

static void recalcData_curves(TransInfo *t)
{
  const Span<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  for (const TransDataContainer &tc : trans_data_contrainers) {
    Curves *curves_id = static_cast<Curves *>(tc.obedit->data);
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();

    curves.calculate_bezier_auto_handles();
    curves.tag_positions_changed();
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
  }
}

}  // namespace blender::ed::transform::curves

/** \} */

TransConvertTypeInfo TransConvertType_Curves = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*createTransData*/ blender::ed::transform::curves::createTransCurvesVerts,
    /*recalcData*/ blender::ed::transform::curves::recalcData_curves,
    /*special_aftertrans_update*/ nullptr,
};
