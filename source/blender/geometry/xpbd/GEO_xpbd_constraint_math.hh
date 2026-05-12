/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector.hh"

#include <optional>

namespace blender::xpbd {

struct PlaneIntersection {
  /* Intersection point. */
  float3 position;
  /* Position of the intersection relative to segment points. */
  float segment_lambda;
};

/**
 * Test intersection of a line segment with a plane defined by two tangent vectors.
 * \param pos0 First point of the line segment.
 * \param pos1 Second point of the line segment.
 * \param origin Point on the half-plane edge.
 * \param edge Edge of the half-plane.
 * \param normal Direction away from the half-plane.
 * \return Intersection if the line segment intersects the plane.
 */
inline std::optional<PlaneIntersection> intersect_plane(const float3 &pos0,
                                                        const float3 &pos1,
                                                        const float3 &origin,
                                                        const float3 &edge,
                                                        const float3 &normal)
{
  BLI_assert(math::is_unit(edge));
  BLI_assert(math::is_unit(normal));

  const float3 tangent = math::cross(edge, normal);
  BLI_assert(math::is_unit(tangent));

  const float len = math::dot(tangent, pos0 - pos1);
  if (math::abs(len) < 1e-9) {
    /* Segment is parallel to the plane or too short. */
    return std::nullopt;
  }

  const float dist0 = math::dot(tangent, pos0 - origin);
  const float lambda = dist0 / len;
  if (lambda < 0.0f || lambda > 1.0f) {
    return std::nullopt;
  }

  const float3 position = math::interpolate(pos0, pos1, lambda);
  return PlaneIntersection{position, lambda};
}

struct SegmentClosestToRay {
  /* Position relative to segment points. */
  float segment_lambda;
  /* Distance of the closest point along the ray, unclamped. */
  float ray_lambda;
};

/**
 * Find closest point of a segment to a ray.
 * \param pos0 First point of the line segment.
 * \param pos1 Second point of the line segment.
 * \param ray_pos Origin of the ray.
 * \param ray_dir Direction of the ray.
 * \return Closest point on the segment or null if the closest point is outside the segment.
 */
inline SegmentClosestToRay closest_on_segment_to_ray(const float3 &pos0,
                                                     const float3 &pos1,
                                                     const float3 &ray_pos,
                                                     const float3 &ray_dir,
                                                     const bool clamp)
{
  BLI_assert(math::is_unit(ray_dir));

  const float3 segment = pos1 - pos0;
  const float3 dist0 = pos0 - ray_pos;
  const float a = math::dot(segment, dist0);
  const float b = math::dot(ray_dir, dist0);
  const float c = math::dot(segment, ray_dir);

  const float len_segment_sq = math::length_squared(segment);
  if (UNLIKELY(len_segment_sq <= 1e-9)) {
    return SegmentClosestToRay{0.0f, b};
  }

  float segment_lambda;
  if (clamp) {
    segment_lambda = math::clamp(math::safe_divide(c * b - a, len_segment_sq - c * c), 0.0f, 1.0f);
  }
  else {
    segment_lambda = math::safe_divide(c * b - a, len_segment_sq - c * c);
  }

  const float ray_lambda = c * segment_lambda + b;

  return SegmentClosestToRay{segment_lambda, ray_lambda};
}

}  // namespace blender::xpbd
