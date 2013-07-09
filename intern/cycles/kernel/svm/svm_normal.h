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

__device void svm_node_normal(KernelGlobals *kg, ShaderData *sd, float *stack, uint in_normal_offset, uint out_normal_offset, uint out_dot_offset, int *offset)
{
	/* read extra data */
	uint4 node1 = read_node(kg, offset);
	float3 normal = stack_load_float3(stack, in_normal_offset);

	float3 direction;
	direction.x = __int_as_float(node1.x);
	direction.y = __int_as_float(node1.y);
	direction.z = __int_as_float(node1.z);
	direction = normalize(direction);

	if (stack_valid(out_normal_offset))
		stack_store_float3(stack, out_normal_offset, direction);

	if (stack_valid(out_dot_offset))
		stack_store_float(stack, out_dot_offset, dot(direction, normalize(normal)));
}

CCL_NAMESPACE_END

