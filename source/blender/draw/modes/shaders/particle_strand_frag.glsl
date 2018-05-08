uniform mat4 ProjectionMatrix;

in vec3 tangent;
in vec3 viewPosition;
flat in float colRand;
out vec4 fragColor;

void main()
{
	fragColor.rgb = tangent;
	fragColor.a = 1.0;
}
