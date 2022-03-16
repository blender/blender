/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"

namespace blender::bke::curves::catmull_rom {

int calculate_evaluated_size(const int size, const bool cyclic, const int resolution)
{
  const int eval_size = resolution * curve_segment_size(size, cyclic);
  /* If the curve isn't cyclic, one last point is added to the final point. */
  return (cyclic && size > 2) ? eval_size : eval_size + 1;
}

/* Adapted from Cycles #catmull_rom_basis_eval function. */
template<typename T>
static T calculate_basis(const T &a, const T &b, const T &c, const T &d, const float parameter)
{
  const float t = parameter;
  const float s = 1.0f - parameter;
  const float n0 = -t * s * s;
  const float n1 = 2.0f + t * t * (3.0f * t - 5.0f);
  const float n2 = 2.0f + s * s * (3.0f * s - 5.0f);
  const float n3 = -s * t * t;
  return 0.5f * (a * n0 + b * n1 + c * n2 + d * n3);
}

template<typename T>
static void evaluate_segment(const T &a, const T &b, const T &c, const T &d, MutableSpan<T> dst)
{
  const float step = 1.0f / dst.size();
  dst.first() = b;
  for (const int i : dst.index_range().drop_front(1)) {
    dst[i] = calculate_basis<T>(a, b, c, d, i * step);
  }
}

template<typename T>
static void interpolate_to_evaluated(const Span<T> src,
                                     const bool cyclic,
                                     const int resolution,
                                     MutableSpan<T> dst)

{
  BLI_assert(dst.size() == calculate_evaluated_size(src.size(), cyclic, resolution));

  /* - First deal with one and two point curves need special attention.
   * - Then evaluate the first and last segment(s) whose control points need to wrap around
   *   to the other side of the source array.
   * - Finally evaluate all of the segments in the middle in parallel. */

  if (src.size() == 1) {
    dst.first() = src.first();
    return;
  }
  if (src.size() == 2) {
    evaluate_segment(src.first(), src.first(), src.last(), src.last(), dst);
    dst.last() = src.last();
    return;
  }

  if (cyclic) {
    /* The first segment. */
    evaluate_segment(src.last(), src[0], src[1], src[2], dst.take_front(resolution));
    /* The second-to-last segment. */
    evaluate_segment(src.last(2),
                     src.last(1),
                     src.last(),
                     src.first(),
                     dst.take_back(resolution * 2).drop_back(resolution));
    /* The last segment. */
    evaluate_segment(src.last(1), src.last(), src[0], src[1], dst.take_back(resolution));
  }
  else {
    /* The first segment. */
    evaluate_segment(src[0], src[0], src[1], src[2], dst.take_front(resolution));
    /* The last segment. */
    evaluate_segment(
        src.last(2), src.last(1), src.last(), src.last(), dst.drop_back(1).take_back(resolution));
    /* The final point of the last segment. */
    dst.last() = src.last();
  }

  /* Evaluate every segment that isn't the first or last. */
  const int grain_size = std::max(512 / resolution, 1);
  const IndexRange inner_range = src.index_range().drop_back(2).drop_front(1);
  threading::parallel_for(inner_range, grain_size, [&](IndexRange range) {
    for (const int i : range) {
      const IndexRange segment_range(resolution * i, resolution);
      evaluate_segment(src[i - 1], src[i], src[i + 1], src[i + 2], dst.slice(segment_range));
    }
  });
}

void interpolate_to_evaluated(const fn::GSpan src,
                              const bool cyclic,
                              const int resolution,
                              fn::GMutableSpan dst)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    /* TODO: Use DefaultMixer or other generic mixing in the basis evaluation function to simplify
     * supporting more types. */
    if constexpr (is_same_any_v<T, float, float2, float3, float4, int8_t, int, int64_t>) {
      interpolate_to_evaluated(src.typed<T>(), cyclic, resolution, dst.typed<T>());
    }
  });
}

}  // namespace blender::bke::curves::catmull_rom
