
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D outlineColor;
uniform sampler2D outlineDepth;
uniform sampler2D sceneDepth;

uniform float alphaOcclu;
uniform vec2 viewportSize;

void search_outline(ivec2 uv, vec4 ref_col, inout bool ref_occlu, inout bool outline)
{
	if (!outline) {
		vec4 color = texelFetch(outlineColor, uv, 0).rgba;
		if (color != ref_col) {
			outline = true;
		}
		else {
			float depth = texelFetch(outlineDepth, uv, 0).r;
			float scene_depth = texelFetch(sceneDepth, uv, 0).r;
			bool occlu = (depth > scene_depth);

			if (occlu != ref_occlu && !ref_occlu) {
				outline = true;
			}
		}
	}
}

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);

	vec4 color[4];
	/* Idea : Use a 16bit ID to identify the color
	 * and store the colors in a UBO. And fetch all ids
	 * for discontinuity check with one textureGather \o/ */
	vec4 ref_col = texelFetch(outlineColor, uv, 0).rgba;
	color[0] = texelFetchOffset(outlineColor, uv, 0, ivec2( 1,  0)).rgba;
	color[1] = texelFetchOffset(outlineColor, uv, 0, ivec2( 0,  1)).rgba;
	color[2] = texelFetchOffset(outlineColor, uv, 0, ivec2(-1,  0)).rgba;
	color[3] = texelFetchOffset(outlineColor, uv, 0, ivec2( 0, -1)).rgba;

	/* TODO GATHER */
	vec4 depths;
	float depth = texelFetch(outlineDepth, uv, 0).r;
	depths.x = texelFetchOffset(outlineDepth, uv, 0, ivec2( 1,  0)).r;
	depths.y = texelFetchOffset(outlineDepth, uv, 0, ivec2( 0,  1)).r;
	depths.z = texelFetchOffset(outlineDepth, uv, 0, ivec2(-1,  0)).r;
	depths.w = texelFetchOffset(outlineDepth, uv, 0, ivec2( 0, -1)).r;

	vec4 scene_depths;
	float scene_depth = texelFetch(sceneDepth, uv, 0).r;
	scene_depths.x = texelFetchOffset(sceneDepth, uv, 0, ivec2( 1,  0)).r;
	scene_depths.y = texelFetchOffset(sceneDepth, uv, 0, ivec2( 0,  1)).r;
	scene_depths.z = texelFetchOffset(sceneDepth, uv, 0, ivec2(-1,  0)).r;
	scene_depths.w = texelFetchOffset(sceneDepth, uv, 0, ivec2( 0, -1)).r;

	bool ref_occlu = (depth > scene_depth);
	bool outline = false;

#if 1
	bvec4 occlu = (!ref_occlu) ? notEqual(greaterThan(depths, scene_depths), bvec4(ref_occlu)) : bvec4(false);
	outline = (!outline) ? (color[0] != ref_col) || occlu.x : true;
	outline = (!outline) ? (color[1] != ref_col) || occlu.y : true;
	outline = (!outline) ? (color[2] != ref_col) || occlu.z : true;
	outline = (!outline) ? (color[3] != ref_col) || occlu.w : true;
#else
	search_outline(uv + ivec2( 1,  0), ref_col, ref_occlu, outline);
	search_outline(uv + ivec2( 0,  1), ref_col, ref_occlu, outline);
	search_outline(uv + ivec2(-1,  0), ref_col, ref_occlu, outline);
	search_outline(uv + ivec2( 0, -1), ref_col, ref_occlu, outline);
#endif

	FragColor = ref_col;
	FragColor.a *= (outline) ? (ref_occlu) ? alphaOcclu : 1.0 : 0.0;
}
