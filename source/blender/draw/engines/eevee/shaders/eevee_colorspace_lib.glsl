/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name YCoCg
 * \{ */

float3 colorspace_YCoCg_from_scene_linear(float3 rgb_color)
{
  const float3x3 colorspace_tx = transpose(float3x3(float3(1, 2, 1),     /* Y */
                                                    float3(2, 0, -2),    /* Co */
                                                    float3(-1, 2, -1))); /* Cg */
  return colorspace_tx * rgb_color;
}

float4 colorspace_YCoCg_from_scene_linear(float4 rgba_color)
{
  return float4(colorspace_YCoCg_from_scene_linear(rgba_color.rgb), rgba_color.a);
}

float3 colorspace_scene_linear_from_YCoCg(float3 ycocg_color)
{
  float Y = ycocg_color.x;
  float Co = ycocg_color.y;
  float Cg = ycocg_color.z;

  float3 rgb_color;
  rgb_color.r = Y + Co - Cg;
  rgb_color.g = Y + Cg;
  rgb_color.b = Y - Co - Cg;
  return rgb_color * 0.25f;
}

float4 colorspace_scene_linear_from_YCoCg(float4 ycocg_color)
{
  return float4(colorspace_scene_linear_from_YCoCg(ycocg_color.rgb), ycocg_color.a);
}

/** \} */

/**
 * Clamp components to avoid black square artifacts if a pixel goes NaN or negative.
 * Threshold is arbitrary.
 */
float4 colorspace_safe_color(float4 c)
{
  return clamp(c, float4(0.0f), float4(1e20f));
}
float3 colorspace_safe_color(float3 c)
{
  return clamp(c, float3(0.0f), float3(1e20f));
}

/**
 * Clamp all components to the specified maximum and avoid color shifting.
 */
float3 colorspace_brightness_clamp_max(float3 color, float limit)
{
  return color * saturate(limit / max(1e-8f, reduce_max(abs(color))));
}
float4 colorspace_brightness_clamp_max(float4 color, float limit)
{
  return float4(colorspace_brightness_clamp_max(color.rgb, limit), color.a);
}
