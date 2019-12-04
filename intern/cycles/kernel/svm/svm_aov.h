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

ccl_device_inline bool svm_node_aov_check(ccl_addr_space PathState *state,
                                          ccl_global float *buffer)
{
  int path_flag = state->flag;

  bool is_primary = (path_flag & PATH_RAY_CAMERA) && (!(path_flag & PATH_RAY_SINGLE_PASS_DONE));

  return ((buffer != NULL) && is_primary);
}

ccl_device void svm_node_aov_color(
    KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, ccl_global float *buffer)
{
  float3 val = stack_load_float3(stack, node.y);

  if (buffer) {
    kernel_write_pass_float4(buffer + kernel_data.film.pass_aov_color + 4 * node.z,
                             make_float4(val.x, val.y, val.z, 1.0f));
  }
}

ccl_device void svm_node_aov_value(
    KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, ccl_global float *buffer)
{
  float val = stack_load_float(stack, node.y);

  if (buffer) {
    kernel_write_pass_float(buffer + kernel_data.film.pass_aov_value + node.z, val);
  }
}
CCL_NAMESPACE_END
