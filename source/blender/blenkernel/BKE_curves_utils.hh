/* SPDX-License-Identifier: GPL-2.0-or-later */

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

/* --------------------------------------------------------------------
 * Utility structs.
 */

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
 * Tracks indices of the neighbouring control/evaluated point pair associated with the segment
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
 * Cyclical index range. Iterates the interval [start, end).
 */
class IndexRangeCyclic {
  /* Index to the start and end of the iterated range.
   */
  int64_t start_ = 0;
  int64_t end_ = 0;
  /* Index for the start and end of the entire iterable range which contains the iterated range
   * (e.g. the point range for an indiviudal spline/curve within the entire Curves point domain).
   */
  int64_t range_start_ = 0;
  int64_t range_end_ = 0;
  /* Number of times the range end is passed when the range is iterated.
   */
  int64_t cycles_ = 0;

  constexpr IndexRangeCyclic(int64_t begin,
                             int64_t end,
                             int64_t iterable_range_start,
                             int64_t iterable_range_end,
                             int64_t cycles)
      : start_(begin),
        end_(end),
        range_start_(iterable_range_start),
        range_end_(iterable_range_end),
        cycles_(cycles)
  {
  }

 public:
  constexpr IndexRangeCyclic() = default;
  ~IndexRangeCyclic() = default;

  constexpr IndexRangeCyclic(int64_t start, int64_t end, IndexRange iterable_range, int64_t cycles)
      : start_(start),
        end_(end),
        range_start_(iterable_range.first()),
        range_end_(iterable_range.one_after_last()),
        cycles_(cycles)
  {
  }

  /**
   * Create an iterator over the cyclical interval [start_index, end_index).
   */
  constexpr IndexRangeCyclic(int64_t start, int64_t end, IndexRange iterable_range)
      : start_(start),
        end_(end == iterable_range.one_after_last() ? iterable_range.first() : end),
        range_start_(iterable_range.first()),
        range_end_(iterable_range.one_after_last()),
        cycles_(end < start)
  {
  }

  /**
   * Increment the range by adding the given number of indices to the beginning of the range.
   */
  constexpr IndexRangeCyclic push_forward(int n)
  {
    BLI_assert(n >= 0);
    int64_t nstart = start_ - n;
    int64_t cycles = cycles_;
    if (nstart < range_start_) {

      cycles += (int64_t)(n / (range_end_ - range_start_)) + (end_ < nstart) - (end_ < start_);
    }
    return {nstart, end_, range_start_, range_end_, cycles};
  }
  /**
   * Increment the range by adding the given number of indices to the end of the range.
   */
  constexpr IndexRangeCyclic push_backward(int n)
  {
    BLI_assert(n >= 0);
    int64_t new_end = end_ + n;
    int64_t cycles = cycles_;
    if (range_end_ <= new_end) {
      cycles += (int64_t)(n / (range_end_ - range_start_)) + (new_end < start_) - (end_ < start_);
    }
    return {start_, new_end, range_start_, range_end_, cycles};
  }

  /**
   * Get the index range for the curve buffer.
   */
  constexpr IndexRange curve_range() const
  {
    return IndexRange(range_start_, total_size());
  }

  /**
   * Range between the first element up to the end of the range.
   */
  constexpr IndexRange range_before_loop() const
  {
    return IndexRange(start_, size_before_loop());
  }

  /**
   * Range between the first element in the iterable range up to the last element in the range.
   */
  constexpr IndexRange range_after_loop() const
  {
    return IndexRange(range_start_, size_after_loop());
  }

  /**
   * Size of the entire iterable range.
   */
  constexpr int64_t total_size() const
  {
    return range_end_ - range_start_;
  }

  /**
   * Number of elements between the first element in the range up to the last element in the curve.
   */
  constexpr int64_t size_before_loop() const
  {
    return range_end_ - start_;
  }

  /**
   * Number of elements between the first element in the iterable range up to the last element in
   * the range.
   */
  constexpr int64_t size_after_loop() const
  {
    return end_ - range_start_;
  }

  /**
   * Get number of elements iterated by the cyclical index range.
   */
  constexpr int64_t size() const
  {
    if (cycles_ > 0) {
      return size_before_loop() + end_ + (cycles_ - 1) * (range_end_ - range_start_);
    }
    else {
      return end_ - start_;
    }
  }

  /**
   * Return the number of times the iterator will cycle before ending.
   */
  constexpr int64_t cycles() const
  {
    return cycles_;
  }

  constexpr int64_t first() const
  {
    return start_;
  }

  constexpr int64_t one_after_last() const
  {
    return end_;
  }

  struct CyclicIterator; /* Forward declaration */

  constexpr CyclicIterator begin() const
  {
    return CyclicIterator(range_start_, range_end_, start_, 0);
  }

  constexpr CyclicIterator end() const
  {
    return CyclicIterator(range_start_, range_end_, end_, cycles_);
  }

  struct CyclicIterator {
    int64_t index_, begin_, end_, cycles_;

    constexpr CyclicIterator(int64_t range_begin, int64_t range_end, int64_t index, int64_t cycles)
        : index_(index), begin_(range_begin), end_(range_end), cycles_(cycles)
    {
      BLI_assert(range_begin <= index && index <= range_end);
    }

    constexpr CyclicIterator(const CyclicIterator &copy)
        : index_(copy.index_), begin_(copy.begin_), end_(copy.end_), cycles_(copy.cycles_)
    {
    }
    ~CyclicIterator() = default;

    constexpr CyclicIterator &operator=(const CyclicIterator &copy)
    {
      if (this == &copy) {
        return *this;
      }
      index_ = copy.index_;
      begin_ = copy.begin_;
      end_ = copy.end_;
      cycles_ = copy.cycles_;
      return *this;
    }
    constexpr CyclicIterator &operator++()
    {
      index_++;
      if (index_ == end_) {
        index_ = begin_;
        cycles_++;
      }
      return *this;
    }

    void increment(int64_t n)
    {
      for (int i = 0; i < n; i++) {
        ++*this;
      }
    }

    constexpr const int64_t &operator*() const
    {
      return index_;
    }

    constexpr bool operator==(const CyclicIterator &other) const
    {
      return index_ == other.index_ && cycles_ == other.cycles_;
    }
    constexpr bool operator!=(const CyclicIterator &other) const
    {
      return !this->operator==(other);
    }
  };
};

/** \} */

/* --------------------------------------------------------------------
 * Utility functions.
 */

/**
 * Copy the provided point attribute values between all curves in the #curve_ranges index
 * ranges, assuming that all curves have the same number of control points in #src_curves
 * and #dst_curves.
 */
void copy_point_data(const CurvesGeometry &src_curves,
                     const CurvesGeometry &dst_curves,
                     Span<IndexRange> curve_ranges,
                     GSpan src,
                     GMutableSpan dst);

void copy_point_data(const CurvesGeometry &src_curves,
                     const CurvesGeometry &dst_curves,
                     IndexMask src_curve_selection,
                     GSpan src,
                     GMutableSpan dst);

template<typename T>
void copy_point_data(const CurvesGeometry &src_curves,
                     const CurvesGeometry &dst_curves,
                     const IndexMask src_curve_selection,
                     const Span<T> src,
                     MutableSpan<T> dst)
{
  copy_point_data(src_curves, dst_curves, src_curve_selection, GSpan(src), GMutableSpan(dst));
}

void fill_points(const CurvesGeometry &curves,
                 IndexMask curve_selection,
                 GPointer value,
                 GMutableSpan dst);

template<typename T>
void fill_points(const CurvesGeometry &curves,
                 const IndexMask curve_selection,
                 const T &value,
                 MutableSpan<T> dst)
{
  fill_points(curves, curve_selection, &value, dst);
}

/**
 * Copy only the information on the point domain, but not the offsets or any point attributes,
 * meant for operations that change the number of points but not the number of curves.
 * \warning The returned curves have invalid offsets!
 */
bke::CurvesGeometry copy_only_curve_domain(const bke::CurvesGeometry &src_curves);

/**
 * Copy the size of every curve in #curve_ranges to the corresponding index in #counts.
 */
void fill_curve_counts(const bke::CurvesGeometry &curves,
                       Span<IndexRange> curve_ranges,
                       MutableSpan<int> counts);

/**
 * Turn an array of sizes into the offset at each index including all previous sizes.
 */
void accumulate_counts_to_offsets(MutableSpan<int> counts_to_offsets, int start_offset = 0);

IndexMask indices_for_type(const VArray<int8_t> &types,
                           const std::array<int, CURVE_TYPES_NUM> &type_counts,
                           const CurveType type,
                           const IndexMask selection,
                           Vector<int64_t> &r_indices);

void foreach_curve_by_type(const VArray<int8_t> &types,
                           const std::array<int, CURVE_TYPES_NUM> &type_counts,
                           IndexMask selection,
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
