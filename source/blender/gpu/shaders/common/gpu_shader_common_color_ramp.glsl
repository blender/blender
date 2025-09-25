/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

void valtorgb_opti_constant(
    float fac, float edge, float4 color1, float4 color2, out float4 outcol, out float outalpha)
{
  outcol = (fac > edge) ? color2 : color1;
  outalpha = outcol.a;
}

void valtorgb_opti_linear(
    float fac, float2 mulbias, float4 color1, float4 color2, out float4 outcol, out float outalpha)
{
  fac = clamp(fac * mulbias.x + mulbias.y, 0.0f, 1.0f);
  outcol = mix(color1, color2, fac);
  outalpha = outcol.a;
}

void valtorgb_opti_ease(
    float fac, float2 mulbias, float4 color1, float4 color2, out float4 outcol, out float outalpha)
{
  fac = clamp(fac * mulbias.x + mulbias.y, 0.0f, 1.0f);
  fac = fac * fac * (3.0f - 2.0f * fac);
  outcol = mix(color1, color2, fac);
  outalpha = outcol.a;
}

/* Color maps are stored in texture samplers, so ensure that the coordinate evaluates the sampler
 * at the center of the pixels, because samplers are evaluated using linear interpolation. Given
 * the coordinate in the [0, 1] range. */
float compute_color_map_coordinate(float coordinate)
{
  /* Color maps have a fixed width of 257. We offset by the equivalent of half a pixel and scale
   * down such that the normalized coordinate 1.0 corresponds to the center of the last pixel. */
  constexpr float sampler_resolution = 257.0f;
  constexpr float sampler_offset = 0.5f / sampler_resolution;
  constexpr float sampler_scale = 1.0f - (1.0f / sampler_resolution);
  return coordinate * sampler_scale + sampler_offset;
}

void valtorgb(
    float fac, sampler1DArray colormap, float layer, out float4 outcol, out float outalpha)
{
  outcol = texture(colormap, float2(compute_color_map_coordinate(fac), layer));
  outalpha = outcol.a;
}

void valtorgb_nearest(
    float fac, sampler1DArray colormap, float layer, out float4 outcol, out float outalpha)
{
  fac = clamp(fac, 0.0f, 1.0f);
  outcol = texelFetch(colormap, int2(fac * (textureSize(colormap, 0).x - 1), layer), 0);
  outalpha = outcol.a;
}
