/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

#define CMP_NODE_ALPHA_OVER_OPERATION_TYPE_OVER 0
#define CMP_NODE_ALPHA_OVER_OPERATION_TYPE_DISJOINT_OVER 1
#define CMP_NODE_ALPHA_OVER_OPERATION_TYPE_CONJOINT_OVER 2

/* If straight_alpha is true, then the foreground is in straight alpha form and would need to be
 * premultiplied. */
float4 preprocess_foreground(float4 foreground, float straight_alpha)
{
  const float alpha = clamp(foreground.w, 0.0f, 1.0f);
  const float4 premultiplied_foreground = float4(foreground.xyz * alpha, alpha);
  return straight_alpha != 0.0f ? premultiplied_foreground : foreground;
}

/* Computes the Porter and Duff Over compositing operation. */
float4 alpha_over(float4 background, float4 foreground, float factor, float straight_alpha)
{
  const float4 foreground_color = preprocess_foreground(foreground, straight_alpha);

  const float foreground_alpha = clamp(foreground.w, 0.0f, 1.0f);
  const float4 mix_result = foreground_color + background * (1.0f - foreground_alpha);

  return mix(background, mix_result, factor);
}

/* Computes the Porter and Duff Over compositing operation while assuming the background is being
 * held out by the foreground. See for reference:
 *
 *   https://benmcewan.com/blog/disjoint-over-and-conjoint-over-explained */
float4 alpha_over_disjoint(float4 background,
                           float4 foreground,
                           float factor,
                           float straight_alpha)
{
  const float4 foreground_color = preprocess_foreground(foreground, straight_alpha);

  const float foreground_alpha = clamp(foreground.w, 0.0f, 1.0f);
  const float background_alpha = clamp(background.w, 0.0f, 1.0f);

  if (foreground_alpha + background_alpha < 1.0f) {
    const float4 mix_result = foreground_color + background;
    return mix(background, mix_result, factor);
  }

  const float4 straight_background = safe_divide(background, background_alpha);
  const float4 mix_result = foreground_color + straight_background * (1.0f - foreground_alpha);

  return mix(background, mix_result, factor);
}

/* Computes the Porter and Duff Over compositing operation but the foreground completely covers the
 * background if it is more opaque but not necessary completely opaque. See for reference:
 *
 *   https://benmcewan.com/blog/disjoint-over-and-conjoint-over-explained
 *
 * However, the equation is wrong and should actually be A+B(1-a/b), A if a>b. */
float4 alpha_over_conjoint(float4 background,
                           float4 foreground,
                           float factor,
                           float straight_alpha)
{
  const float4 foreground_color = preprocess_foreground(foreground, straight_alpha);

  const float foreground_alpha = clamp(foreground.w, 0.0f, 1.0f);
  const float background_alpha = clamp(background.w, 0.0f, 1.0f);

  if (foreground_alpha > background_alpha) {
    const float4 mix_result = foreground_color;
    return mix(background, mix_result, factor);
  }

  const float alpha_ratio = safe_divide(foreground_alpha, background_alpha);
  const float4 mix_result = foreground_color + background * (1.0f - alpha_ratio);

  return mix(background, mix_result, factor);
}

void node_composite_alpha_over(float4 background,
                               float4 foreground,
                               float factor,
                               float type,
                               float straight_alpha,
                               out float4 result)
{
  result = background;
  switch (int(type)) {
    case CMP_NODE_ALPHA_OVER_OPERATION_TYPE_OVER:
      result = alpha_over(background, foreground, factor, straight_alpha);
      break;
    case CMP_NODE_ALPHA_OVER_OPERATION_TYPE_DISJOINT_OVER:
      result = alpha_over_disjoint(background, foreground, factor, straight_alpha);
      break;
    case CMP_NODE_ALPHA_OVER_OPERATION_TYPE_CONJOINT_OVER:
      result = alpha_over_conjoint(background, foreground, factor, straight_alpha);
      break;
  }
}

#undef CMP_NODE_ALPHA_OVER_OPERATION_TYPE_OVER
#undef CMP_NODE_ALPHA_OVER_OPERATION_TYPE_DISJOINT_OVER
#undef CMP_NODE_ALPHA_OVER_OPERATION_TYPE_CONJOINT_OVER
