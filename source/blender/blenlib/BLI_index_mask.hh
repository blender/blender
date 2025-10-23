/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include <array>
#include <optional>
#include <variant>

#include "BLI_bit_span.hh"
#include "BLI_function_ref.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_index_ranges_builder_fwd.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_offset_indices.hh"
#include "BLI_offset_span.hh"
#include "BLI_task.hh"
#include "BLI_unique_sorted_indices.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"
#include "BLI_virtual_array_fwd.hh"

namespace blender::index_mask {

/**
 * Constants that define the maximum segment size. Segment sizes are limited so that the indices
 * within each segment can be stored as #int16_t, which allows the mask to stored much more
 * compactly than if 32 or 64 bit ints would be used.
 * - Using 8 bit ints does not work well, because then the maximum segment size would be too small
 *   for eliminate per-segment overhead in many cases and also leads to many more segments.
 * - The most-significant-bit is not used so that signed integers can be used which avoids common
 *   issues when mixing signed and unsigned ints.
 * - The second most-significant bit is not used for indices so that #max_segment_size itself can
 *   be stored in the #int16_t.
 * - The maximum number of indices in a segment is 16384, which is generally enough to make the
 *   overhead per segment negligible when processing large index masks.
 * - A power of two is used for #max_segment_size, because that allows for faster construction of
 *   index masks for index ranges.
 */
static constexpr int64_t max_segment_size_shift = 14;
static constexpr int64_t max_segment_size = (1 << max_segment_size_shift); /* 16384 */
static constexpr int64_t max_segment_size_mask_low = max_segment_size - 1;
static constexpr int64_t max_segment_size_mask_high = ~max_segment_size_mask_low;

/**
 * Encodes a position in an #IndexMask. The term "raw" just means that this does not have the usual
 * iterator methods like `operator++`. Supporting those would require storing more data. Generally,
 * the fastest way to iterate over an #IndexMask is using a `foreach_*` method anyway.
 */
struct RawMaskIterator {
  /** Index of the segment in the index mask. */
  int64_t segment_i;
  /** Element within the segment. */
  int16_t index_in_segment;
};

/**
 * Base type of #IndexMask. This only exists to make it more convenient to construct an index mask
 * in a few functions with #IndexMask::data_for_inplace_construction.
 *
 * The names intentionally have a trailing underscore here even though they are public in
 * #IndexMaskData because they are private in #IndexMask.
 */
struct IndexMaskData {
  /**
   * Size of the index mask, i.e. the number of indices.
   */
  int64_t indices_num_;
  /**
   * Number of segments in the index mask. Each segment contains at least one of the indices.
   */
  int64_t segments_num_;
  /**
   * Pointer to the index array for every segment. The size of each array can be computed from
   * #cumulative_segment_sizes_.
   */
  const int16_t **indices_by_segment_;
  /**
   * Offset that is applied to the indices in each segment.
   */
  const int64_t *segment_offsets_;
  /**
   * Encodes the size of each segment. The size of a specific segment can be computed by
   * subtracting consecutive values (also see #OffsetIndices). The size of this array is one
   * larger than #segments_num_. Note that the first value is _not_ necessarily zero when an
   * index mask is a slice of another mask.
   */
  const int64_t *cumulative_segment_sizes_;
  /**
   * Index into the first segment where the #IndexMask starts. This exists to support slicing
   * without having to modify and therefor allocate a new #indices_by_segment_ array.
   */
  int64_t begin_index_in_segment_;
  /**
   * Index into the last segment where the #IndexMask ends. This exists to support slicing without
   * having to modify and therefore allocate a new #cumulative_segment_sizes_ array.
   */
  int64_t end_index_in_segment_;
};

/**
 * #IndexMask does not own any memory itself. In many cases the memory referenced by a mask has
 * static life-time (e.g. when a mask is a range). To create more complex masks, additional memory
 * is necessary. #IndexMaskMemory is a simple wrapper around a linear allocator that has to be
 * passed to functions that might need to allocate extra memory.
 */
class IndexMaskMemory : public LinearAllocator<> {
 private:
  /** Inline buffer to avoid heap allocations when working with small index masks. */
  AlignedBuffer<1024, 8> inline_buffer_;

 public:
  IndexMaskMemory()
  {
    this->provide_buffer(inline_buffer_);
  }
};

/**
 * A sequence of unique and ordered indices in one segment of an IndexMask. The segment as a whole
 * has an `int64_t` index offset that is added to each referenced `int16_t` index.
 */
class IndexMaskSegment : public OffsetSpan<int64_t, int16_t> {
 public:
  using OffsetSpan<int64_t, int16_t>::OffsetSpan;

  explicit IndexMaskSegment(const OffsetSpan<int64_t, int16_t> span);

  IndexMaskSegment slice(const IndexRange &range) const;
  IndexMaskSegment slice(const int64_t start, const int64_t size) const;

  /**
   * Get a new segment where each index is modified by the given amount. This works in constant
   * time, because only the offset value is changed.
   */
  IndexMaskSegment shift(const int64_t shift) const;
};

/**
 * An #IndexMask is a sequence of unique and sorted indices (`BLI_unique_sorted_indices.hh`).
 * It's commonly used when a subset of elements in an array has to be processed.
 *
 * #IndexMask is a non-owning container. That data it references is usually either statically
 * allocated or is owned by an #IndexMaskMemory.
 *
 * Internally, an index mask is split into an arbitrary number of ordered segments. Each segment
 * contains up to #max_segment_size (2^14 = 16384) indices. The indices in a segment are stored as
 * `int16_t`, but each segment also has a `int64_t` offset.
 *
 * The data structure is designed to satisfy the following key requirements:
 * - Construct index mask for an #IndexRange in O(1) time (after initial setup).
 * - Support efficient slicing (O(log n) with a low constant factor).
 * - Support multi-threaded construction without severe serial bottlenecks.
 * - Support efficient iteration over indices that uses #IndexRange when possible.
 *
 * Construction:
 *   A new index mask is usually created by calling one of its constructors which are O(1), or for
 *   more complex masks, by calling various `IndexMask::from_*` functions that create masks from
 *   various sources. Those generally need additional memory which is provided with by an
 *   #IndexMaskMemory.
 *
 *   Some of the `IndexMask::from_*` functions have an `IndexMask universe` input. When provided,
 *   the function will only consider the indices in the "universe". The term comes from
 *   mathematics: https://en.wikipedia.org/wiki/Universe_(mathematics).
 *
 * Iteration:
 *   To iterate over the indices, one usually has to use one of the `foreach_*` functions which
 *   require a callback function. Due to the internal segmentation of the index mask, this is more
 *   efficient than using a normal C++ iterator and range-based for loops.
 *
 *   There are multiple variants of the `foreach_*` functions which are useful in different
 *   scenarios. The callback can generally take one or two arguments. The first is the index
 *   stored in the mask and the second is the index that would have to be passed into `operator[]`
 *   to get the first index.
 *
 *   The `foreach_*` methods also accept an optional `GrainSize` argument. When that is provided,
 *   multi-threading is used when appropriate. Integrating multi-threading at this level works well
 *   because mask iteration and parallelism are often used at the same time.
 *
 * Extraction:
 *   An #IndexMask can be converted into various other forms using the `to_*` methods.
 */
class IndexMask : private IndexMaskData {
 public:
  /** Construct an empty mask. */
  IndexMask();
  /** Construct a mask that contains the indices from 0 to `size - 1`. This takes O(1) time. */
  explicit IndexMask(int64_t size);
  /** Construct a mask that contains the indices in the range. This takes O(1) time. */
  IndexMask(IndexRange range);

  /** Construct a mask from unique sorted indices. */
  template<typename T> static IndexMask from_indices(Span<T> indices, IndexMaskMemory &memory);
  /** Construct a mask from the indices of set bits. */
  static IndexMask from_bits(BitSpan bits, IndexMaskMemory &memory);
  /** Construct a mask from the indices of set bits, but limited to the indices in #universe. */
  static IndexMask from_bits(const IndexMask &universe, BitSpan bits, IndexMaskMemory &memory);
  /** Construct a mask from the true indices. */
  static IndexMask from_bools(Span<bool> bools, IndexMaskMemory &memory);
  static IndexMask from_bools(const VArray<bool> &bools, IndexMaskMemory &memory);
  static IndexMask from_bools_inverse(Span<bool> bools, IndexMaskMemory &memory);
  static IndexMask from_bools_inverse(const VArray<bool> &bools, IndexMaskMemory &memory);
  /** Construct a mask from the true indices, but limited by the indices in #universe. */
  static IndexMask from_bools(const IndexMask &universe,
                              Span<bool> bools,
                              IndexMaskMemory &memory);
  static IndexMask from_bools_inverse(const IndexMask &universe,
                                      Span<bool> bools,
                                      IndexMaskMemory &memory);
  static IndexMask from_bools(const IndexMask &universe,
                              const VArray<bool> &bools,
                              IndexMaskMemory &memory);
  static IndexMask from_bools_inverse(const IndexMask &universe,
                                      const VArray<bool> &bools,
                                      IndexMaskMemory &memory);
  /** Construct a mask from the ranges referenced by the offset indices. */
  template<typename T>
  static IndexMask from_ranges(OffsetIndices<T> offsets,
                               const IndexMask &mask,
                               IndexMaskMemory &memory);
  /**
   * Constructs a mask by repeating the indices in the given mask with a stride.
   * For example, with an input mask containing `{3, 5}` and a stride of 10 the resulting mask
   * would contain `{3, 5, 13, 15, 23, 25, ...}`.
   */
  static IndexMask from_repeating(const IndexMask &mask_to_repeat,
                                  int64_t repetitions,
                                  int64_t stride,
                                  int64_t initial_offset,
                                  IndexMaskMemory &memory);
  /**
   * Constructs a mask that contains every nth index the given number of times.
   */
  static IndexMask from_every_nth(int64_t n,
                                  int64_t indices_num,
                                  const int64_t initial_offset,
                                  IndexMaskMemory &memory);
  /**
   * Construct a mask from the given segments. The provided segments are expected to be
   * sorted and owned by #memory already.
   */
  static IndexMask from_segments(Span<IndexMaskSegment> segments, IndexMaskMemory &memory);
  /**
   * Construct a mask from some parts. This is mainly meant for more concise testing code.
   * The individual items are unioned together.
   */
  using Initializer = std::variant<IndexRange, Span<int64_t>, Span<int>, int64_t>;
  static IndexMask from_initializers(const Span<Initializer> initializers,
                                     IndexMaskMemory &memory);
  /** Construct a mask from the union of #mask_a and #mask_b. */
  static IndexMask from_union(const IndexMask &mask_a,
                              const IndexMask &mask_b,
                              IndexMaskMemory &memory);
  /** Constructs a mask from the union of multiple masks. */
  static IndexMask from_union(Span<IndexMask> masks, IndexMaskMemory &memory);
  /** Construct a mask from the difference of #mask_a and #mask_b. */
  static IndexMask from_difference(const IndexMask &mask_a,
                                   const IndexMask &mask_b,
                                   IndexMaskMemory &memory);
  /** Construct a mask from the intersection of #mask_a and #mask_b. */
  static IndexMask from_intersection(const IndexMask &mask_a,
                                     const IndexMask &mask_b,
                                     IndexMaskMemory &memory);
  /** Construct a mask from all the indices for which the predicate is true. */
  template<typename Fn>
  static IndexMask from_predicate(const IndexMask &universe,
                                  GrainSize grain_size,
                                  IndexMaskMemory &memory,
                                  Fn &&predicate);
  /**
   * This is a variant of #from_predicate that is more efficient if the predicate for many indices
   * can be evaluated at once.
   *
   * \param batch_predicate: A function that finds indices in a certain segment that should become
   * part of the mask. To efficiently handle ranges, this function uses #IndexRangesBuilder. It
   * returns an index offset that should be applied to each index in the builder.
   */
  static IndexMask from_batch_predicate(
      const IndexMask &universe,
      GrainSize grain_size,
      IndexMaskMemory &memory,
      FunctionRef<int64_t(const IndexMaskSegment &universe_segment,
                          IndexRangesBuilder<int16_t> &builder)> batch_predicate);
  /** Sorts all indices from #universe into the different output masks. */
  template<typename T, typename Fn>
  static void from_groups(const IndexMask &universe,
                          IndexMaskMemory &memory,
                          Fn &&get_group_index,
                          MutableSpan<IndexMask> r_masks);

  /** Creates an index mask for every unique group id. */
  static Vector<IndexMask, 4> from_group_ids(const VArray<int> &group_ids,
                                             IndexMaskMemory &memory,
                                             VectorSet<int> &r_index_by_group_id);
  static Vector<IndexMask, 4> from_group_ids(const IndexMask &universe,
                                             const VArray<int> &group_ids,
                                             IndexMaskMemory &memory,
                                             VectorSet<int> &r_index_by_group_id);

  int64_t size() const;
  bool is_empty() const;
  IndexRange index_range() const;
  int64_t first() const;
  int64_t last() const;

  /**
   * Returns the smallest range that contains all indices stored in this mask.
   */
  IndexRange bounds() const;

  /**
   * \return Minimum number of elements an array has to have so that it can be indexed by every
   * index stored in the mask.
   */
  int64_t min_array_size() const;

  /**
   * \return Position where the #query_index is stored, or none if the index is not in the mask.
   */
  std::optional<RawMaskIterator> find(int64_t query_index) const;
  std::optional<RawMaskIterator> find_larger_equal(int64_t query_index) const;
  std::optional<RawMaskIterator> find_smaller_equal(int64_t query_index) const;
  /**
   * \return True when the #query_index is stored in the mask.
   */
  bool contains(int64_t query_index) const;

  /** \return The iterator for the given index such that `mask[iterator] == mask[index]`. */
  RawMaskIterator index_to_iterator(int64_t index) const;
  /** \return The index for the given iterator such that `mask[iterator] == mask[index]`. */
  int64_t iterator_to_index(const RawMaskIterator &it) const;

  /**
   * Get the index at the given position. Prefer `foreach_*` methods for better performance. This
   * takes O(log n) time.
   */
  int64_t operator[](int64_t i) const;
  /**
   * Same as above but takes O(1) time. It's still preferable to use `foreach_*` methods for
   * iteration.
   */
  int64_t operator[](const RawMaskIterator &it) const;

  /**
   * Get a new mask that contains a consecutive subset of this mask. Takes O(log n) time
   * but can reuse the memory from the source mask.
   */
  IndexMask slice(IndexRange range) const;
  IndexMask slice(int64_t start, int64_t size) const;
  IndexMask slice(RawMaskIterator first_it, RawMaskIterator last_it, int64_t size) const;
  /**
   * Slices the mask based on the stored indices. The resulting mask only contains the indices that
   * are within the given range.
   */
  IndexMask slice_content(IndexRange range) const;
  IndexMask slice_content(int64_t start, int64_t size) const;
  /**
   * Same #slice but can also add an offset to every index in the mask.
   * Takes O(log n + range.size()) time but with a very small constant factor.
   */
  IndexMask slice_and_shift(IndexRange range, int64_t offset, IndexMaskMemory &memory) const;
  IndexMask slice_and_shift(int64_t start,
                            int64_t size,
                            int64_t offset,
                            IndexMaskMemory &memory) const;

  /**
   * Adds an offset to every index in the mask.
   */
  IndexMask shift(const int64_t offset, IndexMaskMemory &memory) const;

  /**
   * \return A new index mask that contains all the indices from the universe that are not in the
   * current mask.
   */
  IndexMask complement(const IndexMask &universe, IndexMaskMemory &memory) const;

  /**
   * \return Number of segments in the mask.
   */
  int64_t segments_num() const;
  /**
   * \return Indices stored in the n-th segment.
   */
  IndexMaskSegment segment(int64_t segment_i) const;

  /**
   * Iterate over the indices in multiple masks which have the same size. The given function is
   * called for groups of segments where each segment has the same size and comes from a different
   * input mask.
   * For example, if the input masks are (both have size 18):
   *   A: [0, 15), {20, 24, 25}
   *   B: [0, 5), [10, 15], {20, 30, 40, 50, 60, 70, 80, 90}
   * Then the function will be called multiple times, each time with two segments:
   *   1. [0, 5), [0, 5)
   *   2. [5, 10), [10, 15)
   *   3. [10, 15), {20, 30, 40, 50, 60}
   *   4. {20, 24, 25}, {70, 80, 90}
   */
  static void foreach_segment_zipped(Span<IndexMask> masks,
                                     FunctionRef<bool(Span<IndexMaskSegment> segments)> fn);

  /**
   * Calls the function once for every index.
   *
   * Supported function signatures:
   * - `(int64_t i)`
   * - `(int64_t i, int64_t pos)`
   *
   * `i` is the index that should be processed and `pos` is the position of that index in the mask:
   *   `i == mask[pos]`
   */
  template<typename Fn> void foreach_index(Fn &&fn) const;
  template<typename Fn> void foreach_index(GrainSize grain_size, Fn &&fn) const;

  /**
   * Same as #foreach_index, but generates more code, increasing compile time and binary size. This
   * is because separate loops are generated for segments that are ranges and those that are not.
   * Only use this when very little processing is done for each index.
   */
  template<typename IndexT, typename Fn> void foreach_index_optimized(Fn &&fn) const;
  template<typename IndexT, typename Fn>
  void foreach_index_optimized(GrainSize grain_size, Fn &&fn) const;

  /**
   * Calls the function once for every segment. This should be used instead of #foreach_index if
   * the algorithm can be implemented more efficiently by processing multiple indices at once.
   *
   * Supported function signatures:
   *  - `(IndexMaskSegment segment)`
   *  - `(IndexMaskSegment segment, int64_t segment_pos)`
   *
   * The `segment_pos` is the position in the mask where the segment starts:
   *   `segment[0] == mask[segment_pos]`
   */
  template<typename Fn> void foreach_segment(Fn &&fn) const;
  template<typename Fn> void foreach_segment(GrainSize grain_size, Fn &&fn) const;

  /**
   * This is similar to #foreach_segment but supports slightly different function signatures:
   * - `(auto segment)`
   * - `(auto segment, int64_t segment_pos)`
   *
   * The `segment` input is either of type `IndexMaskSegment` or `IndexRange`, so the function has
   * to support both cases. This also means that more code is generated by the compiler because the
   * function is instantiated twice. Only use this when very little processing happens per index.
   */
  template<typename Fn> void foreach_segment_optimized(Fn &&fn) const;
  template<typename Fn> void foreach_segment_optimized(GrainSize grain_size, Fn &&fn) const;

  /**
   * Calls the function once for every range. Note that this might call the function for each index
   * separately in the worst case if there are no consecutive indices.
   *
   * Support function signatures:
   * - `(IndexRange segment)`
   * - `(IndexRange segment, int64_t segment_pos)`
   */
  template<typename Fn> void foreach_range(Fn &&fn) const;

  /**
   * Fill the provided span with the indices in the mask. The span is expected to have the same
   * size as the mask.
   */
  template<typename T> void to_indices(MutableSpan<T> r_indices) const;
  /**
   * Set the bits at indices in the mask to 1.
   */
  void set_bits(MutableBitSpan r_bits, int64_t offset = 0) const;
  /**
   * Set the bits at indices in the mask to 1 and all other bits to 0.
   */
  void to_bits(MutableBitSpan r_bits, int64_t offset = 0) const;
  /**
   * Set the bools at indices in the mask to true and all others to false.
   */
  void to_bools(MutableSpan<bool> r_bools) const;
  /**
   * Try to convert the entire index mask into a range. This only works if there are no gaps
   * between any indices.
   */
  std::optional<IndexRange> to_range() const;
  /**
   * \return All index ranges in the mask. In the worst case this is a separate range for every
   * index.
   */
  Vector<IndexRange> to_ranges() const;
  /**
   * \return All index ranges in the universe that are not in the mask. In the worst case this is a
   * separate range for every index.
   */
  Vector<IndexRange> to_ranges_invert(IndexRange universe) const;
  /**
   * \return All segments in sorted vector. Segments that encode a range are already converted to
   * an #IndexRange.
   */
  template<int64_t N = 4>
  Vector<std::variant<IndexRange, IndexMaskSegment>, N> to_spans_and_ranges() const;

  /**
   * Is used by some functions to get low level access to the mask in order to construct it.
   */
  IndexMaskData &data_for_inplace_construction();

  friend bool operator==(const IndexMask &a, const IndexMask &b);
  friend bool operator!=(const IndexMask &a, const IndexMask &b);
};

/**
 * Utility that makes it efficient to build many small index masks from segments one after another.
 * The class has to be constructed once. Afterwards, `update` has to be called to fill the mask
 * with the provided segment.
 */
class IndexMaskFromSegment : NonCopyable, NonMovable {
 private:
  int64_t segment_offset_;
  const int16_t *segment_indices_;
  std::array<int64_t, 2> cumulative_segment_sizes_;
  IndexMask mask_;

 public:
  IndexMaskFromSegment();
  const IndexMask &update(IndexMaskSegment segment);
};

inline IndexMaskFromSegment::IndexMaskFromSegment()
{
  IndexMaskData &data = mask_.data_for_inplace_construction();
  cumulative_segment_sizes_[0] = 0;
  data.segments_num_ = 1;
  data.indices_by_segment_ = &segment_indices_;
  data.segment_offsets_ = &segment_offset_;
  data.cumulative_segment_sizes_ = cumulative_segment_sizes_.data();
  data.begin_index_in_segment_ = 0;
}

inline const IndexMask &IndexMaskFromSegment::update(const IndexMaskSegment segment)
{
  const Span<int16_t> indices = segment.base_span();
  BLI_assert(!indices.is_empty());
  BLI_assert(std::is_sorted(indices.begin(), indices.end()));
  BLI_assert(indices[0] >= 0);
  BLI_assert(indices.last() < max_segment_size);
  const int64_t indices_num = indices.size();

  IndexMaskData &data = mask_.data_for_inplace_construction();
  segment_offset_ = segment.offset();
  segment_indices_ = indices.data();
  cumulative_segment_sizes_[1] = int16_t(indices_num);
  data.indices_num_ = indices_num;
  data.end_index_in_segment_ = indices_num;

  return mask_;
}

std::array<int16_t, max_segment_size> build_static_indices_array();
const IndexMask &get_static_index_mask_for_min_size(const int64_t min_size);
std::ostream &operator<<(std::ostream &stream, const IndexMask &mask);

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

inline const std::array<int16_t, max_segment_size> &get_static_indices_array()
{
  alignas(64) static const std::array<int16_t, max_segment_size> data =
      build_static_indices_array();
  return data;
}

template<typename T>
inline void masked_fill(MutableSpan<T> data, const T &value, const IndexMask &mask)
{
  mask.foreach_index_optimized<int64_t>([&](const int64_t i) { data[i] = value; });
}

/**
 * Fill masked indices of \a r_mask with the index of that item in the mask such that
 * `r_map[mask[i]] == i` for the whole mask. The size of `r_map` needs to be at least
 * `mask.min_array_size()`.
 */
template<typename T> void build_reverse_map(const IndexMask &mask, MutableSpan<T> r_map);

/**
 * Joins segments together based on heuristics. Generally, one wants as few segments as possible,
 * but one also wants full-range-segments if possible and we don't want to copy too many indices
 * around to reduce the number of segments.
 *
 * \return Number of consolidated segments. Those are ordered to the beginning of the span.
 */
int64_t consolidate_index_mask_segments(MutableSpan<IndexMaskSegment> segments,
                                        IndexMaskMemory &memory);

/**
 * Adds index mask segments to the vector for the given range. Ranges shorter than
 * #max_segment_size fit into a single segment. Larger ranges are split into multiple segments.
 */
template<int64_t N>
void index_range_to_mask_segments(const IndexRange range, Vector<IndexMaskSegment, N> &r_segments);

/* -------------------------------------------------------------------- */
/** \name #RawMaskIterator Inline Methods
 * \{ */

inline bool operator!=(const RawMaskIterator &a, const RawMaskIterator &b)
{
  return a.segment_i != b.segment_i || a.index_in_segment != b.index_in_segment;
}

inline bool operator==(const RawMaskIterator &a, const RawMaskIterator &b)
{
  return !(a != b);
}

/* -------------------------------------------------------------------- */
/** \name #IndexMaskSegment Inline Methods
 * \{ */

inline IndexMaskSegment::IndexMaskSegment(const OffsetSpan<int64_t, int16_t> span)
    : OffsetSpan<int64_t, int16_t>(span)
{
}

inline IndexMaskSegment IndexMaskSegment::slice(const IndexRange &range) const
{
  return IndexMaskSegment(static_cast<const OffsetSpan<int64_t, int16_t> *>(this)->slice(range));
}

inline IndexMaskSegment IndexMaskSegment::slice(const int64_t start, const int64_t size) const
{
  return IndexMaskSegment(
      static_cast<const OffsetSpan<int64_t, int16_t> *>(this)->slice(start, size));
}

inline IndexMaskSegment IndexMaskSegment::shift(const int64_t shift) const
{
  BLI_assert(this->is_empty() || (*this)[0] + shift >= 0);
  return IndexMaskSegment(this->offset() + shift, this->base_span());
}

/* -------------------------------------------------------------------- */
/** \name #IndexMask Inline Methods
 * \{ */

inline void init_empty_mask(IndexMaskData &data)
{
  static constexpr int64_t cumulative_sizes_for_empty_mask[1] = {0};

  data.indices_num_ = 0;
  data.segments_num_ = 0;
  data.cumulative_segment_sizes_ = cumulative_sizes_for_empty_mask;
  /* Intentionally leave some pointers uninitialized which must not be accessed on empty masks
   * anyway. */
}

inline IndexMask::IndexMask()
{
  init_empty_mask(*this);
}

inline IndexMask::IndexMask(const int64_t size)
{
  if (size == 0) {
    init_empty_mask(*this);
    return;
  }
  *this = get_static_index_mask_for_min_size(size);
  indices_num_ = size;
  segments_num_ = ((size + max_segment_size - 1) >> max_segment_size_shift);
  begin_index_in_segment_ = 0;
  end_index_in_segment_ = size - ((size - 1) & max_segment_size_mask_high);
}

inline IndexMask::IndexMask(const IndexRange range)
{
  if (range.is_empty()) {
    init_empty_mask(*this);
    return;
  }
  const int64_t one_after_last = range.one_after_last();
  *this = get_static_index_mask_for_min_size(one_after_last);

  const int64_t first_segment_i = range.first() >> max_segment_size_shift;
  const int64_t last_segment_i = range.last() >> max_segment_size_shift;

  indices_num_ = range.size();
  segments_num_ = last_segment_i - first_segment_i + 1;
  indices_by_segment_ += first_segment_i;
  segment_offsets_ += first_segment_i;
  cumulative_segment_sizes_ += first_segment_i;
  begin_index_in_segment_ = range.first() & max_segment_size_mask_low;
  end_index_in_segment_ = one_after_last - ((one_after_last - 1) & max_segment_size_mask_high);
}

inline int64_t IndexMask::size() const
{
  return indices_num_;
}

inline bool IndexMask::is_empty() const
{
  return indices_num_ == 0;
}

inline IndexRange IndexMask::index_range() const
{
  return IndexRange(indices_num_);
}

inline IndexRange IndexMask::bounds() const
{
  if (this->is_empty()) {
    return IndexRange();
  }
  const int64_t first = this->first();
  const int64_t last = this->last();
  return IndexRange::from_begin_end_inclusive(first, last);
}

inline int64_t IndexMask::first() const
{
  BLI_assert(indices_num_ > 0);
  return segment_offsets_[0] + indices_by_segment_[0][begin_index_in_segment_];
}

inline int64_t IndexMask::last() const
{
  BLI_assert(indices_num_ > 0);
  const int64_t last_segment_i = segments_num_ - 1;
  return segment_offsets_[last_segment_i] +
         indices_by_segment_[last_segment_i][end_index_in_segment_ - 1];
}

inline int64_t IndexMask::min_array_size() const
{
  if (indices_num_ == 0) {
    return 0;
  }
  return this->last() + 1;
}

inline RawMaskIterator IndexMask::index_to_iterator(const int64_t index) const
{
  BLI_assert(index >= 0);
  BLI_assert(index < indices_num_);
  RawMaskIterator it;
  const int64_t full_index = index + cumulative_segment_sizes_[0] + begin_index_in_segment_;
  it.segment_i = binary_search::last_if(
      cumulative_segment_sizes_,
      cumulative_segment_sizes_ + segments_num_ + 1,
      [&](const int64_t cumulative_size) { return cumulative_size <= full_index; });
  it.index_in_segment = full_index - cumulative_segment_sizes_[it.segment_i];
  return it;
}

inline int64_t IndexMask::iterator_to_index(const RawMaskIterator &it) const
{
  BLI_assert(it.segment_i >= 0);
  BLI_assert(it.segment_i < segments_num_);
  BLI_assert(it.index_in_segment >= 0);
  BLI_assert(it.index_in_segment < cumulative_segment_sizes_[it.segment_i + 1] -
                                       cumulative_segment_sizes_[it.segment_i]);
  return it.index_in_segment + cumulative_segment_sizes_[it.segment_i] -
         cumulative_segment_sizes_[0] - begin_index_in_segment_;
}

inline int64_t IndexMask::operator[](const int64_t i) const
{
  const RawMaskIterator it = this->index_to_iterator(i);
  return (*this)[it];
}

inline int64_t IndexMask::operator[](const RawMaskIterator &it) const
{
  return segment_offsets_[it.segment_i] + indices_by_segment_[it.segment_i][it.index_in_segment];
}

inline int64_t IndexMask::segments_num() const
{
  return segments_num_;
}

inline IndexMaskSegment IndexMask::segment(const int64_t segment_i) const
{
  BLI_assert(segment_i >= 0);
  BLI_assert(segment_i < segments_num_);
  const int64_t full_segment_size = cumulative_segment_sizes_[segment_i + 1] -
                                    cumulative_segment_sizes_[segment_i];
  const int64_t begin_index = (segment_i == 0) ? begin_index_in_segment_ : 0;
  const int64_t end_index = (segment_i == segments_num_ - 1) ? end_index_in_segment_ :
                                                               full_segment_size;
  const int64_t segment_size = end_index - begin_index;
  return IndexMaskSegment{segment_offsets_[segment_i],
                          {indices_by_segment_[segment_i] + begin_index, segment_size}};
}

inline IndexMask IndexMask::slice(const IndexRange range) const
{
  return this->slice(range.start(), range.size());
}

inline IndexMaskData &IndexMask::data_for_inplace_construction()
{
  return *this;
}

template<typename Fn>
constexpr bool has_segment_and_start_parameter =
    std::is_invocable_r_v<void, Fn, IndexMaskSegment, int64_t> ||
    std::is_invocable_r_v<void, Fn, IndexRange, int64_t>;

template<typename Fn> inline void IndexMask::foreach_index(Fn &&fn) const
{
  this->foreach_segment(
      [&](const IndexMaskSegment indices, [[maybe_unused]] const int64_t start_segment_pos) {
        if constexpr (std::is_invocable_r_v<void, Fn, int64_t, int64_t>) {
          for (const int64_t i : indices.index_range()) {
            fn(indices[i], start_segment_pos + i);
          }
        }
        else {
          for (const int64_t index : indices) {
            fn(index);
          }
        }
      });
}

template<typename Fn>
inline void IndexMask::foreach_index(const GrainSize grain_size, Fn &&fn) const
{
  threading::parallel_for(this->index_range(), grain_size.value, [&](const IndexRange range) {
    const IndexMask sub_mask = this->slice(range);
    sub_mask.foreach_index([&](const int64_t i, [[maybe_unused]] const int64_t index_pos) {
      if constexpr (std::is_invocable_r_v<void, Fn, int64_t, int64_t>) {
        fn(i, index_pos + range.start());
      }
      else {
        fn(i);
      }
    });
  });
}

template<typename T, typename Fn>
#if (defined(__GNUC__) && !defined(__clang__))
[[gnu::optimize("O3")]]
#endif
inline void optimized_foreach_index(const IndexMaskSegment segment, const Fn fn)
{
  BLI_assert(segment.last() < std::numeric_limits<T>::max());
  if (unique_sorted_indices::non_empty_is_range(segment.base_span())) {
    const T start = T(segment[0]);
    const T last = T(segment.last());
    for (T i = start; i <= last; i++) {
      fn(i);
    }
  }
  else {
    for (const int64_t i : segment) {
      fn(T(i));
    }
  }
}

template<typename T, typename Fn>
#if (defined(__GNUC__) && !defined(__clang__))
[[gnu::optimize("O3")]]
#endif
inline void optimized_foreach_index_with_pos(const IndexMaskSegment segment,
                                             const int64_t segment_pos,
                                             const Fn fn)
{
  BLI_assert(segment.last() < std::numeric_limits<T>::max());
  BLI_assert(segment.size() + segment_pos < std::numeric_limits<T>::max());
  if (unique_sorted_indices::non_empty_is_range(segment.base_span())) {
    const T start = T(segment[0]);
    const T last = T(segment.last());
    for (T i = start, pos = T(segment_pos); i <= last; i++, pos++) {
      fn(i, pos);
    }
  }
  else {
    T pos = T(segment_pos);
    for (const int64_t i : segment.index_range()) {
      const T index = T(segment[i]);
      fn(index, pos);
      pos++;
    }
  }
}

template<typename IndexT, typename Fn>
inline void IndexMask::foreach_index_optimized(Fn &&fn) const
{
  this->foreach_segment(
      [&](const IndexMaskSegment segment, [[maybe_unused]] const int64_t segment_pos) {
        if constexpr (std::is_invocable_r_v<void, Fn, IndexT, IndexT>) {
          optimized_foreach_index_with_pos<IndexT>(segment, segment_pos, fn);
        }
        else {
          optimized_foreach_index<IndexT>(segment, fn);
        }
      });
}

template<typename IndexT, typename Fn>
inline void IndexMask::foreach_index_optimized(const GrainSize grain_size, Fn &&fn) const
{
  threading::parallel_for(this->index_range(), grain_size.value, [&](const IndexRange range) {
    const IndexMask sub_mask = this->slice(range);
    sub_mask.foreach_segment(
        [&](const IndexMaskSegment segment, [[maybe_unused]] const int64_t segment_pos) {
          if constexpr (std::is_invocable_r_v<void, Fn, IndexT, IndexT>) {
            optimized_foreach_index_with_pos<IndexT>(segment, segment_pos + range.start(), fn);
          }
          else {
            optimized_foreach_index<IndexT>(segment, fn);
          }
        });
  });
}

template<typename Fn> inline void IndexMask::foreach_segment_optimized(Fn &&fn) const
{
  this->foreach_segment(
      [&](const IndexMaskSegment segment, [[maybe_unused]] const int64_t start_segment_pos) {
        if (unique_sorted_indices::non_empty_is_range(segment.base_span())) {
          const IndexRange range(segment[0], segment.size());
          if constexpr (has_segment_and_start_parameter<Fn>) {
            fn(range, start_segment_pos);
          }
          else {
            fn(range);
          }
        }
        else {
          if constexpr (has_segment_and_start_parameter<Fn>) {
            fn(segment, start_segment_pos);
          }
          else {
            fn(segment);
          }
        }
      });
}

template<typename Fn>
inline void IndexMask::foreach_segment_optimized(const GrainSize grain_size, Fn &&fn) const
{
  threading::parallel_for(this->index_range(), grain_size.value, [&](const IndexRange range) {
    const IndexMask sub_mask = this->slice(range);
    sub_mask.foreach_segment_optimized(
        [&fn, range_start = range.start()](const auto segment,
                                           [[maybe_unused]] const int64_t start_segment_pos) {
          if constexpr (has_segment_and_start_parameter<Fn>) {
            fn(segment, start_segment_pos + range_start);
          }
          else {
            fn(segment);
          }
        });
  });
}

template<typename Fn> inline void IndexMask::foreach_segment(Fn &&fn) const
{
  [[maybe_unused]] int64_t segment_pos = 0;
  for (const int64_t segment_i : IndexRange(segments_num_)) {
    const IndexMaskSegment segment = this->segment(segment_i);
    if constexpr (has_segment_and_start_parameter<Fn>) {
      fn(segment, segment_pos);
      segment_pos += segment.size();
    }
    else {
      fn(segment);
    }
  }
}

template<typename Fn>
inline void IndexMask::foreach_segment(const GrainSize grain_size, Fn &&fn) const
{
  threading::parallel_for(this->index_range(), grain_size.value, [&](const IndexRange range) {
    const IndexMask sub_mask = this->slice(range);
    sub_mask.foreach_segment(
        [&fn, range_start = range.start()](const IndexMaskSegment mask_segment,
                                           [[maybe_unused]] const int64_t segment_pos) {
          if constexpr (has_segment_and_start_parameter<Fn>) {
            fn(mask_segment, segment_pos + range_start);
          }
          else {
            fn(mask_segment);
          }
        });
  });
}

template<typename Fn> inline void IndexMask::foreach_range(Fn &&fn) const
{
  this->foreach_segment([&](const IndexMaskSegment indices, [[maybe_unused]] int64_t segment_pos) {
    Span<int16_t> base_indices = indices.base_span();
    while (!base_indices.is_empty()) {
      const int64_t next_range_size = unique_sorted_indices::find_size_of_next_range(base_indices);
      const IndexRange range(int64_t(base_indices[0]) + indices.offset(), next_range_size);
      if constexpr (has_segment_and_start_parameter<Fn>) {
        fn(range, segment_pos);
      }
      else {
        fn(range);
      }
      segment_pos += next_range_size;
      base_indices = base_indices.drop_front(next_range_size);
    }
  });
}

namespace detail {
IndexMask from_predicate_impl(
    const IndexMask &universe,
    GrainSize grain_size,
    IndexMaskMemory &memory,
    FunctionRef<int64_t(IndexMaskSegment indices, int16_t *r_true_indices)> filter_indices);
}

template<typename Fn>
inline IndexMask IndexMask::from_predicate(const IndexMask &universe,
                                           const GrainSize grain_size,
                                           IndexMaskMemory &memory,
                                           Fn &&predicate)
{
  return detail::from_predicate_impl(
      universe,
      grain_size,
      memory,
      [&](const IndexMaskSegment indices, int16_t *__restrict r_true_indices) {
        int16_t *r_current = r_true_indices;
        const int16_t *in_end = indices.base_span().end();
        const int64_t offset = indices.offset();
        for (const int16_t *in_current = indices.base_span().data(); in_current < in_end;
             in_current++) {
          const int16_t local_index = *in_current;
          const int64_t global_index = int64_t(local_index) + offset;
          const bool condition = predicate(global_index);
          *r_current = local_index;
          /* This expects the boolean to be either 0 or 1 which is generally the case but may not
           * be if the values are uninitialized. */
          BLI_assert(ELEM(int8_t(condition), 0, 1));
          /* Branchless conditional increment. */
          r_current += condition;
        }
        const int16_t true_indices_num = int16_t(r_current - r_true_indices);
        return true_indices_num;
      });
}

template<typename T, typename Fn>
void IndexMask::from_groups(const IndexMask &universe,
                            IndexMaskMemory &memory,
                            Fn &&get_group_index,
                            MutableSpan<IndexMask> r_masks)
{
  Vector<Vector<T>> indices_by_group(r_masks.size());
  universe.foreach_index([&](const int64_t i) {
    const int group_index = get_group_index(i);
    indices_by_group[group_index].append(T(i));
  });
  for (const int64_t i : r_masks.index_range()) {
    r_masks[i] = IndexMask::from_indices<T>(indices_by_group[i], memory);
  }
}

std::optional<IndexRange> inline IndexMask::to_range() const
{
  if (indices_num_ == 0) {
    return IndexRange{};
  }
  const int64_t first_index = this->first();
  const int64_t last_index = this->last();
  if (last_index - first_index == indices_num_ - 1) {
    return IndexRange(first_index, indices_num_);
  }
  return std::nullopt;
}

template<int64_t N>
inline Vector<std::variant<IndexRange, IndexMaskSegment>, N> IndexMask::to_spans_and_ranges() const
{
  Vector<std::variant<IndexRange, IndexMaskSegment>, N> segments;
  this->foreach_segment_optimized([&](const auto segment) { segments.append(segment); });
  return segments;
}

inline bool operator!=(const IndexMask &a, const IndexMask &b)
{
  return !(a == b);
}

template<int64_t N>
inline void index_range_to_mask_segments(const IndexRange range,
                                         Vector<IndexMaskSegment, N> &r_segments)
{
  const std::array<int16_t, max_segment_size> &static_indices_array = get_static_indices_array();

  const int64_t full_size = range.size();
  for (int64_t i = 0; i < full_size; i += max_segment_size) {
    const int64_t size = std::min(i + max_segment_size, full_size) - i;
    r_segments.append(
        IndexMaskSegment(range.first() + i, Span(static_indices_array).take_front(size)));
  }
}

/**
 * Return a mask of random points or curves.
 *
 * \param mask: (optional) The elements that should be used in the resulting mask.
 * \param universe_size: The size of the mask.
 * \param random_seed: The seed for the \a RandomNumberGenerator.
 * \param probability: Determines how likely a point/curve will be chosen.
 * If set to 0.0, nothing will be in the mask, if set to 1.0 everything will be in the mask.
 */
IndexMask random_mask(const IndexMask &mask,
                      const int64_t universe_size,
                      const uint32_t random_seed,
                      const float probability,
                      IndexMaskMemory &memory);

IndexMask random_mask(const int64_t universe_size,
                      const uint32_t random_seed,
                      const float probability,
                      IndexMaskMemory &memory);

}  // namespace blender::index_mask

namespace blender {
using index_mask::IndexMask;
using index_mask::IndexMaskFromSegment;
using index_mask::IndexMaskMemory;
using index_mask::IndexMaskSegment;
}  // namespace blender
