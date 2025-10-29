/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_framebuffer.hh"
#include "mtl_immediate.hh"
#include "mtl_memory.hh"
#include "mtl_primitive.hh"
#include "mtl_shader.hh"
#include "mtl_shader_generate.hh"
#include "mtl_shader_interface.hh"
#include "mtl_state.hh"
#include "mtl_storage_buffer.hh"
#include "mtl_uniform_buffer.hh"
#include "mtl_vertex_buffer.hh"

#include "DNA_userdef_types.h"

#include "GPU_capabilities.hh"
#include "GPU_matrix.hh"
#include "GPU_shader.hh"
#include "GPU_storage_buffer.hh"
#include "GPU_texture.hh"
#include "GPU_uniform_buffer.hh"
#include "GPU_vertex_buffer.hh"
#include "intern/gpu_matrix_private.hh"

#include "BLI_time.h"

#include <fstream>
#include <string>

using namespace blender;
using namespace blender::gpu;

/* Fire off a single dispatch per encoder. Can make debugging view clearer for texture resources
 * associated with each dispatch. */
#define MTL_DEBUG_SINGLE_DISPATCH_PER_ENCODER 0

/* Debug option to bind null buffer for missing UBOs. */
#define DEBUG_BIND_NULL_BUFFER_FOR_MISSING_UBO 0

/* Debug option to bind null buffer for missing SSBOs. NOTE: This is unsafe if replacing a
 * write-enabled SSBO and should only be used for debugging to identify binding-related issues. */
#define DEBUG_BIND_NULL_BUFFER_FOR_MISSING_SSBO 0

/* Error or warning depending on debug flag. */
#if DEBUG_BIND_NULL_BUFFER_FOR_MISSING_UBO == 1
#  define MTL_LOG_UBO_ERROR MTL_LOG_WARNING
#else
#  define MTL_LOG_UBO_ERROR MTL_LOG_ERROR
#endif

#if DEBUG_BIND_NULL_BUFFER_FOR_MISSING_SSBO == 1
#  define MTL_LOG_SSBO_ERROR MTL_LOG_WARNING
#else
#  define MTL_LOG_SSBO_ERROR MTL_LOG_ERROR
#endif

namespace blender::gpu {

/* Global memory manager. */
std::mutex MTLContext::global_memory_manager_reflock;
int MTLContext::global_memory_manager_refcount = 0;
MTLBufferPool *MTLContext::global_memory_manager = nullptr;

/* Swap-chain and latency management. */
std::atomic<int> MTLContext::max_drawables_in_flight = 0;
std::atomic<int64_t> MTLContext::avg_drawable_latency_us = 0;
int64_t MTLContext::frame_latency[MTL_FRAME_AVERAGE_COUNT] = {0};

/* -------------------------------------------------------------------- */
/** \name GHOST Context interaction.
 * \{ */

void MTLContext::set_ghost_context(GHOST_ContextHandle ghostCtxHandle)
{
  GHOST_Context *ghost_ctx = reinterpret_cast<GHOST_Context *>(ghostCtxHandle);
  BLI_assert(ghost_ctx != nullptr);

  /* Release old MTLTexture handle */
  if (default_fbo_mtltexture_) {
    [default_fbo_mtltexture_ release];
    default_fbo_mtltexture_ = nil;
  }

  /* Release Framebuffer attachments */
  MTLFrameBuffer *mtl_front_left = static_cast<MTLFrameBuffer *>(this->front_left);
  MTLFrameBuffer *mtl_back_left = static_cast<MTLFrameBuffer *>(this->back_left);
  mtl_front_left->remove_all_attachments();
  mtl_back_left->remove_all_attachments();

  GHOST_ContextMTL *ghost_mtl_ctx = dynamic_cast<GHOST_ContextMTL *>(ghost_ctx);
  if (ghost_mtl_ctx != nullptr) {
    default_fbo_mtltexture_ = ghost_mtl_ctx->metalOverlayTexture();

    MTL_LOG_DEBUG(
        "Binding GHOST context MTL %p to GPU context %p. (Device: %p, queue: %p, texture: %p)",
        ghost_mtl_ctx,
        this,
        this->device,
        this->queue,
        default_fbo_gputexture_);

    /* Check if the GHOST Context provides a default framebuffer: */
    if (default_fbo_mtltexture_) {

      /* Release old gpu::Texture handle */
      if (default_fbo_gputexture_) {
        GPU_texture_free(default_fbo_gputexture_);
        default_fbo_gputexture_ = nullptr;
      }

      /* Retain handle */
      [default_fbo_mtltexture_ retain];

      /*** Create front and back-buffers ***/
      /* Create gpu::MTLTexture objects */
      default_fbo_gputexture_ = new gpu::MTLTexture("MTL_BACKBUFFER",
                                                    TextureFormat::SFLOAT_16_16_16_16,
                                                    GPU_TEXTURE_2D,
                                                    default_fbo_mtltexture_);

      /* Update frame-buffers with new texture attachments. */
      mtl_front_left->add_color_attachment(default_fbo_gputexture_, 0, 0, 0);
      mtl_back_left->add_color_attachment(default_fbo_gputexture_, 0, 0, 0);
#ifndef NDEBUG
      this->label = default_fbo_mtltexture_.label;
#endif
    }
    else {

      /* Add default texture for cases where no other framebuffer is bound */
      if (!default_fbo_gputexture_) {
        default_fbo_gputexture_ = static_cast<gpu::MTLTexture *>(
            GPU_texture_create_2d(__func__,
                                  16,
                                  16,
                                  1,
                                  TextureFormat::SFLOAT_16_16_16_16,
                                  GPU_TEXTURE_USAGE_GENERAL,
                                  nullptr));
      }
      mtl_back_left->add_color_attachment(default_fbo_gputexture_, 0, 0, 0);

      MTL_LOG_DEBUG(
          "-- Bound context %p for GPU context: %p is offscreen and does not have a default "
          "framebuffer",
          ghost_mtl_ctx,
          this);
#ifndef NDEBUG
      this->label = @"Offscreen Metal Context";
#endif
    }
  }
  else {
    MTL_LOG_DEBUG(
        " Failed to bind GHOST context to MTLContext -- GHOST_ContextMTL is null "
        "(GhostContext: %p, GhostContext_MTL: %p)",
        ghost_ctx,
        ghost_mtl_ctx);
    BLI_assert(false);
  }
}

void MTLContext::set_ghost_window(GHOST_WindowHandle ghostWinHandle)
{
  GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghostWinHandle);
  this->set_ghost_context((GHOST_ContextHandle)(ghostWin ? ghostWin->getContext() : nullptr));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name MTLContext
 * \{ */

/* Placeholder functions */
MTLContext::MTLContext(void *ghost_window, void *ghost_context)
    : memory_manager(*this), main_command_buffer(*this)
{
  /* Init debug. */
  debug::mtl_debug_init();

  /* Initialize Render-pass and Frame-buffer State. */
  this->back_left = nullptr;

  /* Initialize command buffer state. */
  this->main_command_buffer.prepare();

  /* Frame management. */
  is_inside_frame_ = false;
  current_frame_index_ = 0;

  /* Prepare null data buffer. */
  null_buffer_ = nil;
  null_attribute_buffer_ = nil;

  /* Zero-initialize MTL textures. */
  default_fbo_mtltexture_ = nil;
  default_fbo_gputexture_ = nullptr;

  /** Fetch GHOSTContext and fetch Metal device/queue. */
  ghost_window_ = ghost_window;
  if (ghost_window_ && ghost_context == nullptr) {
    /* NOTE(Metal): Fetch ghost_context from ghost_window if it is not provided.
     * Regardless of whether windowed or not, we need access to the GhostContext
     * for presentation, and device/queue access. */
    GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghost_window_);
    ghost_context = (ghostWin ? ghostWin->getContext() : nullptr);
  }
  BLI_assert(ghost_context);
  this->ghost_context_ = static_cast<GHOST_ContextMTL *>(ghost_context);
  this->queue = (id<MTLCommandQueue>)this->ghost_context_->metalCommandQueue();
  this->device = (id<MTLDevice>)this->ghost_context_->metalDevice();
  BLI_assert(this->queue);
  BLI_assert(this->device);
  [this->queue retain];
  [this->device retain];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-method-access"
  /* Enable increased concurrent shader compiler limit.
   * NOTE: Disable warning for missing method when building on older OS's, as compiled code will
   * still work correctly when run on a system with the API available. */
  if (@available(macOS 13.3, *)) {
    [this->device setShouldMaximizeConcurrentCompilation:YES];
  }
#pragma clang diagnostic pop

  /* Register present callback. */
  this->ghost_context_->metalRegisterPresentCallback(&present);
  this->ghost_context_->metalRegisterXrBlitCallback(&xr_blit);

  /* Create FrameBuffer handles. */
  MTLFrameBuffer *mtl_front_left = new MTLFrameBuffer(this, "front_left");
  MTLFrameBuffer *mtl_back_left = new MTLFrameBuffer(this, "back_left");
  this->front_left = mtl_front_left;
  this->back_left = mtl_back_left;
  this->active_fb = this->back_left;

  /* Prepare platform and capabilities. (NOTE: With METAL, this needs to be done after CTX
   * initialization). */
  MTLBackend::platform_init(this);
  MTLBackend::capabilities_init(this);

  /* Ensure global memory manager is initialized. */
  MTLContext::global_memory_manager_acquire_ref();
  MTLContext::get_global_memory_manager()->init(this->device);

  /* Initialize Metal modules. */
  this->memory_manager.init();
  this->state_manager = new MTLStateManager(this);
  this->imm = new MTLImmediate(this);

  /* Initialize texture read/update structures. */
  this->get_texture_utils().init();

  /* Bound Samplers struct. */
  for (int i = 0; i < MTL_MAX_TEXTURE_SLOTS; i++) {
    samplers_.mtl_sampler[i] = nil;
    samplers_.mtl_sampler_flags[i] = DEFAULT_SAMPLER_STATE;
  }

  /* Initialize samplers. */
  this->sampler_state_cache_init();
}

MTLContext::~MTLContext()
{
  BLI_assert(this == MTLContext::get());
  /* Ensure rendering is complete command encoders/command buffers are freed. */
  if (MTLBackend::get()->is_inside_render_boundary()) {
    this->finish();

    /* End frame. */
    if (this->get_inside_frame()) {
      this->end_frame();
    }
  }

  /* Wait for all GPU work to finish. */
  main_command_buffer.wait_until_active_command_buffers_complete();

  /* Free textures and frame-buffers in base class. */
  free_resources();

  /* Release context textures. */
  if (default_fbo_gputexture_) {
    GPU_texture_free(default_fbo_gputexture_);
    default_fbo_gputexture_ = nullptr;
  }
  if (default_fbo_mtltexture_) {
    [default_fbo_mtltexture_ release];
    default_fbo_mtltexture_ = nil;
  }

  /* Release Memory Manager */
  this->get_scratch_buffer_manager().free();

  /* Release update/blit shaders. */
  this->get_texture_utils().cleanup();
  this->get_compute_utils().cleanup();

  /* Detach resource references. */
  GPU_texture_unbind_all();

  /* Unbind UBOs. */
  for (auto &ubo_bind : this->pipeline_state.ubo_bindings) {
    if (ubo_bind.bound && ubo_bind.ubo != nullptr) {
      GPU_uniformbuf_unbind(ubo_bind.ubo);
    }
  }

  /* Unbind SSBOs. */
  for (auto &ssbo_bind : this->pipeline_state.ssbo_bindings) {
    if (ssbo_bind.bound && ssbo_bind.ssbo != nullptr) {
      ssbo_bind.ssbo->unbind();
    }
  }

  /* Release Dummy resources. */
  this->free_dummy_resources();

  /* Release Sampler States. */
  for (int extend_yz_i = 0; extend_yz_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_yz_i++) {
    for (int extend_x_i = 0; extend_x_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_x_i++) {
      for (int filtering_i = 0; filtering_i < GPU_SAMPLER_FILTERING_TYPES_COUNT; filtering_i++) {
        if (sampler_state_cache_[extend_yz_i][extend_x_i][filtering_i] != nil) {
          [sampler_state_cache_[extend_yz_i][extend_x_i][filtering_i] release];
          sampler_state_cache_[extend_yz_i][extend_x_i][filtering_i] = nil;
        }
      }
    }
  }

  /* Release Custom Sampler States. */
  for (int i = 0; i < GPU_SAMPLER_CUSTOM_TYPES_COUNT; i++) {
    if (custom_sampler_state_cache_[i] != nil) {
      [custom_sampler_state_cache_[i] release];
      custom_sampler_state_cache_[i] = nil;
    }
  }

  /* Empty cached sampler argument buffers. */
  for (auto *entry : cached_sampler_buffers_.values()) {
    entry->free();
  }
  cached_sampler_buffers_.clear();

  /* Free null buffers. */
  if (null_buffer_) {
    [null_buffer_ release];
  }
  if (null_attribute_buffer_) {
    [null_attribute_buffer_ release];
  }

  /* Release memory manager reference. */
  MTLContext::global_memory_manager_release_ref();

  /* Free Metal objects. */
  if (this->queue) {
    [this->queue release];
  }
  if (this->device) {
    [this->device release];
  }

  this->process_frame_timings();
}

void MTLContext::begin_frame()
{
  BLI_assert(MTLBackend::get()->is_inside_render_boundary());
  if (this->get_inside_frame()) {
    return;
  }

  /* Begin Command buffer for next frame. */
  is_inside_frame_ = true;
}

void MTLContext::end_frame()
{
  BLI_assert(this->get_inside_frame());

  /* Ensure pre-present work is committed. */
  this->flush();

  /* Increment frame counter. */
  is_inside_frame_ = false;

  this->process_frame_timings();
}

void MTLContext::check_error(const char * /*info*/)
{
  /* TODO(Metal): Implement. */
}

void MTLContext::activate()
{
  /* Make sure no other context is already bound to this thread. */
  BLI_assert(is_active_ == false);
  is_active_ = true;
  thread_ = pthread_self();

  /* Re-apply ghost window/context for resizing */
  if (ghost_window_) {
    this->set_ghost_window((GHOST_WindowHandle)ghost_window_);
  }
  else if (ghost_context_) {
    this->set_ghost_context((GHOST_ContextHandle)ghost_context_);
  }

  /* Reset UBO bind state. */
  for (auto &ssbo_bind : this->pipeline_state.ubo_bindings) {
    if (ssbo_bind.bound && ssbo_bind.ubo != nullptr) {
      ssbo_bind.bound = false;
      ssbo_bind.ubo = nullptr;
    }
  }

  /* Reset SSBO bind state. */
  for (auto &ssbo_bind : this->pipeline_state.ssbo_bindings) {
    if (ssbo_bind.bound && ssbo_bind.ssbo != nullptr) {
      ssbo_bind.bound = false;
      ssbo_bind.ssbo = nullptr;
    }
  }

  /* Ensure imm active. */
  immActivate();
}

void MTLContext::deactivate()
{
  BLI_assert(this->is_active_on_thread());
  /* Flush context on deactivate. */
  this->flush();
  is_active_ = false;
  immDeactivate();
}

void MTLContext::flush()
{
  this->main_command_buffer.submit(false);
}

void MTLContext::finish()
{
  this->main_command_buffer.submit(true);
}

void MTLContext::memory_statistics_get(int *r_total_mem, int *r_free_mem)
{
  /* TODO(Metal): Implement. */
  *r_total_mem = 0;
  *r_free_mem = 0;
}

void MTLContext::framebuffer_bind(MTLFrameBuffer *framebuffer)
{
  /* We do not yet begin the pass -- We defer beginning the pass until a draw is requested. */
  BLI_assert(framebuffer);
  this->active_fb = framebuffer;
}

void MTLContext::framebuffer_restore()
{
  /* Bind default framebuffer from context --
   * We defer beginning the pass until a draw is requested. */
  this->active_fb = this->back_left;
}

id<MTLRenderCommandEncoder> MTLContext::ensure_begin_render_pass()
{
  BLI_assert(this);

  /* Ensure the rendering frame has started. */
  if (!this->get_inside_frame()) {
    this->begin_frame();
  }

  /* Check whether a framebuffer is bound. */
  if (!this->active_fb) {
    BLI_assert(false && "No framebuffer is bound!");
    return this->main_command_buffer.get_active_render_command_encoder();
  }

  /* Ensure command buffer workload submissions are optimal --
   * Though do not split a batch mid-IMM recording. */
  if (this->main_command_buffer.do_break_submission() &&
      !((MTLImmediate *)(this->imm))->imm_is_recording())
  {
    this->flush();
  }

  /* Begin pass or perform a pass switch if the active framebuffer has been changed, or if the
   * framebuffer state has been modified (is_dirty). */
  if (!this->main_command_buffer.is_inside_render_pass() ||
      this->active_fb != this->main_command_buffer.get_active_framebuffer() ||
      this->main_command_buffer.get_active_framebuffer()->get_dirty() ||
      this->is_visibility_dirty())
  {

    /* Validate bound framebuffer before beginning render pass. */
    if (!static_cast<MTLFrameBuffer *>(this->active_fb)->validate_render_pass()) {
      MTL_LOG_WARNING("Framebuffer validation failed, falling back to default framebuffer");
      this->framebuffer_restore();

      if (!static_cast<MTLFrameBuffer *>(this->active_fb)->validate_render_pass()) {
        MTL_LOG_ERROR("CRITICAL: DEFAULT FRAMEBUFFER FAIL VALIDATION!!");
      }
    }

    /* Begin RenderCommandEncoder on main CommandBuffer. */
    bool new_render_pass = false;
    id<MTLRenderCommandEncoder> new_enc =
        this->main_command_buffer.ensure_begin_render_command_encoder(
            static_cast<MTLFrameBuffer *>(this->active_fb), true, &new_render_pass);
    if (new_render_pass) {
      /* Flag context pipeline state as dirty - dynamic pipeline state need re-applying. */
      this->pipeline_state.dirty_flags = MTL_PIPELINE_STATE_ALL_FLAG;
    }
    return new_enc;
  }
  BLI_assert(!this->main_command_buffer.get_active_framebuffer()->get_dirty());
  return this->main_command_buffer.get_active_render_command_encoder();
}

MTLFrameBuffer *MTLContext::get_current_framebuffer()
{
  MTLFrameBuffer *last_bound = static_cast<MTLFrameBuffer *>(this->active_fb);
  return last_bound ? last_bound : this->get_default_framebuffer();
}

MTLFrameBuffer *MTLContext::get_default_framebuffer()
{
  return static_cast<MTLFrameBuffer *>(this->back_left);
}

MTLShader *MTLContext::get_active_shader()
{
  return this->pipeline_state.active_shader;
}

id<MTLBuffer> MTLContext::get_null_buffer()
{
  if (null_buffer_ != nil) {
    return null_buffer_;
  }

  /* TODO(mpw_apple_gpusw): Null buffer size temporarily increased to cover
   * maximum possible UBO size. There are a number of cases which need to be
   * resolved in the high level where an expected UBO does not have a bound
   * buffer. The null buffer needs to at least cover the size of these
   * UBOs to avoid any GPU memory issues. */
  static const int null_buffer_size = 20480;
  null_buffer_ = [this->device newBufferWithLength:null_buffer_size
                                           options:MTLResourceStorageModeManaged];
  [null_buffer_ retain];
  uint32_t *null_data = (uint32_t *)calloc(1, null_buffer_size);
  memcpy([null_buffer_ contents], null_data, null_buffer_size);
  [null_buffer_ didModifyRange:NSMakeRange(0, null_buffer_size)];
  free(null_data);

  BLI_assert(null_buffer_ != nil);
  return null_buffer_;
}

id<MTLBuffer> MTLContext::get_null_attribute_buffer()
{
  if (null_attribute_buffer_ != nil) {
    return null_attribute_buffer_;
  }

  /* Allocate Null buffer if it has not yet been created.
   * Min buffer size is 256 bytes -- though we only need 64 bytes of data. */
  static const int null_buffer_size = 256;
  null_attribute_buffer_ = [this->device newBufferWithLength:null_buffer_size
                                                     options:MTLResourceStorageModeManaged];
  BLI_assert(null_attribute_buffer_ != nil);
  [null_attribute_buffer_ retain];
  float data[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  memcpy([null_attribute_buffer_ contents], data, sizeof(float) * 4);
  [null_attribute_buffer_ didModifyRange:NSMakeRange(0, null_buffer_size)];

  return null_attribute_buffer_;
}

gpu::MTLTexture *MTLContext::get_dummy_texture(GPUTextureType type,
                                               GPUSamplerFormat sampler_format)
{
  /* Decrement 1 from texture type as they start from 1 and go to 32 (inclusive). Remap to 0..31 */
  gpu::MTLTexture *dummy_tex = dummy_textures_[sampler_format][type - 1];
  if (dummy_tex != nullptr) {
    return dummy_tex;
  }
  /* Determine format for dummy texture. */
  TextureFormat format = TextureFormat::UNORM_8_8_8_8;
  switch (sampler_format) {
    case GPU_SAMPLER_TYPE_FLOAT:
      format = TextureFormat::UNORM_8_8_8_8;
      break;
    case GPU_SAMPLER_TYPE_INT:
      format = TextureFormat::SINT_8_8_8_8;
      break;
    case GPU_SAMPLER_TYPE_UINT:
      format = TextureFormat::UINT_8_8_8_8;
      break;
    case GPU_SAMPLER_TYPE_DEPTH:
      format = TextureFormat::SFLOAT_32_DEPTH_UINT_8;
      break;
    default:
      BLI_assert_unreachable();
  }

  /* Create dummy texture based on desired type. */
  gpu::Texture *tex = nullptr;
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL;
  switch (type) {
    case GPU_TEXTURE_1D:
      tex = GPU_texture_create_1d("Dummy 1D", 128, 1, format, usage, nullptr);
      break;
    case GPU_TEXTURE_1D_ARRAY:
      tex = GPU_texture_create_1d_array("Dummy 1DArray", 128, 1, 1, format, usage, nullptr);
      break;
    case GPU_TEXTURE_2D:
      tex = GPU_texture_create_2d("Dummy 2D", 128, 128, 1, format, usage, nullptr);
      break;
    case GPU_TEXTURE_2D_ARRAY:
      tex = GPU_texture_create_2d_array("Dummy 2DArray", 128, 128, 1, 1, format, usage, nullptr);
      break;
    case GPU_TEXTURE_3D:
      tex = GPU_texture_create_3d("Dummy 3D", 128, 128, 1, 1, format, usage, nullptr);
      break;
    case GPU_TEXTURE_CUBE:
      tex = GPU_texture_create_cube("Dummy Cube", 128, 1, format, usage, nullptr);
      break;
    case GPU_TEXTURE_CUBE_ARRAY:
      tex = GPU_texture_create_cube_array("Dummy CubeArray", 128, 1, 1, format, usage, nullptr);
      break;
    case GPU_TEXTURE_BUFFER:
      if (!dummy_verts_[sampler_format]) {
        GPU_vertformat_clear(&dummy_vertformat_[sampler_format]);

        VertAttrType attr_type = VertAttrType::SFLOAT_32_32_32_32;

        switch (sampler_format) {
          case GPU_SAMPLER_TYPE_FLOAT:
          case GPU_SAMPLER_TYPE_DEPTH:
            attr_type = VertAttrType::SFLOAT_32_32_32_32;
            break;
          case GPU_SAMPLER_TYPE_INT:
            attr_type = VertAttrType::SINT_32_32_32_32;
            break;
          case GPU_SAMPLER_TYPE_UINT:
            attr_type = VertAttrType::UINT_32_32_32_32;
            break;
          default:
            BLI_assert_unreachable();
        }

        GPU_vertformat_attr_add(&dummy_vertformat_[sampler_format], "dummy", attr_type);
        dummy_verts_[sampler_format] = GPU_vertbuf_create_with_format_ex(
            dummy_vertformat_[sampler_format],
            GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
        GPU_vertbuf_data_alloc(*dummy_verts_[sampler_format], 64);
      }
      tex = GPU_texture_create_from_vertbuf("Dummy TextureBuffer", dummy_verts_[sampler_format]);
      break;
    default:
      BLI_assert_msg(false, "Unrecognised texture type");
      return nullptr;
  }
  gpu::MTLTexture *metal_tex = static_cast<gpu::MTLTexture *>(reinterpret_cast<Texture *>(tex));
  dummy_textures_[sampler_format][type - 1] = metal_tex;
  return metal_tex;
}

void MTLContext::free_dummy_resources()
{
  for (int format = 0; format < GPU_SAMPLER_TYPE_MAX; format++) {
    for (int tex = 0; tex < GPU_TEXTURE_BUFFER; tex++) {
      if (dummy_textures_[format][tex]) {
        GPU_texture_free(reinterpret_cast<gpu::Texture *>(
            static_cast<Texture *>(dummy_textures_[format][tex])));
        dummy_textures_[format][tex] = nullptr;
      }
    }
    if (dummy_verts_[format]) {
      GPU_vertbuf_discard(dummy_verts_[format]);
    }
  }
}

void MTLContext::specialization_constants_set(
    const shader::SpecializationConstants *constants_state)
{
  this->constants_state = (constants_state != nullptr) ? *constants_state :
                                                         shader::SpecializationConstants{};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Global Context State
 * \{ */

/* Needs to run before buffer and push constant binding. */
static void bind_atomic_workaround_buffer(StringRefNull texture_name,
                                          MTLTexture *gpu_tex,
                                          MTLShaderInterface &shader_interface,
                                          MTLShader &active_shader)
{
  BLI_assert(gpu_tex->usage_get() & GPU_TEXTURE_USAGE_ATOMIC);

  std::string buf_name = texture_name + std::string("_buf_");
  std::string metadata_name = texture_name + std::string("_metadata_");

  int buf_slot = shader_interface.ssbo_get(buf_name)->binding;
  gpu_tex->get_storagebuf()->bind(buf_slot);

  int pc_loc = shader_interface.uniform_get(metadata_name)->location;
  active_shader.uniform_int(pc_loc, 4, 1, gpu_tex->get_texture_metadata_ptr());
}

template<typename CommandEncoderT>
static void bind_sampler_argument_buffer(
    CommandEncoderT enc,
    MTLSamplerArray &sampler_array,
    blender::Map<MTLSamplerArray, gpu::MTLBuffer *> &sampler_buffers_cache,
    MTLShaderInterface &shader_interface,
    id<MTLFunction> mtl_function,
    MTLBindingCache<CommandEncoderT> &bindings)
{
  const int arg_buffer_idx = MTL_SAMPLER_ARGUMENT_BUFFER_SLOT;
  BLI_assert(arg_buffer_idx < 32);

  const uint64_t tex_slot_mask = shader_interface.enabled_tex_mask_;
  const uint max_sampler_index = 63 - bitscan_reverse_uint64(tex_slot_mask);

  /* Generate or Fetch argument buffer sampler configuration.
   * NOTE(Metal): we need to base sampler counts off of the maximal texture
   * index. This is not the most optimal, but in practice, not a use-case
   * when argument buffers are required.
   * This is because with explicit texture indices, the binding indices
   * should match across draws, to allow the high-level to optimize bind-points. */
  sampler_array.num_samplers = max_sampler_index + 1;

  gpu::MTLBuffer *encoder_buf = sampler_buffers_cache.lookup_or_add_cb(sampler_array, [&]() {
    id<MTLArgumentEncoder> argument_encoder = shader_interface.ensure_argument_encoder(
        mtl_function);

    /* Populate argument buffer with current global sampler bindings. */
    size_t size = [argument_encoder encodedLength];
    size_t alignment = max_uu([argument_encoder alignment], 256);
    size_t size_align_delta = (size % alignment);
    size_t aligned_alloc_size = ((alignment > 1) && (size_align_delta > 0)) ?
                                    size + (alignment - (size % alignment)) :
                                    size;

    /* Allocate buffer to store encoded sampler arguments. */
    encoder_buf = MTLContext::get_global_memory_manager()->allocate(aligned_alloc_size, true);

    BLI_assert(encoder_buf);
    BLI_assert(encoder_buf->get_metal_buffer());
    [argument_encoder setArgumentBuffer:encoder_buf->get_metal_buffer() offset:0];
    [argument_encoder setSamplerStates:sampler_array.mtl_sampler
                             withRange:NSMakeRange(0, max_sampler_index + 1)];
    encoder_buf->flush();

    return encoder_buf;
  });

  bindings.bind_buffer(enc, encoder_buf->get_metal_buffer(), 0, arg_buffer_idx);
}

/* Ensure texture bindings are correct and up to date for current draw call.
 * We will iterate through all texture bindings on the context and determine if any of the
 * active slots match those in our shader interface. If so, textures will be bound. */
template<typename CommandEncoderT>
static void ensure_texture_bindings(MTLContext &ctx,
                                    MTLShader &shader,
                                    CommandEncoderT enc,
                                    MTLBindingCache<CommandEncoderT> &bindings,
                                    id<MTLFunction> mtl_function,
                                    uint16_t stage_ima_mask = uint16_t(-1),
                                    uint64_t stage_tex_mask = uint64_t(-1))
{
  MTLShaderInterface &shader_interface = shader.get_interface();

  if (shader_interface.enabled_ima_mask_ == 0 && shader_interface.enabled_tex_mask_ == 0) {
    return;
  }

  /* TODO(fclem): Dirty binding tracking optimization. */
  uint16_t dirty_image_mask = ~uint16_t(0u);
  uint64_t dirty_sampler_mask = ~uint64_t(0u);

  uint16_t dirty_enabled_image_mask = shader_interface.enabled_ima_mask_ & dirty_image_mask;
  uint64_t dirty_enabled_sampler_mask = shader_interface.enabled_tex_mask_ & dirty_sampler_mask;

  uint16_t bind_image = dirty_enabled_image_mask & stage_ima_mask;
  for (const uint slot : bits::iter_1_indices(bind_image)) {
    MTLTexture *gpu_tex = ctx.pipeline_state.image_bindings[slot].texture_resource;

    if (gpu_tex == nullptr) {
      /* TODO(fclem): texture_get can also return uniforms or images. */
      const int name_ofs = shader_interface.texture_get(slot)->name_offset;
      MTL_LOG_ERROR("Shader %s: Missing image bind: %s slot(%d).",
                    shader.name_get().c_str(),
                    shader_interface.name_at_offset(name_ofs),
                    slot);
      continue;
    }
    /* If texture resource is an image binding and has a non-default swizzle mask, we need
     * to bind the source texture resource to retain image write access. */
    id<MTLTexture> tex = gpu_tex->has_custom_swizzle() ? gpu_tex->get_metal_handle_base() :
                                                         gpu_tex->get_metal_handle();
    bindings.bind_texture(enc, tex, MTL_IMAGE_SLOT_OFFSET + slot);
    if (shader_interface.use_texture_atomic() && (gpu_tex->usage_get() & GPU_TEXTURE_USAGE_ATOMIC))
    {
      if (MTLBackend::get_capabilities().supports_texture_atomics == false) {
        bind_atomic_workaround_buffer(shader_interface.image_name_get(slot),
                                      gpu_tex,
                                      shader_interface,
                                      *ctx.pipeline_state.active_shader);
      }
    }
  }

  uint64_t bind_sampler = dirty_enabled_sampler_mask & stage_tex_mask;
  for (const uint slot : bits::iter_1_indices(bind_sampler)) {
    MTLTexture *gpu_tex = ctx.pipeline_state.texture_bindings[slot].texture_resource;
    MTLSamplerBinding &sampler_state = ctx.pipeline_state.sampler_bindings[slot];

    if (gpu_tex == nullptr) {
      /* TODO(fclem): texture_get can also return uniforms or images. */
      const int name_ofs = shader_interface.texture_get(slot)->name_offset;
      MTL_LOG_ERROR("Shader %s: Missing texture bind: %s slot(%d).",
                    shader.name_get().c_str(),
                    shader_interface.name_at_offset(name_ofs),
                    slot);
      continue;
    }

    id<MTLSamplerState> mtl_sampler = (sampler_state.state == DEFAULT_SAMPLER_STATE) ?
                                          ctx.get_default_sampler_state() :
                                          ctx.get_sampler_from_state(sampler_state.state);

    bindings.bind_texture(enc, gpu_tex->get_metal_handle(), MTL_SAMPLER_SLOT_OFFSET + slot);
    bindings.bind_sampler(enc,
                          ctx.get_sampler_array(),
                          mtl_sampler,
                          sampler_state.state,
                          shader_interface.use_samplers_argument_buffer(),
                          slot);
    if (shader_interface.use_texture_atomic() && (gpu_tex->usage_get() & GPU_TEXTURE_USAGE_ATOMIC))
    {
      if (MTLBackend::get_capabilities().supports_texture_atomics == false) {
        bind_atomic_workaround_buffer(shader_interface.sampler_name_get(slot),
                                      gpu_tex,
                                      shader_interface,
                                      *ctx.pipeline_state.active_shader);
      }
    }
  }

  /* Construct and Bind argument buffer.
   * NOTE(Metal): Samplers use an argument buffer when the limit of 16 samplers is exceeded. */
  if (shader_interface.use_samplers_argument_buffer()) {
    bind_sampler_argument_buffer(enc,
                                 ctx.get_sampler_array(),
                                 ctx.get_sampler_arg_buf_cache(),
                                 shader_interface,
                                 mtl_function,
                                 bindings);
  }
}

template<typename CommandEncoderT>
static void ensure_push_constant(MTLContext &ctx,
                                 MTLShader &shader,
                                 CommandEncoderT enc,
                                 MTLBindingCache<CommandEncoderT> &bindings,
                                 uint32_t stage_buf_mask = uint32_t(-1))
{
  if ((stage_buf_mask & (1 << MTL_PUSH_CONSTANT_BUFFER_SLOT)) == 0) {
    /* Push constant is not used by this shader. We can skip it. */
    return;
  }
  MTLPushConstantBuf *pc_buf = shader.get_push_constant_buf();
  /* Only need to rebind block if push constants have been modified -- or if no data is bound for
   * the current RenderCommandEncoder. */
  bindings.bind_bytes(enc,
                      ctx.get_scratch_buffer_manager(),
                      pc_buf->data(),
                      pc_buf->size(),
                      MTL_PUSH_CONSTANT_BUFFER_SLOT);
}

/* Bind UBOs and SSBOs to an active render command encoder using the rendering state of the
 * current context -> Active shader, Bound UBOs).
 * NOTE: `ensure_buffer_bindings` must be called after `ensure_texture_bindings` to allow
 * for binding of buffer-backed texture's data buffer and metadata. */
template<typename CommandEncoderT>
static void ensure_buffer_bindings(MTLContext &ctx,
                                   MTLShader &shader,
                                   CommandEncoderT enc,
                                   MTLBindingCache<CommandEncoderT> &bindings,
                                   uint32_t stage_buf_mask = uint32_t(-1))
{
  MTLShaderInterface &shader_interface = shader.get_interface();

  /* TODO(fclem): Dirty binding tracking optimization. */
  uint32_t dirty_ubo_mask = ~uint32_t(0u);
  uint32_t dirty_ssbo_mask = ~uint32_t(0u);

  uint32_t dirty_enabled_ubo_mask = shader_interface.enabled_ubo_mask_ & dirty_ubo_mask;
  uint32_t dirty_enabled_ssbo_mask = shader_interface.enabled_ssbo_mask_ & dirty_ssbo_mask;

  uint32_t bind_ubo = dirty_enabled_ubo_mask & (stage_buf_mask >> MTL_UBO_SLOT_OFFSET);
  for (const uint slot : bits::iter_1_indices(bind_ubo)) {
    MTLUniformBufferBinding &bind = ctx.pipeline_state.ubo_bindings[slot];
    if (bind.ubo) {
      bindings.bind_buffer(enc, bind.ubo->get_metal_buffer(), 0, MTL_UBO_SLOT_OFFSET + slot);
    }
    else {
      const int name_ofs = shader_interface.ubo_get(slot)->name_offset;
      MTL_LOG_ERROR("Shader %s: Missing UBO bind: %s slot(%d).",
                    shader.name_get().c_str(),
                    shader_interface.name_at_offset(name_ofs),
                    slot);
    }
  }

  uint32_t bind_ssbo = dirty_enabled_ssbo_mask & (stage_buf_mask >> MTL_SSBO_SLOT_OFFSET);
  for (const uint slot : bits::iter_1_indices(bind_ssbo)) {
    MTLStorageBufferBinding &bind = ctx.pipeline_state.ssbo_bindings[slot];
    if (bind.ssbo) {
      bindings.bind_buffer(enc, bind.ssbo->get_metal_buffer(), 0, MTL_SSBO_SLOT_OFFSET + slot);
    }
    else {
      const int name_ofs = shader_interface.ssbo_get(slot)->name_offset;
      MTL_LOG_ERROR("Shader %s: Missing SSBO bind: %s slot(%d).",
                    shader.name_get().c_str(),
                    shader_interface.name_at_offset(name_ofs),
                    slot);
    }
  }
}

void MTLContext::pipeline_state_init()
{
  /** Default States. **/

  /* Clear blending State. */
  this->pipeline_state.color_write_mask = MTLColorWriteMaskRed | MTLColorWriteMaskGreen |
                                          MTLColorWriteMaskBlue | MTLColorWriteMaskAlpha;
  this->pipeline_state.blending_enabled = false;
  this->pipeline_state.alpha_blend_op = MTLBlendOperationAdd;
  this->pipeline_state.rgb_blend_op = MTLBlendOperationAdd;
  this->pipeline_state.dest_alpha_blend_factor = MTLBlendFactorZero;
  this->pipeline_state.dest_rgb_blend_factor = MTLBlendFactorZero;
  this->pipeline_state.src_alpha_blend_factor = MTLBlendFactorOne;
  this->pipeline_state.src_rgb_blend_factor = MTLBlendFactorOne;

  /* Viewport and scissor. */
  for (int v = 0; v < GPU_MAX_VIEWPORTS; v++) {
    this->pipeline_state.viewport_offset_x[v] = 0;
    this->pipeline_state.viewport_offset_y[v] = 0;
    this->pipeline_state.viewport_width[v] = 0;
    this->pipeline_state.viewport_height[v] = 0;
  }
  this->pipeline_state.scissor_x = 0;
  this->pipeline_state.scissor_y = 0;
  this->pipeline_state.scissor_width = 0;
  this->pipeline_state.scissor_height = 0;
  this->pipeline_state.scissor_enabled = false;

  /* Culling State. */
  this->pipeline_state.culling_enabled = false;
  this->pipeline_state.cull_mode = GPU_CULL_NONE;
  this->pipeline_state.front_face = GPU_COUNTERCLOCKWISE;

  /* DATA and IMAGE access state. */
  this->pipeline_state.unpack_row_length = 0;

  /* Depth State. */
  this->pipeline_state.depth_stencil_state.depth_write_enable = false;
  this->pipeline_state.depth_stencil_state.depth_test_enabled = false;
  this->pipeline_state.depth_stencil_state.depth_range_near = 0.0;
  this->pipeline_state.depth_stencil_state.depth_range_far = 1.0;
  this->pipeline_state.depth_stencil_state.depth_function = MTLCompareFunctionAlways;
  this->pipeline_state.depth_stencil_state.depth_bias = 0.0;
  this->pipeline_state.depth_stencil_state.depth_slope_scale = 0.0;
  this->pipeline_state.depth_stencil_state.depth_bias_enabled_for_points = false;
  this->pipeline_state.depth_stencil_state.depth_bias_enabled_for_lines = false;
  this->pipeline_state.depth_stencil_state.depth_bias_enabled_for_tris = false;

  /* Stencil State. */
  this->pipeline_state.depth_stencil_state.stencil_test_enabled = false;
  this->pipeline_state.depth_stencil_state.stencil_read_mask = 0xFF;
  this->pipeline_state.depth_stencil_state.stencil_write_mask = 0xFF;
  this->pipeline_state.depth_stencil_state.stencil_ref = 0;
  this->pipeline_state.depth_stencil_state.stencil_func = MTLCompareFunctionAlways;
  this->pipeline_state.depth_stencil_state.stencil_op_front_stencil_fail = MTLStencilOperationKeep;
  this->pipeline_state.depth_stencil_state.stencil_op_front_depth_fail = MTLStencilOperationKeep;
  this->pipeline_state.depth_stencil_state.stencil_op_front_depthstencil_pass =
      MTLStencilOperationKeep;
  this->pipeline_state.depth_stencil_state.stencil_op_back_stencil_fail = MTLStencilOperationKeep;
  this->pipeline_state.depth_stencil_state.stencil_op_back_depth_fail = MTLStencilOperationKeep;
  this->pipeline_state.depth_stencil_state.stencil_op_back_depthstencil_pass =
      MTLStencilOperationKeep;
}

void MTLContext::set_viewport(int origin_x, int origin_y, int width, int height)
{
  BLI_assert(this);
  BLI_assert(width > 0);
  BLI_assert(height > 0);
  BLI_assert(origin_x >= 0);
  BLI_assert(origin_y >= 0);
  bool changed = (this->pipeline_state.viewport_offset_x[0] != origin_x) ||
                 (this->pipeline_state.viewport_offset_y[0] != origin_y) ||
                 (this->pipeline_state.viewport_width[0] != width) ||
                 (this->pipeline_state.viewport_height[0] != height) ||
                 (this->pipeline_state.num_active_viewports != 1);
  this->pipeline_state.viewport_offset_x[0] = origin_x;
  this->pipeline_state.viewport_offset_y[0] = origin_y;
  this->pipeline_state.viewport_width[0] = width;
  this->pipeline_state.viewport_height[0] = height;
  this->pipeline_state.num_active_viewports = 1;

  if (changed) {
    this->pipeline_state.dirty_flags = (this->pipeline_state.dirty_flags |
                                        MTL_PIPELINE_STATE_VIEWPORT_FLAG);
  }
}

void MTLContext::set_viewports(int count, const int (&viewports)[GPU_MAX_VIEWPORTS][4])
{
  BLI_assert(this);
  bool changed = (this->pipeline_state.num_active_viewports != count);
  for (int v = 0; v < count; v++) {
    const int (&viewport_info)[4] = viewports[v];

    BLI_assert(viewport_info[0] >= 0);
    BLI_assert(viewport_info[1] >= 0);
    BLI_assert(viewport_info[2] > 0);
    BLI_assert(viewport_info[3] > 0);

    changed = changed || (this->pipeline_state.viewport_offset_x[v] != viewport_info[0]) ||
              (this->pipeline_state.viewport_offset_y[v] != viewport_info[1]) ||
              (this->pipeline_state.viewport_width[v] != viewport_info[2]) ||
              (this->pipeline_state.viewport_height[v] != viewport_info[3]);
    this->pipeline_state.viewport_offset_x[v] = viewport_info[0];
    this->pipeline_state.viewport_offset_y[v] = viewport_info[1];
    this->pipeline_state.viewport_width[v] = viewport_info[2];
    this->pipeline_state.viewport_height[v] = viewport_info[3];
  }
  this->pipeline_state.num_active_viewports = count;

  if (changed) {
    this->pipeline_state.dirty_flags = (this->pipeline_state.dirty_flags |
                                        MTL_PIPELINE_STATE_VIEWPORT_FLAG);
  }
}

void MTLContext::set_scissor(int scissor_x, int scissor_y, int scissor_width, int scissor_height)
{
  BLI_assert(this);
  bool changed = (this->pipeline_state.scissor_x != scissor_x) ||
                 (this->pipeline_state.scissor_y != scissor_y) ||
                 (this->pipeline_state.scissor_width != scissor_width) ||
                 (this->pipeline_state.scissor_height != scissor_height) ||
                 (this->pipeline_state.scissor_enabled != true);
  this->pipeline_state.scissor_x = scissor_x;
  this->pipeline_state.scissor_y = scissor_y;
  this->pipeline_state.scissor_width = scissor_width;
  this->pipeline_state.scissor_height = scissor_height;
  this->pipeline_state.scissor_enabled = (scissor_width > 0 && scissor_height > 0);

  if (changed) {
    this->pipeline_state.dirty_flags = (this->pipeline_state.dirty_flags |
                                        MTL_PIPELINE_STATE_SCISSOR_FLAG);
  }
}

void MTLContext::set_scissor_enabled(bool scissor_enabled)
{
  /* Only turn on Scissor if requested scissor region is valid */
  scissor_enabled = scissor_enabled && (this->pipeline_state.scissor_width > 0 &&
                                        this->pipeline_state.scissor_height > 0);

  bool changed = (this->pipeline_state.scissor_enabled != scissor_enabled);
  this->pipeline_state.scissor_enabled = scissor_enabled;
  if (changed) {
    this->pipeline_state.dirty_flags = (this->pipeline_state.dirty_flags |
                                        MTL_PIPELINE_STATE_SCISSOR_FLAG);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Command Encoder and pipeline state
 * These utilities ensure that all of the globally bound resources and state have been
 * correctly encoded within the current RenderCommandEncoder. This involves managing
 * buffer bindings, texture bindings, depth stencil state and dynamic pipeline state.
 *
 * We will also trigger compilation of new PSOs where the input state has changed
 * and is required.
 * All of this setup is required in order to perform a valid draw call.
 * \{ */

bool MTLContext::ensure_render_pipeline_state(MTLPrimitiveType mtl_prim_type)
{
  MTLShader *shader = this->pipeline_state.active_shader;
  /* Check if an active shader is bound. */
  if (!shader) {
    MTL_LOG_WARNING("No Metal shader for bound GL shader");
    return false;
  }

  /* Also ensure active shader is valid. */
  if (!shader->is_valid()) {
    MTL_LOG_WARNING(
        "Bound active shader is not valid (Missing/invalid implementation for Metal).", );
    return false;
  }

  /* Apply global state. */
  this->state_manager->apply_state();

  /* Main command buffer tracks the current state of the render pass, based on bound
   * MTLFrameBuffer. */
  MTLRenderPassState &rps = this->main_command_buffer.get_render_pass_state();

  /* Debug Check: Ensure Framebuffer instance is not dirty. */
  BLI_assert(!this->main_command_buffer.get_active_framebuffer()->get_dirty());

  MTLShaderInterface &shader_interface = shader->get_interface();

  /* Fetch shader and bake valid PipelineStateObject (PSO) based on current
   * shader and state combination. This PSO represents the final GPU-executable
   * permutation of the shader. */
  MTLRenderPipelineStateInstance *psi = shader->bake_current_pipeline_state(
      this, mtl_prim_type_to_topology_class(mtl_prim_type));

  if (!psi) {
    MTL_LOG_ERROR("Failed to bake Metal pipeline state for shader: %s",
                  shader_interface.name_get());
    return false;
  }

  if (!psi->pso) {
    MTL_LOG_ERROR("PSO for shader %s is null.", shader_interface.name_get());
    return false;
  }

  /* Fetch render command encoder. A render pass should already be active.
   * This will be NULL if invalid. */
  id<MTLRenderCommandEncoder> rec = this->main_command_buffer.get_active_render_command_encoder();
  BLI_assert(rec);
  if (rec == nil) {
    MTL_LOG_ERROR("ensure_render_pipeline_state called while render pass is not active.");
    return false;
  }

  /* Bind Render Pipeline State. */
  BLI_assert(psi->pso);
  if (rps.bound_pso != psi->pso) {
    [rec setRenderPipelineState:psi->pso];
    rps.bound_pso = psi->pso;
  }

  bool active_shader_changed = assign_if_different(
      rps.last_bound_shader_state, MTLBoundShaderState{shader, psi->shader_pso_index});

  MTLPushConstantBuf *pc_buf = shader->get_push_constant_buf();
  if (active_shader_changed && pc_buf) {
    pc_buf->tag_dirty();
  }

  if (G.debug & G_DEBUG_GPU) {
    [rec pushDebugGroup:@"ApplyState"];
  }

  /** Ensure resource bindings. */
  MTLVertexCommandEncoder vert_rec{rec};
  MTLFragmentCommandEncoder frag_rec{rec};
  ensure_texture_bindings(*this,
                          *shader,
                          vert_rec,
                          rps.vertex_bindings,
                          psi->vert,
                          psi->used_ima_vert_mask,
                          psi->used_tex_vert_mask);
  ensure_texture_bindings(*this,
                          *shader,
                          frag_rec,
                          rps.fragment_bindings,
                          psi->frag,
                          psi->used_ima_frag_mask,
                          psi->used_tex_frag_mask);
  ensure_buffer_bindings(*this, *shader, vert_rec, rps.vertex_bindings, psi->used_buf_vert_mask);
  ensure_buffer_bindings(*this, *shader, frag_rec, rps.fragment_bindings, psi->used_buf_frag_mask);
  if (pc_buf && pc_buf->is_dirty()) {
    ensure_push_constant(*this, *shader, vert_rec, rps.vertex_bindings, psi->used_buf_vert_mask);
    ensure_push_constant(*this, *shader, frag_rec, rps.fragment_bindings, psi->used_buf_frag_mask);
    pc_buf->tag_updated();
  }

  /* Bind Null attribute buffer, if needed. */
  if (psi->null_attribute_buffer_index >= 0) {
    if (G.debug & G_DEBUG_GPU) {
      MTL_LOG_DEBUG("Binding null attribute buffer at index: %d",
                    psi->null_attribute_buffer_index);
    }
    rps.bind_vertex_buffer(this->get_null_attribute_buffer(), 0, psi->null_attribute_buffer_index);
  }

  /** Dynamic Per-draw Render State on RenderCommandEncoder. */
  /* State: Viewport. */
  if (this->pipeline_state.num_active_viewports > 1) {
    /* Multiple Viewports. */
    MTLViewport viewports[GPU_MAX_VIEWPORTS];
    for (int v = 0; v < this->pipeline_state.num_active_viewports; v++) {
      MTLViewport &viewport = viewports[v];
      viewport.originX = (double)this->pipeline_state.viewport_offset_x[v];
      viewport.originY = (double)this->pipeline_state.viewport_offset_y[v];
      viewport.width = (double)this->pipeline_state.viewport_width[v];
      viewport.height = (double)this->pipeline_state.viewport_height[v];
      viewport.znear = this->pipeline_state.depth_stencil_state.depth_range_near;
      viewport.zfar = this->pipeline_state.depth_stencil_state.depth_range_far;
    }
    [rec setViewports:viewports count:this->pipeline_state.num_active_viewports];
  }
  else {
    /* Single Viewport. */
    MTLViewport viewport;
    viewport.originX = (double)this->pipeline_state.viewport_offset_x[0];
    viewport.originY = (double)this->pipeline_state.viewport_offset_y[0];
    viewport.width = (double)this->pipeline_state.viewport_width[0];
    viewport.height = (double)this->pipeline_state.viewport_height[0];
    viewport.znear = this->pipeline_state.depth_stencil_state.depth_range_near;
    viewport.zfar = this->pipeline_state.depth_stencil_state.depth_range_far;
    [rec setViewport:viewport];
  }

  /* State: Scissor. */
  if (this->pipeline_state.dirty_flags & MTL_PIPELINE_STATE_SCISSOR_FLAG) {

    /* Get FrameBuffer associated with active RenderCommandEncoder. */
    MTLFrameBuffer *render_fb = this->main_command_buffer.get_active_framebuffer();

    MTLScissorRect scissor;
    if (this->pipeline_state.scissor_enabled) {
      scissor.x = this->pipeline_state.scissor_x;
      scissor.y = this->pipeline_state.scissor_y;
      scissor.width = this->pipeline_state.scissor_width;
      scissor.height = this->pipeline_state.scissor_height;

      /* Some scissor assignments exceed the bounds of the viewport due to implicitly added
       * padding to the width/height - Clamp width/height. */
      BLI_assert(scissor.x >= 0 && scissor.x < render_fb->get_default_width());
      BLI_assert(scissor.y >= 0 && scissor.y < render_fb->get_default_height());
      scissor.width = (uint)min_ii(scissor.width,
                                   max_ii(render_fb->get_default_width() - (int)(scissor.x), 0));
      scissor.height = (uint)min_ii(scissor.height,
                                    max_ii(render_fb->get_default_height() - (int)(scissor.y), 0));
      BLI_assert(scissor.width > 0 &&
                 (scissor.x + scissor.width <= render_fb->get_default_width()));
      BLI_assert(scissor.height > 0 && (scissor.height <= render_fb->get_default_height()));
    }
    else {
      /* Scissor is disabled, reset to default size as scissor state may have been previously
       * assigned on this encoder.
       * NOTE: If an attachment-less framebuffer is used, fetch specified width/height rather
       * than active attachment width/height as provided by get_default_w/h(). */
      uint default_w = render_fb->get_default_width();
      uint default_h = render_fb->get_default_height();
      bool is_attachmentless = (default_w == 0) && (default_h == 0);
      scissor.x = 0;
      scissor.y = 0;
      scissor.width = (is_attachmentless) ? render_fb->get_width() : default_w;
      scissor.height = (is_attachmentless) ? render_fb->get_height() : default_h;
    }

    /* Scissor state can still be flagged as changed if it is toggled on and off, without
     * parameters changing between draws. */
    if (memcmp(&scissor, &rps.last_scissor_rect, sizeof(MTLScissorRect)) != 0) {
      [rec setScissorRect:scissor];
      rps.last_scissor_rect = scissor;
    }
    this->pipeline_state.dirty_flags = (this->pipeline_state.dirty_flags &
                                        ~MTL_PIPELINE_STATE_SCISSOR_FLAG);
  }

  /* State: Face winding. */
  if (this->pipeline_state.dirty_flags & MTL_PIPELINE_STATE_FRONT_FACING_FLAG) {
    /* We need to invert the face winding in Metal, to account for the inverted-Y coordinate
     * system. */
    MTLWinding winding = (this->pipeline_state.front_face == GPU_CLOCKWISE) ?
                             MTLWindingClockwise :
                             MTLWindingCounterClockwise;
    [rec setFrontFacingWinding:winding];
    this->pipeline_state.dirty_flags = (this->pipeline_state.dirty_flags &
                                        ~MTL_PIPELINE_STATE_FRONT_FACING_FLAG);
  }

  /* State: cull-mode. */
  if (this->pipeline_state.dirty_flags & MTL_PIPELINE_STATE_CULLMODE_FLAG) {

    MTLCullMode mode = MTLCullModeNone;
    if (this->pipeline_state.culling_enabled) {
      switch (this->pipeline_state.cull_mode) {
        case GPU_CULL_NONE:
          mode = MTLCullModeNone;
          break;
        case GPU_CULL_FRONT:
          mode = MTLCullModeFront;
          break;
        case GPU_CULL_BACK:
          mode = MTLCullModeBack;
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
    }
    [rec setCullMode:mode];
    this->pipeline_state.dirty_flags = (this->pipeline_state.dirty_flags &
                                        ~MTL_PIPELINE_STATE_CULLMODE_FLAG);
  }

  if (G.debug & G_DEBUG_GPU) {
    [rec popDebugGroup];
  }

  /* Pipeline state is now good. */
  return true;
}

/* Encode latest depth-stencil state. */
void MTLContext::ensure_depth_stencil_state(MTLPrimitiveType prim_type)
{
  /* Check if we need to update state. */
  if (!(this->pipeline_state.dirty_flags & MTL_PIPELINE_STATE_DEPTHSTENCIL_FLAG)) {
    return;
  }

  /* Fetch render command encoder. */
  id<MTLRenderCommandEncoder> rec = this->main_command_buffer.get_active_render_command_encoder();
  BLI_assert(rec);

  /* Fetch Render Pass state. */
  MTLRenderPassState &rps = this->main_command_buffer.get_render_pass_state();

  /** Prepare Depth-stencil state based on current global pipeline state. */
  MTLFrameBuffer *fb = this->get_current_framebuffer();
  bool hasDepthTarget = fb->has_depth_attachment();
  bool hasStencilTarget = fb->has_stencil_attachment();

  if (hasDepthTarget || hasStencilTarget) {
    /* Update FrameBuffer State. */
    this->pipeline_state.depth_stencil_state.has_depth_target = hasDepthTarget;
    this->pipeline_state.depth_stencil_state.has_stencil_target = hasStencilTarget;

    /* Check if current MTLContextDepthStencilState maps to an existing state object in
     * the Depth-stencil state cache. */
    id<MTLDepthStencilState> ds_state = nil;
    id<MTLDepthStencilState> *depth_stencil_state_lookup =
        this->depth_stencil_state_cache.lookup_ptr(this->pipeline_state.depth_stencil_state);

    /* If not, populate DepthStencil state descriptor. */
    if (depth_stencil_state_lookup == nullptr) {

      MTLDepthStencilDescriptor *ds_state_desc = [[[MTLDepthStencilDescriptor alloc] init]
          autorelease];

      if (hasDepthTarget) {
        ds_state_desc.depthWriteEnabled =
            this->pipeline_state.depth_stencil_state.depth_write_enable;
        ds_state_desc.depthCompareFunction =
            this->pipeline_state.depth_stencil_state.depth_test_enabled ?
                this->pipeline_state.depth_stencil_state.depth_function :
                MTLCompareFunctionAlways;
      }

      if (hasStencilTarget) {
        ds_state_desc.backFaceStencil.readMask =
            this->pipeline_state.depth_stencil_state.stencil_read_mask;
        ds_state_desc.backFaceStencil.writeMask =
            this->pipeline_state.depth_stencil_state.stencil_write_mask;
        ds_state_desc.backFaceStencil.stencilFailureOperation =
            this->pipeline_state.depth_stencil_state.stencil_op_back_stencil_fail;
        ds_state_desc.backFaceStencil.depthFailureOperation =
            this->pipeline_state.depth_stencil_state.stencil_op_back_depth_fail;
        ds_state_desc.backFaceStencil.depthStencilPassOperation =
            this->pipeline_state.depth_stencil_state.stencil_op_back_depthstencil_pass;
        ds_state_desc.backFaceStencil.stencilCompareFunction =
            (this->pipeline_state.depth_stencil_state.stencil_test_enabled) ?
                this->pipeline_state.depth_stencil_state.stencil_func :
                MTLCompareFunctionAlways;

        ds_state_desc.frontFaceStencil.readMask =
            this->pipeline_state.depth_stencil_state.stencil_read_mask;
        ds_state_desc.frontFaceStencil.writeMask =
            this->pipeline_state.depth_stencil_state.stencil_write_mask;
        ds_state_desc.frontFaceStencil.stencilFailureOperation =
            this->pipeline_state.depth_stencil_state.stencil_op_front_stencil_fail;
        ds_state_desc.frontFaceStencil.depthFailureOperation =
            this->pipeline_state.depth_stencil_state.stencil_op_front_depth_fail;
        ds_state_desc.frontFaceStencil.depthStencilPassOperation =
            this->pipeline_state.depth_stencil_state.stencil_op_front_depthstencil_pass;
        ds_state_desc.frontFaceStencil.stencilCompareFunction =
            (this->pipeline_state.depth_stencil_state.stencil_test_enabled) ?
                this->pipeline_state.depth_stencil_state.stencil_func :
                MTLCompareFunctionAlways;
      }

      /* Bake new DS state. */
      ds_state = [this->device newDepthStencilStateWithDescriptor:ds_state_desc];

      /* Store state in cache. */
      BLI_assert(ds_state != nil);
      this->depth_stencil_state_cache.add_new(this->pipeline_state.depth_stencil_state, ds_state);
    }
    else {
      ds_state = *depth_stencil_state_lookup;
      BLI_assert(ds_state != nil);
    }

    /* Bind Depth Stencil State to render command encoder. */
    BLI_assert(ds_state != nil);
    if (ds_state != nil) {
      if (rps.bound_ds_state != ds_state) {
        [rec setDepthStencilState:ds_state];
        rps.bound_ds_state = ds_state;
      }
    }

    /* Apply dynamic depth-stencil state on encoder. */
    if (hasStencilTarget) {
      uint32_t stencil_ref_value =
          (this->pipeline_state.depth_stencil_state.stencil_test_enabled) ?
              this->pipeline_state.depth_stencil_state.stencil_ref :
              0;
      if (stencil_ref_value != rps.last_used_stencil_ref_value) {
        [rec setStencilReferenceValue:stencil_ref_value];
        rps.last_used_stencil_ref_value = stencil_ref_value;
      }
    }

    if (hasDepthTarget) {
      bool doBias = false;
      switch (prim_type) {
        case MTLPrimitiveTypeTriangle:
        case MTLPrimitiveTypeTriangleStrip:
          doBias = this->pipeline_state.depth_stencil_state.depth_bias_enabled_for_tris;
          break;
        case MTLPrimitiveTypeLine:
        case MTLPrimitiveTypeLineStrip:
          doBias = this->pipeline_state.depth_stencil_state.depth_bias_enabled_for_lines;
          break;
        case MTLPrimitiveTypePoint:
          doBias = this->pipeline_state.depth_stencil_state.depth_bias_enabled_for_points;
          break;
      }
      [rec setDepthBias:(doBias) ? this->pipeline_state.depth_stencil_state.depth_bias : 0
             slopeScale:(doBias) ? this->pipeline_state.depth_stencil_state.depth_slope_scale : 0
                  clamp:0];
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compute dispatch.
 * \{ */

const MTLComputePipelineStateInstance *MTLContext::ensure_compute_pipeline_state()
{
  /* Verify if bound shader is valid and fetch MTLComputePipelineStateInstance. */
  /* Check if an active shader is bound. */
  if (!this->pipeline_state.active_shader) {
    MTL_LOG_WARNING("No Metal shader bound!");
    return nullptr;
  }
  /* Also ensure active shader is valid. */
  if (!this->pipeline_state.active_shader->is_valid()) {
    MTL_LOG_WARNING(
        "Bound active shader is not valid (Missing/invalid implementation for Metal).", );
    return nullptr;
  }
  /* Verify this is a compute shader. */
  MTLShader *active_shader = this->pipeline_state.active_shader;

  /* Set descriptor to default shader constants . */
  MTLComputePipelineStateDescriptor compute_pipeline_descriptor(this->constants_state.values);

  const MTLComputePipelineStateInstance *compute_pso_inst =
      active_shader->bake_compute_pipeline_state(this, compute_pipeline_descriptor);

  if (compute_pso_inst == nullptr || compute_pso_inst->pso == nil) {
    MTL_LOG_WARNING("No valid compute PSO for compute dispatch!", );
    return nullptr;
  }
  return compute_pso_inst;
}

void MTLContext::compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len)
{
  /* Ensure all resources required by upcoming compute submission are correctly bound to avoid
   * out of bounds reads/writes. */
  const MTLComputePipelineStateInstance *pipe_state_inst = this->ensure_compute_pipeline_state();
  if (pipe_state_inst == nullptr) {
    return;
  }

#if MTL_DEBUG_SINGLE_DISPATCH_PER_ENCODER == 1
  GPU_flush();
#endif

  MTLShader *shader = this->pipeline_state.active_shader;

  /* Begin compute encoder. */
  id<MTLComputeCommandEncoder> compute_encoder =
      this->main_command_buffer.ensure_begin_compute_encoder();
  BLI_assert(compute_encoder != nil);

  MTLPushConstantBuf *pc_buf = shader->get_push_constant_buf();
  if (pc_buf) {
    /* Always tag dirty since we have a new encoder for each dispatch. */
    pc_buf->tag_dirty();
  }

  if (G.debug & G_DEBUG_GPU) {
    main_command_buffer.unfold_pending_debug_groups();
    [compute_encoder pushDebugGroup:[NSString stringWithFormat:@"Dispatch(Shader:%s)",
                                                               shader->name_get().c_str()]];
  }

  /* Bind PSO. */
  MTLComputeState &cs = this->main_command_buffer.get_compute_state();
  cs.bind_pso(pipe_state_inst->pso);

  if (G.debug & G_DEBUG_GPU) {
    [compute_encoder pushDebugGroup:@"ApplyState"];
  }

  /** Ensure resource bindings. */
  MTLComputeCommandEncoder comp_rec{compute_encoder};
  ensure_texture_bindings(*this, *shader, comp_rec, cs.compute_bindings, pipe_state_inst->compute);
  ensure_buffer_bindings(*this, *shader, comp_rec, cs.compute_bindings);
  if (pc_buf && pc_buf->is_dirty()) {
    ensure_push_constant(*this, *shader, comp_rec, cs.compute_bindings);
    pc_buf->tag_updated();
  }

  if (G.debug & G_DEBUG_GPU) {
    [compute_encoder popDebugGroup];
  }

  /* Dispatch compute. */
  const MTLComputePipelineStateCommon &compute_state_common =
      this->pipeline_state.active_shader->get_compute_common_state();
  [compute_encoder dispatchThreadgroups:MTLSizeMake(max_ii(groups_x_len, 1),
                                                    max_ii(groups_y_len, 1),
                                                    max_ii(groups_z_len, 1))
                  threadsPerThreadgroup:MTLSizeMake(compute_state_common.threadgroup_x_len,
                                                    compute_state_common.threadgroup_y_len,
                                                    compute_state_common.threadgroup_z_len)];

  if (G.debug & G_DEBUG_GPU) {
    [compute_encoder popDebugGroup];
  }

#if MTL_DEBUG_SINGLE_DISPATCH_PER_ENCODER == 1
  GPU_flush();
#endif
}

void MTLContext::compute_dispatch_indirect(StorageBuf *indirect_buf)
{
#if MTL_DEBUG_SINGLE_DISPATCH_PER_ENCODER == 1
  GPU_flush();
#endif

  MTLShader *shader = this->pipeline_state.active_shader;

  /* Ensure all resources required by upcoming compute submission are correctly bound. */
  const MTLComputePipelineStateInstance *pipe_state_inst = this->ensure_compute_pipeline_state();
  BLI_assert(pipe_state_inst != nullptr);

  /* Begin compute encoder. */
  id<MTLComputeCommandEncoder> compute_encoder =
      this->main_command_buffer.ensure_begin_compute_encoder();
  BLI_assert(compute_encoder != nil);

  MTLPushConstantBuf *pc_buf = shader->get_push_constant_buf();
  if (pc_buf) {
    /* Always tag dirty since we have a new encoder for each dispatch. */
    pc_buf->tag_dirty();
  }

  if (G.debug & G_DEBUG_GPU) {
    main_command_buffer.unfold_pending_debug_groups();
    [compute_encoder pushDebugGroup:[NSString stringWithFormat:@"DispatchIndirect(Shader:%s)",
                                                               shader->name_get().c_str()]];
  }

  /* Bind PSO. */
  MTLComputeState &cs = this->main_command_buffer.get_compute_state();
  cs.bind_pso(pipe_state_inst->pso);

  if (G.debug & G_DEBUG_GPU) {
    [compute_encoder pushDebugGroup:@"ApplyState"];
  }

  /** Ensure resource bindings. */
  MTLComputeCommandEncoder comp_rec{compute_encoder};
  ensure_texture_bindings(*this, *shader, comp_rec, cs.compute_bindings, pipe_state_inst->compute);
  ensure_buffer_bindings(*this, *shader, comp_rec, cs.compute_bindings);
  if (pc_buf && pc_buf->is_dirty()) {
    ensure_push_constant(*this, *shader, comp_rec, cs.compute_bindings);
    pc_buf->tag_updated();
  }

  if (G.debug & G_DEBUG_GPU) {
    [compute_encoder popDebugGroup];
  }

  /* Indirect Dispatch compute. */
  MTLStorageBuf *mtlssbo = static_cast<MTLStorageBuf *>(indirect_buf);
  id<MTLBuffer> mtl_indirect_buf = mtlssbo->get_metal_buffer();
  BLI_assert(mtl_indirect_buf != nil);
  if (mtl_indirect_buf == nil) {
    MTL_LOG_WARNING("Metal Indirect Compute dispatch storage buffer does not exist.");
    if (G.debug & G_DEBUG_GPU) {
      [compute_encoder popDebugGroup];
    }
    return;
  }

  /* Indirect Compute dispatch. */
  const MTLComputePipelineStateCommon &compute_state_common =
      this->pipeline_state.active_shader->get_compute_common_state();
  [compute_encoder
      dispatchThreadgroupsWithIndirectBuffer:mtl_indirect_buf
                        indirectBufferOffset:0
                       threadsPerThreadgroup:MTLSizeMake(compute_state_common.threadgroup_x_len,
                                                         compute_state_common.threadgroup_y_len,
                                                         compute_state_common.threadgroup_z_len)];

  if (G.debug & G_DEBUG_GPU) {
    [compute_encoder popDebugGroup];
  }
#if MTL_DEBUG_SINGLE_DISPATCH_PER_ENCODER == 1
  GPU_flush();
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Visibility buffer control for MTLQueryPool.
 * \{ */

void MTLContext::set_visibility_buffer(gpu::MTLBuffer *buffer)
{
  /* Flag visibility buffer as dirty if the buffer being used for visibility has changed --
   * This is required by the render pass, and we will break the pass if the results destination
   * buffer is modified. */
  if (buffer) {
    visibility_is_dirty_ = (buffer != visibility_buffer_) || visibility_is_dirty_;
    visibility_buffer_ = buffer;
    visibility_buffer_->debug_ensure_used();
  }
  else {
    /* If buffer is null, reset visibility state, mark dirty to break render pass if results are no
     * longer needed. */
    visibility_is_dirty_ = (visibility_buffer_ != nullptr) || visibility_is_dirty_;
    visibility_buffer_ = nullptr;
  }
}

gpu::MTLBuffer *MTLContext::get_visibility_buffer() const
{
  return visibility_buffer_;
}

void MTLContext::clear_visibility_dirty()
{
  visibility_is_dirty_ = false;
}

bool MTLContext::is_visibility_dirty() const
{
  return visibility_is_dirty_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture State Management
 * \{ */

void MTLContext::texture_bind(gpu::MTLTexture *mtl_texture, uint texture_unit, bool is_image)
{
  BLI_assert(this);
  BLI_assert(mtl_texture);

  if (texture_unit < 0 || texture_unit >= GPU_max_textures() ||
      texture_unit >= MTL_MAX_TEXTURE_SLOTS)
  {
    MTL_LOG_ERROR("Attempting to bind texture '%s' to invalid texture unit %d",
                  mtl_texture->get_name(),
                  texture_unit);
    BLI_assert(false);
    return;
  }

  MTLTextureBinding &resource_bind = (is_image) ?
                                         this->pipeline_state.image_bindings[texture_unit] :
                                         this->pipeline_state.texture_bindings[texture_unit];

  /* Bind new texture. */
  resource_bind.texture_resource = mtl_texture;
}

void MTLContext::sampler_bind(MTLSamplerState sampler_state, uint sampler_unit)
{
  BLI_assert(this);
  if (sampler_unit < 0 || sampler_unit >= GPU_max_textures() ||
      sampler_unit >= MTL_MAX_SAMPLER_SLOTS)
  {
    MTL_LOG_ERROR("Attempting to bind sampler to invalid sampler unit %d", sampler_unit);
    BLI_assert(false);
    return;
  }

  /* Apply binding. */
  this->pipeline_state.sampler_bindings[sampler_unit] = {sampler_state};
}

void MTLContext::texture_unbind(gpu::MTLTexture *mtl_texture,
                                bool is_image,
                                StateManager *state_manager)
{
  BLI_assert(mtl_texture);

  if (is_image) {
    /* Iterate through textures in state and unbind. */
    int i = 0;
    for (auto &resource_bind : this->pipeline_state.image_bindings) {
      if (resource_bind.texture_resource == mtl_texture) {
        resource_bind.texture_resource = nullptr;
        state_manager->image_formats[i] = TextureWriteFormat::Invalid;
      }
      i++;
    }
  }
  else {
    /* Iterate through textures in state and unbind. */
    for (auto &resource_bind : this->pipeline_state.texture_bindings) {
      if (resource_bind.texture_resource == mtl_texture) {
        resource_bind.texture_resource = nullptr;
      }
    }
  }
}

void MTLContext::texture_unbind_all(bool is_image)
{
  /* Iterate through context's bound textures. */
  if (is_image) {
    int i = 0;
    for (auto &resource_bind : this->pipeline_state.image_bindings) {
      if (resource_bind.texture_resource) {
        resource_bind.texture_resource = nullptr;
        state_manager->image_formats[i] = TextureWriteFormat::Invalid;
      }
      i++;
    }
  }
  else {
    for (auto &resource_bind : this->pipeline_state.texture_bindings) {
      if (resource_bind.texture_resource) {
        resource_bind.texture_resource = nullptr;
      }
    }
  }
}

id<MTLSamplerState> MTLContext::get_sampler_from_state(MTLSamplerState sampler_state)
{
  /* Internal sampler states are signal values and do not correspond to actual samplers. */
  BLI_assert(sampler_state.state.type != GPU_SAMPLER_STATE_TYPE_INTERNAL);

  if (sampler_state.state.type == GPU_SAMPLER_STATE_TYPE_CUSTOM) {
    return custom_sampler_state_cache_[sampler_state.state.custom_type];
  }

  return sampler_state_cache_[sampler_state.state.extend_yz][sampler_state.state.extend_x]
                             [sampler_state.state.filtering];
}

/** A function that maps GPUSamplerExtendMode values to their Metal enum counterparts. */
static inline MTLSamplerAddressMode to_mtl_type(GPUSamplerExtendMode wrap_mode)
{
  switch (wrap_mode) {
    case GPU_SAMPLER_EXTEND_MODE_EXTEND:
      return MTLSamplerAddressModeClampToEdge;
    case GPU_SAMPLER_EXTEND_MODE_REPEAT:
      return MTLSamplerAddressModeRepeat;
    case GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT:
      return MTLSamplerAddressModeMirrorRepeat;
    case GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER:
      return MTLSamplerAddressModeClampToBorderColor;
    default:
      BLI_assert_unreachable();
      return MTLSamplerAddressModeClampToEdge;
  }
}

void MTLContext::sampler_state_cache_init()
{
  for (int extend_yz_i = 0; extend_yz_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_yz_i++) {
    const GPUSamplerExtendMode extend_yz = static_cast<GPUSamplerExtendMode>(extend_yz_i);
    const MTLSamplerAddressMode extend_t = to_mtl_type(extend_yz);

    for (int extend_x_i = 0; extend_x_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_x_i++) {
      const GPUSamplerExtendMode extend_x = static_cast<GPUSamplerExtendMode>(extend_x_i);
      const MTLSamplerAddressMode extend_s = to_mtl_type(extend_x);

      for (int filtering_i = 0; filtering_i < GPU_SAMPLER_FILTERING_TYPES_COUNT; filtering_i++) {
        const GPUSamplerFiltering filtering = GPUSamplerFiltering(filtering_i);

        MTLSamplerDescriptor *descriptor = [[MTLSamplerDescriptor alloc] init];
        descriptor.normalizedCoordinates = true;
        descriptor.sAddressMode = extend_s;
        descriptor.tAddressMode = extend_t;
        descriptor.rAddressMode = extend_t;
        descriptor.borderColor = MTLSamplerBorderColorTransparentBlack;
        descriptor.minFilter = (filtering & GPU_SAMPLER_FILTERING_LINEAR) ?
                                   MTLSamplerMinMagFilterLinear :
                                   MTLSamplerMinMagFilterNearest;
        descriptor.magFilter = (filtering & GPU_SAMPLER_FILTERING_LINEAR) ?
                                   MTLSamplerMinMagFilterLinear :
                                   MTLSamplerMinMagFilterNearest;
        descriptor.mipFilter = (filtering & GPU_SAMPLER_FILTERING_MIPMAP) ?
                                   MTLSamplerMipFilterLinear :
                                   MTLSamplerMipFilterNotMipmapped;
        descriptor.lodMinClamp = -1000;
        descriptor.lodMaxClamp = 1000;
        float aniso_filter = max_ff(16, U.anisotropic_filter);
        descriptor.maxAnisotropy = (filtering & GPU_SAMPLER_FILTERING_MIPMAP) ? aniso_filter : 1;
        descriptor.compareFunction = MTLCompareFunctionAlways;
        descriptor.supportArgumentBuffers = true;

        id<MTLSamplerState> state = [this->device newSamplerStateWithDescriptor:descriptor];
        sampler_state_cache_[extend_yz_i][extend_x_i][filtering_i] = state;

        BLI_assert(state != nil);
        [descriptor autorelease];
      }
    }
  }

  /* Compare sampler for depth textures. */
  {
    MTLSamplerDescriptor *descriptor = [[MTLSamplerDescriptor alloc] init];
    descriptor.minFilter = MTLSamplerMinMagFilterLinear;
    descriptor.magFilter = MTLSamplerMinMagFilterLinear;
    descriptor.compareFunction = MTLCompareFunctionLessEqual;
    descriptor.lodMinClamp = -1000;
    descriptor.lodMaxClamp = 1000;
    descriptor.supportArgumentBuffers = true;

    id<MTLSamplerState> compare_state = [this->device newSamplerStateWithDescriptor:descriptor];
    custom_sampler_state_cache_[GPU_SAMPLER_CUSTOM_COMPARE] = compare_state;

    BLI_assert(compare_state != nil);
    [descriptor autorelease];
  }

  /* Custom sampler for icons. The icon texture is sampled within the shader using a -0.5f LOD
   * bias. */
  {
    MTLSamplerDescriptor *descriptor = [[MTLSamplerDescriptor alloc] init];
    descriptor.minFilter = MTLSamplerMinMagFilterLinear;
    descriptor.magFilter = MTLSamplerMinMagFilterLinear;
    descriptor.mipFilter = MTLSamplerMipFilterNearest;
    descriptor.lodMinClamp = 0;
    descriptor.lodMaxClamp = 1;

    id<MTLSamplerState> icon_state = [this->device newSamplerStateWithDescriptor:descriptor];
    custom_sampler_state_cache_[GPU_SAMPLER_CUSTOM_ICON] = icon_state;

    BLI_assert(icon_state != nil);
    [descriptor autorelease];
  }
}

id<MTLSamplerState> MTLContext::get_default_sampler_state()
{
  if (default_sampler_state_ == nil) {
    default_sampler_state_ = this->get_sampler_from_state({GPUSamplerState::default_sampler()});
  }
  return default_sampler_state_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compute Utils Implementation
 * \{ */

id<MTLComputePipelineState> MTLContextComputeUtils::get_buffer_clear_pso()
{
  if (buffer_clear_pso_ != nil) {
    return buffer_clear_pso_;
  }

  /* Fetch active context. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);

  @autoreleasepool {
    /* Source as NSString. */
    const char *src =
        "\
    struct BufferClearParams {\
      uint clear_value;\
    };\
    kernel void compute_buffer_clear(constant BufferClearParams &params [[buffer(0)]],\
                                     device uint32_t* output_data [[buffer(1)]],\
                                     uint position [[thread_position_in_grid]])\
    {\
      output_data[position] = params.clear_value;\
    }";
    NSString *compute_buffer_clear_src = [NSString stringWithUTF8String:src];

    /* Prepare shader library for buffer clearing. */
    MTLCompileOptions *options = [[[MTLCompileOptions alloc] init] autorelease];
    options.languageVersion = MTLLanguageVersion2_2;

    NSError *error = nullptr;
    id<MTLLibrary> temp_lib = [[ctx->device newLibraryWithSource:compute_buffer_clear_src
                                                         options:options
                                                           error:&error] autorelease];
    if (error) {
      /* Only exit out if genuine error and not warning. */
      if ([[error localizedDescription] rangeOfString:@"Compilation succeeded"].location ==
          NSNotFound)
      {
        NSLog(@"Compile Error - Metal Shader Library error %@ ", error);
        BLI_assert(false);
        return nil;
      }
    }

    /* Fetch compute function. */
    BLI_assert(temp_lib != nil);
    id<MTLFunction> temp_compute_function = [[temp_lib newFunctionWithName:@"compute_buffer_clear"]
        autorelease];
    BLI_assert(temp_compute_function);

    /* Compile compute PSO */
    buffer_clear_pso_ = [ctx->device newComputePipelineStateWithFunction:temp_compute_function
                                                                   error:&error];
    if (error || buffer_clear_pso_ == nil) {
      NSLog(@"Failed to prepare compute_buffer_clear MTLComputePipelineState %@", error);
      BLI_assert(false);
      return nil;
    }

    [buffer_clear_pso_ retain];
  }

  BLI_assert(buffer_clear_pso_ != nil);
  return buffer_clear_pso_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Swap-chain management and Metal presentation.
 * \{ */

void present(MTLRenderPassDescriptor *blit_descriptor,
             id<MTLRenderPipelineState> blit_pso,
             id<MTLTexture> swapchain_texture,
             id<CAMetalDrawable> drawable)
{

  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx);

  /* Flush any outstanding work. */
  ctx->flush();

  /* Always pace CPU to maximum of 3 drawables in flight.
   * nextDrawable may have more in flight if backing swapchain
   * textures are re-allocate, such as during resize events.
   *
   * Determine frames in flight based on current latency. If
   * we are in a high-latency situation, limit frames in flight
   * to increase app responsiveness and keep GPU execution under control.
   * If latency improves, increase frames in flight to improve overall
   * performance. */
  int perf_max_drawables = MTL_MAX_DRAWABLES;
  if (MTLContext::avg_drawable_latency_us > 150000) {
    perf_max_drawables = 1;
  }
  else if (MTLContext::avg_drawable_latency_us > 75000) {
    perf_max_drawables = 2;
  }

  while (MTLContext::max_drawables_in_flight > min_ii(perf_max_drawables, MTL_MAX_DRAWABLES)) {
    BLI_time_sleep_ms(1);
  }

  /* Present is submitted in its own CMD Buffer to ensure drawable reference released as early as
   * possible. This command buffer is separate as it does not utilize the global state
   * for rendering as the main context does. */
  id<MTLCommandBuffer> cmdbuf = [ctx->queue commandBuffer];
  ctx->main_command_buffer.inc_active_command_buffer_count();

  /* Do Present Call and final Blit to MTLDrawable. */
  id<MTLRenderCommandEncoder> enc = [cmdbuf renderCommandEncoderWithDescriptor:blit_descriptor];
  [enc setRenderPipelineState:blit_pso];
  [enc setFragmentTexture:swapchain_texture atIndex:0];
  [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
  [enc endEncoding];

  /* Present drawable. */
  BLI_assert(drawable);
  [cmdbuf presentDrawable:drawable];

  /* Ensure freed buffers have usage tracked against active CommandBuffer submissions. */
  MTLSafeFreeList *cmd_free_buffer_list =
      MTLContext::get_global_memory_manager()->get_current_safe_list();
  BLI_assert(cmd_free_buffer_list);

  /* Increment drawables in flight limiter. */
  MTLContext::max_drawables_in_flight++;
  std::chrono::time_point submission_time = std::chrono::high_resolution_clock::now();

  /* Increment free pool reference and decrement upon command buffer completion. */
  cmd_free_buffer_list->increment_reference();
  [cmdbuf addCompletedHandler:^(id<MTLCommandBuffer> /*cb*/) {
    /* Flag freed buffers associated with this CMD buffer as ready to be freed. */
    cmd_free_buffer_list->decrement_reference();

    /* Decrement count */
    ctx->main_command_buffer.dec_active_command_buffer_count();

    MTL_LOG_DEBUG("Active command buffers: %d",
                  int(MTLCommandBufferManager::num_active_cmd_bufs_in_system));

    /* Drawable count and latency management. */
    MTLContext::max_drawables_in_flight--;
    std::chrono::time_point completion_time = std::chrono::high_resolution_clock::now();
    int64_t microseconds_per_frame = std::chrono::duration_cast<std::chrono::microseconds>(
                                         completion_time - submission_time)
                                         .count();
    MTLContext::latency_resolve_average(microseconds_per_frame);

    MTL_LOG_DEBUG("Frame Latency: %f ms  (Rolling avg: %f ms Drawables: %d)",
                  ((float)microseconds_per_frame) / 1000.0f,
                  ((float)MTLContext::avg_drawable_latency_us) / 1000.0f,
                  perf_max_drawables);
  }];

  [cmdbuf commit];

  /* When debugging, fetch advanced command buffer errors. */
  if (G.debug & G_DEBUG_GPU) {
    [cmdbuf waitUntilCompleted];
    NSError *error = [cmdbuf error];
    if (error != nil) {
      NSLog(@"%@", error);
      BLI_assert(false);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR blitting function called from the GHOST Metal XR binding.
 * \{ */

void xr_blit(id<MTLTexture> metal_xr_texture,
             const int ofsx,
             const int ofsy,
             const int width,
             const int height)
{
  gpu::MTLContext *ctx = gpu::MTLContext::get();

  gpu::MTLFrameBuffer *source_framebuffer = ctx->get_current_framebuffer();
  MTLAttachment src_attachment = source_framebuffer->get_color_attachment(0);
  id<MTLTexture> src_texture = src_attachment.texture->get_metal_handle_base();

  MTLOrigin origin = MTLOriginMake(ofsx, ofsy, 0);
  MTLSize size = MTLSizeMake(width, height, 1);

  id<MTLBlitCommandEncoder> blit_encoder = ctx->main_command_buffer.ensure_begin_blit_encoder();
  [blit_encoder copyFromTexture:src_texture
                    sourceSlice:0
                    sourceLevel:0
                   sourceOrigin:origin
                     sourceSize:size
                      toTexture:metal_xr_texture
               destinationSlice:0
               destinationLevel:0
              destinationOrigin:origin];
}

/** \} */

}  // namespace blender::gpu
