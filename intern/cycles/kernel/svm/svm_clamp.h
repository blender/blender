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

/* Clamp Node */

ccl_device void svm_node_clamp(KernelGlobals *kg,
                               ShaderData *sd,
                               float *stack,
                               uint value_stack_offset,
                               uint parameters_stack_offsets,
                               uint result_stack_offset,
                               int *offset)
{
  uint min_stack_offset, max_stack_offset, type;
  svm_unpack_node_uchar3(parameters_stack_offsets, &min_stack_offset, &max_stack_offset, &type);

  uint4 defaults = read_node(kg, offset);

  float value = stack_load_float(stack, value_stack_offset);
  float min = stack_load_float_default(stack, min_stack_offset, defaults.x);
  float max = stack_load_float_default(stack, max_stack_offset, defaults.y);

  if (type == NODE_CLAMP_RANGE && (min > max)) {
    stack_store_float(stack, result_stack_offset, clamp(value, max, min));
  }
  else {
    stack_store_float(stack, result_stack_offset, clamp(value, min, max));
  }
}

CCL_NAMESPACE_END
