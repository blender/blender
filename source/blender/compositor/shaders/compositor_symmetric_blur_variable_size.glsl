/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* Loads the input color of the pixel at the given texel. If bounds are extended, then the input is
 * treated as padded by a blur size amount of pixels of zero color, and the given texel is assumed
 * to be in the space of the image after padding. So we offset the texel by the blur radius amount
 * and fallback to a zero color if it is out of bounds. For instance, if the input is padded by 5
 * pixels to the left of the image, the first 5 pixels should be out of bounds and thus zero, hence
 * the introduced offset. */
float4 load_input(int2 texel)
{
  float4 color;
  if (extend_bounds) {
    /* Notice that we subtract 1 because the weights texture have an extra center weight, see the
     * SymmetricBlurWeights class for more information. */
    int2 blur_radius = texture_size(weights_tx) - 1;
    color = texture_load(input_tx, texel - blur_radius, float4(0.0f));
  }
  else {
    color = texture_load(input_tx, texel);
  }

  return color;
}

/* Similar to load_input but loads the size instead and clamps to borders instead of returning zero
 * for out of bound access. See load_input for more information. */
float load_size(int2 texel)
{
  int2 blur_radius = texture_size(weights_tx) - 1;
  int2 offset = extend_bounds ? blur_radius : int2(0);
  return clamp(texture_load(size_tx, texel - offset).x, 0.0f, 1.0f);
}

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float4 accumulated_color = float4(0.0f);
  float4 accumulated_weight = float4(0.0f);

  /* The weights texture only stores the weights for the first quadrant, but since the weights are
   * symmetric, other quadrants can be found using mirroring. It follows that the base blur radius
   * is the weights texture size minus one, where the one corresponds to the zero weight. */
  int2 weights_size = texture_size(weights_tx);
  int2 base_radius = weights_size - int2(1);
  int2 radius = int2(ceil(float2(base_radius) * load_size(texel)));
  float2 coordinates_scale = float2(1.0f) / float2(radius + int2(1));

  /* First, compute the contribution of the center pixel. */
  float4 center_color = load_input(texel);
  float center_weight = texture_load(weights_tx, int2(0)).x;
  accumulated_color += center_color * center_weight;
  accumulated_weight += center_weight;

  /* Then, compute the contributions of the pixels along the x axis of the filter, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int x = 1; x <= radius.x; x++) {
    float weight_coordinates = (x + 0.5f) * coordinates_scale.x;
    float weight = texture(weights_tx, float2(weight_coordinates, 0.0f)).x;
    accumulated_color += load_input(texel + int2(x, 0)) * weight;
    accumulated_color += load_input(texel + int2(-x, 0)) * weight;
    accumulated_weight += weight * 2.0f;
  }

  /* Then, compute the contributions of the pixels along the y axis of the filter, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int y = 1; y <= radius.y; y++) {
    float weight_coordinates = (y + 0.5f) * coordinates_scale.y;
    float weight = texture(weights_tx, float2(0.0f, weight_coordinates)).x;
    accumulated_color += load_input(texel + int2(0, y)) * weight;
    accumulated_color += load_input(texel + int2(0, -y)) * weight;
    accumulated_weight += weight * 2.0f;
  }

  /* Finally, compute the contributions of the pixels in the four quadrants of the filter, noting
   * that the weights texture only stores the weights for the upper right quadrant, but since the
   * filter is symmetric, the same weight is used for the rest of the quadrants and we add all four
   * of their contributions. */
  for (int y = 1; y <= radius.y; y++) {
    for (int x = 1; x <= radius.x; x++) {
      float2 weight_coordinates = (float2(x, y) + float2(0.5f)) * coordinates_scale;
      float weight = texture(weights_tx, weight_coordinates).x;
      accumulated_color += load_input(texel + int2(x, y)) * weight;
      accumulated_color += load_input(texel + int2(-x, y)) * weight;
      accumulated_color += load_input(texel + int2(x, -y)) * weight;
      accumulated_color += load_input(texel + int2(-x, -y)) * weight;
      accumulated_weight += weight * 4.0f;
    }
  }

  accumulated_color = safe_divide(accumulated_color, accumulated_weight);

  imageStore(output_img, texel, accumulated_color);
}
