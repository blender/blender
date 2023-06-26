#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

float compute_saturation(vec4 color, ivec3 argmax)
{
  float weighted_average = mix(color[argmax.y], color[argmax.z], key_balance);
  return (color[argmax.x] - weighted_average) * abs(1.0 - weighted_average);
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 input_color = texture_load(input_tx, texel);

  /* We assume that the keying screen will not be overexposed in the image, so if the input
   * brightness is high, we assume the pixel is opaque. */
  if (min_v3(input_color) > 1.0f) {
    imageStore(output_img, texel, vec4(1.0));
    return;
  }

  vec4 key_color = texture_load(key_tx, texel);
  ivec3 key_argmax = argmax(key_color.rgb);
  float input_saturation = compute_saturation(input_color, key_argmax);
  float key_saturation = compute_saturation(key_color, key_argmax);

  float matte = 1.0f - clamp(input_saturation / key_saturation, 0.0, 1.0);

  imageStore(output_img, texel, vec4(matte));
}
