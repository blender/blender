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

/* Marble */

__device_noinline float svm_marble(float3 p, float size, NodeMarbleType type, NodeWaveType wave, NodeNoiseBasis basis, int hard, float turb, int depth)
{
	float x = p.x;
	float y = p.y;
	float z = p.z;

    float n = 5.0f * (x + y + z);

	float mi = n + turb * noise_turbulence(p/size, basis, depth, hard);

	mi = noise_wave(wave, mi);

	if(type == NODE_MARBLE_SHARP)
		mi = sqrt(mi);
	else if(type == NODE_MARBLE_SHARPER)
		mi = sqrt(sqrt(mi));

    return mi;
}

__device void svm_node_tex_marble(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint4 node2 = read_node(kg, offset);

	uint type, wave, basis, hard;
	uint depth;
	uint size_offset, turbulence_offset, co_offset, fac_offset;

	decode_node_uchar4(node.y, &type, &wave, &basis, &hard);
	decode_node_uchar4(node.z, &depth, NULL, NULL, NULL);
	decode_node_uchar4(node.w, &size_offset, &turbulence_offset, &co_offset, &fac_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float size = stack_load_float_default(stack, size_offset, node2.x);
	float turbulence = stack_load_float_default(stack, turbulence_offset, node2.y);
	size = nonzerof(size, 1e-5f);

	float f = svm_marble(co, size, (NodeMarbleType)type, (NodeWaveType)wave,
		(NodeNoiseBasis)basis, hard, turbulence, depth);

	stack_store_float(stack, fac_offset, f);
}

CCL_NAMESPACE_END

