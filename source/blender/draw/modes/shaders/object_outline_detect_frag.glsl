
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform usampler2D outlineId;
uniform sampler2D outlineDepth;
uniform sampler2D sceneDepth;

uniform int idOffsets[5];

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
		return colorGroupActive;
	}
	else if (id < idOffsets[3]) {
		return colorSelect;
	}
	else if (id < idOffsets[4]) {
		return colorGroup;
	}
	else {
		return colorTransform;
	}
}

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);

#ifdef GL_ARB_texture_gather
	vec2 texel_size = 1.0 / vec2(textureSize(outlineId, 0).xy);
	vec2 uv1 = floor(gl_FragCoord.xy) * texel_size - texel_size;
	vec2 uv2 = floor(gl_FragCoord.xy) * texel_size;

	/* Samples order is CW starting from top left. */
	uvec4 tmp1 = textureGather(outlineId, uv1);
	uvec4 tmp2 = textureGather(outlineId, uv2);

	uint ref_id = tmp1.y;
	uvec4 id = uvec4(tmp1.xz, tmp2.xz);
#else
	uvec4 id;
	uint ref_id = texelFetch(outlineId, texel, 0).r;
	id.x = texelFetchOffset(outlineId, texel, 0, ivec2( 1,  0)).r;
	id.y = texelFetchOffset(outlineId, texel, 0, ivec2( 0,  1)).r;
	id.z = texelFetchOffset(outlineId, texel, 0, ivec2(-1,  0)).r;
	id.w = texelFetchOffset(outlineId, texel, 0, ivec2( 0, -1)).r;
#endif

	float ref_depth = texelFetch(outlineDepth, texel, 0).r;
	float scene_depth = texelFetch(sceneDepth, texel, 0).r;

	/* Avoid bad cases of zfighting for occlusion only. */
	const float epsilon = 3.0 / 8388608.0;
	bool occluded = (ref_depth > scene_depth + epsilon);
	bool outline = any(notEqual(id, uvec4(ref_id)));

	FragColor = convert_id_to_color(int(ref_id));
	FragColor.a *= (occluded) ? alphaOcclu : 1.0;
	FragColor.a = (outline) ? FragColor.a : 0.0;
}
