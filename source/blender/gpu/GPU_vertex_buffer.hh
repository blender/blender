/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU vertex buffer
 */

#pragma once

#include "BLI_utildefines.h"

#include "GPU_vertex_format.hh"

enum GPUVertBufStatus {
  /** Initial state. */
  GPU_VERTBUF_INVALID = 0,
  /** Was init with a vertex format. */
  GPU_VERTBUF_INIT = (1 << 0),
  /** Data has been touched and need to be re-uploaded. */
  GPU_VERTBUF_DATA_DIRTY = (1 << 1),
  /** The buffer has been created inside GPU memory. */
  GPU_VERTBUF_DATA_UPLOADED = (1 << 2),
};

ENUM_OPERATORS(GPUVertBufStatus, GPU_VERTBUF_DATA_UPLOADED)

/**
 * How to create a #VertBuf:
 * 1) verts = GPU_vertbuf_calloc()
 * 2) GPU_vertformat_attr_add(verts->format, ...)
 * 3) GPU_vertbuf_data_alloc(verts, vertex_len) <-- finalizes/packs vertex format
 * 4) GPU_vertbuf_attr_fill(verts, pos, application_pos_buffer)
 */

enum GPUUsageType {
  /* can be extended to support more types */
  GPU_USAGE_STREAM = 0,
  GPU_USAGE_STATIC = 1, /* do not keep data in memory */
  GPU_USAGE_DYNAMIC = 2,
  GPU_USAGE_DEVICE_ONLY = 3, /* Do not do host->device data transfers. */

  /** Extended usage flags. */
  /* Flag for vertex buffers used for textures. Skips additional padding/compaction to ensure
   * format matches the texture exactly. Can be masked with other properties, and is stripped
   * during VertBuf::init. */
  GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY = 1 << 3,
};

ENUM_OPERATORS(GPUUsageType, GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

namespace blender::gpu {

/**
 * Implementation of Vertex Buffers.
 * Base class which is then specialized for each implementation (GL, VK, ...).
 */
class VertBuf {
 public:
  static size_t memory_usage;

  GPUVertFormat format = {};
  /** Number of verts we want to draw. */
  uint vertex_len = 0;
  /** Number of verts data. */
  uint vertex_alloc = 0;
  /** Status flag. */
  GPUVertBufStatus flag = GPU_VERTBUF_INVALID;
  /** NULL indicates data in VRAM (unmapped) */
  uchar *data = nullptr;

#ifndef NDEBUG
  /** Usage including extended usage flags. */
  GPUUsageType extended_usage_ = GPU_USAGE_STATIC;
#endif

 protected:
  /** Usage hint for GL optimization. */
  GPUUsageType usage_ = GPU_USAGE_STATIC;

 private:
  /** This counter will only avoid freeing the #VertBuf, not the data. */
  int handle_refcount_ = 1;

 public:
  VertBuf();
  virtual ~VertBuf();

  void init(const GPUVertFormat *format, GPUUsageType usage);
  void clear();

  /* Data management. */
  void allocate(uint vert_len);
  void resize(uint vert_len);
  void upload();
  virtual void bind_as_ssbo(uint binding) = 0;
  virtual void bind_as_texture(uint binding) = 0;

  virtual void wrap_handle(uint64_t handle) = 0;

  VertBuf *duplicate();

  /* Size of the data allocated. */
  size_t size_alloc_get() const
  {
    BLI_assert(format.packed);
    return vertex_alloc * format.stride;
  }
  /* Size of the data uploaded to the GPU. */
  size_t size_used_get() const
  {
    BLI_assert(format.packed);
    return vertex_len * format.stride;
  }

  void reference_add()
  {
    handle_refcount_++;
  }
  void reference_remove()
  {
    BLI_assert(handle_refcount_ > 0);
    handle_refcount_--;
    if (handle_refcount_ == 0) {
      delete this;
    }
  }

  GPUUsageType get_usage_type() const
  {
    return usage_;
  }

  virtual void update_sub(uint start, uint len, const void *data) = 0;
  virtual void read(void *data) const = 0;

 protected:
  virtual void acquire_data() = 0;
  virtual void resize_data() = 0;
  virtual void release_data() = 0;
  virtual void upload_data() = 0;
  virtual void duplicate_data(VertBuf *dst) = 0;
};

}  // namespace blender::gpu

blender::gpu::VertBuf *GPU_vertbuf_calloc();
blender::gpu::VertBuf *GPU_vertbuf_create_with_format_ex(const GPUVertFormat *, GPUUsageType);

#define GPU_vertbuf_create_with_format(format) \
  GPU_vertbuf_create_with_format_ex(format, GPU_USAGE_STATIC)

/**
 * (Download and) fill data with the data from the vertex buffer.
 * NOTE: caller is responsible to reserve enough memory of the data parameter.
 */
void GPU_vertbuf_read(blender::gpu::VertBuf *verts, void *data);
/** Same as discard but does not free. */
void GPU_vertbuf_clear(blender::gpu::VertBuf *verts);
void GPU_vertbuf_discard(blender::gpu::VertBuf *);

/**
 * Avoid blender::gpu::VertBuf data-block being free but not its data.
 */
void GPU_vertbuf_handle_ref_add(blender::gpu::VertBuf *verts);
void GPU_vertbuf_handle_ref_remove(blender::gpu::VertBuf *verts);

void GPU_vertbuf_init_with_format_ex(blender::gpu::VertBuf *, const GPUVertFormat *, GPUUsageType);

void GPU_vertbuf_init_build_on_device(blender::gpu::VertBuf *verts,
                                      GPUVertFormat *format,
                                      uint v_len);

#define GPU_vertbuf_init_with_format(verts, format) \
  GPU_vertbuf_init_with_format_ex(verts, format, GPU_USAGE_STATIC)

blender::gpu::VertBuf *GPU_vertbuf_duplicate(blender::gpu::VertBuf *verts);

/**
 * Create a new allocation, discarding any existing data.
 */
void GPU_vertbuf_data_alloc(blender::gpu::VertBuf *, uint v_len);
/**
 * Resize buffer keeping existing data.
 */
void GPU_vertbuf_data_resize(blender::gpu::VertBuf *, uint v_len);
/**
 * Set vertex count but does not change allocation.
 * Only this many verts will be uploaded to the GPU and rendered.
 * This is useful for streaming data.
 */
void GPU_vertbuf_data_len_set(blender::gpu::VertBuf *, uint v_len);

/**
 * The most important #set_attr variant is the untyped one. Get it right first.
 * It takes a void* so the app developer is responsible for matching their app data types
 * to the vertex attribute's type and component count. They're in control of both, so this
 * should not be a problem.
 */
void GPU_vertbuf_attr_set(blender::gpu::VertBuf *, uint a_idx, uint v_idx, const void *data);

/** Fills a whole vertex (all attributes). Data must match packed layout. */
void GPU_vertbuf_vert_set(blender::gpu::VertBuf *verts, uint v_idx, const void *data);

/**
 * Tightly packed, non interleaved input data.
 */
void GPU_vertbuf_attr_fill(blender::gpu::VertBuf *, uint a_idx, const void *data);

void GPU_vertbuf_attr_fill_stride(blender::gpu::VertBuf *,
                                  uint a_idx,
                                  uint stride,
                                  const void *data);

/**
 * For low level access only.
 */
struct GPUVertBufRaw {
  uint size;
  uint stride;
  unsigned char *data;
  unsigned char *data_init;
#ifndef NDEBUG
  /* Only for overflow check */
  unsigned char *_data_end;
#endif
};

GPU_INLINE void *GPU_vertbuf_raw_step(GPUVertBufRaw *a)
{
  unsigned char *data = a->data;
  a->data += a->stride;
#ifndef NDEBUG
  BLI_assert(data < a->_data_end);
#endif
  return (void *)data;
}

GPU_INLINE uint GPU_vertbuf_raw_used(GPUVertBufRaw *a)
{
  return ((a->data - a->data_init) / a->stride);
}

void GPU_vertbuf_attr_get_raw_data(blender::gpu::VertBuf *, uint a_idx, GPUVertBufRaw *access);

/**
 * Returns the data buffer and set it to null internally to avoid freeing.
 * \note Be careful when using this. The data needs to match the expected format.
 */
void *GPU_vertbuf_steal_data(blender::gpu::VertBuf *verts);

/**
 * \note Be careful when using this. The data needs to match the expected format.
 */
void *GPU_vertbuf_get_data(const blender::gpu::VertBuf *verts);
const GPUVertFormat *GPU_vertbuf_get_format(const blender::gpu::VertBuf *verts);
uint GPU_vertbuf_get_vertex_alloc(const blender::gpu::VertBuf *verts);
uint GPU_vertbuf_get_vertex_len(const blender::gpu::VertBuf *verts);
GPUVertBufStatus GPU_vertbuf_get_status(const blender::gpu::VertBuf *verts);
void GPU_vertbuf_tag_dirty(blender::gpu::VertBuf *verts);

/**
 * Should be rename to #GPU_vertbuf_data_upload.
 */
void GPU_vertbuf_use(blender::gpu::VertBuf *);
void GPU_vertbuf_bind_as_ssbo(blender::gpu::VertBuf *verts, int binding);
void GPU_vertbuf_bind_as_texture(blender::gpu::VertBuf *verts, int binding);

void GPU_vertbuf_wrap_handle(blender::gpu::VertBuf *verts, uint64_t handle);

/**
 * XXX: do not use!
 * This is just a wrapper for the use of the Hair refine workaround.
 * To be used with #GPU_vertbuf_use().
 */
void GPU_vertbuf_update_sub(blender::gpu::VertBuf *verts, uint start, uint len, const void *data);

/* Metrics */
uint GPU_vertbuf_get_memory_usage();

/* Macros */
#define GPU_VERTBUF_DISCARD_SAFE(verts) \
  do { \
    if (verts != nullptr) { \
      GPU_vertbuf_discard(verts); \
      verts = nullptr; \
    } \
  } while (0)
