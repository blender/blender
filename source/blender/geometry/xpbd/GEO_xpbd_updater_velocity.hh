/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_geometry_ref.hh"

namespace blender::xpbd {

/**
 * Updater that writes the changes directly to the simulated points.
 */
class VelocityUpdater {
 private:
  Span<GeometryRef> geometry_refs_;

 public:
  VelocityUpdater(const Span<GeometryRef> geometry_refs) : geometry_refs_(geometry_refs) {}

  void update_velocity(const int geo_i, const int point_i, const float3 &offset)
  {
    geometry_refs_[geo_i].velocities[point_i] += offset;
  }

  void update_angular_velocity(const int geo_i, const int point_i, const float3 &offset)
  {
    geometry_refs_[geo_i].angular_velocities[point_i] += offset;
  }
};

}  // namespace blender::xpbd
