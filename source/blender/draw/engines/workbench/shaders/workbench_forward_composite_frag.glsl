out vec4 fragColor;

uniform usampler2D objectId;
uniform sampler2D transparentAccum;
uniform sampler2D transparentRevealage;
uniform vec2 invertedViewportSize;

layout(std140) uniform world_block {
	WorldData world_data;
};

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec2 uv_viewport = gl_FragCoord.xy * invertedViewportSize;
	uint object_id = texelFetch(objectId, texel, 0).r;

	/* Listing 4 */
	vec4 trans_accum = texelFetch(transparentAccum, texel, 0);
	float trans_revealage = trans_accum.a;
	trans_accum.a = texelFetch(transparentRevealage, texel, 0).r;

#ifdef V3D_SHADING_OBJECT_OUTLINE
	float outline = calculate_object_outline(objectId, texel, object_id);
#else /* V3D_SHADING_OBJECT_OUTLINE */
	float outline = 1.0;
#endif /* V3D_SHADING_OBJECT_OUTLINE */
	vec3 bg_color = background_color(world_data, uv_viewport.y);

	/* TODO: Bypass the whole shader if there is no xray pass and no outline pass. */
	vec3 trans_color = trans_accum.rgb / clamp(trans_accum.a, 1e-4, 5e4);
	vec3 color = mix(trans_color, bg_color, trans_revealage);

	color = mix(world_data.object_outline_color.rgb, color, outline);

	fragColor = vec4(color, 1.0);
}
