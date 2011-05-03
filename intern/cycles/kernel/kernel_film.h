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

__device float4 film_map(KernelGlobals *kg, float4 irradiance, int pass)
{
	float scale = kernel_data.film.exposure*(1.0f/(pass+1));
	float4 result = irradiance*scale;

	if(kernel_data.film.use_response_curve) {
		/* camera response curve */
		result.x = kernel_tex_interp(__response_curve_R, result.x);
		result.y = kernel_tex_interp(__response_curve_G, result.y);
		result.z = kernel_tex_interp(__response_curve_B, result.z);
	}
	else {
		/* conversion to srgb */
		result.x = color_scene_linear_to_srgb(result.x);
		result.y = color_scene_linear_to_srgb(result.y);
		result.z = color_scene_linear_to_srgb(result.z);
	}

	return result;
}

__device uchar4 film_float_to_byte(float4 color)
{
	uchar4 result;

	/* simple float to byte conversion */
	result.x = (uchar)clamp(color.x*255.0f, 0.0f, 255.0f);
	result.y = (uchar)clamp(color.y*255.0f, 0.0f, 255.0f);
	result.z = (uchar)clamp(color.z*255.0f, 0.0f, 255.0f);
	result.w = 255;

	return result;
}

__device void kernel_film_tonemap(KernelGlobals *kg, __global uchar4 *rgba, __global float4 *buffer, int pass, int resolution, int x, int y)
{
	int w = kernel_data.cam.width;
	int index = x + y*w;
	float4 irradiance = buffer[index];

	float4 float_result = film_map(kg, irradiance, pass);
	uchar4 byte_result = film_float_to_byte(float_result);

	rgba[index] = byte_result;
}

CCL_NAMESPACE_END

