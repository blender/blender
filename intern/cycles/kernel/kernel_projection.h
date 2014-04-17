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

#ifndef __KERNEL_PROJECTION_CL__
#define __KERNEL_PROJECTION_CL__

CCL_NAMESPACE_BEGIN

/* Spherical coordinates <-> Cartesian direction  */

ccl_device float2 direction_to_spherical(float3 dir)
{
	float theta = safe_acosf(dir.z);
	float phi = atan2f(dir.x, dir.y);

	return make_float2(theta, phi);
}

ccl_device float3 spherical_to_direction(float theta, float phi)
{
	return make_float3(
		sinf(theta)*cosf(phi),
		sinf(theta)*sinf(phi),
		cosf(theta));
}

/* Equirectangular coordinates <-> Cartesian direction */

ccl_device float2 direction_to_equirectangular(float3 dir)
{
	float u = -atan2f(dir.y, dir.x)/(M_2PI_F) + 0.5f;
	float v = atan2f(dir.z, hypotf(dir.x, dir.y))/M_PI_F + 0.5f;

	return make_float2(u, v);
}

ccl_device float3 equirectangular_to_direction(float u, float v)
{
	float phi = M_PI_F*(1.0f - 2.0f*u);
	float theta = M_PI_F*(1.0f - v);

	return make_float3(
		sinf(theta)*cosf(phi),
		sinf(theta)*sinf(phi),
		cosf(theta));
}

/* Fisheye <-> Cartesian direction */

ccl_device float2 direction_to_fisheye(float3 dir, float fov)
{
	float r = atan2f(sqrtf(dir.y*dir.y +  dir.z*dir.z), dir.x) / fov;
	float phi = atan2f(dir.z, dir.y);

	float u = r * cosf(phi) + 0.5f;
	float v = r * sinf(phi) + 0.5f;

	return make_float2(u, v);
}

ccl_device float3 fisheye_to_direction(float u, float v, float fov)
{
	u = (u - 0.5f) * 2.0f;
	v = (v - 0.5f) * 2.0f;

	float r = sqrtf(u*u + v*v);

	if(r > 1.0f)
		return make_float3(0.0f, 0.0f, 0.0f);

	float phi = safe_acosf((r != 0.0f)? u/r: 0.0f);
	float theta = r * fov * 0.5f;

	if(v < 0.0f) phi = -phi;

	return make_float3(
		 cosf(theta),
		 -cosf(phi)*sinf(theta),
		 sinf(phi)*sinf(theta)
	);
}

ccl_device float2 direction_to_fisheye_equisolid(float3 dir, float lens, float width, float height)
{
	float theta = safe_acosf(dir.x);
	float r = 2.0f * lens * sinf(theta * 0.5f);
	float phi = atan2f(dir.z, dir.y);

	float u = r * cosf(phi) / width + 0.5f;
	float v = r * sinf(phi) / height + 0.5f;

	return make_float2(u, v);
}

ccl_device float3 fisheye_equisolid_to_direction(float u, float v, float lens, float fov, float width, float height)
{
	u = (u - 0.5f) * width;
	v = (v - 0.5f) * height;

	float rmax = 2.0f * lens * sinf(fov * 0.25f);
	float r = sqrtf(u*u + v*v);

	if(r > rmax)
		return make_float3(0.0f, 0.0f, 0.0f);

	float phi = safe_acosf((r != 0.0f)? u/r: 0.0f);
	float theta = 2.0f * asinf(r/(2.0f * lens));

	if(v < 0.0f) phi = -phi;

	return make_float3(
		 cosf(theta),
		 -cosf(phi)*sinf(theta),
		 sinf(phi)*sinf(theta)
	);
}

/* Mirror Ball <-> Cartesion direction */

ccl_device float3 mirrorball_to_direction(float u, float v)
{
	/* point on sphere */
	float3 dir;

	dir.x = 2.0f*u - 1.0f;
	dir.z = 2.0f*v - 1.0f;
	dir.y = -sqrtf(max(1.0f - dir.x*dir.x - dir.z*dir.z, 0.0f));

	/* reflection */
	float3 I = make_float3(0.0f, -1.0f, 0.0f);

	return 2.0f*dot(dir, I)*dir - I;
}

ccl_device float2 direction_to_mirrorball(float3 dir)
{
	/* inverse of mirrorball_to_direction */
	dir.y -= 1.0f;

	float div = 2.0f*sqrtf(max(-0.5f*dir.y, 0.0f));
	if(div > 0.0f)
		dir /= div;

	float u = 0.5f*(dir.x + 1.0f);
	float v = 0.5f*(dir.z + 1.0f);

	return make_float2(u, v);
}

ccl_device float3 panorama_to_direction(KernelGlobals *kg, float u, float v)
{
	switch(kernel_data.cam.panorama_type) {
		case PANORAMA_EQUIRECTANGULAR:
			return equirectangular_to_direction(u, v);
		case PANORAMA_FISHEYE_EQUIDISTANT:
			return fisheye_to_direction(u, v, kernel_data.cam.fisheye_fov);
		case PANORAMA_FISHEYE_EQUISOLID:
		default:
			return fisheye_equisolid_to_direction(u, v, kernel_data.cam.fisheye_lens,
				kernel_data.cam.fisheye_fov, kernel_data.cam.sensorwidth, kernel_data.cam.sensorheight);
	}
}

ccl_device float2 direction_to_panorama(KernelGlobals *kg, float3 dir)
{
	switch(kernel_data.cam.panorama_type) {
		case PANORAMA_EQUIRECTANGULAR:
			return direction_to_equirectangular(dir);
		case PANORAMA_FISHEYE_EQUIDISTANT:
			return direction_to_fisheye(dir, kernel_data.cam.fisheye_fov);
		case PANORAMA_FISHEYE_EQUISOLID:
		default:
			return direction_to_fisheye_equisolid(dir, kernel_data.cam.fisheye_lens,
				kernel_data.cam.sensorwidth, kernel_data.cam.sensorheight);
	}
}

CCL_NAMESPACE_END

#endif /* __KERNEL_PROJECTION_CL__ */

