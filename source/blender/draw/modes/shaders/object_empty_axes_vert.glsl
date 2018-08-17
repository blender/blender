
uniform mat4 ViewProjectionMatrix;
uniform vec3 screenVecs[3];

/* ---- Instanciated Attribs ---- */
in float axis; /* position on the axis. [0.0-1.0] is X axis, [1.0-2.0] is Y, etc... */
in vec2 screenPos;

/* ---- Per instance Attribs ---- */
in vec3 color;
in float size;
in mat4 InstanceModelMatrix;

flat out vec4 finalColor;

void main()
{
	float draw_size = 4.0 * size;
	vec3 chosen_axis = InstanceModelMatrix[int(axis)].xyz;
	vec3 loc = InstanceModelMatrix[3].xyz;
	vec3 wpos = loc + chosen_axis * fract(axis) * draw_size;
	vec3 spos = screenVecs[0].xyz * screenPos.x + screenVecs[1].xyz * screenPos.y;
	/* Scale uniformly by axis length */
	spos *= length(chosen_axis) * draw_size;

	gl_Position = ViewProjectionMatrix * vec4(wpos + spos, 1.0);

	finalColor = vec4(color, 1.0);
}
