/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/**
 * Library to read the index buffer of a `gpu::Batch` using a SSBO rather than using `gl_VertexID`.
 * This is required for primitive expansion without geometry shader.
 * It is **not** needed if it is guaranteed that the processed `gpu::Batch` will not use any index
 * buffer.
 */

struct IndexLoad {
  [[storage(GPU_SSBO_INDEX_BUF_SLOT, read), frequency(GEOMETRY)]] const uint (&gpu_index_buf)[];

  [[push_constant]] const bool gpu_index_no_buffer;
  [[push_constant]] const bool gpu_index_16bit;
  [[push_constant]] const int gpu_index_base_index;

  /**
   * Returns the resolved index after index buffer (a.k.a. element buffer) indirection.
   */
  uint load(uint element_index) const
  {
    if (gpu_index_no_buffer) {
      return element_index;
    }

    uint raw_index = gpu_index_buf[gpu_index_16bit ? element_index >> 1u : element_index];

    if (gpu_index_16bit) {
      raw_index = ((element_index & 1u) == 0u) ? (raw_index & 0xFFFFu) : (raw_index >> 16u);
    }

    return raw_index + uint(gpu_index_base_index);
  }
};
