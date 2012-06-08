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

/* Wave */

__device_noinline float svm_wave(NodeWaveType type, float3 p, float scale, float detail, float distortion, float dscale)
{
	float w, n;

	p *= scale;

	if(type == NODE_WAVE_BANDS)
		n= (p.x + p.y + p.z)*10.0f;
	else /* if(type == NODE_WAVE_RINGS) */
		n= len(p)*20.0f;
	
	if(distortion != 0.0f)
		n += distortion * noise_turbulence(p*dscale, NODE_NOISE_PERLIN, detail, 0);

	w = noise_wave(NODE_WAVE_SINE, n);

	return w;
}

__device void svm_node_tex_wave(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint4 node2 = read_node(kg, offset);

	uint type;
	uint co_offset, scale_offset, detail_offset, dscale_offset, distortion_offset, color_offset, fac_offset;

	decode_node_uchar4(node.y, &type, &color_offset, &fac_offset, &dscale_offset);
	decode_node_uchar4(node.z, &co_offset, &scale_offset, &detail_offset, &distortion_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float scale = stack_load_float_default(stack, scale_offset, node2.x);
	float detail = stack_load_float_default(stack, detail_offset, node2.y);
	float distortion = stack_load_float_default(stack, distortion_offset, node2.z);
	float dscale = stack_load_float_default(stack, dscale_offset, node2.w);

	float f = svm_wave((NodeWaveType)type, co, scale, detail, distortion, dscale);

	if(stack_valid(fac_offset)) stack_store_float(stack, fac_offset, f);
	if(stack_valid(color_offset)) stack_store_float3(stack, color_offset, make_float3(f, f, f));
}

CCL_NAMESPACE_END

