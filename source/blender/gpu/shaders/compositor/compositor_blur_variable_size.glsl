#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

/* Given the texel in the range [-radius, radius] in both axis, load the appropriate weight from
 * the weights texture, where the texel (0, 0) is considered the center of weights texture. */
vec4 load_weight(ivec2 texel, float radius)
{
  /* The center zero texel is always assigned a unit weight regardless of the corresponding weight
   * in the weights texture. That's to guarantee that at last the center pixel will be accumulated
   * even if the weights texture is zero at its center. */
  if (texel == ivec2(0)) {
    return vec4(1.0);
  }

  /* Add the radius to transform the texel into the range [0, radius * 2], then divide by the upper
   * bound plus one to transform the texel into the normalized range [0, 1] needed to sample the
   * weights sampler. Finally, also add 0.5 to sample at the center of the pixels. */
  return texture(weights_tx, (texel + vec2(radius + 0.5)) / (radius * 2 + 1));
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* The mask input is treated as a boolean. If it is zero, then no blurring happens for this
   * pixel. Otherwise, the pixel is blurred normally and the mask value is irrelevant. */
  float mask = texture_load(mask_tx, texel).x;
  if (mask == 0.0) {
    imageStore(output_img, texel, texture_load(input_tx, texel));
    return;
  }

  float center_size = texture_load(size_tx, texel).x * base_size;

  /* Go over the window of the given search radius and accumulate the colors multiplied by their
   * respective weights as well as the weights themselves, but only if both the size of the center
   * pixel and the size of the candidate pixel are less than both the x and y distances of the
   * candidate pixel. */
  vec4 accumulated_color = vec4(0.0);
  vec4 accumulated_weight = vec4(0.0);
  for (int y = -search_radius; y <= search_radius; y++) {
    for (int x = -search_radius; x <= search_radius; x++) {
      float candidate_size = texture_load(size_tx, texel + ivec2(x, y)).x * base_size;

      /* Skip accumulation if either the x or y distances of the candidate pixel are larger than
       * either the center or candidate pixel size. Note that the max and min functions here denote
       * "either" in the aforementioned description. */
      float size = min(center_size, candidate_size);
      if (max(abs(x), abs(y)) > size) {
        continue;
      }

      vec4 weight = load_weight(ivec2(x, y), size);
      accumulated_color += texture_load(input_tx, texel + ivec2(x, y)) * weight;
      accumulated_weight += weight;
    }
  }

  imageStore(output_img, texel, safe_divide(accumulated_color, accumulated_weight));
}
