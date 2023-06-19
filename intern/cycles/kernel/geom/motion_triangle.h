/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Motion Triangle Primitive
 *
 * These are stored as regular triangles, plus extra positions and normals at
 * times other than the frame center. Computing the triangle vertex positions
 * or normals at a given ray time is a matter of interpolation of the two steps
 * between which the ray time lies.
 *
 * The extra positions and normals are stored as ATTR_STD_MOTION_VERTEX_POSITION
 * and ATTR_STD_MOTION_VERTEX_NORMAL mesh attributes.
 */

#pragma once

#include "kernel/bvh/util.h"

CCL_NAMESPACE_BEGIN

/* Time interpolation of vertex positions and normals */

ccl_device_inline void motion_triangle_verts_for_step(KernelGlobals kg,
                                                      uint3 tri_vindex,
                                                      int offset,
                                                      int numverts,
                                                      int numsteps,
                                                      int step,
                                                      float3 verts[3])
{
  if (step == numsteps) {
    /* center step: regular vertex location */
    verts[0] = kernel_data_fetch(tri_verts, tri_vindex.x);
    verts[1] = kernel_data_fetch(tri_verts, tri_vindex.y);
    verts[2] = kernel_data_fetch(tri_verts, tri_vindex.z);
  }
  else {
    /* center step not store in this array */
    if (step > numsteps)
      step--;

    offset += step * numverts;

    verts[0] = kernel_data_fetch(attributes_float3, offset + tri_vindex.x);
    verts[1] = kernel_data_fetch(attributes_float3, offset + tri_vindex.y);
    verts[2] = kernel_data_fetch(attributes_float3, offset + tri_vindex.z);
  }
}

ccl_device_inline void motion_triangle_normals_for_step(KernelGlobals kg,
                                                        uint3 tri_vindex,
                                                        int offset,
                                                        int numverts,
                                                        int numsteps,
                                                        int step,
                                                        float3 normals[3])
{
  if (step == numsteps) {
    /* center step: regular vertex location */
    normals[0] = kernel_data_fetch(tri_vnormal, tri_vindex.x);
    normals[1] = kernel_data_fetch(tri_vnormal, tri_vindex.y);
    normals[2] = kernel_data_fetch(tri_vnormal, tri_vindex.z);
  }
  else {
    /* center step is not stored in this array */
    if (step > numsteps)
      step--;

    offset += step * numverts;

    normals[0] = kernel_data_fetch(attributes_float3, offset + tri_vindex.x);
    normals[1] = kernel_data_fetch(attributes_float3, offset + tri_vindex.y);
    normals[2] = kernel_data_fetch(attributes_float3, offset + tri_vindex.z);
  }
}

ccl_device_inline void motion_triangle_compute_info(KernelGlobals kg,
                                                    int object,
                                                    float time,
                                                    int prim,
                                                    ccl_private uint3 *tri_vindex,
                                                    ccl_private int *numsteps,
                                                    ccl_private int *numverts,
                                                    ccl_private int *step,
                                                    ccl_private float *t)
{
  /* Get object motion info. */
  object_motion_info(kg, object, numsteps, numverts, NULL);

  /* Figure out which steps we need to fetch and their interpolation factor. */
  int maxstep = *numsteps * 2;
  *step = min((int)(time * maxstep), maxstep - 1);
  *t = time * maxstep - *step;

  /* Get triangle indices. */
  *tri_vindex = kernel_data_fetch(tri_vindex, prim);
}

ccl_device_inline void motion_triangle_vertices(KernelGlobals kg,
                                                int object,
                                                uint3 tri_vindex,
                                                int numsteps,
                                                int numverts,
                                                int step,
                                                float t,
                                                float3 verts[3])
{
  /* Find attribute. */
  int offset = intersection_find_attribute(kg, object, ATTR_STD_MOTION_VERTEX_POSITION);
  kernel_assert(offset != ATTR_STD_NOT_FOUND);

  /* Fetch vertex coordinates. */
  float3 next_verts[3];
  motion_triangle_verts_for_step(kg, tri_vindex, offset, numverts, numsteps, step, verts);
  motion_triangle_verts_for_step(kg, tri_vindex, offset, numverts, numsteps, step + 1, next_verts);

  /* Interpolate between steps. */
  verts[0] = (1.0f - t) * verts[0] + t * next_verts[0];
  verts[1] = (1.0f - t) * verts[1] + t * next_verts[1];
  verts[2] = (1.0f - t) * verts[2] + t * next_verts[2];
}

ccl_device_inline void motion_triangle_vertices(
    KernelGlobals kg, int object, int prim, float time, float3 verts[3])
{
  int numsteps, numverts, step;
  float t;
  uint3 tri_vindex;
  motion_triangle_compute_info(
      kg, object, time, prim, &tri_vindex, &numsteps, &numverts, &step, &t);

  motion_triangle_vertices(kg, object, tri_vindex, numsteps, numverts, step, t, verts);
}

ccl_device_inline void motion_triangle_normals(KernelGlobals kg,
                                               int object,
                                               uint3 tri_vindex,
                                               int numsteps,
                                               int numverts,
                                               int step,
                                               float t,
                                               float3 normals[3])
{
  /* Find attribute. */
  int offset = intersection_find_attribute(kg, object, ATTR_STD_MOTION_VERTEX_NORMAL);
  kernel_assert(offset != ATTR_STD_NOT_FOUND);

  /* Fetch normals. */
  float3 next_normals[3];
  motion_triangle_normals_for_step(kg, tri_vindex, offset, numverts, numsteps, step, normals);
  motion_triangle_normals_for_step(
      kg, tri_vindex, offset, numverts, numsteps, step + 1, next_normals);

  /* Interpolate between steps. */
  normals[0] = normalize((1.0f - t) * normals[0] + t * next_normals[0]);
  normals[1] = normalize((1.0f - t) * normals[1] + t * next_normals[1]);
  normals[2] = normalize((1.0f - t) * normals[2] + t * next_normals[2]);
}

ccl_device_inline void motion_triangle_vertices_and_normals(
    KernelGlobals kg, int object, int prim, float time, float3 verts[3], float3 normals[3])
{
  int numsteps, numverts, step;
  float t;
  uint3 tri_vindex;
  motion_triangle_compute_info(
      kg, object, time, prim, &tri_vindex, &numsteps, &numverts, &step, &t);

  motion_triangle_vertices(kg, object, tri_vindex, numsteps, numverts, step, t, verts);
  motion_triangle_normals(kg, object, tri_vindex, numsteps, numverts, step, t, normals);
}

ccl_device_inline float3 motion_triangle_smooth_normal(KernelGlobals kg,
                                                       float3 Ng,
                                                       int object,
                                                       uint3 tri_vindex,
                                                       int numsteps,
                                                       int numverts,
                                                       int step,
                                                       float t,
                                                       float u,
                                                       float v)
{
  float3 normals[3];
  motion_triangle_normals(kg, object, tri_vindex, numsteps, numverts, step, t, normals);

  /* Interpolate between normals. */
  float w = 1.0f - u - v;
  float3 N = safe_normalize(w * normals[0] + u * normals[1] + v * normals[2]);

  return is_zero(N) ? Ng : N;
}

ccl_device_inline float3 motion_triangle_smooth_normal(
    KernelGlobals kg, float3 Ng, int object, int prim, float u, float v, float time)
{
  int numsteps, numverts, step;
  float t;
  uint3 tri_vindex;
  motion_triangle_compute_info(
      kg, object, time, prim, &tri_vindex, &numsteps, &numverts, &step, &t);

  return motion_triangle_smooth_normal(
      kg, Ng, object, tri_vindex, numsteps, numverts, step, t, u, v);
}

CCL_NAMESPACE_END
