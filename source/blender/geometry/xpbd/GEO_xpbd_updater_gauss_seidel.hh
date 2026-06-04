/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_quaternion.hh"

#include "GEO_xpbd_geometry_ref.hh"

namespace blender::xpbd {

inline math::Quaternion apply_rotation_offset(math::Quaternion rotation, float4 offset)
{
  return math::normalize(math::Quaternion(float4(rotation) + offset));
}

/**
 * Updater that writes the changes directly to the simulated points.
 */
class GaussSeidelUpdater {
 private:
  Span<GeometryRef> geometry_refs_;
  float total_error_squared_;
  int total_error_count_;

 public:
  GaussSeidelUpdater(Span<GeometryRef> geometry_refs)
      : geometry_refs_(geometry_refs), total_error_squared_(0.0f), total_error_count_(0.0f)
  {
  }

  float total_error_squared() const
  {
    return total_error_squared_;
  }
  int total_error_count() const
  {
    return total_error_count_;
  }

  void update_position(const int geo_i, const int point_i, const float3 &offset)
  {
    geometry_refs_[geo_i].positions[point_i] += offset;
  }
  void update_rotation(const int geo_i, const int point_i, const math::Quaternion &offset)
  {
    math::Quaternion &rotation = geometry_refs_[geo_i].rotations[point_i];
    rotation = apply_rotation_offset(rotation, float4(offset));
  }
  void add_residual_error(const int /*geo_i*/, const float error_squared)
  {
    /* Geometry index is ignored for now for simplicity. We could record a separate error for each
     * geometry. */
    total_error_squared_ += error_squared;
    ++total_error_count_;
  }
};

}  // namespace blender::xpbd
