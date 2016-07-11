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

struct QBVHStackItem {
	int addr;
	float dist;
};

/* TOOD(sergey): Investigate if using intrinsics helps for both
 * stack item swap and float comparison.
 */
ccl_device_inline void qbvh_item_swap(QBVHStackItem *__restrict a,
                                      QBVHStackItem *__restrict b)
{
	QBVHStackItem tmp = *a;
	*a = *b;
	*b = tmp;
}

ccl_device_inline void qbvh_stack_sort(QBVHStackItem *__restrict s1,
                                       QBVHStackItem *__restrict s2,
                                       QBVHStackItem *__restrict s3)
{
	if(s2->dist < s1->dist) { qbvh_item_swap(s2, s1); }
	if(s3->dist < s2->dist) { qbvh_item_swap(s3, s2); }
	if(s2->dist < s1->dist) { qbvh_item_swap(s2, s1); }
}

ccl_device_inline void qbvh_stack_sort(QBVHStackItem *__restrict s1,
                                       QBVHStackItem *__restrict s2,
                                       QBVHStackItem *__restrict s3,
                                       QBVHStackItem *__restrict s4)
{
	if(s2->dist < s1->dist) { qbvh_item_swap(s2, s1); }
	if(s4->dist < s3->dist) { qbvh_item_swap(s4, s3); }
	if(s3->dist < s1->dist) { qbvh_item_swap(s3, s1); }
	if(s4->dist < s2->dist) { qbvh_item_swap(s4, s2); }
	if(s3->dist < s2->dist) { qbvh_item_swap(s3, s2); }
}

/* Axis-aligned nodes intersection */

ccl_device_inline int qbvh_aligned_node_intersect(KernelGlobals *__restrict kg,
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
	const int offset = nodeAddr + 1;
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

ccl_device_inline int qbvh_aligned_node_intersect_robust(
        KernelGlobals *__restrict kg,
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
	const int offset = nodeAddr + 1;
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

/* Unaligned nodes intersection */

ccl_device_inline int qbvh_unaligned_node_intersect(
        KernelGlobals *__restrict kg,
        const ssef& tnear,
        const ssef& tfar,
#ifdef __KERNEL_AVX2__
        const sse3f& org_idir,
#endif
        const sse3f& org,
        const sse3f& dir,
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
	const int offset = nodeAddr;
	const ssef tfm_x_x = kernel_tex_fetch_ssef(__bvh_nodes, offset+1);
	const ssef tfm_x_y = kernel_tex_fetch_ssef(__bvh_nodes, offset+2);
	const ssef tfm_x_z = kernel_tex_fetch_ssef(__bvh_nodes, offset+3);

	const ssef tfm_y_x = kernel_tex_fetch_ssef(__bvh_nodes, offset+4);
	const ssef tfm_y_y = kernel_tex_fetch_ssef(__bvh_nodes, offset+5);
	const ssef tfm_y_z = kernel_tex_fetch_ssef(__bvh_nodes, offset+6);

	const ssef tfm_z_x = kernel_tex_fetch_ssef(__bvh_nodes, offset+7);
	const ssef tfm_z_y = kernel_tex_fetch_ssef(__bvh_nodes, offset+8);
	const ssef tfm_z_z = kernel_tex_fetch_ssef(__bvh_nodes, offset+9);

	const ssef tfm_t_x = kernel_tex_fetch_ssef(__bvh_nodes, offset+10);
	const ssef tfm_t_y = kernel_tex_fetch_ssef(__bvh_nodes, offset+11);
	const ssef tfm_t_z = kernel_tex_fetch_ssef(__bvh_nodes, offset+12);

	const ssef aligned_dir_x = dir.x*tfm_x_x + dir.y*tfm_x_y + dir.z*tfm_x_z,
	           aligned_dir_y = dir.x*tfm_y_x + dir.y*tfm_y_y + dir.z*tfm_y_z,
	           aligned_dir_z = dir.x*tfm_z_x + dir.y*tfm_z_y + dir.z*tfm_z_z;

	const ssef aligned_P_x = org.x*tfm_x_x + org.y*tfm_x_y + org.z*tfm_x_z + tfm_t_x,
	           aligned_P_y = org.x*tfm_y_x + org.y*tfm_y_y + org.z*tfm_y_z + tfm_t_y,
	           aligned_P_z = org.x*tfm_z_x + org.y*tfm_z_y + org.z*tfm_z_z + tfm_t_z;

	const ssef neg_one(-1.0f, -1.0f, -1.0f, -1.0f);
	const ssef nrdir_x = neg_one / aligned_dir_x,
	           nrdir_y = neg_one / aligned_dir_y,
	           nrdir_z = neg_one / aligned_dir_z;

	const ssef tlower_x = aligned_P_x * nrdir_x,
	           tlower_y = aligned_P_y * nrdir_y,
	           tlower_z = aligned_P_z * nrdir_z;

	const ssef tupper_x = tlower_x - nrdir_x,
	           tupper_y = tlower_y - nrdir_y,
	           tupper_z = tlower_z - nrdir_z;

#ifdef __KERNEL_SSE41__
	const ssef tnear_x = mini(tlower_x, tupper_x);
	const ssef tnear_y = mini(tlower_y, tupper_y);
	const ssef tnear_z = mini(tlower_z, tupper_z);
	const ssef tfar_x = maxi(tlower_x, tupper_x);
	const ssef tfar_y = maxi(tlower_y, tupper_y);
	const ssef tfar_z = maxi(tlower_z, tupper_z);
	const ssef tNear = max4(tnear, tnear_x, tnear_y, tnear_z);
	const ssef tFar = min4(tfar, tfar_x, tfar_y, tfar_z);
	const sseb vmask = tNear <= tFar;
	*dist = tNear;
	return movemask(vmask);
#else
	const ssef tnear_x = min(tlower_x, tupper_x);
	const ssef tnear_y = min(tlower_y, tupper_y);
	const ssef tnear_z = min(tlower_z, tupper_z);
	const ssef tfar_x = max(tlower_x, tupper_x);
	const ssef tfar_y = max(tlower_y, tupper_y);
	const ssef tfar_z = max(tlower_z, tupper_z);
	const ssef tNear = max4(tnear, tnear_x, tnear_y, tnear_z);
	const ssef tFar = min4(tfar, tfar_x, tfar_y, tfar_z);
	const sseb vmask = tNear <= tFar;
	*dist = tNear;
	return movemask(vmask);
#endif
}

ccl_device_inline int qbvh_unaligned_node_intersect_robust(
        KernelGlobals *__restrict kg,
        const ssef& tnear,
        const ssef& tfar,
#ifdef __KERNEL_AVX2__
        const sse3f& P_idir,
#endif
        const sse3f& P,
        const sse3f& dir,
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
	const int offset = nodeAddr;
	const ssef tfm_x_x = kernel_tex_fetch_ssef(__bvh_nodes, offset+1);
	const ssef tfm_x_y = kernel_tex_fetch_ssef(__bvh_nodes, offset+2);
	const ssef tfm_x_z = kernel_tex_fetch_ssef(__bvh_nodes, offset+3);

	const ssef tfm_y_x = kernel_tex_fetch_ssef(__bvh_nodes, offset+4);
	const ssef tfm_y_y = kernel_tex_fetch_ssef(__bvh_nodes, offset+5);
	const ssef tfm_y_z = kernel_tex_fetch_ssef(__bvh_nodes, offset+6);

	const ssef tfm_z_x = kernel_tex_fetch_ssef(__bvh_nodes, offset+7);
	const ssef tfm_z_y = kernel_tex_fetch_ssef(__bvh_nodes, offset+8);
	const ssef tfm_z_z = kernel_tex_fetch_ssef(__bvh_nodes, offset+9);

	const ssef tfm_t_x = kernel_tex_fetch_ssef(__bvh_nodes, offset+10);
	const ssef tfm_t_y = kernel_tex_fetch_ssef(__bvh_nodes, offset+11);
	const ssef tfm_t_z = kernel_tex_fetch_ssef(__bvh_nodes, offset+12);

	const ssef aligned_dir_x = dir.x*tfm_x_x + dir.y*tfm_x_y + dir.z*tfm_x_z,
	           aligned_dir_y = dir.x*tfm_y_x + dir.y*tfm_y_y + dir.z*tfm_y_z,
	           aligned_dir_z = dir.x*tfm_z_x + dir.y*tfm_z_y + dir.z*tfm_z_z;

	const ssef aligned_P_x = P.x*tfm_x_x + P.y*tfm_x_y + P.z*tfm_x_z + tfm_t_x,
	           aligned_P_y = P.x*tfm_y_x + P.y*tfm_y_y + P.z*tfm_y_z + tfm_t_y,
	           aligned_P_z = P.x*tfm_z_x + P.y*tfm_z_y + P.z*tfm_z_z + tfm_t_z;

	const ssef neg_one(-1.0f, -1.0f, -1.0f, -1.0f);
	const ssef nrdir_x = neg_one / aligned_dir_x,
	           nrdir_y = neg_one / aligned_dir_y,
	           nrdir_z = neg_one / aligned_dir_z;

	const ssef tlower_x = aligned_P_x * nrdir_x,
	           tlower_y = aligned_P_y * nrdir_y,
	           tlower_z = aligned_P_z * nrdir_z;

	const ssef tupper_x = tlower_x - nrdir_x,
	           tupper_y = tlower_y - nrdir_y,
	           tupper_z = tlower_z - nrdir_z;

	const float round_down = 1.0f - difl;
	const float round_up = 1.0f + difl;

#ifdef __KERNEL_SSE41__
	const ssef tnear_x = mini(tlower_x, tupper_x);
	const ssef tnear_y = mini(tlower_y, tupper_y);
	const ssef tnear_z = mini(tlower_z, tupper_z);
	const ssef tfar_x = maxi(tlower_x, tupper_x);
	const ssef tfar_y = maxi(tlower_y, tupper_y);
	const ssef tfar_z = maxi(tlower_z, tupper_z);
#else
	const ssef tnear_x = min(tlower_x, tupper_x);
	const ssef tnear_y = min(tlower_y, tupper_y);
	const ssef tnear_z = min(tlower_z, tupper_z);
	const ssef tfar_x = max(tlower_x, tupper_x);
	const ssef tfar_y = max(tlower_y, tupper_y);
	const ssef tfar_z = max(tlower_z, tupper_z);
#endif
	const ssef tNear = max4(tnear, tnear_x, tnear_y, tnear_z);
	const ssef tFar = min4(tfar, tfar_x, tfar_y, tfar_z);
	const sseb vmask = round_down*tNear <= round_up*tFar;
	*dist = tNear;
	return movemask(vmask);
}

/* Intersectors wrappers.
 *
 * They'll check node type and call appropriate intersection code.
 */

ccl_device_inline int qbvh_node_intersect(
        KernelGlobals *__restrict kg,
        const ssef& tnear,
        const ssef& tfar,
#ifdef __KERNEL_AVX2__
        const sse3f& org_idir,
#endif
        const sse3f& org,
        const sse3f& dir,
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
	const int offset = nodeAddr;
	const float4 node = kernel_tex_fetch(__bvh_nodes, offset);
	if(__float_as_uint(node.x) & PATH_RAY_NODE_UNALIGNED) {
		return qbvh_unaligned_node_intersect(kg,
		                                     tnear,
		                                     tfar,
#ifdef __KERNEL_AVX2__
		                                     org_idir,
#endif
		                                     org,
		                                     dir,
		                                     idir,
		                                     near_x, near_y, near_z,
		                                     far_x, far_y, far_z,
		                                     nodeAddr,
		                                     dist);
	}
	else {
		return qbvh_aligned_node_intersect(kg,
		                                   tnear,
		                                   tfar,
#ifdef __KERNEL_AVX2__
		                                   org_idir,
#else
		                                   org,
#endif
		                                   idir,
		                                   near_x, near_y, near_z,
		                                   far_x, far_y, far_z,
		                                   nodeAddr,
		                                   dist);
	}
}

ccl_device_inline int qbvh_node_intersect_robust(
        KernelGlobals *__restrict kg,
        const ssef& tnear,
        const ssef& tfar,
#ifdef __KERNEL_AVX2__
        const sse3f& P_idir,
#endif
        const sse3f& P,
        const sse3f& dir,
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
	const int offset = nodeAddr;
	const float4 node = kernel_tex_fetch(__bvh_nodes, offset);
	if(__float_as_uint(node.x) & PATH_RAY_NODE_UNALIGNED) {
		return qbvh_unaligned_node_intersect_robust(kg,
		                                            tnear,
		                                            tfar,
#ifdef __KERNEL_AVX2__
		                                            P_idir,
#endif
		                                            P,
		                                            dir,
		                                            idir,
		                                            near_x, near_y, near_z,
		                                            far_x, far_y, far_z,
		                                            nodeAddr,
		                                            difl,
		                                            dist);
	}
	else {
		return qbvh_aligned_node_intersect_robust(kg,
		                                          tnear,
		                                          tfar,
#ifdef __KERNEL_AVX2__
		                                          P_idir,
#else
		                                          P,
#endif
		                                          idir,
		                                          near_x, near_y, near_z,
		                                          far_x, far_y, far_z,
		                                          nodeAddr,
		                                          difl,
		                                          dist);
	}
}
