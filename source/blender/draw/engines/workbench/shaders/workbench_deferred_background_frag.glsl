
uniform usampler2D objectId;

uniform vec2 invertedViewportSize;

out vec4 fragColor;

layout(std140) uniform world_block {
	WorldData world_data;
};

void main()
{
	vec2 uv_viewport = gl_FragCoord.xy * invertedViewportSize;
	vec3 background = background_color(world_data, uv_viewport.y);

#ifndef V3D_SHADING_OBJECT_OUTLINE

	fragColor = vec4(background, world_data.background_alpha);

#else /* !V3D_SHADING_OBJECT_OUTLINE */

	ivec2 texel = ivec2(gl_FragCoord.xy);
	uint object_id = texelFetch(objectId, texel, 0).r;
	float object_outline = calculate_object_outline(objectId, texel, object_id);

	if (object_outline == 0.0) {
		fragColor = vec4(background, world_data.background_alpha);
	}
	else {
		/* Do correct alpha blending. */
		vec4 background_color = vec4(background, 1.0) * world_data.background_alpha;
		vec4 outline_color = vec4(world_data.object_outline_color.rgb, 1.0);
		fragColor = mix(outline_color, background_color, object_outline);
		fragColor = vec4(fragColor.rgb / max(1e-8, fragColor.a), fragColor.a);
	}

#endif /* !V3D_SHADING_OBJECT_OUTLINE */
}
