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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GL implementation of GPUBatch.
 * The only specificity of GL here is that it caches a list of
 * Vertex Array Objects based on the bound shader interface.
 */

#include "BLI_assert.h"

#include "glew-mx.h"

#include "GPU_extensions.h"

#include "gpu_batch_private.hh"
#include "gpu_primitive_private.h"

#include "gl_batch.hh"
#include "gl_context.hh"
#include "gl_vertex_array.hh"

using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Vao cache
 *
 * Each GLBatch has a small cache of VAO objects that are used to avoid VAO reconfiguration.
 * TODO(fclem) Could be revisited to avoid so much cross references.
 * \{ */

GLVaoCache::GLVaoCache(void)
{
  init();
}

GLVaoCache::~GLVaoCache()
{
  this->clear();
}

void GLVaoCache::init(void)
{
  context_ = NULL;
  interface_ = NULL;
  is_dynamic_vao_count = false;
  for (int i = 0; i < GPU_VAO_STATIC_LEN; i++) {
    static_vaos.interfaces[i] = NULL;
    static_vaos.vao_ids[i] = 0;
  }
  vao_base_instance_ = 0;
  base_instance_ = 0;
}

/* Create a new VAO object and store it in the cache. */
void GLVaoCache::insert(const GPUShaderInterface *interface, GLuint vao)
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
        if (static_vaos.interfaces[i] != NULL) {
          GPU_shaderinterface_remove_batch_ref(
              const_cast<GPUShaderInterface *>(static_vaos.interfaces[i]), this);
          context_->vao_free(static_vaos.vao_ids[i]);
        }
      }
      /* Not enough place switch to dynamic. */
      is_dynamic_vao_count = true;
      /* Init dynamic arrays and let the branch below set the values. */
      dynamic_vaos.count = GPU_BATCH_VAO_DYN_ALLOC_COUNT;
      dynamic_vaos.interfaces = (const GPUShaderInterface **)MEM_callocN(
          dynamic_vaos.count * sizeof(GPUShaderInterface *), "dyn vaos interfaces");
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
      dynamic_vaos.interfaces = (const GPUShaderInterface **)MEM_recallocN(
          (void *)dynamic_vaos.interfaces, sizeof(GPUShaderInterface *) * dynamic_vaos.count);
      dynamic_vaos.vao_ids = (GLuint *)MEM_recallocN(dynamic_vaos.vao_ids,
                                                     sizeof(GLuint) * dynamic_vaos.count);
    }
    dynamic_vaos.interfaces[i] = interface;
    dynamic_vaos.vao_ids[i] = vao;
  }

  GPU_shaderinterface_add_batch_ref(const_cast<GPUShaderInterface *>(interface), this);
}

void GLVaoCache::remove(const GPUShaderInterface *interface)
{
  const int count = (is_dynamic_vao_count) ? dynamic_vaos.count : GPU_VAO_STATIC_LEN;
  GLuint *vaos = (is_dynamic_vao_count) ? dynamic_vaos.vao_ids : static_vaos.vao_ids;
  const GPUShaderInterface **interfaces = (is_dynamic_vao_count) ? dynamic_vaos.interfaces :
                                                                   static_vaos.interfaces;
  for (int i = 0; i < count; i++) {
    if (interfaces[i] == interface) {
      context_->vao_free(vaos[i]);
      vaos[i] = 0;
      interfaces[i] = NULL;
      break; /* cannot have duplicates */
    }
  }
}

void GLVaoCache::clear(void)
{
  GLContext *ctx = static_cast<GLContext *>(GPU_context_active_get());
  const int count = (is_dynamic_vao_count) ? dynamic_vaos.count : GPU_VAO_STATIC_LEN;
  GLuint *vaos = (is_dynamic_vao_count) ? dynamic_vaos.vao_ids : static_vaos.vao_ids;
  const GPUShaderInterface **interfaces = (is_dynamic_vao_count) ? dynamic_vaos.interfaces :
                                                                   static_vaos.interfaces;
  /* Early out, nothing to free. */
  if (context_ == NULL) {
    return;
  }

  if (context_ == ctx) {
    glDeleteVertexArrays(count, vaos);
    glDeleteVertexArrays(1, &vao_base_instance_);
  }
  else {
    /* TODO(fclem) Slow way. Could avoid multiple mutex lock here */
    for (int i = 0; i < count; i++) {
      context_->vao_free(vaos[i]);
    }
    context_->vao_free(vao_base_instance_);
  }

  for (int i = 0; i < count; i++) {
    if (interfaces[i] == NULL) {
      continue;
    }
    GPU_shaderinterface_remove_batch_ref(const_cast<GPUShaderInterface *>(interfaces[i]), this);
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

/* Return 0 on cache miss (invalid VAO) */
GLuint GLVaoCache::lookup(const GPUShaderInterface *interface)
{
  const int count = (is_dynamic_vao_count) ? dynamic_vaos.count : GPU_VAO_STATIC_LEN;
  const GPUShaderInterface **interfaces = (is_dynamic_vao_count) ? dynamic_vaos.interfaces :
                                                                   static_vaos.interfaces;
  for (int i = 0; i < count; i++) {
    if (interfaces[i] == interface) {
      return (is_dynamic_vao_count) ? dynamic_vaos.vao_ids[i] : static_vaos.vao_ids[i];
    }
  }
  return 0;
}

/* The GLVaoCache object is only valid for one GLContext.
 * Reset the cache if trying to draw in another context; */
void GLVaoCache::context_check(void)
{
  GLContext *ctx = static_cast<GLContext *>(GPU_context_active_get());
  BLI_assert(ctx);

  if (context_ != ctx) {
    if (context_ != NULL) {
      /* IMPORTANT: Trying to draw a batch in multiple different context will trash the VAO cache.
       * This has major performance impact and should be avoided in most cases. */
      context_->vao_cache_unregister(this);
    }
    this->clear();
    context_ = ctx;
    context_->vao_cache_register(this);
  }
}

GLuint GLVaoCache::base_instance_vao_get(GPUBatch *batch, int i_first)
{
  this->context_check();
  /* Make sure the interface is up to date. */
  if (interface_ != GPU_context_active_get()->shader->interface) {
    vao_get(batch);
    /* Trigger update. */
    base_instance_ = 0;
  }
  /**
   * There seems to be a nasty bug when drawing using the same VAO reconfiguring (T71147).
   * We just use a throwaway VAO for that. Note that this is likely to degrade performance.
   **/
#ifdef __APPLE__
  glDeleteVertexArrays(1, &vao_base_instance_);
  vao_base_instance_ = 0;
#endif

  if (vao_base_instance_ == 0) {
    glGenVertexArrays(1, &vao_base_instance_);
  }

  if (base_instance_ != i_first) {
    base_instance_ = i_first;
    GLVertArray::update_bindings(vao_base_instance_, batch, interface_, i_first);
  }
  return base_instance_;
}

GLuint GLVaoCache::vao_get(GPUBatch *batch)
{
  this->context_check();

  GPUContext *ctx = GPU_context_active_get();
  if (interface_ != ctx->shader->interface) {
    interface_ = ctx->shader->interface;
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
/** \name Creation & Deletion
 * \{ */

GLBatch::GLBatch(void)
{
}

GLBatch::~GLBatch()
{
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing
 * \{ */

#if GPU_TRACK_INDEX_RANGE
#  define BASE_INDEX(el) ((el)->base_index)
#  define INDEX_TYPE(el) ((el)->gl_index_type)
#else
#  define BASE_INDEX(el) 0
#  define INDEX_TYPE(el) GL_UNSIGNED_INT
#endif

void GLBatch::bind(int i_first)
{
  GPU_context_active_get()->state_manager->apply_state();

  if (flag & GPU_BATCH_DIRTY) {
    vao_cache_.clear();
  }

#if GPU_TRACK_INDEX_RANGE
  /* Can be removed if GL 4.3 is required. */
  if (!GLEW_ARB_ES3_compatibility && (elem != NULL)) {
    glPrimitiveRestartIndex((elem->index_type == GPU_INDEX_U16) ? 0xFFFFu : 0xFFFFFFFFu);
  }
#endif

  /* Can be removed if GL 4.2 is required. */
  if (!GPU_arb_base_instance_is_supported() && (i_first > 0)) {
    glBindVertexArray(vao_cache_.base_instance_vao_get(this, i_first));
  }
  else {
    glBindVertexArray(vao_cache_.vao_get(this));
  }
}

void GLBatch::draw(int v_first, int v_count, int i_first, int i_count)
{
  this->bind(i_first);

  BLI_assert(v_count > 0 && i_count > 0);

  GLenum gl_type = convert_prim_type_to_gl(prim_type);

  if (elem) {
    const GPUIndexBuf *el = elem;
    GLenum index_type = INDEX_TYPE(el);
    GLint base_index = BASE_INDEX(el);
    void *v_first_ofs = (GLuint *)0 + v_first + el->index_start;

#if GPU_TRACK_INDEX_RANGE
    if (el->index_type == GPU_INDEX_U16) {
      v_first_ofs = (GLushort *)0 + v_first + el->index_start;
    }
#endif

    if (GPU_arb_base_instance_is_supported()) {
      glDrawElementsInstancedBaseVertexBaseInstance(
          gl_type, v_count, index_type, v_first_ofs, i_count, base_index, i_first);
    }
    else {
      glDrawElementsInstancedBaseVertex(
          gl_type, v_count, index_type, v_first_ofs, i_count, base_index);
    }
  }
  else {
#ifdef __APPLE__
    glDisable(GL_PRIMITIVE_RESTART);
#endif
    if (GPU_arb_base_instance_is_supported()) {
      glDrawArraysInstancedBaseInstance(gl_type, v_first, v_count, i_count, i_first);
    }
    else {
      glDrawArraysInstanced(gl_type, v_first, v_count, i_count);
    }
#ifdef __APPLE__
    glEnable(GL_PRIMITIVE_RESTART);
#endif
  }
}

/** \} */
