/*
 * Copyright 2011-2016, Blender Foundation.
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

// TODO(sergey): Look into avoid use of full Transform and use 3x3 matrix and
// 3-vector which might be faster.
ccl_device_forceinline Transform bvh_unaligned_node_fetch_space(KernelGlobals *kg,
                                                                int node_addr,
                                                                int child)
{
  Transform space;
  const int child_addr = node_addr + child * 3;
  space.x = kernel_tex_fetch(__bvh_nodes, child_addr + 1);
  space.y = kernel_tex_fetch(__bvh_nodes, child_addr + 2);
  space.z = kernel_tex_fetch(__bvh_nodes, child_addr + 3);
  return space;
}

#if !defined(__KERNEL_SSE2__)
ccl_device_forceinline int bvh_aligned_node_intersect(KernelGlobals *kg,
                                                      const float3 P,
                                                      const float3 idir,
                                                      const float t,
                                                      const int node_addr,
                                                      const uint visibility,
                                                      float dist[2])
{

  /* fetch node data */
#  ifdef __VISIBILITY_FLAG__
  float4 cnodes = kernel_tex_fetch(__bvh_nodes, node_addr + 0);
#  endif
  float4 node0 = kernel_tex_fetch(__bvh_nodes, node_addr + 1);
  float4 node1 = kernel_tex_fetch(__bvh_nodes, node_addr + 2);
  float4 node2 = kernel_tex_fetch(__bvh_nodes, node_addr + 3);

  /* intersect ray against child nodes */
  float c0lox = (node0.x - P.x) * idir.x;
  float c0hix = (node0.z - P.x) * idir.x;
  float c0loy = (node1.x - P.y) * idir.y;
  float c0hiy = (node1.z - P.y) * idir.y;
  float c0loz = (node2.x - P.z) * idir.z;
  float c0hiz = (node2.z - P.z) * idir.z;
  float c0min = max4(0.0f, min(c0lox, c0hix), min(c0loy, c0hiy), min(c0loz, c0hiz));
  float c0max = min4(t, max(c0lox, c0hix), max(c0loy, c0hiy), max(c0loz, c0hiz));

  float c1lox = (node0.y - P.x) * idir.x;
  float c1hix = (node0.w - P.x) * idir.x;
  float c1loy = (node1.y - P.y) * idir.y;
  float c1hiy = (node1.w - P.y) * idir.y;
  float c1loz = (node2.y - P.z) * idir.z;
  float c1hiz = (node2.w - P.z) * idir.z;
  float c1min = max4(0.0f, min(c1lox, c1hix), min(c1loy, c1hiy), min(c1loz, c1hiz));
  float c1max = min4(t, max(c1lox, c1hix), max(c1loy, c1hiy), max(c1loz, c1hiz));

  dist[0] = c0min;
  dist[1] = c1min;

#  ifdef __VISIBILITY_FLAG__
  /* this visibility test gives a 5% performance hit, how to solve? */
  return (((c0max >= c0min) && (__float_as_uint(cnodes.x) & visibility)) ? 1 : 0) |
         (((c1max >= c1min) && (__float_as_uint(cnodes.y) & visibility)) ? 2 : 0);
#  else
  return ((c0max >= c0min) ? 1 : 0) | ((c1max >= c1min) ? 2 : 0);
#  endif
}

ccl_device_forceinline bool bvh_unaligned_node_intersect_child(KernelGlobals *kg,
                                                               const float3 P,
                                                               const float3 dir,
                                                               const float t,
                                                               int node_addr,
                                                               int child,
                                                               float dist[2])
{
  Transform space = bvh_unaligned_node_fetch_space(kg, node_addr, child);
  float3 aligned_dir = transform_direction(&space, dir);
  float3 aligned_P = transform_point(&space, P);
  float3 nrdir = -bvh_inverse_direction(aligned_dir);
  float3 lower_xyz = aligned_P * nrdir;
  float3 upper_xyz = lower_xyz - nrdir;
  const float near_x = min(lower_xyz.x, upper_xyz.x);
  const float near_y = min(lower_xyz.y, upper_xyz.y);
  const float near_z = min(lower_xyz.z, upper_xyz.z);
  const float far_x = max(lower_xyz.x, upper_xyz.x);
  const float far_y = max(lower_xyz.y, upper_xyz.y);
  const float far_z = max(lower_xyz.z, upper_xyz.z);
  const float tnear = max4(0.0f, near_x, near_y, near_z);
  const float tfar = min4(t, far_x, far_y, far_z);
  *dist = tnear;
  return tnear <= tfar;
}

ccl_device_forceinline int bvh_unaligned_node_intersect(KernelGlobals *kg,
                                                        const float3 P,
                                                        const float3 dir,
                                                        const float3 idir,
                                                        const float t,
                                                        const int node_addr,
                                                        const uint visibility,
                                                        float dist[2])
{
  int mask = 0;
#  ifdef __VISIBILITY_FLAG__
  float4 cnodes = kernel_tex_fetch(__bvh_nodes, node_addr + 0);
#  endif
  if (bvh_unaligned_node_intersect_child(kg, P, dir, t, node_addr, 0, &dist[0])) {
#  ifdef __VISIBILITY_FLAG__
    if ((__float_as_uint(cnodes.x) & visibility))
#  endif
    {
      mask |= 1;
    }
  }
  if (bvh_unaligned_node_intersect_child(kg, P, dir, t, node_addr, 1, &dist[1])) {
#  ifdef __VISIBILITY_FLAG__
    if ((__float_as_uint(cnodes.y) & visibility))
#  endif
    {
      mask |= 2;
    }
  }
  return mask;
}

ccl_device_forceinline int bvh_node_intersect(KernelGlobals *kg,
                                              const float3 P,
                                              const float3 dir,
                                              const float3 idir,
                                              const float t,
                                              const int node_addr,
                                              const uint visibility,
                                              float dist[2])
{
  float4 node = kernel_tex_fetch(__bvh_nodes, node_addr);
  if (__float_as_uint(node.x) & PATH_RAY_NODE_UNALIGNED) {
    return bvh_unaligned_node_intersect(kg, P, dir, idir, t, node_addr, visibility, dist);
  }
  else {
    return bvh_aligned_node_intersect(kg, P, idir, t, node_addr, visibility, dist);
  }
}

#else /* !defined(__KERNEL_SSE2__) */

int ccl_device_forceinline bvh_aligned_node_intersect(KernelGlobals *kg,
                                                      const float3 &P,
                                                      const float3 &dir,
                                                      const ssef &tsplat,
                                                      const ssef Psplat[3],
                                                      const ssef idirsplat[3],
                                                      const shuffle_swap_t shufflexyz[3],
                                                      const int node_addr,
                                                      const uint visibility,
                                                      float dist[2])
{
  /* Intersect two child bounding boxes, SSE3 version adapted from Embree */
  const ssef pn = cast(ssei(0, 0, 0x80000000, 0x80000000));

  /* fetch node data */
  const ssef *bvh_nodes = (ssef *)kg->__bvh_nodes.data + node_addr;

  /* intersect ray against child nodes */
  const ssef tminmaxx = (shuffle_swap(bvh_nodes[1], shufflexyz[0]) - Psplat[0]) * idirsplat[0];
  const ssef tminmaxy = (shuffle_swap(bvh_nodes[2], shufflexyz[1]) - Psplat[1]) * idirsplat[1];
  const ssef tminmaxz = (shuffle_swap(bvh_nodes[3], shufflexyz[2]) - Psplat[2]) * idirsplat[2];

  /* calculate { c0min, c1min, -c0max, -c1max} */
  ssef minmax = max(max(tminmaxx, tminmaxy), max(tminmaxz, tsplat));
  const ssef tminmax = minmax ^ pn;
  const sseb lrhit = tminmax <= shuffle<2, 3, 0, 1>(tminmax);

  dist[0] = tminmax[0];
  dist[1] = tminmax[1];

  int mask = movemask(lrhit);

#  ifdef __VISIBILITY_FLAG__
  /* this visibility test gives a 5% performance hit, how to solve? */
  float4 cnodes = kernel_tex_fetch(__bvh_nodes, node_addr + 0);
  int cmask = (((mask & 1) && (__float_as_uint(cnodes.x) & visibility)) ? 1 : 0) |
              (((mask & 2) && (__float_as_uint(cnodes.y) & visibility)) ? 2 : 0);
  return cmask;
#  else
  return mask & 3;
#  endif
}

ccl_device_forceinline int bvh_unaligned_node_intersect(KernelGlobals *kg,
                                                        const float3 P,
                                                        const float3 dir,
                                                        const ssef &isect_near,
                                                        const ssef &isect_far,
                                                        const int node_addr,
                                                        const uint visibility,
                                                        float dist[2])
{
  Transform space0 = bvh_unaligned_node_fetch_space(kg, node_addr, 0);
  Transform space1 = bvh_unaligned_node_fetch_space(kg, node_addr, 1);

  float3 aligned_dir0 = transform_direction(&space0, dir),
         aligned_dir1 = transform_direction(&space1, dir);
  float3 aligned_P0 = transform_point(&space0, P), aligned_P1 = transform_point(&space1, P);
  float3 nrdir0 = -bvh_inverse_direction(aligned_dir0),
         nrdir1 = -bvh_inverse_direction(aligned_dir1);

  ssef lower_x = ssef(aligned_P0.x * nrdir0.x, aligned_P1.x * nrdir1.x, 0.0f, 0.0f),
       lower_y = ssef(aligned_P0.y * nrdir0.y, aligned_P1.y * nrdir1.y, 0.0f, 0.0f),
       lower_z = ssef(aligned_P0.z * nrdir0.z, aligned_P1.z * nrdir1.z, 0.0f, 0.0f);

  ssef upper_x = lower_x - ssef(nrdir0.x, nrdir1.x, 0.0f, 0.0f),
       upper_y = lower_y - ssef(nrdir0.y, nrdir1.y, 0.0f, 0.0f),
       upper_z = lower_z - ssef(nrdir0.z, nrdir1.z, 0.0f, 0.0f);

  ssef tnear_x = min(lower_x, upper_x);
  ssef tnear_y = min(lower_y, upper_y);
  ssef tnear_z = min(lower_z, upper_z);
  ssef tfar_x = max(lower_x, upper_x);
  ssef tfar_y = max(lower_y, upper_y);
  ssef tfar_z = max(lower_z, upper_z);

  const ssef tnear = max4(isect_near, tnear_x, tnear_y, tnear_z);
  const ssef tfar = min4(isect_far, tfar_x, tfar_y, tfar_z);
  sseb vmask = tnear <= tfar;
  dist[0] = tnear.f[0];
  dist[1] = tnear.f[1];

  int mask = (int)movemask(vmask);

#  ifdef __VISIBILITY_FLAG__
  /* this visibility test gives a 5% performance hit, how to solve? */
  float4 cnodes = kernel_tex_fetch(__bvh_nodes, node_addr + 0);
  int cmask = (((mask & 1) && (__float_as_uint(cnodes.x) & visibility)) ? 1 : 0) |
              (((mask & 2) && (__float_as_uint(cnodes.y) & visibility)) ? 2 : 0);
  return cmask;
#  else
  return mask & 3;
#  endif
}

ccl_device_forceinline int bvh_node_intersect(KernelGlobals *kg,
                                              const float3 &P,
                                              const float3 &dir,
                                              const ssef &isect_near,
                                              const ssef &isect_far,
                                              const ssef &tsplat,
                                              const ssef Psplat[3],
                                              const ssef idirsplat[3],
                                              const shuffle_swap_t shufflexyz[3],
                                              const int node_addr,
                                              const uint visibility,
                                              float dist[2])
{
  float4 node = kernel_tex_fetch(__bvh_nodes, node_addr);
  if (__float_as_uint(node.x) & PATH_RAY_NODE_UNALIGNED) {
    return bvh_unaligned_node_intersect(
        kg, P, dir, isect_near, isect_far, node_addr, visibility, dist);
  }
  else {
    return bvh_aligned_node_intersect(
        kg, P, dir, tsplat, Psplat, idirsplat, shufflexyz, node_addr, visibility, dist);
  }
}
#endif /* !defined(__KERNEL_SSE2__) */
