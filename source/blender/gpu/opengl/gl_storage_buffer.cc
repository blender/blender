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
#include "gl_storage_buffer.hh"
#include "gl_vertex_buffer.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

GLStorageBuf::GLStorageBuf(size_t size, GPUUsageType usage, const char *name)
    : StorageBuf(size, name)
{
  usage_ = usage;
  /* Do not create ubo GL buffer here to allow allocation from any thread. */
  BLI_assert(size <= GLContext::max_ssbo_size);
}

GLStorageBuf::~GLStorageBuf()
{
  GLContext::buf_free(ssbo_id_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data upload / update
 * \{ */

void GLStorageBuf::init()
{
  BLI_assert(GLContext::get());

  glGenBuffers(1, &ssbo_id_);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id_);
  glBufferData(GL_SHADER_STORAGE_BUFFER, size_in_bytes_, nullptr, to_gl(this->usage_));

  debug::object_label(GL_SHADER_STORAGE_BUFFER, ssbo_id_, name_);
}

void GLStorageBuf::update(const void *data)
{
  if (ssbo_id_ == 0) {
    this->init();
  }
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id_);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, size_in_bytes_, data);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Usage
 * \{ */

void GLStorageBuf::bind(int slot)
{
  if (slot >= GLContext::max_ssbo_binds) {
    fprintf(
        stderr,
        "Error: Trying to bind \"%s\" ssbo to slot %d which is above the reported limit of %d.",
        name_,
        slot,
        GLContext::max_ssbo_binds);
    return;
  }

  if (ssbo_id_ == 0) {
    this->init();
  }

  if (data_ != nullptr) {
    this->update(data_);
    MEM_SAFE_FREE(data_);
  }

  slot_ = slot;
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot_, ssbo_id_);

#ifdef DEBUG
  BLI_assert(slot < 16);
  /* TODO */
  // GLContext::get()->bound_ssbo_slots |= 1 << slot;
#endif
}

void GLStorageBuf::unbind()
{
#ifdef DEBUG
  /* NOTE: This only unbinds the last bound slot. */
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot_, 0);
  /* Hope that the context did not change. */
  /* TODO */
  // GLContext::get()->bound_ssbo_slots &= ~(1 << slot_);
#endif
  slot_ = 0;
}

/** \} */

}  // namespace blender::gpu
