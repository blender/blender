/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU vertex buffer
 */

#pragma once

#include "BLI_enum_flags.hh"
#include "BLI_math_base.h"
#include "BLI_span.hh"
#include "BLI_virtual_array.hh"

#include "GPU_common.hh"
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

ENUM_OPERATORS(GPUVertBufStatus)

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

ENUM_OPERATORS(GPUUsageType);

namespace blender::gpu {
class VertBuf;
}  // namespace blender::gpu

void GPU_vertbuf_discard(blender::gpu::VertBuf *);

blender::gpu::VertBuf *GPU_vertbuf_calloc();
blender::gpu::VertBuf *GPU_vertbuf_create_with_format_ex(const GPUVertFormat &format,
                                                         GPUUsageType usage);

static inline blender::gpu::VertBuf *GPU_vertbuf_create_with_format(const GPUVertFormat &format)
{
  return GPU_vertbuf_create_with_format_ex(format, GPU_USAGE_STATIC);
}

namespace blender::gpu {

class VertBufDeleter {
 public:
  void operator()(VertBuf *vbo)
  {
    GPU_vertbuf_discard(vbo);
  }
};

using VertBufPtr = std::unique_ptr<gpu::VertBuf, gpu::VertBufDeleter>;

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

#ifndef NDEBUG
  /** Usage including extended usage flags. */
  GPUUsageType extended_usage_ = GPU_USAGE_STATIC;
#endif

 protected:
  /** NULL indicates data in VRAM (unmapped) */
  uchar *data_ = nullptr;

  /** Usage hint for GL optimization. */
  GPUUsageType usage_ = GPU_USAGE_STATIC;

 private:
  /** This counter will only avoid freeing the #VertBuf, not the data. */
  int handle_refcount_ = 1;

 public:
  VertBuf();
  virtual ~VertBuf();

  template<typename FormatT>
  static VertBufPtr from_size_with_format(const int size, GPUUsageType usage = GPU_USAGE_STATIC)
  {
    BLI_assert(size > 0);
    VertBufPtr buf = VertBufPtr(GPU_vertbuf_create_with_format_ex(FormatT::format(), usage));
    /* GPU formats needs to be aligned to 4 bytes. */
    buf->allocate(ceil_to_multiple_u(size * sizeof(FormatT), 4) / sizeof(FormatT));
    return buf;
  }

  template<typename T>
  static VertBufPtr from_size(const int size, GPUUsageType usage = GPU_USAGE_STATIC)
  {
    return from_size_with_format<GenericVertexFormat<T>>(size, usage);
  }

  template<typename T> static VertBufPtr from_span(const Span<T> data)
  {
    BLI_assert(!data.is_empty());
    VertBufPtr buf = VertBufPtr(GPU_vertbuf_create_with_format_ex(
        GenericVertexFormat<T>::format(), GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY));
    /* GPU formats needs to be aligned to 4 bytes. */
    buf->allocate(ceil_to_multiple_u(data.size_in_bytes(), 4) / sizeof(GenericVertexFormat<T>));
    buf->data<T>().slice(0, data.size()).copy_from(data);
    return buf;
  }

  template<typename T> static VertBufPtr from_varray(const VArray<T> &array)
  {
    BLI_assert(!array.is_empty());
    VertBufPtr buf = VertBufPtr(GPU_vertbuf_create_with_format_ex(
        GenericVertexFormat<T>::format(), GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY));
    /* GPU formats needs to be aligned to 4 bytes. */
    buf->allocate(ceil_to_multiple_u(array.size() * sizeof(T), 4) /
                  sizeof(GenericVertexFormat<T>));
    array.materialize(buf->data<T>().slice(0, array.size()));
    return buf;
  }

  template<typename T> static VertBufPtr device_only(uint size)
  {
    BLI_assert(size > 0);
    VertBufPtr buf = VertBufPtr(GPU_vertbuf_create_with_format_ex(
        GenericVertexFormat<T>::format(),
        GPU_USAGE_DEVICE_ONLY | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY));
    buf->allocate(size);
    return buf;
  }

  void init(const GPUVertFormat &format, GPUUsageType usage);
  void clear();

  /* Data management. */
  void allocate(uint vert_len);
  void resize(uint vert_len);
  void upload();
  virtual void bind_as_ssbo(uint binding) = 0;
  virtual void bind_as_texture(uint binding) = 0;

  virtual void wrap_handle(uint64_t handle) = 0;

  /* Size of the data allocated. */
  size_t size_alloc_get() const
  {
    BLI_assert(this->format.packed);
    return size_t(this->vertex_alloc) * this->format.stride;
  }
  /* Size of the data uploaded to the GPU. */
  size_t size_used_get() const
  {
    BLI_assert(format.packed);
    return size_t(this->vertex_len) * this->format.stride;
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

  /**
   * Returns access to the data allocated for the vertex buffer. The size of the data type must
   * match the data type used on the GPU.
   */
  template<typename T> MutableSpan<T> data()
  {
    return MutableSpan<uchar>(data_, this->size_alloc_get()).cast<T>();
  }

  virtual void update_sub(uint start, uint len, const void *data) = 0;
  virtual void read(void *data) const = 0;

 protected:
  virtual void acquire_data() = 0;
  virtual void resize_data() = 0;
  virtual void release_data() = 0;
  virtual void upload_data() = 0;
};

}  // namespace blender::gpu

/**
 * (Download and) fill data with the data from the vertex buffer.
 * NOTE: caller is responsible to reserve enough memory of the data parameter.
 */
void GPU_vertbuf_read(const blender::gpu::VertBuf *verts, void *data);
/** Same as discard but does not free. */
void GPU_vertbuf_clear(blender::gpu::VertBuf *verts);

/**
 * Avoid blender::gpu::VertBuf data-block being free but not its data.
 */
void GPU_vertbuf_handle_ref_add(blender::gpu::VertBuf *verts);
void GPU_vertbuf_handle_ref_remove(blender::gpu::VertBuf *verts);

void GPU_vertbuf_init_with_format_ex(blender::gpu::VertBuf &verts,
                                     const GPUVertFormat &format,
                                     GPUUsageType);

void GPU_vertbuf_init_build_on_device(blender::gpu::VertBuf &verts,
                                      const GPUVertFormat &format,
                                      uint v_len);

blender::gpu::VertBuf *GPU_vertbuf_create_on_device(const GPUVertFormat &format, uint v_len);

#define GPU_vertbuf_init_with_format(verts, format) \
  GPU_vertbuf_init_with_format_ex(verts, format, GPU_USAGE_STATIC)

/**
 * Create a new allocation, discarding any existing data.
 */
void GPU_vertbuf_data_alloc(blender::gpu::VertBuf &verts, uint v_len);
/**
 * Resize buffer keeping existing data.
 */
void GPU_vertbuf_data_resize(blender::gpu::VertBuf &verts, uint v_len);
/**
 * Set vertex count but does not change allocation.
 * Only this many verts will be uploaded to the GPU and rendered.
 * This is useful for streaming data.
 */
void GPU_vertbuf_data_len_set(blender::gpu::VertBuf &verts, uint v_len);

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
 *
 * \note This is obsolete, use #VertBuf::data<T>() instead.
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

GPU_INLINE uint GPU_vertbuf_raw_used(const GPUVertBufRaw *a)
{
  return ((a->data - a->data_init) / a->stride);
}

void GPU_vertbuf_attr_get_raw_data(blender::gpu::VertBuf *, uint a_idx, GPUVertBufRaw *access);

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
