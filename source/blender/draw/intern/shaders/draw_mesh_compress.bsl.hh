/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Attribute compression shaders.
 */

#pragma once

#include "draw_attribute_shader_shared.hh"
#include "gpu_shader_compat.hh"

namespace draw::mesh {

/**
 * Both shaders process a batch of `count` elements starting at `start`, to stay within the
 * potentially limited GPU work group count.
 */

template<typename In, typename Out> struct CompressResources {
  [[storage(0, read)]] const In (&src)[];
  [[storage(1, write)]] Out (&dst)[];

  [[push_constant]] const int start;
  [[push_constant]] const int count;
};

template struct CompressResources<StoredFloat3, StoredNormalLQ>;
template struct CompressResources<StoredFloat3, StoredNormalHQ>;

/** `dst[i] = src[i]`. One thread processes one element. */
template<typename In, typename Out>
[[compute, local_size(DRW_MESH_COMPRESS_GROUP_SIZE)]]
void compress_main([[resource_table]] CompressResources<In, Out> &srt,
                   [[global_invocation_id]] const uint3 global_id)
{
  if (global_id.x >= uint(srt.count)) {
    return;
  }
  const uint i = uint(srt.start) + global_id.x;
  /* Need temp variables to avoid issues with NVidia GL compiler. */
  In data = srt.src[i];
  srt.dst[i] = Out::as_data(data.load_data());
}

template void compress_main<StoredFloat3, StoredNormalLQ>(
    CompressResources<StoredFloat3, StoredNormalLQ> &, const uint3);
template void compress_main<StoredFloat3, StoredNormalHQ>(
    CompressResources<StoredFloat3, StoredNormalHQ> &, const uint3);

PipelineCompute compress_normal_lq(compress_main<StoredFloat3, StoredNormalLQ>);
PipelineCompute compress_normal_hq(compress_main<StoredFloat3, StoredNormalHQ>);

}  // namespace draw::mesh
