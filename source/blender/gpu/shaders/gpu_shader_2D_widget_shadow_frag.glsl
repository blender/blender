
in float shadowFalloff;

out vec4 fragColor;

uniform float alpha;

void main()
{
	fragColor = vec4(0.0);
	/* Manual curve fit of the falloff curve of previous drawing method. */
	fragColor.a = alpha * (shadowFalloff * shadowFalloff * 0.722 + shadowFalloff * 0.277);
}
