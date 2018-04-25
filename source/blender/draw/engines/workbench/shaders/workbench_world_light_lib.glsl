vec3 get_world_diffuse_light(WorldData world_data, vec3 N)
{
	vec4 result = world_data.diffuse_light_xp * clamp(N.x, 0.0, 1.0);
	result = mix(result, world_data.diffuse_light_xn, clamp(-N.x, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_yp, clamp( N.y, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_yn, clamp(-N.y, 0.0, 1.0));
	result = mix(result, world_data.diffuse_light_zp, clamp( N.z, 0.0, 1.0));
	return mix(result, world_data.diffuse_light_zn, clamp(-N.z, 0.0, 1.0)).xyz;
}
