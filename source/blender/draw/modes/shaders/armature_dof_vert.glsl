
uniform mat4 ViewProjectionMatrix;

/* ---- Instantiated Attribs ---- */
in vec2 pos;

/* ---- Per instance Attribs ---- */
/* Assumed to be in world coordinate already. */
in mat4 InstanceModelMatrix;
in vec4 color;
in vec2 amin;
in vec2 amax;

flat out vec4 finalColor;

vec3 sphere_project(float ax, float az)
{
	float sine = 1.0 - ax * ax - az * az;
	float q3 = sqrt(max(0.0, sine));

	return vec3(-az * q3, 0.5 - sine, ax * q3) * 2.0;
}

void main()
{
	vec3 final_pos = sphere_project(pos.x * abs((pos.x > 0.0) ? amax.x : amin.x),
	                                pos.y * abs((pos.y > 0.0) ? amax.y : amin.y));
	gl_Position = ViewProjectionMatrix * (InstanceModelMatrix * vec4(final_pos, 1.0));
	finalColor = color;
}
