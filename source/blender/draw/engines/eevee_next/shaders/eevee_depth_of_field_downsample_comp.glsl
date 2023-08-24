/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Downsample pass: CoC aware downsample to quarter resolution.
 *
 * Pretty much identical to the setup pass but get CoC from buffer.
 * Also does not weight luma for the bilateral weights.
 */

#pragma BLENDER_REQUIRE(eevee_depth_of_field_lib.glsl)

void main()
{
  vec2 halfres_texel_size = 1.0 / vec2(textureSize(color_tx, 0).xy);
  /* Center uv around the 4 halfres pixels. */
  vec2 quad_center = vec2(gl_GlobalInvocationID.xy * 2 + 1) * halfres_texel_size;

  vec4 colors[4];
  vec4 cocs;
  for (int i = 0; i < 4; i++) {
    vec2 sample_uv = quad_center + quad_offsets[i] * halfres_texel_size;
    colors[i] = textureLod(color_tx, sample_uv, 0.0);
    cocs[i] = textureLod(coc_tx, sample_uv, 0.0).r;
  }

  vec4 weights = dof_bilateral_coc_weights(cocs);
  /* Normalize so that the sum is 1. */
  weights *= safe_rcp(sum(weights));

  vec4 out_color = weighted_sum_array(colors, weights);

  imageStore(out_color_img, ivec2(gl_GlobalInvocationID.xy), out_color);
}
