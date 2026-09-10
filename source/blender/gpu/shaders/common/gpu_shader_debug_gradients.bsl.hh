/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/*
 * For debugging purpose mainly.
 * From https://www.shadertoy.com/view/4dsSzr
 * By Morgan McGuire @morgan3d, http://graphicscodex.com
 * Reuse permitted under the BSD license.
 */
float3 neon_gradient(float t)
{
  float tt = abs(0.43f - t) * 1.7f;
  return clamp(float3(t * 1.3f + 0.1f, tt * tt, (1.0f - t) * 1.7f), 0.0f, 1.0f);
}
float3 heatmap_gradient(float t)
{
  float a = pow(t, 1.5f) * 0.8f + 0.2f;
  float b = smoothstep(0.0f, 0.35f, t) + t * 0.5f;
  float c = smoothstep(0.5f, 1.0f, t);
  float d = max(1.0f - t * 1.7f, t * 7.0f - 6.0f);
  return clamp(a * float3(b, c, d), float3(0.0f), float3(1.0f));
}
float3 hue_gradient(float t)
{
  float3 p = abs(fract(t + float3(1.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f - 3.0f);
  return (clamp(p - 1.0f, 0.0f, 1.0f));
}
float3 green_to_red_gradient(float t)
{
  return mix(float3(0.0f, 1.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), t);
}
