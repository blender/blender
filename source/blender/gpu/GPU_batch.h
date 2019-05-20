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

#ifndef __GPU_BATCH_H__
#define __GPU_BATCH_H__

#include "GPU_vertex_buffer.h"
#include "GPU_element.h"
#include "GPU_shader_interface.h"
#include "GPU_shader.h"

typedef enum {
  GPU_BATCH_UNUSED,
  GPU_BATCH_READY_TO_FORMAT,
  GPU_BATCH_READY_TO_BUILD,
  GPU_BATCH_BUILDING,
  GPU_BATCH_READY_TO_DRAW,
} GPUBatchPhase;

#define GPU_BATCH_VBO_MAX_LEN 4
#define GPU_BATCH_VAO_STATIC_LEN 3
#define GPU_BATCH_VAO_DYN_ALLOC_COUNT 16

typedef struct GPUBatch {
  /* geometry */

  /** verts[0] is required, others can be NULL */
  GPUVertBuf *verts[GPU_BATCH_VBO_MAX_LEN];
  /** Instance attributes. */
  GPUVertBuf *inst;
  /** NULL if element list not needed */
  GPUIndexBuf *elem;
  uint32_t gl_prim_type;

  /* cached values (avoid dereferencing later) */
  uint32_t vao_id;
  uint32_t program;
  const struct GPUShaderInterface *interface;

  /* book-keeping */
  uint owns_flag;
  /** used to free all vaos. this implies all vaos were created under the same context. */
  struct GPUContext *context;
  GPUBatchPhase phase;
  bool program_in_use;

  /* Vao management: remembers all geometry state (vertex attribute bindings & element buffer)
   * for each shader interface. Start with a static number of vaos and fallback to dynamic count
   * if necessary. Once a batch goes dynamic it does not go back. */
  bool is_dynamic_vao_count;
  union {
    /** Static handle count */
    struct {
      const struct GPUShaderInterface *interfaces[GPU_BATCH_VAO_STATIC_LEN];
      uint32_t vao_ids[GPU_BATCH_VAO_STATIC_LEN];
    } static_vaos;
    /** Dynamic handle count */
    struct {
      uint count;
      const struct GPUShaderInterface **interfaces;
      uint32_t *vao_ids;
    } dynamic_vaos;
  };

  /* XXX This is the only solution if we want to have some data structure using
   * batches as key to identify nodes. We must destroy these nodes with this callback. */
  void (*free_callback)(struct GPUBatch *, void *);
  void *callback_data;
} GPUBatch;

enum {
  GPU_BATCH_OWNS_VBO = (1 << 0),
  /* each vbo index gets bit-shifted */
  GPU_BATCH_OWNS_INSTANCES = (1 << 30),
  GPU_BATCH_OWNS_INDEX = (1u << 31u),
};

GPUBatch *GPU_batch_create_ex(GPUPrimType, GPUVertBuf *, GPUIndexBuf *, uint owns_flag);
void GPU_batch_init_ex(GPUBatch *, GPUPrimType, GPUVertBuf *, GPUIndexBuf *, uint owns_flag);
void GPU_batch_copy(GPUBatch *batch_dst, GPUBatch *batch_src);

#define GPU_batch_create(prim, verts, elem) GPU_batch_create_ex(prim, verts, elem, 0)
#define GPU_batch_init(batch, prim, verts, elem) GPU_batch_init_ex(batch, prim, verts, elem, 0)

void GPU_batch_clear(
    GPUBatch *); /* Same as discard but does not free. (does not clal free callback) */
void GPU_batch_discard(GPUBatch *); /* verts & elem are not discarded */

void GPU_batch_vao_cache_clear(GPUBatch *);

void GPU_batch_callback_free_set(GPUBatch *, void (*callback)(GPUBatch *, void *), void *);

void GPU_batch_instbuf_set(GPUBatch *, GPUVertBuf *, bool own_vbo); /* Instancing */

int GPU_batch_vertbuf_add_ex(GPUBatch *, GPUVertBuf *, bool own_vbo);

#define GPU_batch_vertbuf_add(batch, verts) GPU_batch_vertbuf_add_ex(batch, verts, false)

void GPU_batch_program_set_no_use(GPUBatch *, uint32_t program, const GPUShaderInterface *);
void GPU_batch_program_set(GPUBatch *, uint32_t program, const GPUShaderInterface *);
void GPU_batch_program_set_shader(GPUBatch *, GPUShader *shader);
void GPU_batch_program_set_builtin(GPUBatch *batch, eGPUBuiltinShader shader_id);
void GPU_batch_program_set_builtin_with_config(GPUBatch *batch,
                                               eGPUBuiltinShader shader_id,
                                               eGPUShaderConfig sh_cfg);
/* Entire batch draws with one shader program, but can be redrawn later with another program. */
/* Vertex shader's inputs must be compatible with the batch's vertex format. */

void GPU_batch_program_use_begin(GPUBatch *); /* call before Batch_Uniform (temp hack?) */
void GPU_batch_program_use_end(GPUBatch *);

void GPU_batch_uniform_1ui(GPUBatch *, const char *name, uint value);
void GPU_batch_uniform_1i(GPUBatch *, const char *name, int value);
void GPU_batch_uniform_1b(GPUBatch *, const char *name, bool value);
void GPU_batch_uniform_1f(GPUBatch *, const char *name, float value);
void GPU_batch_uniform_2f(GPUBatch *, const char *name, float x, float y);
void GPU_batch_uniform_3f(GPUBatch *, const char *name, float x, float y, float z);
void GPU_batch_uniform_4f(GPUBatch *, const char *name, float x, float y, float z, float w);
void GPU_batch_uniform_2fv(GPUBatch *, const char *name, const float data[2]);
void GPU_batch_uniform_3fv(GPUBatch *, const char *name, const float data[3]);
void GPU_batch_uniform_4fv(GPUBatch *, const char *name, const float data[4]);
void GPU_batch_uniform_2fv_array(GPUBatch *, const char *name, int len, const float *data);
void GPU_batch_uniform_4fv_array(GPUBatch *, const char *name, int len, const float *data);
void GPU_batch_uniform_mat4(GPUBatch *, const char *name, const float data[4][4]);

void GPU_batch_draw(GPUBatch *);

/* This does not bind/unbind shader and does not call GPU_matrix_bind() */
void GPU_batch_draw_range_ex(GPUBatch *, int v_first, int v_count, bool force_instance);

/* Does not even need batch */
void GPU_draw_primitive(GPUPrimType, int v_count);

#if 0 /* future plans */

/* Can multiple batches share a GPUVertBuf? Use ref count? */

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

#endif /* __GPU_BATCH_H__ */
