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

float direct_diffuse_point(vec3 N, vec4 l_vector)
{
	float dist = l_vector.w;
	vec3 L = l_vector.xyz / dist;
	float bsdf = max(0.0, dot(N, L));
	bsdf /= dist * dist;
	return bsdf;
}

/* infinitly far away point source, no decay */
float direct_diffuse_sun(LightData ld, vec3 N)
{
	float bsdf = max(0.0, dot(N, -ld.l_forward));
	bsdf *= M_1_PI; /* Normalize */
	return bsdf;
}

/* From Frostbite PBR Course
 * Analytical irradiance from a sphere with correct horizon handling
 * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf */
float direct_diffuse_sphere(LightData ld, vec3 N, vec4 l_vector)
{
	float dist = l_vector.w;
	vec3 L = l_vector.xyz / dist;
	float radius = max(ld.l_sizex, 0.0001);
	float costheta = clamp(dot(N, L), -0.999, 0.999);
	float h = min(ld.l_radius / dist , 0.9999);
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

#ifdef USE_LTC
float direct_diffuse_rectangle(LightData ld, vec3 N, vec3 V, vec4 l_vector)
{
	vec3 corners[4];
	corners[0] = l_vector.xyz + ld.l_right * -ld.l_sizex + ld.l_up *  ld.l_sizey;
	corners[1] = l_vector.xyz + ld.l_right * -ld.l_sizex + ld.l_up * -ld.l_sizey;
	corners[2] = l_vector.xyz + ld.l_right *  ld.l_sizex + ld.l_up * -ld.l_sizey;
	corners[3] = l_vector.xyz + ld.l_right *  ld.l_sizex + ld.l_up *  ld.l_sizey;

	return ltc_evaluate_quad_diffuse(corners);
}
#endif

#if 0
float direct_diffuse_unit_disc(vec3 N, vec3 L)
{

}
#endif

/* ----------- GGx ------------ */
vec3 direct_ggx_point(vec3 N, vec3 V, vec4 l_vector, float roughness, vec3 f0)
{
	roughness = max(1e-3, roughness);
	float dist = l_vector.w;
	vec3 L = l_vector.xyz / dist;
	float bsdf = bsdf_ggx(N, L, V, roughness);
	bsdf /= dist * dist;

	/* Fresnel */
	float VH = max(dot(V, normalize(V + L)), 0.0);
	return F_schlick(f0, VH) * bsdf;
}

vec3 direct_ggx_sun(LightData ld, vec3 N, vec3 V, float roughness, vec3 f0)
{
	roughness = max(1e-3, roughness);
	float bsdf = bsdf_ggx(N, -ld.l_forward, V, roughness);
	float VH = dot(V, -ld.l_forward) * 0.5 + 0.5;
	return F_schlick(f0, VH) * bsdf;
}

#ifdef USE_LTC
vec3 direct_ggx_sphere(LightData ld, vec3 N, vec3 V, vec4 l_vector, float roughness, vec3 f0)
{
	roughness = clamp(roughness, 0.0008, 0.999); /* Fix low roughness artifacts. */

	vec2 uv = lut_coords(dot(N, V), sqrt(roughness));
	vec3 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rgb;
	vec4 ltc_lut = texture(utilTex, vec3(uv, 0.0)).rgba;
	mat3 ltc_mat = ltc_matrix(ltc_lut);

	/* Make orthonormal basis. */
	vec3 L = l_vector.xyz / l_vector.w;
	vec3 Px, Py;
	make_orthonormal_basis(L, Px, Py);
	Px *= ld.l_radius;
	Py *= ld.l_radius;

	vec3 points[3];
	points[0] = l_vector.xyz - Px - Py;
	points[1] = l_vector.xyz + Px - Py;
	points[2] = l_vector.xyz + Px + Py;

	float bsdf = ltc_evaluate_disk(N, V, ltc_mat, points);
	bsdf *= brdf_lut.b; /* Bsdf intensity */
	bsdf *= M_1_PI;

	vec3 spec = F_area(f0, brdf_lut.xy) * bsdf;

	return spec;
}

vec3 direct_ggx_rectangle(LightData ld, vec3 N, vec3 V, vec4 l_vector, float roughness, vec3 f0)
{
	vec3 corners[4];
	corners[0] = l_vector.xyz + ld.l_right * -ld.l_sizex + ld.l_up *  ld.l_sizey;
	corners[1] = l_vector.xyz + ld.l_right * -ld.l_sizex + ld.l_up * -ld.l_sizey;
	corners[2] = l_vector.xyz + ld.l_right *  ld.l_sizex + ld.l_up * -ld.l_sizey;
	corners[3] = l_vector.xyz + ld.l_right *  ld.l_sizex + ld.l_up *  ld.l_sizey;

	vec2 uv = lut_coords(dot(N, V), sqrt(roughness));
	vec3 brdf_lut = texture(utilTex, vec3(uv, 1.0)).rgb;
	vec4 ltc_lut = texture(utilTex, vec3(uv, 0.0)).rgba;
	mat3 ltc_mat = ltc_matrix(ltc_lut);

	float bsdf = ltc_evaluate_quad(N, V, ltc_mat, corners);
	bsdf *= brdf_lut.b; /* Bsdf intensity */

	vec3 spec = F_area(f0, brdf_lut.xy) * bsdf;

	return spec;
}
#endif

#if 0
float direct_ggx_disc(vec3 N, vec3 L)
{

}
#endif