
uniform float lineWidth;

in vec4 finalColor;
noperspective in float smoothline;
#ifdef CLIP
in float clip;
#endif

out vec4 fragColor;

#define SMOOTH_WIDTH 1.0

void main()
{
#ifdef CLIP
  if (clip < 0.0) {
    discard;
  }
#endif
  fragColor = finalColor;
  fragColor.a *= clamp((lineWidth + SMOOTH_WIDTH) * 0.5 - abs(smoothline), 0.0, 1.0);
  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
