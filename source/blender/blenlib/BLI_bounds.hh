/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Generic algorithms for finding the largest and smallest elements in a span.
 */

#include <optional>

#include "BLI_bounds_types.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

namespace blender {

namespace bounds {

template<typename T> [[nodiscard]] inline Bounds<T> merge(const Bounds<T> &a, const Bounds<T> &b)
{
  return {math::min(a.min, b.min), math::max(a.max, b.max)};
}

template<typename T>
[[nodiscard]] inline std::optional<Bounds<T>> merge(const std::optional<Bounds<T>> &a,
                                                    const std::optional<Bounds<T>> &b)
{
  if (a.has_value() && b.has_value()) {
    return merge(*a, *b);
  }
  if (a.has_value()) {
    return a;
  }
  if (b.has_value()) {
    return b;
  }
  return std::nullopt;
}

template<typename T>
[[nodiscard]] inline std::optional<Bounds<T>> merge(const std::optional<Bounds<T>> &a,
                                                    const Bounds<T> &b)
{
  return merge(a, std::optional<Bounds<T>>(b));
}

template<typename T>
[[nodiscard]] inline std::optional<Bounds<T>> min_max(const std::optional<Bounds<T>> &a,
                                                      const T &b)
{
  if (a.has_value()) {
    return merge(*a, {b, b});
  }
  return Bounds<T>{b, b};
}

/**
 * Find the smallest and largest values element-wise in the span.
 */
template<typename T> [[nodiscard]] inline std::optional<Bounds<T>> min_max(const Span<T> values)
{
  if (values.is_empty()) {
    return std::nullopt;
  }
  const Bounds<T> init{values.first(), values.first()};
  return threading::parallel_reduce(
      values.index_range(),
      1024,
      init,
      [&](const IndexRange range, const Bounds<T> &init) {
        Bounds<T> result = init;
        for (const int i : range) {
          math::min_max(values[i], result.min, result.max);
        }
        return result;
      },
      [](const Bounds<T> &a, const Bounds<T> &b) { return merge(a, b); });
}

template<typename T>
[[nodiscard]] inline std::optional<Bounds<T>> min_max(const IndexMask &mask, const Span<T> values)
{
  if (values.is_empty() || mask.is_empty()) {
    return std::nullopt;
  }
  if (mask.size() == values.size()) {
    /* To avoid mask slice/lookup. */
    return min_max(values);
  }
  const Bounds<T> init{values.first(), values.first()};
  return threading::parallel_reduce(
      mask.index_range(),
      1024,
      init,
      [&](const IndexRange range, const Bounds<T> &init) {
        Bounds<T> result = init;
        mask.slice(range).foreach_index_optimized<int64_t>(
            [&](const int i) { math::min_max(values[i], result.min, result.max); });
        return result;
      },
      [](const Bounds<T> &a, const Bounds<T> &b) { return merge(a, b); });
}

/**
 * Find the smallest and largest values element-wise in the span, adding the radius to each element
 * first. The template type T is expected to have an addition operator implemented with RadiusT.
 */
template<typename T, typename RadiusT>
[[nodiscard]] inline std::optional<Bounds<T>> min_max_with_radii(const Span<T> values,
                                                                 const Span<RadiusT> radii)
{
  BLI_assert(values.size() == radii.size());
  if (values.is_empty()) {
    return std::nullopt;
  }
  const Bounds<T> init{values.first(), values.first()};
  return threading::parallel_reduce(
      values.index_range(),
      1024,
      init,
      [&](const IndexRange range, const Bounds<T> &init) {
        Bounds<T> result = init;
        for (const int i : range) {
          result.min = math::min(values[i] - radii[i], result.min);
          result.max = math::max(values[i] + radii[i], result.max);
        }
        return result;
      },
      [](const Bounds<T> &a, const Bounds<T> &b) { return merge(a, b); });
}

/**
 * Returns a new bound that contains the intersection of the two given bound.
 * Returns no box if there are no overlap.
 */
template<typename T>
[[nodiscard]] inline std::optional<Bounds<T>> intersect(const Bounds<T> &a, const Bounds<T> &b)
{
  const Bounds<T> result{math::max(a.min, b.min), math::min(a.max, b.max)};
  if (result.is_empty()) {
    return std::nullopt;
  }
  return result;
}
template<typename T>
[[nodiscard]] inline std::optional<Bounds<T>> intersect(const std::optional<Bounds<T>> &a,
                                                        const std::optional<Bounds<T>> &b)
{
  if (!a.has_value() || !b.has_value()) {
    return std::nullopt;
  }
  return intersect(*a, *b);
}

/**
 * Finds the maximum value for elements in the array.
 */
template<typename T> inline std::optional<T> max(const VArray<T> &values)
{
  if (values.is_empty()) {
    return std::nullopt;
  }
  if (const std::optional<T> value = values.get_if_single()) {
    return value;
  }
  const VArraySpan<int> values_span = values;
  return threading::parallel_reduce(
      values_span.index_range(),
      2048,
      std::numeric_limits<T>::min(),
      [&](const IndexRange range, int current_max) {
        for (const int value : values_span.slice(range)) {
          current_max = std::max(current_max, value);
        }
        return current_max;
      },
      [](const int a, const int b) { return std::max(a, b); });
}

/**
 * Return the eight corners of a 3D bounding box.
 * <pre>
 *
 * Z  Y
 * | /
 * |/
 * .-----X
 *     2----------6
 *    /|         /|
 *   / |        / |
 *  1----------5  |
 *  |  |       |  |
 *  |  3-------|--7
 *  | /        | /
 *  |/         |/
 *  0----------4
 * </pre>
 */
template<typename T>
inline std::array<VecBase<T, 3>, 8> corners(const Bounds<VecBase<T, 3>> &bounds)
{
  return {
      VecBase<T, 3>{bounds.min[0], bounds.min[1], bounds.min[2]},
      VecBase<T, 3>{bounds.min[0], bounds.min[1], bounds.max[2]},
      VecBase<T, 3>{bounds.min[0], bounds.max[1], bounds.max[2]},
      VecBase<T, 3>{bounds.min[0], bounds.max[1], bounds.min[2]},
      VecBase<T, 3>{bounds.max[0], bounds.min[1], bounds.min[2]},
      VecBase<T, 3>{bounds.max[0], bounds.min[1], bounds.max[2]},
      VecBase<T, 3>{bounds.max[0], bounds.max[1], bounds.max[2]},
      VecBase<T, 3>{bounds.max[0], bounds.max[1], bounds.min[2]},
  };
}

/**
 * Transform a 3D bounding box.
 *
 * Note: this necessarily grows the bounding box, to ensure that the transformed
 * bounding box fully contains the original. Therefore, calling this iteratively
 * to transform from space A to space B, and then from space B to space C, etc.,
 * will also iteratively grow the bounding box on each call. Try to avoid doing
 * that, and instead first compose the transform matrices and then use that to
 * transform the bounding box.
 */
template<typename T, int D>
inline Bounds<VecBase<T, 3>> transform_bounds(const MatBase<T, D, D> &matrix,
                                              const Bounds<VecBase<T, 3>> &bounds)
{
  std::array<VecBase<T, 3>, 8> points = corners(bounds);
  for (VecBase<T, 3> &p : points) {
    p = math::transform_point(matrix, p);
  }
  return {math::min(Span(points)), math::max(Span(points))};
}

}  // namespace bounds

namespace detail {

template<typename T, int Size>
[[nodiscard]] inline bool any_less_or_equal_than(const VecBase<T, Size> &a,
                                                 const VecBase<T, Size> &b)
{
  for (int i = 0; i < Size; i++) {
    if (a[i] <= b[i]) {
      return true;
    }
  }
  return false;
}

}  // namespace detail

template<typename T> inline bool Bounds<T>::is_empty() const
{
  if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
    return this->max <= this->min;
  }
  else {
    return detail::any_less_or_equal_than(this->max, this->min);
  }
}

template<typename T> inline T Bounds<T>::center() const
{
  return math::midpoint(this->min, this->max);
}

template<typename T> inline T Bounds<T>::size() const
{
  return math::abs(max - min);
}

template<typename T> inline void Bounds<T>::translate(const T &offset)
{
  this->min += offset;
  this->max += offset;
}

template<typename T> inline void Bounds<T>::scale_from_center(const T &scale)
{
  const T center = this->center();
  const T new_half_size = this->size() / T(2) * scale;
  this->min = center - new_half_size;
  this->max = center + new_half_size;
}

template<typename T> inline void Bounds<T>::resize(const T &new_size)
{
  this->min = this->center() - (new_size / T(2));
  this->max = this->min + new_size;
}

template<typename T> inline void Bounds<T>::recenter(const T &new_center)
{
  const T offset = new_center - this->center();
  this->translate(offset);
}

template<typename T>
template<typename PaddingT>
inline void Bounds<T>::pad(const PaddingT &padding)
{
  this->min = this->min - padding;
  this->max = this->max + padding;
}

}  // namespace blender
