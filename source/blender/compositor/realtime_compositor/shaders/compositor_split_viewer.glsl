#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
#if defined(SPLIT_HORIZONTAL)
  bool condition = (view_size.x * split_ratio) < texel.x;
#elif defined(SPLIT_VERTICAL)
  bool condition = (view_size.y * split_ratio) < texel.y;
#endif
  vec4 color = condition ? texture_load(first_image_tx, texel) :
                           texture_load(second_image_tx, texel);
  imageStore(output_img, texel + compositing_region_lower_bound, color);
}
