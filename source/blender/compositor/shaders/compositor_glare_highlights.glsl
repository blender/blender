/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

/* A Quadratic Polynomial smooth minimum function *without* normalization, based on:
 *
 *   https://iquilezles.org/articles/smin/
 *
 * This should not be converted into a common utility function because the glare code is
 * specifically designed for it as can be seen in the adaptive_smooth_clamp method, and it is
 * intentionally not normalized. */
float smooth_min(float a, float b, float smoothness)
{
  if (smoothness == 0.0) {
    return min(a, b);
  }
  float h = max(smoothness - abs(a - b), 0.0) / smoothness;
  return min(a, b) - h * h * smoothness * (1.0 / 4.0);
}

float smooth_max(float a, float b, float smoothness)
{
  return -smooth_min(-a, -b, smoothness);
}

/* Clamps the input x within min_value and max_value using a quadratic polynomial smooth minimum
 * and maximum functions, with individual control over their smoothness. */
float smooth_clamp(
    float x, float min_value, float max_value, float min_smoothness, float max_smoothness)
{
  return smooth_min(max_value, smooth_max(min_value, x, min_smoothness), max_smoothness);
}

/* A variant of smooth_clamp that limits the smoothness such that the function evaluates to the
 * given min for 0 <= min <= max and x >= 0. The aforementioned guarantee holds for the standard
 * clamp function by definition, but since the smooth clamp function gradually increases before
 * the specified min/max, if min/max are sufficiently close together or to zero, they will not
 * evaluate to min at zero or at min, since zero or min will be at the region of the gradual
 * increase.
 *
 * It can be shown that the width of the gradual increase region is equivalent to the smoothness
 * parameter, so smoothness can't be larger than the difference between the min/max and zero, or
 * larger than the difference between min and max themselves. Otherwise, zero or min will lie
 * inside the gradual increase region of min/max. So we limit the smoothness of min/max by taking
 * the minimum with the distances to zero and to the distance to the other bound. */
float adaptive_smooth_clamp(float x, float min_value, float max_value, float smoothness)
{
  float range_distance = distance(min_value, max_value);
  float distance_from_min_to_zero = distance(min_value, 0.0);
  float distance_from_max_to_zero = distance(max_value, 0.0);

  float max_safe_smoothness_for_min = min(distance_from_min_to_zero, range_distance);
  float max_safe_smoothness_for_max = min(distance_from_max_to_zero, range_distance);

  float min_smoothness = min(smoothness, max_safe_smoothness_for_min);
  float max_smoothness = min(smoothness, max_safe_smoothness_for_max);

  return smooth_clamp(x, min_value, max_value, min_smoothness, max_smoothness);
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec2 normalized_coordinates = (vec2(texel) + vec2(0.5)) / vec2(imageSize(output_img));

  vec4 hsva;
  rgb_to_hsv(texture(input_tx, normalized_coordinates), hsva);

  /* Clamp the brightness of the highlights such that pixels whose brightness are less than the
   * threshold will be equal to the threshold and will become zero once threshold is subtracted
   * later. We also clamp by the specified max brightness to suppress very bright highlights.
   *
   * We use a smooth clamping function such that highlights do not become very sharp but use
   * the adaptive variant such that we guarantee that zero highlights remain zero even after
   * smoothing. Notice that when we mention zero, we mean zero after subtracting the threshold,
   * so we actually mean the minimum bound, the threshold. See the adaptive_smooth_clamp
   * function for more information. */
  float clamped_brightness = adaptive_smooth_clamp(
      hsva.z, threshold, max_brightness, highlights_smoothness);

  /* The final brightness is relative to the threshold. */
  hsva.z = clamped_brightness - threshold;

  vec4 rgba;
  hsv_to_rgb(hsva, rgba);

  imageStore(output_img, texel, vec4(rgba.rgb, 1.0));
}
