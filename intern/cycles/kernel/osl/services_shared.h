/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Shared functions between OSL on CPU and GPU. */

#pragma once

#include "kernel/geom/motion_triangle.h"
#include "kernel/geom/triangle.h"

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_OPTIX__
typedef long long TypeDesc;
#endif

template<typename T>
ccl_device_inline bool set_attribute(const dual<T> v,
                                     const TypeDesc type,
                                     bool derivatives,
                                     ccl_private void *val);

ccl_device_inline void set_data_float(const dual1 data, bool derivatives, ccl_private void *val)
{
  ccl_private float *fval = static_cast<ccl_private float *>(val);
  fval[0] = data.val;
  if (derivatives) {
    fval[1] = data.dx;
    fval[2] = data.dy;
  }
}

ccl_device_inline void set_data_float3(const dual3 data, bool derivatives, ccl_private void *val)
{
  ccl_private float *fval = static_cast<ccl_private float *>(val);
  copy_v3_v3(fval, data.val);
  if (derivatives) {
    copy_v3_v3(fval + 3, data.dx);
    copy_v3_v3(fval + 6, data.dy);
  }
}

ccl_device_inline void set_data_float4(const dual4 data, bool derivatives, ccl_private void *val)
{
  ccl_private float *fval = static_cast<ccl_private float *>(val);
  copy_v4_v4(fval, data.val);
  if (derivatives) {
    copy_v4_v4(fval + 4, data.dx);
    copy_v4_v4(fval + 8, data.dy);
  }
}

ccl_device bool attribute_bump_map_normal(KernelGlobals kg,
                                          ccl_private const ShaderData *sd,
                                          ccl_private dual3 &f)
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
    f.val = triangle_smooth_normal(kg, Ng, sd->prim, sd->u, sd->v, sd->du, sd->dv, f.dx, f.dy);
  }
  else {
    assert(sd->type & PRIMITIVE_MOTION_TRIANGLE);
    f.val = motion_triangle_smooth_normal(
        kg, Ng, sd->object, sd->prim, sd->time, sd->u, sd->v, sd->du, sd->dv, f.dx, f.dy);
  }

  if (sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED) {
    /* Transform to local space. */
    object_inverse_normal_transform(kg, sd, &f.val);
    object_inverse_normal_transform(kg, sd, &f.dx);
    object_inverse_normal_transform(kg, sd, &f.dy);
  }

  if (backfacing) {
    f = -f;
  }

  f.dx -= f.val;
  f.dy -= f.val;

  return true;
}

CCL_NAMESPACE_END
