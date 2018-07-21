
uniform float sphere_size;
uniform int offset;
uniform ivec3 grid_resolution;
uniform vec3 corner;
uniform vec3 increment_x;
uniform vec3 increment_y;
uniform vec3 increment_z;
uniform vec3 screen_vecs[2];

flat out int cellOffset;
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
	int cell_id = gl_VertexID / 6;
	int vert_id = gl_VertexID % 6;

	vec3 ls_cell_location;
	/* Keep in sync with update_irradiance_probe */
	ls_cell_location.z = float(cell_id % grid_resolution.z);
	ls_cell_location.y = float((cell_id / grid_resolution.z) % grid_resolution.y);
	ls_cell_location.x = float(cell_id / (grid_resolution.z * grid_resolution.y));

	cellOffset = offset + cell_id;

	vec3 ws_cell_location = corner +
	    (increment_x * ls_cell_location.x +
	     increment_y * ls_cell_location.y +
	     increment_z * ls_cell_location.z);


	quadCoord = pos[vert_id];
	vec3 screen_pos = screen_vecs[0] * quadCoord.x + screen_vecs[1] * quadCoord.y;
	ws_cell_location += screen_pos * sphere_size;

	gl_Position = ViewProjectionMatrix * vec4(ws_cell_location , 1.0);
	gl_Position.z += 0.0001; /* Small bias to let the icon draw without zfighting */
}
