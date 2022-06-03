/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.hh"
#include "BLI_math_color.hh"
#include "BLI_math_vector.hh"
#include "BLI_vector.hh"

namespace blender::length_parameterize {

/**
 * Return the size of the necessary lengths array for a group of points, taking into account the
 * possible last cyclic segment.
 *
 * \note This is the same as #bke::curves::curve_segment_num.
 */
inline int lengths_num(const int points_num, const bool cyclic)
{
  return cyclic ? points_num : points_num - 1;
}

/**
 * Accumulate the length of the next segment into each point.
 */
template<typename T>
void accumulate_lengths(const Span<T> values, const bool cyclic, MutableSpan<float> lengths)
{
  BLI_assert(lengths.size() == lengths_num(values.size(), cyclic));
  float length = 0.0f;
  for (const int i : IndexRange(values.size() - 1)) {
    length += math::distance(values[i], values[i + 1]);
    lengths[i] = length;
  }
  if (cyclic) {
    lengths.last() = length + math::distance(values.last(), values.first());
  }
}

template<typename T>
void linear_interpolation(const Span<T> src,
                          const Span<int> indices,
                          const Span<float> factors,
                          MutableSpan<T> dst)
{
  BLI_assert(indices.size() == factors.size());
  BLI_assert(indices.size() == dst.size());
  const int last_src_index = src.index_range().last();

  int cyclic_sample_count = 0;
  for (int i = indices.index_range().last(); i > 0; i--) {
    if (indices[i] != last_src_index) {
      break;
    }
    dst[i] = math::interpolate(src.last(), src.first(), factors[i]);
    cyclic_sample_count++;
  }

  for (const int i : dst.index_range().drop_back(cyclic_sample_count)) {
    dst[i] = math::interpolate(src[indices[i]], src[indices[i] + 1], factors[i]);
  }
}

/**
 * Find the given number of points, evenly spaced along the provided length. For non-cyclic
 * sequences, the first point will always be included, and last point will always be included if
 * the #count is greater than zero. For cyclic sequences, the first point will always be included.
 *
 * \warning The #count argument must be greater than zero.
 */
void create_uniform_samples(Span<float> lengths,
                            bool cyclic,
                            MutableSpan<int> indices,
                            MutableSpan<float> factors);

/**
 * For each provided sample length, find the segment index and interpolation factor.
 *
 * \param lengths: The accumulated lengths of the original elements being sampled.
 * Could be calculated by #accumulate_lengths.
 * \param sample_lengths: Sampled locations in the #lengths array. Must be sorted and is expected
 * to be within the range of the #lengths values.
 * \param cyclic: Whether the points described by the #lengths input is cyclic. This is likely
 * redundant information theoretically.
 * \param indices: The index of the previous point at each sample.
 * \param factors: The portion of the length in each segment at each sample.
 */
void create_samples_from_sorted_lengths(Span<float> lengths,
                                        Span<float> sample_lengths,
                                        bool cyclic,
                                        MutableSpan<int> indices,
                                        MutableSpan<float> factors);

}  // namespace blender::length_parameterize
