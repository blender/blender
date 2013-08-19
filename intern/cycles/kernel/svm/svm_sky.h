/*
 * Copyright 2011-2013 Blender Foundation
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
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

/*
 * "A Practical Analytic Model for Daylight"
 * A. J. Preetham, Peter Shirley, Brian Smits
 */

__device float sky_angle_between(float thetav, float phiv, float theta, float phi)
{
	float cospsi = sinf(thetav)*sinf(theta)*cosf(phi - phiv) + cosf(thetav)*cosf(theta);
	return safe_acosf(cospsi);
}

__device float sky_perez_function(__constant float *lam, float theta, float gamma)
{
	float ctheta = cosf(theta);
	float cgamma = cosf(gamma);

	return (1.0f + lam[0]*expf(lam[1]/ctheta)) * (1.0f + lam[2]*expf(lam[3]*gamma)  + lam[4]*cgamma*cgamma);
}

__device float3 sky_radiance(KernelGlobals *kg, float3 dir)
{
	/* convert vector to spherical coordinates */
	float2 spherical = direction_to_spherical(dir);
	float theta = spherical.x;
	float phi = spherical.y;

	/* angle between sun direction and dir */
	float gamma = sky_angle_between(theta, phi, kernel_data.sunsky.theta, kernel_data.sunsky.phi);

	/* clamp theta to horizon */
	theta = min(theta, M_PI_2_F - 0.001f);

	/* compute xyY color space values */
	float x = kernel_data.sunsky.zenith_x * sky_perez_function(kernel_data.sunsky.perez_x, theta, gamma);
	float y = kernel_data.sunsky.zenith_y * sky_perez_function(kernel_data.sunsky.perez_y, theta, gamma);
	float Y = kernel_data.sunsky.zenith_Y * sky_perez_function(kernel_data.sunsky.perez_Y, theta, gamma);

	/* convert to RGB */
	float3 xyz = xyY_to_xyz(x, y, Y);
	return xyz_to_rgb(xyz.x, xyz.y, xyz.z);
}

__device void svm_node_tex_sky(KernelGlobals *kg, ShaderData *sd, float *stack, uint dir_offset, uint out_offset)
{
	float3 dir = stack_load_float3(stack, dir_offset);
	float3 f = sky_radiance(kg, dir);

	stack_store_float3(stack, out_offset, f);
}

CCL_NAMESPACE_END

