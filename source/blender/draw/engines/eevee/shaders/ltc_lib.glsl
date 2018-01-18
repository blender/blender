/**
 * Adapted from :
 * Real-Time Polygonal-Light Shading with Linearly Transformed Cosines.
 * Eric Heitz, Jonathan Dupuy, Stephen Hill and David Neubelt.
 * ACM Transactions on Graphics (Proceedings of ACM SIGGRAPH 2016) 35(4), 2016.
 * Project page: https://eheitzresearch.wordpress.com/415-2/
 **/

#define USE_LTC

#ifndef UTIL_TEX
#define UTIL_TEX
uniform sampler2DArray utilTex;
#define texelfetch_noise_tex(coord) texelFetch(utilTex, ivec3(ivec2(coord) % LUT_SIZE, 2.0), 0)
#endif /* UTIL_TEX */

/**
 * An extended version of the implementation from
 * "How to solve a cubic equation, revisited"
 * http://momentsingraphics.de/?p=105
 **/
vec3 solve_cubic(vec4 coefs)
{
	/* Normalize the polynomial */
	coefs.xyz /= coefs.w;
	/* Divide middle coefficients by three */
	coefs.yz /= 3.0;

	float A = coefs.w;
	float B = coefs.z;
	float C = coefs.y;
	float D = coefs.x;

	/* Compute the Hessian and the discriminant */
	vec3 delta = vec3(
		-coefs.z*coefs.z + coefs.y,
		-coefs.y*coefs.z + coefs.x,
		dot(vec2(coefs.z, -coefs.y), coefs.xy)
	);

	/* Discriminant */
	float discr = dot(vec2(4.0 * delta.x, -delta.y), delta.zy);

	vec2 xlc, xsc;

	/* Algorithm A */
	{
		float A_a = 1.0;
		float C_a = delta.x;
		float D_a = -2.0 * B * delta.x + delta.y;

		/* Take the cubic root of a normalized complex number */
		float theta = atan(sqrt(discr), -D_a) / 3.0;

		float x_1a = 2.0 * sqrt(-C_a) * cos(theta);
		float x_3a = 2.0 * sqrt(-C_a) * cos(theta + (2.0 / 3.0) * M_PI);

		float xl;
		if ((x_1a + x_3a) > 2.0 * B) {
			xl = x_1a;
		}
		else {
			xl = x_3a;
		}

		xlc = vec2(xl - B, A);
	}

	/* Algorithm D */
	{
		float A_d = D;
		float C_d = delta.z;
		float D_d = -D * delta.y + 2.0 * C * delta.z;

		/* Take the cubic root of a normalized complex number */
		float theta = atan(D * sqrt(discr), -D_d) / 3.0;

		float x_1d = 2.0 * sqrt(-C_d) * cos(theta);
		float x_3d = 2.0 * sqrt(-C_d) * cos(theta + (2.0 / 3.0) * M_PI);

		float xs;
		if (x_1d + x_3d < 2.0 * C)
			xs = x_1d;
		else
			xs = x_3d;

		xsc = vec2(-D, xs + C);
	}

	float E =  xlc.y * xsc.y;
	float F = -xlc.x * xsc.y - xlc.y * xsc.x;
	float G =  xlc.x * xsc.x;

	vec2 xmc = vec2(C * F - B * G, -B * F + C * E);

	vec3 root = vec3(xsc.x / xsc.y,
	                 xmc.x / xmc.y,
	                 xlc.x / xlc.y);

	if (root.x < root.y && root.x < root.z) {
		root.xyz = root.yxz;
	}
	else if (root.z < root.x && root.z < root.y) {
		root.xyz = root.xzy;
	}

	return root;
}

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

mat3 ltc_matrix(vec4 lut)
{
	/* load inverse matrix */
	mat3 Minv = mat3(
		vec3(  1,   0, lut.y),
		vec3(  0, lut.z,   0),
		vec3(lut.w,   0, lut.x)
	);

	return Minv;
}

float ltc_evaluate(vec3 N, vec3 V, mat3 Minv, vec3 corners[4])
{
	/* Avoid dot(N, V) == 1 in ortho mode, leading T1 normalize to fail. */
	V = normalize(V + 1e-8);

	/* construct orthonormal basis around N */
	vec3 T1, T2;
	T1 = normalize(V - N * dot(N, V));
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

float diffuseSphereIntegralCheap(vec3 F, float l)
{
	return max((l*l + F.z) / (l+1.0), 0.0);
}

/* disk_points are WS vectors from the shading point to the disk "bounding domain" */
float ltc_evaluate_disk(vec3 N, vec3 V, mat3 Minv, vec3 disk_points[3])
{
	/* Avoid dot(N, V) == 1 in ortho mode, leading T1 normalize to fail. */
	V = normalize(V + 1e-8);

	/* construct orthonormal basis around N */
	vec3 T1, T2;
	T1 = normalize(V - N * dot(V, N));
	T2 = cross(N, T1);

	/* rotate area light in (T1, T2, R) basis */
	mat3 R = transpose(mat3(T1, T2, N));

	/* Intermediate step: init ellipse. */
	vec3 L_[3];
	L_[0] = mul(R, disk_points[0]);
	L_[1] = mul(R, disk_points[1]);
	L_[2] = mul(R, disk_points[2]);

	vec3 C  = 0.5 * (L_[0] + L_[2]);
	vec3 V1 = 0.5 * (L_[1] - L_[2]);
	vec3 V2 = 0.5 * (L_[1] - L_[0]);

	/* Transform ellipse by Minv. */
	C  = Minv * C;
	V1 = Minv * V1;
	V2 = Minv * V2;

	/* Compute eigenvectors of new ellipse. */

	float d11 = dot(V1, V1);
	float d22 = dot(V2, V2);
	float d12 = dot(V1, V2);
	float a, b; /* Eigenvalues */
	const float threshold = 0.0007; /* Can be adjusted. Fix artifacts. */
	if (abs(d12) / sqrt(d11 * d22) > threshold) {
		float tr = d11 + d22;
		float det = -d12 * d12 + d11 * d22;

		/* use sqrt matrix to solve for eigenvalues */
		det = sqrt(det);
		float u = 0.5 * sqrt(tr - 2.0 * det);
		float v = 0.5 * sqrt(tr + 2.0 * det);
		float e_max = (u + v);
		float e_min = (u - v);
		e_max *= e_max;
		e_min *= e_min;

		vec3 V1_, V2_;
		if (d11 > d22) {
			V1_ = d12 * V1 + (e_max - d11) * V2;
			V2_ = d12 * V1 + (e_min - d11) * V2;
		}
		else {
			V1_ = d12 * V2 + (e_max - d22) * V1;
			V2_ = d12 * V2 + (e_min - d22) * V1;
		}

		a = 1.0 / e_max;
		b = 1.0 / e_min;
		V1 = normalize(V1_);
		V2 = normalize(V2_);
	}
	else {
		a = 1.0 / d11;
		b = 1.0 / d22;
		V1 *= sqrt(a);
		V2 *= sqrt(b);
	}

	/* Now find front facing ellipse with same solid angle. */

	vec3 V3 = normalize(cross(V1, V2));
	if (dot(C, V3) < 0.0)
		V3 *= -1.0;

	float L  = dot(V3, C);
	float x0 = dot(V1, C) / L;
	float y0 = dot(V2, C) / L;

	a *= L*L;
	b *= L*L;

	float c0 = a * b;
	float c1 = a * b * (1.0 + x0 * x0 + y0 * y0) - a - b;
	float c2 = 1.0 - a * (1.0 + x0 * x0) - b * (1.0 + y0 * y0);
	float c3 = 1.0;

	vec3 roots = solve_cubic(vec4(c0, c1, c2, c3));
	float e1 = roots.x;
	float e2 = roots.y;
	float e3 = roots.z;

	vec3 avgDir = vec3(a * x0 / (a - e2), b * y0 / (b - e2), 1.0);

	mat3 rotate = mat3(V1, V2, V3);

	avgDir = rotate * avgDir;
	avgDir = normalize(avgDir);

	/* L1, L2 are the extends of the front facing ellipse. */
	float L1 = sqrt(-e2/e3);
	float L2 = sqrt(-e2/e1);

	/* Find the sphere and compute lighting. */
	float formFactor = L1 * L2 * inversesqrt((1.0 + L1 * L1) * (1.0 + L2 * L2));

	/* use tabulated horizon-clipped sphere */
	vec2 uv = vec2(avgDir.z * 0.5 + 0.5, formFactor);
	uv = uv * (64.0 - 1.0) / 64.0 + 0.5 / 64.0;

	float sphere_cosine_integral = formFactor * texture(utilTex, vec3(uv, 1.0)).w;
	/* Less accurate version, a bit cheaper. */
	//float sphere_cosine_integral = formFactor * diffuseSphereIntegralCheap(avgDir, formFactor);

	return max(0.0, sphere_cosine_integral);
}

