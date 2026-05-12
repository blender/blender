/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

namespace blender::xpbd {

/**
 * References to the data of a geometry that is being simulated.
 */
struct GeometryRef {
  /** The position of each point. */
  MutableSpan<float3> positions;
  /** The linear velocity of each point. */
  MutableSpan<float3> velocities;
  /** Positions before time integration, at the beginning of the current substep. */
  Span<float3> prev_positions;
  /** Inverse mass of each point. */
  Span<float> inv_masses;

  /** Optional rotation data. */
  MutableSpan<math::Quaternion> rotations;
  /** Optional angular_velocity data. */
  MutableSpan<float3> angular_velocities;
  /** Rotations before time integration. */
  Span<math::Quaternion> prev_rotations;
  /** Optional moment of inertia of each point. */
  Span<float3> moments_of_inertia;
  /** Inverse of the above. */
  Span<float3> inv_moments_of_inertia;

  uint64_t size() const
  {
    return this->positions.size();
  }
};

}  // namespace blender::xpbd
