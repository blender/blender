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

/* Attribute Node */

ccl_device AttributeDescriptor svm_node_attr_init(ccl_global const KernelGlobals *kg,
                                                  ccl_private ShaderData *sd,
                                                  uint4 node,
                                                  ccl_private NodeAttributeOutputType *type,
                                                  ccl_private uint *out_offset)
{
  *out_offset = node.z;
  *type = (NodeAttributeOutputType)node.w;

  AttributeDescriptor desc;

  if (sd->object != OBJECT_NONE) {
    desc = find_attribute(kg, sd, node.y);
    if (desc.offset == ATTR_STD_NOT_FOUND) {
      desc = attribute_not_found();
      desc.offset = 0;
      desc.type = (NodeAttributeType)node.w;
    }
  }
  else {
    /* background */
    desc = attribute_not_found();
    desc.offset = 0;
    desc.type = (NodeAttributeType)node.w;
  }

  return desc;
}

template<uint node_feature_mask>
ccl_device_noinline void svm_node_attr(ccl_global const KernelGlobals *kg,
                                       ccl_private ShaderData *sd,
                                       ccl_private float *stack,
                                       uint4 node)
{
  NodeAttributeOutputType type = NODE_ATTR_OUTPUT_FLOAT;
  uint out_offset = 0;
  AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);

#ifdef __VOLUME__
  if (KERNEL_NODES_FEATURE(VOLUME)) {
    /* Volumes
     * NOTE: moving this into its own node type might help improve performance. */
    if (primitive_is_volume_attribute(sd, desc)) {
      const float4 value = volume_attribute_float4(kg, sd, desc);

      if (type == NODE_ATTR_OUTPUT_FLOAT) {
        const float f = volume_attribute_value_to_float(value);
        stack_store_float(stack, out_offset, f);
      }
      else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
        const float3 f = volume_attribute_value_to_float3(value);
        stack_store_float3(stack, out_offset, f);
      }
      else {
        const float f = volume_attribute_value_to_alpha(value);
        stack_store_float(stack, out_offset, f);
      }
      return;
    }
  }
#endif

  if (node.y == ATTR_STD_GENERATED && desc.element == ATTR_ELEMENT_NONE) {
    /* No generated attribute, fall back to object coordinates. */
    float3 f = sd->P;
    object_inverse_position_transform(kg, sd, &f);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, average(f));
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, f);
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
    return;
  }

  /* Surface. */
  if (desc.type == NODE_ATTR_FLOAT) {
    float f = primitive_surface_attribute_float(kg, sd, desc, NULL, NULL);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, f);
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, make_float3(f, f, f));
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
  }
  else if (desc.type == NODE_ATTR_FLOAT2) {
    float2 f = primitive_surface_attribute_float2(kg, sd, desc, NULL, NULL);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, f.x);
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, make_float3(f.x, f.y, 0.0f));
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
  }
  else if (desc.type == NODE_ATTR_FLOAT4 || desc.type == NODE_ATTR_RGBA) {
    float4 f = primitive_surface_attribute_float4(kg, sd, desc, NULL, NULL);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, average(float4_to_float3(f)));
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, float4_to_float3(f));
    }
    else {
      stack_store_float(stack, out_offset, f.w);
    }
  }
  else {
    float3 f = primitive_surface_attribute_float3(kg, sd, desc, NULL, NULL);
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
}

ccl_device_noinline void svm_node_attr_bump_dx(ccl_global const KernelGlobals *kg,
                                               ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               uint4 node)
{
  NodeAttributeOutputType type = NODE_ATTR_OUTPUT_FLOAT;
  uint out_offset = 0;
  AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);

#ifdef __VOLUME__
  /* Volume */
  if (primitive_is_volume_attribute(sd, desc)) {
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, 0.0f);
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, make_float3(0.0f, 0.0f, 0.0f));
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
    return;
  }
#endif

  if (node.y == ATTR_STD_GENERATED && desc.element == ATTR_ELEMENT_NONE) {
    /* No generated attribute, fall back to object coordinates. */
    float3 f = sd->P + sd->dP.dx;
    object_inverse_position_transform(kg, sd, &f);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, average(f));
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, f);
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
    return;
  }

  /* Surface */
  if (desc.type == NODE_ATTR_FLOAT) {
    float dx;
    float f = primitive_surface_attribute_float(kg, sd, desc, &dx, NULL);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, f + dx);
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, make_float3(f + dx, f + dx, f + dx));
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
  }
  else if (desc.type == NODE_ATTR_FLOAT2) {
    float2 dx;
    float2 f = primitive_surface_attribute_float2(kg, sd, desc, &dx, NULL);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, f.x + dx.x);
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, make_float3(f.x + dx.x, f.y + dx.y, 0.0f));
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
  }
  else if (desc.type == NODE_ATTR_FLOAT4 || desc.type == NODE_ATTR_RGBA) {
    float4 dx;
    float4 f = primitive_surface_attribute_float4(kg, sd, desc, &dx, NULL);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, average(float4_to_float3(f + dx)));
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, float4_to_float3(f + dx));
    }
    else {
      stack_store_float(stack, out_offset, f.w + dx.w);
    }
  }
  else {
    float3 dx;
    float3 f = primitive_surface_attribute_float3(kg, sd, desc, &dx, NULL);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, average(f + dx));
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, f + dx);
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
  }
}

ccl_device_noinline void svm_node_attr_bump_dy(ccl_global const KernelGlobals *kg,
                                               ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               uint4 node)
{
  NodeAttributeOutputType type = NODE_ATTR_OUTPUT_FLOAT;
  uint out_offset = 0;
  AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);

#ifdef __VOLUME__
  /* Volume */
  if (primitive_is_volume_attribute(sd, desc)) {
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, 0.0f);
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, make_float3(0.0f, 0.0f, 0.0f));
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
    return;
  }
#endif

  if (node.y == ATTR_STD_GENERATED && desc.element == ATTR_ELEMENT_NONE) {
    /* No generated attribute, fall back to object coordinates. */
    float3 f = sd->P + sd->dP.dy;
    object_inverse_position_transform(kg, sd, &f);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, average(f));
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, f);
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
    return;
  }

  /* Surface */
  if (desc.type == NODE_ATTR_FLOAT) {
    float dy;
    float f = primitive_surface_attribute_float(kg, sd, desc, NULL, &dy);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, f + dy);
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, make_float3(f + dy, f + dy, f + dy));
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
  }
  else if (desc.type == NODE_ATTR_FLOAT2) {
    float2 dy;
    float2 f = primitive_surface_attribute_float2(kg, sd, desc, NULL, &dy);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, f.x + dy.x);
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, make_float3(f.x + dy.x, f.y + dy.y, 0.0f));
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
  }
  else if (desc.type == NODE_ATTR_FLOAT4 || desc.type == NODE_ATTR_RGBA) {
    float4 dy;
    float4 f = primitive_surface_attribute_float4(kg, sd, desc, NULL, &dy);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, average(float4_to_float3(f + dy)));
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, float4_to_float3(f + dy));
    }
    else {
      stack_store_float(stack, out_offset, f.w + dy.w);
    }
  }
  else {
    float3 dy;
    float3 f = primitive_surface_attribute_float3(kg, sd, desc, NULL, &dy);
    if (type == NODE_ATTR_OUTPUT_FLOAT) {
      stack_store_float(stack, out_offset, average(f + dy));
    }
    else if (type == NODE_ATTR_OUTPUT_FLOAT3) {
      stack_store_float3(stack, out_offset, f + dy);
    }
    else {
      stack_store_float(stack, out_offset, 1.0f);
    }
  }
}

CCL_NAMESPACE_END
