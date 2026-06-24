/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <utility>

#include "BLI_math_vector.hh"

namespace blender::bke::pbvh {

/**
 * Rasterizes a triangle in pixel space using precomputed edge functions.
 * Uses canonical edge ordering to make it watertight.
 */
struct TriRasterizer {
 private:
  struct Edge {
    /* Edge function coefficients, so that:
     *
     * cross(edge0, edge1, (x, y)) =
     * coefficients.x * x + coefficients.y * y + coefficients.z
     */
    float3 coefficients;
    /* Are we inside when the edge function is positive? */
    bool positive_side;
  };

  Edge edges[3];

 public:
  float3 dx_step;
  float3 dy_step;

  TriRasterizer(const float2 p0, const float2 p1, const float2 p2)
  {
    const float2 p[3] = {p0, p1, p2};

    for (int i = 0; i < 3; i++) {
      float2 a = p[i];
      float2 b = p[(i + 1) % 3];
      const float2 c = p[(i + 2) % 3];
      /* Sort edges in canonical order. */
      if (b.x < a.x || (b.x == a.x && b.y < a.y)) {
        std::swap(a, b);
      }
      const float2 d = b - a;
      edges[i].coefficients = {-d.y, d.x, math::cross(a, d)};
      /* Check which side the c vertex is on. */
      edges[i].positive_side = (math::cross(d, c - a) > 0.0f);
    }

    dx_step = {edges[0].coefficients.x, edges[1].coefficients.x, edges[2].coefficients.x};
    dy_step = {edges[0].coefficients.y, edges[1].coefficients.y, edges[2].coefficients.y};
  }

  /* Edge function values at pixel center. */
  float3 edge_values(const int x, const int y) const
  {
    const float3 xyz = {float(x) + 0.5f, float(y) + 0.5f, 1.0f};
    return {math::dot(edges[0].coefficients, xyz),
            math::dot(edges[1].coefficients, xyz),
            math::dot(edges[2].coefficients, xyz)};
  }

  /* True if the point with edge values e is inside. */
  bool inside(const float3 e) const
  {
    /* Note the positive side uses >= and negative uses < to make it watertight. */
    return (edges[0].positive_side ? e.x >= 0.0f : e.x < 0.0f) &&
           (edges[1].positive_side ? e.y >= 0.0f : e.y < 0.0f) &&
           (edges[2].positive_side ? e.z >= 0.0f : e.z < 0.0f);
  }
};

}  // namespace blender::bke::pbvh
