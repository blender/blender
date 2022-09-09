#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

vec4 load_input(ivec2 texel)
{
  vec4 color;
  if (extend_bounds) {
    /* If bounds are extended, then we treat the input as padded by a radius amount of pixels. So
     * we load the input with an offset by the radius amount and fallback to a transparent color if
     * it is out of bounds. */
    color = texture_load(input_tx, texel - radius, vec4(0.0));
  }
  else {
    color = texture_load(input_tx, texel);
  }

  return color;
}

/* Given the texel in the range [-radius, radius] in both axis, load the appropriate weight from
 * the weights texture, where the texel (0, 0) is considered the center of weights texture. */
vec4 load_weight(ivec2 texel)
{
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

  /* Go over the window of the given radius and accumulate the colors multiplied by their
   * respective weights as well as the weights themselves. */
  vec4 accumulated_color = vec4(0.0);
  vec4 accumulated_weight = vec4(0.0);
  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      vec4 weight = load_weight(ivec2(x, y));
      accumulated_color += load_input(texel + ivec2(x, y)) * weight;
      accumulated_weight += weight;
    }
  }

  imageStore(output_img, texel, safe_divide(accumulated_color, accumulated_weight));
}
