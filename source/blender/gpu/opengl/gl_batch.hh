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

#define GPU_VAO_STATIC_LEN 3

/* Vao management: remembers all geometry state (vertex attribute bindings & element buffer)
 * for each shader interface. Start with a static number of vaos and fallback to dynamic count
 * if necessary. Once a batch goes dynamic it does not go back. */
class GLVaoCache {
 private:
  /** Context for which the vao_cache_ was generated. */
  struct GLContext *context_ = NULL;
  /** Last interface this batch was drawn with. */
  GPUShaderInterface *interface_ = NULL;
  /** Cached vao for the last interface. */
  GLuint vao_id_ = 0;
  /** Used whend arb_base_instance is not supported. */
  GLuint vao_base_instance_ = 0;
  int base_instance_ = 0;

  bool is_dynamic_vao_count = false;
  union {
    /** Static handle count */
    struct {
      const GPUShaderInterface *interfaces[GPU_VAO_STATIC_LEN];
      GLuint vao_ids[GPU_VAO_STATIC_LEN];
    } static_vaos;
    /** Dynamic handle count */
    struct {
      uint count;
      const GPUShaderInterface **interfaces;
      GLuint *vao_ids;
    } dynamic_vaos;
  };

 public:
  GLVaoCache();
  ~GLVaoCache();

  GLuint vao_get(GPUBatch *batch);
  GLuint base_instance_vao_get(GPUBatch *batch, int i_first);

  GLuint lookup(const GPUShaderInterface *interface);
  void insert(const GPUShaderInterface *interface, GLuint vao_id);
  void remove(const GPUShaderInterface *interface);
  void clear(void);

 private:
  void init(void);
  void context_check(void);
};

class GLBatch : public Batch {
 public:
  /** All vaos corresponding to all the GPUShaderInterface this batch was drawn with. */
  GLVaoCache vao_cache_;

 public:
  GLBatch();
  ~GLBatch();

  void draw(int v_first, int v_count, int i_first, int i_count) override;
  void bind(int i_first);

  MEM_CXX_CLASS_ALLOC_FUNCS("GLBatch");
};

}  // namespace gpu
}  // namespace blender
