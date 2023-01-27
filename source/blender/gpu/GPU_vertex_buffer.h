/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 by Mike Erwin. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * GPU vertex buffer
 */

#pragma once

#include "BLI_utildefines.h"

#include "GPU_vertex_format.h"

typedef enum {
  /** Initial state. */
  GPU_VERTBUF_INVALID = 0,
  /** Was init with a vertex format. */
  GPU_VERTBUF_INIT = (1 << 0),
  /** Data has been touched and need to be re-uploaded. */
  GPU_VERTBUF_DATA_DIRTY = (1 << 1),
  /** The buffer has been created inside GPU memory. */
  GPU_VERTBUF_DATA_UPLOADED = (1 << 2),
} GPUVertBufStatus;

ENUM_OPERATORS(GPUVertBufStatus, GPU_VERTBUF_DATA_UPLOADED)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * How to create a #GPUVertBuf:
 * 1) verts = GPU_vertbuf_calloc()
 * 2) GPU_vertformat_attr_add(verts->format, ...)
 * 3) GPU_vertbuf_data_alloc(verts, vertex_len) <-- finalizes/packs vertex format
 * 4) GPU_vertbuf_attr_fill(verts, pos, application_pos_buffer)
 */

typedef enum {
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
} GPUUsageType;

ENUM_OPERATORS(GPUUsageType, GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

/** Opaque type hiding blender::gpu::VertBuf. */
typedef struct GPUVertBuf GPUVertBuf;

GPUVertBuf *GPU_vertbuf_calloc(void);
GPUVertBuf *GPU_vertbuf_create_with_format_ex(const GPUVertFormat *, GPUUsageType);

#define GPU_vertbuf_create_with_format(format) \
  GPU_vertbuf_create_with_format_ex(format, GPU_USAGE_STATIC)

/**
 * (Download and) return a pointer containing the data of a vertex buffer.
 *
 * Note that the returned pointer is still owned by the driver. To get an
 * local copy, use `GPU_vertbuf_unmap` after calling `GPU_vertbuf_read`.
 */
const void *GPU_vertbuf_read(GPUVertBuf *verts);
void *GPU_vertbuf_unmap(const GPUVertBuf *verts, const void *mapped_data);
/** Same as discard but does not free. */
void GPU_vertbuf_clear(GPUVertBuf *verts);
void GPU_vertbuf_discard(GPUVertBuf *);

/**
 * Avoid GPUVertBuf data-block being free but not its data.
 */
void GPU_vertbuf_handle_ref_add(GPUVertBuf *verts);
void GPU_vertbuf_handle_ref_remove(GPUVertBuf *verts);

void GPU_vertbuf_init_with_format_ex(GPUVertBuf *, const GPUVertFormat *, GPUUsageType);

void GPU_vertbuf_init_build_on_device(GPUVertBuf *verts, GPUVertFormat *format, uint v_len);

#define GPU_vertbuf_init_with_format(verts, format) \
  GPU_vertbuf_init_with_format_ex(verts, format, GPU_USAGE_STATIC)

GPUVertBuf *GPU_vertbuf_duplicate(GPUVertBuf *verts);

/**
 * Create a new allocation, discarding any existing data.
 */
void GPU_vertbuf_data_alloc(GPUVertBuf *, uint v_len);
/**
 * Resize buffer keeping existing data.
 */
void GPU_vertbuf_data_resize(GPUVertBuf *, uint v_len);
/**
 * Set vertex count but does not change allocation.
 * Only this many verts will be uploaded to the GPU and rendered.
 * This is useful for streaming data.
 */
void GPU_vertbuf_data_len_set(GPUVertBuf *, uint v_len);

/**
 * The most important #set_attr variant is the untyped one. Get it right first.
 * It takes a void* so the app developer is responsible for matching their app data types
 * to the vertex attribute's type and component count. They're in control of both, so this
 * should not be a problem.
 */
void GPU_vertbuf_attr_set(GPUVertBuf *, uint a_idx, uint v_idx, const void *data);

/** Fills a whole vertex (all attributes). Data must match packed layout. */
void GPU_vertbuf_vert_set(GPUVertBuf *verts, uint v_idx, const void *data);

/**
 * Tightly packed, non interleaved input data.
 */
void GPU_vertbuf_attr_fill(GPUVertBuf *, uint a_idx, const void *data);

void GPU_vertbuf_attr_fill_stride(GPUVertBuf *, uint a_idx, uint stride, const void *data);

/**
 * For low level access only.
 */
typedef struct GPUVertBufRaw {
  uint size;
  uint stride;
  unsigned char *data;
  unsigned char *data_init;
#ifdef DEBUG
  /* Only for overflow check */
  unsigned char *_data_end;
#endif
} GPUVertBufRaw;

GPU_INLINE void *GPU_vertbuf_raw_step(GPUVertBufRaw *a)
{
  unsigned char *data = a->data;
  a->data += a->stride;
#ifdef DEBUG
  BLI_assert(data < a->_data_end);
#endif
  return (void *)data;
}

GPU_INLINE uint GPU_vertbuf_raw_used(GPUVertBufRaw *a)
{
  return ((a->data - a->data_init) / a->stride);
}

void GPU_vertbuf_attr_get_raw_data(GPUVertBuf *, uint a_idx, GPUVertBufRaw *access);

/**
 * Returns the data buffer and set it to null internally to avoid freeing.
 * \note Be careful when using this. The data needs to match the expected format.
 */
void *GPU_vertbuf_steal_data(GPUVertBuf *verts);

/**
 * \note Be careful when using this. The data needs to match the expected format.
 */
void *GPU_vertbuf_get_data(const GPUVertBuf *verts);
const GPUVertFormat *GPU_vertbuf_get_format(const GPUVertBuf *verts);
uint GPU_vertbuf_get_vertex_alloc(const GPUVertBuf *verts);
uint GPU_vertbuf_get_vertex_len(const GPUVertBuf *verts);
GPUVertBufStatus GPU_vertbuf_get_status(const GPUVertBuf *verts);
void GPU_vertbuf_tag_dirty(GPUVertBuf *verts);

/**
 * Should be rename to #GPU_vertbuf_data_upload.
 */
void GPU_vertbuf_use(GPUVertBuf *);
void GPU_vertbuf_bind_as_ssbo(struct GPUVertBuf *verts, int binding);
void GPU_vertbuf_bind_as_texture(struct GPUVertBuf *verts, int binding);

void GPU_vertbuf_wrap_handle(GPUVertBuf *verts, uint64_t handle);

/**
 * XXX: do not use!
 * This is just a wrapper for the use of the Hair refine workaround.
 * To be used with #GPU_vertbuf_use().
 */
void GPU_vertbuf_update_sub(GPUVertBuf *verts, uint start, uint len, const void *data);

/* Metrics */
uint GPU_vertbuf_get_memory_usage(void);

/* Macros */
#define GPU_VERTBUF_DISCARD_SAFE(verts) \
  do { \
    if (verts != NULL) { \
      GPU_vertbuf_discard(verts); \
      verts = NULL; \
    } \
  } while (0)

#ifdef __cplusplus
}
#endif
