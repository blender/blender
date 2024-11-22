/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_base_lib.glsl"

/* Preprocess the input of the blur filter by squaring it in its alpha straight form, assuming the
 * given color is alpha pre-multiplied. */
vec4 gamma_correct_blur_input(vec4 color)
{
  float alpha = color.a > 0.0 ? color.a : 1.0;
  vec3 corrected_color = square(max(color.rgb / alpha, vec3(0.0))) * alpha;
  return vec4(corrected_color, color.a);
}

/* Postprocess the output of the blur filter by taking its square root it in its alpha straight
 * form, assuming the given color is alpha pre-multiplied. This essential undoes the processing
 * done by the gamma_correct_blur_input function. */
vec4 gamma_uncorrect_blur_output(vec4 color)
{
  float alpha = color.a > 0.0 ? color.a : 1.0;
  vec3 uncorrected_color = sqrt(max(color.rgb / alpha, vec3(0.0))) * alpha;
  return vec4(uncorrected_color, color.a);
}
