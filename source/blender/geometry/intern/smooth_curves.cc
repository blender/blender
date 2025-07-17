/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"

#include "BLI_array.hh"
#include "BLI_generic_span.hh"
#include "BLI_index_mask.hh"
#include "BLI_index_range.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "GEO_smooth_curves.hh"

namespace blender::geometry {

template<typename T>
static void gaussian_blur_1D(const Span<T> src,
                             const int iterations,
                             const VArray<float> &influence_by_point,
                             const bool smooth_ends,
                             const bool keep_shape,
                             const bool is_cyclic,
                             MutableSpan<T> dst)
{
  /**
   * 1D Gaussian-like smoothing function.
   *
   * NOTE: This is the algorithm used by #BKE_gpencil_stroke_smooth_point (legacy),
   *       but generalized and written in C++.
   *
   * This function uses a binomial kernel, which is the discrete version of gaussian blur.
   * The weight for a value at the relative index is:
   * `w = nCr(n, j + n/2) / 2^n = (n/1 * (n-1)/2 * ... * (n-j-n/2)/(j+n/2)) / 2^n`.
   * All weights together sum up to 1.
   * This is equivalent to doing multiple iterations of averaging neighbors,
   * where: `n = iterations * 2 and -n/2 <= j <= n/2`.
   *
   * Now the problem is that `nCr(n, j + n/2)` is very hard to compute for `n > 500`, since even
   * double precision isn't sufficient. A very good robust approximation for `n > 20` is:
   * `nCr(n, j + n/2) / 2^n = sqrt(2/(pi*n)) * exp(-2*j*j/n)`.
   *
   * `keep_shape` is a new option to stop the points from severely deforming.
   * It uses different partially negative weights.
   * `w = 2 * (nCr(n, j + n/2) / 2^n) - (nCr(3*n, j + n) / 2^(3*n))`
   * `  ~ 2 * sqrt(2/(pi*n)) * exp(-2*j*j/n) - sqrt(2/(pi*3*n)) * exp(-2*j*j/(3*n))`
   * All weights still sum up to 1.
   * Note that these weights only work because the averaging is done in relative coordinates.
   */

  BLI_assert(!src.is_empty());
  BLI_assert(src.size() == dst.size());

  /* Avoid computation if the there is just one point. */
  if (src.size() == 1) {
    return;
  }

  /* Weight Initialization. */
  const int n_half = keep_shape ? (iterations * iterations) / 8 + iterations :
                                  (iterations * iterations) / 4 + 2 * iterations + 12;
  double w = keep_shape ? 2.0 : 1.0;
  double w2 = keep_shape ?
                  (1.0 / M_SQRT3) * exp((2 * iterations * iterations) / double(n_half * 3)) :
                  0.0;
  Array<double> total_weight(src.size(), 0.0);

  const int64_t total_points = src.size();
  const int64_t last_pt = total_points - 1;

  auto is_end_and_fixed = [smooth_ends, is_cyclic, last_pt](int index) {
    return !smooth_ends && !is_cyclic && ELEM(index, 0, last_pt);
  };

  /* Initialize at zero. */
  threading::parallel_for(dst.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t index : range) {
      if (!is_end_and_fixed(index)) {
        dst[index] = T(0);
      }
    }
  });

  /* Compute weights. */
  for (const int64_t step : IndexRange(iterations)) {
    const int64_t offset = iterations - step;
    threading::parallel_for(dst.index_range(), 1024, [&](const IndexRange range) {
      for (const int64_t index : range) {
        /* Filter out endpoints. */
        if (is_end_and_fixed(index)) {
          continue;
        }

        double w_before = w - w2;
        double w_after = w - w2;

        /* Compute the neighboring points. */
        int64_t before = index - offset;
        int64_t after = index + offset;
        if (is_cyclic) {
          before = (before % total_points + total_points) % total_points;
          after = after % total_points;
        }
        else {
          if (!smooth_ends && (before < 0)) {
            w_before *= -before / float(index);
          }
          before = math::max(before, int64_t(0));

          if (!smooth_ends && (after > last_pt)) {
            w_after *= (after - (total_points - 1)) / float(total_points - 1 - index);
          }
          after = math::min(after, last_pt);
        }

        /* Add the neighboring values. */
        const T bval = src[before];
        const T aval = src[after];
        const T cval = src[index];

        dst[index] += (bval - cval) * w_before;
        dst[index] += (aval - cval) * w_after;

        /* Update the weight values. */
        total_weight[index] += w_before;
        total_weight[index] += w_after;
      }
    });

    w *= (n_half + offset) / double(n_half + 1 - offset);
    w2 *= (n_half * 3 + offset) / double(n_half * 3 + 1 - offset);
  }

  /* Normalize the weights. */
  devirtualize_varray(influence_by_point, [&](const auto influence_by_point) {
    threading::parallel_for(dst.index_range(), 1024, [&](const IndexRange range) {
      for (const int64_t index : range) {
        if (!is_end_and_fixed(index)) {
          total_weight[index] += w - w2;
          dst[index] = src[index] + influence_by_point[index] * dst[index] / total_weight[index];
        }
      }
    });
  });
}

void gaussian_blur_1D(const GSpan src,
                      const int iterations,
                      const VArray<float> &influence_by_point,
                      const bool smooth_ends,
                      const bool keep_shape,
                      const bool is_cyclic,
                      GMutableSpan dst)
{
  bke::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    /* Only allow smoothing of float, float2, or float3. */
    /* Reduces unnecessary code generation. */
    if constexpr (is_same_any_v<T, float, float2, float3>) {
      gaussian_blur_1D(src.typed<T>(),
                       iterations,
                       influence_by_point,
                       smooth_ends,
                       keep_shape,
                       is_cyclic,
                       dst.typed<T>());
    }
  });
}

void smooth_curve_attribute(const IndexMask &curves_to_smooth,
                            const OffsetIndices<int> points_by_curve,
                            const VArray<bool> &point_selection,
                            const VArray<bool> &cyclic,
                            const int iterations,
                            const VArray<float> &influence_by_point,
                            const bool smooth_ends,
                            const bool keep_shape,
                            GMutableSpan attribute_data)
{
  VArraySpan<float> influences(influence_by_point);

  auto smooth_points_range =
      [&](const IndexRange range, const bool cyclic, Vector<std::byte> &orig_data) {
        GMutableSpan dst_data = attribute_data.slice(range);
        orig_data.resize(dst_data.size_in_bytes());
        dst_data.type().copy_assign_n(dst_data.data(), orig_data.data(), range.size());
        const GSpan src_data(dst_data.type(), orig_data.data(), range.size());

        gaussian_blur_1D(src_data,
                         iterations,
                         VArray<float>::from_span(influences.slice(range)),
                         smooth_ends,
                         keep_shape,
                         cyclic,
                         dst_data);
      };

  curves_to_smooth.foreach_index(GrainSize(512), [&](const int curve_i) {
    Vector<std::byte> orig_data;
    const IndexRange points = points_by_curve[curve_i];

    IndexMaskMemory memory;
    const IndexMask selection_mask = IndexMask::from_bools(points, point_selection, memory);
    if (selection_mask.is_empty()) {
      return;
    }

    const std::optional<IndexRange> selection_range = selection_mask.to_range();
    if (selection_range && *selection_range == points) {
      smooth_points_range(points, cyclic[curve_i], orig_data);
    }
    else {
      selection_mask.foreach_range([&](const IndexRange range) {
        /* Individual ranges should be treated as non-cyclic. */
        smooth_points_range(range, false, orig_data);
      });
    }
  });
}

void smooth_curve_attribute(const IndexMask &curves_to_smooth,
                            const OffsetIndices<int> points_by_curve,
                            const VArray<bool> &point_selection,
                            const VArray<bool> &cyclic,
                            const int iterations,
                            const float influence,
                            const bool smooth_ends,
                            const bool keep_shape,
                            GMutableSpan attribute_data)
{
  smooth_curve_attribute(curves_to_smooth,
                         points_by_curve,
                         point_selection,
                         cyclic,
                         iterations,
                         VArray<float>::from_single(influence, points_by_curve.total_size()),
                         smooth_ends,
                         keep_shape,
                         attribute_data);
}

void smooth_curve_positions(bke::CurvesGeometry &curves,
                            const IndexMask &curves_to_smooth,
                            const int iterations,
                            const VArray<float> &influence_by_point,
                            const bool smooth_ends,
                            const bool keep_shape)
{
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();
  const VArray<bool> point_selection = *curves.attributes().lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Point, true);
  if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    bke::GSpanAttributeWriter positions = attributes.lookup_for_write_span("position");
    smooth_curve_attribute(curves_to_smooth,
                           points_by_curve,
                           point_selection,
                           cyclic,
                           iterations,
                           influence_by_point,
                           smooth_ends,
                           keep_shape,
                           positions.span);
    positions.finish();
  }
  else {
    IndexMaskMemory memory;
    const IndexMask bezier_curves_to_smooth = curves.indices_for_curve_type(
        CURVE_TYPE_BEZIER, curves_to_smooth, memory);

    /* Write the positions of the handles and the control points into a flat array.
     * This will smooth the handle positions together with the control point positions, because the
     * smoothing algorithm takes neighboring values to apply the gaussian smoothing to. */
    Array<float3> all_positions = bke::curves::bezier::retrieve_all_positions(
        curves, bezier_curves_to_smooth);

    VArraySpan<float> influences(influence_by_point);
    bezier_curves_to_smooth.foreach_index(GrainSize(512), [&](const int curve) {
      Vector<float3> orig_data;
      const IndexRange points = points_by_curve[curve];

      IndexMaskMemory memory;
      const IndexMask selection_mask = IndexMask::from_bools(points, point_selection, memory);
      if (selection_mask.is_empty()) {
        return;
      }

      selection_mask.foreach_range([&](const IndexRange range) {
        IndexRange positions_range(range.start() * 3, range.size() * 3);
        /* Ignore the left handle of the first point and the right handle of the last point. */
        if (!smooth_ends && !cyclic[curve]) {
          positions_range = positions_range.drop_front(1).drop_back(1);
        }
        MutableSpan<float3> dst_data = all_positions.as_mutable_span().slice(positions_range);

        orig_data.resize(dst_data.size());
        orig_data.as_mutable_span().copy_from(dst_data);

        /* The influence is mapped from handle+control point index to only control point index. */
        Array<float> point_influences(positions_range.size());
        if (!smooth_ends && !cyclic[curve]) {
          threading::parallel_for(
              positions_range.index_range(), 4096, [&](const IndexRange influences_range) {
                for (const int index : influences_range) {
                  /* Account for the left handle of the first
                   * point being ignored. */
                  point_influences[index] = influences.slice(range)[(index + 1) / 3];
                }
              });
        }
        else {
          threading::parallel_for(
              positions_range.index_range(), 4096, [&](const IndexRange influences_range) {
                for (const int index : influences_range) {
                  point_influences[index] = influences.slice(range)[index / 3];
                }
              });
        }

        gaussian_blur_1D(orig_data.as_span(),
                         iterations,
                         VArray<float>::from_span(point_influences.as_span()),
                         smooth_ends,
                         keep_shape,
                         cyclic[curve],
                         dst_data);
      });
    });

    /* Copy the resulting values from the flat array back into the three position attributes for
     * the left and right handles as well as the control points. */
    bke::curves::bezier::write_all_positions(curves, bezier_curves_to_smooth, all_positions);

    /* Smooth the other curve positions. */
    const IndexMask other_curves_to_smooth = bezier_curves_to_smooth.complement(
        curves.curves_range(), memory);
    if (!other_curves_to_smooth.is_empty()) {
      bke::GSpanAttributeWriter positions = attributes.lookup_for_write_span("position");
      smooth_curve_attribute(other_curves_to_smooth,
                             points_by_curve,
                             point_selection,
                             cyclic,
                             iterations,
                             influence_by_point,
                             smooth_ends,
                             keep_shape,
                             positions.span);
      positions.finish();
    }

    curves.calculate_bezier_auto_handles();
  }

  curves.tag_positions_changed();
}

void smooth_curve_positions(bke::CurvesGeometry &curves,
                            const IndexMask &curves_to_smooth,
                            const int iterations,
                            const float influence,
                            const bool smooth_ends,
                            const bool keep_shape)
{
  smooth_curve_positions(curves,
                         curves_to_smooth,
                         iterations,
                         VArray<float>::from_single(influence, curves.points_num()),
                         smooth_ends,
                         keep_shape);
}

}  // namespace blender::geometry
