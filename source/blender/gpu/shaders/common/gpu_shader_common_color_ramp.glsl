/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void valtorgb_opti_constant(
    float fac, float edge, vec4 color1, vec4 color2, out vec4 outcol, out float outalpha)
{
  outcol = (fac > edge) ? color2 : color1;
  outalpha = outcol.a;
}

void valtorgb_opti_linear(
    float fac, vec2 mulbias, vec4 color1, vec4 color2, out vec4 outcol, out float outalpha)
{
  fac = clamp(fac * mulbias.x + mulbias.y, 0.0, 1.0);
  outcol = mix(color1, color2, fac);
  outalpha = outcol.a;
}

void valtorgb_opti_ease(
    float fac, vec2 mulbias, vec4 color1, vec4 color2, out vec4 outcol, out float outalpha)
{
  fac = clamp(fac * mulbias.x + mulbias.y, 0.0, 1.0);
  fac = fac * fac * (3.0 - 2.0 * fac);
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
  const float sampler_resolution = 257.0;
  const float sampler_offset = 0.5 / sampler_resolution;
  const float sampler_scale = 1.0 - (1.0 / sampler_resolution);
  return coordinate * sampler_scale + sampler_offset;
}

void valtorgb(float fac, sampler1DArray colormap, float layer, out vec4 outcol, out float outalpha)
{
  outcol = texture(colormap, vec2(compute_color_map_coordinate(fac), layer));
  outalpha = outcol.a;
}

void valtorgb_nearest(
    float fac, sampler1DArray colormap, float layer, out vec4 outcol, out float outalpha)
{
  fac = clamp(fac, 0.0, 1.0);
  outcol = texelFetch(colormap, ivec2(fac * (textureSize(colormap, 0).x - 1), layer), 0);
  outalpha = outcol.a;
}
