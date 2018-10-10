/* ******************************************************************* */
/* Resolve GLOW pass                                                   */
/* ******************************************************************* */
uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
uniform sampler2D glowColor;
uniform sampler2D glowDepth;
uniform int alpha_mode;

out vec4 FragColor;

void main()
{
	vec4 outcolor;
	ivec2 uv = ivec2(gl_FragCoord.xy);

	float stroke_depth = texelFetch(strokeDepth, uv.xy, 0).r;
	vec4 src_pixel= texelFetch(strokeColor, uv.xy, 0);
	vec4 glow_pixel= texelFetch(glowColor, uv.xy, 0);
	float glow_depth = texelFetch(glowDepth, uv.xy, 0).r;

	if (alpha_mode == 0) {
		outcolor = src_pixel + glow_pixel;
	}
	else {
		if ((src_pixel.a < 0.1) || (glow_pixel.a < 0.1)) {
			outcolor = src_pixel + glow_pixel;
		}
		else {
			outcolor = src_pixel;
		}
	}
	
	if (src_pixel.a < glow_pixel.a) {
		gl_FragDepth = glow_depth;
	}
	else {
		gl_FragDepth = stroke_depth;
	}
	
	if (outcolor.a < 0.001) {
		discard;
	}
	
	FragColor = outcolor;
}
