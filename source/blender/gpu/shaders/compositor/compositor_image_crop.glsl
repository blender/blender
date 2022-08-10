#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  imageStore(output_img, texel, texture_load(input_tx, texel + lower_bound));
}
