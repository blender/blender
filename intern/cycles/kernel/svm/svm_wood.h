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

/* Wood */

__device_noinline float svm_wood(float3 p, float size, NodeWoodType type, NodeWaveType wave, NodeNoiseBasis basis, uint hard, float turb)
{
	float x = p.x;
	float y = p.y;
	float z = p.z;

	if(type == NODE_WOOD_BANDS) {
		return noise_wave(wave, (x + y + z)*10.0f);
	}
	else if(type == NODE_WOOD_RINGS) {
		return noise_wave(wave, sqrt(x*x + y*y + z*z)*20.0f);
	}
	else if (type == NODE_WOOD_BAND_NOISE) {
		float wi = turb*noise_basis_hard(p/size, basis, hard);
		return noise_wave(wave, (x + y + z)*10.0f + wi);
	}
	else if (type == NODE_WOOD_RING_NOISE) {
		float wi = turb*noise_basis_hard(p/size, basis, hard);
		return noise_wave(wave, sqrt(x*x + y*y + z*z)*20.0f + wi);
	}

	return 0.0f;
}

__device void svm_node_tex_wood(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint4 node2 = read_node(kg, offset);

	uint type, wave, basis, hard;
	uint co_offset, size_offset, turbulence_offset, fac_offset;

	decode_node_uchar4(node.y, &type, &wave, &basis, &hard);
	decode_node_uchar4(node.z, &co_offset, &size_offset, &turbulence_offset, &fac_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float size = stack_load_float_default(stack, size_offset, node2.y);
	float turbulence = stack_load_float_default(stack, turbulence_offset, node2.z);
	size = nonzerof(size, 1e-5f);

	float f = svm_wood(co, size, (NodeWoodType)type, (NodeWaveType)wave,
		(NodeNoiseBasis)basis, hard, turbulence);

	stack_store_float(stack, fac_offset, f);
}

CCL_NAMESPACE_END

