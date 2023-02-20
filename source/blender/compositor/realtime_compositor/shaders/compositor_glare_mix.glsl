#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the input image
   * size to get the relevant coordinates into the sampler's expected [0, 1] range. Make sure the
   * input color is not negative to avoid a subtractive effect when mixing the glare. */
  vec2 normalized_coordinates = (vec2(texel) + vec2(0.5)) / vec2(texture_size(input_tx));
  vec4 glare_color = texture(glare_tx, normalized_coordinates);
  vec4 input_color = max(vec4(0.0), texture_load(input_tx, texel));

  /* The mix factor is in the range [-1, 1] and linearly interpolate between the three values such
   * that:
   *   1 => Glare only.
   *   0 => Input + Glare.
   *  -1 => Input only.
   * We implement that as a weighted sum as follows. When the mix factor is 1, the glare weight
   * should be 1 and the input weight should be 0. When the mix factor is -1, the glare weight
   * should be 0 and the input weight should be 1. When the mix factor is 0, both weights should
   * be 1. This can be expressed using the following compact min max expressions. */
  float input_weight = 1.0 - max(0.0, mix_factor);
  float glare_weight = 1.0 + min(0.0, mix_factor);
  vec3 highlights = input_weight * input_color.rgb + glare_weight * glare_color.rgb;

  imageStore(output_img, texel, vec4(highlights, input_color.a));
}
