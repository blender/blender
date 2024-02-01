/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GL implementation of GPUBatch.
 * The only specificity of GL here is that it caches a list of
 * Vertex Array Objects based on the bound shader interface.
 */

#include "BLI_assert.h"

#include "gpu_batch_private.hh"
#include "gpu_shader_private.hh"

#include "gl_context.hh"
#include "gl_debug.hh"
#include "gl_index_buffer.hh"
#include "gl_primitive.hh"
#include "gl_storage_buffer.hh"
#include "gl_vertex_array.hh"

#include "gl_batch.hh"

using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name VAO Cache
 *
 * Each #GLBatch has a small cache of VAO objects that are used to avoid VAO reconfiguration.
 * TODO(fclem): Could be revisited to avoid so much cross references.
 * \{ */

GLVaoCache::GLVaoCache()
{
  init();
}

GLVaoCache::~GLVaoCache()
{
  this->clear();
}

void GLVaoCache::init()
{
  context_ = nullptr;
  interface_ = nullptr;
  is_dynamic_vao_count = false;
  for (int i = 0; i < GPU_VAO_STATIC_LEN; i++) {
    static_vaos.interfaces[i] = nullptr;
    static_vaos.vao_ids[i] = 0;
  }
  vao_base_instance_ = 0;
  base_instance_ = 0;
  vao_id_ = 0;
}

void GLVaoCache::insert(const GLShaderInterface *interface, GLuint vao)
{
  /* Now insert the cache. */
  if (!is_dynamic_vao_count) {
    int i; /* find first unused slot */
    for (i = 0; i < GPU_VAO_STATIC_LEN; i++) {
      if (static_vaos.vao_ids[i] == 0) {
        break;
      }
    }

    if (i < GPU_VAO_STATIC_LEN) {
      static_vaos.interfaces[i] = interface;
      static_vaos.vao_ids[i] = vao;
    }
    else {
      /* Erase previous entries, they will be added back if drawn again. */
      for (int i = 0; i < GPU_VAO_STATIC_LEN; i++) {
        if (static_vaos.interfaces[i] != nullptr) {
          const_cast<GLShaderInterface *>(static_vaos.interfaces[i])->ref_remove(this);
          context_->vao_free(static_vaos.vao_ids[i]);
        }
      }
      /* Not enough place switch to dynamic. */
      is_dynamic_vao_count = true;
      /* Init dynamic arrays and let the branch below set the values. */
      dynamic_vaos.count = GPU_BATCH_VAO_DYN_ALLOC_COUNT;
      dynamic_vaos.interfaces = (const GLShaderInterface **)MEM_callocN(
          dynamic_vaos.count * sizeof(GLShaderInterface *), "dyn vaos interfaces");
      dynamic_vaos.vao_ids = (GLuint *)MEM_callocN(dynamic_vaos.count * sizeof(GLuint),
                                                   "dyn vaos ids");
    }
  }

  if (is_dynamic_vao_count) {
    int i; /* find first unused slot */
    for (i = 0; i < dynamic_vaos.count; i++) {
      if (dynamic_vaos.vao_ids[i] == 0) {
        break;
      }
    }

    if (i == dynamic_vaos.count) {
      /* Not enough place, realloc the array. */
      i = dynamic_vaos.count;
      dynamic_vaos.count += GPU_BATCH_VAO_DYN_ALLOC_COUNT;
      dynamic_vaos.interfaces = (const GLShaderInterface **)MEM_recallocN(
          (void *)dynamic_vaos.interfaces, sizeof(GLShaderInterface *) * dynamic_vaos.count);
      dynamic_vaos.vao_ids = (GLuint *)MEM_recallocN(dynamic_vaos.vao_ids,
                                                     sizeof(GLuint) * dynamic_vaos.count);
    }
    dynamic_vaos.interfaces[i] = interface;
    dynamic_vaos.vao_ids[i] = vao;
  }

  const_cast<GLShaderInterface *>(interface)->ref_add(this);
}

void GLVaoCache::remove(const GLShaderInterface *interface)
{
  const int count = (is_dynamic_vao_count) ? dynamic_vaos.count : GPU_VAO_STATIC_LEN;
  GLuint *vaos = (is_dynamic_vao_count) ? dynamic_vaos.vao_ids : static_vaos.vao_ids;
  const GLShaderInterface **interfaces = (is_dynamic_vao_count) ? dynamic_vaos.interfaces :
                                                                  static_vaos.interfaces;
  for (int i = 0; i < count; i++) {
    if (interfaces[i] == interface) {
      context_->vao_free(vaos[i]);
      vaos[i] = 0;
      interfaces[i] = nullptr;
      break; /* cannot have duplicates */
    }
  }

  if (interface_ == interface) {
    interface_ = nullptr;
    vao_id_ = 0;
  }
}

void GLVaoCache::clear()
{
  GLContext *ctx = GLContext::get();
  const int count = (is_dynamic_vao_count) ? dynamic_vaos.count : GPU_VAO_STATIC_LEN;
  GLuint *vaos = (is_dynamic_vao_count) ? dynamic_vaos.vao_ids : static_vaos.vao_ids;
  const GLShaderInterface **interfaces = (is_dynamic_vao_count) ? dynamic_vaos.interfaces :
                                                                  static_vaos.interfaces;
  /* Early out, nothing to free. */
  if (context_ == nullptr) {
    return;
  }

  if (context_ == ctx) {
    glDeleteVertexArrays(count, vaos);
    glDeleteVertexArrays(1, &vao_base_instance_);
  }
  else {
    /* TODO(fclem): Slow way. Could avoid multiple mutex lock here */
    for (int i = 0; i < count; i++) {
      context_->vao_free(vaos[i]);
    }
    context_->vao_free(vao_base_instance_);
  }

  for (int i = 0; i < count; i++) {
    if (interfaces[i] != nullptr) {
      const_cast<GLShaderInterface *>(interfaces[i])->ref_remove(this);
    }
  }

  if (is_dynamic_vao_count) {
    MEM_freeN((void *)dynamic_vaos.interfaces);
    MEM_freeN(dynamic_vaos.vao_ids);
  }

  if (context_) {
    context_->vao_cache_unregister(this);
  }
  /* Reinit. */
  this->init();
}

GLuint GLVaoCache::lookup(const GLShaderInterface *interface)
{
  const int count = (is_dynamic_vao_count) ? dynamic_vaos.count : GPU_VAO_STATIC_LEN;
  const GLShaderInterface **interfaces = (is_dynamic_vao_count) ? dynamic_vaos.interfaces :
                                                                  static_vaos.interfaces;
  for (int i = 0; i < count; i++) {
    if (interfaces[i] == interface) {
      return (is_dynamic_vao_count) ? dynamic_vaos.vao_ids[i] : static_vaos.vao_ids[i];
    }
  }
  return 0;
}

void GLVaoCache::context_check()
{
  GLContext *ctx = GLContext::get();
  BLI_assert(ctx);

  if (context_ != ctx) {
    if (context_ != nullptr) {
      /* IMPORTANT: Trying to draw a batch in multiple different context will trash the VAO cache.
       * This has major performance impact and should be avoided in most cases. */
      context_->vao_cache_unregister(this);
    }
    this->clear();
    context_ = ctx;
    context_->vao_cache_register(this);
  }
}

GLuint GLVaoCache::vao_get(GPUBatch *batch)
{
  this->context_check();

  Shader *shader = GLContext::get()->shader;
  GLShaderInterface *interface = static_cast<GLShaderInterface *>(shader->interface);
  if (interface_ != interface) {
    interface_ = interface;
    vao_id_ = this->lookup(interface_);

    if (vao_id_ == 0) {
      /* Cache miss, create a new VAO. */
      glGenVertexArrays(1, &vao_id_);
      this->insert(interface_, vao_id_);
      GLVertArray::update_bindings(vao_id_, batch, interface_, 0);
    }
  }

  return vao_id_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing
 * \{ */

void GLBatch::bind()
{
  GLContext::get()->state_manager->apply_state();

  if (flag & GPU_BATCH_DIRTY) {
    flag &= ~GPU_BATCH_DIRTY;
    vao_cache_.clear();
  }

  glBindVertexArray(vao_cache_.vao_get(this));
}

void GLBatch::draw(int v_first, int v_count, int i_first, int i_count)
{
  GL_CHECK_RESOURCES("Batch");

  this->bind();

  BLI_assert(v_count > 0 && i_count > 0);

  GLenum gl_type = to_gl(prim_type);

  if (elem) {
    const GLIndexBuf *el = this->elem_();
    GLenum index_type = to_gl(el->index_type_);
    GLint base_index = el->index_base_;
    void *v_first_ofs = el->offset_ptr(v_first);

    glDrawElementsInstancedBaseVertexBaseInstance(
        gl_type, v_count, index_type, v_first_ofs, i_count, base_index, i_first);
  }
  else {
    glDrawArraysInstancedBaseInstance(gl_type, v_first, v_count, i_count, i_first);
  }
}

void GLBatch::draw_indirect(GPUStorageBuf *indirect_buf, intptr_t offset)
{
  GL_CHECK_RESOURCES("Batch");

  this->bind();
  dynamic_cast<GLStorageBuf *>(unwrap(indirect_buf))->bind_as(GL_DRAW_INDIRECT_BUFFER);

  GLenum gl_type = to_gl(prim_type);
  if (elem) {
    const GLIndexBuf *el = this->elem_();
    GLenum index_type = to_gl(el->index_type_);
    glDrawElementsIndirect(gl_type, index_type, (GLvoid *)offset);
  }
  else {
    glDrawArraysIndirect(gl_type, (GLvoid *)offset);
  }
  /* Unbind. */
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void GLBatch::multi_draw_indirect(GPUStorageBuf *indirect_buf,
                                  int count,
                                  intptr_t offset,
                                  intptr_t stride)
{
  GL_CHECK_RESOURCES("Batch");

  this->bind();
  dynamic_cast<GLStorageBuf *>(unwrap(indirect_buf))->bind_as(GL_DRAW_INDIRECT_BUFFER);

  GLenum gl_type = to_gl(prim_type);
  if (elem) {
    const GLIndexBuf *el = this->elem_();
    GLenum index_type = to_gl(el->index_type_);
    glMultiDrawElementsIndirect(gl_type, index_type, (GLvoid *)offset, count, stride);
  }
  else {
    glMultiDrawArraysIndirect(gl_type, (GLvoid *)offset, count, stride);
  }
  /* Unbind. */
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

/** \} */
