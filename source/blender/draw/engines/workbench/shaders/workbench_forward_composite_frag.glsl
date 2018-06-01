out vec4 fragColor;

uniform usampler2D objectId;
uniform sampler2D transparentAccum;
#ifdef WORKBENCH_REVEALAGE_ENABLED
uniform sampler2D transparentRevealage;
#endif
uniform vec2 invertedViewportSize;

layout(std140) uniform world_block {
	WorldData world_data;
};

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec2 uv_viewport = gl_FragCoord.xy * invertedViewportSize;
	uint object_id = texelFetch(objectId, texel, 0).r;
	vec4 transparent_accum = texelFetch(transparentAccum, texel, 0);
#ifdef WORKBENCH_REVEALAGE_ENABLED
	float transparent_revealage = texelFetch(transparentRevealage, texel, 0).r;
#endif
	vec4 color;
	vec4 bg_color;

#ifdef V3D_SHADING_OBJECT_OUTLINE
	float outline = calculate_object_outline(objectId, texel, object_id);
#else /* V3D_SHADING_OBJECT_OUTLINE */
	float outline = 1.0;
#endif /* V3D_SHADING_OBJECT_OUTLINE */
	bg_color = vec4(background_color(world_data, uv_viewport.y), 0.0);
	if (object_id == NO_OBJECT_ID) {
		color = bg_color;
	} else {
#ifdef WORKBENCH_REVEALAGE_ENABLED
		color = vec4((transparent_accum.xyz / max(transparent_accum.a, EPSILON)) * (1.0 - transparent_revealage), 1.0);
		color = mix(bg_color, color, clamp(1.0 - transparent_revealage, 0.0, 1.0));
#else
		color = vec4(transparent_accum.xyz / max(transparent_accum.a, EPSILON), 1.0);
#endif
	}

	fragColor = vec4(mix(world_data.object_outline_color.rgb, color.xyz, outline), 1.0);
}
