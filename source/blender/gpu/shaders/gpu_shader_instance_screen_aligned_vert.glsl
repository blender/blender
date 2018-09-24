
uniform mat4 ViewProjectionMatrix;
uniform vec3 screen_vecs[2];

/* ---- Instantiated Attribs ---- */
in vec3 pos; /* using Z as axis id */

/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;
in vec3 color;
in float size;

flat out vec4 finalColor;

void main()
{
	vec3 offset = vec3(0.0);

#ifdef AXIS_NAME
	if (pos.z == 0.0)
		offset = vec3(1.125, 0.0, 0.0);
	else if (pos.z == 1.0)
		offset = vec3(0.0, 1.125, 0.0);
	else
		offset = vec3(0.0, 0.0, 1.125);
	offset *= size;
#endif

	vec3 screen_pos = screen_vecs[0].xyz * pos.x + screen_vecs[1].xyz * pos.y;
	gl_Position = ViewProjectionMatrix * (InstanceModelMatrix * vec4(offset, 1.0) + vec4(screen_pos * size, 0.0));
	finalColor = vec4(color, 1.0);
}
