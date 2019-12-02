
in vec3 finalColor;
flat in vec2 edgeStart;
noperspective in vec2 edgePos;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 lineOutput;

void main()
{
  lineOutput = pack_line_data(gl_FragCoord.xy, edgeStart, edgePos);
  fragColor.rgb = finalColor;
  fragColor.a = 1.0;
}
