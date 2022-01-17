#ifndef USE_GPU_SHADER_CREATE_INFO
uniform vec4 color1;
uniform vec4 color2;
uniform int size1;
uniform int size2;

out vec4 fragColor;
#endif

void main()
{
  float phase = mod((gl_FragCoord.x + gl_FragCoord.y), (size1 + size2));

  if (phase < size1) {
    fragColor = color1;
  }
  else {
    fragColor = color2;
  }
}
