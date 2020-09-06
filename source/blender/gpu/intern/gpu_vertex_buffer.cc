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

#include "MEM_guardedalloc.h"

#include "gpu_backend.hh"
#include "gpu_vertex_format_private.h"

#include "gl_vertex_buffer.hh"    /* TODO remove */
#include "gpu_context_private.hh" /* TODO remove */

#include "gpu_vertex_buffer_private.hh"

#include <cstring>

#define VRAM_USAGE 1

/* -------------------------------------------------------------------- */
/** \name VertBuf
 * \{ */

namespace blender::gpu {

size_t VertBuf::memory_usage = 0;

}  // namespace blender::gpu

/** \} */

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

using namespace blender;
using namespace blender::gpu;

static uint GPU_vertbuf_size_get(const VertBuf *verts);

/* -------- Creation & deletion -------- */

GPUVertBuf *GPU_vertbuf_calloc(void)
{
  return wrap(GPUBackend::get()->vertbuf_alloc());
}

GPUVertBuf *GPU_vertbuf_create_with_format_ex(const GPUVertFormat *format, GPUUsageType usage)
{
  GPUVertBuf *verts = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(verts, format, usage);
  return verts;
}

void GPU_vertbuf_init_with_format_ex(GPUVertBuf *verts_,
                                     const GPUVertFormat *format,
                                     GPUUsageType usage)
{
  VertBuf *verts = unwrap(verts_);
  verts->usage = usage;
  verts->flag = GPU_VERTBUF_DATA_DIRTY;
  verts->handle_refcount = 1;
  GPU_vertformat_copy(&verts->format, format);
  if (!format->packed) {
    VertexFormat_pack(&verts->format);
  }
  verts->flag |= GPU_VERTBUF_INIT;
}

GPUVertBuf *GPU_vertbuf_duplicate(GPUVertBuf *verts_)
{
  VertBuf *verts = unwrap(verts_);
  VertBuf *verts_dst = unwrap(GPU_vertbuf_calloc());
  /* Full copy. */
  *verts_dst = *verts;
  verts_dst->handle_refcount = 1;
  GPU_vertformat_copy(&verts_dst->format, &verts->format);

  if (verts->vbo_id) {
    uint buffer_sz = GPU_vertbuf_size_get(verts);

    verts_dst->vbo_id = GPU_buf_alloc();

    glBindBuffer(GL_COPY_READ_BUFFER, verts->vbo_id);
    glBindBuffer(GL_COPY_WRITE_BUFFER, verts_dst->vbo_id);

    glBufferData(GL_COPY_WRITE_BUFFER, buffer_sz, NULL, to_gl(verts->usage));

    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, buffer_sz);

    VertBuf::memory_usage += GPU_vertbuf_size_get(verts);
  }

  if (verts->data) {
    verts_dst->data = (uchar *)MEM_dupallocN(verts->data);
  }
  return wrap(verts_dst);
}

/** Same as discard but does not free. */
void GPU_vertbuf_clear(GPUVertBuf *verts_)
{
  VertBuf *verts = unwrap(verts_);
  if (verts->vbo_id) {
    GPU_buf_free(verts->vbo_id);
    verts->vbo_id = 0;
    VertBuf::memory_usage -= GPU_vertbuf_size_get(verts);
  }
  if (verts->data) {
    MEM_SAFE_FREE(verts->data);
  }
  verts->flag = GPU_VERTBUF_INVALID;
}

void GPU_vertbuf_discard(GPUVertBuf *verts)
{
  GPU_vertbuf_clear(verts);
  GPU_vertbuf_handle_ref_remove(verts);
}

void GPU_vertbuf_handle_ref_add(GPUVertBuf *verts_)
{
  VertBuf *verts = unwrap(verts_);
  verts->handle_refcount++;
}

void GPU_vertbuf_handle_ref_remove(GPUVertBuf *verts_)
{
  VertBuf *verts = unwrap(verts_);
  BLI_assert(verts->handle_refcount > 0);
  verts->handle_refcount--;
  if (verts->handle_refcount == 0) {
    /* Should already have been cleared. */
    BLI_assert(verts->vbo_id == 0 && verts->data == NULL);
    MEM_freeN(verts);
  }
}

uint GPU_vertbuf_size_get(const VertBuf *verts)
{
  return vertex_buffer_size(&verts->format, verts->vertex_len);
}

/* -------- Data update -------- */

/* create a new allocation, discarding any existing data */
void GPU_vertbuf_data_alloc(GPUVertBuf *verts_, uint v_len)
{
  VertBuf *verts = unwrap(verts_);
  GPUVertFormat *format = &verts->format;
  if (!format->packed) {
    VertexFormat_pack(format);
  }
#if TRUST_NO_ONE
  /* catch any unnecessary use */
  assert(verts->vertex_alloc != v_len || verts->data == NULL);
#endif
  /* discard previous data if any */
  if (verts->data) {
    MEM_freeN(verts->data);
  }

  uint new_size = vertex_buffer_size(&verts->format, v_len);
  VertBuf::memory_usage += new_size - GPU_vertbuf_size_get(verts);

  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  verts->vertex_len = verts->vertex_alloc = v_len;
  verts->data = (uchar *)MEM_mallocN(sizeof(GLubyte) * GPU_vertbuf_size_get(verts), __func__);
}

/* resize buffer keeping existing data */
void GPU_vertbuf_data_resize(GPUVertBuf *verts_, uint v_len)
{
  VertBuf *verts = unwrap(verts_);
#if TRUST_NO_ONE
  assert(verts->data != NULL);
  assert(verts->vertex_alloc != v_len);
#endif

  uint new_size = vertex_buffer_size(&verts->format, v_len);
  VertBuf::memory_usage += new_size - GPU_vertbuf_size_get(verts);

  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  verts->vertex_len = verts->vertex_alloc = v_len;
  verts->data = (uchar *)MEM_reallocN(verts->data, sizeof(GLubyte) * GPU_vertbuf_size_get(verts));
}

/* Set vertex count but does not change allocation.
 * Only this many verts will be uploaded to the GPU and rendered.
 * This is useful for streaming data. */
void GPU_vertbuf_data_len_set(GPUVertBuf *verts_, uint v_len)
{
  VertBuf *verts = unwrap(verts_);
  BLI_assert(verts->data != NULL); /* Only for dynamic data. */
  BLI_assert(v_len <= verts->vertex_alloc);

  uint new_size = vertex_buffer_size(&verts->format, v_len);
  VertBuf::memory_usage += new_size - GPU_vertbuf_size_get(verts);

  verts->vertex_len = v_len;
}

void GPU_vertbuf_attr_set(GPUVertBuf *verts_, uint a_idx, uint v_idx, const void *data)
{
  VertBuf *verts = unwrap(verts_);
  const GPUVertFormat *format = &verts->format;
  const GPUVertAttr *a = &format->attrs[a_idx];

#if TRUST_NO_ONE
  assert(a_idx < format->attr_len);
  assert(v_idx < verts->vertex_alloc);
  assert(verts->data != NULL);
#endif
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  memcpy((uchar *)verts->data + a->offset + v_idx * format->stride, data, a->sz);
}

void GPU_vertbuf_attr_fill(GPUVertBuf *verts_, uint a_idx, const void *data)
{
  VertBuf *verts = unwrap(verts_);
  const GPUVertFormat *format = &verts->format;
  const GPUVertAttr *a = &format->attrs[a_idx];

#if TRUST_NO_ONE
  assert(a_idx < format->attr_len);
#endif
  const uint stride = a->sz; /* tightly packed input data */
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;

  GPU_vertbuf_attr_fill_stride(verts_, a_idx, stride, data);
}

/** Fills a whole vertex (all attributes). Data must match packed layout.  */
void GPU_vertbuf_vert_set(GPUVertBuf *verts_, uint v_idx, const void *data)
{
  VertBuf *verts = unwrap(verts_);
  const GPUVertFormat *format = &verts->format;

#if TRUST_NO_ONE
  assert(v_idx < verts->vertex_alloc);
  assert(verts->data != NULL);
#endif
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  memcpy((uchar *)verts->data + v_idx * format->stride, data, format->stride);
}

void GPU_vertbuf_attr_fill_stride(GPUVertBuf *verts_, uint a_idx, uint stride, const void *data)
{
  VertBuf *verts = unwrap(verts_);
  const GPUVertFormat *format = &verts->format;
  const GPUVertAttr *a = &format->attrs[a_idx];

#if TRUST_NO_ONE
  assert(a_idx < format->attr_len);
  assert(verts->data != NULL);
#endif
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  const uint vertex_len = verts->vertex_len;

  if (format->attr_len == 1 && stride == format->stride) {
    /* we can copy it all at once */
    memcpy(verts->data, data, vertex_len * a->sz);
  }
  else {
    /* we must copy it per vertex */
    for (uint v = 0; v < vertex_len; v++) {
      memcpy((uchar *)verts->data + a->offset + v * format->stride,
             (const uchar *)data + v * stride,
             a->sz);
    }
  }
}

void GPU_vertbuf_attr_get_raw_data(GPUVertBuf *verts_, uint a_idx, GPUVertBufRaw *access)
{
  VertBuf *verts = unwrap(verts_);
  const GPUVertFormat *format = &verts->format;
  const GPUVertAttr *a = &format->attrs[a_idx];

#if TRUST_NO_ONE
  assert(a_idx < format->attr_len);
  assert(verts->data != NULL);
#endif

  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  verts->flag &= ~GPU_VERTBUF_DATA_UPLOADED;
  access->size = a->sz;
  access->stride = format->stride;
  access->data = (uchar *)verts->data + a->offset;
  access->data_init = access->data;
#if TRUST_NO_ONE
  access->_data_end = access->data_init + (size_t)(verts->vertex_alloc * format->stride);
#endif
}

/* -------- Getters -------- */

/* NOTE: Be careful when using this. The data needs to match the expected format. */
void *GPU_vertbuf_get_data(const GPUVertBuf *verts)
{
  /* TODO Assert that the format has no padding. */
  return unwrap(verts)->data;
}

/* Returns the data buffer and set it to null internally to avoid freeing.
 * NOTE: Be careful when using this. The data needs to match the expected format. */
void *GPU_vertbuf_steal_data(GPUVertBuf *verts_)
{
  VertBuf *verts = unwrap(verts_);
  /* TODO Assert that the format has no padding. */
  BLI_assert(verts->data);
  void *data = verts->data;
  verts->data = nullptr;
  return data;
}

const GPUVertFormat *GPU_vertbuf_get_format(const GPUVertBuf *verts)
{
  return &unwrap(verts)->format;
}

uint GPU_vertbuf_get_vertex_alloc(const GPUVertBuf *verts)
{
  return unwrap(verts)->vertex_alloc;
}

uint GPU_vertbuf_get_vertex_len(const GPUVertBuf *verts)
{
  return unwrap(verts)->vertex_len;
}

GPUVertBufStatus GPU_vertbuf_get_status(const GPUVertBuf *verts)
{
  return unwrap(verts)->flag;
}

uint GPU_vertbuf_get_memory_usage(void)
{
  return VertBuf::memory_usage;
}

static void VertBuffer_upload_data(GPUVertBuf *verts_)
{
  VertBuf *verts = unwrap(verts_);
  uint buffer_sz = GPU_vertbuf_size_get(verts);

  /* orphan the vbo to avoid sync */
  glBufferData(GL_ARRAY_BUFFER, buffer_sz, NULL, to_gl(verts->usage));
  /* upload data */
  glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_sz, verts->data);

  if (verts->usage == GPU_USAGE_STATIC) {
    MEM_SAFE_FREE(verts->data);
  }
  verts->flag &= ~GPU_VERTBUF_DATA_DIRTY;
  verts->flag |= GPU_VERTBUF_DATA_UPLOADED;
}

void GPU_vertbuf_use(GPUVertBuf *verts_)
{
  VertBuf *verts = unwrap(verts_);
  /* only create the buffer the 1st time */
  if (verts->vbo_id == 0) {
    verts->vbo_id = GPU_buf_alloc();
  }
  glBindBuffer(GL_ARRAY_BUFFER, verts->vbo_id);
  if (verts->flag & GPU_VERTBUF_DATA_DIRTY) {
    VertBuffer_upload_data(verts_);
  }
}

/** \} */