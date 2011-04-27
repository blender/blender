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

__device float svm_noise_texture_value(float3 p)
{
	return cellnoise(p*1e8f);
}

__device float3 svm_noise_texture_color(float3 p)
{
	return cellnoise_color(p*1e8f);
}

__device void svm_node_tex_noise_f(ShaderData *sd, float *stack, uint co_offset, uint out_offset)
{
	float3 co = stack_load_float3(stack, co_offset);
	float f = svm_noise_texture_value(co);

	stack_store_float(stack, out_offset, f);
}

__device void svm_node_tex_noise_v(ShaderData *sd, float *stack, uint co_offset, uint out_offset)
{
	float3 co = stack_load_float3(stack, co_offset);
	float3 f = svm_noise_texture_color(co);

	stack_store_float3(stack, out_offset, f);
}

CCL_NAMESPACE_END

