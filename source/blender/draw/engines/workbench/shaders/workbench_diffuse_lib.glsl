float normalized_dot(vec3 v1, vec3 v2)
{
	return max(0.0, dot(v1, v2));
}

float lambert_diffuse(vec3 light_direction, vec3 surface_normal)
{
	return normalized_dot(light_direction, surface_normal);
}

vec3 get_world_diffuse_light(vec3 N, vec3 xp, vec3 xn, vec3 yp, vec3 yn, vec3 zp, vec3 zn)
{
	vec3 result = vec3(0.0, 0.0, 0.0);
	result = mix(result, xp, normalized_dot(vec3( 1.0,  0.0,  0.0), N));
	result = mix(result, xn, normalized_dot(vec3(-1.0,  0.0,  0.0), N));
	result = mix(result, yp, normalized_dot(vec3( 0.0,  1.0,  0.0), N));
	result = mix(result, yn, normalized_dot(vec3( 0.0, -1.0,  0.0), N));
	result = mix(result, zp, normalized_dot(vec3( 0.0,  0.0,  1.0), N));
	result = mix(result, zn, normalized_dot(vec3( 0.0,  0.0, -1.0), N));
	return result;
}
