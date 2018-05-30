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
	return result.xyz;
}

/* N And I are in View Space. */
vec3 get_world_specular_light(WorldData world_data, vec3 N, vec3 I)
{
#ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
	vec3 reflection_vector = reflect(I, N);
	vec3 specular_light = vec3(1.0);
	/* Simple frontal specular highlights. */
	float specular_influence = pow(max(0.0, dot(world_data.light_direction_vs.xyz, reflection_vector)), world_data.specular_sharpness);
	vec3 specular_color = specular_light * specular_influence;

#else /* V3D_SHADING_SPECULAR_HIGHLIGHT */
	vec3 specular_color = vec3(0.0);
#endif /* V3D_SHADING_SPECULAR_HIGHLIGHT */
	return specular_color;
}
