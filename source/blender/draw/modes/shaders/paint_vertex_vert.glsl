
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in vec3 color;

out vec3 finalColor;

vec3 srgb_to_linear_attrib(vec3 c) {
	c = max(c, vec3(0.0));
	vec3 c1 = c * (1.0 / 12.92);
	vec3 c2 = pow((c + 0.055) * (1.0 / 1.055), vec3(2.4));
	return mix(c1, c2, step(vec3(0.04045), c));
}

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	finalColor = srgb_to_linear_attrib(color);
}
