/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

CCL_NAMESPACE_BEGIN

__device float3 xyY_to_xyz(float x, float y, float Y)
{
	float X, Z;

	if(y != 0.0f) X = (x / y) * Y;
	else X = 0.0f;

	if(y != 0.0f && Y != 0.0f) Z = (1.0f - x - y) / y * Y;
	else Z = 0.0f;

	return make_float3(X, Y, Z);
}

__device float3 xyz_to_rgb(float x, float y, float z)
{
	return make_float3(3.240479f * x + -1.537150f * y + -0.498535f * z,
					  -0.969256f * x +  1.875991f * y +  0.041556f * z,
					   0.055648f * x + -0.204043f * y +  1.057311f * z);
}

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

