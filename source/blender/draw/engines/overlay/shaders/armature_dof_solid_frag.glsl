
flat in vec4 finalColor;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 lineOutput;

void main()
{
  fragColor = finalColor;
  lineOutput = vec4(0.0);
}
