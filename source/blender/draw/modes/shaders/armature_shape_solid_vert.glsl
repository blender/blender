
uniform mat3 NormalMatrix;
uniform mat4 ViewMatrixInverse;
uniform mat4 ViewProjectionMatrix;

uniform mat4 ViewMatrix;
uniform mat4 ProjectionMatrix;

/* ---- Instanciated Attribs ---- */
in vec3 pos;
in vec3 nor;

/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;
in vec3 boneColor;
in vec3 stateColor;

out vec4 finalColor;

void main()
{
	mat3 NormalMatrix = transpose(inverse(mat3(ViewMatrix * InstanceModelMatrix)));
	vec3 normal = normalize(NormalMatrix * nor);

	/* Do lighting at an angle to avoid flat shading on front facing bone. */
	const vec3 light = vec3(0.1, 0.1, 0.8);
	float n = dot(normal, light);

	/* Smooth lighting factor. */
	const float s = 0.2; /* [0.0-0.5] range */
	float fac = clamp((n * (1.0 - s)) + s, 0.0, 1.0);
	finalColor.rgb = mix(stateColor, boneColor, fac);
	finalColor.a = 1.0;

	gl_Position = ViewProjectionMatrix * (InstanceModelMatrix * vec4(pos, 1.0));
}
