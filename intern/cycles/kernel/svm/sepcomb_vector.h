/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Vector combine / separate, used for the RGB and XYZ nodes */

ccl_device void svm_node_combine_vector(ccl_private float *stack,
                                        const uint in_offset,
                                        const uint vector_index,
                                        const uint out_offset,
                                        const bool derivative)
{
  const dual1 vector = stack_load_float(stack, in_offset, derivative);

  if (stack_valid(out_offset)) {
    stack_store_float(stack, out_offset + vector_index, vector.val);
    if (derivative) {
      stack_store_float(stack, out_offset + vector_index + 3, vector.dx);
      stack_store_float(stack, out_offset + vector_index + 6, vector.dy);
    }
  }
}

ccl_device void svm_node_separate_vector(ccl_private float *stack,
                                         const uint ivector_offset,
                                         const uint vector_index,
                                         const uint out_offset,
                                         const bool derivative)
{
  const dual3 vector = stack_load_float3(stack, ivector_offset, derivative);

  if (stack_valid(out_offset)) {
    if (vector_index == 0) {
      stack_store_float(stack, out_offset, vector.x(), derivative);
    }
    else if (vector_index == 1) {
      stack_store_float(stack, out_offset, vector.y(), derivative);
    }
    else {
      stack_store_float(stack, out_offset, vector.z(), derivative);
    }
  }
}

CCL_NAMESPACE_END
