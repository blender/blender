/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU geometry batch
 * Contains VAOs + VBOs + Shader representing a drawable entity.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_platform.hh"
#include "GPU_shader.hh"

#include "GPU_index_buffer.hh"
#include "GPU_vertex_buffer.hh"
#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_shader_private.hh"

#include "GPU_batch.hh"

#include <cstring>

using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

void GPU_batch_zero(Batch *batch)
{
  std::fill_n(batch->verts, sizeof(GPU_BATCH_VBO_MAX_LEN), nullptr);
  std::fill_n(batch->inst, sizeof(GPU_BATCH_INST_VBO_MAX_LEN), nullptr);
  batch->elem = nullptr;
  batch->resource_id_buf = nullptr;
  batch->flag = eGPUBatchFlag(0);
  batch->prim_type = GPUPrimType(0);
  batch->shader = nullptr;
}

Batch *GPU_batch_calloc()
{
  Batch *batch = GPUBackend::get()->batch_alloc();
  GPU_batch_zero(batch);
  return batch;
}

Batch *GPU_batch_create_ex(GPUPrimType primitive_type,
                           VertBuf *vertex_buf,
                           IndexBuf *index_buf,
                           eGPUBatchFlag owns_flag)
{
  Batch *batch = GPU_batch_calloc();
  GPU_batch_init_ex(batch, primitive_type, vertex_buf, index_buf, owns_flag);
  return batch;
}

void GPU_batch_init_ex(Batch *batch,
                       GPUPrimType primitive_type,
                       VertBuf *vertex_buf,
                       IndexBuf *index_buf,
                       eGPUBatchFlag owns_flag)
{
  /* Do not pass any other flag */
  BLI_assert((owns_flag & ~(GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX)) == 0);
  /* Batch needs to be in cleared state. */
  BLI_assert((batch->flag & GPU_BATCH_INIT) == 0);

  batch->verts[0] = vertex_buf;
  for (int v = 1; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    batch->verts[v] = nullptr;
  }
  for (auto &v : batch->inst) {
    v = nullptr;
  }
  batch->elem = index_buf;
  batch->prim_type = primitive_type;
  batch->flag = owns_flag | GPU_BATCH_INIT | GPU_BATCH_DIRTY;
  batch->shader = nullptr;
}

void GPU_batch_copy(Batch *batch_dst, Batch *batch_src)
{
  GPU_batch_clear(batch_dst);
  GPU_batch_init_ex(
      batch_dst, GPU_PRIM_POINTS, batch_src->verts[0], batch_src->elem, GPU_BATCH_INVALID);

  batch_dst->prim_type = batch_src->prim_type;
  for (int v = 1; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    batch_dst->verts[v] = batch_src->verts[v];
  }
}

void GPU_batch_clear(Batch *batch)
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

void GPU_batch_discard(Batch *batch)
{
  GPU_batch_clear(batch);
  delete batch;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Buffers Management
 * \{ */

void GPU_batch_instbuf_set(Batch *batch, VertBuf *vertex_buf, bool own_vbo)
{
  BLI_assert(vertex_buf);
  batch->flag |= GPU_BATCH_DIRTY;

  if (batch->inst[0] && (batch->flag & GPU_BATCH_OWNS_INST_VBO)) {
    GPU_vertbuf_discard(batch->inst[0]);
  }
  batch->inst[0] = vertex_buf;

  SET_FLAG_FROM_TEST(batch->flag, own_vbo, GPU_BATCH_OWNS_INST_VBO);
}

void GPU_batch_elembuf_set(Batch *batch, blender::gpu::IndexBuf *index_buf, bool own_ibo)
{
  BLI_assert(index_buf);
  batch->flag |= GPU_BATCH_DIRTY;

  if (batch->elem && (batch->flag & GPU_BATCH_OWNS_INDEX)) {
    GPU_indexbuf_discard(batch->elem);
  }
  batch->elem = index_buf;

  SET_FLAG_FROM_TEST(batch->flag, own_ibo, GPU_BATCH_OWNS_INDEX);
}

int GPU_batch_instbuf_add(Batch *batch, VertBuf *vertex_buf, bool own_vbo)
{
  BLI_assert(vertex_buf);
  batch->flag |= GPU_BATCH_DIRTY;

  for (uint v = 0; v < GPU_BATCH_INST_VBO_MAX_LEN; v++) {
    if (batch->inst[v] == nullptr) {
      /* for now all VertexBuffers must have same vertex_len */
      if (batch->inst[0]) {
        /* Allow for different size of vertex buffer (will choose the smallest number of verts). */
        // BLI_assert(insts->vertex_len == batch->inst[0]->vertex_len);
      }

      batch->inst[v] = vertex_buf;
      SET_FLAG_FROM_TEST(batch->flag, own_vbo, (eGPUBatchFlag)(GPU_BATCH_OWNS_INST_VBO << v));
      return v;
    }
  }
  /* we only make it this far if there is no room for another VertBuf */
  BLI_assert_msg(0, "Not enough Instance VBO slot in batch");
  return -1;
}

int GPU_batch_vertbuf_add(Batch *batch, VertBuf *vertex_buf, bool own_vbo)
{
  BLI_assert(vertex_buf);
  batch->flag |= GPU_BATCH_DIRTY;

  for (uint v = 0; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    if (batch->verts[v] == nullptr) {
      /* for now all VertexBuffers must have same vertex_len */
      if (batch->verts[0] != nullptr) {
        /* This is an issue for the HACK inside DRW_vbo_request(). */
        // BLI_assert(verts->vertex_len == batch->verts[0]->vertex_len);
      }
      batch->verts[v] = vertex_buf;
      SET_FLAG_FROM_TEST(batch->flag, own_vbo, (eGPUBatchFlag)(GPU_BATCH_OWNS_VBO << v));
      return v;
    }
  }
  /* we only make it this far if there is no room for another VertBuf */
  BLI_assert_msg(0, "Not enough VBO slot in batch");
  return -1;
}

bool GPU_batch_vertbuf_has(Batch *batch, VertBuf *vertex_buf)
{
  for (uint v = 0; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    if (batch->verts[v] == vertex_buf) {
      return true;
    }
  }
  return false;
}

void GPU_batch_resource_id_buf_set(Batch *batch, GPUStorageBuf *resource_id_buf)
{
  BLI_assert(resource_id_buf);
  batch->flag |= GPU_BATCH_DIRTY;
  batch->resource_id_buf = resource_id_buf;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform setters
 *
 * \{ */

void GPU_batch_set_shader(Batch *batch, GPUShader *shader)
{
  batch->shader = shader;
  GPU_shader_bind(batch->shader);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing / Drawcall functions
 * \{ */

void GPU_batch_draw_parameter_get(Batch *gpu_batch,
                                  int *r_vertex_count,
                                  int *r_vertex_first,
                                  int *r_base_index,
                                  int *r_indices_count)
{
  Batch *batch = static_cast<Batch *>(gpu_batch);

  if (batch->elem) {
    *r_vertex_count = batch->elem_()->index_len_get();
    *r_vertex_first = batch->elem_()->index_start_get();
    *r_base_index = batch->elem_()->index_base_get();
  }
  else {
    *r_vertex_count = batch->verts_(0)->vertex_len;
    *r_vertex_first = 0;
    *r_base_index = -1;
  }

  int i_count = (batch->inst[0]) ? batch->inst_(0)->vertex_len : 1;
  /* Meh. This is to be able to use different numbers of verts in instance VBO's. */
  if (batch->inst[1] != nullptr) {
    i_count = min_ii(i_count, batch->inst_(1)->vertex_len);
  }
  *r_indices_count = i_count;
}

void GPU_batch_draw(Batch *batch)
{
  GPU_shader_bind(batch->shader);
  GPU_batch_draw_advanced(batch, 0, 0, 0, 0);
}

void GPU_batch_draw_range(Batch *batch, int vertex_first, int vertex_count)
{
  GPU_shader_bind(batch->shader);
  GPU_batch_draw_advanced(batch, vertex_first, vertex_count, 0, 0);
}

void GPU_batch_draw_instance_range(Batch *batch, int instance_first, int instance_count)
{
  BLI_assert(batch->inst[0] == nullptr);

  GPU_shader_bind(batch->shader);
  GPU_batch_draw_advanced(batch, 0, 0, instance_first, instance_count);
}

void GPU_batch_draw_advanced(
    Batch *gpu_batch, int vertex_first, int vertex_count, int instance_first, int instance_count)
{
  BLI_assert(Context::get()->shader != nullptr);
  Batch *batch = static_cast<Batch *>(gpu_batch);

  if (vertex_count == 0) {
    if (batch->elem) {
      vertex_count = batch->elem_()->index_len_get();
    }
    else {
      vertex_count = batch->verts_(0)->vertex_len;
    }
  }
  if (instance_count == 0) {
    instance_count = (batch->inst[0]) ? batch->inst_(0)->vertex_len : 1;
    /* Meh. This is to be able to use different numbers of verts in instance VBO's. */
    if (batch->inst[1] != nullptr) {
      instance_count = min_ii(instance_count, batch->inst_(1)->vertex_len);
    }
  }

  if (vertex_count == 0 || instance_count == 0) {
    /* Nothing to draw. */
    return;
  }

  batch->draw(vertex_first, vertex_count, instance_first, instance_count);
}

void GPU_batch_draw_indirect(Batch *gpu_batch, GPUStorageBuf *indirect_buf, intptr_t offset)
{
  BLI_assert(Context::get()->shader != nullptr);
  BLI_assert(indirect_buf != nullptr);
  Batch *batch = static_cast<Batch *>(gpu_batch);

  batch->draw_indirect(indirect_buf, offset);
}

void GPU_batch_multi_draw_indirect(
    Batch *gpu_batch, GPUStorageBuf *indirect_buf, int count, intptr_t offset, intptr_t stride)
{
  BLI_assert(Context::get()->shader != nullptr);
  BLI_assert(indirect_buf != nullptr);
  Batch *batch = static_cast<Batch *>(gpu_batch);

  batch->multi_draw_indirect(indirect_buf, count, offset, stride);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

void GPU_batch_program_set_builtin_with_config(Batch *batch,
                                               eGPUBuiltinShader shader_id,
                                               eGPUShaderConfig sh_cfg)
{
  GPUShader *shader = GPU_shader_get_builtin_shader_with_config(shader_id, sh_cfg);
  GPU_batch_set_shader(batch, shader);
}

void GPU_batch_program_set_builtin(Batch *batch, eGPUBuiltinShader shader_id)
{
  GPU_batch_program_set_builtin_with_config(batch, shader_id, GPU_SHADER_CFG_DEFAULT);
}

void GPU_batch_program_set_imm_shader(Batch *batch)
{
  GPU_batch_set_shader(batch, immGetShader());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init/Exit
 * \{ */

void gpu_batch_init()
{
  gpu_batch_presets_init();
}

void gpu_batch_exit()
{
  gpu_batch_presets_exit();
}

/** \} */
