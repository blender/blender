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

#include "BLI_math_base.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_matrix.h"
#include "GPU_platform.h"
#include "GPU_shader.h"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_index_buffer_private.hh"
#include "gpu_shader_private.hh"
#include "gpu_vertex_buffer_private.hh"

#include "gpu_batch_private.hh"

#include <string.h>

using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

GPUBatch *GPU_batch_calloc(void)
{
  GPUBatch *batch = GPUBackend::get()->batch_alloc();
  memset(batch, 0, sizeof(*batch));
  return batch;
}

GPUBatch *GPU_batch_create_ex(GPUPrimType prim_type,
                              GPUVertBuf *verts,
                              GPUIndexBuf *elem,
                              eGPUBatchFlag owns_flag)
{
  GPUBatch *batch = GPU_batch_calloc();
  GPU_batch_init_ex(batch, prim_type, verts, elem, owns_flag);
  return batch;
}

void GPU_batch_init_ex(GPUBatch *batch,
                       GPUPrimType prim_type,
                       GPUVertBuf *verts,
                       GPUIndexBuf *elem,
                       eGPUBatchFlag owns_flag)
{
  BLI_assert(verts != nullptr);
  /* Do not pass any other flag */
  BLI_assert((owns_flag & ~(GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX)) == 0);

  batch->verts[0] = verts;
  for (int v = 1; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    batch->verts[v] = nullptr;
  }
  for (int v = 0; v < GPU_BATCH_INST_VBO_MAX_LEN; v++) {
    batch->inst[v] = nullptr;
  }
  batch->elem = elem;
  batch->prim_type = prim_type;
  batch->flag = owns_flag | GPU_BATCH_INIT | GPU_BATCH_DIRTY;
  batch->shader = nullptr;
}

/* This will share the VBOs with the new batch. */
void GPU_batch_copy(GPUBatch *batch_dst, GPUBatch *batch_src)
{
  GPU_batch_init_ex(
      batch_dst, GPU_PRIM_POINTS, batch_src->verts[0], batch_src->elem, GPU_BATCH_INVALID);

  batch_dst->prim_type = batch_src->prim_type;
  for (int v = 1; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    batch_dst->verts[v] = batch_src->verts[v];
  }
}

void GPU_batch_clear(GPUBatch *batch)
{
  if (batch->flag & GPU_BATCH_OWNS_INDEX) {
    GPU_indexbuf_discard(batch->elem);
  }
  if (batch->flag & GPU_BATCH_OWNS_VBO_ANY) {
    for (int v = 0; (v < GPU_BATCH_VBO_MAX_LEN) && batch->verts[v]; v++) {
      if (batch->flag & (GPU_BATCH_OWNS_VBO << v)) {
        GPU_VERTBUF_DISCARD_SAFE(batch->verts[v]);
      }
    }
  }
  if (batch->flag & GPU_BATCH_OWNS_INST_VBO_ANY) {
    for (int v = 0; (v < GPU_BATCH_INST_VBO_MAX_LEN) && batch->inst[v]; v++) {
      if (batch->flag & (GPU_BATCH_OWNS_INST_VBO << v)) {
        GPU_VERTBUF_DISCARD_SAFE(batch->inst[v]);
      }
    }
  }
  batch->flag = GPU_BATCH_INVALID;
}

void GPU_batch_discard(GPUBatch *batch)
{
  GPU_batch_clear(batch);

  delete static_cast<Batch *>(batch);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Buffers Management
 * \{ */

/* NOTE: Override ONLY the first instance vbo (and free them if owned). */
void GPU_batch_instbuf_set(GPUBatch *batch, GPUVertBuf *inst, bool own_vbo)
{
  BLI_assert(inst);
  batch->flag |= GPU_BATCH_DIRTY;

  if (batch->inst[0] && (batch->flag & GPU_BATCH_OWNS_INST_VBO)) {
    GPU_vertbuf_discard(batch->inst[0]);
  }
  batch->inst[0] = inst;

  SET_FLAG_FROM_TEST(batch->flag, own_vbo, GPU_BATCH_OWNS_INST_VBO);
}

/* NOTE: Override any previously assigned elem (and free it if owned). */
void GPU_batch_elembuf_set(GPUBatch *batch, GPUIndexBuf *elem, bool own_ibo)
{
  BLI_assert(elem);
  batch->flag |= GPU_BATCH_DIRTY;

  if (batch->elem && (batch->flag & GPU_BATCH_OWNS_INDEX)) {
    GPU_indexbuf_discard(batch->elem);
  }
  batch->elem = elem;

  SET_FLAG_FROM_TEST(batch->flag, own_ibo, GPU_BATCH_OWNS_INDEX);
}

int GPU_batch_instbuf_add_ex(GPUBatch *batch, GPUVertBuf *insts, bool own_vbo)
{
  BLI_assert(insts);
  batch->flag |= GPU_BATCH_DIRTY;

  for (uint v = 0; v < GPU_BATCH_INST_VBO_MAX_LEN; v++) {
    if (batch->inst[v] == nullptr) {
      /* for now all VertexBuffers must have same vertex_len */
      if (batch->inst[0]) {
        /* Allow for different size of vertex buffer (will choose the smallest number of verts). */
        // BLI_assert(insts->vertex_len == batch->inst[0]->vertex_len);
      }

      batch->inst[v] = insts;
      SET_FLAG_FROM_TEST(batch->flag, own_vbo, (eGPUBatchFlag)(GPU_BATCH_OWNS_INST_VBO << v));
      return v;
    }
  }
  /* we only make it this far if there is no room for another GPUVertBuf */
  BLI_assert(0 && "Not enough Instance VBO slot in batch");
  return -1;
}

/* Returns the index of verts in the batch. */
int GPU_batch_vertbuf_add_ex(GPUBatch *batch, GPUVertBuf *verts, bool own_vbo)
{
  BLI_assert(verts);
  batch->flag |= GPU_BATCH_DIRTY;

  for (uint v = 0; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    if (batch->verts[v] == nullptr) {
      /* for now all VertexBuffers must have same vertex_len */
      if (batch->verts[0] != nullptr) {
        /* This is an issue for the HACK inside DRW_vbo_request(). */
        // BLI_assert(verts->vertex_len == batch->verts[0]->vertex_len);
      }
      batch->verts[v] = verts;
      SET_FLAG_FROM_TEST(batch->flag, own_vbo, (eGPUBatchFlag)(GPU_BATCH_OWNS_VBO << v));
      return v;
    }
  }
  /* we only make it this far if there is no room for another GPUVertBuf */
  BLI_assert(0 && "Not enough VBO slot in batch");
  return -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform setters
 *
 * TODO(fclem): port this to GPUShader.
 * \{ */

void GPU_batch_set_shader(GPUBatch *batch, GPUShader *shader)
{
  batch->shader = shader;
  GPU_shader_bind(batch->shader);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing / Drawcall functions
 * \{ */

void GPU_batch_draw(GPUBatch *batch)
{
  GPU_shader_bind(batch->shader);
  GPU_batch_draw_advanced(batch, 0, 0, 0, 0);
}

void GPU_batch_draw_range(GPUBatch *batch, int v_first, int v_count)
{
  GPU_shader_bind(batch->shader);
  GPU_batch_draw_advanced(batch, v_first, v_count, 0, 0);
}

/* Draw multiple instance of a batch without having any instance attributes. */
void GPU_batch_draw_instanced(GPUBatch *batch, int i_count)
{
  BLI_assert(batch->inst[0] == nullptr);

  GPU_shader_bind(batch->shader);
  GPU_batch_draw_advanced(batch, 0, 0, 0, i_count);
}

void GPU_batch_draw_advanced(
    GPUBatch *gpu_batch, int v_first, int v_count, int i_first, int i_count)
{
  BLI_assert(Context::get()->shader != nullptr);
  Batch *batch = static_cast<Batch *>(gpu_batch);

  if (v_count == 0) {
    if (batch->elem) {
      v_count = batch->elem_()->index_len_get();
    }
    else {
      v_count = batch->verts_(0)->vertex_len;
    }
  }
  if (i_count == 0) {
    i_count = (batch->inst[0]) ? batch->inst_(0)->vertex_len : 1;
    /* Meh. This is to be able to use different numbers of verts in instance vbos. */
    if (batch->inst[1] != nullptr) {
      i_count = min_ii(i_count, batch->inst_(1)->vertex_len);
    }
  }

  if (v_count == 0 || i_count == 0) {
    /* Nothing to draw. */
    return;
  }

  batch->draw(v_first, v_count, i_first, i_count);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

void GPU_batch_program_set_builtin_with_config(GPUBatch *batch,
                                               eGPUBuiltinShader shader_id,
                                               eGPUShaderConfig sh_cfg)
{
  GPUShader *shader = GPU_shader_get_builtin_shader_with_config(shader_id, sh_cfg);
  GPU_batch_set_shader(batch, shader);
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
  GPU_batch_set_shader(batch, immGetShader());
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
