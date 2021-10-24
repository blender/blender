/*
 * Copyright 2011-2020 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Vector Rotate */

ccl_device_noinline void svm_node_vector_rotate(ccl_private ShaderData *sd,
                                                ccl_private float *stack,
                                                uint input_stack_offsets,
                                                uint axis_stack_offsets,
                                                uint result_stack_offset)
{
  uint type, vector_stack_offset, rotation_stack_offset, center_stack_offset, axis_stack_offset,
      angle_stack_offset, invert;

  svm_unpack_node_uchar4(
      input_stack_offsets, &type, &vector_stack_offset, &rotation_stack_offset, &invert);
  svm_unpack_node_uchar3(
      axis_stack_offsets, &center_stack_offset, &axis_stack_offset, &angle_stack_offset);

  if (stack_valid(result_stack_offset)) {

    float3 vector = stack_load_float3(stack, vector_stack_offset);
    float3 center = stack_load_float3(stack, center_stack_offset);
    float3 result = make_float3(0.0f, 0.0f, 0.0f);

    if (type == NODE_VECTOR_ROTATE_TYPE_EULER_XYZ) {
      float3 rotation = stack_load_float3(stack, rotation_stack_offset);  // Default XYZ.
      Transform rotationTransform = euler_to_transform(rotation);
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
