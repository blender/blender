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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.h"

#include "BLI_string.h"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"

#include "gl_backend.hh"
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
    fprintf(stderr,
            "Error: Trying to bind \"%s\" ubo to slot %d which is above the reported limit of %d.",
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
