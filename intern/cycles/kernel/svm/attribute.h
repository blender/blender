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

/* Store attribute to the stack. Float3Type is float3 or dual3. */

template<typename Float3Type>
ccl_device_inline void svm_node_attr_store(const NodeAttributeOutputType type,
                                           ccl_private float *stack,
                                           const uint out_offset,
                                           const ccl_private Float3Type &f)
{
  using FloatType = dual_scalar_t<Float3Type>;
  if (type == NODE_ATTR_OUTPUT_FLOAT3) {
    stack_store(stack, out_offset, f);
  }
  else {
    stack_store(stack, out_offset, FloatType(average(f)));
  }
}

/* Core surface attribute evaluation, returning Float3Type = float3 or dual3.
 * Fetches the attribute, applies output type conversion (float3 or scalar-as-float3),
 * and computes derivatives when Float3Type is a dual type. */

template<typename Float3Type>
ccl_device_inline Float3Type svm_node_attr_surface_eval(KernelGlobals kg,
                                                        ccl_private ShaderData *sd,
                                                        const uint4 node,
                                                        const NodeAttributeOutputType type,
                                                        const AttributeDescriptor desc)
{
  using FloatType = dual_scalar_t<Float3Type>;

  if (sd->type == PRIMITIVE_LAMP && node.y == ATTR_STD_UV) {
    Float3Type uv(make_float3(1.0f - sd->u - sd->v, sd->u, 0.0f));
    if constexpr (is_dual_v<Float3Type>) {
      uv.dx = make_float3(-sd->du.dx - sd->dv.dx, sd->du.dx, 0.0f);
      uv.dy = make_float3(-sd->du.dy - sd->dv.dy, sd->du.dy, 0.0f);
    }
    return uv;
  }

  if (node.y == ATTR_STD_GENERATED && desc.element == ATTR_ELEMENT_NONE) {
    Float3Type f = shading_position<Float3Type>(sd);
    object_inverse_position_transform_if_object(kg, sd, &f);
    return f;
  }

  /* Surface attribute fetch with output type conversion. */
  if (desc.type == NODE_ATTR_FLOAT) {
    FloatType f = primitive_surface_attribute<FloatType>(kg, sd, desc);
    if (type == NODE_ATTR_OUTPUT_FLOAT_ALPHA) {
      return make_float3(FloatType(1.0f));
    }
    return make_float3(f, f, f);
  }

  if (desc.type == NODE_ATTR_FLOAT2) {
    if constexpr (is_dual_v<Float3Type>) {
      dual2 f = primitive_surface_attribute<dual2>(kg, sd, desc);
      if (type == NODE_ATTR_OUTPUT_FLOAT) {
        return make_float3(f.x());
      }
      if (type == NODE_ATTR_OUTPUT_FLOAT_ALPHA) {
        return make_float3(FloatType(1.0f));
      }
      return make_float3(f);
    }
    else {
      float2 f = primitive_surface_attribute<float2>(kg, sd, desc);
      if (type == NODE_ATTR_OUTPUT_FLOAT) {
        return make_float3(f.x);
      }
      if (type == NODE_ATTR_OUTPUT_FLOAT_ALPHA) {
        return make_float3(FloatType(1.0f));
      }
      return make_float3(f);
    }
  }

  if (desc.type == NODE_ATTR_FLOAT4 || desc.type == NODE_ATTR_RGBA) {
    if constexpr (is_dual_v<Float3Type>) {
      dual4 f = primitive_surface_attribute<dual4>(kg, sd, desc);
      if (type == NODE_ATTR_OUTPUT_FLOAT) {
        return make_float3(average(make_float3(f)));
      }
      if (type == NODE_ATTR_OUTPUT_FLOAT_ALPHA) {
        return make_float3(f.w());
      }
      return make_float3(f);
    }
    else {
      float4 f = primitive_surface_attribute<float4>(kg, sd, desc);
      if (type == NODE_ATTR_OUTPUT_FLOAT) {
        return make_float3(average(make_float3(f)));
      }
      if (type == NODE_ATTR_OUTPUT_FLOAT_ALPHA) {
        return make_float3(f.w);
      }
      return make_float3(f);
    }
  }

  Float3Type f = primitive_surface_attribute<Float3Type>(kg, sd, desc);
  if (type == NODE_ATTR_OUTPUT_FLOAT) {
    return make_float3(average(f));
  }
  if (type == NODE_ATTR_OUTPUT_FLOAT_ALPHA) {
    return make_float3(FloatType(1.0f));
  }
  return f;
}

/* Surface attribute node. */
ccl_device_noinline void svm_node_attr_surface(KernelGlobals kg,
                                               ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               const uint4 node)
{
  NodeAttributeOutputType type = NODE_ATTR_OUTPUT_FLOAT;
  uint out_offset = 0;
  const AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);

  float3 data = svm_node_attr_surface_eval<float3>(kg, sd, node, type, desc);
  svm_node_attr_store(type, stack, out_offset, data);
}

/* Evaluate surface attributes with derivatives and optional bump offset.
 * Used for derivative tracking and bump mapping. */

ccl_device_noinline void svm_node_attr_derivative(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  const uint4 node)
{
  NodeAttributeOutputType type = NODE_ATTR_OUTPUT_FLOAT;
  uint out_offset = 0;
  const AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);

  uint unused1, unused2, bump_offset, store_derivatives;
  svm_unpack_node_uchar4(node.z, &unused1, &unused2, &bump_offset, &store_derivatives);
  const float bump_filter_width = __uint_as_float(node.w);

  dual3 data = svm_node_attr_surface_eval<dual3>(kg, sd, node, type, desc);
  if (bump_offset == NODE_BUMP_OFFSET_DX) {
    data.val += data.dx * bump_filter_width;
  }
  else if (bump_offset == NODE_BUMP_OFFSET_DY) {
    data.val += data.dy * bump_filter_width;
  }

  if (store_derivatives) {
    svm_node_attr_store(type, stack, out_offset, data);
  }
  else {
    svm_node_attr_store(type, stack, out_offset, float3(data.val));
  }
}

/* Volume attribute node. Volumes have no derivatives or bump. */
ccl_device_noinline void svm_node_attr_volume(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              ccl_private float *stack,
                                              const uint4 node)
{
  kernel_assert(primitive_is_volume_attribute(sd));

  NodeAttributeOutputType type = NODE_ATTR_OUTPUT_FLOAT;
  uint out_offset = 0;
  const AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);

  const bool stochastic_sample = node.w;
  const float4 value = volume_attribute_float4(kg, sd, desc, stochastic_sample);

  if (type == NODE_ATTR_OUTPUT_FLOAT) {
    stack_store_float(stack, out_offset, volume_attribute_value<float>(value));
  }
  else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
    stack_store_float3(stack, out_offset, volume_attribute_value<float3>(value));
  }
  else {
    stack_store_float(stack, out_offset, volume_attribute_alpha(value));
  }
}

CCL_NAMESPACE_END
