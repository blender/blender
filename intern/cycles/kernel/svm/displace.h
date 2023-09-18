/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/sample/mapping.h"

CCL_NAMESPACE_BEGIN

/* Bump Node */
template<uint node_feature_mask>
ccl_device_noinline void svm_node_set_bump(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           ccl_private float *stack,
                                           uint4 node)
{
#ifdef __RAY_DIFFERENTIALS__
  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    /* get normal input */
    uint normal_offset, scale_offset, invert, use_object_space;
    svm_unpack_node_uchar4(node.y, &normal_offset, &scale_offset, &invert, &use_object_space);

    float3 normal_in = stack_valid(normal_offset) ? stack_load_float3(stack, normal_offset) :
                                                    sd->N;

    differential3 dP = differential_from_compact(sd->Ng, sd->dP);

    if (use_object_space) {
      object_inverse_normal_transform(kg, sd, &normal_in);
      object_inverse_dir_transform(kg, sd, &dP.dx);
      object_inverse_dir_transform(kg, sd, &dP.dy);
    }

    /* get surface tangents from normal */
    float3 Rx = cross(dP.dy, normal_in);
    float3 Ry = cross(normal_in, dP.dx);

    /* get bump values */
    uint c_offset, x_offset, y_offset, strength_offset;
    svm_unpack_node_uchar4(node.z, &c_offset, &x_offset, &y_offset, &strength_offset);

    float h_c = stack_load_float(stack, c_offset);
    float h_x = stack_load_float(stack, x_offset);
    float h_y = stack_load_float(stack, y_offset);

    /* compute surface gradient and determinant */
    float det = dot(dP.dx, Rx);
    float3 surfgrad = (h_x - h_c) * Rx + (h_y - h_c) * Ry;

    float absdet = fabsf(det);

    float strength = stack_load_float(stack, strength_offset);
    float scale = stack_load_float(stack, scale_offset);

    if (invert)
      scale *= -1.0f;

    strength = max(strength, 0.0f);

    /* compute and output perturbed normal */
    float3 normal_out = safe_normalize(absdet * normal_in - scale * signf(det) * surfgrad);
    if (is_zero(normal_out)) {
      normal_out = normal_in;
    }
    else {
      normal_out = normalize(strength * normal_out + (1.0f - strength) * normal_in);
    }

    if (use_object_space) {
      object_normal_transform(kg, sd, &normal_out);
    }

    stack_store_float3(stack, node.w, normal_out);
  }
  else {
    stack_store_float3(stack, node.w, zero_float3());
  }
#endif
}

/* Displacement Node */

template<uint node_feature_mask>
ccl_device void svm_node_set_displacement(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          ccl_private float *stack,
                                          uint fac_offset)
{
  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    float3 dP = stack_load_float3(stack, fac_offset);
    sd->P += dP;
  }
}

template<uint node_feature_mask>
ccl_device_noinline void svm_node_displacement(KernelGlobals kg,
                                               ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               uint4 node)
{
  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    uint height_offset, midlevel_offset, scale_offset, normal_offset;
    svm_unpack_node_uchar4(
        node.y, &height_offset, &midlevel_offset, &scale_offset, &normal_offset);

    float height = stack_load_float(stack, height_offset);
    float midlevel = stack_load_float(stack, midlevel_offset);
    float scale = stack_load_float(stack, scale_offset);
    float3 normal = stack_valid(normal_offset) ? stack_load_float3(stack, normal_offset) : sd->N;
    uint space = node.w;

    float3 dP = normal;

    if (space == NODE_NORMAL_MAP_OBJECT) {
      /* Object space. */
      object_inverse_normal_transform(kg, sd, &dP);
      dP *= (height - midlevel) * scale;
      object_dir_transform(kg, sd, &dP);
    }
    else {
      /* World space. */
      dP *= (height - midlevel) * scale;
    }

    stack_store_float3(stack, node.z, dP);
  }
  else {
    stack_store_float3(stack, node.z, zero_float3());
  }
}

template<uint node_feature_mask>
ccl_device_noinline int svm_node_vector_displacement(
    KernelGlobals kg, ccl_private ShaderData *sd, ccl_private float *stack, uint4 node, int offset)
{
  uint4 data_node = read_node(kg, &offset);
  uint vector_offset, midlevel_offset, scale_offset, displacement_offset;
  svm_unpack_node_uchar4(
      node.y, &vector_offset, &midlevel_offset, &scale_offset, &displacement_offset);

  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    uint space = data_node.x;

    float3 vector = stack_load_float3(stack, vector_offset);
    float midlevel = stack_load_float(stack, midlevel_offset);
    float scale = stack_load_float(stack, scale_offset);
    float3 dP = (vector - make_float3(midlevel, midlevel, midlevel)) * scale;

    if (space == NODE_NORMAL_MAP_TANGENT) {
      /* Tangent space. */
      float3 normal = sd->N;
      object_inverse_normal_transform(kg, sd, &normal);

      const AttributeDescriptor attr = find_attribute(kg, sd, node.z);
      float3 tangent;
      if (attr.offset != ATTR_STD_NOT_FOUND) {
        tangent = primitive_surface_attribute_float3(kg, sd, attr, NULL, NULL);
      }
      else {
        tangent = normalize(sd->dPdu);
      }

      float3 bitangent = safe_normalize(cross(normal, tangent));
      const AttributeDescriptor attr_sign = find_attribute(kg, sd, node.w);
      if (attr_sign.offset != ATTR_STD_NOT_FOUND) {
        float sign = primitive_surface_attribute_float(kg, sd, attr_sign, NULL, NULL);
        bitangent *= sign;
      }

      dP = tangent * dP.x + normal * dP.y + bitangent * dP.z;
    }

    if (space != NODE_NORMAL_MAP_WORLD) {
      /* Tangent or object space. */
      object_dir_transform(kg, sd, &dP);
    }

    stack_store_float3(stack, displacement_offset, dP);
  }
  else {
    stack_store_float3(stack, displacement_offset, zero_float3());
    (void)data_node;
  }

  return offset;
}

CCL_NAMESPACE_END
