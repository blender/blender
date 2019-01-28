
uniform mat4 ViewMatrix;
uniform mat4 ViewProjectionMatrix;

/* ---- Instantiated Attrs ---- */
in vec3 pos;
in vec3 nor;

/* ---- Per instance Attrs ---- */
in mat4 InstanceModelMatrix;
in vec4 color;

out vec3 normal;
flat out vec4 finalColor;

void main()
{
	mat4 ModelViewProjectionMatrix = ViewProjectionMatrix * InstanceModelMatrix;
	/* This is slow and run per vertex, but it's still faster than
	 * doing it per instance on CPU and sending it on via instance attr. */
	mat3 NormalMatrix = transpose(inverse(mat3(ViewMatrix * InstanceModelMatrix)));

	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	normal = NormalMatrix * nor;

	finalColor = color;
}
