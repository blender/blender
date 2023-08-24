/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_smaa_lib.glsl)

void main()
{
#if SMAA_STAGE == 0
  /* Detect edges in color and revealage buffer. */
  out_edges = SMAALumaEdgeDetectionPS(uvs, offset, colorTex);
  out_edges = max(out_edges, SMAALumaEdgeDetectionPS(uvs, offset, revealTex));
  /* Discard if there is no edge. */
  if (dot(out_edges, float2(1.0, 1.0)) == 0.0) {
    discard;
    return;
  }

#elif SMAA_STAGE == 1
  out_weights = SMAABlendingWeightCalculationPS(
      uvs, pixcoord, offset, edgesTex, areaTex, searchTex, vec4(0));

#elif SMAA_STAGE == 2
  /* Resolve both buffers. */
  if (doAntiAliasing) {
    out_color = SMAANeighborhoodBlendingPS(uvs, offset[0], colorTex, blendTex);
    out_reveal = SMAANeighborhoodBlendingPS(uvs, offset[0], revealTex, blendTex);
  }
  else {
    out_color = texture(colorTex, uvs);
    out_reveal = texture(revealTex, uvs);
  }

  /* Revealage, how much light passes through. */
  /* Average for alpha channel. */
  out_reveal.a = clamp(dot(out_reveal.rgb, vec3(0.333334)), 0.0, 1.0);
  /* Color buffer is already premultiplied. Just add it to the color. */
  /* Add the alpha. */
  out_color.a = 1.0 - out_reveal.a;

  if (onlyAlpha) {
    /* Special case in wire-frame X-ray mode. */
    out_color = vec4(0.0);
    out_reveal.rgb = out_reveal.aaa;
  }
#endif
}
