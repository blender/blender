vec3 get_world_diffuse_light(WorldData world_data, vec3 N)
{
	vec4 result = world_data.diffuse_light_x_pos * clamp(N.x, 0.0, 1.0);
	result = mix(result, world_data.diffuse_light_x_neg, clamp(-N.x, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_y_pos, clamp( N.y, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_y_neg, clamp(-N.y, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_z_pos, clamp( N.z, 0.0, 1.0));
	return mix(result, world_data.diffuse_light_z_neg, clamp(-N.z, 0.0, 1.0)).xyz;
}
