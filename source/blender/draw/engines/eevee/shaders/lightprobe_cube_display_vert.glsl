
in vec3 pos;
in vec3 nor;

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
	worldPosition = pos * 0.1 * sphere_size + probe_location;
	gl_Position = ViewProjectionMatrix * vec4(worldPosition, 1.0);
	worldNormal = normalize(nor);
}