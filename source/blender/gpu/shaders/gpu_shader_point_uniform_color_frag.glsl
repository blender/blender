
uniform vec4 color;

#if __VERSION__ == 120
  #define fragColor gl_FragColor
#else
  out vec4 fragColor;
#endif

void main()
{
	vec2 centered = gl_PointCoord - vec2(0.5);
	float dist_squared = dot(centered, centered);
	const float rad_squared = 0.25;

	// round point with jaggy edges
	if (dist_squared > rad_squared)
		discard;

	fragColor = color;
}
