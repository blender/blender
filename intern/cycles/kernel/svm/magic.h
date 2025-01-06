/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Magic */

ccl_device_noinline_cpu float3 svm_magic(const float3 p,
                                         const float scale,
                                         const int n,
                                         float distortion)
{
  /*
   * Prevent NaNs due to input p
   * Sin and Cosine are periodic about [0 2*PI) so the following
   * will yield a more accurate result. As it stops the input values
   * going out of range for floats which caused a NaN. The
   * calculation of (px + py + pz)*5 can cause an Inf when one or more
   * values are very large the cos or sin of this results in a NaN
   * It also addresses the case where one dimension is large relative
   * to another which caused banding due to the loss of precision in the
   * smaller value. This is due to the value in the -2*PI to 2*PI range
   * effectively being lost due to floating point precision.
   */
  const float px = fmodf(p.x, M_2PI_F);
  const float py = fmodf(p.y, M_2PI_F);
  const float pz = fmodf(p.z, M_2PI_F);

  float x = sinf((px + py + pz) * 5.0f * scale);
  float y = cosf((-px + py - pz) * 5.0f * scale);
  float z = -cosf((-px - py + pz) * 5.0f * scale);

  if (n > 0) {
    x *= distortion;
    y *= distortion;
    z *= distortion;
    y = -cosf(x - y + z);
    y *= distortion;

    if (n > 1) {
      x = cosf(x - y - z);
      x *= distortion;

      if (n > 2) {
        z = sinf(-x - y - z);
        z *= distortion;

        if (n > 3) {
          x = -cosf(-x + y - z);
          x *= distortion;

          if (n > 4) {
            y = -sinf(-x + y + z);
            y *= distortion;

            if (n > 5) {
              y = -cosf(-x + y + z);
              y *= distortion;

              if (n > 6) {
                x = cosf(x + y + z);
                x *= distortion;

                if (n > 7) {
                  z = sinf(x + y - z);
                  z *= distortion;

                  if (n > 8) {
                    x = -cosf(-x - y + z);
                    x *= distortion;

                    if (n > 9) {
                      y = -sinf(x - y + z);
                      y *= distortion;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  if (distortion != 0.0f) {
    distortion *= 2.0f;
    x /= distortion;
    y /= distortion;
    z /= distortion;
  }

  return make_float3(0.5f - x, 0.5f - y, 0.5f - z);
}

ccl_device_noinline int svm_node_tex_magic(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           ccl_private float *stack,
                                           const uint4 node,
                                           int offset)
{
  uint depth;
  uint scale_offset;
  uint distortion_offset;
  uint co_offset;
  uint fac_offset;
  uint color_offset;

  svm_unpack_node_uchar3(node.y, &depth, &color_offset, &fac_offset);
  svm_unpack_node_uchar3(node.z, &co_offset, &scale_offset, &distortion_offset);

  const uint4 node2 = read_node(kg, &offset);
  const float3 co = stack_load_float3(stack, co_offset);
  const float scale = stack_load_float_default(stack, scale_offset, node2.x);
  const float distortion = stack_load_float_default(stack, distortion_offset, node2.y);

  const float3 color = svm_magic(co, scale, depth, distortion);

  if (stack_valid(fac_offset)) {
    stack_store_float(stack, fac_offset, average(color));
  }
  if (stack_valid(color_offset)) {
    stack_store_float3(stack, color_offset, color);
  }
  return offset;
}

CCL_NAMESPACE_END
