/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_string.h"
#include "BLI_time.h"

#include "GPU_state.hh"
#include "gpu_backend.hh"
#include "gpu_context_private.hh"

#include "mtl_backend.hh"
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_index_buffer.hh"
#include "mtl_memory.hh"
#include "mtl_storage_buffer.hh"
#include "mtl_uniform_buffer.hh"
#include "mtl_vertex_buffer.hh"
#include <cstdio>

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

MTLStorageBuf::MTLStorageBuf(size_t size) : StorageBuf(size, "Immediate")
{
  usage_ = GPU_USAGE_STREAM;
  storage_source_ = MTL_STORAGE_BUF_TYPE_DEFAULT;
  metal_buffer_ = MTLContext::get_global_memory_manager()->allocate_aligned(size, 256, true);
}

MTLStorageBuf::MTLStorageBuf(size_t size, GPUUsageType usage, const char *name)
    : StorageBuf(size, name)
{
  usage_ = usage;
  /* Do not create SSBO MTL buffer here to allow allocation from any thread. */
  storage_source_ = MTL_STORAGE_BUF_TYPE_DEFAULT;
  metal_buffer_ = nullptr;
}

MTLStorageBuf::MTLStorageBuf(MTLUniformBuf *uniform_buf, size_t size)
    : StorageBuf(size, "UniformBuffer_as_SSBO")
{
  usage_ = GPU_USAGE_DYNAMIC;
  storage_source_ = MTL_STORAGE_BUF_TYPE_UNIFORMBUF;
  uniform_buffer_ = uniform_buf;
  BLI_assert(uniform_buffer_ != nullptr);
}

MTLStorageBuf::MTLStorageBuf(MTLVertBuf *vert_buf, size_t size)
    : StorageBuf(size, "VertexBuffer_as_SSBO")
{
  usage_ = GPU_USAGE_DYNAMIC;
  storage_source_ = MTL_STORAGE_BUF_TYPE_VERTBUF;
  vertex_buffer_ = vert_buf;
  BLI_assert(vertex_buffer_ != nullptr);
}

MTLStorageBuf::MTLStorageBuf(MTLIndexBuf *index_buf, size_t size)
    : StorageBuf(size, "IndexBuffer_as_SSBO")
{
  usage_ = GPU_USAGE_DYNAMIC;
  storage_source_ = MTL_STORAGE_BUF_TYPE_INDEXBUF;
  index_buffer_ = index_buf;
  BLI_assert(index_buffer_ != nullptr);
}

MTLStorageBuf::MTLStorageBuf(MTLTexture *texture, size_t size)
    : StorageBuf(size, "Texture_as_SSBO")
{
  usage_ = GPU_USAGE_DYNAMIC;
  storage_source_ = MTL_STORAGE_BUF_TYPE_TEXTURE;
  texture_ = texture;
  BLI_assert(texture_ != nullptr);
}

MTLStorageBuf::~MTLStorageBuf()
{
  if (storage_source_ == MTL_STORAGE_BUF_TYPE_DEFAULT) {
    if (metal_buffer_ != nullptr) {
      metal_buffer_->free();
      metal_buffer_ = nullptr;
    }
    has_data_ = false;
  }

  if (gpu_write_fence_ != nil) {
    [gpu_write_fence_ release];
    gpu_write_fence_ = nil;
  }

  /* Ensure SSBO is not bound to active CTX.
   * SSBO bindings are reset upon Context-switch so we do not need
   * to check deactivated context's. */
  MTLContext *ctx = MTLContext::get();
  if (ctx) {
    for (MTLStorageBufferBinding &slot : ctx->pipeline_state.ssbo_bindings) {
      if (slot.bound && slot.ssbo == this) {
        slot.bound = false;
        slot.ssbo = nullptr;
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data upload / update
 * \{ */

void MTLStorageBuf::init()
{
  /* We only need to initialize the storage buffer for default buffer types. */
  if (storage_source_ != MTL_STORAGE_BUF_TYPE_DEFAULT) {
    return;
  }
  BLI_assert(this);
  BLI_assert(size_in_bytes_ > 0);

  /* Allocate MTL buffer */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);
  BLI_assert(ctx->device);
  UNUSED_VARS_NDEBUG(ctx);

  metal_buffer_ = MTLContext::get_global_memory_manager()->allocate(
      size_in_bytes_, (usage_ == GPU_USAGE_DEVICE_ONLY) ? false : true);

#ifndef NDEBUG
  static std::atomic<int> global_counter = 0;
  int index = global_counter.fetch_add(1);
  metal_buffer_->set_label([NSString stringWithFormat:@"SSBO %i %s", index, name_]);
#endif
  BLI_assert(metal_buffer_ != nullptr);
  BLI_assert(metal_buffer_->get_metal_buffer() != nil);

  has_data_ = false;
}

void MTLStorageBuf::update(const void *data)
{
  /* We only need to initialize the storage buffer for default buffer types. */
  if (storage_source_ != MTL_STORAGE_BUF_TYPE_DEFAULT) {
    return;
  }

  /* For device-only Storage buffers, update private resource via staging buffer in command
   * stream. */
  bool device_only = (usage_ == GPU_USAGE_DEVICE_ONLY);
  bool do_upload_data = (data != nullptr);

  /* If host-visible, upload data to new buffer, as previous data may still be in-use by executing
   * GPU commands. */
  if (!device_only && do_upload_data) {
    if (metal_buffer_ != nullptr) {
      metal_buffer_->free();
      metal_buffer_ = nullptr;
    }
  }

  /* Ensure buffer has been allocated. */
  if (metal_buffer_ == nullptr) {
    init();
  }

  BLI_assert(do_upload_data);
  if (do_upload_data) {
    if (device_only) {

      /* Fetch active context. */
      MTLContext *ctx = MTLContext::get();
      BLI_assert(ctx);

      /* Prepare staging buffer. */
      gpu::MTLBuffer *staging_buf = MTLContext::get_global_memory_manager()->allocate(
          size_in_bytes_, true);
      memcpy(staging_buf->get_host_ptr(), data, size_in_bytes_);
      staging_buf->flush_range(0, size_in_bytes_);
      id<MTLBuffer> staging_buf_mtl = staging_buf->get_metal_buffer();
      BLI_assert(staging_buf_mtl != nil);

      /* Ensure destination buffer. */
      id<MTLBuffer> dst_buf = this->metal_buffer_->get_metal_buffer();
      BLI_assert(dst_buf != nil);

      id<MTLBlitCommandEncoder> blit_encoder =
          ctx->main_command_buffer.ensure_begin_blit_encoder();
      [blit_encoder copyFromBuffer:staging_buf_mtl
                      sourceOffset:0
                          toBuffer:dst_buf
                 destinationOffset:0
                              size:size_in_bytes_];
      staging_buf->free();
    }
    else {
      /* Upload data. */
      BLI_assert(data != nullptr);
      BLI_assert(!(metal_buffer_->get_resource_options() & MTLResourceStorageModePrivate));
      BLI_assert(size_in_bytes_ <= metal_buffer_->get_size());
      BLI_assert(size_in_bytes_ <= [metal_buffer_->get_metal_buffer() length]);
      memcpy(metal_buffer_->get_host_ptr(), data, size_in_bytes_);
      metal_buffer_->flush_range(0, size_in_bytes_);
    }
    has_data_ = true;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Usage
 * \{ */

void MTLStorageBuf::bind(int slot)
{
  if (slot >= MTL_MAX_BUFFER_BINDINGS) {
    fprintf(
        stderr,
        "Error: Trying to bind \"%s\" ssbo to slot %d which is above the reported limit of %d.\n",
        name_,
        slot,
        MTL_MAX_BUFFER_BINDINGS);
    BLI_assert(false);
    return;
  }

  if (metal_buffer_ == nullptr) {
    this->init();
  }

  if (data_ != nullptr) {
    this->update(data_);
    MEM_SAFE_FREE(data_);
  }

  /* Bind current UBO to active context. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);

  MTLStorageBufferBinding &ctx_ssbo_bind_slot = ctx->pipeline_state.ssbo_bindings[slot];
  ctx_ssbo_bind_slot.ssbo = this;
  ctx_ssbo_bind_slot.bound = true;

  bind_slot_ = slot;
  bound_ctx_ = ctx;
}

void MTLStorageBuf::unbind()
{
  /* Unbind in debug mode to validate missing binds.
   * Otherwise, only perform a full unbind upon destruction
   * to ensure no lingering references. */
#ifndef NDEBUG
  if (true)
#else
  if (G.debug & G_DEBUG_GPU)
#endif
  {
    if (bound_ctx_ != nullptr && bind_slot_ > -1) {
      MTLStorageBufferBinding &ctx_ssbo_bind_slot =
          bound_ctx_->pipeline_state.ssbo_bindings[bind_slot_];
      if (ctx_ssbo_bind_slot.bound && ctx_ssbo_bind_slot.ssbo == this) {
        ctx_ssbo_bind_slot.bound = false;
        ctx_ssbo_bind_slot.ssbo = nullptr;
      }
    }
  }

  /* Reset bind index. */
  bind_slot_ = -1;
  bound_ctx_ = nullptr;
}

void MTLStorageBuf::clear(uint32_t clear_value)
{
  /* Fetch active context. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert_msg(ctx, "Clears should always be performed while a valid context exists.");

  if (metal_buffer_ == nullptr) {
    this->init();
  }

  if (ctx) {
    /* If all 4 bytes within clear value are equal, use the builtin fast-path for clearing. */
    uint clear_byte = clear_value & 0xFF;
    bool clear_value_bytes_equal = (clear_byte == ((clear_value >> 8) & 0xFF)) &&
                                   (clear_byte == ((clear_value >> 16) & 0xFF)) &&
                                   (clear_byte == ((clear_value >> 24) & 0xFF));
    if (clear_value_bytes_equal) {
      id<MTLBlitCommandEncoder> blit_encoder =
          ctx->main_command_buffer.ensure_begin_blit_encoder();
      [blit_encoder fillBuffer:metal_buffer_->get_metal_buffer()
                         range:NSMakeRange(0, size_in_bytes_)
                         value:clear_byte];
    }
    else {
      /* We need a special compute routine to update 32 bit values efficiently. */
      id<MTLComputePipelineState> pso = ctx->get_compute_utils().get_buffer_clear_pso();
      id<MTLComputeCommandEncoder> compute_encoder =
          ctx->main_command_buffer.ensure_begin_compute_encoder();

      MTLComputeState &cs = ctx->main_command_buffer.get_compute_state();
      cs.bind_pso(pso);
      cs.bind_compute_bytes(&clear_value, sizeof(uint32_t), 0);
      cs.bind_compute_buffer(metal_buffer_->get_metal_buffer(), 0, 1);
      [compute_encoder dispatchThreads:MTLSizeMake(size_in_bytes_ / sizeof(uint32_t), 1, 1)
                 threadsPerThreadgroup:MTLSizeMake(128, 1, 1)];
    }
  }
}

void MTLStorageBuf::copy_sub(VertBuf *src_, uint dst_offset, uint src_offset, uint copy_size)
{
  MTLVertBuf *src = static_cast<MTLVertBuf *>(src_);
  MTLStorageBuf *dst = this;

  if (dst->metal_buffer_ == nullptr) {
    dst->init();
  }
  if (copy_size == 0) {
    return;
  }
  if (src->vbo_ == nullptr) {
    src->bind();
  }

  /* Fetch active context. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);

  /* Fetch Metal buffers. */
  id<MTLBuffer> src_buf = src->vbo_->get_metal_buffer();
  id<MTLBuffer> dst_buf = dst->metal_buffer_->get_metal_buffer();
  BLI_assert(src_buf != nil);
  BLI_assert(dst_buf != nil);

  id<MTLBlitCommandEncoder> blit_encoder = ctx->main_command_buffer.ensure_begin_blit_encoder();
  [blit_encoder copyFromBuffer:src_buf
                  sourceOffset:src_offset
                      toBuffer:dst_buf
             destinationOffset:dst_offset
                          size:copy_size];
}

void MTLStorageBuf::async_flush_to_host()
{
  bool device_only = (usage_ == GPU_USAGE_DEVICE_ONLY);
  BLI_assert_msg(!device_only,
                 "Storage buffers with usage GPU_USAGE_DEVICE_ONLY cannot have their data "
                 "synchronized to the host.");
  if (device_only) {
    return;
  }

  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);

  if (gpu_write_fence_ == nil) {
    gpu_write_fence_ = [ctx->device newSharedEvent];
  }

  if (metal_buffer_ == nullptr) {
    this->init();
  }

  /* For discrete memory systems, explicitly flush GPU-resident memory back to host. */
  id<MTLBuffer> storage_buf_mtl = this->metal_buffer_->get_metal_buffer();
  if (storage_buf_mtl.storageMode == MTLStorageModeManaged) {
    id<MTLBlitCommandEncoder> blit_encoder = ctx->main_command_buffer.ensure_begin_blit_encoder();
    [blit_encoder synchronizeResource:storage_buf_mtl];
  }

  /* Encode event signal and flush command buffer to ensure GPU work is in the pipeline for future
   * reads. */
  ctx->main_command_buffer.encode_signal_event(gpu_write_fence_, ++host_read_signal_value_);
  ctx->flush();
}

void MTLStorageBuf::read(void *data)
{
  if (data == nullptr) {
    return;
  }

  if (metal_buffer_ == nullptr) {
    this->init();
  }

  /* Device-only storage buffers cannot be read directly and require staging.
   * This path should only be used for unit testing. */
  bool device_only = (usage_ == GPU_USAGE_DEVICE_ONLY);
  if (device_only) {
    /** Read storage buffer contents via staging buffer. */
    /* Fetch active context. */
    MTLContext *ctx = MTLContext::get();
    BLI_assert(ctx);

    /* Prepare staging buffer. */
    gpu::MTLBuffer *staging_buf = MTLContext::get_global_memory_manager()->allocate(size_in_bytes_,
                                                                                    true);
    id<MTLBuffer> staging_buf_mtl = staging_buf->get_metal_buffer();
    BLI_assert(staging_buf_mtl != nil);

    /* Ensure destination buffer. */
    id<MTLBuffer> storage_buf_mtl = this->metal_buffer_->get_metal_buffer();
    BLI_assert(storage_buf_mtl != nil);

    id<MTLBlitCommandEncoder> blit_encoder = ctx->main_command_buffer.ensure_begin_blit_encoder();
    [blit_encoder copyFromBuffer:storage_buf_mtl
                    sourceOffset:0
                        toBuffer:staging_buf_mtl
               destinationOffset:0
                            size:size_in_bytes_];
    if (staging_buf_mtl.storageMode == MTLStorageModeManaged) {
      [blit_encoder synchronizeResource:staging_buf_mtl];
    }

    /* Device-only reads will always stall the GPU pipe. */
    GPU_finish();
    MTL_LOG_WARNING(
        "Device-only storage buffer being read. This will stall the GPU pipeline. Ensure this "
        "path is only used in testing.");

    /* Read contents back to data. */
    memcpy(data, staging_buf->get_host_ptr(), size_in_bytes_);
    staging_buf->free();
  }
  else {
    /** Direct storage buffer read. */
    /* If we have a synchronization event from a prior memory sync, ensure memory is fully synced.
     * Otherwise, assume read is synchronous and stall until in-flight work is complete. */
    if (gpu_write_fence_ != nil) {
      /* Ensure the GPU updates are visible to the host before reading. */
      while (gpu_write_fence_.signaledValue < host_read_signal_value_) {
        BLI_time_sleep_ms(1);
      }
    }
    else {
      /* In the case of unified memory. Wait for all pending operation. */
      GPU_finish();
    }

    /* Managed buffers need to be explicitly flushed back to host. */
    if (metal_buffer_->get_resource_options() & MTLResourceStorageModeManaged) {
      /* Fetch active context. */
      MTLContext *ctx = MTLContext::get();
      BLI_assert(ctx);

      /* Ensure GPU updates are flushed back to CPU. */
      id<MTLBlitCommandEncoder> blit_encoder =
          ctx->main_command_buffer.ensure_begin_blit_encoder();
      [blit_encoder synchronizeResource:metal_buffer_->get_metal_buffer()];

      /* Wait for the blit to finish. */
      GPU_finish();
    }

    /* Read data. NOTE: Unless explicitly synchronized with GPU work, results may not be ready. */
    memcpy(data, metal_buffer_->get_host_ptr(), size_in_bytes_);
  }
}

id<MTLBuffer> MTLStorageBuf::get_metal_buffer()
{

  gpu::MTLBuffer *source_buffer = nullptr;
  switch (storage_source_) {
    /* Default SSBO buffer comes from own allocation. */
    case MTL_STORAGE_BUF_TYPE_DEFAULT: {
      /* NOTE: We should always ensure that the data is primed prior to requiring fetching of the
       * buffer. If get_metal_buffer is called during a resource bind phase, invoking a blit
       * command encoder to upload data would override the active encoder state being prepared.
       * Resource generation and data upload should happen earlier as a resource is bound. */
      BLI_assert_msg(metal_buffer_ != nullptr,
                     "Storage Buffer backing resource does not yet exist. Ensure the resource is "
                     "bound with data before the calling code requires its underlying MTLBuffer.");
      BLI_assert_msg(
          data_ == nullptr,
          "Storage Buffer backing resource data has not yet been uploaded. Ensure the resource is "
          "bound with data before the calling code requires its underlying MTLBuffer.");
      source_buffer = metal_buffer_;
    } break;
    /* SSBO buffer comes from Uniform Buffer. */
    case MTL_STORAGE_BUF_TYPE_UNIFORMBUF: {
      source_buffer = uniform_buffer_->metal_buffer_;
    } break;
    /* SSBO buffer comes from Vertex Buffer. */
    case MTL_STORAGE_BUF_TYPE_VERTBUF: {
      source_buffer = vertex_buffer_->vbo_;
    } break;
    /* SSBO buffer comes from Index Buffer. */
    case MTL_STORAGE_BUF_TYPE_INDEXBUF: {
      source_buffer = index_buffer_->ibo_;
    } break;
    /* SSBO buffer comes from Texture. */
    case MTL_STORAGE_BUF_TYPE_TEXTURE: {
      BLI_assert(texture_);
      /* Fetch metal texture to ensure it has been initialized. */
      id<MTLTexture> tex = texture_->get_metal_handle_base();
      BLI_assert(tex != nil);
      UNUSED_VARS_NDEBUG(tex);
      source_buffer = texture_->backing_buffer_;
    }
  }

  /* Return Metal allocation handle and flag as used. */
  BLI_assert(source_buffer != nullptr);
  source_buffer->debug_ensure_used();
  return source_buffer->get_metal_buffer();
}

size_t MTLStorageBuf::get_size()
{
  BLI_assert(this);
  return size_in_bytes_;
}

}  // namespace blender::gpu
