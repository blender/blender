
uniform vec3 light;
uniform float alpha;
uniform float global;

in vec3 normal;
in vec4 finalColor;
out vec4 fragColor;

void main()
{
	fragColor = finalColor * (global + (1.0 - global) * max(0.0, dot(normalize(normal), light)));
	fragColor.a = alpha;
}
