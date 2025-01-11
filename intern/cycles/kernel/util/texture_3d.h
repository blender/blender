/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#if !defined(__KERNEL_METAL__) && !defined(__KERNEL_ONEAPI__)
#  ifdef WITH_NANOVDB
#    include "kernel/util/nanovdb.h"
#  endif
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
/* Make template functions private so symbols don't conflict between kernels with different
 * instruction sets. */
namespace {
#endif

ccl_device_inline float interp_frac(const float x, ccl_private int *ix)
{
  int i = float_to_int(x) - ((x < 0.0f) ? 1 : 0);
  *ix = i;
  return x - (float)i;
}

#ifdef WITH_NANOVDB
template<typename OutT, typename Acc>
ccl_device OutT kernel_tex_image_interp_trilinear_nanovdb(ccl_private Acc &acc, const float3 P)
{
  int ix, iy, iz;
  const float tx = interp_frac(P.x - 0.5f, &ix);
  const float ty = interp_frac(P.y - 0.5f, &iy);
  const float tz = interp_frac(P.z - 0.5f, &iz);

  return mix(mix(mix(OutT(acc.getValue(make_int3(ix, iy, iz))),
                     OutT(acc.getValue(make_int3(ix, iy, iz + 1))),
                     tz),
                 mix(OutT(acc.getValue(make_int3(ix, iy + 1, iz + 1))),
                     OutT(acc.getValue(make_int3(ix, iy + 1, iz))),
                     1.0f - tz),
                 ty),
             mix(mix(OutT(acc.getValue(make_int3(ix + 1, iy + 1, iz))),
                     OutT(acc.getValue(make_int3(ix + 1, iy + 1, iz + 1))),
                     tz),
                 mix(OutT(acc.getValue(make_int3(ix + 1, iy, iz + 1))),
                     OutT(acc.getValue(make_int3(ix + 1, iy, iz))),
                     1.0f - tz),
                 1.0f - ty),
             tx);
}

template<typename OutT, typename Acc>
ccl_device OutT kernel_tex_image_interp_tricubic_nanovdb(ccl_private Acc &acc, const float3 P)
{
  int ix, iy, iz;
  int nix, niy, niz;
  int pix, piy, piz;
  int nnix, nniy, nniz;

  /* A -0.5 offset is used to center the cubic samples around the sample point. */
  const float tx = interp_frac(P.x - 0.5f, &ix);
  const float ty = interp_frac(P.y - 0.5f, &iy);
  const float tz = interp_frac(P.z - 0.5f, &iz);

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

#  define DATA(x, y, z) (OutT(acc.getValue(make_int3(xc[x], yc[y], zc[z]))))
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
                                                               const float3 P,
                                                               const InterpolationType interp)
#  else
template<typename OutT, typename T>
ccl_device_noinline OutT kernel_tex_image_interp_nanovdb(const ccl_global TextureInfo &info,
                                                         const float3 P,
                                                         const InterpolationType interp)
#  endif
{
  using namespace nanovdb;

  ccl_global NanoGrid<T> *const grid = (ccl_global NanoGrid<T> *)info.data;

  switch (interp) {
    case INTERPOLATION_CLOSEST: {
      ReadAccessor<T> acc(grid->tree().root());
      return OutT(acc.getValue(make_int3(floor(P))));
    }
    case INTERPOLATION_LINEAR: {
      CachedReadAccessor<T> acc(grid->tree().root());
      return kernel_tex_image_interp_trilinear_nanovdb<OutT>(acc, P);
    }
    default: {
      CachedReadAccessor<T> acc(grid->tree().root());
      return kernel_tex_image_interp_tricubic_nanovdb<OutT>(acc, P);
    }
  }
}
#endif /* WITH_NANOVDB */

ccl_device float4 kernel_tex_image_interp_3d(KernelGlobals kg,
                                             const int id,
                                             float3 P,
                                             InterpolationType interp)
{
  const ccl_global TextureInfo &info = kernel_data_fetch(texture_info, id);

  if (info.use_transform_3d) {
    P = transform_point(&info.transform_3d, P);
  }

  const InterpolationType interpolation = (interp == INTERPOLATION_NONE) ?
                                              (InterpolationType)info.interpolation :
                                              interp;
  const ImageDataType data_type = (ImageDataType)info.data_type;

#ifdef WITH_NANOVDB
  if (data_type == IMAGE_DATA_TYPE_NANOVDB_FLOAT) {
    const float f = kernel_tex_image_interp_nanovdb<float, float>(info, P, interpolation);
    return make_float4(f, f, f, 1.0f);
  }
  if (data_type == IMAGE_DATA_TYPE_NANOVDB_FLOAT3) {
    const float3 f = kernel_tex_image_interp_nanovdb<float3, packed_float3>(
        info, P, interpolation);
    return make_float4(f, 1.0f);
  }
  if (data_type == IMAGE_DATA_TYPE_NANOVDB_FLOAT4) {
    return kernel_tex_image_interp_nanovdb<float4, float4>(info, P, interpolation);
  }
  if (data_type == IMAGE_DATA_TYPE_NANOVDB_FPN) {
    const float f = kernel_tex_image_interp_nanovdb<float, nanovdb::FpN>(info, P, interpolation);
    return make_float4(f, f, f, 1.0f);
  }
  if (data_type == IMAGE_DATA_TYPE_NANOVDB_FP16) {
    const float f = kernel_tex_image_interp_nanovdb<float, nanovdb::Fp16>(info, P, interpolation);
    return make_float4(f, f, f, 1.0f);
  }
  if (data_type == IMAGE_DATA_TYPE_NANOVDB_EMPTY) {
    return zero_float4();
  }
#endif

  return make_float4(
      TEX_IMAGE_MISSING_R, TEX_IMAGE_MISSING_G, TEX_IMAGE_MISSING_B, TEX_IMAGE_MISSING_A);
}

#ifndef __KERNEL_GPU__
} /* Namespace. */
#endif

CCL_NAMESPACE_END
