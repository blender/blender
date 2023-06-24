/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Checker */

ccl_device float svm_checker(float3 p)
{
  /* avoid precision issues on unit coordinates */
  p.x = (p.x + 0.000001f) * 0.999999f;
  p.y = (p.y + 0.000001f) * 0.999999f;
  p.z = (p.z + 0.000001f) * 0.999999f;

  int xi = abs(float_to_int(floorf(p.x)));
  int yi = abs(float_to_int(floorf(p.y)));
  int zi = abs(float_to_int(floorf(p.z)));

  return ((xi % 2 == yi % 2) == (zi % 2)) ? 1.0f : 0.0f;
}

ccl_device_noinline void svm_node_tex_checker(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              ccl_private float *stack,
                                              uint4 node)
{
  uint co_offset, color1_offset, color2_offset, scale_offset;
  uint color_offset, fac_offset;

  svm_unpack_node_uchar4(node.y, &co_offset, &color1_offset, &color2_offset, &scale_offset);
  svm_unpack_node_uchar2(node.z, &color_offset, &fac_offset);

  float3 co = stack_load_float3(stack, co_offset);
  float3 color1 = stack_load_float3(stack, color1_offset);
  float3 color2 = stack_load_float3(stack, color2_offset);
  float scale = stack_load_float_default(stack, scale_offset, node.w);

  float f = svm_checker(co * scale);

  if (stack_valid(color_offset))
    stack_store_float3(stack, color_offset, (f == 1.0f) ? color1 : color2);
  if (stack_valid(fac_offset))
    stack_store_float(stack, fac_offset, f);
}

CCL_NAMESPACE_END
