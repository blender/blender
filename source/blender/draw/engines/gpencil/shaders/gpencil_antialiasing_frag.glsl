/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpencil_antialiasing_stage_1)

#include "gpu_shader_smaa_lib.glsl"

void main()
{
  float4 offset[3];
  offset[0] = offset0;
  offset[1] = offset1;
  offset[2] = offset2;

#if SMAA_STAGE == 0
  /* Detect edges in color and revealage buffer. */
  out_edges = SMAALumaEdgeDetectionPS(uvs, offset, color_tx);
  out_edges = max(out_edges, SMAALumaEdgeDetectionPS(uvs, offset, reveal_tx));
  /* Discard if there is no edge. */
  if (dot(out_edges, float2(1.0f, 1.0f)) == 0.0f) {
    gpu_discard_fragment();
    return;
  }

#elif SMAA_STAGE == 1
  out_weights = SMAABlendingWeightCalculationPS(
      uvs, pixcoord, offset, edges_tx, area_tx, search_tx, float4(0));

#elif SMAA_STAGE == 2
  /* Resolve both buffers. */
  if (do_anti_aliasing) {
    out_color = SMAANeighborhoodBlendingPS(uvs, offset[0], color_tx, blend_tx);
    out_reveal = SMAANeighborhoodBlendingPS(uvs, offset[0], reveal_tx, blend_tx);
  }
  else {
    out_color = texture(color_tx, uvs);
    out_reveal = texture(reveal_tx, uvs);
  }

  /* Revealage, how much light passes through. */
  /* Average for alpha channel. */
  out_reveal.a = clamp(dot(out_reveal.rgb, float3(0.333334f)), 0.0f, 1.0f);
  /* Color buffer is already pre-multiplied. Just add it to the color. */
  /* Add the alpha. */
  out_color.a = 1.0f - out_reveal.a;

  if (only_alpha) {
    /* Special case in wire-frame X-ray mode. */
    out_color = float4(0.0f);
    out_reveal.rgb = out_reveal.aaa;
  }
#endif
}
