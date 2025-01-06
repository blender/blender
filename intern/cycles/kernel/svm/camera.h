/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_camera(KernelGlobals kg,
                                         ccl_private ShaderData *sd,
                                         ccl_private float *stack,
                                         const uint out_vector,
                                         const uint out_zdepth,
                                         const uint out_distance)
{
  float distance;
  float zdepth;
  float3 vector;

  const Transform tfm = kernel_data.cam.worldtocamera;
  vector = transform_point(&tfm, sd->P);
  zdepth = vector.z;
  distance = len(vector);

  if (stack_valid(out_vector)) {
    stack_store_float3(stack, out_vector, normalize(vector));
  }

  if (stack_valid(out_zdepth)) {
    stack_store_float(stack, out_zdepth, zdepth);
  }

  if (stack_valid(out_distance)) {
    stack_store_float(stack, out_distance, distance);
  }
}

CCL_NAMESPACE_END
