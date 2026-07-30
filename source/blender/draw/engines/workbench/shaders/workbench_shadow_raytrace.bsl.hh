/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view_lib.glsl"
#include "gpu_shader_fullscreen_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "workbench_common.bsl.hh"
#include "workbench_shader_shared.hh"

namespace workbench::shadow::rt {

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[uniform(1)]] const ShadowPassData &pass_data;
  [[sampler(2)]] const sampler2DDepth depth_tx;
  [[sampler(3)]] const sampler2D normal_tx;
  [[acceleration_structure(0)]] const accelerationStructureEXT shadow_as;
};

[[vertex]] void vert([[vertex_id]] const int &vert_id, [[position]] float4 &out_pos)
{
  fullscreen_vertex(vert_id, out_pos);
}

[[fragment]] void frag([[frag_coord]] const float4 &frag_coord, [[resource_table]] Resources &srt)
{
  const float2 resolution = float2(textureSize(srt.depth_tx, 0).xy);
  const float2 screen_uv = frag_coord.xy / resolution;

  const float depth = texture(srt.depth_tx, screen_uv).r;
  if (depth == 1.0f) {
    gpu_discard_fragment();
    return;
  }

  const float3 N = drw_normal_view_to_world(
      workbench::normal_decode(texture(srt.normal_tx, screen_uv)));
  if (dot(N, srt.pass_data.light_direction_ws) >= 0.0f) {
    /* We already know the fragment is in shadow. No need to query. */
    return;
  }

  /* Offset depth to compensate for depth buffer precision. */
  float3 P = drw_point_screen_to_world(
      float3(screen_uv, intBitsToFloat(floatBitsToInt(depth) - 2)));

  const float pixel_size = srt.pass_data.pixel_size *
                           (drw_view_is_perspective() ? drw_view_z_distance(P) : 1.0f);

  /* Offset by pixel size to compensate for floating point precision. */
  const float tMin = pixel_size / max(dot(N, -srt.pass_data.light_direction_ws), 0.1f);

  rayQueryEXT query;
  rayQueryInitializeEXT(query,
                        srt.shadow_as,
                        gl_RayFlagsTerminateOnFirstHitEXT,
                        0xFF,
                        P,
                        tMin,
                        -srt.pass_data.light_direction_ws,
                        FLT_MAX);
  rayQueryProceedEXT(query);

  const bool is_light_occluded = rayQueryGetIntersectionTypeEXT(query, true) !=
                                 gl_RayQueryCommittedIntersectionNoneEXT;

  if (!is_light_occluded) {
    /* Writing the stencil means the fragment is in shadow. */
    gpu_discard_fragment();
    return;
  }
}

PipelineGraphic raytrace(vert, frag);

}  // namespace workbench::shadow::rt
