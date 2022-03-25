#ifndef USE_GPU_SHADER_CREATE_INFO
uniform vec4 color1;
uniform vec4 color2;
uniform int size;

out vec4 fragColor;
#endif

void main()
{
  vec2 phase = mod(gl_FragCoord.xy, (size * 2));

  if ((phase.x > size && phase.y < size) || (phase.x < size && phase.y > size)) {
    fragColor = color1;
  }
  else {
    fragColor = color2;
  }
}
