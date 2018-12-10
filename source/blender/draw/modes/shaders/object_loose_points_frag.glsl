
uniform vec4 color;
uniform vec4 innerColor;

out vec4 fragColor;

void main()
{
	vec2 centered = abs(gl_PointCoord - vec2(0.5));
	float dist = max(centered.x, centered.y);

	float fac = dist * dist * 4.0;
	fragColor = mix(innerColor, color, 0.45 + fac * 0.65);

	/* Make the effect more like a fresnel by offsetting
	 * the depth and creating mini-spheres.
	 * Disabled as it has performance impact. */
	// gl_FragDepth = gl_FragCoord.z + 1e-6 * fac;
}
