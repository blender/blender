/*
 * Copyright 2013, Blender Foundation.
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

__device void svm_node_combine_hsv(KernelGlobals *kg, ShaderData *sd, float *stack, uint hue_in, uint saturation_in, uint value_in, int *offset)
{
	uint4 node1 = read_node(kg, offset);
	uint color_out = node1.y;
	
	float hue = stack_load_float(stack, hue_in);
	float saturation = stack_load_float(stack, saturation_in);
	float value = stack_load_float(stack, value_in);
	
	/* Combine, and convert back to RGB */
	float3 color = hsv_to_rgb(make_float3(hue, saturation, value));

	if (stack_valid(color_out))
		stack_store_float3(stack, color_out, color);
}

__device void svm_node_separate_hsv(KernelGlobals *kg, ShaderData *sd, float *stack, uint color_in, uint hue_out, uint saturation_out, int *offset)
{
	uint4 node1 = read_node(kg, offset);
	uint value_out = node1.y;
	
	float3 color = stack_load_float3(stack, color_in);
	
	/* Convert to HSV */
	color = rgb_to_hsv(color);

	if (stack_valid(hue_out)) 
			stack_store_float(stack, hue_out, color.x);
	if (stack_valid(saturation_out)) 
			stack_store_float(stack, saturation_out, color.y);
	if (stack_valid(value_out)) 
			stack_store_float(stack, value_out, color.z);
}

CCL_NAMESPACE_END

