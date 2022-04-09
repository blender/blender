/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_length_parameterize.hh"

namespace blender::length_parameterize {

void create_uniform_samples(const Span<float> lengths,
                            const bool cyclic,
                            MutableSpan<int> indices,
                            MutableSpan<float> factors)
{
  const int count = indices.size();
  BLI_assert(count > 0);
  BLI_assert(lengths.size() >= 1);
  BLI_assert(std::is_sorted(lengths.begin(), lengths.end()));
  const int segments_num = lengths.size();
  const int points_num = cyclic ? segments_num : segments_num + 1;

  indices.first() = 0;
  factors.first() = 0.0f;
  if (count == 1) {
    return;
  }

  const float total_length = lengths.last();
  if (total_length == 0.0f) {
    indices.fill(0);
    factors.fill(0.0f);
    return;
  }

  const float step_length = total_length / (count - (cyclic ? 0 : 1));
  const float step_length_inv = 1.0f / step_length;

  int i_dst = 1;
  /* Store the length at the previous point in a variable so it can start out at zero
   * (the lengths array doesn't contain 0 for the first point). */
  float prev_length = 0.0f;
  for (const int i_src : IndexRange(points_num - 1)) {
    const float next_length = lengths[i_src];
    const float segment_length = next_length - prev_length;
    if (segment_length == 0.0f) {
      continue;
    }
    /* Add every sample that fits in this segment. */
    const float segment_length_inv = 1.0f / segment_length;
    const int segment_samples_num = std::ceil(next_length * step_length_inv - i_dst);
    indices.slice(i_dst, segment_samples_num).fill(i_src);

    for (const int i : factors.index_range().slice(i_dst, segment_samples_num)) {
      const float length_in_segment = step_length * i - prev_length;
      factors[i] = length_in_segment * segment_length_inv;
    }

    i_dst += segment_samples_num;

    prev_length = next_length;
  }

  /* Add the samples on the last cyclic segment if necessary, and also the samples
   * that weren't created in the previous loop due to floating point inaccuracy. */
  if (cyclic && lengths.size() > 1) {
    indices.drop_front(i_dst).fill(points_num - 1);
    const float segment_length = lengths.last() - lengths.last(1);
    if (segment_length == 0.0f) {
      return;
    }
    const float segment_length_inv = 1.0f / segment_length;
    for (const int i : indices.index_range().drop_front(i_dst)) {
      const float length_in_segment = step_length * i - prev_length;
      factors[i] = length_in_segment * segment_length_inv;
    }
  }
  else {
    indices.drop_front(i_dst).fill(points_num - 2);
    factors.drop_front(i_dst).fill(1.0f);
  }
}

void create_samples_from_sorted_lengths(const Span<float> lengths,
                                        const Span<float> sample_lengths,
                                        const bool cyclic,
                                        MutableSpan<int> indices,
                                        MutableSpan<float> factors)
{
  BLI_assert(std::is_sorted(lengths.begin(), lengths.end()));
  BLI_assert(std::is_sorted(sample_lengths.begin(), sample_lengths.end()));
  BLI_assert(indices.size() == sample_lengths.size());
  BLI_assert(indices.size() == factors.size());
  const int segments_num = lengths.size();
  const int points_num = cyclic ? segments_num : segments_num + 1;

  const float total_length = lengths.last();
  if (total_length == 0.0f) {
    indices.fill(0);
    factors.fill(0.0f);
    return;
  }

  int i_dst = 0;
  /* Store the length at the previous point in a variable so it can start out at zero
   * (the lengths array doesn't contain 0 for the first point). */
  float prev_length = 0.0f;
  for (const int i_src : IndexRange(points_num - 1)) {
    const float next_length = lengths[i_src];
    const float segment_length = next_length - prev_length;
    if (segment_length == 0.0f) {
      continue;
    }
    /* Add every sample that fits in this segment. It's also necessary to check if the last sample
     * has been reached, since there is no upper bound on the number of samples in each segment. */
    const float segment_length_inv = 1.0f / segment_length;
    while (i_dst < sample_lengths.size() && sample_lengths[i_dst] < next_length) {
      const float length_in_segment = sample_lengths[i_dst] - prev_length;
      const float factor = length_in_segment * segment_length_inv;
      indices[i_dst] = i_src;
      factors[i_dst] = factor;
      i_dst++;
    }

    prev_length = next_length;
  }

  /* Add the samples on the last cyclic segment if necessary, and also the samples
   * that weren't created in the previous loop due to floating point inaccuracy. */
  if (cyclic && lengths.size() > 1) {
    const float segment_length = lengths.last() - lengths.last(1);
    while (sample_lengths[i_dst] < total_length) {
      const float length_in_segment = sample_lengths[i_dst] - prev_length;
      const float factor = length_in_segment / segment_length;
      indices[i_dst] = points_num - 1;
      factors[i_dst] = factor;
      i_dst++;
    }
    indices.drop_front(i_dst).fill(points_num - 1);
    factors.drop_front(i_dst).fill(1.0f);
  }
  else {
    indices.drop_front(i_dst).fill(points_num - 2);
    factors.drop_front(i_dst).fill(1.0f);
  }
}

}  // namespace blender::length_parameterize
