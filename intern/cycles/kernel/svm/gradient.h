/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Gradient */

ccl_device float svm_gradient(float3 p, NodeGradientType type)
{
  float x, y, z;

  x = p.x;
  y = p.y;
  z = p.z;

  if (type == NODE_BLEND_LINEAR) {
    return x;
  }
  else if (type == NODE_BLEND_QUADRATIC) {
    float r = fmaxf(x, 0.0f);
    return r * r;
  }
  else if (type == NODE_BLEND_EASING) {
    float r = fminf(fmaxf(x, 0.0f), 1.0f);
    float t = r * r;

    return (3.0f * t - 2.0f * t * r);
  }
  else if (type == NODE_BLEND_DIAGONAL) {
    return (x + y) * 0.5f;
  }
  else if (type == NODE_BLEND_RADIAL) {
    return atan2f(y, x) / M_2PI_F + 0.5f;
  }
  else {
    /* Bias a little bit for the case where p is a unit length vector,
     * to get exactly zero instead of a small random value depending
     * on float precision. */
    float r = fmaxf(0.999999f - sqrtf(x * x + y * y + z * z), 0.0f);

    if (type == NODE_BLEND_QUADRATIC_SPHERE)
      return r * r;
    else if (type == NODE_BLEND_SPHERICAL)
      return r;
  }

  return 0.0f;
}

ccl_device_noinline void svm_node_tex_gradient(ccl_private ShaderData *sd,
                                               ccl_private float *stack,
                                               uint4 node)
{
  uint type, co_offset, color_offset, fac_offset;

  svm_unpack_node_uchar4(node.y, &type, &co_offset, &fac_offset, &color_offset);

  float3 co = stack_load_float3(stack, co_offset);

  float f = svm_gradient(co, (NodeGradientType)type);
  f = saturatef(f);

  if (stack_valid(fac_offset))
    stack_store_float(stack, fac_offset, f);
  if (stack_valid(color_offset))
    stack_store_float3(stack, color_offset, make_float3(f, f, f));
}

CCL_NAMESPACE_END
