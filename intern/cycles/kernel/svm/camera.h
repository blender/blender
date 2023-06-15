/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_camera(KernelGlobals kg,
                                         ccl_private ShaderData *sd,
                                         ccl_private float *stack,
                                         uint out_vector,
                                         uint out_zdepth,
                                         uint out_distance)
{
  float distance;
  float zdepth;
  float3 vector;

  Transform tfm = kernel_data.cam.worldtocamera;
  vector = transform_point(&tfm, sd->P);
  zdepth = vector.z;
  distance = len(vector);

  if (stack_valid(out_vector))
    stack_store_float3(stack, out_vector, normalize(vector));

  if (stack_valid(out_zdepth))
    stack_store_float(stack, out_zdepth, zdepth);

  if (stack_valid(out_distance))
    stack_store_float(stack, out_distance, distance);
}

CCL_NAMESPACE_END
