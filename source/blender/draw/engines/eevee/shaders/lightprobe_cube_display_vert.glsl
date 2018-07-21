
/* XXX TODO fix code duplication */
struct CubeData {
	vec4 position_type;
	vec4 attenuation_fac_type;
	mat4 influencemat;
	mat4 parallaxmat;
};

layout(std140) uniform probe_block {
	CubeData probes_data[MAX_PROBE];
};

uniform float sphere_size;
uniform vec3 screen_vecs[2];

flat out int pid;
out vec2 quadCoord;

const vec2 pos[6] = vec2[6](
	vec2(-1.0, -1.0),
	vec2( 1.0, -1.0),
	vec2(-1.0,  1.0),

	vec2( 1.0, -1.0),
	vec2( 1.0,  1.0),
	vec2(-1.0,  1.0)
);

void main()
{
	pid = 1 + (gl_VertexID / 6); /* +1 for the world */
	int vert_id = gl_VertexID % 6;

	quadCoord = pos[vert_id];

	vec3 ws_location = probes_data[pid].position_type.xyz;
	vec3 screen_pos = screen_vecs[0] * quadCoord.x + screen_vecs[1] * quadCoord.y;
	ws_location += screen_pos * sphere_size;

	gl_Position = ViewProjectionMatrix * vec4(ws_location, 1.0);
	gl_Position.z += 0.0001; /* Small bias to let the icon draw without zfighting */
}
