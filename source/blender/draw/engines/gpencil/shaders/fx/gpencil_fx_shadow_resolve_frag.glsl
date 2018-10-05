/* ******************************************************************* */
/* Resolve Shadow pass                                                 */
/* ******************************************************************* */
uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
uniform sampler2D shadowColor;
uniform sampler2D shadowDepth;

out vec4 FragColor;

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);

	float stroke_depth = texelFetch(strokeDepth, uv.xy, 0).r;
	float shadow_depth = texelFetch(shadowDepth, uv.xy, 0).r;
	vec4 stroke_pixel= texelFetch(strokeColor, uv.xy, 0);
	vec4 shadow_pixel= texelFetch(shadowColor, uv.xy, 0);

	/* copy original pixel */
	vec4 outcolor = stroke_pixel;
	float outdepth = stroke_depth;

	/* if stroke is not on top, copy shadow */
	if ((stroke_pixel.a <= 0.2) && (shadow_pixel.a > 0.0))  {
		outcolor = shadow_pixel;
		outdepth = shadow_depth;
	}

	gl_FragDepth = outdepth;
	FragColor = outcolor;
}
