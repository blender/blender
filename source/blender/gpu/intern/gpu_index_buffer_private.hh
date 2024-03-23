/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_assert.h"

#include "GPU_index_buffer.hh"

#define GPU_TRACK_INDEX_RANGE 1

namespace blender::gpu {

enum GPUIndexBufType {
  GPU_INDEX_U16,
  GPU_INDEX_U32,
};

static inline size_t to_bytesize(GPUIndexBufType type)
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
  IndexBuf(){};
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
  inline uint index_range(uint *r_min, uint *r_max);
  virtual void strip_restart_indices() = 0;
};

/* Syntactic sugar. */
static inline GPUIndexBuf *wrap(IndexBuf *indexbuf)
{
  return reinterpret_cast<GPUIndexBuf *>(indexbuf);
}
static inline IndexBuf *unwrap(GPUIndexBuf *indexbuf)
{
  return reinterpret_cast<IndexBuf *>(indexbuf);
}
static inline const IndexBuf *unwrap(const GPUIndexBuf *indexbuf)
{
  return reinterpret_cast<const IndexBuf *>(indexbuf);
}

static inline int indices_per_primitive(GPUPrimType prim_type)
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
    default:
      return -1;
  }
}

}  // namespace blender::gpu
