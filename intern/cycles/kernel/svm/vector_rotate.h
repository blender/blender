/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Vector Rotate */

ccl_device_noinline void svm_node_vector_rotate(ccl_private ShaderData *sd,
                                                ccl_private float *stack,
                                                const uint input_stack_offsets,
                                                const uint axis_stack_offsets,
                                                const uint result_stack_offset)
{
  uint type;
  uint vector_stack_offset;
  uint rotation_stack_offset;
  uint center_stack_offset;
  uint axis_stack_offset;
  uint angle_stack_offset;
  uint invert;

  svm_unpack_node_uchar4(
      input_stack_offsets, &type, &vector_stack_offset, &rotation_stack_offset, &invert);
  svm_unpack_node_uchar3(
      axis_stack_offsets, &center_stack_offset, &axis_stack_offset, &angle_stack_offset);

  if (stack_valid(result_stack_offset)) {

    const float3 vector = stack_load_float3(stack, vector_stack_offset);
    const float3 center = stack_load_float3(stack, center_stack_offset);
    float3 result = make_float3(0.0f, 0.0f, 0.0f);

    if (type == NODE_VECTOR_ROTATE_TYPE_EULER_XYZ) {
      const float3 rotation = stack_load_float3(stack, rotation_stack_offset);  // Default XYZ.
      const Transform rotationTransform = euler_to_transform(rotation);
      if (invert) {
        result = transform_direction_transposed(&rotationTransform, vector - center) + center;
      }
      else {
        result = transform_direction(&rotationTransform, vector - center) + center;
      }
    }
    else {
      float3 axis;
      float axis_length;
      switch (type) {
        case NODE_VECTOR_ROTATE_TYPE_AXIS_X:
          axis = make_float3(1.0f, 0.0f, 0.0f);
          axis_length = 1.0f;
          break;
        case NODE_VECTOR_ROTATE_TYPE_AXIS_Y:
          axis = make_float3(0.0f, 1.0f, 0.0f);
          axis_length = 1.0f;
          break;
        case NODE_VECTOR_ROTATE_TYPE_AXIS_Z:
          axis = make_float3(0.0f, 0.0f, 1.0f);
          axis_length = 1.0f;
          break;
        default:
          axis = stack_load_float3(stack, axis_stack_offset);
          axis_length = len(axis);
          break;
      }
      float angle = stack_load_float(stack, angle_stack_offset);
      angle = invert ? -angle : angle;
      result = (axis_length != 0.0f) ?
                   rotate_around_axis(vector - center, axis / axis_length, angle) + center :
                   vector;
    }

    stack_store_float3(stack, result_stack_offset, result);
  }
}

CCL_NAMESPACE_END
