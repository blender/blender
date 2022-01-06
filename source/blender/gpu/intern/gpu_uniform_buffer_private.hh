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
 * Copyright 2020, Blender Foundation.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_sys_types.h"

struct GPUUniformBuf;

namespace blender {
namespace gpu {

#ifdef DEBUG
#  define DEBUG_NAME_LEN 64
#else
#  define DEBUG_NAME_LEN 8
#endif

/**
 * Implementation of Uniform Buffers.
 * Base class which is then specialized for each implementation (GL, VK, ...).
 */
class UniformBuf {
 protected:
  /** Data size in bytes. */
  size_t size_in_bytes_;
  /** Continuous memory block to copy to GPU. This data is owned by the UniformBuf. */
  void *data_ = NULL;
  /** Debugging name */
  char name_[DEBUG_NAME_LEN];

 public:
  UniformBuf(size_t size, const char *name);
  virtual ~UniformBuf();

  virtual void update(const void *data) = 0;
  virtual void bind(int slot) = 0;
  virtual void unbind() = 0;

  /** Used to defer data upload at drawing time.
   * This is useful if the thread has no context bound.
   * This transfers ownership to this UniformBuf. */
  void attach_data(void *data)
  {
    data_ = data;
  }
};

/* Syntactic sugar. */
static inline GPUUniformBuf *wrap(UniformBuf *vert)
{
  return reinterpret_cast<GPUUniformBuf *>(vert);
}
static inline UniformBuf *unwrap(GPUUniformBuf *vert)
{
  return reinterpret_cast<UniformBuf *>(vert);
}
static inline const UniformBuf *unwrap(const GPUUniformBuf *vert)
{
  return reinterpret_cast<const UniformBuf *>(vert);
}

#undef DEBUG_NAME_LEN

}  // namespace gpu
}  // namespace blender
