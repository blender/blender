/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Metal implementation of gpu::Batch.
 */

#include "BLI_assert.h"
#include "BLI_span.hh"

#include "BKE_global.hh"

#include "GPU_batch.hh"
#include "GPU_common.hh"
#include "gpu_shader_private.hh"
#include "gpu_vertex_format_private.hh"

#include "mtl_batch.hh"
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_index_buffer.hh"
#include "mtl_shader.hh"
#include "mtl_storage_buffer.hh"
#include "mtl_vertex_buffer.hh"

#include <string>

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */
void MTLBatch::draw(int v_first, int v_count, int i_first, int i_count)
{
  this->draw_advanced(v_first, v_count, i_first, i_count);
}

void MTLBatch::draw_indirect(StorageBuf *indirect_buf, intptr_t offset)
{
  this->draw_advanced_indirect(indirect_buf, offset);
}

void MTLBatch::MTLVertexDescriptorCache::vertex_descriptor_cache_init(MTLContext *ctx)
{
  BLI_assert(ctx != nullptr);
  this->vertex_descriptor_cache_clear();
  cache_context_ = ctx;
}

void MTLBatch::MTLVertexDescriptorCache::vertex_descriptor_cache_clear()
{
  cache_life_index_++;
  cache_context_ = nullptr;
}

void MTLBatch::MTLVertexDescriptorCache::vertex_descriptor_cache_ensure()
{
  if (this->cache_context_ != nullptr) {

    /* Invalidate vertex descriptor bindings cache if batch has changed. */
    if (batch_->flag & GPU_BATCH_DIRTY) {
      batch_->flag &= ~GPU_BATCH_DIRTY;
      this->vertex_descriptor_cache_clear();
    }
  }

  /* Initialize cache if not ready. */
  if (cache_context_ == nullptr) {
    this->vertex_descriptor_cache_init(MTLContext::get());
  }
}

MTLBatch::VertexDescriptorShaderInterfacePair *MTLBatch::MTLVertexDescriptorCache::find(
    const ShaderInterface *interface)
{
  this->vertex_descriptor_cache_ensure();
  for (int i = 0; i < GPU_VAO_STATIC_LEN; ++i) {
    if (cache_[i].interface == interface && cache_[i].cache_life_index == cache_life_index_) {
      return &cache_[i];
    }
  }
  return nullptr;
}

bool MTLBatch::MTLVertexDescriptorCache::insert(
    MTLBatch::VertexDescriptorShaderInterfacePair &data)
{
  vertex_descriptor_cache_ensure();
  for (int i = 0; i < GPU_VAO_STATIC_LEN; ++i) {
    if (cache_[i].interface == nullptr || cache_[i].cache_life_index != cache_life_index_) {
      cache_[i] = data;
      cache_[i].cache_life_index = cache_life_index_;
      return true;
    }
  }
  return false;
}

/* Return index inside the vertex descriptor. */
VertBufBinding MTLBatch::prepare_vertex_binding(MTLVertBuf *verts,
                                                MTLRenderPipelineStateDescriptor &desc,
                                                const MTLShaderInterface &interface,
                                                uint16_t &attr_mask,
                                                uint32_t &buffer_mask)
{

  const GPUVertFormat *format = &verts->format;
  /* Per-vertex stride of current vertex buffer. */
  int buffer_stride = format->stride;
  /* Buffer binding index of the vertex buffer once added to the buffer layout descriptor. */
  VertBufBinding binding{-1, -1};
  int attribute_offset = 0;

  /* Iterate over VertBuf vertex format and find attributes matching those in the active
   * shader's interface. */
  for (uint32_t a_idx = 0; a_idx < format->attr_len; a_idx++) {
    const GPUVertAttr *a = &format->attrs[a_idx];

    if (format->deinterleaved) {
      attribute_offset += ((a_idx == 0) ? 0 : format->attrs[a_idx - 1].type.size()) *
                          verts->vertex_len;
      buffer_stride = a->type.size();
    }
    else {
      attribute_offset = a->offset;
    }

    /* Find attribute with the matching name. Attributes may have multiple compatible
     * name aliases. */
    for (uint32_t n_idx = 0; n_idx < a->name_len; n_idx++) {
      const char *name = GPU_vertformat_attr_name_get(format, a, n_idx);
      const ShaderInput *input = interface.attr_get(name);

      if (input == nullptr || input->location == -1) {
        continue;
      }

      /* Check if attribute is already present in the given slot. */
      if ((~attr_mask) & (1 << input->location)) {
        MTL_LOG_DEBUG(
            "  -- [Batch] Skipping attribute with input location %d (As one is already bound)",
            input->location);
        continue;
      }

      /* Update attribute used-slot mask. */
      attr_mask &= ~(1 << input->location);

      /* Add buffer layout entry in descriptor if it has not yet been added
       * for current vertex buffer. */
      if (binding.desc_id == -1) {
        if (buffer_mask == 0) {
          MTL_LOG_DEBUG("  -- [Batch] Error: Not enough available slot to bind vertex buffer");
          return binding;
        }

        binding.desc_id = desc.vertex_descriptor.num_vert_buffers++;
        binding.slot_id = bitscan_forward_clear_uint(&buffer_mask);

        auto &buffer_layout = desc.vertex_descriptor.buffer_layouts[binding.desc_id];
        buffer_layout.step_function = MTLVertexStepFunctionPerVertex;
        buffer_layout.step_rate = 1;
        buffer_layout.stride = buffer_stride;
        buffer_layout.buffer_slot = binding.slot_id;

        MTL_LOG_DEBUG("  -- [Batch] Adding source vertex buffer (Index: %d, Stride: %d)",
                      binding.desc_id,
                      buffer_stride);
      }
      else {
        /* Ensure stride is correct for de-interleaved attributes. */
        desc.vertex_descriptor.buffer_layouts[binding.desc_id].stride = buffer_stride;
      }

      {
        MTLVertexAttributeDescriptorPSO &pso_attr =
            desc.vertex_descriptor.attributes[input->location];

        pso_attr.format = gpu_vertex_format_to_metal(a->type.format);
        pso_attr.offset = attribute_offset;
        pso_attr.buffer_index = binding.slot_id;
        pso_attr.format_conversion_mode = is_fetch_float(a->type.format) ?
                                              GPUVertFetchMode(GPU_FETCH_FLOAT) :
                                              GPUVertFetchMode(GPU_FETCH_INT);

        desc.vertex_descriptor.max_attribute_value =
            ((input->location) > desc.vertex_descriptor.max_attribute_value) ?
                (input->location) :
                desc.vertex_descriptor.max_attribute_value;
        desc.vertex_descriptor.total_attributes++;

        /* NOTE: We are setting max_attribute_value to be up to the maximum found index, because
         * of this, it is possible that we may skip over certain attributes if they were not in
         * the source GPUVertFormat. */
        MTL_LOG_DEBUG(
            " -- Batch Attribute(%d): ORIG Vert format: %d, Vert "
            "components: %d, Fetch Mode %d --> FINAL FORMAT: %d",
            input->location,
            (int)a->type.comp_type(),
            (int)a->type.comp_len(),
            (int)a->type.fetch_mode(),
            (int)pso_attr.format);

        MTL_LOG_DEBUG(
            "  -- [Batch] matching vertex attribute '%s' (Attribute Index: %d, Buffer index: "
            "%d, "
            "offset: %d)",
            name,
            input->location,
            binding.desc_id,
            attribute_offset);
      }
    }
  }
  return binding;
}

id<MTLRenderCommandEncoder> MTLBatch::bind()
{
  /* Setup draw call and render pipeline state here. Called by every draw, but setup here so that
   * MTLDrawList only needs to perform setup a single time. */
  BLI_assert(this);

  /* Fetch Metal device. */
  MTLContext *ctx = MTLContext::get();
  if (!ctx) {
    BLI_assert_msg(false, "No context available for rendering.");
    return nil;
  }

  /* Fetch bound shader from context. */
  active_shader_ = static_cast<MTLShader *>(ctx->shader);

  if (active_shader_ == nullptr || !active_shader_->is_valid()) {
    /* Skip drawing if there is no valid Metal shader.
     * This will occur if the path through which the shader is prepared
     * is invalid (e.g. Python without create-info), or, the source shader uses a geometry pass. */
    BLI_assert_msg(false, "No valid Metal shader!");
    return nil;
  }

  /* Prepare Vertex Descriptor and extract VertexBuffers to bind. */
  MTLVertBuf *buffers[GPU_BATCH_VBO_MAX_LEN] = {nullptr};
  int binding_slot[GPU_BATCH_VBO_MAX_LEN] = {0};
  int num_buffers = 0;

  /* Ensure Index Buffer is ready. */
  MTLIndexBuf *mtl_elem = static_cast<MTLIndexBuf *>(this->elem);
  if (mtl_elem != nullptr) {
    mtl_elem->upload_data();
  }

  /* Populate vertex descriptor with attribute binding information.
   * The vertex descriptor and buffer layout descriptors describe
   * how vertex data from bound vertex buffers maps to the
   * shader's input.
   * A unique vertex descriptor will result in a new PipelineStateObject
   * being generated for the currently bound shader. */
  prepare_vertex_descriptor_and_bindings(
      {buffers, GPU_BATCH_VBO_MAX_LEN}, {binding_slot, GPU_BATCH_VBO_MAX_LEN}, num_buffers);

  /* Prepare Vertex Buffers - Run before RenderCommandEncoder in case BlitCommandEncoder buffer
   * data operations are required. */
  for (int i = 0; i < num_buffers; i++) {
    buffers[i]->bind();
  }

  /* Ensure render pass is active and fetch active RenderCommandEncoder. */
  id<MTLRenderCommandEncoder> rec = ctx->ensure_begin_render_pass();

  /* Fetch RenderPassState to enable resource binding for active pass. */
  MTLRenderPassState &rps = ctx->main_command_buffer.get_render_pass_state();

  /* Debug Check: Ensure Frame-buffer instance is not dirty. */
  BLI_assert(!ctx->main_command_buffer.get_active_framebuffer()->get_dirty());

  /* GPU debug markers. */
  if (G.debug & G_DEBUG_GPU) {
    ctx->main_command_buffer.unfold_pending_debug_groups();
    [rec pushDebugGroup:[NSString stringWithFormat:@"Batch(Shader:%s)",
                                                   active_shader_->name_get().c_str()]];
  }

  /*** Bind Vertex Buffers and Index Buffers **/

  /* Ensure Context Render Pipeline State is fully setup and ready to execute the draw.
   * This should happen after all other final rendering setup is complete. */
  MTLPrimitiveType mtl_prim_type = gpu_prim_type_to_metal(this->prim_type);
  if (!ctx->ensure_render_pipeline_state(mtl_prim_type)) {
    MTL_LOG_ERROR("Failed to prepare and apply render pipeline state.");
    BLI_assert(false);
    return nil;
  }

  /* Bind Vertex Buffers. */
  for (int i = 0; i < num_buffers; i++) {
    MTLVertBuf *buf_at_index = buffers[i];
    /* Buffer handle. */
    MTLVertBuf *mtlvbo = static_cast<MTLVertBuf *>(reinterpret_cast<VertBuf *>(buf_at_index));
    mtlvbo->flag_used();

    /* Fetch buffer from MTLVertexBuffer and bind. */
    id<MTLBuffer> mtl_buffer = mtlvbo->get_metal_buffer();

    BLI_assert(mtl_buffer != nil);
    rps.bind_vertex_buffer(mtl_buffer, 0, binding_slot[i]);
  }

  /* Return Render Command Encoder used with setup. */
  return rec;
}

void MTLBatch::unbind(id<MTLRenderCommandEncoder> rec)
{
  /* Pop bind debug group. */
  if (G.debug & G_DEBUG_GPU) {
    [rec popDebugGroup];
  }
}

void MTLBatch::prepare_vertex_descriptor_and_bindings(MutableSpan<MTLVertBuf *> buffers,
                                                      MutableSpan<int> buffer_slots,
                                                      int &num_buffers)
{

  /* Here we populate the MTLContext vertex descriptor and resolve which buffers need to be bound.
   */
  MTLStateManager *state_manager = static_cast<MTLStateManager *>(
      MTLContext::get()->state_manager);
  MTLRenderPipelineStateDescriptor &desc = state_manager->get_pipeline_descriptor();
  const MTLShaderInterface &interface = active_shader_->get_interface();
  uint16_t attr_mask = interface.enabled_attr_mask_;
  uint32_t buffer_mask = interface.vertex_buffer_mask();

  /* Reset vertex descriptor to default state. */
  desc.reset_vertex_descriptor();

  /* Fetch Vertex Buffers. */
  Span<VertBuf *> mtl_verts(this->verts, GPU_BATCH_VBO_MAX_LEN);

  /* Resolve Metal vertex buffer bindings. */
  /* Vertex Descriptors
   * ------------------
   * Vertex Descriptors are required to generate a pipeline state, based on the current Batch's
   * buffer bindings. These bindings are a unique matching, depending on what input attributes a
   * batch has in its buffers, and those which are supported by the shader interface.
   *
   * We iterate through the buffers and resolve which attributes satisfy the requirements of the
   * currently bound shader. We cache this data, for a given Batch<->ShderInterface pairing in a
   * VAO cache to avoid the need to recalculate this data. */

  VertexDescriptorShaderInterfacePair *pair = this->vao_cache.find(&interface);
  if (pair) {
    desc.vertex_descriptor = pair->vertex_descriptor;
    attr_mask = pair->attr_mask;
    num_buffers = pair->num_buffers;

    for (int bid = 0; bid < GPU_BATCH_VBO_MAX_LEN; ++bid) {
      if (pair->bufferIds[bid].used) {
        buffers[bid] = static_cast<MTLVertBuf *>(this->verts[pair->bufferIds[bid].id]);
        buffer_slots[bid] = pair->bufferIds[bid].slot;
      }
    }
  }
  else {
    VertexDescriptorShaderInterfacePair pair{};
    pair.interface = &interface;

    for (int i = 0; i < GPU_BATCH_VBO_MAX_LEN; ++i) {
      pair.bufferIds[i].id = -1;
      pair.bufferIds[i].used = 0;
    }
    /* NOTE: Attribute extraction order from buffer is the reverse of the OpenGL as we flag once an
     * attribute is found, rather than pre-setting the mask. */

    /* Extract Vertex attributes (First-bound vertex buffer takes priority). */
    for (int v = 0; v < GPU_BATCH_VBO_MAX_LEN; v++) {
      if (mtl_verts[v] != nullptr) {
        MTL_LOG_DEBUG(" -- [Batch] Checking bindings for bound vertex buffer %p", mtl_verts[v]);
        VertBufBinding bindings = this->prepare_vertex_binding(
            static_cast<MTLVertBuf *>(this->verts[v]), desc, interface, attr_mask, buffer_mask);
        if (bindings.desc_id >= 0) {
          buffers[bindings.desc_id] = static_cast<MTLVertBuf *>(this->verts[v]);
          buffer_slots[bindings.desc_id] = bindings.slot_id;
          pair.bufferIds[bindings.desc_id].id = v;
          pair.bufferIds[bindings.desc_id].used = 1;
          pair.bufferIds[bindings.desc_id].slot = bindings.slot_id;
          num_buffers = max_ii(bindings.desc_id + 1, num_buffers);
        }
      }
    }

    /* Add to VertexDescriptor cache */
    pair.attr_mask = attr_mask;
    pair.vertex_descriptor = desc.vertex_descriptor;
    pair.num_buffers = num_buffers;
    if (!this->vao_cache.insert(pair)) {
      printf(
          "[Performance Warning] cache is full (Size: %d), vertex descriptor will not be cached\n",
          GPU_VAO_STATIC_LEN);
    }
  }

/* DEBUG: verify if our attribute bindings have been fully provided as expected. */
#if MTL_DEBUG_SHADER_ATTRIBUTES == 1
  if (attr_mask != 0) {
    /* Attributes are not necessarily contiguous. */
    for (int i = 0; i < active_shader_->get_interface()->get_total_attributes(); i++) {
      const MTLShaderInputAttribute &attr = active_shader_->get_interface()->get_attribute(i);
      if (attr_mask & (1 << attr.location)) {
        MTL_LOG_WARNING(
            "Warning: Missing expected attribute '%s' with location: %u in shader %s (attr "
            "number: %u)",
            active_shader_->get_interface()->name_at_offset(attr.name_offset),
            attr.location,
            active_shader_->name_get(),
            i);

        /* If an attribute is not included, then format in vertex descriptor should be invalid due
         * to nil assignment. */
        BLI_assert(desc.vertex_descriptor.attributes[attr.location].format ==
                   MTLVertexFormatInvalid);
      }
    }
  }
#endif
}

void MTLBatch::draw_advanced(int v_first, int v_count, int i_first, int i_count)
{
  BLI_assert(v_count > 0 && i_count > 0);

  /* Setup RenderPipelineState for batch. */
  MTLContext *ctx = MTLContext::get();
  id<MTLRenderCommandEncoder> rec = this->bind();
  if (rec == nil) {
    /* End of draw. */
    this->unbind(rec);
    return;
  }

  /* Fetch IndexBuffer and resolve primitive type. */
  MTLIndexBuf *mtl_elem = static_cast<MTLIndexBuf *>(reinterpret_cast<IndexBuf *>(this->elem));
  MTLPrimitiveType mtl_prim_type = gpu_prim_type_to_metal(this->prim_type);

  /* Perform regular draw. */
  if (mtl_elem == nullptr) {
    /* Primitive Type topology emulation. */
    if (mtl_needs_topology_emulation(this->prim_type)) {
      /* Generate index buffer for primitive types requiring emulation. */
      GPUPrimType emulated_prim_type = this->prim_type;
      uint32_t emulated_v_count = v_count;
      id<MTLBuffer> generated_index_buffer = this->get_emulated_toplogy_buffer(emulated_prim_type,
                                                                               emulated_v_count);
      BLI_assert(generated_index_buffer != nil);

      MTLPrimitiveType emulated_mtl_prim_type = gpu_prim_type_to_metal(emulated_prim_type);

      /* Temp: Disable culling for emulated primitive types.
       * TODO(Metal): Support face winding in topology buffer. */
      [rec setCullMode:MTLCullModeNone];

      if (generated_index_buffer != nil) {
        BLI_assert(emulated_mtl_prim_type == MTLPrimitiveTypeTriangle ||
                   emulated_mtl_prim_type == MTLPrimitiveTypeLine);
        if (emulated_mtl_prim_type == MTLPrimitiveTypeTriangle) {
          BLI_assert(emulated_v_count % 3 == 0);
        }
        if (emulated_mtl_prim_type == MTLPrimitiveTypeLine) {
          BLI_assert(emulated_v_count % 2 == 0);
        }

        /* Set depth stencil state (requires knowledge of primitive type). */
        ctx->ensure_depth_stencil_state(emulated_mtl_prim_type);

        [rec drawIndexedPrimitives:emulated_mtl_prim_type
                        indexCount:emulated_v_count
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:generated_index_buffer
                 indexBufferOffset:0
                     instanceCount:i_count
                        baseVertex:v_first
                      baseInstance:i_first];
      }
      else {
        printf("[Note] Cannot draw batch -- Emulated Topology mode: %u not yet supported\n",
               this->prim_type);
      }
    }
    else {
      /* Set depth stencil state (requires knowledge of primitive type). */
      ctx->ensure_depth_stencil_state(mtl_prim_type);

      /* Issue draw call. */
      [rec drawPrimitives:mtl_prim_type
              vertexStart:v_first
              vertexCount:v_count
            instanceCount:i_count
             baseInstance:i_first];
    }
    ctx->main_command_buffer.register_draw_counters(v_count * i_count);
  }
  /* Perform indexed draw. */
  else {

    MTLIndexType index_type = MTLIndexBuf::gpu_index_type_to_metal(mtl_elem->index_type_);
    uint32_t base_index = mtl_elem->index_base_;
    uint32_t index_size = (mtl_elem->index_type_ == GPU_INDEX_U16) ? 2 : 4;
    uint32_t v_first_ofs = ((v_first + mtl_elem->index_start_) * index_size);
    BLI_assert_msg((v_first_ofs % index_size) == 0,
                   "Index offset is not 2/4-byte aligned as per METAL spec");

    /* Fetch index buffer. May return an index buffer of a differing format,
     * if index buffer optimization is used. In these cases, final_prim_type and
     * index_count get updated with the new properties. */
    GPUPrimType final_prim_type = this->prim_type;
    uint index_count = v_count;

    id<MTLBuffer> index_buffer = mtl_elem->get_index_buffer(final_prim_type, index_count);
    mtl_prim_type = gpu_prim_type_to_metal(final_prim_type);
    BLI_assert(index_buffer != nil);

    if (index_buffer != nil) {

      /* Set depth stencil state (requires knowledge of primitive type). */
      ctx->ensure_depth_stencil_state(mtl_prim_type);

      /* Issue draw call. */
      [rec drawIndexedPrimitives:mtl_prim_type
                      indexCount:index_count
                       indexType:index_type
                     indexBuffer:index_buffer
               indexBufferOffset:v_first_ofs
                   instanceCount:i_count
                      baseVertex:base_index
                    baseInstance:i_first];
      ctx->main_command_buffer.register_draw_counters(index_count * i_count);
    }
    else {
      BLI_assert_msg(false, "Index buffer does not have backing Metal buffer");
    }
  }

  /* End of draw. */
  this->unbind(rec);
}

void MTLBatch::draw_advanced_indirect(StorageBuf *indirect_buf, intptr_t offset)
{
  /* Setup RenderPipelineState for batch. */
  MTLContext *ctx = MTLContext::get();
  id<MTLRenderCommandEncoder> rec = this->bind();
  if (rec == nil) {
    printf("Failed to open Render Command encoder for DRAW INDIRECT\n");

    /* End of draw. */
    this->unbind(rec);
    return;
  }

  /* Fetch indirect buffer Metal handle. */
  MTLStorageBuf *mtlssbo = static_cast<MTLStorageBuf *>(indirect_buf);
  id<MTLBuffer> mtl_indirect_buf = mtlssbo->get_metal_buffer();
  BLI_assert(mtl_indirect_buf != nil);
  if (mtl_indirect_buf == nil) {
    MTL_LOG_WARNING("Metal Indirect Draw Storage Buffer is nil.");

    /* End of draw. */
    this->unbind(rec);
    return;
  }

  /* Unsupported primitive type check. */
  BLI_assert_msg(this->prim_type != GPU_PRIM_TRI_FAN,
                 "TriangleFan is not supported in Metal for Indirect draws.");

  /* Fetch IndexBuffer and resolve primitive type. */
  MTLIndexBuf *mtl_elem = static_cast<MTLIndexBuf *>(reinterpret_cast<IndexBuf *>(this->elem));
  MTLPrimitiveType mtl_prim_type = gpu_prim_type_to_metal(this->prim_type);

  if (mtl_needs_topology_emulation(this->prim_type)) {
    BLI_assert_msg(false, "Metal Topology emulation unsupported for draw indirect.\n");

    /* End of draw. */
    this->unbind(rec);
    return;
  }

  if (mtl_elem == nullptr) {
    /* Set depth stencil state (requires knowledge of primitive type). */
    ctx->ensure_depth_stencil_state(mtl_prim_type);

    /* Issue draw call. */
    [rec drawPrimitives:mtl_prim_type indirectBuffer:mtl_indirect_buf indirectBufferOffset:offset];
    ctx->main_command_buffer.register_draw_counters(1);
  }
  else {
    /* Fetch index buffer. May return an index buffer of a differing format,
     * if index buffer optimization is used. In these cases, final_prim_type and
     * index_count get updated with the new properties. */
    MTLIndexType index_type = MTLIndexBuf::gpu_index_type_to_metal(mtl_elem->index_type_);
    GPUPrimType final_prim_type = this->prim_type;
    uint index_count = 0;

    /* Disable index optimization for indirect draws. */
    mtl_elem->flag_can_optimize(false);

    id<MTLBuffer> index_buffer = mtl_elem->get_index_buffer(final_prim_type, index_count);
    mtl_prim_type = gpu_prim_type_to_metal(final_prim_type);
    BLI_assert(index_buffer != nil);

    if (index_buffer != nil) {

      /* Set depth stencil state (requires knowledge of primitive type). */
      ctx->ensure_depth_stencil_state(mtl_prim_type);

      /* Issue draw call. */
      [rec drawIndexedPrimitives:mtl_prim_type
                       indexType:index_type
                     indexBuffer:index_buffer
               indexBufferOffset:0
                  indirectBuffer:mtl_indirect_buf
            indirectBufferOffset:offset];
      ctx->main_command_buffer.register_draw_counters(1);
    }
    else {
      BLI_assert_msg(false, "Index buffer does not have backing Metal buffer");
    }
  }

  /* End of draw. */
  this->unbind(rec);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Topology emulation and optimization
 * \{ */

id<MTLBuffer> MTLBatch::get_emulated_toplogy_buffer(GPUPrimType &in_out_prim_type,
                                                    uint32_t &in_out_v_count)
{

  BLI_assert(in_out_v_count > 0);
  /* Determine emulated primitive types. */
  GPUPrimType input_prim_type = in_out_prim_type;
  uint32_t v_count = in_out_v_count;
  GPUPrimType output_prim_type;
  switch (input_prim_type) {
    case GPU_PRIM_POINTS:
    case GPU_PRIM_LINES:
    case GPU_PRIM_TRIS:
      BLI_assert_msg(false, "Optimal primitive types should not reach here.");
      return nil;
      break;
    case GPU_PRIM_LINES_ADJ:
    case GPU_PRIM_TRIS_ADJ:
      BLI_assert_msg(false, "Adjacency primitive types should not reach here.");
      return nil;
      break;
    case GPU_PRIM_LINE_STRIP:
    case GPU_PRIM_LINE_LOOP:
    case GPU_PRIM_LINE_STRIP_ADJ:
      output_prim_type = GPU_PRIM_LINES;
      break;
    case GPU_PRIM_TRI_STRIP:
    case GPU_PRIM_TRI_FAN:
      output_prim_type = GPU_PRIM_TRIS;
      break;
    default:
      BLI_assert_msg(false, "Invalid primitive type.");
      return nil;
  }

  /* Check if topology buffer exists and is valid. */
  if (this->emulated_topology_buffer_ != nullptr &&
      (emulated_topology_type_ != input_prim_type || topology_buffer_input_v_count_ != v_count))
  {

    /* Release existing topology buffer. */
    emulated_topology_buffer_->free();
    emulated_topology_buffer_ = nullptr;
  }

  /* Generate new topology index buffer. */
  if (this->emulated_topology_buffer_ == nullptr) {
    /* Calculate IB len. */
    uint32_t output_prim_count = 0;
    switch (input_prim_type) {
      case GPU_PRIM_LINE_STRIP:
      case GPU_PRIM_LINE_STRIP_ADJ:
        output_prim_count = v_count - 1;
        break;
      case GPU_PRIM_LINE_LOOP:
        output_prim_count = v_count;
        break;
      case GPU_PRIM_TRI_STRIP:
      case GPU_PRIM_TRI_FAN:
        output_prim_count = v_count - 2;
        break;
      default:
        BLI_assert_msg(false, "Cannot generate optimized topology buffer for other types.");
        break;
    }
    uint32_t output_IB_elems = output_prim_count * ((output_prim_type == GPU_PRIM_TRIS) ? 3 : 2);

    /* Allocate buffer. */
    uint32_t buffer_bytes = output_IB_elems * 4;
    BLI_assert(buffer_bytes > 0);
    this->emulated_topology_buffer_ = MTLContext::get_global_memory_manager()->allocate(
        buffer_bytes, true);

    /* Populate. */
    uint32_t *data = (uint32_t *)this->emulated_topology_buffer_->get_host_ptr();
    BLI_assert(data != nullptr);

    /* TODO(Metal): Support inverse winding modes. */
    bool winding_clockwise = false;
    UNUSED_VARS(winding_clockwise);

    switch (input_prim_type) {
      /* Line Loop. */
      case GPU_PRIM_LINE_LOOP: {
        int line = 0;
        for (line = 0; line < output_prim_count - 1; line++) {
          data[line * 2 + 0] = line + 0;
          data[line * 2 + 1] = line + 1;
        }
        /* Closing line. */
        data[line * 2 + 0] = line + 0;
        data[line * 2 + 1] = 0;
      } break;

      /* Triangle Fan. */
      case GPU_PRIM_TRI_FAN: {
        for (int triangle = 0; triangle < output_prim_count; triangle++) {
          data[triangle * 3 + 0] = 0; /* Always 0 */
          data[triangle * 3 + 1] = triangle + 1;
          data[triangle * 3 + 2] = triangle + 2;
        }
      } break;

      default:
        BLI_assert_msg(false, "Other primitive types do not require emulation.");
        return nil;
    }

    /* Flush. */
    this->emulated_topology_buffer_->flush();
    /* Assign members relating to current cached IB. */
    topology_buffer_input_v_count_ = v_count;
    topology_buffer_output_v_count_ = output_IB_elems;
    emulated_topology_type_ = input_prim_type;
  }

  /* Return. */
  in_out_v_count = topology_buffer_output_v_count_;
  in_out_prim_type = output_prim_type;
  return (emulated_topology_buffer_) ? emulated_topology_buffer_->get_metal_buffer() : nil;
}

/** \} */

}  // namespace blender::gpu
