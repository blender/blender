
noperspective in float colorFac;
flat in vec4 finalWireColor;
flat in vec4 finalInnerColor;

out vec4 fragColor;

void main()
{
	float fac = smoothstep(1.0, 0.2, colorFac);
	fragColor.rgb = mix(finalInnerColor.rgb, finalWireColor.rgb, fac);
	fragColor.a = 1.0;
}
