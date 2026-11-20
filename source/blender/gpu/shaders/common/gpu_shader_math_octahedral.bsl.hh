/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Octahedral mapping of directions to the [0..1] square.
 * Upper hemisphere maps to the inner diamond, lower one is folded into the corners.
 *
 * References:
 *
 *  [octa2017]  Rune Stubbe
 *              Octahedral Encoding
 *              https://www.shadertoy.com/view/Mtfyzl (link is not author's source)
 */

#include "gpu_shader_compat.hh"
#include "gpu_shader_math_vector.bsl.hh"

/**
 * Convert from a direction to an octahedron UV coordinate [octa2017].
 * \a L doesn't need to be normalized.
 */
float2 octahedral_uv_from_direction(float3 L)
{
  /* Projection onto octahedron. */
  L /= length_manhattan(L);
  /* Out-folding of the downward faces. */
  if (L.z < 0.0f) {
    L.xy = (1.0f - abs(L.yx)) * (step(0.0f, L.xy) * 2.0f - 1.0f);
  }
  /* Mapping to [0;1]^2 texture space. */
  return L.xy * 0.5f + 0.5f;
}

/**
 * Convert from an octahedron UV coordinate to a direction [octa2017].
 * Returned vector is not normalized.
 */
float3 octahedral_uv_to_direction(float2 uv)
{
  uv = uv * 2.0f - 1.0f;
  float3 L = float3(uv, 1.0f - abs(uv.x) - abs(uv.y));
  float t = max(-L.z, 0.0f);
  L.x += (L.x >= 0.0f) ? -t : t;
  L.y += (L.y >= 0.0f) ? -t : t;
  return L;
}
