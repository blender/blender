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

/*
 * "An Analytic Model for Full Spectral Sky-Dome Radiance"
 * Lukas Hosek, Alexander Wilkie
 */

__device float sky_angle_between(float thetav, float phiv, float theta, float phi)
{
	float cospsi = sinf(thetav)*sinf(theta)*cosf(phi - phiv) + cosf(thetav)*cosf(theta);
	return safe_acosf(cospsi);
}

/* ArHosekSkyModel_GetRadianceInternal */
__device float sky_radiance_internal(__constant float *configuration, float theta, float gamma)
{
    const float expM = expf(configuration[4] * gamma);
    const float rayM = cosf(gamma)*cosf(gamma);
    const float mieM = (1.0f + cosf(gamma)*cosf(gamma)) / powf((1.0f + configuration[8]*configuration[8] - 2.0f*configuration[8]*cosf(gamma)), 1.5f);
    const float zenith = sqrt(cosf(theta));

    return (1.0f + configuration[0] * expf(configuration[1] / (cosf(theta) + 0.01f))) *
            (configuration[2] + configuration[3] * expM + configuration[5] * rayM + configuration[6] * mieM + configuration[7] * zenith);
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

	/* compute xyz color space values */
	float x = sky_radiance_internal(kernel_data.sunsky.config_x, theta, gamma) * kernel_data.sunsky.radiance_x;
	float y = sky_radiance_internal(kernel_data.sunsky.config_y, theta, gamma) * kernel_data.sunsky.radiance_y;
	float z = sky_radiance_internal(kernel_data.sunsky.config_z, theta, gamma) * kernel_data.sunsky.radiance_z;

	/* convert to RGB */
	return xyz_to_rgb(x, y, z);
}

__device void svm_node_tex_sky(KernelGlobals *kg, ShaderData *sd, float *stack, uint dir_offset, uint out_offset)
{
	float3 dir = stack_load_float3(stack, dir_offset);
	float3 f = sky_radiance(kg, dir);

	stack_store_float3(stack, out_offset, f);
}

CCL_NAMESPACE_END

