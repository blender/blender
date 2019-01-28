
uniform mat4 ViewProjectionMatrix;
uniform vec3 screenVecs[3];

/* ---- Instantiated Attrs ---- */
in float axis; /* position on the axis. [0.0-1.0] is X axis, [1.0-2.0] is Y, etc... */
in vec2 screenPos;
in vec3 colorAxis;

/* ---- Per instance Attrs ---- */
in mat4 InstanceModelMatrix;
in vec4 color;

flat out vec4 finalColor;

void main()
{
	vec3 chosen_axis = InstanceModelMatrix[int(axis)].xyz;
	vec3 y_axis = InstanceModelMatrix[1].xyz;
	vec3 bone_loc = InstanceModelMatrix[3].xyz;
	vec3 wpos = bone_loc + y_axis + chosen_axis * fract(axis);
	vec3 spos = screenVecs[0].xyz * screenPos.x + screenVecs[1].xyz * screenPos.y;
	/* Scale uniformly by axis length */
	spos *= length(chosen_axis);

	gl_Position = ViewProjectionMatrix * vec4(wpos + spos, 1.0);

	finalColor.rgb = mix(colorAxis, color.rgb, color.a);
	finalColor.a = 1.0;
}
