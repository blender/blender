
noperspective in vec4 finalColor;
out vec4 fragColor;

void main()
{
  fragColor = finalColor;
  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
