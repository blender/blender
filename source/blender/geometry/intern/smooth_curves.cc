/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"

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
    if constexpr (std::is_same_v<T, float> || std::is_same_v<T, float2> ||
                  std::is_same_v<T, float3>)
    {
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

  curves_to_smooth.foreach_index(GrainSize(512), [&](const int curve_i) {
    Vector<std::byte> orig_data;
    const IndexRange points = points_by_curve[curve_i];

    IndexMaskMemory memory;
    const IndexMask selection_mask = IndexMask::from_bools(points, point_selection, memory);
    if (selection_mask.is_empty()) {
      return;
    }

    selection_mask.foreach_range([&](const IndexRange range) {
      GMutableSpan dst_data = attribute_data.slice(range);

      orig_data.resize(dst_data.size_in_bytes());
      dst_data.type().copy_assign_n(dst_data.data(), orig_data.data(), range.size());
      const GSpan src_data(dst_data.type(), orig_data.data(), range.size());

      gaussian_blur_1D(src_data,
                       iterations,
                       VArray<float>::ForSpan(influences.slice(range)),
                       smooth_ends,
                       keep_shape,
                       cyclic[curve_i],
                       dst_data);
    });
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
                         VArray<float>::ForSingle(influence, points_by_curve.total_size()),
                         smooth_ends,
                         keep_shape,
                         attribute_data);
}

}  // namespace blender::geometry
