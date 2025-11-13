/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This pass scan the depth buffer and tag all tiles that are needed for light shadowing as
 * needed.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_shadow_tag_usage_opaque)

#include "eevee_shadow_tag_usage_lib.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int2 tex_size = input_depth_extent;

  if (!in_range_inclusive(texel, int2(0), int2(tex_size - 1))) {
    return;
  }

  float depth = texelFetch(hiz_tx, texel, 0).r;
  if (depth == 1.0f) {
    return;
  }

  float2 uv = (float2(texel) + 0.5f) / float2(tex_size);
  float3 vP = drw_point_screen_to_view(float3(uv, depth));
  float3 P = drw_point_view_to_world(vP);
  float2 pixel = float2(gl_GlobalInvocationID.xy);

  shadow_tag_usage(vP, P, pixel);
}
