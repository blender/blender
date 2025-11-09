/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once
#include "gpu_shader_compat.hh"

/* A shorthand for 1D textureSize with a zero LOD. */
int texture_size(sampler1D sampler_1d)
{
  return textureSize(sampler_1d, 0);
}

/* A shorthand for 1D texelFetch with zero LOD and bounded access clamped to border. */
float4 texture_load(sampler1D sampler_1d, int x)
{
  const int texture_bound = texture_size(sampler_1d) - 1;
  return texelFetch(sampler_1d, clamp(x, 0, texture_bound), 0);
}

/* A shorthand for 2D textureSize with a zero LOD. */
int2 texture_size(sampler2D sampler_2d)
{
  return textureSize(sampler_2d, 0);
}

/* A shorthand for 2D texelFetch with zero LOD and bounded access clamped to border. */
float4 texture_load(sampler2D sampler_2d, int2 texel)
{
  const int2 texture_bounds = texture_size(sampler_2d) - int2(1);
  return texelFetch(sampler_2d, clamp(texel, int2(0), texture_bounds), 0);
}

/* A shorthand for 2D texelFetch with zero LOD. */
float4 texture_load_unbound(sampler2D sampler_2d, int2 texel)
{
  return texelFetch(sampler_2d, texel, 0);
}

/* A shorthand for 2D texelFetch with zero LOD and a fallback value for out-of-bound access. */
float4 texture_load(sampler2D sampler_2d, int2 texel, float4 fallback)
{
  const int2 texture_bounds = texture_size(sampler_2d) - int2(1);
  if (any(lessThan(texel, int2(0))) || any(greaterThan(texel, texture_bounds))) {
    return fallback;
  }
  return texelFetch(sampler_2d, texel, 0);
}

/* A shorthand for 2D textureSize with a zero LOD. */
int2 texture_size(isampler2D sampler_2d)
{
  return textureSize(sampler_2d, 0);
}

/* A shorthand for 2D texelFetch with zero LOD and bounded access clamped to border. */
int4 texture_load(isampler2D sampler_2d, int2 texel)
{
  const int2 texture_bounds = texture_size(sampler_2d) - int2(1);
  return texelFetch(sampler_2d, clamp(texel, int2(0), texture_bounds), 0);
}

/* A shorthand for 2D texelFetch with zero LOD and a fallback value for out-of-bound access. */
int4 texture_load(isampler2D sampler_2d, int2 texel, int4 fallback)
{
  const int2 texture_bounds = texture_size(sampler_2d) - int2(1);
  if (any(lessThan(texel, int2(0))) || any(greaterThan(texel, texture_bounds))) {
    return fallback;
  }
  return texelFetch(sampler_2d, texel, 0);
}
