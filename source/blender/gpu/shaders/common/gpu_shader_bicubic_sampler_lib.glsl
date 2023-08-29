/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \param f: Offset from texel center in pixel space. */
void cubic_bspline_coefficients(vec2 f, out vec2 w0, out vec2 w1, out vec2 w2, out vec2 w3)
{
  vec2 f2 = f * f;
  vec2 f3 = f2 * f;
  /* Optimized formulae for cubic B-Spline coefficients. */
  w3 = f3 / 6.0;
  w0 = -w3 + f2 * 0.5 - f * 0.5 + 1.0 / 6.0;
  w1 = f3 * 0.5 - f2 * 1.0 + 2.0 / 3.0;
  w2 = 1.0 - w0 - w1 - w3;
}

/* Samples the given 2D sampler at the given coordinates using Bicubic interpolation. This function
 * uses an optimized algorithm which assumes a linearly filtered sampler, so the caller needs to
 * take that into account when setting up the sampler. */
vec4 texture_bicubic(sampler2D sampler_2d, vec2 coordinates)
{
  vec2 texture_size = vec2(textureSize(sampler_2d, 0).xy);
  coordinates.xy *= texture_size;

  vec2 w0, w1, w2, w3;
  vec2 texel_center = floor(coordinates.xy - 0.5) + 0.5;
  cubic_bspline_coefficients(coordinates.xy - texel_center, w0, w1, w2, w3);

#if 1 /* Optimized version using 4 filtered taps. */
  vec2 s0 = w0 + w1;
  vec2 s1 = w2 + w3;

  vec2 f0 = w1 / (w0 + w1);
  vec2 f1 = w3 / (w2 + w3);

  vec4 sampling_coordinates;
  sampling_coordinates.xy = texel_center - 1.0 + f0;
  sampling_coordinates.zw = texel_center + 1.0 + f1;

  sampling_coordinates /= texture_size.xyxy;

  vec4 sampled_color = textureLod(sampler_2d, sampling_coordinates.xy, 0.0) * s0.x * s0.y;
  sampled_color += textureLod(sampler_2d, sampling_coordinates.zy, 0.0) * s1.x * s0.y;
  sampled_color += textureLod(sampler_2d, sampling_coordinates.xw, 0.0) * s0.x * s1.y;
  sampled_color += textureLod(sampler_2d, sampling_coordinates.zw, 0.0) * s1.x * s1.y;

  return sampled_color;

#else /* Reference brute-force 16 taps. */
  vec4 color = texelFetch(sampler_2d, ivec2(texel_center + vec2(-1.0, -1.0)), 0) * w0.x * w0.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(0.0, -1.0)), 0) * w1.x * w0.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(1.0, -1.0)), 0) * w2.x * w0.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(2.0, -1.0)), 0) * w3.x * w0.y;

  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(-1.0, 0.0)), 0) * w0.x * w1.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(0.0, 0.0)), 0) * w1.x * w1.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(1.0, 0.0)), 0) * w2.x * w1.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(2.0, 0.0)), 0) * w3.x * w1.y;

  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(-1.0, 1.0)), 0) * w0.x * w2.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(0.0, 1.0)), 0) * w1.x * w2.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(1.0, 1.0)), 0) * w2.x * w2.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(2.0, 1.0)), 0) * w3.x * w2.y;

  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(-1.0, 2.0)), 0) * w0.x * w3.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(0.0, 2.0)), 0) * w1.x * w3.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(1.0, 2.0)), 0) * w2.x * w3.y;
  color += texelFetch(sampler_2d, ivec2(texel_center + vec2(2.0, 2.0)), 0) * w3.x * w3.y;

  return color;
#endif
}
