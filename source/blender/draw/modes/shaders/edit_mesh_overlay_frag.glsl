
uniform float edgeScale;
uniform bool isXray = false;

flat in vec4 finalColorStipple;
in float dist;
flat in float base_dist;

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
	float dist_px = dist - base_dist;
	dist_px /= fwidth(dist_px);
	float mix_fac = step(0.5, fract(abs(dist_px) * (1.0 / 20.0)));
	if (finalColorStipple.a == 0.0) {
		mix_fac = 1.0;
	}
#if defined(EDGE) && !defined(FLAT)
	vec4 prim_col = mix(colorEditMeshMiddle, colorEdgeSelect, finalColor.a);
	prim_col = (selectOveride != 0) ? prim_col : finalColor;
	prim_col.a = 1.0;
#else
#  define prim_col finalColor
#endif
	FragColor = mix(finalColorStipple, prim_col, mix_fac);
}
