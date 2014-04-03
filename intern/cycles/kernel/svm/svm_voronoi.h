/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

/* Voronoi */

ccl_device_noinline float4 svm_voronoi(NodeVoronoiColoring coloring, float3 p)
{
	if(coloring == NODE_VORONOI_INTENSITY) {
		/* compute squared distance to the nearest neighbour */
		float fac = voronoi_F1_distance(p);
		return make_float4(fac, fac, fac, fac);
	}
	else {
		/* compute color of the nearest neighbour */
		float3 color = voronoi_F1_color(p);
		return make_float4(color.x, color.y, color.z, average(color));
	}
}

ccl_device void svm_node_tex_voronoi(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint coloring = node.y;
	uint scale_offset, co_offset, fac_offset, color_offset;

	decode_node_uchar4(node.z, &scale_offset, &co_offset, &fac_offset, &color_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float scale = stack_load_float_default(stack, scale_offset, node.w);

	float4 result = svm_voronoi((NodeVoronoiColoring)coloring, co*scale);
	float3 color = make_float3(result.x, result.y, result.z);
	float f = result.w;

	if(stack_valid(fac_offset)) stack_store_float(stack, fac_offset, f);
	if(stack_valid(color_offset)) stack_store_float3(stack, color_offset, color);
}

CCL_NAMESPACE_END

