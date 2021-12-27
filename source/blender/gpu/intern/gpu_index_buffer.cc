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

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "gpu_backend.hh"

#include "gpu_index_buffer_private.hh"

#include <cstring>

#define KEEP_SINGLE_COPY 1

#define RESTART_INDEX 0xFFFFFFFF

/* -------------------------------------------------------------------- */
/** \name IndexBufBuilder
 * \{ */

using namespace blender;
using namespace blender::gpu;

void GPU_indexbuf_init_ex(GPUIndexBufBuilder *builder,
                          GPUPrimType prim_type,
                          uint index_len,
                          uint vertex_len)
{
  builder->max_allowed_index = vertex_len - 1;
  builder->max_index_len = index_len;
  builder->index_len = 0;  // start empty
  builder->index_min = UINT32_MAX;
  builder->index_max = 0;
  builder->prim_type = prim_type;
  builder->data = (uint *)MEM_callocN(builder->max_index_len * sizeof(uint), "GPUIndexBuf data");
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

GPUIndexBuf *GPU_indexbuf_build_on_device(uint index_len)
{
  GPUIndexBuf *elem_ = GPU_indexbuf_calloc();
  GPU_indexbuf_init_build_on_device(elem_, index_len);
  return elem_;
}

void GPU_indexbuf_init_build_on_device(GPUIndexBuf *elem, uint index_len)
{
  IndexBuf *elem_ = unwrap(elem);
  elem_->init_build_on_device(index_len);
}

void GPU_indexbuf_join(GPUIndexBufBuilder *builder_to, const GPUIndexBufBuilder *builder_from)
{
  BLI_assert(builder_to->data == builder_from->data);
  builder_to->index_len = max_uu(builder_to->index_len, builder_from->index_len);
  builder_to->index_min = min_uu(builder_to->index_min, builder_from->index_min);
  builder_to->index_max = max_uu(builder_to->index_max, builder_from->index_max);
}

void GPU_indexbuf_add_generic_vert(GPUIndexBufBuilder *builder, uint v)
{
#if TRUST_NO_ONE
  assert(builder->data != nullptr);
  assert(builder->index_len < builder->max_index_len);
  assert(v <= builder->max_allowed_index);
#endif
  builder->data[builder->index_len++] = v;
  builder->index_min = MIN2(builder->index_min, v);
  builder->index_max = MAX2(builder->index_max, v);
}

void GPU_indexbuf_add_primitive_restart(GPUIndexBufBuilder *builder)
{
#if TRUST_NO_ONE
  assert(builder->data != nullptr);
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

void GPU_indexbuf_set_point_vert(GPUIndexBufBuilder *builder, uint elem, uint v1)
{
  BLI_assert(builder->prim_type == GPU_PRIM_POINTS);
  BLI_assert(elem < builder->max_index_len);
  builder->data[elem++] = v1;
  builder->index_min = MIN2(builder->index_min, v1);
  builder->index_max = MAX2(builder->index_max, v1);
  builder->index_len = MAX2(builder->index_len, elem);
}

void GPU_indexbuf_set_line_verts(GPUIndexBufBuilder *builder, uint elem, uint v1, uint v2)
{
  BLI_assert(builder->prim_type == GPU_PRIM_LINES);
  BLI_assert(v1 != v2);
  BLI_assert(v1 <= builder->max_allowed_index);
  BLI_assert(v2 <= builder->max_allowed_index);
  BLI_assert((elem + 1) * 2 <= builder->max_index_len);
  uint idx = elem * 2;
  builder->data[idx++] = v1;
  builder->data[idx++] = v2;
  builder->index_min = MIN3(builder->index_min, v1, v2);
  builder->index_max = MAX3(builder->index_max, v1, v2);
  builder->index_len = MAX2(builder->index_len, idx);
}

void GPU_indexbuf_set_tri_verts(GPUIndexBufBuilder *builder, uint elem, uint v1, uint v2, uint v3)
{
  BLI_assert(builder->prim_type == GPU_PRIM_TRIS);
  BLI_assert(v1 != v2 && v2 != v3 && v3 != v1);
  BLI_assert(v1 <= builder->max_allowed_index);
  BLI_assert(v2 <= builder->max_allowed_index);
  BLI_assert(v3 <= builder->max_allowed_index);
  BLI_assert((elem + 1) * 3 <= builder->max_index_len);
  uint idx = elem * 3;
  builder->data[idx++] = v1;
  builder->data[idx++] = v2;
  builder->data[idx++] = v3;

  builder->index_min = MIN4(builder->index_min, v1, v2, v3);
  builder->index_max = MAX4(builder->index_max, v1, v2, v3);
  builder->index_len = MAX2(builder->index_len, idx);
}

void GPU_indexbuf_set_point_restart(GPUIndexBufBuilder *builder, uint elem)
{
  BLI_assert(builder->prim_type == GPU_PRIM_POINTS);
  BLI_assert(elem < builder->max_index_len);
  builder->data[elem++] = RESTART_INDEX;
  builder->index_len = MAX2(builder->index_len, elem);
}

void GPU_indexbuf_set_line_restart(GPUIndexBufBuilder *builder, uint elem)
{
  BLI_assert(builder->prim_type == GPU_PRIM_LINES);
  BLI_assert((elem + 1) * 2 <= builder->max_index_len);
  uint idx = elem * 2;
  builder->data[idx++] = RESTART_INDEX;
  builder->data[idx++] = RESTART_INDEX;
  builder->index_len = MAX2(builder->index_len, idx);
}

void GPU_indexbuf_set_tri_restart(GPUIndexBufBuilder *builder, uint elem)
{
  BLI_assert(builder->prim_type == GPU_PRIM_TRIS);
  BLI_assert((elem + 1) * 3 <= builder->max_index_len);
  uint idx = elem * 3;
  builder->data[idx++] = RESTART_INDEX;
  builder->data[idx++] = RESTART_INDEX;
  builder->data[idx++] = RESTART_INDEX;
  builder->index_len = MAX2(builder->index_len, idx);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

namespace blender::gpu {

IndexBuf::~IndexBuf()
{
  if (!is_subrange_) {
    MEM_SAFE_FREE(data_);
  }
}

void IndexBuf::init(uint indices_len, uint32_t *indices, uint min_index, uint max_index)
{
  is_init_ = true;
  data_ = indices;
  index_start_ = 0;
  index_len_ = indices_len;

#if GPU_TRACK_INDEX_RANGE
  /* Everything remains 32 bit while building to keep things simple.
   * Find min/max after, then convert to smallest index type possible. */
  uint range = min_index < max_index ? max_index - min_index : 0;
  /* count the primitive restart index. */
  range += 1;

  if (range <= 0xFFFF) {
    index_type_ = GPU_INDEX_U16;
    this->squeeze_indices_short(min_index, max_index);
  }
#endif
}

void IndexBuf::init_build_on_device(uint index_len)
{
  is_init_ = true;
  index_start_ = 0;
  index_len_ = index_len;
  index_type_ = GPU_INDEX_U32;
  data_ = nullptr;
}

void IndexBuf::init_subrange(IndexBuf *elem_src, uint start, uint length)
{
  /* We don't support nested subranges. */
  BLI_assert(elem_src && elem_src->is_subrange_ == false);
  BLI_assert((length == 0) || (start + length <= elem_src->index_len_));

  is_init_ = true;
  is_subrange_ = true;
  src_ = elem_src;
  index_start_ = start;
  index_len_ = length;
  index_base_ = elem_src->index_base_;
  index_type_ = elem_src->index_type_;
}

uint IndexBuf::index_range(uint *r_min, uint *r_max)
{
  if (index_len_ == 0) {
    *r_min = *r_max = 0;
    return 0;
  }
  const uint32_t *uint_idx = (uint32_t *)data_;
  uint min_value = RESTART_INDEX;
  uint max_value = 0;
  for (uint i = 0; i < index_len_; i++) {
    const uint value = uint_idx[i];
    if (value == RESTART_INDEX) {
      continue;
    }
    if (value < min_value) {
      min_value = value;
    }
    else if (value > max_value) {
      max_value = value;
    }
  }
  if (min_value == RESTART_INDEX) {
    *r_min = *r_max = 0;
    return 0;
  }
  *r_min = min_value;
  *r_max = max_value;
  return max_value - min_value;
}

void IndexBuf::squeeze_indices_short(uint min_idx, uint max_idx)
{
  /* data will never be *larger* than builder->data...
   * converting in place to avoid extra allocation */
  uint16_t *ushort_idx = (uint16_t *)data_;
  const uint32_t *uint_idx = (uint32_t *)data_;

  if (max_idx >= 0xFFFF) {
    index_base_ = min_idx;
    for (uint i = 0; i < index_len_; i++) {
      ushort_idx[i] = (uint16_t)MIN2(0xFFFF, uint_idx[i] - min_idx);
    }
  }
  else {
    index_base_ = 0;
    for (uint i = 0; i < index_len_; i++) {
      ushort_idx[i] = (uint16_t)(uint_idx[i]);
    }
  }
}

uint32_t *IndexBuf::unmap(const uint32_t *mapped_memory) const
{
  size_t size = size_get();
  uint32_t *result = static_cast<uint32_t *>(MEM_mallocN(size, __func__));
  memcpy(result, mapped_memory, size);
  return result;
}

}  // namespace blender::gpu

/** \} */

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

GPUIndexBuf *GPU_indexbuf_calloc()
{
  return wrap(GPUBackend::get()->indexbuf_alloc());
}

GPUIndexBuf *GPU_indexbuf_build(GPUIndexBufBuilder *builder)
{
  GPUIndexBuf *elem = GPU_indexbuf_calloc();
  GPU_indexbuf_build_in_place(builder, elem);
  return elem;
}

GPUIndexBuf *GPU_indexbuf_create_subrange(GPUIndexBuf *elem_src, uint start, uint length)
{
  GPUIndexBuf *elem = GPU_indexbuf_calloc();
  GPU_indexbuf_create_subrange_in_place(elem, elem_src, start, length);
  return elem;
}

void GPU_indexbuf_build_in_place(GPUIndexBufBuilder *builder, GPUIndexBuf *elem)
{
  BLI_assert(builder->data != nullptr);
  /* Transfer data ownership to GPUIndexBuf.
   * It will be uploaded upon first use. */
  unwrap(elem)->init(builder->index_len, builder->data, builder->index_min, builder->index_max);
  builder->data = nullptr;
}

void GPU_indexbuf_create_subrange_in_place(GPUIndexBuf *elem,
                                           GPUIndexBuf *elem_src,
                                           uint start,
                                           uint length)
{
  unwrap(elem)->init_subrange(unwrap(elem_src), start, length);
}

const uint32_t *GPU_indexbuf_read(GPUIndexBuf *elem)
{
  return unwrap(elem)->read();
}

uint32_t *GPU_indexbuf_unmap(const GPUIndexBuf *elem, const uint32_t *mapped_buffer)
{
  return unwrap(elem)->unmap(mapped_buffer);
}

void GPU_indexbuf_discard(GPUIndexBuf *elem)
{
  delete unwrap(elem);
}

bool GPU_indexbuf_is_init(GPUIndexBuf *elem)
{
  return unwrap(elem)->is_init();
}

int GPU_indexbuf_primitive_len(GPUPrimType prim_type)
{
  return indices_per_primitive(prim_type);
}

void GPU_indexbuf_use(GPUIndexBuf *elem)
{
  unwrap(elem)->upload_data();
}

void GPU_indexbuf_bind_as_ssbo(GPUIndexBuf *elem, int binding)
{
  unwrap(elem)->bind_as_ssbo(binding);
}

void GPU_indexbuf_update_sub(GPUIndexBuf *elem, uint start, uint len, const void *data)
{
  unwrap(elem)->update_sub(start, len, data);
}

/** \} */
