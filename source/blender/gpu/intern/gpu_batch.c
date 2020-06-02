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
#include "GPU_extensions.h"
#include "GPU_matrix.h"
#include "GPU_platform.h"
#include "GPU_shader.h"

#include "gpu_batch_private.h"
#include "gpu_context_private.h"
#include "gpu_primitive_private.h"
#include "gpu_shader_private.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static GLuint g_default_attr_vbo = 0;

static void batch_update_program_bindings(GPUBatch *batch, uint i_first);

void GPU_batch_vao_cache_clear(GPUBatch *batch)
{
  if (batch->context == NULL) {
    return;
  }
  if (batch->is_dynamic_vao_count) {
    for (int i = 0; i < batch->dynamic_vaos.count; i++) {
      if (batch->dynamic_vaos.vao_ids[i]) {
        GPU_vao_free(batch->dynamic_vaos.vao_ids[i], batch->context);
      }
      if (batch->dynamic_vaos.interfaces[i]) {
        GPU_shaderinterface_remove_batch_ref(
            (GPUShaderInterface *)batch->dynamic_vaos.interfaces[i], batch);
      }
    }
    MEM_freeN((void *)batch->dynamic_vaos.interfaces);
    MEM_freeN(batch->dynamic_vaos.vao_ids);
  }
  else {
    for (int i = 0; i < GPU_BATCH_VAO_STATIC_LEN; i++) {
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
  for (int i = 0; i < GPU_BATCH_VAO_STATIC_LEN; i++) {
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
  for (int v = 1; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    batch->verts[v] = NULL;
  }
  for (int v = 0; v < GPU_BATCH_INST_VBO_MAX_LEN; v++) {
    batch->inst[v] = NULL;
  }
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
  for (int v = 1; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    batch_dst->verts[v] = batch_src->verts[v];
  }
}

void GPU_batch_clear(GPUBatch *batch)
{
  if (batch->owns_flag & GPU_BATCH_OWNS_INDEX) {
    GPU_indexbuf_discard(batch->elem);
  }
  if (batch->owns_flag & GPU_BATCH_OWNS_INSTANCES) {
    GPU_vertbuf_discard(batch->inst[0]);
    GPU_VERTBUF_DISCARD_SAFE(batch->inst[1]);
  }
  if ((batch->owns_flag & ~GPU_BATCH_OWNS_INDEX) != 0) {
    for (int v = 0; v < GPU_BATCH_VBO_MAX_LEN; v++) {
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

  if (batch->inst[0] != NULL && (batch->owns_flag & GPU_BATCH_OWNS_INSTANCES)) {
    GPU_vertbuf_discard(batch->inst[0]);
    GPU_VERTBUF_DISCARD_SAFE(batch->inst[1]);
  }
  batch->inst[0] = inst;

  if (own_vbo) {
    batch->owns_flag |= GPU_BATCH_OWNS_INSTANCES;
  }
  else {
    batch->owns_flag &= ~GPU_BATCH_OWNS_INSTANCES;
  }
}

void GPU_batch_elembuf_set(GPUBatch *batch, GPUIndexBuf *elem, bool own_ibo)
{
  BLI_assert(elem != NULL);
  /* redo the bindings */
  GPU_batch_vao_cache_clear(batch);

  if (batch->elem != NULL && (batch->owns_flag & GPU_BATCH_OWNS_INDEX)) {
    GPU_indexbuf_discard(batch->elem);
  }
  batch->elem = elem;

  if (own_ibo) {
    batch->owns_flag |= GPU_BATCH_OWNS_INDEX;
  }
  else {
    batch->owns_flag &= ~GPU_BATCH_OWNS_INDEX;
  }
}

/* A bit of a quick hack. Should be streamlined as the vbos handling */
int GPU_batch_instbuf_add_ex(GPUBatch *batch, GPUVertBuf *insts, bool own_vbo)
{
  /* redo the bindings */
  GPU_batch_vao_cache_clear(batch);

  for (uint v = 0; v < GPU_BATCH_INST_VBO_MAX_LEN; v++) {
    if (batch->inst[v] == NULL) {
#if TRUST_NO_ONE
      /* for now all VertexBuffers must have same vertex_len */
      if (batch->inst[0] != NULL) {
        /* Allow for different size of vertex buf (will choose the smallest number of verts). */
        // assert(insts->vertex_len == batch->inst[0]->vertex_len);
        assert(own_vbo == ((batch->owns_flag & GPU_BATCH_OWNS_INSTANCES) != 0));
      }
#endif
      batch->inst[v] = insts;
      if (own_vbo) {
        batch->owns_flag |= GPU_BATCH_OWNS_INSTANCES;
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

/* Returns the index of verts in the batch. */
int GPU_batch_vertbuf_add_ex(GPUBatch *batch, GPUVertBuf *verts, bool own_vbo)
{
  /* redo the bindings */
  GPU_batch_vao_cache_clear(batch);

  for (uint v = 0; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    if (batch->verts[v] == NULL) {
#if TRUST_NO_ONE
      /* for now all VertexBuffers must have same vertex_len */
      if (batch->verts[0] != NULL) {
        assert(verts->vertex_len == batch->verts[0]->vertex_len);
      }
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
    for (int i = 0; i < batch->dynamic_vaos.count; i++) {
      if (batch->dynamic_vaos.interfaces[i] == batch->interface) {
        return batch->dynamic_vaos.vao_ids[i];
      }
    }
  }
  else {
    for (int i = 0; i < GPU_BATCH_VAO_STATIC_LEN; i++) {
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
    for (i = 0; i < GPU_BATCH_VAO_STATIC_LEN; i++) {
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
      for (int j = 0; j < GPU_BATCH_VAO_STATIC_LEN; j++) {
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
    for (i = 0; i < batch->dynamic_vaos.count; i++) {
      if (batch->dynamic_vaos.vao_ids[i] == 0) {
        break;
      }
    }

    if (i == batch->dynamic_vaos.count) {
      /* Not enough place, realloc the array. */
      i = batch->dynamic_vaos.count;
      batch->dynamic_vaos.count += GPU_BATCH_VAO_DYN_ALLOC_COUNT;
      batch->dynamic_vaos.interfaces = MEM_recallocN((void *)batch->dynamic_vaos.interfaces,
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
  assert(glIsProgram(program));
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
    for (int i = 0; i < batch->dynamic_vaos.count; i++) {
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
    for (i = 0; i < GPU_BATCH_VAO_STATIC_LEN; i++) {
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
                            uint16_t *attr_mask,
                            uint v_first,
                            const bool use_instancing)
{
  const GPUVertFormat *format = &verts->format;

  const uint attr_len = format->attr_len;
  uint stride = format->stride;
  uint offset = 0;

  GPU_vertbuf_use(verts);

  for (uint a_idx = 0; a_idx < attr_len; a_idx++) {
    const GPUVertAttr *a = &format->attrs[a_idx];

    if (format->deinterleaved) {
      offset += ((a_idx == 0) ? 0 : format->attrs[a_idx - 1].sz) * verts->vertex_len;
      stride = a->sz;
    }
    else {
      offset = a->offset;
    }

    const GLvoid *pointer = (const GLubyte *)0 + offset + v_first * stride;

    for (uint n_idx = 0; n_idx < a->name_len; n_idx++) {
      const char *name = GPU_vertformat_attr_name_get(format, a, n_idx);
      const GPUShaderInput *input = GPU_shaderinterface_attr(interface, name);

      if (input == NULL) {
        continue;
      }

      *attr_mask &= ~(1 << input->location);

      if (a->comp_len == 16 || a->comp_len == 12 || a->comp_len == 8) {
#if TRUST_NO_ONE
        assert(a->fetch_mode == GPU_FETCH_FLOAT);
        assert(a->gl_comp_type == GL_FLOAT);
#endif
        for (int i = 0; i < a->comp_len / 4; i++) {
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

static void batch_update_program_bindings(GPUBatch *batch, uint i_first)
{
  uint16_t attr_mask = batch->interface->enabled_attr_mask;

  /* Reverse order so first VBO'S have more prevalence (in term of attribute override). */
  for (int v = GPU_BATCH_VBO_MAX_LEN - 1; v > -1; v--) {
    if (batch->verts[v] != NULL) {
      create_bindings(batch->verts[v], batch->interface, &attr_mask, 0, false);
    }
  }

  for (int v = GPU_BATCH_INST_VBO_MAX_LEN - 1; v > -1; v--) {
    if (batch->inst[v]) {
      create_bindings(batch->inst[v], batch->interface, &attr_mask, i_first, true);
    }
  }

  if (attr_mask != 0 && GLEW_ARB_vertex_attrib_binding) {
    for (uint16_t mask = 1, a = 0; a < 16; a++, mask <<= 1) {
      if (attr_mask & mask) {
        /* This replaces glVertexAttrib4f(a, 0.0f, 0.0f, 0.0f, 1.0f); with a more modern style.
         * Fix issues for some drivers (see T75069). */
        glBindVertexBuffer(a, g_default_attr_vbo, (intptr_t)0, (intptr_t)0);

        glEnableVertexAttribArray(a);
        glVertexAttribFormat(a, 4, GL_FLOAT, GL_FALSE, 0);
        glVertexAttribBinding(a, a);
      }
    }
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

static void *elem_offset(const GPUIndexBuf *el, int v_first)
{
#if GPU_TRACK_INDEX_RANGE
  if (el->index_type == GPU_INDEX_U16) {
    return (GLushort *)0 + v_first + el->index_start;
  }
#endif
  return (GLuint *)0 + v_first + el->index_start;
}

/* Use when drawing with GPU_batch_draw_advanced */
void GPU_batch_bind(GPUBatch *batch)
{
  glBindVertexArray(batch->vao_id);

#if GPU_TRACK_INDEX_RANGE
  /* Can be removed if GL 4.3 is required. */
  if (!GLEW_ARB_ES3_compatibility && batch->elem != NULL) {
    GLuint restart_index = (batch->elem->index_type == GPU_INDEX_U16) ? (GLuint)0xFFFF :
                                                                        (GLuint)0xFFFFFFFF;
    glPrimitiveRestartIndex(restart_index);
  }
#endif
}

void GPU_batch_draw(GPUBatch *batch)
{
#if TRUST_NO_ONE
  assert(batch->phase == GPU_BATCH_READY_TO_DRAW);
  assert(batch->verts[0]->vbo_id != 0);
#endif
  GPU_batch_program_use_begin(batch);
  GPU_matrix_bind(batch->interface);  // external call.
  GPU_shader_set_srgb_uniform(batch->interface);

  GPU_batch_bind(batch);
  GPU_batch_draw_advanced(batch, 0, 0, 0, 0);

  GPU_batch_program_use_end(batch);
}

#if GPU_TRACK_INDEX_RANGE
#  define BASE_INDEX(el) ((el)->base_index)
#  define INDEX_TYPE(el) ((el)->gl_index_type)
#else
#  define BASE_INDEX(el) 0
#  define INDEX_TYPE(el) GL_UNSIGNED_INT
#endif

void GPU_batch_draw_advanced(GPUBatch *batch, int v_first, int v_count, int i_first, int i_count)
{
  BLI_assert(batch->program_in_use);
  /* TODO could assert that VAO is bound. */

  if (v_count == 0) {
    v_count = (batch->elem) ? batch->elem->index_len : batch->verts[0]->vertex_len;
  }
  if (i_count == 0) {
    i_count = (batch->inst[0]) ? batch->inst[0]->vertex_len : 1;
    /* Meh. This is to be able to use different numbers of verts in instance vbos. */
    if (batch->inst[1] && i_count > batch->inst[1]->vertex_len) {
      i_count = batch->inst[1]->vertex_len;
    }
  }

  if (v_count == 0 || i_count == 0) {
    /* Nothing to draw. */
    return;
  }

  /* Verify there is enough data do draw. */
  /* TODO(fclem) Nice to have but this is invalid when using procedural draw-calls.
   * The right assert would be to check if there is an enabled attribute from each VBO
   * and check their length. */
  // BLI_assert(i_first + i_count <= (batch->inst ? batch->inst->vertex_len : INT_MAX));
  // BLI_assert(v_first + v_count <=
  //            (batch->elem ? batch->elem->index_len : batch->verts[0]->vertex_len));

#ifdef __APPLE__
  GLuint vao = 0;
#endif

  if (!GPU_arb_base_instance_is_supported()) {
    if (i_first > 0) {
#ifdef __APPLE__
      /**
       * There seems to be a nasty bug when drawing using the same VAO reconfiguring. (see T71147)
       * We just use a throwaway VAO for that. Note that this is likely to degrade performance.
       **/
      glGenVertexArrays(1, &vao);
      glBindVertexArray(vao);
#else
      /* If using offset drawing with instancing, we must
       * use the default VAO and redo bindings. */
      glBindVertexArray(GPU_vao_default());
#endif
      batch_update_program_bindings(batch, i_first);
    }
    else {
      /* Previous call could have bind the default vao
       * see above. */
      glBindVertexArray(batch->vao_id);
    }
  }

  if (batch->elem) {
    const GPUIndexBuf *el = batch->elem;
    GLenum index_type = INDEX_TYPE(el);
    GLint base_index = BASE_INDEX(el);
    void *v_first_ofs = elem_offset(el, v_first);

    if (GPU_arb_base_instance_is_supported()) {
      glDrawElementsInstancedBaseVertexBaseInstance(
          batch->gl_prim_type, v_count, index_type, v_first_ofs, i_count, base_index, i_first);
    }
    else {
      glDrawElementsInstancedBaseVertex(
          batch->gl_prim_type, v_count, index_type, v_first_ofs, i_count, base_index);
    }
  }
  else {
#ifdef __APPLE__
    glDisable(GL_PRIMITIVE_RESTART);
#endif
    if (GPU_arb_base_instance_is_supported()) {
      glDrawArraysInstancedBaseInstance(batch->gl_prim_type, v_first, v_count, i_count, i_first);
    }
    else {
      glDrawArraysInstanced(batch->gl_prim_type, v_first, v_count, i_count);
    }
#ifdef __APPLE__
    glEnable(GL_PRIMITIVE_RESTART);
#endif
  }

#ifdef __APPLE__
  if (vao != 0) {
    glDeleteVertexArrays(1, &vao);
  }
#endif
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
/** \name Indirect Draw Calls
 * \{ */

#if 0
#  define USE_MULTI_DRAW_INDIRECT 0
#else
#  define USE_MULTI_DRAW_INDIRECT \
    (GL_ARB_multi_draw_indirect && GPU_arb_base_instance_is_supported())
#endif

typedef struct GPUDrawCommand {
  uint v_count;
  uint i_count;
  uint v_first;
  uint i_first;
} GPUDrawCommand;

typedef struct GPUDrawCommandIndexed {
  uint v_count;
  uint i_count;
  uint v_first;
  uint base_index;
  uint i_first;
} GPUDrawCommandIndexed;

struct GPUDrawList {
  GPUBatch *batch;
  uint base_index;  /* Avoid dereferencing batch. */
  uint cmd_offset;  /* in bytes, offset  inside indirect command buffer. */
  uint cmd_len;     /* Number of used command for the next call. */
  uint buffer_size; /* in bytes, size of indirect command buffer. */
  GLuint buffer_id; /* Draw Indirect Buffer id */
  union {
    GPUDrawCommand *commands;
    GPUDrawCommandIndexed *commands_indexed;
  };
};

GPUDrawList *GPU_draw_list_create(int length)
{
  GPUDrawList *list = MEM_callocN(sizeof(GPUDrawList), "GPUDrawList");
  /* Alloc the biggest possible command list which is indexed. */
  list->buffer_size = sizeof(GPUDrawCommandIndexed) * length;
  if (USE_MULTI_DRAW_INDIRECT) {
    list->buffer_id = GPU_buf_alloc();
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, list->buffer_id);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, list->buffer_size, NULL, GL_DYNAMIC_DRAW);
  }
  else {
    list->commands = MEM_mallocN(list->buffer_size, "GPUDrawList data");
  }
  return list;
}

void GPU_draw_list_discard(GPUDrawList *list)
{
  if (list->buffer_id) {
    GPU_buf_free(list->buffer_id);
  }
  else {
    MEM_SAFE_FREE(list->commands);
  }
  MEM_freeN(list);
}

void GPU_draw_list_init(GPUDrawList *list, GPUBatch *batch)
{
  BLI_assert(batch->phase == GPU_BATCH_READY_TO_DRAW);
  list->batch = batch;
  list->base_index = batch->elem ? BASE_INDEX(batch->elem) : UINT_MAX;
  list->cmd_len = 0;

  if (USE_MULTI_DRAW_INDIRECT) {
    if (list->commands == NULL) {
      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, list->buffer_id);
      if (list->cmd_offset >= list->buffer_size) {
        /* Orphan buffer data and start fresh. */
        glBufferData(GL_DRAW_INDIRECT_BUFFER, list->buffer_size, NULL, GL_DYNAMIC_DRAW);
        list->cmd_offset = 0;
      }
      GLenum flags = GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
      list->commands = glMapBufferRange(
          GL_DRAW_INDIRECT_BUFFER, list->cmd_offset, list->buffer_size - list->cmd_offset, flags);
    }
  }
  else {
    list->cmd_offset = 0;
  }
}

void GPU_draw_list_command_add(
    GPUDrawList *list, int v_first, int v_count, int i_first, int i_count)
{
  BLI_assert(list->commands);

  if (v_count == 0 || i_count == 0) {
    return;
  }

  if (list->base_index != UINT_MAX) {
    GPUDrawCommandIndexed *cmd = list->commands_indexed + list->cmd_len;
    cmd->v_first = v_first;
    cmd->v_count = v_count;
    cmd->i_count = i_count;
    cmd->base_index = list->base_index;
    cmd->i_first = i_first;
  }
  else {
    GPUDrawCommand *cmd = list->commands + list->cmd_len;
    cmd->v_first = v_first;
    cmd->v_count = v_count;
    cmd->i_count = i_count;
    cmd->i_first = i_first;
  }

  list->cmd_len++;
  uint offset = list->cmd_offset + list->cmd_len * sizeof(GPUDrawCommandIndexed);

  if (offset == list->buffer_size) {
    GPU_draw_list_submit(list);
    GPU_draw_list_init(list, list->batch);
  }
}

void GPU_draw_list_submit(GPUDrawList *list)
{
  GPUBatch *batch = list->batch;

  if (list->cmd_len == 0) {
    return;
  }

  BLI_assert(list->commands);
  BLI_assert(batch->program_in_use);
  /* TODO could assert that VAO is bound. */

  /* TODO We loose a bit of memory here if we only draw arrays. Fix that. */
  uintptr_t offset = list->cmd_offset;
  uint cmd_len = list->cmd_len;
  size_t bytes_used = cmd_len * sizeof(GPUDrawCommandIndexed);
  list->cmd_len = 0; /* Avoid reuse. */

  /* Only do multi-draw indirect if doing more than 2 drawcall.
   * This avoids the overhead of buffer mapping if scene is
   * not very instance friendly.
   * BUT we also need to take into account the case where only
   * a few instances are needed to finish filling a call buffer. */
  const bool do_mdi = (cmd_len > 2) || (list->cmd_offset + bytes_used == list->buffer_size);

  if (USE_MULTI_DRAW_INDIRECT && do_mdi) {
    GLenum prim = batch->gl_prim_type;

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, list->buffer_id);
    glFlushMappedBufferRange(GL_DRAW_INDIRECT_BUFFER, 0, bytes_used);
    glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);
    list->commands = NULL; /* Unmapped */
    list->cmd_offset += bytes_used;

    if (batch->elem) {
      glMultiDrawElementsIndirect(prim, INDEX_TYPE(batch->elem), (void *)offset, cmd_len, 0);
    }
    else {
      glMultiDrawArraysIndirect(prim, (void *)offset, cmd_len, 0);
    }
  }
  else {
    /* Fallback */
    if (batch->elem) {
      GPUDrawCommandIndexed *cmd = list->commands_indexed;
      for (int i = 0; i < cmd_len; i++, cmd++) {
        /* Index start was added by Draw manager. Avoid counting it twice. */
        cmd->v_first -= batch->elem->index_start;
        GPU_batch_draw_advanced(batch, cmd->v_first, cmd->v_count, cmd->i_first, cmd->i_count);
      }
    }
    else {
      GPUDrawCommand *cmd = list->commands;
      for (int i = 0; i < cmd_len; i++, cmd++) {
        GPU_batch_draw_advanced(batch, cmd->v_first, cmd->v_count, cmd->i_first, cmd->i_count);
      }
    }
  }
}

/** \} */

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

/* Bind program bound to IMM to the batch.
 * XXX Use this with much care. Drawing with the GPUBatch API is not compatible with IMM.
 * DO NOT DRAW WITH THE BATCH BEFORE CALLING immUnbindProgram. */
void GPU_batch_program_set_imm_shader(GPUBatch *batch)
{
  GLuint program;
  GPUShaderInterface *interface;
  immGetProgram(&program, &interface);
  GPU_batch_program_set(batch, program, interface);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init/Exit
 * \{ */

void gpu_batch_init(void)
{
  if (g_default_attr_vbo == 0) {
    g_default_attr_vbo = GPU_buf_alloc();

    float default_attrib_data[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    glBindBuffer(GL_ARRAY_BUFFER, g_default_attr_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4, default_attrib_data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  gpu_batch_presets_init();
}

void gpu_batch_exit(void)
{
  GPU_buf_free(g_default_attr_vbo);
  g_default_attr_vbo = 0;

  gpu_batch_presets_exit();
}

/** \} */
