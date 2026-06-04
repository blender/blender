/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_defines.hh"
#include "eevee_lightprobe_shared.hh"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee::lightprobe::plane {

float3 parallax(PlanarProbeData planar, float3 P, float3 N, float3 V)
{
  /* Compute distorted reflection vector based on the distance to the reflected object.
   * In other words find intersection between reflection vector and the sphere center
   * around point_on_plane. */
  float3 proj_ref = reflect(-V, N) * planar.parallax_distance;
  /* Then reflect around the planar probe normal plane to get the final position on screen. */
  return P + reflect(proj_ref, planar.normal);
}

}  // namespace eevee::lightprobe::plane

namespace eevee {

struct LightprobePlaneRenderData {
  [[uniform(PLANAR_PROBE_BUF_SLOT)]] PlanarProbeData (&probe_planar_buf)[PLANAR_PROBE_MAX];
  [[sampler(PLANAR_PROBE_RADIANCE_TEX_SLOT)]] sampler2DArray planar_radiance_tx;
  [[sampler(PLANAR_PROBE_DEPTH_TEX_SLOT)]] sampler2DArrayDepth planar_depth_tx;

  /**
   * Return the best planar probe index for a given light direction vector and position.
   */
  int select_probe(float3 P, float3 N) const
  {
    float best_score = 0.0;
    int best_index = -1;

    for (int index = 0; index < PLANAR_PROBE_MAX; index++) {
      if (probe_planar_buf[index].layer_id == -1) {
        /* PlanarProbeData doesn't contain any gap, exit at first item that is invalid. */
        break;
      }
      float score = probe_score(probe_planar_buf[index], P, N);
      if (score > best_score) {
        best_score = score;
        best_index = index;
      }
    }
    return best_index;
  }

 private:
  static float distance_score(PlanarProbeData planar, float3 P)
  {
    float3 lP = float4(P, 1.0f) * planar.world_to_object_transposed;
    /* TODO: Transition in Z. Dither? */
    return float(all(lessThan(abs(lP), float3(1.0f))));
  }

  static float probe_score(PlanarProbeData planar, float3 P, float3 N)
  {
    return saturate(dot(N, planar.normal)) * distance_score(planar, P);
  }
};

}  // namespace eevee
