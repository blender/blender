#ifndef USE_GPU_SHADER_CREATE_INFO
uniform vec4 color;
uniform float scale;

in vec2 uv;

out vec4 fragColor;
#endif

void main()
{
  /* Should be 0.8 but minimize the AA on the edges. */
  float dist = (length(uv) - 0.78) * scale;

  fragColor = color;
  fragColor.a *= smoothstep(-0.09, 1.09, dist);
}
