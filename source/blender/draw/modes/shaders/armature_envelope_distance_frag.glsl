
flat in vec3 finalStateColor; /* UNUSED */
flat in vec3 finalBoneColor; /* UNUSED */
in vec3 normalView;

out vec4 fragColor;

uniform vec4 color = vec4(1.0, 1.0, 1.0, 0.2);

void main()
{
	float n = normalize(normalView).z;
	n = 1.0 - clamp(-n, 0.0, 1.0);
	fragColor = color * n;
}
