out vec4 fragColor;

uniform usampler2D objectId;
uniform sampler2D colorBuffer;
uniform sampler2D normalBuffer;
/* normalBuffer contains viewport normals */
uniform vec2 invertedViewportSize;
uniform float shadowMultiplier;
uniform float lightMultiplier;
uniform float shadowShift = 0.1;
uniform mat3 normalWorldMatrix;


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
		fragColor = vec4(background_color(world_data, uv_viewport.y), 0.0);
		return;
	}
#else /* !V3D_SHADING_OBJECT_OUTLINE */
	float object_outline = calculate_object_outline(objectId, texel, object_id);

	if (object_id == NO_OBJECT_ID) {
		vec3 background = background_color(world_data, uv_viewport.y);
		if (object_outline == 0.0) {
			fragColor = vec4(background, 0.0);
		}
		else {
			fragColor = vec4(mix(world_data.object_outline_color.rgb, background, object_outline), 1.0-object_outline);
		}
		return;
	}
#endif /* !V3D_SHADING_OBJECT_OUTLINE */

	vec4 diffuse_color = texelFetch(colorBuffer, texel, 0);
/* Do we need normals */
#ifdef NORMAL_VIEWPORT_PASS_ENABLED
#ifdef WORKBENCH_ENCODE_NORMALS
	vec3 normal_viewport = normal_decode(texelFetch(normalBuffer, texel, 0).rg);
	if (diffuse_color.a == 1.0) {
		normal_viewport = -normal_viewport;
	}
#else /* WORKBENCH_ENCODE_NORMALS */
	vec3 normal_viewport = texelFetch(normalBuffer, texel, 0).rgb;
#endif /* WORKBENCH_ENCODE_NORMALS */
#endif


#ifdef V3D_LIGHTING_STUDIO
  #ifdef STUDIOLIGHT_ORIENTATION_CAMERA
	vec3 diffuse_light = get_camera_diffuse_light(world_data, normal_viewport);
  #endif

  #ifdef STUDIOLIGHT_ORIENTATION_WORLD
	vec3 normal_world = normalWorldMatrix * normal_viewport;
	vec3 diffuse_light = get_world_diffuse_light(world_data, normal_world);
  #endif

	vec3 specular_color = get_world_specular_light(world_data, normal_viewport, vec3(0.0, 0.0, 1.0));
	vec3 shaded_color = diffuse_light * diffuse_color.rgb + specular_color;

#else /* V3D_LIGHTING_STUDIO */
  #ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
	vec3 specular_color = get_world_specular_light(world_data, normal_viewport, vec3(0.0, 0.0, 1.0));
	vec3 shaded_color = diffuse_color.rgb + specular_color;

  #else /* V3D_SHADING_SPECULAR_HIGHLIGHT */
	vec3 shaded_color = diffuse_color.rgb;

  #endif /* V3D_SHADING_SPECULAR_HIGHLIGHT */

#endif /* V3D_LIGHTING_STUDIO */

#ifdef V3D_SHADING_SHADOW
	float shadow_mix = step(-shadowShift, dot(normal_viewport, world_data.light_direction_vs.xyz));
	float light_multiplier;
	light_multiplier = mix(lightMultiplier, shadowMultiplier, shadow_mix);

#else /* V3D_SHADING_SHADOW */
	float light_multiplier = 1.0;
#endif /* V3D_SHADING_SHADOW */

	shaded_color *= light_multiplier;

#ifdef V3D_SHADING_OBJECT_OUTLINE
	shaded_color = mix(world_data.object_outline_color.rgb, shaded_color, object_outline);
#endif /* V3D_SHADING_OBJECT_OUTLINE */
	fragColor = vec4(shaded_color, 1.0);
}
