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

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_uniform_buffer_private.hh"

#include "glew-mx.h"

namespace blender {
namespace gpu {

/**
 * Implementation of Uniform Buffers using OpenGL.
 */
class GLUniformBuf : public UniformBuf {
 private:
  /** Slot to which this UBO is currently bound. -1 if not bound. */
  int slot_ = -1;
  /** OpenGL Object handle. */
  GLuint ubo_id_ = 0;

 public:
  GLUniformBuf(size_t size, const char *name);
  ~GLUniformBuf();

  void update(const void *data) override;
  void bind(int slot) override;
  void unbind() override;

 private:
  void init();

  MEM_CXX_CLASS_ALLOC_FUNCS("GLUniformBuf");
};

}  // namespace gpu
}  // namespace blender
