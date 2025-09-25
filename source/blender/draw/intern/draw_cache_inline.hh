/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "GPU_batch.hh"

/* Common */
// #define DRW_DEBUG_MESH_CACHE_REQUEST

#ifdef DRW_DEBUG_MESH_CACHE_REQUEST
#  define DRW_ADD_FLAG_FROM_VBO_REQUEST(flag, vbo, value) \
    (flag |= DRW_vbo_requested(vbo) ? (printf("  VBO requested " #vbo "\n") ? value : value) : 0)
#  define DRW_ADD_FLAG_FROM_IBO_REQUEST(flag, ibo, value) \
    (flag |= DRW_ibo_requested(ibo) ? (printf("  IBO requested " #ibo "\n") ? value : value) : 0)
#else
#  define DRW_ADD_FLAG_FROM_VBO_REQUEST(flag, vbo, value) \
    (flag |= DRW_vbo_requested(vbo) ? (value) : 0)
#  define DRW_ADD_FLAG_FROM_IBO_REQUEST(flag, ibo, value) \
    (flag |= DRW_ibo_requested(ibo) ? (value) : 0)
#endif

inline blender::gpu::Batch *DRW_batch_request(blender::gpu::Batch **batch)
{
  /* XXX TODO(fclem): We are writing to batch cache here. Need to make this thread safe. */
  if (*batch == nullptr) {
    *batch = GPU_batch_calloc();
  }
  return *batch;
}

inline bool DRW_batch_requested(blender::gpu::Batch *batch, GPUPrimType prim_type)
{
  /* Batch has been requested if it has been created but not initialized. */
  if (batch != nullptr && batch->verts[0] == nullptr) {
    /* HACK. We init without a valid VBO and let the first vbo binding
     * fill verts[0]. */
    GPU_batch_init_ex(batch, prim_type, (blender::gpu::VertBuf *)1, nullptr, (GPUBatchFlag)0);
    batch->verts[0] = nullptr;
    return true;
  }
  return false;
}

inline void DRW_ibo_request(blender::gpu::Batch *batch, blender::gpu::IndexBuf **ibo)
{
  if (*ibo == nullptr) {
    *ibo = GPU_indexbuf_calloc();
  }
  if (batch != nullptr) {
    GPU_batch_elembuf_set(batch, *ibo, false);
  }
}

inline bool DRW_ibo_requested(blender::gpu::IndexBuf *ibo)
{
  /* TODO: do not rely on data uploaded. This prevents multi-threading.
   * (need access to a GPU context). */
  return (ibo != nullptr && !GPU_indexbuf_is_init(ibo));
}

inline void DRW_vbo_request(blender::gpu::Batch *batch, blender::gpu::VertBuf **vbo)
{
  if (*vbo == nullptr) {
    *vbo = GPU_vertbuf_calloc();
  }
  if (batch != nullptr) {
    /* HACK we set VBO's that may not yet be valid. */
    GPU_batch_vertbuf_add(batch, *vbo, false);
  }
}

inline bool DRW_vbo_requested(blender::gpu::VertBuf *vbo)
{
  return (vbo != nullptr && (GPU_vertbuf_get_status(vbo) & GPU_VERTBUF_INIT) == 0);
}
