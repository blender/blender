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

#ifndef __SVM_RAMP_H__
#define __SVM_RAMP_H__

CCL_NAMESPACE_BEGIN

__device float4 rgb_ramp_lookup(KernelGlobals *kg, int offset, float f)
{
	f = clamp(f, 0.0f, 1.0f)*(RAMP_TABLE_SIZE-1);

	int i = (int)f;
	float t = f - (float)i;

	float4 a = fetch_node_float(kg, offset+i);

	if(t > 0.0f)
		a = (1.0f - t)*a + t*fetch_node_float(kg, offset+i+1);

	return a;
}

__device void svm_node_rgb_ramp(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint fac_offset = node.y;
	uint color_offset = node.z;
	uint alpha_offset = node.w;

	float fac = stack_load_float(stack, fac_offset);
	float4 color = rgb_ramp_lookup(kg, *offset, fac);

	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, float4_to_float3(color));
	if(stack_valid(alpha_offset))
		stack_store_float(stack, alpha_offset, color.w);

	*offset += RAMP_TABLE_SIZE;
}

__device void svm_node_rgb_curves(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint fac_offset = node.y;
	uint color_offset = node.z;
	uint out_offset = node.w;

	float fac = stack_load_float(stack, fac_offset);
	float3 color = stack_load_float3(stack, color_offset);

	float r = rgb_ramp_lookup(kg, *offset, rgb_ramp_lookup(kg, *offset, color.x).w).x;
	float g = rgb_ramp_lookup(kg, *offset, rgb_ramp_lookup(kg, *offset, color.y).w).y;
	float b = rgb_ramp_lookup(kg, *offset, rgb_ramp_lookup(kg, *offset, color.z).w).z;

	color = (1.0f - fac)*color + fac*make_float3(r, g, b);
	stack_store_float3(stack, out_offset, color);

	*offset += RAMP_TABLE_SIZE;
}

CCL_NAMESPACE_END

#endif /* __SVM_RAMP_H__ */

