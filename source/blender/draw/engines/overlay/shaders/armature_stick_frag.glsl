
uniform float alpha = 1.0;

noperspective in float colorFac;
flat in vec4 finalWireColor;
flat in vec4 finalInnerColor;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 lineOutput;

void main()
{
  float fac = smoothstep(1.0, 0.2, colorFac);
  fragColor.rgb = mix(finalInnerColor.rgb, finalWireColor.rgb, fac);
  fragColor.a = alpha;
  lineOutput = vec4(0.0);
}
