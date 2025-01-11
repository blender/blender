/* SPDX-FileCopyrightText: 2017-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline float frac(const float x, ccl_private int *ix)
{
  int i = float_to_int(x) - ((x < 0.0f) ? 1 : 0);
  *ix = i;
  return x - (float)i;
}

/* w0, w1, w2, and w3 are the four cubic B-spline basis functions. */
ccl_device float cubic_w0(const float a)
{
  return (1.0f / 6.0f) * (a * (a * (-a + 3.0f) - 3.0f) + 1.0f);
}
ccl_device float cubic_w1(const float a)
{
  return (1.0f / 6.0f) * (a * a * (3.0f * a - 6.0f) + 4.0f);
}
ccl_device float cubic_w2(const float a)
{
  return (1.0f / 6.0f) * (a * (a * (-3.0f * a + 3.0f) + 3.0f) + 1.0f);
}
ccl_device float cubic_w3(const float a)
{
  return (1.0f / 6.0f) * (a * a * a);
}

/* g0 and g1 are the two amplitude functions. */
ccl_device float cubic_g0(const float a)
{
  return cubic_w0(a) + cubic_w1(a);
}
ccl_device float cubic_g1(const float a)
{
  return cubic_w2(a) + cubic_w3(a);
}

/* h0 and h1 are the two offset functions */
ccl_device float cubic_h0(const float a)
{
  return (cubic_w1(a) / cubic_g0(a)) - 1.0f;
}
ccl_device float cubic_h1(const float a)
{
  return (cubic_w3(a) / cubic_g1(a)) + 1.0f;
}

/* Fast bicubic texture lookup using 4 bilinear lookups, adapted from CUDA samples. */
template<typename T>
ccl_device_noinline T kernel_tex_image_interp_bicubic(const ccl_global TextureInfo &info,
                                                      float x,
                                                      float y)
{
  ccl_gpu_tex_object_2D tex = (ccl_gpu_tex_object_2D)info.data;

  x = (x * info.width) - 0.5f;
  y = (y * info.height) - 0.5f;

  float px = floorf(x);
  float py = floorf(y);
  float fx = x - px;
  float fy = y - py;

  float g0x = cubic_g0(fx);
  float g1x = cubic_g1(fx);
  /* Note +0.5 offset to compensate for CUDA linear filtering convention. */
  float x0 = (px + cubic_h0(fx) + 0.5f) / info.width;
  float x1 = (px + cubic_h1(fx) + 0.5f) / info.width;
  float y0 = (py + cubic_h0(fy) + 0.5f) / info.height;
  float y1 = (py + cubic_h1(fy) + 0.5f) / info.height;

  return cubic_g0(fy) * (g0x * ccl_gpu_tex_object_read_2D<T>(tex, x0, y0) +
                         g1x * ccl_gpu_tex_object_read_2D<T>(tex, x1, y0)) +
         cubic_g1(fy) * (g0x * ccl_gpu_tex_object_read_2D<T>(tex, x0, y1) +
                         g1x * ccl_gpu_tex_object_read_2D<T>(tex, x1, y1));
}

ccl_device float4 kernel_tex_image_interp(KernelGlobals kg, const int id, const float x, float y)
{
  const ccl_global TextureInfo &info = kernel_data_fetch(texture_info, id);

  /* float4, byte4, ushort4 and half4 */
  const int texture_type = info.data_type;
  if (texture_type == IMAGE_DATA_TYPE_FLOAT4 || texture_type == IMAGE_DATA_TYPE_BYTE4 ||
      texture_type == IMAGE_DATA_TYPE_HALF4 || texture_type == IMAGE_DATA_TYPE_USHORT4)
  {
    if (info.interpolation == INTERPOLATION_CUBIC || info.interpolation == INTERPOLATION_SMART) {
      return kernel_tex_image_interp_bicubic<float4>(info, x, y);
    }
    else {
      ccl_gpu_tex_object_2D tex = (ccl_gpu_tex_object_2D)info.data;
      return ccl_gpu_tex_object_read_2D<float4>(tex, x, y);
    }
  }
  /* float, byte and half */
  else {
    float f;

    if (info.interpolation == INTERPOLATION_CUBIC || info.interpolation == INTERPOLATION_SMART) {
      f = kernel_tex_image_interp_bicubic<float>(info, x, y);
    }
    else {
      ccl_gpu_tex_object_2D tex = (ccl_gpu_tex_object_2D)info.data;
      f = ccl_gpu_tex_object_read_2D<float>(tex, x, y);
    }

    return make_float4(f, f, f, 1.0f);
  }
}

CCL_NAMESPACE_END
