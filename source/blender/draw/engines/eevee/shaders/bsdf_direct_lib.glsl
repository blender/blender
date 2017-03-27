/* Bsdf direct light function */
/* in other word, how materials react to scene lamps */

/* Naming convention
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
	bsdf *= M_1_PI; /* Normalize */
	return bsdf;
}
#if 0
float direct_diffuse_sphere(vec3 N, vec3 L)
{

}

float direct_diffuse_rectangle(vec3 N, vec3 L)
{

}
#endif

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

/* ----------- GGx ------------ */
float direct_ggx_point(vec3 N, vec3 L)
{
	float bsdf = max(0.0, dot(N, L));
	bsdf *= M_1_PI; /* Normalize */
	return bsdf;
}

float direct_ggx_sphere(vec3 N, vec3 L)
{

}

float direct_ggx_rectangle(vec3 N, vec3 L)
{

}

float direct_ggx_disc(vec3 N, vec3 L)
{

}
#endif