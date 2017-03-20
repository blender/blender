
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D outlineColor;
#ifdef DEPTH_TEST
uniform sampler2D outlineDepth;
uniform sampler2D sceneDepth;
#endif

uniform float alpha;

#define ALPHA_OCCLU 0.4

void search_outline(ivec2 uv, ivec2 offset, inout float weight, inout vec4 col_accum)
{
	vec4 color = texelFetchOffset(outlineColor, uv, 0, offset).rgba;
	if (color.a != 0.0) {
#ifdef DEPTH_TEST
		/* Modulate color if occluded */
		/* TODO bias in linear depth not exponential */
		float depth = texelFetchOffset(outlineDepth, uv, 0, offset).r;
		float scene_depth = texelFetchOffset(sceneDepth, uv, 0, offset).r;
		if (depth > scene_depth) {
			color *= ALPHA_OCCLU;
		}
#endif
		col_accum += color;
		weight += 1.0;
	}
}

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);
	// vec2 uv = uvcoordsvar.xy + 0.5 / viewportSize;
	FragColor = texelFetch(outlineColor, uv, 0).rgba;

	if (FragColor.a != 0.0){
#ifdef DEPTH_TEST
		/* Modulate color if occluded */
		float depth = texelFetch(outlineDepth, uv, 0).r;
		float scene_depth = texelFetch(sceneDepth, uv, 0).r;
		/* TODO bias in linear depth not exponential */
		if (depth > scene_depth) {
			FragColor *= ALPHA_OCCLU;
		}
#endif
		return;
	}

	float weight = 0.0;
	vec4 col = vec4(0.0);

	search_outline(uv, ivec2( 1,  0), weight, col);
	search_outline(uv, ivec2( 0,  1), weight, col);
	search_outline(uv, ivec2(-1,  0), weight, col);
	search_outline(uv, ivec2( 0, -1), weight, col);

	/* We Hit something ! */
	if (weight != 0.0) {
		FragColor = col / weight;
		FragColor.a *= alpha;
	}
	else {
		FragColor = vec4(0.0);
	}
}
