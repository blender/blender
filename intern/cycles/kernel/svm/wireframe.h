/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/geom/motion_triangle.h"
#include "kernel/geom/object.h"
#include "kernel/geom/triangle.h"
#include "kernel/svm/util.h"
#include "kernel/util/differential.h"

CCL_NAMESPACE_BEGIN

/* Wireframe Node */

ccl_device_inline float wireframe(KernelGlobals kg,
                                  ccl_private ShaderData *sd,
                                  const differential3 dP,
                                  const float size,
                                  const int pixel_size,
                                  ccl_private float3 *P)
{
#if defined(__HAIR__) || defined(__POINTCLOUD__)
  if (sd->prim != PRIM_NONE && sd->type & PRIMITIVE_TRIANGLE)
#else
  if (sd->prim != PRIM_NONE)
#endif
  {
    float3 Co[3];
    float pixelwidth = 1.0f;

    /* Triangles */
    const int np = 3;

    if (sd->type & PRIMITIVE_MOTION) {
      motion_triangle_vertices(kg, sd->object, sd->prim, sd->time, Co);
    }
    else {
      triangle_vertices(kg, sd->prim, Co);
    }

    if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      object_position_transform(kg, sd, &Co[0]);
      object_position_transform(kg, sd, &Co[1]);
      object_position_transform(kg, sd, &Co[2]);
    }

    if (pixel_size) {
      // Project the derivatives of P to the viewing plane defined
      // by I so we have a measure of how big is a pixel at this point
      const float pixelwidth_x = len(dP.dx - dot(dP.dx, sd->wi) * sd->wi);
      const float pixelwidth_y = len(dP.dy - dot(dP.dy, sd->wi) * sd->wi);
      // Take the average of both axis' length
      pixelwidth = (pixelwidth_x + pixelwidth_y) * 0.5f;
    }

    // Use half the width as the neighbor face will render the
    // other half. And take the square for fast comparison
    pixelwidth *= 0.5f * size;
    pixelwidth *= pixelwidth;
    for (int i = 0; i < np; i++) {
      const int i2 = i ? i - 1 : np - 1;
      const float3 dir = *P - Co[i];
      const float3 edge = Co[i] - Co[i2];
      const float3 crs = cross(edge, dir);
      // At this point dot(crs, crs) / dot(edge, edge) is
      // the square of area / length(edge) == square of the
      // distance to the edge.
      if (dot(crs, crs) < (dot(edge, edge) * pixelwidth)) {
        return 1.0f;
      }
    }
  }
  return 0.0f;
}

ccl_device_noinline void svm_node_wireframe(KernelGlobals kg,
                                            ccl_private ShaderData *sd,
                                            ccl_private float *stack,
                                            const uint4 node)
{
  const uint in_size = node.y;
  const uint out_fac = node.z;
  uint use_pixel_size;
  uint bump_offset;
  svm_unpack_node_uchar2(node.w, &use_pixel_size, &bump_offset);

  /* Input Data */
  const float size = stack_load_float(stack, in_size);
  const int pixel_size = (int)use_pixel_size;

  /* Calculate wireframe */
  const differential3 dP = differential_from_compact(sd->Ng, sd->dP);

  float3 P = sd->P;
  if (bump_offset == NODE_BUMP_OFFSET_DX) {
    P += dP.dx * BUMP_DX;
  }
  else if (bump_offset == NODE_BUMP_OFFSET_DY) {
    P += dP.dy * BUMP_DY;
  }

  const float f = wireframe(kg, sd, dP, size, pixel_size, &P);

  if (stack_valid(out_fac)) {
    stack_store_float(stack, out_fac, f);
  }
}

CCL_NAMESPACE_END
