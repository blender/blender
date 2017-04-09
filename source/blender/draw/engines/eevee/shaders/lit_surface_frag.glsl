
uniform int light_count;
uniform vec3 cameraPos;
uniform vec3 eye;
uniform mat4 ProjectionMatrix;


layout(std140) uniform light_block {
	LightData   lights_data[MAX_LIGHT];
};

in vec3 worldPosition;
in vec3 worldNormal;

out vec4 fragColor;

/* type */
#define POINT    0.0
#define SUN      1.0
#define SPOT     2.0
#define HEMI     3.0
#define AREA     4.0

float light_diffuse(LightData ld, ShadingData sd)
{
	if (ld.l_type == SUN) {
		return direct_diffuse_sun(ld, sd);
	}
	else if (ld.l_type == AREA) {
		return direct_diffuse_rectangle(ld, sd);
	}
	else {
		return direct_diffuse_sphere(ld, sd);
	}
}

float light_specular(LightData ld, ShadingData sd, float roughness)
{
	if (ld.l_type == SUN) {
		return direct_ggx_point(sd, roughness);
	}
	else if (ld.l_type == AREA) {
		return direct_ggx_rectangle(ld, sd, roughness);
	}
	else {
		// return direct_ggx_point(sd, roughness);
		return direct_ggx_sphere(ld, sd, roughness);
	}
}

float light_visibility(LightData ld, ShadingData sd)
{
	float vis = 1.0;

	if (ld.l_type == SPOT) {
		float z = dot(ld.l_forward, sd.l_vector);
		vec3 lL = sd.l_vector / z;
		float x = dot(ld.l_right, lL) / ld.l_sizex;
		float y = dot(ld.l_up, lL) / ld.l_sizey;

		float ellipse = 1.0 / sqrt(1.0 + x * x + y * y);

		float spotmask = smoothstep(0.0, 1.0, (ellipse - ld.l_spot_size) / ld.l_spot_blend);

		vis *= spotmask;
	}
	else if (ld.l_type == AREA) {
		vis *= step(0.0, -dot(sd.L, ld.l_forward));
	}

	return vis;
}

/* Calculation common to all bsdfs */
float light_common(inout LightData ld, inout ShadingData sd)
{
	float vis = 1.0;

	if (ld.l_type == SUN) {
		sd.L = -ld.l_forward;
	}
	else {
		sd.L = sd.l_vector / sd.l_distance;
	}

	if (ld.l_type == AREA) {
		sd.area_data.corner[0] = sd.l_vector + ld.l_right * -ld.l_sizex + ld.l_up *  ld.l_sizey;
		sd.area_data.corner[1] = sd.l_vector + ld.l_right * -ld.l_sizex + ld.l_up * -ld.l_sizey;
		sd.area_data.corner[2] = sd.l_vector + ld.l_right *  ld.l_sizex + ld.l_up * -ld.l_sizey;
		sd.area_data.corner[3] = sd.l_vector + ld.l_right *  ld.l_sizex + ld.l_up *  ld.l_sizey;
#ifndef USE_LTC
		sd.area_data.solid_angle = rectangle_solid_angle(sd.area_data);
#endif
	}

	return vis;
}

void main()
{
	ShadingData sd;
	sd.N = normalize(worldNormal);
	sd.V = (ProjectionMatrix[3][3] == 0.0) /* if perspective */
	            ? normalize(cameraPos - worldPosition)
	            : normalize(eye);
	sd.W = worldPosition;
	sd.R = reflect(-sd.V, sd.N);

	/* hardcoded test vars */
	vec3 albedo = vec3(0.0);
	vec3 specular = mix(vec3(1.0), vec3(1.0), pow(max(0.0, 1.0 - dot(sd.N, sd.V)), 5.0));
	float roughness = 0.1;

	sd.spec_dominant_dir = get_specular_dominant_dir(sd.N, sd.R, roughness);

	vec3 radiance = vec3(0.0);
	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		LightData ld = lights_data[i];

		sd.l_vector = ld.l_position - worldPosition;
		sd.l_distance = length(sd.l_vector);

		light_common(ld, sd);

		float vis = light_visibility(ld, sd);
		float spec = light_specular(ld, sd, roughness);
		float diff = light_diffuse(ld, sd);

		radiance += vis * (albedo * diff + specular * spec) * ld.l_color;
	}

	fragColor = vec4(radiance, 1.0);
}