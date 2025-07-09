/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* If straight_alpha is true, then the foreground is in straight alpha form and would need to be
 * premultiplied. */
float4 preprocess_foreground(float4 foreground, float straight_alpha)
{
  const float alpha = clamp(foreground.w, 0.0f, 1.0f);
  const float4 premultiplied_foreground = float4(foreground.xyz * alpha, alpha);
  return straight_alpha != 0.0f ? premultiplied_foreground : foreground;
}

/* Computes the Porter and Duff Over compositing operation. */
void node_composite_alpha_over(
    float factor, float4 background, float4 foreground, float straight_alpha, out float4 result)
{
  const float4 foreground_color = preprocess_foreground(foreground, straight_alpha);

  const float foreground_alpha = clamp(foreground.w, 0.0f, 1.0f);
  const float4 mix_result = foreground_color + background * (1.0f - foreground_alpha);

  result = mix(background, mix_result, factor);
}

/* Computes the Porter and Duff Over compositing operation while assuming the background is being
 * held out by the foreground. See for reference:
 *
 *   https://benmcewan.com/blog/disjoint-over-and-conjoint-over-explained */
void node_composite_alpha_over_disjoint(
    float factor, float4 background, float4 foreground, float straight_alpha, out float4 result)
{
  const float4 foreground_color = preprocess_foreground(foreground, straight_alpha);

  const float foreground_alpha = clamp(foreground.w, 0.0f, 1.0f);
  const float background_alpha = clamp(background.w, 0.0f, 1.0f);

  if (foreground_alpha + background_alpha < 1.0f) {
    const float4 mix_result = foreground_color + background;
    result = mix(background, mix_result, factor);
    return;
  }

  const float4 straight_background = safe_divide(background, background_alpha);
  const float4 mix_result = foreground_color + straight_background * (1.0f - foreground_alpha);

  result = mix(background, mix_result, factor);
}

/* Computes the Porter and Duff Over compositing operation but the foreground completely covers the
 * background if it is more opaque but not necessary completely opaque. See for reference:
 *
 *   https://benmcewan.com/blog/disjoint-over-and-conjoint-over-explained
 *
 * However, the equation is wrong and should actually be A+B(1-a/b), A if a>b. */
void node_composite_alpha_over_conjoint(
    float factor, float4 background, float4 foreground, float straight_alpha, out float4 result)
{
  const float4 foreground_color = preprocess_foreground(foreground, straight_alpha);

  const float foreground_alpha = clamp(foreground.w, 0.0f, 1.0f);
  const float background_alpha = clamp(background.w, 0.0f, 1.0f);

  if (foreground_alpha > background_alpha) {
    const float4 mix_result = foreground_color;
    result = mix(background, mix_result, factor);
    return;
  }

  const float alpha_ratio = safe_divide(foreground_alpha, background_alpha);
  const float4 mix_result = foreground_color + background * (1.0f - alpha_ratio);

  result = mix(background, mix_result, factor);
}
