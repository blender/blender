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

__device void svm_node_combine_rgb(ShaderData *sd, float *stack, uint in_offset, uint color_index, uint out_offset)
{
	float color = stack_load_float(stack, in_offset);

	if (stack_valid(out_offset))
		stack_store_float(stack, out_offset+color_index, color);
}

__device void svm_node_separate_rgb(ShaderData *sd, float *stack, uint icolor_offset, uint color_index, uint out_offset)
{
	float3 color = stack_load_float3(stack, icolor_offset);

	if (stack_valid(out_offset)) {
		if (color_index == 0)
			stack_store_float(stack, out_offset, color.x);
		else if (color_index == 1)
			stack_store_float(stack, out_offset, color.y);
		else
			stack_store_float(stack, out_offset, color.z);
	}
}

CCL_NAMESPACE_END

