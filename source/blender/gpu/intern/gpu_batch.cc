/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU geometry batch
 * Contains VAOs + VBOs + Shader representing a drawable entity.
 */

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_shader.hh"

#include "GPU_index_buffer.hh"
#include "GPU_vertex_buffer.hh"
#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_debug_private.hh"
#include "gpu_shader_private.hh"

#include <cstring>

using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

void GPU_batch_zero(Batch *batch)
{
  std::fill_n(batch->verts, ARRAY_SIZE(batch->verts), nullptr);
  batch->elem = nullptr;
  batch->flag = GPUBatchFlag(0);
  batch->prim_type = GPUPrimType(0);
  batch->shader = nullptr;
  batch->procedural_vertices = -1;
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
                           GPUBatchFlag owns_flag)
{
  Batch *batch = GPU_batch_calloc();
  GPU_batch_init_ex(batch, primitive_type, vertex_buf, index_buf, owns_flag);
  return batch;
}

void GPU_batch_init_ex(Batch *batch,
                       GPUPrimType primitive_type,
                       VertBuf *vertex_buf,
                       IndexBuf *index_buf,
                       GPUBatchFlag owns_flag)
{
  /* Do not pass any other flag */
  BLI_assert((owns_flag & ~(GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX)) == 0);
  /* Batch needs to be in cleared state. */
  BLI_assert((batch->flag & GPU_BATCH_INIT) == 0);

  batch->verts[0] = vertex_buf;
  for (int v = 1; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    batch->verts[v] = nullptr;
  }
  batch->elem = index_buf;
  batch->prim_type = primitive_type;
  batch->flag = owns_flag | GPU_BATCH_INIT | GPU_BATCH_DIRTY;
  batch->shader = nullptr;
  batch->procedural_vertices = -1;
}

Batch *GPU_batch_create_procedural(GPUPrimType primitive_type, int32_t vertex_count)
{
  BLI_assert(vertex_count >= 0);
  Batch *batch = GPU_batch_calloc();
  for (auto &v : batch->verts) {
    v = nullptr;
  }
  batch->elem = nullptr;
  batch->prim_type = primitive_type;
  batch->flag = GPU_BATCH_INIT | GPU_BATCH_DIRTY;
  batch->shader = nullptr;
  batch->procedural_vertices = vertex_count;
  return batch;
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
  batch_dst->procedural_vertices = batch_src->procedural_vertices;
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
  batch->flag = GPU_BATCH_INVALID;
  batch->procedural_vertices = -1;
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
      SET_FLAG_FROM_TEST(batch->flag, own_vbo, (GPUBatchFlag)(GPU_BATCH_OWNS_VBO << v));
      return v;
    }
  }
  /* we only make it this far if there is no room for another VertBuf */
  BLI_assert_msg(0, "Not enough VBO slot in batch");
  return -1;
}

bool GPU_batch_vertbuf_has(const Batch *batch, const VertBuf *vertex_buf)
{
  for (uint v = 0; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    if (batch->verts[v] == vertex_buf) {
      return true;
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform setters
 *
 * \{ */

void GPU_batch_set_shader(Batch *batch,
                          blender::gpu::Shader *shader,
                          const shader::SpecializationConstants *constants_state)
{
  batch->shader = shader;
  GPU_shader_bind(batch->shader, constants_state);
}

static uint16_t bind_attribute_as_ssbo(const ShaderInterface *interface,
                                       blender::gpu::Shader *shader,
                                       VertBuf *vbo)
{
  const GPUVertFormat *format = &vbo->format;

  /* We need to support GPU OpenSubdiv meshes. This assert can be enabled back after we refactor
   * our OpenSubdiv implementation to output the same layout as the regular mesh extraction. */
  // BLI_assert_msg(format->attr_len == 1, "Multi attribute buffers are not supported for now");

  char uniform_name[] = "gpu_attr_0";
  uint stride = format->stride;
  uint offset = 0;
  uint16_t bound_attr = 0u;
  for (uint a_idx = 0; a_idx < format->attr_len; a_idx++) {
    const GPUVertAttr *a = &format->attrs[a_idx];

    if (format->deinterleaved) {
      offset += ((a_idx == 0) ? 0 : format->attrs[a_idx - 1].type.size()) * vbo->vertex_len;
      stride = a->type.size();
    }
    else {
      offset = a->offset;
    }

    for (uint n_idx = 0; n_idx < a->name_len; n_idx++) {
      const char *name = GPU_vertformat_attr_name_get(format, a, n_idx);
      const ShaderInput *input = interface->ssbo_get(name);
      if (input == nullptr || input->location == -1) {
        continue;
      }
      GPU_vertbuf_bind_as_ssbo(vbo, input->location);
      bound_attr |= (1 << input->location);

      /* WORKAROUND: This is to support complex format. But ideally this should not be supported.
       */
      uniform_name[9] = '0' + input->location;

      /* Only support 4byte aligned attributes. */
      BLI_assert((stride % 4) == 0);
      BLI_assert((offset % 4) == 0);
      int descriptor[2] = {int(stride) / 4, int(offset) / 4};
      GPU_shader_uniform_2iv(shader, uniform_name, descriptor);
      /* WORKAROUND: Fix for polyline workaround. Ideally should be fused with `gpu_attr_0`.
       * But for now, changes are a bit too invasive. Will need to be revisited later on. */
      char uniform_name_len[] = "gpu_attr_0_len";
      uniform_name_len[9] = '0' + input->location;
      const int loc = GPU_shader_get_uniform(shader, uniform_name_len);
      if (loc != -1) {
        int data = a->type.comp_len();
        GPU_shader_uniform_int_ex(shader, loc, 1, 1, &data);
      }
    }
  }
  return bound_attr;
}

void GPU_batch_bind_as_resources(Batch *batch,
                                 blender::gpu::Shader *shader,
                                 const shader::SpecializationConstants *constants)
{
  const ShaderInterface *interface = shader->interface;
  if (interface->ssbo_attr_mask_ == 0) {
    return;
  }

  uint16_t ssbo_attributes = interface->ssbo_attr_mask_;

  if (ssbo_attributes & (1 << GPU_SSBO_INDEX_BUF_SLOT)) {
    /* Ensure binding for setting uniforms. This is required by the OpenGL backend. */
    GPU_shader_bind(shader, constants);
    if (batch->elem) {
      GPU_indexbuf_bind_as_ssbo(batch->elem, GPU_SSBO_INDEX_BUF_SLOT);
      GPU_shader_uniform_1b(shader, "gpu_index_no_buffer", false);
      GPU_shader_uniform_1b(shader, "gpu_index_16bit", !batch->elem->is_32bit());
      GPU_shader_uniform_1i(shader, "gpu_index_base_index", batch->elem->index_base_get());
    }
    else {
      /* Still fulfill the binding requirements even if the buffer will not be read. */
      GPU_vertbuf_bind_as_ssbo(batch->verts[0], GPU_SSBO_INDEX_BUF_SLOT);
      GPU_shader_uniform_1b(shader, "gpu_index_no_buffer", true);
    }
    ssbo_attributes &= ~(1 << GPU_SSBO_INDEX_BUF_SLOT);
  }

  /* Reverse order so first VBO'S have more prevalence (in term of attribute override). */
  for (int v = GPU_BATCH_VBO_MAX_LEN - 1; v > -1; v--) {
    VertBuf *vbo = batch->verts_(v);
    if (vbo) {
      ssbo_attributes &= ~bind_attribute_as_ssbo(interface, shader, vbo);
    }
  }

  BLI_assert_msg(ssbo_attributes == 0, "Not all attribute storage buffer fulfilled");
  UNUSED_VARS_NDEBUG(ssbo_attributes);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing / Draw-call functions
 * \{ */

void GPU_batch_draw_parameter_get(Batch *batch,
                                  int *r_vertex_count,
                                  int *r_vertex_first,
                                  int *r_base_index,
                                  int *r_instance_count)
{
  if (batch->procedural_vertices >= 0) {
    *r_vertex_count = batch->procedural_vertices;
    *r_vertex_first = 0;
    *r_base_index = -1;
  }
  else if (batch->elem) {
    *r_vertex_count = batch->elem_()->index_len_get();
    *r_vertex_first = batch->elem_()->index_start_get();
    *r_base_index = batch->elem_()->index_base_get();
  }
  else {
    *r_vertex_count = batch->verts_(0)->vertex_len;
    *r_vertex_first = 0;
    *r_base_index = -1;
  }

  *r_instance_count = 1;
}

blender::IndexRange GPU_batch_draw_expanded_parameter_get(GPUPrimType input_prim_type,
                                                          GPUPrimType output_prim_type,
                                                          int vertex_count,
                                                          int vertex_first,
                                                          int output_primitive_cout)
{
  int vert_per_original_primitive = indices_per_primitive(input_prim_type);
  int vert_per_expanded_primitive = indices_per_primitive(output_prim_type);

  int prim_first = vertex_first / vert_per_original_primitive;
  int prim_len = vertex_count / vert_per_original_primitive;

  BLI_assert_msg(vert_per_original_primitive != -1,
                 "Primitive expansion only works for primitives with known amount of vertices");

  /* WORKAROUND: Needed for polyline_draw_workaround. */
  if (input_prim_type == GPU_PRIM_LINE_STRIP) {
    prim_len = vertex_count - 1;
  }

  int out_vertex_first = prim_first * vert_per_expanded_primitive * output_primitive_cout;
  int out_vertex_count = prim_len * vert_per_expanded_primitive * output_primitive_cout;

  return blender::IndexRange(out_vertex_first, out_vertex_count);
}

static void polyline_draw_workaround(
    Batch *batch, int vertex_first, int vertex_count, int instance_first, int instance_count)
{
  /* Early out as this can cause crashes on some backend (see #136831). */
  if (vertex_count == 0) {
    return;
  }
  /* Check compatible input primitive. */
  BLI_assert(ELEM(batch->prim_type, GPU_PRIM_LINES, GPU_PRIM_LINE_STRIP, GPU_PRIM_LINE_LOOP));

  GPU_batch_bind_as_resources(batch, batch->shader);
  blender::IndexRange range = GPU_batch_draw_expanded_parameter_get(
      batch->prim_type, GPU_PRIM_TRIS, vertex_count, vertex_first, 2);
  Batch *tri_batch = Context::get()->procedural_triangles_batch_get();
  GPU_batch_set_shader(tri_batch, batch->shader);

  int vert_stride_count[3] = {(batch->prim_type == GPU_PRIM_LINES) ? 2 : 1, vertex_count, 0};
  GPU_shader_uniform_3iv(batch->shader, "gpu_vert_stride_count_offset", vert_stride_count);
  /* Assume GPU_FETCH_FLOAT for now. A bit cumbersome to assert for this or to find the correct
   * attribute. */
  GPU_shader_uniform_1b(batch->shader, "gpu_attr_0_fetch_int", false);

  /* Allow byte color fetch. */
  const GPUVertFormat *format = GPU_vertbuf_get_format(batch->verts[0]);
  int id = GPU_vertformat_attr_id_get(format, "color");
  if (id != -1) {
    const GPUVertAttr &attr = format->attrs[id];
    const bool is_unorm8 = attr.type.format == blender::gpu::VertAttrType::UNORM_8_8_8_8;
    BLI_assert_msg(is_unorm8 || attr.type.fetch_mode() == GPU_FETCH_FLOAT,
                   "color attribute for polylines can only use GPU_FETCH_INT_TO_FLOAT_UNIT or "
                   "GPU_FETCH_FLOAT");
    GPU_shader_uniform_1b(batch->shader, "gpu_attr_1_fetch_unorm8", is_unorm8);
  }

  GPU_batch_draw_advanced(tri_batch, range.start(), range.size(), instance_first, instance_count);
}

void GPU_batch_draw(Batch *batch)
{
  BLI_assert(batch != nullptr);
  GPU_shader_bind(batch->shader);
  if (batch->shader->is_polyline) {
    polyline_draw_workaround(batch, 0, batch->vertex_count_get(), 0, 0);
  }
  else {
    GPU_batch_draw_advanced(batch, 0, 0, 0, 0);
  }
}

void GPU_batch_draw_range(Batch *batch, int vertex_first, int vertex_count)
{
  BLI_assert(batch != nullptr);
  GPU_shader_bind(batch->shader);
  if (batch->shader->is_polyline) {
    polyline_draw_workaround(batch, vertex_first, vertex_count, 0, 0);
  }
  else {
    GPU_batch_draw_advanced(batch, vertex_first, vertex_count, 0, 0);
  }
}

void GPU_batch_draw_instance_range(Batch *batch, int instance_first, int instance_count)
{
  BLI_assert(batch != nullptr);
  /* Not polyline shaders support instancing. */
  BLI_assert(batch->shader->is_polyline == false);

  GPU_shader_bind(batch->shader);
  GPU_batch_draw_advanced(batch, 0, 0, instance_first, instance_count);
}

void GPU_batch_draw_advanced(
    Batch *batch, int vertex_first, int vertex_count, int instance_first, int instance_count)
{
  BLI_assert(batch != nullptr);
  BLI_assert(Context::get()->shader != nullptr);
  Context::get()->assert_framebuffer_shader_compatibility(Context::get()->shader);

  if (vertex_count == 0) {
    if (batch->procedural_vertices > 0) {
      vertex_count = batch->procedural_vertices;
    }
    else if (batch->elem) {
      vertex_count = batch->elem_()->index_len_get();
    }
    else {
      vertex_count = batch->verts_(0)->vertex_len;
    }
  }
  if (instance_count == 0) {
    instance_count = 1;
  }

  if (vertex_count == 0 || instance_count == 0) {
    /* Nothing to draw. */
    return;
  }

#ifndef NDEBUG
  debug_validate_binding_image_format();
#endif

  batch->draw(vertex_first, vertex_count, instance_first, instance_count);
}

void GPU_batch_draw_indirect(Batch *batch, blender::gpu::StorageBuf *indirect_buf, intptr_t offset)
{
  BLI_assert(batch != nullptr);
  BLI_assert(indirect_buf != nullptr);
  BLI_assert(Context::get()->shader != nullptr);
  Context::get()->assert_framebuffer_shader_compatibility(Context::get()->shader);

#ifndef NDEBUG
  debug_validate_binding_image_format();
#endif

  batch->draw_indirect(indirect_buf, offset);
}

void GPU_batch_multi_draw_indirect(Batch *batch,
                                   blender::gpu::StorageBuf *indirect_buf,
                                   int count,
                                   intptr_t offset,
                                   intptr_t stride)
{
  BLI_assert(batch != nullptr);
  BLI_assert(indirect_buf != nullptr);
  BLI_assert(Context::get()->shader != nullptr);
  Context::get()->assert_framebuffer_shader_compatibility(Context::get()->shader);

#ifndef NDEBUG
  debug_validate_binding_image_format();
#endif

  batch->multi_draw_indirect(indirect_buf, count, offset, stride);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

void GPU_batch_program_set_builtin_with_config(Batch *batch,
                                               GPUBuiltinShader shader_id,
                                               GPUShaderConfig sh_cfg)
{
  blender::gpu::Shader *shader = GPU_shader_get_builtin_shader_with_config(shader_id, sh_cfg);
  GPU_batch_set_shader(batch, shader);
}

void GPU_batch_program_set_builtin(Batch *batch, GPUBuiltinShader shader_id)
{
  GPU_batch_program_set_builtin_with_config(batch, shader_id, GPU_SHADER_CFG_DEFAULT);
}

void GPU_batch_program_set_imm_shader(Batch *batch)
{
  GPU_batch_set_shader(batch, immGetShader());
}

blender::gpu::Batch *GPU_batch_procedural_points_get()
{
  return blender::gpu::Context::get()->procedural_points_batch_get();
}

blender::gpu::Batch *GPU_batch_procedural_lines_get()
{
  return blender::gpu::Context::get()->procedural_lines_batch_get();
}

blender::gpu::Batch *GPU_batch_procedural_triangles_get()
{
  return blender::gpu::Context::get()->procedural_triangles_batch_get();
}

blender::gpu::Batch *GPU_batch_procedural_triangle_strips_get()
{
  return blender::gpu::Context::get()->procedural_triangle_strips_batch_get();
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
