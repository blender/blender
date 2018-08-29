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
 *
 * Aligned nodes intersection AVX code is adopted from Embree,
 */

struct OBVHStackItem {
	int addr;
	float dist;
};

ccl_device_inline void obvh_near_far_idx_calc(const float3& idir,
                                              int *ccl_restrict near_x,
                                              int *ccl_restrict near_y,
                                              int *ccl_restrict near_z,
                                              int *ccl_restrict far_x,
                                              int *ccl_restrict far_y,
                                              int *ccl_restrict far_z)

{
#ifdef __KERNEL_SSE__
	*near_x = 0; *far_x = 1;
	*near_y = 2; *far_y = 3;
	*near_z = 4; *far_z = 5;

	const size_t mask = movemask(ssef(idir.m128));

	const int mask_x = mask & 1;
	const int mask_y = (mask & 2) >> 1;
	const int mask_z = (mask & 4) >> 2;

	*near_x += mask_x; *far_x -= mask_x;
	*near_y += mask_y; *far_y -= mask_y;
	*near_z += mask_z; *far_z -= mask_z;
#else
	if(idir.x >= 0.0f) { *near_x = 0; *far_x = 1; } else { *near_x = 1; *far_x = 0; }
	if(idir.y >= 0.0f) { *near_y = 2; *far_y = 3; } else { *near_y = 3; *far_y = 2; }
	if(idir.z >= 0.0f) { *near_z = 4; *far_z = 5; } else { *near_z = 5; *far_z = 4; }
#endif
}

ccl_device_inline void obvh_item_swap(OBVHStackItem *ccl_restrict a,
                                      OBVHStackItem *ccl_restrict b)
{
	OBVHStackItem tmp = *a;
	*a = *b;
	*b = tmp;
}

ccl_device_inline void obvh_stack_sort(OBVHStackItem *ccl_restrict s1,
                                       OBVHStackItem *ccl_restrict s2,
                                       OBVHStackItem *ccl_restrict s3)
{
	if(s2->dist < s1->dist) { obvh_item_swap(s2, s1); }
	if(s3->dist < s2->dist) { obvh_item_swap(s3, s2); }
	if(s2->dist < s1->dist) { obvh_item_swap(s2, s1); }
}

ccl_device_inline void obvh_stack_sort(OBVHStackItem *ccl_restrict s1,
                                       OBVHStackItem *ccl_restrict s2,
                                       OBVHStackItem *ccl_restrict s3,
                                       OBVHStackItem *ccl_restrict s4)
{
	if(s2->dist < s1->dist) { obvh_item_swap(s2, s1); }
	if(s4->dist < s3->dist) { obvh_item_swap(s4, s3); }
	if(s3->dist < s1->dist) { obvh_item_swap(s3, s1); }
	if(s4->dist < s2->dist) { obvh_item_swap(s4, s2); }
	if(s3->dist < s2->dist) { obvh_item_swap(s3, s2); }
}

ccl_device_inline void obvh_stack_sort(OBVHStackItem *ccl_restrict s1,
                                       OBVHStackItem *ccl_restrict s2,
                                       OBVHStackItem *ccl_restrict s3,
                                       OBVHStackItem *ccl_restrict s4,
                                       OBVHStackItem *ccl_restrict s5)
{
	obvh_stack_sort(s1, s2, s3, s4);
	if(s5->dist < s4->dist) {
		obvh_item_swap(s4, s5);
		if(s4->dist < s3->dist) {
			obvh_item_swap(s3, s4);
			if(s3->dist < s2->dist) {
				obvh_item_swap(s2, s3);
				if(s2->dist < s1->dist) {
					obvh_item_swap(s1, s2);
				}
			}
		}
	}
}

ccl_device_inline void obvh_stack_sort(OBVHStackItem *ccl_restrict s1,
                                       OBVHStackItem *ccl_restrict s2,
                                       OBVHStackItem *ccl_restrict s3,
                                       OBVHStackItem *ccl_restrict s4,
                                       OBVHStackItem *ccl_restrict s5,
                                       OBVHStackItem *ccl_restrict s6)
{
	obvh_stack_sort(s1, s2, s3, s4, s5);
	if(s6->dist < s5->dist) {
		obvh_item_swap(s5, s6);
		if(s5->dist < s4->dist) {
			obvh_item_swap(s4, s5);
			if(s4->dist < s3->dist) {
				obvh_item_swap(s3, s4);
				if(s3->dist < s2->dist) {
					obvh_item_swap(s2, s3);
					if(s2->dist < s1->dist) {
						obvh_item_swap(s1, s2);
					}
				}
			}
		}
	}
}

ccl_device_inline void obvh_stack_sort(OBVHStackItem *ccl_restrict s1,
                                       OBVHStackItem *ccl_restrict s2,
                                       OBVHStackItem *ccl_restrict s3,
                                       OBVHStackItem *ccl_restrict s4,
                                       OBVHStackItem *ccl_restrict s5,
                                       OBVHStackItem *ccl_restrict s6,
                                       OBVHStackItem *ccl_restrict s7)
{
	obvh_stack_sort(s1, s2, s3, s4, s5, s6);
	if(s7->dist < s6->dist) {
		obvh_item_swap(s6, s7);
		if(s6->dist < s5->dist) {
			obvh_item_swap(s5, s6);
			if(s5->dist < s4->dist) {
				obvh_item_swap(s4, s5);
				if(s4->dist < s3->dist) {
					obvh_item_swap(s3, s4);
					if(s3->dist < s2->dist) {
						obvh_item_swap(s2, s3);
						if(s2->dist < s1->dist) {
							obvh_item_swap(s1, s2);
						}
					}
				}
			}
		}
	}
}

ccl_device_inline void obvh_stack_sort(OBVHStackItem *ccl_restrict s1,
                                       OBVHStackItem *ccl_restrict s2,
                                       OBVHStackItem *ccl_restrict s3,
                                       OBVHStackItem *ccl_restrict s4,
                                       OBVHStackItem *ccl_restrict s5,
                                       OBVHStackItem *ccl_restrict s6,
                                       OBVHStackItem *ccl_restrict s7,
                                       OBVHStackItem *ccl_restrict s8)
{
	obvh_stack_sort(s1, s2, s3, s4, s5, s6, s7);
	if(s8->dist < s7->dist) {
		obvh_item_swap(s7, s8);
		if(s7->dist < s6->dist) {
			obvh_item_swap(s6, s7);
			if(s6->dist < s5->dist) {
				obvh_item_swap(s5, s6);
				if(s5->dist < s4->dist) {
					obvh_item_swap(s4, s5);
					if(s4->dist < s3->dist) {
						obvh_item_swap(s3, s4);
						if(s3->dist < s2->dist) {
							obvh_item_swap(s2, s3);
							if(s2->dist < s1->dist) {
								obvh_item_swap(s1, s2);
							}
						}
					}
				}
			}
		}
	}
}

/* Axis-aligned nodes intersection */

ccl_device_inline int obvh_aligned_node_intersect(KernelGlobals *ccl_restrict kg,
                                                  const avxf& isect_near,
                                                  const avxf& isect_far,
#ifdef __KERNEL_AVX2__
                                                  const avx3f& org_idir,
#else
                                                  const avx3f& org,
#endif
                                                  const avx3f& idir,
                                                  const int near_x,
                                                  const int near_y,
                                                  const int near_z,
                                                  const int far_x,
                                                  const int far_y,
                                                  const int far_z,
                                                  const int node_addr,
                                                  avxf *ccl_restrict dist)
{
	const int offset = node_addr + 2;
#ifdef __KERNEL_AVX2__
	const avxf tnear_x = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset+near_x*2), idir.x, org_idir.x);
	const avxf tnear_y = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset+near_y*2), idir.y, org_idir.y);
	const avxf tnear_z = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset+near_z*2), idir.z, org_idir.z);
	const avxf tfar_x = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset+far_x*2), idir.x, org_idir.x);
	const avxf tfar_y = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset+far_y*2), idir.y, org_idir.y);
	const avxf tfar_z = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset+far_z*2), idir.z, org_idir.z);

	const avxf tnear = max4(tnear_x, tnear_y, tnear_z, isect_near);
	const avxf tfar = min4(tfar_x, tfar_y, tfar_z, isect_far);
	const avxb vmask = tnear <= tfar;
	int mask = (int)movemask(vmask);
	*dist = tnear;
	return mask;
#else
	return 0;
#endif
}

ccl_device_inline int obvh_aligned_node_intersect_robust(
        KernelGlobals *ccl_restrict kg,
        const avxf& isect_near,
        const avxf& isect_far,
#ifdef __KERNEL_AVX2__
        const avx3f& P_idir,
#else
        const avx3f& P,
#endif
        const avx3f& idir,
        const int near_x,
        const int near_y,
        const int near_z,
        const int far_x,
        const int far_y,
        const int far_z,
        const int node_addr,
        const float difl,
        avxf *ccl_restrict dist)
{
	const int offset = node_addr + 2;
#ifdef __KERNEL_AVX2__
	const avxf tnear_x = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset + near_x * 2), idir.x, P_idir.x);
	const avxf tfar_x = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset + far_x * 2), idir.x, P_idir.x);
	const avxf tnear_y = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset + near_y * 2), idir.y, P_idir.y);
	const avxf tfar_y = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset + far_y * 2), idir.y, P_idir.y);
	const avxf tnear_z = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset + near_z * 2), idir.z, P_idir.z);
	const avxf tfar_z = msub(kernel_tex_fetch_avxf(__bvh_nodes, offset + far_z * 2), idir.z, P_idir.z);

	const float round_down = 1.0f - difl;
	const float round_up = 1.0f + difl;
	const avxf tnear = max4(tnear_x, tnear_y, tnear_z, isect_near);
	const avxf tfar = min4(tfar_x, tfar_y, tfar_z, isect_far);
	const avxb vmask = round_down*tnear <= round_up*tfar;
	int mask = (int)movemask(vmask);
	*dist = tnear;
	return mask;
#else
	return 0;
#endif
}

/* Unaligned nodes intersection */

ccl_device_inline int obvh_unaligned_node_intersect(
        KernelGlobals *ccl_restrict kg,
        const avxf& isect_near,
        const avxf& isect_far,
#ifdef __KERNEL_AVX2__
        const avx3f& org_idir,
#endif
        const avx3f& org,
        const avx3f& dir,
        const avx3f& idir,
        const int near_x,
        const int near_y,
        const int near_z,
        const int far_x,
        const int far_y,
        const int far_z,
        const int node_addr,
        avxf *ccl_restrict dist)
{
	const int offset = node_addr;
	const avxf tfm_x_x = kernel_tex_fetch_avxf(__bvh_nodes, offset+2);
	const avxf tfm_x_y = kernel_tex_fetch_avxf(__bvh_nodes, offset+4);
	const avxf tfm_x_z = kernel_tex_fetch_avxf(__bvh_nodes, offset+6);

	const avxf tfm_y_x = kernel_tex_fetch_avxf(__bvh_nodes, offset+8);
	const avxf tfm_y_y = kernel_tex_fetch_avxf(__bvh_nodes, offset+10);
	const avxf tfm_y_z = kernel_tex_fetch_avxf(__bvh_nodes, offset+12);

	const avxf tfm_z_x = kernel_tex_fetch_avxf(__bvh_nodes, offset+14);
	const avxf tfm_z_y = kernel_tex_fetch_avxf(__bvh_nodes, offset+16);
	const avxf tfm_z_z = kernel_tex_fetch_avxf(__bvh_nodes, offset+18);

	const avxf tfm_t_x = kernel_tex_fetch_avxf(__bvh_nodes, offset+20);
	const avxf tfm_t_y = kernel_tex_fetch_avxf(__bvh_nodes, offset+22);
	const avxf tfm_t_z = kernel_tex_fetch_avxf(__bvh_nodes, offset+24);

	const avxf aligned_dir_x = dir.x*tfm_x_x + dir.y*tfm_x_y + dir.z*tfm_x_z,
	           aligned_dir_y = dir.x*tfm_y_x + dir.y*tfm_y_y + dir.z*tfm_y_z,
	           aligned_dir_z = dir.x*tfm_z_x + dir.y*tfm_z_y + dir.z*tfm_z_z;

	const avxf aligned_P_x = org.x*tfm_x_x + org.y*tfm_x_y + org.z*tfm_x_z + tfm_t_x,
	           aligned_P_y = org.x*tfm_y_x + org.y*tfm_y_y + org.z*tfm_y_z + tfm_t_y,
	           aligned_P_z = org.x*tfm_z_x + org.y*tfm_z_y + org.z*tfm_z_z + tfm_t_z;

	const avxf neg_one(-1.0f);
	const avxf nrdir_x = neg_one / aligned_dir_x,
	           nrdir_y = neg_one / aligned_dir_y,
	           nrdir_z = neg_one / aligned_dir_z;

	const avxf tlower_x = aligned_P_x * nrdir_x,
	           tlower_y = aligned_P_y * nrdir_y,
	           tlower_z = aligned_P_z * nrdir_z;

	const avxf tupper_x = tlower_x - nrdir_x,
	           tupper_y = tlower_y - nrdir_y,
	           tupper_z = tlower_z - nrdir_z;

	const avxf tnear_x = min(tlower_x, tupper_x);
	const avxf tnear_y = min(tlower_y, tupper_y);
	const avxf tnear_z = min(tlower_z, tupper_z);
	const avxf tfar_x = max(tlower_x, tupper_x);
	const avxf tfar_y = max(tlower_y, tupper_y);
	const avxf tfar_z = max(tlower_z, tupper_z);
	const avxf tnear = max4(isect_near, tnear_x, tnear_y, tnear_z);
	const avxf tfar = min4(isect_far, tfar_x, tfar_y, tfar_z);
	const avxb vmask = tnear <= tfar;
	*dist = tnear;
	return movemask(vmask);
}

ccl_device_inline int obvh_unaligned_node_intersect_robust(
        KernelGlobals *ccl_restrict kg,
        const avxf& isect_near,
        const avxf& isect_far,
#ifdef __KERNEL_AVX2__
        const avx3f& P_idir,
#endif
        const avx3f& P,
        const avx3f& dir,
        const avx3f& idir,
        const int near_x,
        const int near_y,
        const int near_z,
        const int far_x,
        const int far_y,
        const int far_z,
        const int node_addr,
        const float difl,
        avxf *ccl_restrict dist)
{
	const int offset = node_addr;
	const avxf tfm_x_x = kernel_tex_fetch_avxf(__bvh_nodes, offset+2);
	const avxf tfm_x_y = kernel_tex_fetch_avxf(__bvh_nodes, offset+4);
	const avxf tfm_x_z = kernel_tex_fetch_avxf(__bvh_nodes, offset+6);

	const avxf tfm_y_x = kernel_tex_fetch_avxf(__bvh_nodes, offset+8);
	const avxf tfm_y_y = kernel_tex_fetch_avxf(__bvh_nodes, offset+10);
	const avxf tfm_y_z = kernel_tex_fetch_avxf(__bvh_nodes, offset+12);

	const avxf tfm_z_x = kernel_tex_fetch_avxf(__bvh_nodes, offset+14);
	const avxf tfm_z_y = kernel_tex_fetch_avxf(__bvh_nodes, offset+16);
	const avxf tfm_z_z = kernel_tex_fetch_avxf(__bvh_nodes, offset+18);

	const avxf tfm_t_x = kernel_tex_fetch_avxf(__bvh_nodes, offset+20);
	const avxf tfm_t_y = kernel_tex_fetch_avxf(__bvh_nodes, offset+22);
	const avxf tfm_t_z = kernel_tex_fetch_avxf(__bvh_nodes, offset+24);

	const avxf aligned_dir_x = dir.x*tfm_x_x + dir.y*tfm_x_y + dir.z*tfm_x_z,
	           aligned_dir_y = dir.x*tfm_y_x + dir.y*tfm_y_y + dir.z*tfm_y_z,
	           aligned_dir_z = dir.x*tfm_z_x + dir.y*tfm_z_y + dir.z*tfm_z_z;

	const avxf aligned_P_x = P.x*tfm_x_x + P.y*tfm_x_y + P.z*tfm_x_z + tfm_t_x,
	           aligned_P_y = P.x*tfm_y_x + P.y*tfm_y_y + P.z*tfm_y_z + tfm_t_y,
	           aligned_P_z = P.x*tfm_z_x + P.y*tfm_z_y + P.z*tfm_z_z + tfm_t_z;

	const avxf neg_one(-1.0f);
	const avxf nrdir_x = neg_one / aligned_dir_x,
	           nrdir_y = neg_one / aligned_dir_y,
	           nrdir_z = neg_one / aligned_dir_z;

	const avxf tlower_x = aligned_P_x * nrdir_x,
	           tlower_y = aligned_P_y * nrdir_y,
	           tlower_z = aligned_P_z * nrdir_z;

	const avxf tupper_x = tlower_x - nrdir_x,
	           tupper_y = tlower_y - nrdir_y,
	           tupper_z = tlower_z - nrdir_z;

	const float round_down = 1.0f - difl;
	const float round_up = 1.0f + difl;

	const avxf tnear_x = min(tlower_x, tupper_x);
	const avxf tnear_y = min(tlower_y, tupper_y);
	const avxf tnear_z = min(tlower_z, tupper_z);
	const avxf tfar_x = max(tlower_x, tupper_x);
	const avxf tfar_y = max(tlower_y, tupper_y);
	const avxf tfar_z = max(tlower_z, tupper_z);

	const avxf tnear = max4(isect_near, tnear_x, tnear_y, tnear_z);
	const avxf tfar = min4(isect_far, tfar_x, tfar_y, tfar_z);
	const avxb vmask = round_down*tnear <= round_up*tfar;
	*dist = tnear;
	return movemask(vmask);
}

/* Intersectors wrappers.
 *
 * They'll check node type and call appropriate intersection code.
 */

ccl_device_inline int obvh_node_intersect(
        KernelGlobals *ccl_restrict kg,
        const avxf& isect_near,
        const avxf& isect_far,
#ifdef __KERNEL_AVX2__
        const avx3f& org_idir,
#endif
        const avx3f& org,
        const avx3f& dir,
        const avx3f& idir,
        const int near_x,
        const int near_y,
        const int near_z,
        const int far_x,
        const int far_y,
        const int far_z,
        const int node_addr,
        avxf *ccl_restrict dist)
{
	const int offset = node_addr;
	const float4 node = kernel_tex_fetch(__bvh_nodes, offset);
	if(__float_as_uint(node.x) & PATH_RAY_NODE_UNALIGNED) {
		return obvh_unaligned_node_intersect(kg,
		                                     isect_near,
		                                     isect_far,
#ifdef __KERNEL_AVX2__
		                                     org_idir,
#endif
		                                     org,
		                                     dir,
		                                     idir,
		                                     near_x, near_y, near_z,
		                                     far_x, far_y, far_z,
		                                     node_addr,
		                                     dist);
	}
	else {
		return obvh_aligned_node_intersect(kg,
		                                   isect_near,
		                                   isect_far,
#ifdef __KERNEL_AVX2__
		                                   org_idir,
#else
		                                   org,
#endif
		                                   idir,
		                                   near_x, near_y, near_z,
		                                   far_x, far_y, far_z,
		                                   node_addr,
		                                   dist);
	}
}

ccl_device_inline int obvh_node_intersect_robust(
        KernelGlobals *ccl_restrict kg,
        const avxf& isect_near,
        const avxf& isect_far,
#ifdef __KERNEL_AVX2__
        const avx3f& P_idir,
#endif
        const avx3f& P,
        const avx3f& dir,
        const avx3f& idir,
        const int near_x,
        const int near_y,
        const int near_z,
        const int far_x,
        const int far_y,
        const int far_z,
        const int node_addr,
        const float difl,
        avxf *ccl_restrict dist)
{
	const int offset = node_addr;
	const float4 node = kernel_tex_fetch(__bvh_nodes, offset);
	if(__float_as_uint(node.x) & PATH_RAY_NODE_UNALIGNED) {
		return obvh_unaligned_node_intersect_robust(kg,
		                                            isect_near,
		                                            isect_far,
#ifdef __KERNEL_AVX2__
		                                            P_idir,
#endif
		                                            P,
		                                            dir,
		                                            idir,
		                                            near_x, near_y, near_z,
		                                            far_x, far_y, far_z,
		                                            node_addr,
		                                            difl,
		                                            dist);
	}
	else {
		return obvh_aligned_node_intersect_robust(kg,
		                                          isect_near,
		                                          isect_far,
#ifdef __KERNEL_AVX2__
		                                          P_idir,
#else
		                                          P,
#endif
		                                          idir,
		                                          near_x, near_y, near_z,
		                                          far_x, far_y, far_z,
		                                          node_addr,
		                                          difl,
		                                          dist);
	}
}
