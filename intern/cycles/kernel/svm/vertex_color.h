/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/geom/attribute.h"
#include "kernel/geom/primitive.h"
#include "kernel/svm/util.h"
#include "util/math_base.h"

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_vertex_color(KernelGlobals kg,
                                               ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               const uint4 node)
{
  uint layer_id;
  uint color_offset;
  uint alpha_offset;
  svm_unpack_node_uchar3(node.y, &layer_id, &color_offset, &alpha_offset);

  const AttributeDescriptor descriptor = find_attribute(kg, sd, layer_id);
  if (descriptor.offset != ATTR_STD_NOT_FOUND) {
    if (descriptor.type == NODE_ATTR_FLOAT4 || descriptor.type == NODE_ATTR_RGBA) {
      const float4 vertex_color = primitive_surface_attribute_float4(
          kg, sd, descriptor, nullptr, nullptr);
      stack_store_float3(stack, color_offset, make_float3(vertex_color));
      stack_store_float(stack, alpha_offset, vertex_color.w);
    }
    else {
      const float3 vertex_color = primitive_surface_attribute_float3(
          kg, sd, descriptor, nullptr, nullptr);
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
                                                       const uint4 node)
{
  uint layer_id;
  uint color_offset;
  uint alpha_offset;
  svm_unpack_node_uchar3(node.y, &layer_id, &color_offset, &alpha_offset);
  const float bump_filter_width = __uint_as_float(node.z);

  const AttributeDescriptor descriptor = find_attribute(kg, sd, layer_id);
  if (descriptor.offset != ATTR_STD_NOT_FOUND) {
    if (descriptor.type == NODE_ATTR_FLOAT4 || descriptor.type == NODE_ATTR_RGBA) {
      float4 dfdx;
      float4 vertex_color = primitive_surface_attribute_float4(kg, sd, descriptor, &dfdx, nullptr);
      vertex_color += dfdx * bump_filter_width;
      stack_store_float3(stack, color_offset, make_float3(vertex_color));
      stack_store_float(stack, alpha_offset, vertex_color.w);
    }
    else {
      float3 dfdx;
      float3 vertex_color = primitive_surface_attribute_float3(kg, sd, descriptor, &dfdx, nullptr);
      vertex_color += dfdx * bump_filter_width;
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
                                                       const uint4 node)
{
  uint layer_id;
  uint color_offset;
  uint alpha_offset;
  svm_unpack_node_uchar3(node.y, &layer_id, &color_offset, &alpha_offset);
  const float bump_filter_width = __uint_as_float(node.z);

  const AttributeDescriptor descriptor = find_attribute(kg, sd, layer_id);
  if (descriptor.offset != ATTR_STD_NOT_FOUND) {
    if (descriptor.type == NODE_ATTR_FLOAT4 || descriptor.type == NODE_ATTR_RGBA) {
      float4 dfdy;
      float4 vertex_color = primitive_surface_attribute_float4(kg, sd, descriptor, nullptr, &dfdy);
      vertex_color += dfdy * bump_filter_width;
      stack_store_float3(stack, color_offset, make_float3(vertex_color));
      stack_store_float(stack, alpha_offset, vertex_color.w);
    }
    else {
      float3 dfdy;
      float3 vertex_color = primitive_surface_attribute_float3(kg, sd, descriptor, nullptr, &dfdy);
      vertex_color += dfdy * bump_filter_width;
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
