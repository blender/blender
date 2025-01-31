/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/geom/attribute.h"
#include "kernel/geom/object.h"
#include "kernel/geom/primitive.h"

#include "kernel/svm/util.h"

#include "kernel/util/differential.h"

CCL_NAMESPACE_BEGIN

/* Bump Node */
template<uint node_feature_mask>
ccl_device_noinline void svm_node_set_bump(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           ccl_private float *stack,
                                           const uint4 node)
{
  uint out_offset;
  uint bump_state_offset;
  uint dummy;
  svm_unpack_node_uchar4(node.w, &out_offset, &bump_state_offset, &dummy, &dummy);

#ifdef __RAY_DIFFERENTIALS__
  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    /* get normal input */
    uint normal_offset;
    uint scale_offset;
    uint invert;
    uint use_object_space;
    svm_unpack_node_uchar4(node.y, &normal_offset, &scale_offset, &invert, &use_object_space);

    float3 normal_in = stack_valid(normal_offset) ? stack_load_float3(stack, normal_offset) :
                                                    sd->N;

    /* If we have saved bump state, read the full differential from there.
     * Just using the compact form in those cases leads to incorrect normals (see #111588). */
    differential3 dP;
    if (bump_state_offset == SVM_STACK_INVALID) {
      dP = differential_from_compact(sd->Ng, sd->dP);
    }
    else {
      dP.dx = stack_load_float3(stack, bump_state_offset + 4);
      dP.dy = stack_load_float3(stack, bump_state_offset + 7);
    }

    if (use_object_space) {
      object_inverse_normal_transform(kg, sd, &normal_in);
      object_inverse_dir_transform(kg, sd, &dP.dx);
      object_inverse_dir_transform(kg, sd, &dP.dy);
    }

    /* get surface tangents from normal */
    const float3 Rx = cross(dP.dy, normal_in);
    const float3 Ry = cross(normal_in, dP.dx);

    /* get bump values */
    uint c_offset;
    uint x_offset;
    uint y_offset;
    uint strength_offset;
    svm_unpack_node_uchar4(node.z, &c_offset, &x_offset, &y_offset, &strength_offset);

    const float h_c = stack_load_float(stack, c_offset);
    const float h_x = stack_load_float(stack, x_offset);
    const float h_y = stack_load_float(stack, y_offset);

    /* compute surface gradient and determinant */
    const float det = dot(dP.dx, Rx);
    const float3 surfgrad = (h_x - h_c) * Rx + (h_y - h_c) * Ry;

    const float absdet = fabsf(det);

    float strength = stack_load_float(stack, strength_offset);
    float scale = stack_load_float(stack, scale_offset);

    if (invert) {
      scale *= -1.0f;
    }

    strength = max(strength, 0.0f);

    /* Compute and output perturbed normal.
     * dP'dx = dPdx + scale * (h_x - h_c) / BUMP_DX * normal
     * dP'dy = dPdy + scale * (h_y - h_c) / BUMP_DY * normal
     * N' = cross(dP'dx, dP'dy)
     *    = cross(dPdx, dPdy) - scale * ((h_y - h_c) / BUMP_DY * Ry + (h_x - h_c) / BUMP_DX * Rx)
     *    ≈ det * normal_in - scale * surfgrad / BUMP_DX
     */
    kernel_assert(BUMP_DX == BUMP_DY);
    float3 normal_out = safe_normalize(BUMP_DX * absdet * normal_in -
                                       scale * signf(det) * surfgrad);
    if (is_zero(normal_out)) {
      normal_out = normal_in;
    }
    else {
      normal_out = normalize(strength * normal_out + (1.0f - strength) * normal_in);
    }

    if (use_object_space) {
      object_normal_transform(kg, sd, &normal_out);
    }

    stack_store_float3(stack, out_offset, normal_out);
  }
  else {
    stack_store_float3(stack, out_offset, zero_float3());
  }
#endif
}

/* Displacement Node */

template<uint node_feature_mask>
ccl_device void svm_node_set_displacement(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          ccl_private float *stack,
                                          const uint fac_offset)
{
  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    const float3 dP = stack_load_float3(stack, fac_offset);
    sd->P += dP;
  }
}

template<uint node_feature_mask>
ccl_device_noinline void svm_node_displacement(KernelGlobals kg,
                                               ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               const uint4 node)
{
  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    uint height_offset;
    uint midlevel_offset;
    uint scale_offset;
    uint normal_offset;
    svm_unpack_node_uchar4(
        node.y, &height_offset, &midlevel_offset, &scale_offset, &normal_offset);

    const float height = stack_load_float(stack, height_offset);
    const float midlevel = stack_load_float(stack, midlevel_offset);
    const float scale = stack_load_float(stack, scale_offset);
    const float3 normal = stack_valid(normal_offset) ? stack_load_float3(stack, normal_offset) :
                                                       sd->N;
    const uint space = node.w;

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
ccl_device_noinline int svm_node_vector_displacement(KernelGlobals kg,
                                                     ccl_private ShaderData *sd,
                                                     ccl_private float *stack,
                                                     const uint4 node,
                                                     int offset)
{
  const uint4 data_node = read_node(kg, &offset);
  uint vector_offset;
  uint midlevel_offset;
  uint scale_offset;
  uint displacement_offset;
  svm_unpack_node_uchar4(
      node.y, &vector_offset, &midlevel_offset, &scale_offset, &displacement_offset);

  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    const uint space = data_node.x;

    const float3 vector = stack_load_float3(stack, vector_offset);
    const float midlevel = stack_load_float(stack, midlevel_offset);
    const float scale = stack_load_float(stack, scale_offset);
    float3 dP = (vector - make_float3(midlevel, midlevel, midlevel)) * scale;

    if (space == NODE_NORMAL_MAP_TANGENT) {
      /* Tangent space. */
      float3 normal = sd->N;
      object_inverse_normal_transform(kg, sd, &normal);

      const AttributeDescriptor attr = find_attribute(kg, sd, node.z);
      float3 tangent;
      if (attr.offset != ATTR_STD_NOT_FOUND) {
        tangent = primitive_surface_attribute_float3(kg, sd, attr, nullptr, nullptr);
      }
      else {
        tangent = normalize(sd->dPdu);
      }

      float3 bitangent = safe_normalize(cross(normal, tangent));
      const AttributeDescriptor attr_sign = find_attribute(kg, sd, node.w);
      if (attr_sign.offset != ATTR_STD_NOT_FOUND) {
        const float sign = primitive_surface_attribute_float(kg, sd, attr_sign, nullptr, nullptr);
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
