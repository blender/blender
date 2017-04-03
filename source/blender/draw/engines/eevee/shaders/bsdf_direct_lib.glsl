/* Bsdf direct light function */
/* in other word, how materials react to scene lamps */

/* Naming convention
 * V       View vector (normalized)
 * N       World Normal (normalized)
 * L       Outgoing Light Vector (Surface to Light in World Space) (normalized)
 * Ldist   Distance from surface to the light
 * W       World Pos
 */

/* ------------ Diffuse ------------- */

float direct_diffuse_point(LightData ld, ShadingData sd)
{
	float bsdf = max(0.0, dot(sd.N, sd.L));
	bsdf /= sd.l_distance * sd.l_distance;
	return bsdf;
}

/* From Frostbite PBR Course
 * Analitical irradiance from a sphere with correct horizon handling
 * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf */
float direct_diffuse_sphere(LightData ld, ShadingData sd)
{
	float radius = max(ld.l_sizex, 0.0001);
	float costheta = clamp(dot(sd.N, sd.L), -0.999, 0.999);
	float h = min(ld.l_radius / sd.l_distance , 0.9999);
	float h2 = h*h;
	float costheta2 = costheta * costheta;
	float bsdf;

	if (costheta2 > h2) {
		bsdf = M_PI * h2 * clamp(costheta, 0.0, 1.0);
	}
	else {
		float sintheta = sqrt(1.0 - costheta2);
		float x = sqrt(1.0 / h2 - 1.0);
		float y = -x * (costheta / sintheta);
		float sinthetasqrty = sintheta * sqrt(1.0 - y * y);
		bsdf = (costheta * acos(y) - x * sinthetasqrty) * h2 + atan(sinthetasqrty / x);
	}

	bsdf = max(bsdf, 0.0);
	bsdf *= M_1_PI2;

	return bsdf;
}

/* From Frostbite PBR Course
 * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf */
float direct_diffuse_rectangle(LightData ld, ShadingData sd)
{
#ifdef USE_LTC
	float bsdf = ltc_evaluate(sd.N, sd.V, mat3(1.0), sd.area_data.corner);
	bsdf *= M_1_2PI;
#else
	float bsdf = sd.area_data.solid_angle * 0.2 * (
		max(0.0, dot(normalize(sd.area_data.corner[0]), sd.N)) +
		max(0.0, dot(normalize(sd.area_data.corner[1]), sd.N)) +
		max(0.0, dot(normalize(sd.area_data.corner[2]), sd.N)) +
		max(0.0, dot(normalize(sd.area_data.corner[3]), sd.N)) +
		max(0.0, dot(sd.L, sd.N))
	);
	bsdf *= M_1_PI;
#endif
	return bsdf;
}

/* infinitly far away point source, no decay */
float direct_diffuse_sun(LightData ld, ShadingData sd)
{
	float bsdf = max(0.0, dot(sd.N, sd.L));
	bsdf *= M_1_PI; /* Normalize */
	return bsdf;
}

#if 0
float direct_diffuse_unit_disc(vec3 N, vec3 L)
{

}
#endif

/* ----------- GGx ------------ */
float direct_ggx_point(ShadingData sd, float roughness)
{
	float bsdf = bsdf_ggx(sd.N, sd.L, sd.V, roughness);
	bsdf /= sd.l_distance * sd.l_distance;
	return bsdf;
}

float direct_ggx_sphere(LightData ld, ShadingData sd, float roughness)
{
#ifdef USE_LTC
	vec3 P = line_aligned_plane_intersect(vec3(0.0), sd.spec_dominant_dir, sd.l_vector);

	vec3 Px = normalize(P - sd.l_vector) * ld.l_radius;
	vec3 Py = cross(Px, sd.L);

	float NV = max(dot(sd.N, sd.V), 1e-8);
	vec2 uv = ltc_coords(NV, sqrt(roughness));
	mat3 ltcmat = ltc_matrix(uv);

// #define HIGHEST_QUALITY
#ifdef HIGHEST_QUALITY
	vec3 Pxy1 = normalize( Px + Py) * ld.l_radius;
	vec3 Pxy2 = normalize(-Px + Py) * ld.l_radius;

	/* counter clockwise */
	vec3 points[8];
	points[0] = sd.l_vector + Px;
	points[1] = sd.l_vector - Pxy2;
	points[2] = sd.l_vector - Py;
	points[3] = sd.l_vector - Pxy1;
	points[4] = sd.l_vector - Px;
	points[5] = sd.l_vector + Pxy2;
	points[6] = sd.l_vector + Py;
	points[7] = sd.l_vector + Pxy1;
	float bsdf = ltc_evaluate_circle(sd.N, sd.V, ltcmat, points);
#else
	vec3 points[4];
	points[0] = sd.l_vector + Px;
	points[1] = sd.l_vector - Py;
	points[2] = sd.l_vector - Px;
	points[3] = sd.l_vector + Py;
	float bsdf = ltc_evaluate(sd.N, sd.V, ltcmat, points);
	/* sqrt(pi/2) difference between square and disk area */
	bsdf *= 1.25331413731;
#endif

	bsdf *= texture(ltcMag, uv).r; /* Bsdf intensity */
	bsdf *= M_1_2PI * M_1_PI;
#else
	float energy_conservation;
	vec3 L = mrp_sphere(ld, sd, sd.spec_dominant_dir, roughness, energy_conservation);
	float bsdf = bsdf_ggx(sd.N, L, sd.V, roughness);

	bsdf *= energy_conservation / (sd.l_distance * sd.l_distance);
	bsdf *= max(ld.l_radius * ld.l_radius, 1e-16); /* radius is already inside energy_conservation */
#endif
	return bsdf;
}

float direct_ggx_rectangle(LightData ld, ShadingData sd, float roughness)
{
#ifdef USE_LTC
	float NV = max(dot(sd.N, sd.V), 1e-8);
	vec2 uv = ltc_coords(NV, sqrt(roughness));
	mat3 ltcmat = ltc_matrix(uv);

	float bsdf = ltc_evaluate(sd.N, sd.V, ltcmat, sd.area_data.corner);
	bsdf *= texture(ltcMag, uv).r; /* Bsdf intensity */
	bsdf *= M_1_2PI;
#else
	float energy_conservation;
	vec3 L = mrp_area(ld, sd, sd.spec_dominant_dir, roughness, energy_conservation);
	float bsdf = bsdf_ggx(sd.N, L, sd.V, roughness);

	bsdf *= energy_conservation;
	/* fade mrp artifacts */
	bsdf *= max(0.0, dot(-sd.spec_dominant_dir, ld.l_forward));
	bsdf *= max(0.0, -dot(L, ld.l_forward));
#endif
	return bsdf;
}

#if 0
float direct_ggx_disc(vec3 N, vec3 L)
{

}
#endif