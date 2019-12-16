
noperspective in vec2 stipple_coord;
flat in vec2 stipple_start;
flat in vec4 finalColor;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 lineOutput;

void main()
{
  fragColor = finalColor;

  /* Stipple */
  const float dash_width = 6.0;
  const float dash_factor = 0.5;

  lineOutput = pack_line_data(gl_FragCoord.xy, stipple_start, stipple_coord);

  float dist = distance(stipple_start, stipple_coord);

  if (fragColor.a == 0.0) {
    /* Disable stippling. */
    dist = 0.0;
  }

  fragColor.a = 1.0;

  if (fract(dist / dash_width) > dash_factor) {
    discard;
  }
}
