
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D outlineColor;

uniform float alpha;
uniform bool doExpand;

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);
	FragColor = texelFetch(outlineColor, uv, 0).rgba;

	vec4 color[4];
	color[0] = texelFetchOffset(outlineColor, uv, 0, ivec2( 1,  0)).rgba;
	color[1] = texelFetchOffset(outlineColor, uv, 0, ivec2( 0,  1)).rgba;
	color[2] = texelFetchOffset(outlineColor, uv, 0, ivec2(-1,  0)).rgba;
	color[3] = texelFetchOffset(outlineColor, uv, 0, ivec2( 0, -1)).rgba;

	vec4 values = vec4(color[0].a, color[1].a, color[2].a, color[3].a);

	vec4 tests = step(vec4(1e-6), values); /* (color.a != 0.0) */
	bvec4 btests = equal(tests, vec4(1.0));

	if (FragColor.a != 0.0) {
		return;
	}

#ifdef LARGE_OUTLINE
	if (!any(btests)) {
		color[0] = texelFetchOffset(outlineColor, uv, 0, ivec2( 2,  0)).rgba;
		color[1] = texelFetchOffset(outlineColor, uv, 0, ivec2( 0,  2)).rgba;
		color[2] = texelFetchOffset(outlineColor, uv, 0, ivec2(-2,  0)).rgba;
		color[3] = texelFetchOffset(outlineColor, uv, 0, ivec2( 0, -2)).rgba;

		values = vec4(color[0].a, color[1].a, color[2].a, color[3].a);

		tests = step(vec4(1e-6), values); /* (color.a != 0.0) */
		btests = equal(tests, vec4(1.0));
	}
#endif

	FragColor = (btests.x) ? color[0] : FragColor;
	FragColor = (btests.y) ? color[1] : FragColor;
	FragColor = (btests.z) ? color[2] : FragColor;
	FragColor = (btests.w) ? color[3] : FragColor;

	FragColor.a *= (!doExpand) ? 0.0 : 1.0;
}
