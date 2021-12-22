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
using blender::fn::GVArray;

void PolySpline::copy_settings(Spline &UNUSED(dst)) const
{
  /* Poly splines have no settings not covered by the base class. */
}

void PolySpline::copy_data(Spline &dst) const
{
  PolySpline &poly = static_cast<PolySpline &>(dst);
  poly.positions_ = positions_;
  poly.radii_ = radii_;
  poly.tilts_ = tilts_;
}

int PolySpline::size() const
{
  const int size = positions_.size();
  BLI_assert(size == radii_.size());
  BLI_assert(size == tilts_.size());
  return size;
}

void PolySpline::resize(const int size)
{
  positions_.resize(size);
  radii_.resize(size);
  tilts_.resize(size);
  this->mark_cache_invalid();
  attributes.reallocate(size);
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

void PolySpline::reverse_impl()
{
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

GVArray PolySpline::interpolate_to_evaluated(const GVArray &src) const
{
  BLI_assert(src.size() == this->size());
  return src;
}
