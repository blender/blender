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

__device float4 film_map(KernelGlobals *kg, float4 irradiance, int sample)
{
	float scale = 1.0f/(float)(sample+1);
	float exposure = kernel_data.film.exposure;
	float4 result = irradiance*scale;

	/* conversion to srgb */
	result.x = color_scene_linear_to_srgb(result.x*exposure);
	result.y = color_scene_linear_to_srgb(result.y*exposure);
	result.z = color_scene_linear_to_srgb(result.z*exposure);

	/* clamp since alpha might be > 1.0 due to russian roulette */
	result.w = clamp(result.w, 0.0f, 1.0f);

	return result;
}

__device uchar4 film_float_to_byte(float4 color)
{
	uchar4 result;

	/* simple float to byte conversion */
	result.x = (uchar)clamp(color.x*255.0f, 0.0f, 255.0f);
	result.y = (uchar)clamp(color.y*255.0f, 0.0f, 255.0f);
	result.z = (uchar)clamp(color.z*255.0f, 0.0f, 255.0f);
	result.w = (uchar)clamp(color.w*255.0f, 0.0f, 255.0f);

	return result;
}

__device void kernel_film_tonemap(KernelGlobals *kg,
	__global uchar4 *rgba, __global float *buffer,
	int sample, int resolution, int x, int y, int offset, int stride)
{
	/* buffer offset */
	int index = offset + x + y*stride;

	rgba += index;
	buffer += index*kernel_data.film.pass_stride;

	/* map colors */
	float4 irradiance = *((__global float4*)buffer);
	float4 float_result = film_map(kg, irradiance, sample);
	uchar4 byte_result = film_float_to_byte(float_result);

	*rgba = byte_result;
}

CCL_NAMESPACE_END

