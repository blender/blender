/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_composite_alpha_over_mixed(
    float factor, vec4 color, vec4 over_color, float premultiply_factor, out vec4 result)
{
  if (over_color.a <= 0.0) {
    result = color;
  }
  else if (factor == 1.0 && over_color.a >= 1.0) {
    result = over_color;
  }
  else {
    float add_factor = 1.0 - premultiply_factor + over_color.a * premultiply_factor;
    float premultiplier = factor * add_factor;
    float multiplier = 1.0 - factor * over_color.a;

    result = multiplier * color + vec2(premultiplier, factor).xxxy * over_color;
  }
}

void node_composite_alpha_over_key(float factor, vec4 color, vec4 over_color, out vec4 result)
{
  if (over_color.a <= 0.0) {
    result = color;
  }
  else if (factor == 1.0 && over_color.a >= 1.0) {
    result = over_color;
  }
  else {
    result = mix(color, vec4(over_color.rgb, 1.0), factor * over_color.a);
  }
}

void node_composite_alpha_over_premultiply(float factor,
                                           vec4 color,
                                           vec4 over_color,
                                           out vec4 result)
{
  if (over_color.a < 0.0) {
    result = color;
  }
  else if (factor == 1.0 && over_color.a >= 1.0) {
    result = over_color;
  }
  else {
    float multiplier = 1.0 - factor * over_color.a;

    result = multiplier * color + factor * over_color;
  }
}
