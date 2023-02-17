uniform float faceSetsPatSeed;
uniform float faceSetsPatScale;

float tent(float f)
{
  return 1.0 - abs(fract(f) - 0.5) * 2.0;
}

void main()
{
  vec3 final_color = faceset_color;
  vec3 white = vec3(1.0, 1.0, 1.0);

  if (showIds) {
    float id = float(sculpt_id);

    if (sculpt_id == -1) {
      final_color *= vec3(0.0, 0.0, 0.0);
    }
    else {
      id *= 0.1;

      vec3 id_color = vec3(fract(id), fract(id * 0.1 + 0.1), fract(id * 0.01 + 0.2));
      // final_color = mix(final_color, id_color, 0.5);
      final_color *= id_color;
    }
  }

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
