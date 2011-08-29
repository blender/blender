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

/* Distorted Noise (variable lacunarity noise) */

__device_noinline float svm_distorted_noise(float3 p, float size, NodeNoiseBasis basis, NodeNoiseBasis distortion_basis, float distortion)
{
	float3 r;
	float3 offset = make_float3(13.5f, 13.5f, 13.5f);

	p /= size;

	r.x = noise_basis(p + offset, basis) * distortion;
	r.y = noise_basis(p, basis) * distortion;
	r.z = noise_basis(p - offset, basis) * distortion;

    return noise_basis(p + r, distortion_basis); /* distorted-domain noise */
}

__device void svm_node_tex_distorted_noise(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint4 node2 = read_node(kg, offset);

	uint basis, distortion_basis;
	uint size_offset, distortion_offset, co_offset, fac_offset;

	decode_node_uchar4(node.y, &basis, &distortion_basis, NULL, NULL);
	decode_node_uchar4(node.z, &size_offset, &distortion_offset, &co_offset, &fac_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float size = stack_load_float_default(stack, size_offset, node2.x);
	float distortion = stack_load_float_default(stack, distortion_offset, node2.y);
	size = nonzerof(size, 1e-5f);

	float f = svm_distorted_noise(co, size, (NodeNoiseBasis)basis,
		(NodeNoiseBasis)distortion_basis, distortion);

	stack_store_float(stack, fac_offset, f);
}

CCL_NAMESPACE_END

