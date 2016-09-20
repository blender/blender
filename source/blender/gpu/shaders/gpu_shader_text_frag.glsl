
#if __VERSION__ == 120
  flat varying vec4 color;
  noperspective varying vec2 texcoord;
  #define fragColor gl_FragColor
#else
  flat in vec4 color;
  noperspective in vec2 texcoord;
  out vec4 fragColor;
#endif

uniform sampler2D glyph;

void main()
{
	// input color replaces texture color
	fragColor.rgb = color.rgb;

	// modulate input alpha & texture alpha
	fragColor.a = color.a * texture2D(glyph, texcoord).a;
}
