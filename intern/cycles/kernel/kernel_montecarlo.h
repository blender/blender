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

/// Given values x and y on [0,1], convert them in place to values on
/// [-1,1] uniformly distributed over a unit sphere.  This code is
/// derived from Peter Shirley, "Realistic Ray Tracing", p. 103.
__device void to_unit_disk(float *x, float *y)
{
	float r, phi;
	float a = 2.0f * (*x) - 1.0f;
	float b = 2.0f * (*y) - 1.0f;
	if(a > -b) {
		if(a > b) {
			r = a;
			 phi = M_PI_4_F *(b/a);
		 } else {
			 r = b;
			 phi = M_PI_4_F *(2.0f - a/b);
		 }
	} else {
		if(a < b) {
			r = -a;
			phi = M_PI_4_F *(4.0f + b/a);
		} else {
			r = -b;
			if(b != 0.0f)
				phi = M_PI_4_F *(6.0f - a/b);
			else
				phi = 0.0f;
		}
	}
	*x = r * cosf(phi);
	*y = r * sinf(phi);
}

__device void make_orthonormals_tangent(const float3 N, const float3 T, float3 *a, float3 *b)
{
	*b = cross(N, T);
	*a = cross(*b, N);
}

__device_inline void sample_cos_hemisphere(const float3 N,
	float randu, float randv, float3 *omega_in, float *pdf)
{
	// Default closure BSDF implementation: uniformly sample
	// cosine-weighted hemisphere above the point.
	to_unit_disk(&randu, &randv);
	float costheta = sqrtf(max(1.0f - randu * randu - randv * randv, 0.0f));
	float3 T, B;
	make_orthonormals(N, &T, &B);
	*omega_in = randu * T + randv * B + costheta * N;
	*pdf = costheta *M_1_PI_F;
}

__device_inline void sample_uniform_hemisphere(const float3 N,
											 float randu, float randv,
											 float3 *omega_in, float *pdf)
{
	float z = randu;
	float r = sqrtf(max(0.f, 1.f - z*z));
	float phi = 2.f * M_PI_F * randv;
	float x = r * cosf(phi);
	float y = r * sinf(phi);

	float3 T, B;
	make_orthonormals (N, &T, &B);
	*omega_in = x * T + y * B + z * N;
	*pdf = 0.5f * M_1_PI_F;
}

__device float3 sample_uniform_sphere(float u1, float u2)
{
	float z = 1.0f - 2.0f*u1;
	float r = sqrtf(fmaxf(0.0f, 1.0f - z*z));
	float phi = 2.0f*M_PI_F*u2;
	float x = r*cosf(phi);
	float y = r*sinf(phi);

	return make_float3(x, y, z);
}

__device float power_heuristic(float a, float b)
{
	return (a*a)/(a*a + b*b);
}

__device float2 concentric_sample_disk(float u1, float u2)
{
	float r, theta;
	// Map uniform random numbers to $[-1,1]^2$
	float sx = 2 * u1 - 1;
	float sy = 2 * u2 - 1;

	// Map square to $(r,\theta)$

	// Handle degeneracy at the origin
	if(sx == 0.0f && sy == 0.0f) {
		return make_float2(0.0f, 0.0f);
	}
	if(sx >= -sy) {
		if(sx > sy) {
			// Handle first region of disk
			r = sx;
			if(sy > 0.0f) theta = sy/r;
			else		  theta = 8.0f + sy/r;
		}
		else {
			// Handle second region of disk
			r = sy;
			theta = 2.0f - sx/r;
		}
	}
	else {
		if(sx <= sy) {
			// Handle third region of disk
			r = -sx;
			theta = 4.0f - sy/r;
		}
		else {
			// Handle fourth region of disk
			r = -sy;
			theta = 6.0f + sx/r;
		}
	}

	theta *= M_PI_4_F;
	return make_float2(r * cosf(theta), r * sinf(theta));
}

__device float2 regular_polygon_sample(float corners, float rotation, float u, float v)
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

/* Spherical coordinates <-> Cartesion direction  */

__device float2 direction_to_spherical(float3 dir)
{
	float theta = acosf(dir.z);
	float phi = atan2f(dir.x, dir.y);

	return make_float2(theta, phi);
}

__device float3 spherical_to_direction(float theta, float phi)
{
	return make_float3(
		sinf(theta)*cosf(phi),
		sinf(theta)*sinf(phi),
		cosf(theta));
}

/* Equirectangular */

__device float2 direction_to_equirectangular(float3 dir)
{
	float u = (atan2f(dir.y, dir.x) + M_PI_F)/(2.0f*M_PI_F);
	float v = atan2f(dir.z, hypotf(dir.x, dir.y))/M_PI_F + 0.5f;

	return make_float2(u, v);
}

__device float3 equirectangular_to_direction(float u, float v)
{
	/* XXX check correctness? */
	float theta = M_PI_F*v;
	float phi = 2.0f*M_PI_F*u;

	return make_float3(
		sin(theta)*cos(phi),
		sin(theta)*sin(phi),
		cos(theta));
}

CCL_NAMESPACE_END

#endif /* __KERNEL_MONTECARLO_CL__ */

