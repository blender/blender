/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

/* Returns true if the given color is close enough to the given reference color within the
 * threshold supplied by the user, and returns false otherwise. */
bool is_close(vec4 reference_color, vec4 color)
{
  return all(lessThan(abs(reference_color - color).rgb, vec3(threshold)));
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* A 3x3 weights kernel whose weights are the inverse of the distance to the center of the
   * kernel. So the center weight is zero, the corners weights are (1 / sqrt(2)), and the rest
   * of the weights are 1. The total sum of weights is 4 plus quadruple the corner weight. */
  float corner_weight = 1.0 / sqrt(2.0);
  float sum_of_weights = 4.0 + corner_weight * 4.0;
  mat3 weights = mat3(vec3(corner_weight, 1.0, corner_weight),
                      vec3(1.0, 0.0, 1.0),
                      vec3(corner_weight, 1.0, corner_weight));

  vec4 center_color = texture_load(input_tx, texel);

  /* Go over the pixels in the 3x3 window around the center pixel and compute the total sum of
   * their colors multiplied by their weights. Additionally, for pixels whose colors are not close
   * enough to the color of the center pixel, accumulate their color as well as their weights. */
  vec4 sum_of_colors = vec4(0);
  float accumulated_weight = 0.0;
  vec4 accumulated_color = vec4(0);
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      float weight = weights[j][i];
      vec4 color = texture_load(input_tx, texel + ivec2(i - 1, j - 1)) * weight;
      sum_of_colors += color;
      if (!is_close(center_color, color)) {
        accumulated_color += color;
        accumulated_weight += weight;
      }
    }
  }

  /* If the accumulated weight is zero, that means all pixels in the 3x3 window are similar and no
   * need to despeckle anything, so write the original center color and return. */
  if (accumulated_weight == 0.0) {
    imageStore(output_img, texel, center_color);
    return;
  }

  /* If the ratio between the accumulated weights and the total sum of weights is not larger than
   * the user specified neighbor threshold, then the number of pixels in the neighborhood that are
   * not close enough to the center pixel is low, and no need to despeckle anything, so write the
   * original center color and return. */
  if (accumulated_weight / sum_of_weights < neighbor_threshold) {
    imageStore(output_img, texel, center_color);
    return;
  }

  /* If the weighted average color of the neighborhood is close enough to the center pixel, then no
   * need to despeckle anything, so write the original center color and return. */
  if (is_close(center_color, sum_of_colors / sum_of_weights)) {
    imageStore(output_img, texel, center_color);
    return;
  }

  /* We need to despeckle, so write the mean accumulated color. */
  float factor = texture_load(factor_tx, texel).x;
  vec4 mean_color = accumulated_color / accumulated_weight;
  imageStore(output_img, texel, mix(center_color, mean_color, factor));
}
