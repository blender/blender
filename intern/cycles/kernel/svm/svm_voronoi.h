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

__device_noinline float4 svm_voronoi(NodeDistanceMetric distance_metric, NodeVoronoiColoring coloring,
	float weight1, float weight2, float weight3, float weight4,
	float exponent, float intensity, float size, float3 p)
{
	float aw1 = fabsf(weight1);
	float aw2 = fabsf(weight2);
	float aw3 = fabsf(weight3);
	float aw4 = fabsf(weight4);
	float sc = (aw1 + aw2 + aw3 + aw4);

	if(sc != 0.0f)
		sc = intensity/sc;
	
	/* compute distance and point coordinate of 4 nearest neighbours */
	float da[4];
	float3 pa[4];

	voronoi(p/size, distance_metric, exponent, da, pa);

	/* Scalar output */
	float fac = sc * fabsf(weight1*da[0] + weight2*da[1] + weight3*da[2] + weight4*da[3]);
	float3 color;

	/* colored output */
	if(coloring == NODE_VORONOI_INTENSITY) {
		color = make_float3(fac, fac, fac);
	}
	else {
		color = aw1*cellnoise_color(pa[0]);
		color += aw2*cellnoise_color(pa[1]);
		color += aw3*cellnoise_color(pa[2]);
		color += aw4*cellnoise_color(pa[3]);

		if(coloring != NODE_VORONOI_POSITION) {
			float t1 = min((da[1] - da[0])*10.0f, 1.0f);

			if(coloring == NODE_VORONOI_POSITION_OUTLINE_INTENSITY)
				color *= t1*fac;
			else if(coloring == NODE_VORONOI_POSITION_OUTLINE)
				color *= t1*sc;
		}
		else {
			color *= sc;
		}
	}

	return make_float4(color.x, color.y, color.z, fac);
}

__device void svm_node_tex_voronoi(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint4 node2 = read_node(kg, offset);
	uint4 node3 = read_node(kg, offset);

	uint distance_metric, coloring, exponent_offset;
	uint size_offset, co_offset, fac_offset, color_offset;
	uint weight1_offset, weight2_offset, weight3_offset, weight4_offset;

	decode_node_uchar4(node.y, &distance_metric, &coloring, &exponent_offset, NULL);
	decode_node_uchar4(node.z, &size_offset, &co_offset, &fac_offset, &color_offset);
	decode_node_uchar4(node.w, &weight1_offset, &weight2_offset, &weight3_offset, &weight4_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float weight1 = stack_load_float_default(stack, weight1_offset, node2.x);
	float weight2 = stack_load_float_default(stack, weight2_offset, node2.y);
	float weight3 = stack_load_float_default(stack, weight3_offset, node2.z);
	float weight4 = stack_load_float_default(stack, weight4_offset, node2.w);
	float exponent = stack_load_float_default(stack, exponent_offset, node3.x);
	float size = stack_load_float_default(stack, size_offset, node3.y);

	exponent = fmaxf(exponent, 1e-5f);
	size = nonzerof(size, 1e-5f);

	float4 result = svm_voronoi((NodeDistanceMetric)distance_metric,
		(NodeVoronoiColoring)coloring,
		weight1, weight2, weight3, weight4, exponent, 1.0f, size, co);
	float3 color = make_float3(result.x, result.y, result.z);
	float f = result.w;

	if(stack_valid(fac_offset)) stack_store_float(stack, fac_offset, f);
	if(stack_valid(color_offset)) stack_store_float3(stack, color_offset, color);
}

CCL_NAMESPACE_END

