
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in vec4 nor; /* flag stored in w */

flat out vec4 finalColor;

void main()
{
	bool is_select = (nor.w > 0.0);
	bool is_hidden = (nor.w < 0.0);
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	/* Add offset in Z to avoid zfighting and render selected wires on top. */
	/* TODO scale this bias using znear and zfar range. */
	gl_Position.zw -= exp2(-20) * (is_select ? 2.0 : 1.0);

	if (is_hidden) {
		gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);
	}

#ifdef VERTEX_MODE
	vec4 colSel = colorEdgeSelect;
	colSel.rgb = clamp(colSel.rgb - 0.2, 0.0, 1.0);
#else
	const vec4 colSel = vec4(1.0, 1.0, 1.0, 1.0);
#endif

	finalColor = (is_select) ? colSel : colorWire;
	finalColor.a = nor.w;
}
