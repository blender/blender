
#ifdef FLAT
flat in vec4 finalColor;
#else
in vec4 finalColor;
#  ifdef EDGE
flat in int selectOveride;
#  endif
#endif

out vec4 FragColor;

void main()
{
#if defined(EDGE) && !defined(FLAT)
	vec4 prim_col = mix(colorEditMeshMiddle, colorEdgeSelect, finalColor.a);
	prim_col = (selectOveride != 0) ? prim_col : finalColor;
	prim_col.a = 1.0;
#else
#  define prim_col finalColor
#endif
	FragColor = prim_col;
}
