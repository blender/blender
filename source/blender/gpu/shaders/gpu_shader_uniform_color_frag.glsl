
uniform vec4 color;

out vec4 fragColor;

void main()
{
  fragColor = blender_srgb_to_framebuffer_space(color);
}
