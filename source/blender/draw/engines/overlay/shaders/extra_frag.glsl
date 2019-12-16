
noperspective in vec2 edgePos;
flat in vec2 edgeStart;
flat in vec4 finalColor;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 lineOutput;

void main()
{
  fragColor = finalColor;
  lineOutput = pack_line_data(gl_FragCoord.xy, edgeStart, edgePos);
}
