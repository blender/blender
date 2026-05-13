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

 public:
  GaussSeidelUpdater(Span<GeometryRef> geometry_refs) : geometry_refs_(geometry_refs) {}

  void update_position(const int geo_i, const int point_i, const float3 &offset)
  {
    geometry_refs_[geo_i].positions[point_i] += offset;
  }
  void update_rotation(const int geo_i, const int point_i, const math::Quaternion &offset)
  {
    math::Quaternion &rotation = geometry_refs_[geo_i].rotations[point_i];
    rotation = apply_rotation_offset(rotation, float4(offset));
  }
};

}  // namespace blender::xpbd
