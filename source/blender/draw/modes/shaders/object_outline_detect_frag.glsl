
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
	vec4 ref_col = texelFetch(outlineColor, uv, 0).rgba;

	float depth = texelFetch(outlineDepth, uv, 0).r;
	/* Modulate color if occluded */
	float scene_depth = texelFetch(sceneDepth, uv, 0).r;

	bool ref_occlu = (depth > scene_depth);

	bool outline = false;

	search_outline(uv + ivec2( 1,  0), ref_col, ref_occlu, outline);
	search_outline(uv + ivec2( 0,  1), ref_col, ref_occlu, outline);
	search_outline(uv + ivec2(-1,  0), ref_col, ref_occlu, outline);
	search_outline(uv + ivec2( 0, -1), ref_col, ref_occlu, outline);

	FragColor = ref_col;

	/* We Hit something ! */
	if (outline) {
		if (ref_occlu) {
			FragColor.a *= alphaOcclu;
		}
	}
	else {
		FragColor.a = 0.0;
	}
}
