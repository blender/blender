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

ccl_device_noinline void svm_node_vertex_color(KernelGlobals kg,
                                               ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               uint layer_id,
                                               uint color_offset,
                                               uint alpha_offset)
{
  AttributeDescriptor descriptor = find_attribute(kg, sd, layer_id);
  if (descriptor.offset != ATTR_STD_NOT_FOUND) {
    float4 vertex_color = primitive_surface_attribute_float4(kg, sd, descriptor, NULL, NULL);
    stack_store_float3(stack, color_offset, float4_to_float3(vertex_color));
    stack_store_float(stack, alpha_offset, vertex_color.w);
  }
  else {
    stack_store_float3(stack, color_offset, make_float3(0.0f, 0.0f, 0.0f));
    stack_store_float(stack, alpha_offset, 0.0f);
  }
}

ccl_device_noinline void svm_node_vertex_color_bump_dx(KernelGlobals kg,
                                                       ccl_private ShaderData *sd,
                                                       ccl_private float *stack,
                                                       uint layer_id,
                                                       uint color_offset,
                                                       uint alpha_offset)
{
  AttributeDescriptor descriptor = find_attribute(kg, sd, layer_id);
  if (descriptor.offset != ATTR_STD_NOT_FOUND) {
    float4 dx;
    float4 vertex_color = primitive_surface_attribute_float4(kg, sd, descriptor, &dx, NULL);
    vertex_color += dx;
    stack_store_float3(stack, color_offset, float4_to_float3(vertex_color));
    stack_store_float(stack, alpha_offset, vertex_color.w);
  }
  else {
    stack_store_float3(stack, color_offset, make_float3(0.0f, 0.0f, 0.0f));
    stack_store_float(stack, alpha_offset, 0.0f);
  }
}

ccl_device_noinline void svm_node_vertex_color_bump_dy(KernelGlobals kg,
                                                       ccl_private ShaderData *sd,
                                                       ccl_private float *stack,
                                                       uint layer_id,
                                                       uint color_offset,
                                                       uint alpha_offset)
{
  AttributeDescriptor descriptor = find_attribute(kg, sd, layer_id);
  if (descriptor.offset != ATTR_STD_NOT_FOUND) {
    float4 dy;
    float4 vertex_color = primitive_surface_attribute_float4(kg, sd, descriptor, NULL, &dy);
    vertex_color += dy;
    stack_store_float3(stack, color_offset, float4_to_float3(vertex_color));
    stack_store_float(stack, alpha_offset, vertex_color.w);
  }
  else {
    stack_store_float3(stack, color_offset, make_float3(0.0f, 0.0f, 0.0f));
    stack_store_float(stack, alpha_offset, 0.0f);
  }
}

CCL_NAMESPACE_END
