/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#include "mtl_vertex_buffer.hh"
#include "mtl_debug.hh"
#include "mtl_storage_buffer.hh"

namespace blender::gpu {

MTLVertBuf::MTLVertBuf() : VertBuf() {}

MTLVertBuf::~MTLVertBuf()
{
  this->release_data();
}

void MTLVertBuf::acquire_data()
{
  /* Discard previous data, if any. */
  MEM_SAFE_FREE(data);
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    data = nullptr;
  }
  else {
    data = (uchar *)MEM_mallocN(sizeof(uchar) * this->size_alloc_get(), __func__);
  }
}

void MTLVertBuf::resize_data()
{
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    data = nullptr;
  }
  else {
    data = (uchar *)MEM_reallocN(data, sizeof(uchar) * this->size_alloc_get());
  }
}

void MTLVertBuf::release_data()
{
  if (vbo_ != nullptr) {
    vbo_->free();
    vbo_ = nullptr;
    is_wrapper_ = false;
  }

  GPU_TEXTURE_FREE_SAFE(buffer_texture_);

  MEM_SAFE_FREE(data);

  if (ssbo_wrapper_) {
    delete ssbo_wrapper_;
    ssbo_wrapper_ = nullptr;
  }
}

void MTLVertBuf::duplicate_data(VertBuf *dst_)
{
  BLI_assert(MTLContext::get() != NULL);
  MTLVertBuf *src = this;
  MTLVertBuf *dst = static_cast<MTLVertBuf *>(dst_);

  /* Ensure buffer has been initialized. */
  src->bind();

  if (src->vbo_) {

    /* Fetch active context. */
    MTLContext *ctx = MTLContext::get();
    BLI_assert(ctx);

    /* Ensure destination does not have an active VBO. */
    BLI_assert(dst->vbo_ == nullptr);

    /* Allocate VBO for destination vertbuf. */
    uint64_t length = src->vbo_->get_size();
    dst->vbo_ = MTLContext::get_global_memory_manager()->allocate(
        length, (dst->get_usage_type() != GPU_USAGE_DEVICE_ONLY));
    dst->alloc_size_ = length;

    /* Fetch Metal buffer handles. */
    id<MTLBuffer> src_buffer = src->vbo_->get_metal_buffer();
    id<MTLBuffer> dest_buffer = dst->vbo_->get_metal_buffer();

    /* Use blit encoder to copy data to duplicate buffer allocation. */
    id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
    if (G.debug & G_DEBUG_GPU) {
      [enc insertDebugSignpost:@"VertexBufferDuplicate"];
    }
    [enc copyFromBuffer:src_buffer
             sourceOffset:0
                 toBuffer:dest_buffer
        destinationOffset:0
                     size:length];

    /* Flush results back to host buffer, if one exists. */
    if (dest_buffer.storageMode == MTLStorageModeManaged) {
      [enc synchronizeResource:dest_buffer];
    }

    if (G.debug & G_DEBUG_GPU) {
      [enc insertDebugSignpost:@"VertexBufferDuplicateEnd"];
    }

    /* Mark as in-use, as contents are updated via GPU command. */
    src->flag_used();
  }

  /* Copy raw CPU data. */
  if (data != nullptr) {
    dst->data = (uchar *)MEM_dupallocN(src->data);
  }
}

void MTLVertBuf::upload_data()
{
  this->bind();
}

void MTLVertBuf::bind()
{
  /* Determine allocation size. Set minimum allocation size to be
   * the maximal of a single attribute to avoid validation and
   * correctness errors. */
  uint64_t required_size_raw = sizeof(uchar) * this->size_used_get();
  uint64_t required_size = max_ulul(required_size_raw, 128);

  if (required_size_raw == 0) {
    MTL_LOG_INFO("Vertex buffer required_size = 0");
  }

  /* If the vertex buffer has already been allocated, but new data is ready,
   * or the usage size has changed, we release the existing buffer and
   * allocate a new buffer to ensure we do not overwrite in-use GPU resources.
   *
   * NOTE: We only need to free the existing allocation if contents have been
   * submitted to the GPU. Otherwise we can simply upload new data to the
   * existing buffer, if it will fit.
   *
   * NOTE: If a buffer is re-sized, but no new data is provided, the previous
   * contents are copied into the newly allocated buffer. */
  bool requires_reallocation = (vbo_ != nullptr) && (alloc_size_ != required_size);
  bool new_data_ready = (this->flag & GPU_VERTBUF_DATA_DIRTY) && this->data;

  gpu::MTLBuffer *prev_vbo = nullptr;
  GPUVertBufStatus prev_flag = this->flag;

  if (vbo_ != nullptr) {
    if (requires_reallocation || (new_data_ready && contents_in_flight_)) {
      /* Track previous VBO to copy data from. */
      prev_vbo = vbo_;

      /* Reset current allocation status. */
      vbo_ = nullptr;
      is_wrapper_ = false;
      alloc_size_ = 0;

      /* Flag as requiring data upload. */
      if (requires_reallocation) {
        this->flag &= ~GPU_VERTBUF_DATA_UPLOADED;
      }
    }
  }

  /* Create MTLBuffer of requested size. */
  if (vbo_ == nullptr) {
    vbo_ = MTLContext::get_global_memory_manager()->allocate(
        required_size, (this->get_usage_type() != GPU_USAGE_DEVICE_ONLY));
    vbo_->set_label(@"Vertex Buffer");
    BLI_assert(vbo_ != nullptr);
    BLI_assert(vbo_->get_metal_buffer() != nil);

    is_wrapper_ = false;
    alloc_size_ = required_size;
    contents_in_flight_ = false;
  }

  /* Upload new data, if provided. */
  if (new_data_ready) {

    /* Only upload data if usage size is greater than zero.
     * Do not upload data for device-only buffers. */
    if (required_size_raw > 0 && usage_ != GPU_USAGE_DEVICE_ONLY) {

      /* Debug: Verify allocation is large enough. */
      BLI_assert(vbo_->get_size() >= required_size_raw);

      /* Fetch mapped buffer host ptr and upload data. */
      void *dst_data = vbo_->get_host_ptr();
      memcpy((uint8_t *)dst_data, this->data, required_size_raw);
      vbo_->flush_range(0, required_size_raw);
    }

    /* If static usage, free host-side data. */
    if (usage_ == GPU_USAGE_STATIC) {
      MEM_SAFE_FREE(data);
    }

    /* Flag data as having been uploaded. */
    this->flag &= ~GPU_VERTBUF_DATA_DIRTY;
    this->flag |= GPU_VERTBUF_DATA_UPLOADED;
  }
  else if (requires_reallocation) {

    /* If buffer has been re-sized, copy existing data if host
     * data had been previously uploaded. */
    BLI_assert(prev_vbo != nullptr);

    if (prev_flag & GPU_VERTBUF_DATA_UPLOADED) {

      /* Fetch active context. */
      MTLContext *ctx = MTLContext::get();
      BLI_assert(ctx);

      id<MTLBuffer> copy_prev_buffer = prev_vbo->get_metal_buffer();
      id<MTLBuffer> copy_new_buffer = vbo_->get_metal_buffer();
      BLI_assert(copy_prev_buffer != nil);
      BLI_assert(copy_new_buffer != nil);

      /* Ensure a blit command encoder is active for buffer copy operation. */
      id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
      [enc copyFromBuffer:copy_prev_buffer
               sourceOffset:0
                   toBuffer:copy_new_buffer
          destinationOffset:0
                       size:min_ulul([copy_new_buffer length], [copy_prev_buffer length])];

      /* Flush newly copied data back to host-side buffer, if one exists.
       * Ensures data and cache coherency for managed MTLBuffers. */
      if (copy_new_buffer.storageMode == MTLStorageModeManaged) {
        [enc synchronizeResource:copy_new_buffer];
      }

      /* For VBOs flagged as static, release host data as it will no longer be needed. */
      if (usage_ == GPU_USAGE_STATIC) {
        MEM_SAFE_FREE(data);
      }

      /* Flag data as uploaded. */
      this->flag |= GPU_VERTBUF_DATA_UPLOADED;

      /* Flag as in-use, as contents have been updated via GPU commands. */
      this->flag_used();
    }
  }

  /* Release previous buffer if re-allocated. */
  if (prev_vbo != nullptr) {
    prev_vbo->free();
  }

  /* Ensure buffer has been created. */
  BLI_assert(vbo_ != nullptr);
}

/* Update Sub currently only used by hair */
void MTLVertBuf::update_sub(uint start, uint len, const void *data)
{
  /* Fetch and verify active context. */
  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(ctx);
  BLI_assert(ctx->device);

  /* Ensure vertbuf has been created. */
  this->bind();
  BLI_assert(start + len <= alloc_size_);

  /* Create temporary scratch buffer allocation for sub-range of data. */
  MTLTemporaryBuffer scratch_allocation =
      ctx->get_scratchbuffer_manager().scratch_buffer_allocate_range_aligned(len, 256);
  memcpy(scratch_allocation.data, data, len);
  [scratch_allocation.metal_buffer
      didModifyRange:NSMakeRange(scratch_allocation.buffer_offset, len)];
  id<MTLBuffer> data_buffer = scratch_allocation.metal_buffer;
  uint64_t data_buffer_offset = scratch_allocation.buffer_offset;

  BLI_assert(vbo_ != nullptr && data != nullptr);
  BLI_assert((start + len) <= vbo_->get_size());

  /* Fetch destination buffer. */
  id<MTLBuffer> dst_buffer = vbo_->get_metal_buffer();

  /* Ensure blit command encoder for copying data. */
  id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
  [enc copyFromBuffer:data_buffer
           sourceOffset:data_buffer_offset
               toBuffer:dst_buffer
      destinationOffset:start
                   size:len];

  /* Flush modified buffer back to host buffer, if one exists. */
  if (dst_buffer.storageMode == MTLStorageModeManaged) {
    [enc synchronizeResource:dst_buffer];
  }
}

void MTLVertBuf::bind_as_ssbo(uint binding)
{
  this->flag_used();

  /* Ensure resource is initialized. */
  this->bind();

  /* Create MTLStorageBuffer to wrap this resource and use conventional binding. */
  if (ssbo_wrapper_ == nullptr) {
    ssbo_wrapper_ = new MTLStorageBuf(this, alloc_size_);
  }
  ssbo_wrapper_->bind(binding);
}

void MTLVertBuf::bind_as_texture(uint binding)
{
  /* Ensure allocations are ready, and data uploaded. */
  this->bind();
  BLI_assert(vbo_ != nullptr);

  /* If vertex buffer updated, release existing texture and re-create. */
  id<MTLBuffer> buf = this->get_metal_buffer();
  if (buffer_texture_ != nullptr) {
    gpu::MTLTexture *mtl_buffer_tex = static_cast<gpu::MTLTexture *>(
        unwrap(this->buffer_texture_));
    id<MTLBuffer> tex_buf = mtl_buffer_tex->get_vertex_buffer();
    if (tex_buf != buf) {
      GPU_TEXTURE_FREE_SAFE(buffer_texture_);
      buffer_texture_ = nullptr;
    }
  }

  /* Create texture from vertex buffer. */
  if (buffer_texture_ == nullptr) {
    buffer_texture_ = GPU_texture_create_from_vertbuf("vertbuf_as_texture", wrap(this));
  }

  /* Verify successful creation and bind. */
  BLI_assert(buffer_texture_ != nullptr);
  GPU_texture_bind(buffer_texture_, binding);
}

void MTLVertBuf::read(void *data) const
{
  BLI_assert(vbo_ != nullptr);
  BLI_assert(usage_ != GPU_USAGE_DEVICE_ONLY);
  void *host_ptr = vbo_->get_host_ptr();
  memcpy(data, host_ptr, alloc_size_);
}

void MTLVertBuf::wrap_handle(uint64_t handle)
{
  BLI_assert(vbo_ == nullptr);

  /* Attempt to cast to Metal buffer handle. */
  BLI_assert(handle != 0);
  id<MTLBuffer> buffer = reinterpret_cast<id<MTLBuffer>>((void *)handle);

  is_wrapper_ = true;
  vbo_ = new gpu::MTLBuffer(buffer);

  /* We assume the data is already on the device, so no need to allocate or send it. */
  flag = GPU_VERTBUF_DATA_UPLOADED;
}

void MTLVertBuf::flag_used()
{
  contents_in_flight_ = true;
}

}  // namespace blender::gpu
