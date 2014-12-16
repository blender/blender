/*
 * Copyright 2011-2014, Blender Foundation.
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

ccl_device_inline void qbvh_stack_sort(int *__restrict s1,
                                       int *__restrict s2,
                                       int *__restrict s3,
                                       float *__restrict d1,
                                       float *__restrict d2,
                                       float *__restrict d3)
{
	if(*d2 < *d1) { util_swap(s2, s1); util_swap(d2, d1); }
	if(*d3 < *d2) { util_swap(s3, s2); util_swap(d3, d2); }
	if(*d2 < *d1) { util_swap(s2, s1); util_swap(d2, d1); }
}

ccl_device_inline void qbvh_stack_sort(int *__restrict s1,
                                       int *__restrict s2,
                                       int *__restrict s3,
                                       int *__restrict s4,
                                       float *__restrict d1,
                                       float *__restrict d2,
                                       float *__restrict d3,
                                       float *__restrict d4)
{
	if(*d2 < *d1) { util_swap(s2, s1); util_swap(d2, d1); }
	if(*d4 < *d3) { util_swap(s4, s3); util_swap(d4, d3); }
	if(*d3 < *d1) { util_swap(s3, s1); util_swap(d3, d1); }
	if(*d4 < *d2) { util_swap(s4, s2); util_swap(d4, d2); }
	if(*d3 < *d2) { util_swap(s3, s2); util_swap(d3, d2); }
}

ccl_device_inline int qbvh_node_intersect(KernelGlobals *__restrict kg,
                                          const ssef& tnear,
                                          const ssef& tfar,
#ifdef __KERNEL_AVX2__
                                          const sse3f& org_idir,
#else
                                          const sse3f& org,
#endif
                                          const sse3f& idir,
                                          const int near_x,
                                          const int near_y,
                                          const int near_z,
                                          const int far_x,
                                          const int far_y,
                                          const int far_z,
                                          const int nodeAddr,
                                          ssef *__restrict dist)
{
	const int offset = nodeAddr*BVH_QNODE_SIZE;
#ifdef __KERNEL_AVX2__
	const ssef tnear_x = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+near_x), idir.x, org_idir.x);
	const ssef tnear_y = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+near_y), idir.y, org_idir.y);
	const ssef tnear_z = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+near_z), idir.z, org_idir.z);
	const ssef tfar_x = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+far_x), idir.x, org_idir.x);
	const ssef tfar_y = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+far_y), idir.y, org_idir.y);
	const ssef tfar_z = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+far_z), idir.z, org_idir.z);
#else
	const ssef tnear_x = (kernel_tex_fetch_ssef(__bvh_nodes, offset+near_x) - org.x) * idir.x;
	const ssef tnear_y = (kernel_tex_fetch_ssef(__bvh_nodes, offset+near_y) - org.y) * idir.y;
	const ssef tnear_z = (kernel_tex_fetch_ssef(__bvh_nodes, offset+near_z) - org.z) * idir.z;
	const ssef tfar_x = (kernel_tex_fetch_ssef(__bvh_nodes, offset+far_x) - org.x) * idir.x;
	const ssef tfar_y = (kernel_tex_fetch_ssef(__bvh_nodes, offset+far_y) - org.y) * idir.y;
	const ssef tfar_z = (kernel_tex_fetch_ssef(__bvh_nodes, offset+far_z) - org.z) * idir.z;
#endif

#ifdef __KERNEL_SSE41__
	const ssef tNear = maxi(maxi(tnear_x, tnear_y), maxi(tnear_z, tnear));
	const ssef tFar = mini(mini(tfar_x, tfar_y), mini(tfar_z, tfar));
	const sseb vmask = cast(tNear) > cast(tFar);
	int mask = (int)movemask(vmask)^0xf;
#else
	const ssef tNear = max4(tnear_x, tnear_y, tnear_z, tnear);
	const ssef tFar = min4(tfar_x, tfar_y, tfar_z, tfar);
	const sseb vmask = tNear <= tFar;
	int mask = (int)movemask(vmask);
#endif
	*dist = tNear;
	return mask;
}

ccl_device_inline int qbvh_node_intersect_robust(KernelGlobals *__restrict kg,
                                                 const ssef& tnear,
                                                 const ssef& tfar,
#ifdef __KERNEL_AVX2__
                                                 const sse3f& P_idir,
#else
                                                 const sse3f& P,
#endif
                                                 const sse3f& idir,
                                                 const int near_x,
                                                 const int near_y,
                                                 const int near_z,
                                                 const int far_x,
                                                 const int far_y,
                                                 const int far_z,
                                                 const int nodeAddr,
                                                 const float difl,
                                                 ssef *__restrict dist)
{
	const int offset = nodeAddr*BVH_QNODE_SIZE;
#ifdef __KERNEL_AVX2__
	const ssef tnear_x = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+near_x), idir.x, P_idir.x);
	const ssef tnear_y = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+near_y), idir.y, P_idir.y);
	const ssef tnear_z = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+near_z), idir.z, P_idir.z);
	const ssef tfar_x = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+far_x), idir.x, P_idir.x);
	const ssef tfar_y = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+far_y), idir.y, P_idir.y);
	const ssef tfar_z = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset+far_z), idir.z, P_idir.z);
#else
	const ssef tnear_x = (kernel_tex_fetch_ssef(__bvh_nodes, offset+near_x) - P.x) * idir.x;
	const ssef tnear_y = (kernel_tex_fetch_ssef(__bvh_nodes, offset+near_y) - P.y) * idir.y;
	const ssef tnear_z = (kernel_tex_fetch_ssef(__bvh_nodes, offset+near_z) - P.z) * idir.z;
	const ssef tfar_x = (kernel_tex_fetch_ssef(__bvh_nodes, offset+far_x) - P.x) * idir.x;
	const ssef tfar_y = (kernel_tex_fetch_ssef(__bvh_nodes, offset+far_y) - P.y) * idir.y;
	const ssef tfar_z = (kernel_tex_fetch_ssef(__bvh_nodes, offset+far_z) - P.z) * idir.z;
#endif

	const float round_down = 1.0f - difl;
	const float round_up = 1.0f + difl;
	const ssef tNear = max4(tnear_x, tnear_y, tnear_z, tnear);
	const ssef tFar = min4(tfar_x, tfar_y, tfar_z, tfar);
	const sseb vmask = round_down*tNear <= round_up*tFar;
	*dist = tNear;
	return (int)movemask(vmask);
}
