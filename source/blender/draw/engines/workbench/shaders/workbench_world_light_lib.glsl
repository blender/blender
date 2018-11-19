#define BLINN

#if STUDIOLIGHT_SH_BANDS == 2
vec3 spherical_harmonics(vec3 N, vec3 sh_coefs[STUDIOLIGHT_SH_MAX_COMPONENTS])
{
	/* http://www.geomerics.com/wp-content/uploads/2015/08/CEDEC_Geomerics_ReconstructingDiffuseLighting1.pdf */
	/* Highly optimized form, precompute as much as we can. */
	/**
	 * R1 = 0.5 * vec3(L3.r, L2.r, L1.r);
	 * sh_coefs[0..2] = R1 / length(R1);
	 **/
	vec3 q;
	q.x = dot(sh_coefs[1], N);
	q.y = dot(sh_coefs[2], N);
	q.z = dot(sh_coefs[3], N);
	q = 0.5 * q + 0.5;

	/**
	 * R0 = L0.r;
	 * lr1_r0 = lenR1 / R0;
	 * p = 1.0 + 2.0 * lr1_r0;
	 * a = (1.0 - lr1_r0) / (1.0 + lr1_r0);
	 * return R0 * (a + (1.0 - a) * (p + 1.0) * pow(q, p));
	 *
	 * sh_coefs[4] = p;
	 * sh_coefs[5] = R0 * a;
	 * sh_coefs[0] = R0 * (1.0 - a) * (p + 1.0);
	 **/
	q = pow(q, sh_coefs[4]);
	return sh_coefs[0] * q + sh_coefs[5];
}

#else

vec3 spherical_harmonics(vec3 N, vec3 sh_coefs[STUDIOLIGHT_SH_MAX_COMPONENTS])
{
	vec3 sh = 0.282095 * sh_coefs[0];

#  if STUDIOLIGHT_SH_BANDS > 1
	float nx = N.x;
	float ny = N.y;
	float nz = N.z;
	sh += -0.488603 * nz * sh_coefs[1];
	sh += 0.488603 * ny * sh_coefs[2];
	sh += -0.488603 * nx * sh_coefs[3];
#  endif
#  if STUDIOLIGHT_SH_BANDS > 2
	float nx2 = nx * nx;
	float ny2 = ny * ny;
	float nz2 = nz * nz;
	sh += 1.092548 * nx * nz * sh_coefs[4];
	sh += -1.092548 * nz * ny * sh_coefs[5];
	sh += 0.315392 * (3.0 * ny2 - 1.0) * sh_coefs[6];
	sh += -1.092548 * nx * ny * sh_coefs[7];
	sh += 0.546274 * (nx2 - nz2) * sh_coefs[8];
#  endif
#  if STUDIOLIGHT_SH_BANDS > 4
	float nx4 = nx2 * nx2;
	float ny4 = ny2 * ny2;
	float nz4 = nz2 * nz2;
	sh += (2.5033429417967046 * nx * nz * (nx2 - nz2)) * sh_coefs[9];
	sh += (-1.7701307697799304 * nz * ny * (3.0 * nx2 - nz2)) * sh_coefs[10];
	sh += (0.9461746957575601 * nz * nx * (-1.0 +7.0*ny2)) * sh_coefs[11];
	sh += (-0.6690465435572892 * nz * ny * (-3.0 + 7.0 * ny2)) * sh_coefs[12];
	sh += ((105.0*ny4-90.0*ny2+9.0)/28.359261614) * sh_coefs[13];
	sh += (-0.6690465435572892 * nx * ny * (-3.0 + 7.0 * ny2)) * sh_coefs[14];
	sh += (0.9461746957575601 * (nx2 - nz2) * (-1.0 + 7.0 * ny2)) * sh_coefs[15];
	sh += (-1.7701307697799304 * nx * ny * (nx2 - 3.0 * nz2)) * sh_coefs[16];
	sh += (0.6258357354491761 * (nx4 - 6.0 * nz2 * nx2 + nz4)) * sh_coefs[17];
#  endif
	return sh;
}
#endif

vec3 get_world_diffuse_light(WorldData world_data, vec3 N)
{
	return spherical_harmonics(N, world_data.spherical_harmonics_coefs);
}

vec3 get_camera_diffuse_light(WorldData world_data, vec3 N)
{
	return spherical_harmonics(vec3(N.x, -N.z, N.y), world_data.spherical_harmonics_coefs);
}

/* N And I are in View Space. */
vec3 get_world_specular_light(vec4 specular_data, LightData light_data, vec3 N, vec3 I)
{
#ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
	vec3 specular_light = specular_data.rgb * light_data.specular_color.rgb * light_data.specular_color.a;

	float shininess = exp2(10.0 * (1.0 - specular_data.a) + 1);

#  ifdef BLINN
	float normalization_factor = (shininess + 8.0) / (8.0 * M_PI);
	vec3 L = -light_data.light_direction_vs.xyz;
	vec3 halfDir = normalize(L + I);
	float spec_angle = max(dot(halfDir, N), 0.0);
	float NL = max(dot(L, N), 0.0);
	float specular_influence = pow(spec_angle, shininess) * NL  * normalization_factor;

#  else
	vec3 reflection_vector = reflect(I, N);
	float spec_angle = max(dot(light_data.light_direction_vs.xyz, reflection_vector), 0.0);
	float specular_influence = pow(spec_angle, shininess);
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
	/* Manual loop unrolling provide much better perf. */
	if (world_data.num_lights > 0) {
		specular_light += get_world_specular_light(specular_data, world_data.lights[0], N, I);
	}
	if (world_data.num_lights > 1) {
		specular_light += get_world_specular_light(specular_data, world_data.lights[1], N, I);
	}
	if (world_data.num_lights > 2) {
		specular_light += get_world_specular_light(specular_data, world_data.lights[2], N, I);
	}
	return specular_light;
}
