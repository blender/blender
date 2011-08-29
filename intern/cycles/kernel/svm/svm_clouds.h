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

/* Clouds */

__device_inline void svm_clouds(NodeNoiseBasis basis, int hard, int depth, float size, float3 p, float *fac, float3 *color)
{
	p /= size;

	*fac = noise_turbulence(p, basis, depth, hard);
	*color = make_float3(*fac,
		noise_turbulence(make_float3(p.y, p.x, p.z), basis, depth, hard),
		noise_turbulence(make_float3(p.y, p.z, p.x), basis, depth, hard));
}

__device void svm_node_tex_clouds(ShaderData *sd, float *stack, uint4 node)
{
	uint basis, hard, depth;
	uint size_offset, co_offset, fac_offset, color_offset;

	decode_node_uchar4(node.y, &basis, &hard, &depth, NULL);
	decode_node_uchar4(node.z, &size_offset, &co_offset, &fac_offset, &color_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float size = stack_load_float_default(stack, size_offset, node.w);
	size = nonzerof(size, 1e-5f);

	float3 color;
	float f;

	svm_clouds((NodeNoiseBasis)basis, hard, depth, size, co, &f, &color);

	if(stack_valid(fac_offset)) stack_store_float(stack, fac_offset, f);
	if(stack_valid(color_offset)) stack_store_float3(stack, color_offset, color);
}

CCL_NAMESPACE_END

