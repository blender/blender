/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_array.hh"
#include "BLI_bounds.hh"
#include "BLI_lasso_2d.hh"
#include "BLI_rect.hh"
#include "BLI_task.hh"

#include "DNA_brush_types.h"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_paint.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "RNA_access.hh"

#include "WM_api.hh"

#include "grease_pencil_segments_intern.hh"

namespace blender {

namespace ed::greasepencil {

namespace trim {

using namespace segment;

static bool check_line_segment_lasso_intersection(const int2 &pos_a,
                                                  const int2 &pos_b,
                                                  const Span<int2> mcoords)
{
  Bounds<int2> bbox_ab{math::min(pos_a, pos_b), math::max(pos_a, pos_b)};
  bbox_ab.pad(BBOX_PADDING);

  /* Check the lasso bounding box first as an optimization. */
  if (bbox_ab.intersects_segment(pos_a, pos_b) &&
      BLI_lasso_is_edge_inside(mcoords, pos_a.x, pos_a.y, pos_b.x, pos_b.y, IS_CLIPPED))
  {
    return true;
  }
  return false;
}

static void check_segments_in_lasso(const Span<float2> screen_space_positions,
                                    const Span<Bounds<float2>> screen_space_bbox,
                                    const Span<int2> mcoords,
                                    const Span<Segment> all_segments,
                                    const IndexMask &editable_curves,
                                    const OffsetIndices<int> segments_by_curve,
                                    MutableSpan<bool> segments_to_keep)
{
  const Bounds<int2> bbox_lasso_int = *bounds::min_max(mcoords);
  const Bounds<float2> bbox_lasso{float2(bbox_lasso_int.min), float2(bbox_lasso_int.max)};

  editable_curves.foreach_index(
      [&](const int curve_i) {
        /* To speed things up: Do a bounding box check on the curve and the lasso area. */
        if (!bounds::intersect(bbox_lasso, screen_space_bbox[curve_i]).has_value()) {
          return;
        }

        const IndexRange &segment_range = segments_by_curve[curve_i];
        for (const int segment_i : segment_range) {
          const Segment &segment = all_segments[segment_i];

          const IndexRange point_range = segment.point_range();

          if (point_range.is_empty()) {
            const float start_factor = segment.intersection_factor[Side::Start];
            const int2 start_edge = segment.edge(Side::Start);
            const float end_factor = segment.intersection_factor[Side::End];
            const int2 end_edge = segment.edge(Side::End);
            const float2 pos_1 = math::interpolate(screen_space_positions[start_edge.x],
                                                   screen_space_positions[start_edge.y],
                                                   start_factor);
            const float2 pos_2 = math::interpolate(screen_space_positions[end_edge.x],
                                                   screen_space_positions[end_edge.y],
                                                   end_factor);

            if (check_line_segment_lasso_intersection(int2(pos_1), int2(pos_2), mcoords)) {
              segments_to_keep[segment_i] = false;
            }

            continue;
          }

          for (const int64_t i : point_range.drop_back(1)) {
            const int point_i1 = segment.wrap_index(i);
            const int point_i2 = segment.wrap_index(i + 1);

            const float2 pos_1 = screen_space_positions[point_i1];
            const float2 pos_2 = screen_space_positions[point_i2];

            if (check_line_segment_lasso_intersection(int2(pos_1), int2(pos_2), mcoords)) {
              segments_to_keep[segment_i] = false;
              continue;
            }
          }

          if (segment_range.size() == 1 && segment.is_loop()) {
            const float2 pos_1 = screen_space_positions[segment.wrap_index(point_range.first())];
            const float2 pos_2 = screen_space_positions[segment.wrap_index(point_range.last())];

            if (check_line_segment_lasso_intersection(int2(pos_1), int2(pos_2), mcoords)) {
              segments_to_keep[segment_i] = false;
              continue;
            }
          }
          else {
            if (segment.has_intersection(Side::Start)) {
              const float start_factor = segment.intersection_factor[Side::Start];
              const int2 start_edge = segment.edge(Side::Start);
              const float2 pos_1 = math::interpolate(screen_space_positions[start_edge.x],
                                                     screen_space_positions[start_edge.y],
                                                     start_factor);
              const float2 pos_2 = screen_space_positions[segment.wrap_index(point_range.first())];

              if (check_line_segment_lasso_intersection(int2(pos_1), int2(pos_2), mcoords)) {
                segments_to_keep[segment_i] = false;
                continue;
              }
            }

            if (segment.has_intersection(Side::End)) {
              const float end_factor = segment.intersection_factor[Side::End];
              const int2 end_edge = segment.edge(Side::End);
              const float2 pos_1 = screen_space_positions[segment.wrap_index(point_range.last())];
              const float2 pos_2 = math::interpolate(screen_space_positions[end_edge.x],
                                                     screen_space_positions[end_edge.y],
                                                     end_factor);

              if (check_line_segment_lasso_intersection(int2(pos_1), int2(pos_2), mcoords)) {
                segments_to_keep[segment_i] = false;
                continue;
              }
            }
          }
        }
      },
      exec_mode::grain_size(128));
}

static bke::CurvesGeometry create_curves_from_segments(const bke::CurvesGeometry &src,
                                                       const Span<Segment> segments,
                                                       const Span<bool> segment_reversed,
                                                       const Span<bool> cyclic,
                                                       const OffsetIndices<int> segment_offsets)
{
  struct InterpolatePoint {
    int src_point_1;
    int src_point_2;
    float factor;
  };

  Array<int> point_offsets(segment_offsets.size() + 1);
  Vector<InterpolatePoint> point_to_interpolate;

  for (const int curve_i : segment_offsets.index_range()) {
    point_offsets[curve_i] = point_to_interpolate.size();

    const IndexRange segment_range = segment_offsets[curve_i];
    for (const int seg_i : segment_range) {
      const Segment &segment = segments[seg_i];
      const bool reversed = segment_reversed[seg_i];
      const Side start_side = reversed ? Side::End : Side::Start;
      const Side end_side = reversed ? Side::Start : Side::End;

      if (segment.has_intersection(start_side) && !segment.is_loop()) {
        const float start_factor = segment.intersection_factor[start_side];
        const int2 start_edge = segment.edge(start_side);

        point_to_interpolate.append({start_edge.x, start_edge.y, start_factor});
      }

      segment.foreach_point(
          [&](const int index) { point_to_interpolate.append({index, index, 0.0f}); });

      if (reversed) {
        point_to_interpolate.as_mutable_span().take_back(segment.points_num()).reverse();
      }

      if (seg_i == segment_range.last() && segment.has_intersection(end_side) && !cyclic[curve_i])
      {
        const float end_factor = segment.intersection_factor[end_side];
        const int2 end_edge = segment.edge(end_side);

        point_to_interpolate.append({end_edge.x, end_edge.y, end_factor});
      }
    }
  }

  point_offsets.last() = point_to_interpolate.size();
  const OffsetIndices<int> dst_points_by_curve = OffsetIndices<int>(point_offsets);

  if (dst_points_by_curve.total_size() == 0) {
    return bke::CurvesGeometry();
  }

  bke::CurvesGeometry dst_curves(dst_points_by_curve.total_size(), dst_points_by_curve.size());
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();

  dst_curves.offsets_for_write().copy_from(dst_points_by_curve.data());
  dst_curves.cyclic_for_write().copy_from(cyclic);

  Array<int> old_by_new_map(dst_points_by_curve.size());

  threading::parallel_for(dst_points_by_curve.index_range(), 4096, [&](const IndexRange points) {
    for (const int i : points) {
      const IndexRange segment_range = segment_offsets[i];
      old_by_new_map[i] = segments[segment_range.first()].curve;
    }
  });

  const bke::AttributeAccessor src_attributes = src.attributes();
  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Curve,
                         bke::AttrDomain::Curve,
                         bke::attribute_filter_from_skip_ref({"cyclic"}),
                         old_by_new_map,
                         dst_attributes);

  /* Copy/Interpolate point attributes. */
  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes, dst_attributes, {bke::AttrDomain::Point}, {}))
  {
    bke::attribute_math::to_static_type(attribute.dst.span.type(), [&]<typename T>() {
      if constexpr (!std::is_same_v<T, std::string>) {
        const Span<T> src_attr = attribute.src.typed<T>();
        MutableSpan<T> dst_attr = attribute.dst.span.typed<T>();

        threading::parallel_for(
            point_to_interpolate.index_range(), 4096, [&](const IndexRange points) {
              for (const int i : points) {
                const InterpolatePoint &int_point = point_to_interpolate[i];

                if (int_point.factor == 0.0f) {
                  dst_attr[i] = src_attr[int_point.src_point_1];
                }
                else if (int_point.factor == 1.0f) {
                  dst_attr[i] = src_attr[int_point.src_point_2];
                }
                else {
                  dst_attr[i] = bke::attribute_math::mix2<T>(int_point.factor,
                                                             src_attr[int_point.src_point_1],
                                                             src_attr[int_point.src_point_2]);
                }
              }
            });
      }
    });

    attribute.dst.finish();
  }

  return dst_curves;
}

bke::CurvesGeometry trim_curve_segments(const bke::CurvesGeometry &src,
                                        const Span<float2> screen_space_positions,
                                        const Span<int2> mcoords,
                                        const IndexMask &editable_curves,
                                        const IndexMask &visible_curves,
                                        const bool keep_caps)
{
  if (src.is_empty()) {
    return src;
  }

  const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
  const VArray<bool> is_cyclic = src.cyclic();

  Array<Bounds<float2>> screen_space_bbox(src.curves_num());
  compute_bounding_boxes(src_points_by_curve, screen_space_positions, screen_space_bbox);

  Vector<IntersectionPoint> intersections;
  Array<int> all_segment_offset_data(src_points_by_curve.size() + 1);
  Vector<Segment> all_segments;

  Array<Vector<int>> inters_per_curves(src_points_by_curve.size());
  find_intersections_between_all_curves(screen_space_positions,
                                        screen_space_bbox,
                                        src_points_by_curve,
                                        is_cyclic,
                                        visible_curves,
                                        inters_per_curves,
                                        intersections);
  create_segments_from_intersections(inters_per_curves,
                                     src_points_by_curve,
                                     intersections,
                                     is_cyclic,
                                     all_segments,
                                     all_segment_offset_data.as_mutable_span().drop_back(1));
  store_segment_map_on_intersections(all_segments, intersections);
  const OffsetIndices<int> segments_by_curve = offset_indices::accumulate_counts_to_offsets(
      all_segment_offset_data);

  Array<bool> segments_to_keep(all_segments.size(), true);
  check_segments_in_lasso(screen_space_positions,
                          screen_space_bbox,
                          mcoords,
                          all_segments,
                          editable_curves,
                          segments_by_curve,
                          segments_to_keep.as_mutable_span());

  Array<SegmentConnections> segment_connections(all_segments.size(),
                                                SegmentConnections(SEGMENT_CONNECTION_NULL));
  create_connections_from_curves(
      segments_by_curve, segments_to_keep, is_cyclic, segment_connections.as_mutable_span());

  Vector<Segment> segments;
  Vector<int> segment_offset_data;
  Vector<bool> segment_reversed;
  Vector<bool> cyclic;
  follow_segment_connections(all_segments,
                             segments_to_keep,
                             segment_connections,
                             segments,
                             segment_offset_data,
                             segment_reversed,
                             cyclic);
  const OffsetIndices<int> segment_offsets = OffsetIndices<int>(segment_offset_data);

  bke::CurvesGeometry dst = create_curves_from_segments(
      src, segments, segment_reversed, cyclic, segment_offsets);

  if (!keep_caps) {
    cut_caps(dst, segments, segment_reversed, cyclic, segment_offsets);
  }

  return dst;
}

bke::CurvesGeometry trim_curve_segment_ends(const bke::CurvesGeometry &src,
                                            const Span<float2> screen_space_positions,
                                            const IndexMask &editable_curves,
                                            const IndexMask &visible_curves,
                                            const bool keep_caps)
{
  if (src.is_empty()) {
    return src;
  }

  const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
  const VArray<bool> is_cyclic = src.cyclic();

  Array<Bounds<float2>> screen_space_bbox(src.curves_num());
  compute_bounding_boxes(src_points_by_curve, screen_space_positions, screen_space_bbox);

  Vector<IntersectionPoint> intersections;
  Array<int> all_segment_offset_data(src_points_by_curve.size() + 1);
  Vector<Segment> all_segments;

  Array<Vector<int>> inters_per_curves(src_points_by_curve.size());
  find_intersections_between_all_curves(screen_space_positions,
                                        screen_space_bbox,
                                        src_points_by_curve,
                                        is_cyclic,
                                        visible_curves,
                                        inters_per_curves,
                                        intersections);
  create_segments_from_intersections(inters_per_curves,
                                     src_points_by_curve,
                                     intersections,
                                     is_cyclic,
                                     all_segments,
                                     all_segment_offset_data.as_mutable_span().drop_back(1));
  store_segment_map_on_intersections(all_segments, intersections);
  const OffsetIndices<int> segments_by_curve = offset_indices::accumulate_counts_to_offsets(
      all_segment_offset_data);

  Array<bool> segments_to_keep(all_segments.size(), true);
  /* Remove the end segments unless that would delete the whole curve. */
  editable_curves.foreach_index(
      [&](const int curve_i) {
        const IndexRange segment_range = segments_by_curve[curve_i];

        if (segment_range.size() > 2) {
          segments_to_keep[segment_range.first()] = false;
          segments_to_keep[segment_range.last()] = false;
        }
      },
      exec_mode::grain_size(128));

  Array<SegmentConnections> segment_connections(all_segments.size(),
                                                SegmentConnections(SEGMENT_CONNECTION_NULL));
  create_connections_from_curves(
      segments_by_curve, segments_to_keep, is_cyclic, segment_connections.as_mutable_span());

  Vector<Segment> segments;
  Vector<int> segment_offset_data;
  Vector<bool> segment_reversed;
  Vector<bool> cyclic;
  follow_segment_connections(all_segments,
                             segments_to_keep,
                             segment_connections,
                             segments,
                             segment_offset_data,
                             segment_reversed,
                             cyclic);
  const OffsetIndices<int> segment_offsets = OffsetIndices<int>(segment_offset_data);

  bke::CurvesGeometry dst = create_curves_from_segments(
      src, segments, segment_reversed, cyclic, segment_offsets);

  if (!keep_caps) {
    cut_caps(dst, segments, segment_reversed, cyclic, segment_offsets);
  }

  return dst;
}

}  // namespace trim

/**
 * Apply the stroke trim to a drawing.
 */
static bool execute_trim_on_drawing(const int layer_index,
                                    const Object &ob_eval,
                                    Object &obact,
                                    const ARegion &region,
                                    const float4x4 &projection,
                                    const Span<int2> mcoords,
                                    const bool keep_caps,
                                    bke::greasepencil::Drawing &drawing)
{
  const bke::CurvesGeometry &src = drawing.strokes();

  /* Get evaluated geometry. */
  bke::crazyspace::GeometryDeformation deformation =
      bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(&ob_eval, obact, drawing);

  /* Compute screen space positions. */
  Array<float2> screen_space_positions(src.points_num());
  threading::parallel_for(src.points_range(), 4096, [&](const IndexRange src_points) {
    for (const int src_point : src_points) {
      screen_space_positions[src_point] = ED_view3d_project_float_v2_m4(
          &region, deformation.positions[src_point], projection);
    }
  });

  IndexMaskMemory memory;
  const IndexMask editable_strokes = ed::greasepencil::retrieve_editable_strokes(
      obact, drawing, layer_index, memory);
  const IndexMask visible_strokes = ed::greasepencil::retrieve_visible_strokes(
      obact, drawing, memory);

  /* Apply trim. */
  bke::CurvesGeometry cut_strokes = ed::greasepencil::trim::trim_curve_segments(
      src, screen_space_positions, mcoords, editable_strokes, visible_strokes, keep_caps);

  /* Set the new geometry. */
  drawing.strokes_for_write() = std::move(cut_strokes);
  drawing.tag_topology_changed();

  return true;
}

/**
 * Apply the stroke trim to all layers.
 */
static wmOperatorStatus stroke_trim_execute(const bContext *C, const Span<int2> mcoords)
{
  const Scene *scene = CTX_data_scene(C);
  const ARegion *region = CTX_wm_region(C);
  const RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *obact = CTX_data_active_object(C);
  Object *ob_eval = DEG_get_evaluated(depsgraph, obact);

  GreasePencil &grease_pencil = *id_cast<GreasePencil *>(obact->data);

  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  if (brush->gpencil_settings == nullptr) {
    BKE_brush_init_gpencil_settings(brush);
  }
  const bool keep_caps = (brush->gpencil_settings->flag & GP_BRUSH_ERASER_KEEP_CAPS) != 0;
  const bool active_layer_only = (brush->gpencil_settings->flag & GP_BRUSH_ACTIVE_LAYER_ONLY) != 0;
  std::atomic<bool> changed = false;

  bool inserted_keyframe = false;
  if (active_layer_only) {
    /* Apply trim on drawings of active layer. */
    if (!grease_pencil.has_active_layer()) {
      return OPERATOR_CANCELLED;
    }

    bke::greasepencil::Layer &layer = *grease_pencil.get_active_layer();
    if (!layer.is_editable()) {
      return OPERATOR_CANCELLED;
    }

    ensure_active_keyframe(*scene, grease_pencil, layer, true, inserted_keyframe);
    const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
    const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(rv3d, layer_to_world);
    const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings_from_layer(*scene, grease_pencil, layer);
    threading::parallel_for_each(drawings, [&](const ed::greasepencil::MutableDrawingInfo &info) {
      if (execute_trim_on_drawing(info.layer_index,
                                  *ob_eval,
                                  *obact,
                                  *region,
                                  projection,
                                  mcoords,
                                  keep_caps,
                                  info.drawing))
      {
        changed = true;
      }
    });
  }
  else {
    for (bke::greasepencil::Layer *layer : grease_pencil.layers_for_write()) {
      if (layer->is_editable()) {
        ed::greasepencil::ensure_active_keyframe(
            *scene, grease_pencil, *layer, true, inserted_keyframe);
      }
    }

    /* Apply trim on every editable drawing. */
    const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
    threading::parallel_for_each(drawings, [&](const ed::greasepencil::MutableDrawingInfo &info) {
      const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
      const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
      const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(rv3d, layer_to_world);
      if (execute_trim_on_drawing(info.layer_index,
                                  *ob_eval,
                                  *obact,
                                  *region,
                                  projection,
                                  mcoords,
                                  keep_caps,
                                  info.drawing))
      {
        changed = true;
      }
    });
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
    if (inserted_keyframe) {
      WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
    }
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus grease_pencil_stroke_trim_exec(bContext *C, wmOperator *op)
{
  const Array<int2> mcoords = WM_gesture_lasso_path_to_array(C, op);

  if (mcoords.is_empty()) {
    return OPERATOR_PASS_THROUGH;
  }

  return stroke_trim_execute(C, mcoords);
}

}  // namespace ed::greasepencil

void GREASE_PENCIL_OT_stroke_trim(wmOperatorType *ot)
{
  using namespace blender::ed::greasepencil;

  ot->name = "Grease Pencil Trim";
  ot->idname = "GREASE_PENCIL_OT_stroke_trim";
  ot->description = "Delete stroke points in between intersecting strokes";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = grease_pencil_stroke_trim_exec;
  ot->poll = grease_pencil_painting_poll;
  ot->cancel = WM_gesture_lasso_cancel;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  WM_operator_properties_gesture_lasso(ot);
}

}  // namespace blender
