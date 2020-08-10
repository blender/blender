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
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPU geometry batch
 * Contains VAOs + VBOs + Shader representing a drawable entity.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_batch_private.hh"

#include "glew-mx.h"

#include "GPU_shader_interface.h"

namespace blender {
namespace gpu {

#define GPU_BATCH_VAO_STATIC_LEN 3

class GLVaoCache {
  /* Vao management: remembers all geometry state (vertex attribute bindings & element buffer)
   * for each shader interface. Start with a static number of vaos and fallback to dynamic count
   * if necessary. Once a batch goes dynamic it does not go back. */
  bool is_dynamic_vao_count = false;
  union {
    /** Static handle count */
    struct {
      const GPUShaderInterface *interfaces[GPU_BATCH_VAO_STATIC_LEN];
      GLuint vao_ids[GPU_BATCH_VAO_STATIC_LEN];
    } static_vaos;
    /** Dynamic handle count */
    struct {
      uint count;
      const GPUShaderInterface **interfaces;
      GLuint *vao_ids;
    } dynamic_vaos;
  };

  GLuint search(const GPUShaderInterface *interface);
  void insert(GLuint vao_id, const GPUShaderInterface *interface);
  void clear(void);
  void interface_remove(const GPUShaderInterface *interface);
};

class GLBatch : public Batch {
 private:
  /** Cached values (avoid dereferencing later). */
  GLuint vao_id;
  /** All vaos corresponding to all the GPUShaderInterface this batch was drawn with. */
  GLVaoCache vaos;

 public:
  GLBatch();
  ~GLBatch();

  void draw(int v_first, int v_count, int i_first, int i_count) override;

  MEM_CXX_CLASS_ALLOC_FUNCS("GLBatch");
};

}  // namespace gpu
}  // namespace blender
