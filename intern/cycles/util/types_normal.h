/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/defines.h"
#include "util/math_float3.h"
#include "util/math_int4.h"

CCL_NAMESPACE_BEGIN

/* Packed normal using octahedral encoding into 2 x 16-bit signed integers.
 *
 * References:
 * "A Survey of Efficient Representations for Independent Unit Vectors"
 * JCGT 2014.
 * "Octahedron normal vector encoding"
 * https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
 * */
struct packed_normal {
  uint value;

  ccl_device_inline_method packed_normal() = default;

  ccl_device_inline_method packed_normal(const float3 in)
  {
    constexpr uint mu = (1u << 16) - 1u;
    constexpr float hmu = float(mu) / 2.0f;
    const float inv_l1 = 1.0f / (fabsf(in.x) + fabsf(in.y) + fabsf(in.z) + 1e-6f);
    float vx = in.x * inv_l1;
    float vy = in.y * inv_l1;
    const float wrapx = (1.0f - fabsf(vy)) * copysignf(1.0f, vx);
    const float wrapy = (1.0f - fabsf(vx)) * copysignf(1.0f, vy);
    vx = (in.z >= 0.0f) ? vx : wrapx;
    vy = (in.z >= 0.0f) ? vy : wrapy;
    const uint dx = uint(clamp(vx * hmu + (hmu + 0.5f), 0.0f, float(mu)));
    const uint dy = uint(clamp(vy * hmu + (hmu + 0.5f), 0.0f, float(mu)));
    value = dx | (dy << 16);
  }

  ccl_device_inline_method float3 decode() const
  {
    constexpr uint mu = (1u << 16) - 1u;
    constexpr float inv_hmu = 2.0f / float(mu);
    float nx = float(value & mu) * inv_hmu - 1.0f;
    float ny = float((value >> 16) & mu) * inv_hmu - 1.0f;
    const float nz = 1.0f - fabsf(nx) - fabsf(ny);
    const float t = max(-nz, 0.0f);
    nx += copysignf(t, -nx);
    ny += copysignf(t, -ny);
    return normalize(make_float3(nx, ny, nz));
  }

  ccl_device_inline_method bool operator==(const packed_normal other) const
  {
    return value == other.value;
  }

  ccl_device_inline_method bool operator!=(const packed_normal other) const
  {
    return value != other.value;
  }
};

#ifndef __KERNEL_GPU__
/* SIMD version of octahedral normal decoding. Decodes 4 normals at once. */
ccl_device_inline void packed_normal_decode_simd(const int4 value,
                                                 ccl_private float4 &nx,
                                                 ccl_private float4 &ny,
                                                 ccl_private float4 &nz)
{
  const int4 mu = make_int4(0xFFFF);
  const float4 inv_hmu = make_float4(2.0f / 65535.0f);
  const float4 one = make_float4(1.0f);

  nx = make_float4(value & mu) * inv_hmu - one;
  ny = make_float4((value >> 16) & mu) * inv_hmu - one;
  nz = one - fabs(nx) - fabs(ny);

  const float4 t = max(-nz, zero_float4());

  const int4 nx_pos = nx > zero_float4();
  const int4 ny_pos = ny > zero_float4();

  nx += select(nx_pos, -t, t);
  ny += select(ny_pos, -t, t);

  const float4 inv_len = one / sqrt(nx * nx + ny * ny + nz * nz);
  nx *= inv_len;
  ny *= inv_len;
  nz *= inv_len;
}
#endif

CCL_NAMESPACE_END
