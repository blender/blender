/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <algorithm>
#include <limits>

#include "BLI_normalized_int_types.hh"
#include "BLI_span.hh"

namespace blender::gpu {

template<typename GPUType> inline GPUType convert_normal(const float3 &src);

template<> inline int1010102_norm convert_normal(const float3 &src)
{
  return int1010102_norm(float4(src, 0.0f));
}

template<> inline short4 convert_normal(const float3 &src)
{
  return short4(src * float(std::numeric_limits<short>::max()), 0);
}

template<typename GPUType> void convert_normals(Span<float3> src, MutableSpan<GPUType> dst);

}  // namespace blender::gpu
