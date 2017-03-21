
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D outlineColor;
uniform sampler2D outlineDepth;
uniform sampler2D sceneDepth;

uniform float alphaOcclu;

void search_outline(ivec2 uv, ivec2 offset, vec4 ref_col, inout bool outline)
{
	if (!outline) {
		vec4 color = texelFetchOffset(outlineColor, uv, 0, offset).rgba;
		if (color != ref_col) {
			outline = true;
		}
	}
}

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);
	vec4 ref_col = texelFetch(outlineColor, uv, 0).rgba;

	bool outline = false;

	search_outline(uv, ivec2( 1,  0), ref_col, outline);
	search_outline(uv, ivec2( 0,  1), ref_col, outline);
	search_outline(uv, ivec2(-1,  0), ref_col, outline);
	search_outline(uv, ivec2( 0, -1), ref_col, outline);

	/* We Hit something ! */
	if (outline) {
		FragColor = ref_col;
		/* Modulate color if occluded */
		float depth = texelFetch(outlineDepth, uv, 0).r;
		float scene_depth = texelFetch(sceneDepth, uv, 0).r;
		/* TODO bias in linear depth not exponential */
		if (depth > scene_depth) {
			FragColor.a *= alphaOcclu;
		}
	}
	else {
		FragColor = vec4(0.0);
	}
}
