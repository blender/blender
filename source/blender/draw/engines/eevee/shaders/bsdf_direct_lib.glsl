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
	bsdf *= M_1_PI * M_1_PI;

	return bsdf;
}

/* From Frostbite PBR Course
 * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf */
float direct_diffuse_rectangle(LightData ld, ShadingData sd)
{
	float bsdf = sd.area_data.solid_angle * 0.2 * (
		max(0.0, dot(normalize(sd.area_data.corner[0]), sd.N)) +
		max(0.0, dot(normalize(sd.area_data.corner[1]), sd.N)) +
		max(0.0, dot(normalize(sd.area_data.corner[2]), sd.N)) +
		max(0.0, dot(normalize(sd.area_data.corner[3]), sd.N)) +
		max(0.0, dot(sd.L, sd.N))
	);

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
	float energy_conservation;
	vec3 L = mrp_sphere(ld, sd, sd.spec_dominant_dir, roughness, energy_conservation);
	float bsdf = bsdf_ggx(sd.N, L, sd.V, roughness);

	bsdf *= energy_conservation / (sd.l_distance * sd.l_distance);
	bsdf *= max(ld.l_radius * ld.l_radius, 1e-16); /* radius is already inside energy_conservation */

	return bsdf;
}

float direct_ggx_rectangle(LightData ld, ShadingData sd, float roughness)
{
	vec3 L = mrp_area(ld, sd, sd.spec_dominant_dir);

	float bsdf = bsdf_ggx(sd.N, L, sd.V, roughness) * sd.area_data.solid_angle;

	bsdf *= max(0.0, dot(-sd.spec_dominant_dir, ld.l_forward)); /* fade mrp artifacts */

	return bsdf;
}

#if 0
float direct_ggx_disc(vec3 N, vec3 L)
{

}
#endif