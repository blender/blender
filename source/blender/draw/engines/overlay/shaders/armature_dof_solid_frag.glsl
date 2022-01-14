
uniform float alpha = 1.0;

flat in vec4 finalColor;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 lineOutput;

void main()
{
  fragColor = vec4(finalColor.rgb, finalColor.a * alpha);
  lineOutput = vec4(0.0);
}
