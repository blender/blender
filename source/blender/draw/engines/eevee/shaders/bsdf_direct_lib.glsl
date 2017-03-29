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

float direct_diffuse_point(vec3 N, vec3 L, float Ldist)
{
	float bsdf = max(0.0, dot(N, L));
	bsdf /= Ldist * Ldist;
	bsdf *= M_PI / 2.0; /* Normalize */
	return bsdf;
}

/* From Frostbite PBR Course
 * Analitical irradiance from a sphere with correct horizon handling
 * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf */
float direct_diffuse_sphere(vec3 N, vec3 L, float Ldist, float radius)
{
	radius = max(radius, 0.0001);
	float costheta = clamp(dot(N, L), -0.999, 0.999);
	float h = min(radius / Ldist , 0.9999);
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

	/* Energy conservation + cycle matching */
	bsdf = max(bsdf, 0.0);
	bsdf *= M_1_PI;
	bsdf *= sphere_energy(radius);

	return bsdf;
}

/* From Frostbite PBR Course
 * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf */
float direct_diffuse_rectangle(
        vec3 W, vec3 N, vec3 L,
        float Ldist, vec3 Lx, vec3 Ly, vec3 Lz, float Lsizex, float Lsizey)
{
	vec3 lco = L * Ldist;

	/* Surface to corner vectors */
	vec3 p0 = lco + Lx * -Lsizex + Ly *  Lsizey;
	vec3 p1 = lco + Lx * -Lsizex + Ly * -Lsizey;
	vec3 p2 = lco + Lx *  Lsizex + Ly * -Lsizey;
	vec3 p3 = lco + Lx *  Lsizex + Ly *  Lsizey;

	float solidAngle = rectangle_solid_angle(p0, p1, p2, p3);

	float bsdf = solidAngle * 0.2 * (
		max(0.0, dot(normalize(p0), N)) +
		max(0.0, dot(normalize(p1), N)) +
		max(0.0, dot(normalize(p2), N)) +
		max(0.0, dot(normalize(p3), N)) +
		max(0.0, dot(L, N))
	);

	bsdf *= rectangle_energy(Lsizex * 2.0, Lsizey * 2.0);

	return bsdf;
}

/* infinitly far away point source, no decay */
float direct_diffuse_sun(vec3 N, vec3 L)
{
	float bsdf = max(0.0, dot(N, L));
	bsdf *= M_1_PI; /* Normalize */
	return bsdf;
}

#if 0
float direct_diffuse_unit_disc(vec3 N, vec3 L)
{

}
#endif

/* ----------- GGx ------------ */
float direct_ggx_point(vec3 N, vec3 L, vec3 V, float roughness)
{
	return bsdf_ggx(N, L, V, roughness);
}

float direct_ggx_sphere(vec3 N, vec3 L, vec3 V, float Ldist, float radius, float roughness)
{
	vec3 R = reflect(V, N);

	float energy_conservation = 1.0;
	mrp_sphere(radius, Ldist, R, L, roughness, energy_conservation);
	float bsdf = bsdf_ggx(N, L, V, roughness);

	bsdf *= energy_conservation / (Ldist * Ldist);
	bsdf *= sphere_energy(radius) * max(radius * radius, 1e-16); /* radius is already inside energy_conservation */
	bsdf *= M_PI;

	return bsdf;
}

float direct_ggx_rectangle(
        vec3 W, vec3 N, vec3 L, vec3 V,
        float Ldist, vec3 Lx, vec3 Ly, vec3 Lz, float Lsizex, float Lsizey, float roughness)
{
	vec3 lco = L * Ldist;

	/* Surface to corner vectors */
	vec3 p0 = lco + Lx * -Lsizex + Ly *  Lsizey;
	vec3 p1 = lco + Lx * -Lsizex + Ly * -Lsizey;
	vec3 p2 = lco + Lx *  Lsizex + Ly * -Lsizey;
	vec3 p3 = lco + Lx *  Lsizex + Ly *  Lsizey;

	float solidAngle = rectangle_solid_angle(p0, p1, p2, p3);

	vec3 R = reflect(V, N);
	mrp_area(R, N, W, W + lco, Lx, Ly, Lz, Lsizex, Lsizey, Ldist, L);

	float bsdf = bsdf_ggx(N, L, V, roughness) * solidAngle;

	bsdf *= pow(max(0.0, dot(R, Lz)), 0.5); /* fade mrp artifacts */
	bsdf *= rectangle_energy(Lsizex * 2.0, Lsizey * 2.0);

	return bsdf;
}

#if 0
float direct_ggx_disc(vec3 N, vec3 L)
{

}
#endif