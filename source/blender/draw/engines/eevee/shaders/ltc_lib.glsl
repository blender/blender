/* Mainly From https://eheitzresearch.wordpress.com/415-2/ */

#define USE_LTC
#define LTC_LUT_SIZE 64

uniform sampler2D ltcMat;
uniform sampler2D ltcMag;

/* from Real-Time Area Lighting: a Journey from Research to Production
 * Stephen Hill and Eric Heitz */
float edge_integral(vec3 p1, vec3 p2)
{
#if 0
	/* more accurate replacement of acos */
	float x = dot(p1, p2);
	float y = abs(x);

	float a = 5.42031 + (3.12829 + 0.0902326 * y) * y;
	float b = 3.45068 + (4.18814 + y) * y;
	float theta_sintheta = a / b;

	if (x < 0.0) {
		theta_sintheta = (M_PI / sqrt(1.0 - x * x)) - theta_sintheta;
	}
	vec3 u = cross(p1, p2);
	return theta_sintheta * dot(u, N);
#endif
	float cos_theta = dot(p1, p2);
	cos_theta = clamp(cos_theta, -0.9999, 0.9999);

	float theta = acos(cos_theta);
	vec3 u = normalize(cross(p1, p2));
	return theta * cross(p1, p2).z / sin(theta);
}

int clip_quad_to_horizon(inout vec3 L[5])
{
	/* detect clipping config */
	int config = 0;
	if (L[0].z > 0.0) config += 1;
	if (L[1].z > 0.0) config += 2;
	if (L[2].z > 0.0) config += 4;
	if (L[3].z > 0.0) config += 8;

	/* clip */
	int n = 0;

	if (config == 0)
	{
		/* clip all */
	}
	else if (config == 1) /* V1 clip V2 V3 V4 */
	{
		n = 3;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[3].z * L[0] + L[0].z * L[3];
	}
	else if (config == 2) /* V2 clip V1 V3 V4 */
	{
		n = 3;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	}
	else if (config == 3) /* V1 V2 clip V3 V4 */
	{
		n = 4;
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
		L[3] = -L[3].z * L[0] + L[0].z * L[3];
	}
	else if (config == 4) /* V3 clip V1 V2 V4 */
	{
		n = 3;
		L[0] = -L[3].z * L[2] + L[2].z * L[3];
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
	}
	else if (config == 5) /* V1 V3 clip V2 V4) impossible */
	{
		n = 0;
	}
	else if (config == 6) /* V2 V3 clip V1 V4 */
	{
		n = 4;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	}
	else if (config == 7) /* V1 V2 V3 clip V4 */
	{
		n = 5;
		L[4] = -L[3].z * L[0] + L[0].z * L[3];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	}
	else if (config == 8) /* V4 clip V1 V2 V3 */
	{
		n = 3;
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
		L[1] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] =  L[3];
	}
	else if (config == 9) /* V1 V4 clip V2 V3 */
	{
		n = 4;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[2].z * L[3] + L[3].z * L[2];
	}
	else if (config == 10) /* V2 V4 clip V1 V3) impossible */
	{
		n = 0;
	}
	else if (config == 11) /* V1 V2 V4 clip V3 */
	{
		n = 5;
		L[4] = L[3];
		L[3] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	}
	else if (config == 12) /* V3 V4 clip V1 V2 */
	{
		n = 4;
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
	}
	else if (config == 13) /* V1 V3 V4 clip V2 */
	{
		n = 5;
		L[4] = L[3];
		L[3] = L[2];
		L[2] = -L[1].z * L[2] + L[2].z * L[1];
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
	}
	else if (config == 14) /* V2 V3 V4 clip V1 */
	{
		n = 5;
		L[4] = -L[0].z * L[3] + L[3].z * L[0];
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
	}
	else if (config == 15) /* V1 V2 V3 V4 */
	{
		n = 4;
	}

	if (n == 3)
		L[3] = L[0];
	if (n == 4)
		L[4] = L[0];

	return n;
}

vec2 ltc_coords(float cosTheta, float roughness)
{
	float theta = acos(cosTheta);
	vec2 coords = vec2(roughness, theta/(0.5*3.14159));

	/* scale and bias coordinates, for correct filtered lookup */
	return coords * (LTC_LUT_SIZE - 1.0) / LTC_LUT_SIZE + 0.5 / LTC_LUT_SIZE;
}

mat3 ltc_matrix(vec2 coord)
{
	/* load inverse matrix */
	vec4 t = texture(ltcMat, coord);
	mat3 Minv = mat3(
		vec3(  1,   0, t.y),
		vec3(  0, t.z,   0),
		vec3(t.w,   0, t.x)
	);

	return Minv;
}

float ltc_evaluate(vec3 N, vec3 V, mat3 Minv, vec3 corners[4])
{
	/* construct orthonormal basis around N */
	vec3 T1, T2;
	T1 = normalize(V - N*dot(V, N));
	T2 = cross(N, T1);

	/* rotate area light in (T1, T2, R) basis */
	Minv = Minv * transpose(mat3(T1, T2, N));

	/* polygon (allocate 5 vertices for clipping) */
	vec3 L[5];
	L[0] = Minv * corners[0];
	L[1] = Minv * corners[1];
	L[2] = Minv * corners[2];
	L[3] = Minv * corners[3];

	int n = clip_quad_to_horizon(L);

	if (n == 0)
		return 0.0;

	/* project onto sphere */
	L[0] = normalize(L[0]);
	L[1] = normalize(L[1]);
	L[2] = normalize(L[2]);
	L[3] = normalize(L[3]);
	L[4] = normalize(L[4]);

	/* integrate */
	float sum = 0.0;

	sum += edge_integral(L[0], L[1]);
	sum += edge_integral(L[1], L[2]);
	sum += edge_integral(L[2], L[3]);
	if (n >= 4)
		sum += edge_integral(L[3], L[4]);
	if (n == 5)
		sum += edge_integral(L[4], L[0]);

	return abs(sum);
}

/* Aproximate circle with an octogone */
#define LTC_CIRCLE_RES 8
float ltc_evaluate_circle(vec3 N, vec3 V, mat3 Minv, vec3 p[LTC_CIRCLE_RES])
{
	/* construct orthonormal basis around N */
	vec3 T1, T2;
	T1 = normalize(V - N*dot(V, N));
	T2 = cross(N, T1);

	/* rotate area light in (T1, T2, R) basis */
	Minv = Minv * transpose(mat3(T1, T2, N));

	for (int i = 0; i < LTC_CIRCLE_RES; ++i) {
		p[i] = Minv * p[i];
		/* clip to horizon */
		p[i].z = max(0.0, p[i].z);
		/* project onto sphere */
		p[i] = normalize(p[i]);
	}

	/* integrate */
	float sum = 0.0;
	for (int i = 0; i < LTC_CIRCLE_RES - 1; ++i) {
		sum += edge_integral(p[i], p[i + 1]);
	}
	sum += edge_integral(p[LTC_CIRCLE_RES - 1], p[0]);

	return max(0.0, sum);
}

