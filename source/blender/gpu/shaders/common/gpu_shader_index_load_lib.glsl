/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/gpu_index_load_infos.hh"

SHADER_LIBRARY_CREATE_INFO(gpu_index_buffer_load)

/**
 * Library to read the index buffer of a `gpu::Batch` using a SSBO rather than using `gl_VertexID`.
 * This is required for primitive expansion without geometry shader.
 * It is **not** needed if it is guaranteed that the processed `gpu::Batch` will not use any index
 * buffer.
 */

#ifndef WORKAROUND_INDEX_LOAD_INCLUDE
#  ifndef GPU_INDEX_LOAD
#    error Missing gpu_index_buffer_load create info dependency
#  endif

/**
 * Returns the resolved index after index buffer (a.k.a. element buffer) indirection.
 */
uint gpu_index_load(uint element_index)
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

#endif
