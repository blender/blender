#define BLINN

vec3 get_world_diffuse_light(WorldData world_data, vec3 N)
{
	vec4 result = world_data.diffuse_light_x_pos * clamp(N.x, 0.0, 1.0);
	result = mix(result, world_data.diffuse_light_x_neg, clamp(-N.x, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_y_pos, clamp(-N.y, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_y_neg, clamp(N.y, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_z_pos, clamp(N.z, 0.0, 1.0));
	return mix(result, world_data.diffuse_light_z_neg, clamp(-N.z, 0.0, 1.0)).xyz;
}

vec3 get_camera_diffuse_light(WorldData world_data, vec3 N)
{
	vec4 result = world_data.diffuse_light_x_pos * clamp(N.x, 0.0, 1.0);
	result = mix(result, world_data.diffuse_light_x_neg, clamp(-N.x, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_z_pos, clamp( N.y, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_z_neg, clamp(-N.y, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_y_pos, clamp( N.z, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_y_neg, clamp(-N.z, 0.0, 1.0));
	return result.rgb;
}

/* N And I are in View Space. */
vec3 get_world_specular_light(vec4 specular_data, LightData light_data, vec3 N, vec3 I)
{
#ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
	vec3 specular_light = specular_data.rgb * light_data.specular_color.rgb * light_data.specular_color.a;

	float shininess = exp2(10*(1.0-specular_data.a) + 1);

#  ifdef BLINN
	float normalization_factor = (shininess + 8) / (8 * M_PI);
	vec3 L = -light_data.light_direction_vs.xyz;
	vec3 halfDir = normalize(L + I);
	float specAngle = max(dot(halfDir, N), 0.0);
	float NL = max(dot(L, N), 0.0);
	float specular_influence = pow(specAngle, shininess) * NL  * normalization_factor;

#  else
	vec3 reflection_vector = reflect(I, N);
	float specAngle = max(dot(light_data.light_direction_vs.xyz, reflection_vector), 0.0);
	float specular_influence = pow(specAngle, shininess);
#  endif

	vec3 specular_color = specular_light * specular_influence;

#else /* V3D_SHADING_SPECULAR_HIGHLIGHT */
	vec3 specular_color = vec3(0.0);
#endif /* V3D_SHADING_SPECULAR_HIGHLIGHT */
	return specular_color;
}

vec3 get_world_specular_lights(WorldData world_data, vec4 specular_data, vec3 N, vec3 I)
{
	vec3 specular_light = vec3(0.0);
	specular_light += get_world_specular_light(specular_data, world_data.lights[0], N, I);
	for (int i = 0 ; i < world_data.num_lights ; i ++) {
		specular_light += get_world_specular_light(specular_data, world_data.lights[i], N, I);
	}
	return specular_light;
}

