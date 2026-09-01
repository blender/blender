/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gl_context.hh"

#include "gl_index_buffer.hh"

namespace blender::gpu {

GLIndexBuf::~GLIndexBuf()
{
  GLContext::buffer_free(ibo_id_);
}

void GLIndexBuf::bind()
{
  if (is_subrange_) {
    static_cast<GLIndexBuf *>(src_)->bind();
    return;
  }

  const bool allocate_on_device = ibo_id_ == 0;
  if (allocate_on_device) {
    glGenBuffers(1, &ibo_id_);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id_);

  if (data_ != nullptr || allocate_on_device) {
    size_t size = this->size_get();
    /* Pad the buffer to avoid out of bound reads when using vertex pulling mode. */
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ceil_to_multiple_ul(size, 16), nullptr, GL_STATIC_DRAW);

    if (data_ != nullptr) {
      /* Sends data to GPU. */
      glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size, data_);
    }
    /* No need to keep copy of data in system memory. */
    MEM_SAFE_DELETE_VOID(data_);
  }
}

void GLIndexBuf::bind_as_ssbo(uint binding)
{
  if (is_subrange_) {
    src_->bind_as_ssbo(binding);
    return;
  }

  if (ibo_id_ == 0 || data_ != nullptr) {
    /* Calling `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id_)` changes the index buffer
     * of the currently bound VAO.
     *
     * In the OpenGL backend, the VAO state persists even after `GLVertArray::update_bindings`
     * is called.
     *
     * NOTE: For safety, we could call `glBindVertexArray(0)` right after drawing a `gpu::Batch`.
     * However, for performance reasons, we have chosen not to do so. */
    glBindVertexArray(0);
    bind();
  }

  BLI_assert(ibo_id_ != 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, ibo_id_);

#ifndef NDEBUG
  BLI_assert(binding < 16);
  GLContext::get()->bound_ssbo_slots |= 1 << binding;
#endif
}

void GLIndexBuf::read(uint32_t *data) const
{
  BLI_assert(is_active());
  const void *buffer = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_READ_ONLY);
  memcpy(data, buffer, size_get());
  glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
}

bool GLIndexBuf::is_active() const
{
  if (!ibo_id_) {
    return false;
  }
  int active_ibo_id = 0;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &active_ibo_id);
  return ibo_id_ == active_ibo_id;
}

void GLIndexBuf::upload_data()
{
  bind();
}

void GLIndexBuf::update_sub(uint start, uint len, const void *data)
{
  glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, start, len, data);
}

void GLIndexBuf::copy_sub(IndexBuf &source_buf,
                          uint source_first_index,
                          uint dest_first_index,
                          uint index_len)
{
  GLIndexBuf &src = static_cast<GLIndexBuf &>(source_buf);
  BLI_assert(!is_subrange_);
  BLI_assert(!src.is_subrange_);
  BLI_assert_msg(src.index_type_ == index_type_,
                 "Index type mismatch between source and destination");
  BLI_assert_msg(size_t(source_first_index) + index_len <= src.index_len_,
                 "Copy source range exceeds the source index buffer bounds");
  BLI_assert_msg(size_t(dest_first_index) + index_len <= index_len_,
                 "Copy destination range exceeds the destination index buffer bounds");
  BLI_assert_msg(ibo_id_, "GPU_indexbuf_use() not called on this buffer");
  BLI_assert_msg(src.ibo_id_, "GPU_indexbuf_use() not called on the source buffer");

  const size_t src_offset = size_t(source_first_index) * to_bytesize(src.index_type_);
  const size_t dst_offset = size_t(dest_first_index) * to_bytesize(index_type_);
  const size_t copy_size = size_t(index_len) * to_bytesize(index_type_);

  if (GLContext::direct_state_access_support) {
    glCopyNamedBufferSubData(src.ibo_id_, ibo_id_, src_offset, dst_offset, copy_size);
  }
  else {
    glBindBuffer(GL_COPY_READ_BUFFER, src.ibo_id_);
    glBindBuffer(GL_COPY_WRITE_BUFFER, ibo_id_);
    glCopyBufferSubData(
        GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, src_offset, dst_offset, copy_size);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
  }
}

}  // namespace blender::gpu
