#ifndef USE_GPU_SHADER_CREATE_INFO
in vec4 finalColor;
out vec4 fragColor;
#endif

#if defined(VERT)
in float vertexCrease;
#endif

void main()
{
  vec2 centered = gl_PointCoord - vec2(0.5);
  float dist_squared = dot(centered, centered);
  const float rad_squared = 0.25;

  // round point with jaggy edges
  if (dist_squared > rad_squared) {
    discard;
  }

#if defined(VERT)
  fragColor = finalColor;

  float midStroke = 0.5 * rad_squared;
  if (vertexCrease > 0.0 && dist_squared > midStroke) {
    fragColor.rgb = mix(finalColor.rgb, colorEdgeCrease.rgb, vertexCrease);
  }
#else
  fragColor = finalColor;
#endif
}
