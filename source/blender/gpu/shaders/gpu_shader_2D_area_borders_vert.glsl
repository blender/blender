
uniform mat4 ModelViewProjectionMatrix;

uniform vec4 rect;
uniform int cornerLen;
uniform float scale;

in vec2 pos;

const vec2 jitter_ofs[8] = vec2[8](
	vec2( 0.468813, -0.481430), vec2(-0.155755, -0.352820),
	vec2( 0.219306, -0.238501), vec2(-0.393286, -0.110949),
	vec2(-0.024699,  0.013908), vec2( 0.343805,  0.147431),
	vec2(-0.272855,  0.269918), vec2( 0.095909,  0.388710)
);

void main()
{
	int corner_id = (gl_VertexID / cornerLen) % 4;
	int jitter_id = gl_VertexID / (cornerLen * 4) % 8;

	vec2 final_pos = pos * scale;

	if (corner_id == 0)
		final_pos += rect.yw;  /* top right */
	else if (corner_id == 1)
		final_pos += rect.xw;  /* top left */
	else if (corner_id == 2)
		final_pos += rect.xz;  /* bottom left */
	else
		final_pos += rect.yz;  /* bottom right */

	/* Only jitter verts inside the corner (not the one shared with the edges). */
	if ((gl_VertexID % cornerLen) % (cornerLen - 2) != 0) {
		/* Only jitter intern verts not boundaries. */
		if ((gl_VertexID % 2) == 0) {
			final_pos += jitter_ofs[jitter_id];
		}
	}

	gl_Position = (ModelViewProjectionMatrix * vec4(final_pos, 0.0, 1.0));
}
