/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Shared functions between OSL on CPU and GPU. */

#pragma once

#include "kernel/geom/motion_triangle.h"
#include "kernel/geom/triangle.h"

CCL_NAMESPACE_BEGIN

/* TODO: deduplicate function `set_attribute_float3()` in CPU and GPU. */

ccl_device bool attribute_bump_map_normal(KernelGlobals kg,
                                          ccl_private const ShaderData *sd,
                                          float3 f[3])
{
  if (!(sd->type & PRIMITIVE_TRIANGLE) || !(sd->shader & SHADER_SMOOTH_NORMAL)) {
    /* TODO: implement for curve. */
    return false;
  }

  const bool backfacing = (sd->flag & SD_BACKFACING);

  /* Fallback when the smooth normal is zero. */
  float3 Ng = backfacing ? -sd->Ng : sd->Ng;
  object_inverse_normal_transform(kg, sd, &Ng);

  if (sd->type == PRIMITIVE_TRIANGLE) {
    f[0] = triangle_smooth_normal(kg, Ng, sd->prim, sd->u, sd->v, sd->du, sd->dv, f[1], f[2]);
  }
  else {
    assert(sd->type & PRIMITIVE_MOTION_TRIANGLE);
    f[0] = motion_triangle_smooth_normal(
        kg, Ng, sd->object, sd->prim, sd->time, sd->u, sd->v, sd->du, sd->dv, f[1], f[2]);
  }

  if (sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED) {
    /* Transform to local space. */
    object_inverse_normal_transform(kg, sd, f);
    object_inverse_normal_transform(kg, sd, f + 1);
    object_inverse_normal_transform(kg, sd, f + 2);
  }

  if (backfacing) {
    f[0] = -f[0];
    f[1] = -f[1];
    f[2] = -f[2];
  }

  f[1] -= f[0];
  f[2] -= f[0];

  return true;
}

CCL_NAMESPACE_END
