/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_geom.h"
#include "BLI_task.hh"

#include "BKE_brush.hh"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_grease_pencil.h"
#include "BKE_grease_pencil.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"
#include "DNA_brush_enums.h"

#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class EraseOperation : public GreasePencilStrokeOperation {

 public:
  ~EraseOperation() override {}

  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext &C) override;

  bool keep_caps = false;
  float radius = 50.0f;
  eGP_BrushEraserMode eraser_mode = GP_BRUSH_ERASER_HARD;
  bool active_layer_only = false;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct EraseOperationExecutor {

  float2 mouse_position{};
  float eraser_radius{};

  EraseOperationExecutor(const bContext & /*C*/) {}

  /**
   * Computes the intersection between the eraser tool and a 2D segment.
   *
   * \param point: coordinates of the first point in the segment.
   * \param point_after: coordinates of the second point in the segment.
   *
   * \param r_mu0: output factor of the first intersection if it exists, otherwise (-1).
   * \param r_mu1: output factor of the second intersection if it exists, otherwise (-1).
   *
   * \returns total number of intersections lying inside the segment (ie whose factor is in [0,1]).
   *
   * Note that the eraser is represented as a circle, and thus there can be only 0, 1 or 2
   * intersections with a segment.
   */
  int intersections_with_segment(const float2 &point,
                                 const float2 &point_after,
                                 float &r_mu0,
                                 float &r_mu1) const
  {
    /* Compute the intersection points. */
    float2 inter0{};
    float2 inter1{};
    const int nb_inter = isect_line_sphere_v2(
        point, point_after, this->mouse_position, this->eraser_radius, inter0, inter1);

    /* Retrieve the line factor from the coordinates of the intersection points. */
    const auto compute_intersection_parameter =
        [](const float2 p0, const float2 p1, const float2 inter) {
          const float mu = (math::length(inter - p0) / math::length(p1 - p0));
          const float sign_mu = (math::dot(inter - p0, p1 - p0) < 0) ? -1.0 : 1.0;
          return sign_mu * mu;
        };
    r_mu0 = (nb_inter > 0) ? compute_intersection_parameter(point, point_after, inter0) : -1.0;
    r_mu1 = (nb_inter > 1) ? compute_intersection_parameter(point, point_after, inter1) : -1.0;

    /* Sort intersections by line factor. */
    if ((nb_inter > 1) && (r_mu0 > r_mu1)) {
      std::swap(r_mu0, r_mu1);
    }

    /* Return the number of intersections that actually lies within the segment. */
    return int(IN_RANGE(r_mu0, 0, 1)) + int(IN_RANGE(r_mu1, 0, 1));
  }

  /**
   * Compute intersections between the eraser and the input Curves Geometry.
   *
   * \param screen_space_positions: input parameter containing the 2D positions of the geometry in
   * screen space.
   *
   * \param r_nb_intersections: output parameter filled with the number of intersections
   * per-segment. Should be the size of the source point range.
   * \param r_intersections_factors: output parameter filled with the factors of the potential
   * intersections with each segment. Should be the size of the source point range.
   * \returns total number of intersections found.
   *
   * Note that for the two output arrays the last element may contain intersections if the
   * corresponding curve is cyclic.
   */
  int intersections_with_curves(const bke::CurvesGeometry &src,
                                const Span<float2> screen_space_positions,
                                MutableSpan<int> r_nb_intersections,
                                MutableSpan<float2> r_intersections_factors) const
  {
    const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
    const VArray<bool> src_cyclic = src.cyclic();

    threading::parallel_for(src.curves_range(), 256, [&](const IndexRange src_curves) {
      for (const int src_curve : src_curves) {
        const IndexRange src_curve_points = src_points_by_curve[src_curve];

        threading::parallel_for(
            src_curve_points.drop_back(1), 512, [&](const IndexRange src_points) {
              for (int src_point : src_points) {

                float mu0;
                float mu1;
                r_nb_intersections[src_point] = intersections_with_segment(
                    screen_space_positions[src_point],
                    screen_space_positions[src_point + 1],
                    mu0,
                    mu1);

                r_intersections_factors[src_point] = float2(mu0, mu1);
              }
            });

        if (src_cyclic[src_curve]) {
          /* If the curve is cyclic, we need to check for the closing segment. */
          const int src_last_point = src_curve_points.last();

          float mu0;
          float mu1;
          r_nb_intersections[src_last_point] = intersections_with_segment(
              screen_space_positions[src_last_point],
              screen_space_positions[src_curve_points.first()],
              mu0,
              mu1);

          r_intersections_factors[src_last_point] = float2(mu0, mu1);
        }
      }
    });

    /* Compute total number of intersections. */
    int total_intersections = 0;
    for (const int src_point : src.points_range()) {
      total_intersections += r_nb_intersections[src_point];
    }
    return total_intersections;
  }

  /**
   * Checks if a point is inside the eraser or not.
   */
  inline bool contains_point(const float2 &point) const
  {
    return (math::distance_squared(point, this->mouse_position) <=
            this->eraser_radius * this->eraser_radius);
  }

  /**
   * Checks if each point is inside the eraser or not.
   *
   * \param screen_space_positions: input parameter containing the 2D positions of the geometry in
   * screen space.
   * \param points_range: ranges of points to check.
   *
   * \param r_point_inside: output parameter filled with booleans : true if the point is inside the
   * eraser, false otherwise.
   * \returns total number of inside points.
   *
   * Note that for the two output arrays the last element may contain intersections if the
   * corresponding curve is cyclic.
   */
  int compute_points_inside(const Span<float2> screen_space_positions,
                            const IndexRange points_range,
                            MutableSpan<bool> r_point_inside) const
  {
    /* Check if points are inside the eraser. */
    threading::parallel_for(points_range, 1024, [&](const IndexRange src_points) {
      for (const int src_point : src_points) {
        const float2 pos_view = screen_space_positions[src_point];
        r_point_inside[src_point] = contains_point(pos_view);
      }
    });
    /* Compute total number of points inside the eraser. */
    int total_points_inside = 0;
    for (const int src_point : points_range) {
      total_points_inside += r_point_inside[src_point] ? 1 : 0;
    }

    return total_points_inside;
  }

  bool hard_eraser(const bke::CurvesGeometry &src,
                   const Span<float2> screen_space_positions,
                   bke::CurvesGeometry &dst,
                   const bool keep_caps) const
  {
    const VArray<bool> src_cyclic = src.cyclic();
    const int src_points_num = src.points_num();
    const int src_curves_num = src.curves_num();
    const OffsetIndices<int> src_points_by_curve = src.points_by_curve();

    /* Compute intersections between the eraser and the curves in the source domain. */
    Array<int> nb_intersections(src_points_num, 0);
    Array<float2> src_intersections_parameters(src_points_num);
    const int total_intersections = intersections_with_curves(
        src, screen_space_positions, nb_intersections, src_intersections_parameters);

    /* Check if points are inside the eraser. */
    Array<bool> is_point_inside(src_points_num, false);
    const int total_points_inside = compute_points_inside(
        screen_space_positions, src.points_range(), is_point_inside);

    /* Total number of points in the destination :
     *   - intersections with the eraser are added,
     *   - points that are inside the erase are removed.
     */
    const int dst_points_num = src_points_num + total_intersections - total_points_inside;

    if ((total_intersections == 0) && (total_points_inside == 0)) {
      /* Return early if nothing to change. */
      return false;
    }

    if (dst_points_num == 0) {
      /* Return early if no points left. */
      dst.resize(0, 0);
      return true;
    }

    /* Set the intersection parameters in the destination domain : a pair of int and float numbers
     * for which the integer is the index of the corresponding segment in the source curves, and
     * the float part is the (0,1) factor representing its position in the segment.
     */
    Array<std::pair<int, float>> dst_points_parameters(dst_points_num);
    Array<bool> is_cut(dst_points_num, false);
    Array<int> src_pivot_point(src_curves_num, -1);
    Array<int> dst_interm_curves_offsets(src_curves_num + 1, 0);
    int dst_point = -1;
    for (const int src_curve : src.curves_range()) {
      const IndexRange src_points = src_points_by_curve[src_curve];

      for (const int src_point : src_points) {
        if (!is_point_inside[src_point]) {
          /* Add a point from the source : the factor is only the index in the source. */
          dst_points_parameters[++dst_point] = {src_point, 0.0};
        }
        if (nb_intersections[src_point] > 0) {
          float mu0 = src_intersections_parameters[src_point].x;
          float mu1 = src_intersections_parameters[src_point].y;

          if (IN_RANGE(mu0, 0, 1)) {
            /* Add an intersection with the eraser and mark it as a cut. */
            dst_points_parameters[++dst_point] = {src_point, mu0};
            is_cut[dst_point] = true;
          }
          if (IN_RANGE(mu1, 0, 1)) {
            /* Add an intersection with the eraser and mark it as a cut. */
            dst_points_parameters[++dst_point] = {src_point, mu1};
            is_cut[dst_point] = true;
          }

          /* For cyclic curves, mark the pivot point as the last intersection with the eraser
           * that starts a new segment in the destination.
           */
          if (src_cyclic[src_curve] &&
              (is_point_inside[src_point] || (nb_intersections[src_point] == 2))) {
            src_pivot_point[src_curve] = dst_point;
          }
        }
      }
      /* We store intermediate curve offsets represent an intermediate state of the destination
       * curves before cutting the curves at eraser's intersection. Thus, it contains the same
       * number of curves than in the source, but the offsets are different, because points may
       * have been added or removed. */
      dst_interm_curves_offsets[src_curve + 1] = dst_point + 1;
    }

    /* Cyclic curves. */
    Array<bool> src_now_cyclic(src_curves_num);
    threading::parallel_for(src.curves_range(), 4096, [&](const IndexRange src_curves) {
      for (const int src_curve : src_curves) {
        const int pivot_point = src_pivot_point[src_curve];

        if (pivot_point == -1) {
          /* Either the curve was not cyclic or it wasn't cut : no need to change it. */
          src_now_cyclic[src_curve] = src_cyclic[src_curve];
          continue;
        }

        /* A cyclic curve was cut :
         *  - this curve is not cyclic anymore,
         *  - and we have to shift points to keep the closing segment.
         */
        src_now_cyclic[src_curve] = false;

        const int dst_interm_first = dst_interm_curves_offsets[src_curve];
        const int dst_interm_last = dst_interm_curves_offsets[src_curve + 1];
        std::rotate(dst_points_parameters.begin() + dst_interm_first,
                    dst_points_parameters.begin() + pivot_point,
                    dst_points_parameters.begin() + dst_interm_last);
        std::rotate(is_cut.begin() + dst_interm_first,
                    is_cut.begin() + pivot_point,
                    is_cut.begin() + dst_interm_last);
      }
    });

    /* Compute the destination curve offsets. */
    Vector<int> dst_curves_offset;
    Vector<int> dst_to_src_curve;
    dst_curves_offset.append(0);
    for (int src_curve : src.curves_range()) {
      const IndexRange dst_points(dst_interm_curves_offsets[src_curve],
                                  dst_interm_curves_offsets[src_curve + 1] -
                                      dst_interm_curves_offsets[src_curve]);
      int length_of_current = 0;

      for (int dst_point : dst_points) {
        const int src_point = dst_points_parameters[dst_point].first;
        if ((length_of_current > 0) && is_cut[dst_point] && is_point_inside[src_point]) {
          /* This is the new first point of a curve. */
          dst_curves_offset.append(dst_point);
          dst_to_src_curve.append(src_curve);
          length_of_current = 0;
        }
        ++length_of_current;
      }

      if (length_of_current != 0) {
        /* End of a source curve. */
        dst_curves_offset.append(dst_points.one_after_last());
        dst_to_src_curve.append(src_curve);
      }
    }
    const int dst_curves_num = dst_curves_offset.size() - 1;

    /* Create the new curves geometry. */
    dst.resize(dst_points_num, dst_curves_num);
    array_utils::copy(dst_curves_offset.as_span(), dst.offsets_for_write());

    /* Attributes. */
    const bke::AttributeAccessor src_attributes = src.attributes();
    bke::MutableAttributeAccessor dst_attributes = dst.attributes_for_write();
    const bke::AnonymousAttributePropagationInfo propagation_info{};

    /* Copy curves attributes. */
    for (bke::AttributeTransferData &attribute : bke::retrieve_attributes_for_transfer(
             src_attributes, dst_attributes, ATTR_DOMAIN_MASK_CURVE, propagation_info, {"cyclic"}))
    {
      bke::attribute_math::gather(attribute.src, dst_to_src_curve, attribute.dst.span);
      attribute.dst.finish();
    }
    array_utils::gather(
        src_now_cyclic.as_span(), dst_to_src_curve.as_span(), dst.cyclic_for_write());

    /* Display intersections with flat caps. */
    const OffsetIndices<int> dst_points_by_curve = dst.points_by_curve();
    if (!keep_caps) {
      bke::SpanAttributeWriter<int8_t> dst_start_caps =
          dst_attributes.lookup_or_add_for_write_span<int8_t>("start_cap", ATTR_DOMAIN_CURVE);
      bke::SpanAttributeWriter<int8_t> dst_end_caps =
          dst_attributes.lookup_or_add_for_write_span<int8_t>("end_cap", ATTR_DOMAIN_CURVE);

      threading::parallel_for(dst.curves_range(), 4096, [&](const IndexRange dst_curves) {
        for (const int dst_curve : dst_curves) {
          const IndexRange dst_curve_points = dst_points_by_curve[dst_curve];
          if (is_cut[dst_curve_points.first()]) {
            dst_start_caps.span[dst_curve] = GP_STROKE_CAP_TYPE_FLAT;
          }
          if (is_cut[dst_curve_points.last()]) {
            dst_end_caps.span[dst_curve] = GP_STROKE_CAP_TYPE_FLAT;
          }
        }
      });

      dst_start_caps.finish();
      dst_end_caps.finish();
    }

    /* Copy/Interpolate point attributes. */
    for (bke::AttributeTransferData &attribute : bke::retrieve_attributes_for_transfer(
             src_attributes, dst_attributes, ATTR_DOMAIN_MASK_POINT, propagation_info))
    {
      bke::attribute_math::convert_to_static_type(attribute.dst.span.type(), [&](auto dummy) {
        using T = decltype(dummy);
        auto src_attr = attribute.src.typed<T>();
        auto dst_attr = attribute.dst.span.typed<T>();

        threading::parallel_for(dst.curves_range(), 512, [&](const IndexRange dst_curves) {
          for (const int dst_curve : dst_curves) {
            const IndexRange dst_curve_points = dst_points_by_curve[dst_curve];

            const int src_curve = dst_to_src_curve[dst_curve];
            const IndexRange src_curve_points = src_points_by_curve[src_curve];

            threading::parallel_for(dst_curve_points, 4096, [&](const IndexRange dst_points) {
              for (const int dst_point : dst_points) {
                const int src_point = dst_points_parameters[dst_point].first;

                if (!is_cut[dst_point]) {
                  dst_attr[dst_point] = src_attr[src_point];
                  continue;
                }

                const float src_pt_factor = dst_points_parameters[dst_point].second;

                /* Compute the endpoint of the segment in the source domain.
                 * Note that if this is the closing segment of a cyclic curve, then the
                 * endpoint of the segment in the first point of the curve. */
                const int src_next_point = (src_point == src_curve_points.last()) ?
                                               src_curve_points.first() :
                                               (src_point + 1);

                dst_attr[dst_point] = bke::attribute_math::mix2<T>(
                    src_pt_factor, src_attr[src_point], src_attr[src_next_point]);
              }
            });
          }
        });

        attribute.dst.finish();
      });
    }

    return true;
  }

  bool stroke_eraser(const bke::CurvesGeometry &src,
                     const Span<float2> screen_space_positions,
                     bke::CurvesGeometry &dst) const
  {
    const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
    const VArray<bool> src_cyclic = src.cyclic();

    IndexMaskMemory memory;
    const IndexMask strokes_to_keep = IndexMask::from_predicate(
        src.curves_range(), GrainSize(256), memory, [&](const int64_t src_curve) {
          const IndexRange src_curve_points = src_points_by_curve[src_curve];

          /* If any segment of the stroke is closer to the eraser than its radius, then remove the
           * stroke. */
          for (const int src_point : src_curve_points.drop_back(1)) {
            const float dist_to_eraser = dist_to_line_segment_v2(
                this->mouse_position,
                screen_space_positions[src_point],
                screen_space_positions[src_point + 1]);
            if (dist_to_eraser < this->eraser_radius) {
              return false;
            }
          }

          if (src_cyclic[src_curve]) {
            const float dist_to_eraser = dist_to_line_segment_v2(
                this->mouse_position,
                screen_space_positions[src_curve_points.first()],
                screen_space_positions[src_curve_points.last()]);
            if (dist_to_eraser < this->eraser_radius) {
              return false;
            }
          }

          return true;
        });

    if (strokes_to_keep.size() == src.curves_num()) {
      return false;
    }

    dst = bke::curves_copy_curve_selection(src, strokes_to_keep, {});
    return true;
  }

  void execute(EraseOperation &self, const bContext &C, const InputSample &extension_sample)
  {
    using namespace blender::bke::greasepencil;
    Scene *scene = CTX_data_scene(&C);
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
    ARegion *region = CTX_wm_region(&C);
    Object *obact = CTX_data_active_object(&C);
    Object *ob_eval = DEG_get_evaluated_object(depsgraph, obact);

    Paint *paint = &scene->toolsettings->gp_paint->paint;
    Brush *brush = BKE_paint_brush(paint);

    /* Get the tool's data. */
    this->mouse_position = extension_sample.mouse_position;
    this->eraser_radius = self.radius;
    if (BKE_brush_use_size_pressure(brush)) {
      this->eraser_radius *= BKE_curvemapping_evaluateF(
          brush->gpencil_settings->curve_strength, 0, extension_sample.pressure);
    }

    /* Get the grease pencil drawing. */
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(obact->data);

    bool changed = false;
    const auto execute_eraser_on_drawing = [&](int drawing_index, Drawing &drawing) {
      const bke::CurvesGeometry &src = drawing.strokes();

      /* Evaluated geometry. */
      bke::crazyspace::GeometryDeformation deformation =
          bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
              ob_eval, *obact, drawing_index);

      /* Compute screen space positions. */
      Array<float2> screen_space_positions(src.points_num());
      threading::parallel_for(src.points_range(), 4096, [&](const IndexRange src_points) {
        for (const int src_point : src_points) {
          ED_view3d_project_float_global(region,
                                         deformation.positions[src_point],
                                         screen_space_positions[src_point],
                                         V3D_PROJ_TEST_NOP);
        }
      });

      /* Erasing operator. */
      bke::CurvesGeometry dst;
      bool erased = false;
      switch (self.eraser_mode) {
        case GP_BRUSH_ERASER_STROKE:
          erased = stroke_eraser(src, screen_space_positions, dst);
          break;
        case GP_BRUSH_ERASER_HARD:
          erased = hard_eraser(src, screen_space_positions, dst, self.keep_caps);
          break;
        case GP_BRUSH_ERASER_SOFT:
          // To be implemented
          return;
      }

      if (erased) {
        /* Set the new geometry. */
        drawing.geometry.wrap() = std::move(dst);
        drawing.tag_topology_changed();
        changed = true;
      }
    };

    if (self.active_layer_only) {
      /* Erase only on the drawing at the current frame of the active layer. */
      const Layer *active_layer = grease_pencil.get_active_layer();
      Drawing *drawing = grease_pencil.get_editable_drawing_at(active_layer, scene->r.cfra);

      if (drawing == nullptr) {
        return;
      }

      execute_eraser_on_drawing(active_layer->drawing_index_at(scene->r.cfra), *drawing);
    }
    else {
      /* Erase on all editable drawings. */
      grease_pencil.foreach_editable_drawing(scene->r.cfra, execute_eraser_on_drawing);
    }

    if (changed) {
      DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil);
    }
  }
};

void EraseOperation::on_stroke_begin(const bContext &C, const InputSample & /*start_sample*/)
{
  Scene *scene = CTX_data_scene(&C);
  Paint *paint = BKE_paint_get_active_from_context(&C);
  Brush *brush = BKE_paint_brush(paint);

  BLI_assert(brush->gpencil_settings != nullptr);

  BKE_curvemapping_init(brush->gpencil_settings->curve_strength);

  this->radius = BKE_brush_size_get(scene, brush);
  this->eraser_mode = eGP_BrushEraserMode(brush->gpencil_settings->eraser_mode);
  this->keep_caps = ((brush->gpencil_settings->flag & GP_BRUSH_ERASER_KEEP_CAPS) != 0);
  this->active_layer_only = ((brush->gpencil_settings->flag & GP_BRUSH_ACTIVE_LAYER_ONLY) != 0);
}

void EraseOperation::on_stroke_extended(const bContext &C, const InputSample &extension_sample)
{
  EraseOperationExecutor executor{C};
  executor.execute(*this, C, extension_sample);
}

void EraseOperation::on_stroke_done(const bContext & /*C*/) {}

std::unique_ptr<GreasePencilStrokeOperation> new_erase_operation()
{
  return std::make_unique<EraseOperation>();
}

}  // namespace blender::ed::sculpt_paint::greasepencil
