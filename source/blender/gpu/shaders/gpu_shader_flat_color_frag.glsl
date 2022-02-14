#pragma BLENDER_REQUIRE(gpu_shader_colorspace_lib.glsl)

#ifndef USE_GPU_SHADER_CREATE_INFO
flat in vec4 finalColor;
out vec4 fragColor;
#endif

void main()
{
  fragColor = finalColor;
  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
