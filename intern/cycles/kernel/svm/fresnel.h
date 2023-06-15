/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Fresnel Node */

ccl_device_noinline void svm_node_fresnel(ccl_private ShaderData *sd,
                                          ccl_private float *stack,
                                          uint ior_offset,
                                          uint ior_value,
                                          uint node)
{
  uint normal_offset, out_offset;
  svm_unpack_node_uchar2(node, &normal_offset, &out_offset);
  float eta = (stack_valid(ior_offset)) ? stack_load_float(stack, ior_offset) :
                                          __uint_as_float(ior_value);
  float3 normal_in = stack_valid(normal_offset) ? stack_load_float3(stack, normal_offset) : sd->N;

  eta = fmaxf(eta, 1e-5f);
  eta = (sd->flag & SD_BACKFACING) ? 1.0f / eta : eta;

  float f = fresnel_dielectric_cos(dot(sd->wi, normal_in), eta);

  stack_store_float(stack, out_offset, f);
}

/* Layer Weight Node */

ccl_device_noinline void svm_node_layer_weight(ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               uint4 node)
{
  uint blend_offset = node.y;
  uint blend_value = node.z;

  uint type, normal_offset, out_offset;
  svm_unpack_node_uchar3(node.w, &type, &normal_offset, &out_offset);

  float blend = (stack_valid(blend_offset)) ? stack_load_float(stack, blend_offset) :
                                              __uint_as_float(blend_value);
  float3 normal_in = (stack_valid(normal_offset)) ? stack_load_float3(stack, normal_offset) :
                                                    sd->N;

  float f;

  if (type == NODE_LAYER_WEIGHT_FRESNEL) {
    float eta = fmaxf(1.0f - blend, 1e-5f);
    eta = (sd->flag & SD_BACKFACING) ? eta : 1.0f / eta;

    f = fresnel_dielectric_cos(dot(sd->wi, normal_in), eta);
  }
  else {
    f = fabsf(dot(sd->wi, normal_in));

    if (blend != 0.5f) {
      blend = clamp(blend, 0.0f, 1.0f - 1e-5f);
      blend = (blend < 0.5f) ? 2.0f * blend : 0.5f / (1.0f - blend);

      f = powf(f, blend);
    }

    f = 1.0f - f;
  }

  stack_store_float(stack, out_offset, f);
}

CCL_NAMESPACE_END
