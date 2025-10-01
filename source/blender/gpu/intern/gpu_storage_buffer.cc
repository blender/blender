/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"
#include <cstring>

#include "BLI_string.h"

#include "BKE_global.hh"

#include "gpu_backend.hh"

#include "GPU_storage_buffer.hh"
#include "GPU_vertex_buffer.hh" /* For GPUUsageType. */

#include "gpu_context_private.hh"
#include "gpu_storage_buffer_private.hh"

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

namespace blender::gpu {

StorageBuf::StorageBuf(size_t size, const char *name)
{
  size_in_bytes_ = usage_size_in_bytes_ = size;
  STRNCPY(name_, name);
}

StorageBuf::~StorageBuf()
{
  MEM_SAFE_FREE(data_);
}

void StorageBuf::usage_size_set(size_t usage_size)
{
  BLI_assert(usage_size <= size_in_bytes_);
  usage_size_in_bytes_ = usage_size;
}

}  // namespace blender::gpu

/** \} */

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

using namespace blender::gpu;

blender::gpu::StorageBuf *GPU_storagebuf_create_ex(size_t size,
                                                   const void *data,
                                                   GPUUsageType usage,
                                                   const char *name)
{
  StorageBuf *ssbo = GPUBackend::get()->storagebuf_alloc(size, usage, name);
  /* Direct init. */
  if (data != nullptr) {
    ssbo->update(data);
  }
  else if (G.debug & G_DEBUG_GPU) {
    /* Fill the buffer with poison values.
     * (NaN for floats, -1 for `int` and "max value" for `uint`). */
    blender::Vector<uchar> uninitialized_data(size, 0xFF);
    ssbo->update(uninitialized_data.data());
  }

  return ssbo;
}

void GPU_storagebuf_free(blender::gpu::StorageBuf *ssbo)
{
  delete ssbo;
}

void GPU_storagebuf_usage_size_set(blender::gpu::StorageBuf *ssbo, size_t usage_size)
{
  ssbo->usage_size_set(usage_size);
}

void GPU_storagebuf_update(blender::gpu::StorageBuf *ssbo, const void *data)
{
  ssbo->update(data);
}

void GPU_storagebuf_bind(blender::gpu::StorageBuf *ssbo, int slot)
{
  ssbo->bind(slot);
}

void GPU_storagebuf_unbind(blender::gpu::StorageBuf *ssbo)
{
  ssbo->unbind();
}

void GPU_storagebuf_debug_unbind_all()
{
  Context::get()->debug_unbind_all_ssbo();
}

void GPU_storagebuf_clear_to_zero(blender::gpu::StorageBuf *ssbo)
{
  GPU_storagebuf_clear(ssbo, 0);
}

void GPU_storagebuf_clear(blender::gpu::StorageBuf *ssbo, uint32_t clear_value)
{
  ssbo->clear(clear_value);
}

void GPU_storagebuf_copy_sub_from_vertbuf(blender::gpu::StorageBuf *ssbo,
                                          blender::gpu::VertBuf *src,
                                          uint dst_offset,
                                          uint src_offset,
                                          uint copy_size)
{
  ssbo->copy_sub(src, dst_offset, src_offset, copy_size);
}

void GPU_storagebuf_sync_to_host(blender::gpu::StorageBuf *ssbo)
{
  ssbo->async_flush_to_host();
}

void GPU_storagebuf_read(blender::gpu::StorageBuf *ssbo, void *data)
{
  ssbo->read(data);
}

void GPU_storagebuf_sync_as_indirect_buffer(blender::gpu::StorageBuf *ssbo)
{
  ssbo->sync_as_indirect_buffer();
}

/** \} */
