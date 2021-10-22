uniform float faceSetsOpacity;
uniform float faceSetsPatSeed;
uniform float faceSetsPatScale;
uniform bool useMoire;

flat in vec3 faceset_color;
in float mask_color;

out vec4 fragColor;

float tent(float f)
{
  return 1.0 - abs(fract(f) - 0.5) * 2.0;
}

void main()
{
  vec3 final_color = faceset_color;
  vec3 white = vec3(1.0, 1.0, 1.0);

  if (!useMoire) {
    fragColor = vec4(final_color * vec3(mask_color), 1.0);
    return;
  }

  vec2 xy = gl_FragCoord.xy * faceSetsPatScale;

  /* Basic moire pattern */

  float seed = 1.0 / 3.0 + faceSetsPatSeed;

  float dx1 = tent(xy.x);
  float dy1 = tent(xy.y);
  float dx2 = tent(tent(seed) * xy.x + tent(seed * seed + 0.5) * xy.y);
  float dy2 = tent(tent(seed) * xy.y - tent(seed * seed + 0.5) * xy.x);

  float fac = (dx1 + dy1 + dx2 + dy2) / 4.0;
  fac = step(fac, 1.0 - faceSetsOpacity);

  final_color += (white - final_color) * fac;

  fragColor = vec4(final_color * vec3(mask_color), 1.0);
}
