
flat in vec4 finalColor;
in vec3 normalView;

out vec4 fragColor;

void main()
{
	float n = normalize(normalView).z;
	n = gl_FrontFacing ? n : -n;
	n = clamp(n, 0.0, 1.0);
	n = gl_FrontFacing ? n : 1.0 - n;
	fragColor = finalColor;
	fragColor.rgb *= n;
}
