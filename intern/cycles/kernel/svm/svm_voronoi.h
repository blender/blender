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
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

/* Voronoi */

ccl_device float voronoi_F1_distance(float3 p)
{
	/* returns squared distance in da */
	float da = 1e10f;

	int3 xyzi = quick_floor_to_int3(p);

	for(int xx = -1; xx <= 1; xx++) {
		for(int yy = -1; yy <= 1; yy++) {
			for(int zz = -1; zz <= 1; zz++) {
				int3 ip = xyzi + make_int3(xx, yy, zz);
				float3 fp = make_float3(ip.x, ip.y, ip.z);
				float3 vp = fp + cellnoise3(fp);
				float d = len_squared(p - vp);
				da = min(d, da);
			}
		}
	}

	return da;
}

ccl_device float3 voronoi_F1_color(float3 p)
{
	/* returns color of the nearest point */
	float da = 1e10f;
	float3 pa;

	int3 xyzi = quick_floor_to_int3(p);

	for(int xx = -1; xx <= 1; xx++) {
		for(int yy = -1; yy <= 1; yy++) {
			for(int zz = -1; zz <= 1; zz++) {
				int3 ip = xyzi + make_int3(xx, yy, zz);
				float3 fp = make_float3(ip.x, ip.y, ip.z);
				float3 vp = fp + cellnoise3(fp);
				float d = len_squared(p - vp);

				if(d < da) {
					da = d;
					pa = vp;
				}
			}
		}
	}

	return cellnoise3(pa);
}

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
