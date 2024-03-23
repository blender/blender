/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Implementation of Multi Draw Indirect using OpenGL.
 * Fallback if the needed extensions are not supported.
 */

#include "BLI_assert.h"

#include "GPU_batch.hh"
#include "mtl_common.hh"
#include "mtl_drawlist.hh"
#include "mtl_primitive.hh"

using namespace blender::gpu;

namespace blender::gpu {

/* Indirect draw call structure for reference. */
/* MTLDrawPrimitivesIndirectArguments --
 * https://developer.apple.com/documentation/metal/mtldrawprimitivesindirectarguments?language=objc
 */
/* struct MTLDrawPrimitivesIndirectArguments {
 * uint32_t vertexCount;
 * uint32_t instanceCount;
 * uint32_t vertexStart;
 * uint32_t baseInstance;
 * }; */

/* MTLDrawIndexedPrimitivesIndirectArguments --
 * https://developer.apple.com/documentation/metal/mtldrawindexedprimitivesindirectarguments?language=objc
 */
/* struct MTLDrawIndexedPrimitivesIndirectArguments {
 * uint32_t indexCount;
 * uint32_t instanceCount;
 * uint32_t indexStart;
 * uint32_t baseVertex;
 * uint32_t baseInstance;
 * }; */

#define MDI_ENABLED (buffer_size_ != 0)
#define MDI_DISABLED (buffer_size_ == 0)
#define MDI_INDEXED (base_index_ != UINT_MAX)

MTLDrawList::MTLDrawList(int length)
{
  BLI_assert(length > 0);
  batch_ = nullptr;
  command_len_ = 0;
  base_index_ = 0;
  command_offset_ = 0;
  data_size_ = 0;
  buffer_size_ = sizeof(MTLDrawIndexedPrimitivesIndirectArguments) * length;
  data_ = (void *)MEM_mallocN(buffer_size_, __func__);
}

MTLDrawList::~MTLDrawList()
{
  if (data_) {
    MEM_freeN(data_);
    data_ = nullptr;
  }
}

void MTLDrawList::init()
{
  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  BLI_assert(ctx);
  BLI_assert(MDI_ENABLED);
  BLI_assert(data_ == nullptr);
  UNUSED_VARS_NDEBUG(ctx);

  batch_ = nullptr;
  command_len_ = 0;
  BLI_assert(data_);

  command_offset_ = 0;
}

void MTLDrawList::append(GPUBatch *gpu_batch, int i_first, int i_count)
{
  /* Fallback when MultiDrawIndirect is not supported/enabled. */
  MTLShader *shader = static_cast<MTLShader *>(unwrap(gpu_batch->shader));
  bool requires_ssbo = (shader->get_uses_ssbo_vertex_fetch());
  bool requires_emulation = mtl_needs_topology_emulation(gpu_batch->prim_type);
  if (MDI_DISABLED || requires_ssbo || requires_emulation) {
    GPU_batch_draw_advanced(gpu_batch, 0, 0, i_first, i_count);
    return;
  }

  if (data_ == nullptr) {
    this->init();
  }
  BLI_assert(data_);

  MTLBatch *mtl_batch = static_cast<MTLBatch *>(gpu_batch);
  BLI_assert(mtl_batch);
  if (mtl_batch != batch_) {
    /* Submit existing calls. */
    this->submit();

    /* Begin new batch. */
    batch_ = mtl_batch;

    /* Cached for faster access. */
    MTLIndexBuf *el = batch_->elem_();
    base_index_ = el ? el->index_base_ : UINT_MAX;
    v_first_ = el ? el->index_start_ : 0;
    v_count_ = el ? el->index_len_ : batch_->verts_(0)->vertex_len;
  }

  if (v_count_ == 0) {
    /* Nothing to draw. */
    return;
  }

  if (MDI_INDEXED) {
    MTLDrawIndexedPrimitivesIndirectArguments *cmd =
        reinterpret_cast<MTLDrawIndexedPrimitivesIndirectArguments *>((char *)data_ +
                                                                      command_offset_);
    cmd->indexStart = v_first_;
    cmd->indexCount = v_count_;
    cmd->instanceCount = i_count;
    cmd->baseVertex = base_index_;
    cmd->baseInstance = i_first;
  }
  else {
    MTLDrawPrimitivesIndirectArguments *cmd =
        reinterpret_cast<MTLDrawPrimitivesIndirectArguments *>((char *)data_ + command_offset_);
    cmd->vertexStart = v_first_;
    cmd->vertexCount = v_count_;
    cmd->instanceCount = i_count;
    cmd->baseInstance = i_first;
  }

  size_t command_size = MDI_INDEXED ? sizeof(MTLDrawIndexedPrimitivesIndirectArguments) :
                                      sizeof(MTLDrawPrimitivesIndirectArguments);

  command_offset_ += command_size;
  command_len_++;

  /* Check if we can fit at least one other command. */
  if (command_offset_ + command_size > buffer_size_) {
    this->submit();
  }

  return;
}

void MTLDrawList::submit()
{
  /* Metal does not support MDI from the host side, but we still benefit from only executing the
   * batch bind a single time, rather than per-draw.
   * NOTE(Metal): Consider using #MTLIndirectCommandBuffer to achieve similar behavior. */
  if (command_len_ == 0) {
    return;
  }

  /* Something's wrong if we get here without MDI support. */
  BLI_assert(MDI_ENABLED);
  BLI_assert(data_);

  /* Host-side MDI Currently unsupported on Metal. */
  bool can_use_MDI = false;

  /* Verify context. */
  MTLContext *ctx = reinterpret_cast<MTLContext *>(GPU_context_active_get());
  BLI_assert(ctx);

  /* Execute indirect draw calls. */
  MTLShader *shader = static_cast<MTLShader *>(unwrap(batch_->shader));
  bool SSBO_MODE = (shader->get_uses_ssbo_vertex_fetch());
  if (SSBO_MODE) {
    can_use_MDI = false;
    BLI_assert(false);
    return;
  }

  /* Heuristic to determine whether using indirect drawing is more efficient. */
  size_t command_size = MDI_INDEXED ? sizeof(MTLDrawIndexedPrimitivesIndirectArguments) :
                                      sizeof(MTLDrawPrimitivesIndirectArguments);
  const bool is_finishing_a_buffer = (command_offset_ + command_size > buffer_size_);
  can_use_MDI = can_use_MDI && (is_finishing_a_buffer || command_len_ > 2);

  /* Bind Batch to setup render pipeline state. */
  BLI_assert(batch_ != nullptr);
  id<MTLRenderCommandEncoder> rec = batch_->bind(0);
  if (rec == nil) {
    BLI_assert_msg(false, "A RenderCommandEncoder should always be available!\n");

    /* Unbind batch. */
    batch_->unbind(rec);
    return;
  }

  /* Common properties. */
  MTLPrimitiveType mtl_prim_type = gpu_prim_type_to_metal(batch_->prim_type);

  /* Execute multi-draw indirect. */
  if (can_use_MDI && false) {
    /* Metal Doesn't support MDI -- Singular Indirect draw calls are supported,
     * but Multi-draw is not.
     * TODO(Metal): Consider using #IndirectCommandBuffers to provide similar
     * behavior. */
  }
  else {

    /* Execute draws manually. */
    if (MDI_INDEXED) {
      MTLDrawIndexedPrimitivesIndirectArguments *cmd =
          (MTLDrawIndexedPrimitivesIndirectArguments *)data_;
      MTLIndexBuf *mtl_elem = static_cast<MTLIndexBuf *>(
          reinterpret_cast<IndexBuf *>(batch_->elem));
      BLI_assert(mtl_elem);
      MTLIndexType index_type = MTLIndexBuf::gpu_index_type_to_metal(mtl_elem->index_type_);
      uint32_t index_size = (mtl_elem->index_type_ == GPU_INDEX_U16) ? 2 : 4;
      uint32_t v_first_ofs = (mtl_elem->index_start_ * index_size);
      uint32_t index_count = cmd->indexCount;

      /* Fetch index buffer. May return an index buffer of a differing format,
       * if index buffer optimization is used. In these cases, mtl_prim_type and
       * index_count get updated with the new properties. */
      GPUPrimType final_prim_type = batch_->prim_type;
      id<MTLBuffer> index_buffer = mtl_elem->get_index_buffer(final_prim_type, index_count);
      BLI_assert(index_buffer != nil);

      /* Final primitive type. */
      mtl_prim_type = gpu_prim_type_to_metal(final_prim_type);

      if (index_buffer != nil) {

        /* Set depth stencil state (requires knowledge of primitive type). */
        ctx->ensure_depth_stencil_state(mtl_prim_type);

        for (int i = 0; i < command_len_; i++, cmd++) {
          [rec drawIndexedPrimitives:mtl_prim_type
                          indexCount:index_count
                           indexType:index_type
                         indexBuffer:index_buffer
                   indexBufferOffset:v_first_ofs
                       instanceCount:cmd->instanceCount
                          baseVertex:cmd->baseVertex
                        baseInstance:cmd->baseInstance];
          ctx->main_command_buffer.register_draw_counters(cmd->indexCount * cmd->instanceCount);
        }
      }
      else {
        BLI_assert_msg(false, "Index buffer does not have backing Metal buffer");
      }
    }
    else {
      MTLDrawPrimitivesIndirectArguments *cmd = (MTLDrawPrimitivesIndirectArguments *)data_;

      /* Verify if topology emulation is required. */
      if (mtl_needs_topology_emulation(batch_->prim_type)) {
        BLI_assert_msg(false, "topology emulation cases should use fallback.");
      }
      else {

        /* Set depth stencil state (requires knowledge of primitive type). */
        ctx->ensure_depth_stencil_state(mtl_prim_type);

        for (int i = 0; i < command_len_; i++, cmd++) {
          [rec drawPrimitives:mtl_prim_type
                  vertexStart:cmd->vertexStart
                  vertexCount:cmd->vertexCount
                instanceCount:cmd->instanceCount
                 baseInstance:cmd->baseInstance];
          ctx->main_command_buffer.register_draw_counters(cmd->vertexCount * cmd->instanceCount);
        }
      }
    }
  }

  /* Unbind batch. */
  batch_->unbind(rec);

  /* Reset command offsets. */
  command_len_ = 0;
  command_offset_ = 0;

  /* Avoid keeping reference to the batch. */
  batch_ = nullptr;
}

}  // namespace blender::gpu
