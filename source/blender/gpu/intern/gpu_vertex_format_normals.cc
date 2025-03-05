/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"

#include "GPU_attribute_convert.hh"

namespace blender::gpu {

template<typename GPUType>
static void convert_normals_impl(const Span<float3> src, MutableSpan<GPUType> dst)
{
  threading::parallel_for(src.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      dst[i] = convert_normal<GPUType>(src[i]);
    }
  });
}

template<> void convert_normals(const Span<float3> src, MutableSpan<PackedNormal> dst)
{
  convert_normals_impl(src, dst);
}
template<> void convert_normals(const Span<float3> src, MutableSpan<short4> dst)
{
  convert_normals_impl(src, dst);
}

}  // namespace blender::gpu
