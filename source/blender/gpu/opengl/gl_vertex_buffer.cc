/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "GPU_texture.hh"

#include "gl_context.hh"

#include "gl_vertex_buffer.hh"

namespace blender::gpu {

void GLVertBuf::acquire_data()
{
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    return;
  }

  /* Discard previous data if any. */
  MEM_SAFE_DELETE(data_);
  data_ = MEM_new_array_uninitialized<uchar>(this->size_alloc_get(), __func__);
}

void GLVertBuf::resize_data()
{
  if (usage_ == GPU_USAGE_DEVICE_ONLY) {
    return;
  }

  data_ = static_cast<uchar *>(
      MEM_realloc_uninitialized(data_, sizeof(uchar) * this->size_alloc_get()));
}

void GLVertBuf::release_data()
{
  if (is_wrapper_) {
    return;
  }

  if (vbo_id_ != 0) {
    GPU_TEXTURE_FREE_SAFE(buffer_texture_);
    GLContext::buffer_free(vbo_id_);
    vbo_id_ = 0;
    memory_usage -= vbo_size_;
  }

  MEM_SAFE_DELETE(data_);
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

    /* This is fine on some systems but will crash on others. */
    BLI_assert(vbo_size_ != 0);
    /* Orphan the vbo to avoid sync then upload data. */
    glBufferData(GL_ARRAY_BUFFER, ceil_to_multiple_ul(vbo_size_, 16), nullptr, to_gl(usage_));
    /* Do not transfer data from host to device when buffer is device only. */
    if (usage_ != GPU_USAGE_DEVICE_ONLY) {
      glBufferSubData(GL_ARRAY_BUFFER, 0, vbo_size_, data_);
    }
    memory_usage += vbo_size_;

    if (usage_ == GPU_USAGE_STATIC) {
      MEM_SAFE_DELETE(data_);
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

#ifndef NDEBUG
  BLI_assert(binding < 16);
  GLContext::get()->bound_ssbo_slots |= 1 << binding;
#endif
}

void GLVertBuf::bind_as_texture(uint binding)
{
  bind();
  BLI_assert(vbo_id_ != 0);
  if (buffer_texture_ == nullptr) {
    buffer_texture_ = GPU_texture_create_from_vertbuf("vertbuf_as_texture", this);
  }
  GPU_texture_bind(buffer_texture_, binding);
}

void GLVertBuf::read(void *data) const
{
  BLI_assert(is_active());
  void *result = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
  memcpy(data, result, size_used_get());
  glUnmapBuffer(GL_ARRAY_BUFFER);
}

void GLVertBuf::wrap_handle(uint64_t handle)
{
  BLI_assert(vbo_id_ == 0);
  BLI_assert(glIsBuffer(uint(handle)));
  is_wrapper_ = true;
  vbo_id_ = uint(handle);
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

void GLVertBuf::copy_sub(VertBuf &source_buf,
                         uint source_first_vertex,
                         uint dest_first_vertex,
                         uint vertex_len)
{
  BLI_assert(format.stride == source_buf.format.stride);
  BLI_assert_msg(size_t(source_first_vertex) + vertex_len <= source_buf.vertex_alloc,
                 "Copy source range exceeds the source vertex buffer bounds");
  BLI_assert_msg(size_t(dest_first_vertex) + vertex_len <= vertex_alloc,
                 "Copy destination range exceeds the vertex buffer bounds");
  GLVertBuf &src = static_cast<GLVertBuf &>(source_buf);
  BLI_assert_msg(vbo_id_, "GPU_vertbuf_use() not called on this buffer");
  BLI_assert_msg(src.vbo_id_, "GPU_vertbuf_use() not called on the source buffer");

  const size_t src_offset = size_t(source_first_vertex) * src.format.stride;
  const size_t dst_offset = size_t(dest_first_vertex) * format.stride;
  const size_t copy_size = size_t(vertex_len) * format.stride;

  if (GLContext::direct_state_access_support) {
    glCopyNamedBufferSubData(src.vbo_id_, vbo_id_, src_offset, dst_offset, copy_size);
  }
  else {
    glBindBuffer(GL_COPY_READ_BUFFER, src.vbo_id_);
    glBindBuffer(GL_COPY_WRITE_BUFFER, vbo_id_);
    glCopyBufferSubData(
        GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, src_offset, dst_offset, copy_size);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
  }
}

}  // namespace blender::gpu
