
in vec3 pos;

/* Instance attrib */
in int probe_id;
in vec3 probe_location;
in float sphere_size;

uniform mat4 ViewProjectionMatrix;

flat out int pid;
out vec3 worldNormal;
out vec3 worldPosition;

void main()
{
	pid = probe_id;

	/* While this is not performant, we do this to
	 * match the object mode engine instancing shader. */
	mat4 offsetmat = mat4(1.0); /* Identity */
	offsetmat[3].xyz = probe_location;

	worldPosition = pos * sphere_size;
	gl_Position = ViewProjectionMatrix * offsetmat * vec4(worldPosition, 1.0);
	worldNormal = normalize(pos);
}