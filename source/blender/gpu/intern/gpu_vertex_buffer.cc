/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU vertex buffer
 */

#include "MEM_guardedalloc.h"

#include "gpu_backend.hh"
#include "gpu_vertex_format_private.hh"

#include "GPU_vertex_buffer.hh"

#include "gpu_context_private.hh" /* TODO: remove. */

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

void VertBuf::init(const GPUVertFormat &format, GPUUsageType usage)
{
  /* Strip extended usage flags. */
  usage_ = usage & ~GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;
#ifndef NDEBUG
  /* Store extended usage. */
  extended_usage_ = usage;
#endif
  flag = GPU_VERTBUF_DATA_DIRTY;
  GPU_vertformat_copy(&this->format, format);
  if (!this->format.packed) {
    VertexFormat_pack(&this->format);
  }
  flag |= GPU_VERTBUF_INIT;
}

void VertBuf::clear()
{
  this->release_data();
  flag = GPU_VERTBUF_INVALID;
}

void VertBuf::allocate(uint vert_len)
{
  BLI_assert(format.packed);
  /* Catch any unnecessary usage. */
  BLI_assert(vertex_alloc != vert_len || data_ == nullptr);
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

VertBuf *GPU_vertbuf_calloc()
{
  return GPUBackend::get()->vertbuf_alloc();
}

VertBuf *GPU_vertbuf_create_with_format_ex(const GPUVertFormat &format, GPUUsageType usage)
{
  VertBuf *verts = GPU_vertbuf_calloc();
  verts->init(format, usage);
  return verts;
}

void GPU_vertbuf_init_with_format_ex(VertBuf &verts,
                                     const GPUVertFormat &format,
                                     GPUUsageType usage)
{
  verts.init(format, usage);
}

void GPU_vertbuf_init_build_on_device(VertBuf &verts, const GPUVertFormat &format, uint v_len)
{
  GPU_vertbuf_init_with_format_ex(verts, format, GPU_USAGE_DEVICE_ONLY);
  GPU_vertbuf_data_alloc(verts, v_len);
}

VertBuf *GPU_vertbuf_create_on_device(const GPUVertFormat &format, uint v_len)
{
  VertBuf *verts = GPU_vertbuf_create_with_format_ex(format, GPU_USAGE_DEVICE_ONLY);
  GPU_vertbuf_data_alloc(*verts, v_len);
  return verts;
}

void GPU_vertbuf_read(const VertBuf *verts, void *data)
{
  verts->read(data);
}

void GPU_vertbuf_clear(VertBuf *verts)
{
  verts->clear();
}

void GPU_vertbuf_discard(VertBuf *verts)
{
  verts->clear();
  verts->reference_remove();
}

void GPU_vertbuf_handle_ref_add(VertBuf *verts)
{
  verts->reference_add();
}

void GPU_vertbuf_handle_ref_remove(VertBuf *verts)
{
  verts->reference_remove();
}

/* -------- Data update -------- */

void GPU_vertbuf_data_alloc(VertBuf &verts, uint v_len)
{
  verts.allocate(v_len);
}

void GPU_vertbuf_data_resize(VertBuf &verts, uint v_len)
{
  verts.resize(v_len);
}

void GPU_vertbuf_data_len_set(VertBuf &verts, uint v_len)
{
  BLI_assert(verts.data<uchar>().data() != nullptr); /* Only for dynamic data. */
  BLI_assert(v_len <= verts.vertex_alloc);
  verts.vertex_len = v_len;
}

void GPU_vertbuf_attr_set(VertBuf *verts, uint a_idx, uint v_idx, const void *data)
{
  BLI_assert(verts->get_usage_type() != GPU_USAGE_DEVICE_ONLY);
  const GPUVertFormat *format = &verts->format;
  const GPUVertAttr *a = &format->attrs[a_idx];
  BLI_assert(v_idx < verts->vertex_alloc);
  BLI_assert(a_idx < format->attr_len);
  BLI_assert(verts->data<uchar>().data() != nullptr);
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  memcpy(verts->data<uchar>().data() + a->offset + v_idx * format->stride, data, a->type.size());
}

void GPU_vertbuf_attr_fill(VertBuf *verts, uint a_idx, const void *data)
{
  const GPUVertFormat *format = &verts->format;
  BLI_assert(a_idx < format->attr_len);
  const GPUVertAttr *a = &format->attrs[a_idx];
  const uint stride = a->type.size(); /* tightly packed input data */
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  GPU_vertbuf_attr_fill_stride(verts, a_idx, stride, data);
}

void GPU_vertbuf_vert_set(VertBuf *verts, uint v_idx, const void *data)
{
  BLI_assert(verts->get_usage_type() != GPU_USAGE_DEVICE_ONLY);
  const GPUVertFormat *format = &verts->format;
  BLI_assert(v_idx < verts->vertex_alloc);
  BLI_assert(verts->data<uchar>().data() != nullptr);
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  memcpy(verts->data<uchar>().data() + v_idx * format->stride, data, format->stride);
}

void GPU_vertbuf_attr_fill_stride(VertBuf *verts, uint a_idx, uint stride, const void *data)
{
  BLI_assert(verts->get_usage_type() != GPU_USAGE_DEVICE_ONLY);
  const GPUVertFormat *format = &verts->format;
  const GPUVertAttr *a = &format->attrs[a_idx];
  BLI_assert(a_idx < format->attr_len);
  BLI_assert(verts->data<uchar>().data() != nullptr);
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  const uint vertex_len = verts->vertex_len;

  if (format->attr_len == 1 && stride == format->stride) {
    /* we can copy it all at once */
    memcpy(verts->data<uchar>().data(), data, vertex_len * a->type.size());
  }
  else {
    /* we must copy it per vertex */
    for (uint v = 0; v < vertex_len; v++) {
      memcpy(verts->data<uchar>().data() + a->offset + v * format->stride,
             (const uchar *)data + v * stride,
             a->type.size());
    }
  }
}

void GPU_vertbuf_attr_get_raw_data(VertBuf *verts, uint a_idx, GPUVertBufRaw *access)
{
  const GPUVertFormat *format = &verts->format;
  const GPUVertAttr *a = &format->attrs[a_idx];
  BLI_assert(a_idx < format->attr_len);
  BLI_assert(verts->data<uchar>().data() != nullptr);

  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
  verts->flag &= ~GPU_VERTBUF_DATA_UPLOADED;
  access->size = a->type.size();
  access->stride = format->stride;
  access->data = (uchar *)verts->data<uchar>().data() + a->offset;
  access->data_init = access->data;
#ifndef NDEBUG
  access->_data_end = access->data_init + size_t(verts->vertex_alloc * format->stride);
#endif
}

/* -------- Getters -------- */

const GPUVertFormat *GPU_vertbuf_get_format(const VertBuf *verts)
{
  return &verts->format;
}

uint GPU_vertbuf_get_vertex_alloc(const VertBuf *verts)
{
  return verts->vertex_alloc;
}

uint GPU_vertbuf_get_vertex_len(const VertBuf *verts)
{
  return verts->vertex_len;
}

GPUVertBufStatus GPU_vertbuf_get_status(const VertBuf *verts)
{
  return verts->flag;
}

void GPU_vertbuf_tag_dirty(VertBuf *verts)
{
  verts->flag |= GPU_VERTBUF_DATA_DIRTY;
}

uint GPU_vertbuf_get_memory_usage()
{
  return VertBuf::memory_usage;
}

void GPU_vertbuf_use(VertBuf *verts)
{
  verts->upload();
}

void GPU_vertbuf_wrap_handle(VertBuf *verts, uint64_t handle)
{
  verts->wrap_handle(handle);
}

void GPU_vertbuf_bind_as_ssbo(VertBuf *verts, int binding)
{
  verts->bind_as_ssbo(binding);
}

void GPU_vertbuf_bind_as_texture(VertBuf *verts, int binding)
{
  verts->bind_as_texture(binding);
}

void GPU_vertbuf_update_sub(VertBuf *verts, uint start, uint len, const void *data)
{
  verts->update_sub(start, len, data);
}

/** \} */
