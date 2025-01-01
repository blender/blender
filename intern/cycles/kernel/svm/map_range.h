/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Map Range Node */

ccl_device_inline float smootherstep(const float edge0, const float edge1, float x)
{
  x = clamp(safe_divide((x - edge0), (edge1 - edge0)), 0.0f, 1.0f);
  return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

ccl_device_noinline int svm_node_map_range(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           ccl_private float *stack,
                                           const uint value_stack_offset,
                                           const uint parameters_stack_offsets,
                                           const uint results_stack_offsets,
                                           int offset)
{
  uint from_min_stack_offset;
  uint from_max_stack_offset;
  uint to_min_stack_offset;
  uint to_max_stack_offset;
  uint type_stack_offset;
  uint steps_stack_offset;
  uint result_stack_offset;
  svm_unpack_node_uchar4(parameters_stack_offsets,
                         &from_min_stack_offset,
                         &from_max_stack_offset,
                         &to_min_stack_offset,
                         &to_max_stack_offset);
  svm_unpack_node_uchar3(
      results_stack_offsets, &type_stack_offset, &steps_stack_offset, &result_stack_offset);

  const uint4 defaults = read_node(kg, &offset);
  const uint4 defaults2 = read_node(kg, &offset);

  const float value = stack_load_float(stack, value_stack_offset);
  const float from_min = stack_load_float_default(stack, from_min_stack_offset, defaults.x);
  const float from_max = stack_load_float_default(stack, from_max_stack_offset, defaults.y);
  const float to_min = stack_load_float_default(stack, to_min_stack_offset, defaults.z);
  const float to_max = stack_load_float_default(stack, to_max_stack_offset, defaults.w);
  const float steps = stack_load_float_default(stack, steps_stack_offset, defaults2.x);

  float result;

  if (from_max != from_min) {
    float factor = value;
    switch (type_stack_offset) {
      default:
      case NODE_MAP_RANGE_LINEAR:
        factor = (value - from_min) / (from_max - from_min);
        break;
      case NODE_MAP_RANGE_STEPPED: {
        factor = (value - from_min) / (from_max - from_min);
        factor = (steps > 0.0f) ? floorf(factor * (steps + 1.0f)) / steps : 0.0f;
        break;
      }
      case NODE_MAP_RANGE_SMOOTHSTEP: {
        factor = (from_min > from_max) ? 1.0f - smoothstep(from_max, from_min, factor) :
                                         smoothstep(from_min, from_max, factor);
        break;
      }
      case NODE_MAP_RANGE_SMOOTHERSTEP: {
        factor = (from_min > from_max) ? 1.0f - smootherstep(from_max, from_min, factor) :
                                         smootherstep(from_min, from_max, factor);
        break;
      }
    }
    result = to_min + factor * (to_max - to_min);
  }
  else {
    result = 0.0f;
  }
  stack_store_float(stack, result_stack_offset, result);
  return offset;
}

ccl_device_noinline int svm_node_vector_map_range(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  const uint value_stack_offset,
                                                  const uint parameters_stack_offsets,
                                                  const uint results_stack_offsets,
                                                  const int offset)
{
  uint from_min_stack_offset;
  uint from_max_stack_offset;
  uint to_min_stack_offset;
  uint to_max_stack_offset;
  uint steps_stack_offset;
  uint clamp_stack_offset;
  uint range_type_stack_offset;
  uint result_stack_offset;
  svm_unpack_node_uchar4(parameters_stack_offsets,
                         &from_min_stack_offset,
                         &from_max_stack_offset,
                         &to_min_stack_offset,
                         &to_max_stack_offset);
  svm_unpack_node_uchar4(results_stack_offsets,
                         &steps_stack_offset,
                         &clamp_stack_offset,
                         &range_type_stack_offset,
                         &result_stack_offset);

  const float3 value = stack_load_float3(stack, value_stack_offset);
  const float3 from_min = stack_load_float3(stack, from_min_stack_offset);
  const float3 from_max = stack_load_float3(stack, from_max_stack_offset);
  const float3 to_min = stack_load_float3(stack, to_min_stack_offset);
  const float3 to_max = stack_load_float3(stack, to_max_stack_offset);
  const float3 steps = stack_load_float3(stack, steps_stack_offset);

  const int type = range_type_stack_offset;
  const int use_clamp = (type == NODE_MAP_RANGE_SMOOTHSTEP ||
                         type == NODE_MAP_RANGE_SMOOTHERSTEP) ?
                            0 :
                            clamp_stack_offset;
  float3 result;
  float3 factor = value;
  switch (range_type_stack_offset) {
    default:
    case NODE_MAP_RANGE_LINEAR:
      factor = safe_divide((value - from_min), (from_max - from_min));
      break;
    case NODE_MAP_RANGE_STEPPED: {
      factor = safe_divide((value - from_min), (from_max - from_min));
      factor = make_float3((steps.x > 0.0f) ? floorf(factor.x * (steps.x + 1.0f)) / steps.x : 0.0f,
                           (steps.y > 0.0f) ? floorf(factor.y * (steps.y + 1.0f)) / steps.y : 0.0f,
                           (steps.z > 0.0f) ? floorf(factor.z * (steps.z + 1.0f)) / steps.z :
                                              0.0f);
      break;
    }
    case NODE_MAP_RANGE_SMOOTHSTEP: {
      factor = safe_divide((value - from_min), (from_max - from_min));
      factor = clamp(factor, zero_float3(), one_float3());
      factor = (make_float3(3.0f, 3.0f, 3.0f) - 2.0f * factor) * (factor * factor);
      break;
    }
    case NODE_MAP_RANGE_SMOOTHERSTEP: {
      factor = safe_divide((value - from_min), (from_max - from_min));
      factor = clamp(factor, zero_float3(), one_float3());
      factor = factor * factor * factor * (factor * (factor * 6.0f - 15.0f) + 10.0f);
      break;
    }
  }
  result = to_min + factor * (to_max - to_min);
  if (use_clamp > 0) {
    result.x = (to_min.x > to_max.x) ? clamp(result.x, to_max.x, to_min.x) :
                                       clamp(result.x, to_min.x, to_max.x);
    result.y = (to_min.y > to_max.y) ? clamp(result.y, to_max.y, to_min.y) :
                                       clamp(result.y, to_min.y, to_max.y);
    result.z = (to_min.z > to_max.z) ? clamp(result.z, to_max.z, to_min.z) :
                                       clamp(result.z, to_min.z, to_max.z);
  }

  stack_store_float3(stack, result_stack_offset, result);
  return offset;
}

CCL_NAMESPACE_END
