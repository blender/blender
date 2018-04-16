
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

const ivec2 ofs[4] = ivec2[4](
    ivec2( 1,  0), ivec2( 0,  1),
    ivec2(-1,  0), ivec2( 0, -1)
);

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec2 uv = gl_FragCoord.xy / vec2(textureSize(outlineId, 0).xy);

	uvec4 id;
	uint ref_id = texelFetch(outlineId, texel, 0).r;
#if 0 /* commented out until being tested */
	id = textureGatherOffsets(outlineId, uv, ofs);
#else
	id.x = texelFetchOffset(outlineId, texel, 0, ofs[0]).r;
	id.y = texelFetchOffset(outlineId, texel, 0, ofs[1]).r;
	id.z = texelFetchOffset(outlineId, texel, 0, ofs[2]).r;
	id.w = texelFetchOffset(outlineId, texel, 0, ofs[3]).r;
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
