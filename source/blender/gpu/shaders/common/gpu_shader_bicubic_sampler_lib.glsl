/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/** \param f: Offset from texel center in pixel space. */
void cubic_bspline_coefficients(
    float2 f, out float2 w0, out float2 w1, out float2 w2, out float2 w3)
{
  float2 f2 = f * f;
  float2 f3 = f2 * f;
  /* Optimized formulae for cubic B-Spline coefficients. */
  w3 = f3 / 6.0f;
  w0 = -w3 + f2 * 0.5f - f * 0.5f + 1.0f / 6.0f;
  w1 = f3 * 0.5f - f2 * 1.0f + 2.0f / 3.0f;
  w2 = 1.0f - w0 - w1 - w3;
}

/* Samples the given 2D sampler at the given coordinates using Bicubic interpolation. This function
 * uses an optimized algorithm which assumes a linearly filtered sampler, so the caller needs to
 * take that into account when setting up the sampler. */
float4 texture_bicubic(sampler2D sampler_2d, float2 coordinates)
{
  float2 texture_size = float2(textureSize(sampler_2d, 0).xy);
  coordinates.xy *= texture_size;

  float2 w0, w1, w2, w3;
  float2 texel_center = floor(coordinates.xy - 0.5f) + 0.5f;
  cubic_bspline_coefficients(coordinates.xy - texel_center, w0, w1, w2, w3);

#if 1 /* Optimized version using 4 filtered taps. */
  float2 s0 = w0 + w1;
  float2 s1 = w2 + w3;

  float2 f0 = w1 / (w0 + w1);
  float2 f1 = w3 / (w2 + w3);

  float4 sampling_coordinates;
  sampling_coordinates.xy = texel_center - 1.0f + f0;
  sampling_coordinates.zw = texel_center + 1.0f + f1;

  sampling_coordinates /= texture_size.xyxy;

  float4 sampled_color = textureLod(sampler_2d, sampling_coordinates.xy, 0.0f) * s0.x * s0.y;
  sampled_color += textureLod(sampler_2d, sampling_coordinates.zy, 0.0f) * s1.x * s0.y;
  sampled_color += textureLod(sampler_2d, sampling_coordinates.xw, 0.0f) * s0.x * s1.y;
  sampled_color += textureLod(sampler_2d, sampling_coordinates.zw, 0.0f) * s1.x * s1.y;

  return sampled_color;

#else /* Reference brute-force 16 taps. */
  float4 color = texelFetch(sampler_2d, int2(texel_center + float2(-1.0f, -1.0f)), 0) * w0.x *
                 w0.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(0.0f, -1.0f)), 0) * w1.x * w0.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(1.0f, -1.0f)), 0) * w2.x * w0.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(2.0f, -1.0f)), 0) * w3.x * w0.y;

  color += texelFetch(sampler_2d, int2(texel_center + float2(-1.0f, 0.0f)), 0) * w0.x * w1.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(0.0f, 0.0f)), 0) * w1.x * w1.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(1.0f, 0.0f)), 0) * w2.x * w1.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(2.0f, 0.0f)), 0) * w3.x * w1.y;

  color += texelFetch(sampler_2d, int2(texel_center + float2(-1.0f, 1.0f)), 0) * w0.x * w2.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(0.0f, 1.0f)), 0) * w1.x * w2.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(1.0f, 1.0f)), 0) * w2.x * w2.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(2.0f, 1.0f)), 0) * w3.x * w2.y;

  color += texelFetch(sampler_2d, int2(texel_center + float2(-1.0f, 2.0f)), 0) * w0.x * w3.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(0.0f, 2.0f)), 0) * w1.x * w3.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(1.0f, 2.0f)), 0) * w2.x * w3.y;
  color += texelFetch(sampler_2d, int2(texel_center + float2(2.0f, 2.0f)), 0) * w3.x * w3.y;

  return color;
#endif
}
