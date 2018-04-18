float lambert_diffuse(vec3 light_direction, vec3 surface_normal) {
	return max(0.0, dot(light_direction, surface_normal));
}
