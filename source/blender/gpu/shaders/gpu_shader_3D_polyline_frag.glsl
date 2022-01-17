#ifndef USE_GPU_SHADER_CREATE_INFO
uniform float lineWidth;
uniform bool lineSmooth = true;

in vec4 finalColor;
noperspective in float smoothline;
#  ifdef CLIP
in float clip;
#  endif

out vec4 fragColor;
#endif

#define SMOOTH_WIDTH 1.0

void main()
{
#ifdef CLIP
  if (clip < 0.0) {
    discard;
  }
#endif
  fragColor = finalColor;
  if (lineSmooth) {
    fragColor.a *= clamp((lineWidth + SMOOTH_WIDTH) * 0.5 - abs(smoothline), 0.0, 1.0);
  }
  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
