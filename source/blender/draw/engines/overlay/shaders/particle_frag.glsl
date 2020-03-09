
in vec4 finalColor;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 lineOutput;

void main()
{
  float dist = length(gl_PointCoord - vec2(0.5));

  if (dist > 0.5) {
    discard;
  }
  /* Nice sphere falloff. */
  float intensity = sqrt(1.0 - dist * 2.0) * 0.5 + 0.5;
  fragColor = finalColor * vec4(intensity, intensity, intensity, 1.0);

  lineOutput = vec4(0.0);
}
