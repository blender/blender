
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D outlineColor;
uniform sampler2D outlineDepth;

uniform float alpha;
uniform bool doExpand;

void search_outline(ivec2 uv, ivec2 offset, inout bool found_edge)
{
	if (!found_edge) {
		vec4 color = texelFetchOffset(outlineColor, uv, 0, offset).rgba;
		if (color.a != 0.0) {
			if (doExpand || color.a != 1.0) {
				FragColor = color;
				found_edge = true;
			}
		}
	}
}

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);
	FragColor = texelFetch(outlineColor, uv, 0).rgba;
	float depth = texelFetch(outlineDepth, uv, 0).r;

	if (FragColor.a != 0.0 || (depth == 1.0 && !doExpand))
		return;

	bool found_edge = false;
	search_outline(uv, ivec2( 1,  0), found_edge);
	search_outline(uv, ivec2( 0,  1), found_edge);
	search_outline(uv, ivec2(-1,  0), found_edge);
	search_outline(uv, ivec2( 0, -1), found_edge);

	/* We Hit something ! */
	if (found_edge) {
		/* only change alpha */
		FragColor.a *= alpha;
	}
}
