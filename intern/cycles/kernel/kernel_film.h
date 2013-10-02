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

__device float4 film_map(KernelGlobals *kg, float4 irradiance, float scale)
{
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

__device void kernel_film_convert_to_byte(KernelGlobals *kg,
	__global uchar4 *rgba, __global float *buffer,
	float sample_scale, int x, int y, int offset, int stride)
{
	/* buffer offset */
	int index = offset + x + y*stride;

	rgba += index;
	buffer += index*kernel_data.film.pass_stride;

	/* map colors */
	float4 irradiance = *((__global float4*)buffer);
	float4 float_result = film_map(kg, irradiance, sample_scale);
	uchar4 byte_result = film_float_to_byte(float_result);

	*rgba = byte_result;
}

__device void kernel_film_convert_to_half_float(KernelGlobals *kg,
	__global uchar4 *rgba, __global float *buffer,
	float sample_scale, int x, int y, int offset, int stride)
{
	/* buffer offset */
	int index = offset + x + y*stride;

	__global float4 *in = (__global float4*)(buffer + index*kernel_data.film.pass_stride);
	__global half *out = (__global half*)rgba + index*4;
	float scale = kernel_data.film.exposure*sample_scale;

	float4_store_half(out, in, scale);
}

CCL_NAMESPACE_END

