#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

ivec3 compute_saturation_indices(vec3 v)
{
  int index_of_max = ((v.x > v.y) ? ((v.x > v.z) ? 0 : 2) : ((v.y > v.z) ? 1 : 2));
  ivec2 other_indices = ivec2(mod(ivec2(index_of_max) + ivec2(1, 2), ivec2(3)));
  int min_index = min(other_indices.x, other_indices.y);
  int max_index = max(other_indices.x, other_indices.y);
  return ivec3(index_of_max, max_index, min_index);
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 key = texture_load(key_tx, texel);
  vec4 color = texture_load(input_tx, texel);
  float matte = texture_load(matte_tx, texel).x;

  /* Alpha multiply the matte to the image. */
  color *= matte;

  /* Color despill. */
  ivec3 indices = compute_saturation_indices(key.rgb);
  float weighted_average = mix(color[indices.y], color[indices.z], despill_balance);
  color[indices.x] -= max(0.0, (color[indices.x] - weighted_average) * despill_factor);

  imageStore(output_img, texel, color);
}
