uniform int color_type;
uniform sampler2D myTexture;

in vec4 mColor;
in vec2 mTexCoord;
in float uvfac;

out vec4 fragColor;

#define texture2D texture

/* keep this list synchronized with list in gpencil_engine.h */
#define GPENCIL_COLOR_SOLID   0
#define GPENCIL_COLOR_TEXTURE 1
#define GPENCIL_COLOR_PATTERN 2

void main()
{
	vec4 tColor = vec4(mColor);
	/* if alpha < 0, then encap (only solid mode ) */
	if ((mColor.a < 0) && (color_type == GPENCIL_COLOR_SOLID)) {
		vec2 center = vec2(uvfac, 1.0);
		tColor.a = tColor.a * -1.0;
		float dist = length(mTexCoord - center);
		if (dist > 0.50) {
			discard;
		}
	}
	/* Solid */
	if (color_type == GPENCIL_COLOR_SOLID) {
		fragColor = tColor;
	}
	/* texture */
	if (color_type == GPENCIL_COLOR_TEXTURE) {
		fragColor =  texture2D(myTexture, mTexCoord);
		/* mult both alpha factor to use strength factor */
		fragColor.a = min(fragColor.a * tColor.a, fragColor.a);
	}
	/* pattern */
	if (color_type == GPENCIL_COLOR_PATTERN) {
		vec4 text_color = texture2D(myTexture, mTexCoord);
		fragColor = tColor;
		/* mult both alpha factor to use strength factor with color alpha limit */
		fragColor.a = min(text_color.a * tColor.a, tColor.a);
	}
}
