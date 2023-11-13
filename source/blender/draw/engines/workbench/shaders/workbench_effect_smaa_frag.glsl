/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_smaa_lib.glsl)

void main()
{
#if SMAA_STAGE == 0
  /* Detect edges in color and revealage buffer. */
  out_edges = SMAALumaEdgeDetectionPS(uvs, offset, colorTex);
  /* Discard if there is no edge. */
  if (dot(out_edges, float2(1.0, 1.0)) == 0.0) {
    discard;
    return;
  }

#elif SMAA_STAGE == 1
  out_weights = SMAABlendingWeightCalculationPS(
      uvs, pixcoord, offset, edgesTex, areaTex, searchTex, vec4(0));

#elif SMAA_STAGE == 2
  out_color = vec4(0.0);
  if (mixFactor > 0.0) {
    out_color += SMAANeighborhoodBlendingPS(uvs, offset[0], colorTex, blendTex) * mixFactor;
  }
  if (mixFactor < 1.0) {
    out_color += texture(colorTex, uvs) * (1.0 - mixFactor);
  }
  out_color /= taaAccumulatedWeight;
  /* Exit log2 space used for Anti-aliasing. */
  out_color = exp2(out_color) - 0.5;

  /* Avoid float precision issue. */
  if (out_color.a > 0.999) {
    out_color.a = 1.0;
  }
#endif
}
