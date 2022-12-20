/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.h"

#include "BLI_string.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>

#include <cstring>

#include "GPU_platform.h"
#include "GPU_vertex_format.h"

#include "mtl_common.hh"
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_pso_descriptor_state.hh"
#include "mtl_shader.hh"
#include "mtl_shader_generator.hh"
#include "mtl_shader_interface.hh"
#include "mtl_texture.hh"
#include "mtl_vertex_buffer.hh"

extern char datatoc_mtl_shader_common_msl[];

using namespace blender;
using namespace blender::gpu;
using namespace blender::gpu::shader;

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction.
 * \{ */

/* Create empty shader to be populated later. */
MTLShader::MTLShader(MTLContext *ctx, const char *name) : Shader(name)
{
  context_ = ctx;

  /* Create SHD builder to hold temporary resources until compilation is complete. */
  shd_builder_ = new MTLShaderBuilder();

#ifndef NDEBUG
  /* Remove invalid symbols from shader name to ensure debug entry-point function name is valid. */
  for (uint i : IndexRange(strlen(this->name))) {
    char c = this->name[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
    }
    else {
      this->name[i] = '_';
    }
  }
#endif
}

/* Create shader from MSL source. */
MTLShader::MTLShader(MTLContext *ctx,
                     MTLShaderInterface *interface,
                     const char *name,
                     NSString *input_vertex_source,
                     NSString *input_fragment_source,
                     NSString *vert_function_name,
                     NSString *frag_function_name)
    : MTLShader(ctx, name)
{
  BLI_assert([vert_function_name length]);
  BLI_assert([frag_function_name length]);

  this->set_vertex_function_name(vert_function_name);
  this->set_fragment_function_name(frag_function_name);
  this->shader_source_from_msl(input_vertex_source, input_fragment_source);
  this->set_interface(interface);
  this->finalize(nullptr);
}

MTLShader::~MTLShader()
{
  if (this->is_valid()) {

    /* Free uniform data block. */
    if (push_constant_data_ != nullptr) {
      MEM_freeN(push_constant_data_);
      push_constant_data_ = nullptr;
    }

    /* Free Metal resources. */
    if (shader_library_vert_ != nil) {
      [shader_library_vert_ release];
      shader_library_vert_ = nil;
    }
    if (shader_library_frag_ != nil) {
      [shader_library_frag_ release];
      shader_library_frag_ = nil;
    }

    if (pso_descriptor_ != nil) {
      [pso_descriptor_ release];
      pso_descriptor_ = nil;
    }

    /* Free Pipeline Cache. */
    for (const MTLRenderPipelineStateInstance *pso_inst : pso_cache_.values()) {
      if (pso_inst->vert) {
        [pso_inst->vert release];
      }
      if (pso_inst->frag) {
        [pso_inst->frag release];
      }
      if (pso_inst->pso) {
        [pso_inst->pso release];
      }
      delete pso_inst;
    }
    pso_cache_.clear();

    /* NOTE(Metal): #ShaderInterface deletion is handled in the super destructor `~Shader()`. */
  }
  valid_ = false;

  if (shd_builder_ != nullptr) {
    delete shd_builder_;
    shd_builder_ = nullptr;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader stage creation.
 * \{ */

void MTLShader::vertex_shader_from_glsl(MutableSpan<const char *> sources)
{
  /* Flag source as not being compiled from native MSL. */
  BLI_assert(shd_builder_ != nullptr);
  shd_builder_->source_from_msl_ = false;

  /* Remove #version tag entry. */
  sources[0] = "";

  /* Consolidate GLSL vertex sources. */
  std::stringstream ss;
  for (int i = 0; i < sources.size(); i++) {
    ss << sources[i] << std::endl;
  }
  shd_builder_->glsl_vertex_source_ = ss.str();
}

void MTLShader::geometry_shader_from_glsl(MutableSpan<const char *> sources)
{
  MTL_LOG_ERROR("MTLShader::geometry_shader_from_glsl - Geometry shaders unsupported!\n");
}

void MTLShader::fragment_shader_from_glsl(MutableSpan<const char *> sources)
{
  /* Flag source as not being compiled from native MSL. */
  BLI_assert(shd_builder_ != nullptr);
  shd_builder_->source_from_msl_ = false;

  /* Remove #version tag entry. */
  sources[0] = "";

  /* Consolidate GLSL fragment sources. */
  std::stringstream ss;
  for (int i = 0; i < sources.size(); i++) {
    ss << sources[i] << std::endl;
  }
  shd_builder_->glsl_fragment_source_ = ss.str();
}

void MTLShader::compute_shader_from_glsl(MutableSpan<const char *> sources)
{
  /* Remove #version tag entry. */
  sources[0] = "";

  /* TODO(Metal): Support compute shaders in Metal. */
  MTL_LOG_WARNING(
      "MTLShader::compute_shader_from_glsl - Compute shaders currently unsupported!\n");
}

bool MTLShader::finalize(const shader::ShaderCreateInfo *info)
{
  /* Check if Shader has already been finalized. */
  if (this->is_valid()) {
    MTL_LOG_ERROR("Shader (%p) '%s' has already been finalized!\n", this, this->name_get());
  }

  /* Perform GLSL to MSL source translation. */
  BLI_assert(shd_builder_ != nullptr);
  if (!shd_builder_->source_from_msl_) {
    bool success = generate_msl_from_glsl(info);
    if (!success) {
      /* GLSL to MSL translation has failed, or is unsupported for this shader. */
      valid_ = false;
      BLI_assert_msg(false, "Shader translation from GLSL to MSL has failed. \n");

      /* Create empty interface to allow shader to be silently used. */
      MTLShaderInterface *mtl_interface = new MTLShaderInterface(this->name_get());
      this->set_interface(mtl_interface);

      /* Release temporary compilation resources. */
      delete shd_builder_;
      shd_builder_ = nullptr;
      return false;
    }
  }

  /* Ensure we have a valid shader interface. */
  MTLShaderInterface *mtl_interface = this->get_interface();
  BLI_assert(mtl_interface != nullptr);

  /* Verify Context handle, fetch device and compile shader. */
  BLI_assert(context_);
  id<MTLDevice> device = context_->device;
  BLI_assert(device != nil);

  /* Ensure source and stage entry-point names are set. */
  BLI_assert([vertex_function_name_ length] > 0);
  if (transform_feedback_type_ == GPU_SHADER_TFB_NONE) {
    BLI_assert([fragment_function_name_ length] > 0);
  }
  BLI_assert(shd_builder_ != nullptr);
  BLI_assert([shd_builder_->msl_source_vert_ length] > 0);

  @autoreleasepool {
    MTLCompileOptions *options = [[[MTLCompileOptions alloc] init] autorelease];
    options.languageVersion = MTLLanguageVersion2_2;
    options.fastMathEnabled = YES;

    NSString *source_to_compile = shd_builder_->msl_source_vert_;
    for (int src_stage = 0; src_stage <= 1; src_stage++) {

      source_to_compile = (src_stage == 0) ? shd_builder_->msl_source_vert_ :
                                             shd_builder_->msl_source_frag_;

      /* Transform feedback, skip compilation. */
      if (src_stage == 1 && (transform_feedback_type_ != GPU_SHADER_TFB_NONE)) {
        shader_library_frag_ = nil;
        break;
      }

      /* Concatenate common source. */
      NSString *str = [NSString stringWithUTF8String:datatoc_mtl_shader_common_msl];
      NSString *source_with_header_a = [str stringByAppendingString:source_to_compile];

      /* Inject unique context ID to avoid cross-context shader cache collisions.
       * Required on macOS 11.0. */
      NSString *source_with_header = source_with_header_a;
      if (@available(macos 11.0, *)) {
        /* Pass-through. Availability syntax requirement, expression cannot be negated. */
      }
      else {
        source_with_header = [source_with_header_a
            stringByAppendingString:[NSString stringWithFormat:@"\n\n#define MTL_CONTEXT_IND %d\n",
                                                               context_->context_id]];
      }
      [source_with_header retain];

      /* Prepare Shader Library. */
      NSError *error = nullptr;
      id<MTLLibrary> library = [device newLibraryWithSource:source_with_header
                                                    options:options
                                                      error:&error];
      if (error) {
        /* Only exit out if genuine error and not warning. */
        if ([[error localizedDescription] rangeOfString:@"Compilation succeeded"].location ==
            NSNotFound) {
          NSLog(
              @"Compile Error - Metal Shader Library (Stage: %d), error %@ \n", src_stage, error);
          BLI_assert(false);

          /* Release temporary compilation resources. */
          delete shd_builder_;
          shd_builder_ = nullptr;
          return false;
        }
      }

      MTL_LOG_INFO("Successfully compiled Metal Shader Library (Stage: %d) for shader; %s\n",
                   src_stage,
                   name);
      BLI_assert(library != nil);
      if (src_stage == 0) {
        /* Retain generated library and assign debug name. */
        shader_library_vert_ = library;
        [shader_library_vert_ retain];
        shader_library_vert_.label = [NSString stringWithUTF8String:this->name];
      }
      else {
        /* Retain generated library for fragment shader and assign debug name. */
        shader_library_frag_ = library;
        [shader_library_frag_ retain];
        shader_library_frag_.label = [NSString stringWithUTF8String:this->name];
      }

      [source_with_header autorelease];
    }
    pso_descriptor_.label = [NSString stringWithUTF8String:this->name];

    /* Prepare descriptor. */
    pso_descriptor_ = [[MTLRenderPipelineDescriptor alloc] init];
    [pso_descriptor_ retain];

    /* Shader has successfully been created. */
    valid_ = true;

    /* Prepare backing data storage for local uniforms. */
    const MTLShaderUniformBlock &push_constant_block = mtl_interface->get_push_constant_block();
    if (push_constant_block.size > 0) {
      push_constant_data_ = MEM_callocN(push_constant_block.size, __func__);
      this->push_constant_bindstate_mark_dirty(true);
    }
    else {
      push_constant_data_ = nullptr;
    }
  }

  /* Release temporary compilation resources. */
  delete shd_builder_;
  shd_builder_ = nullptr;
  return true;
}

void MTLShader::transform_feedback_names_set(Span<const char *> name_list,
                                             const eGPUShaderTFBType geom_type)
{
  tf_output_name_list_.clear();
  for (int i = 0; i < name_list.size(); i++) {
    tf_output_name_list_.append(std::string(name_list[i]));
  }
  transform_feedback_type_ = geom_type;
}

bool MTLShader::transform_feedback_enable(GPUVertBuf *buf)
{
  BLI_assert(transform_feedback_type_ != GPU_SHADER_TFB_NONE);
  BLI_assert(buf);
  transform_feedback_active_ = true;
  transform_feedback_vertbuf_ = buf;
  BLI_assert(static_cast<MTLVertBuf *>(unwrap(transform_feedback_vertbuf_))->get_usage_type() ==
             GPU_USAGE_DEVICE_ONLY);
  return true;
}

void MTLShader::transform_feedback_disable()
{
  transform_feedback_active_ = false;
  transform_feedback_vertbuf_ = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader Binding.
 * \{ */

void MTLShader::bind()
{
  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  if (interface == nullptr || !this->is_valid()) {
    MTL_LOG_WARNING(
        "MTLShader::bind - Shader '%s' has no valid implementation in Metal, draw calls will be "
        "skipped.\n",
        this->name_get());
  }
  ctx->pipeline_state.active_shader = this;
}

void MTLShader::unbind()
{
  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
  ctx->pipeline_state.active_shader = nullptr;
}

void MTLShader::uniform_float(int location, int comp_len, int array_size, const float *data)
{
  BLI_assert(this);
  if (!this->is_valid()) {
    return;
  }
  MTLShaderInterface *mtl_interface = get_interface();
  if (location < 0 || location >= mtl_interface->get_total_uniforms()) {
    MTL_LOG_WARNING("Uniform location %d is not valid in Shader %s\n", location, this->name_get());
    return;
  }

  /* Fetch more information about uniform from interface. */
  const MTLShaderUniform &uniform = mtl_interface->get_uniform(location);

  /* Prepare to copy data into local shader push constant memory block. */
  BLI_assert(push_constant_data_ != nullptr);
  uint8_t *dest_ptr = (uint8_t *)push_constant_data_;
  dest_ptr += uniform.byte_offset;
  uint32_t copy_size = sizeof(float) * comp_len * array_size;

  /* Test per-element size. It is valid to copy less array elements than the total, but each
   * array element needs to match. */
  uint32_t source_per_element_size = sizeof(float) * comp_len;
  uint32_t dest_per_element_size = uniform.size_in_bytes / uniform.array_len;
  BLI_assert_msg(
      source_per_element_size <= dest_per_element_size,
      "source Per-array-element size must be smaller than destination storage capacity for "
      "that data");

  if (source_per_element_size < dest_per_element_size) {
    switch (uniform.type) {

      /* Special case for handling 'vec3' array upload. */
      case MTL_DATATYPE_FLOAT3: {
        int numvecs = uniform.array_len;
        uint8_t *data_c = (uint8_t *)data;

        /* It is more efficient on the host to only modify data if it has changed.
         * Data modifications are small, so memory comparison is cheap.
         * If uniforms have remained unchanged, then we avoid both copying
         * data into the local uniform struct, and upload of the modified uniform
         * contents in the command stream. */
        bool changed = false;
        for (int i = 0; i < numvecs; i++) {
          changed = changed || (memcmp((void *)dest_ptr, (void *)data_c, sizeof(float) * 3) != 0);
          if (changed) {
            memcpy((void *)dest_ptr, (void *)data_c, sizeof(float) * 3);
          }
          data_c += sizeof(float) * 3;
          dest_ptr += sizeof(float) * 4;
        }
        if (changed) {
          this->push_constant_bindstate_mark_dirty(true);
        }
        return;
      }

      /* Special case for handling 'mat3' upload. */
      case MTL_DATATYPE_FLOAT3x3: {
        int numvecs = 3 * uniform.array_len;
        uint8_t *data_c = (uint8_t *)data;

        /* It is more efficient on the host to only modify data if it has changed.
         * Data modifications are small, so memory comparison is cheap.
         * If uniforms have remained unchanged, then we avoid both copying
         * data into the local uniform struct, and upload of the modified uniform
         * contents in the command stream. */
        bool changed = false;
        for (int i = 0; i < numvecs; i++) {
          changed = changed || (memcmp((void *)dest_ptr, (void *)data_c, sizeof(float) * 3) != 0);
          if (changed) {
            memcpy((void *)dest_ptr, (void *)data_c, sizeof(float) * 3);
          }
          data_c += sizeof(float) * 3;
          dest_ptr += sizeof(float) * 4;
        }
        if (changed) {
          this->push_constant_bindstate_mark_dirty(true);
        }
        return;
      }
      default:
        shader_debug_printf("INCOMPATIBLE UNIFORM TYPE: %d\n", uniform.type);
        break;
    }
  }

  /* Debug checks. */
  BLI_assert_msg(
      copy_size <= uniform.size_in_bytes,
      "Size of provided uniform data is greater than size specified in Shader interface\n");

  /* Only flag UBO as modified if data is different -- This can avoid re-binding of unmodified
   * local uniform data. */
  bool data_changed = (memcmp((void *)dest_ptr, (void *)data, copy_size) != 0);
  if (data_changed) {
    this->push_constant_bindstate_mark_dirty(true);
    memcpy((void *)dest_ptr, (void *)data, copy_size);
  }
}

void MTLShader::uniform_int(int location, int comp_len, int array_size, const int *data)
{
  BLI_assert(this);
  if (!this->is_valid()) {
    return;
  }

  /* NOTE(Metal): Invalidation warning for uniform re-mapping of texture slots, unsupported in
   * Metal, as we cannot point a texture binding at a different slot. */
  MTLShaderInterface *mtl_interface = this->get_interface();
  if (location >= mtl_interface->get_total_uniforms() &&
      location < (mtl_interface->get_total_uniforms() + mtl_interface->get_total_textures())) {
    MTL_LOG_WARNING(
        "Texture uniform location re-mapping unsupported in Metal. (Possibly also bad uniform "
        "location %d)\n",
        location);
    return;
  }

  if (location < 0 || location >= mtl_interface->get_total_uniforms()) {
    MTL_LOG_WARNING(
        "Uniform is not valid at location %d - Shader %s\n", location, this->name_get());
    return;
  }

  /* Fetch more information about uniform from interface. */
  const MTLShaderUniform &uniform = mtl_interface->get_uniform(location);

  /* Determine data location in uniform block. */
  BLI_assert(push_constant_data_ != nullptr);
  uint8_t *ptr = (uint8_t *)push_constant_data_;
  ptr += uniform.byte_offset;

  /* Copy data into local block. Only flag UBO as modified if data is different
   * This can avoid re-binding of unmodified local uniform data, reducing
   * the total number of copy operations needed and data transfers between
   * CPU and GPU. */
  bool data_changed = (memcmp((void *)ptr, (void *)data, sizeof(int) * comp_len * array_size) !=
                       0);
  if (data_changed) {
    this->push_constant_bindstate_mark_dirty(true);
    memcpy((void *)ptr, (void *)data, sizeof(int) * comp_len * array_size);
  }
}

bool MTLShader::get_push_constant_is_dirty()
{
  return push_constant_modified_;
}

void MTLShader::push_constant_bindstate_mark_dirty(bool is_dirty)
{
  push_constant_modified_ = is_dirty;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name METAL Custom Behavior
 * \{ */

void MTLShader::set_vertex_function_name(NSString *vert_function_name)
{
  vertex_function_name_ = vert_function_name;
}

void MTLShader::set_fragment_function_name(NSString *frag_function_name)
{
  fragment_function_name_ = frag_function_name;
}

void MTLShader::shader_source_from_msl(NSString *input_vertex_source,
                                       NSString *input_fragment_source)
{
  BLI_assert(shd_builder_ != nullptr);
  shd_builder_->msl_source_vert_ = input_vertex_source;
  shd_builder_->msl_source_frag_ = input_fragment_source;
  shd_builder_->source_from_msl_ = true;
}

void MTLShader::set_interface(MTLShaderInterface *interface)
{
  /* Assign gpu::Shader super-class interface. */
  BLI_assert(Shader::interface == nullptr);
  Shader::interface = interface;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bake Pipeline State Objects
 * \{ */

/**
 * Bakes or fetches a pipeline state using the current
 * #MTLRenderPipelineStateDescriptor state.
 *
 * This state contains information on shader inputs/outputs, such
 * as the vertex descriptor, used to control vertex assembly for
 * current vertex data, and active render target information,
 * describing the output attachment pixel formats.
 *
 * Other rendering parameters such as global point-size, blend state, color mask
 * etc; are also used. See mtl_shader.h for full #MLRenderPipelineStateDescriptor.
 */
MTLRenderPipelineStateInstance *MTLShader::bake_current_pipeline_state(
    MTLContext *ctx, MTLPrimitiveTopologyClass prim_type)
{
  /* NOTE(Metal): PSO cache can be accessed from multiple threads, though these operations should
   * be thread-safe due to organization of high-level renderer. If there are any issues, then
   * access can be guarded as appropriate. */
  BLI_assert(this);
  MTLShaderInterface *mtl_interface = this->get_interface();
  BLI_assert(mtl_interface);
  BLI_assert(this->is_valid());

  /* NOTE(Metal): Vertex input assembly description will have been populated externally
   * via #MTLBatch or #MTLImmediate during binding or draw. */

  /* Resolve Context Frame-buffer state. */
  MTLFrameBuffer *framebuffer = ctx->get_current_framebuffer();

  /* Update global pipeline descriptor. */
  MTLStateManager *state_manager = static_cast<MTLStateManager *>(
      MTLContext::get()->state_manager);
  MTLRenderPipelineStateDescriptor &pipeline_descriptor = state_manager->get_pipeline_descriptor();

  pipeline_descriptor.num_color_attachments = 0;
  for (int attachment = 0; attachment < GPU_FB_MAX_COLOR_ATTACHMENT; attachment++) {
    MTLAttachment color_attachment = framebuffer->get_color_attachment(attachment);

    if (color_attachment.used) {
      /* If SRGB is disabled and format is SRGB, use color data directly with no conversions
       * between linear and SRGB. */
      MTLPixelFormat mtl_format = gpu_texture_format_to_metal(
          color_attachment.texture->format_get());
      if (framebuffer->get_is_srgb() && !framebuffer->get_srgb_enabled()) {
        mtl_format = MTLPixelFormatRGBA8Unorm;
      }
      pipeline_descriptor.color_attachment_format[attachment] = mtl_format;
    }
    else {
      pipeline_descriptor.color_attachment_format[attachment] = MTLPixelFormatInvalid;
    }

    pipeline_descriptor.num_color_attachments += (color_attachment.used) ? 1 : 0;
  }
  MTLAttachment depth_attachment = framebuffer->get_depth_attachment();
  MTLAttachment stencil_attachment = framebuffer->get_stencil_attachment();
  pipeline_descriptor.depth_attachment_format = (depth_attachment.used) ?
                                                    gpu_texture_format_to_metal(
                                                        depth_attachment.texture->format_get()) :
                                                    MTLPixelFormatInvalid;
  pipeline_descriptor.stencil_attachment_format =
      (stencil_attachment.used) ?
          gpu_texture_format_to_metal(stencil_attachment.texture->format_get()) :
          MTLPixelFormatInvalid;

  /* Resolve Context Pipeline State (required by PSO). */
  pipeline_descriptor.color_write_mask = ctx->pipeline_state.color_write_mask;
  pipeline_descriptor.blending_enabled = ctx->pipeline_state.blending_enabled;
  pipeline_descriptor.alpha_blend_op = ctx->pipeline_state.alpha_blend_op;
  pipeline_descriptor.rgb_blend_op = ctx->pipeline_state.rgb_blend_op;
  pipeline_descriptor.dest_alpha_blend_factor = ctx->pipeline_state.dest_alpha_blend_factor;
  pipeline_descriptor.dest_rgb_blend_factor = ctx->pipeline_state.dest_rgb_blend_factor;
  pipeline_descriptor.src_alpha_blend_factor = ctx->pipeline_state.src_alpha_blend_factor;
  pipeline_descriptor.src_rgb_blend_factor = ctx->pipeline_state.src_rgb_blend_factor;
  pipeline_descriptor.point_size = ctx->pipeline_state.point_size;

  /* Resolve clipping plane enablement. */
  pipeline_descriptor.clipping_plane_enable_mask = 0;
  for (const int plane : IndexRange(6)) {
    pipeline_descriptor.clipping_plane_enable_mask =
        pipeline_descriptor.clipping_plane_enable_mask |
        ((ctx->pipeline_state.clip_distance_enabled[plane]) ? (1 << plane) : 0);
  }

  /* Primitive Type -- Primitive topology class needs to be specified for layered rendering. */
  bool requires_specific_topology_class = uses_mtl_array_index_ ||
                                          prim_type == MTLPrimitiveTopologyClassPoint;
  pipeline_descriptor.vertex_descriptor.prim_topology_class =
      (requires_specific_topology_class) ? prim_type : MTLPrimitiveTopologyClassUnspecified;

  /* Check if current PSO exists in the cache. */
  MTLRenderPipelineStateInstance **pso_lookup = pso_cache_.lookup_ptr(pipeline_descriptor);
  MTLRenderPipelineStateInstance *pipeline_state = (pso_lookup) ? *pso_lookup : nullptr;
  if (pipeline_state != nullptr) {
    return pipeline_state;
  }

  shader_debug_printf("Baking new pipeline variant for shader: %s\n", this->name);

  /* Generate new Render Pipeline State Object (PSO). */
  @autoreleasepool {
    /* Prepare Render Pipeline Descriptor. */

    /* Setup function specialization constants, used to modify and optimize
     * generated code based on current render pipeline configuration. */
    MTLFunctionConstantValues *values = [[MTLFunctionConstantValues new] autorelease];

    /* Prepare Vertex descriptor based on current pipeline vertex binding state. */
    MTLRenderPipelineStateDescriptor &current_state = pipeline_descriptor;
    MTLRenderPipelineDescriptor *desc = pso_descriptor_;
    [desc reset];
    pso_descriptor_.label = [NSString stringWithUTF8String:this->name];

    /* Offset the bind index for Uniform buffers such that they begin after the VBO
     * buffer bind slots. `MTL_uniform_buffer_base_index` is passed as a function
     * specialization constant, customized per unique pipeline state permutation.
     *
     * NOTE: For binding point compaction, we could use the number of VBOs present
     * in the current PSO configuration `current_state.vertex_descriptor.num_vert_buffers`).
     * However, it is more efficient to simply offset the uniform buffer base index to the
     * maximal number of VBO bind-points, as then UBO bind-points for similar draw calls
     * will align and avoid the requirement for additional binding. */
    int MTL_uniform_buffer_base_index = GPU_BATCH_VBO_MAX_LEN;

    /* Null buffer index is used if an attribute is not found in the
     * bound VBOs #VertexFormat. */
    int null_buffer_index = current_state.vertex_descriptor.num_vert_buffers;
    bool using_null_buffer = false;

    if (this->get_uses_ssbo_vertex_fetch()) {
      /* If using SSBO Vertex fetch mode, no vertex descriptor is required
       * as we wont be using stage-in. */
      desc.vertexDescriptor = nil;
      desc.inputPrimitiveTopology = MTLPrimitiveTopologyClassUnspecified;

      /* We want to offset the uniform buffer base to allow for sufficient VBO binding slots - We
       * also require +1 slot for the Index buffer. */
      MTL_uniform_buffer_base_index = MTL_SSBO_VERTEX_FETCH_IBO_INDEX + 1;
    }
    else {
      for (const uint i : IndexRange(current_state.vertex_descriptor.max_attribute_value + 1)) {

        /* Metal back-end attribute descriptor state. */
        MTLVertexAttributeDescriptorPSO &attribute_desc =
            current_state.vertex_descriptor.attributes[i];

        /* Flag format conversion */
        /* In some cases, Metal cannot implicitly convert between data types.
         * In these instances, the fetch mode #GPUVertFetchMode as provided in the vertex format
         * is passed in, and used to populate function constants named: MTL_AttributeConvert0..15.
         *
         * It is then the responsibility of the vertex shader to perform any necessary type
         * casting.
         *
         * See `mtl_shader.hh` for more information. Relevant Metal API documentation:
         * https://developer.apple.com/documentation/metal/mtlvertexattributedescriptor/1516081-format?language=objc
         */
        if (attribute_desc.format == MTLVertexFormatInvalid) {
          /* If attributes are non-contiguous, we can skip over gaps. */
          MTL_LOG_WARNING(
              "MTLShader: baking pipeline state for '%s'- skipping input attribute at "
              "index '%d' but none was specified in the current vertex state\n",
              mtl_interface->get_name(),
              i);

          /* Write out null conversion constant if attribute unused. */
          int MTL_attribute_conversion_mode = 0;
          [values setConstantValue:&MTL_attribute_conversion_mode
                              type:MTLDataTypeInt
                          withName:[NSString stringWithFormat:@"MTL_AttributeConvert%d", i]];
          continue;
        }

        int MTL_attribute_conversion_mode = (int)attribute_desc.format_conversion_mode;
        [values setConstantValue:&MTL_attribute_conversion_mode
                            type:MTLDataTypeInt
                        withName:[NSString stringWithFormat:@"MTL_AttributeConvert%d", i]];
        if (MTL_attribute_conversion_mode == GPU_FETCH_INT_TO_FLOAT_UNIT ||
            MTL_attribute_conversion_mode == GPU_FETCH_INT_TO_FLOAT) {
          shader_debug_printf(
              "TODO(Metal): Shader %s needs to support internal format conversion\n",
              mtl_interface->name);
        }

        /* Copy metal back-end attribute descriptor state into PSO descriptor.
         * NOTE: need to copy each element due to direct assignment restrictions.
         * Also note */
        MTLVertexAttributeDescriptor *mtl_attribute = desc.vertexDescriptor.attributes[i];

        mtl_attribute.format = attribute_desc.format;
        mtl_attribute.offset = attribute_desc.offset;
        mtl_attribute.bufferIndex = attribute_desc.buffer_index;
      }

      for (const uint i : IndexRange(current_state.vertex_descriptor.num_vert_buffers)) {
        /* Metal back-end state buffer layout. */
        const MTLVertexBufferLayoutDescriptorPSO &buf_layout =
            current_state.vertex_descriptor.buffer_layouts[i];
        /* Copy metal back-end buffer layout state into PSO descriptor.
         * NOTE: need to copy each element due to copying from internal
         * back-end descriptor to Metal API descriptor. */
        MTLVertexBufferLayoutDescriptor *mtl_buf_layout = desc.vertexDescriptor.layouts[i];

        mtl_buf_layout.stepFunction = buf_layout.step_function;
        mtl_buf_layout.stepRate = buf_layout.step_rate;
        mtl_buf_layout.stride = buf_layout.stride;
      }

      /* Mark empty attribute conversion. */
      for (int i = current_state.vertex_descriptor.max_attribute_value + 1;
           i < GPU_VERT_ATTR_MAX_LEN;
           i++) {
        int MTL_attribute_conversion_mode = 0;
        [values setConstantValue:&MTL_attribute_conversion_mode
                            type:MTLDataTypeInt
                        withName:[NSString stringWithFormat:@"MTL_AttributeConvert%d", i]];
      }

      /* DEBUG: Missing/empty attributes. */
      /* Attributes are normally mapped as part of the state setting based on the used
       * #GPUVertFormat, however, if attributes have not been set, we can sort them out here. */
      for (const uint i : IndexRange(mtl_interface->get_total_attributes())) {
        const MTLShaderInputAttribute &attribute = mtl_interface->get_attribute(i);
        MTLVertexAttributeDescriptor *current_attribute =
            desc.vertexDescriptor.attributes[attribute.location];

        if (current_attribute.format == MTLVertexFormatInvalid) {
#if MTL_DEBUG_SHADER_ATTRIBUTES == 1
          printf("-> Filling in unbound attribute '%s' for shader PSO '%s' with location: %u\n",
                 mtl_interface->get_name_at_offset(attribute.name_offset),
                 mtl_interface->get_name(),
                 attribute.location);
#endif
          current_attribute.format = attribute.format;
          current_attribute.offset = 0;
          current_attribute.bufferIndex = null_buffer_index;

          /* Add Null vert buffer binding for invalid attributes. */
          if (!using_null_buffer) {
            MTLVertexBufferLayoutDescriptor *null_buf_layout =
                desc.vertexDescriptor.layouts[null_buffer_index];

            /* Use constant step function such that null buffer can
             * contain just a singular dummy attribute. */
            null_buf_layout.stepFunction = MTLVertexStepFunctionConstant;
            null_buf_layout.stepRate = 0;
            null_buf_layout.stride = max_ii(null_buf_layout.stride, attribute.size);

            /* If we are using the maximum number of vertex buffers, or tight binding indices,
             * MTL_uniform_buffer_base_index needs shifting to the bind slot after the null buffer
             * index. */
            if (null_buffer_index >= MTL_uniform_buffer_base_index) {
              MTL_uniform_buffer_base_index = null_buffer_index + 1;
            }
            using_null_buffer = true;
#if MTL_DEBUG_SHADER_ATTRIBUTES == 1
            MTL_LOG_INFO("Setting up buffer binding for null attribute with buffer index %d\n",
                         null_buffer_index);
#endif
          }
        }
      }

      /* Primitive Topology. */
      desc.inputPrimitiveTopology = pipeline_descriptor.vertex_descriptor.prim_topology_class;
    }

    /* Update constant value for 'MTL_uniform_buffer_base_index'. */
    [values setConstantValue:&MTL_uniform_buffer_base_index
                        type:MTLDataTypeInt
                    withName:@"MTL_uniform_buffer_base_index"];

    /* Transform feedback constant.
     * Ensure buffer is placed after existing buffers, including default buffers. */
    int MTL_transform_feedback_buffer_index = (this->transform_feedback_type_ !=
                                               GPU_SHADER_TFB_NONE) ?
                                                  MTL_uniform_buffer_base_index +
                                                      mtl_interface->get_max_ubo_index() + 2 :
                                                  -1;

    if (this->transform_feedback_type_ != GPU_SHADER_TFB_NONE) {
      [values setConstantValue:&MTL_transform_feedback_buffer_index
                          type:MTLDataTypeInt
                      withName:@"MTL_transform_feedback_buffer_index"];
    }

    /* Clipping planes. */
    int MTL_clip_distances_enabled = (pipeline_descriptor.clipping_plane_enable_mask > 0) ? 1 : 0;

    /* Only define specialization constant if planes are required.
     * We guard clip_planes usage on this flag. */
    [values setConstantValue:&MTL_clip_distances_enabled
                        type:MTLDataTypeInt
                    withName:@"MTL_clip_distances_enabled"];

    if (MTL_clip_distances_enabled > 0) {
      /* Assign individual enablement flags. Only define a flag function constant
       * if it is used. */
      for (const int plane : IndexRange(6)) {
        int plane_enabled = ctx->pipeline_state.clip_distance_enabled[plane] ? 1 : 0;
        if (plane_enabled) {
          [values
              setConstantValue:&plane_enabled
                          type:MTLDataTypeInt
                      withName:[NSString stringWithFormat:@"MTL_clip_distance_enabled%d", plane]];
        }
      }
    }

    /* gl_PointSize constant. */
    bool null_pointsize = true;
    float MTL_pointsize = pipeline_descriptor.point_size;
    if (pipeline_descriptor.vertex_descriptor.prim_topology_class ==
        MTLPrimitiveTopologyClassPoint) {
      /* `if pointsize is > 0.0`, PROGRAM_POINT_SIZE is enabled, and `gl_PointSize` shader keyword
       * overrides the value. Otherwise, if < 0.0, use global constant point size. */
      if (MTL_pointsize < 0.0) {
        MTL_pointsize = fabsf(MTL_pointsize);
        [values setConstantValue:&MTL_pointsize
                            type:MTLDataTypeFloat
                        withName:@"MTL_global_pointsize"];
        null_pointsize = false;
      }
    }

    if (null_pointsize) {
      MTL_pointsize = 0.0f;
      [values setConstantValue:&MTL_pointsize
                          type:MTLDataTypeFloat
                      withName:@"MTL_global_pointsize"];
    }

    /* Compile functions */
    NSError *error = nullptr;
    desc.vertexFunction = [shader_library_vert_ newFunctionWithName:vertex_function_name_
                                                     constantValues:values
                                                              error:&error];
    if (error) {
      NSLog(@"Compile Error - Metal Shader vertex function, error %@", error);

      /* Only exit out if genuine error and not warning */
      if ([[error localizedDescription] rangeOfString:@"Compilation succeeded"].location ==
          NSNotFound) {
        BLI_assert(false);
        return nullptr;
      }
    }

    /* If transform feedback is used, Vertex-only stage */
    if (transform_feedback_type_ == GPU_SHADER_TFB_NONE) {
      desc.fragmentFunction = [shader_library_frag_ newFunctionWithName:fragment_function_name_
                                                         constantValues:values
                                                                  error:&error];
      if (error) {
        NSLog(@"Compile Error - Metal Shader fragment function, error %@", error);

        /* Only exit out if genuine error and not warning */
        if ([[error localizedDescription] rangeOfString:@"Compilation succeeded"].location ==
            NSNotFound) {
          BLI_assert(false);
          return nullptr;
        }
      }
    }
    else {
      desc.fragmentFunction = nil;
      desc.rasterizationEnabled = false;
    }

    /* Setup pixel format state */
    for (int color_attachment = 0; color_attachment < GPU_FB_MAX_COLOR_ATTACHMENT;
         color_attachment++) {
      /* Fetch color attachment pixel format in back-end pipeline state. */
      MTLPixelFormat pixel_format = current_state.color_attachment_format[color_attachment];
      /* Populate MTL API PSO attachment descriptor. */
      MTLRenderPipelineColorAttachmentDescriptor *col_attachment =
          desc.colorAttachments[color_attachment];

      col_attachment.pixelFormat = pixel_format;
      if (pixel_format != MTLPixelFormatInvalid) {
        bool format_supports_blending = mtl_format_supports_blending(pixel_format);

        col_attachment.writeMask = current_state.color_write_mask;
        col_attachment.blendingEnabled = current_state.blending_enabled &&
                                         format_supports_blending;
        if (format_supports_blending && current_state.blending_enabled) {
          col_attachment.alphaBlendOperation = current_state.alpha_blend_op;
          col_attachment.rgbBlendOperation = current_state.rgb_blend_op;
          col_attachment.destinationAlphaBlendFactor = current_state.dest_alpha_blend_factor;
          col_attachment.destinationRGBBlendFactor = current_state.dest_rgb_blend_factor;
          col_attachment.sourceAlphaBlendFactor = current_state.src_alpha_blend_factor;
          col_attachment.sourceRGBBlendFactor = current_state.src_rgb_blend_factor;
        }
        else {
          if (current_state.blending_enabled && !format_supports_blending) {
            shader_debug_printf(
                "[Warning] Attempting to Bake PSO, but MTLPixelFormat %d does not support "
                "blending\n",
                *((int *)&pixel_format));
          }
        }
      }
    }
    desc.depthAttachmentPixelFormat = current_state.depth_attachment_format;
    desc.stencilAttachmentPixelFormat = current_state.stencil_attachment_format;

    /* Compile PSO */

    MTLAutoreleasedRenderPipelineReflection reflection_data;
    id<MTLRenderPipelineState> pso = [ctx->device
        newRenderPipelineStateWithDescriptor:desc
                                     options:MTLPipelineOptionBufferTypeInfo
                                  reflection:&reflection_data
                                       error:&error];
    if (error) {
      NSLog(@"Failed to create PSO for shader: %s error %@\n", this->name, error);
      BLI_assert(false);
      return nullptr;
    }
    else if (!pso) {
      NSLog(@"Failed to create PSO for shader: %s, but no error was provided!\n", this->name);
      BLI_assert(false);
      return nullptr;
    }
    else {
      NSLog(@"Successfully compiled PSO for shader: %s (Metal Context: %p)\n", this->name, ctx);
    }

    /* Prepare pipeline state instance. */
    MTLRenderPipelineStateInstance *pso_inst = new MTLRenderPipelineStateInstance();
    pso_inst->vert = desc.vertexFunction;
    pso_inst->frag = desc.fragmentFunction;
    pso_inst->pso = pso;
    pso_inst->base_uniform_buffer_index = MTL_uniform_buffer_base_index;
    pso_inst->null_attribute_buffer_index = (using_null_buffer) ? null_buffer_index : -1;
    pso_inst->transform_feedback_buffer_index = MTL_transform_feedback_buffer_index;
    pso_inst->shader_pso_index = pso_cache_.size();

    pso_inst->reflection_data_available = (reflection_data != nil);
    if (reflection_data != nil) {

      /* Extract shader reflection data for buffer bindings.
       * This reflection data is used to contrast the binding information
       * we know about in the interface against the bindings in the finalized
       * PSO. This accounts for bindings which have been stripped out during
       * optimization, and allows us to both avoid over-binding and also
       * allows us to verify size-correctness for bindings, to ensure
       * that buffers bound are not smaller than the size of expected data. */
      NSArray<MTLArgument *> *vert_args = [reflection_data vertexArguments];

      pso_inst->buffer_bindings_reflection_data_vert.clear();
      int buffer_binding_max_ind = 0;

      for (int i = 0; i < [vert_args count]; i++) {
        MTLArgument *arg = [vert_args objectAtIndex:i];
        if ([arg type] == MTLArgumentTypeBuffer) {
          int buf_index = [arg index] - MTL_uniform_buffer_base_index;
          if (buf_index >= 0) {
            buffer_binding_max_ind = max_ii(buffer_binding_max_ind, buf_index);
          }
        }
      }
      pso_inst->buffer_bindings_reflection_data_vert.resize(buffer_binding_max_ind + 1);
      for (int i = 0; i < buffer_binding_max_ind + 1; i++) {
        pso_inst->buffer_bindings_reflection_data_vert[i] = {0, 0, 0, false};
      }

      for (int i = 0; i < [vert_args count]; i++) {
        MTLArgument *arg = [vert_args objectAtIndex:i];
        if ([arg type] == MTLArgumentTypeBuffer) {
          int buf_index = [arg index] - MTL_uniform_buffer_base_index;

          if (buf_index >= 0) {
            pso_inst->buffer_bindings_reflection_data_vert[buf_index] = {
                (uint32_t)([arg index]),
                (uint32_t)([arg bufferDataSize]),
                (uint32_t)([arg bufferAlignment]),
                ([arg isActive] == YES) ? true : false};
          }
        }
      }

      NSArray<MTLArgument *> *frag_args = [reflection_data fragmentArguments];

      pso_inst->buffer_bindings_reflection_data_frag.clear();
      buffer_binding_max_ind = 0;

      for (int i = 0; i < [frag_args count]; i++) {
        MTLArgument *arg = [frag_args objectAtIndex:i];
        if ([arg type] == MTLArgumentTypeBuffer) {
          int buf_index = [arg index] - MTL_uniform_buffer_base_index;
          if (buf_index >= 0) {
            buffer_binding_max_ind = max_ii(buffer_binding_max_ind, buf_index);
          }
        }
      }
      pso_inst->buffer_bindings_reflection_data_frag.resize(buffer_binding_max_ind + 1);
      for (int i = 0; i < buffer_binding_max_ind + 1; i++) {
        pso_inst->buffer_bindings_reflection_data_frag[i] = {0, 0, 0, false};
      }

      for (int i = 0; i < [frag_args count]; i++) {
        MTLArgument *arg = [frag_args objectAtIndex:i];
        if ([arg type] == MTLArgumentTypeBuffer) {
          int buf_index = [arg index] - MTL_uniform_buffer_base_index;
          shader_debug_printf(" BUF IND: %d (arg name: %s)\n", buf_index, [[arg name] UTF8String]);
          if (buf_index >= 0) {
            pso_inst->buffer_bindings_reflection_data_frag[buf_index] = {
                (uint32_t)([arg index]),
                (uint32_t)([arg bufferDataSize]),
                (uint32_t)([arg bufferAlignment]),
                ([arg isActive] == YES) ? true : false};
          }
        }
      }
    }

    [pso_inst->vert retain];
    [pso_inst->frag retain];
    [pso_inst->pso retain];

    /* Insert into pso cache. */
    pso_cache_.add(pipeline_descriptor, pso_inst);
    shader_debug_printf("PSO CACHE: Stored new variant in PSO cache for shader '%s'\n",
                        this->name);
    return pso_inst;
  }
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name SSBO-vertex-fetch-mode attribute control.
 * \{ */

int MTLShader::ssbo_vertex_type_to_attr_type(MTLVertexFormat attribute_type)
{
  switch (attribute_type) {
    case MTLVertexFormatFloat:
      return GPU_SHADER_ATTR_TYPE_FLOAT;
    case MTLVertexFormatInt:
      return GPU_SHADER_ATTR_TYPE_INT;
    case MTLVertexFormatUInt:
      return GPU_SHADER_ATTR_TYPE_UINT;
    case MTLVertexFormatShort:
      return GPU_SHADER_ATTR_TYPE_SHORT;
    case MTLVertexFormatUChar:
      return GPU_SHADER_ATTR_TYPE_CHAR;
    case MTLVertexFormatUChar2:
      return GPU_SHADER_ATTR_TYPE_CHAR2;
    case MTLVertexFormatUChar3:
      return GPU_SHADER_ATTR_TYPE_CHAR3;
    case MTLVertexFormatUChar4:
      return GPU_SHADER_ATTR_TYPE_CHAR4;
    case MTLVertexFormatFloat2:
      return GPU_SHADER_ATTR_TYPE_VEC2;
    case MTLVertexFormatFloat3:
      return GPU_SHADER_ATTR_TYPE_VEC3;
    case MTLVertexFormatFloat4:
      return GPU_SHADER_ATTR_TYPE_VEC4;
    case MTLVertexFormatUInt2:
      return GPU_SHADER_ATTR_TYPE_UVEC2;
    case MTLVertexFormatUInt3:
      return GPU_SHADER_ATTR_TYPE_UVEC3;
    case MTLVertexFormatUInt4:
      return GPU_SHADER_ATTR_TYPE_UVEC4;
    case MTLVertexFormatInt2:
      return GPU_SHADER_ATTR_TYPE_IVEC2;
    case MTLVertexFormatInt3:
      return GPU_SHADER_ATTR_TYPE_IVEC3;
    case MTLVertexFormatInt4:
      return GPU_SHADER_ATTR_TYPE_IVEC4;
    case MTLVertexFormatUCharNormalized:
      return GPU_SHADER_ATTR_TYPE_UCHAR_NORM;
    case MTLVertexFormatUChar2Normalized:
      return GPU_SHADER_ATTR_TYPE_UCHAR2_NORM;
    case MTLVertexFormatUChar3Normalized:
      return GPU_SHADER_ATTR_TYPE_UCHAR3_NORM;
    case MTLVertexFormatUChar4Normalized:
      return GPU_SHADER_ATTR_TYPE_UCHAR4_NORM;
    case MTLVertexFormatInt1010102Normalized:
      return GPU_SHADER_ATTR_TYPE_INT1010102_NORM;
    case MTLVertexFormatShort3Normalized:
      return GPU_SHADER_ATTR_TYPE_SHORT3_NORM;
    default:
      BLI_assert_msg(false,
                     "Not yet supported attribute type for SSBO vertex fetch -- Add entry "
                     "GPU_SHADER_ATTR_TYPE_** to shader defines, and in this table");
      return -1;
  }
  return -1;
}

void MTLShader::ssbo_vertex_fetch_bind_attributes_begin()
{
  MTLShaderInterface *mtl_interface = this->get_interface();
  ssbo_vertex_attribute_bind_active_ = true;
  ssbo_vertex_attribute_bind_mask_ = (1 << mtl_interface->get_total_attributes()) - 1;

  /* Reset tracking of actively used VBO bind slots for SSBO vertex fetch mode. */
  for (int i = 0; i < MTL_SSBO_VERTEX_FETCH_MAX_VBOS; i++) {
    ssbo_vbo_slot_used_[i] = false;
  }
}

void MTLShader::ssbo_vertex_fetch_bind_attribute(const MTLSSBOAttribute &ssbo_attr)
{
  /* Fetch attribute. */
  MTLShaderInterface *mtl_interface = this->get_interface();
  BLI_assert(ssbo_attr.mtl_attribute_index >= 0 &&
             ssbo_attr.mtl_attribute_index < mtl_interface->get_total_attributes());
  UNUSED_VARS_NDEBUG(mtl_interface);

  /* Update bind-mask to verify this attribute has been used. */
  BLI_assert((ssbo_vertex_attribute_bind_mask_ & (1 << ssbo_attr.mtl_attribute_index)) ==
                 (1 << ssbo_attr.mtl_attribute_index) &&
             "Attribute has already been bound");
  ssbo_vertex_attribute_bind_mask_ &= ~(1 << ssbo_attr.mtl_attribute_index);

  /* Fetch attribute uniform addresses from cache. */
  ShaderSSBOAttributeBinding &cached_ssbo_attribute =
      cached_ssbo_attribute_bindings_[ssbo_attr.mtl_attribute_index];
  BLI_assert(cached_ssbo_attribute.attribute_index >= 0);

  /* Write attribute descriptor properties to shader uniforms. */
  this->uniform_int(cached_ssbo_attribute.uniform_offset, 1, 1, &ssbo_attr.attribute_offset);
  this->uniform_int(cached_ssbo_attribute.uniform_stride, 1, 1, &ssbo_attr.per_vertex_stride);
  int inst_val = (ssbo_attr.is_instance ? 1 : 0);
  this->uniform_int(cached_ssbo_attribute.uniform_fetchmode, 1, 1, &inst_val);
  this->uniform_int(cached_ssbo_attribute.uniform_vbo_id, 1, 1, &ssbo_attr.vbo_id);
  BLI_assert(ssbo_attr.attribute_format >= 0);
  this->uniform_int(cached_ssbo_attribute.uniform_attr_type, 1, 1, &ssbo_attr.attribute_format);
  ssbo_vbo_slot_used_[ssbo_attr.vbo_id] = true;
}

void MTLShader::ssbo_vertex_fetch_bind_attributes_end(id<MTLRenderCommandEncoder> active_encoder)
{
  ssbo_vertex_attribute_bind_active_ = false;

  /* If our mask is non-zero, we have unassigned attributes. */
  if (ssbo_vertex_attribute_bind_mask_ != 0) {
    MTLShaderInterface *mtl_interface = this->get_interface();

    /* Determine if there is a free slot we can bind the null buffer to -- We should have at
     * least ONE free slot in this instance. */
    int null_attr_buffer_slot = -1;
    for (int i = 0; i < MTL_SSBO_VERTEX_FETCH_MAX_VBOS; i++) {
      if (!ssbo_vbo_slot_used_[i]) {
        null_attr_buffer_slot = i;
        break;
      }
    }
    BLI_assert_msg(null_attr_buffer_slot >= 0,
                   "No suitable bind location for a NULL buffer was found");

    for (int i = 0; i < mtl_interface->get_total_attributes(); i++) {
      if (ssbo_vertex_attribute_bind_mask_ & (1 << i)) {
        const MTLShaderInputAttribute *mtl_shader_attribute = &mtl_interface->get_attribute(i);
#if MTL_DEBUG_SHADER_ATTRIBUTES == 1
        MTL_LOG_WARNING(
            "SSBO Vertex Fetch missing attribute with index: %d. Shader: %s, Attr "
            "Name: "
            "%s - Null buffer bound\n",
            i,
            this->name_get(),
            mtl_shader_attribute->name);
#endif
        /* Bind Attribute with NULL buffer index and stride zero (for constant access). */
        MTLSSBOAttribute ssbo_attr(
            i, null_attr_buffer_slot, 0, 0, GPU_SHADER_ATTR_TYPE_FLOAT, false);
        ssbo_vertex_fetch_bind_attribute(ssbo_attr);
        MTL_LOG_WARNING(
            "Unassigned Shader attribute: %s, Attr Name: %s -- Binding NULL BUFFER to "
            "slot %d\n",
            this->name_get(),
            mtl_interface->get_name_at_offset(mtl_shader_attribute->name_offset),
            null_attr_buffer_slot);
      }
    }

    /* Bind NULL buffer to given VBO slot. */
    MTLContext *ctx = reinterpret_cast<MTLContext *>(GPU_context_active_get());
    id<MTLBuffer> null_buf = ctx->get_null_attribute_buffer();
    BLI_assert(null_buf);

    MTLRenderPassState &rps = ctx->main_command_buffer.get_render_pass_state();
    rps.bind_vertex_buffer(null_buf, 0, null_attr_buffer_slot);
  }
}

GPUVertBuf *MTLShader::get_transform_feedback_active_buffer()
{
  if (transform_feedback_type_ == GPU_SHADER_TFB_NONE || !transform_feedback_active_) {
    return nullptr;
  }
  return transform_feedback_vertbuf_;
}

bool MTLShader::has_transform_feedback_varying(std::string str)
{
  if (this->transform_feedback_type_ == GPU_SHADER_TFB_NONE) {
    return false;
  }

  return (std::find(tf_output_name_list_.begin(), tf_output_name_list_.end(), str) !=
          tf_output_name_list_.end());
}

}  // blender::gpu::shdaer
