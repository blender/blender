/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Computes the Porter and Duff Over compositing operation. If straight_alpha is true, then the
 * foreground is in straight alpha form and would need to be premultiplied. */
void node_composite_alpha_over(
    float factor, float4 background, float4 foreground, float straight_alpha, out float4 result)
{
  /* Premultiply the alpha of the foreground if it is straight. */
  const float alpha = clamp(foreground.w, 0.0f, 1.0f);
  const float4 premultiplied_foreground = float4(foreground.xyz * alpha, alpha);
  const float4 foreground_color = straight_alpha != 0.0f ? premultiplied_foreground : foreground;

  const float4 mix_result = background * (1.0f - alpha) + foreground_color;
  result = mix(background, mix_result, factor);
}
