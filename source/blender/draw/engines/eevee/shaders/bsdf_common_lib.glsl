
#define M_PI        3.14159265358979323846  /* pi */
#define M_1_PI      0.318309886183790671538  /* 1/pi */
#define M_1_2PI     0.159154943091895335768  /* 1/(2*pi) */
#define M_1_PI2     0.101321183642337771443  /* 1/(pi^2) */

/* ------- Structures -------- */

struct LightData {
	vec4 position_influence;     /* w : InfluenceRadius */
	vec4 color_spec;          /* w : Spec Intensity */
	vec4 spotdata_shadow;  /* x : spot size, y : spot blend */
	vec4 rightvec_sizex;         /* xyz: Normalized up vector, w: Lamp Type */
	vec4 upvec_sizey;      /* xyz: Normalized right vector, w: Lamp Type */
	vec4 forwardvec_type;     /* xyz: Normalized forward vector, w: Lamp Type */
};

/* convenience aliases */
#define l_color        color_spec.rgb
#define l_spec         color_spec.a
#define l_position     position_influence.xyz
#define l_influence    position_influence.w
#define l_sizex        rightvec_sizex.w
#define l_radius       rightvec_sizex.w
#define l_sizey        upvec_sizey.w
#define l_right        rightvec_sizex.xyz
#define l_up           upvec_sizey.xyz
#define l_forward      forwardvec_type.xyz
#define l_type         forwardvec_type.w
#define l_spot_size    spotdata_shadow.x
#define l_spot_blend   spotdata_shadow.y

struct AreaData {
	vec3 corner[4];
	float solid_angle;
};

struct ShadingData {
	vec3 V; /* View vector */
	vec3 N; /* World Normal of the fragment */
	vec3 W; /* World Position of the fragment */
	vec3 R; /* Reflection vector */
	vec3 L; /* Current Light vector (normalized) */
	vec3 spec_dominant_dir; /* dominant direction of the specular rays */
	vec3 l_vector; /* Current Light vector */
	float l_distance; /* distance(l_position, W) */
	AreaData area_data; /* If current light is an area light */
};

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

float line_aligned_plane_intersect_dist(vec3 lineorigin, vec3 linedirection, vec3 planeorigin)
{
	/* aligned plane normal */
	vec3 L = planeorigin - lineorigin;
	float diskdist = length(L);
	vec3 planenormal = -normalize(L);
	return -diskdist / dot(planenormal, linedirection);
}

vec3 line_aligned_plane_intersect(vec3 lineorigin, vec3 linedirection, vec3 planeorigin)
{
	float dist = line_aligned_plane_intersect_dist(lineorigin, linedirection, planeorigin);
	if (dist < 0) {
		/* if intersection is behind we fake the intersection to be
		 * really far and (hopefully) not inside the radius of interest */
		dist = 1e16;
	}
	return lineorigin + linedirection * dist;
}

float rectangle_solid_angle(AreaData ad)
{
	vec3 n0 = normalize(cross(ad.corner[0], ad.corner[1]));
	vec3 n1 = normalize(cross(ad.corner[1], ad.corner[2]));
	vec3 n2 = normalize(cross(ad.corner[2], ad.corner[3]));
	vec3 n3 = normalize(cross(ad.corner[3], ad.corner[0]));

	float g0 = acos(dot(-n0, n1));
	float g1 = acos(dot(-n1, n2));
	float g2 = acos(dot(-n2, n3));
	float g3 = acos(dot(-n3, n0));

	return max(0.0, (g0 + g1 + g2 + g3 - 2.0 * M_PI));
}

vec3 get_specular_dominant_dir(vec3 N, vec3 R, float roughness)
{
	return normalize(mix(N, R, 1.0 - roughness * roughness));
}

/* From UE4 paper */
vec3 mrp_sphere(LightData ld, ShadingData sd, vec3 dir, inout float roughness, out float energy_conservation)
{
	roughness = max(3e-3, roughness); /* Artifacts appear with roughness below this threshold */

	/* energy preservation */
	float sphere_angle = saturate(ld.l_radius / sd.l_distance);
	energy_conservation = pow(roughness / saturate(roughness + 0.5 * sphere_angle), 2.0);

	/* sphere light */
	float inter_dist = dot(sd.l_vector, dir);
	vec3 closest_point_on_ray = inter_dist * dir;
	vec3 center_to_ray = closest_point_on_ray - sd.l_vector;

	/* closest point on sphere */
	vec3 closest_point_on_sphere = sd.l_vector + center_to_ray * saturate(ld.l_radius * inverse_distance(center_to_ray));

	return normalize(closest_point_on_sphere);
}

vec3 mrp_area(LightData ld, ShadingData sd, vec3 dir, inout float roughness, out float energy_conservation)
{
	roughness = max(3e-3, roughness); /* Artifacts appear with roughness below this threshold */

	/* FIXME : This needs to be fixed */
	energy_conservation = pow(roughness / saturate(roughness + 0.5 * sd.area_data.solid_angle), 2.0);

	vec3 refproj = line_plane_intersect(sd.W, dir, ld.l_position, ld.l_forward);

	/* Project the point onto the light plane */
	vec3 refdir = refproj - ld.l_position;
	vec2 mrp = vec2(dot(refdir, ld.l_right), dot(refdir, ld.l_up));

	/* clamp to light shape bounds */
	vec2 area_half_size = vec2(ld.l_sizex, ld.l_sizey);
	mrp = clamp(mrp, -area_half_size, area_half_size);

	/* go back in world space */
	vec3 closest_point_on_rectangle = sd.l_vector + mrp.x * ld.l_right + mrp.y * ld.l_up;

	float len = length(closest_point_on_rectangle);
	energy_conservation /= len * len;

	return closest_point_on_rectangle / len;
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
	return NX + sqrt(NX * (NX - NX * a2) + a2);
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
