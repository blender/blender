/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "mtl_index_buffer.hh"
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_storage_buffer.hh"

#include "BLI_span.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Core MTLIndexBuf implementation.
 * \{ */

MTLIndexBuf::~MTLIndexBuf()
{
  if (ibo_ != nullptr && !this->is_subrange_) {
    ibo_->free();
  }
  this->free_optimized_buffer();

  if (ssbo_wrapper_) {
    delete ssbo_wrapper_;
    ssbo_wrapper_ = nullptr;
  }
}

void MTLIndexBuf::free_optimized_buffer()
{
  if (optimized_ibo_) {
    optimized_ibo_->free();
    optimized_ibo_ = nullptr;
  }
}

void MTLIndexBuf::bind_as_ssbo(uint32_t binding)
{
  /* Flag buffer as incompatible with optimized/patched buffers as contents
   * can now have partial modifications from the GPU. */
  this->flag_can_optimize(false);
  this->free_optimized_buffer();

  /* Ensure resource is initialized. */
  this->upload_data();

  /* Ensure we have a valid IBO. */
  BLI_assert(this->ibo_);

  /* Create MTLStorageBuffer to wrap this resource and use conventional binding. */
  if (ssbo_wrapper_ == nullptr) {
    /* Buffer's size in bytes is required to be multiple of 16. */
    int multiple_of_16 = ceil_to_multiple_u(alloc_size_, 16);
    ssbo_wrapper_ = new MTLStorageBuf(this, multiple_of_16);
  }
  ssbo_wrapper_->bind(binding);
}

void MTLIndexBuf::read(uint32_t *data) const
{
  if (ibo_ != nullptr) {
    /* Fetch active context. */
    MTLContext *ctx = MTLContext::get();
    BLI_assert(ctx);

    /* Ensure data is flushed for host caches. */
    id<MTLBuffer> source_buffer = ibo_->get_metal_buffer();
    if (source_buffer.storageMode == MTLStorageModeManaged) {
      id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
      [enc synchronizeResource:source_buffer];
    }

    /* Ensure GPU has finished operating on commands which may modify data. */
    GPU_finish();

    /* Read data. */
    void *host_ptr = ibo_->get_host_ptr();
    memcpy(data, host_ptr, size_get());
    return;
  }
  BLI_assert(false && "Index buffer not ready to be read.");
}

void MTLIndexBuf::upload_data()
{
  /* Handle sub-range upload. */
  if (is_subrange_) {
    MTLIndexBuf *mtlsrc = static_cast<MTLIndexBuf *>(src_);
    mtlsrc->upload_data();

#ifndef NDEBUG
    BLI_assert_msg(!mtlsrc->point_restarts_stripped_,
                   "Cannot use sub-range on stripped point buffer.");
#endif

    /* If parent sub-range allocation has changed,
     * update our index buffer. */
    if (alloc_size_ != mtlsrc->alloc_size_ || ibo_ != mtlsrc->ibo_) {

      /* Update index buffer and allocation from source. */
      alloc_size_ = mtlsrc->alloc_size_;
      ibo_ = mtlsrc->ibo_;

      /* Reset any allocated patched or optimized index buffers. */
      this->free_optimized_buffer();
    }
    return;
  }

  /* If new data ready, and index buffer already exists, release current. */
  if ((ibo_ != nullptr) && (this->data_ != nullptr)) {
    MTL_LOG_DEBUG("Re-creating index buffer with new data. IndexBuf %p", this);
    ibo_->free();
    ibo_ = nullptr;
  }

  /* Prepare Buffer and Upload Data. */
  if (ibo_ == nullptr) {
    alloc_size_ = this->size_get();
    if (alloc_size_ == 0) {
      MTL_LOG_WARNING("Warning! Trying to allocate index buffer with size=0 bytes");
    }
    else {
      if (data_) {
        ibo_ = MTLContext::get_global_memory_manager()->allocate_with_data(
            alloc_size_, true, data_);
      }
      else {
        ibo_ = MTLContext::get_global_memory_manager()->allocate(alloc_size_, true);
      }
      BLI_assert(ibo_);
#ifndef NDEBUG
      static std::atomic<int> global_counter = 0;
      int index = global_counter.fetch_add(1);
      ibo_->set_label([NSString stringWithFormat:@"IBO %i", index]);
#endif
    }

    /* No need to keep copy of data_ in system memory. */
    if (data_) {
      MEM_SAFE_FREE(data_);
    }
  }
}

void MTLIndexBuf::update_sub(uint32_t start, uint32_t len, const void *data)
{
  BLI_assert(!is_subrange_);

  /* If host-side data still exists, modify and upload as normal */
  if (data_ != nullptr) {

    /* Free index buffer if one exists. */
    if (ibo_ != nullptr && !this->is_subrange_) {
      ibo_->free();
      ibo_ = nullptr;
    }

    BLI_assert(start + len < this->size_get());

    /* Apply start byte offset to data pointer. */
    void *modified_base_ptr = data_;
    uint8_t *ptr = static_cast<uint8_t *>(modified_base_ptr);
    ptr += start;
    modified_base_ptr = static_cast<void *>(ptr);

    /* Modify host-side data. */
    memcpy(modified_base_ptr, data, len);
    return;
  }

  /* Verify buffer. */
  BLI_assert(ibo_ != nullptr);

  /* Otherwise, we will inject a data update, using staged data, into the command stream.
   * Stage update contents in temporary buffer. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);
  MTLTemporaryBuffer range = ctx->get_scratch_buffer_manager().scratch_buffer_allocate_range(len);
  memcpy(range.data, data, len);

  /* Copy updated contents into primary buffer.
   * These changes need to be uploaded via blit to ensure the data copies happen in-order. */
  id<MTLBuffer> dest_buffer = ibo_->get_metal_buffer();
  BLI_assert(dest_buffer != nil);

  id<MTLBlitCommandEncoder> enc = ctx->main_command_buffer.ensure_begin_blit_encoder();
  [enc copyFromBuffer:range.metal_buffer
           sourceOffset:(uint32_t)range.buffer_offset
               toBuffer:dest_buffer
      destinationOffset:start
                   size:len];

  /* Synchronize changes back to host to ensure CPU-side data is up-to-date for non
   * Shared buffers. */
  if (dest_buffer.storageMode == MTLStorageModeManaged) {
    [enc synchronizeResource:dest_buffer];
  }

  /* Invalidate patched/optimized buffers. */
  this->free_optimized_buffer();

  /* Flag buffer as incompatible with optimized/patched buffers as contents
   * have partial modifications. */
  this->flag_can_optimize(false);

  BLI_assert(false);
}

void MTLIndexBuf::flag_can_optimize(bool can_optimize)
{
  can_optimize_ = can_optimize;

  /* NOTE: Index buffer optimization needs to be disabled for Indirect draws, as the index count is
   * unknown at submission time. However, if the index buffer has already been optimized by a
   * separate draw pass, errors will occur and these cases need to be resolved at the high-level,
   * ensuring primitive types without primitive restart are used instead, as these perform far
   * more optimally on hardware. */
  BLI_assert_msg(can_optimize_ || (optimized_ibo_ == nullptr),
                 "Index buffer optimization disabled, but optimal buffer already generated.");
}

/** \} */

/** \name Index buffer optimization and topology emulation
 *
 * Index buffer optimization and emulation. Optimize index buffers by
 * eliminating restart-indices.
 * Emulate unsupported index types e.g. Triangle Fan and Line Loop.
 * \{ */

/* Returns total vertices in new buffer. */
template<typename T>
static uint32_t populate_optimized_tri_strip_buf(Span<T> original_data,
                                                 MutableSpan<T> output_data,
                                                 uint32_t input_index_len)
{
  /* Generate #TriangleList from #TriangleStrip. */
  uint32_t current_vert_len = 0;
  uint32_t current_output_ind = 0;
  T indices[3];

  for (int c_index = 0; c_index < input_index_len; c_index++) {
    T current_index = original_data[c_index];
    if (current_index == T(-1)) {
      /* Stop current primitive. Move onto next. */
      current_vert_len = 0;
    }
    else {
      if (current_vert_len < 3) {
        /* Prepare first triangle.
         * Cache indices before generating a triangle, in case we have bad primitive-restarts. */
        indices[current_vert_len] = current_index;
      }

      /* Emit triangle once we reach 3 input verts in current strip. */
      if (current_vert_len == 3) {
        /* First triangle in strip. */
        output_data[current_output_ind++] = indices[0];
        output_data[current_output_ind++] = indices[1];
        output_data[current_output_ind++] = indices[2];
      }
      else if (current_vert_len > 3) {
        /* All other triangles in strip.
         * These triangles are populated using data from previous 2 vertices
         * and the latest index. */
        uint32_t tri_id = current_vert_len - 3;
        uint32_t base_output_ind = current_output_ind;
        if ((tri_id % 2) == 0) {
          output_data[base_output_ind + 0] = output_data[base_output_ind - 2];
          output_data[base_output_ind + 1] = current_index;
          output_data[base_output_ind + 2] = output_data[base_output_ind - 1];
        }
        else {
          output_data[base_output_ind + 0] = output_data[base_output_ind - 1];
          output_data[base_output_ind + 1] = output_data[base_output_ind - 2];
          output_data[base_output_ind + 2] = current_index;
        }
        current_output_ind += 3;
      }

      /* Increment relative vertex index. */
      current_vert_len++;
    }
  }
  return current_output_ind;
}

/* Returns total vertices in new buffer. */
template<typename T>
static uint32_t populate_emulated_tri_fan_buf(Span<T> original_data,
                                              MutableSpan<T> output_data,
                                              uint32_t input_index_len)
{
  /* Generate #TriangleList from #TriangleFan. */
  T base_prim_ind_val = 0;
  uint32_t current_vert_len = 0;
  uint32_t current_output_ind = 0;
  T indices[3];

  for (int c_index = 0; c_index < input_index_len; c_index++) {
    T current_index = original_data[c_index];
    if (current_index == T(-1)) {
      /* Stop current primitive. Move onto next. */
      current_vert_len = 0;
    }
    else {
      if (current_vert_len < 3) {
        /* Prepare first triangle.
         * Cache indices before generating a triangle, in case we have bad primitive-restarts. */
        indices[current_vert_len] = current_index;
      }

      /* emit triangle once we reach 3 input verts in current strip. */
      if (current_vert_len == 3) {
        /* First triangle in strip. */
        output_data[current_output_ind++] = indices[0];
        output_data[current_output_ind++] = indices[1];
        output_data[current_output_ind++] = indices[2];
        base_prim_ind_val = indices[0];
      }
      else if (current_vert_len > 3) {
        /* All other triangles in strip.
         * These triangles are populated using data from previous 2 vertices
         * and the latest index. */
        uint32_t base_output_ind = current_output_ind;

        output_data[base_output_ind + 0] = base_prim_ind_val;
        output_data[base_output_ind + 1] = output_data[base_output_ind - 1];
        output_data[base_output_ind + 2] = current_index;
        current_output_ind += 3;
      }

      /* Increment relative vertex index. */
      current_vert_len++;
    }
  }
  return current_output_ind;
}

id<MTLBuffer> MTLIndexBuf::get_index_buffer(GPUPrimType &in_out_primitive_type,
                                            uint32_t &in_out_v_count)
{
  /* Determine whether to return the original index buffer, or whether we
   * should emulate an unsupported primitive type, or optimize a restart-
   * compatible type for faster performance. */
  bool should_optimize_or_emulate = (in_out_primitive_type == GPU_PRIM_TRI_FAN) ||
                                    (in_out_primitive_type == GPU_PRIM_TRI_STRIP);
  if (!should_optimize_or_emulate || is_subrange_ || !can_optimize_) {
    /* Ensure we are not optimized. */
    BLI_assert(this->optimized_ibo_ == nullptr);

    /* Return regular index buffer. */
    BLI_assert(this->ibo_ && this->ibo_->get_metal_buffer());
    return this->ibo_->get_metal_buffer();
  }

  /* Perform optimization on type. */
  GPUPrimType input_prim_type = in_out_primitive_type;
  this->upload_data();
  if (!ibo_ && optimized_ibo_ == nullptr) {
    /* Cannot optimize buffer if no source IBO exists. */
    return nil;
  }

  /* Verify whether existing index buffer is valid. */
  if (optimized_ibo_ != nullptr && optimized_primitive_type_ != input_prim_type) {
    BLI_assert_msg(false,
                   "Cannot change the optimized primitive format after generation, as source "
                   "index buffer data is discarded.");
    return nil;
  }

  /* Generate optimized index buffer. */
  if (optimized_ibo_ == nullptr) {

    /* Generate unwrapped index buffer. */
    switch (input_prim_type) {
      case GPU_PRIM_TRI_FAN: {

        /* Calculate maximum size. */
        uint32_t max_possible_verts = (this->index_len_ - 2) * 3;
        BLI_assert(max_possible_verts > 0);

        /* Allocate new buffer. */
        optimized_ibo_ = MTLContext::get_global_memory_manager()->allocate(
            max_possible_verts *
                ((index_type_ == GPU_INDEX_U16) ? sizeof(uint16_t) : sizeof(uint32_t)),
            true);

        /* Populate new index buffer. */
        if (index_type_ == GPU_INDEX_U16) {
          Span<uint16_t> orig_data(static_cast<const uint16_t *>(ibo_->get_host_ptr()),
                                   this->index_len_);
          MutableSpan<uint16_t> output_data(
              static_cast<uint16_t *>(optimized_ibo_->get_host_ptr()), max_possible_verts);
          emulated_v_count = populate_emulated_tri_fan_buf<uint16_t>(
              orig_data, output_data, this->index_len_);
        }
        else {
          Span<uint32_t> orig_data(static_cast<const uint32_t *>(ibo_->get_host_ptr()),
                                   this->index_len_);
          MutableSpan<uint32_t> output_data(
              static_cast<uint32_t *>(optimized_ibo_->get_host_ptr()), max_possible_verts);
          emulated_v_count = populate_emulated_tri_fan_buf<uint32_t>(
              orig_data, output_data, this->index_len_);
        }

        BLI_assert(emulated_v_count <= max_possible_verts);

        /* Flush buffer and output. */
        optimized_ibo_->flush();
        optimized_primitive_type_ = input_prim_type;
        in_out_v_count = emulated_v_count;
        in_out_primitive_type = GPU_PRIM_TRIS;
      }

      case GPU_PRIM_TRI_STRIP: {

        /* Calculate maximum size. */
        uint32_t max_possible_verts = (this->index_len_ - 2) * 3;
        BLI_assert(max_possible_verts > 0);

        /* Allocate new buffer. */
        optimized_ibo_ = MTLContext::get_global_memory_manager()->allocate(
            max_possible_verts *
                ((index_type_ == GPU_INDEX_U16) ? sizeof(uint16_t) : sizeof(uint32_t)),
            true);

        /* Populate new index buffer. */
        if (index_type_ == GPU_INDEX_U16) {
          Span<uint16_t> orig_data(static_cast<const uint16_t *>(ibo_->get_host_ptr()),
                                   this->index_len_);
          MutableSpan<uint16_t> output_data(
              static_cast<uint16_t *>(optimized_ibo_->get_host_ptr()), max_possible_verts);
          emulated_v_count = populate_optimized_tri_strip_buf<uint16_t>(
              orig_data, output_data, this->index_len_);
        }
        else {
          Span<uint32_t> orig_data(static_cast<const uint32_t *>(ibo_->get_host_ptr()),
                                   this->index_len_);
          MutableSpan<uint32_t> output_data(
              static_cast<uint32_t *>(optimized_ibo_->get_host_ptr()), max_possible_verts);
          emulated_v_count = populate_optimized_tri_strip_buf<uint32_t>(
              orig_data, output_data, this->index_len_);
        }

        BLI_assert(emulated_v_count <= max_possible_verts);

        /* Flush buffer and output. */
        optimized_ibo_->flush();
        optimized_primitive_type_ = input_prim_type;
        in_out_v_count = emulated_v_count;
        in_out_primitive_type = GPU_PRIM_TRIS;
      } break;

      case GPU_PRIM_LINE_STRIP: {
        /* TODO(Metal): Line strip topology types would benefit from optimization to remove
         * primitive restarts, however, these do not occur frequently, nor with
         * significant geometry counts. */
        MTL_LOG_DEBUG("TODO: Primitive topology: Optimize line strip topology types");
      } break;

      case GPU_PRIM_LINE_LOOP: {
        /* TODO(Metal): Line Loop primitive type requires use of optimized index buffer for
         * emulation, if used with indexed rendering. This path is currently not hit as #LineLoop
         * does not currently appear to be used alongside an index buffer. */
        MTL_LOG_WARNING(
            "TODO: Primitive topology: Line Loop Index buffer optimization required for "
            "emulation.");
      } break;

      case GPU_PRIM_TRIS:
      case GPU_PRIM_LINES:
      case GPU_PRIM_POINTS: {
        /* Should not get here - TRIS/LINES/POINTS do not require emulation or optimization. */
        BLI_assert_unreachable();
        return nil;
      }

      default:
        /* Should not get here - Invalid primitive type. */
        BLI_assert_unreachable();
        break;
    }
  }

  /* Return optimized buffer. */
  if (optimized_ibo_ != nullptr) {

    /* Delete original buffer if one still exists, as we do no need it. */
    if (ibo_ != nullptr) {
      ibo_->free();
      ibo_ = nullptr;
    }

    /* Output parameters. */
    in_out_v_count = emulated_v_count;
    in_out_primitive_type = GPU_PRIM_TRIS;
    return optimized_ibo_->get_metal_buffer();
  }
  return nil;
}

void MTLIndexBuf::strip_restart_indices()
{
  /* We remove point buffer primitive restart indices by swapping restart indices
   * with the first valid index at the end of the index buffer and reducing the
   * length. Primitive restarts are invalid in Metal for non-restart-compatible
   * primitive types. We also cannot just use zero unlike for Lines and Triangles,
   * as we cannot create de-generative point primitives to hide geometry, as each
   * point is independent.
   * Instead, we must remove these hidden indices from the index buffer.
   * NOTE: This happens prior to index squeezing so operate on 32-bit indices. */
  MutableSpan<uint32_t> uint_idx(static_cast<uint32_t *>(data_), index_len_);
  for (uint i = 0; i < index_len_; i++) {
    if (uint_idx[i] == 0xFFFFFFFFu) {

      /* Find swap index at end of index buffer. */
      int swap_index = -1;
      for (uint j = index_len_ - 1; j >= i && index_len_ > 0; j--) {
        /* If end index is restart, just reduce length. */
        if (uint_idx[j] == 0xFFFFFFFFu) {
          index_len_--;
          continue;
        }
        /* Otherwise assign swap index. */
        swap_index = j;
        break;
      }

      /* If index_len_ == 0, this means all indices were flagged as hidden, with restart index
       * values. Hence we will entirely skip the draw. */
      if (index_len_ > 0) {
        /* If swap index is not valid, then there were no valid non-restart indices
         * to swap with. However, the above loop will have removed these indices by
         * reducing the length of indices. Debug assertions verify that the restart
         * index is no longer included. */
        if (swap_index == -1) {
          BLI_assert(index_len_ <= i);
        }
        else {
          /* If we have found an index we can swap with, flip the values.
           * We also reduce the length. As per above loop, swap_index should
           * now be outside the index length range. */
          uint32_t swap_index_value = uint_idx[swap_index];
          uint_idx[i] = swap_index_value;
          uint_idx[swap_index] = 0xFFFFFFFFu;
          index_len_--;
          BLI_assert(index_len_ <= swap_index);
        }
      }
    }
  }

#ifndef NDEBUG
  /* Flag as having been stripped to ensure invalid usage is tracked. */
  point_restarts_stripped_ = true;
#endif
}

/** \} */

}  // namespace blender::gpu
