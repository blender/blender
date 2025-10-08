/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU index buffer
 */

#pragma once

#include "BLI_span.hh"

#include "GPU_primitive.hh"

#define GPU_TRACK_INDEX_RANGE 1

namespace blender::gpu {

/** Value for invisible elements in a #GPU_PRIM_POINTS index buffer. */
constexpr uint32_t RESTART_INDEX = 0xFFFFFFFF;

enum GPUIndexBufType {
  GPU_INDEX_U16,
  GPU_INDEX_U32,
};

inline size_t to_bytesize(GPUIndexBufType type)
{
  return (type == GPU_INDEX_U32) ? sizeof(uint32_t) : sizeof(uint16_t);
}

/**
 * Base class which is then specialized for each implementation (GL, VK, ...).
 *
 * \note #IndexBuf does not hold any #GPUPrimType.
 * This is because it can be interpreted differently by multiple batches.
 */
class IndexBuf {
 protected:
  /** Type of indices used inside this buffer. */
  GPUIndexBufType index_type_ = GPU_INDEX_U32;
  /** Offset in this buffer to the first index to render. Is 0 if not a subrange. */
  uint32_t index_start_ = 0;
  /** Number of indices to render. */
  uint32_t index_len_ = 0;
  /** Base index: Added to all indices after fetching. Allows index compression. */
  uint32_t index_base_ = 0;
  /** Bookkeeping. */
  bool is_init_ = false;
  /** Is this object only a reference to a subrange of another IndexBuf. */
  bool is_subrange_ = false;
  /** True if buffer only contains restart indices. */
  bool is_empty_ = false;

  union {
    /** Mapped buffer data. non-NULL indicates not yet sent to VRAM. */
    void *data_ = nullptr;
    /** If is_subrange is true, this is the source index buffer. */
    IndexBuf *src_;
  };

 public:
  IndexBuf() {};
  virtual ~IndexBuf();

  void init(uint indices_len,
            uint32_t *indices,
            uint min_index,
            uint max_index,
            GPUPrimType prim_type,
            bool uses_restart_indices);
  void init_subrange(IndexBuf *elem_src, uint start, uint length);
  void init_build_on_device(uint index_len);

  /* Returns render index count (not precise). */
  uint32_t index_len_get() const
  {
    /* Return 0 to bypass drawing for index buffers full of restart indices.
     * They can lead to graphical glitches on some systems. (See #96892) */
    return is_empty_ ? 0 : index_len_;
  }
  uint32_t index_start_get() const
  {
    return index_start_;
  }
  uint32_t index_base_get() const
  {
    return index_base_;
  }
  bool is_32bit() const
  {
    return index_type_ == GPU_INDEX_U32;
  }
  /* Return size in byte of the drawable data buffer range. Actual buffer size might be bigger. */
  size_t size_get() const
  {
    return index_len_ * to_bytesize(index_type_);
  };

  bool is_init() const
  {
    return is_init_;
  };

  virtual void upload_data() = 0;

  virtual void bind_as_ssbo(uint binding) = 0;

  virtual void read(uint32_t *data) const = 0;

  virtual void update_sub(uint start, uint len, const void *data) = 0;

 private:
  inline void squeeze_indices_short(uint min_idx,
                                    uint max_idx,
                                    GPUPrimType prim_type,
                                    bool clamp_indices_in_range);
  virtual void strip_restart_indices() = 0;
};

inline int indices_per_primitive(GPUPrimType prim_type)
{
  switch (prim_type) {
    case GPU_PRIM_POINTS:
      return 1;
    case GPU_PRIM_LINES:
      return 2;
    case GPU_PRIM_TRIS:
      return 3;
    case GPU_PRIM_LINES_ADJ:
      return 4;
    case GPU_PRIM_TRIS_ADJ:
      return 6;
    /** IMPORTANT: These last two expects no restart primitive.
     * Asserting for this would be too slow. Just don't be stupid.
     * This is needed for polylines but should be deprecated.
     * See GPU_batch_draw_expanded_parameter_get */
    case GPU_PRIM_LINE_STRIP:
      return 1; /* Minus one for the whole length. */
    case GPU_PRIM_LINE_LOOP:
      return 1;
    default:
      return -1;
  }
}

}  // namespace blender::gpu

blender::gpu::IndexBuf *GPU_indexbuf_calloc();

struct GPUIndexBufBuilder {
  uint max_allowed_index;
  uint max_index_len;
  uint index_len;
  uint index_min;
  uint index_max;
  uint restart_index_value;
  bool uses_restart_indices;

  GPUPrimType prim_type;
  uint32_t *data;
};

/** Supports all primitive types. */
void GPU_indexbuf_init_ex(GPUIndexBufBuilder *, GPUPrimType, uint index_len, uint vertex_len);

/** Supports only #GPU_PRIM_POINTS, #GPU_PRIM_LINES and #GPU_PRIM_TRIS. */
void GPU_indexbuf_init(GPUIndexBufBuilder *, GPUPrimType, uint prim_len, uint vertex_len);
blender::gpu::IndexBuf *GPU_indexbuf_build_on_device(uint index_len);

void GPU_indexbuf_init_build_on_device(blender::gpu::IndexBuf *elem, uint index_len);

blender::MutableSpan<uint32_t> GPU_indexbuf_get_data(GPUIndexBufBuilder *);

/*
 * Thread safe.
 *
 * Function inspired by the reduction directives of multi-thread work API's.
 */
void GPU_indexbuf_join(GPUIndexBufBuilder *builder, const GPUIndexBufBuilder *builder_from);

void GPU_indexbuf_add_generic_vert(GPUIndexBufBuilder *, uint v);
void GPU_indexbuf_add_primitive_restart(GPUIndexBufBuilder *);

void GPU_indexbuf_add_point_vert(GPUIndexBufBuilder *, uint v);
void GPU_indexbuf_add_line_verts(GPUIndexBufBuilder *, uint v1, uint v2);
void GPU_indexbuf_add_tri_verts(GPUIndexBufBuilder *, uint v1, uint v2, uint v3);
void GPU_indexbuf_add_line_adj_verts(GPUIndexBufBuilder *, uint v1, uint v2, uint v3, uint v4);

void GPU_indexbuf_set_point_vert(GPUIndexBufBuilder *builder, uint elem, uint v1);
void GPU_indexbuf_set_line_verts(GPUIndexBufBuilder *builder, uint elem, uint v1, uint v2);
void GPU_indexbuf_set_tri_verts(GPUIndexBufBuilder *builder, uint elem, uint v1, uint v2, uint v3);

/* Skip primitive rendering at the given index. */
void GPU_indexbuf_set_point_restart(GPUIndexBufBuilder *builder, uint elem);
void GPU_indexbuf_set_line_restart(GPUIndexBufBuilder *builder, uint elem);
void GPU_indexbuf_set_tri_restart(GPUIndexBufBuilder *builder, uint elem);

blender::gpu::IndexBuf *GPU_indexbuf_build(GPUIndexBufBuilder *);
blender::gpu::IndexBuf *GPU_indexbuf_build_ex(GPUIndexBufBuilder *builder,
                                              uint index_min,
                                              uint index_max,
                                              bool uses_restart_indices);
void GPU_indexbuf_build_in_place(GPUIndexBufBuilder *, blender::gpu::IndexBuf *);
void GPU_indexbuf_build_in_place_ex(GPUIndexBufBuilder *builder,
                                    uint index_min,
                                    uint index_max,
                                    bool uses_restart_indices,
                                    blender::gpu::IndexBuf *elem);

/**
 * Fill an IBO by uploading the referenced data directly to the GPU, bypassing the separate storage
 * in the IBO. This should be used whenever the equivalent indices already exist in a contiguous
 * array on the CPU.
 *
 * \todo The optimization to avoid the local copy currently isn't implemented.
 */
blender::gpu::IndexBuf *GPU_indexbuf_build_from_memory(GPUPrimType prim_type,
                                                       const uint32_t *data,
                                                       int32_t data_len,
                                                       int32_t index_min,
                                                       int32_t index_max,
                                                       bool uses_restart_indices);

/**
 * \note Sub-ranges are not taken into account, the whole buffer will be bound without any offset.
 */
void GPU_indexbuf_bind_as_ssbo(blender::gpu::IndexBuf *elem, int binding);

blender::gpu::IndexBuf *GPU_indexbuf_build_curves_on_device(GPUPrimType prim_type,
                                                            uint curves_num,
                                                            uint verts_per_curve);

/* Upload data to the GPU (if not built on the device) and bind the buffer to its default target.
 */
void GPU_indexbuf_use(blender::gpu::IndexBuf *elem);

/* Partially update the blender::gpu::IndexBuf which was already sent to the device, or built
 * directly on the device. The data needs to be compatible with potential compression applied to
 * the original indices when the index buffer was built, i.e., if the data was compressed to use
 * shorts instead of ints, shorts should passed here. */
void GPU_indexbuf_update_sub(blender::gpu::IndexBuf *elem, uint start, uint len, const void *data);

/* Create a sub-range of an existing index-buffer. */
blender::gpu::IndexBuf *GPU_indexbuf_create_subrange(blender::gpu::IndexBuf *elem_src,
                                                     uint start,
                                                     uint length);
void GPU_indexbuf_create_subrange_in_place(blender::gpu::IndexBuf *elem,
                                           blender::gpu::IndexBuf *elem_src,
                                           uint start,
                                           uint length);

/**
 * (Download and) fill data with the contents of the index buffer.
 *
 * NOTE: caller is responsible to reserve enough memory.
 */
void GPU_indexbuf_read(blender::gpu::IndexBuf *elem, uint32_t *data);

void GPU_indexbuf_discard(blender::gpu::IndexBuf *elem);

bool GPU_indexbuf_is_init(blender::gpu::IndexBuf *elem);

int GPU_indexbuf_primitive_len(GPUPrimType prim_type);

/* Macros */

#define GPU_INDEXBUF_DISCARD_SAFE(elem) \
  do { \
    if (elem != nullptr) { \
      GPU_indexbuf_discard(elem); \
      elem = nullptr; \
    } \
  } while (0)

namespace blender::gpu {

class IndexBufDeleter {
 public:
  void operator()(IndexBuf *ibo)
  {
    GPU_indexbuf_discard(ibo);
  }
};

using IndexBufPtr = std::unique_ptr<IndexBuf, IndexBufDeleter>;

}  // namespace blender::gpu
