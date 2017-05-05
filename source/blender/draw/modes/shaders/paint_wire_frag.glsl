
flat in int finalSelect;
out vec4 fragColor;

void main()
{
#ifdef VERTEX_MODE
	vec4 colSel = colorEdgeSelect;
	colSel.rgb = clamp(colSel.rgb - 0.2, 0.0, 1.0);
#else
	const vec4 colSel = vec4(1.0, 1.0, 1.0, 1.0);
#endif

	const vec4 colUnsel = vec4(0.5, 0.5, 0.5, 1.0);

	fragColor = bool(finalSelect) ? colSel : colUnsel;
}
