/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_composite_alpha_over_key(float factor,
                                   float4 color,
                                   float4 over_color,
                                   out float4 result)
{
  if (over_color.a <= 0.0f) {
    result = color;
  }
  else if (factor == 1.0f && over_color.a >= 1.0f) {
    result = over_color;
  }
  else {
    result = mix(color, float4(over_color.rgb, 1.0f), factor * over_color.a);
  }
}

void node_composite_alpha_over_premultiply(float factor,
                                           float4 color,
                                           float4 over_color,
                                           out float4 result)
{
  if (over_color.a < 0.0f) {
    result = color;
  }
  else if (factor == 1.0f && over_color.a >= 1.0f) {
    result = over_color;
  }
  else {
    float multiplier = 1.0f - factor * over_color.a;

    result = multiplier * color + factor * over_color;
  }
}
