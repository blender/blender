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

#include "BLI_span.hh"
#include "BLI_virtual_array.hh"

#include "BKE_spline.hh"

using blender::float3;
using blender::MutableSpan;
using blender::Span;

SplinePtr PolySpline::copy() const
{
  return std::make_unique<PolySpline>(*this);
}

int PolySpline::size() const
{
  const int size = positions_.size();
  BLI_assert(size == radii_.size());
  BLI_assert(size == tilts_.size());
  return size;
}

void PolySpline::add_point(const float3 position, const float radius, const float tilt)
{
  positions_.append(position);
  radii_.append(radius);
  tilts_.append(tilt);
  this->mark_cache_invalid();
}

void PolySpline::resize(const int size)
{
  positions_.resize(size);
  radii_.resize(size);
  tilts_.resize(size);
  this->mark_cache_invalid();
}

MutableSpan<float3> PolySpline::positions()
{
  return positions_;
}
Span<float3> PolySpline::positions() const
{
  return positions_;
}
MutableSpan<float> PolySpline::radii()
{
  return radii_;
}
Span<float> PolySpline::radii() const
{
  return radii_;
}
MutableSpan<float> PolySpline::tilts()
{
  return tilts_;
}
Span<float> PolySpline::tilts() const
{
  return tilts_;
}

void PolySpline::mark_cache_invalid()
{
  tangent_cache_dirty_ = true;
  normal_cache_dirty_ = true;
  length_cache_dirty_ = true;
}

int PolySpline::evaluated_points_size() const
{
  return this->size();
}

void PolySpline::correct_end_tangents() const
{
}

Span<float3> PolySpline::evaluated_positions() const
{
  return this->positions();
}

/**
 * Poly spline interpolation from control points to evaluated points is a special case, since
 * the result data is the same as the input data. This function returns a GVArray that points to
 * the original data. Therefore the lifetime of the returned virtual array must not be longer than
 * the source data.
 */
blender::fn::GVArrayPtr PolySpline::interpolate_to_evaluated_points(
    const blender::fn::GVArray &source_data) const
{
  BLI_assert(source_data.size() == this->size());

  return source_data.shallow_copy();
}
