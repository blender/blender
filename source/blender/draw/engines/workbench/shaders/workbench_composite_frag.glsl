out vec4 fragColor;

uniform sampler2D depthBuffer;
uniform usampler2D objectId;
uniform sampler2D colorBuffer;
uniform sampler2D normalBuffer;
/* normalBuffer contains viewport normals */
uniform vec2 invertedViewportSize;
uniform vec3 objectOverlapColor = vec3(0.0);
uniform float lightMultiplier;

layout(std140) uniform world_block {
	WorldData world_data;
};


void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec2 uv_viewport = gl_FragCoord.xy * invertedViewportSize;
	float depth = texelFetch(depthBuffer, texel, 0).r;

#ifndef V3D_DRAWOPTION_OBJECT_OVERLAP
	if (depth == 1.0) {
		fragColor = vec4(linearrgb_to_srgb(background_color(world_data, uv_viewport.y)), 0.0);
		return;
	}
#else /* !V3D_DRAWOPTION_OBJECT_OVERLAP */
	uint object_id = depth == 1.0? NO_OBJECT_ID: texelFetch(objectId, texel, 0).r;
	float object_overlap = calculate_object_overlap(objectId, texel, object_id);

	if (object_id == NO_OBJECT_ID) {
		vec3 background = background_color(world_data, uv_viewport.y);
		if (object_overlap == 0.0) {
			fragColor = vec4(linearrgb_to_srgb(background), 0.0);
		} else {
			fragColor = vec4(linearrgb_to_srgb(mix(objectOverlapColor, background, object_overlap)), 1.0-object_overlap);
		}
		return;
	}
#endif /* !V3D_DRAWOPTION_OBJECT_OVERLAP */


#ifdef V3D_LIGHTING_STUDIO
	vec4 diffuse_color = texelFetch(colorBuffer, texel, 0);
#ifdef WORKBENCH_ENCODE_NORMALS
	vec3 normal_viewport = normal_decode(texelFetch(normalBuffer, texel, 0).rg);
	if (diffuse_color.a == 1.0) {
		normal_viewport = - normal_viewport;
	}
#else /* WORKBENCH_ENCODE_NORMALS */
	vec3 normal_viewport = texelFetch(normalBuffer, texel, 0).rgb;
#endif /* WORKBENCH_ENCODE_NORMALS */
	vec3 diffuse_light = get_world_diffuse_light(world_data, normal_viewport);
	vec3 shaded_color = diffuse_light * diffuse_color.rgb * lightMultiplier;

#else /* V3D_LIGHTING_STUDIO */
	vec3 diffuse_color = texelFetch(colorBuffer, texel, 0).rgb;
	vec3 shaded_color = diffuse_color * lightMultiplier;
#endif /* V3D_LIGHTING_STUDIO */


#ifdef V3D_DRAWOPTION_OBJECT_OVERLAP
	shaded_color = mix(objectOverlapColor, shaded_color, object_overlap);
#endif /* V3D_DRAWOPTION_OBJECT_OVERLAP */

	fragColor = vec4(linearrgb_to_srgb(shaded_color), 1.0);
}
