
uniform mat4 ViewProjectionMatrix;

/* ---- Instantiated Attribs ---- */
in float pos;

/* ---- Per instance Attribs ---- */
in vec3 color;
in vec4 corners[2]; /* trouble fetching vec2 */
in float depth;
in vec4 tria;
in mat4 InstanceModelMatrix;

flat out vec4 finalColor;

void main()
{
	vec3 pPos;

	if (pos == 1.0) {
		pPos = vec3(corners[0].xy, depth);
	}
	else if (pos == 2.0) {
		pPos = vec3(corners[0].zw, depth);
	}
	else if (pos == 3.0) {
		pPos = vec3(corners[1].xy, depth);
	}
	else if (pos == 4.0) {
		pPos = vec3(corners[1].zw, depth);
	}
	else if (pos == 5.0) {
		pPos = vec3(tria.xy, depth);
	}
	else if (pos == 6.0) {
		vec2 ofs = tria.xy - corners[0].xy;
		ofs.x = -ofs.x;
		pPos = vec3(corners[1].zw + ofs, depth);
	}
	else if (pos == 7.0) {
		pPos = vec3(tria.zw, depth);
	}
	else {
		pPos = vec3(0.0);
	}

	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * vec4(pPos, 1.0);

	finalColor = vec4(color, 1.0);
}
