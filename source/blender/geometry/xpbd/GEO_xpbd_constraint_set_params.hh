/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_geometry_ref.hh"

namespace blender::xpbd {

/** Provides access to the input data that should be considered by a constraint. */
class ConstraintSetParams {
 private:
  Span<GeometryRef> geometry_refs_;

 public:
  float delta_time;
  float compliance_term_factor;
  float dynamic_friction_factor;

  ConstraintSetParams(Span<GeometryRef> geometry_refs, float delta_time);

  Span<GeometryRef> geometry_refs() const;

  Span<float3> positions(int geo_i) const;
  const float3 &position(int geo_i, int point_i) const;

  Span<math::Quaternion> rotations(int geo_i) const;
  const math::Quaternion &rotation(int geo_i, int point_i) const;

  Span<float3> prev_positions(int geo_i) const;
  const float3 &prev_position(int geo_i, int point_i) const;

  Span<math::Quaternion> prev_rotations(int geo_i) const;
  const math::Quaternion &prev_rotation(int geo_i, int point_i) const;

  Span<float3> velocities(int geo_i) const;
  const float3 &velocity(int geo_i, int point_i) const;

  Span<float3> angular_velocities(int geo_i) const;
  const float3 &angular_velocity(int geo_i, int point_i) const;

  Span<float> inv_masses(int geo_i) const;
  float inv_mass(int geo_i, int point_i) const;

  Span<float3> moments_of_inertia(int geo_i) const;
  float3 moment_of_inertia(int geo_i, int point_i) const;

  Span<float3> inv_moments_of_inertia(int geo_i) const;
  float3 inv_moment_of_inertia(int geo_i, int point_i) const;
};

/* -------------------------------------------------------------------- */
/** \name Inline Functions
 * \{ */

inline ConstraintSetParams::ConstraintSetParams(Span<GeometryRef> geometry_refs,
                                                const float delta_time)
    : geometry_refs_(geometry_refs),
      delta_time(delta_time),
      compliance_term_factor(math::safe_rcp(delta_time * delta_time)),
      dynamic_friction_factor(math::safe_rcp(delta_time))
{
}

inline Span<GeometryRef> ConstraintSetParams::geometry_refs() const
{
  return geometry_refs_;
}

inline const float3 &ConstraintSetParams::position(const int geo_i, const int point_i) const
{
  return geometry_refs_[geo_i].positions[point_i];
}

inline const math::Quaternion &ConstraintSetParams::rotation(const int geo_i,
                                                             const int point_i) const
{
  return geometry_refs_[geo_i].rotations[point_i];
}

inline const float3 &ConstraintSetParams::prev_position(const int geo_i, const int point_i) const
{
  return geometry_refs_[geo_i].prev_positions[point_i];
}

inline const math::Quaternion &ConstraintSetParams::prev_rotation(const int geo_i,
                                                                  const int point_i) const
{
  return geometry_refs_[geo_i].prev_rotations[point_i];
}

inline const float3 &ConstraintSetParams::velocity(const int geo_i, const int point_i) const
{
  return geometry_refs_[geo_i].velocities[point_i];
}

inline const float3 &ConstraintSetParams::angular_velocity(const int geo_i,
                                                           const int point_i) const
{
  return geometry_refs_[geo_i].angular_velocities[point_i];
}

inline Span<float3> ConstraintSetParams::positions(const int geo_i) const
{
  return geometry_refs_[geo_i].positions;
}

inline Span<math::Quaternion> ConstraintSetParams::rotations(const int geo_i) const
{
  return geometry_refs_[geo_i].rotations;
}

inline Span<float3> ConstraintSetParams::prev_positions(const int geo_i) const
{
  return geometry_refs_[geo_i].prev_positions;
}

inline Span<math::Quaternion> ConstraintSetParams::prev_rotations(const int geo_i) const
{
  return geometry_refs_[geo_i].prev_rotations;
}

inline Span<float3> ConstraintSetParams::velocities(const int geo_i) const
{
  return geometry_refs_[geo_i].velocities;
}

inline Span<float3> ConstraintSetParams::angular_velocities(const int geo_i) const
{
  return geometry_refs_[geo_i].angular_velocities;
}

inline float ConstraintSetParams::inv_mass(const int geo_i, const int point_i) const
{
  return geometry_refs_[geo_i].inv_masses[point_i];
}

inline Span<float> ConstraintSetParams::inv_masses(const int geo_i) const
{
  return geometry_refs_[geo_i].inv_masses;
}

inline float3 ConstraintSetParams::moment_of_inertia(const int geo_i, const int point_i) const
{
  return geometry_refs_[geo_i].moments_of_inertia[point_i];
}

inline Span<float3> ConstraintSetParams::moments_of_inertia(const int geo_i) const
{
  return geometry_refs_[geo_i].moments_of_inertia;
}

inline float3 ConstraintSetParams::inv_moment_of_inertia(const int geo_i, const int point_i) const
{
  return geometry_refs_[geo_i].inv_moments_of_inertia[point_i];
}

inline Span<float3> ConstraintSetParams::inv_moments_of_inertia(const int geo_i) const
{
  return geometry_refs_[geo_i].inv_moments_of_inertia;
}

/** \} */

}  // namespace blender::xpbd
