
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform usampler2D outlineId;
uniform sampler2D outlineDepth;
uniform sampler2D sceneDepth;

uniform int idOffsets[3];

uniform float alphaOcclu;
uniform vec2 viewportSize;

vec4 convert_id_to_color(int id)
{
	if (id == 0) {
		return vec4(0.0);
	}
	if (id < idOffsets[1]) {
		return colorActive;
	}
	else if (id < idOffsets[2]) {
		return colorSelect;
	}
	else {
		return colorTransform;
	}
}

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);

#ifdef GPU_ARB_texture_gather
	vec2 texel_size = 1.0 / vec2(textureSize(outlineId, 0).xy);
	vec2 uv = ceil(gl_FragCoord.xy) * texel_size;

	/* Samples order is CW starting from top left. */
	uvec4 tmp1 = textureGather(outlineId, uv - texel_size);
	uvec4 tmp2 = textureGather(outlineId, uv);

	uint ref_id = tmp1.y;
	uvec4 id = uvec4(tmp1.xz, tmp2.xz);
#else
	uvec4 id;
	uint ref_id = texelFetch(outlineId, texel, 0).r;
	id.x = texelFetchOffset(outlineId, texel, 0, ivec2(-1,  0)).r;
	id.y = texelFetchOffset(outlineId, texel, 0, ivec2( 0, -1)).r;
	id.z = texelFetchOffset(outlineId, texel, 0, ivec2( 0,  1)).r;
	id.w = texelFetchOffset(outlineId, texel, 0, ivec2( 1,  0)).r;
#endif

#ifdef WIRE
	/* We want only 2px outlines. */
	/* TODO optimize, don't sample if we don't need to. */
	id.xy = uvec2(ref_id);
#endif

	bool outline = any(notEqual(id, uvec4(ref_id)));

	ivec2 depth_texel = texel;
	/* If texel is an outline but has no valid id ...
	 * replace id and depth texel by a valid one.
	 * This keeps the outline thickness consistent everywhere. */
	if (ref_id == 0u && outline) {
		depth_texel = (id.x != 0u) ? texel + ivec2(-1,  0) : depth_texel;
		depth_texel = (id.y != 0u) ? texel + ivec2( 0, -1) : depth_texel;
		depth_texel = (id.z != 0u) ? texel + ivec2( 0,  1) : depth_texel;
		depth_texel = (id.w != 0u) ? texel + ivec2( 1,  0) : depth_texel;

		ref_id = (id.x != 0u) ? id.x : ref_id;
		ref_id = (id.y != 0u) ? id.y : ref_id;
		ref_id = (id.z != 0u) ? id.z : ref_id;
		ref_id = (id.w != 0u) ? id.w : ref_id;
	}

	float ref_depth = texelFetch(outlineDepth, depth_texel, 0).r;
	float scene_depth = texelFetch(sceneDepth, depth_texel, 0).r;

	/* Avoid bad cases of zfighting for occlusion only. */
	const float epsilon = 3.0 / 8388608.0;
	bool occluded = (ref_depth > scene_depth + epsilon);

	FragColor = convert_id_to_color(int(ref_id));
	FragColor.a *= (occluded) ? alphaOcclu : 1.0;
	FragColor.a = (outline) ? FragColor.a : 0.0;
}
