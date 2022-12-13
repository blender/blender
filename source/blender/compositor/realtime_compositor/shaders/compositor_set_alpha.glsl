#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  vec4 color = vec4(texture_load(image_tx, texel).rgb, texture_load(alpha_tx, texel).x);
  imageStore(output_img, texel, color);
}
