
#ifndef USE_GPU_SHADER_CREATE_INFO
uniform vec4 color;
out vec4 fragColor;
#endif

void main()
{
  fragColor = blender_srgb_to_framebuffer_space(color);
}
