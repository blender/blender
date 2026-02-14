/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "util/half.h"
#include "util/types_image.h"

CCL_NAMESPACE_BEGIN

/* Make template functions private so symbols don't conflict between kernels with different
 * instruction sets. */
namespace {

#define SET_CUBIC_SPLINE_WEIGHTS(u, t) \
  { \
    u[0] = (((-1.0f / 6.0f) * t + 0.5f) * t - 0.5f) * t + (1.0f / 6.0f); \
    u[1] = ((0.5f * t - 1.0f) * t) * t + (2.0f / 3.0f); \
    u[2] = ((-0.5f * t + 0.5f) * t + 0.5f) * t + (1.0f / 6.0f); \
    u[3] = (1.0f / 6.0f) * t * t * t; \
  } \
  (void)0

ccl_device_inline float frac(const float x, int *ix)
{
  int i = float_to_int(x) - ((x < 0.0f) ? 1 : 0);
  *ix = i;
  return x - (float)i;
}

template<typename TexT, typename OutT = float4> struct ImageInterpolator {

  static ccl_always_inline OutT zero()
  {
    if constexpr (std::is_same_v<OutT, float4>) {
      return zero_float4();
    }
    else {
      return 0.0f;
    }
  }

  static ccl_always_inline float4 read(const float4 r)
  {
    return r;
  }

  static ccl_always_inline float4 read(const uchar4 r)
  {
    const float f = 1.0f / 255.0f;
    return make_float4(r.x * f, r.y * f, r.z * f, r.w * f);
  }

  static ccl_always_inline float read(const uchar r)
  {
    return r * (1.0f / 255.0f);
  }

  static ccl_always_inline float read(const float r)
  {
    return r;
  }

  static ccl_always_inline float4 read(half4 r)
  {
    return half4_to_float4_image(r);
  }

  static ccl_always_inline float read(half r)
  {
    return half_to_float_image(r);
  }

  static ccl_always_inline float read(const uint16_t r)
  {
    return r * (1.0f / 65535.0f);
  }

  static ccl_always_inline float4 read(ushort4 r)
  {
    const float f = 1.0f / 65535.0f;
    return make_float4(r.x * f, r.y * f, r.z * f, r.w * f);
  }

  /* Read 2D Texture Data
   * Does not check if data request is in bounds. */
  static ccl_always_inline OutT
  read(const TexT *data, const int x, int y, const int width, const int /*height*/)
  {
    return read(data[y * width + x]);
  }

  /* Read 2D Texture Data Clip
   * Returns transparent black if data request is out of bounds. */
  static ccl_always_inline OutT
  read_clip(const TexT *data, const int x, int y, const int width, const int height)
  {
    if (x < 0 || x >= width || y < 0 || y >= height) {
      return zero();
    }
    return read(data[y * width + x]);
  }

  static ccl_always_inline int wrap_periodic(int x, const int width)
  {
    x %= width;
    if (x < 0) {
      x += width;
    }
    return x;
  }

  static ccl_always_inline int wrap_clamp(const int x, const int width)
  {
    return clamp(x, 0, width - 1);
  }

  static ccl_always_inline int wrap_mirror(const int x, const int width)
  {
    const int m = abs(x + (x < 0)) % (2 * width);
    if (m >= width) {
      return 2 * width - m - 1;
    }
    return m;
  }

  /* ********  2D interpolation ******** */

  static ccl_always_inline OutT interp_closest(const KernelImageInfo &info, const float x, float y)
  {
    const int width = info.width;
    const int height = info.height;
    int ix, iy;
    frac(x * (float)width, &ix);
    frac(y * (float)height, &iy);
    switch (info.extension) {
      case EXTENSION_REPEAT:
        ix = wrap_periodic(ix, width);
        iy = wrap_periodic(iy, height);
        break;
      case EXTENSION_CLIP:
        /* No samples are inside the clip region. */
        if (ix < 0 || ix >= width || iy < 0 || iy >= height) {
          return zero();
        }
        break;
      case EXTENSION_EXTEND:
        ix = wrap_clamp(ix, width);
        iy = wrap_clamp(iy, height);
        break;
      case EXTENSION_MIRROR:
        ix = wrap_mirror(ix, width);
        iy = wrap_mirror(iy, height);
        break;
      default:
        kernel_assert(0);
        return zero();
    }

    const TexT *data = (const TexT *)info.data;
    return read(data, ix, iy, width, height);
  }

  static ccl_always_inline OutT interp_linear(const KernelImageInfo &info, const float x, float y)
  {
    const int width = info.width;
    const int height = info.height;

    /* A -0.5 offset is used to center the linear samples around the sample point. */
    int ix, iy;
    int nix, niy;
    const float tx = frac(x * (float)width - 0.5f, &ix);
    const float ty = frac(y * (float)height - 0.5f, &iy);
    const TexT *data = (const TexT *)info.data;

    switch (info.extension) {
      case EXTENSION_REPEAT:
        ix = wrap_periodic(ix, width);
        nix = wrap_periodic(ix + 1, width);

        iy = wrap_periodic(iy, height);
        niy = wrap_periodic(iy + 1, height);
        break;
      case EXTENSION_CLIP:
        /* No linear samples are inside the clip region. */
        if (ix < -1 || ix >= width || iy < -1 || iy >= height) {
          return zero();
        }
        nix = ix + 1;
        niy = iy + 1;
        return (1.0f - ty) * (1.0f - tx) * read_clip(data, ix, iy, width, height) +
               (1.0f - ty) * tx * read_clip(data, nix, iy, width, height) +
               ty * (1.0f - tx) * read_clip(data, ix, niy, width, height) +
               ty * tx * read_clip(data, nix, niy, width, height);
      case EXTENSION_EXTEND:
        nix = wrap_clamp(ix + 1, width);
        ix = wrap_clamp(ix, width);
        niy = wrap_clamp(iy + 1, height);
        iy = wrap_clamp(iy, height);
        break;
      case EXTENSION_MIRROR:
        nix = wrap_mirror(ix + 1, width);
        ix = wrap_mirror(ix, width);
        niy = wrap_mirror(iy + 1, height);
        iy = wrap_mirror(iy, height);
        break;
      default:
        kernel_assert(0);
        return zero();
    }

    return (1.0f - ty) * (1.0f - tx) * read(data, ix, iy, width, height) +
           (1.0f - ty) * tx * read(data, nix, iy, width, height) +
           ty * (1.0f - tx) * read(data, ix, niy, width, height) +
           ty * tx * read(data, nix, niy, width, height);
  }

  static ccl_always_inline OutT interp_cubic(const KernelImageInfo &info, const float x, float y)
  {
    const int width = info.width;
    const int height = info.height;

    /* A -0.5 offset is used to center the cubic samples around the sample point. */
    int ix, iy;
    const float tx = frac(x * (float)width - 0.5f, &ix);
    const float ty = frac(y * (float)height - 0.5f, &iy);

    int pix, piy;
    int nix, niy;
    int nnix, nniy;

    switch (info.extension) {
      case EXTENSION_REPEAT:
        ix = wrap_periodic(ix, width);
        pix = wrap_periodic(ix - 1, width);
        nix = wrap_periodic(ix + 1, width);
        nnix = wrap_periodic(ix + 2, width);

        iy = wrap_periodic(iy, height);
        piy = wrap_periodic(iy - 1, height);
        niy = wrap_periodic(iy + 1, height);
        nniy = wrap_periodic(iy + 2, height);
        break;
      case EXTENSION_CLIP:
        /* No cubic samples are inside the clip region. */
        if (ix < -2 || ix > width || iy < -2 || iy > height) {
          return zero();
        }

        pix = ix - 1;
        nix = ix + 1;
        nnix = ix + 2;

        piy = iy - 1;
        niy = iy + 1;
        nniy = iy + 2;
        break;
      case EXTENSION_EXTEND:
        pix = wrap_clamp(ix - 1, width);
        nix = wrap_clamp(ix + 1, width);
        nnix = wrap_clamp(ix + 2, width);
        ix = wrap_clamp(ix, width);

        piy = wrap_clamp(iy - 1, height);
        niy = wrap_clamp(iy + 1, height);
        nniy = wrap_clamp(iy + 2, height);
        iy = wrap_clamp(iy, height);
        break;
      case EXTENSION_MIRROR:
        pix = wrap_mirror(ix - 1, width);
        nix = wrap_mirror(ix + 1, width);
        nnix = wrap_mirror(ix + 2, width);
        ix = wrap_mirror(ix, width);

        piy = wrap_mirror(iy - 1, height);
        niy = wrap_mirror(iy + 1, height);
        nniy = wrap_mirror(iy + 2, height);
        iy = wrap_mirror(iy, height);
        break;
      default:
        kernel_assert(0);
        return zero();
    }

    const TexT *data = (const TexT *)info.data;
    const int xc[4] = {pix, ix, nix, nnix};
    const int yc[4] = {piy, iy, niy, nniy};
    float u[4], v[4];

    /* Some helper macros to keep code size reasonable.
     * Lets the compiler inline all the matrix multiplications.
     */
#define DATA(x, y) (read_clip(data, xc[x], yc[y], width, height))
#define TERM(col) \
  (v[col] * \
   (u[0] * DATA(0, col) + u[1] * DATA(1, col) + u[2] * DATA(2, col) + u[3] * DATA(3, col)))

    SET_CUBIC_SPLINE_WEIGHTS(u, tx);
    SET_CUBIC_SPLINE_WEIGHTS(v, ty);

    /* Actual interpolation. */
    return TERM(0) + TERM(1) + TERM(2) + TERM(3);
#undef TERM
#undef DATA
  }

  static ccl_always_inline OutT interp(const KernelImageInfo &info, const float x, float y)
  {
    switch (info.interpolation) {
      case INTERPOLATION_CLOSEST:
        return interp_closest(info, x, y);
      case INTERPOLATION_LINEAR:
        return interp_linear(info, x, y);
      default:
        return interp_cubic(info, x, y);
    }
  }
};

#undef SET_CUBIC_SPLINE_WEIGHTS

ccl_device float4 kernel_image_interp(KernelGlobals kg,
                                      const int image_texture_id,
                                      const float x,
                                      float y)
{
  if (image_texture_id == KERNEL_IMAGE_NONE) {
    return IMAGE_MISSING_RGBA;
  }
  const ccl_global KernelImageTexture &tex = kernel_data_fetch(image_textures, image_texture_id);
  if (tex.image_info_id == KERNEL_IMAGE_NONE) {
    return IMAGE_MISSING_RGBA;
  }
  const KernelImageInfo &info = kernel_data_fetch(image_info, tex.image_info_id);

  if (UNLIKELY(!info.data)) {
    return zero_float4();
  }

  switch (info.data_type) {
    case IMAGE_DATA_TYPE_HALF: {
      const float f = ImageInterpolator<half, float>::interp(info, x, y);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_BYTE: {
      const float f = ImageInterpolator<uchar, float>::interp(info, x, y);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_USHORT: {
      const float f = ImageInterpolator<uint16_t, float>::interp(info, x, y);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_FLOAT: {
      const float f = ImageInterpolator<float, float>::interp(info, x, y);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_HALF4:
      return ImageInterpolator<half4>::interp(info, x, y);
    case IMAGE_DATA_TYPE_BYTE4:
      return ImageInterpolator<uchar4>::interp(info, x, y);
    case IMAGE_DATA_TYPE_USHORT4:
      return ImageInterpolator<ushort4>::interp(info, x, y);
    case IMAGE_DATA_TYPE_FLOAT4:
      return ImageInterpolator<float4>::interp(info, x, y);
    default:
      assert(0);
      return IMAGE_MISSING_RGBA;
  }
}

} /* Namespace. */

CCL_NAMESPACE_END
