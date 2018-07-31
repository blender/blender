uniform int color_type;
uniform int mode;
uniform sampler2D myTexture;

in vec4 mColor;
in vec2 mTexCoord;
out vec4 fragColor;

#define texture2D texture

#define GPENCIL_MODE_LINE   0
#define GPENCIL_MODE_DOTS   1
#define GPENCIL_MODE_BOX    2

/* keep this list synchronized with list in gpencil_engine.h */
#define GPENCIL_COLOR_SOLID   0
#define GPENCIL_COLOR_TEXTURE 1
#define GPENCIL_COLOR_PATTERN 2

void main()
{
	vec2 centered = mTexCoord - vec2(0.5);
	float dist_squared = dot(centered, centered);
	const float rad_squared = 0.25;

	// round point with jaggy edges
	if ((mode != GPENCIL_MODE_BOX) && (dist_squared > rad_squared))
		discard;

	vec4 tmp_color = texture2D(myTexture, mTexCoord);

	/* Solid */
	if (color_type == GPENCIL_COLOR_SOLID) {
		fragColor = mColor;
	}
	/* texture */
	if (color_type == GPENCIL_COLOR_TEXTURE) {
		fragColor =  texture2D(myTexture, mTexCoord);
		/* mult both alpha factor to use strength factor with texture */
		fragColor.a = min(fragColor.a * mColor.a, fragColor.a);
	}
	/* pattern */
	if (color_type == GPENCIL_COLOR_PATTERN) {
		vec4 text_color = texture2D(myTexture, mTexCoord);
		fragColor = mColor;
		/* mult both alpha factor to use strength factor with color alpha limit */
		fragColor.a = min(text_color.a * mColor.a, mColor.a);
	}
}
