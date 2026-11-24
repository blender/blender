/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.hh"

#include "BLI_string.hh"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"

#include "mtl_backend.hh"
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_storage_buffer.hh"
#include "mtl_uniform_buffer.hh"

namespace blender::gpu {

MTLUniformBuf::MTLUniformBuf(size_t size, const char *name) : UniformBuf(size, name) {}

MTLUniformBuf::~MTLUniformBuf()
{
  if (metal_buffer_ != nullptr) {
    metal_buffer_->free();
    metal_buffer_ = nullptr;
  }

  this->unbind();

  if (ssbo_wrapper_) {
    delete ssbo_wrapper_;
    ssbo_wrapper_ = nullptr;
  }
}

void MTLUniformBuf::update(const void *data)
{
  BLI_assert(this);
  BLI_assert(size_in_bytes_ > 0);

  /* Free existing allocation.
   * The previous UBO resource will be tracked by the memory manager,
   * in case dependent GPU work is still executing. */
  if (metal_buffer_ != nullptr) {
    metal_buffer_->free();
    metal_buffer_ = nullptr;
  }

  /* Allocate MTL buffer */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);
  BLI_assert(ctx->device);
  UNUSED_VARS_NDEBUG(ctx);

  if (data) {
    metal_buffer_ = MTLContext::get_global_memory_manager()->allocate_with_data(
        size_in_bytes_, true, data);
  }
  else {
    metal_buffer_ = MTLContext::get_global_memory_manager()->allocate(size_in_bytes_, true);
  }

#ifndef NDEBUG
  static std::atomic<int> global_counter = 0;
  int index = global_counter.fetch_add(1);
  metal_buffer_->set_label([NSString stringWithFormat:@"UBO %i %s", index, name_]);
#endif

  BLI_assert(metal_buffer_ != nullptr);
  BLI_assert(metal_buffer_->get_metal_buffer() != nil);
}

void MTLUniformBuf::clear_to_zero()
{
  /* TODO(fclem): Avoid another allocation and just do the clear on the GPU if possible. */
  void *clear_data = calloc(1, size_in_bytes_);
  this->update(clear_data);
  free(clear_data);
}

void MTLUniformBuf::bind(int slot)
{
  if (slot < 0) {
    MTL_LOG_WARNING("Failed to bind UBO %p. uniform location %d invalid.", this, slot);
    return;
  }

  if (slot >= MTL_MAX_UBO) {
    MTL_LOG_ERROR("Failed to bind UBO %p. Uniform buffer slot %d exceeds the maximum of %d.",
                  this,
                  slot,
                  MTL_MAX_UBO);
    BLI_assert(false);
    return;
  }

  /* Bind current UBO to active context. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);

  MTLUniformBufferBinding &ctx_ubo_bind_slot = ctx->pipeline_state.ubo_bindings[slot];
  ctx_ubo_bind_slot.ubo = this;
  ctx_ubo_bind_slot.bound = true;

  /* Check if we have any deferred data to upload. */
  if (data_ != nullptr) {
    this->update(data_);
    MEM_SAFE_DELETE_VOID(data_);
  }

  /* Ensure there is at least an empty dummy buffer. */
  if (metal_buffer_ == nullptr) {
    this->update(nullptr);
  }
}

void MTLUniformBuf::bind_as_ssbo(int slot)
{
  if (slot < 0) {
    MTL_LOG_WARNING("Failed to bind UBO %p as SSBO. uniform location %d invalid.", this, slot);
    return;
  }

  /* We need to ensure data is actually allocated if using as an SSBO, as resource may be written
   * to. */
  if (metal_buffer_ == nullptr) {
    /* Check if we have any deferred data to upload. */
    if (data_ != nullptr) {
      this->update(data_);
      MEM_SAFE_DELETE_VOID(data_);
    }
    else {
      this->clear_to_zero();
    }
  }

  /* Create MTLStorageBuffer to wrap this resource and use conventional binding. */
  if (ssbo_wrapper_ == nullptr) {
    ssbo_wrapper_ = new MTLStorageBuf(this, size_in_bytes_);
  }

  ssbo_wrapper_->bind(slot);
}

void MTLUniformBuf::unbind()
{
  /* Only remove the bindings from the context active on this thread. Other contexts must not be
   * accessed, as they may be in use on another thread or already be destroyed. Their bindings are
   * reset when they are activated or destroyed. */
  MTLContext *ctx = MTLContext::get();
  if (ctx) {
    for (MTLUniformBufferBinding &slot : ctx->pipeline_state.ubo_bindings) {
      if (slot.bound && slot.ubo == this) {
        slot.bound = false;
        slot.ubo = nullptr;
      }
    }
  }

  if (ssbo_wrapper_) {
    ssbo_wrapper_->unbind();
  }
}

id<MTLBuffer> MTLUniformBuf::get_metal_buffer()
{
  BLI_assert(this);
  if (metal_buffer_ != nullptr) {
    metal_buffer_->debug_ensure_used();
    return metal_buffer_->get_metal_buffer();
  }
  return nil;
}

size_t MTLUniformBuf::get_size()
{
  BLI_assert(this);
  return size_in_bytes_;
}

}  // namespace blender::gpu
