/* The Morphological Distance Feather operation is a linear combination between the result of two
 * operations. The first operation is a Gaussian blur with a radius equivalent to the dilate/erode
 * distance, which is straightforward and implemented as a separable filter similar to the blur
 * operation.
 *
 * The second operation is an approximation of a morphological inverse distance operation evaluated
 * at a distance falloff function. The result of a morphological inverse distance operation is a
 * narrow band distance field that starts at its maximum value at boundaries where a difference in
 * values took place and linearly deceases until it reaches zero in the span of a number of pixels
 * equivalent to the erode/dilate distance. Additionally, instead of linearly decreasing, the user
 * may choose a different falloff which is evaluated at the computed distance. For dilation, the
 * distance field decreases outwards, and for erosion, the distance field decreased inwards.
 *
 * The reason why the result of a Gaussian blur is mixed in with the distance field is because the
 * distance field is merely approximated and not accurately computed, the defects of which is more
 * apparent away from boundaries and especially at corners where the distance field should take a
 * circular shape. That's why the Gaussian blur is mostly mixed only further from boundaries.
 *
 * The morphological inverse distance operation is approximated using a separable implementation
 * and intertwined with the Gaussian blur implementation as follows. A search window of a radius
 * equivalent to the dilate/erode distance is applied on the image to find either the minimum or
 * maximum pixel value multiplied by its corresponding falloff value in the window. For dilation,
 * we try to find the maximum, and for erosion, we try to find the minimum. Additionally, we also
 * save the falloff value where the minimum or maximum was found. The found value will be that of
 * the narrow band distance field and the saved falloff value will be used as the mixing factor
 * with the Gaussian blur.
 *
 * To make sense of the aforementioned algorithm, assume we are dilating a binary image by 5 pixels
 * whose half has a value of 1 and the other half has a value of zero. Consider the following:
 *
 * - A pixel of value 1 already has the maximum possible value, so its value will remain unchanged
 *   regardless of its position.
 * - A pixel of value 0 that is right at the boundary of the 1's region will have a maximum value
 *   of around 0.8 depending on the falloff. That's because the search window intersects the 1's
 *   region, which when multiplied by the falloff gives the first value of the falloff, which is
 *   larger than the initially zero value computed at the center of the search window.
 * - A pixel of value 0 that is 3 pixels away from the boundary will have a maximum value of around
 *   0.4 depending on the falloff. That's because the search window intersects the 1's region,
 *   which when multiplied by the falloff gives the third value of the falloff, which is larger
 *   than the initially zero value computed at the center of the search window.
 * - Finally, a pixel of value 0 that is 6 pixels away from the boundary will have a maximum value
 *   of 0, because the search window doesn't intersects the 1's region and only spans zero values.
 *
 * The previous example demonstrates how the distance field naturally arises, and the same goes for
 * the erode case, except the minimum value is computed instead.
 */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* A value for accumulating the blur result. */
  float accumulated_value = 0.0;

  /* Compute the contribution of the center pixel to the blur result. */
  float center_value = texture_load(input_tx, texel).x;
  accumulated_value += center_value * texture_load(weights_tx, 0).x;

  /* Start with the center value as the maximum/minimum distance and reassign to the true maximum
   * or minimum in the search loop below. Additionally, the center falloff is always 1.0, so start
   * with that. */
  float limit_distance = center_value;
  float limit_distance_falloff = 1.0;

  /* Compute the contributions of the pixels to the right and left, noting that the weights and
   * falloffs textures only store the weights and falloffs for the positive half, but since the
   * they are both symmetric, the same weights and falloffs are used for the negative half and we
   * compute both of their contributions. */
  for (int i = 1; i < texture_size(weights_tx); i++) {
    float weight = texture_load(weights_tx, i).x;
    float falloff = texture_load(falloffs_tx, i).x;

    /* Loop for two iterations, where s takes the value of -1 and 1, which is used as the sign
     * needed to evaluated the positive and negative sides as explain above. */
    for (int s = -1; s < 2; s += 2) {
      /* Compute the contribution of the pixel to the blur result. */
      float value = texture_load(input_tx, texel + ivec2(s * i, 0)).x;
      accumulated_value += value * weight;

      /* The distance is computed such that its highest value is the pixel value itself, so
       * multiply the distance falloff by the pixel value. */
      float falloff_distance = value * falloff;

      /* Find either the maximum or the minimum for the dilate and erode cases respectively. */
      if (COMPARE(falloff_distance, limit_distance)) {
        limit_distance = falloff_distance;
        limit_distance_falloff = falloff;
      }
    }
  }

  /* Mix between the limit distance and the blurred accumulated value such that the limit distance
   * is used for pixels closer to the boundary and the blurred value is used for pixels away from
   * the boundary. */
  float value = mix(accumulated_value, limit_distance, limit_distance_falloff);

  /* Write the value using the transposed texel. See the execute_distance_feather_horizontal_pass
   * method for more information on the rational behind this. */
  imageStore(output_img, texel.yx, vec4(value));
}
