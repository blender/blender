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

#include "kernel/globals.h"
#include "kernel/types.h"

#include "kernel/geom/motion_triangle.h"
#include "kernel/geom/motion_triangle_intersect.h"

CCL_NAMESPACE_BEGIN

/* Setup of motion triangle specific parts of ShaderData, moved into this one
 * function to more easily share computation of interpolated positions and
 * normals */

/* return 3 triangle vertex normals */
ccl_device_noinline void motion_triangle_shader_setup(KernelGlobals kg, ccl_private ShaderData *sd)
{
  /* Get shader. */
  sd->shader = kernel_data_fetch(tri_shader, sd->prim);

  /* Compute motion info. */
  int numsteps;
  int step;
  float t;
  uint3 tri_vindex;
  motion_triangle_compute_info(
      kg, sd->object, sd->time, sd->prim, &tri_vindex, &numsteps, &step, &t);

  float3 verts[3];
  const int numverts = kernel_data_fetch(objects, sd->object).numverts;
  motion_triangle_vertices(kg, sd->object, tri_vindex, numsteps, numverts, step, t, verts);

  /* Compute refined position. */
  sd->P = motion_triangle_point_from_uv(kg, sd, sd->u, sd->v, verts);
  /* Compute face normal. */
  float3 Ng;
  if (object_negative_scale_applied(sd->object_flag)) {
    Ng = normalize(cross(verts[2] - verts[0], verts[1] - verts[0]));
  }
  else {
    Ng = normalize(cross(verts[1] - verts[0], verts[2] - verts[0]));
  }
  sd->Ng = Ng;
  sd->N = Ng;
  /* Compute derivatives of P w.r.t. uv. */
#ifdef __DPDU__
  sd->dPdu = (verts[1] - verts[0]);
  sd->dPdv = (verts[2] - verts[0]);
#endif
  /* Compute smooth normal. */
  if (sd->shader & SHADER_SMOOTH_NORMAL) {
    sd->N = motion_triangle_smooth_normal(
        kg, Ng, sd->object, tri_vindex, numsteps, step, t, sd->u, sd->v);
  }
}

CCL_NAMESPACE_END
