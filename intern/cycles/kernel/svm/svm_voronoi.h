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

ccl_device void voronoi_neighbors(float3 p, NodeVoronoiDistanceMetric distance, float e, float da[4], float3 pa[4])
{
	/* Compute the distance to and the position of the closest neighbors to p.
	 *
	 * The neighbors are randomly placed, 1 each in a 3x3x3 grid (Worley pattern).
	 * The distances and points are returned in ascending order, i.e. da[0] and pa[0] will
	 * contain the distance to the closest point and its coordinates respectively.
	 */

	da[0] = 1e10f;
	da[1] = 1e10f;
	da[2] = 1e10f;
	da[3] = 1e10f;

	int3 xyzi = quick_floor_to_int3(p);

	for(int xx = -1; xx <= 1; xx++) {
		for(int yy = -1; yy <= 1; yy++) {
			for(int zz = -1; zz <= 1; zz++) {
				int3 ip = xyzi + make_int3(xx, yy, zz);
				float3 fp = make_float3(ip.x, ip.y, ip.z);
				float3 vp = fp + cellnoise3(fp);

				float d;
				switch(distance) {
					case NODE_VORONOI_DISTANCE:
						d = len_squared(p - vp);
						break;
					case NODE_VORONOI_MANHATTAN:
						d = reduce_add(fabs(vp - p));
						break;
					case NODE_VORONOI_CHEBYCHEV:
						d = max3(fabs(vp - p));
						break;
					case NODE_VORONOI_MINKOWSKI:
						float3 n = fabs(vp - p);
						if(e == 0.5f) {
							d = sqr(reduce_add(sqrt(n)));
						}
						else {
							d = powf(reduce_add(pow3(n, e)), 1.0f/e);
						}
						break;
				}

				/* To keep the shortest four distances and associated points we have to keep them in sorted order. */
				if (d < da[0]) {
					da[3] = da[2];
					da[2] = da[1];
					da[1] = da[0];
					da[0] = d;

					pa[3] = pa[2];
					pa[2] = pa[1];
					pa[1] = pa[0];
					pa[0] = vp;
				}
				else if (d < da[1]) {
					da[3] = da[2];
					da[2] = da[1];
					da[1] = d;

					pa[3] = pa[2];
					pa[2] = pa[1];
					pa[1] = vp;
				}
				else if (d < da[2]) {
					da[3] = da[2];
					da[2] = d;

					pa[3] = pa[2];
					pa[2] = vp;
				}
				else if (d < da[3]) {
					da[3] = d;
					pa[3] = vp;
				}
			}
		}
	}
}

ccl_device void svm_node_tex_voronoi(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint4 node2 = read_node(kg, offset);

	uint co_offset, coloring, distance, feature;
	uint scale_offset, e_offset, fac_offset, color_offset;

	decode_node_uchar4(node.y, &co_offset, &coloring, &distance, &feature);
	decode_node_uchar4(node.z, &scale_offset, &e_offset, &fac_offset, &color_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float scale = stack_load_float_default(stack, scale_offset, node2.x);
	float exponent = stack_load_float_default(stack, e_offset, node2.y);

	float dist[4];
	float3 neighbor[4];
	voronoi_neighbors(co*scale, (NodeVoronoiDistanceMetric)distance, exponent, dist, neighbor);

	float3 color;
	float fac;
	if(coloring == NODE_VORONOI_INTENSITY) {
		switch(feature) {
			case NODE_VORONOI_F1: fac = dist[0]; break;
			case NODE_VORONOI_F2: fac = dist[1]; break;
			case NODE_VORONOI_F3: fac = dist[2]; break;
			case NODE_VORONOI_F4: fac = dist[3]; break;
			case NODE_VORONOI_F2F1: fac = dist[1] - dist[0]; break;
		}

		color = make_float3(fac, fac, fac);
	}
	else {
		 /* NODE_VORONOI_CELLS */
		switch(feature) {
			case NODE_VORONOI_F1: color = neighbor[0]; break;
			case NODE_VORONOI_F2: color = neighbor[1]; break;
			case NODE_VORONOI_F3: color = neighbor[2]; break;
			case NODE_VORONOI_F4: color = neighbor[3]; break;
			/* Usefulness of this vector is questionable. Note F2 >= F1 but the
			 * individual vector components might not be. */
			case NODE_VORONOI_F2F1: color = fabs(neighbor[1] - neighbor[0]); break;
		}

		color = cellnoise3(color);
		fac = average(color);
	}

	if(stack_valid(fac_offset)) stack_store_float(stack, fac_offset, fac);
	if(stack_valid(color_offset)) stack_store_float3(stack, color_offset, color);
}

CCL_NAMESPACE_END
