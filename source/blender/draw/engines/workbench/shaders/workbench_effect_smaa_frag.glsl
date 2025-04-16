/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_effect_antialiasing_info.hh"

/* Adjust according to SMAA_STAGE for C++ compilation. */
FRAGMENT_SHADER_CREATE_INFO(workbench_smaa_stage_1)

#include "gpu_shader_smaa_lib.glsl"

void main()
{
#if SMAA_STAGE == 0
  /* Detect edges in color and revealage buffer. */
  out_edges = SMAALumaEdgeDetectionPS(uvs, offset, colorTex);
  /* Discard if there is no edge. */
  if (dot(out_edges, float2(1.0f, 1.0f)) == 0.0f) {
    discard;
    return;
  }

#elif SMAA_STAGE == 1
  out_weights = SMAABlendingWeightCalculationPS(
      uvs, pixcoord, offset, edgesTex, areaTex, searchTex, float4(0));

#elif SMAA_STAGE == 2
  out_color = float4(0.0f);
  if (mixFactor > 0.0f) {
    out_color += SMAANeighborhoodBlendingPS(uvs, offset[0], colorTex, blendTex) * mixFactor;
  }
  if (mixFactor < 1.0f) {
    out_color += texture(colorTex, uvs) * (1.0f - mixFactor);
  }
  out_color /= taaAccumulatedWeight;
  /* Exit log2 space used for Anti-aliasing. */
  out_color = exp2(out_color) - 0.5f;

  /* Avoid float precision issue. */
  if (out_color.a > 0.999f) {
    out_color.a = 1.0f;
  }
#endif
}
