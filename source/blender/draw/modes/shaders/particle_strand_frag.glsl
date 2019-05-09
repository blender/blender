
in vec4 finalColor;
#ifdef USE_POINTS
in vec2 radii;
#endif

out vec4 fragColor;

void main()
{
  fragColor = finalColor;

#ifdef USE_POINTS
  float dist = length(gl_PointCoord - vec2(0.5));

  fragColor.a = mix(finalColor.a, 0.0, smoothstep(radii[1], radii[0], dist));

  if (fragColor.a == 0.0) {
    discard;
  }
#endif
}
