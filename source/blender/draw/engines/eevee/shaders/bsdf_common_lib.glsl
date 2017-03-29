
#define M_PI        3.14159265358979323846  /* pi */
#define M_1_PI      0.318309886183790671538  /* 1/pi */

/* ------- Convenience functions --------- */

vec3 mul(mat3 m, vec3 v) { return m * v; }
mat3 mul(mat3 m1, mat3 m2) { return m1 * m2; }

float saturate(float a) { return clamp(a, 0.0, 1.0); }
vec2 saturate(vec2 a) { return vec2(saturate(a.x), saturate(a.y)); }
vec3 saturate(vec3 a) { return vec3(saturate(a.x), saturate(a.y), saturate(a.z)); }
vec4 saturate(vec4 a) { return vec4(saturate(a.x), saturate(a.y), saturate(a.z), saturate(a.w)); }

float distance_squared(vec2 a, vec2 b) { a -= b; return dot(a, a); }
float distance_squared(vec3 a, vec3 b) { a -= b; return dot(a, a); }

float hypot(float x, float y) { return sqrt(x*x + y*y); }

float inverse_distance(vec3 V) { return max( 1 / length(V), 1e-8); }

float line_plane_intersect_dist(vec3 lineorigin, vec3 linedirection, vec3 planeorigin, vec3 planenormal)
{
    return dot(planenormal, planeorigin - lineorigin) / dot(planenormal, linedirection);
}

vec3 line_plane_intersect(vec3 lineorigin, vec3 linedirection, vec3 planeorigin, vec3 planenormal)
{
	float dist = line_plane_intersect_dist(lineorigin, linedirection, planeorigin, planenormal);
	return lineorigin + linedirection * dist;
}

float rectangle_solid_angle(vec3 p0, vec3 p1, vec3 p2, vec3 p3)
{
	vec3 n0 = normalize(cross(p0, p1));
	vec3 n1 = normalize(cross(p1, p2));
	vec3 n2 = normalize(cross(p2, p3));
	vec3 n3 = normalize(cross(p3, p0));

	float g0 = acos(dot(-n0, n1));
	float g1 = acos(dot(-n1, n2));
	float g2 = acos(dot(-n2, n3));
	float g3 = acos(dot(-n3, n0));

	return max(0.0, (g0 + g1 + g2 + g3 - 2.0 * M_PI));
}


/* ------- Energy Conversion for lights ------- */
/* from Sebastien Lagarde
 * course_notes_moving_frostbite_to_pbr.pdf */

float sphere_energy(float radius)
{
	radius = max(radius, 1e-8);
	return 0.25 / (radius*radius * M_PI * M_PI) /* 1/(4*r²*Pi²) */
		* M_PI * M_PI * 10.0;  /* XXX : Empirical, Fit cycles power */
}

float rectangle_energy(float width, float height)
{
	return M_1_PI / (width*height) /* 1/(w*h*Pi) */
		* 20.0;  /* XXX : Empirical, Fit cycles power */
}

/* From UE4 paper */
void mrp_sphere(
        float radius, float dist, vec3 R, inout vec3 L,
        inout float roughness, inout float energy_conservation)
{
	L = dist * L;

	/* Sphere Light */
	roughness = max(3e-3, roughness); /* Artifacts appear with roughness below this threshold */

	/* energy preservation */
	float sphere_angle = saturate(radius / dist);
	energy_conservation *= pow(roughness / saturate(roughness + 0.5 * sphere_angle), 2.0);

	/* sphere light */
	float inter_dist = dot(L, R);
	vec3 closest_point_on_ray = inter_dist * R;
	vec3 center_to_ray = closest_point_on_ray - L;

	/* closest point on sphere */
	L = L + center_to_ray * saturate(radius * inverse_distance(center_to_ray));

	L = normalize(L);
}

void mrp_area(vec3 R, vec3 N, vec3 W, vec3 Lpos, vec3 Lx, vec3 Ly, vec3 Lz, float sizeX, float sizeY, float dist, inout vec3 L)
{
	vec3 refproj = line_plane_intersect(W, R, Lpos, Lz);
	vec3 norproj = line_plane_intersect(W, N, Lpos, Lz);

	vec2 area_half_size = vec2(sizeX, sizeY);

	/* Find the closest point to the rectangular light shape */
	vec3 refdir = refproj - Lpos;
	vec2 mrp = vec2(dot(refdir, Lx), dot(refdir, Ly));

	/* clamp to corners */
	mrp = clamp(mrp, -area_half_size, area_half_size);

	L = dist * L;
	L = L + mrp.x * Lx + mrp.y * Ly ;

	L = normalize(L);
}

/* GGX */
float D_ggx_opti(float NH, float a2)
{
	float tmp = (NH * a2 - NH) * NH + 1.0;
	return M_PI * tmp*tmp; /* Doing RCP and mul a2 at the end */
}

float G1_Smith_GGX(float NX, float a2)
{
	/* Using Brian Karis approach and refactoring by NX/NX
	 * this way the (2*NL)*(2*NV) in G = G1(V) * G1(L) gets canceled by the brdf denominator 4*NL*NV
	 * Rcp is done on the whole G later
	 * Note that this is not convenient for the transmition formula */
	return NX + sqrt( NX * (NX - NX * a2) + a2 );
	/* return 2 / (1 + sqrt(1 + a2 * (1 - NX*NX) / (NX*NX) ) ); /* Reference function */
}

float bsdf_ggx(vec3 N, vec3 L, vec3 V, float roughness)
{
	float a = roughness;
	float a2 = a * a;

	vec3 H = normalize(L + V);
	float NH = max(dot(N, H), 1e-8);
	float NL = max(dot(N, L), 1e-8);
	float NV = max(dot(N, V), 1e-8);

	float G = G1_Smith_GGX(NV, a2) * G1_Smith_GGX(NL, a2); /* Doing RCP at the end */
	float D = D_ggx_opti(NH, a2);

	/* Denominator is canceled by G1_Smith */
	/* bsdf = D * G / (4.0 * NL * NV); /* Reference function */
	return NL * a2 / (D * G); /* NL to Fit cycles Equation : line. 345 in bsdf_microfacet.h */
}
