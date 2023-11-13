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

void valtorgb(float fac, sampler1DArray colormap, float layer, out vec4 outcol, out float outalpha)
{
  outcol = texture(colormap, vec2(fac, layer));
  outalpha = outcol.a;
}

void valtorgb_nearest(
    float fac, sampler1DArray colormap, float layer, out vec4 outcol, out float outalpha)
{
  fac = clamp(fac, 0.0, 1.0);
  outcol = texelFetch(colormap, ivec2(fac * (textureSize(colormap, 0).x - 1), layer), 0);
  outalpha = outcol.a;
}
