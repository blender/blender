/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "util/texture.h"

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

#ifdef WITH_NANOVDB
/* Stochastically turn a tricubic filter into a trilinear filter. */
ccl_device_inline float3 interp_tricubic_to_trilinear_stochastic(const float3 P, float randu)
{
  /* Some optimizations possible:
   * - Could use select() for SIMD if we split the random number into 10
   *   bits each and use that for each dimensions.
   * - For GPU would be better not to compute P0 and P1 for all dimensions
   *   in advance?
   * - 1/g0 and 1/(1 - g0) are computed twice.
   */

  const float3 p = floor(P);
  const float3 t = P - p;

  /* Cubic weights. */
  const float3 w0 = (1.0f / 6.0f) * (t * (t * (-t + 3.0f) - 3.0f) + 1.0f);
  const float3 w1 = (1.0f / 6.0f) * (t * t * (3.0f * t - 6.0f) + 4.0f);
  //    float3 w2 = (1.0f / 6.0f) * (t * (t * (-3.0f * t + 3.0f) + 3.0f) + 1.0f);
  const float3 w3 = (1.0f / 6.0f) * (t * t * t);

  const float3 g0 = w0 + w1;
  const float3 P0 = p + (w1 / g0) - 1.0f;
  const float3 P1 = p + (w3 / (make_float3(1.0f) - g0)) + 1.0f;

  float3 Pnew = P0;

  if (randu < g0.x) {
    randu /= g0.x;
  }
  else {
    Pnew.x = P1.x;
    randu = (randu - g0.x) / (1 - g0.x);
  }

  if (randu < g0.y) {
    randu /= g0.y;
  }
  else {
    Pnew.y = P1.y;
    randu = (randu - g0.y) / (1 - g0.y);
  }

  if (randu < g0.z) {
  }
  else {
    Pnew.z = P1.z;
  }

  return Pnew;
}

/* From "Stochastic Texture Filtering": https://arxiv.org/abs/2305.05810
 *
 * Could be used in specific situations where we are certain a single
 * tap is enough. Maybe better to try optimizing bilinear lookups in
 * NanoVDB (detect when fully inside a single leaf) than deal with this. */

#  if 0
ccl_device int3 interp_tricubic_stochastic(const float3 P, float randu)
{
  const float ix = floorf(P.x);
  const float iy = floorf(P.y);
  const float iz = floorf(P.z);
  const float deltas[3] = {P.x - ix, P.y - iy, P.z - iz};
  int idx[3] = {(int)ix - 1, (int)iy - 1, (int)iz - 1};

  for (int i = 0; i < 3; i++) {
    const float t = deltas[i];
    const float t2 = t * t;

    /* Weighted reservoir sampling, first tap always accepted */
    const float w0 = (1.0f / 6.0f) * (-t * t2 + 3 * t2 - 3 * t + 1);
    float sumWt = w0;
    int index = 0;

    /* TODO: reduce number of divisions? */

    /* Sample the other 3 filter taps. */
    {
      const float w1 = (1.0f / 6.0f) * (3 * t * t2 - 6 * t2 + 4);
      sumWt += w1;
      const float p = w1 / sumWt;
      if (randu < p) {
        index = 1;
        randu /= p;
      }
      else {
        randu = (randu - p) / (1 - p);
      }
    }

    {
      const float w2 = (1.0f / 6.0f) * (-3 * t * t2 + 3 * t2 + 3 * t + 1);
      sumWt += w2;
      const float p = w2 / sumWt;
      if (randu < p) {
        index = 2;
        randu /= p;
      }
      else {
        randu = (randu - p) / (1 - p);
      }
    }

    {
      const float w3 = (1.0f / 6.0f) * t * t2;
      sumWt += w3;
      const float p = w3 / sumWt;
      if (randu < p) {
        index = 3;
        randu /= p;
      }
      else {
        randu = (randu - p) / (1 - p);
      }
    }

    idx[i] += index;
  }

  return make_int3(idx[0], idx[1], idx[2]);
}

ccl_device int3 interp_trilinear_stochastic(const float3 P, float randu)
{
  const float ix = floorf(P.x);
  const float iy = floorf(P.y);
  const float iz = floorf(P.z);
  int idx[3] = {(int)ix, (int)iy, (int)iz};

  const float tx = P.x - ix;
  const float ty = P.y - iy;
  const float tz = P.z - iz;

  if (randu < tx) {
    idx[0]++;
    randu /= tx;
  }
  else {
    randu = (randu - tx) / (1 - tx);
  }

  if (randu < ty) {
    idx[1]++;
    randu /= ty;
  }
  else {
    randu = (randu - ty) / (1 - ty);
  }

  if (randu < tz) {
    idx[2]++;
  }

  return make_int3(idx[0], idx[1], idx[2]);
}
#  endif

template<typename OutT, typename Acc>
ccl_device OutT kernel_tex_image_interp_trilinear_nanovdb(ccl_private Acc &acc, const float3 P)
{
  const float3 floor_P = floor(P);
  const float3 t = P - floor_P;
  const int3 index = make_int3(floor_P);

  const int ix = index.x;
  const int iy = index.y;
  const int iz = index.z;

  return mix(mix(mix(OutT(acc.getValue(make_int3(ix, iy, iz))),
                     OutT(acc.getValue(make_int3(ix, iy, iz + 1))),
                     t.z),
                 mix(OutT(acc.getValue(make_int3(ix, iy + 1, iz + 1))),
                     OutT(acc.getValue(make_int3(ix, iy + 1, iz))),
                     1.0f - t.z),
                 t.y),
             mix(mix(OutT(acc.getValue(make_int3(ix + 1, iy + 1, iz))),
                     OutT(acc.getValue(make_int3(ix + 1, iy + 1, iz + 1))),
                     t.z),
                 mix(OutT(acc.getValue(make_int3(ix + 1, iy, iz + 1))),
                     OutT(acc.getValue(make_int3(ix + 1, iy, iz))),
                     1.0f - t.z),
                 1.0f - t.y),
             t.x);
}

template<typename OutT, typename Acc>
ccl_device OutT kernel_tex_image_interp_tricubic_nanovdb(ccl_private Acc &acc, const float3 P)
{
  const float3 floor_P = floor(P);
  const float3 t = P - floor_P;
  const int3 index = make_int3(floor_P);

  const int xc[4] = {index.x - 1, index.x, index.x + 1, index.x + 2};
  const int yc[4] = {index.y - 1, index.y, index.y + 1, index.y + 2};
  const int zc[4] = {index.z - 1, index.z, index.z + 1, index.z + 2};
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

  SET_CUBIC_SPLINE_WEIGHTS(u, t.x);
  SET_CUBIC_SPLINE_WEIGHTS(v, t.y);
  SET_CUBIC_SPLINE_WEIGHTS(w, t.z);

  /* Actual interpolation. */
  return ROW_TERM(0) + ROW_TERM(1) + ROW_TERM(2) + ROW_TERM(3);

#  undef COL_TERM
#  undef ROW_TERM
#  undef DATA
#  undef SET_CUBIC_SPLINE_WEIGHTS
}

template<typename OutT, typename T>
#  if defined(__KERNEL_METAL__)
__attribute__((noinline))
#  else
ccl_device_noinline
#  endif
OutT kernel_tex_image_interp_nanovdb(const ccl_global TextureInfo &info,
                                     float3 P,
                                     const InterpolationType interp)
{
  ccl_global nanovdb::NanoGrid<T> *const grid = (ccl_global nanovdb::NanoGrid<T> *)info.data;

  if (interp == INTERPOLATION_CLOSEST) {
    nanovdb::ReadAccessor<T> acc(grid->tree().root());
    return OutT(acc.getValue(make_int3(floor(P))));
  }

  nanovdb::CachedReadAccessor<T> acc(grid->tree().root());
  if (interp == INTERPOLATION_LINEAR) {
    return kernel_tex_image_interp_trilinear_nanovdb<OutT>(acc, P);
  }

  return kernel_tex_image_interp_tricubic_nanovdb<OutT>(acc, P);
}
#endif /* WITH_NANOVDB */

ccl_device float4 kernel_tex_image_interp_3d(
    KernelGlobals kg, const int id, float3 P, InterpolationType interp, const float randu)
{
#ifdef WITH_NANOVDB
  const ccl_global TextureInfo &info = kernel_data_fetch(texture_info, id);

  if (info.use_transform_3d) {
    P = transform_point(&info.transform_3d, P);
  }

  InterpolationType interpolation = (interp == INTERPOLATION_NONE) ?
                                        (InterpolationType)info.interpolation :
                                        interp;

  /* A -0.5 offset is used to center the cubic samples around the sample point. */
  P = P - make_float3(0.5f);
  if (interpolation == INTERPOLATION_CUBIC && randu >= 0.0f) {
    P = interp_tricubic_to_trilinear_stochastic(P, randu);
    interpolation = INTERPOLATION_LINEAR;
  }

  const ImageDataType data_type = (ImageDataType)info.data_type;
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
#else
  (void)kg;
  (void)id;
  (void)P;
  (void)interp;
  (void)randu;
#endif

  return make_float4(
      TEX_IMAGE_MISSING_R, TEX_IMAGE_MISSING_G, TEX_IMAGE_MISSING_B, TEX_IMAGE_MISSING_A);
}

#ifndef __KERNEL_GPU__
} /* Namespace. */
#endif

CCL_NAMESPACE_END
