
// Draw dashed lines, perforated in screen space.
// Based on a (3D) version by Mike Erwin.

#if __VERSION__ == 120
  noperspective varying float distance_along_line;
  #define fragColor gl_FragColor
#else
  noperspective in float distance_along_line;
  out vec4 fragColor;
#endif

uniform float dash_width;
uniform float dash_width_on;
uniform vec4 color1;
uniform vec4 color2;

void main()
{
	if (mod(distance_along_line, dash_width) <= dash_width_on) {
		fragColor = color1;
	} else {
		fragColor = color2;
	}
}
