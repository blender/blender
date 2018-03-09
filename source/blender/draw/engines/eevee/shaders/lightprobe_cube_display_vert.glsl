
in vec3 pos;

/* Instance attrib */
in int probe_id;
in vec3 probe_location;
in float sphere_size;

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

	vec4 wpos = offsetmat * vec4(pos * sphere_size, 1.0);
	worldPosition = wpos.xyz;
	gl_Position = ViewProjectionMatrix * wpos;
	worldNormal = normalize(pos);
}