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
 * GPU element list (AKA index buffer)
 */

#ifndef __GPU_ELEMENT_H__
#define __GPU_ELEMENT_H__

#include "GPU_primitive.h"

#define GPU_TRACK_INDEX_RANGE 1

typedef enum {
  GPU_INDEX_U16,
  GPU_INDEX_U32,
} GPUIndexBufType;

typedef struct GPUIndexBuf {
  uint index_len;
#if GPU_TRACK_INDEX_RANGE
  GPUIndexBufType index_type;
  uint32_t gl_index_type;
  uint base_index;
#endif
  uint32_t ibo_id; /* 0 indicates not yet sent to VRAM */
  void *data;      /* non-NULL indicates not yet sent to VRAM */
} GPUIndexBuf;

void GPU_indexbuf_use(GPUIndexBuf *);
uint GPU_indexbuf_size_get(const GPUIndexBuf *);

typedef struct GPUIndexBufBuilder {
  uint max_allowed_index;
  uint max_index_len;
  uint index_len;
  GPUPrimType prim_type;
  uint *data;
} GPUIndexBufBuilder;

/* supports all primitive types. */
void GPU_indexbuf_init_ex(GPUIndexBufBuilder *, GPUPrimType, uint index_len, uint vertex_len);

/* supports only GPU_PRIM_POINTS, GPU_PRIM_LINES and GPU_PRIM_TRIS. */
void GPU_indexbuf_init(GPUIndexBufBuilder *, GPUPrimType, uint prim_len, uint vertex_len);

void GPU_indexbuf_add_generic_vert(GPUIndexBufBuilder *, uint v);
void GPU_indexbuf_add_primitive_restart(GPUIndexBufBuilder *);

void GPU_indexbuf_add_point_vert(GPUIndexBufBuilder *, uint v);
void GPU_indexbuf_add_line_verts(GPUIndexBufBuilder *, uint v1, uint v2);
void GPU_indexbuf_add_tri_verts(GPUIndexBufBuilder *, uint v1, uint v2, uint v3);
void GPU_indexbuf_add_line_adj_verts(GPUIndexBufBuilder *, uint v1, uint v2, uint v3, uint v4);

GPUIndexBuf *GPU_indexbuf_build(GPUIndexBufBuilder *);
void GPU_indexbuf_build_in_place(GPUIndexBufBuilder *, GPUIndexBuf *);

void GPU_indexbuf_discard(GPUIndexBuf *);

int GPU_indexbuf_primitive_len(GPUPrimType prim_type);

/* Macros */

#define GPU_INDEXBUF_DISCARD_SAFE(elem) \
  do { \
    if (elem != NULL) { \
      GPU_indexbuf_discard(elem); \
      elem = NULL; \
    } \
  } while (0)

#endif /* __GPU_ELEMENT_H__ */
