#ifndef USE_GPU_SHADER_CREATE_INFO
in vec4 mColor;
in vec2 mTexCoord;

out vec4 fragColor;
#endif

void main()
{
  const vec2 center = vec2(0, 0.5);
  vec4 tColor = vec4(geometry_out.mColor);
  /* if alpha < 0, then encap */
  if (geometry_out.mColor.a < 0) {
    tColor.a = tColor.a * -1.0;
    float dist = length(geometry_out.mTexCoord - center);
    if (dist > 0.25) {
      discard;
    }
  }
  /* Solid */
  fragColor = tColor;
}
