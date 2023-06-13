#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

#if defined(KARIS_AVERAGE)
/* Computes the weighted average of the given four colors, which are assumed to the colors of
 * spatially neighbouring pixels. The weights are computed so as to reduce the contributions of
 * fireflies on the result by applying a form of local tone mapping as described by Brian Karis in
 * the article "Graphic Rants: Tone Mapping".
 *
 * https://graphicrants.blogspot.com/2013/12/tone-mapping.html */
vec4 karis_brightness_weighted_sum(vec4 color1, vec4 color2, vec4 color3, vec4 color4)
{
  vec4 brightness = vec4(max_v3(color1), max_v3(color2), max_v3(color3), max_v3(color4));
  vec4 weights = 1.0 / (brightness + 1.0);
  return weighted_sum(color1, color2, color3, color4, weights);
}
#endif

void main()
{
  /* Each invocation corresponds to one output pixel, where the output has half the size of the
   * input. */
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Add 0.5 to evaluate the sampler at the center of the pixel and divide by the image size to get
   * the coordinates into the sampler's expected [0, 1] range. */
  vec2 coordinates = (vec2(texel) + vec2(0.5)) / vec2(imageSize(output_img));

  /* All the offsets in the following code section are in the normalized pixel space of the input
   * texture, so compute its normalized pixel size. */
  vec2 pixel_size = 1.0 / vec2(texture_size(input_tx));

  /* Each invocation downsamples a 6x6 area of pixels around the center of the corresponding output
   * pixel, but instead of sampling each of the 36 pixels in the area, we only sample 13 positions
   * using bilinear fetches at the center of a number of overlapping square 4-pixel groups. This
   * downsampling strategy is described in the talk:
   *
   *   Next Generation Post Processing in Call of Duty: Advanced Warfare
   *   https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
   *
   * In particular, the downsampling strategy is described and illustrated in slide 153 titled
   * "Downsampling - Our Solution". This is employed as it significantly improves the stability of
   * the glare as can be seen in the videos in the talk. */
  vec4 center = texture(input_tx, coordinates);
  vec4 upper_left_near = texture(input_tx, coordinates + pixel_size * vec2(-1.0, 1.0));
  vec4 upper_right_near = texture(input_tx, coordinates + pixel_size * vec2(1.0, 1.0));
  vec4 lower_left_near = texture(input_tx, coordinates + pixel_size * vec2(-1.0, -1.0));
  vec4 lower_right_near = texture(input_tx, coordinates + pixel_size * vec2(1.0, -1.0));
  vec4 left_far = texture(input_tx, coordinates + pixel_size * vec2(-2.0, 0.0));
  vec4 right_far = texture(input_tx, coordinates + pixel_size * vec2(2.0, 0.0));
  vec4 upper_far = texture(input_tx, coordinates + pixel_size * vec2(0.0, 2.0));
  vec4 lower_far = texture(input_tx, coordinates + pixel_size * vec2(0.0, -2.0));
  vec4 upper_left_far = texture(input_tx, coordinates + pixel_size * vec2(-2.0, 2.0));
  vec4 upper_right_far = texture(input_tx, coordinates + pixel_size * vec2(2.0, 2.0));
  vec4 lower_left_far = texture(input_tx, coordinates + pixel_size * vec2(-2.0, -2.0));
  vec4 lower_right_far = texture(input_tx, coordinates + pixel_size * vec2(2.0, -2.0));

#if defined(SIMPLE_AVERAGE)
  /* The original weights equation mentioned in slide 153 is:
   *   0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
   * The 0.5 corresponds to the center group of pixels and the 0.125 corresponds to the other
   * groups of pixels. The center is sampled 4 times, the far non corner pixels are sampled 2
   * times, the near corner pixels are sampled only once; but their weight is quadruple the weights
   * of other groups; so they count as sampled 4 times, finally the far corner pixels are sampled
   * only once, essentially totalling 32 samples. So the weights are as used in the following code
   * section. */
  vec4 result = (4.0 / 32.0) * center +
                (4.0 / 32.0) *
                    (upper_left_near + upper_right_near + lower_left_near + lower_right_near) +
                (2.0 / 32.0) * (left_far + right_far + upper_far + lower_far) +
                (1.0 / 32.0) *
                    (upper_left_far + upper_right_far + lower_left_far + lower_right_far);
#elif defined(KARIS_AVERAGE)
  /* Reduce the contributions of fireflies on the result by reducing each group of pixels using a
   * Karis brightness weighted sum. This is described in slide 168 titled "Fireflies - Partial
   * Karis Average".
   *
   * This needn't be done on all downsampling passes, but only the first one, since fireflies
   * will not survive the first pass, later passes can use the weighted average. */
  vec4 center_weighted_sum = karis_brightness_weighted_sum(
      upper_left_near, upper_right_near, lower_right_near, lower_left_near);
  vec4 upper_left_weighted_sum = karis_brightness_weighted_sum(
      upper_left_far, upper_far, center, left_far);
  vec4 upper_right_weighted_sum = karis_brightness_weighted_sum(
      upper_far, upper_right_far, right_far, center);
  vec4 lower_right_weighted_sum = karis_brightness_weighted_sum(
      center, right_far, lower_right_far, lower_far);
  vec4 lower_left_weighted_sum = karis_brightness_weighted_sum(
      left_far, center, lower_far, lower_left_far);

  /* The original weights equation mentioned in slide 153 is:
   *   0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
   * Multiply both sides by 8 and you get:
   *   4 + 1 + 1 + 1 + 1 = 8
   * So the weights are as used in the following code section. */
  vec4 result = (4.0 / 8.0) * center_weighted_sum +
                (1.0 / 8.0) * (upper_left_weighted_sum + upper_right_weighted_sum +
                               lower_left_weighted_sum + lower_right_weighted_sum);
#endif

  imageStore(output_img, texel, result);
}
