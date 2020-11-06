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

/* -------------------------------------------------------------------- */
/** \name VertBuf
 * \{ */

namespace blender::gpu {

size_t VertBuf::memory_usage = 0;

VertBuf::VertBuf()
{
  /* Needed by some code check. */
  format.attr_len = 0;
}

VertBuf::~VertBuf()
{
  /* Should already have been cleared. */
  BLI_assert(flag == GPU_VERTBUF_INVALID);
}

void VertBuf::init(const GPUVertFormat *format, GPUUsageType usage)
{
  usage_ = usage;
  flag = GPU_VERTBUF_DATA_DIRTY;
  GPU_vertformat_copy(&this->format, format);
  if (!format->packed) {
    VertexFormat_pack(&this->format);
  }
  flag |= GPU_VERTBUF_INIT;
}

void VertBuf::clear()
{
  this->release_data();
  flag = GPU_VERTBUF_INVALID;
}

VertBuf *VertBuf::duplicate()
{
  VertBuf *dst = GPUBackend::get()->vertbuf_alloc();
  /* Full copy. */
  *dst = *this;
  /* Almost full copy... */
  dst->handle_refcount_ = 1;
  /* Duplicate all needed implementation specifics data. */
  this->duplicate_data(dst);
  return dst;
}

void VertBuf::allocate(uint vert_len)
{
  BLI_assert(format.packed);
  /* Catch any unnecessary usage. */
  BLI_assert(vertex_alloc != vert_len || data == nullptr);
  vertex_len = vertex_alloc = vert_len;

  this->acquire_data();

  flag |= GPU_VERTBUF_DATA_DIRTY;
}

void VertBuf::resize(uint vert_len)
{
  /* Catch any unnecessary usage. */
  BLI_assert(vertex_alloc != vert_len);
  vertex_len = vertex_alloc = vert_len;

  this->resize_data();

  flag |= GPU_VERTBUF_DATA_DIRTY;
}

void VertBuf::upload()
{
  this->upload_data();
}

}  // namespace blender::gpu

/** \} */

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

using namespace blender;
using namespace blender::gpu;

/* -------- Creation & deletion -------- */

GPUVertBuf *GPU_vertbuf_calloc()
{
  return wrap(GPUBackend::get()->vertbuf_alloc());
}

GPUVertBuf *GPU_vertbuf_create_with_format_ex(const GPUVertFormat *format, GPUUsageType usage)
{
  GPUVertBuf *verts = GPU_vertbuf_calloc();
  unwrap(verts)->init(format, usage);
  return verts;
}

void GPU_vertbuf_init_with_format_ex(GPUVertBuf *verts_,
                                     const GPUVertFormat *format,
                                     GPUUsageType usage)
{
  unwrap(verts_)->init(format, usage);
}

GPUVertBuf *GPU_vertbuf_duplicate(GPUVertBuf *verts_)
{
  return wrap(unwrap(verts_)->duplicate());
}

/** Same as discard but does not free. */
void GPU_vertbuf_clear(GPUVertBuf *verts)
{
  unwrap(verts)->clear();
}

void GPU_vertbuf_discard(GPUVertBuf *verts)
{
  unwrap(verts)->clear();
  unwrap(verts)->reference_remove();
}

void GPU_vertbuf_handle_ref_add(GPUVertBuf *verts)
{
  unwrap(verts)->reference_add();
}

void GPU_vertbuf_handle_ref_remove(GPUVertBuf *verts)
{
  unwrap(verts)->reference_remove();
}

/* -------- Data update -------- */

/* create a new allocation, discarding any existing data */
void GPU_vertbuf_data_alloc(GPUVertBuf *verts, uint v_len)
{
  unwrap(verts)->allocate(v_len);
}

/* resize buffer keeping existing data */
void GPU_vertbuf_data_resize(GPUVertBuf *verts, uint v_len)
{
  unwrap(verts)->resize(v_len);
}

/* Set vertex count but does not change allocation.
 * Only this many verts will be uploaded to the GPU and rendered.
 * This is useful for streaming data. */
void GPU_vertbuf_data_len_set(GPUVertBuf *verts_, uint v_len)
{
  VertBuf *verts = unwrap(verts_);
  BLI_assert(verts->data != nullptr); /* Only for dynamic data. */
  BLI_assert(v_len <= verts->vertex_alloc);
  verts->vertex_len = v_len;
}

void GPU_vertbuf_attr_set(GPUVertBuf *verts_, uint a_idx, uint v_idx, const void *data)
{
  VertBuf *verts = unwrap(verts_);
  const GPUVertFormat *format = &verts->format;
  const GPUVertAttr *a = &format->attrs[a_idx];
  BLI_assert(v_idx < verts->vertex_alloc);
  BLI_assert(a_idx < format->attr_len);
  BLI_assert(verts->data != nullptr);
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  memcpy(verts->data + a->offset + v_idx * format->stride, data, a->sz);
}

void GPU_vertbuf_attr_fill(GPUVertBuf *verts_, uint a_idx, const void *data)
{
  VertBuf *verts = unwrap(verts_);
  const GPUVertFormat *format = &verts->format;
  BLI_assert(a_idx < format->attr_len);
  const GPUVertAttr *a = &format->attrs[a_idx];
  const uint stride = a->sz; /* tightly packed input data */
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  GPU_vertbuf_attr_fill_stride(verts_, a_idx, stride, data);
}

/** Fills a whole vertex (all attributes). Data must match packed layout.  */
void GPU_vertbuf_vert_set(GPUVertBuf *verts_, uint v_idx, const void *data)
{
  VertBuf *verts = unwrap(verts_);
  const GPUVertFormat *format = &verts->format;
  BLI_assert(v_idx < verts->vertex_alloc);
  BLI_assert(verts->data != nullptr);
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  memcpy(verts->data + v_idx * format->stride, data, format->stride);
}

void GPU_vertbuf_attr_fill_stride(GPUVertBuf *verts_, uint a_idx, uint stride, const void *data)
{
  VertBuf *verts = unwrap(verts_);
  const GPUVertFormat *format = &verts->format;
  const GPUVertAttr *a = &format->attrs[a_idx];
  BLI_assert(a_idx < format->attr_len);
  BLI_assert(verts->data != nullptr);
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  const uint vertex_len = verts->vertex_len;

  if (format->attr_len == 1 && stride == format->stride) {
    /* we can copy it all at once */
    memcpy(verts->data, data, vertex_len * a->sz);
  }
  else {
    /* we must copy it per vertex */
    for (uint v = 0; v < vertex_len; v++) {
      memcpy(
          verts->data + a->offset + v * format->stride, (const uchar *)data + v * stride, a->sz);
    }
  }
}

void GPU_vertbuf_attr_get_raw_data(GPUVertBuf *verts_, uint a_idx, GPUVertBufRaw *access)
{
  VertBuf *verts = unwrap(verts_);
  const GPUVertFormat *format = &verts->format;
  const GPUVertAttr *a = &format->attrs[a_idx];
  BLI_assert(a_idx < format->attr_len);
  BLI_assert(verts->data != nullptr);

  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  verts->flag &= ~GPU_VERTBUF_DATA_UPLOADED;
  access->size = a->sz;
  access->stride = format->stride;
  access->data = (uchar *)verts->data + a->offset;
  access->data_init = access->data;
#ifdef DEBUG
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

uint GPU_vertbuf_get_memory_usage()
{
  return VertBuf::memory_usage;
}

/* Should be rename to GPU_vertbuf_data_upload */
void GPU_vertbuf_use(GPUVertBuf *verts)
{
  unwrap(verts)->upload();
}

/* XXX this is just a wrapper for the use of the Hair refine workaround.
 * To be used with GPU_vertbuf_use(). */
void GPU_vertbuf_update_sub(GPUVertBuf *verts, uint start, uint len, void *data)
{
  unwrap(verts)->update_sub(start, len, data);
}

/** \} */
