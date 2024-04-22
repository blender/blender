/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <iostream>
#include <mutex>

#include "BLI_array.hh"
#include "BLI_bit_vector.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_base.hh"
#include "BLI_set.hh"
#include "BLI_sort.hh"
#include "BLI_strict_flags.h"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_timeit.hh"
#include "BLI_virtual_array.hh"

namespace blender::index_mask {

template<typename T> void build_reverse_map(const IndexMask &mask, MutableSpan<T> r_map)
{
#ifndef NDEBUG
  /* Catch errors with asserts in debug builds. */
  r_map.fill(-1);
#endif
  BLI_assert(r_map.size() >= mask.min_array_size());
  mask.foreach_index_optimized<T>(GrainSize(4096),
                                  [&](const T src, const T dst) { r_map[src] = dst; });
}

template void build_reverse_map<int>(const IndexMask &mask, MutableSpan<int> r_map);

std::array<int16_t, max_segment_size> build_static_indices_array()
{
  std::array<int16_t, max_segment_size> data;
  for (int16_t i = 0; i < max_segment_size; i++) {
    data[size_t(i)] = i;
  }
  return data;
}

const IndexMask &get_static_index_mask_for_min_size(const int64_t min_size)
{
  static constexpr int64_t size_shift = 31;
  static constexpr int64_t max_size = (int64_t(1) << size_shift);      /* 2'147'483'648 */
  static constexpr int64_t segments_num = max_size / max_segment_size; /*       131'072 */

  /* Make sure we are never requesting a size that's larger than what was statically allocated.
   * If that's ever needed, we can either increase #size_shift or dynamically allocate an even
   * larger mask. */
  BLI_assert(min_size <= max_size);
  UNUSED_VARS_NDEBUG(min_size);

  static IndexMask static_mask = []() {
    static Array<const int16_t *> indices_by_segment(segments_num);
    /* The offsets and cumulative segment sizes array contain the same values here, so just use a
     * single array for both. */
    static Array<int64_t> segment_offsets(segments_num + 1);

    static const int16_t *static_offsets = get_static_indices_array().data();

    /* Isolate because the mutex protecting the initialization of #static_mask is locked. */
    threading::isolate_task([&]() {
      threading::parallel_for(IndexRange(segments_num), 1024, [&](const IndexRange range) {
        for (const int64_t segment_i : range) {
          indices_by_segment[segment_i] = static_offsets;
          segment_offsets[segment_i] = segment_i * max_segment_size;
        }
      });
    });
    segment_offsets.last() = max_size;

    IndexMask mask;
    IndexMaskData &data = mask.data_for_inplace_construction();
    data.indices_num_ = max_size;
    data.segments_num_ = segments_num;
    data.indices_by_segment_ = indices_by_segment.data();
    data.segment_offsets_ = segment_offsets.data();
    data.cumulative_segment_sizes_ = segment_offsets.data();
    data.begin_index_in_segment_ = 0;
    data.end_index_in_segment_ = max_segment_size;

    return mask;
  }();
  return static_mask;
}

std::ostream &operator<<(std::ostream &stream, const IndexMask &mask)
{
  Array<int64_t> indices(mask.size());
  mask.to_indices<int64_t>(indices);
  Vector<std::variant<IndexRange, Span<int64_t>>> segments;
  unique_sorted_indices::split_to_ranges_and_spans<int64_t>(indices, 8, segments);
  std::cout << "(Size: " << mask.size() << " | ";
  for (const std::variant<IndexRange, Span<int64_t>> &segment : segments) {
    if (std::holds_alternative<IndexRange>(segment)) {
      const IndexRange range = std::get<IndexRange>(segment);
      std::cout << range;
    }
    else {
      const Span<int64_t> segment_indices = std::get<Span<int64_t>>(segment);
      std::cout << "[";
      for (const int64_t index : segment_indices) {
        std::cout << index << ",";
      }
      std::cout << "]";
    }
    std::cout << ", ";
  }
  std::cout << ")";
  return stream;
}

IndexMask IndexMask::slice(const int64_t start, const int64_t size) const
{
  if (size == 0) {
    return {};
  }
  const RawMaskIterator first_it = this->index_to_iterator(start);
  const RawMaskIterator last_it = this->index_to_iterator(start + size - 1);

  IndexMask sliced = *this;
  sliced.indices_num_ = size;
  sliced.segments_num_ = last_it.segment_i - first_it.segment_i + 1;
  sliced.indices_by_segment_ += first_it.segment_i;
  sliced.segment_offsets_ += first_it.segment_i;
  sliced.cumulative_segment_sizes_ += first_it.segment_i;
  sliced.begin_index_in_segment_ = first_it.index_in_segment;
  sliced.end_index_in_segment_ = last_it.index_in_segment + 1;
  return sliced;
}

IndexMask IndexMask::slice(const RawMaskIterator first_it,
                           const RawMaskIterator last_it,
                           const int64_t size) const
{
  BLI_assert(this->iterator_to_index(last_it) - this->iterator_to_index(first_it) + 1 == size);
  IndexMask sliced = *this;
  sliced.indices_num_ = size;
  sliced.segments_num_ = last_it.segment_i - first_it.segment_i + 1;
  sliced.indices_by_segment_ += first_it.segment_i;
  sliced.segment_offsets_ += first_it.segment_i;
  sliced.cumulative_segment_sizes_ += first_it.segment_i;
  sliced.begin_index_in_segment_ = first_it.index_in_segment;
  sliced.end_index_in_segment_ = last_it.index_in_segment + 1;
  return sliced;
}

IndexMask IndexMask::slice_content(const IndexRange range) const
{
  return this->slice_content(range.start(), range.size());
}

IndexMask IndexMask::slice_content(const int64_t start, const int64_t size) const
{
  if (size <= 0) {
    return {};
  }
  const std::optional<RawMaskIterator> first_it = this->find_larger_equal(start);
  const std::optional<RawMaskIterator> last_it = this->find_smaller_equal(start + size - 1);
  if (!first_it || !last_it) {
    return {};
  }
  const int64_t first_index = this->iterator_to_index(*first_it);
  const int64_t last_index = this->iterator_to_index(*last_it);
  if (last_index < first_index) {
    return {};
  }
  const int64_t sliced_mask_size = last_index - first_index + 1;
  return this->slice(*first_it, *last_it, sliced_mask_size);
}

IndexMask IndexMask::slice_and_offset(const IndexRange range,
                                      const int64_t offset,
                                      IndexMaskMemory &memory) const
{
  return this->slice_and_offset(range.start(), range.size(), offset, memory);
}

IndexMask IndexMask::slice_and_offset(const int64_t start,
                                      const int64_t size,
                                      const int64_t offset,
                                      IndexMaskMemory &memory) const
{
  if (size == 0) {
    return {};
  }
  if (std::optional<IndexRange> range = this->to_range()) {
    return range->slice(start, size).shift(offset);
  }
  IndexMask sliced_mask = this->slice(start, size);
  if (offset == 0) {
    return sliced_mask;
  }
  if (std::optional<IndexRange> range = sliced_mask.to_range()) {
    return range->shift(offset);
  }
  MutableSpan<int64_t> new_segment_offsets = memory.allocate_array<int64_t>(
      sliced_mask.segments_num_);
  for (const int64_t i : new_segment_offsets.index_range()) {
    new_segment_offsets[i] = sliced_mask.segment_offsets_[i] + offset;
  }
  sliced_mask.segment_offsets_ = new_segment_offsets.data();
  return sliced_mask;
}

/**
 * Merges consecutive segments in some cases. Having fewer but larger segments generally allows for
 * better performance when using the mask later on.
 */
static void consolidate_segments(Vector<IndexMaskSegment, 16> &segments,
                                 IndexMaskMemory & /*memory*/)
{
  if (segments.is_empty()) {
    return;
  }

  const Span<int16_t> static_indices = get_static_indices_array();

  /* TODO: Support merging non-range segments in some cases as well. */
  int64_t group_start_segment_i = 0;
  int64_t group_first = segments[0][0];
  int64_t group_last = segments[0].last();
  bool group_as_range = unique_sorted_indices::non_empty_is_range(segments[0].base_span());

  auto finish_group = [&](const int64_t last_segment_i) {
    if (group_start_segment_i == last_segment_i) {
      return;
    }
    /* Join multiple ranges together into a bigger range. */
    const IndexRange range{group_first, group_last + 1 - group_first};
    segments[group_start_segment_i] = IndexMaskSegment(range[0],
                                                       static_indices.take_front(range.size()));
    for (int64_t i = group_start_segment_i + 1; i <= last_segment_i; i++) {
      segments[i] = {};
    }
  };

  for (const int64_t segment_i : segments.index_range().drop_front(1)) {
    const IndexMaskSegment segment = segments[segment_i];
    const std::optional<IndexRange> segment_base_range =
        unique_sorted_indices::non_empty_as_range_try(segment.base_span());
    const bool segment_is_range = segment_base_range.has_value();

    if (group_as_range && segment_is_range) {
      if (group_last + 1 == segment[0]) {
        if (segment.last() - group_first + 1 < max_segment_size) {
          /* Can combine previous and current range. */
          group_last = segment.last();
          continue;
        }
      }
    }
    finish_group(segment_i - 1);

    group_start_segment_i = segment_i;
    group_first = segment[0];
    group_last = segment.last();
    group_as_range = segment_is_range;
  }
  finish_group(segments.size() - 1);

  /* Remove all segments that have been merged into previous segments. */
  segments.remove_if([](const IndexMaskSegment segment) { return segment.is_empty(); });
}

IndexMask IndexMask::from_segments(const Span<IndexMaskSegment> segments, IndexMaskMemory &memory)
{
  if (segments.is_empty()) {
    return {};
  }
#ifndef NDEBUG
  {
    int64_t last_index = segments[0].last();
    for (const IndexMaskSegment &segment : segments.drop_front(1)) {
      BLI_assert(std::is_sorted(segment.base_span().begin(), segment.base_span().end()));
      BLI_assert(last_index < segment[0]);
      last_index = segment.last();
    }
  }
#endif
  const int64_t segments_num = segments.size();

  /* Allocate buffers for the mask. */
  MutableSpan<const int16_t *> indices_by_segment = memory.allocate_array<const int16_t *>(
      segments_num);
  MutableSpan<int64_t> segment_offsets = memory.allocate_array<int64_t>(segments_num);
  MutableSpan<int64_t> cumulative_segment_sizes = memory.allocate_array<int64_t>(segments_num + 1);

  /* Fill buffers. */
  cumulative_segment_sizes[0] = 0;
  for (const int64_t segment_i : segments.index_range()) {
    const IndexMaskSegment segment = segments[segment_i];
    indices_by_segment[segment_i] = segment.base_span().data();
    segment_offsets[segment_i] = segment.offset();
    cumulative_segment_sizes[segment_i + 1] = cumulative_segment_sizes[segment_i] + segment.size();
  }

  /* Initialize mask. */
  IndexMask mask;
  IndexMaskData &data = mask.data_for_inplace_construction();
  data.indices_num_ = cumulative_segment_sizes.last();
  data.segments_num_ = segments_num;
  data.indices_by_segment_ = indices_by_segment.data();
  data.segment_offsets_ = segment_offsets.data();
  data.cumulative_segment_sizes_ = cumulative_segment_sizes.data();
  data.begin_index_in_segment_ = 0;
  data.end_index_in_segment_ = segments.last().size();
  return mask;
}

/**
 * Split the indices into segments. Afterwards, the indices referenced by #r_segments are either
 * owned by #allocator or statically allocated.
 */
template<typename T, int64_t InlineBufferSize>
static void segments_from_indices(const Span<T> indices,
                                  LinearAllocator<> &allocator,
                                  Vector<IndexMaskSegment, InlineBufferSize> &r_segments)
{
  Vector<std::variant<IndexRange, Span<T>>, 16> segments;

  for (int64_t start = 0; start < indices.size(); start += max_segment_size) {
    /* Slice to make sure that each segment is no longer than #max_segment_size. */
    const Span<T> indices_slice = indices.slice_safe(start, max_segment_size);
    unique_sorted_indices::split_to_ranges_and_spans<T>(indices_slice, 64, segments);
  }

  const Span<int16_t> static_indices = get_static_indices_array();
  for (const auto &segment : segments) {
    if (std::holds_alternative<IndexRange>(segment)) {
      const IndexRange segment_range = std::get<IndexRange>(segment);
      r_segments.append_as(segment_range.start(), static_indices.take_front(segment_range.size()));
    }
    else {
      Span<T> segment_indices = std::get<Span<T>>(segment);
      MutableSpan<int16_t> offset_indices = allocator.allocate_array<int16_t>(
          segment_indices.size());
      while (!segment_indices.is_empty()) {
        const int64_t offset = segment_indices[0];
        const int64_t next_segment_size = binary_search::find_predicate_begin(
            segment_indices.take_front(max_segment_size),
            [&](const T value) { return value - offset >= max_segment_size; });
        for (const int64_t i : IndexRange(next_segment_size)) {
          const int64_t offset_index = segment_indices[i] - offset;
          BLI_assert(offset_index < max_segment_size);
          offset_indices[i] = int16_t(offset_index);
        }
        r_segments.append_as(offset, offset_indices.take_front(next_segment_size));
        segment_indices = segment_indices.drop_front(next_segment_size);
        offset_indices = offset_indices.drop_front(next_segment_size);
      }
    }
  }
}

/**
 * Utility to generate segments on multiple threads and to reduce the result in the end.
 */
struct ParallelSegmentsCollector {
  struct LocalData {
    LinearAllocator<> allocator;
    Vector<IndexMaskSegment, 16> segments;
  };

  threading::EnumerableThreadSpecific<LocalData> data_by_thread;

  /**
   * Move ownership of memory allocated from all threads to #main_allocator. Also, extend
   * #main_segments with the segments created on each thread. The segments are also sorted to make
   * sure that they are in the correct order.
   */
  void reduce(LinearAllocator<> &main_allocator, Vector<IndexMaskSegment, 16> &main_segments)
  {
    for (LocalData &data : this->data_by_thread) {
      main_allocator.transfer_ownership_from(data.allocator);
      main_segments.extend(data.segments);
    }
    parallel_sort(main_segments.begin(),
                  main_segments.end(),
                  [](const IndexMaskSegment a, const IndexMaskSegment b) { return a[0] < b[0]; });
  }
};

/**
 * Convert a range to potentially multiple index mask segments.
 */
static void range_to_segments(const IndexRange range, Vector<IndexMaskSegment, 16> &r_segments)
{
  const Span<int16_t> static_indices = get_static_indices_array();
  for (int64_t start = 0; start < range.size(); start += max_segment_size) {
    const int64_t size = std::min(max_segment_size, range.size() - start);
    r_segments.append_as(range.start() + start, static_indices.take_front(size));
  }
}

static int64_t get_size_before_gap(const Span<int16_t> indices)
{
  BLI_assert(indices.size() >= 2);
  if (indices[1] > indices[0] + 1) {
    /* For sparse indices, often the next gap is just after the next index.
     * In this case we can skip the logarithmic check below. */
    return 1;
  }
  return unique_sorted_indices::find_size_of_next_range(indices);
}

static void inverted_indices_to_segments(const IndexMaskSegment segment,
                                         LinearAllocator<> &allocator,
                                         Vector<IndexMaskSegment, 16> &r_segments)
{
  constexpr int64_t range_threshold = 64;
  const int64_t offset = segment.offset();
  const Span<int16_t> static_indices = get_static_indices_array();

  int64_t inverted_index_count = 0;
  std::array<int16_t, max_segment_size> inverted_indices_array;
  auto add_indices = [&](const int16_t start, const int16_t num) {
    int16_t *new_indices_begin = inverted_indices_array.data() + inverted_index_count;
    std::iota(new_indices_begin, new_indices_begin + num, start);
    inverted_index_count += num;
  };

  auto finish_indices = [&]() {
    if (inverted_index_count == 0) {
      return;
    }
    MutableSpan<int16_t> offset_indices = allocator.allocate_array<int16_t>(inverted_index_count);
    offset_indices.copy_from(Span(inverted_indices_array).take_front(inverted_index_count));
    r_segments.append_as(offset, offset_indices);
    inverted_index_count = 0;
  };

  Span<int16_t> indices = segment.base_span();
  while (indices.size() > 1) {
    const int64_t size_before_gap = get_size_before_gap(indices);
    if (size_before_gap == indices.size()) {
      break;
    }

    const int16_t gap_first = int16_t(indices[size_before_gap - 1] + 1);
    const int16_t next = int16_t(indices[size_before_gap]);
    const int16_t gap_size = int16_t(next - gap_first);
    if (gap_size > range_threshold) {
      finish_indices();
      r_segments.append_as(offset + gap_first, static_indices.take_front(gap_size));
    }
    else {
      add_indices(gap_first, gap_size);
    }

    indices = indices.drop_front(size_before_gap);
  }

  finish_indices();
}

static void invert_segments(const IndexMask &mask,
                            const IndexRange segment_range,
                            LinearAllocator<> &allocator,
                            Vector<IndexMaskSegment, 16> &r_segments)
{
  for (const int64_t segment_i : segment_range) {
    const IndexMaskSegment segment = mask.segment(segment_i);
    inverted_indices_to_segments(segment, allocator, r_segments);

    const IndexMaskSegment next_segment = mask.segment(segment_i + 1);
    const int64_t between_start = segment.last() + 1;
    const int64_t size_between_segments = next_segment[0] - segment.last() - 1;
    const IndexRange range_between_segments(between_start, size_between_segments);
    if (!range_between_segments.is_empty()) {
      range_to_segments(range_between_segments, r_segments);
    }
  }
}

IndexMask IndexMask::complement(const IndexRange universe, IndexMaskMemory &memory) const
{
  if (this->is_empty()) {
    return universe;
  }
  if (universe.is_empty()) {
    return {};
  }
  const std::optional<IndexRange> this_range = this->to_range();
  if (this_range) {
    const bool first_in_range = this_range->first() <= universe.first();
    const bool last_in_range = this_range->last() >= universe.last();
    if (first_in_range && last_in_range) {
      /* This mask fills the entire universe, so the complement is empty. */
      return {};
    }
    if (first_in_range) {
      /* This mask is a range that contains the start of the universe.
       * The complement is a range that contains the end of the universe. */
      const int64_t complement_start = this_range->one_after_last();
      const int64_t complement_size = universe.one_after_last() - complement_start;
      return IndexRange(complement_start, complement_size);
    }
    if (last_in_range) {
      /* This mask is a range that contains the end of the universe.
       * The complement is a range that contains the start of the universe. */
      const int64_t complement_start = universe.first();
      const int64_t complement_size = this_range->first() - complement_start;
      return IndexRange(complement_start, complement_size);
    }
  }

  Vector<IndexMaskSegment, 16> segments;

  if (universe.start() < this->first()) {
    range_to_segments(universe.take_front(this->first() - universe.start()), segments);
  }

  if (!this_range) {
    const int64_t segments_num = this->segments_num();

    constexpr int64_t min_grain_size = 16;
    constexpr int64_t max_grain_size = 4096;
    const int64_t threads_num = BLI_system_thread_count();
    const int64_t grain_size = std::clamp(
        segments_num / threads_num, min_grain_size, max_grain_size);

    const IndexRange non_last_segments = IndexRange(segments_num).drop_back(1);
    if (segments_num < min_grain_size) {
      invert_segments(*this, non_last_segments, memory, segments);
    }
    else {
      ParallelSegmentsCollector segments_collector;
      threading::parallel_for(non_last_segments, grain_size, [&](const IndexRange range) {
        ParallelSegmentsCollector::LocalData &local_data =
            segments_collector.data_by_thread.local();
        invert_segments(*this, range, local_data.allocator, local_data.segments);
      });
      segments_collector.reduce(memory, segments);
    }
    inverted_indices_to_segments(this->segment(segments_num - 1), memory, segments);
  }

  if (universe.last() > this->first()) {
    range_to_segments(universe.take_back(universe.last() - this->last()), segments);
  }

  return IndexMask::from_segments(segments, memory);
}

template<typename T>
IndexMask IndexMask::from_indices(const Span<T> indices, IndexMaskMemory &memory)
{
  if (indices.is_empty()) {
    return {};
  }
  if (const std::optional<IndexRange> range = unique_sorted_indices::non_empty_as_range_try(
          indices))
  {
    /* Fast case when the indices encode a single range. */
    return *range;
  }

  Vector<IndexMaskSegment, 16> segments;

  constexpr int64_t min_grain_size = 4096;
  constexpr int64_t max_grain_size = max_segment_size;
  if (indices.size() <= min_grain_size) {
    segments_from_indices(indices, memory, segments);
  }
  else {
    const int64_t threads_num = BLI_system_thread_count();
    /* Can be faster with a larger grain size, but only when there are enough indices. */
    const int64_t grain_size = std::clamp(
        indices.size() / (threads_num * 4), min_grain_size, max_grain_size);

    ParallelSegmentsCollector segments_collector;
    threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
      ParallelSegmentsCollector::LocalData &local_data = segments_collector.data_by_thread.local();
      segments_from_indices(indices.slice(range), local_data.allocator, local_data.segments);
    });
    segments_collector.reduce(memory, segments);
  }
  consolidate_segments(segments, memory);
  return IndexMask::from_segments(segments, memory);
}

IndexMask IndexMask::from_bits(const BitSpan bits, IndexMaskMemory &memory)
{
  return IndexMask::from_bits(bits.index_range(), bits, memory);
}

IndexMask IndexMask::from_bits(const IndexMask &universe,
                               const BitSpan bits,
                               IndexMaskMemory &memory)
{
  return IndexMask::from_predicate(universe, GrainSize(1024), memory, [bits](const int64_t index) {
    return bits[index].test();
  });
}

IndexMask IndexMask::from_bools(Span<bool> bools, IndexMaskMemory &memory)
{
  return IndexMask::from_bools(bools.index_range(), bools, memory);
}

IndexMask IndexMask::from_bools(const VArray<bool> &bools, IndexMaskMemory &memory)
{
  return IndexMask::from_bools(bools.index_range(), bools, memory);
}

IndexMask IndexMask::from_bools(const IndexMask &universe,
                                Span<bool> bools,
                                IndexMaskMemory &memory)
{
  return IndexMask::from_predicate(
      universe, GrainSize(1024), memory, [bools](const int64_t index) { return bools[index]; });
}

IndexMask IndexMask::from_bools(const IndexMask &universe,
                                const VArray<bool> &bools,
                                IndexMaskMemory &memory)
{
  const CommonVArrayInfo info = bools.common_info();
  if (info.type == CommonVArrayInfo::Type::Single) {
    return *static_cast<const bool *>(info.data) ? universe : IndexMask();
  }
  if (info.type == CommonVArrayInfo::Type::Span) {
    const Span<bool> span(static_cast<const bool *>(info.data), bools.size());
    return IndexMask::from_bools(universe, span, memory);
  }
  return IndexMask::from_predicate(
      universe, GrainSize(512), memory, [&](const int64_t index) { return bools[index]; });
}

IndexMask IndexMask::from_union(const IndexMask &mask_a,
                                const IndexMask &mask_b,
                                IndexMaskMemory &memory)
{
  const int64_t new_size = math::max(mask_a.min_array_size(), mask_b.min_array_size());
  Array<bool> tmp(new_size, false);
  mask_a.foreach_index_optimized<int64_t>(GrainSize(2048),
                                          [&](const int64_t i) { tmp[i] = true; });
  mask_b.foreach_index_optimized<int64_t>(GrainSize(2048),
                                          [&](const int64_t i) { tmp[i] = true; });
  return IndexMask::from_bools(tmp, memory);
}

IndexMask IndexMask::from_initializers(const Span<Initializer> initializers,
                                       IndexMaskMemory &memory)
{
  Set<int64_t> values;
  for (const Initializer &item : initializers) {
    if (const auto *range = std::get_if<IndexRange>(&item)) {
      for (const int64_t i : *range) {
        values.add(i);
      }
    }
    else if (const auto *span_i64 = std::get_if<Span<int64_t>>(&item)) {
      for (const int64_t i : *span_i64) {
        values.add(i);
      }
    }
    else if (const auto *span_i32 = std::get_if<Span<int>>(&item)) {
      for (const int i : *span_i32) {
        values.add(i);
      }
    }
    else if (const auto *index = std::get_if<int64_t>(&item)) {
      values.add(*index);
    }
  }
  Vector<int64_t> values_vec;
  values_vec.extend(values.begin(), values.end());
  std::sort(values_vec.begin(), values_vec.end());
  return IndexMask::from_indices(values_vec.as_span(), memory);
}

template<typename T> void IndexMask::to_indices(MutableSpan<T> r_indices) const
{
  BLI_assert(this->size() == r_indices.size());
  this->foreach_index_optimized<int64_t>(
      GrainSize(1024), [r_indices = r_indices.data()](const int64_t i, const int64_t pos) {
        r_indices[pos] = T(i);
      });
}

void IndexMask::to_bits(MutableBitSpan r_bits) const
{
  BLI_assert(r_bits.size() >= this->min_array_size());
  r_bits.reset_all();
  this->foreach_segment_optimized([&](const auto segment) {
    if constexpr (std::is_same_v<std::decay_t<decltype(segment)>, IndexRange>) {
      const IndexRange range = segment;
      r_bits.slice(range).set_all();
    }
    else {
      for (const int64_t i : segment) {
        r_bits[i].set();
      }
    }
  });
}

void IndexMask::to_bools(MutableSpan<bool> r_bools) const
{
  BLI_assert(r_bools.size() >= this->min_array_size());
  r_bools.fill(false);
  this->foreach_index_optimized<int64_t>(GrainSize(2048),
                                         [&](const int64_t i) { r_bools[i] = true; });
}

Vector<IndexRange> IndexMask::to_ranges() const
{
  Vector<IndexRange> ranges;
  this->foreach_range([&](const IndexRange range) { ranges.append(range); });
  return ranges;
}

Vector<IndexRange> IndexMask::to_ranges_invert(const IndexRange universe) const
{
  IndexMaskMemory memory;
  return this->complement(universe, memory).to_ranges();
}

namespace detail {

/**
 * Filter the indices from #universe_segment using #filter_indices. Store the resulting indices as
 * segments.
 */
static void segments_from_predicate_filter(
    const IndexMaskSegment universe_segment,
    LinearAllocator<> &allocator,
    const FunctionRef<int64_t(IndexMaskSegment indices, int16_t *r_true_indices)> filter_indices,
    Vector<IndexMaskSegment, 16> &r_segments)
{
  std::array<int16_t, max_segment_size> indices_array;
  const int64_t true_indices_num = filter_indices(universe_segment, indices_array.data());
  if (true_indices_num == 0) {
    return;
  }
  const Span<int16_t> true_indices{indices_array.data(), true_indices_num};
  Vector<std::variant<IndexRange, Span<int16_t>>> true_segments;
  unique_sorted_indices::split_to_ranges_and_spans<int16_t>(true_indices, 64, true_segments);

  const Span<int16_t> static_indices = get_static_indices_array();

  for (const auto &true_segment : true_segments) {
    if (std::holds_alternative<IndexRange>(true_segment)) {
      const IndexRange segment_range = std::get<IndexRange>(true_segment);
      r_segments.append_as(universe_segment.offset(), static_indices.slice(segment_range));
    }
    else {
      const Span<int16_t> segment_indices = std::get<Span<int16_t>>(true_segment);
      r_segments.append_as(universe_segment.offset(),
                           allocator.construct_array_copy(segment_indices));
    }
  }
}

IndexMask from_predicate_impl(
    const IndexMask &universe,
    const GrainSize grain_size,
    IndexMaskMemory &memory,
    const FunctionRef<int64_t(IndexMaskSegment indices, int16_t *r_true_indices)> filter_indices)
{
  if (universe.is_empty()) {
    return {};
  }

  Vector<IndexMaskSegment, 16> segments;
  if (universe.size() <= grain_size.value) {
    for (const int64_t segment_i : IndexRange(universe.segments_num())) {
      const IndexMaskSegment universe_segment = universe.segment(segment_i);
      segments_from_predicate_filter(universe_segment, memory, filter_indices, segments);
    }
  }
  else {
    ParallelSegmentsCollector segments_collector;
    universe.foreach_segment(grain_size, [&](const IndexMaskSegment universe_segment) {
      ParallelSegmentsCollector::LocalData &data = segments_collector.data_by_thread.local();
      segments_from_predicate_filter(
          universe_segment, data.allocator, filter_indices, data.segments);
    });
    segments_collector.reduce(memory, segments);
  }

  consolidate_segments(segments, memory);
  return IndexMask::from_segments(segments, memory);
}
}  // namespace detail

std::optional<RawMaskIterator> IndexMask::find(const int64_t query_index) const
{
  if (const std::optional<RawMaskIterator> it = this->find_larger_equal(query_index)) {
    if ((*this)[*it] == query_index) {
      return it;
    }
  }
  return std::nullopt;
}

std::optional<RawMaskIterator> IndexMask::find_larger_equal(const int64_t query_index) const
{
  const int64_t segment_i = binary_search::find_predicate_begin(
      IndexRange(segments_num_),
      [&](const int64_t seg_i) { return this->segment(seg_i).last() >= query_index; });
  if (segment_i == segments_num_) {
    /* The query index is larger than the largest index in this mask. */
    return std::nullopt;
  }
  const IndexMaskSegment segment = this->segment(segment_i);
  const int64_t segment_begin_index = segment.base_span().data() - indices_by_segment_[segment_i];
  if (query_index < segment[0]) {
    /* The query index is the first element in this segment. */
    const int64_t index_in_segment = segment_begin_index;
    BLI_assert(index_in_segment < max_segment_size);
    return RawMaskIterator{segment_i, int16_t(index_in_segment)};
  }
  /* The query index is somewhere within this segment. */
  const int64_t local_index = query_index - segment.offset();
  const int64_t index_in_segment = binary_search::find_predicate_begin(
      segment.base_span(), [&](const int16_t i) { return i >= local_index; });
  const int64_t actual_index_in_segment = index_in_segment + segment_begin_index;
  BLI_assert(actual_index_in_segment < max_segment_size);
  return RawMaskIterator{segment_i, int16_t(actual_index_in_segment)};
}

std::optional<RawMaskIterator> IndexMask::find_smaller_equal(const int64_t query_index) const
{
  if (indices_num_ == 0) {
    return std::nullopt;
  }
  const std::optional<RawMaskIterator> larger_equal_it = this->find_larger_equal(query_index);
  if (!larger_equal_it) {
    /* Return the last element. */
    return RawMaskIterator{segments_num_ - 1, int16_t(end_index_in_segment_ - 1)};
  }
  if ((*this)[*larger_equal_it] == query_index) {
    /* This is an exact hit. */
    return larger_equal_it;
  }
  if (larger_equal_it->segment_i > 0) {
    if (larger_equal_it->index_in_segment > 0) {
      /* Previous element in same segment. */
      return RawMaskIterator{larger_equal_it->segment_i,
                             int16_t(larger_equal_it->index_in_segment - 1)};
    }
    /* Last element in previous segment. */
    return RawMaskIterator{larger_equal_it->segment_i - 1,
                           int16_t(cumulative_segment_sizes_[larger_equal_it->segment_i] -
                                   cumulative_segment_sizes_[larger_equal_it->segment_i - 1] - 1)};
  }
  if (larger_equal_it->index_in_segment > begin_index_in_segment_) {
    /* Previous element in same segment. */
    return RawMaskIterator{0, int16_t(larger_equal_it->index_in_segment - 1)};
  }
  return std::nullopt;
}

bool IndexMask::contains(const int64_t query_index) const
{
  return this->find(query_index).has_value();
}

template IndexMask IndexMask::from_indices(Span<int32_t>, IndexMaskMemory &);
template IndexMask IndexMask::from_indices(Span<int64_t>, IndexMaskMemory &);
template void IndexMask::to_indices(MutableSpan<int32_t>) const;
template void IndexMask::to_indices(MutableSpan<int64_t>) const;

}  // namespace blender::index_mask
