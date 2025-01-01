/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Vector combine / separate, used for the RGB and XYZ nodes */

ccl_device void svm_node_combine_vector(ccl_private ShaderData *sd,
                                        ccl_private float *stack,
                                        const uint in_offset,
                                        const uint vector_index,
                                        const uint out_offset)
{
  const float vector = stack_load_float(stack, in_offset);

  if (stack_valid(out_offset)) {
    stack_store_float(stack, out_offset + vector_index, vector);
  }
}

ccl_device void svm_node_separate_vector(ccl_private ShaderData *sd,
                                         ccl_private float *stack,
                                         const uint ivector_offset,
                                         const uint vector_index,
                                         const uint out_offset)
{
  const float3 vector = stack_load_float3(stack, ivector_offset);

  if (stack_valid(out_offset)) {
    if (vector_index == 0) {
      stack_store_float(stack, out_offset, vector.x);
    }
    else if (vector_index == 1) {
      stack_store_float(stack, out_offset, vector.y);
    }
    else {
      stack_store_float(stack, out_offset, vector.z);
    }
  }
}

CCL_NAMESPACE_END
