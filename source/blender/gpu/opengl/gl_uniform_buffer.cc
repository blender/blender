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

#include "GPU_extensions.h"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"

#include "gl_backend.hh"
#include "gl_uniform_buffer.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

GLUniformBuf::GLUniformBuf(size_t size, const char *name) : UniformBuf(size, name)
{
  /* Do not create ubo GL buffer here to allow allocation from any thread. */
}

GLUniformBuf::~GLUniformBuf()
{
  GLBackend::get()->buf_free(ubo_id_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data upload / update
 * \{ */

void GLUniformBuf::init(void)
{
  BLI_assert(GPU_context_active_get());

  glGenBuffers(1, &ubo_id_);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo_id_);
  glBufferData(GL_UNIFORM_BUFFER, size_in_bytes_, NULL, GL_DYNAMIC_DRAW);

#ifndef __APPLE__
  if ((G.debug & G_DEBUG_GPU) && (GLEW_VERSION_4_3 || GLEW_KHR_debug)) {
    char sh_name[64];
    SNPRINTF(sh_name, "UBO-%s", name_);
    glObjectLabel(GL_BUFFER, ubo_id_, -1, sh_name);
  }
#endif
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
  if (slot >= GPU_max_ubo_binds()) {
    fprintf(stderr,
            "Error: Trying to bind \"%s\" ubo to slot %d which is above the reported limit of %d.",
            name_,
            slot,
            GPU_max_ubo_binds());
    return;
  }

  if (ubo_id_ == 0) {
    this->init();
  }

  if (data_ != NULL) {
    this->update(data_);
    MEM_SAFE_FREE(data_);
  }

  slot_ = slot;
  glBindBufferBase(GL_UNIFORM_BUFFER, slot_, ubo_id_);
}

void GLUniformBuf::unbind(void)
{
#ifdef DEBUG
  /* NOTE: This only unbinds the last bound slot. */
  glBindBufferBase(GL_UNIFORM_BUFFER, slot_, 0);
#endif
  slot_ = 0;
}

/** \} */

}  // namespace blender::gpu