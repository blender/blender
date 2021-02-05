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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

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
  GPU_USAGE_STREAM,
  GPU_USAGE_STATIC, /* do not keep data in memory */
  GPU_USAGE_DYNAMIC,
} GPUUsageType;

/** Opaque type hiding blender::gpu::VertBuf. */
typedef struct GPUVertBuf GPUVertBuf;

GPUVertBuf *GPU_vertbuf_calloc(void);
GPUVertBuf *GPU_vertbuf_create_with_format_ex(const GPUVertFormat *, GPUUsageType);

#define GPU_vertbuf_create_with_format(format) \
  GPU_vertbuf_create_with_format_ex(format, GPU_USAGE_STATIC)

void GPU_vertbuf_clear(GPUVertBuf *verts);
void GPU_vertbuf_discard(GPUVertBuf *);

/* Avoid GPUVertBuf datablock being free but not its data. */
void GPU_vertbuf_handle_ref_add(GPUVertBuf *verts);
void GPU_vertbuf_handle_ref_remove(GPUVertBuf *verts);

void GPU_vertbuf_init_with_format_ex(GPUVertBuf *, const GPUVertFormat *, GPUUsageType);

#define GPU_vertbuf_init_with_format(verts, format) \
  GPU_vertbuf_init_with_format_ex(verts, format, GPU_USAGE_STATIC)

GPUVertBuf *GPU_vertbuf_duplicate(GPUVertBuf *verts);

void GPU_vertbuf_data_alloc(GPUVertBuf *, uint v_len);
void GPU_vertbuf_data_resize(GPUVertBuf *, uint v_len);
void GPU_vertbuf_data_len_set(GPUVertBuf *, uint v_len);

/* The most important #set_attr variant is the untyped one. Get it right first.
 * It takes a void* so the app developer is responsible for matching their app data types
 * to the vertex attribute's type and component count. They're in control of both, so this
 * should not be a problem. */

void GPU_vertbuf_attr_set(GPUVertBuf *, uint a_idx, uint v_idx, const void *data);

void GPU_vertbuf_vert_set(GPUVertBuf *verts, uint v_idx, const void *data);

/* Tightly packed, non interleaved input data. */
void GPU_vertbuf_attr_fill(GPUVertBuf *, uint a_idx, const void *data);

void GPU_vertbuf_attr_fill_stride(GPUVertBuf *, uint a_idx, uint stride, const void *data);

/* For low level access only */
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
  BLI_assert(data < a->_data_end);
  return (void *)data;
}

GPU_INLINE uint GPU_vertbuf_raw_used(GPUVertBufRaw *a)
{
  return ((a->data - a->data_init) / a->stride);
}

void GPU_vertbuf_attr_get_raw_data(GPUVertBuf *, uint a_idx, GPUVertBufRaw *access);

void *GPU_vertbuf_steal_data(GPUVertBuf *verts);

void *GPU_vertbuf_get_data(const GPUVertBuf *verts);
const GPUVertFormat *GPU_vertbuf_get_format(const GPUVertBuf *verts);
uint GPU_vertbuf_get_vertex_alloc(const GPUVertBuf *verts);
uint GPU_vertbuf_get_vertex_len(const GPUVertBuf *verts);
GPUVertBufStatus GPU_vertbuf_get_status(const GPUVertBuf *verts);

void GPU_vertbuf_use(GPUVertBuf *);

/* XXX do not use. */
void GPU_vertbuf_update_sub(GPUVertBuf *verts, uint start, uint len, void *data);

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
