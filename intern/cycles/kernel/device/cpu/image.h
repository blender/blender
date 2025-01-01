/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#ifdef WITH_NANOVDB
#  include "kernel/util/nanovdb.h"
#endif

#include "util/half.h"

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

template<typename TexT, typename OutT = float4> struct TextureInterpolator {

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
  read(const TexT *data, const int x, int y, const int width, const int height)
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

  /* Read 3D Texture Data
   * Does not check if data request is in bounds. */
  static ccl_always_inline OutT read(const TexT *data,
                                     const int x,
                                     int y,
                                     const int z,
                                     int width,
                                     const int height,
                                     const int depth)
  {
    return read(data[x + y * width + z * width * height]);
  }

  /* Read 3D Texture Data Clip
   * Returns transparent black if data request is out of bounds. */
  static ccl_always_inline OutT read_clip(const TexT *data,
                                          const int x,
                                          int y,
                                          const int z,
                                          int width,
                                          const int height,
                                          const int depth)
  {
    if (x < 0 || x >= width || y < 0 || y >= height || z < 0 || z >= depth) {
      return zero();
    }
    return read(data[x + y * width + z * width * height]);
  }

  /* Trilinear Interpolation */
  static ccl_always_inline OutT
  trilinear_lookup(const TexT *data,
                   const float tx,
                   const float ty,
                   const float tz,
                   const int ix,
                   const int iy,
                   const int iz,
                   const int nix,
                   const int niy,
                   const int niz,
                   const int width,
                   const int height,
                   const int depth,
                   OutT read(const TexT *, int, int, int, int, int, int))
  {
    OutT r = (1.0f - tz) * (1.0f - ty) * (1.0f - tx) *
             read(data, ix, iy, iz, width, height, depth);
    r += (1.0f - tz) * (1.0f - ty) * tx * read(data, nix, iy, iz, width, height, depth);
    r += (1.0f - tz) * ty * (1.0f - tx) * read(data, ix, niy, iz, width, height, depth);
    r += (1.0f - tz) * ty * tx * read(data, nix, niy, iz, width, height, depth);

    r += tz * (1.0f - ty) * (1.0f - tx) * read(data, ix, iy, niz, width, height, depth);
    r += tz * (1.0f - ty) * tx * read(data, nix, iy, niz, width, height, depth);
    r += tz * ty * (1.0f - tx) * read(data, ix, niy, niz, width, height, depth);
    r += tz * ty * tx * read(data, nix, niy, niz, width, height, depth);
    return r;
  }

  /** Tricubic Interpolation */
  static ccl_always_inline OutT
  tricubic_lookup(const TexT *data,
                  const float tx,
                  const float ty,
                  const float tz,
                  const int xc[4],
                  const int yc[4],
                  const int zc[4],
                  const int width,
                  const int height,
                  const int depth,
                  OutT read(const TexT *, int, int, int, int, int, int))
  {
    float u[4], v[4], w[4];

    /* Some helper macros to keep code size reasonable.
     * Lets the compiler inline all the matrix multiplications.
     */
#define DATA(x, y, z) (read(data, xc[x], yc[y], zc[z], width, height, depth))
#define COL_TERM(col, row) \
  (v[col] * (u[0] * DATA(0, col, row) + u[1] * DATA(1, col, row) + u[2] * DATA(2, col, row) + \
             u[3] * DATA(3, col, row)))
#define ROW_TERM(row) \
  (w[row] * (COL_TERM(0, row) + COL_TERM(1, row) + COL_TERM(2, row) + COL_TERM(3, row)))

    SET_CUBIC_SPLINE_WEIGHTS(u, tx);
    SET_CUBIC_SPLINE_WEIGHTS(v, ty);
    SET_CUBIC_SPLINE_WEIGHTS(w, tz);
    /* Actual interpolation. */
    return ROW_TERM(0) + ROW_TERM(1) + ROW_TERM(2) + ROW_TERM(3);

#undef COL_TERM
#undef ROW_TERM
#undef DATA
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

  static ccl_always_inline OutT interp_closest(const TextureInfo &info, const float x, float y)
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

  static ccl_always_inline OutT interp_linear(const TextureInfo &info, const float x, float y)
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

  static ccl_always_inline OutT interp_cubic(const TextureInfo &info, const float x, float y)
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

  static ccl_always_inline OutT interp(const TextureInfo &info, const float x, float y)
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

  /* ********  3D interpolation ******** */

  static ccl_always_inline OutT interp_3d_closest(const TextureInfo &info,
                                                  const float x,
                                                  const float y,
                                                  const float z)
  {
    const int width = info.width;
    const int height = info.height;
    const int depth = info.depth;
    int ix, iy, iz;

    frac(x * (float)width, &ix);
    frac(y * (float)height, &iy);
    frac(z * (float)depth, &iz);

    switch (info.extension) {
      case EXTENSION_REPEAT:
        ix = wrap_periodic(ix, width);
        iy = wrap_periodic(iy, height);
        iz = wrap_periodic(iz, depth);
        break;
      case EXTENSION_CLIP:
        /* No samples are inside the clip region. */
        if (ix < 0 || ix >= width || iy < 0 || iy >= height || iz < 0 || iz >= depth) {
          return zero();
        }
        break;
      case EXTENSION_EXTEND:
        ix = wrap_clamp(ix, width);
        iy = wrap_clamp(iy, height);
        iz = wrap_clamp(iz, depth);
        break;
      case EXTENSION_MIRROR:
        ix = wrap_mirror(ix, width);
        iy = wrap_mirror(iy, height);
        iz = wrap_mirror(iz, depth);
        break;
      default:
        kernel_assert(0);
        return zero();
    }

    const TexT *data = (const TexT *)info.data;
    return read(data, ix, iy, iz, width, height, depth);
  }

  static ccl_always_inline OutT interp_3d_linear(const TextureInfo &info,
                                                 const float x,
                                                 const float y,
                                                 const float z)
  {
    const int width = info.width;
    const int height = info.height;
    const int depth = info.depth;
    int ix, iy, iz;
    int nix, niy, niz;

    /* A -0.5 offset is used to center the linear samples around the sample point. */
    float tx = frac(x * (float)width - 0.5f, &ix);
    float ty = frac(y * (float)height - 0.5f, &iy);
    float tz = frac(z * (float)depth - 0.5f, &iz);

    switch (info.extension) {
      case EXTENSION_REPEAT:
        ix = wrap_periodic(ix, width);
        nix = wrap_periodic(ix + 1, width);

        iy = wrap_periodic(iy, height);
        niy = wrap_periodic(iy + 1, height);

        iz = wrap_periodic(iz, depth);
        niz = wrap_periodic(iz + 1, depth);
        break;
      case EXTENSION_CLIP:
        /* No linear samples are inside the clip region. */
        if (ix < -1 || ix >= width || iy < -1 || iy >= height || iz < -1 || iz >= depth) {
          return zero();
        }

        nix = ix + 1;
        niy = iy + 1;
        niz = iz + 1;

        /* All linear samples are inside the clip region. */
        if (ix >= 0 && nix < width && iy >= 0 && niy < height && iz >= 0 && niz < depth) {
          break;
        }

        /* The linear samples span the clip border.
         * #read_clip is used to ensure proper interpolation across the clip border. */
        return trilinear_lookup((const TexT *)info.data,
                                tx,
                                ty,
                                tz,
                                ix,
                                iy,
                                iz,
                                nix,
                                niy,
                                niz,
                                width,
                                height,
                                depth,
                                read_clip);
      case EXTENSION_EXTEND:
        nix = wrap_clamp(ix + 1, width);
        ix = wrap_clamp(ix, width);

        niy = wrap_clamp(iy + 1, height);
        iy = wrap_clamp(iy, height);

        niz = wrap_clamp(iz + 1, depth);
        iz = wrap_clamp(iz, depth);
        break;
      case EXTENSION_MIRROR:
        nix = wrap_mirror(ix + 1, width);
        ix = wrap_mirror(ix, width);

        niy = wrap_mirror(iy + 1, height);
        iy = wrap_mirror(iy, height);

        niz = wrap_mirror(iz + 1, depth);
        iz = wrap_mirror(iz, depth);
        break;
      default:
        kernel_assert(0);
        return zero();
    }

    return trilinear_lookup((const TexT *)info.data,
                            tx,
                            ty,
                            tz,
                            ix,
                            iy,
                            iz,
                            nix,
                            niy,
                            niz,
                            width,
                            height,
                            depth,
                            read);
  }

  /* Tricubic b-spline interpolation.
   *
   * TODO(sergey): For some unspeakable reason both GCC-6 and Clang-3.9 are
   * causing stack overflow issue in this function unless it is inlined.
   *
   * Only happens for AVX2 kernel and global __KERNEL_SSE__ vectorization
   * enabled.
   */
#if defined(__GNUC__) || defined(__clang__)
  static ccl_always_inline
#else
  static ccl_never_inline
#endif
      OutT
      interp_3d_cubic(const TextureInfo &info, const float x, float y, const float z)
  {
    int width = info.width;
    int height = info.height;
    int depth = info.depth;
    int ix, iy, iz;

    /* A -0.5 offset is used to center the cubic samples around the sample point. */
    const float tx = frac(x * (float)width - 0.5f, &ix);
    const float ty = frac(y * (float)height - 0.5f, &iy);
    const float tz = frac(z * (float)depth - 0.5f, &iz);

    int pix, piy, piz;
    int nix, niy, niz;
    int nnix, nniy, nniz;

    switch (info.extension) {
      case EXTENSION_REPEAT:
        ix = wrap_periodic(ix, width);
        pix = wrap_periodic(ix - 1, width);
        nix = wrap_periodic(ix + 1, width);
        nnix = wrap_periodic(ix + 2, width);

        iy = wrap_periodic(iy, height);
        niy = wrap_periodic(iy + 1, height);
        piy = wrap_periodic(iy - 1, height);
        nniy = wrap_periodic(iy + 2, height);

        iz = wrap_periodic(iz, depth);
        piz = wrap_periodic(iz - 1, depth);
        niz = wrap_periodic(iz + 1, depth);
        nniz = wrap_periodic(iz + 2, depth);
        break;
      case EXTENSION_CLIP: {
        /* No cubic samples are inside the clip region. */
        if (ix < -2 || ix > width || iy < -2 || iy > height || iz < -2 || iz > depth) {
          return zero();
        }

        pix = ix - 1;
        nnix = ix + 2;
        nix = ix + 1;

        piy = iy - 1;
        niy = iy + 1;
        nniy = iy + 2;

        piz = iz - 1;
        niz = iz + 1;
        nniz = iz + 2;

        /* All cubic samples are inside the clip region. */
        if (pix >= 0 && nnix < width && piy >= 0 && nniy < height && piz >= 0 && nniz < depth) {
          break;
        }

        /* The Cubic samples span the clip border.
         * read_clip is used to ensure proper interpolation across the clip border. */
        const int xc[4] = {pix, ix, nix, nnix};
        const int yc[4] = {piy, iy, niy, nniy};
        const int zc[4] = {piz, iz, niz, nniz};
        return tricubic_lookup(
            (const TexT *)info.data, tx, ty, tz, xc, yc, zc, width, height, depth, read_clip);
      }
      case EXTENSION_EXTEND:
        pix = wrap_clamp(ix - 1, width);
        nix = wrap_clamp(ix + 1, width);
        nnix = wrap_clamp(ix + 2, width);
        ix = wrap_clamp(ix, width);

        piy = wrap_clamp(iy - 1, height);
        niy = wrap_clamp(iy + 1, height);
        nniy = wrap_clamp(iy + 2, height);
        iy = wrap_clamp(iy, height);

        piz = wrap_clamp(iz - 1, depth);
        niz = wrap_clamp(iz + 1, depth);
        nniz = wrap_clamp(iz + 2, depth);
        iz = wrap_clamp(iz, depth);
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

        piz = wrap_mirror(iz - 1, depth);
        niz = wrap_mirror(iz + 1, depth);
        nniz = wrap_mirror(iz + 2, depth);
        iz = wrap_mirror(iz, depth);
        break;
      default:
        kernel_assert(0);
        return zero();
    }
    const int xc[4] = {pix, ix, nix, nnix};
    const int yc[4] = {piy, iy, niy, nniy};
    const int zc[4] = {piz, iz, niz, nniz};
    const TexT *data = (const TexT *)info.data;
    return tricubic_lookup(data, tx, ty, tz, xc, yc, zc, width, height, depth, read);
  }

  static ccl_always_inline OutT interp_3d(
      const TextureInfo &info, const float x, float y, const float z, InterpolationType interp)
  {
    switch ((interp == INTERPOLATION_NONE) ? info.interpolation : interp) {
      case INTERPOLATION_CLOSEST:
        return interp_3d_closest(info, x, y, z);
      case INTERPOLATION_LINEAR:
        return interp_3d_linear(info, x, y, z);
      default:
        return interp_3d_cubic(info, x, y, z);
    }
  }
};

#ifdef WITH_NANOVDB
template<typename TexT, typename OutT> struct NanoVDBInterpolator {

  static ccl_always_inline float read(const float r)
  {
    return r;
  }

  static ccl_always_inline float4 read(const packed_float3 r)
  {
    return make_float4(r.x, r.y, r.z, 1.0f);
  }

  template<typename Acc>
  static ccl_always_inline OutT
  interp_3d_closest(const Acc &acc, const float x, float y, const float z)
  {
    const nanovdb::Coord coord((int32_t)floorf(x), (int32_t)floorf(y), (int32_t)floorf(z));
    return read(acc.getValue(coord));
  }

  template<typename Acc>
  static ccl_always_inline OutT
  interp_3d_linear(const Acc &acc, const float x, float y, const float z)
  {
    int ix, iy, iz;
    const float tx = frac(x - 0.5f, &ix);
    const float ty = frac(y - 0.5f, &iy);
    const float tz = frac(z - 0.5f, &iz);

    return mix(mix(mix(read(acc.getValue(nanovdb::Coord(ix, iy, iz))),
                       read(acc.getValue(nanovdb::Coord(ix, iy, iz + 1))),
                       tz),
                   mix(read(acc.getValue(nanovdb::Coord(ix, iy + 1, iz + 1))),
                       read(acc.getValue(nanovdb::Coord(ix, iy + 1, iz))),
                       1.0f - tz),
                   ty),
               mix(mix(read(acc.getValue(nanovdb::Coord(ix + 1, iy + 1, iz))),
                       read(acc.getValue(nanovdb::Coord(ix + 1, iy + 1, iz + 1))),
                       tz),
                   mix(read(acc.getValue(nanovdb::Coord(ix + 1, iy, iz + 1))),
                       read(acc.getValue(nanovdb::Coord(ix + 1, iy, iz))),
                       1.0f - tz),
                   1.0f - ty),
               tx);
  }

  /* Tricubic b-spline interpolation. */
  template<typename Acc>
#  if defined(__GNUC__) || defined(__clang__)
  static ccl_always_inline
#  else
  static ccl_never_inline
#  endif
      OutT
      interp_3d_cubic(const Acc &acc, const float x, float y, const float z)
  {
    int ix, iy, iz;
    int nix, niy, niz;
    int pix, piy, piz;
    int nnix, nniy, nniz;

    /* A -0.5 offset is used to center the cubic samples around the sample point. */
    const float tx = frac(x - 0.5f, &ix);
    const float ty = frac(y - 0.5f, &iy);
    const float tz = frac(z - 0.5f, &iz);

    pix = ix - 1;
    piy = iy - 1;
    piz = iz - 1;
    nix = ix + 1;
    niy = iy + 1;
    niz = iz + 1;
    nnix = ix + 2;
    nniy = iy + 2;
    nniz = iz + 2;

    const int xc[4] = {pix, ix, nix, nnix};
    const int yc[4] = {piy, iy, niy, nniy};
    const int zc[4] = {piz, iz, niz, nniz};
    float u[4], v[4], w[4];

    /* Some helper macros to keep code size reasonable.
     * Lets the compiler inline all the matrix multiplications.
     */
#  define DATA(x, y, z) (read(acc.getValue(nanovdb::Coord(xc[x], yc[y], zc[z]))))
#  define COL_TERM(col, row) \
    (v[col] * (u[0] * DATA(0, col, row) + u[1] * DATA(1, col, row) + u[2] * DATA(2, col, row) + \
               u[3] * DATA(3, col, row)))
#  define ROW_TERM(row) \
    (w[row] * (COL_TERM(0, row) + COL_TERM(1, row) + COL_TERM(2, row) + COL_TERM(3, row)))

    SET_CUBIC_SPLINE_WEIGHTS(u, tx);
    SET_CUBIC_SPLINE_WEIGHTS(v, ty);
    SET_CUBIC_SPLINE_WEIGHTS(w, tz);

    /* Actual interpolation. */
    return ROW_TERM(0) + ROW_TERM(1) + ROW_TERM(2) + ROW_TERM(3);

#  undef COL_TERM
#  undef ROW_TERM
#  undef DATA
  }

  static ccl_always_inline OutT interp_3d(
      const TextureInfo &info, const float x, float y, const float z, InterpolationType interp)
  {
    using namespace nanovdb;

    NanoGrid<TexT> *const grid = (NanoGrid<TexT> *)info.data;

    switch ((interp == INTERPOLATION_NONE) ? info.interpolation : interp) {
      case INTERPOLATION_CLOSEST: {
        ReadAccessor<TexT> acc(grid->tree().root());
        return interp_3d_closest(acc, x, y, z);
      }
      case INTERPOLATION_LINEAR: {
        CachedReadAccessor<TexT> acc(grid->tree().root());
        return interp_3d_linear(acc, x, y, z);
      }
      default: {
        CachedReadAccessor<TexT> acc(grid->tree().root());
        return interp_3d_cubic(acc, x, y, z);
      }
    }
  }
};
#endif

#undef SET_CUBIC_SPLINE_WEIGHTS

ccl_device float4 kernel_tex_image_interp(KernelGlobals kg, const int id, const float x, float y)
{
  const TextureInfo &info = kernel_data_fetch(texture_info, id);

  if (UNLIKELY(!info.data)) {
    return zero_float4();
  }

  switch (info.data_type) {
    case IMAGE_DATA_TYPE_HALF: {
      const float f = TextureInterpolator<half, float>::interp(info, x, y);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_BYTE: {
      const float f = TextureInterpolator<uchar, float>::interp(info, x, y);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_USHORT: {
      const float f = TextureInterpolator<uint16_t, float>::interp(info, x, y);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_FLOAT: {
      const float f = TextureInterpolator<float, float>::interp(info, x, y);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_HALF4:
      return TextureInterpolator<half4>::interp(info, x, y);
    case IMAGE_DATA_TYPE_BYTE4:
      return TextureInterpolator<uchar4>::interp(info, x, y);
    case IMAGE_DATA_TYPE_USHORT4:
      return TextureInterpolator<ushort4>::interp(info, x, y);
    case IMAGE_DATA_TYPE_FLOAT4:
      return TextureInterpolator<float4>::interp(info, x, y);
    default:
      assert(0);
      return make_float4(
          TEX_IMAGE_MISSING_R, TEX_IMAGE_MISSING_G, TEX_IMAGE_MISSING_B, TEX_IMAGE_MISSING_A);
  }
}

ccl_device float4 kernel_tex_image_interp_3d(KernelGlobals kg,
                                             const int id,
                                             float3 P,
                                             InterpolationType interp)
{
  const TextureInfo &info = kernel_data_fetch(texture_info, id);

  if (UNLIKELY(!info.data)) {
    return zero_float4();
  }

  if (info.use_transform_3d) {
    P = transform_point(&info.transform_3d, P);
  }
  switch (info.data_type) {
    case IMAGE_DATA_TYPE_HALF: {
      const float f = TextureInterpolator<half, float>::interp_3d(info, P.x, P.y, P.z, interp);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_BYTE: {
      const float f = TextureInterpolator<uchar, float>::interp_3d(info, P.x, P.y, P.z, interp);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_USHORT: {
      const float f = TextureInterpolator<uint16_t, float>::interp_3d(info, P.x, P.y, P.z, interp);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_FLOAT: {
      const float f = TextureInterpolator<float, float>::interp_3d(info, P.x, P.y, P.z, interp);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_HALF4:
      return TextureInterpolator<half4>::interp_3d(info, P.x, P.y, P.z, interp);
    case IMAGE_DATA_TYPE_BYTE4:
      return TextureInterpolator<uchar4>::interp_3d(info, P.x, P.y, P.z, interp);
    case IMAGE_DATA_TYPE_USHORT4:
      return TextureInterpolator<ushort4>::interp_3d(info, P.x, P.y, P.z, interp);
    case IMAGE_DATA_TYPE_FLOAT4:
      return TextureInterpolator<float4>::interp_3d(info, P.x, P.y, P.z, interp);
#ifdef WITH_NANOVDB
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT: {
      const float f = NanoVDBInterpolator<float, float>::interp_3d(info, P.x, P.y, P.z, interp);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT3:
      return NanoVDBInterpolator<packed_float3, float4>::interp_3d(info, P.x, P.y, P.z, interp);
    case IMAGE_DATA_TYPE_NANOVDB_FPN: {
      const float f = NanoVDBInterpolator<nanovdb::FpN, float>::interp_3d(
          info, P.x, P.y, P.z, interp);
      return make_float4(f, f, f, 1.0f);
    }
    case IMAGE_DATA_TYPE_NANOVDB_FP16: {
      const float f = NanoVDBInterpolator<nanovdb::Fp16, float>::interp_3d(
          info, P.x, P.y, P.z, interp);
      return make_float4(f, f, f, 1.0f);
    }
#endif
    default:
      assert(0);
      return make_float4(
          TEX_IMAGE_MISSING_R, TEX_IMAGE_MISSING_G, TEX_IMAGE_MISSING_B, TEX_IMAGE_MISSING_A);
  }
}

} /* Namespace. */

CCL_NAMESPACE_END
