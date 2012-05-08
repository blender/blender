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

/* Mapping Node */

__device void svm_node_mapping(KernelGlobals *kg, ShaderData *sd, float *stack, uint vec_offset, uint out_offset, int *offset)
{
	float3 v = stack_load_float3(stack, vec_offset);

	Transform tfm;
	tfm.x = read_node_float(kg, offset);
	tfm.y = read_node_float(kg, offset);
	tfm.z = read_node_float(kg, offset);
	tfm.w = read_node_float(kg, offset);

	float3 r = transform_point(&tfm, v);
	stack_store_float3(stack, out_offset, r);
}

CCL_NAMESPACE_END

