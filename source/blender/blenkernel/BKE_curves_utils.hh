/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.hh"

/** \file
 * \ingroup bke
 * \brief Low-level operations for curves.
 */

#include "BLI_function_ref.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_index_range.hh"

namespace blender::bke::curves {

/* -------------------------------------------------------------------- */
/** \name Utility Structs
 * \{ */

/**
 * Reference to a piecewise segment on a spline curve.
 */
struct CurveSegment {
  /**
   * Index of the previous control/evaluated point on the curve. First point on the segment.
   */
  int index;
  /**
   * Index of the next control/evaluated point on the curve. Last point on the curve segment.
   * Should be 0 for looped segments.
   */
  int next_index;
};

/**
 * Reference to a point on a piecewise curve (spline).
 *
 * Tracks indices of the neighboring control/evaluated point pair associated with the segment
 * in which the point resides. Referenced point within the segment is defined by a
 * normalized parameter in the range [0, 1].
 */
struct CurvePoint : public CurveSegment {
  /**
   * Normalized parameter in the range [0, 1] defining the point on the piecewise segment.
   * Note that the curve point representation is not unique at segment endpoints.
   */
  float parameter;

  /**
   * True if the parameter is an integer and references a control/evaluated point.
   */
  inline bool is_controlpoint() const;

  /*
   * Compare if the points are equal.
   */
  inline bool operator==(const CurvePoint &other) const;
  inline bool operator!=(const CurvePoint &other) const;

  /**
   * Compare if 'this' point comes before 'other'. Loop segment for cyclical curves counts
   * as the first (least) segment.
   */
  inline bool operator<(const CurvePoint &other) const;
};

/**
 * Cyclical index range. Allows iteration over a plain 'IndexRange' interval on form [start, end)
 * while also supporting treating the underlying array as a cyclic array where the last index is
 * followed by the first index in the 'cyclical' range. The cyclical index range can then be
 * considered a combination of the intervals separated by the last index of the underlying array,
 * namely [start, range_size) and [0, end) where start/end is the indices iterated between and
 * range_size is the size of the underlying array. To cycle the underlying array the interval
 * [0, range_size) can be iterated over an arbitrary amount of times in between.
 */
class IndexRangeCyclic {
  /**
   * Index to the start and end of the iterated range.
   */
  int start_ = 0;
  int end_ = 0;
  /**
   * Size of the underlying iterable range.
   */
  int range_size_ = 0;
  /**
   * Number of times the range end is passed when the range is iterated.
   */
  int cycles_ = 0;

 public:
  constexpr IndexRangeCyclic() = default;
  ~IndexRangeCyclic() = default;

  constexpr IndexRangeCyclic(const int start,
                             const int end,
                             const int iterable_range_size,
                             const int cycles)
      : start_(start), end_(end), range_size_(iterable_range_size), cycles_(cycles)
  {
  }

  /**
   * Create an iterator over the cyclical interval [start_index, end_index).
   */
  constexpr IndexRangeCyclic(const int start, const int end, const int iterable_range_size)
      : start_(start),
        end_(end == iterable_range_size ? 0 : end),
        range_size_(iterable_range_size),
        cycles_(end < start)
  {
  }

  /**
   * Create a cyclical iterator of the specified size.
   *
   * \param start_point: Point on the curve that define the starting point of the interval.
   * \param iterator_size: Number of elements to iterate (size of the iterated cyclical range).
   * \param iterable_range_size: Size of the underlying range (superset to the cyclical range).
   */
  static IndexRangeCyclic get_range_from_size(const int start_index,
                                              const int iterator_size,
                                              const int iterable_range_size)
  {
    BLI_assert(start_index >= 0);
    BLI_assert(iterator_size >= 0);
    BLI_assert(iterable_range_size > 0);
    const int num_until_loop = iterable_range_size - start_index;
    if (iterator_size < num_until_loop) {
      return IndexRangeCyclic(start_index, start_index + iterator_size, iterable_range_size, 0);
    }

    const int num_remaining = iterator_size - num_until_loop;

    /* Integer division (rounded down). */
    const int num_full_cycles = num_remaining / iterable_range_size;

    const int end_index = num_remaining - num_full_cycles * iterable_range_size;
    return IndexRangeCyclic(start_index, end_index, iterable_range_size, num_full_cycles + 1);
  }

  /**
   * Create a cyclical iterator for all control points within the interval [start_point, end_point]
   * including any control point at the start or end point.
   *
   * \param start_point: Point on the curve that define the starting point of the interval.
   * \param end_point: Point on the curve that define the end point of the interval (included).
   * \param iterable_range_size: Size of the underlying range (superset to the cyclical range).
   */
  static IndexRangeCyclic get_range_between_endpoints(const CurvePoint start_point,
                                                      const CurvePoint end_point,
                                                      const int iterable_range_size)
  {
    BLI_assert(iterable_range_size > 0);
    const int start_index = start_point.parameter == 0.0 ? start_point.index :
                                                           start_point.next_index;
    int end_index = end_point.parameter == 0.0 ? end_point.index : end_point.next_index;
    int cycles;

    if (end_point.is_controlpoint()) {
      BLI_assert(end_index < iterable_range_size);
      ++end_index;
      if (end_index == iterable_range_size) {
        end_index = 0;
      }
      /* end_point < start_point but parameter is irrelevant (end_point is controlpoint), and loop
       * when equal due to increment. */
      cycles = end_index <= start_index;
    }
    else {
      cycles = end_point < start_point || end_index < start_index;
    }
    return IndexRangeCyclic(start_index, end_index, iterable_range_size, cycles);
  }

  /**
   * Next index within the iterable range.
   */
  template<typename IndexT> constexpr IndexT next_index(const IndexT index, const bool cyclic)
  {
    static_assert((is_same_any_v<IndexT, int, int>), "Expected signed integer type.");
    const IndexT next_index = index + 1;
    if (next_index == this->size_range()) {
      return cyclic ? 0 : index;
    }
    return next_index;
  }

  /**
   * Previous index within the iterable range.
   */
  template<typename IndexT> constexpr IndexT previous_index(const IndexT index, const bool cyclic)
  {
    static_assert((is_same_any_v<IndexT, int, int64_t>), "Expected signed integer type.");
    const IndexT prev_index = index - 1;
    if (prev_index < 0) {
      return cyclic ? this->size_range() - 1 : 0;
    }
    return prev_index;
  }

  /**
   * Increment the range by adding `n` loops to the range. This invokes undefined behavior when n
   * is negative.
   */
  constexpr IndexRangeCyclic push_loop(const int n = 1) const
  {
    return {this->start_, this->end_, this->range_size_, this->cycles_ + n};
  }

  /**
   * Increment the range by adding the given number of indices to the beginning of the iterated
   * range. This invokes undefined behavior when n is negative.
   */
  constexpr IndexRangeCyclic push_front(const int n = 1) const
  {
    BLI_assert(n >= 0);
    int new_start = this->start_ - n;
    int num_cycles = this->cycles_;
    if (new_start < 0) {
      const int new_cycles = n / this->size_range(); /* Integer division (floor) */
      const int remainder = new_start + this->size_range() * new_cycles;
      const bool underflow = remainder < 0;
      new_start = remainder + (underflow ? this->size_range() : 0);
      num_cycles += new_cycles + int(underflow);
    }
    BLI_assert(num_cycles >= 0);
    BLI_assert(num_cycles > 0 ||
               (new_start <= this->end_ || (this->end_ == 0 && new_start < this->size_range())));
    return {new_start, this->end_, this->range_size_, num_cycles};
  }

  /**
   * Increment the range by adding the given number of indices to the end of the iterated range.
   * This invokes undefined behavior when n is negative.
   */
  constexpr IndexRangeCyclic push_back(const int n = 1) const
  {
    BLI_assert(n >= 0);
    int new_end = this->end_ + n;
    int num_cycles = this->cycles_;
    if (this->size_range() <= new_end) {
      const int new_cycles = n / this->size_range(); /* Integer division (floor) */
      const int remainder = new_end - this->size_range() * new_cycles;
      const bool overflow = remainder >= this->size_range();
      new_end = remainder - (overflow ? this->size_range() : 0);
      num_cycles += new_cycles + int(overflow);
    }
    BLI_assert(num_cycles >= 0);
    BLI_assert(num_cycles > 0 || (this->start_ <= new_end || new_end == 0));
    return {this->start_, new_end, this->range_size_, num_cycles};
  }

  /**
   * Returns a new range with n indices removed from the beginning of the range.
   * This invokes undefined behavior.
   */
  constexpr IndexRangeCyclic drop_front(const int n = 1) const
  {
    BLI_assert(n >= 0);
    int new_start = this->start_ + n;
    int num_cycles = this->cycles_;
    if (this->size_range() <= new_start) {
      const int dropped_cycles = n / this->size_range(); /* Integer division (floor) */
      const int remainder = new_start - this->size_range() * dropped_cycles;
      const bool overflow = remainder >= this->size_range();
      new_start = remainder - (overflow ? this->size_range() : 0);
      num_cycles -= dropped_cycles + int(overflow);
    }
    BLI_assert(num_cycles >= 0);
    BLI_assert(num_cycles > 0 ||
               (new_start <= this->end_ || (this->end_ == 0 && new_start < this->size_range())));
    return {new_start, this->end_, this->range_size_, num_cycles};
  }

  /**
   * Returns a new range with n indices removed from the end of the range.
   * This invokes undefined behavior when n is negative or n is larger then the underlying range.
   */
  constexpr IndexRangeCyclic drop_back(const int n = 1) const
  {
    BLI_assert(n >= 0);
    int new_end = this->end_ - n;
    int num_cycles = this->cycles_;
    if (0 >= new_end) {
      const int dropped_cycles = n / this->size_range(); /* Integer division (floor) */
      const int remainder = new_end + this->size_range() * dropped_cycles;
      const bool underflow = remainder < 0;
      new_end = remainder + (underflow ? this->size_range() : 0);
      num_cycles -= dropped_cycles + int(underflow);
    }
    BLI_assert(num_cycles >= 0);
    BLI_assert(num_cycles > 0 || (this->start_ <= new_end || new_end == 0));
    return {this->start_, new_end, this->range_size_, num_cycles};
  }

  /**
   * Get the index range for the curve buffer.
   */
  constexpr IndexRange curve_range() const
  {
    return IndexRange(0, this->size_range());
  }

  /**
   * Range between the first element up to the end of the range.
   */
  constexpr IndexRange range_before_loop() const
  {
    return IndexRange(this->start_, this->size_before_loop());
  }

  /**
   * Range between the first element in the iterable range up to the last element in the range.
   */
  constexpr IndexRange range_after_loop() const
  {
    return IndexRange(0, this->size_after_loop());
  }

  /**
   * Number of elements in the underlying iterable range.
   */
  constexpr int size_range() const
  {
    return this->range_size_;
  }

  /**
   * Number of elements between the first element in the range up to the last element in the curve.
   */
  constexpr int size_before_loop() const
  {
    return this->range_size_ - this->start_;
  }

  /**
   * Number of elements between the first element in the iterable range up to the last element in
   * the range.
   */
  constexpr int size_after_loop() const
  {
    return this->end_;
  }

  /**
   * Number of elements iterated by the cyclical index range.
   */
  constexpr int size() const
  {
    if (this->cycles_ > 0) {
      return this->size_before_loop() + this->end_ + (this->cycles_ - 1) * this->range_size_;
    }
    else {
      return int(this->end_ - this->start_);
    }
  }

  /**
   * Return the number of times the iterator will cycle before ending.
   */
  constexpr int cycles() const
  {
    return this->cycles_;
  }

  constexpr int first() const
  {
    return this->start_;
  }

  constexpr int last() const
  {
    BLI_assert(this->size() > 0);
    return int(this->end_ - 1);
  }

  constexpr int one_after_last() const
  {
    return this->end_;
  }

  constexpr bool operator==(const IndexRangeCyclic &other) const
  {
    return this->start_ == other.start_ && this->end_ == other.end_ &&
           this->cycles_ == other.cycles_ && this->range_size_ == other.range_size_;
  }
  constexpr bool operator!=(const IndexRangeCyclic &other) const
  {
    return !this->operator==(other);
  }

  struct CyclicIterator; /* Forward declaration */

  constexpr CyclicIterator begin() const
  {
    return CyclicIterator(this->range_size_, this->start_, 0);
  }

  constexpr CyclicIterator end() const
  {
    return CyclicIterator(this->range_size_, this->end_, this->cycles_);
  }

  struct CyclicIterator {
    int index_, range_end_, cycles_;

    constexpr CyclicIterator(const int range_end, const int index, const int cycles)
        : index_(index), range_end_(range_end), cycles_(cycles)
    {
      BLI_assert(0 <= index && index <= range_end);
    }

    constexpr CyclicIterator(const CyclicIterator &copy)
        : index_(copy.index_), range_end_(copy.range_end_), cycles_(copy.cycles_)
    {
    }
    ~CyclicIterator() = default;

    constexpr CyclicIterator &operator=(const CyclicIterator &copy)
    {
      if (this == &copy) {
        return *this;
      }
      this->index_ = copy.index_;
      this->range_end_ = copy.range_end_;
      this->cycles_ = copy.cycles_;
      return *this;
    }
    constexpr CyclicIterator &operator++()
    {
      this->index_++;
      if (this->index_ == this->range_end_) {
        this->index_ = 0;
        this->cycles_++;
      }
      return *this;
    }

    void increment(const int n)
    {
      for (int i = 0; i < n; i++) {
        ++*this;
      }
    }

    constexpr const int &operator*() const
    {
      return this->index_;
    }

    constexpr bool operator==(const CyclicIterator &other) const
    {
      return this->index_ == other.index_ && this->cycles_ == other.cycles_;
    }
    constexpr bool operator!=(const CyclicIterator &other) const
    {
      return !this->operator==(other);
    }
  };
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

/**
 * Copy the provided point attribute values between all curves in the #curve_ranges index
 * ranges, assuming that all curves have the same number of control points in #src_curves
 * and #dst_curves.
 */
void copy_point_data(OffsetIndices<int> src_points_by_curve,
                     OffsetIndices<int> dst_points_by_curve,
                     const IndexMask &src_curve_selection,
                     GSpan src,
                     GMutableSpan dst);

template<typename T>
void copy_point_data(OffsetIndices<int> src_points_by_curve,
                     OffsetIndices<int> dst_points_by_curve,
                     const IndexMask &src_curve_selection,
                     Span<T> src,
                     MutableSpan<T> dst)
{
  copy_point_data(src_points_by_curve,
                  dst_points_by_curve,
                  src_curve_selection,
                  GSpan(src),
                  GMutableSpan(dst));
}

void fill_points(OffsetIndices<int> points_by_curve,
                 const IndexMask &curve_selection,
                 GPointer value,
                 GMutableSpan dst);

template<typename T>
void fill_points(const OffsetIndices<int> points_by_curve,
                 const IndexMask &curve_selection,
                 const T &value,
                 MutableSpan<T> dst)
{
  fill_points(points_by_curve, curve_selection, &value, dst);
}

/**
 * Create new curves with the same number of curves as the input, but no points. Copy all curve
 * domain attributes to the new curves, except the offsets encoding the size of each curve.
 *
 * Used for operations that change the number of points but not the number of curves, allowing
 * creation of the new offsets directly inside the new array.
 *
 * \warning The returned curves have invalid offsets!
 */
bke::CurvesGeometry copy_only_curve_domain(const bke::CurvesGeometry &src_curves);

IndexMask indices_for_type(const VArray<int8_t> &types,
                           const std::array<int, CURVE_TYPES_NUM> &type_counts,
                           const CurveType type,
                           const IndexMask &selection,
                           IndexMaskMemory &memory);

void foreach_curve_by_type(const VArray<int8_t> &types,
                           const std::array<int, CURVE_TYPES_NUM> &type_counts,
                           const IndexMask &selection,
                           FunctionRef<void(IndexMask)> catmull_rom_fn,
                           FunctionRef<void(IndexMask)> poly_fn,
                           FunctionRef<void(IndexMask)> bezier_fn,
                           FunctionRef<void(IndexMask)> nurbs_fn);

/** \} */

/* -------------------------------------------------------------------- */
/** \name #CurvePoint Inline Methods
 * \{ */

inline bool CurvePoint::is_controlpoint() const
{
  return parameter == 0.0 || parameter == 1.0;
}

inline bool CurvePoint::operator==(const CurvePoint &other) const
{
  return (parameter == other.parameter && index == other.index) ||
         (parameter == 1.0 && other.parameter == 0.0 && next_index == other.index) ||
         (parameter == 0.0 && other.parameter == 1.0 && index == other.next_index);
}
inline bool CurvePoint::operator!=(const CurvePoint &other) const
{
  return !this->operator==(other);
}

inline bool CurvePoint::operator<(const CurvePoint &other) const
{
  if (index == other.index) {
    return parameter < other.parameter;
  }
  else {
    /* Use next index for cyclic comparison due to loop segment < first segment. */
    return next_index < other.next_index &&
           !(next_index == other.index && parameter == 1.0 && other.parameter == 0.0);
  }
}

/** \} */

}  // namespace blender::bke::curves
