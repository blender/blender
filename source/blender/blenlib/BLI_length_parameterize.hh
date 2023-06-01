/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_index_mask.hh"
#include "BLI_math_base.hh"
#include "BLI_math_color.hh"
#include "BLI_math_vector.hh"

namespace blender::length_parameterize {

/**
 * Return the size of the necessary lengths array for a group of points, taking into account the
 * possible last cyclic segment.
 *
 * \note This is the same as #bke::curves::segments_num.
 */
inline int segments_num(const int points_num, const bool cyclic)
{
  return cyclic ? points_num : points_num - 1;
}

/**
 * Accumulate the length of the next segment into each point.
 */
template<typename T>
void accumulate_lengths(const Span<T> values, const bool cyclic, MutableSpan<float> lengths)
{
  BLI_assert(lengths.size() == segments_num(values.size(), cyclic));
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
inline void interpolate_to_masked(const Span<T> src,
                                  const Span<int> indices,
                                  const Span<float> factors,
                                  const IndexMask &dst_mask,
                                  MutableSpan<T> dst)
{
  BLI_assert(indices.size() == factors.size());
  BLI_assert(indices.size() == dst_mask.size());
  const int last_src_index = src.size() - 1;

  dst_mask.foreach_segment_optimized([&](const auto dst_segment, const int64_t dst_segment_pos) {
    for (const int i : dst_segment.index_range()) {
      const int prev_index = indices[dst_segment_pos + i];
      const float factor = factors[dst_segment_pos + i];
      const bool is_cyclic_case = prev_index == last_src_index;
      if (is_cyclic_case) {
        dst[dst_segment[i]] = math::interpolate(src.last(), src.first(), factor);
      }
      else {
        dst[dst_segment[i]] = math::interpolate(src[prev_index], src[prev_index + 1], factor);
      }
    }
  });
}

template<typename T>
inline void interpolate(const Span<T> src,
                        const Span<int> indices,
                        const Span<float> factors,
                        MutableSpan<T> dst)
{
  interpolate_to_masked(src, indices, factors, dst.index_range(), dst);
}

/**
 * Passing this to consecutive calls of #sample_at_length can increase performance.
 */
struct SampleSegmentHint {
  int segment_index = -1;
  float segment_start;
  float segment_length_inv;
};

/**
 * \param accumulated_segment_lengths: Lengths of individual segments added up.
 * Each value describes the total length at the end of the segment following a point.
 * \param sample_length: The position to sample at.
 * \param r_segment_index: Returns the index of the segment that #sample_length is in.
 * \param r_factor: Returns the position within the segment.
 */
inline void sample_at_length(const Span<float> accumulated_segment_lengths,
                             const float sample_length,
                             int &r_segment_index,
                             float &r_factor,
                             SampleSegmentHint *hint = nullptr)
{
  /* Use a shorter variable name. */
  const Span<float> lengths = accumulated_segment_lengths;

  BLI_assert(lengths.size() > 0);
  BLI_assert(sample_length >= 0.0f);

  if (hint != nullptr && hint->segment_index >= 0) {
    const float length_in_segment = sample_length - hint->segment_start;
    const float factor = length_in_segment * hint->segment_length_inv;
    if (factor >= 0.0f && factor < 1.0f) {
      r_segment_index = hint->segment_index;
      r_factor = factor;
      return;
    }
  }

  const float total_length = lengths.last();
  if (sample_length >= total_length) {
    /* Return the last position on the last segment. */
    r_segment_index = lengths.size() - 1;
    r_factor = 1.0f;
    return;
  }

  const int prev_point_index = std::upper_bound(lengths.begin(), lengths.end(), sample_length) -
                               lengths.begin();
  const float segment_start = prev_point_index == 0 ? 0.0f : lengths[prev_point_index - 1];
  const float segment_end = lengths[prev_point_index];
  const float segment_length = segment_end - segment_start;
  const float segment_length_inv = math::safe_divide(1.0f, segment_length);
  const float length_in_segment = sample_length - segment_start;
  const float factor = length_in_segment * segment_length_inv;

  r_segment_index = prev_point_index;
  r_factor = factor;

  if (hint != nullptr) {
    hint->segment_index = r_segment_index;
    hint->segment_start = segment_start;
    hint->segment_length_inv = segment_length_inv;
  }
}

/**
 * Find evenly spaced samples along the lengths.
 *
 * \param accumulated_segment_lengths: The accumulated lengths of the original elements being
 * sampled. Could be calculated by #accumulate_lengths.
 * \param include_last_point: Generally false for cyclic sequences and true otherwise.
 * \param r_segment_indices: The index of the previous point at each sample.
 * \param r_factors: The portion of the length in each segment at each sample.
 */
void sample_uniform(Span<float> accumulated_segment_lengths,
                    bool include_last_point,
                    MutableSpan<int> r_segment_indices,
                    MutableSpan<float> r_factors);

/**
 * For each provided sample length, find the segment index and interpolation factor.
 *
 * \param accumulated_segment_lengths: The accumulated lengths of the original elements being
 * sampled. Could be calculated by #accumulate_lengths.
 * \param sample_lengths: Sampled locations in the #lengths array. Must be sorted and is expected
 * to be within the range of the #lengths values.
 * \param r_segment_indices: The index of the previous point at each sample.
 * \param r_factors: The portion of the length in each segment at each sample.
 */
void sample_at_lengths(Span<float> accumulated_segment_lengths,
                       Span<float> sample_lengths,
                       MutableSpan<int> r_segment_indices,
                       MutableSpan<float> r_factors);

}  // namespace blender::length_parameterize
