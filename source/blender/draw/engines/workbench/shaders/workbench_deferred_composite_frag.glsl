out vec4 fragColor;

uniform mat4 ProjectionMatrix;
uniform mat4 ViewMatrixInverse;

uniform usampler2D objectId;
uniform sampler2D colorBuffer;
uniform sampler2D specularBuffer;
uniform sampler2D normalBuffer;
/* normalBuffer contains viewport normals */
uniform sampler2D cavityBuffer;
uniform sampler2D matcapImage;

uniform vec2 invertedViewportSize;
uniform vec4 viewvecs[3];
uniform float shadowMultiplier;
uniform float lightMultiplier;
uniform float shadowShift = 0.1;

layout(std140) uniform world_block {
	WorldData world_data;
};

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec2 uv_viewport = gl_FragCoord.xy * invertedViewportSize;
	uint object_id = texelFetch(objectId, texel, 0).r;

#ifndef V3D_SHADING_OBJECT_OUTLINE
	if (object_id == NO_OBJECT_ID) {
		fragColor = vec4(background_color(world_data, uv_viewport.y), world_data.background_alpha);
		return;
	}
#else /* !V3D_SHADING_OBJECT_OUTLINE */
	float object_outline = calculate_object_outline(objectId, texel, object_id);

	if (object_id == NO_OBJECT_ID) {
		vec3 background = background_color(world_data, uv_viewport.y);
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
		return;
	}
#endif /* !V3D_SHADING_OBJECT_OUTLINE */

	vec4 diffuse_color = texelFetch(colorBuffer, texel, 0);

/* Do we need normals */
#ifdef NORMAL_VIEWPORT_PASS_ENABLED
#  ifdef WORKBENCH_ENCODE_NORMALS
	vec3 normal_viewport = normal_decode(texelFetch(normalBuffer, texel, 0).rg);
	if (diffuse_color.a == 0.0) {
		normal_viewport = -normal_viewport;
	}
#  else /* WORKBENCH_ENCODE_NORMALS */
	vec3 normal_viewport = texelFetch(normalBuffer, texel, 0).rgb;
#  endif /* WORKBENCH_ENCODE_NORMALS */
#endif

	vec3 I_vs = view_vector_from_screen_uv(uv_viewport, viewvecs, ProjectionMatrix);

	/* -------- SHADING --------- */
#ifdef V3D_LIGHTING_FLAT
	vec3 shaded_color = diffuse_color.rgb;

#elif defined(V3D_LIGHTING_MATCAP)
	bool flipped = world_data.matcap_orientation != 0;
	vec2 matcap_uv = matcap_uv_compute(I_vs, normal_viewport, flipped);
	vec3 matcap = textureLod(matcapImage, matcap_uv, 0.0).rgb;
	vec3 shaded_color = matcap * diffuse_color.rgb;

#elif defined(V3D_LIGHTING_STUDIO)

#  ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
	vec4 specular_data = texelFetch(specularBuffer, texel, 0);
#  else
	vec4 specular_data = vec4(0.0);
#  endif
	vec3 shaded_color = get_world_lighting(world_data,
	                                       diffuse_color.rgb, specular_data.rgb, specular_data.a,
	                                       normal_viewport, I_vs);
#endif

	/* -------- POST EFFECTS --------- */
#ifdef WB_CAVITY
	/* Using UNORM texture so decompress the range */
	shaded_color *= texelFetch(cavityBuffer, texel, 0).r * CAVITY_BUFFER_RANGE;
#endif

#ifdef V3D_SHADING_SHADOW
	float light_factor = -dot(normal_viewport, world_data.shadow_direction_vs.xyz);
	/* The step function might be ok for meshes but it's
	 * clearly not the case for hairs. Do smoothstep in this case. */
	float shadow_mix = smoothstep(1.0, shadowShift, light_factor);
	shaded_color *= mix(lightMultiplier, shadowMultiplier, shadow_mix);
#endif

#ifdef V3D_SHADING_OBJECT_OUTLINE
	shaded_color = mix(world_data.object_outline_color.rgb, shaded_color, object_outline);
#endif

	fragColor = vec4(shaded_color, 1.0);
}
