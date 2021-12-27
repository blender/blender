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
 */

#include "gl_context.hh"

#include "gl_vertex_buffer.hh"

namespace blender::gpu {

void GLVertBuf::acquire_data()
{
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    return;
  }

  /* Discard previous data if any. */
  MEM_SAFE_FREE(data);
  data = (uchar *)MEM_mallocN(sizeof(uchar) * this->size_alloc_get(), __func__);
}

void GLVertBuf::resize_data()
{
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    return;
  }

  data = (uchar *)MEM_reallocN(data, sizeof(uchar) * this->size_alloc_get());
}

void GLVertBuf::release_data()
{
  if (is_wrapper_) {
    return;
  }

  if (vbo_id_ != 0) {
    GLContext::buf_free(vbo_id_);
    vbo_id_ = 0;
    memory_usage -= vbo_size_;
  }

  MEM_SAFE_FREE(data);
}

void GLVertBuf::duplicate_data(VertBuf *dst_)
{
  BLI_assert(GLContext::get() != nullptr);
  GLVertBuf *src = this;
  GLVertBuf *dst = static_cast<GLVertBuf *>(dst_);

  if (src->vbo_id_ != 0) {
    dst->vbo_size_ = src->size_used_get();

    glGenBuffers(1, &dst->vbo_id_);
    glBindBuffer(GL_COPY_WRITE_BUFFER, dst->vbo_id_);
    glBufferData(GL_COPY_WRITE_BUFFER, dst->vbo_size_, nullptr, to_gl(dst->usage_));

    glBindBuffer(GL_COPY_READ_BUFFER, src->vbo_id_);

    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, dst->vbo_size_);

    memory_usage += dst->vbo_size_;
  }

  if (data != nullptr) {
    dst->data = (uchar *)MEM_dupallocN(src->data);
  }
}

void GLVertBuf::upload_data()
{
  this->bind();
}

void GLVertBuf::bind()
{
  BLI_assert(GLContext::get() != nullptr);

  if (vbo_id_ == 0) {
    glGenBuffers(1, &vbo_id_);
  }

  glBindBuffer(GL_ARRAY_BUFFER, vbo_id_);

  if (flag & GPU_VERTBUF_DATA_DIRTY) {
    vbo_size_ = this->size_used_get();
    /* Orphan the vbo to avoid sync then upload data. */
    glBufferData(GL_ARRAY_BUFFER, vbo_size_, nullptr, to_gl(usage_));
    /* Do not transfer data from host to device when buffer is device only. */
    if (usage_ != GPU_USAGE_DEVICE_ONLY) {
      glBufferSubData(GL_ARRAY_BUFFER, 0, vbo_size_, data);
    }
    memory_usage += vbo_size_;

    if (usage_ == GPU_USAGE_STATIC) {
      MEM_SAFE_FREE(data);
    }
    flag &= ~GPU_VERTBUF_DATA_DIRTY;
    flag |= GPU_VERTBUF_DATA_UPLOADED;
  }
}

void GLVertBuf::bind_as_ssbo(uint binding)
{
  bind();
  BLI_assert(vbo_id_ != 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, vbo_id_);
}

const void *GLVertBuf::read() const
{
  BLI_assert(is_active());
  void *result = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
  return result;
}

void *GLVertBuf::unmap(const void *mapped_data) const
{
  void *result = MEM_mallocN(vbo_size_, __func__);
  memcpy(result, mapped_data, vbo_size_);
  return result;
}

void GLVertBuf::wrap_handle(uint64_t handle)
{
  BLI_assert(vbo_id_ == 0);
  BLI_assert(glIsBuffer(static_cast<uint>(handle)));
  is_wrapper_ = true;
  vbo_id_ = static_cast<uint>(handle);
  /* We assume the data is already on the device, so no need to allocate or send it. */
  flag = GPU_VERTBUF_DATA_UPLOADED;
}

bool GLVertBuf::is_active() const
{
  if (!vbo_id_) {
    return false;
  }
  int active_vbo_id = 0;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &active_vbo_id);
  return vbo_id_ == active_vbo_id;
}

void GLVertBuf::update_sub(uint start, uint len, const void *data)
{
  glBufferSubData(GL_ARRAY_BUFFER, start, len, data);
}

}  // namespace blender::gpu
