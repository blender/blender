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
 * GPU geometry batch
 * Contains VAOs + VBOs + Shader representing a drawable entity.
 */

#include "MEM_guardedalloc.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_matrix.h"
#include "GPU_shader.h"

#include "gpu_batch_private.h"
#include "gpu_context_private.h"
#include "gpu_primitive_private.h"
#include "gpu_shader_private.h"

#include <stdlib.h>
#include <string.h>

static void batch_update_program_bindings(GPUBatch *batch, uint v_first);

void GPU_batch_vao_cache_clear(GPUBatch *batch)
{
  if (batch->context == NULL) {
    return;
  }
  if (batch->is_dynamic_vao_count) {
    for (int i = 0; i < batch->dynamic_vaos.count; ++i) {
      if (batch->dynamic_vaos.vao_ids[i]) {
        GPU_vao_free(batch->dynamic_vaos.vao_ids[i], batch->context);
      }
      if (batch->dynamic_vaos.interfaces[i]) {
        GPU_shaderinterface_remove_batch_ref(
            (GPUShaderInterface *)batch->dynamic_vaos.interfaces[i], batch);
      }
    }
    MEM_freeN(batch->dynamic_vaos.interfaces);
    MEM_freeN(batch->dynamic_vaos.vao_ids);
  }
  else {
    for (int i = 0; i < GPU_BATCH_VAO_STATIC_LEN; ++i) {
      if (batch->static_vaos.vao_ids[i]) {
        GPU_vao_free(batch->static_vaos.vao_ids[i], batch->context);
      }
      if (batch->static_vaos.interfaces[i]) {
        GPU_shaderinterface_remove_batch_ref(
            (GPUShaderInterface *)batch->static_vaos.interfaces[i], batch);
      }
    }
  }
  batch->is_dynamic_vao_count = false;
  for (int i = 0; i < GPU_BATCH_VAO_STATIC_LEN; ++i) {
    batch->static_vaos.vao_ids[i] = 0;
    batch->static_vaos.interfaces[i] = NULL;
  }
  gpu_context_remove_batch(batch->context, batch);
  batch->context = NULL;
}

GPUBatch *GPU_batch_create_ex(GPUPrimType prim_type,
                              GPUVertBuf *verts,
                              GPUIndexBuf *elem,
                              uint owns_flag)
{
  GPUBatch *batch = MEM_callocN(sizeof(GPUBatch), "GPUBatch");
  GPU_batch_init_ex(batch, prim_type, verts, elem, owns_flag);
  return batch;
}

void GPU_batch_init_ex(
    GPUBatch *batch, GPUPrimType prim_type, GPUVertBuf *verts, GPUIndexBuf *elem, uint owns_flag)
{
#if TRUST_NO_ONE
  assert(verts != NULL);
#endif

  batch->verts[0] = verts;
  for (int v = 1; v < GPU_BATCH_VBO_MAX_LEN; ++v) {
    batch->verts[v] = NULL;
  }
  batch->inst = NULL;
  batch->elem = elem;
  batch->gl_prim_type = convert_prim_type_to_gl(prim_type);
  batch->phase = GPU_BATCH_READY_TO_DRAW;
  batch->is_dynamic_vao_count = false;
  batch->owns_flag = owns_flag;
  batch->free_callback = NULL;
}

/* This will share the VBOs with the new batch. */
void GPU_batch_copy(GPUBatch *batch_dst, GPUBatch *batch_src)
{
  GPU_batch_init_ex(batch_dst, GPU_PRIM_POINTS, batch_src->verts[0], batch_src->elem, 0);

  batch_dst->gl_prim_type = batch_src->gl_prim_type;
  for (int v = 1; v < GPU_BATCH_VBO_MAX_LEN; ++v) {
    batch_dst->verts[v] = batch_src->verts[v];
  }
}

void GPU_batch_clear(GPUBatch *batch)
{
  if (batch->owns_flag & GPU_BATCH_OWNS_INDEX) {
    GPU_indexbuf_discard(batch->elem);
  }
  if (batch->owns_flag & GPU_BATCH_OWNS_INSTANCES) {
    GPU_vertbuf_discard(batch->inst);
  }
  if ((batch->owns_flag & ~GPU_BATCH_OWNS_INDEX) != 0) {
    for (int v = 0; v < GPU_BATCH_VBO_MAX_LEN; ++v) {
      if (batch->verts[v] == NULL) {
        break;
      }
      if (batch->owns_flag & (1 << v)) {
        GPU_vertbuf_discard(batch->verts[v]);
      }
    }
  }
  GPU_batch_vao_cache_clear(batch);
  batch->phase = GPU_BATCH_UNUSED;
}

void GPU_batch_discard(GPUBatch *batch)
{
  if (batch->free_callback) {
    batch->free_callback(batch, batch->callback_data);
  }

  GPU_batch_clear(batch);
  MEM_freeN(batch);
}

void GPU_batch_callback_free_set(GPUBatch *batch,
                                 void (*callback)(GPUBatch *, void *),
                                 void *user_data)
{
  batch->free_callback = callback;
  batch->callback_data = user_data;
}

void GPU_batch_instbuf_set(GPUBatch *batch, GPUVertBuf *inst, bool own_vbo)
{
#if TRUST_NO_ONE
  assert(inst != NULL);
#endif
  /* redo the bindings */
  GPU_batch_vao_cache_clear(batch);

  if (batch->inst != NULL && (batch->owns_flag & GPU_BATCH_OWNS_INSTANCES)) {
    GPU_vertbuf_discard(batch->inst);
  }
  batch->inst = inst;

  if (own_vbo) {
    batch->owns_flag |= GPU_BATCH_OWNS_INSTANCES;
  }
  else {
    batch->owns_flag &= ~GPU_BATCH_OWNS_INSTANCES;
  }
}

/* Returns the index of verts in the batch. */
int GPU_batch_vertbuf_add_ex(GPUBatch *batch, GPUVertBuf *verts, bool own_vbo)
{
  /* redo the bindings */
  GPU_batch_vao_cache_clear(batch);

  for (uint v = 0; v < GPU_BATCH_VBO_MAX_LEN; ++v) {
    if (batch->verts[v] == NULL) {
#if TRUST_NO_ONE
      /* for now all VertexBuffers must have same vertex_len */
      assert(verts->vertex_len == batch->verts[0]->vertex_len);
#endif
      batch->verts[v] = verts;
      /* TODO: mark dirty so we can keep attribute bindings up-to-date */
      if (own_vbo) {
        batch->owns_flag |= (1 << v);
      }
      return v;
    }
  }

  /* we only make it this far if there is no room for another GPUVertBuf */
#if TRUST_NO_ONE
  assert(false);
#endif
  return -1;
}

static GLuint batch_vao_get(GPUBatch *batch)
{
  /* Search through cache */
  if (batch->is_dynamic_vao_count) {
    for (int i = 0; i < batch->dynamic_vaos.count; ++i) {
      if (batch->dynamic_vaos.interfaces[i] == batch->interface) {
        return batch->dynamic_vaos.vao_ids[i];
      }
    }
  }
  else {
    for (int i = 0; i < GPU_BATCH_VAO_STATIC_LEN; ++i) {
      if (batch->static_vaos.interfaces[i] == batch->interface) {
        return batch->static_vaos.vao_ids[i];
      }
    }
  }

  /* Set context of this batch.
   * It will be bound to it until GPU_batch_vao_cache_clear is called.
   * Until then it can only be drawn with this context. */
  if (batch->context == NULL) {
    batch->context = GPU_context_active_get();
    gpu_context_add_batch(batch->context, batch);
  }
#if TRUST_NO_ONE
  else {
    /* Make sure you are not trying to draw this batch in another context. */
    assert(batch->context == GPU_context_active_get());
  }
#endif

  /* Cache miss, time to add a new entry! */
  GLuint new_vao = 0;
  if (!batch->is_dynamic_vao_count) {
    int i; /* find first unused slot */
    for (i = 0; i < GPU_BATCH_VAO_STATIC_LEN; ++i) {
      if (batch->static_vaos.vao_ids[i] == 0) {
        break;
      }
    }

    if (i < GPU_BATCH_VAO_STATIC_LEN) {
      batch->static_vaos.interfaces[i] = batch->interface;
      batch->static_vaos.vao_ids[i] = new_vao = GPU_vao_alloc();
    }
    else {
      /* Not enough place switch to dynamic. */
      batch->is_dynamic_vao_count = true;
      /* Erase previous entries, they will be added back if drawn again. */
      for (int j = 0; j < GPU_BATCH_VAO_STATIC_LEN; ++j) {
        GPU_shaderinterface_remove_batch_ref(
            (GPUShaderInterface *)batch->static_vaos.interfaces[j], batch);
        GPU_vao_free(batch->static_vaos.vao_ids[j], batch->context);
      }
      /* Init dynamic arrays and let the branch below set the values. */
      batch->dynamic_vaos.count = GPU_BATCH_VAO_DYN_ALLOC_COUNT;
      batch->dynamic_vaos.interfaces = MEM_callocN(
          batch->dynamic_vaos.count * sizeof(GPUShaderInterface *), "dyn vaos interfaces");
      batch->dynamic_vaos.vao_ids = MEM_callocN(batch->dynamic_vaos.count * sizeof(GLuint),
                                                "dyn vaos ids");
    }
  }

  if (batch->is_dynamic_vao_count) {
    int i; /* find first unused slot */
    for (i = 0; i < batch->dynamic_vaos.count; ++i) {
      if (batch->dynamic_vaos.vao_ids[i] == 0) {
        break;
      }
    }

    if (i == batch->dynamic_vaos.count) {
      /* Not enough place, realloc the array. */
      i = batch->dynamic_vaos.count;
      batch->dynamic_vaos.count += GPU_BATCH_VAO_DYN_ALLOC_COUNT;
      batch->dynamic_vaos.interfaces = MEM_recallocN(batch->dynamic_vaos.interfaces,
                                                     sizeof(GPUShaderInterface *) *
                                                         batch->dynamic_vaos.count);
      batch->dynamic_vaos.vao_ids = MEM_recallocN(batch->dynamic_vaos.vao_ids,
                                                  sizeof(GLuint) * batch->dynamic_vaos.count);
    }
    batch->dynamic_vaos.interfaces[i] = batch->interface;
    batch->dynamic_vaos.vao_ids[i] = new_vao = GPU_vao_alloc();
  }

  GPU_shaderinterface_add_batch_ref((GPUShaderInterface *)batch->interface, batch);

#if TRUST_NO_ONE
  assert(new_vao != 0);
#endif

  /* We just got a fresh VAO we need to initialize it. */
  glBindVertexArray(new_vao);
  batch_update_program_bindings(batch, 0);
  glBindVertexArray(0);

  return new_vao;
}

void GPU_batch_program_set_no_use(GPUBatch *batch,
                                  uint32_t program,
                                  const GPUShaderInterface *shaderface)
{
#if TRUST_NO_ONE
  assert(glIsProgram(shaderface->program));
  assert(batch->program_in_use == 0);
#endif
  batch->interface = shaderface;
  batch->program = program;
  batch->vao_id = batch_vao_get(batch);
}

void GPU_batch_program_set(GPUBatch *batch, uint32_t program, const GPUShaderInterface *shaderface)
{
  GPU_batch_program_set_no_use(batch, program, shaderface);
  GPU_batch_program_use_begin(batch); /* hack! to make Batch_Uniform* simpler */
}

void gpu_batch_remove_interface_ref(GPUBatch *batch, const GPUShaderInterface *interface)
{
  if (batch->is_dynamic_vao_count) {
    for (int i = 0; i < batch->dynamic_vaos.count; ++i) {
      if (batch->dynamic_vaos.interfaces[i] == interface) {
        GPU_vao_free(batch->dynamic_vaos.vao_ids[i], batch->context);
        batch->dynamic_vaos.vao_ids[i] = 0;
        batch->dynamic_vaos.interfaces[i] = NULL;
        break; /* cannot have duplicates */
      }
    }
  }
  else {
    int i;
    for (i = 0; i < GPU_BATCH_VAO_STATIC_LEN; ++i) {
      if (batch->static_vaos.interfaces[i] == interface) {
        GPU_vao_free(batch->static_vaos.vao_ids[i], batch->context);
        batch->static_vaos.vao_ids[i] = 0;
        batch->static_vaos.interfaces[i] = NULL;
        break; /* cannot have duplicates */
      }
    }
  }
}

static void create_bindings(GPUVertBuf *verts,
                            const GPUShaderInterface *interface,
                            uint v_first,
                            const bool use_instancing)
{
  const GPUVertFormat *format = &verts->format;

  const uint attr_len = format->attr_len;
  const uint stride = format->stride;

  GPU_vertbuf_use(verts);

  for (uint a_idx = 0; a_idx < attr_len; ++a_idx) {
    const GPUVertAttr *a = &format->attrs[a_idx];
    const GLvoid *pointer = (const GLubyte *)0 + a->offset + v_first * stride;

    for (uint n_idx = 0; n_idx < a->name_len; ++n_idx) {
      const char *name = GPU_vertformat_attr_name_get(format, a, n_idx);
      const GPUShaderInput *input = GPU_shaderinterface_attr(interface, name);

      if (input == NULL) {
        continue;
      }

      if (a->comp_len == 16 || a->comp_len == 12 || a->comp_len == 8) {
#if TRUST_NO_ONE
        assert(a->fetch_mode == GPU_FETCH_FLOAT);
        assert(a->gl_comp_type == GL_FLOAT);
#endif
        for (int i = 0; i < a->comp_len / 4; ++i) {
          glEnableVertexAttribArray(input->location + i);
          glVertexAttribDivisor(input->location + i, (use_instancing) ? 1 : 0);
          glVertexAttribPointer(input->location + i,
                                4,
                                a->gl_comp_type,
                                GL_FALSE,
                                stride,
                                (const GLubyte *)pointer + i * 16);
        }
      }
      else {
        glEnableVertexAttribArray(input->location);
        glVertexAttribDivisor(input->location, (use_instancing) ? 1 : 0);

        switch (a->fetch_mode) {
          case GPU_FETCH_FLOAT:
          case GPU_FETCH_INT_TO_FLOAT:
            glVertexAttribPointer(
                input->location, a->comp_len, a->gl_comp_type, GL_FALSE, stride, pointer);
            break;
          case GPU_FETCH_INT_TO_FLOAT_UNIT:
            glVertexAttribPointer(
                input->location, a->comp_len, a->gl_comp_type, GL_TRUE, stride, pointer);
            break;
          case GPU_FETCH_INT:
            glVertexAttribIPointer(input->location, a->comp_len, a->gl_comp_type, stride, pointer);
            break;
        }
      }
    }
  }
}

static void batch_update_program_bindings(GPUBatch *batch, uint v_first)
{
  for (int v = 0; v < GPU_BATCH_VBO_MAX_LEN && batch->verts[v] != NULL; ++v) {
    create_bindings(batch->verts[v], batch->interface, (batch->inst) ? 0 : v_first, false);
  }
  if (batch->inst) {
    create_bindings(batch->inst, batch->interface, v_first, true);
  }
  if (batch->elem) {
    GPU_indexbuf_use(batch->elem);
  }
}

void GPU_batch_program_use_begin(GPUBatch *batch)
{
  /* NOTE: use_program & done_using_program are fragile, depend on staying in sync with
   *       the GL context's active program.
   *       use_program doesn't mark other programs as "not used". */
  /* TODO: make not fragile (somehow) */

  if (!batch->program_in_use) {
    glUseProgram(batch->program);
    batch->program_in_use = true;
  }
}

void GPU_batch_program_use_end(GPUBatch *batch)
{
  if (batch->program_in_use) {
#if PROGRAM_NO_OPTI
    glUseProgram(0);
#endif
    batch->program_in_use = false;
  }
}

#if TRUST_NO_ONE
#  define GET_UNIFORM \
    const GPUShaderInput *uniform = GPU_shaderinterface_uniform_ensure(batch->interface, name); \
    assert(uniform);
#else
#  define GET_UNIFORM \
    const GPUShaderInput *uniform = GPU_shaderinterface_uniform_ensure(batch->interface, name);
#endif

void GPU_batch_uniform_1ui(GPUBatch *batch, const char *name, uint value)
{
  GET_UNIFORM
  glUniform1ui(uniform->location, value);
}

void GPU_batch_uniform_1i(GPUBatch *batch, const char *name, int value)
{
  GET_UNIFORM
  glUniform1i(uniform->location, value);
}

void GPU_batch_uniform_1b(GPUBatch *batch, const char *name, bool value)
{
  GET_UNIFORM
  glUniform1i(uniform->location, value ? GL_TRUE : GL_FALSE);
}

void GPU_batch_uniform_2f(GPUBatch *batch, const char *name, float x, float y)
{
  GET_UNIFORM
  glUniform2f(uniform->location, x, y);
}

void GPU_batch_uniform_3f(GPUBatch *batch, const char *name, float x, float y, float z)
{
  GET_UNIFORM
  glUniform3f(uniform->location, x, y, z);
}

void GPU_batch_uniform_4f(GPUBatch *batch, const char *name, float x, float y, float z, float w)
{
  GET_UNIFORM
  glUniform4f(uniform->location, x, y, z, w);
}

void GPU_batch_uniform_1f(GPUBatch *batch, const char *name, float x)
{
  GET_UNIFORM
  glUniform1f(uniform->location, x);
}

void GPU_batch_uniform_2fv(GPUBatch *batch, const char *name, const float data[2])
{
  GET_UNIFORM
  glUniform2fv(uniform->location, 1, data);
}

void GPU_batch_uniform_3fv(GPUBatch *batch, const char *name, const float data[3])
{
  GET_UNIFORM
  glUniform3fv(uniform->location, 1, data);
}

void GPU_batch_uniform_4fv(GPUBatch *batch, const char *name, const float data[4])
{
  GET_UNIFORM
  glUniform4fv(uniform->location, 1, data);
}

void GPU_batch_uniform_2fv_array(GPUBatch *batch,
                                 const char *name,
                                 const int len,
                                 const float *data)
{
  GET_UNIFORM
  glUniform2fv(uniform->location, len, data);
}

void GPU_batch_uniform_4fv_array(GPUBatch *batch,
                                 const char *name,
                                 const int len,
                                 const float *data)
{
  GET_UNIFORM
  glUniform4fv(uniform->location, len, data);
}

void GPU_batch_uniform_mat4(GPUBatch *batch, const char *name, const float data[4][4])
{
  GET_UNIFORM
  glUniformMatrix4fv(uniform->location, 1, GL_FALSE, (const float *)data);
}

static void primitive_restart_enable(const GPUIndexBuf *el)
{
  // TODO(fclem) Replace by GL_PRIMITIVE_RESTART_FIXED_INDEX when we have ogl 4.3
  glEnable(GL_PRIMITIVE_RESTART);
  GLuint restart_index = (GLuint)0xFFFFFFFF;

#if GPU_TRACK_INDEX_RANGE
  if (el->index_type == GPU_INDEX_U8) {
    restart_index = (GLuint)0xFF;
  }
  else if (el->index_type == GPU_INDEX_U16) {
    restart_index = (GLuint)0xFFFF;
  }
#endif

  glPrimitiveRestartIndex(restart_index);
}

static void primitive_restart_disable(void)
{
  glDisable(GL_PRIMITIVE_RESTART);
}

static void *elem_offset(const GPUIndexBuf *el, int v_first)
{
#if GPU_TRACK_INDEX_RANGE
  if (el->index_type == GPU_INDEX_U8) {
    return (GLubyte *)0 + v_first;
  }
  else if (el->index_type == GPU_INDEX_U16) {
    return (GLushort *)0 + v_first;
  }
  else {
#endif
    return (GLuint *)0 + v_first;
  }
}

void GPU_batch_draw(GPUBatch *batch)
{
#if TRUST_NO_ONE
  assert(batch->phase == GPU_BATCH_READY_TO_DRAW);
  assert(batch->verts[0]->vbo_id != 0);
#endif
  GPU_batch_program_use_begin(batch);
  GPU_matrix_bind(batch->interface);  // external call.

  GPU_batch_draw_range_ex(batch, 0, 0, false);

  GPU_batch_program_use_end(batch);
}

void GPU_batch_draw_range_ex(GPUBatch *batch, int v_first, int v_count, bool force_instance)
{
#if TRUST_NO_ONE
  assert(!(force_instance && (batch->inst == NULL)) ||
         v_count > 0);  // we cannot infer length if force_instance
#endif

  const bool do_instance = (force_instance || batch->inst);

  // If using offset drawing, use the default VAO and redo bindings.
  if (v_first != 0 && do_instance) {
    glBindVertexArray(GPU_vao_default());
    batch_update_program_bindings(batch, v_first);
  }
  else {
    glBindVertexArray(batch->vao_id);
  }

  if (do_instance) {
    /* Infer length if vertex count is not given */
    if (v_count == 0) {
      v_count = batch->inst->vertex_len;
    }

    if (batch->elem) {
      const GPUIndexBuf *el = batch->elem;

      if (el->use_prim_restart) {
        primitive_restart_enable(el);
      }
#if GPU_TRACK_INDEX_RANGE
      glDrawElementsInstancedBaseVertex(
          batch->gl_prim_type, el->index_len, el->gl_index_type, 0, v_count, el->base_index);
#else
      glDrawElementsInstanced(batch->gl_prim_type, el->index_len, GL_UNSIGNED_INT, 0, v_count);
#endif
      if (el->use_prim_restart) {
        primitive_restart_disable();
      }
    }
    else {
      glDrawArraysInstanced(batch->gl_prim_type, 0, batch->verts[0]->vertex_len, v_count);
    }
  }
  else {
    /* Infer length if vertex count is not given */
    if (v_count == 0) {
      v_count = (batch->elem) ? batch->elem->index_len : batch->verts[0]->vertex_len;
    }

    if (batch->elem) {
      const GPUIndexBuf *el = batch->elem;

      if (el->use_prim_restart) {
        primitive_restart_enable(el);
      }

      void *v_first_ofs = elem_offset(el, v_first);

#if GPU_TRACK_INDEX_RANGE
      if (el->base_index) {
        glDrawRangeElementsBaseVertex(batch->gl_prim_type,
                                      el->min_index,
                                      el->max_index,
                                      v_count,
                                      el->gl_index_type,
                                      v_first_ofs,
                                      el->base_index);
      }
      else {
        glDrawRangeElements(batch->gl_prim_type,
                            el->min_index,
                            el->max_index,
                            v_count,
                            el->gl_index_type,
                            v_first_ofs);
      }
#else
      glDrawElements(batch->gl_prim_type, v_count, GL_UNSIGNED_INT, v_first_ofs);
#endif
      if (el->use_prim_restart) {
        primitive_restart_disable();
      }
    }
    else {
      glDrawArrays(batch->gl_prim_type, v_first, v_count);
    }
  }

  /* Performance hog if you are drawing with the same vao multiple time.
   * Only activate for debugging. */
  // glBindVertexArray(0);
}

/* just draw some vertices and let shader place them where we want. */
void GPU_draw_primitive(GPUPrimType prim_type, int v_count)
{
  /* we cannot draw without vao ... annoying ... */
  glBindVertexArray(GPU_vao_default());

  GLenum type = convert_prim_type_to_gl(prim_type);
  glDrawArrays(type, 0, v_count);

  /* Performance hog if you are drawing with the same vao multiple time.
   * Only activate for debugging.*/
  // glBindVertexArray(0);
}

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

void GPU_batch_program_set_shader(GPUBatch *batch, GPUShader *shader)
{
  GPU_batch_program_set(batch, shader->program, shader->interface);
}

void GPU_batch_program_set_builtin_with_config(GPUBatch *batch,
                                               eGPUBuiltinShader shader_id,
                                               eGPUShaderConfig sh_cfg)
{
  GPUShader *shader = GPU_shader_get_builtin_shader_with_config(shader_id, sh_cfg);
  GPU_batch_program_set(batch, shader->program, shader->interface);
}

void GPU_batch_program_set_builtin(GPUBatch *batch, eGPUBuiltinShader shader_id)
{
  GPU_batch_program_set_builtin_with_config(batch, shader_id, GPU_SHADER_CFG_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init/Exit
 * \{ */

void gpu_batch_init(void)
{
  gpu_batch_presets_init();
}

void gpu_batch_exit(void)
{
  gpu_batch_presets_exit();
}

/** \} */
