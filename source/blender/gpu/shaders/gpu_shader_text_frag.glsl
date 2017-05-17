
flat in vec4 color_flat;
noperspective in vec2 texCoord_interp;
out vec4 fragColor;
#define texture2D texture

uniform sampler2D glyph;

void main()
{
	// input color replaces texture color
	fragColor.rgb = color_flat.rgb;

	// modulate input alpha & texture alpha
	fragColor.a = color_flat.a * texture2D(glyph, texCoord_interp).r;
}
