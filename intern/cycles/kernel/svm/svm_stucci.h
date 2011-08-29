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

/* Stucci */

__device_noinline float svm_stucci(NodeStucciType type, NodeNoiseBasis basis, int hard, float turbulence, float size, float3 p)
{
	p /= size;

	float b2 = noise_basis_hard(p, basis, hard);
	float ofs = turbulence/200.0f;

	if(type != NODE_STUCCI_PLASTIC)
		ofs *= b2*b2;
	
	float r = noise_basis_hard(make_float3(p.x, p.y, p.z+ofs), basis, hard);

	if(type == NODE_STUCCI_WALL_OUT)
		r = 1.0f - r;
	
	return fmaxf(r, 0.0f);
}

__device void svm_node_tex_stucci(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint4 node2 = read_node(kg, offset);

	uint type, basis, hard;
	uint size_offset, turbulence_offset, co_offset, fac_offset;

	decode_node_uchar4(node.y, &type, &basis, &hard, NULL);
	decode_node_uchar4(node.z, &size_offset, &turbulence_offset, &co_offset, &fac_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float size = stack_load_float_default(stack, size_offset, node2.x);
	float turbulence = stack_load_float_default(stack, turbulence_offset, node2.y);
	size = nonzerof(size, 1e-5f);

	float f = svm_stucci((NodeStucciType)type, (NodeNoiseBasis)basis, hard,
		turbulence, size, co);

	stack_store_float(stack, fac_offset, f);
}

CCL_NAMESPACE_END

