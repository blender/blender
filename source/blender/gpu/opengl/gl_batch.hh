/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup gpu
 *
 * GPU geometry batch
 * Contains VAOs + VBOs + Shader representing a drawable entity.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_batch_private.hh"

#include "gl_index_buffer.hh"
#include "gl_vertex_buffer.hh"

namespace blender {
namespace gpu {

class GLContext;
class GLShaderInterface;

#define GPU_VAO_STATIC_LEN 3

/**
 * VAO management: remembers all geometry state (vertex attribute bindings & element buffer)
 * for each shader interface. Start with a static number of VAO's and fallback to dynamic count
 * if necessary. Once a batch goes dynamic it does not go back.
 */
class GLVaoCache {
 private:
  /** Context for which the vao_cache_ was generated. */
  GLContext *context_ = nullptr;
  /** Last interface this batch was drawn with. */
  GLShaderInterface *interface_ = nullptr;
  /** Cached VAO for the last interface. */
  GLuint vao_id_ = 0;
  /** Used when arb_base_instance is not supported. */
  GLuint vao_base_instance_ = 0;
  int base_instance_ = 0;

  bool is_dynamic_vao_count = false;
  union {
    /** Static handle count */
    struct {
      const GLShaderInterface *interfaces[GPU_VAO_STATIC_LEN];
      GLuint vao_ids[GPU_VAO_STATIC_LEN];
    } static_vaos;
    /** Dynamic handle count */
    struct {
      uint count;
      const GLShaderInterface **interfaces;
      GLuint *vao_ids;
    } dynamic_vaos;
  };

 public:
  GLVaoCache();
  ~GLVaoCache();

  GLuint vao_get(GPUBatch *batch);
  GLuint base_instance_vao_get(GPUBatch *batch, int i_first);

  /**
   * Return 0 on cache miss (invalid VAO).
   */
  GLuint lookup(const GLShaderInterface *interface);
  /**
   * Create a new VAO object and store it in the cache.
   */
  void insert(const GLShaderInterface *interface, GLuint vao_id);
  void remove(const GLShaderInterface *interface);
  void clear();

 private:
  void init();
  /**
   * The #GLVaoCache object is only valid for one #GLContext.
   * Reset the cache if trying to draw in another context;.
   */
  void context_check();
};

class GLBatch : public Batch {
 public:
  /** All vaos corresponding to all the GPUShaderInterface this batch was drawn with. */
  GLVaoCache vao_cache_;

 public:
  void draw(int v_first, int v_count, int i_first, int i_count) override;
  void draw_indirect(GPUStorageBuf *indirect_buf, intptr_t offset) override;
  void multi_draw_indirect(GPUStorageBuf *indirect_buf,
                           int count,
                           intptr_t offset,
                           intptr_t stride) override;
  void bind(int i_first);

  /* Convenience getters. */

  GLIndexBuf *elem_() const
  {
    return static_cast<GLIndexBuf *>(unwrap(elem));
  }
  GLVertBuf *verts_(const int index) const
  {
    return static_cast<GLVertBuf *>(unwrap(verts[index]));
  }
  GLVertBuf *inst_(const int index) const
  {
    return static_cast<GLVertBuf *>(unwrap(inst[index]));
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("GLBatch");
};

}  // namespace gpu
}  // namespace blender
