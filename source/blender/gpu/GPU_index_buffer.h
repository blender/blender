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
 * GPU index buffer
 */

#pragma once

#include "GPU_primitive.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque type hiding blender::gpu::IndexBuf. */
typedef struct GPUIndexBuf GPUIndexBuf;

GPUIndexBuf *GPU_indexbuf_calloc(void);

typedef struct GPUIndexBufBuilder {
  uint max_allowed_index;
  uint max_index_len;
  uint index_len;
  uint index_min;
  uint index_max;
  GPUPrimType prim_type;
  uint32_t *data;
} GPUIndexBufBuilder;

/* supports all primitive types. */
void GPU_indexbuf_init_ex(GPUIndexBufBuilder *, GPUPrimType, uint index_len, uint vertex_len);

/* supports only GPU_PRIM_POINTS, GPU_PRIM_LINES and GPU_PRIM_TRIS. */
void GPU_indexbuf_init(GPUIndexBufBuilder *, GPUPrimType, uint prim_len, uint vertex_len);
GPUIndexBuf *GPU_indexbuf_build_on_device(uint index_len);

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

GPUIndexBuf *GPU_indexbuf_build(GPUIndexBufBuilder *);
void GPU_indexbuf_build_in_place(GPUIndexBufBuilder *, GPUIndexBuf *);

void GPU_indexbuf_bind_as_ssbo(GPUIndexBuf *elem, int binding);

/* Create a sub-range of an existing index-buffer. */
GPUIndexBuf *GPU_indexbuf_create_subrange(GPUIndexBuf *elem_src, uint start, uint length);
void GPU_indexbuf_create_subrange_in_place(GPUIndexBuf *elem,
                                           GPUIndexBuf *elem_src,
                                           uint start,
                                           uint length);

/**
 * (Download and) return a pointer containing the data of an index buffer.
 *
 * Note that the returned pointer is still owned by the driver. To get an
 * local copy, use `GPU_indexbuf_unmap` after calling `GPU_indexbuf_read`.
 */
const uint32_t *GPU_indexbuf_read(GPUIndexBuf *elem);
uint32_t *GPU_indexbuf_unmap(const GPUIndexBuf *elem, const uint32_t *mapped_buffer);

void GPU_indexbuf_discard(GPUIndexBuf *elem);

bool GPU_indexbuf_is_init(GPUIndexBuf *elem);

int GPU_indexbuf_primitive_len(GPUPrimType prim_type);

/* Macros */

#define GPU_INDEXBUF_DISCARD_SAFE(elem) \
  do { \
    if (elem != NULL) { \
      GPU_indexbuf_discard(elem); \
      elem = NULL; \
    } \
  } while (0)

#ifdef __cplusplus
}
#endif
