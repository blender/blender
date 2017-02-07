
uniform vec4 color1;
uniform vec4 color2;
uniform int size;

#if __VERSION__ == 120
  #define fragColor gl_FragColor
#else
  out vec4 fragColor;
#endif

void main()
{
	vec2 phase = mod(gl_FragCoord.xy, (size*2));

	if ((phase.x > size && phase.y < size) ||
		(phase.x < size && phase.y > size))
	{
		fragColor = color1;
	}
	else {
		fragColor = color2;
	}
}
