/*
 * Copyright 2011-2017 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_MATH_INTERSECT_H__
#define __UTIL_MATH_INTERSECT_H__

CCL_NAMESPACE_BEGIN

/* Ray Intersection */

ccl_device bool ray_sphere_intersect(
	float3 ray_P, float3 ray_D, float ray_t,
	float3 sphere_P, float sphere_radius,
	float3 *isect_P, float *isect_t)
{
	float3 d = sphere_P - ray_P;
	float radiussq = sphere_radius*sphere_radius;
	float tsq = dot(d, d);

	if(tsq > radiussq) { /* ray origin outside sphere */
		float tp = dot(d, ray_D);

		if(tp < 0.0f) /* dir points away from sphere */
			return false;

		float dsq = tsq - tp*tp; /* pythagoras */

		if(dsq > radiussq) /* closest point on ray outside sphere */
			return false;

		float t = tp - sqrtf(radiussq - dsq); /* pythagoras */

		if(t < ray_t) {
			*isect_t = t;
			*isect_P = ray_P + ray_D*t;
			return true;
		}
	}

	return false;
}

ccl_device bool ray_aligned_disk_intersect(
	float3 ray_P, float3 ray_D, float ray_t,
	float3 disk_P, float disk_radius,
	float3 *isect_P, float *isect_t)
{
	/* aligned disk normal */
	float disk_t;
	float3 disk_N = normalize_len(ray_P - disk_P, &disk_t);
	float div = dot(ray_D, disk_N);

	if(UNLIKELY(div == 0.0f))
		return false;

	/* compute t to intersection point */
	float t = -disk_t/div;
	if(t < 0.0f || t > ray_t)
		return false;
	
	/* test if within radius */
	float3 P = ray_P + ray_D*t;
	if(len_squared(P - disk_P) > disk_radius*disk_radius)
		return false;

	*isect_P = P;
	*isect_t = t;

	return true;
}

ccl_device_inline bool ray_triangle_intersect_uv(
        float3 ray_P, float3 ray_D, float ray_t,
        float3 v0, float3 v1, float3 v2,
        float *isect_u, float *isect_v, float *isect_t)
{
	/* Calculate intersection */
	float3 e1 = v1 - v0;
	float3 e2 = v2 - v0;
	float3 s1 = cross(ray_D, e2);

	const float divisor = dot(s1, e1);
	if(UNLIKELY(divisor == 0.0f))
		return false;

	const float invdivisor = 1.0f/divisor;

	/* compute first barycentric coordinate */
	const float3 d = ray_P - v0;
	const float u = dot(d, s1)*invdivisor;
	if(u < 0.0f)
		return false;

	/* Compute second barycentric coordinate */
	const float3 s2 = cross(d, e1);
	const float v = dot(ray_D, s2)*invdivisor;
	if(v < 0.0f)
		return false;

	const float b0 = 1.0f - u - v;
	if(b0 < 0.0f)
		return false;

	/* compute t to intersection point */
	const float t = dot(e2, s2)*invdivisor;
	if(t < 0.0f || t > ray_t)
		return false;

	*isect_u = u;
	*isect_v = v;
	*isect_t = t;

	return true;
}

ccl_device bool ray_quad_intersect(float3 ray_P, float3 ray_D, float ray_mint, float ray_maxt,
                                   float3 quad_P, float3 quad_u, float3 quad_v, float3 quad_n,
                                   float3 *isect_P, float *isect_t, float *isect_u, float *isect_v)
{
	float t = -(dot(ray_P, quad_n) - dot(quad_P, quad_n)) / dot(ray_D, quad_n);
	if(t < ray_mint || t > ray_maxt)
		return false;

	float3 hit = ray_P + t*ray_D;
	float3 inplane = hit - quad_P;

	float u = dot(inplane, quad_u) / dot(quad_u, quad_u) + 0.5f;
	if(u < 0.0f || u > 1.0f)
		return false;

	float v = dot(inplane, quad_v) / dot(quad_v, quad_v) + 0.5f;
	if(v < 0.0f || v > 1.0f)
		return false;

	if(isect_P) *isect_P = hit;
	if(isect_t) *isect_t = t;
	if(isect_u) *isect_u = u;
	if(isect_v) *isect_v = v;

	return true;
}

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_INTERSECT_H__ */
