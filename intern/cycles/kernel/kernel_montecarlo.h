/*
 * Parts adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __KERNEL_MONTECARLO_CL__
#define __KERNEL_MONTECARLO_CL__

CCL_NAMESPACE_BEGIN

/* distribute uniform xy on [0,1] over unit disk [-1,1] */
ccl_device void to_unit_disk(float *x, float *y)
{
	float phi = M_2PI_F * (*x);
	float r = sqrtf(*y);

	*x = r * cosf(phi);
	*y = r * sinf(phi);
}

/* return an orthogonal tangent and bitangent given a normal and tangent that
 * may not be exactly orthogonal */
ccl_device void make_orthonormals_tangent(const float3 N, const float3 T, float3 *a, float3 *b)
{
	*b = normalize(cross(N, T));
	*a = cross(*b, N);
}

/* sample direction with cosine weighted distributed in hemisphere */
ccl_device_inline void sample_cos_hemisphere(const float3 N,
	float randu, float randv, float3 *omega_in, float *pdf)
{
	to_unit_disk(&randu, &randv);
	float costheta = sqrtf(max(1.0f - randu * randu - randv * randv, 0.0f));
	float3 T, B;
	make_orthonormals(N, &T, &B);
	*omega_in = randu * T + randv * B + costheta * N;
	*pdf = costheta *M_1_PI_F;
}

/* sample direction uniformly distributed in hemisphere */
ccl_device_inline void sample_uniform_hemisphere(const float3 N,
                                                 float randu, float randv,
                                                 float3 *omega_in, float *pdf)
{
	float z = randu;
	float r = sqrtf(max(0.0f, 1.0f - z*z));
	float phi = M_2PI_F * randv;
	float x = r * cosf(phi);
	float y = r * sinf(phi);

	float3 T, B;
	make_orthonormals (N, &T, &B);
	*omega_in = x * T + y * B + z * N;
	*pdf = 0.5f * M_1_PI_F;
}

/* sample direction uniformly distributed in cone */
ccl_device_inline void sample_uniform_cone(const float3 N, float angle,
                                           float randu, float randv,
                                           float3 *omega_in, float *pdf)
{
	float z = cosf(angle*randu);
	float r = sqrtf(max(0.0f, 1.0f - z*z));
	float phi = M_2PI_F * randv;
	float x = r * cosf(phi);
	float y = r * sinf(phi);

	float3 T, B;
	make_orthonormals (N, &T, &B);
	*omega_in = x * T + y * B + z * N;
	*pdf = 0.5f * M_1_PI_F / (1.0f - cosf(angle));
}

/* sample uniform point on the surface of a sphere */
ccl_device float3 sample_uniform_sphere(float u1, float u2)
{
	float z = 1.0f - 2.0f*u1;
	float r = sqrtf(fmaxf(0.0f, 1.0f - z*z));
	float phi = M_2PI_F*u2;
	float x = r*cosf(phi);
	float y = r*sinf(phi);

	return make_float3(x, y, z);
}

ccl_device float balance_heuristic(float a, float b)
{
	return (a)/(a + b);
}

ccl_device float balance_heuristic_3(float a, float b, float c)
{
	return (a)/(a + b + c);
}

ccl_device float power_heuristic(float a, float b)
{
	return (a*a)/(a*a + b*b);
}

ccl_device float power_heuristic_3(float a, float b, float c)
{
	return (a*a)/(a*a + b*b + c*c);
}

ccl_device float max_heuristic(float a, float b)
{
	return (a > b)? 1.0f: 0.0f;
}

/* distribute uniform xy on [0,1] over unit disk [-1,1], with concentric mapping
 * to better preserve stratification for some RNG sequences */
ccl_device float2 concentric_sample_disk(float u1, float u2)
{
	float phi, r;
	float a = 2.0f*u1 - 1.0f;
	float b = 2.0f*u2 - 1.0f;

	if(a == 0.0f && b == 0.0f) {
		return make_float2(0.0f, 0.0f);
	}
	else if(a*a > b*b) {
		r = a;
		phi = M_PI_4_F * (b/a);
	}
	else {
		r = b;
		phi = M_PI_2_F - M_PI_4_F * (a/b);
	}

	return make_float2(r*cosf(phi), r*sinf(phi));
}

/* sample point in unit polygon with given number of corners and rotation */
ccl_device float2 regular_polygon_sample(float corners, float rotation, float u, float v)
{
	/* sample corner number and reuse u */
	float corner = floorf(u*corners);
	u = u*corners - corner;

	/* uniform sampled triangle weights */
	u = sqrtf(u);
	v = v*u;
	u = 1.0f - u;

	/* point in triangle */
	float angle = M_PI_F/corners;
	float2 p = make_float2((u + v)*cosf(angle), (u - v)*sinf(angle));

	/* rotate */
	rotation += corner*2.0f*angle;

	float cr = cosf(rotation);
	float sr = sinf(rotation);

	return make_float2(cr*p.x - sr*p.y, sr*p.x + cr*p.y);
}

ccl_device float3 ensure_valid_reflection(float3 Ng, float3 I, float3 N)
{
	float3 R = 2*dot(N, I)*N - I;
	if(dot(Ng, R) >= 0.05f) {
		return N;
	}

	/* Form coordinate system with Ng as the Z axis and N inside the X-Z-plane.
	 * The X axis is found by normalizing the component of N that's orthogonal to Ng.
	 * The Y axis isn't actually needed.
	 */
	float3 X = normalize(N - dot(N, Ng)*Ng);

	/* Calculate N.z and N.x in the local coordinate system. */
	float Iz = dot(I, Ng);
	float Ix2 = sqr(dot(I, X)), Iz2 = sqr(Iz);
	float Ix2Iz2 = Ix2 + Iz2;

	float a = sqrtf(Ix2*(Ix2Iz2 - sqr(0.05f)));
	float b = Iz*0.05f + Ix2Iz2;
	float c = (a + b > 0.0f)? (a + b) : (-a + b);

	float Nz = sqrtf(0.5f * c * (1.0f / Ix2Iz2));
	float Nx = sqrtf(1.0f - sqr(Nz));

	/* Transform back into global coordinates. */
	return Nx*X + Nz*Ng;
}

CCL_NAMESPACE_END

#endif /* __KERNEL_MONTECARLO_CL__ */
