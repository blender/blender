struct LightData {
	vec4 direction;
	vec4 specular_color;
	vec4 diffuse_color_wrap; /* rgb: diffuse col a: wrapped lighting factor */
};

struct WorldData {
	vec4 background_color_low;
	vec4 background_color_high;
	vec4 object_outline_color;
	vec4 shadow_direction_vs;
	LightData lights[4];
	vec4 ambient_color;
	int num_lights;
	int matcap_orientation;
	float background_alpha;
	float curvature_ridge;
	float curvature_valley;
	int pad[3];
};
