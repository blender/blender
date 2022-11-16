#pragma BLENDER_REQUIRE(gpu_shader_compositor_blur_common.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

vec4 load_input(ivec2 texel)
{
  vec4 color;
  if (extend_bounds) {
    /* If bounds are extended, then we treat the input as padded by a radius amount of pixels. So
     * we load the input with an offset by the radius amount and fallback to a transparent color if
     * it is out of bounds. Notice that we subtract 1 because the weights texture have an extra
     * center weight, see the SymmetricBlurWeights for more information. */
    ivec2 blur_size = texture_size(weights_tx) - 1;
    color = texture_load(input_tx, texel - blur_size, vec4(0.0));
  }
  else {
    color = texture_load(input_tx, texel);
  }

  if (gamma_correct) {
    color = gamma_correct_blur_input(color);
  }

  return color;
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 accumulated_color = vec4(0.0);

  /* First, compute the contribution of the center pixel. */
  vec4 center_color = load_input(texel);
  accumulated_color += center_color * texture_load(weights_tx, ivec2(0)).x;

  ivec2 weights_size = texture_size(weights_tx);

  /* Then, compute the contributions of the pixels along the x axis of the filter, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int x = 1; x < weights_size.x; x++) {
    float weight = texture_load(weights_tx, ivec2(x, 0)).x;
    accumulated_color += load_input(texel + ivec2(x, 0)) * weight;
    accumulated_color += load_input(texel + ivec2(-x, 0)) * weight;
  }

  /* Then, compute the contributions of the pixels along the y axis of the filter, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int y = 1; y < weights_size.y; y++) {
    float weight = texture_load(weights_tx, ivec2(0, y)).x;
    accumulated_color += load_input(texel + ivec2(0, y)) * weight;
    accumulated_color += load_input(texel + ivec2(0, -y)) * weight;
  }

  /* Finally, compute the contributions of the pixels in the four quadrants of the filter, noting
   * that the weights texture only stores the weights for the upper right quadrant, but since the
   * filter is symmetric, the same weight is used for the rest of the quadrants and we add all four
   * of their contributions. */
  for (int y = 1; y < weights_size.y; y++) {
    for (int x = 1; x < weights_size.x; x++) {
      float weight = texture_load(weights_tx, ivec2(x, y)).x;
      accumulated_color += load_input(texel + ivec2(x, y)) * weight;
      accumulated_color += load_input(texel + ivec2(-x, y)) * weight;
      accumulated_color += load_input(texel + ivec2(x, -y)) * weight;
      accumulated_color += load_input(texel + ivec2(-x, -y)) * weight;
    }
  }

  if (gamma_correct) {
    accumulated_color = gamma_uncorrect_blur_output(accumulated_color);
  }

  imageStore(output_img, texel, accumulated_color);
}
