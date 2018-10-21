uniform mat4 ProjectionMatrix;
uniform mat4 ViewMatrix;

/* ******************************************************************* */
/* create glow mask                                                    */
/* ******************************************************************* */
uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;

uniform vec3 glow_color;
uniform vec3 select_color;
uniform float threshold;
uniform int mode;

out vec4 FragColor;

#define MODE_LUMINANCE   0
#define MODE_COLOR       1

/* calc luminance */
float luma( vec3 color ) {
	/* the color is linear, so do not apply tonemapping */
	return (color.r + color.g + color.b) / 3.0;
}

bool check_color(vec3 color_a, vec3 color_b)
{
 /* need round the number to avoid precision errors */
	if ((floor(color_a.r * 100) == floor(color_b.r * 100)) &&
		(floor(color_a.g * 100) == floor(color_b.g * 100)) &&
		(floor(color_a.b * 100) == floor(color_b.b * 100)))
		{
		return true;
		}

 return false;
}

void main()
{
	vec2 uv = vec2(gl_FragCoord.xy);

	float stroke_depth = texelFetch(strokeDepth, ivec2(uv.xy), 0).r;
	vec4 src_pixel= texelFetch(strokeColor, ivec2(uv.xy), 0);
	vec4 outcolor;

	/* is transparent */
	if (src_pixel.a == 0.0f) {
		discard;
	}

	if (mode == MODE_LUMINANCE) {
		if (luma(src_pixel.rgb) < threshold) {
			discard;
		}
	}
	else if (mode == MODE_COLOR) {
		if (!check_color(src_pixel.rgb, select_color.rgb)) {
			discard;
		}
	}
	else {
		discard;
	}

	gl_FragDepth = stroke_depth;
	FragColor = vec4(glow_color.rgb, 1.0);
}
