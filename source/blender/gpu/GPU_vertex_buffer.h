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

#ifndef __GPU_VERTEX_BUFFER_H__
#define __GPU_VERTEX_BUFFER_H__

#include "GPU_vertex_format.h"

#define VRAM_USAGE 1
/**
 * How to create a #GPUVertBuf:
 * 1) verts = GPU_vertbuf_create() or GPU_vertbuf_init(verts)
 * 2) GPU_vertformat_attr_add(verts->format, ...)
 * 3) GPU_vertbuf_data_alloc(verts, vertex_len) <-- finalizes/packs vertex format
 * 4) GPU_vertbuf_attr_fill(verts, pos, application_pos_buffer)
 */

/* Is GPUVertBuf always used as part of a GPUBatch? */

typedef enum {
  /* can be extended to support more types */
  GPU_USAGE_STREAM,
  GPU_USAGE_STATIC, /* do not keep data in memory */
  GPU_USAGE_DYNAMIC,
} GPUUsageType;

typedef struct GPUVertBuf {
  GPUVertFormat format;
  uint vertex_len;   /* number of verts we want to draw */
  uint vertex_alloc; /* number of verts data */
  bool dirty;
  unsigned char *data; /* NULL indicates data in VRAM (unmapped) */
  uint32_t vbo_id;     /* 0 indicates not yet allocated */
  GPUUsageType usage;  /* usage hint for GL optimisation */
} GPUVertBuf;

GPUVertBuf *GPU_vertbuf_create(GPUUsageType);
GPUVertBuf *GPU_vertbuf_create_with_format_ex(const GPUVertFormat *, GPUUsageType);

#define GPU_vertbuf_create_with_format(format) \
  GPU_vertbuf_create_with_format_ex(format, GPU_USAGE_STATIC)

void GPU_vertbuf_clear(GPUVertBuf *verts);
void GPU_vertbuf_discard(GPUVertBuf *);

void GPU_vertbuf_init(GPUVertBuf *, GPUUsageType);
void GPU_vertbuf_init_with_format_ex(GPUVertBuf *, const GPUVertFormat *, GPUUsageType);

#define GPU_vertbuf_init_with_format(verts, format) \
  GPU_vertbuf_init_with_format_ex(verts, format, GPU_USAGE_STATIC)

uint GPU_vertbuf_size_get(const GPUVertBuf *);
void GPU_vertbuf_data_alloc(GPUVertBuf *, uint v_len);
void GPU_vertbuf_data_resize(GPUVertBuf *, uint v_len);
void GPU_vertbuf_data_len_set(GPUVertBuf *, uint v_len);

/* The most important #set_attr variant is the untyped one. Get it right first.
 * It takes a void* so the app developer is responsible for matching their app data types
 * to the vertex attribute's type and component count. They're in control of both, so this
 * should not be a problem. */

void GPU_vertbuf_attr_set(GPUVertBuf *, uint a_idx, uint v_idx, const void *data);
void GPU_vertbuf_attr_fill(GPUVertBuf *,
                           uint a_idx,
                           const void *data); /* tightly packed, non interleaved input data */
void GPU_vertbuf_attr_fill_stride(GPUVertBuf *, uint a_idx, uint stride, const void *data);

/* For low level access only */
typedef struct GPUVertBufRaw {
  uint size;
  uint stride;
  unsigned char *data;
  unsigned char *data_init;
#if TRUST_NO_ONE
  /* Only for overflow check */
  unsigned char *_data_end;
#endif
} GPUVertBufRaw;

GPU_INLINE void *GPU_vertbuf_raw_step(GPUVertBufRaw *a)
{
  unsigned char *data = a->data;
  a->data += a->stride;
#if TRUST_NO_ONE
  assert(data < a->_data_end);
#endif
  return (void *)data;
}

GPU_INLINE uint GPU_vertbuf_raw_used(GPUVertBufRaw *a)
{
  return ((a->data - a->data_init) / a->stride);
}

void GPU_vertbuf_attr_get_raw_data(GPUVertBuf *, uint a_idx, GPUVertBufRaw *access);

void GPU_vertbuf_use(GPUVertBuf *);

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

#endif /* __GPU_VERTEX_BUFFER_H__ */
