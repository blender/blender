/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/geom/attribute.h"
#include "kernel/geom/object.h"
#include "kernel/geom/primitive.h"
#include "kernel/geom/volume.h"

#include "kernel/svm/util.h"

#include "kernel/util/differential.h"

CCL_NAMESPACE_BEGIN

/* Attribute Node */

ccl_device AttributeDescriptor svm_node_attr_init(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  const uint4 node,
                                                  ccl_private NodeAttributeOutputType *type,
                                                  ccl_private uint *out_offset)
{
  uint type_value;
  svm_unpack_node_uchar2(node.z, out_offset, &type_value);
  *type = (NodeAttributeOutputType)type_value;

  AttributeDescriptor desc;

  if (sd->object != OBJECT_NONE) {
    desc = find_attribute(kg, sd, node.y);
    if (desc.offset == ATTR_STD_NOT_FOUND) {
      desc = attribute_not_found();
      desc.offset = 0;
      desc.type = (NodeAttributeType)type_value;
    }
  }
  else {
    /* background */
    desc = attribute_not_found();
    desc.offset = 0;
    desc.type = (NodeAttributeType)type_value;
  }

  return desc;
}

ccl_device_inline void svm_node_attr_store(const NodeAttributeOutputType type,
                                           ccl_private float *stack,
                                           const uint out_offset,
                                           const float f)
{
  if (type == NODE_ATTR_OUTPUT_FLOAT) {
    stack_store_float(stack, out_offset, f);
  }
  else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
    stack_store_float3(stack, out_offset, make_float3(f));
  }
  else {
    stack_store_float(stack, out_offset, 1.0f);
  }
}

ccl_device_inline void svm_node_attr_store(const NodeAttributeOutputType type,
                                           ccl_private float *stack,
                                           const uint out_offset,
                                           const ccl_private float2 &f)
{
  if (type == NODE_ATTR_OUTPUT_FLOAT) {
    stack_store_float(stack, out_offset, f.x);
  }
  else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
    stack_store_float3(stack, out_offset, make_float3(f));
  }
  else {
    stack_store_float(stack, out_offset, 1.0f);
  }
}

ccl_device_inline void svm_node_attr_store(const NodeAttributeOutputType type,
                                           ccl_private float *stack,
                                           const uint out_offset,
                                           const ccl_private float3 &f)
{
  if (type == NODE_ATTR_OUTPUT_FLOAT) {
    stack_store_float(stack, out_offset, average(f));
  }
  else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
    stack_store_float3(stack, out_offset, f);
  }
  else {
    stack_store_float(stack, out_offset, 1.0f);
  }
}

ccl_device_inline void svm_node_attr_store(const NodeAttributeOutputType type,
                                           ccl_private float *stack,
                                           const uint out_offset,
                                           const ccl_private float4 &f)
{
  if (type == NODE_ATTR_OUTPUT_FLOAT) {
    stack_store_float(stack, out_offset, average(make_float3(f)));
  }
  else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
    stack_store_float3(stack, out_offset, make_float3(f));
  }
  else {
    kernel_assert(type == NODE_ATTR_OUTPUT_FLOAT_ALPHA);
    stack_store_float(stack, out_offset, f.w);
  }
}

template<class T>
ccl_device_inline void svm_surface_attr(KernelGlobals kg,
                                        const ccl_private ShaderData *sd,
                                        const AttributeDescriptor desc,
                                        const NodeAttributeOutputType type,
                                        ccl_private float *stack,
                                        const uint out_offset)
{
  T f = primitive_surface_attribute<T>(kg, sd, desc).val;
  svm_node_attr_store(type, stack, out_offset, f);
}

template<class T>
ccl_device_inline void svm_surface_attr_dx(KernelGlobals kg,
                                           const ccl_private ShaderData *sd,
                                           const AttributeDescriptor desc,
                                           const float bump_filter_width,
                                           const NodeAttributeOutputType type,
                                           ccl_private float *stack,
                                           const uint out_offset)
{
  dual<T> f = primitive_surface_attribute<T>(kg, sd, desc, true, false);
  f.val += f.dx * bump_filter_width;
  svm_node_attr_store(type, stack, out_offset, f.val);
}

template<class T>
ccl_device_inline void svm_surface_attr_dy(KernelGlobals kg,
                                           const ccl_private ShaderData *sd,
                                           const AttributeDescriptor desc,
                                           const float bump_filter_width,
                                           const NodeAttributeOutputType type,
                                           ccl_private float *stack,
                                           const uint out_offset)
{
  dual<T> f = primitive_surface_attribute<T>(kg, sd, desc, false, true);
  f.val += f.dy * bump_filter_width;
  svm_node_attr_store(type, stack, out_offset, f.val);
}

template<uint node_feature_mask>
ccl_device_noinline void svm_node_attr(KernelGlobals kg,
                                       ccl_private ShaderData *sd,
                                       ccl_private float *stack,
                                       const uint4 node)
{
  NodeAttributeOutputType type = NODE_ATTR_OUTPUT_FLOAT;
  uint out_offset = 0;
  const AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);

#ifdef __VOLUME__
  IF_KERNEL_NODES_FEATURE(VOLUME)
  {
    /* Volumes
     * NOTE: moving this into its own node type might help improve performance. */
    if (primitive_is_volume_attribute(sd)) {
      const bool stochastic_sample = node.w;
      const float4 value = volume_attribute_float4(kg, sd, desc, stochastic_sample);

      if (type == NODE_ATTR_OUTPUT_FLOAT) {
        const float f = volume_attribute_value<float>(value);
        stack_store_float(stack, out_offset, f);
      }
      else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
        const float3 f = volume_attribute_value<float3>(value);
        stack_store_float3(stack, out_offset, f);
      }
      else {
        const float f = volume_attribute_alpha(value);
        stack_store_float(stack, out_offset, f);
      }
      return;
    }
  }
#endif

  if (sd->type == PRIMITIVE_LAMP && node.y == ATTR_STD_UV) {
    stack_store_float3(stack, out_offset, make_float3(1.0f - sd->u - sd->v, sd->u, 0.0f));
    return;
  }

  if (node.y == ATTR_STD_GENERATED && desc.element == ATTR_ELEMENT_NONE) {
    /* No generated attribute, fall back to object coordinates. */
    float3 f = sd->P;
    if (sd->object != OBJECT_NONE) {
      object_inverse_position_transform(kg, sd, &f);
    }
    svm_node_attr_store(type, stack, out_offset, f);
    return;
  }

  /* Surface. */
  if (desc.type == NODE_ATTR_FLOAT) {
    svm_surface_attr<float>(kg, sd, desc, type, stack, out_offset);
  }
  else if (desc.type == NODE_ATTR_FLOAT2) {
    svm_surface_attr<float2>(kg, sd, desc, type, stack, out_offset);
  }
  else if (desc.type == NODE_ATTR_FLOAT4 || desc.type == NODE_ATTR_RGBA) {
    svm_surface_attr<float4>(kg, sd, desc, type, stack, out_offset);
  }
  else {
    svm_surface_attr<float3>(kg, sd, desc, type, stack, out_offset);
  }
}

/* Position offsetted in x direction. */
ccl_device_forceinline float3 svm_node_bump_P_dx(const ccl_private ShaderData *sd,
                                                 const float bump_filter_width)
{
  return sd->P + dPdx(sd) * bump_filter_width;
}

/* Position offsetted in y direction. */
ccl_device_forceinline float3 svm_node_bump_P_dy(const ccl_private ShaderData *sd,
                                                 const float bump_filter_width)
{
  return sd->P + dPdy(sd) * bump_filter_width;
}

/* Evaluate attributes at a position shifted in x direction. */
ccl_device_noinline void svm_node_attr_bump_dx(KernelGlobals kg,
                                               ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               const uint4 node)
{
  NodeAttributeOutputType type = NODE_ATTR_OUTPUT_FLOAT;
  uint out_offset = 0;
  const AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);
  const float bump_filter_width = __uint_as_float(node.w);

#ifdef __VOLUME__
  /* Volume */
  if (primitive_is_volume_attribute(sd)) {
    svm_node_attr_store(type, stack, out_offset, 0.0f);
    return;
  }
#endif

  if (node.y == ATTR_STD_GENERATED && desc.element == ATTR_ELEMENT_NONE) {
    /* No generated attribute, fall back to object coordinates. */
    float3 f_x = svm_node_bump_P_dx(sd, bump_filter_width);
    if (sd->object != OBJECT_NONE) {
      object_inverse_position_transform(kg, sd, &f_x);
    }
    svm_node_attr_store(type, stack, out_offset, f_x);
    return;
  }

  /* Surface */
  if (desc.type == NODE_ATTR_FLOAT) {
    svm_surface_attr_dx<float>(kg, sd, desc, bump_filter_width, type, stack, out_offset);
  }
  else if (desc.type == NODE_ATTR_FLOAT2) {
    svm_surface_attr_dx<float2>(kg, sd, desc, bump_filter_width, type, stack, out_offset);
  }
  else if (desc.type == NODE_ATTR_FLOAT4 || desc.type == NODE_ATTR_RGBA) {
    svm_surface_attr_dx<float4>(kg, sd, desc, bump_filter_width, type, stack, out_offset);
  }
  else {
    svm_surface_attr_dx<float3>(kg, sd, desc, bump_filter_width, type, stack, out_offset);
  }
}

/* Evaluate attributes at a position shifted in y direction. */
ccl_device_noinline void svm_node_attr_bump_dy(KernelGlobals kg,
                                               ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               const uint4 node)
{
  NodeAttributeOutputType type = NODE_ATTR_OUTPUT_FLOAT;
  uint out_offset = 0;
  const AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);
  const float bump_filter_width = __uint_as_float(node.w);

#ifdef __VOLUME__
  /* Volume */
  if (primitive_is_volume_attribute(sd)) {
    svm_node_attr_store(type, stack, out_offset, 0.0f);
    return;
  }
#endif

  if (node.y == ATTR_STD_GENERATED && desc.element == ATTR_ELEMENT_NONE) {
    /* No generated attribute, fall back to object coordinates. */
    /* TODO:(weizhen) */
    float3 f_y = svm_node_bump_P_dy(sd, bump_filter_width);
    if (sd->object != OBJECT_NONE) {
      object_inverse_position_transform(kg, sd, &f_y);
    }
    svm_node_attr_store(type, stack, out_offset, f_y);
    return;
  }

  /* Surface */
  if (desc.type == NODE_ATTR_FLOAT) {
    svm_surface_attr_dy<float>(kg, sd, desc, bump_filter_width, type, stack, out_offset);
  }
  else if (desc.type == NODE_ATTR_FLOAT2) {
    svm_surface_attr_dy<float2>(kg, sd, desc, bump_filter_width, type, stack, out_offset);
  }
  else if (desc.type == NODE_ATTR_FLOAT4 || desc.type == NODE_ATTR_RGBA) {
    svm_surface_attr_dy<float4>(kg, sd, desc, bump_filter_width, type, stack, out_offset);
  }
  else {
    svm_surface_attr_dy<float3>(kg, sd, desc, bump_filter_width, type, stack, out_offset);
  }
}

CCL_NAMESPACE_END
