/* ******************************************************************* */
/* Resolve RIM pass and add blur if needed                                 */
/* ******************************************************************* */
uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
uniform sampler2D strokeRim;

uniform vec3 mask_color;
uniform int mode;

out vec4 FragColor;

#define MODE_NORMAL   0
#define MODE_OVERLAY  1
#define MODE_ADD      2
#define MODE_SUB      3
#define MODE_MULTIPLY 4
#define MODE_DIVIDE   5

float overlay_color(float a, float b)
{
	float rtn;
		if (a < 0.5) {
			rtn = 2.0 * a * b;
		}
		else {
			rtn = 1.0 - 2.0 * (1.0 - a) * (1.0 - b);
		}

	return rtn;
}

vec4 get_blend_color(int mode, vec4 src_color, vec4 mix_color)
{
	vec4 outcolor;
	if (mode == MODE_NORMAL) {
		outcolor = mix_color;
	}
	else if (mode == MODE_OVERLAY) {
		outcolor.r = overlay_color(src_color.r, mix_color.r);
		outcolor.g = overlay_color(src_color.g, mix_color.g);
		outcolor.b = overlay_color(src_color.b, mix_color.b);
	}
	else if (mode == MODE_ADD){
		outcolor = src_color + mix_color;
	}
	else if (mode == MODE_SUB){
		outcolor = src_color - mix_color;
	}
	else if (mode == MODE_MULTIPLY)	{
		outcolor = src_color * mix_color;
	}
	else if (mode == MODE_DIVIDE) {
		outcolor = src_color / mix_color;
	}
	else {
		outcolor = mix_color;
	}

	/* use always the alpha of source color */

	outcolor.a = src_color.a;
	/* use alpha to calculate the weight of the mixed color */
	outcolor = mix(src_color, outcolor, mix_color.a);

	return outcolor;
}

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);

	float stroke_depth = texelFetch(strokeDepth, uv.xy, 0).r;
	vec4 src_pixel= texelFetch(strokeColor, uv.xy, 0);
	vec4 rim_pixel= texelFetch(strokeRim, uv.xy, 0);

	vec4 outcolor = src_pixel;

	/* is transparent */
	if (src_pixel.a == 0.0f) {
		discard;
	}
	/* pixel is equal to mask color, keep */
	else if (src_pixel.rgb == mask_color.rgb) {
		outcolor = src_pixel;
	}
	else {
		if (rim_pixel.a == 0.0f) {
			outcolor = src_pixel;
		}
		else {
			outcolor = get_blend_color(mode, src_pixel, rim_pixel);
		}
	}

	gl_FragDepth = stroke_depth;
	FragColor = outcolor;
}



