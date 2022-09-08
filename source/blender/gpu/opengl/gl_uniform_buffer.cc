/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "BLI_string.h"

#include "gpu_context_private.hh"

#include "gl_debug.hh"
#include "gl_uniform_buffer.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

GLUniformBuf::GLUniformBuf(size_t size, const char *name) : UniformBuf(size, name)
{
  /* Do not create ubo GL buffer here to allow allocation from any thread. */
  BLI_assert(size <= GLContext::max_ubo_size);
}

GLUniformBuf::~GLUniformBuf()
{
  GLContext::buf_free(ubo_id_);
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

#ifdef DEBUG
  BLI_assert(slot < 16);
  GLContext::get()->bound_ubo_slots |= 1 << slot;
#endif
}

void GLUniformBuf::unbind()
{
#ifdef DEBUG
  /* NOTE: This only unbinds the last bound slot. */
  glBindBufferBase(GL_UNIFORM_BUFFER, slot_, 0);
  /* Hope that the context did not change. */
  GLContext::get()->bound_ubo_slots &= ~(1 << slot_);
#endif
  slot_ = 0;
}

/** \} */

}  // namespace blender::gpu
