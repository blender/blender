/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Vector combine / separate, used for the RGB and XYZ nodes */

template<typename Float3Type>
ccl_device void svm_node_combine_vector(ccl_private float *ccl_restrict stack,
                                        const ccl_global SVMNodeCombineVector &ccl_restrict node)
{
  using FloatType = dual_scalar_t<Float3Type>;
  const FloatType value = stack_load<FloatType>(stack, node.in);

  if (stack_valid(node.out_offset)) {
    if constexpr (is_dual_v<Float3Type>) {
      stack_store_float(stack, node.out_offset + node.vector_index, value.val);
      stack_store_float(stack, node.out_offset + node.vector_index + 3, value.dx);
      stack_store_float(stack, node.out_offset + node.vector_index + 6, value.dy);
    }
    else {
      stack_store_float(stack, node.out_offset + node.vector_index, value);
    }
  }
}

template<typename Float3Type>
ccl_device void svm_node_separate_vector(ccl_private float *ccl_restrict stack,
                                         const ccl_global SVMNodeSeparateVector &ccl_restrict node)
{
  const Float3Type vector = stack_load<Float3Type>(stack, node.vector);

  if (stack_valid(node.out_offset)) {
    if constexpr (is_dual_v<Float3Type>) {
      if (node.vector_index == 0) {
        stack_store(stack, node.out_offset, vector.x());
      }
      else if (node.vector_index == 1) {
        stack_store(stack, node.out_offset, vector.y());
      }
      else {
        stack_store(stack, node.out_offset, vector.z());
      }
    }
    else {
      if (node.vector_index == 0) {
        stack_store(stack, node.out_offset, vector.x);
      }
      else if (node.vector_index == 1) {
        stack_store(stack, node.out_offset, vector.y);
      }
      else {
        stack_store(stack, node.out_offset, vector.z);
      }
    }
  }
}

template<typename Float3Type>
ccl_device void svm_node_get_vector_component(
    ccl_private float *ccl_restrict stack,
    const ccl_global SVMNodeGetVectorComponent &ccl_restrict node)
{
  const Float3Type vector = stack_load<Float3Type>(stack, node.vector);
  const int index = stack_load(stack, node.index);

  if (stack_valid(node.out_offset)) {
    if (index < 0 || index > 2) {
      stack_store(stack, node.out_offset, make_zero<dual_scalar_t<Float3Type>>());
    }
    else if constexpr (is_dual_v<Float3Type>) {
      if (index == 0) {
        stack_store(stack, node.out_offset, vector.x());
      }
      else if (index == 1) {
        stack_store(stack, node.out_offset, vector.y());
      }
      else {
        stack_store(stack, node.out_offset, vector.z());
      }
    }
    else {
      if (index == 0) {
        stack_store(stack, node.out_offset, vector.x);
      }
      else if (index == 1) {
        stack_store(stack, node.out_offset, vector.y);
      }
      else {
        stack_store(stack, node.out_offset, vector.z);
      }
    }
  }
}

CCL_NAMESPACE_END
