struct LightData {
	vec4 light_direction_vs;
	vec4 specular_color;
};

struct WorldData {
	vec3 spherical_harmonics_coefs[STUDIOLIGHT_SPHERICAL_HARMONICS_MAX_COMPONENTS];
	vec4 background_color_low;
	vec4 background_color_high;
	vec4 object_outline_color;
	vec4 shadow_direction_vs;
	LightData lights[3];
	int num_lights;
	int matcap_orientation;
	float background_alpha;
	int pad[1];
};
