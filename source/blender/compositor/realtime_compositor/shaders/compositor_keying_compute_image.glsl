#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 key = texture_load(key_tx, texel);
  vec4 color = texture_load(input_tx, texel);
  float matte = texture_load(matte_tx, texel).x;

  /* Alpha multiply the matte to the image. */
  color *= matte;

  /* Color despill. */
  ivec3 key_argmax = argmax(key.rgb);
  float weighted_average = mix(color[key_argmax.y], color[key_argmax.z], despill_balance);
  color[key_argmax.x] -= (color[key_argmax.x] - weighted_average) * despill_factor;

  imageStore(output_img, texel, color);
}
