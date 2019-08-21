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

ccl_device AttributeDescriptor svm_node_attr_init(
    KernelGlobals *kg, ShaderData *sd, uint4 node, NodeAttributeType *type, uint *out_offset)
{
  *out_offset = node.z;
  *type = (NodeAttributeType)node.w;

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

ccl_device void svm_node_attr(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
  NodeAttributeType type = NODE_ATTR_FLOAT;
  uint out_offset = 0;
  AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);

  /* fetch and store attribute */
  if (desc.type == NODE_ATTR_FLOAT) {
    float f = primitive_attribute_float(kg, sd, desc, NULL, NULL);
    if (type == NODE_ATTR_FLOAT) {
      stack_store_float(stack, out_offset, f);
    }
    else {
      stack_store_float3(stack, out_offset, make_float3(f, f, f));
    }
  }
  else if (desc.type == NODE_ATTR_FLOAT2) {
    float2 f = primitive_attribute_float2(kg, sd, desc, NULL, NULL);
    if (type == NODE_ATTR_FLOAT) {
      stack_store_float(stack, out_offset, f.x);
    }
    else {
      stack_store_float3(stack, out_offset, make_float3(f.x, f.y, 0.0f));
    }
  }
  else {
    float3 f = primitive_attribute_float3(kg, sd, desc, NULL, NULL);
    if (type == NODE_ATTR_FLOAT) {
      stack_store_float(stack, out_offset, average(f));
    }
    else {
      stack_store_float3(stack, out_offset, f);
    }
  }
}

#ifndef __KERNEL_CUDA__
ccl_device
#else
ccl_device_noinline
#endif
    void
    svm_node_attr_bump_dx(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
  NodeAttributeType type = NODE_ATTR_FLOAT;
  uint out_offset = 0;
  AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);

  /* fetch and store attribute */
  if (desc.type == NODE_ATTR_FLOAT) {
    float dx;
    float f = primitive_surface_attribute_float(kg, sd, desc, &dx, NULL);
    if (type == NODE_ATTR_FLOAT) {
      stack_store_float(stack, out_offset, f + dx);
    }
    else {
      stack_store_float3(stack, out_offset, make_float3(f + dx, f + dx, f + dx));
    }
  }
  else if (desc.type == NODE_ATTR_FLOAT2) {
    float2 dx;
    float2 f = primitive_attribute_float2(kg, sd, desc, &dx, NULL);
    if (type == NODE_ATTR_FLOAT) {
      stack_store_float(stack, out_offset, f.x + dx.x);
    }
    else {
      stack_store_float3(stack, out_offset, make_float3(f.x + dx.x, f.y + dx.y, 0.0f));
    }
  }
  else {
    float3 dx;
    float3 f = primitive_surface_attribute_float3(kg, sd, desc, &dx, NULL);
    if (type == NODE_ATTR_FLOAT) {
      stack_store_float(stack, out_offset, average(f + dx));
    }
    else {
      stack_store_float3(stack, out_offset, f + dx);
    }
  }
}

#ifndef __KERNEL_CUDA__
ccl_device
#else
ccl_device_noinline
#endif
    void
    svm_node_attr_bump_dy(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
  NodeAttributeType type = NODE_ATTR_FLOAT;
  uint out_offset = 0;
  AttributeDescriptor desc = svm_node_attr_init(kg, sd, node, &type, &out_offset);

  /* fetch and store attribute */
  if (desc.type == NODE_ATTR_FLOAT) {
    float dy;
    float f = primitive_surface_attribute_float(kg, sd, desc, NULL, &dy);
    if (type == NODE_ATTR_FLOAT) {
      stack_store_float(stack, out_offset, f + dy);
    }
    else {
      stack_store_float3(stack, out_offset, make_float3(f + dy, f + dy, f + dy));
    }
  }
  else if (desc.type == NODE_ATTR_FLOAT2) {
    float2 dy;
    float2 f = primitive_attribute_float2(kg, sd, desc, NULL, &dy);
    if (type == NODE_ATTR_FLOAT) {
      stack_store_float(stack, out_offset, f.x + dy.x);
    }
    else {
      stack_store_float3(stack, out_offset, make_float3(f.x + dy.x, f.y + dy.y, 0.0f));
    }
  }
  else {
    float3 dy;
    float3 f = primitive_surface_attribute_float3(kg, sd, desc, NULL, &dy);
    if (type == NODE_ATTR_FLOAT) {
      stack_store_float(stack, out_offset, average(f + dy));
    }
    else {
      stack_store_float3(stack, out_offset, f + dy);
    }
  }
}

CCL_NAMESPACE_END
