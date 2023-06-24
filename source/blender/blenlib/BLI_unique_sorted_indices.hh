/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * This file provides functions that deal with integer arrays fulfilling two constraints:
 * - Values are sorted in ascending order, e.g. [2, 3, 6, 8].
 * - The array doesn't have any duplicate elements, so [3, 4, 4, 5] is not allowed.
 *
 * Arrays satisfying these constraints are useful to "mask" indices that should be processed for
 * two main reasons:
 * - The sorted order makes hardware prefetching work best, because memory access patterns are
 *   more predictable (unless the indices are too far apart).
 * - One can check in constant time whether an array of indices contains consecutive integers which
 *   can be represented more efficiently with an #IndexRange.
 *
 * Just using a single array as a mask works well as long as the number of indices is not too
 * large. For potentially larger masks it's better to use #IndexMask which allows for better
 * multi-threading.
 */

#include <optional>
#include <variant>

#include "BLI_binary_search.hh"
#include "BLI_vector.hh"

namespace blender::unique_sorted_indices {

/**
 * \return True when the indices are consecutive and can be encoded as #IndexRange.
 */
template<typename T> inline bool non_empty_is_range(const Span<T> indices)
{
  BLI_assert(!indices.is_empty());
  return indices.last() - indices.first() == indices.size() - 1;
}

/**
 * \return The range encoded by the indices. It is assumed that all indices are consecutive.
 */
template<typename T> inline IndexRange non_empty_as_range(const Span<T> indices)
{
  BLI_assert(!indices.is_empty());
  BLI_assert(non_empty_is_range(indices));
  return IndexRange(indices.first(), indices.size());
}

/**
 * \return The range encoded by the indices if all indices are consecutive. Otherwise none.
 */
template<typename T> inline std::optional<IndexRange> non_empty_as_range_try(const Span<T> indices)
{
  if (non_empty_is_range(indices)) {
    return non_empty_as_range(indices);
  }
  return std::nullopt;
}

/**
 * \return Amount of consecutive indices at the start of the span. This takes O(log #indices) time.
 *
 * Example:
 *   [3, 4, 5, 6, 8, 9, 10]
 *                ^ Range ends here because 6 and 8 are not consecutive.
 */
template<typename T> inline int64_t find_size_of_next_range(const Span<T> indices)
{
  BLI_assert(!indices.is_empty());
  return binary_search::find_predicate_begin(indices,
                                             [indices, offset = indices[0]](const T &value) {
                                               const int64_t index = &value - indices.begin();
                                               return value - offset > index;
                                             });
}

/**
 * \return Amount of non-consecutive indices until the next encoded range of at least
 * #min_range_size elements starts. This takes O(size_until_next_range) time.
 *
 * Example:
 *   [1, 2, 4, 6, 7, 8, 9, 10, 13];
 *             ^ Range of at least size 4 starts here.
 */
template<typename T>
inline int64_t find_size_until_next_range(const Span<T> indices, const int64_t min_range_size)
{
  BLI_assert(!indices.is_empty());
  int64_t current_range_size = 1;
  int64_t last_value = indices[0];
  for (const int64_t i : indices.index_range().drop_front(1)) {
    const T current_value = indices[i];
    if (current_value == last_value + 1) {
      current_range_size++;
      if (current_range_size >= min_range_size) {
        return i - min_range_size + 1;
      }
    }
    else {
      current_range_size = 1;
    }
    last_value = current_value;
  }
  return indices.size();
}

/**
 * Split the indices up into segments, where each segment is either a range (because the indices
 * are consecutive) or not. There are two opposing goals: The number of segments should be
 * minimized while the amount of indices in a range should be maximized. The #range_threshold
 * allows the caller to balance these goals.
 */
template<typename T, int64_t InlineBufferSize>
inline int64_t split_to_ranges_and_spans(
    const Span<T> indices,
    const int64_t range_threshold,
    Vector<std::variant<IndexRange, Span<T>>, InlineBufferSize> &r_segments)
{
  BLI_assert(range_threshold >= 1);
  const int64_t old_segments_num = r_segments.size();
  Span<T> remaining_indices = indices;
  while (!remaining_indices.is_empty()) {
    if (const std::optional<IndexRange> range = non_empty_as_range_try(remaining_indices)) {
      /* All remaining indices are range. */
      r_segments.append(*range);
      break;
    }
    if (non_empty_is_range(remaining_indices.take_front(range_threshold))) {
      /* Next segment is a range. Now find the place where the range ends. */
      const int64_t segment_size = find_size_of_next_range(remaining_indices);
      r_segments.append(IndexRange(remaining_indices[0], segment_size));
      remaining_indices = remaining_indices.drop_front(segment_size);
      continue;
    }
    /* Next segment is just indices. Now find the place where the next range starts. */
    const int64_t segment_size = find_size_until_next_range(remaining_indices, range_threshold);
    const Span<T> segment_indices = remaining_indices.take_front(segment_size);
    if (const std::optional<IndexRange> range = non_empty_as_range_try(segment_indices)) {
      r_segments.append(*range);
    }
    else {
      r_segments.append(segment_indices);
    }
    remaining_indices = remaining_indices.drop_front(segment_size);
  }
  return r_segments.size() - old_segments_num;
}

}  // namespace blender::unique_sorted_indices
