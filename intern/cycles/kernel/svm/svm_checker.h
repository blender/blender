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

/* Checker */

__device_noinline float svm_checker(float3 p, float scale)
{	
	p *= scale;

	/* 0.00001  because of unit sized stuff */
	int xi = (int)fabsf(floor(0.00001f + p.x));
	int yi = (int)fabsf(floor(0.00001f + p.y));
	int zi = (int)fabsf(floor(0.00001f + p.z));
	
	return ((xi % 2 == yi % 2) == (zi % 2))? 1.0f: 0.0f;
}

__device void svm_node_tex_checker(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{	
	uint co_offset, color1_offset, color2_offset, scale_offset;
	uint color_offset, fac_offset;

	decode_node_uchar4(node.y, &co_offset, &color1_offset, &color2_offset, &scale_offset);
	decode_node_uchar4(node.z, &color_offset, &fac_offset, NULL, NULL);

	float3 co = stack_load_float3(stack, co_offset);
	float3 color1 = stack_load_float3(stack, color1_offset);
	float3 color2 = stack_load_float3(stack, color2_offset);
	float scale = stack_load_float_default(stack, scale_offset, node.w);
	
	float f = svm_checker(co, scale);

	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, (f == 1.0f)? color1: color2);
	if(stack_valid(fac_offset))
		stack_store_float(stack, fac_offset, f);
}

CCL_NAMESPACE_END

