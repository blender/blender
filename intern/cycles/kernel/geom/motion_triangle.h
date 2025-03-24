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

#include "kernel/geom/triangle.h"

CCL_NAMESPACE_BEGIN

/* Time interpolation of vertex positions and normals */

ccl_device_inline void motion_triangle_verts_for_step(KernelGlobals kg,
                                                      const uint3 tri_vindex,
                                                      int offset,
                                                      const int numverts,
                                                      const int numsteps,
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
    if (step > numsteps) {
      step--;
    }

    offset += step * numverts;

    verts[0] = kernel_data_fetch(attributes_float3, offset + tri_vindex.x);
    verts[1] = kernel_data_fetch(attributes_float3, offset + tri_vindex.y);
    verts[2] = kernel_data_fetch(attributes_float3, offset + tri_vindex.z);
  }
}

ccl_device_inline void motion_triangle_normals_for_step(KernelGlobals kg,
                                                        const uint3 tri_vindex,
                                                        int offset,
                                                        const int numverts,
                                                        const int numsteps,
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
    if (step > numsteps) {
      step--;
    }

    offset += step * numverts;

    normals[0] = kernel_data_fetch(attributes_float3, offset + tri_vindex.x);
    normals[1] = kernel_data_fetch(attributes_float3, offset + tri_vindex.y);
    normals[2] = kernel_data_fetch(attributes_float3, offset + tri_vindex.z);
  }
}

ccl_device_inline void motion_triangle_compute_info(KernelGlobals kg,
                                                    const int object,
                                                    const float time,
                                                    const int prim,
                                                    ccl_private uint3 *tri_vindex,
                                                    ccl_private int *numsteps,
                                                    ccl_private int *step,
                                                    ccl_private float *t)
{
  /* Get object motion info. */
  *numsteps = kernel_data_fetch(objects, object).num_geom_steps;

  /* Figure out which steps we need to fetch and their interpolation factor. */
  const int maxstep = *numsteps * 2;
  *step = min((int)(time * maxstep), maxstep - 1);
  *t = time * maxstep - *step;

  /* Get triangle indices. */
  *tri_vindex = kernel_data_fetch(tri_vindex, prim);
}

ccl_device_inline void motion_triangle_vertices(KernelGlobals kg,
                                                const int object,
                                                const uint3 tri_vindex,
                                                const int numsteps,
                                                const int numverts,
                                                const int step,
                                                const float t,
                                                float3 verts[3])
{
  /* Find attribute. */
  const int offset = intersection_find_attribute(kg, object, ATTR_STD_MOTION_VERTEX_POSITION);
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
    KernelGlobals kg, const int object, const int prim, const float time, float3 verts[3])
{
  int numsteps;
  int step;
  float t;
  uint3 tri_vindex;
  motion_triangle_compute_info(kg, object, time, prim, &tri_vindex, &numsteps, &step, &t);

  const int numverts = kernel_data_fetch(objects, object).numverts;
  motion_triangle_vertices(kg, object, tri_vindex, numsteps, numverts, step, t, verts);
}

ccl_device_inline void motion_triangle_normals(KernelGlobals kg,
                                               const int object,
                                               const uint3 tri_vindex,
                                               const int numsteps,
                                               const int numverts,
                                               const int step,
                                               const float t,
                                               float3 normals[3])
{
  /* Find attribute. */
  const int offset = intersection_find_attribute(kg, object, ATTR_STD_MOTION_VERTEX_NORMAL);
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

ccl_device_inline void motion_triangle_vertices_and_normals(KernelGlobals kg,
                                                            const int object,
                                                            const int prim,
                                                            const float time,
                                                            float3 verts[3],
                                                            float3 normals[3])
{
  int numsteps;
  int step;
  float t;
  uint3 tri_vindex;
  motion_triangle_compute_info(kg, object, time, prim, &tri_vindex, &numsteps, &step, &t);

  const int numverts = kernel_data_fetch(objects, object).numverts;
  motion_triangle_vertices(kg, object, tri_vindex, numsteps, numverts, step, t, verts);
  motion_triangle_normals(kg, object, tri_vindex, numsteps, numverts, step, t, normals);
}

ccl_device_inline float3 motion_triangle_smooth_normal(KernelGlobals kg,
                                                       const float3 Ng,
                                                       const int object,
                                                       const uint3 tri_vindex,
                                                       const int numsteps,
                                                       const int step,
                                                       const float t,
                                                       const float u,
                                                       const float v)
{
  float3 normals[3];
  const int numverts = kernel_data_fetch(objects, object).numverts;
  motion_triangle_normals(kg, object, tri_vindex, numsteps, numverts, step, t, normals);

  /* Interpolate between normals. */
  const float w = 1.0f - u - v;
  const float3 N = safe_normalize(w * normals[0] + u * normals[1] + v * normals[2]);

  return is_zero(N) ? Ng : N;
}

ccl_device_inline float3 motion_triangle_smooth_normal(KernelGlobals kg,
                                                       const float3 Ng,
                                                       const int object,
                                                       const int prim,
                                                       const float u,
                                                       float v,
                                                       const float time)
{
  int numsteps;
  int step;
  float t;
  uint3 tri_vindex;
  motion_triangle_compute_info(kg, object, time, prim, &tri_vindex, &numsteps, &step, &t);

  return motion_triangle_smooth_normal(kg, Ng, object, tri_vindex, numsteps, step, t, u, v);
}

/* Compute motion triangle normals at the hit position, and offsetted positions in x and y
 * direction for bump mapping. */
ccl_device_inline float3 motion_triangle_smooth_normal(KernelGlobals kg,
                                                       const float3 Ng,
                                                       const int object,
                                                       const int prim,
                                                       const float time,
                                                       const float u,
                                                       const float v,
                                                       const differential du,
                                                       const differential dv,
                                                       ccl_private float3 &N_x,
                                                       ccl_private float3 &N_y)
{
  int numsteps, step;
  float t;
  uint3 tri_vindex;
  motion_triangle_compute_info(kg, object, time, prim, &tri_vindex, &numsteps, &step, &t);

  float3 n[3];
  const int numverts = kernel_data_fetch(objects, object).numverts;
  motion_triangle_normals(kg, object, tri_vindex, numsteps, numverts, step, t, n);

  const float3 N = safe_normalize(triangle_interpolate(u, v, n[0], n[1], n[2]));
  N_x = safe_normalize(triangle_interpolate(u + du.dx, v + dv.dx, n[0], n[1], n[2]));
  N_y = safe_normalize(triangle_interpolate(u + du.dy, v + dv.dy, n[0], n[1], n[2]));

  N_x = is_zero(N_x) ? Ng : N_x;
  N_y = is_zero(N_y) ? Ng : N_y;
  return is_zero(N) ? Ng : N;
}

CCL_NAMESPACE_END
