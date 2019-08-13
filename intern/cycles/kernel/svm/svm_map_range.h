/*
 * Copyright 2011-2013 Blender Foundation
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

CCL_NAMESPACE_BEGIN

/* Map Range Node */

ccl_device void svm_node_map_range(KernelGlobals *kg,
                                   ShaderData *sd,
                                   float *stack,
                                   uint value_stack_offset,
                                   uint parameters_stack_offsets,
                                   uint result_stack_offset,
                                   int *offset)
{
  uint from_min_stack_offset, from_max_stack_offset, to_min_stack_offset, to_max_stack_offset;
  decode_node_uchar4(parameters_stack_offsets,
                     &from_min_stack_offset,
                     &from_max_stack_offset,
                     &to_min_stack_offset,
                     &to_max_stack_offset);

  uint4 defaults = read_node(kg, offset);

  float value = stack_load_float(stack, value_stack_offset);
  float from_min = stack_load_float_default(stack, from_min_stack_offset, defaults.x);
  float from_max = stack_load_float_default(stack, from_max_stack_offset, defaults.y);
  float to_min = stack_load_float_default(stack, to_min_stack_offset, defaults.z);
  float to_max = stack_load_float_default(stack, to_max_stack_offset, defaults.w);

  float result;
  if (from_max != from_min) {
    result = to_min + ((value - from_min) / (from_max - from_min)) * (to_max - to_min);
  }
  else {
    result = 0.0f;
  }
  stack_store_float(stack, result_stack_offset, result);
}

CCL_NAMESPACE_END
