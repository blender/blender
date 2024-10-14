/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The algorithm is by Werner D. Streidt, extracted of OpenCV demhist.c:
 *   http://visca.com/ffactory/archives/5-99/msg00021.html */

#include "gpu_shader_common_color_utils.glsl"

#define FLT_EPSILON 1.192092896e-07F

void node_composite_bright_contrast(
    vec4 color, float brightness, float contrast, const float use_premultiply, out vec4 result)
{
  float scaled_brightness = brightness / 100.0;
  float delta = contrast / 200.0;

  float multiplier, offset;
  if (contrast > 0.0) {
    multiplier = 1.0 - delta * 2.0;
    multiplier = 1.0 / max(multiplier, FLT_EPSILON);
    offset = multiplier * (scaled_brightness - delta);
  }
  else {
    delta *= -1.0;
    multiplier = max(1.0 - delta * 2.0, 0.0);
    offset = multiplier * scaled_brightness + delta;
  }

  vec4 input_color = color;
  if (use_premultiply != 0.0) {
    color_alpha_unpremultiply(input_color, input_color);
  }

  result = vec4(input_color.rgb * multiplier + offset, input_color.a);

  if (use_premultiply != 0.0) {
    color_alpha_premultiply(result, result);
  }
}
