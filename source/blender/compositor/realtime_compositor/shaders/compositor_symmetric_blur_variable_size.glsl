/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_blur_common.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

/* Loads the input color of the pixel at the given texel. If gamma correction is enabled, the color
 * is gamma corrected. If bounds are extended, then the input is treated as padded by a blur size
 * amount of pixels of zero color, and the given texel is assumed to be in the space of the image
 * after padding. So we offset the texel by the blur radius amount and fallback to a zero color if
 * it is out of bounds. For instance, if the input is padded by 5 pixels to the left of the image,
 * the first 5 pixels should be out of bounds and thus zero, hence the introduced offset. */
vec4 load_input(ivec2 texel)
{
  vec4 color;
  if (extend_bounds) {
    /* Notice that we subtract 1 because the weights texture have an extra center weight, see the
     * SymmetricBlurWeights class for more information. */
    ivec2 blur_radius = texture_size(weights_tx) - 1;
    color = texture_load(input_tx, texel - blur_radius, vec4(0.0));
  }
  else {
    color = texture_load(input_tx, texel);
  }

  if (gamma_correct) {
    color = gamma_correct_blur_input(color);
  }

  return color;
}

/* Similar to load_input but loads the size instead, has no gamma correction, and clamps to borders
 * instead of returning zero for out of bound access. See load_input for more information. */
float load_size(ivec2 texel)
{
  if (extend_bounds) {
    ivec2 blur_radius = texture_size(weights_tx) - 1;
    return clamp(texture_load(size_tx, texel - blur_radius).x, 0.0, 1.0);
  }
  else {
    return clamp(texture_load(size_tx, texel).x, 0.0, 1.0);
  }
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 accumulated_color = vec4(0.0);
  vec4 accumulated_weight = vec4(0.0);

  /* The weights texture only stores the weights for the first quadrant, but since the weights are
   * symmetric, other quadrants can be found using mirroring. It follows that the base blur radius
   * is the weights texture size minus one, where the one corresponds to the zero weight. */
  ivec2 weights_size = texture_size(weights_tx);
  ivec2 base_radius = weights_size - ivec2(1);
  ivec2 radius = ivec2(ceil(base_radius * load_size(texel)));
  vec2 coordinates_scale = vec2(1.0) / (radius + ivec2(1));

  /* First, compute the contribution of the center pixel. */
  vec4 center_color = load_input(texel);
  float center_weight = texture_load(weights_tx, ivec2(0)).x;
  accumulated_color += center_color * center_weight;
  accumulated_weight += center_weight;

  /* Then, compute the contributions of the pixels along the x axis of the filter, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int x = 1; x <= radius.x; x++) {
    float weight_coordinates = (x + 0.5) * coordinates_scale.x;
    float weight = texture(weights_tx, vec2(weight_coordinates, 0.0)).x;
    accumulated_color += load_input(texel + ivec2(x, 0)) * weight;
    accumulated_color += load_input(texel + ivec2(-x, 0)) * weight;
    accumulated_weight += weight * 2.0;
  }

  /* Then, compute the contributions of the pixels along the y axis of the filter, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int y = 1; y <= radius.y; y++) {
    float weight_coordinates = (y + 0.5) * coordinates_scale.y;
    float weight = texture(weights_tx, vec2(0.0, weight_coordinates)).x;
    accumulated_color += load_input(texel + ivec2(0, y)) * weight;
    accumulated_color += load_input(texel + ivec2(0, -y)) * weight;
    accumulated_weight += weight * 2.0;
  }

  /* Finally, compute the contributions of the pixels in the four quadrants of the filter, noting
   * that the weights texture only stores the weights for the upper right quadrant, but since the
   * filter is symmetric, the same weight is used for the rest of the quadrants and we add all four
   * of their contributions. */
  for (int y = 1; y <= radius.y; y++) {
    for (int x = 1; x <= radius.x; x++) {
      vec2 weight_coordinates = (vec2(x, y) + vec2(0.5)) * coordinates_scale;
      float weight = texture(weights_tx, weight_coordinates).x;
      accumulated_color += load_input(texel + ivec2(x, y)) * weight;
      accumulated_color += load_input(texel + ivec2(-x, y)) * weight;
      accumulated_color += load_input(texel + ivec2(x, -y)) * weight;
      accumulated_color += load_input(texel + ivec2(-x, -y)) * weight;
      accumulated_weight += weight * 4.0;
    }
  }

  accumulated_color = safe_divide(accumulated_color, accumulated_weight);

  if (gamma_correct) {
    accumulated_color = gamma_uncorrect_blur_output(accumulated_color);
  }

  imageStore(output_img, texel, accumulated_color);
}
