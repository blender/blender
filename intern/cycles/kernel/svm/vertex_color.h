/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

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
    if (descriptor.type == NODE_ATTR_FLOAT4 || descriptor.type == NODE_ATTR_RGBA) {
      float4 vertex_color = primitive_surface_attribute_float4(kg, sd, descriptor, NULL, NULL);
      stack_store_float3(stack, color_offset, float4_to_float3(vertex_color));
      stack_store_float(stack, alpha_offset, vertex_color.w);
    }
    else {
      float3 vertex_color = primitive_surface_attribute_float3(kg, sd, descriptor, NULL, NULL);
      stack_store_float3(stack, color_offset, vertex_color);
      stack_store_float(stack, alpha_offset, 1.0f);
    }
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
    if (descriptor.type == NODE_ATTR_FLOAT4 || descriptor.type == NODE_ATTR_RGBA) {
      float4 dx;
      float4 vertex_color = primitive_surface_attribute_float4(kg, sd, descriptor, &dx, NULL);
      vertex_color += dx;
      stack_store_float3(stack, color_offset, float4_to_float3(vertex_color));
      stack_store_float(stack, alpha_offset, vertex_color.w);
    }
    else {
      float3 dx;
      float3 vertex_color = primitive_surface_attribute_float3(kg, sd, descriptor, &dx, NULL);
      vertex_color += dx;
      stack_store_float3(stack, color_offset, vertex_color);
      stack_store_float(stack, alpha_offset, 1.0f);
    }
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
    if (descriptor.type == NODE_ATTR_FLOAT4 || descriptor.type == NODE_ATTR_RGBA) {
      float4 dy;
      float4 vertex_color = primitive_surface_attribute_float4(kg, sd, descriptor, NULL, &dy);
      vertex_color += dy;
      stack_store_float3(stack, color_offset, float4_to_float3(vertex_color));
      stack_store_float(stack, alpha_offset, vertex_color.w);
    }
    else {
      float3 dy;
      float3 vertex_color = primitive_surface_attribute_float3(kg, sd, descriptor, NULL, &dy);
      vertex_color += dy;
      stack_store_float3(stack, color_offset, vertex_color);
      stack_store_float(stack, alpha_offset, 1.0f);
    }
  }
  else {
    stack_store_float3(stack, color_offset, make_float3(0.0f, 0.0f, 0.0f));
    stack_store_float(stack, alpha_offset, 0.0f);
  }
}

CCL_NAMESPACE_END
