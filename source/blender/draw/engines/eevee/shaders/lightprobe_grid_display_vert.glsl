
in vec3 pos;

uniform float sphere_size;
uniform int offset;
uniform ivec3 grid_resolution;
uniform vec3 corner;
uniform vec3 increment_x;
uniform vec3 increment_y;
uniform vec3 increment_z;

flat out int cellOffset;
out vec3 worldNormal;

void main()
{
	vec3 ls_cell_location;
	/* Keep in sync with update_irradiance_probe */
	ls_cell_location.z = float(gl_InstanceID % grid_resolution.z);
	ls_cell_location.y = float((gl_InstanceID / grid_resolution.z) % grid_resolution.y);
	ls_cell_location.x = float(gl_InstanceID / (grid_resolution.z * grid_resolution.y));

	cellOffset = offset + gl_InstanceID;

	vec3 ws_cell_location = corner +
	    (increment_x * ls_cell_location.x +
	     increment_y * ls_cell_location.y +
	     increment_z * ls_cell_location.z);

	gl_Position = ViewProjectionMatrix * vec4(pos * 0.02 * sphere_size + ws_cell_location, 1.0);
	worldNormal = normalize(pos);
}