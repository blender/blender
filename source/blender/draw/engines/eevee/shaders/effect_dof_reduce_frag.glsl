/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Reduce pass: Downsample the color buffer to generate mipmaps.
 * Also decide if a pixel is to be convolved by scattering or gathering during the first pass.
 */

#pragma BLENDER_REQUIRE(effect_dof_lib.glsl)

#ifdef COPY_PASS

/* NOTE: Do not compare alpha as it is not scattered by the scatter pass. */
float dof_scatter_neighborhood_rejection(vec3 color)
{
  DEFINE_DOF_QUAD_OFFSETS;
  color = min(vec3(scatterColorNeighborMax), color);

  float validity = 0.0;

  /* Centered in the middle of 4 quarter res texel. */
  vec2 texel_size = 1.0 / vec2(textureSize(downsampledBuffer, 0).xy);
  vec2 uv = (gl_FragCoord.xy * 0.5) * texel_size;

  vec3 max_diff = vec3(0.0);
  for (int i = 0; i < 4; i++) {
    vec2 sample_uv = uv + quad_offsets[i] * texel_size;
    vec3 ref = textureLod(downsampledBuffer, sample_uv, 0.0).rgb;

    ref = min(vec3(scatterColorNeighborMax), ref);
    float diff = max_v3(max(vec3(0.0), abs(ref - color)));

    const float rejection_threshold = 0.7;
    diff = saturate(diff / rejection_threshold - 1.0);
    validity = max(validity, diff);
  }

  return validity;
}

/* This avoids sprite popping in and out at the screen border and
 * drawing sprites larger than the screen. */
float dof_scatter_screen_border_rejection(float coc, vec2 uv, vec2 screen_size)
{
  vec2 screen_pos = uv * screen_size;
  float min_screen_border_distance = min_v2(min(screen_pos, screen_size - screen_pos));
  /* Full-resolution to half-resolution CoC. */
  coc *= 0.5;
  /* Allow 10px transition. */
  const float rejection_hardeness = 1.0 / 10.0;
  return saturate((min_screen_border_distance - abs(coc)) * rejection_hardeness + 1.0);
}

float dof_scatter_luminosity_rejection(vec3 color)
{
  const float rejection_hardness = 1.0;
  return saturate(max_v3(color - scatterColorThreshold) * rejection_hardness);
}

float dof_scatter_coc_radius_rejection(float coc)
{
  const float rejection_hardness = 0.3;
  return saturate((abs(coc) - scatterCocThreshold) * rejection_hardness);
}

float fast_luma(vec3 color)
{
  return (2.0 * color.g) + color.r + color.b;
}

/* Lightweight version of neighborhood clamping found in TAA. */
vec3 dof_neighborhood_clamping(vec3 color)
{
  vec2 texel_size = 1.0 / vec2(textureSize(colorBuffer, 0));
  vec2 uv = gl_FragCoord.xy * texel_size;
  vec4 ofs = vec4(-1, 1, -1, 1) * texel_size.xxyy;

  /* Luma clamping. 3x3 square neighborhood. */
  float c00 = fast_luma(textureLod(colorBuffer, uv + ofs.xz, 0.0).rgb);
  float c01 = fast_luma(textureLod(colorBuffer, uv + ofs.xz * vec2(1.0, 0.0), 0.0).rgb);
  float c02 = fast_luma(textureLod(colorBuffer, uv + ofs.xw, 0.0).rgb);

  float c10 = fast_luma(textureLod(colorBuffer, uv + ofs.xz * vec2(0.0, 1.0), 0.0).rgb);
  float c11 = fast_luma(color);
  float c12 = fast_luma(textureLod(colorBuffer, uv + ofs.xw * vec2(0.0, 1.0), 0.0).rgb);

  float c20 = fast_luma(textureLod(colorBuffer, uv + ofs.yz, 0.0).rgb);
  float c21 = fast_luma(textureLod(colorBuffer, uv + ofs.yz * vec2(1.0, 0.0), 0.0).rgb);
  float c22 = fast_luma(textureLod(colorBuffer, uv + ofs.yw, 0.0).rgb);

  float avg_luma = avg8(c00, c01, c02, c10, c12, c20, c21, c22);
  float max_luma = max8(c00, c01, c02, c10, c12, c20, c21, c22);

  float upper_bound = mix(max_luma, avg_luma, colorNeighborClamping);
  upper_bound = mix(c11, upper_bound, colorNeighborClamping);

  float clamped_luma = min(upper_bound, c11);

  return color * clamped_luma * safe_rcp(c11);
}

/* Simple copy pass where we select what pixels to scatter. Also the resolution might change.
 * NOTE: The texture can end up being too big because of the mipmap padding. We correct for
 * that during the convolution phase. */
void main()
{
  vec2 halfres = vec2(textureSize(colorBuffer, 0).xy);
  vec2 uv = gl_FragCoord.xy / halfres;

  outColor = textureLod(colorBuffer, uv, 0.0);
  outCoc = textureLod(cocBuffer, uv, 0.0).r;

  outColor.rgb = dof_neighborhood_clamping(outColor.rgb);

  /* Only scatter if luminous enough. */
  float do_scatter = dof_scatter_luminosity_rejection(outColor.rgb);
  /* Only scatter if CoC is big enough. */
  do_scatter *= dof_scatter_coc_radius_rejection(outCoc);
  /* Only scatter if CoC is not too big to avoid performance issues. */
  do_scatter *= dof_scatter_screen_border_rejection(outCoc, uv, halfres);
  /* Only scatter if neighborhood is different enough. */
  do_scatter *= dof_scatter_neighborhood_rejection(outColor.rgb);
  /* For debugging. */
  do_scatter *= float(!no_scatter_pass);

  outScatterColor = mix(vec3(0.0), outColor.rgb, do_scatter);
  outColor.rgb = mix(outColor.rgb, vec3(0.0), do_scatter);

  /* Apply energy conservation to anamorphic scattered bokeh. */
  outScatterColor /= min_v2(bokehAnisotropy);
}

#else /* REDUCE_PASS */

/* Downsample pass done for each mip starting from mip1. */
void main()
{
  DEFINE_DOF_QUAD_OFFSETS
  vec2 input_texel_size = 1.0 / vec2(textureSize(colorBuffer, 0).xy);
  /* Center uv around the 4 pixels of the previous mip. */
  vec2 quad_center = (floor(gl_FragCoord.xy) * 2.0 + 1.0) * input_texel_size;

  vec4 colors[4];
  vec4 cocs;
  for (int i = 0; i < 4; i++) {
    vec2 sample_uv = quad_center + quad_offsets[i] * input_texel_size;
    colors[i] = dof_load_gather_color(colorBuffer, sample_uv, 0.0);
    cocs[i] = textureLod(cocBuffer, sample_uv, 0.0).r;
  }

  vec4 weights = dof_downsample_bilateral_coc_weights(cocs);
  weights *= dof_downsample_bilateral_color_weights(colors);
  /* Normalize so that the sum is 1. */
  weights *= safe_rcp(sum(weights));

  outColor = weighted_sum_array(colors, weights);
  outCoc = dot(cocs, weights);
}

#endif
