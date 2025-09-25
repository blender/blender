/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_string.h"

#include "GPU_capabilities.hh"

#include "gpu_context_private.hh"

#include "gl_debug.hh"
#include "gl_texture.hh"
#include "gl_uniform_buffer.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

GLUniformBuf::GLUniformBuf(size_t size, const char *name) : UniformBuf(size, name)
{
  /* Do not create ubo GL buffer here to allow allocation from any thread. */
  BLI_assert(size <= GPU_max_uniform_buffer_size());
}

GLUniformBuf::~GLUniformBuf()
{
  GLContext::buffer_free(ubo_id_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data upload / update
 * \{ */

void GLUniformBuf::init()
{
  BLI_assert(GLContext::get());

  glGenBuffers(1, &ubo_id_);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo_id_);
  glBufferData(GL_UNIFORM_BUFFER, size_in_bytes_, nullptr, GL_DYNAMIC_DRAW);

  debug::object_label(GL_UNIFORM_BUFFER, ubo_id_, name_);
}

void GLUniformBuf::update(const void *data)
{
  if (ubo_id_ == 0) {
    this->init();
  }
  glBindBuffer(GL_UNIFORM_BUFFER, ubo_id_);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, size_in_bytes_, data);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GLUniformBuf::clear_to_zero()
{
  if (ubo_id_ == 0) {
    this->init();
  }

  uint32_t data = 0;
  TextureFormat internal_format = TextureFormat::UINT_32;
  eGPUDataFormat data_format = GPU_DATA_UINT;

  if (GLContext::direct_state_access_support) {
    glClearNamedBufferData(ubo_id_,
                           to_gl_internal_format(internal_format),
                           to_gl_data_format(internal_format),
                           to_gl(data_format),
                           &data);
  }
  else {
    /* WATCH(@fclem): This should be ok since we only use clear outside of drawing functions. */
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_id_);
    glClearBufferData(GL_UNIFORM_BUFFER,
                      to_gl_internal_format(internal_format),
                      to_gl_data_format(internal_format),
                      to_gl(data_format),
                      &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Usage
 * \{ */

void GLUniformBuf::bind(int slot)
{
  if (slot >= GLContext::max_ubo_binds) {
    fprintf(
        stderr,
        "Error: Trying to bind \"%s\" ubo to slot %d which is above the reported limit of %d.\n",
        name_,
        slot,
        GLContext::max_ubo_binds);
    return;
  }

  if (ubo_id_ == 0) {
    this->init();
  }

  if (data_ != nullptr) {
    this->update(data_);
    MEM_SAFE_FREE(data_);
  }

  slot_ = slot;
  glBindBufferBase(GL_UNIFORM_BUFFER, slot_, ubo_id_);

#ifndef NDEBUG
  BLI_assert(slot < 16);
  GLContext::get()->bound_ubo_slots |= 1 << slot;
#endif
}

void GLUniformBuf::bind_as_ssbo(int slot)
{
  if (ubo_id_ == 0) {
    this->init();
  }
  if (data_ != nullptr) {
    this->update(data_);
    MEM_SAFE_FREE(data_);
  }

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, ubo_id_);
#ifndef NDEBUG
  BLI_assert(slot < 16);
  GLContext::get()->bound_ssbo_slots |= 1 << slot;
#endif
}

void GLUniformBuf::unbind()
{
#ifndef NDEBUG
  /* NOTE: This only unbinds the last bound slot. */
  glBindBufferBase(GL_UNIFORM_BUFFER, slot_, 0);
  /* Hope that the context did not change. */
  GLContext::get()->bound_ubo_slots &= ~(1 << slot_);
#endif
  slot_ = 0;
}

/** \} */

}  // namespace blender::gpu
