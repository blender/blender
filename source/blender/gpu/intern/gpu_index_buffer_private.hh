/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_assert.h"

#include "GPU_index_buffer.h"

#define GPU_TRACK_INDEX_RANGE 1

namespace blender::gpu {

typedef enum {
  GPU_INDEX_U16,
  GPU_INDEX_U32,
} GPUIndexBufType;

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
  /** Offset in this buffer to the first index to render. Is 0 if not a subrange.  */
  uint32_t index_start_ = 0;
  /** Number of indices to render. */
  uint32_t index_len_ = 0;
  /** Base index: Added to all indices after fetching. Allows index compression. */
  uint32_t index_base_ = 0;
  /** Bookeeping. */
  bool is_init_ = false;
  /** Is this object only a reference to a subrange of another IndexBuf. */
  bool is_subrange_ = false;

  union {
    /** Mapped buffer data. non-NULL indicates not yet sent to VRAM. */
    void *data_ = nullptr;
    /** If is_subrange is true, this is the source index buffer. */
    IndexBuf *src_;
  };

 public:
  IndexBuf(){};
  virtual ~IndexBuf();

  void init(uint indices_len, uint32_t *indices);
  void init_subrange(IndexBuf *elem_src, uint start, uint length);

  uint32_t index_len_get(void) const
  {
    return index_len_;
  }
  /* Return size in byte of the drawable data buffer range. Actual buffer size might be bigger. */
  size_t size_get(void)
  {
    return index_len_ * to_bytesize(index_type_);
  };

  bool is_init(void) const
  {
    return is_init_;
  };

 private:
  inline void squeeze_indices_short(uint min_idx, uint max_idx);
  inline uint index_range(uint *r_min, uint *r_max);
};

/* Syntacting suggar. */
static inline GPUIndexBuf *wrap(IndexBuf *indexbuf)
{
  return reinterpret_cast<GPUIndexBuf *>(indexbuf);
}
static inline IndexBuf *unwrap(GPUIndexBuf *indexbuf)
{
  return reinterpret_cast<IndexBuf *>(indexbuf);
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
