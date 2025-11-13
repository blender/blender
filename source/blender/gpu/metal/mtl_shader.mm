/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.hh"

#include "DNA_userdef_types.h"

#include "BLI_string.h"
#include "BLI_time.h"

#include <algorithm>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>

#include <cstring>

#include "GPU_platform.hh"
#include "GPU_vertex_format.hh"

#include "gpu_shader_dependency_private.hh"
#include "mtl_common.hh"
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_pso_descriptor_state.hh"
#include "mtl_shader.hh"
#include "mtl_shader_generate.hh"
#include "mtl_shader_interface.hh"
#include "mtl_shader_log.hh"
#include "mtl_texture.hh"
#include "mtl_vertex_buffer.hh"

#include "GHOST_C-api.h"

using namespace blender;
using namespace blender::gpu;
using namespace blender::gpu::shader;

namespace blender::gpu {

const char *to_string(ShaderStage stage)
{
  switch (stage) {
    case ShaderStage::VERTEX:
      return "Vertex Shader";
    case ShaderStage::FRAGMENT:
      return "Fragment Shader";
    case ShaderStage::COMPUTE:
      return "Compute Shader";
    case ShaderStage::ANY:
      break;
  }
  return "Unknown Shader Stage";
}

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction.
 * \{ */

/* Create empty shader to be populated later. */
MTLShader::MTLShader(MTLContext *ctx, const char *name) : Shader(name)
{
  context_ = ctx;
}

MTLShader::~MTLShader()
{
  if (this->is_valid()) {
    /* Free uniform data block. */
    MEM_SAFE_DELETE(push_constant_buf_);

    /* Free Metal resources.
     * This is done in the order of:
     * 1. PipelineState objects
     * 2. MTLFunctions
     * 3. MTLLibraries
     * So that each object releases it's references to the one following it. */
    if (pso_descriptor_ != nil) {
      [pso_descriptor_ release];
      pso_descriptor_ = nil;
    }

    /* Free Pipeline Cache. */
    pso_cache_lock_.lock();
    for (const MTLRenderPipelineStateInstance *pso_inst : pso_cache_.values()) {
      /* Free pipeline state object. */
      if (pso_inst->pso) {
        [pso_inst->pso release];
      }
      /* Free vertex function. */
      if (pso_inst->vert) {
        [pso_inst->vert release];
      }
      /* Free fragment function. */
      if (pso_inst->frag) {
        [pso_inst->frag release];
      }
      delete pso_inst;
    }
    pso_cache_.clear();

    /* Free Compute pipeline cache. */
    for (const MTLComputePipelineStateInstance *pso_inst : compute_pso_cache_.values()) {
      /* Free pipeline state object. */
      if (pso_inst->pso) {
        [pso_inst->pso release];
      }
      /* Free compute function. */
      if (pso_inst->compute) {
        [pso_inst->compute release];
      }
    }
    compute_pso_cache_.clear();
    pso_cache_lock_.unlock();

    /* Free shader libraries. */
    if (shader_library_vert_ != nil) {
      [shader_library_vert_ release];
      shader_library_vert_ = nil;
    }
    if (shader_library_frag_ != nil) {
      [shader_library_frag_ release];
      shader_library_frag_ = nil;
    }
    if (shader_library_comp_ != nil) {
      [shader_library_comp_ release];
      shader_library_comp_ = nil;
    }

    /* NOTE(Metal): #ShaderInterface deletion is handled in the super destructor `~Shader()`. */
  }
  valid_ = false;
}

void MTLShader::init(const shader::ShaderCreateInfo & /*info*/, bool is_batch_compilation)
{
  async_compilation_ = is_batch_compilation;
}

const shader::ShaderCreateInfo &MTLShader::patch_create_info(
    const shader::ShaderCreateInfo &original_info)
{
  if (!MTLBackend::get_capabilities().supports_texture_atomics) {
    /* This function can lazily create the patched_info_. */
    patch_create_info_atomic_workaround(patched_info_, original_info);
  }

  if (original_info.max_sampler_slot() > 16) {
    if (patched_info_ == nullptr) {
      patched_info_ = std::make_unique<PatchedShaderCreateInfo>(original_info);
    }
    patched_info_->info.builtins_ |= BuiltinBits::USE_SAMPLER_ARG_BUFFER;
  }

  return patched_info_ != nullptr ? patched_info_->info : original_info;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader stage creation.
 * \{ */

std::string MTLShader::entry_point_name_get(const ShaderStage stage)
{
  std::string name = this->name_get();
  /* Escape the shader name to be able to use it inside an identifier. */
  for (char &c : name) {
    if (!std::isalnum(c)) {
      c = '_';
    }
  }

  switch (stage) {
    case ShaderStage::VERTEX:
      return "_" + name + "_vert";
    case ShaderStage::FRAGMENT:
      return "_" + name + "_frag";
    case ShaderStage::COMPUTE:
      return "_" + name + "_comp";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

/* Note: returns a retained object. */
static ::MTLCompileOptions *get_compile_options(const bool use_subpass_input,
                                                const bool use_texture_atomic)
{
  ::MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
  options.languageVersion = MTLLanguageVersion2_2;
  options.fastMathEnabled = YES;
  /* TODO(fclem): Only use this on shaders that matters. */
  options.preserveInvariance = YES;
  /* Raster order groups for tile data in struct require Metal 2.3.
   * Retaining Metal 2.2. for old shaders to maintain backwards
   * compatibility for existing features. */
  if (use_subpass_input) {
    options.languageVersion = MTLLanguageVersion2_3;
  }
#if defined(MAC_OS_VERSION_14_0)
  if (@available(macOS 14.00, *)) {
    /* Texture atomics require Metal 3.1. */
    if (use_texture_atomic) {
      options.languageVersion = MTLLanguageVersion3_1;
    }
  }
#endif
  return options;
}

id<MTLLibrary> MTLShader::create_shader_library(const shader::ShaderCreateInfo &info,
                                                const ShaderStage stage,
                                                MutableSpan<StringRefNull> sources)
{
  std::pair<std::string, std::string> wrapper = generate_entry_point(
      info, stage, entry_point_name_get(stage));

  std::string shader_compat;
  {
    std::stringstream ss;
    ss << "#define MTL_WORKGROUP_SIZE_X " << info.compute_layout_.local_size_x << "\n";
    ss << "#define MTL_WORKGROUP_SIZE_Y " << info.compute_layout_.local_size_y << "\n";
    ss << "#define MTL_WORKGROUP_SIZE_Z " << info.compute_layout_.local_size_z << "\n";
    if (flag_is_set(info.builtins_, BuiltinBits::USE_SAMPLER_ARG_BUFFER)) {
      ss << "#define MTL_USE_SAMPLER_ARGUMENT_BUFFER\n";
    }

    if (flag_is_set(info.builtins_, BuiltinBits::TEXTURE_ATOMIC) &&
        MTLBackend::get_capabilities().supports_texture_atomics)
    {
      ss << "#define MTL_SUPPORTS_TEXTURE_ATOMICS 1\n";
    }

    shader::GeneratedSource defines_src{"gpu_shader_msl_defines.msl", {}, ss.str()};
    shader::GeneratedSource wrapper_src{
        "gpu_shader_msl_wrapper.msl", {"gpu_shader_msl_types.msl"}, wrapper.first};
    shader::GeneratedSourceList generated_sources{defines_src, wrapper_src};

    /* Concatenate common source. */
    Vector<StringRefNull> compatibility_src = gpu_shader_dependency_get_resolved_source(
        "gpu_shader_compat_msl.msl", generated_sources);
    shader_compat = fmt::to_string(fmt::join(compatibility_src, ""));
  }

  sources[SOURCES_INDEX_VERSION] = shader_compat;

  std::string concat_source = fmt::to_string(fmt::join(sources, "")) + wrapper.second;

  if (this->name_get() == G.gpu_debug_shader_source_name) {
    NSFileManager *sharedFM = [NSFileManager defaultManager];
    NSURL *app_bundle_url = [[NSBundle mainBundle] bundleURL];
    NSURL *shader_dir = [[app_bundle_url URLByDeletingLastPathComponent]
        URLByAppendingPathComponent:@"Shaders/"
                        isDirectory:YES];

    [sharedFM createDirectoryAtURL:shader_dir
        withIntermediateDirectories:YES
                         attributes:nil
                              error:nil];

    const char *path_cstr = [shader_dir fileSystemRepresentation];

    std::ofstream output_source_file(std::string(path_cstr) + "/" +
                                     this->entry_point_name_get(stage) + ".msl");
    output_source_file << concat_source;
    output_source_file.close();
  }

  {
    ::MTLCompileOptions *options = get_compile_options(
        !info.subpass_inputs_.is_empty(), bool(info.builtins_ & BuiltinBits::TEXTURE_ATOMIC));

    NSError *error = nullptr;
    id<MTLLibrary> library = [context_->device
        newLibraryWithSource:[NSString stringWithUTF8String:concat_source.c_str()]
                     options:options
                       error:&error];
    library.label = [NSString stringWithUTF8String:this->name];

    [options release];

    if (error == nullptr) {
      return library;
    }

    NSString *error_localized = [error localizedDescription];

    /* Only fail if genuine error and not warning. */
    if ([error_localized rangeOfString:@"Compilation succeeded"].length > 0) {
      /* TODO(fclem): Add option to display warning. */
      return library;
    }

    [library release];

    MTLLogParser parser;
    print_log({concat_source}, [error_localized UTF8String], to_string(stage), true, &parser);
  }
  return nil;
}

void MTLShader::vertex_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                        MutableSpan<StringRefNull> sources)
{
  shader_library_vert_ = create_shader_library(info, ShaderStage::VERTEX, sources);
}

void MTLShader::geometry_shader_from_glsl(const shader::ShaderCreateInfo & /*info*/,
                                          MutableSpan<StringRefNull> /*sources*/)
{
  MTL_LOG_ERROR("MTLShader::geometry_shader_from_glsl - Geometry shaders unsupported!");
}

void MTLShader::fragment_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                          MutableSpan<StringRefNull> sources)
{
  shader_library_frag_ = create_shader_library(info, ShaderStage::FRAGMENT, sources);
}

void MTLShader::compute_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                         MutableSpan<StringRefNull> sources)
{
  shader_library_comp_ = create_shader_library(info, ShaderStage::COMPUTE, sources);
}

bool MTLShader::finalize(const shader::ShaderCreateInfo *info)
{
  /* Check if Shader has already been finalized. */
  if (this->is_valid()) {
    MTL_LOG_ERROR("Shader (%p) '%s' has already been finalized!", this, this->name_get().c_str());
    return false;
  }

  if (this->shader_library_frag_ == nil && this->shader_library_frag_ == nil &&
      this->shader_library_comp_ == nil)
  {
    /* All compilations failed. */
    return false;
  }

  const bool is_compute = (this->shader_library_frag_ == nil && this->shader_library_frag_ == nil);

  if (!is_compute && (this->shader_library_frag_ == nil || this->shader_library_frag_ == nil)) {
    /* One stage failed to compile. */
    return false;
  }

  /* Prepare backing data storage for local uniforms. */
  if (info->push_constants_.is_empty() == false) {
    push_constant_buf_ = MEM_new<MTLPushConstantBuf>("MTLPushConstantBuf", *info);
  }

  /* Ensure we have a valid shader interface. */
  this->interface = new MTLShaderInterface(this->name, *info, push_constant_buf_);

  /* Shader has successfully been created. */
  valid_ = true;

  if (is_compute) {
    /* If this is a compute shader, bake base PSO for compute straight-away.
     * NOTE: This will compile the base unspecialized variant. */

    this->compute_pso_common_state_.set_compute_workgroup_size(info->compute_layout_.local_size_x,
                                                               info->compute_layout_.local_size_y,
                                                               info->compute_layout_.local_size_z);

    /* Set descriptor to default shader constants */
    MTLComputePipelineStateDescriptor compute_pipeline_descriptor(this->constants->values);

    this->bake_compute_pipeline_state(context_, compute_pipeline_descriptor);
  }
  else {
    /* Prepare Render pipeline descriptor. */
    pso_descriptor_ = [[MTLRenderPipelineDescriptor alloc] init];
    pso_descriptor_.label = [NSString stringWithUTF8String:this->name];
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader Binding.
 * \{ */

void MTLShader::bind(const shader::SpecializationConstants *constants_state)
{
  MTLContext *ctx = MTLContext::get();
  /* Copy constants state. */
  ctx->specialization_constants_set(constants_state);

  if (interface == nullptr || !this->is_valid()) {
    MTL_LOG_WARNING(
        "MTLShader::bind - Shader '%s' has no valid implementation in Metal, draw calls will be "
        "skipped.",
        this->name_get().c_str());
  }
  ctx->pipeline_state.active_shader = this;
}

void MTLShader::unbind()
{
  MTLContext *ctx = MTLContext::get();
  ctx->pipeline_state.active_shader = nullptr;
}

void MTLShader::uniform_float(int location, int comp_len, int array_size, const float *data)
{
  BLI_assert(this);
  if (!this->is_valid()) {
    return;
  }

  if (push_constant_buf_ == nullptr || location < 0 || location >= push_constant_buf_->size()) {
    MTL_LOG_WARNING("Shader %s: Invalid uniform location %d", this->name, location);
    return;
  }

  if (comp_len == 9) {
    /* Convert float3x3 case into float3[3] case. They have the same alignment. */
    comp_len = 3;
    array_size *= 3;
  }
  else if (comp_len == 16) {
    /* Convert float4x4 case into float4[4] case. They have the same alignment. */
    comp_len = 4;
    array_size *= 4;
  }
  else {
    /* Input and output are both packed. Convert to a 1 iteration assignment below. */
    comp_len *= array_size;
    array_size = 1;
  }

  /* It is more efficient on the host to only modify data if it has changed.
   * Data modifications are small, so memory comparison is cheap.
   * If uniforms have remained unchanged, then we avoid both copying
   * data into the local uniform struct, and upload of the modified uniform
   * contents in the command stream. */
  bool update = false;

  /* float3 is 16 bytes on Metal. This is the only case where we need this iteration. */
  constexpr int size_padded = 16;
  const size_t data_size = comp_len * sizeof(float);
  for (int i : IndexRange(array_size)) {
    const void *src = data + i * comp_len;
    void *dst = push_constant_buf_->data() + (location + i * size_padded);
    if (update || memcmp(dst, src, data_size) != 0) {
      memcpy(dst, src, data_size);
      update = true;
    }
  }

  if (update) {
    push_constant_buf_->tag_dirty();
  }
}

void MTLShader::uniform_int(int location, int comp_len, int array_size, const int *data)
{
  static_assert(sizeof(int) == sizeof(float), "int to float reinterpret expect matching size");
  uniform_float(location, comp_len, array_size, reinterpret_cast<const float *>(data));
}

/* Attempts to pre-generate a PSO based on the parent shaders PSO
 * (Material shaders only) */
void MTLShader::warm_cache(int limit)
{
  if (parent_shader_ != nullptr) {
    MTLContext *ctx = MTLContext::get();
    MTLShader *parent_mtl = static_cast<MTLShader *>(parent_shader_);

    /* Extract PSO descriptors from parent shader. */
    blender::Vector<MTLRenderPipelineStateDescriptor> descriptors;
    blender::Vector<MTLPrimitiveTopologyClass> prim_classes;

    parent_mtl->pso_cache_lock_.lock();
    for (const auto &pso_entry : parent_mtl->pso_cache_.items()) {
      const MTLRenderPipelineStateDescriptor &pso_descriptor = pso_entry.key;
      const MTLRenderPipelineStateInstance *pso_inst = pso_entry.value;
      descriptors.append(pso_descriptor);
      prim_classes.append(pso_inst->prim_type);
    }
    parent_mtl->pso_cache_lock_.unlock();

    /* Warm shader cache with applied limit.
     * If limit is <= 0, compile all PSO permutations. */
    limit = (limit > 0) ? limit : descriptors.size();
    for (int i : IndexRange(min_ii(descriptors.size(), limit))) {
      const MTLRenderPipelineStateDescriptor &pso_descriptor = descriptors[i];
      const MTLPrimitiveTopologyClass &prim_class = prim_classes[i];
      bake_graphic_pipeline_state(ctx, prim_class, pso_descriptor);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader specialization common utilities.
 *
 * \{ */

/**
 * Populates `values` with the given `SpecializationStateDescriptor` values.
 * Setup function specialization constants, used to modify and optimize
 * generated code based on current render pipeline configuration.
 * Note: Returns a retained object.
 */
static ::MTLFunctionConstantValues *populate_specialization_constant_values(
    const shader::SpecializationConstants &shader_constants,
    const SpecializationStateDescriptor &specialization_descriptor)
{
  ::MTLFunctionConstantValues *values = [MTLFunctionConstantValues new];

  for (auto i : shader_constants.types.index_range()) {
    const shader::SpecializationConstant::Value &value = specialization_descriptor.values[i];

    uint index = i + MTL_SPECIALIZATION_CONSTANT_OFFSET;
    switch (shader_constants.types[i]) {
      case Type::int_t:
        [values setConstantValue:&value.i type:MTLDataTypeInt atIndex:index];
        break;
      case Type::uint_t:
        [values setConstantValue:&value.u type:MTLDataTypeUInt atIndex:index];
        break;
      case Type::bool_t:
        [values setConstantValue:&value.u type:MTLDataTypeBool atIndex:index];
        break;
      case Type::float_t:
        [values setConstantValue:&value.f type:MTLDataTypeFloat atIndex:index];
        break;
      default:
        BLI_assert_msg(false, "Unsupported custom constant type.");
        break;
    }
  }
  return values;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Bake Pipeline State Objects
 * \{ */

static uint32_t get_buffers_binding_mask(NSArray<MTLArgument *> *args)
{
  uint32_t mask = 0u;
  for (int i = 0; i < [args count]; i++) {
    MTLArgument *arg = [args objectAtIndex:i];
    if ([arg type] == MTLArgumentTypeBuffer && [arg isActive] == TRUE) {
      int index = [arg index];
      if (index >= 0 && index < MTL_MAX_BUFFER_BINDINGS) {
        mask |= (1 << index);
      }
    }
  }
  return mask;
}

static uint16_t get_images_binding_mask(NSArray<MTLArgument *> *args)
{
  uint16_t mask = 0u;
  for (int i = 0; i < [args count]; i++) {
    MTLArgument *arg = [args objectAtIndex:i];
    if ([arg type] == MTLArgumentTypeTexture && [arg isActive] == TRUE) {
      int index = [arg index];
      if (index >= 0 && index < MTL_MAX_IMAGE_SLOTS) {
        mask |= (1 << index);
      }
    }
  }
  return mask;
}

static uint64_t get_samplers_binding_mask(NSArray<MTLArgument *> *args)
{
  uint64_t mask = 0u;
  for (int i = 0; i < [args count]; i++) {
    MTLArgument *arg = [args objectAtIndex:i];
    if ([arg type] == MTLArgumentTypeTexture && [arg isActive] == TRUE) {
      int index = [arg index];
      if (index >= MTL_MAX_IMAGE_SLOTS && index < MTL_MAX_TEXTURE_SLOTS) {
        mask |= (1 << (index - MTL_MAX_IMAGE_SLOTS));
      }
    }
  }
  return mask;
}

void MTLRenderPipelineStateInstance::parse_reflection_data(
    MTLRenderPipelineReflection *reflection_data)
{
  /* Extract shader reflection data for buffer bindings.
   * This reflection data is used to contrast the binding information
   * we know about in the interface against the bindings in the finalized
   * PSO. This accounts for bindings which have been stripped out during
   * optimization, and allows us to both avoid over-binding and also
   * allows us to verify size-correctness for bindings, to ensure
   * that buffers bound are not smaller than the size of expected data. */
  this->used_buf_vert_mask = get_buffers_binding_mask([reflection_data vertexArguments]);
  this->used_buf_frag_mask = get_buffers_binding_mask([reflection_data fragmentArguments]);

  this->used_ima_vert_mask = get_images_binding_mask([reflection_data vertexArguments]);
  this->used_ima_frag_mask = get_images_binding_mask([reflection_data fragmentArguments]);

  this->used_tex_vert_mask = get_samplers_binding_mask([reflection_data vertexArguments]);
  this->used_tex_frag_mask = get_samplers_binding_mask([reflection_data fragmentArguments]);
}

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
  /** Populate global pipeline descriptor and use this to prepare new PSO. */
  /* NOTE(Metal): PSO cache can be accessed from multiple threads, though these operations should
   * be thread-safe due to organization of high-level renderer. If there are any issues, then
   * access can be guarded as appropriate. */
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
  MTLShaderInterface &interface = this->get_interface();
  /* Primitive Type -- Primitive topology class needs to be specified for layered rendering. */
  bool requires_specific_topology_class = interface.use_layer() ||
                                          interface.use_viewport_index() ||
                                          prim_type == MTLPrimitiveTopologyClassPoint;
  pipeline_descriptor.vertex_descriptor.prim_topology_class =
      (requires_specific_topology_class) ? prim_type : MTLPrimitiveTopologyClassUnspecified;

  /* Specialization configuration. */
  pipeline_descriptor.specialization_state = {ctx->constants_state.values};

  /* Bake pipeline state using global descriptor. */
  return bake_graphic_pipeline_state(ctx, prim_type, pipeline_descriptor);
}

/* Variant which bakes a pipeline state based on an existing MTLRenderPipelineStateDescriptor.
 * This function should be callable from a secondary compilation thread. */
MTLRenderPipelineStateInstance *MTLShader::bake_graphic_pipeline_state(
    MTLContext *ctx,
    MTLPrimitiveTopologyClass prim_type,
    const MTLRenderPipelineStateDescriptor &pipeline_descriptor)
{
  BLI_assert(this->is_valid());

  /* Check if current PSO exists in the cache. */
  pso_cache_lock_.lock();
  MTLRenderPipelineStateInstance **pso_lookup = pso_cache_.lookup_ptr(pipeline_descriptor);
  MTLRenderPipelineStateInstance *pipeline_state = (pso_lookup) ? *pso_lookup : nullptr;
  pso_cache_lock_.unlock();

  if (pipeline_state != nullptr) {
    return pipeline_state;
  }

  /* TODO: When fetching a specialized variant of a shader, if this does not yet exist, verify
   * whether the base unspecialized variant exists:
   * - If unspecialized version exists: Compile specialized PSO asynchronously, returning base PSO
   * and flagging state of specialization in cache as being built.
   * - If unspecialized does NOT exist, build specialized version straight away, as we pay the
   * cost of compilation in both cases regardless. */

  /* Prepare Render Pipeline Descriptor. */

  ::MTLFunctionConstantValues *values = populate_specialization_constant_values(
      *this->constants, pipeline_descriptor.specialization_state);

  /* Prepare Vertex descriptor based on current pipeline vertex binding state. */
  ::MTLRenderPipelineDescriptor *desc = pso_descriptor_;
  [desc reset];
  desc.label = [NSString stringWithUTF8String:this->name];

  const MTLVertexDescriptor &gpu_vert_desc = pipeline_descriptor.vertex_descriptor;
  ::MTLVertexDescriptor *mtl_vert_desc = desc.vertexDescriptor;

  uint32_t vbo_bind_mask = 0;

  for (const uint i : IndexRange(gpu_vert_desc.max_attribute_value + 1)) {
    const MTLVertexAttributeDescriptorPSO &attribute_desc = gpu_vert_desc.attributes[i];
    /* Copy metal back-end attribute descriptor state into PSO descriptor.
     * NOTE: need to copy each element due to direct assignment restrictions.
     * Also note */
    ::MTLVertexAttributeDescriptor *mtl_attribute = mtl_vert_desc.attributes[i];
    mtl_attribute.format = attribute_desc.format;
    mtl_attribute.offset = attribute_desc.offset;
    mtl_attribute.bufferIndex = attribute_desc.buffer_index;
  }

  for (const uint i : IndexRange(gpu_vert_desc.num_vert_buffers)) {
    const MTLVertexBufferLayoutDescriptorPSO &buf_layout = gpu_vert_desc.buffer_layouts[i];
    /* Copy metal back-end buffer layout state into PSO descriptor.
     * NOTE: need to copy each element due to copying from internal
     * back-end descriptor to Metal API descriptor. */
    MTLVertexBufferLayoutDescriptor *mtl_layout = mtl_vert_desc.layouts[buf_layout.buffer_slot];
    mtl_layout.stepFunction = buf_layout.step_function;
    mtl_layout.stepRate = buf_layout.step_rate;
    mtl_layout.stride = buf_layout.stride;
    vbo_bind_mask |= 1 << buf_layout.buffer_slot;
  }

  /* Null buffer index is used if an attribute is not found in the
   * bound VBOs #VertexFormat. */
  /* WATCH: Hope that it doesn't conflict with and existing buffer. */
  const int null_buffer_index = 31 - bitscan_reverse_uint(get_interface().vertex_buffer_mask());
  bool using_null_buffer = false;

  for (const ShaderInput &attr : Span<ShaderInput>(interface->inputs_, interface->attr_len_)) {
    ::MTLVertexAttributeDescriptor *mtl_attribute = mtl_vert_desc.attributes[attr.binding];
    if (mtl_attribute.format == MTLVertexFormatInvalid) {
      /* An attribute should be bound there but no buffer was provided.
       * Mimic OpenGL behavior by binding a dummy buffer with 0 stride. */
      shader::Type input_type = shader::Type(get_interface().attr_types_[attr.binding]);
      mtl_attribute.format = gpu_type_to_metal_vertex_format(input_type);
      mtl_attribute.offset = 0;
      mtl_attribute.bufferIndex = null_buffer_index;
      using_null_buffer = true;
    }
  }

  if (using_null_buffer) {
    MTLVertexBufferLayoutDescriptor *mtl_layout = mtl_vert_desc.layouts[null_buffer_index];
    /* Use constant step function such that null buffer can contain just a singular dummy
     * attribute. */
    mtl_layout.stepFunction = MTLVertexStepFunctionConstant;
    mtl_layout.stepRate = 0;
    mtl_layout.stride = 16; /* Doesn't matter as stepRate is 0. */
    vbo_bind_mask |= 1 << null_buffer_index;
  }

  /* Primitive Topology. */
  desc.inputPrimitiveTopology = pipeline_descriptor.vertex_descriptor.prim_topology_class;

  /* gl_PointSize constant. */
  bool null_pointsize = true;
  float MTL_pointsize = pipeline_descriptor.point_size;
  if (pipeline_descriptor.vertex_descriptor.prim_topology_class == MTLPrimitiveTopologyClassPoint)
  {
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

  bool has_error = false;
  {
    std::string function_name = entry_point_name_get(ShaderStage::VERTEX);
    NSError *error = nullptr;

    desc.vertexFunction = [shader_library_vert_
        newFunctionWithName:[NSString stringWithUTF8String:function_name.c_str()]
             constantValues:values
                      error:&error];
    if (error) {
      has_error |= (
          [[error localizedDescription] rangeOfString:@"Compilation succeeded"].location ==
          NSNotFound);

      const char *errors_c_str = [[error localizedDescription] UTF8String];
      MTL_LOG_ERROR("%s : %s", function_name.c_str(), errors_c_str);
    }
  }

  {
    std::string function_name = entry_point_name_get(ShaderStage::FRAGMENT);
    NSError *error = nullptr;

    desc.fragmentFunction = [shader_library_frag_
        newFunctionWithName:[NSString stringWithUTF8String:function_name.c_str()]
             constantValues:values
                      error:&error];
    if (error) {
      has_error |= (
          [[error localizedDescription] rangeOfString:@"Compilation succeeded"].location ==
          NSNotFound);

      const char *errors_c_str = [[error localizedDescription] UTF8String];
      MTL_LOG_ERROR("%s : %s", function_name.c_str(), errors_c_str);
    }
  }

  [values release];

  /* Only exit out if genuine error and not warning */
  if (has_error) {
    return nullptr;
  }

  /* Setup pixel format state */
  for (int color_attachment = 0; color_attachment < GPU_FB_MAX_COLOR_ATTACHMENT;
       color_attachment++)
  {
    /* Fetch color attachment pixel format in back-end pipeline state. */
    MTLPixelFormat pixel_format = pipeline_descriptor.color_attachment_format[color_attachment];
    /* Populate MTL API PSO attachment descriptor. */
    MTLRenderPipelineColorAttachmentDescriptor *col_attachment =
        desc.colorAttachments[color_attachment];

    col_attachment.pixelFormat = pixel_format;
    if (pixel_format != MTLPixelFormatInvalid) {
      bool format_supports_blending = mtl_format_supports_blending(pixel_format);

      col_attachment.writeMask = pipeline_descriptor.color_write_mask;
      col_attachment.blendingEnabled = pipeline_descriptor.blending_enabled &&
                                       format_supports_blending;
      if (format_supports_blending && pipeline_descriptor.blending_enabled) {
        col_attachment.alphaBlendOperation = pipeline_descriptor.alpha_blend_op;
        col_attachment.rgbBlendOperation = pipeline_descriptor.rgb_blend_op;
        col_attachment.destinationAlphaBlendFactor = pipeline_descriptor.dest_alpha_blend_factor;
        col_attachment.destinationRGBBlendFactor = pipeline_descriptor.dest_rgb_blend_factor;
        col_attachment.sourceAlphaBlendFactor = pipeline_descriptor.src_alpha_blend_factor;
        col_attachment.sourceRGBBlendFactor = pipeline_descriptor.src_rgb_blend_factor;
      }
      else {
        if (pipeline_descriptor.blending_enabled && !format_supports_blending) {
          shader_debug_printf(
              "[Warning] Attempting to Bake PSO, but MTLPixelFormat %d does not support "
              "blending\n",
              *((int *)&pixel_format));
        }
      }
    }
  }
  desc.depthAttachmentPixelFormat = pipeline_descriptor.depth_attachment_format;
  desc.stencilAttachmentPixelFormat = pipeline_descriptor.stencil_attachment_format;

  /* Bind-point range validation.
   * We need to ensure that the PSO will have valid bind-point ranges, or is using the
   * appropriate bindless fallback path if any bind limits are exceeded. */
#ifdef NDEBUG
  /* Ensure Buffer bindings are within range. */
  BLI_assert_msg((MTL_uniform_buffer_base_index + get_max_ubo_index() + 2) <
                     MTL_MAX_BUFFER_BINDINGS,
                 "UBO and SSBO bindings exceed the fragment bind table limit.");
#endif

  @autoreleasepool {
    MTLAutoreleasedRenderPipelineReflection reflection_data;
    NSError *error = nullptr;

    /* Compile PSO */
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
    if (!pso) {
      NSLog(@"Failed to create PSO for shader: %s, but no error was provided!\n", this->name);
      BLI_assert(false);
      return nullptr;
    }

    /* Prepare pipeline state instance. */
    MTLRenderPipelineStateInstance *pso_inst = new MTLRenderPipelineStateInstance();
    pso_inst->vert = desc.vertexFunction;
    pso_inst->frag = desc.fragmentFunction;
    pso_inst->pso = pso;
    pso_inst->null_attribute_buffer_index = (using_null_buffer) ? null_buffer_index : -1;
    pso_inst->prim_type = prim_type;

    pso_inst->parse_reflection_data(reflection_data);
    /* IMPORTANT: Discard any stale SSBOs or UBOs bindings that could override vertex bindings. */
    pso_inst->used_buf_vert_mask &= ~vbo_bind_mask;

    /* Insert into pso cache. */
    pso_cache_lock_.lock();
    pso_inst->shader_pso_index = pso_cache_.size();
    pso_cache_.add(pipeline_descriptor, pso_inst);
    pso_cache_lock_.unlock();
    shader_debug_printf(
        "PSO CACHE: Stored new variant in PSO cache for shader '%s' Hash: '%llu'\n",
        this->name,
        pipeline_descriptor.hash());
    return pso_inst;
  }
}

MTLComputePipelineStateInstance *MTLShader::bake_compute_pipeline_state(
    MTLContext *ctx, MTLComputePipelineStateDescriptor &compute_pipeline_descriptor)
{
  BLI_assert(this->is_valid());
  BLI_assert(shader_library_comp_ != nil);

  /* Check if current PSO exists in the cache. */
  pso_cache_lock_.lock();
  MTLComputePipelineStateInstance *const *pso_lookup = compute_pso_cache_.lookup_ptr(
      compute_pipeline_descriptor);
  MTLComputePipelineStateInstance *pipeline_state = (pso_lookup) ? *pso_lookup : nullptr;
  pso_cache_lock_.unlock();

  if (pipeline_state != nullptr) {
    /* Return cached PSO state. */
    BLI_assert(pipeline_state->pso != nil);
    return pipeline_state;
  }
  /* Prepare Compute Pipeline Descriptor. */

  /* Setup function specialization constants, used to modify and optimize
   * generated code based on current render pipeline configuration. */
  ::MTLFunctionConstantValues *values = populate_specialization_constant_values(
      *this->constants, compute_pipeline_descriptor.specialization_state);

  /* TODO: Compile specialized shader variants asynchronously. */

  std::string function_name = entry_point_name_get(ShaderStage::COMPUTE);
  NSError *error = nullptr;

  /* Compile compute function. */
  id<MTLFunction> compute_function = [shader_library_comp_
      newFunctionWithName:[NSString stringWithUTF8String:function_name.c_str()]
           constantValues:values
                    error:&error];
  compute_function.label = [NSString stringWithUTF8String:this->name];

  [values release];

  if (error) {
    NSLog(@"Compile Error - Metal Shader compute function, error %@", error);

    /* Only exit out if genuine error and not warning */
    if ([[error localizedDescription] rangeOfString:@"Compilation succeeded"].location ==
        NSNotFound)
    {
      return nullptr;
    }
  }

  /* Compile PSO. */
  ::MTLComputePipelineDescriptor *desc = [[MTLComputePipelineDescriptor alloc] init];
  desc.label = [NSString stringWithUTF8String:this->name];
  desc.computeFunction = compute_function;

  id<MTLComputePipelineState> pso = [ctx->device
      newComputePipelineStateWithDescriptor:desc
                                    options:MTLPipelineOptionNone
                                 reflection:nullptr
                                      error:&error];

  /* If PSO has compiled but max theoretical threads-per-threadgroup is lower than required
   * dispatch size, recompile with increased limit. NOTE: This will result in a performance drop,
   * ideally the source shader should be modified to reduce local register pressure, or, local
   * work-group size should be reduced.
   * Similarly, the custom tuning parameter "mtl_max_total_threads_per_threadgroup" can be
   * specified to a sufficiently large value to avoid this. */
  if (pso) {
    uint num_required_threads_per_threadgroup = compute_pso_common_state_.threadgroup_x_len *
                                                compute_pso_common_state_.threadgroup_y_len *
                                                compute_pso_common_state_.threadgroup_z_len;
    if (pso.maxTotalThreadsPerThreadgroup < num_required_threads_per_threadgroup) {
      MTL_LOG_WARNING(
          "Shader '%s' requires %u threads per threadgroup, but PSO limit is: %lu. Recompiling "
          "with increased limit on descriptor.\n",
          this->name,
          num_required_threads_per_threadgroup,
          (unsigned long)pso.maxTotalThreadsPerThreadgroup);
      [pso release];
      pso = nil;
      desc.maxTotalThreadsPerThreadgroup = 1024;
      pso = [ctx->device newComputePipelineStateWithDescriptor:desc
                                                       options:MTLPipelineOptionNone
                                                    reflection:nullptr
                                                         error:&error];
    }
  }

  [desc release];

  if (error) {
    NSLog(@"Failed to create PSO for compute shader: %s error %@\n", this->name, error);
    return nullptr;
  }
  if (!pso) {
    NSLog(@"Failed to create PSO for compute shader: %s, but no error was provided!\n",
          this->name);
    return nullptr;
  }

  /* Gather reflection data and create MTLComputePipelineStateInstance to store results. */
  MTLComputePipelineStateInstance *compute_pso_instance = new MTLComputePipelineStateInstance();
  compute_pso_instance->compute = compute_function;
  compute_pso_instance->pso = pso;
  pso_cache_lock_.lock();
  compute_pso_instance->shader_pso_index = compute_pso_cache_.size();
  compute_pso_cache_.add(compute_pipeline_descriptor, compute_pso_instance);
  pso_cache_lock_.unlock();

  return compute_pso_instance;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name MTLShaderCompiler
 * \{ */

MTLShaderCompiler::MTLShaderCompiler()
    : ShaderCompiler(GPU_max_parallel_compilations(), GPUWorker::ContextType::PerThread, true)
{
}

Shader *MTLShaderCompiler::compile_shader(const shader::ShaderCreateInfo &info)
{
  MTLShader *shader = static_cast<MTLShader *>(compile(info, true));

  if (shader) {
    /* Generate and cache any render PSOs if possible (typically materials only)
     * (Finalize() will already bake a Compute PSO if possible) */
    shader->warm_cache(-1);
  }

  return shader;
}

void MTLShaderCompiler::specialize_shader(ShaderSpecialization &specialization)
{
  MTLShader *shader = static_cast<MTLShader *>(specialization.shader);

  BLI_assert_msg(shader->is_valid(),
                 "Shader must be finalized before precompiling specializations");

  if (!shader->has_compute_shader_lib()) {
    /* Currently only support Compute */
    return;
  }

  /* Create descriptor using these specialization constants. */
  MTLComputePipelineStateDescriptor compute_pipeline_descriptor(specialization.constants.values);

  MTLContext *metal_context = static_cast<MTLContext *>(Context::get());
  shader->bake_compute_pipeline_state(metal_context, compute_pipeline_descriptor);
}

/** \} */

}  // namespace blender::gpu
