
in vec4 finalColor;

out vec4 fragColor;

void main()
{
  float dist = length(gl_PointCoord - vec2(0.5));

  if (dist > 0.5) {
    discard;
  }
  /* Nice sphere falloff. */
  float intensity = sqrt(1.0 - dist * 2.0) * 0.5 + 0.5;
  fragColor = finalColor * vec4(intensity, intensity, intensity, 1.0);
}
