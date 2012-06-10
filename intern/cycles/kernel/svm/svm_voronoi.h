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

/* Voronoi */

__device_noinline float4 svm_voronoi(NodeVoronoiColoring coloring, float scale, float3 p)
{
	/* compute distance and point coordinate of 4 nearest neighbours */
	float da[4];
	float3 pa[4];

	voronoi(p*scale, NODE_VORONOI_DISTANCE_SQUARED, 1.0f, da, pa);

	/* output */
	float fac;
	float3 color;

	if(coloring == NODE_VORONOI_INTENSITY) {
		fac = fabsf(da[0]);
		color = make_float3(fac, fac, fac);
	}
	else {
		color = cellnoise_color(pa[0]);
		fac = average(color);
	}

	return make_float4(color.x, color.y, color.z, fac);
}

__device void svm_node_tex_voronoi(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint coloring = node.y;
	uint scale_offset, co_offset, fac_offset, color_offset;

	decode_node_uchar4(node.z, &scale_offset, &co_offset, &fac_offset, &color_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float scale = stack_load_float_default(stack, scale_offset, node.w);

	float4 result = svm_voronoi((NodeVoronoiColoring)coloring, scale, co);
	float3 color = make_float3(result.x, result.y, result.z);
	float f = result.w;

	if(stack_valid(fac_offset)) stack_store_float(stack, fac_offset, f);
	if(stack_valid(color_offset)) stack_store_float3(stack, color_offset, color);
}

CCL_NAMESPACE_END

