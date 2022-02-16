/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bli
 *
 * Generic algorithms for finding the largest and smallest elements in a span.
 */

#include <optional>

#include "BLI_math_vector.hh"
#include "BLI_task.hh"

namespace blender::bounds {

template<typename T> struct MinMaxResult {
  T min;
  T max;
};

/**
 * Find the smallest and largest values element-wise in the span.
 */
template<typename T> static std::optional<MinMaxResult<T>> min_max(Span<T> values)
{
  if (values.is_empty()) {
    return std::nullopt;
  }
  return threading::parallel_reduce(
      values.index_range(),
      1024,
      MinMaxResult<T>(),
      [&](IndexRange range, const MinMaxResult<T> &init) {
        MinMaxResult<T> result = init;
        for (const int i : range) {
          math::min_max(values[i], result.min, result.max);
        }
        return result;
      },
      [](const MinMaxResult<T> &a, const MinMaxResult<T> &b) {
        return MinMaxResult<T>{math::min(a.min, b.min), math::max(a.max, b.max)};
      });
}

/**
 * Find the smallest and largest values element-wise in the span, adding the radius to each element
 * first. The template type T is expected to have an addition operator implemented with RadiusT.
 */
template<typename T, typename RadiusT>
static std::optional<MinMaxResult<T>> min_max_with_radii(Span<T> values, Span<RadiusT> radii)
{
  BLI_assert(values.size() == radii.size());
  if (values.is_empty()) {
    return std::nullopt;
  }
  return threading::parallel_reduce(
      values.index_range(),
      1024,
      MinMaxResult<T>(),
      [&](IndexRange range, const MinMaxResult<T> &init) {
        MinMaxResult<T> result = init;
        for (const int i : range) {
          result.min = math::min(values[i] - radii[i], result.min);
          result.max = math::max(values[i] + radii[i], result.max);
        }
        return result;
      },
      [](const MinMaxResult<T> &a, const MinMaxResult<T> &b) {
        return MinMaxResult<T>{math::min(a.min, b.min), math::max(a.max, b.max)};
      });
}

}  // namespace blender::bounds
