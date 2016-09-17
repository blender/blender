
flat varying vec4 color;
varying vec2 texcoord;

uniform sampler2D glyph;

void main()
{
	// input color replaces texture color
	gl_FragColor.rgb = color.rgb;

	// modulate input alpha & texture alpha
	gl_FragColor.a = color.a * texture2D(glyph, texcoord).a;
}
