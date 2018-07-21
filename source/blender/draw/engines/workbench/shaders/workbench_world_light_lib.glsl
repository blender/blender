#define BLINN

vec3 spherical_harmonics(vec3 N, vec3 spherical_harmonics_coefs[STUDIOLIGHT_SPHERICAL_HARMONICS_MAX_COMPONENTS])
{
	vec3 sh = vec3(0.0);

	sh += 0.282095 * spherical_harmonics_coefs[0];

#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL > 0
	float nx = N.x;
	float ny = N.y;
	float nz = N.z;
	sh += -0.488603 * nz * spherical_harmonics_coefs[1];
	sh += 0.488603 * ny * spherical_harmonics_coefs[2];
	sh += -0.488603 * nx * spherical_harmonics_coefs[3];
#endif

#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL > 1
	float nx2 = nx * nx;
	float ny2 = ny * ny;
	float nz2 = nz * nz;

	sh += 1.092548 * nx * nz * spherical_harmonics_coefs[4];
	sh += -1.092548 * nz * ny * spherical_harmonics_coefs[5];
	sh += 0.315392 * (3.0 * ny2 - 1.0) * spherical_harmonics_coefs[6];
	sh += -1.092548 * nx * ny * spherical_harmonics_coefs[7];
	sh += 0.546274 * (nx2 - nz2) * spherical_harmonics_coefs[8];
#endif

#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL > 3
	float nx4 = nx2 * nx2;
	float ny4 = ny2 * ny2;
	float nz4 = nz2 * nz2;

	sh += (2.5033429417967046 * nx * nz * (nx2 - nz2)) * spherical_harmonics_coefs[9];
	sh += (-1.7701307697799304 * nz * ny * (3.0 * nx2 - nz2)) * spherical_harmonics_coefs[10];
	sh += (0.9461746957575601 * nz * nx * (-1.0 +7.0*ny2)) * spherical_harmonics_coefs[11];
	sh += (-0.6690465435572892 * nz * ny * (-3.0 + 7.0 * ny2)) * spherical_harmonics_coefs[12];
	sh += ((105.0*ny4-90.0*ny2+9.0)/28.359261614) * spherical_harmonics_coefs[13];
	sh += (-0.6690465435572892 * nx * ny * (-3.0 + 7.0 * ny2)) * spherical_harmonics_coefs[14];
	sh += (0.9461746957575601 * (nx2 - nz2) * (-1.0 + 7.0 * ny2)) * spherical_harmonics_coefs[15];
	sh += (-1.7701307697799304 * nx * ny * (nx2 - 3.0 * nz2)) * spherical_harmonics_coefs[16];
	sh += (0.6258357354491761 * (nx4 - 6.0 * nz2 * nx2 + nz4)) * spherical_harmonics_coefs[17];
#endif

	return sh;
}

vec3 get_world_diffuse_light(WorldData world_data, vec3 N)
{
	return (spherical_harmonics(vec3(N.x, N.y, -N.z), world_data.spherical_harmonics_coefs));
}

vec3 get_camera_diffuse_light(WorldData world_data, vec3 N)
{
	return (spherical_harmonics(vec3(N.x, -N.z, -N.y), world_data.spherical_harmonics_coefs));
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
	for (int i = 0 ; i < world_data.num_lights ; i ++) {
		specular_light += get_world_specular_light(specular_data, world_data.lights[i], N, I);
	}
	return specular_light;
}
