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

/* Value Nodes */

__device void svm_node_value_f(KernelGlobals *kg, ShaderData *sd, float *stack, uint ivalue, uint out_offset)
{
	stack_store_float(stack, out_offset, __int_as_float(ivalue));
}

__device void svm_node_value_v(KernelGlobals *kg, ShaderData *sd, float *stack, uint out_offset, int *offset)
{
	/* read extra data */
	uint4 node1 = read_node(kg, offset);
	float3 p = make_float3(__int_as_float(node1.y), __int_as_float(node1.z), __int_as_float(node1.w));

	stack_store_float3(stack, out_offset, p);
}

CCL_NAMESPACE_END

