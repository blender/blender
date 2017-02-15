
uniform mat4 ViewProjectionMatrix;
uniform vec3 screen_vecs[2];

/* ---- Instanciated Attribs ---- */
in vec3 pos; /* using Z as axis id */

/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;
in vec3 color;
in float size;

flat out vec4 finalColor;

void main()
{
	vec3 offset;

	if (pos.z == 0.0)
		offset = vec3(1.125, 0.0, 0.0);
	else if (pos.z == 1.0)
		offset = vec3(0.0, 1.125, 0.0);
	else
		offset = vec3(0.0, 0.0, 1.125);

	vec3 screen_pos = screen_vecs[0].xyz * pos.x + screen_vecs[1].xyz * pos.y;
	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * vec4((screen_pos + offset) * size, 1.0);
	finalColor = vec4(color, 1.0);
}
