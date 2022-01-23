#ifndef USE_GPU_SHADER_CREATE_INFO
flat in vec4 finalColor;
out vec4 fragColor;
#endif

void main()
{
  if (finalColor.a > 0.0) {
    fragColor = finalColor;
  }
  else {
    discard;
  }
}
