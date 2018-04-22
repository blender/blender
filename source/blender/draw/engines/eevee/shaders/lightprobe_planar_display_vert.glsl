
in vec3 pos;

in int probe_id;
in mat4 probe_mat;

out vec3 worldPosition;
flat out int probeIdx;

void main()
{
	gl_Position = ViewProjectionMatrix * probe_mat * vec4(pos, 1.0);
	worldPosition = (probe_mat * vec4(pos, 1.0)).xyz;
	probeIdx = probe_id;
}
