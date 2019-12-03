
flat in vec2 edgeStart;

#ifndef SELECT_EDGES
in vec3 finalColor;
noperspective in vec2 edgePos;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 lineOutput;
#endif

void main()
{
  /* Needed only because of wireframe slider.
   * If we could get rid of it would be nice because of performance drain of discard. */
  if (edgeStart.r == -1.0) {
    discard;
  }

#ifndef SELECT_EDGES
  lineOutput = pack_line_data(gl_FragCoord.xy, edgeStart, edgePos);
  fragColor.rgb = finalColor;
  fragColor.a = 1.0;
#endif
}
