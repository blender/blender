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
 * Aligned nodes intersection SSE code is adopted from Embree,
 */

struct QBVHStackItem {
  int addr;
  float dist;
};

ccl_device_inline void qbvh_near_far_idx_calc(const float3 &idir,
                                              int *ccl_restrict near_x,
                                              int *ccl_restrict near_y,
                                              int *ccl_restrict near_z,
                                              int *ccl_restrict far_x,
                                              int *ccl_restrict far_y,
                                              int *ccl_restrict far_z)

{
#ifdef __KERNEL_SSE__
  *near_x = 0;
  *far_x = 1;
  *near_y = 2;
  *far_y = 3;
  *near_z = 4;
  *far_z = 5;

  const size_t mask = movemask(ssef(idir.m128));

  const int mask_x = mask & 1;
  const int mask_y = (mask & 2) >> 1;
  const int mask_z = (mask & 4) >> 2;

  *near_x += mask_x;
  *far_x -= mask_x;
  *near_y += mask_y;
  *far_y -= mask_y;
  *near_z += mask_z;
  *far_z -= mask_z;
#else
  if (idir.x >= 0.0f) {
    *near_x = 0;
    *far_x = 1;
  }
  else {
    *near_x = 1;
    *far_x = 0;
  }
  if (idir.y >= 0.0f) {
    *near_y = 2;
    *far_y = 3;
  }
  else {
    *near_y = 3;
    *far_y = 2;
  }
  if (idir.z >= 0.0f) {
    *near_z = 4;
    *far_z = 5;
  }
  else {
    *near_z = 5;
    *far_z = 4;
  }
#endif
}

/* TOOD(sergey): Investigate if using intrinsics helps for both
 * stack item swap and float comparison.
 */
ccl_device_inline void qbvh_item_swap(QBVHStackItem *ccl_restrict a, QBVHStackItem *ccl_restrict b)
{
  QBVHStackItem tmp = *a;
  *a = *b;
  *b = tmp;
}

ccl_device_inline void qbvh_stack_sort(QBVHStackItem *ccl_restrict s1,
                                       QBVHStackItem *ccl_restrict s2,
                                       QBVHStackItem *ccl_restrict s3)
{
  if (s2->dist < s1->dist) {
    qbvh_item_swap(s2, s1);
  }
  if (s3->dist < s2->dist) {
    qbvh_item_swap(s3, s2);
  }
  if (s2->dist < s1->dist) {
    qbvh_item_swap(s2, s1);
  }
}

ccl_device_inline void qbvh_stack_sort(QBVHStackItem *ccl_restrict s1,
                                       QBVHStackItem *ccl_restrict s2,
                                       QBVHStackItem *ccl_restrict s3,
                                       QBVHStackItem *ccl_restrict s4)
{
  if (s2->dist < s1->dist) {
    qbvh_item_swap(s2, s1);
  }
  if (s4->dist < s3->dist) {
    qbvh_item_swap(s4, s3);
  }
  if (s3->dist < s1->dist) {
    qbvh_item_swap(s3, s1);
  }
  if (s4->dist < s2->dist) {
    qbvh_item_swap(s4, s2);
  }
  if (s3->dist < s2->dist) {
    qbvh_item_swap(s3, s2);
  }
}

/* Axis-aligned nodes intersection */

// ccl_device_inline int qbvh_aligned_node_intersect(KernelGlobals *ccl_restrict kg,
static int qbvh_aligned_node_intersect(KernelGlobals *ccl_restrict kg,
                                       const ssef &isect_near,
                                       const ssef &isect_far,
#ifdef __KERNEL_AVX2__
                                       const sse3f &org_idir,
#else
                                       const sse3f &org,
#endif
                                       const sse3f &idir,
                                       const int near_x,
                                       const int near_y,
                                       const int near_z,
                                       const int far_x,
                                       const int far_y,
                                       const int far_z,
                                       const int node_addr,
                                       ssef *ccl_restrict dist)
{
  const int offset = node_addr + 1;
#ifdef __KERNEL_AVX2__
  const ssef tnear_x = msub(
      kernel_tex_fetch_ssef(__bvh_nodes, offset + near_x), idir.x, org_idir.x);
  const ssef tnear_y = msub(
      kernel_tex_fetch_ssef(__bvh_nodes, offset + near_y), idir.y, org_idir.y);
  const ssef tnear_z = msub(
      kernel_tex_fetch_ssef(__bvh_nodes, offset + near_z), idir.z, org_idir.z);
  const ssef tfar_x = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset + far_x), idir.x, org_idir.x);
  const ssef tfar_y = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset + far_y), idir.y, org_idir.y);
  const ssef tfar_z = msub(kernel_tex_fetch_ssef(__bvh_nodes, offset + far_z), idir.z, org_idir.z);
#else
  const ssef tnear_x = (kernel_tex_fetch_ssef(__bvh_nodes, offset + near_x) - org.x) * idir.x;
  const ssef tnear_y = (kernel_tex_fetch_ssef(__bvh_nodes, offset + near_y) - org.y) * idir.y;
  const ssef tnear_z = (kernel_tex_fetch_ssef(__bvh_nodes, offset + near_z) - org.z) * idir.z;
  const ssef tfar_x = (kernel_tex_fetch_ssef(__bvh_nodes, offset + far_x) - org.x) * idir.x;
  const ssef tfar_y = (kernel_tex_fetch_ssef(__bvh_nodes, offset + far_y) - org.y) * idir.y;
  const ssef tfar_z = (kernel_tex_fetch_ssef(__bvh_nodes, offset + far_z) - org.z) * idir.z;
#endif

#ifdef __KERNEL_SSE41__
  const ssef tnear = maxi(maxi(tnear_x, tnear_y), maxi(tnear_z, isect_near));
  const ssef tfar = mini(mini(tfar_x, tfar_y), mini(tfar_z, isect_far));
  const sseb vmask = cast(tnear) > cast(tfar);
  int mask = (int)movemask(vmask) ^ 0xf;
#else
  const ssef tnear = max4(isect_near, tnear_x, tnear_y, tnear_z);
  const ssef tfar = min4(isect_far, tfar_x, tfar_y, tfar_z);
  const sseb vmask = tnear <= tfar;
  int mask = (int)movemask(vmask);
#endif
  *dist = tnear;
  return mask;
}

/* Unaligned nodes intersection */

ccl_device_inline int qbvh_unaligned_node_intersect(KernelGlobals *ccl_restrict kg,
                                                    const ssef &isect_near,
                                                    const ssef &isect_far,
#ifdef __KERNEL_AVX2__
                                                    const sse3f &org_idir,
#endif
                                                    const sse3f &org,
                                                    const sse3f &dir,
                                                    const sse3f &idir,
                                                    const int near_x,
                                                    const int near_y,
                                                    const int near_z,
                                                    const int far_x,
                                                    const int far_y,
                                                    const int far_z,
                                                    const int node_addr,
                                                    ssef *ccl_restrict dist)
{
  const int offset = node_addr;
  const ssef tfm_x_x = kernel_tex_fetch_ssef(__bvh_nodes, offset + 1);
  const ssef tfm_x_y = kernel_tex_fetch_ssef(__bvh_nodes, offset + 2);
  const ssef tfm_x_z = kernel_tex_fetch_ssef(__bvh_nodes, offset + 3);

  const ssef tfm_y_x = kernel_tex_fetch_ssef(__bvh_nodes, offset + 4);
  const ssef tfm_y_y = kernel_tex_fetch_ssef(__bvh_nodes, offset + 5);
  const ssef tfm_y_z = kernel_tex_fetch_ssef(__bvh_nodes, offset + 6);

  const ssef tfm_z_x = kernel_tex_fetch_ssef(__bvh_nodes, offset + 7);
  const ssef tfm_z_y = kernel_tex_fetch_ssef(__bvh_nodes, offset + 8);
  const ssef tfm_z_z = kernel_tex_fetch_ssef(__bvh_nodes, offset + 9);

  const ssef tfm_t_x = kernel_tex_fetch_ssef(__bvh_nodes, offset + 10);
  const ssef tfm_t_y = kernel_tex_fetch_ssef(__bvh_nodes, offset + 11);
  const ssef tfm_t_z = kernel_tex_fetch_ssef(__bvh_nodes, offset + 12);

  const ssef aligned_dir_x = dir.x * tfm_x_x + dir.y * tfm_x_y + dir.z * tfm_x_z,
             aligned_dir_y = dir.x * tfm_y_x + dir.y * tfm_y_y + dir.z * tfm_y_z,
             aligned_dir_z = dir.x * tfm_z_x + dir.y * tfm_z_y + dir.z * tfm_z_z;

  const ssef aligned_P_x = org.x * tfm_x_x + org.y * tfm_x_y + org.z * tfm_x_z + tfm_t_x,
             aligned_P_y = org.x * tfm_y_x + org.y * tfm_y_y + org.z * tfm_y_z + tfm_t_y,
             aligned_P_z = org.x * tfm_z_x + org.y * tfm_z_y + org.z * tfm_z_z + tfm_t_z;

  const ssef neg_one(-1.0f, -1.0f, -1.0f, -1.0f);
  const ssef nrdir_x = neg_one / aligned_dir_x, nrdir_y = neg_one / aligned_dir_y,
             nrdir_z = neg_one / aligned_dir_z;

  const ssef tlower_x = aligned_P_x * nrdir_x, tlower_y = aligned_P_y * nrdir_y,
             tlower_z = aligned_P_z * nrdir_z;

  const ssef tupper_x = tlower_x - nrdir_x, tupper_y = tlower_y - nrdir_y,
             tupper_z = tlower_z - nrdir_z;

#ifdef __KERNEL_SSE41__
  const ssef tnear_x = mini(tlower_x, tupper_x);
  const ssef tnear_y = mini(tlower_y, tupper_y);
  const ssef tnear_z = mini(tlower_z, tupper_z);
  const ssef tfar_x = maxi(tlower_x, tupper_x);
  const ssef tfar_y = maxi(tlower_y, tupper_y);
  const ssef tfar_z = maxi(tlower_z, tupper_z);
  const ssef tnear = max4(isect_near, tnear_x, tnear_y, tnear_z);
  const ssef tfar = min4(isect_far, tfar_x, tfar_y, tfar_z);
  const sseb vmask = tnear <= tfar;
  *dist = tnear;
  return movemask(vmask);
#else
  const ssef tnear_x = min(tlower_x, tupper_x);
  const ssef tnear_y = min(tlower_y, tupper_y);
  const ssef tnear_z = min(tlower_z, tupper_z);
  const ssef tfar_x = max(tlower_x, tupper_x);
  const ssef tfar_y = max(tlower_y, tupper_y);
  const ssef tfar_z = max(tlower_z, tupper_z);
  const ssef tnear = max4(isect_near, tnear_x, tnear_y, tnear_z);
  const ssef tfar = min4(isect_far, tfar_x, tfar_y, tfar_z);
  const sseb vmask = tnear <= tfar;
  *dist = tnear;
  return movemask(vmask);
#endif
}

/* Intersectors wrappers.
 *
 * They'll check node type and call appropriate intersection code.
 */

ccl_device_inline int qbvh_node_intersect(KernelGlobals *ccl_restrict kg,
                                          const ssef &isect_near,
                                          const ssef &isect_far,
#ifdef __KERNEL_AVX2__
                                          const sse3f &org_idir,
#endif
                                          const sse3f &org,
                                          const sse3f &dir,
                                          const sse3f &idir,
                                          const int near_x,
                                          const int near_y,
                                          const int near_z,
                                          const int far_x,
                                          const int far_y,
                                          const int far_z,
                                          const int node_addr,
                                          ssef *ccl_restrict dist)
{
  const int offset = node_addr;
  const float4 node = kernel_tex_fetch(__bvh_nodes, offset);
  if (__float_as_uint(node.x) & PATH_RAY_NODE_UNALIGNED) {
    return qbvh_unaligned_node_intersect(kg,
                                         isect_near,
                                         isect_far,
#ifdef __KERNEL_AVX2__
                                         org_idir,
#endif
                                         org,
                                         dir,
                                         idir,
                                         near_x,
                                         near_y,
                                         near_z,
                                         far_x,
                                         far_y,
                                         far_z,
                                         node_addr,
                                         dist);
  }
  else {
    return qbvh_aligned_node_intersect(kg,
                                       isect_near,
                                       isect_far,
#ifdef __KERNEL_AVX2__
                                       org_idir,
#else
                                       org,
#endif
                                       idir,
                                       near_x,
                                       near_y,
                                       near_z,
                                       far_x,
                                       far_y,
                                       far_z,
                                       node_addr,
                                       dist);
  }
}
