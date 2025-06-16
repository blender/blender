/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

#define CMP_NODE_GLARE_QUALITY_HIGH 0
#define CMP_NODE_GLARE_QUALITY_MEDIUM 1
#define CMP_NODE_GLARE_QUALITY_LOW 2

/* A Quadratic Polynomial smooth minimum function *without* normalization, based on:
 *
 *   https://iquilezles.org/articles/smin/
 *
 * This should not be converted into a common utility function because the glare code is
 * specifically designed for it as can be seen in the adaptive_smooth_clamp method, and it is
 * intentionally not normalized. */
float smooth_min(float a, float b, float smoothness)
{
  if (smoothness == 0.0f) {
    return min(a, b);
  }
  float h = max(smoothness - abs(a - b), 0.0f) / smoothness;
  return min(a, b) - h * h * smoothness * (1.0f / 4.0f);
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
  float distance_from_min_to_zero = distance(min_value, 0.0f);
  float distance_from_max_to_zero = distance(max_value, 0.0f);

  float max_safe_smoothness_for_min = min(distance_from_min_to_zero, range_distance);
  float max_safe_smoothness_for_max = min(distance_from_max_to_zero, range_distance);

  float min_smoothness = min(smoothness, max_safe_smoothness_for_min);
  float max_smoothness = min(smoothness, max_safe_smoothness_for_max);

  return smooth_clamp(x, min_value, max_value, min_smoothness, max_smoothness);
}

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float4 color = float4(0.0f);

  switch (quality) {
    case CMP_NODE_GLARE_QUALITY_HIGH: {
      color = texture_load(input_tx, texel);
      break;
    }

    /* Down-sample the image 2 times to match the output size by averaging the 2x2 block of
     * pixels into a single output pixel. This is done due to the bilinear interpolation at the
     * center of the 2x2 block of pixels. */
    case CMP_NODE_GLARE_QUALITY_MEDIUM: {
      float2 normalized_coordinates = (float2(texel) * 2.0f + float2(1.0f)) /
                                      float2(texture_size(input_tx));
      color = texture(input_tx, normalized_coordinates);
      break;
    }

    /* Down-sample the image 4 times to match the output size by averaging each 4x4 block of
     * pixels into a single output pixel. This is done by averaging 4 bilinear taps at the
     * center of each of the corner 2x2 pixel blocks, which are themselves the average of the
     * 2x2 block due to the bilinear interpolation at the center. */
    case CMP_NODE_GLARE_QUALITY_LOW: {
      float2 lower_left_coordinates = (float2(texel) * 4.0f + float2(1.0f)) /
                                      float2(texture_size(input_tx));
      float4 lower_left_color = texture(input_tx, lower_left_coordinates);

      float2 lower_right_coordinates = (float2(texel) * 4.0f + float2(3.0f, 1.0f)) /
                                       float2(texture_size(input_tx));
      float4 lower_right_color = texture(input_tx, lower_right_coordinates);

      float2 upper_left_coordinates = (float2(texel) * 4.0f + float2(1.0f, 3.0f)) /
                                      float2(texture_size(input_tx));
      float4 upper_left_color = texture(input_tx, upper_left_coordinates);

      float2 upper_right_coordinates = (float2(texel) * 4.0f + float2(3.0f)) /
                                       float2(texture_size(input_tx));
      float4 upper_right_color = texture(input_tx, upper_right_coordinates);

      color = (upper_left_color + upper_right_color + lower_left_color + lower_right_color) / 4.0f;
      break;
    }
  }

  float4 hsva;
  rgb_to_hsv(color, hsva);

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

  float4 rgba;
  hsv_to_rgb(hsva, rgba);

  imageStore(output_img, texel, float4(rgba.rgb, 1.0f));
}
