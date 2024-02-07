/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_geom.h"
#include "BLI_task.hh"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_grease_pencil.h"
#include "BKE_grease_pencil.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.hh"
#include "DNA_brush_enums.h"

#include "ED_grease_pencil.hh"
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

  int2 mouse_position_pixels{};
  int64_t eraser_squared_radius_pixels{};

  EraseOperationExecutor(const bContext & /*C*/) {}

  /**
   * Computes the intersections between a 2D line segment and a circle with integer values.
   *
   * \param s0, s1: endpoints of the segment.
   * \param center: center of the circle,
   * \param radius_2: squared radius of the circle.
   *
   * \param r_mu0: (output) signed distance from \a s0 to the first intersection, if it exists.
   * \param r_mu1: (output) signed distance from \a s0 to the second  intersection, if it exists.
   *
   * All intersections with the infinite line of the segment are considered.
   *
   * \returns the number of intersection found.
   */
  static int8_t intersections_segment_circle_integers(const int2 &s0,
                                                      const int2 &s1,
                                                      const int2 &center,
                                                      const int64_t radius_2,
                                                      int64_t &r_mu0,
                                                      int64_t &r_mu1)
  {
    const int64_t d_s0_center = math::distance_squared(s0, center);
    const int64_t a = math::distance_squared(s0, s1);
    const int64_t b = 2 * math::dot(s0 - center, s1 - s0);
    const int64_t c = d_s0_center - radius_2;
    const int64_t i = b * b - 4 * a * c;

    if (i < 0) {
      /* No intersections. */
      return 0;
    }

    const int64_t segment_length = math::distance(s0, s1);

    if (i == 0) {
      /* One intersection. */
      const float mu0_f = -b / (2.0f * a);
      r_mu0 = round_fl_to_int(mu0_f * segment_length);
      return 1;
    }

    /* Two intersections. */
    const float i_sqrt = sqrtf(float(i));
    const float mu0_f = (-b + i_sqrt) / (2.0f * a);
    const float mu1_f = (-b - i_sqrt) / (2.0f * a);

    r_mu0 = round_fl_to_int(mu0_f * segment_length);
    r_mu1 = round_fl_to_int(mu1_f * segment_length);

    return 2;
  }

  struct SegmentCircleIntersection {
    /* Position of the intersection in the segment. */
    float factor = -1.0f;

    /* True if the intersection corresponds to an inside/outside transition with respect to the
     * circle, false if it corresponds to an outside/inside transition. */
    bool inside_outside_intersection = false;

    /* An intersection is considered valid if it lies inside of the segment, i.e.
     * if its factor is in (0,1). */
    bool is_valid() const
    {
      return IN_RANGE(factor, 0.0f, 1.0f);
    }
  };
  enum class PointCircleSide { Outside, OutsideInsideBoundary, InsideOutsideBoundary, Inside };

  /**
   * Computes the intersection between the eraser tool and a 2D segment, using integer values.
   * Also computes if the endpoints of the segment lie inside/outside, or in the boundary of the
   * eraser.
   *
   * \param point, point_after: coordinates of the first (resp. second) endpoint in the segment.
   *
   * \param squared_radius: squared radius of the brush in pixels.
   *
   * \param r_mu0, r_mu0: (output) factor of the two intersections if they exists, otherwise (-1).
   *
   * \param point_side, point_after_side: (output) enum describing where the first (resp. second)
   * endpoint lies relatively to the eraser: inside, outside or at the boundary of the eraser.
   *
   * \returns total number of intersections lying inside the segment (ie whose factor is in ]0,1[).
   *
   * Note that the eraser is represented as a circle, and thus there can be only 0, 1 or 2
   * intersections with a segment.
   */
  int8_t segment_intersections_and_points_sides(const int2 &point,
                                                const int2 &point_after,
                                                const int64_t squared_radius,
                                                float &r_mu0,
                                                float &r_mu1,
                                                PointCircleSide &r_point_side,
                                                PointCircleSide &r_point_after_side) const
  {

    /* Compute the integer values of the intersection. */
    const int64_t segment_length = math::distance(point, point_after);
    int64_t mu0 = -1;
    int64_t mu1 = -1;
    const int8_t nb_intersections = intersections_segment_circle_integers(
        point, point_after, this->mouse_position_pixels, squared_radius, mu0, mu1);

    if (nb_intersections != 2) {
      /* No intersection with the infinite line : none of the points are inside the circle.
       * If only one intersection was found, then the eraser is tangential to the line, we don't
       * account for intersections in this case.
       */
      r_mu0 = r_mu1 = -1.0f;
      r_point_side = PointCircleSide::Outside;
      r_point_after_side = PointCircleSide::Outside;
      return 0;
    }

    if (mu0 > mu1) {
      std::swap(mu0, mu1);
    }

    /* Compute on which side of the segment each intersection lies.
     * -1 : before or at the first endpoint,
     *  0 : in-between the endpoints,
     *  1 : after or at the last endpoint.
     */
    const int8_t side_mu0 = (mu0 <= 0) ? (-1) : ((mu0 >= segment_length) ? 1 : 0);
    const int8_t side_mu1 = (mu1 <= 0) ? (-1) : ((mu1 >= segment_length) ? 1 : 0);

    /* The endpoints are on the circle's boundary if one of the intersection falls exactly on them.
     */
    r_point_side = (mu0 == 0) ? PointCircleSide::OutsideInsideBoundary :
                                ((mu1 == 0) ? PointCircleSide::InsideOutsideBoundary :
                                              PointCircleSide::Inside);
    r_point_after_side = (mu0 == segment_length) ?
                             PointCircleSide::OutsideInsideBoundary :
                             ((mu1 == segment_length) ? PointCircleSide::InsideOutsideBoundary :
                                                        PointCircleSide::Inside);

    /* Compute the normalized position of the intersection in the curve. */
    r_mu0 = mu0 / float(segment_length);
    r_mu1 = mu1 / float(segment_length);

    const bool is_mu0_inside = (side_mu0 == 0);
    const bool is_mu1_inside = (side_mu1 == 0);
    if (!is_mu0_inside && !is_mu1_inside) {
      /* None of the intersection lie within the segment the infinite line. */

      if (side_mu0 == side_mu1) {
        /* If they are on the same side of the line, then none of the point are inside the circle.
         */
        r_point_side = PointCircleSide::Outside;
        r_point_after_side = PointCircleSide::Outside;
        return 0;
      }

      /* If they are on different sides of the line, then both points are inside the circle, or in
       * the boundary. */
      return 0;
    }

    if (is_mu0_inside && is_mu1_inside) {
      /* Both intersections lie within the segment, none of the points are inside the circle. */
      r_point_side = PointCircleSide::Outside;
      r_point_after_side = PointCircleSide::Outside;
      return 2;
    }

    /* Only one intersection lies within the segment. Only one point should be erased, depending on
     * the side of the other intersection. */
    const int8_t side_outside_intersection = is_mu0_inside ? side_mu1 : side_mu0;

    /* If the other intersection lies before the first endpoint, the first endpoint is inside. */
    r_point_side = (side_outside_intersection == -1) ? r_point_side : PointCircleSide::Outside;
    r_point_after_side = (side_outside_intersection == 1) ? r_point_after_side :
                                                            PointCircleSide::Outside;

    if (is_mu1_inside) {
      std::swap(r_mu0, r_mu1);
    }
    return 1;
  }

  /**
   * Compute intersections between the eraser and the input \a src Curves Geometry.
   * Also computes if the points of the geometry lie inside/outside, or in the boundary of the
   * eraser.
   *
   * \param screen_space_positions: 2D positions of the geometry in screen space.
   *
   * \param intersections_max_per_segment: maximum number of intersections per-segment.
   *
   * \param r_point_side: (output) for each point in the source, enum describing where the point
   * lies relatively to the eraser: inside, outside or at the boundary of the eraser.
   *
   * \param r_intersections: (output) array containing all the intersections found in the curves
   * geometry. The size of the array should be `src.points_num*intersections_max_per_segment`.
   * Initially all intersections are set as invalid, and the function fills valid intersections at
   * an offset of `src_point*intersections_max_per_segment`.
   *
   * \returns total number of intersections found.
   */
  int curves_intersections_and_points_sides(
      const bke::CurvesGeometry &src,
      const Span<float2> screen_space_positions,
      const int intersections_max_per_segment,
      MutableSpan<PointCircleSide> r_point_side,
      MutableSpan<SegmentCircleIntersection> r_intersections) const
  {

    const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
    const VArray<bool> src_cyclic = src.cyclic();

    Array<int2> screen_space_positions_pixel(src.points_num());
    threading::parallel_for(src.points_range(), 1024, [&](const IndexRange src_points) {
      for (const int src_point : src_points) {
        const float2 pos = screen_space_positions[src_point];
        screen_space_positions_pixel[src_point] = int2(round_fl_to_int(pos[0]),
                                                       round_fl_to_int(pos[1]));
      }
    });

    threading::parallel_for(src.curves_range(), 512, [&](const IndexRange src_curves) {
      for (const int src_curve : src_curves) {
        const IndexRange src_curve_points = src_points_by_curve[src_curve];

        if (src_curve_points.size() == 1) {
          /* One-point stroke : just check if the point is inside the eraser. */
          const int src_point = src_curve_points.first();
          const int64_t squared_distance = math::distance_squared(
              this->mouse_position_pixels, screen_space_positions_pixel[src_point]);

          /* Note: We don't account for boundaries here, since we are not going to split any
           * curve. */
          r_point_side[src_point] = (squared_distance <= this->eraser_squared_radius_pixels) ?
                                        PointCircleSide::Inside :
                                        PointCircleSide::Outside;
          continue;
        }

        for (const int src_point : src_curve_points.drop_back(1)) {
          SegmentCircleIntersection inter0;
          SegmentCircleIntersection inter1;

          const int8_t nb_inter = segment_intersections_and_points_sides(
              screen_space_positions_pixel[src_point],
              screen_space_positions_pixel[src_point + 1],
              this->eraser_squared_radius_pixels,
              inter0.factor,
              inter1.factor,
              r_point_side[src_point],
              r_point_side[src_point + 1]);

          if (nb_inter > 0) {
            const int intersection_offset = src_point * intersections_max_per_segment;

            inter0.inside_outside_intersection = (inter0.factor > inter1.factor);
            r_intersections[intersection_offset + 0] = inter0;

            if (nb_inter > 1) {
              inter1.inside_outside_intersection = true;
              r_intersections[intersection_offset + 1] = inter1;
            }
          }
        }

        if (src_cyclic[src_curve]) {
          /* If the curve is cyclic, we need to check for the closing segment. */
          const int src_last_point = src_curve_points.last();
          const int src_first_point = src_curve_points.first();

          SegmentCircleIntersection inter0;
          SegmentCircleIntersection inter1;

          const int8_t nb_inter = segment_intersections_and_points_sides(
              screen_space_positions_pixel[src_last_point],
              screen_space_positions_pixel[src_first_point],
              this->eraser_squared_radius_pixels,
              inter0.factor,
              inter1.factor,
              r_point_side[src_last_point],
              r_point_side[src_first_point]);

          if (nb_inter > 0) {
            const int intersection_offset = src_last_point * intersections_max_per_segment;

            inter0.inside_outside_intersection = (inter0.factor > inter1.factor);
            r_intersections[intersection_offset + 0] = inter0;

            if (nb_inter > 1) {
              inter1.inside_outside_intersection = true;
              r_intersections[intersection_offset + 1] = inter1;
            }
          }
        }
      }
    });

    /* Compute total number of intersections. */
    int total_intersections = 0;
    for (const SegmentCircleIntersection &intersection : r_intersections) {
      if (intersection.is_valid()) {
        total_intersections++;
      }
    }

    return total_intersections;
  }

  /**
   * Structure describing a point in the destination relatively to the source.
   * If a point in the destination \a is_src_point, then it corresponds
   * exactly to the point at \a src_point index in the source geometry.
   * Otherwise, it is a linear combination of points at \a src_point and \a src_next_point in the
   * source geometry, with the given \a factor.
   * A point in the destination is a \a cut if it splits the source curves geometry, meaning it is
   * the first point of a new curve in the destination.
   */
  struct PointTransferData {
    int src_point;
    int src_next_point;
    float factor;
    bool is_src_point;
    bool is_cut;
  };

  /**
   * Computes a \a dst curves geometry by applying a change of topology from a \a src curves
   * geometry.
   * The change of topology is described by \a src_to_dst_points, which size should be
   * equal to the number of points in the source.
   * For each point in the source, the corresponding vector in \a src_to_dst_points contains a set
   * of destination points (PointTransferData), which can correspond to points of the source, or
   * linear combination of them. Note that this vector can be empty, if we want to remove points
   * for example. Curves can also be split if a destination point is marked as a cut.
   *
   * \returns an array containing the same elements as \a src_to_dst_points, but in the destination
   * points domain.
   */
  static Array<PointTransferData> compute_topology_change(
      const bke::CurvesGeometry &src,
      bke::CurvesGeometry &dst,
      const Span<Vector<PointTransferData>> src_to_dst_points,
      const bool keep_caps)
  {
    const int src_curves_num = src.curves_num();
    const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
    const VArray<bool> src_cyclic = src.cyclic();

    int dst_points_num = 0;
    for (const Vector<PointTransferData> &src_transfer_data : src_to_dst_points) {
      dst_points_num += src_transfer_data.size();
    }
    if (dst_points_num == 0) {
      dst.resize(0, 0);
      return Array<PointTransferData>(0);
    }

    /* Set the intersection parameters in the destination domain : a pair of int and float
     * numbers for which the integer is the index of the corresponding segment in the
     * source curves, and the float part is the (0,1) factor representing its position in
     * the segment.
     */
    Array<PointTransferData> dst_transfer_data(dst_points_num);

    Array<int> src_pivot_point(src_curves_num, -1);
    Array<int> dst_interm_curves_offsets(src_curves_num + 1, 0);
    int dst_point = -1;
    for (const int src_curve : src.curves_range()) {
      const IndexRange src_points = src_points_by_curve[src_curve];

      for (const int src_point : src_points) {
        for (const PointTransferData &dst_point_transfer : src_to_dst_points[src_point]) {
          if (dst_point_transfer.is_src_point) {
            dst_transfer_data[++dst_point] = dst_point_transfer;
            continue;
          }

          /* Add an intersection with the eraser and mark it as a cut. */
          dst_transfer_data[++dst_point] = dst_point_transfer;

          /* For cyclic curves, mark the pivot point as the last intersection with the eraser
           * that starts a new segment in the destination.
           */
          if (src_cyclic[src_curve] && dst_point_transfer.is_cut) {
            src_pivot_point[src_curve] = dst_point;
          }
        }
      }
      /* We store intermediate curve offsets represent an intermediate state of the
       * destination curves before cutting the curves at eraser's intersection. Thus, it
       * contains the same number of curves than in the source, but the offsets are
       * different, because points may have been added or removed. */
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
        std::rotate(dst_transfer_data.begin() + dst_interm_first,
                    dst_transfer_data.begin() + pivot_point,
                    dst_transfer_data.begin() + dst_interm_last);
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

        if ((length_of_current > 0) && dst_transfer_data[dst_point].is_cut) {
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
    if (dst_curves_num == 0) {
      dst.resize(0, 0);
      return dst_transfer_data;
    }

    /* Build destination curves geometry. */
    dst.resize(dst_points_num, dst_curves_num);
    array_utils::copy(dst_curves_offset.as_span(), dst.offsets_for_write());
    const OffsetIndices<int> dst_points_by_curve = dst.points_by_curve();

    /* Attributes. */
    const bke::AttributeAccessor src_attributes = src.attributes();
    bke::MutableAttributeAccessor dst_attributes = dst.attributes_for_write();
    const bke::AnonymousAttributePropagationInfo propagation_info{};

    /* Copy curves attributes. */
    bke::gather_attributes(src_attributes,
                           bke::AttrDomain::Curve,
                           propagation_info,
                           {"cyclic"},
                           dst_to_src_curve,
                           dst_attributes);
    if (src_cyclic.get_if_single().value_or(true)) {
      array_utils::gather(
          src_now_cyclic.as_span(), dst_to_src_curve.as_span(), dst.cyclic_for_write());
    }

    dst.update_curve_types();

    /* Display intersections with flat caps. */
    if (!keep_caps) {
      bke::SpanAttributeWriter<int8_t> dst_start_caps =
          dst_attributes.lookup_or_add_for_write_span<int8_t>("start_cap", bke::AttrDomain::Curve);
      bke::SpanAttributeWriter<int8_t> dst_end_caps =
          dst_attributes.lookup_or_add_for_write_span<int8_t>("end_cap", bke::AttrDomain::Curve);

      threading::parallel_for(dst.curves_range(), 4096, [&](const IndexRange dst_curves) {
        for (const int dst_curve : dst_curves) {
          const IndexRange dst_curve_points = dst_points_by_curve[dst_curve];
          if (dst_transfer_data[dst_curve_points.first()].is_cut) {
            dst_start_caps.span[dst_curve] = GP_STROKE_CAP_TYPE_FLAT;
          }

          if (dst_curve == dst_curves.last()) {
            continue;
          }

          const PointTransferData &next_point_transfer =
              dst_transfer_data[dst_points_by_curve[dst_curve + 1].first()];

          if (next_point_transfer.is_cut) {
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

        threading::parallel_for(dst.points_range(), 4096, [&](const IndexRange dst_points) {
          for (const int dst_point : dst_points) {
            const PointTransferData &point_transfer = dst_transfer_data[dst_point];
            if (point_transfer.is_src_point) {
              dst_attr[dst_point] = src_attr[point_transfer.src_point];
            }
            else {
              dst_attr[dst_point] = bke::attribute_math::mix2<T>(
                  point_transfer.factor,
                  src_attr[point_transfer.src_point],
                  src_attr[point_transfer.src_next_point]);
            }
          }
        });

        attribute.dst.finish();
      });
    }

    return dst_transfer_data;
  }

  /* The hard eraser cuts out the curves at their intersection with the eraser, and removes
   * everything that lies in-between two consecutive intersections. Note that intersections are
   * computed using integers (pixel-space) to avoid floating-point approximation errors. */

  bool hard_eraser(const bke::CurvesGeometry &src,
                   const Span<float2> screen_space_positions,
                   bke::CurvesGeometry &dst,
                   const bool keep_caps) const
  {
    const VArray<bool> src_cyclic = src.cyclic();
    const int src_points_num = src.points_num();

    /* For the hard erase, we compute with a circle, so there can only be a maximum of two
     * intersection per segment. */
    const int intersections_max_per_segment = 2;

    /* Compute intersections between the eraser and the curves in the source domain. */
    Array<PointCircleSide> src_point_side(src_points_num, PointCircleSide::Outside);
    Array<SegmentCircleIntersection> src_intersections(src_points_num *
                                                       intersections_max_per_segment);
    curves_intersections_and_points_sides(src,
                                          screen_space_positions,
                                          intersections_max_per_segment,
                                          src_point_side,
                                          src_intersections);

    Array<Vector<PointTransferData>> src_to_dst_points(src_points_num);
    const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
    for (const int src_curve : src.curves_range()) {
      const IndexRange src_points = src_points_by_curve[src_curve];

      for (const int src_point : src_points) {
        Vector<PointTransferData> &dst_points = src_to_dst_points[src_point];
        const int src_next_point = (src_point == src_points.last()) ? src_points.first() :
                                                                      (src_point + 1);
        const PointCircleSide point_side = src_point_side[src_point];

        /* Add the source point only if it does not lie inside of the eraser. */
        if (point_side != PointCircleSide::Inside) {
          dst_points.append({src_point,
                             src_next_point,
                             0.0f,
                             true,
                             (point_side == PointCircleSide::InsideOutsideBoundary)});
        }

        /* Add all intersections with the eraser. */
        const IndexRange src_point_intersections(src_point * intersections_max_per_segment,
                                                 intersections_max_per_segment);
        for (const SegmentCircleIntersection &intersection :
             src_intersections.as_span().slice(src_point_intersections))
        {
          if (!intersection.is_valid()) {
            /* Stop at the first non valid intersection. */
            break;
          }
          dst_points.append({src_point,
                             src_next_point,
                             intersection.factor,
                             false,
                             intersection.inside_outside_intersection});
        }
      }
    }

    compute_topology_change(src, dst, src_to_dst_points, keep_caps);

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
        src.curves_range(), GrainSize(256), memory, [&](const int src_curve) {
          const IndexRange src_curve_points = src_points_by_curve[src_curve];

          /* One-point stroke : remove the stroke if the point lies inside of the eraser. */
          if (src_curve_points.size() == 1) {
            const float2 &point_pos = screen_space_positions[src_curve_points.first()];
            const float dist_to_eraser = math::distance(point_pos, this->mouse_position);
            return !(dist_to_eraser < eraser_radius);
          }

          /* If any segment of the stroke is closer to the eraser than its radius, then remove
           * the stroke. */
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

    this->mouse_position_pixels = int2(round_fl_to_int(mouse_position[0]),
                                       round_fl_to_int(mouse_position[1]));
    const int64_t eraser_radius_pixels = round_fl_to_int(eraser_radius);
    this->eraser_squared_radius_pixels = eraser_radius_pixels * eraser_radius_pixels;

    /* Get the grease pencil drawing. */
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(obact->data);

    bool changed = false;
    const auto execute_eraser_on_drawing = [&](const int layer_index,
                                               const int frame_number,
                                               Drawing &drawing) {
      const Layer &layer = *grease_pencil.layers()[layer_index];
      const bke::CurvesGeometry &src = drawing.strokes();

      /* Evaluated geometry. */
      bke::crazyspace::GeometryDeformation deformation =
          bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
              ob_eval, *obact, layer_index, frame_number);

      /* Compute screen space positions. */
      Array<float2> screen_space_positions(src.points_num());
      threading::parallel_for(src.points_range(), 4096, [&](const IndexRange src_points) {
        for (const int src_point : src_points) {
          ED_view3d_project_float_global(region,
                                         math::transform_point(layer.to_world_space(*ob_eval),
                                                               deformation.positions[src_point]),
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
      const Layer &active_layer = *grease_pencil.get_active_layer();
      Drawing *drawing = grease_pencil.get_editable_drawing_at(active_layer, scene->r.cfra);

      if (drawing == nullptr) {
        return;
      }

      execute_eraser_on_drawing(
          active_layer.drawing_index_at(scene->r.cfra), scene->r.cfra, *drawing);
    }
    else {
      /* Erase on all editable drawings. */
      const Array<ed::greasepencil::MutableDrawingInfo> drawings =
          ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
      threading::parallel_for_each(
          drawings, [&](const ed::greasepencil::MutableDrawingInfo &info) {
            execute_eraser_on_drawing(info.layer_index, info.frame_number, info.drawing);
          });
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
