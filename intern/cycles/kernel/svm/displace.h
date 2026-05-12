/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/geom/attribute.h"
#include "kernel/geom/object.h"
#include "kernel/geom/primitive.h"

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

#include "kernel/util/differential.h"

CCL_NAMESPACE_BEGIN

/* Bump Node */
template<uint node_feature_mask>
ccl_device_noinline void svm_node_set_bump(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           ccl_private float *stack,
                                           const ccl_global SVMNodeSetBump &node)
{
#ifdef __RAY_DIFFERENTIALS__
  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    /* get normal input */
    float3 normal_in = stack_load_float3_default(stack, node.normal_offset, sd->N);

    /* If we have saved bump state, read the full differential from there.
     * Just using the compact form in those cases leads to incorrect normals (see #111588). */
    differential3 dP;
    if (node.bump_state_offset == SVM_STACK_INVALID) {
      dP = differential_from_compact(sd->Ng, sd->dP);
    }
    else {
      dP.dx = stack_load_float3(stack, node.bump_state_offset + 4);
      dP.dy = stack_load_float3(stack, node.bump_state_offset + 7);
    }

    if (node.use_object_space) {
      object_inverse_normal_transform(kg, sd, &normal_in);
      object_inverse_dir_transform(kg, sd, &dP.dx);
      object_inverse_dir_transform(kg, sd, &dP.dy);
    }

    /* get surface tangents from normal */
    const float3 Rx = cross(dP.dy, normal_in);
    const float3 Ry = cross(normal_in, dP.dx);

    /* get bump values */
    const float h_c = stack_load_float(stack, node.center_offset);
    const float h_x = stack_load_float(stack, node.dx_offset);
    const float h_y = stack_load_float(stack, node.dy_offset);

    /* compute surface gradient and determinant */
    const float det = dot(dP.dx, Rx);
    const float3 surfgrad = (h_x - h_c) * Rx + (h_y - h_c) * Ry;

    const float absdet = fabsf(det);

    float strength = stack_load(stack, node.strength);
    float scale = stack_load(stack, node.scale);

    if (node.invert) {
      scale *= -1.0f;
    }

    strength = max(strength, 0.0f);

    /* Compute and output perturbed normal.
     * dP'dx = dPdx + scale * (h_x - h_c) / filter_width * normal
     * dP'dy = dPdy + scale * (h_y - h_c) / filter_width * normal
     * N' = cross(dP'dx, dP'dy)
     *    = cross(dPdx, dPdy) - scale * ((h_y - h_c) / filter_width * Ry + (h_x - h_c) /
     * filter_width * Rx) ≈ det * normal_in - scale * surfgrad / filter_width
     */
    float3 normal_out = safe_normalize(node.bump_filter_width * absdet * normal_in -
                                       scale * signf(det) * surfgrad);
    if (is_zero(normal_out)) {
      normal_out = normal_in;
    }
    else {
      normal_out = normalize(strength * normal_out + (1.0f - strength) * normal_in);
    }

    if (node.use_object_space) {
      object_normal_transform(kg, sd, &normal_out);
    }

    stack_store_float3(stack, node.out_offset, normal_out);
  }
  else {
    stack_store_float3(stack, node.out_offset, zero_float3());
  }
#endif
}

/* Displacement Node */

template<uint node_feature_mask>
ccl_device void svm_node_set_displacement(ccl_private ShaderData *sd,
                                          ccl_private float *stack,
                                          const ccl_global SVMNodeSetDisplacement &node)
{
  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    const float3 dP = stack_load_float3(stack, node.fac_offset);
    sd->P += dP;
  }
}

template<uint node_feature_mask>
ccl_device_noinline void svm_node_displacement(KernelGlobals kg,
                                               ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               const ccl_global SVMNodeDisplacement &node)
{
  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    const float height = stack_load(stack, node.height);
    const float midlevel = stack_load(stack, node.midlevel);
    const float scale = stack_load(stack, node.scale);
    const float3 normal = stack_load_float3_default(stack, node.normal_offset, sd->N);

    float3 dP = normal;

    if (node.space == NODE_NORMAL_MAP_OBJECT) {
      /* Object space. */
      object_inverse_normal_transform(kg, sd, &dP);
      dP *= (height - midlevel) * scale;
      object_dir_transform(kg, sd, &dP);
    }
    else {
      /* World space. */
      dP *= (height - midlevel) * scale;
    }

    stack_store_float3(stack, node.out_offset, dP);
  }
  else {
    stack_store_float3(stack, node.out_offset, zero_float3());
  }
}

template<uint node_feature_mask>
ccl_device_noinline void svm_node_vector_displacement(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    ccl_private float *stack,
    const ccl_global SVMNodeVectorDisplacement &node)
{
  IF_KERNEL_NODES_FEATURE(BUMP)
  {
    const float3 vector = stack_load(stack, node.vector);
    const float midlevel = stack_load(stack, node.midlevel);
    const float scale = stack_load(stack, node.scale);
    float3 dP = (vector - make_float3(midlevel, midlevel, midlevel)) * scale;

    if (node.space == NODE_NORMAL_MAP_TANGENT) {
      /* Tangent space. */
      float3 normal = sd->N;
      object_inverse_normal_transform(kg, sd, &normal);

      const AttributeDescriptor attr = find_attribute(kg, sd, node.attr);
      float3 tangent;
      if (is_attribute_found(attr)) {
        tangent = primitive_surface_attribute<float3>(kg, sd, attr);
      }
      else {
        tangent = normalize(sd->dPdu);
      }

      float3 bitangent = safe_normalize(cross(normal, tangent));
      const AttributeDescriptor attr_sign = find_attribute(kg, sd, node.attr_sign);
      if (is_attribute_found(attr_sign)) {
        const float sign = primitive_surface_attribute<float>(kg, sd, attr_sign);
        bitangent *= sign;
      }

      dP = tangent * dP.x + normal * dP.y + bitangent * dP.z;
    }

    if (node.space != NODE_NORMAL_MAP_WORLD) {
      /* Tangent or object space. */
      object_dir_transform(kg, sd, &dP);
    }

    stack_store_float3(stack, node.displacement_offset, dP);
  }
  else {
    stack_store_float3(stack, node.displacement_offset, zero_float3());
  }
}

CCL_NAMESPACE_END
