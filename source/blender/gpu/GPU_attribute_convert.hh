/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <algorithm>
#include <limits>

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

namespace blender::gpu {

struct PackedNormal {
  int x : 10;
  int y : 10;
  int z : 10;
  int w : 2; /* 0 by default, can manually set to { -2, -1, 0, 1 } */

  PackedNormal() = default;
  PackedNormal(int _x, int _y, int _z, int _w = 0) : x(_x), y(_y), z(_z), w(_w) {}

  /* Cast from int to float. */
  operator float4()
  {
    return float4(x, y, z, w);
  }
};

inline int convert_normalized_f32_to_i10(float x)
{
  /* OpenGL ES packs in a different order as desktop GL but component conversion is the same.
   * Of the code here, only PackedNormal needs to change. */
  constexpr int signed_int_10_max = 511;
  constexpr int signed_int_10_min = -512;

  int qx = x * signed_int_10_max;
  return std::clamp(qx, signed_int_10_min, signed_int_10_max);
}

template<typename GPUType> inline GPUType convert_normal(const float3 &src);

template<> inline PackedNormal convert_normal(const float3 &src)
{
  return {
      convert_normalized_f32_to_i10(src[0]),
      convert_normalized_f32_to_i10(src[1]),
      convert_normalized_f32_to_i10(src[2]),
      0,
  };
}

template<> inline short4 convert_normal(const float3 &src)
{
  return short4(src * float(std::numeric_limits<short>::max()), 0);
}

template<typename GPUType> void convert_normals(Span<float3> src, MutableSpan<GPUType> dst);

}  // namespace blender::gpu
