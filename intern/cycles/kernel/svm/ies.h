/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/util/ies.h"

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_ies(KernelGlobals kg,
                                      ccl_private ShaderData *sd,
                                      ccl_private float *stack,
                                      uint4 node)
{
  uint vector_offset, strength_offset, fac_offset, slot = node.z;
  svm_unpack_node_uchar3(node.y, &strength_offset, &vector_offset, &fac_offset);

  float3 vector = stack_load_float3(stack, vector_offset);
  float strength = stack_load_float_default(stack, strength_offset, node.w);

  vector = normalize(vector);
  float v_angle = safe_acosf(-vector.z);
  float h_angle = atan2f(vector.x, vector.y) + M_PI_F;

  float fac = strength * kernel_ies_interp(kg, slot, h_angle, v_angle);

  if (stack_valid(fac_offset)) {
    stack_store_float(stack, fac_offset, fac);
  }
}

CCL_NAMESPACE_END
