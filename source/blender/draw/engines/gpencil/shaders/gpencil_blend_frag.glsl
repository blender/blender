in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
uniform sampler2D blendColor;
uniform sampler2D blendDepth;
uniform int mode;
uniform int clamp_layer;
uniform float blend_opacity;

#define ON 1
#define OFF 0

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

vec4 get_blend_color(int mode, vec4 src_color, vec4 blend_color)
{
	vec4 mix_color = blend_color;
	vec4 outcolor;

    if (mix_color.a == 0) {
		outcolor = src_color;
	}
	else if (mode == MODE_OVERLAY) {
		mix_color.rgb = mix_color.rgb * mix_color.a * blend_opacity;
		outcolor.r = overlay_color(src_color.r, mix_color.r);
		outcolor.g = overlay_color(src_color.g, mix_color.g);
		outcolor.b = overlay_color(src_color.b, mix_color.b);
		outcolor.a = src_color.a;
	}
	else if (mode == MODE_ADD){
		mix_color.rgb = mix_color.rgb * mix_color.a * blend_opacity;
		outcolor = src_color + mix_color;
		outcolor.a = src_color.a;
	}
	else if (mode == MODE_SUB){
		outcolor = src_color - mix_color;
		outcolor.a = clamp(src_color.a - (mix_color.a * blend_opacity), 0.0, 1.0);
	}
	else if (mode == MODE_MULTIPLY)	{
		mix_color.rgb = mix_color.rgb * mix_color.a * blend_opacity;
		outcolor = src_color * mix_color;
		outcolor.a = src_color.a;
	}
	else if (mode == MODE_DIVIDE) {
		mix_color.rgb = mix_color.rgb * mix_color.a * blend_opacity;
		outcolor = src_color / mix_color;
		outcolor.a = src_color.a;
	}
	else {
		outcolor = mix_color * blend_opacity;;
		outcolor.a = src_color.a;
	}
	
	return outcolor;
}

void main()
{
	vec4 outcolor;
	ivec2 uv = ivec2(gl_FragCoord.xy);
	vec4 stroke_color =  texelFetch(strokeColor, uv, 0).rgba;
	float stroke_depth = texelFetch(strokeDepth, uv, 0).r;
	
	vec4 mix_color =  texelFetch(blendColor, uv, 0).rgba;
	float mix_depth = texelFetch(blendDepth, uv, 0).r;

	/* premult alpha factor to remove double blend effects */
	if (stroke_color.a > 0) {
		stroke_color = vec4(vec3(stroke_color.rgb / stroke_color.a), stroke_color.a);
	}
	if (mix_color.a > 0) {
		mix_color = vec4(vec3(mix_color.rgb / mix_color.a), mix_color.a);
	}
	
	/* Normal mode */
	if (mode == MODE_NORMAL) {
		if (stroke_color.a > 0) {
			if (mix_color.a > 0) {
				FragColor = vec4(mix(stroke_color.rgb, mix_color.rgb, mix_color.a), stroke_color.a);
				gl_FragDepth = mix_depth;
			}
			else {
				FragColor = stroke_color;
				gl_FragDepth = stroke_depth;
			}
		}
		else {
			if (clamp_layer == ON) {
				discard;
			}
			else {
				FragColor = mix_color;
				gl_FragDepth = mix_depth;
			}
		}
		return;
	}
	
	/* if not using mask, return mix color */
	if ((stroke_color.a == 0) && (clamp_layer == OFF)) {
		FragColor = mix_color;
		gl_FragDepth = mix_depth;
		return;
	}

	/* apply blend mode */
	FragColor = get_blend_color(mode, stroke_color, mix_color);
	gl_FragDepth = stroke_depth;
}
