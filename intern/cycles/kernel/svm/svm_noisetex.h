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

/* Noise */

__device_inline void svm_noise(float3 p, float scale, float detail, float distortion, float *fac, float3 *color)
{
	NodeNoiseBasis basis = NODE_NOISE_PERLIN;
	int hard = 0;

	p *= scale;

	if(distortion != 0.0f) {
		float3 r, offset = make_float3(13.5f, 13.5f, 13.5f);

		r.x = noise_basis(p + offset, basis) * distortion;
		r.y = noise_basis(p, basis) * distortion;
		r.z = noise_basis(p - offset, basis) * distortion;

		p += r;
	}

	*fac = noise_turbulence(p, basis, detail, hard);
	*color = make_float3(*fac,
		noise_turbulence(make_float3(p.y, p.x, p.z), basis, detail, hard),
		noise_turbulence(make_float3(p.y, p.z, p.x), basis, detail, hard));
}

__device void svm_node_tex_noise(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint co_offset, scale_offset, detail_offset, distortion_offset, fac_offset, color_offset;

	decode_node_uchar4(node.y, &co_offset, &scale_offset, &detail_offset, &distortion_offset);

	uint4 node2 = read_node(kg, offset);

	float scale = stack_load_float_default(stack, scale_offset, node2.x);
	float detail = stack_load_float_default(stack, detail_offset, node2.y);
	float distortion = stack_load_float_default(stack, distortion_offset, node2.z);
	float3 co = stack_load_float3(stack, co_offset);

	float3 color;
	float f;

	svm_noise(co, scale, detail, distortion, &f, &color);

	decode_node_uchar4(node.z, &color_offset, &fac_offset, NULL, NULL);

	if(stack_valid(fac_offset))
		stack_store_float(stack, fac_offset, f);
	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, color);
}

CCL_NAMESPACE_END

