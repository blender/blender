/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_effect_antialiasing_infos.hh"

/* Adjust according to SMAA_STAGE for C++ compilation. */
FRAGMENT_SHADER_CREATE_INFO(workbench_smaa_stage_1)

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
  /* Discard if there is no edge. */
  if (dot(out_edges, float2(1.0f, 1.0f)) == 0.0f) {
    gpu_discard_fragment();
    return;
  }

#elif SMAA_STAGE == 1
  out_weights = SMAABlendingWeightCalculationPS(
      uvs, pixcoord, offset, edges_tx, area_tx, search_tx, float4(0));

#elif SMAA_STAGE == 2
  out_color = float4(0.0f);
  if (mix_factor > 0.0f) {
    out_color += SMAANeighborhoodBlendingPS(uvs, offset[0], color_tx, blend_tx) * mix_factor;
  }
  if (mix_factor < 1.0f) {
    out_color += texture(color_tx, uvs) * (1.0f - mix_factor);
  }
  out_color /= taa_accumulated_weight;
  /* Exit log2 space used for Anti-aliasing. */
  out_color.rgb = exp2(out_color.rgb) - 1.0f;

  /* Avoid float precision issue. */
  if (out_color.a > 0.999f) {
    out_color.a = 1.0f;
  }
#endif
}
