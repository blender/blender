uniform int color_type;
uniform sampler2D myTexture;

in vec4 mColor;
in vec2 mTexCoord;
in vec2 uvfac;

out vec4 fragColor;

#define texture2D texture

/* keep this list synchronized with list in gpencil_engine.h */
#define GPENCIL_COLOR_SOLID   0
#define GPENCIL_COLOR_TEXTURE 1
#define GPENCIL_COLOR_PATTERN 2

#define ENDCAP 1.0

void main()
{
	vec4 tColor = vec4(mColor);
	/* if uvfac[1]  == 1, then encap */
	if (uvfac[1] == ENDCAP) {
		vec2 center = vec2(uvfac[0], 0.5);
		float dist = length(mTexCoord - center);
		if (dist > 0.50) {
			discard;
		}
	}
	/* Solid */
	if (color_type == GPENCIL_COLOR_SOLID) {
		fragColor = tColor;
	}

	/* texture for endcaps */
	vec4 text_color;
	if (uvfac[1] == ENDCAP) {
		text_color = texture2D(myTexture, vec2(mTexCoord.x,  mTexCoord.y));
	}
	else {
		text_color = texture2D(myTexture, mTexCoord);
	}

	/* texture */
	if (color_type == GPENCIL_COLOR_TEXTURE) {
		fragColor =  text_color;
		/* mult both alpha factor to use strength factor */
		fragColor.a = min(fragColor.a * tColor.a, fragColor.a);
	}
	/* pattern */
	if (color_type == GPENCIL_COLOR_PATTERN) {
		fragColor = tColor;
		/* mult both alpha factor to use strength factor with color alpha limit */
		fragColor.a = min(text_color.a * tColor.a, tColor.a);
	}

	if(fragColor.a < 0.0035)
		discard;
}
