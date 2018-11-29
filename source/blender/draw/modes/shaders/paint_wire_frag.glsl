
flat in int finalFlag;
out vec4 fragColor;

#define VERTEX_SELECTED (1 << 0)
#define VERTEX_HIDE     (1 << 4)

void main()
{
	if (bool(finalFlag & VERTEX_HIDE)) {
		discard;
	}

	/* Apply depth offset by taking slope and distance into account. */
	gl_FragDepth = gl_FragCoord.z - mix(exp2(-10), exp2(-23), gl_FragCoord.z) - 2.0 * fwidth(gl_FragCoord.z);

#ifdef VERTEX_MODE
	vec4 colSel = colorEdgeSelect;
	colSel.rgb = clamp(colSel.rgb - 0.2, 0.0, 1.0);
#else
	const vec4 colSel = vec4(1.0, 1.0, 1.0, 1.0);
#endif

	fragColor = bool(finalFlag & VERTEX_SELECTED) ? colSel : colorWire;
}
