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

#pragma once

#include "BLI_utildefines.h"

#include "GPU_index_buffer.h"
#include "GPU_shader.h"
#include "GPU_uniform_buffer.h"
#include "GPU_vertex_buffer.h"

#define GPU_BATCH_VBO_MAX_LEN 16
#define GPU_BATCH_INST_VBO_MAX_LEN 2
#define GPU_BATCH_VAO_STATIC_LEN 3
#define GPU_BATCH_VAO_DYN_ALLOC_COUNT 16

typedef enum eGPUBatchFlag {
  /** Invalid default state. */
  GPU_BATCH_INVALID = 0,

  /** GPUVertBuf ownership. (One bit per vbo) */
  GPU_BATCH_OWNS_VBO = (1 << 0),
  GPU_BATCH_OWNS_VBO_MAX = (GPU_BATCH_OWNS_VBO << (GPU_BATCH_VBO_MAX_LEN - 1)),
  GPU_BATCH_OWNS_VBO_ANY = ((GPU_BATCH_OWNS_VBO << GPU_BATCH_VBO_MAX_LEN) - 1),
  /** Instance GPUVertBuf ownership. (One bit per vbo) */
  GPU_BATCH_OWNS_INST_VBO = (GPU_BATCH_OWNS_VBO_MAX << 1),
  GPU_BATCH_OWNS_INST_VBO_MAX = (GPU_BATCH_OWNS_INST_VBO << (GPU_BATCH_INST_VBO_MAX_LEN - 1)),
  GPU_BATCH_OWNS_INST_VBO_ANY = ((GPU_BATCH_OWNS_INST_VBO << GPU_BATCH_INST_VBO_MAX_LEN) - 1) &
                                ~GPU_BATCH_OWNS_VBO_ANY,
  /** GPUIndexBuf ownership. */
  GPU_BATCH_OWNS_INDEX = (GPU_BATCH_OWNS_INST_VBO_MAX << 1),

  /** Has been initialized. At least one VBO is set. */
  GPU_BATCH_INIT = (1 << 26),
  /** Batch is initialized but its VBOs are still being populated. (optional) */
  GPU_BATCH_BUILDING = (1 << 26),
  /** Cached data need to be rebuild. (VAO, PSO, ...) */
  GPU_BATCH_DIRTY = (1 << 27),
} eGPUBatchFlag;

#define GPU_BATCH_OWNS_NONE GPU_BATCH_INVALID

BLI_STATIC_ASSERT(GPU_BATCH_OWNS_INDEX < GPU_BATCH_INIT,
                  "eGPUBatchFlag: Error: status flags are shadowed by the ownership bits!")

ENUM_OPERATORS(eGPUBatchFlag, GPU_BATCH_DIRTY)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * IMPORTANT: Do not allocate manually as the real struct is bigger (i.e: GLBatch). This is only
 * the common and "public" part of the struct. Use the provided allocator.
 * TODO(fclem): Make the content of this struct hidden and expose getters/setters.
 */
typedef struct GPUBatch {
  /** verts[0] is required, others can be NULL */
  GPUVertBuf *verts[GPU_BATCH_VBO_MAX_LEN];
  /** Instance attributes. */
  GPUVertBuf *inst[GPU_BATCH_INST_VBO_MAX_LEN];
  /** NULL if element list not needed */
  GPUIndexBuf *elem;
  /** Bookkeeping. */
  eGPUBatchFlag flag;
  /** Type of geometry to draw. */
  GPUPrimType prim_type;
  /** Current assigned shader. DEPRECATED. Here only for uniform binding. */
  struct GPUShader *shader;
} GPUBatch;

GPUBatch *GPU_batch_calloc(void);
GPUBatch *GPU_batch_create_ex(GPUPrimType prim,
                              GPUVertBuf *vert,
                              GPUIndexBuf *elem,
                              eGPUBatchFlag owns_flag);
void GPU_batch_init_ex(GPUBatch *batch,
                       GPUPrimType prim,
                       GPUVertBuf *vert,
                       GPUIndexBuf *elem,
                       eGPUBatchFlag owns_flag);
/**
 * This will share the VBOs with the new batch.
 */
void GPU_batch_copy(GPUBatch *batch_dst, GPUBatch *batch_src);

#define GPU_batch_create(prim, verts, elem) GPU_batch_create_ex(prim, verts, elem, 0)
#define GPU_batch_init(batch, prim, verts, elem) GPU_batch_init_ex(batch, prim, verts, elem, 0)

/**
 * Same as discard but does not free. (does not call free callback).
 */
void GPU_batch_clear(GPUBatch *);

/**
 * \note Verts & elem are not discarded.
 */
void GPU_batch_discard(GPUBatch *);

/**
 * \note Override ONLY the first instance VBO (and free them if owned).
 */
void GPU_batch_instbuf_set(GPUBatch *, GPUVertBuf *, bool own_vbo); /* Instancing */
/**
 * \note Override any previously assigned elem (and free it if owned).
 */
void GPU_batch_elembuf_set(GPUBatch *batch, GPUIndexBuf *elem, bool own_ibo);

int GPU_batch_instbuf_add_ex(GPUBatch *, GPUVertBuf *, bool own_vbo);
/**
 * Returns the index of verts in the batch.
 */
int GPU_batch_vertbuf_add_ex(GPUBatch *, GPUVertBuf *, bool own_vbo);

#define GPU_batch_vertbuf_add(batch, verts) GPU_batch_vertbuf_add_ex(batch, verts, false)

void GPU_batch_set_shader(GPUBatch *batch, GPUShader *shader);
/**
 * Bind program bound to IMM to the batch.
 *
 * XXX Use this with much care. Drawing with the #GPUBatch API is not compatible with IMM.
 * DO NOT DRAW WITH THE BATCH BEFORE CALLING #immUnbindProgram.
 */
void GPU_batch_program_set_imm_shader(GPUBatch *batch);
void GPU_batch_program_set_builtin(GPUBatch *batch, eGPUBuiltinShader shader_id);
void GPU_batch_program_set_builtin_with_config(GPUBatch *batch,
                                               eGPUBuiltinShader shader_id,
                                               eGPUShaderConfig sh_cfg);

/* Will only work after setting the batch program. */
/* TODO(fclem): These need to be replaced by GPU_shader_uniform_* with explicit shader. */

#define GPU_batch_uniform_1i(batch, name, x) GPU_shader_uniform_1i((batch)->shader, name, x);
#define GPU_batch_uniform_1b(batch, name, x) GPU_shader_uniform_1b((batch)->shader, name, x);
#define GPU_batch_uniform_1f(batch, name, x) GPU_shader_uniform_1f((batch)->shader, name, x);
#define GPU_batch_uniform_2f(batch, name, x, y) GPU_shader_uniform_2f((batch)->shader, name, x, y);
#define GPU_batch_uniform_3f(batch, name, x, y, z) \
  GPU_shader_uniform_3f((batch)->shader, name, x, y, z);
#define GPU_batch_uniform_4f(batch, name, x, y, z, w) \
  GPU_shader_uniform_4f((batch)->shader, name, x, y, z, w);
#define GPU_batch_uniform_2fv(batch, name, val) GPU_shader_uniform_2fv((batch)->shader, name, val);
#define GPU_batch_uniform_3fv(batch, name, val) GPU_shader_uniform_3fv((batch)->shader, name, val);
#define GPU_batch_uniform_4fv(batch, name, val) GPU_shader_uniform_4fv((batch)->shader, name, val);
#define GPU_batch_uniform_2fv_array(batch, name, len, val) \
  GPU_shader_uniform_2fv_array((batch)->shader, name, len, val);
#define GPU_batch_uniform_4fv_array(batch, name, len, val) \
  GPU_shader_uniform_4fv_array((batch)->shader, name, len, val);
#define GPU_batch_uniform_mat4(batch, name, val) \
  GPU_shader_uniform_mat4((batch)->shader, name, val);
#define GPU_batch_uniformbuf_bind(batch, name, ubo) \
  GPU_uniformbuf_bind(ubo, GPU_shader_get_uniform_block_binding((batch)->shader, name));
#define GPU_batch_texture_bind(batch, name, tex) \
  GPU_texture_bind(tex, GPU_shader_get_texture_binding((batch)->shader, name));

void GPU_batch_draw(GPUBatch *batch);
void GPU_batch_draw_range(GPUBatch *batch, int v_first, int v_count);
/**
 * Draw multiple instance of a batch without having any instance attributes.
 */
void GPU_batch_draw_instanced(GPUBatch *batch, int i_count);

/**
 * This does not bind/unbind shader and does not call GPU_matrix_bind().
 */
void GPU_batch_draw_advanced(GPUBatch *, int v_first, int v_count, int i_first, int i_count);

#if 0 /* future plans */

/* Can multiple batches share a #GPUVertBuf? Use ref count? */

/* We often need a batch with its own data, to be created and discarded together. */
/* WithOwn variants reduce number of system allocations. */

typedef struct BatchWithOwnVertexBuffer {
  GPUBatch batch;
  GPUVertBuf verts; /* link batch.verts to this */
} BatchWithOwnVertexBuffer;

typedef struct BatchWithOwnElementList {
  GPUBatch batch;
  GPUIndexBuf elem; /* link batch.elem to this */
} BatchWithOwnElementList;

typedef struct BatchWithOwnVertexBufferAndElementList {
  GPUBatch batch;
  GPUIndexBuf elem; /* link batch.elem to this */
  GPUVertBuf verts; /* link batch.verts to this */
} BatchWithOwnVertexBufferAndElementList;

GPUBatch *create_BatchWithOwnVertexBuffer(GPUPrimType, GPUVertFormat *, uint v_len, GPUIndexBuf *);
GPUBatch *create_BatchWithOwnElementList(GPUPrimType, GPUVertBuf *, uint prim_len);
GPUBatch *create_BatchWithOwnVertexBufferAndElementList(GPUPrimType,
                                                        GPUVertFormat *,
                                                        uint v_len,
                                                        uint prim_len);
/* verts: shared, own */
/* elem: none, shared, own */
GPUBatch *create_BatchInGeneral(GPUPrimType, VertexBufferStuff, ElementListStuff);

#endif /* future plans */

void gpu_batch_init(void);
void gpu_batch_exit(void);

/* Macros */

#define GPU_BATCH_DISCARD_SAFE(batch) \
  do { \
    if (batch != NULL) { \
      GPU_batch_discard(batch); \
      batch = NULL; \
    } \
  } while (0)

#define GPU_BATCH_CLEAR_SAFE(batch) \
  do { \
    if (batch != NULL) { \
      GPU_batch_clear(batch); \
      memset(batch, 0, sizeof(*(batch))); \
    } \
  } while (0)

#define GPU_BATCH_DISCARD_ARRAY_SAFE(_batch_array, _len) \
  do { \
    if (_batch_array != NULL) { \
      BLI_assert(_len > 0); \
      for (int _i = 0; _i < _len; _i++) { \
        GPU_BATCH_DISCARD_SAFE(_batch_array[_i]); \
      } \
      MEM_freeN(_batch_array); \
    } \
  } while (0)

#ifdef __cplusplus
}
#endif
