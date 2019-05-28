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

#include "MEM_guardedalloc.h"

#include "GPU_element.h"

#include "gpu_context_private.h"

#include <stdlib.h>

#define KEEP_SINGLE_COPY 1

#define RESTART_INDEX 0xFFFFFFFF

static GLenum convert_index_type_to_gl(GPUIndexBufType type)
{
  static const GLenum table[] = {
      [GPU_INDEX_U16] = GL_UNSIGNED_SHORT,
      [GPU_INDEX_U32] = GL_UNSIGNED_INT,
  };
  return table[type];
}

uint GPU_indexbuf_size_get(const GPUIndexBuf *elem)
{
#if GPU_TRACK_INDEX_RANGE
  static const uint table[] = {
      [GPU_INDEX_U16] = sizeof(GLushort),
      [GPU_INDEX_U32] = sizeof(GLuint),
  };
  return elem->index_len * table[elem->index_type];
#else
  return elem->index_len * sizeof(GLuint);
#endif
}

int GPU_indexbuf_primitive_len(GPUPrimType prim_type)
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
      break;
  }
#if TRUST_NO_ONE
  assert(false);
#endif
  return -1;
}

void GPU_indexbuf_init_ex(GPUIndexBufBuilder *builder,
                          GPUPrimType prim_type,
                          uint index_len,
                          uint vertex_len)
{
  builder->max_allowed_index = vertex_len - 1;
  builder->max_index_len = index_len;
  builder->index_len = 0;  // start empty
  builder->prim_type = prim_type;
  builder->data = MEM_callocN(builder->max_index_len * sizeof(uint), "GPUIndexBuf data");
}

void GPU_indexbuf_init(GPUIndexBufBuilder *builder,
                       GPUPrimType prim_type,
                       uint prim_len,
                       uint vertex_len)
{
  int verts_per_prim = GPU_indexbuf_primitive_len(prim_type);
#if TRUST_NO_ONE
  assert(verts_per_prim != -1);
#endif
  GPU_indexbuf_init_ex(builder, prim_type, prim_len * (uint)verts_per_prim, vertex_len);
}

void GPU_indexbuf_add_generic_vert(GPUIndexBufBuilder *builder, uint v)
{
#if TRUST_NO_ONE
  assert(builder->data != NULL);
  assert(builder->index_len < builder->max_index_len);
  assert(v <= builder->max_allowed_index);
#endif
  builder->data[builder->index_len++] = v;
}

void GPU_indexbuf_add_primitive_restart(GPUIndexBufBuilder *builder)
{
#if TRUST_NO_ONE
  assert(builder->data != NULL);
  assert(builder->index_len < builder->max_index_len);
#endif
  builder->data[builder->index_len++] = RESTART_INDEX;
}

void GPU_indexbuf_add_point_vert(GPUIndexBufBuilder *builder, uint v)
{
#if TRUST_NO_ONE
  assert(builder->prim_type == GPU_PRIM_POINTS);
#endif
  GPU_indexbuf_add_generic_vert(builder, v);
}

void GPU_indexbuf_add_line_verts(GPUIndexBufBuilder *builder, uint v1, uint v2)
{
#if TRUST_NO_ONE
  assert(builder->prim_type == GPU_PRIM_LINES);
  assert(v1 != v2);
#endif
  GPU_indexbuf_add_generic_vert(builder, v1);
  GPU_indexbuf_add_generic_vert(builder, v2);
}

void GPU_indexbuf_add_tri_verts(GPUIndexBufBuilder *builder, uint v1, uint v2, uint v3)
{
#if TRUST_NO_ONE
  assert(builder->prim_type == GPU_PRIM_TRIS);
  assert(v1 != v2 && v2 != v3 && v3 != v1);
#endif
  GPU_indexbuf_add_generic_vert(builder, v1);
  GPU_indexbuf_add_generic_vert(builder, v2);
  GPU_indexbuf_add_generic_vert(builder, v3);
}

void GPU_indexbuf_add_line_adj_verts(
    GPUIndexBufBuilder *builder, uint v1, uint v2, uint v3, uint v4)
{
#if TRUST_NO_ONE
  assert(builder->prim_type == GPU_PRIM_LINES_ADJ);
  assert(v2 != v3); /* only the line need diff indices */
#endif
  GPU_indexbuf_add_generic_vert(builder, v1);
  GPU_indexbuf_add_generic_vert(builder, v2);
  GPU_indexbuf_add_generic_vert(builder, v3);
  GPU_indexbuf_add_generic_vert(builder, v4);
}

#if GPU_TRACK_INDEX_RANGE
/* Everything remains 32 bit while building to keep things simple.
 * Find min/max after, then convert to smallest index type possible. */

static uint index_range(const uint values[], uint value_len, uint *min_out, uint *max_out)
{
  if (value_len == 0) {
    *min_out = 0;
    *max_out = 0;
    return 0;
  }
  uint min_value = values[0];
  uint max_value = values[0];
  for (uint i = 1; i < value_len; ++i) {
    const uint value = values[i];
    if (value == RESTART_INDEX) {
      continue;
    }
    else if (value < min_value) {
      min_value = value;
    }
    else if (value > max_value) {
      max_value = value;
    }
  }
  *min_out = min_value;
  *max_out = max_value;
  return max_value - min_value;
}

static void squeeze_indices_short(GPUIndexBufBuilder *builder,
                                  GPUIndexBuf *elem,
                                  uint min_index,
                                  uint max_index)
{
  const uint *values = builder->data;
  const uint index_len = elem->index_len;

  /* data will never be *larger* than builder->data...
   * converting in place to avoid extra allocation */
  GLushort *data = (GLushort *)builder->data;

  if (max_index > 0xFFFF) {
    elem->base_index = min_index;
    for (uint i = 0; i < index_len; ++i) {
      data[i] = (values[i] == RESTART_INDEX) ? 0xFFFF : (GLushort)(values[i] - min_index);
    }
  }
  else {
    elem->base_index = 0;
    for (uint i = 0; i < index_len; ++i) {
      data[i] = (GLushort)(values[i]);
    }
  }
}

#endif /* GPU_TRACK_INDEX_RANGE */

GPUIndexBuf *GPU_indexbuf_build(GPUIndexBufBuilder *builder)
{
  GPUIndexBuf *elem = MEM_callocN(sizeof(GPUIndexBuf), "GPUIndexBuf");
  GPU_indexbuf_build_in_place(builder, elem);
  return elem;
}

void GPU_indexbuf_build_in_place(GPUIndexBufBuilder *builder, GPUIndexBuf *elem)
{
#if TRUST_NO_ONE
  assert(builder->data != NULL);
#endif
  elem->index_len = builder->index_len;
  elem->ibo_id = 0; /* Created at first use. */

#if GPU_TRACK_INDEX_RANGE
  uint min_index, max_index;
  uint range = index_range(builder->data, builder->index_len, &min_index, &max_index);

  /* count the primitive restart index. */
  range += 1;

  if (range <= 0xFFFF) {
    elem->index_type = GPU_INDEX_U16;
    squeeze_indices_short(builder, elem, min_index, max_index);
  }
  else {
    elem->index_type = GPU_INDEX_U32;
    elem->base_index = 0;
  }
  elem->gl_index_type = convert_index_type_to_gl(elem->index_type);
#endif

  /* Transfer data ownership to GPUIndexBuf.
   * It will be uploaded upon first use. */
  elem->data = builder->data;
  builder->data = NULL;
  /* other fields are safe to leave */
}

static void indexbuf_upload_data(GPUIndexBuf *elem)
{
  /* send data to GPU */
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, GPU_indexbuf_size_get(elem), elem->data, GL_STATIC_DRAW);
  /* No need to keep copy of data in system memory. */
  MEM_freeN(elem->data);
  elem->data = NULL;
}

void GPU_indexbuf_use(GPUIndexBuf *elem)
{
  if (elem->ibo_id == 0) {
    elem->ibo_id = GPU_buf_alloc();
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elem->ibo_id);
  if (elem->data != NULL) {
    indexbuf_upload_data(elem);
  }
}

void GPU_indexbuf_discard(GPUIndexBuf *elem)
{
  if (elem->ibo_id) {
    GPU_buf_free(elem->ibo_id);
  }
  if (elem->data) {
    MEM_freeN(elem->data);
  }
  MEM_freeN(elem);
}
