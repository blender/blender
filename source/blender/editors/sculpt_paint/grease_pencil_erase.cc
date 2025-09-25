/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "BLI_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_base.hh"
#include "BLI_math_geom.h"
#include "BLI_task.hh"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"
#include "BKE_paint.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_material_types.h"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "GEO_curves_remove_and_split.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class EraseOperation : public GreasePencilStrokeOperation {
  friend struct EraseOperationExecutor;

 private:
  /* Eraser is used by the draw tool temporarily. */
  bool temp_eraser_ = false;

  bool keep_caps_ = false;
  float radius_ = 50.0f;
  float strength_ = 0.1f;
  eGP_BrushEraserMode eraser_mode_ = GP_BRUSH_ERASER_HARD;
  bool active_layer_only_ = false;

  Set<GreasePencilDrawing *> affected_drawings_;

 public:
  EraseOperation(bool temp_use_eraser = false) : temp_eraser_(temp_use_eraser) {}
  ~EraseOperation() override = default;

  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext &C) override;
};

struct SegmentCircleIntersection {
  /* Position of the intersection in the segment.
   * Note: we use a value > 1.0f as initial value so that sorting intersection by increasing
   * factor can directly put the invalid ones at the end. */
  float factor = 2.0f;

  /* True if the intersection corresponds to an inside/outside transition with respect to the
   * circle, false if it corresponds to an outside/inside transition. */
  bool inside_outside_intersection = false;

  int ring_index = -1;

  /* An intersection is considered valid if it lies inside of the segment, i.e.
   * if its factor is in (0,1). */
  bool is_valid() const
  {
    return IN_RANGE(factor, 0.0f, 1.0f);
  }
};
enum class PointCircleSide { Outside, OutsideInsideBoundary, InsideOutsideBoundary, Inside };

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct EraseOperationExecutor {

  float2 mouse_position;
  float eraser_radius;
  float eraser_strength;
  Brush *brush_;

  int2 mouse_position_pixels;
  int64_t eraser_squared_radius_pixels;

  /* Threshold below which points are considered as transparent and thus shall be removed. */
  static constexpr float opacity_threshold = 0.05f;

  EraseOperationExecutor(const bContext & /*C*/) {}

  struct EraserRing {
    float radius;
    int64_t squared_radius;
    float opacity;
    bool hard_erase{false};
  };

  /**
   * Computes the intersections between a 2D line segment and a circle with integer values.
   *
   * \param s0, s1: endpoints of the segment.
   * \param center: center of the circle,
   * \param radius_2: squared radius of the circle.
   *
   * \param r_mu0: (output) signed distance from \a s0 to the first intersection, if it exists.
   * \param r_mu1: (output) signed distance from \a s0 to the second intersection, if it exists.
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

    /* If points are close together there is no direction vector.
     * Since the solution multiplies by this factor for integer math,
     * the valid case of degenerate segments inside the circle needs special handling. */
    if (a == 0) {
      const int64_t i = -4 * c;
      if (i < 0) {
        /* No intersections. */
        return 0;
      }
      if (i == 0) {
        /* One intersection. */
        r_mu0 = 0.0f;
        return 1;
      }
      /* Two intersections. */
      const float i_sqrt = math::sqrt(float(i));
      r_mu0 = math::round(i_sqrt / 2.0f);
      r_mu1 = math::round(-i_sqrt / 2.0f);
      return 2;
    }

    const int64_t i = b * b - 4 * a * c;

    if (i < 0) {
      /* No intersections. */
      return 0;
    }

    const int64_t segment_length = math::distance(s0, s1);

    if (i == 0) {
      /* One intersection. */
      const float mu0_f = -b / (2.0f * a);
      r_mu0 = math::round(mu0_f * segment_length);
      return 1;
    }

    /* Two intersections. */
    const float i_sqrt = math::sqrt(float(i));
    const float mu0_f = (-b + i_sqrt) / (2.0f * a);
    const float mu1_f = (-b - i_sqrt) / (2.0f * a);

    r_mu0 = math::round(mu0_f * segment_length);
    r_mu1 = math::round(mu1_f * segment_length);

    return 2;
  }

  /**
   * Computes the intersection between the eraser brush and a 2D segment, using integer values.
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
      const Span<EraserRing> rings,
      MutableSpan<std::pair<int, PointCircleSide>> r_point_ring,
      MutableSpan<SegmentCircleIntersection> r_intersections) const
  {
    /* Each ring can generate zero, one or two intersections per segment. */
    const int intersections_max_per_segment = rings.size() * 2;
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
          int ring_index = 0;
          for (const EraserRing &eraser_point : rings) {
            const int src_point = src_curve_points.first();
            const int64_t squared_distance = math::distance_squared(
                this->mouse_position_pixels, screen_space_positions_pixel[src_point]);

            /* NOTE: We don't account for boundaries here, since we are not going to split any
             * curve. */
            if ((r_point_ring[src_point].first == -1) &&
                (squared_distance <= eraser_point.squared_radius))
            {
              r_point_ring[src_point] = {ring_index, PointCircleSide::Inside};
            }
            ++ring_index;
          }
          continue;
        }

        for (const int src_point : src_curve_points.drop_back(1)) {
          int ring_index = 0;
          int intersection_offset = src_point * intersections_max_per_segment - 1;

          for (const EraserRing &eraser_point : rings) {
            SegmentCircleIntersection inter0;
            SegmentCircleIntersection inter1;

            inter0.ring_index = ring_index;
            inter1.ring_index = ring_index;

            PointCircleSide point_side;
            PointCircleSide point_after_side;

            const int8_t nb_inter = segment_intersections_and_points_sides(
                screen_space_positions_pixel[src_point],
                screen_space_positions_pixel[src_point + 1],
                eraser_point.squared_radius,
                inter0.factor,
                inter1.factor,
                point_side,
                point_after_side);

            /* The point belongs in the ring of the smallest radius circle it is in.
             * Since our rings are stored in increasing radius order, it corresponds to the first
             * ring that contains the point. We only include the InsideOutside boundary of the
             * ring, that is why we do not check for OutsideInsideBoundary.
             */
            if ((r_point_ring[src_point].first == -1) &&
                ELEM(point_side, PointCircleSide::Inside, PointCircleSide::InsideOutsideBoundary))
            {
              r_point_ring[src_point] = {ring_index, point_side};
            }

            if (src_point + 1 == src_curve_points.last()) {
              if ((r_point_ring[src_point + 1].first == -1) &&
                  ELEM(point_after_side,
                       PointCircleSide::Inside,
                       PointCircleSide::InsideOutsideBoundary))
              {
                r_point_ring[src_point + 1] = {ring_index, point_after_side};
              }
            }

            if (nb_inter > 0) {
              inter0.inside_outside_intersection = (inter0.factor > inter1.factor);
              r_intersections[++intersection_offset] = inter0;

              if (nb_inter > 1) {
                inter1.inside_outside_intersection = true;
                r_intersections[++intersection_offset] = inter1;
              }
            }

            ++ring_index;
          }
        }

        if (src_cyclic[src_curve]) {
          /* If the curve is cyclic, we need to check for the closing segment. */
          const int src_last_point = src_curve_points.last();
          const int src_first_point = src_curve_points.first();
          int ring_index = 0;
          int intersection_offset = src_last_point * intersections_max_per_segment - 1;

          for (const EraserRing &eraser_point : rings) {
            SegmentCircleIntersection inter0;
            SegmentCircleIntersection inter1;

            inter0.ring_index = ring_index;
            inter1.ring_index = ring_index;

            PointCircleSide point_side;
            PointCircleSide point_after_side;

            const int8_t nb_inter = segment_intersections_and_points_sides(
                screen_space_positions_pixel[src_last_point],
                screen_space_positions_pixel[src_first_point],
                eraser_point.squared_radius,
                inter0.factor,
                inter1.factor,
                point_side,
                point_after_side);

            /* Note : we don't need to set the point side here, since it was already set by the
             * former loop. */

            if (nb_inter > 0) {
              inter0.inside_outside_intersection = (inter0.factor > inter1.factor);
              r_intersections[++intersection_offset] = inter0;

              if (nb_inter > 1) {
                inter1.inside_outside_intersection = true;
                r_intersections[++intersection_offset] = inter1;
              }
            }

            ++ring_index;
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

  static bool skip_strokes_with_locked_material(
      Object &ob,
      const int src_curve,
      const IndexRange &src_points,
      const VArray<int> stroke_material,
      const VArray<float> &point_opacity,
      Array<Vector<ed::greasepencil::PointTransferData>> &src_to_dst_points)
  {
    const MaterialGPencilStyle *mat = BKE_gpencil_material_settings(
        &ob, stroke_material[src_curve] + 1);

    if ((mat->flag & GP_MATERIAL_LOCKED) == 0) {
      return false;
    }

    for (const int src_point : src_points) {
      const int src_next_point = (src_point == src_points.last()) ? src_points.first() :
                                                                    (src_point + 1);
      src_to_dst_points[src_point].append(
          {src_point, src_next_point, 0.0f, true, false, point_opacity[src_point]});
    }
    return true;
  }

  /* The hard eraser cuts out the curves at their intersection with the eraser, and removes
   * everything that lies in-between two consecutive intersections. Note that intersections are
   * computed using integers (pixel-space) to avoid floating-point approximation errors. */

  bool hard_eraser(Object &ob,
                   const bke::CurvesGeometry &src,
                   const Span<float2> screen_space_positions,
                   bke::CurvesGeometry &dst,
                   const bool keep_caps) const
  {
    const VArray<bool> src_cyclic = src.cyclic();
    const int src_points_num = src.points_num();

    /* For the hard erase, we compute with a circle, so there can only be a maximum of two
     * intersection per segment. */
    const Vector<EraserRing> eraser_rings(
        1, {this->eraser_radius, this->eraser_squared_radius_pixels, 0.0f, true});
    const int intersections_max_per_segment = eraser_rings.size() * 2;

    /* Compute intersections between the eraser and the curves in the source domain. */
    Array<std::pair<int, PointCircleSide>> src_point_ring(src_points_num,
                                                          {-1, PointCircleSide::Outside});
    Array<SegmentCircleIntersection> src_intersections(src_points_num *
                                                       intersections_max_per_segment);
    curves_intersections_and_points_sides(
        src, screen_space_positions, eraser_rings, src_point_ring, src_intersections);

    Array<Vector<ed::greasepencil::PointTransferData>> src_to_dst_points(src_points_num);

    const VArray<int> &stroke_material = *src.attributes().lookup_or_default<int>(
        "material_index", bke::AttrDomain::Curve, 0);
    const VArray<float> &point_opacity = *src.attributes().lookup_or_default<float>(
        "opacity", bke::AttrDomain::Point, 1.0f);

    const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
    for (const int src_curve : src.curves_range()) {
      const IndexRange src_points = src_points_by_curve[src_curve];

      if (skip_strokes_with_locked_material(
              ob, src_curve, src_points, stroke_material, point_opacity, src_to_dst_points))
      {
        continue;
      }

      for (const int src_point : src_points) {
        Vector<ed::greasepencil::PointTransferData> &dst_points = src_to_dst_points[src_point];
        const int src_next_point = (src_point == src_points.last()) ? src_points.first() :
                                                                      (src_point + 1);
        const PointCircleSide point_side = src_point_ring[src_point].second;

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

    ed::greasepencil::compute_topology_change(src, dst, src_to_dst_points, keep_caps);

    return true;
  }

  Vector<EraserRing> compute_piecewise_linear_falloff() const
  {
    /* The changes in opacity implied by the soft eraser are described by a falloff curve
     * mapping. Abscissa of the curve is the normalized distance to the brush, and ordinate of
     * the curve is the strength of the eraser.
     *
     * To apply this falloff as precisely as possible, we compute a set of "rings" to the brush,
     * meaning a set of samples in the curve mapping in between which the strength of the eraser
     * is applied linearly.
     *
     * In other words, we compute a minimal set of samples that describe the falloff curve as a
     * polyline. */

    /* First, distance-based sampling with a small pixel distance.
     * The samples are stored in increasing radius order. */
    const int step_pixels = 2;
    int nb_samples = math::round(this->eraser_radius / step_pixels);
    Vector<EraserRing> eraser_rings(nb_samples);
    for (const int sample_index : eraser_rings.index_range()) {
      const int64_t sampled_distance = (sample_index + 1) * step_pixels;

      EraserRing &ring = eraser_rings[sample_index];
      ring.radius = sampled_distance;
      ring.squared_radius = sampled_distance * sampled_distance;
      ring.opacity = 1.0 - this->eraser_strength *
                               BKE_brush_curve_strength(
                                   this->brush_, float(sampled_distance), this->eraser_radius);
    }

    /* Then, prune samples that are under the opacity threshold. */
    Array<bool> prune_sample(nb_samples, false);
    for (const int sample_index : eraser_rings.index_range()) {
      EraserRing &sample = eraser_rings[sample_index];

      if (sample_index == nb_samples - 1) {
        /* If this is the last samples, we need to keep it at the same position (it corresponds
         * to the brush overall radius). It is a cut if the opacity is under the threshold. */
        sample.hard_erase = (sample.opacity < opacity_threshold);
        continue;
      }

      EraserRing next_sample = eraser_rings[sample_index + 1];

      /* If both samples are under the threshold, prune it !
       * If none of them are under the threshold, leave them as they are.
       */
      if ((sample.opacity < opacity_threshold) == (next_sample.opacity < opacity_threshold)) {
        prune_sample[sample_index] = (sample.opacity < opacity_threshold);
        continue;
      }

      /* Otherwise, shift the sample to the spot where the opacity is exactly at the threshold.
       * This way we don't remove larger opacity values in-between the samples. */
      const EraserRing &sample_after = eraser_rings[sample_index + 1];

      const float t = (opacity_threshold - sample.opacity) /
                      (sample_after.opacity - sample.opacity);

      const int64_t radius = math::round(
          math::interpolate(sample.radius, float(sample_after.radius), t));

      sample.radius = radius;
      sample.squared_radius = radius * radius;
      sample.opacity = opacity_threshold;
      sample.hard_erase = !(next_sample.opacity < opacity_threshold);
    }

    for (const int rev_sample_index : eraser_rings.index_range()) {
      const int sample_index = nb_samples - rev_sample_index - 1;
      if (prune_sample[sample_index]) {
        eraser_rings.remove(sample_index);
      }
    }

    /* Finally, simplify the array to have a minimal set of samples. */
    nb_samples = eraser_rings.size();

    const auto opacity_distance = [&](int64_t first_index, int64_t last_index, int64_t index) {
      /* Distance function for the simplification algorithm.
       * It is computed as the difference in opacity that may result from removing the
       * samples inside the range. */
      const EraserRing &sample_first = eraser_rings[first_index];
      const EraserRing &sample_last = eraser_rings[last_index];
      const EraserRing &sample = eraser_rings[index];

      /* If we were to remove the samples between sample_first and sample_last, then the opacity
       * at sample.radius would be a linear interpolation between the opacities in the endpoints
       * of the range, with a parameter depending on the distance between radii. That is what we
       * are computing here. */
      const float t = (sample.radius - sample_first.radius) /
                      (sample_last.radius - sample_first.radius);
      const float linear_opacity = math::interpolate(sample_first.opacity, sample_last.opacity, t);

      return math::abs(sample.opacity - linear_opacity);
    };
    Array<bool> simplify_sample(nb_samples, false);
    const float distance_threshold = 0.1f;
    ed::greasepencil::ramer_douglas_peucker_simplify(
        eraser_rings.index_range(), distance_threshold, opacity_distance, simplify_sample);

    for (const int rev_sample_index : eraser_rings.index_range()) {
      const int sample_index = nb_samples - rev_sample_index - 1;
      if (simplify_sample[sample_index]) {
        eraser_rings.remove(sample_index);
      }
    }

    return eraser_rings;
  }

  /**
   * The soft eraser decreases the opacity of the points it hits.
   * The new opacity is computed as a minimum between the current opacity and
   * a falloff function of the distance of the point to the center of the eraser.
   * If the opacity of a point falls below a threshold, then the point is removed from the
   * curves.
   */
  bool soft_eraser(Object &ob,
                   const blender::bke::CurvesGeometry &src,
                   const Span<float2> screen_space_positions,
                   blender::bke::CurvesGeometry &dst,
                   const bool keep_caps)
  {
    using namespace blender::bke;
    const std::string opacity_attr = "opacity";

    /* The soft eraser changes the opacity of the strokes underneath it using a curve falloff. We
     * sample this curve to get a set of rings in the brush. */
    const Vector<EraserRing> eraser_rings = compute_piecewise_linear_falloff();
    const int intersections_max_per_segment = eraser_rings.size() * 2;

    /* Compute intersections between the source curves geometry and all the rings of the eraser.
     */
    const int src_points_num = src.points_num();
    Array<std::pair<int, PointCircleSide>> src_point_ring(src_points_num,
                                                          {-1, PointCircleSide::Outside});
    Array<SegmentCircleIntersection> src_intersections(src_points_num *
                                                       intersections_max_per_segment);
    curves_intersections_and_points_sides(
        src, screen_space_positions, eraser_rings, src_point_ring, src_intersections);

    /* Function to get the resulting opacity at a specific point in the source. */
    const VArray<float> &src_opacity = *src.attributes().lookup_or_default<float>(
        opacity_attr, bke::AttrDomain::Point, 1.0f);
    const VArray<int> &stroke_material = *src.attributes().lookup_or_default<int>(
        "material_index", bke::AttrDomain::Curve, 0);

    const auto compute_opacity = [&](const int src_point) {
      const float distance = math::distance(screen_space_positions[src_point],
                                            this->mouse_position);
      const float brush_strength = this->eraser_strength *
                                   BKE_brush_curve_strength(
                                       this->brush_, distance, this->eraser_radius);
      return math::clamp(src_opacity[src_point] - brush_strength, 0.0f, 1.0f);
    };

    /* Compute the map of points in the destination.
     * For each point in the source, we create a vector of destination points. Destination points
     * can either be directly a point of the source, or a point inside a segment of the source. A
     * destination point can also carry the role of a "cut", meaning it is going to be the first
     * point of a new curve split into the destination. */
    Array<Vector<ed::greasepencil::PointTransferData>> src_to_dst_points(src_points_num);
    const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
    for (const int src_curve : src.curves_range()) {
      const IndexRange src_points = src_points_by_curve[src_curve];

      if (skip_strokes_with_locked_material(
              ob, src_curve, src_points, stroke_material, src_opacity, src_to_dst_points))
      {
        continue;
      }
      for (const int src_point : src_points) {
        Vector<ed::greasepencil::PointTransferData> &dst_points = src_to_dst_points[src_point];
        const int src_next_point = (src_point == src_points.last()) ? src_points.first() :
                                                                      (src_point + 1);

        /* Get the ring into which the source point lies.
         * If the point is completely outside of the eraser, then the index is (-1). */
        const int point_ring = src_point_ring[src_point].first;
        const bool ring_is_cut = (point_ring != -1) && eraser_rings[point_ring].hard_erase;
        const PointCircleSide point_side = src_point_ring[src_point].second;

        const bool point_is_cut = ring_is_cut &&
                                  (point_side == PointCircleSide::InsideOutsideBoundary);
        const bool remove_point = ring_is_cut && (point_side == PointCircleSide::Inside);
        if (!remove_point) {
          dst_points.append(
              {src_point, src_next_point, 0.0f, true, point_is_cut, compute_opacity(src_point)});
        }

        const IndexRange src_point_intersections(src_point * intersections_max_per_segment,
                                                 intersections_max_per_segment);

        std::sort(src_intersections.begin() + src_point_intersections.first(),
                  src_intersections.begin() + src_point_intersections.last() + 1,
                  [](SegmentCircleIntersection a, SegmentCircleIntersection b) {
                    return a.factor < b.factor;
                  });

        /* Add all intersections with the rings. */
        for (const SegmentCircleIntersection &intersection :
             src_intersections.as_span().slice(src_point_intersections))
        {
          if (!intersection.is_valid()) {
            /* Stop at the first non valid intersection. */
            break;
          }

          const EraserRing &ring = eraser_rings[intersection.ring_index];
          const bool is_cut = intersection.inside_outside_intersection && ring.hard_erase;
          const float initial_opacity = math::interpolate(
              src_opacity[src_point], src_opacity[src_next_point], intersection.factor);

          const float opacity = math::max(0.0f, math::min(initial_opacity, ring.opacity));

          /* Avoid the accumulation of multiple cuts. */
          if (is_cut && !dst_points.is_empty() && dst_points.last().is_cut) {
            dst_points.remove_last();
          }

          dst_points.append(
              {src_point, src_next_point, intersection.factor, false, is_cut, opacity});
        }
      }
    }

    const Array<ed::greasepencil::PointTransferData> dst_points = compute_topology_change(
        src, dst, src_to_dst_points, keep_caps);

    /* Set opacity. */
    bke::MutableAttributeAccessor dst_attributes = dst.attributes_for_write();

    if (bke::SpanAttributeWriter<float> dst_opacity =
            dst_attributes.lookup_or_add_for_write_span<float>(opacity_attr,
                                                               bke::AttrDomain::Point))
    {
      threading::parallel_for(dst.points_range(), 4096, [&](const IndexRange dst_points_range) {
        for (const int dst_point_index : dst_points_range) {
          const ed::greasepencil::PointTransferData &dst_point = dst_points[dst_point_index];
          dst_opacity.span[dst_point_index] = dst_point.opacity;
        }
      });
      dst_opacity.finish();
    }

    SpanAttributeWriter<bool> dst_inserted = dst_attributes.lookup_or_add_for_write_span<bool>(
        "_eraser_inserted", bke::AttrDomain::Point);
    BLI_assert(dst_inserted);
    const OffsetIndices<int> &dst_points_by_curve = dst.points_by_curve();
    threading::parallel_for(dst.curves_range(), 4096, [&](const IndexRange dst_curves_range) {
      for (const int dst_curve : dst_curves_range) {
        IndexRange dst_points_range = dst_points_by_curve[dst_curve];

        dst_inserted.span[dst_points_range.first()] = false;
        dst_inserted.span[dst_points_range.last()] = false;

        if (dst_points_range.size() < 3) {
          continue;
        }

        for (const int dst_point_index : dst_points_range.drop_back(1).drop_front(1)) {
          const ed::greasepencil::PointTransferData &dst_point = dst_points[dst_point_index];
          dst_inserted.span[dst_point_index] |= !dst_point.is_src_point;
        }
      }
    });
    dst_inserted.finish();

    return true;
  }

  bool stroke_eraser(Object &ob,
                     const bke::CurvesGeometry &src,
                     const Span<float2> screen_space_positions,
                     bke::CurvesGeometry &dst) const
  {
    const OffsetIndices<int> src_points_by_curve = src.points_by_curve();
    const VArray<bool> src_cyclic = src.cyclic();

    IndexMaskMemory memory;
    const VArray<int> &stroke_materials = *src.attributes().lookup_or_default<int>(
        "material_index", bke::AttrDomain::Curve, 0);
    const IndexMask strokes_to_keep = IndexMask::from_predicate(
        src.curves_range(), GrainSize(256), memory, [&](const int src_curve) {
          const MaterialGPencilStyle *mat = BKE_gpencil_material_settings(
              &ob, stroke_materials[src_curve] + 1);
          /* Keep strokes with locked material. */
          if (mat->flag & GP_MATERIAL_LOCKED) {
            return true;
          }

          const IndexRange src_curve_points = src_points_by_curve[src_curve];

          /* One-point stroke : remove the stroke if the point lies inside of the eraser. */
          if (src_curve_points.size() == 1) {
            const float2 &point_pos = screen_space_positions[src_curve_points.first()];
            const float dist_to_eraser = math::distance(point_pos, this->mouse_position);
            return !(dist_to_eraser < this->eraser_radius);
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
    Object *ob_eval = DEG_get_evaluated(depsgraph, obact);

    Paint *paint = &scene->toolsettings->gp_paint->paint;
    Brush *brush = BKE_paint_brush(paint);

    if (brush->gpencil_brush_type == GPAINT_BRUSH_TYPE_DRAW) {
      brush = BKE_paint_eraser_brush(paint);
    }

    /* Get the brush data. */
    this->mouse_position = extension_sample.mouse_position;
    this->eraser_radius = self.radius_;
    this->eraser_strength = self.strength_;

    if (BKE_brush_use_size_pressure(brush)) {
      this->eraser_radius *= BKE_curvemapping_evaluateF(
          brush->gpencil_settings->curve_strength, 0, extension_sample.pressure);
    }
    if (BKE_brush_use_alpha_pressure(brush)) {
      this->eraser_strength *= BKE_curvemapping_evaluateF(
          brush->gpencil_settings->curve_strength, 0, extension_sample.pressure);
    }
    this->brush_ = brush;

    this->mouse_position_pixels = int2(round_fl_to_int(this->mouse_position.x),
                                       round_fl_to_int(this->mouse_position.y));
    const int64_t eraser_radius_pixels = round_fl_to_int(this->eraser_radius);
    this->eraser_squared_radius_pixels = eraser_radius_pixels * eraser_radius_pixels;

    /* Get the grease pencil drawing. */
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(obact->data);

    bool changed = false;
    const auto execute_eraser_on_drawing = [&](const int layer_index, Drawing &drawing) {
      const Layer &layer = grease_pencil.layer(layer_index);
      const bke::CurvesGeometry &src = drawing.strokes();

      /* Evaluated geometry. */
      bke::crazyspace::GeometryDeformation deformation =
          bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
              ob_eval, *obact, drawing);

      /* Compute screen space positions. */
      Array<float2> screen_space_positions(src.points_num());
      threading::parallel_for(src.points_range(), 4096, [&](const IndexRange src_points) {
        for (const int src_point : src_points) {
          const int result = ED_view3d_project_float_global(
              region,
              math::transform_point(layer.to_world_space(*ob_eval),
                                    deformation.positions[src_point]),
              screen_space_positions[src_point],
              V3D_PROJ_TEST_CLIP_NEAR | V3D_PROJ_TEST_CLIP_FAR);
          if (result != V3D_PROJ_RET_OK) {
            /* Set the screen space position to a impossibly far coordinate for all the points
             * that are outside near/far clipping planes, this is to prevent accidental
             * intersections with strokes not visibly present in the camera. */
            screen_space_positions[src_point] = float2(1e20);
          }
        }
      });

      /* Erasing operator. */
      bke::CurvesGeometry dst;
      bool erased = false;
      switch (self.eraser_mode_) {
        case GP_BRUSH_ERASER_STROKE:
          erased = stroke_eraser(*obact, src, screen_space_positions, dst);
          break;
        case GP_BRUSH_ERASER_HARD:
          erased = hard_eraser(*obact, src, screen_space_positions, dst, self.keep_caps_);
          break;
        case GP_BRUSH_ERASER_SOFT:
          erased = soft_eraser(*obact, src, screen_space_positions, dst, self.keep_caps_);
          break;
      }

      if (erased) {
        /* Set the new geometry. */
        drawing.geometry.wrap() = std::move(dst);
        drawing.tag_topology_changed();
        changed = true;
        self.affected_drawings_.add(&drawing);
      }
    };

    if (self.active_layer_only_) {
      /* Erase only on the drawing at the current frame of the active layer. */
      if (!grease_pencil.has_active_layer()) {
        return;
      }
      const Layer &active_layer = *grease_pencil.get_active_layer();
      Drawing *drawing = grease_pencil.get_editable_drawing_at(active_layer, scene->r.cfra);

      if (drawing == nullptr) {
        return;
      }

      execute_eraser_on_drawing(*grease_pencil.get_layer_index(active_layer), *drawing);
    }
    else {
      /* Erase on all editable drawings. */
      const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
          ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
      for (const ed::greasepencil::MutableDrawingInfo &info : drawings) {
        execute_eraser_on_drawing(info.layer_index, info.drawing);
      }
    }

    if (changed) {
      DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil);
    }
  }
};

void EraseOperation::on_stroke_begin(const bContext &C, const InputSample & /*start_sample*/)
{
  Paint *paint = BKE_paint_get_active_from_context(&C);
  Brush *brush = BKE_paint_brush(paint);

  /* If we're using the draw tool to erase (e.g. while holding ctrl), then we should use the
   * eraser brush instead. */
  if (temp_eraser_) {
    Object *object = CTX_data_active_object(&C);
    GreasePencil *grease_pencil = static_cast<GreasePencil *>(object->data);

    radius_ = paint->eraser_brush->size / 2.0f;
    grease_pencil->runtime->temp_eraser_size = radius_;
    grease_pencil->runtime->temp_use_eraser = true;

    brush = BKE_paint_eraser_brush(paint);
  }
  else {
    radius_ = brush->size / 2.0f;
  }

  if (brush->gpencil_settings == nullptr) {
    BKE_brush_init_gpencil_settings(brush);
  }
  BLI_assert(brush->gpencil_settings != nullptr);

  BKE_curvemapping_init(brush->curve_distance_falloff);
  BKE_curvemapping_init(brush->gpencil_settings->curve_strength);

  eraser_mode_ = eGP_BrushEraserMode(brush->gpencil_settings->eraser_mode);
  keep_caps_ = ((brush->gpencil_settings->flag & GP_BRUSH_ERASER_KEEP_CAPS) != 0);
  active_layer_only_ = ((brush->gpencil_settings->flag & GP_BRUSH_ACTIVE_LAYER_ONLY) != 0);
  strength_ = brush->alpha;
}

void EraseOperation::on_stroke_extended(const bContext &C, const InputSample &extension_sample)
{
  EraseOperationExecutor executor{C};
  executor.execute(*this, C, extension_sample);
}

static void simplify_opacities(blender::bke::CurvesGeometry &curves,
                               const VArray<float> &opacities,
                               const float epsilon)
{
  /* Simplify in between the ranges of inserted points. */
  const VArray<bool> point_was_inserted = *curves.attributes().lookup<bool>(
      "_eraser_inserted", bke::AttrDomain::Point);
  BLI_assert(point_was_inserted);
  IndexMaskMemory memory;
  const IndexMask inserted_points = IndexMask::from_bools(point_was_inserted, memory);

  /* Distance function for the simplification algorithm.
   * It is computed as the difference in opacity that may result from removing the
   * samples inside the range. */
  const Span<float3> positions = curves.positions();
  const auto opacity_distance = [&](int64_t first_index, int64_t last_index, int64_t index) {
    const float3 &s0 = positions[first_index];
    const float3 &s1 = positions[last_index];
    const float segment_length = math::distance(s0, s1);
    if (segment_length < 1e-6) {
      return 0.0f;
    }
    const float t = math::distance(s0, positions[index]) / segment_length;
    const float linear_opacity = math::interpolate(
        opacities[first_index], opacities[last_index], t);
    return math::abs(opacities[index] - linear_opacity);
  };

  Array<bool> dissolve_points(curves.points_num(), false);
  inserted_points.foreach_range([&](const IndexRange &range) {
    const IndexRange range_to_simplify(range.one_before_start(), range.size() + 2);
    ed::greasepencil::ramer_douglas_peucker_simplify(
        range_to_simplify, epsilon, opacity_distance, dissolve_points);
  });

  /* Remove the points. */
  const IndexMask points_to_dissolve = IndexMask::from_bools(dissolve_points, memory);
  curves.remove_points(points_to_dissolve, {});
}

static void remove_points_with_low_opacity(blender::bke::CurvesGeometry &curves,
                                           const VArray<float> &opacities,
                                           const float epsilon)
{
  IndexMaskMemory memory;
  const IndexMask points_to_remove_and_split = IndexMask::from_predicate(
      curves.points_range(), GrainSize(4096), memory, [&](const int64_t point) {
        return opacities[point] < epsilon;
      });
  curves = geometry::remove_points_and_split(curves, points_to_remove_and_split);
}

void EraseOperation::on_stroke_done(const bContext &C)
{
  Object *object = CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  if (temp_eraser_) {
    /* If we're using the draw tool to temporarily erase, then we need to reset the
     * `temp_use_eraser` flag here. */
    grease_pencil.runtime->temp_use_eraser = false;
    grease_pencil.runtime->temp_eraser_size = 0.0f;
  }

  for (GreasePencilDrawing *drawing_ : affected_drawings_) {
    bke::greasepencil::Drawing &drawing = drawing_->wrap();

    if (drawing.strokes().attributes().contains("_eraser_inserted")) {
      simplify_opacities(drawing.strokes_for_write(), drawing.opacities(), 0.01f);
    }
    remove_points_with_low_opacity(drawing.strokes_for_write(), drawing.opacities(), 0.0001f);

    drawing.strokes_for_write().attributes_for_write().remove("_eraser_inserted");
    drawing.tag_topology_changed();
  }

  affected_drawings_.clear();

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil.id);
}

std::unique_ptr<GreasePencilStrokeOperation> new_erase_operation(const bool temp_eraser)
{
  return std::make_unique<EraseOperation>(temp_eraser);
}

}  // namespace blender::ed::sculpt_paint::greasepencil
