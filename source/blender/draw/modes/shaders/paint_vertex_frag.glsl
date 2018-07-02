
in vec3 finalColor;

out vec4 fragColor;
uniform float alpha = 1.0;
vec3 linear_to_srgb_attrib(vec3 c) {
	c = max(c, vec3(0.0));
	vec3 c1 = c * 12.92;
	vec3 c2 = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
	return mix(c1, c2, step(vec3(0.0031308), c));
}

void main()
{
	fragColor.rgb = linear_to_srgb_attrib(finalColor);
	fragColor.a = alpha;
}
