#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  /* The dispatch domain covers the output image size, which might be a fraction of the input image
   * size, so you will notice the output image size used throughout the shader instead of the input
   * one. */
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Since the output image might be a fraction of the input image size, and since we want to
   * evaluate the input sampler at the center of the output pixel, we add an offset equal to half
   * the number of input pixels that covers a single output pixel. In case the input and output
   * have the same size, this will be 0.5, which is the offset required to evaluate the sampler at
   * the center of the pixel. */
  vec2 offset = vec2(texture_size(input_tx) / imageSize(output_img)) / 2.0;

  /* Add the aforementioned offset and divide by the output image size to get the coordinates into
   * the sampler's expected [0, 1] range. */
  vec2 normalized_coordinates = (vec2(texel) + offset) / vec2(imageSize(output_img));

  vec4 input_color = texture(input_tx, normalized_coordinates);
  float luminance = dot(input_color.rgb, luminance_coefficients);

  /* The pixel whose luminance is less than the threshold luminance is not considered part of the
   * highlights and is given a value of zero. Otherwise, the pixel is considered part of the
   * highlights, whose value is the difference to the threshold value clamped to zero. */
  bool is_highlights = luminance >= threshold;
  vec3 highlights = is_highlights ? max(vec3(0.0), input_color.rgb - threshold) : vec3(0.0);

  imageStore(output_img, texel, vec4(highlights, 1.0));
}
