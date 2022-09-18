/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <mutex>

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

namespace blender {

static RawVector<RawArray<int64_t, 0>> arrays;
static std::mutex current_array_mutex;
std::atomic<int64_t> IndexRange::s_current_array_size = 0;
std::atomic<int64_t *> IndexRange::s_current_array = nullptr;

Span<int64_t> IndexRange::as_span_internal() const
{
  int64_t min_required_size = start_ + size_;

  std::lock_guard<std::mutex> lock(current_array_mutex);

  /* Double checked lock. */
  if (min_required_size <= s_current_array_size) {
    return Span<int64_t>(s_current_array + start_, size_);
  }

  /* Isolate, because a mutex is locked. */
  threading::isolate_task([&]() {
    int64_t new_size = std::max<int64_t>(1000, power_of_2_max_u(min_required_size));
    RawArray<int64_t, 0> new_array(new_size);
    threading::parallel_for(IndexRange(new_size), 4096, [&](const IndexRange range) {
      for (const int64_t i : range) {
        new_array[i] = i;
      }
    });
    arrays.append(std::move(new_array));

    s_current_array.store(arrays.last().data(), std::memory_order_release);
    s_current_array_size.store(new_size, std::memory_order_release);
  });

  return Span<int64_t>(s_current_array + start_, size_);
}

AlignedIndexRanges split_index_range_by_alignment(const IndexRange range, const int64_t alignment)
{
  BLI_assert(is_power_of_2_i(alignment));
  const int64_t mask = alignment - 1;

  AlignedIndexRanges aligned_ranges;

  const int64_t start_chunk = range.start() & ~mask;
  const int64_t end_chunk = range.one_after_last() & ~mask;
  if (start_chunk == end_chunk) {
    aligned_ranges.prefix = range;
  }
  else {
    int64_t prefix_size = 0;
    int64_t suffix_size = 0;
    if (range.start() != start_chunk) {
      prefix_size = alignment - (range.start() & mask);
    }
    if (range.one_after_last() != end_chunk) {
      suffix_size = range.one_after_last() - end_chunk;
    }
    aligned_ranges.prefix = IndexRange(range.start(), prefix_size);
    aligned_ranges.suffix = IndexRange(end_chunk, suffix_size);
    aligned_ranges.aligned = IndexRange(aligned_ranges.prefix.one_after_last(),
                                        range.size() - prefix_size - suffix_size);
  }

  return aligned_ranges;
}

}  // namespace blender
