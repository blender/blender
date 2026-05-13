/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

namespace colorspace {

/* -------------------------------------------------------------------- */
/** \name YCoCg
 * \{ */

float3 YCoCg_from_scene_linear(float3 rgb_color)
{
  const float3x3 colorspace_tx = transpose(float3x3(float3(1, 2, 1),     /* Y */
                                                    float3(2, 0, -2),    /* Co */
                                                    float3(-1, 2, -1))); /* Cg */
  return colorspace_tx * rgb_color;
}

float4 YCoCg_from_scene_linear(float4 rgba_color)
{
  return float4(YCoCg_from_scene_linear(rgba_color.rgb), rgba_color.a);
}

float3 scene_linear_from_YCoCg(float3 ycocg_color)
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

float4 scene_linear_from_YCoCg(float4 ycocg_color)
{
  return float4(scene_linear_from_YCoCg(ycocg_color.rgb), ycocg_color.a);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Logarithmic space
 *
 * Used to crunch highlights and noise during denoising accumulations.
 * \{ */

float3 log_from_scene_linear(float3 color)
{
  return log2(1.0f + color);
}
float4 log_from_scene_linear(float4 color)
{
  return float4(log_from_scene_linear(color.rgb), color.a);
}

float3 scene_linear_from_log(float3 color)
{
  return exp2(color) - 1.0f;
}
float4 scene_linear_from_log(float4 color)
{
  return float4(scene_linear_from_log(color.rgb), color.a);
}

/** \} */

/**
 * Clamp components to avoid black square artifacts if a pixel goes NaN or negative.
 * Threshold is arbitrary.
 */
float4 safe_color(float4 c)
{
  return clamp(c, float4(0.0f), float4(1e20f));
}
float3 safe_color(float3 c)
{
  return clamp(c, float3(0.0f), float3(1e20f));
}

/**
 * Clamp all components to the specified maximum and avoid color shifting.
 */
float3 brightness_clamp_max(float3 color, float limit)
{
  return color * saturate(limit / max(1e-8f, reduce_max(abs(color))));
}
float4 brightness_clamp_max(float4 color, float limit)
{
  return float4(brightness_clamp_max(color.rgb, limit), color.a);
}

}  // namespace colorspace
