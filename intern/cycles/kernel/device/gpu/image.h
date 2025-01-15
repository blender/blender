/* SPDX-FileCopyrightText: 2017-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

#if !defined __KERNEL_METAL__
#  ifdef WITH_NANOVDB
#    include "kernel/util/nanovdb.h"
#  endif
#endif

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

/* Fast tricubic texture lookup using 8 trilinear lookups. */
template<typename T>
ccl_device_noinline T
kernel_tex_image_interp_tricubic(const ccl_global TextureInfo &info, float x, float y, float z)
{
  ccl_gpu_tex_object_3D tex = (ccl_gpu_tex_object_3D)info.data;

  x = (x * info.width) - 0.5f;
  y = (y * info.height) - 0.5f;
  z = (z * info.depth) - 0.5f;

  float px = floorf(x);
  float py = floorf(y);
  float pz = floorf(z);
  float fx = x - px;
  float fy = y - py;
  float fz = z - pz;

  float g0x = cubic_g0(fx);
  float g1x = cubic_g1(fx);
  float g0y = cubic_g0(fy);
  float g1y = cubic_g1(fy);
  float g0z = cubic_g0(fz);
  float g1z = cubic_g1(fz);

  /* Note +0.5 offset to compensate for CUDA linear filtering convention. */
  float x0 = (px + cubic_h0(fx) + 0.5f) / info.width;
  float x1 = (px + cubic_h1(fx) + 0.5f) / info.width;
  float y0 = (py + cubic_h0(fy) + 0.5f) / info.height;
  float y1 = (py + cubic_h1(fy) + 0.5f) / info.height;
  float z0 = (pz + cubic_h0(fz) + 0.5f) / info.depth;
  float z1 = (pz + cubic_h1(fz) + 0.5f) / info.depth;

  return g0z * (g0y * (g0x * ccl_gpu_tex_object_read_3D<T>(tex, x0, y0, z0) +
                       g1x * ccl_gpu_tex_object_read_3D<T>(tex, x1, y0, z0)) +
                g1y * (g0x * ccl_gpu_tex_object_read_3D<T>(tex, x0, y1, z0) +
                       g1x * ccl_gpu_tex_object_read_3D<T>(tex, x1, y1, z0))) +
         g1z * (g0y * (g0x * ccl_gpu_tex_object_read_3D<T>(tex, x0, y0, z1) +
                       g1x * ccl_gpu_tex_object_read_3D<T>(tex, x1, y0, z1)) +
                g1y * (g0x * ccl_gpu_tex_object_read_3D<T>(tex, x0, y1, z1) +
                       g1x * ccl_gpu_tex_object_read_3D<T>(tex, x1, y1, z1)));
}

#ifdef WITH_NANOVDB
template<typename OutT, typename Acc>
ccl_device OutT kernel_tex_image_interp_trilinear_nanovdb(ccl_private Acc &acc,
                                                          const float x,
                                                          float y,
                                                          const float z)
{
  int ix, iy, iz;
  const float tx = frac(x - 0.5f, &ix);
  const float ty = frac(y - 0.5f, &iy);
  const float tz = frac(z - 0.5f, &iz);

  return mix(mix(mix(OutT(acc.getValue(nanovdb::Coord(ix, iy, iz))),
                     OutT(acc.getValue(nanovdb::Coord(ix, iy, iz + 1))),
                     tz),
                 mix(OutT(acc.getValue(nanovdb::Coord(ix, iy + 1, iz + 1))),
                     OutT(acc.getValue(nanovdb::Coord(ix, iy + 1, iz))),
                     1.0f - tz),
                 ty),
             mix(mix(OutT(acc.getValue(nanovdb::Coord(ix + 1, iy + 1, iz))),
                     OutT(acc.getValue(nanovdb::Coord(ix + 1, iy + 1, iz + 1))),
                     tz),
                 mix(OutT(acc.getValue(nanovdb::Coord(ix + 1, iy, iz + 1))),
                     OutT(acc.getValue(nanovdb::Coord(ix + 1, iy, iz))),
                     1.0f - tz),
                 1.0f - ty),
             tx);
}

template<typename OutT, typename Acc>
ccl_device OutT kernel_tex_image_interp_tricubic_nanovdb(ccl_private Acc &acc,
                                                         const float x,
                                                         const float y,
                                                         const float z)
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
#  define SET_CUBIC_SPLINE_WEIGHTS(u, t) \
    { \
      u[0] = (((-1.0f / 6.0f) * t + 0.5f) * t - 0.5f) * t + (1.0f / 6.0f); \
      u[1] = ((0.5f * t - 1.0f) * t) * t + (2.0f / 3.0f); \
      u[2] = ((-0.5f * t + 0.5f) * t + 0.5f) * t + (1.0f / 6.0f); \
      u[3] = (1.0f / 6.0f) * t * t * t; \
    } \
    (void)0

#  define DATA(x, y, z) (OutT(acc.getValue(nanovdb::Coord(xc[x], yc[y], zc[z]))))
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
#  undef SET_CUBIC_SPLINE_WEIGHTS
}

#  if defined(__KERNEL_METAL__)
template<typename OutT, typename T>
__attribute__((noinline)) OutT kernel_tex_image_interp_nanovdb(const ccl_global TextureInfo &info,
                                                               const float x,
                                                               const float y,
                                                               const float z,
                                                               const uint interpolation)
#  else
template<typename OutT, typename T>
ccl_device_noinline OutT kernel_tex_image_interp_nanovdb(const ccl_global TextureInfo &info,
                                                         const float x,
                                                         const float y,
                                                         const float z,
                                                         const uint interpolation)
#  endif
{
  using namespace nanovdb;

  ccl_global NanoGrid<T> *const grid = (ccl_global NanoGrid<T> *)info.data;

  switch (interpolation) {
    case INTERPOLATION_CLOSEST: {
      ReadAccessor<T> acc(grid->tree().root());
      const nanovdb::Coord coord((int32_t)floorf(x), (int32_t)floorf(y), (int32_t)floorf(z));
      return OutT(acc.getValue(coord));
    }
    case INTERPOLATION_LINEAR: {
      CachedReadAccessor<T> acc(grid->tree().root());
      return kernel_tex_image_interp_trilinear_nanovdb<OutT>(acc, x, y, z);
    }
    default: {
      CachedReadAccessor<T> acc(grid->tree().root());
      return kernel_tex_image_interp_tricubic_nanovdb<OutT>(acc, x, y, z);
    }
  }
}
#endif

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

ccl_device float4 kernel_tex_image_interp_3d(KernelGlobals kg,
                                             const int id,
                                             float3 P,
                                             InterpolationType interp)
{
  const ccl_global TextureInfo &info = kernel_data_fetch(texture_info, id);

  if (info.use_transform_3d) {
    P = transform_point(&info.transform_3d, P);
  }

  const float x = P.x;
  const float y = P.y;
  const float z = P.z;

  uint interpolation = (interp == INTERPOLATION_NONE) ? info.interpolation : interp;
  const int texture_type = info.data_type;

#ifdef WITH_NANOVDB
  if (texture_type == IMAGE_DATA_TYPE_NANOVDB_FLOAT) {
    float f = kernel_tex_image_interp_nanovdb<float, float>(info, x, y, z, interpolation);
    return make_float4(f, f, f, 1.0f);
  }
  if (texture_type == IMAGE_DATA_TYPE_NANOVDB_FLOAT3) {
    float3 f = kernel_tex_image_interp_nanovdb<float3, packed_float3>(
        info, x, y, z, interpolation);
    return make_float4(f, 1.0f);
  }
  if (texture_type == IMAGE_DATA_TYPE_NANOVDB_FPN) {
    float f = kernel_tex_image_interp_nanovdb<float, nanovdb::FpN>(info, x, y, z, interpolation);
    return make_float4(f, f, f, 1.0f);
  }
  if (texture_type == IMAGE_DATA_TYPE_NANOVDB_FP16) {
    float f = kernel_tex_image_interp_nanovdb<float, nanovdb::Fp16>(info, x, y, z, interpolation);
    return make_float4(f, f, f, 1.0f);
  }
#endif
  if (texture_type == IMAGE_DATA_TYPE_FLOAT4 || texture_type == IMAGE_DATA_TYPE_BYTE4 ||
      texture_type == IMAGE_DATA_TYPE_HALF4 || texture_type == IMAGE_DATA_TYPE_USHORT4)
  {
    if (interpolation == INTERPOLATION_CUBIC || interpolation == INTERPOLATION_SMART) {
      return kernel_tex_image_interp_tricubic<float4>(info, x, y, z);
    }
    else {
      ccl_gpu_tex_object_3D tex = (ccl_gpu_tex_object_3D)info.data;
      return ccl_gpu_tex_object_read_3D<float4>(tex, x, y, z);
    }
  }
  else {
    float f;

    if (interpolation == INTERPOLATION_CUBIC || interpolation == INTERPOLATION_SMART) {
      f = kernel_tex_image_interp_tricubic<float>(info, x, y, z);
    }
    else {
      ccl_gpu_tex_object_3D tex = (ccl_gpu_tex_object_3D)info.data;
      f = ccl_gpu_tex_object_read_3D<float>(tex, x, y, z);
    }

    return make_float4(f, f, f, 1.0f);
  }
}

CCL_NAMESPACE_END
