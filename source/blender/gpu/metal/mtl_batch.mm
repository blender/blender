/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Metal implementation of GPUBatch.
 */

#include "BLI_assert.h"
#include "BLI_span.hh"

#include "BKE_global.h"

#include "GPU_common.h"
#include "gpu_batch_private.hh"
#include "gpu_shader_private.hh"

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
  if (this->flag & GPU_BATCH_INVALID) {
    this->shader_in_use_ = false;
  }
  this->draw_advanced(v_first, v_count, i_first, i_count);
}

void MTLBatch::draw_indirect(GPUStorageBuf *indirect_buf, intptr_t offset)
{
  if (this->flag & GPU_BATCH_INVALID) {
    this->shader_in_use_ = false;
  }
  this->draw_advanced_indirect(indirect_buf, offset);
}

void MTLBatch::shader_bind()
{
  if (active_shader_ && active_shader_->is_valid()) {
    active_shader_->bind();
    shader_in_use_ = true;
  }
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

int MTLBatch::prepare_vertex_binding(MTLVertBuf *verts,
                                     MTLRenderPipelineStateDescriptor &desc,
                                     const MTLShaderInterface *interface,
                                     uint16_t &attr_mask,
                                     bool instanced)
{

  const GPUVertFormat *format = &verts->format;
  /* Whether the current vertex buffer has been added to the buffer layout descriptor. */
  bool buffer_added = false;
  /* Per-vertex stride of current vertex buffer. */
  int buffer_stride = format->stride;
  /* Buffer binding index of the vertex buffer once added to the buffer layout descriptor. */
  int buffer_index = -1;
  int attribute_offset = 0;

  if (!active_shader_->get_uses_ssbo_vertex_fetch()) {
    BLI_assert(
        buffer_stride >= 4 &&
        "In Metal, Vertex buffer stride should be 4. SSBO Vertex fetch is not affected by this");
  }

  /* Iterate over GPUVertBuf vertex format and find attributes matching those in the active
   * shader's interface. */
  for (uint32_t a_idx = 0; a_idx < format->attr_len; a_idx++) {
    const GPUVertAttr *a = &format->attrs[a_idx];

    if (format->deinterleaved) {
      attribute_offset += ((a_idx == 0) ? 0 : format->attrs[a_idx - 1].size) * verts->vertex_len;
      buffer_stride = a->size;
    }
    else {
      attribute_offset = a->offset;
    }

    /* Find attribute with the matching name. Attributes may have multiple compatible
     * name aliases. */
    for (uint32_t n_idx = 0; n_idx < a->name_len; n_idx++) {
      const char *name = GPU_vertformat_attr_name_get(format, a, n_idx);
      const ShaderInput *input = interface->attr_get(name);

      if (input == nullptr || input->location == -1) {
        /* Vertex/instance buffers provided have attribute data for attributes which are not needed
         * by this particular shader. This shader only needs binding information for the attributes
         * has in the shader interface. */
        MTL_LOG_WARNING(
            "MTLBatch: Could not find attribute with name '%s' (defined in active vertex format) "
            "in the shader interface for shader '%s'\n",
            name,
            interface->get_name());
        continue;
      }

      /* Fetch metal attribute information (ShaderInput->binding is used to fetch the corresponding
       * slot. */
      const MTLShaderInputAttribute &mtl_attr = interface->get_attribute(input->binding);
      BLI_assert(mtl_attr.location >= 0);
      /* Verify that the attribute location from the shader interface
       * matches the attribute location returned in the input table. These should always be the
       * same. */
      BLI_assert(mtl_attr.location == input->location);

      /* Check if attribute is already present in the given slot. */
      if ((~attr_mask) & (1 << mtl_attr.location)) {
        MTL_LOG_INFO(
            "  -- [Batch] Skipping attribute with input location %d (As one is already bound)\n",
            mtl_attr.location);
      }
      else {

        /* Update attribute used-slot mask. */
        attr_mask &= ~(1 << mtl_attr.location);

        /* Add buffer layout entry in descriptor if it has not yet been added
         * for current vertex buffer. */
        if (!buffer_added) {
          buffer_index = desc.vertex_descriptor.num_vert_buffers;
          desc.vertex_descriptor.buffer_layouts[buffer_index].step_function =
              (instanced) ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
          desc.vertex_descriptor.buffer_layouts[buffer_index].step_rate = 1;
          desc.vertex_descriptor.buffer_layouts[buffer_index].stride = buffer_stride;
          desc.vertex_descriptor.num_vert_buffers++;
          buffer_added = true;

          MTL_LOG_INFO("  -- [Batch] Adding source %s buffer (Index: %d, Stride: %d)\n",
                       (instanced) ? "instance" : "vertex",
                       buffer_index,
                       buffer_stride);
        }
        else {
          /* Ensure stride is correct for de-interleaved attributes. */
          desc.vertex_descriptor.buffer_layouts[buffer_index].stride = buffer_stride;
        }

        /* Handle Matrix/Array vertex attribute types.
         * Metal does not natively support these as attribute types, so we handle these cases
         * by stacking together compatible types (e.g. 4xVec4 for Mat4) and combining
         * the data in the shader.
         * The generated Metal shader will contain a generated input binding, which reads
         * in individual attributes and merges them into the desired type after vertex
         * assembly. e.g. a Mat4 (Float4x4) will generate 4 Float4 attributes. */
        if (a->comp_len == 16 || a->comp_len == 12 || a->comp_len == 8) {
          BLI_assert_msg(
              a->comp_len == 16,
              "only mat4 attributes currently supported -- Not ready to handle other long "
              "component length attributes yet");

          /* SSBO Vertex Fetch Attribute safety checks. */
          if (active_shader_->get_uses_ssbo_vertex_fetch()) {
            /* When using SSBO vertex fetch, we do not need to expose split attributes,
             * A matrix can be read directly as a whole block of contiguous data. */
            MTLSSBOAttribute ssbo_attr(mtl_attr.index,
                                       buffer_index,
                                       attribute_offset,
                                       buffer_stride,
                                       GPU_SHADER_ATTR_TYPE_MAT4,
                                       instanced);
            active_shader_->ssbo_vertex_fetch_bind_attribute(ssbo_attr);
            desc.vertex_descriptor.ssbo_attributes[desc.vertex_descriptor.num_ssbo_attributes] =
                ssbo_attr;
            desc.vertex_descriptor.num_ssbo_attributes++;
          }
          else {

            /* Handle Mat4 attributes. */
            if (a->comp_len == 16) {
              /* Debug safety checks. */
              BLI_assert_msg(mtl_attr.matrix_element_count == 4,
                             "mat4 type expected but there are fewer components");
              BLI_assert_msg(mtl_attr.size == 16, "Expecting subtype 'vec4' with 16 bytes");
              BLI_assert_msg(
                  mtl_attr.format == MTLVertexFormatFloat4,
                  "Per-attribute vertex format MUST be float4 for an input type of 'mat4'");

              /* We have found the 'ROOT' attribute. A mat4 contains 4 consecutive float4 attribute
               * locations we must map to. */
              for (int i = 0; i < a->comp_len / 4; i++) {
                desc.vertex_descriptor.attributes[mtl_attr.location + i].format =
                    MTLVertexFormatFloat4;
                /* Data is consecutive in the buffer for the whole matrix, each float4 will shift
                 * the offset by 16 bytes. */
                desc.vertex_descriptor.attributes[mtl_attr.location + i].offset =
                    attribute_offset + i * 16;
                /* All source data for a matrix is in the same singular buffer. */
                desc.vertex_descriptor.attributes[mtl_attr.location + i].buffer_index =
                    buffer_index;

                /* Update total attribute account. */
                desc.vertex_descriptor.total_attributes++;
                desc.vertex_descriptor.max_attribute_value = max_ii(
                    mtl_attr.location + i, desc.vertex_descriptor.max_attribute_value);
                MTL_LOG_INFO("-- Sub-Attrib Location: %d, offset: %d, buffer index: %d\n",
                             mtl_attr.location + i,
                             attribute_offset + i * 16,
                             buffer_index);

                /* Update attribute used-slot mask for array elements. */
                attr_mask &= ~(1 << (mtl_attr.location + i));
              }
              MTL_LOG_INFO(
                  "Float4x4 attribute type added for '%s' at attribute locations: %d to %d\n",
                  name,
                  mtl_attr.location,
                  mtl_attr.location + 3);
            }

            /* Ensure we are not exceeding the attribute limit. */
            BLI_assert(desc.vertex_descriptor.max_attribute_value <
                       MTL_MAX_VERTEX_INPUT_ATTRIBUTES);
          }
        }
        else {

          /* Handle Any required format conversions.
           * NOTE(Metal): If there is a mis-match between the format of an attribute
           * in the shader interface, and the specified format in the VertexBuffer VertexFormat,
           * we need to perform a format conversion.
           *
           * The Metal API can perform certain conversions internally during vertex assembly:
           *   - Type Normalization e.g short2 to float2 between 0.0 to 1.0.
           *   - Type Truncation e.g. Float4 to Float2.
           *   - Type expansion e,g, Float3 to Float4 (Following 0,0,0,1 for assignment to empty
           * elements).
           *
           * Certain conversion cannot be performed however, and in these cases, we need to
           * instruct the shader to generate a specialized version with a conversion routine upon
           * attribute read.
           *   - This handles cases such as conversion between types e.g. Integer to float without
           * normalization.
           *
           * For more information on the supported and unsupported conversions, see:
           * https://developer.apple.com/documentation/metal/mtlvertexattributedescriptor/1516081-format?language=objc
           */
          MTLVertexFormat converted_format;
          bool can_use_internal_conversion = mtl_convert_vertex_format(
              mtl_attr.format,
              (GPUVertCompType)a->comp_type,
              a->comp_len,
              (GPUVertFetchMode)a->fetch_mode,
              &converted_format);
          bool is_floating_point_format = (a->comp_type == GPU_COMP_F32);

          if (can_use_internal_conversion) {
            desc.vertex_descriptor.attributes[mtl_attr.location].format = converted_format;
            desc.vertex_descriptor.attributes[mtl_attr.location].format_conversion_mode =
                is_floating_point_format ? (GPUVertFetchMode)GPU_FETCH_FLOAT :
                                           (GPUVertFetchMode)GPU_FETCH_INT;
            BLI_assert(converted_format != MTLVertexFormatInvalid);
          }
          else {
            /* The internal implicit conversion is not supported.
             * In this case, we need to handle conversion inside the shader.
             * This is handled using `format_conversion_mode`.
             * `format_conversion_mode` is assigned the blender-specified fetch mode (GPU_FETCH_*).
             * This then controls how a given attribute is interpreted. The data will be read
             * as specified and then converted appropriately to the correct form.
             *
             * e.g. if `GPU_FETCH_INT_TO_FLOAT` is specified, the specialized read-routine
             * in the shader will read the data as an int, and cast this to floating point
             * representation. (Rather than reading the source data as float).
             *
             * NOTE: Even if full conversion is not supported, we may still partially perform an
             * implicit conversion where possible, such as vector truncation or expansion. */
            MTLVertexFormat converted_format;
            bool can_convert = mtl_vertex_format_resize(
                mtl_attr.format, a->comp_len, &converted_format);
            desc.vertex_descriptor.attributes[mtl_attr.location].format = can_convert ?
                                                                              converted_format :
                                                                              mtl_attr.format;
            desc.vertex_descriptor.attributes[mtl_attr.location].format_conversion_mode =
                (GPUVertFetchMode)a->fetch_mode;
            BLI_assert(desc.vertex_descriptor.attributes[mtl_attr.location].format !=
                       MTLVertexFormatInvalid);
          }
          desc.vertex_descriptor.attributes[mtl_attr.location].offset = attribute_offset;
          desc.vertex_descriptor.attributes[mtl_attr.location].buffer_index = buffer_index;
          desc.vertex_descriptor.max_attribute_value =
              ((mtl_attr.location) > desc.vertex_descriptor.max_attribute_value) ?
                  (mtl_attr.location) :
                  desc.vertex_descriptor.max_attribute_value;
          desc.vertex_descriptor.total_attributes++;
          /* SSBO Vertex Fetch attribute bind. */
          if (active_shader_->get_uses_ssbo_vertex_fetch()) {
            BLI_assert_msg(desc.vertex_descriptor.attributes[mtl_attr.location].format ==
                               mtl_attr.format,
                           "SSBO Vertex Fetch does not support attribute conversion.");

            MTLSSBOAttribute ssbo_attr(
                mtl_attr.index,
                buffer_index,
                attribute_offset,
                buffer_stride,
                MTLShader::ssbo_vertex_type_to_attr_type(
                    desc.vertex_descriptor.attributes[mtl_attr.location].format),
                instanced);

            active_shader_->ssbo_vertex_fetch_bind_attribute(ssbo_attr);
            desc.vertex_descriptor.ssbo_attributes[desc.vertex_descriptor.num_ssbo_attributes] =
                ssbo_attr;
            desc.vertex_descriptor.num_ssbo_attributes++;
          }

          /* NOTE: We are setting max_attribute_value to be up to the maximum found index, because
           * of this, it is possible that we may skip over certain attributes if they were not in
           * the source GPUVertFormat. */
          MTL_LOG_INFO(
              " -- Batch Attribute(%d): ORIG Shader Format: %d, ORIG Vert format: %d, Vert "
              "components: %d, Fetch Mode %d --> FINAL FORMAT: %d\n",
              mtl_attr.location,
              (int)mtl_attr.format,
              (int)a->comp_type,
              (int)a->comp_len,
              (int)a->fetch_mode,
              (int)desc.vertex_descriptor.attributes[mtl_attr.location].format);

          MTL_LOG_INFO(
              "  -- [Batch] matching %s attribute '%s' (Attribute Index: %d, Buffer index: %d, "
              "offset: %d)\n",
              (instanced) ? "instance" : "vertex",
              name,
              mtl_attr.location,
              buffer_index,
              attribute_offset);
        }
      }
    }
  }
  if (buffer_added) {
    return buffer_index;
  }
  return -1;
}

id<MTLRenderCommandEncoder> MTLBatch::bind(uint v_count)
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

  /* Verify Shader. */
  active_shader_ = (shader) ? static_cast<MTLShader *>(unwrap(shader)) : nullptr;

  if (active_shader_ == nullptr || !active_shader_->is_valid()) {
    /* Skip drawing if there is no valid Metal shader.
     * This will occur if the path through which the shader is prepared
     * is invalid (e.g. Python without create-info), or, the source shader uses a geometry pass. */
    BLI_assert_msg(false, "No valid Metal shader!");
    return nil;
  }

  /* Check if using SSBO Fetch Mode.
   * This is an alternative drawing mode to geometry shaders, wherein vertex buffers
   * are bound as readable (random-access) GPU buffers and certain descriptor properties
   * are passed using Shader uniforms. */
  bool uses_ssbo_fetch = active_shader_->get_uses_ssbo_vertex_fetch();

  /* Prepare Vertex Descriptor and extract VertexBuffers to bind. */
  MTLVertBuf *buffers[GPU_BATCH_VBO_MAX_LEN] = {nullptr};
  int num_buffers = 0;

  /* Ensure Index Buffer is ready. */
  MTLIndexBuf *mtl_elem = static_cast<MTLIndexBuf *>(reinterpret_cast<IndexBuf *>(this->elem));
  if (mtl_elem != NULL) {
    mtl_elem->upload_data();
  }

  /* Populate vertex descriptor with attribute binding information.
   * The vertex descriptor and buffer layout descriptors describe
   * how vertex data from bound vertex buffers maps to the
   * shader's input.
   * A unique vertex descriptor will result in a new PipelineStateObject
   * being generated for the currently bound shader. */
  prepare_vertex_descriptor_and_bindings(buffers, num_buffers);

  /* Prepare Vertex Buffers - Run before RenderCommandEncoder in case BlitCommandEncoder buffer
   * data operations are required. */
  for (int i = 0; i < num_buffers; i++) {
    MTLVertBuf *buf_at_index = buffers[i];
    if (buf_at_index == NULL) {
      BLI_assert_msg(
          false,
          "Total buffer count does not match highest buffer index, could be gaps in bindings");
      continue;
    }

    MTLVertBuf *mtlvbo = static_cast<MTLVertBuf *>(reinterpret_cast<VertBuf *>(buf_at_index));
    mtlvbo->bind();
  }

  /* Ensure render pass is active and fetch active RenderCommandEncoder. */
  id<MTLRenderCommandEncoder> rec = ctx->ensure_begin_render_pass();

  /* Fetch RenderPassState to enable resource binding for active pass. */
  MTLRenderPassState &rps = ctx->main_command_buffer.get_render_pass_state();

  /* Debug Check: Ensure Frame-buffer instance is not dirty. */
  BLI_assert(!ctx->main_command_buffer.get_active_framebuffer()->get_dirty());

  /* Bind Shader. */
  this->shader_bind();

  /* GPU debug markers. */
  if (G.debug & G_DEBUG_GPU) {
    [rec pushDebugGroup:[NSString stringWithFormat:@"Draw Commands%@ (GPUShader: %s)",
                                                   this->elem ? @"(indexed)" : @"",
                                                   active_shader_->get_interface()->get_name()]];
    [rec insertDebugSignpost:[NSString
                                 stringWithFormat:@"Draw Commands %@ (GPUShader: %s)",
                                                  this->elem ? @"(indexed)" : @"",
                                                  active_shader_->get_interface()->get_name()]];
  }

  /*** Bind Vertex Buffers and Index Buffers **/

  /* SSBO Vertex Fetch Buffer bindings. */
  if (uses_ssbo_fetch) {

    /* SSBO Vertex Fetch - Bind Index Buffer to appropriate slot -- if used. */
    id<MTLBuffer> idx_buffer = nil;
    GPUPrimType final_prim_type = this->prim_type;

    if (mtl_elem != nullptr) {

      /* Fetch index buffer. This function can situationally return an optimized
       * index buffer of a different primitive type. If this is the case, `final_prim_type`
       * and `v_count` will be updated with the new format.
       * NOTE: For indexed rendering, v_count represents the number of indices. */
      idx_buffer = mtl_elem->get_index_buffer(final_prim_type, v_count);
      BLI_assert(idx_buffer != nil);

      /* Update uniforms for SSBO-vertex-fetch-mode indexed rendering to flag usage. */
      int &uniform_ssbo_index_mode_u16 = active_shader_->uni_ssbo_uses_index_mode_u16;
      BLI_assert(uniform_ssbo_index_mode_u16 != -1);
      int uses_index_mode_u16 = (mtl_elem->index_type_ == GPU_INDEX_U16) ? 1 : 0;
      active_shader_->uniform_int(uniform_ssbo_index_mode_u16, 1, 1, &uses_index_mode_u16);
    }
    else {
      idx_buffer = ctx->get_null_buffer();
    }
    rps.bind_vertex_buffer(idx_buffer, 0, MTL_SSBO_VERTEX_FETCH_IBO_INDEX);

    /* Ensure all attributes are set. */
    active_shader_->ssbo_vertex_fetch_bind_attributes_end(rec);

    /* Bind NULL Buffers for unused vertex data slots. */
    id<MTLBuffer> null_buffer = ctx->get_null_buffer();
    BLI_assert(null_buffer != nil);
    for (int i = num_buffers; i < MTL_SSBO_VERTEX_FETCH_MAX_VBOS; i++) {
      if (rps.cached_vertex_buffer_bindings[i].metal_buffer == nil) {
        rps.bind_vertex_buffer(null_buffer, 0, i);
      }
    }

    /* Flag whether Indexed rendering is used or not. */
    int &uniform_ssbo_use_indexed = active_shader_->uni_ssbo_uses_indexed_rendering;
    BLI_assert(uniform_ssbo_use_indexed != -1);
    int uses_indexed_rendering = (mtl_elem != nullptr) ? 1 : 0;
    active_shader_->uniform_int(uniform_ssbo_use_indexed, 1, 1, &uses_indexed_rendering);

    /* Set SSBO-fetch-mode status uniforms. */
    BLI_assert(active_shader_->uni_ssbo_input_prim_type_loc != -1);
    BLI_assert(active_shader_->uni_ssbo_input_vert_count_loc != -1);
    GPU_shader_uniform_int_ex(reinterpret_cast<GPUShader *>(wrap(active_shader_)),
                              active_shader_->uni_ssbo_input_prim_type_loc,
                              1,
                              1,
                              (const int *)(&final_prim_type));
    GPU_shader_uniform_int_ex(reinterpret_cast<GPUShader *>(wrap(active_shader_)),
                              active_shader_->uni_ssbo_input_vert_count_loc,
                              1,
                              1,
                              (const int *)(&v_count));
  }

  /* Ensure Context Render Pipeline State is fully setup and ready to execute the draw.
   * This should happen after all other final rendering setup is complete. */
  MTLPrimitiveType mtl_prim_type = gpu_prim_type_to_metal(this->prim_type);
  if (!ctx->ensure_render_pipeline_state(mtl_prim_type)) {
    MTL_LOG_ERROR("Failed to prepare and apply render pipeline state.\n");
    BLI_assert(false);
    return nil;
  }

  /* Bind Vertex Buffers. */
  for (int i = 0; i < num_buffers; i++) {
    MTLVertBuf *buf_at_index = buffers[i];
    if (buf_at_index == NULL) {
      BLI_assert_msg(
          false,
          "Total buffer count does not match highest buffer index, could be gaps in bindings");
      continue;
    }
    /* Buffer handle. */
    MTLVertBuf *mtlvbo = static_cast<MTLVertBuf *>(reinterpret_cast<VertBuf *>(buf_at_index));
    mtlvbo->flag_used();

    /* Fetch buffer from MTLVertexBuffer and bind. */
    id<MTLBuffer> mtl_buffer = mtlvbo->get_metal_buffer();

    BLI_assert(mtl_buffer != nil);
    rps.bind_vertex_buffer(mtl_buffer, 0, i);
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

void MTLBatch::prepare_vertex_descriptor_and_bindings(MTLVertBuf **buffers, int &num_buffers)
{

  /* Here we populate the MTLContext vertex descriptor and resolve which buffers need to be bound.
   */
  MTLStateManager *state_manager = static_cast<MTLStateManager *>(
      MTLContext::get()->state_manager);
  MTLRenderPipelineStateDescriptor &desc = state_manager->get_pipeline_descriptor();
  const MTLShaderInterface *interface = active_shader_->get_interface();
  uint16_t attr_mask = interface->get_enabled_attribute_mask();

  /* Reset vertex descriptor to default state. */
  desc.reset_vertex_descriptor();

  /* Fetch Vertex and Instance Buffers. */
  Span<MTLVertBuf *> mtl_verts(reinterpret_cast<MTLVertBuf **>(this->verts),
                               GPU_BATCH_VBO_MAX_LEN);
  Span<MTLVertBuf *> mtl_inst(reinterpret_cast<MTLVertBuf **>(this->inst),
                              GPU_BATCH_INST_VBO_MAX_LEN);

  /* SSBO Vertex fetch also passes vertex descriptor information into the shader. */
  if (active_shader_->get_uses_ssbo_vertex_fetch()) {
    active_shader_->ssbo_vertex_fetch_bind_attributes_begin();
  }

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
  bool buffer_is_instanced[GPU_BATCH_VBO_MAX_LEN] = {false};

  VertexDescriptorShaderInterfacePair *descriptor = this->vao_cache.find(interface);
  if (descriptor) {
    desc.vertex_descriptor = descriptor->vertex_descriptor;
    attr_mask = descriptor->attr_mask;
    num_buffers = descriptor->num_buffers;

    for (int bid = 0; bid < GPU_BATCH_VBO_MAX_LEN; ++bid) {
      if (descriptor->bufferIds[bid].used) {
        if (descriptor->bufferIds[bid].is_instance) {
          buffers[bid] = mtl_inst[descriptor->bufferIds[bid].id];
          buffer_is_instanced[bid] = true;
        }
        else {
          buffers[bid] = mtl_verts[descriptor->bufferIds[bid].id];
          buffer_is_instanced[bid] = false;
        }
      }
    }

    /* Use cached ssbo attribute binding data. */
    if (active_shader_->get_uses_ssbo_vertex_fetch()) {
      BLI_assert(desc.vertex_descriptor.uses_ssbo_vertex_fetch);
      for (int attr_id = 0; attr_id < desc.vertex_descriptor.num_ssbo_attributes; attr_id++) {
        active_shader_->ssbo_vertex_fetch_bind_attribute(
            desc.vertex_descriptor.ssbo_attributes[attr_id]);
      }
    }
  }
  else {
    VertexDescriptorShaderInterfacePair pair{};
    pair.interface = interface;

    for (int i = 0; i < GPU_BATCH_VBO_MAX_LEN; ++i) {
      pair.bufferIds[i].id = -1;
      pair.bufferIds[i].is_instance = 0;
      pair.bufferIds[i].used = 0;
    }
    /* NOTE: Attribute extraction order from buffer is the reverse of the OpenGL as we flag once an
     * attribute is found, rather than pre-setting the mask. */
    /* Extract Instance attributes (These take highest priority). */
    for (int v = 0; v < GPU_BATCH_INST_VBO_MAX_LEN; v++) {
      if (mtl_inst[v]) {
        MTL_LOG_INFO(" -- [Batch] Checking bindings for bound instance buffer %p\n", mtl_inst[v]);
        int buffer_ind = this->prepare_vertex_binding(
            mtl_inst[v], desc, interface, attr_mask, true);
        if (buffer_ind >= 0) {
          buffers[buffer_ind] = mtl_inst[v];
          buffer_is_instanced[buffer_ind] = true;

          pair.bufferIds[buffer_ind].id = v;
          pair.bufferIds[buffer_ind].used = 1;
          pair.bufferIds[buffer_ind].is_instance = 1;
          num_buffers = ((buffer_ind + 1) > num_buffers) ? (buffer_ind + 1) : num_buffers;
        }
      }
    }

    /* Extract Vertex attributes (First-bound vertex buffer takes priority). */
    for (int v = 0; v < GPU_BATCH_VBO_MAX_LEN; v++) {
      if (mtl_verts[v] != NULL) {
        MTL_LOG_INFO(" -- [Batch] Checking bindings for bound vertex buffer %p\n", mtl_verts[v]);
        int buffer_ind = this->prepare_vertex_binding(
            mtl_verts[v], desc, interface, attr_mask, false);
        if (buffer_ind >= 0) {
          buffers[buffer_ind] = mtl_verts[v];
          buffer_is_instanced[buffer_ind] = false;

          pair.bufferIds[buffer_ind].id = v;
          pair.bufferIds[buffer_ind].used = 1;
          pair.bufferIds[buffer_ind].is_instance = 0;
          num_buffers = ((buffer_ind + 1) > num_buffers) ? (buffer_ind + 1) : num_buffers;
        }
      }
    }

    /* Add to VertexDescriptor cache */
    desc.vertex_descriptor.uses_ssbo_vertex_fetch = active_shader_->get_uses_ssbo_vertex_fetch();
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
            "number: %u)\n",
            active_shader_->get_interface()->get_name_at_offset(attr.name_offset),
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

#if TRUST_NO_ONE
  BLI_assert(v_count > 0 && i_count > 0);
#endif

  /* Setup RenderPipelineState for batch. */
  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  id<MTLRenderCommandEncoder> rec = this->bind(v_count);
  if (rec == nil) {
    /* End of draw. */
    this->unbind(rec);
    return;
  }

  /* Fetch IndexBuffer and resolve primitive type. */
  MTLIndexBuf *mtl_elem = static_cast<MTLIndexBuf *>(reinterpret_cast<IndexBuf *>(this->elem));
  MTLPrimitiveType mtl_prim_type = gpu_prim_type_to_metal(this->prim_type);

  /* Render using SSBO Vertex Fetch. */
  if (active_shader_->get_uses_ssbo_vertex_fetch()) {

    /* Submit draw call with modified vertex count, which reflects vertices per primitive defined
     * in the USE_SSBO_VERTEX_FETCH pragma. */
    int num_input_primitives = gpu_get_prim_count_from_type(v_count, this->prim_type);
    int output_num_verts = num_input_primitives *
                           active_shader_->get_ssbo_vertex_fetch_output_num_verts();
    BLI_assert_msg(
        mtl_vertex_count_fits_primitive_type(
            output_num_verts, active_shader_->get_ssbo_vertex_fetch_output_prim_type()),
        "Output Vertex count is not compatible with the requested output vertex primitive type");

    /* Set depth stencil state (requires knowledge of primitive type). */
    ctx->ensure_depth_stencil_state(active_shader_->get_ssbo_vertex_fetch_output_prim_type());

    [rec drawPrimitives:active_shader_->get_ssbo_vertex_fetch_output_prim_type()
            vertexStart:0
            vertexCount:output_num_verts
          instanceCount:i_count
           baseInstance:i_first];
    ctx->main_command_buffer.register_draw_counters(output_num_verts * i_count);
  }
  /* Perform regular draw. */
  else if (mtl_elem == NULL) {

    /* Primitive Type toplogy emulation. */
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

void MTLBatch::draw_advanced_indirect(GPUStorageBuf *indirect_buf, intptr_t offset)
{
  /* Setup RenderPipelineState for batch. */
  MTLContext *ctx = reinterpret_cast<MTLContext *>(GPU_context_active_get());
  id<MTLRenderCommandEncoder> rec = this->bind(0);
  if (rec == nil) {
    printf("Failed to open Render Command encoder for DRAW INDIRECT\n");

    /* End of draw. */
    this->unbind(rec);
    return;
  }

  /* Render using SSBO Vertex Fetch not supported by Draw Indirect.
   * NOTE: Add support? */
  if (active_shader_->get_uses_ssbo_vertex_fetch()) {
    printf("Draw indirect for SSBO vertex fetch disabled\n");

    /* End of draw. */
    this->unbind(rec);
    return;
  }

  /* Fetch IndexBuffer and resolve primitive type. */
  MTLIndexBuf *mtl_elem = static_cast<MTLIndexBuf *>(reinterpret_cast<IndexBuf *>(this->elem));
  MTLPrimitiveType mtl_prim_type = gpu_prim_type_to_metal(this->prim_type);

  if (mtl_needs_topology_emulation(this->prim_type)) {
    BLI_assert_msg(false, "Metal Topology emulation unsupported for draw indirect.\n");

    /* End of draw. */
    this->unbind(rec);
    return;
  }

  /* Fetch indirect buffer Metal handle. */
  MTLStorageBuf *mtlssbo = static_cast<MTLStorageBuf *>(unwrap(indirect_buf));
  id<MTLBuffer> mtl_indirect_buf = mtlssbo->get_metal_buffer();
  BLI_assert(mtl_indirect_buf != nil);
  if (mtl_indirect_buf == nil) {
    MTL_LOG_WARNING("Metal Indirect Draw Storage Buffer is nil.\n");

    /* End of draw. */
    this->unbind(rec);
    return;
  }

  if (mtl_elem == NULL) {
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
          data[line * 3 + 0] = line + 0;
          data[line * 3 + 1] = line + 1;
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
