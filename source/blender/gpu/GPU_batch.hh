/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU geometry batch.
 *
 * Contains Vertex Buffers, Index Buffers, and Shader reference, altogether representing a drawable
 * entity. It is meant to be used for drawing large (> 1000 vertices) reusable (drawn multiple
 * times) model with complex data layout. In other words, it is meant for all cases where the
 * immediate drawing module (imm) is inadequate.
 *
 * Vertex & Index Buffers can be owned by a batch. In such case they will be freed when the batch
 * gets cleared or discarded.
 */

#pragma once

#include "BLI_utildefines.h"

#include "GPU_index_buffer.hh"
#include "GPU_shader.hh"
#include "GPU_storage_buffer.hh"
#include "GPU_uniform_buffer.hh"
#include "GPU_vertex_buffer.hh"

struct GPUShader;

#define GPU_BATCH_VBO_MAX_LEN 16
#define GPU_BATCH_INST_VBO_MAX_LEN 2
#define GPU_BATCH_VAO_STATIC_LEN 3
#define GPU_BATCH_VAO_DYN_ALLOC_COUNT 16

enum eGPUBatchFlag {
  /** Invalid default state. */
  GPU_BATCH_INVALID = 0,

  /** blender::gpu::VertBuf ownership. (One bit per vbo) */
  GPU_BATCH_OWNS_VBO = (1 << 0),
  GPU_BATCH_OWNS_VBO_MAX = (GPU_BATCH_OWNS_VBO << (GPU_BATCH_VBO_MAX_LEN - 1)),
  GPU_BATCH_OWNS_VBO_ANY = ((GPU_BATCH_OWNS_VBO << GPU_BATCH_VBO_MAX_LEN) - 1),
  /** Instance blender::gpu::VertBuf ownership. (One bit per vbo) */
  GPU_BATCH_OWNS_INST_VBO = (GPU_BATCH_OWNS_VBO_MAX << 1),
  GPU_BATCH_OWNS_INST_VBO_MAX = (GPU_BATCH_OWNS_INST_VBO << (GPU_BATCH_INST_VBO_MAX_LEN - 1)),
  GPU_BATCH_OWNS_INST_VBO_ANY = ((GPU_BATCH_OWNS_INST_VBO << GPU_BATCH_INST_VBO_MAX_LEN) - 1) &
                                ~GPU_BATCH_OWNS_VBO_ANY,
  /** blender::gpu::IndexBuf ownership. */
  GPU_BATCH_OWNS_INDEX = (GPU_BATCH_OWNS_INST_VBO_MAX << 1),

  /** Has been initialized. At least one VBO is set. */
  GPU_BATCH_INIT = (1 << 26),
  /** Batch is initialized but its VBOs are still being populated. (optional) */
  GPU_BATCH_BUILDING = (1 << 26),
  /** Cached data need to be rebuild. (VAO, PSO, ...) */
  GPU_BATCH_DIRTY = (1 << 27),
};

#define GPU_BATCH_OWNS_NONE GPU_BATCH_INVALID

BLI_STATIC_ASSERT(GPU_BATCH_OWNS_INDEX < GPU_BATCH_INIT,
                  "eGPUBatchFlag: Error: status flags are shadowed by the ownership bits!")

ENUM_OPERATORS(eGPUBatchFlag, GPU_BATCH_DIRTY)

namespace blender::gpu {

/**
 * Base class which is then specialized for each implementation (GL, VK, ...).
 * TODO(fclem): Make the content of this struct hidden and expose getters/setters.
 *
 * Do not allocate manually as the real struct is bigger (i.e: GLBatch). This is only
 * the common and "public" part of the struct. Use `GPU_batch_calloc()` and `GPU_batch_create()`
 * instead.
 */
class Batch {
 public:
  /** verts[0] is required, others can be nullptr */
  blender::gpu::VertBuf *verts[GPU_BATCH_VBO_MAX_LEN];
  /** Instance attributes. */
  blender::gpu::VertBuf *inst[GPU_BATCH_INST_VBO_MAX_LEN];
  /** nullptr if element list not needed */
  blender::gpu::IndexBuf *elem;
  /** Resource ID attribute workaround. */
  GPUStorageBuf *resource_id_buf;
  /** Bookkeeping. */
  eGPUBatchFlag flag;
  /** Type of geometry to draw. */
  GPUPrimType prim_type;
  /** Current assigned shader. DEPRECATED. Here only for uniform binding. */
  GPUShader *shader;

  virtual ~Batch() = default;

  virtual void draw(int v_first, int v_count, int i_first, int i_count) = 0;
  virtual void draw_indirect(GPUStorageBuf *indirect_buf, intptr_t offset) = 0;
  virtual void multi_draw_indirect(GPUStorageBuf *indirect_buf,
                                   int count,
                                   intptr_t offset,
                                   intptr_t stride) = 0;

  /* Convenience casts. */
  IndexBuf *elem_() const
  {
    return elem;
  }
  VertBuf *verts_(const int index) const
  {
    return verts[index];
  }
  VertBuf *inst_(const int index) const
  {
    return inst[index];
  }
};

}  // namespace blender::gpu

/* -------------------------------------------------------------------- */
/** \name Creation
 * \{ */

/**
 * Allocate a #blender::gpu::Batch with a cleared state.
 * The returned #blender::gpu::Batch needs to be passed to `GPU_batch_init` before being usable.
 */
blender::gpu::Batch *GPU_batch_calloc();

/**
 * Creates a #blender::gpu::Batch with explicit buffer ownership.
 */
blender::gpu::Batch *GPU_batch_create_ex(GPUPrimType primitive_type,
                                         blender::gpu::VertBuf *vertex_buf,
                                         blender::gpu::IndexBuf *index_buf,
                                         eGPUBatchFlag owns_flag);
/**
 * Creates a #blender::gpu::Batch without buffer ownership.
 */
#define GPU_batch_create(primitive_type, vertex_buf, index_buf) \
  GPU_batch_create_ex(primitive_type, vertex_buf, index_buf, (eGPUBatchFlag)0)

/**
 * Initialize a cleared #blender::gpu::Batch with explicit buffer ownership.
 * A #blender::gpu::Batch is in cleared state if it was just allocated using `GPU_batch_calloc()`
 * or cleared using `GPU_batch_clear()`.
 */
void GPU_batch_init_ex(blender::gpu::Batch *batch,
                       GPUPrimType primitive_type,
                       blender::gpu::VertBuf *vertex_buf,
                       blender::gpu::IndexBuf *index_buf,
                       eGPUBatchFlag owns_flag);
/**
 * Initialize a cleared #blender::gpu::Batch without buffer ownership.
 * A #blender::gpu::Batch is in cleared state if it was just allocated using `GPU_batch_calloc()`
 * or cleared using `GPU_batch_clear()`.
 */
#define GPU_batch_init(batch, primitive_type, vertex_buf, index_buf) \
  GPU_batch_init_ex(batch, primitive_type, vertex_buf, index_buf, (eGPUBatchFlag)0)

/**
 * DEPRECATED: It is easy to loose ownership with this. To be removed.
 * This will share the VBOs with the new batch.
 */
void GPU_batch_copy(blender::gpu::Batch *batch_dst, blender::gpu::Batch *batch_src);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deletion
 * \{ */

/**
 * Clear a #blender::gpu::Batch without freeing its own memory.
 * The #blender::gpu::Batch can then be reused using `GPU_batch_init()`.
 * Discards all owned vertex and index buffers.
 */
void GPU_batch_clear(blender::gpu::Batch *batch);

void GPU_batch_zero(blender::gpu::Batch *batch);

#define GPU_BATCH_CLEAR_SAFE(batch) \
  do { \
    if (batch != nullptr) { \
      GPU_batch_clear(batch); \
      GPU_batch_zero(batch); \
    } \
  } while (0)

/**
 * Free a #blender::gpu::Batch object.
 * Discards all owned vertex and index buffers.
 */
void GPU_batch_discard(blender::gpu::Batch *batch);

#define GPU_BATCH_DISCARD_SAFE(batch) \
  do { \
    if (batch != nullptr) { \
      GPU_batch_discard(batch); \
      batch = nullptr; \
    } \
  } while (0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Buffers Management
 * \{ */

/**
 * Add the given \a vertex_buf as vertex buffer to a #blender::gpu::Batch.
 * \return the index of verts in the batch.
 */
int GPU_batch_vertbuf_add(blender::gpu::Batch *batch,
                          blender::gpu::VertBuf *vertex_buf,
                          bool own_vbo);

/**
 * Add the given \a vertex_buf as instanced vertex buffer to a #blender::gpu::Batch.
 * \return the index of verts in the batch.
 */
int GPU_batch_instbuf_add(blender::gpu::Batch *batch,
                          blender::gpu::VertBuf *vertex_buf,
                          bool own_vbo);

/**
 * Set the first instanced vertex buffer of a #blender::gpu::Batch.
 * \note Override ONLY the first instance VBO (and free them if owned).
 */
void GPU_batch_instbuf_set(blender::gpu::Batch *batch,
                           blender::gpu::VertBuf *vertex_buf,
                           bool own_vbo);

/**
 * Set the index buffer of a #blender::gpu::Batch.
 * \note Override any previously assigned index buffer (and free it if owned).
 */
void GPU_batch_elembuf_set(blender::gpu::Batch *batch,
                           blender::gpu::IndexBuf *index_buf,
                           bool own_ibo);

/**
 * Returns true if the #GPUbatch has \a vertex_buf in its vertex buffer list.
 * \note The search is only conducted on the non-instance rate vertex buffer list.
 */
bool GPU_batch_vertbuf_has(const blender::gpu::Batch *batch, blender::gpu::VertBuf *vertex_buf);

/**
 * Set resource id buffer to bind as instance attribute to workaround the lack of gl_BaseInstance
 * on some hardware / platform.
 * \note Only to be used by draw manager.
 */
void GPU_batch_resource_id_buf_set(blender::gpu::Batch *batch, GPUStorageBuf *resource_id_buf);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader Binding & Uniforms
 *
 * TODO(fclem): This whole section should be removed. See the other `TODO`s in this section.
 * This is because we want to remove #blender::gpu::Batch.shader to avoid usage mistakes.
 * Interacting directly with the #GPUShader provide a clearer interface and less error-prone.
 * \{ */

/**
 * Sets the shader to be drawn with this #blender::gpu::Batch.
 * \note This need to be called first for the `GPU_batch_uniform_*` functions to work.
 */
/* TODO(fclem): These should be removed and replaced by `GPU_shader_bind()`. */
void GPU_batch_set_shader(blender::gpu::Batch *batch, GPUShader *shader);
void GPU_batch_program_set_builtin(blender::gpu::Batch *batch, eGPUBuiltinShader shader_id);
void GPU_batch_program_set_builtin_with_config(blender::gpu::Batch *batch,
                                               eGPUBuiltinShader shader_id,
                                               eGPUShaderConfig sh_cfg);
/**
 * Bind program bound to IMM (immediate mode) to the #blender::gpu::Batch.
 *
 * XXX: Use this with much care. Drawing with the #blender::gpu::Batch API is not compatible with
 * IMM. DO NOT DRAW WITH THE BATCH BEFORE CALLING #immUnbindProgram.
 */
void GPU_batch_program_set_imm_shader(blender::gpu::Batch *batch);

/**
 * Set uniform variables for the shader currently bound to the #blender::gpu::Batch.
 */
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
  GPU_uniformbuf_bind(ubo, GPU_shader_get_ubo_binding((batch)->shader, name));
#define GPU_batch_texture_bind(batch, name, tex) \
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding((batch)->shader, name));

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader Binding & Uniforms
 * \{ */

/**
 * Draw the #blender::gpu::Batch with vertex count and instance count from its vertex buffers
 * lengths. Ensures the associated shader is bound. TODO(fclem) remove this behavior.
 */
void GPU_batch_draw(blender::gpu::Batch *batch);

/**
 * Draw the #blender::gpu::Batch with vertex count and instance count from its vertex buffers
 * lengths. Ensures the associated shader is bound. TODO(fclem) remove this behavior.
 *
 * A \a vertex_count of 0 will use the default number of vertex.
 * The \a vertex_first sets the start of the instance-rate attributes.
 *
 * \note No out-of-bound access check is made on the vertex buffers since they are tricky to
 * detect. Double check that the range of vertex has data or that the data isn't read by the
 * shader.
 */
void GPU_batch_draw_range(blender::gpu::Batch *batch, int vertex_first, int vertex_count);

/**
 * Draw multiple instances of the #blender::gpu::Batch with custom instance range.
 * Ensures the associated shader is bound. TODO(fclem) remove this behavior.
 *
 * An \a instance_count of 0 will use the default number of instances.
 * The \a instance_first sets the start of the instance-rate attributes.
 *
 * \note this can be used even if the #blender::gpu::Batch contains no instance-rate attributes.
 * \note No out-of-bound access check is made on the vertex buffers since they are tricky to
 * detect. Double check that the range of vertex has data or that the data isn't read by the
 * shader.
 */
void GPU_batch_draw_instance_range(blender::gpu::Batch *batch,
                                   int instance_first,
                                   int instance_count);

/**
 * Draw the #blender::gpu::Batch custom parameters.
 * IMPORTANT: This does not bind/unbind shader and does not call GPU_matrix_bind().
 *
 * A \a vertex_count of 0 will use the default number of vertex.
 * An \a instance_count of 0 will use the default number of instances.
 *
 * \note No out-of-bound access check is made on the vertex buffers since they are tricky to
 * detect. Double check that the range of vertex has data or that the data isn't read by the
 * shader.
 */
void GPU_batch_draw_advanced(blender::gpu::Batch *batch,
                             int vertex_first,
                             int vertex_count,
                             int instance_first,
                             int instance_count);

/**
 * Issue a single draw call using arguments sourced from a #GPUStorageBuf.
 * The argument are expected to be valid for the type of geometry contained by this
 * #blender::gpu::Batch (index or non-indexed).
 *
 * The indirect buffer needs to be synced after filling its contents and before calling this
 * function using `GPU_storagebuf_sync_as_indirect_buffer`.
 *
 * For more info see the GL documentation:
 * https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDrawArraysIndirect.xhtml
 */
void GPU_batch_draw_indirect(blender::gpu::Batch *batch,
                             GPUStorageBuf *indirect_buf,
                             intptr_t offset);

/**
 * Issue \a count draw calls using arguments sourced from a #GPUStorageBuf.
 * The \a stride (in bytes) control the spacing between each command description.
 * The argument are expected to be valid for the type of geometry contained by this
 * #blender::gpu::Batch (index or non-indexed).
 *
 * The indirect buffer needs to be synced after filling its contents and before calling this
 * function using `GPU_storagebuf_sync_as_indirect_buffer`.
 *
 * For more info see the GL documentation:
 * https://registry.khronos.org/OpenGL-Refpages/gl4/html/glMultiDrawArraysIndirect.xhtml
 */
void GPU_batch_multi_draw_indirect(blender::gpu::Batch *batch,
                                   GPUStorageBuf *indirect_buf,
                                   int count,
                                   intptr_t offset,
                                   intptr_t stride);

/**
 * Return indirect draw call parameters for this #blender::gpu::Batch.
 * NOTE: \a r_base_index is set to -1 if not using an index buffer.
 */
void GPU_batch_draw_parameter_get(blender::gpu::Batch *batch,
                                  int *r_vertex_count,
                                  int *r_vertex_first,
                                  int *r_base_index,
                                  int *r_indices_count);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module init/exit
 * \{ */

void gpu_batch_init();
void gpu_batch_exit();

/** \} */
