/* SPDX-License-Identifier: GPL-2.0-or-later */

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
#include "mtl_shader_interface.hh"
#include "mtl_state.hh"
#include "mtl_storage_buffer.hh"
#include "mtl_uniform_buffer.hh"
#include "mtl_vertex_buffer.hh"

#include "DNA_userdef_types.h"

#include "GPU_capabilities.h"
#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_storage_buffer.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"
#include "GPU_vertex_buffer.h"
#include "intern/gpu_matrix_private.h"

#include "PIL_time.h"

#include <fstream>
#include <string>

using namespace blender;
using namespace blender::gpu;

/* Debug option to bind null buffer for missing UBOs.
 * Enabled by default. TODO: Ensure all required UBO bindings are present. */
#define DEBUG_BIND_NULL_BUFFER_FOR_MISSING_UBO 1

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

  GHOST_ContextCGL *ghost_cgl_ctx = dynamic_cast<GHOST_ContextCGL *>(ghost_ctx);
  if (ghost_cgl_ctx != NULL) {
    default_fbo_mtltexture_ = ghost_cgl_ctx->metalOverlayTexture();

    MTL_LOG_INFO(
        "Binding GHOST context CGL %p to GPU context %p. (Device: %p, queue: %p, texture: %p)\n",
        ghost_cgl_ctx,
        this,
        this->device,
        this->queue,
        default_fbo_gputexture_);

    /* Check if the GHOST Context provides a default framebuffer: */
    if (default_fbo_mtltexture_) {

      /* Release old GPUTexture handle */
      if (default_fbo_gputexture_) {
        GPU_texture_free(wrap(static_cast<Texture *>(default_fbo_gputexture_)));
        default_fbo_gputexture_ = nullptr;
      }

      /* Retain handle */
      [default_fbo_mtltexture_ retain];

      /*** Create front and back-buffers ***/
      /* Create gpu::MTLTexture objects */
      default_fbo_gputexture_ = new gpu::MTLTexture(
          "MTL_BACKBUFFER", GPU_RGBA16F, GPU_TEXTURE_2D, default_fbo_mtltexture_);

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
        default_fbo_gputexture_ = static_cast<gpu::MTLTexture *>(unwrap(GPU_texture_create_2d(
            __func__, 16, 16, 1, GPU_RGBA16F, GPU_TEXTURE_USAGE_GENERAL, nullptr)));
      }
      mtl_back_left->add_color_attachment(default_fbo_gputexture_, 0, 0, 0);

      MTL_LOG_INFO(
          "-- Bound context %p for GPU context: %p is offscreen and does not have a default "
          "framebuffer\n",
          ghost_cgl_ctx,
          this);
#ifndef NDEBUG
      this->label = @"Offscreen Metal Context";
#endif
    }
  }
  else {
    MTL_LOG_INFO(
        "[ERROR] Failed to bind GHOST context to MTLContext -- GHOST_ContextCGL is null "
        "(GhostContext: %p, GhostContext_CGL: %p)\n",
        ghost_ctx,
        ghost_cgl_ctx);
    BLI_assert(false);
  }
}

void MTLContext::set_ghost_window(GHOST_WindowHandle ghostWinHandle)
{
  GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghostWinHandle);
  this->set_ghost_context((GHOST_ContextHandle)(ghostWin ? ghostWin->getContext() : NULL));
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

  /* Initialize IMM and pipeline state */
  this->pipeline_state.initialised = false;

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
  if (ghost_window_ && ghost_context == NULL) {
    /* NOTE(Metal): Fetch ghost_context from ghost_window if it is not provided.
     * Regardless of whether windowed or not, we need access to the GhostContext
     * for presentation, and device/queue access. */
    GHOST_Window *ghostWin = reinterpret_cast<GHOST_Window *>(ghost_window_);
    ghost_context = (ghostWin ? ghostWin->getContext() : NULL);
  }
  BLI_assert(ghost_context);
  this->ghost_context_ = static_cast<GHOST_ContextCGL *>(ghost_context);
  this->queue = (id<MTLCommandQueue>)this->ghost_context_->metalCommandQueue();
  this->device = (id<MTLDevice>)this->ghost_context_->metalDevice();
  BLI_assert(this->queue);
  BLI_assert(this->device);
  [this->queue retain];
  [this->device retain];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-method-access"
  /* Enable increased concurrent shader compiler limit.
   * Note: Disable warning for missing method when building on older OS's, as compiled code will
   * still work correctly when run on a system with the API available. */
  if (@available(macOS 13.3, *)) {
    [this->device setShouldMaximizeConcurrentCompilation:YES];
  }
#pragma clang diagnostic pop

  /* Register present callback. */
  this->ghost_context_->metalRegisterPresentCallback(&present);

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

  /* Initialize Metal modules. */
  this->memory_manager.init();
  this->state_manager = new MTLStateManager(this);
  this->imm = new MTLImmediate(this);

  /* Ensure global memory manager is initialized. */
  MTLContext::global_memory_manager_acquire_ref();
  MTLContext::get_global_memory_manager()->init(this->device);

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
  BLI_assert(this == reinterpret_cast<MTLContext *>(GPU_context_active_get()));
  /* Ensure rendering is complete command encoders/command buffers are freed. */
  if (MTLBackend::get()->is_inside_render_boundary()) {
    this->finish();

    /* End frame. */
    if (this->get_inside_frame()) {
      this->end_frame();
    }
  }

  /* Release context textures. */
  if (default_fbo_gputexture_) {
    GPU_texture_free(wrap(static_cast<Texture *>(default_fbo_gputexture_)));
    default_fbo_gputexture_ = nullptr;
  }
  if (default_fbo_mtltexture_) {
    [default_fbo_mtltexture_ release];
    default_fbo_mtltexture_ = nil;
  }

  /* Release Memory Manager */
  this->get_scratchbuffer_manager().free();

  /* Release update/blit shaders. */
  this->get_texture_utils().cleanup();

  /* Detach resource references. */
  GPU_texture_unbind_all();

  /* Unbind UBOs. */
  for (int i = 0; i < MTL_MAX_BUFFER_BINDINGS; i++) {
    if (this->pipeline_state.ubo_bindings[i].bound &&
        this->pipeline_state.ubo_bindings[i].ubo != nullptr)
    {
      GPUUniformBuf *ubo = wrap(
          static_cast<UniformBuf *>(this->pipeline_state.ubo_bindings[i].ubo));
      GPU_uniformbuf_unbind(ubo);
    }
  }

  /* Unbind SSBOs. */
  for (int i = 0; i < MTL_MAX_BUFFER_BINDINGS; i++) {
    if (this->pipeline_state.ssbo_bindings[i].bound &&
        this->pipeline_state.ssbo_bindings[i].ssbo != nullptr)
    {
      this->pipeline_state.ssbo_bindings[i].ssbo->unbind();
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
  for (auto entry : cached_sampler_buffers_.values()) {
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
}

void MTLContext::check_error(const char *info)
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
  for (int i = 0; i < MTL_MAX_BUFFER_BINDINGS; i++) {
    if (this->pipeline_state.ubo_bindings[i].bound &&
        this->pipeline_state.ubo_bindings[i].ubo != nullptr)
    {
      this->pipeline_state.ubo_bindings[i].bound = false;
      this->pipeline_state.ubo_bindings[i].ubo = nullptr;
    }
  }

  /* Reset SSBO bind state. */
  for (int i = 0; i < MTL_MAX_BUFFER_BINDINGS; i++) {
    if (this->pipeline_state.ssbo_bindings[i].bound &&
        this->pipeline_state.ssbo_bindings[i].ssbo != nullptr)
    {
      this->pipeline_state.ssbo_bindings[i].bound = false;
      this->pipeline_state.ssbo_bindings[i].ssbo = nullptr;
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

void MTLContext::memory_statistics_get(int *total_mem, int *free_mem)
{
  /* TODO(Metal): Implement. */
  *total_mem = 0;
  *free_mem = 0;
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
      MTL_LOG_WARNING("Framebuffer validation failed, falling back to default framebuffer\n");
      this->framebuffer_restore();

      if (!static_cast<MTLFrameBuffer *>(this->active_fb)->validate_render_pass()) {
        MTL_LOG_ERROR("CRITICAL: DEFAULT FRAMEBUFFER FAIL VALIDATION!!\n");
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

gpu::MTLTexture *MTLContext::get_dummy_texture(eGPUTextureType type,
                                               eGPUSamplerFormat sampler_format)
{
  /* Decrement 1 from texture type as they start from 1 and go to 32 (inclusive). Remap to 0..31 */
  gpu::MTLTexture *dummy_tex = dummy_textures_[sampler_format][type - 1];
  if (dummy_tex != nullptr) {
    return dummy_tex;
  }
  else {
    /* Determine format for dummy texture. */
    eGPUTextureFormat format = GPU_RGBA8;
    switch (sampler_format) {
      case GPU_SAMPLER_TYPE_FLOAT:
        format = GPU_RGBA8;
        break;
      case GPU_SAMPLER_TYPE_INT:
        format = GPU_RGBA8I;
        break;
      case GPU_SAMPLER_TYPE_UINT:
        format = GPU_RGBA8UI;
        break;
      case GPU_SAMPLER_TYPE_DEPTH:
        format = GPU_DEPTH32F_STENCIL8;
        break;
      default:
        BLI_assert_unreachable();
    }

    /* Create dummy texture based on desired type. */
    GPUTexture *tex = nullptr;
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

          GPUVertCompType comp_type = GPU_COMP_F32;
          GPUVertFetchMode fetch_mode = GPU_FETCH_FLOAT;

          switch (sampler_format) {
            case GPU_SAMPLER_TYPE_FLOAT:
            case GPU_SAMPLER_TYPE_DEPTH:
              comp_type = GPU_COMP_F32;
              fetch_mode = GPU_FETCH_FLOAT;
              break;
            case GPU_SAMPLER_TYPE_INT:
              comp_type = GPU_COMP_I32;
              fetch_mode = GPU_FETCH_INT;
              break;
            case GPU_SAMPLER_TYPE_UINT:
              comp_type = GPU_COMP_U32;
              fetch_mode = GPU_FETCH_INT;
              break;
            default:
              BLI_assert_unreachable();
          }

          GPU_vertformat_attr_add(
              &dummy_vertformat_[sampler_format], "dummy", comp_type, 4, fetch_mode);
          dummy_verts_[sampler_format] = GPU_vertbuf_create_with_format_ex(
              &dummy_vertformat_[sampler_format],
              GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
          GPU_vertbuf_data_alloc(dummy_verts_[sampler_format], 64);
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
  return nullptr;
}

void MTLContext::free_dummy_resources()
{
  for (int format = 0; format < GPU_SAMPLER_TYPE_MAX; format++) {
    for (int tex = 0; tex < GPU_TEXTURE_BUFFER; tex++) {
      if (dummy_textures_[format][tex]) {
        GPU_texture_free(
            reinterpret_cast<GPUTexture *>(static_cast<Texture *>(dummy_textures_[format][tex])));
        dummy_textures_[format][tex] = nullptr;
      }
    }
    if (dummy_verts_[format]) {
      GPU_vertbuf_discard(dummy_verts_[format]);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Global Context State
 * \{ */

/* Metal Context Pipeline State. */
void MTLContext::pipeline_state_init()
{
  /*** Initialize state only once. ***/
  if (!this->pipeline_state.initialised) {
    this->pipeline_state.initialised = true;
    this->pipeline_state.active_shader = nullptr;

    /* Clear bindings state. */
    for (int t = 0; t < GPU_max_textures(); t++) {
      /* Textures. */
      this->pipeline_state.texture_bindings[t].used = false;
      this->pipeline_state.texture_bindings[t].slot_index = -1;
      this->pipeline_state.texture_bindings[t].texture_resource = nullptr;

      /* Images. */
      this->pipeline_state.image_bindings[t].used = false;
      this->pipeline_state.image_bindings[t].slot_index = -1;
      this->pipeline_state.image_bindings[t].texture_resource = nullptr;
    }
    for (int s = 0; s < MTL_MAX_SAMPLER_SLOTS; s++) {
      this->pipeline_state.sampler_bindings[s].used = false;
    }
    for (int u = 0; u < MTL_MAX_BUFFER_BINDINGS; u++) {
      this->pipeline_state.ubo_bindings[u].bound = false;
      this->pipeline_state.ubo_bindings[u].ubo = nullptr;
    }
    for (int u = 0; u < MTL_MAX_BUFFER_BINDINGS; u++) {
      this->pipeline_state.ssbo_bindings[u].bound = false;
      this->pipeline_state.ssbo_bindings[u].ssbo = nullptr;
    }
  }

  /*** State defaults -- restored by GPU_state_init. ***/
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
  this->pipeline_state.viewport_offset_x = 0;
  this->pipeline_state.viewport_offset_y = 0;
  this->pipeline_state.viewport_width = 0;
  this->pipeline_state.viewport_height = 0;
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
  bool changed = (this->pipeline_state.viewport_offset_x != origin_x) ||
                 (this->pipeline_state.viewport_offset_y != origin_y) ||
                 (this->pipeline_state.viewport_width != width) ||
                 (this->pipeline_state.viewport_height != height);
  this->pipeline_state.viewport_offset_x = origin_x;
  this->pipeline_state.viewport_offset_y = origin_y;
  this->pipeline_state.viewport_width = width;
  this->pipeline_state.viewport_height = height;
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
  BLI_assert(this->pipeline_state.initialised);

  /* Check if an active shader is bound. */
  if (!this->pipeline_state.active_shader) {
    MTL_LOG_WARNING("No Metal shader for bound GL shader\n");
    return false;
  }

  /* Also ensure active shader is valid. */
  if (!this->pipeline_state.active_shader->is_valid()) {
    MTL_LOG_WARNING(
        "Bound active shader is not valid (Missing/invalid implementation for Metal).\n", );
    return false;
  }

  /* Apply global state. */
  this->state_manager->apply_state();

  /* Main command buffer tracks the current state of the render pass, based on bound
   * MTLFrameBuffer. */
  MTLRenderPassState &rps = this->main_command_buffer.get_render_pass_state();

  /* Debug Check: Ensure Framebuffer instance is not dirty. */
  BLI_assert(!this->main_command_buffer.get_active_framebuffer()->get_dirty());

  /* Fetch shader interface. */
  MTLShaderInterface *shader_interface = this->pipeline_state.active_shader->get_interface();
  if (shader_interface == nullptr) {
    MTL_LOG_WARNING("Bound active shader does not have a valid shader interface!\n", );
    return false;
  }

  /* Fetch shader and bake valid PipelineStateObject (PSO) based on current
   * shader and state combination. This PSO represents the final GPU-executable
   * permutation of the shader. */
  MTLRenderPipelineStateInstance *pipeline_state_instance =
      this->pipeline_state.active_shader->bake_current_pipeline_state(
          this, mtl_prim_type_to_topology_class(mtl_prim_type));
  if (!pipeline_state_instance) {
    MTL_LOG_ERROR("Failed to bake Metal pipeline state for shader: %s\n",
                  shader_interface->get_name());
    return false;
  }

  bool result = false;
  if (pipeline_state_instance->pso) {

    /* Fetch render command encoder. A render pass should already be active.
     * This will be NULL if invalid. */
    id<MTLRenderCommandEncoder> rec =
        this->main_command_buffer.get_active_render_command_encoder();
    BLI_assert(rec);
    if (rec == nil) {
      MTL_LOG_ERROR("ensure_render_pipeline_state called while render pass is not active.\n");
      return false;
    }

    /* Bind Render Pipeline State. */
    BLI_assert(pipeline_state_instance->pso);
    if (rps.bound_pso != pipeline_state_instance->pso) {
      [rec setRenderPipelineState:pipeline_state_instance->pso];
      rps.bound_pso = pipeline_state_instance->pso;
    }

    /** Ensure resource bindings. */
    /* Texture Bindings. */
    /* We will iterate through all texture bindings on the context and determine if any of the
     * active slots match those in our shader interface. If so, textures will be bound. */
    if (shader_interface->get_total_textures() > 0) {
      this->ensure_texture_bindings(rec, shader_interface, pipeline_state_instance);
    }

    /* Transform feedback buffer binding. */
    GPUVertBuf *tf_vbo =
        this->pipeline_state.active_shader->get_transform_feedback_active_buffer();
    if (tf_vbo != nullptr && pipeline_state_instance->transform_feedback_buffer_index >= 0) {

      /* Ensure primitive type is either GPU_LINES, GPU_TRIANGLES or GPU_POINT */
      BLI_assert(mtl_prim_type == MTLPrimitiveTypeLine ||
                 mtl_prim_type == MTLPrimitiveTypeTriangle ||
                 mtl_prim_type == MTLPrimitiveTypePoint);

      /* Fetch active transform feedback buffer from vertbuf */
      MTLVertBuf *tf_vbo_mtl = static_cast<MTLVertBuf *>(reinterpret_cast<VertBuf *>(tf_vbo));
      /* Ensure TF buffer is ready. */
      tf_vbo_mtl->bind();
      id<MTLBuffer> tf_buffer_mtl = tf_vbo_mtl->get_metal_buffer();
      BLI_assert(tf_buffer_mtl != nil);

      if (tf_buffer_mtl != nil) {
        [rec setVertexBuffer:tf_buffer_mtl
                      offset:0
                     atIndex:pipeline_state_instance->transform_feedback_buffer_index];
        MTL_LOG_INFO("Successfully bound VBO: %p for transform feedback (MTL Buffer: %p)\n",
                     tf_vbo_mtl,
                     tf_buffer_mtl);
      }
    }

    /* Matrix Bindings. */
    /* This is now called upon shader bind. We may need to re-evaluate this though,
     * as was done here to ensure uniform changes between draws were tracked.
     * NOTE(Metal): We may be able to remove this. */
    GPU_matrix_bind(reinterpret_cast<struct GPUShader *>(
        static_cast<Shader *>(this->pipeline_state.active_shader)));

    /* Bind Uniforms */
    this->ensure_uniform_buffer_bindings(rec, shader_interface, pipeline_state_instance);

    /* Bind Null attribute buffer, if needed. */
    if (pipeline_state_instance->null_attribute_buffer_index >= 0) {
      if (G.debug & G_DEBUG_GPU) {
        MTL_LOG_INFO("Binding null attribute buffer at index: %d\n",
                     pipeline_state_instance->null_attribute_buffer_index);
      }
      rps.bind_vertex_buffer(this->get_null_attribute_buffer(),
                             0,
                             pipeline_state_instance->null_attribute_buffer_index);
    }

    /** Dynamic Per-draw Render State on RenderCommandEncoder. */
    /* State: Viewport. */
    if (this->pipeline_state.dirty_flags & MTL_PIPELINE_STATE_VIEWPORT_FLAG) {
      MTLViewport viewport;
      viewport.originX = (double)this->pipeline_state.viewport_offset_x;
      viewport.originY = (double)this->pipeline_state.viewport_offset_y;
      viewport.width = (double)this->pipeline_state.viewport_width;
      viewport.height = (double)this->pipeline_state.viewport_height;
      viewport.znear = this->pipeline_state.depth_stencil_state.depth_range_near;
      viewport.zfar = this->pipeline_state.depth_stencil_state.depth_range_far;
      [rec setViewport:viewport];

      this->pipeline_state.dirty_flags = (this->pipeline_state.dirty_flags &
                                          ~MTL_PIPELINE_STATE_VIEWPORT_FLAG);
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
        scissor.height = (uint)min_ii(
            scissor.height, max_ii(render_fb->get_default_height() - (int)(scissor.y), 0));
        BLI_assert(scissor.width > 0 &&
                   (scissor.x + scissor.width <= render_fb->get_default_width()));
        BLI_assert(scissor.height > 0 && (scissor.height <= render_fb->get_default_height()));
      }
      else {
        /* Scissor is disabled, reset to default size as scissor state may have been previously
         * assigned on this encoder. */
        scissor.x = 0;
        scissor.y = 0;
        scissor.width = render_fb->get_default_width();
        scissor.height = render_fb->get_default_height();
      }

      /* Scissor state can still be flagged as changed if it is toggled on and off, without
       * parameters changing between draws. */
      if (memcmp(&scissor, &rps.last_scissor_rect, sizeof(MTLScissorRect))) {
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

    /* Pipeline state is now good. */
    result = true;
  }
  return result;
}

/* Bind uniform buffers to an active render command encoder using the rendering state of the
 * current context -> Active shader, Bound UBOs). */
bool MTLContext::ensure_uniform_buffer_bindings(
    id<MTLRenderCommandEncoder> rec,
    const MTLShaderInterface *shader_interface,
    const MTLRenderPipelineStateInstance *pipeline_state_instance)
{
  /* Fetch Render Pass state. */
  MTLRenderPassState &rps = this->main_command_buffer.get_render_pass_state();

  /* Shader owned push constant block for uniforms.. */
  bool active_shader_changed = (rps.last_bound_shader_state.shader_ !=
                                    this->pipeline_state.active_shader ||
                                rps.last_bound_shader_state.shader_ == nullptr ||
                                rps.last_bound_shader_state.pso_index_ !=
                                    pipeline_state_instance->shader_pso_index);

  const MTLShaderBufferBlock &push_constant_block = shader_interface->get_push_constant_block();
  if (push_constant_block.size > 0) {

    /* Fetch uniform buffer base binding index from pipeline_state_instance - There buffer index
     * will be offset by the number of bound VBOs. */
    uint32_t block_size = push_constant_block.size;
    uint32_t buffer_index = pipeline_state_instance->base_uniform_buffer_index +
                            push_constant_block.buffer_index;
    BLI_assert(buffer_index >= 0 && buffer_index < MTL_MAX_BUFFER_BINDINGS);

    /* Only need to rebind block if push constants have been modified -- or if no data is bound for
     * the current RenderCommandEncoder. */
    if (this->pipeline_state.active_shader->get_push_constant_is_dirty() ||
        active_shader_changed || !rps.cached_vertex_buffer_bindings[buffer_index].is_bytes ||
        !rps.cached_fragment_buffer_bindings[buffer_index].is_bytes || true)
    {

      /* Bind push constant data. */
      BLI_assert(this->pipeline_state.active_shader->get_push_constant_data() != nullptr);
      rps.bind_vertex_bytes(
          this->pipeline_state.active_shader->get_push_constant_data(), block_size, buffer_index);
      rps.bind_fragment_bytes(
          this->pipeline_state.active_shader->get_push_constant_data(), block_size, buffer_index);

      /* Only need to rebind block if it has been modified. */
      this->pipeline_state.active_shader->push_constant_bindstate_mark_dirty(false);
    }
  }
  rps.last_bound_shader_state.set(this->pipeline_state.active_shader,
                                  pipeline_state_instance->shader_pso_index);

  /* Bind Global GPUUniformBuffers */
  /* Iterate through expected UBOs in the shader interface, and check if the globally bound ones
   * match. This is used to support the gpu_uniformbuffer module, where the uniform data is global,
   * and not owned by the shader instance. */
  for (const uint ubo_index : IndexRange(shader_interface->get_total_uniform_blocks())) {
    const MTLShaderBufferBlock &ubo = shader_interface->get_uniform_block(ubo_index);

    if (ubo.buffer_index >= 0 && ubo.location >= 0) {
      /* Explicit lookup location for UBO in bind table. */
      const uint32_t ubo_location = ubo.location;
      /* buffer(N) index of where to bind the UBO. */
      const uint32_t buffer_index = ubo.buffer_index;
      id<MTLBuffer> ubo_buffer = nil;
      int ubo_size = 0;

      bool bind_dummy_buffer = false;
      if (this->pipeline_state.ubo_bindings[ubo_location].bound) {

        /* Fetch UBO global-binding properties from slot. */
        ubo_buffer = this->pipeline_state.ubo_bindings[ubo_location].ubo->get_metal_buffer();
        ubo_size = this->pipeline_state.ubo_bindings[ubo_location].ubo->get_size();

        /* Use dummy zero buffer if no buffer assigned -- this is an optimization to avoid
         * allocating zero buffers. */
        if (ubo_buffer == nil) {
          bind_dummy_buffer = true;
        }
        else {
          BLI_assert(ubo_buffer != nil);
          BLI_assert(ubo_size > 0);

          if (pipeline_state_instance->reflection_data_available) {
            /* NOTE: While the vertex and fragment stages have different UBOs, the indices in each
             * case will be the same for the same UBO.
             * We also determine expected size and then ensure buffer of the correct size
             * exists in one of the vertex/fragment shader binding tables. This path is used
             * to verify that the size of the bound UBO matches what is expected in the shader. */
            uint32_t expected_size =
                (buffer_index <
                 pipeline_state_instance->buffer_bindings_reflection_data_vert.size()) ?
                    pipeline_state_instance->buffer_bindings_reflection_data_vert[buffer_index]
                        .size :
                    0;
            if (expected_size == 0) {
              expected_size =
                  (buffer_index <
                   pipeline_state_instance->buffer_bindings_reflection_data_frag.size()) ?
                      pipeline_state_instance->buffer_bindings_reflection_data_frag[buffer_index]
                          .size :
                      0;
            }
            BLI_assert_msg(
                expected_size > 0,
                "Shader interface expects UBO, but shader reflection data reports that it "
                "is not present");

            /* If ubo size is smaller than the size expected by the shader, we need to bind the
             * dummy buffer, which will be big enough, to avoid an OOB error. */
            if (ubo_size < expected_size) {
              MTL_LOG_UBO_ERROR(
                  "[Error][UBO] UBO (UBO Name: %s) bound at location: %d (buffer[[%d]]) with size "
                  "%d (Expected size "
                  "%d)  (Shader Name: %s) is too small -- binding NULL buffer. This is likely an "
                  "over-binding, which is not used,  but we need this to avoid validation "
                  "issues\n",
                  shader_interface->get_name_at_offset(ubo.name_offset),
                  ubo_location,
                  pipeline_state_instance->base_uniform_buffer_index + buffer_index,
                  ubo_size,
                  expected_size,
                  shader_interface->get_name());
              bind_dummy_buffer = true;
            }
          }
        }
      }
      else {
        MTL_LOG_UBO_ERROR(
            "[Warning][UBO] Shader '%s' expected UBO '%s' to be bound at buffer slot: %d "
            "(buffer[[%d]])-- but "
            "nothing was bound -- binding dummy buffer\n",
            shader_interface->get_name(),
            shader_interface->get_name_at_offset(ubo.name_offset),
            ubo_location,
            pipeline_state_instance->base_uniform_buffer_index + buffer_index);
        bind_dummy_buffer = true;
      }

      if (bind_dummy_buffer) {
        /* Perform Dummy binding. */
        ubo_buffer = this->get_null_buffer();
        ubo_size = [ubo_buffer length];
      }

      if (ubo_buffer != nil) {

        uint32_t buffer_bind_index = pipeline_state_instance->base_uniform_buffer_index +
                                     buffer_index;

        /* Bind Vertex UBO. */
        if (bool(ubo.stage_mask & ShaderStage::VERTEX)) {
          BLI_assert(buffer_bind_index >= 0 && buffer_bind_index < MTL_MAX_BUFFER_BINDINGS);
          rps.bind_vertex_buffer(ubo_buffer, 0, buffer_bind_index);
        }

        /* Bind Fragment UBOs. */
        if (bool(ubo.stage_mask & ShaderStage::FRAGMENT)) {
          BLI_assert(buffer_bind_index >= 0 && buffer_bind_index < MTL_MAX_BUFFER_BINDINGS);
          rps.bind_fragment_buffer(ubo_buffer, 0, buffer_bind_index);
        }
      }
      else {
        MTL_LOG_UBO_ERROR(
            "[UBO] Shader '%s' has UBO '%s' bound at buffer index: %d -- but MTLBuffer "
            "is NULL!\n",
            shader_interface->get_name(),
            shader_interface->get_name_at_offset(ubo.name_offset),
            buffer_index);
      }
    }
  }

  /* Bind Global GPUStorageBuf's */
  /* Iterate through expected SSBOs in the shader interface, and check if the globally bound ones
   * match. This is used to support the gpu_uniformbuffer module, where the uniform data is global,
   * and not owned by the shader instance. */
  for (const uint ssbo_index : IndexRange(shader_interface->get_total_storage_blocks())) {
    const MTLShaderBufferBlock &ssbo = shader_interface->get_storage_block(ssbo_index);

    if (ssbo.buffer_index >= 0 && ssbo.location >= 0) {
      /* Explicit lookup location for SSBO in bind table.*/
      const uint32_t ssbo_location = ssbo.location;
      /* buffer(N) index of where to bind the SSBO. */
      const uint32_t buffer_index = ssbo.buffer_index;
      id<MTLBuffer> ssbo_buffer = nil;
      int ssbo_size = 0;
      UNUSED_VARS_NDEBUG(ssbo_size);

      if (this->pipeline_state.ssbo_bindings[ssbo_location].bound) {

        /* Fetch SSBO global-binding properties from slot. */
        ssbo_buffer = this->pipeline_state.ssbo_bindings[ssbo_location].ssbo->get_metal_buffer();
        ssbo_size = this->pipeline_state.ssbo_bindings[ssbo_location].ssbo->get_size();

        /* For SSBOs, we always need to ensure the buffer exists, as it may be written to. */
        BLI_assert(ssbo_buffer != nil);
        BLI_assert(ssbo_size > 0);
      }
      else {
        MTL_LOG_SSBO_ERROR(
            "[Error][SSBO] Shader '%s' expected SSBO '%s' to be bound at buffer location: %d "
            "(buffer[[%d]]) -- "
            "but "
            "nothing was bound.\n",
            shader_interface->get_name(),
            shader_interface->get_name_at_offset(ssbo.name_offset),
            ssbo_location,
            pipeline_state_instance->base_storage_buffer_index + buffer_index);

#if DEBUG_BIND_NULL_BUFFER_FOR_MISSING_SSBO == 1
        ssbo_buffer = this->get_null_buffer();
        ssbo_size = [ssbo_buffer length];
#endif
      }

      if (ssbo_buffer != nil) {
        uint32_t buffer_bind_index = pipeline_state_instance->base_storage_buffer_index +
                                     buffer_index;

        /* Bind Vertex SSBO. */
        if (bool(ssbo.stage_mask & ShaderStage::VERTEX)) {
          BLI_assert(buffer_bind_index >= 0 && buffer_bind_index < MTL_MAX_BUFFER_BINDINGS);
          rps.bind_vertex_buffer(ssbo_buffer, 0, buffer_bind_index);
        }

        /* Bind Fragment SSBOs. */
        if (bool(ssbo.stage_mask & ShaderStage::FRAGMENT)) {
          BLI_assert(buffer_bind_index >= 0 && buffer_bind_index < MTL_MAX_BUFFER_BINDINGS);
          rps.bind_fragment_buffer(ssbo_buffer, 0, buffer_bind_index);
        }
      }
      else {
        MTL_LOG_SSBO_ERROR(
            "[Error][SSBO] Shader '%s' had SSBO '%s' bound at SSBO location: %d "
            "(buffer[["
            "%d]]) -- but bound MTLStorageBuf was nil.\n",
            shader_interface->get_name(),
            shader_interface->get_name_at_offset(ssbo.name_offset),
            ssbo_location,
            pipeline_state_instance->base_storage_buffer_index + buffer_index);
      }
    }
  }

  return true;
}

/* Variant for compute. Bind uniform buffers to an active compute command encoder using the
 * rendering state of the current context -> Active shader, Bound UBOs). */
bool MTLContext::ensure_uniform_buffer_bindings(
    id<MTLComputeCommandEncoder> rec,
    const MTLShaderInterface *shader_interface,
    const MTLComputePipelineStateInstance &pipeline_state_instance)
{
  /* Fetch Compute Pass state. */
  MTLComputeState &cs = this->main_command_buffer.get_compute_state();

  /* Fetch push constant block and bind. */
  const MTLShaderBufferBlock &push_constant_block = shader_interface->get_push_constant_block();
  if (push_constant_block.size > 0) {

    /* Fetch uniform buffer base binding index from pipeline_state_instance - There buffer index
     * will be offset by the number of bound VBOs. */
    uint32_t block_size = push_constant_block.size;
    uint32_t buffer_index = pipeline_state_instance.base_uniform_buffer_index +
                            push_constant_block.buffer_index;
    BLI_assert(buffer_index >= 0 && buffer_index < MTL_MAX_BUFFER_BINDINGS);

    /* For compute, we must always re-bind the push constant block as other compute
     * operations may have assigned resources over the top, outside of the compiled
     * compute shader path. */
    /* Bind push constant data. */
    BLI_assert(this->pipeline_state.active_shader->get_push_constant_data() != nullptr);
    cs.bind_compute_bytes(
        this->pipeline_state.active_shader->get_push_constant_data(), block_size, buffer_index);

    /* Only need to rebind block if it has been modified. */
    this->pipeline_state.active_shader->push_constant_bindstate_mark_dirty(false);
  }

  /* Bind Global GPUUniformBuffers */
  /* Iterate through expected UBOs in the shader interface, and check if the globally bound ones
   * match. This is used to support the gpu_uniformbuffer module, where the uniform data is global,
   * and not owned by the shader instance. */
  for (const uint ubo_index : IndexRange(shader_interface->get_total_uniform_blocks())) {
    const MTLShaderBufferBlock &ubo = shader_interface->get_uniform_block(ubo_index);

    if (ubo.buffer_index >= 0) {
      /* Explicit lookup location for UBO in bind table. */
      const uint32_t ubo_location = ubo.location;
      /* buffer(N) index of where to bind the UBO. */
      const uint32_t buffer_index = ubo.buffer_index;
      id<MTLBuffer> ubo_buffer = nil;
      int ubo_size = 0;

      bool bind_dummy_buffer = false;
      if (this->pipeline_state.ubo_bindings[ubo_location].bound) {

        /* Fetch UBO global-binding properties from slot. */
        ubo_buffer = this->pipeline_state.ubo_bindings[ubo_location].ubo->get_metal_buffer();
        ubo_size = this->pipeline_state.ubo_bindings[ubo_location].ubo->get_size();
        UNUSED_VARS_NDEBUG(ubo_size);

        /* Use dummy zero buffer if no buffer assigned -- this is an optimization to avoid
         * allocating zero buffers. */
        if (ubo_buffer == nil) {
          bind_dummy_buffer = true;
        }
        else {
          BLI_assert(ubo_buffer != nil);
          BLI_assert(ubo_size > 0);
        }
      }
      else {
        MTL_LOG_UBO_ERROR(
            "[Warning][UBO] Shader '%s' expected UBO '%s' to be bound at buffer location: %d "
            "(buffer[[%d]]) -- but "
            "nothing was bound -- binding dummy buffer\n",
            shader_interface->get_name(),
            shader_interface->get_name_at_offset(ubo.name_offset),
            ubo_location,
            pipeline_state_instance.base_uniform_buffer_index + buffer_index);
        bind_dummy_buffer = true;
      }

      if (bind_dummy_buffer) {
        /* Perform Dummy binding. */
        ubo_buffer = this->get_null_buffer();
        ubo_size = [ubo_buffer length];
      }

      if (ubo_buffer != nil) {
        uint32_t buffer_bind_index = pipeline_state_instance.base_uniform_buffer_index +
                                     buffer_index;

        /* Bind Compute UBO. */
        if (bool(ubo.stage_mask & ShaderStage::COMPUTE)) {
          BLI_assert(buffer_bind_index >= 0 && buffer_bind_index < MTL_MAX_BUFFER_BINDINGS);
          cs.bind_compute_buffer(ubo_buffer, 0, buffer_bind_index);
        }
      }
      else {
        MTL_LOG_UBO_ERROR(
            "[UBO] Compute Shader '%s' has UBO '%s' bound at buffer index: %d -- but MTLBuffer "
            "is NULL!\n",
            shader_interface->get_name(),
            shader_interface->get_name_at_offset(ubo.name_offset),
            buffer_index);
      }
    }
  }

  /* Bind Global GPUStorageBuffers. */
  /* Iterate through expected SSBOs in the shader interface, and check if the globally bound ones
   * match. */
  for (const uint ssbo_index : IndexRange(shader_interface->get_total_storage_blocks())) {
    const MTLShaderBufferBlock &ssbo = shader_interface->get_storage_block(ssbo_index);

    if (ssbo.buffer_index >= 0 && ssbo.location >= 0) {
      /* Explicit lookup location for UBO in bind table.*/
      const uint32_t ssbo_location = ssbo.location;
      /* buffer(N) index of where to bind the UBO. */
      const uint32_t buffer_index = ssbo.buffer_index;
      id<MTLBuffer> ssbo_buffer = nil;
      int ssbo_size = 0;

      if (this->pipeline_state.ssbo_bindings[ssbo_location].bound) {

        /* Fetch UBO global-binding properties from slot. */
        ssbo_buffer = this->pipeline_state.ssbo_bindings[ssbo_location].ssbo->get_metal_buffer();
        ssbo_size = this->pipeline_state.ssbo_bindings[ssbo_location].ssbo->get_size();
        UNUSED_VARS_NDEBUG(ssbo_size);

        /* For SSBOs, we always need to ensure the buffer exists, as it may be written to. */
        BLI_assert(ssbo_buffer != nil);
        BLI_assert(ssbo_size > 0);
      }
      else {
        MTL_LOG_SSBO_ERROR(
            "[Error][SSBO] Shader '%s' expected SSBO '%s' to be bound at SSBO location: %d "
            "(buffer[["
            "%d]]) -- but "
            "nothing was bound.\n",
            shader_interface->get_name(),
            shader_interface->get_name_at_offset(ssbo.name_offset),
            ssbo_location,
            pipeline_state_instance.base_storage_buffer_index + buffer_index);

#if DEBUG_BIND_NULL_BUFFER_FOR_MISSING_SSBO == 1
        ssbo_buffer = this->get_null_buffer();
        ssbo_size = [ssbo_buffer length];
#endif
      }

      if (ssbo_buffer != nil) {
        uint32_t buffer_bind_index = pipeline_state_instance.base_storage_buffer_index +
                                     buffer_index;

        /* Bind Vertex UBO. */
        if (bool(ssbo.stage_mask & ShaderStage::COMPUTE)) {
          BLI_assert(buffer_bind_index >= 0 && buffer_bind_index < MTL_MAX_BUFFER_BINDINGS);
          cs.bind_compute_buffer(ssbo_buffer, 0, buffer_bind_index);
        }
      }
      else {
        MTL_LOG_SSBO_ERROR(
            "[Error][SSBO] Shader '%s' had SSBO '%s' bound at SSBO location: %d "
            "(buffer[["
            "%d]]) -- but bound MTLStorageBuf was nil.\n",
            shader_interface->get_name(),
            shader_interface->get_name_at_offset(ssbo.name_offset),
            ssbo_location,
            pipeline_state_instance.base_storage_buffer_index + buffer_index);
      }
    }
  }

  return true;
}

/* Ensure texture bindings are correct and up to date for current draw call. */
void MTLContext::ensure_texture_bindings(
    id<MTLRenderCommandEncoder> rec,
    MTLShaderInterface *shader_interface,
    const MTLRenderPipelineStateInstance *pipeline_state_instance)
{
  BLI_assert(shader_interface != nil);
  BLI_assert(rec != nil);

  /* Fetch Render Pass state. */
  MTLRenderPassState &rps = this->main_command_buffer.get_render_pass_state();

  @autoreleasepool {
    int vertex_arg_buffer_bind_index = -1;
    int fragment_arg_buffer_bind_index = -1;

    /* Argument buffers are used for samplers, when the limit of 16 is exceeded. */
    bool use_argument_buffer_for_samplers = shader_interface->uses_argument_buffer_for_samplers();
    vertex_arg_buffer_bind_index = shader_interface->get_argument_buffer_bind_index(
        ShaderStage::VERTEX);
    fragment_arg_buffer_bind_index = shader_interface->get_argument_buffer_bind_index(
        ShaderStage::FRAGMENT);

    /* Loop through expected textures in shader interface and resolve bindings with currently
     * bound textures.. */
    for (const uint t : IndexRange(shader_interface->get_max_texture_index() + 1)) {
      /* Ensure the bound texture is compatible with the shader interface. If the
       * shader does not expect a texture to be bound for the current slot, we skip
       * binding.
       * NOTE: Global texture bindings may be left over from prior draw calls. */
      const MTLShaderTexture &shader_texture_info = shader_interface->get_texture(t);
      if (!shader_texture_info.used) {
        /* Skip unused binding points if explicit indices are specified. */
        continue;
      }

      /* Determine bind lookup table depending on whether an image binding or texture.
       * NOTE: Images and Texture Samplers share a binding table in Metal. */
      bool is_resource_sampler = shader_texture_info.is_texture_sampler;
      MTLTextureBinding(&resource_bind_table)[MTL_MAX_TEXTURE_SLOTS] =
          (is_resource_sampler) ? this->pipeline_state.texture_bindings :
                                  this->pipeline_state.image_bindings;

      /* Texture resource bind slot in shader `[[texture(n)]]`. */
      int slot = shader_texture_info.slot_index;
      /* Explicit bind location for texture. */
      int location = shader_texture_info.location;
      /* Default sampler. */
      MTLSamplerBinding default_binding = {true, DEFAULT_SAMPLER_STATE};

      if (slot >= 0 && slot < GPU_max_textures()) {
        bool bind_dummy_texture = true;
        if (resource_bind_table[location].used) {
          gpu::MTLTexture *bound_texture = resource_bind_table[location].texture_resource;
          MTLSamplerBinding &bound_sampler = (is_resource_sampler) ?
                                                 this->pipeline_state.sampler_bindings[location] :
                                                 default_binding;
          BLI_assert(bound_texture);
          BLI_assert(bound_sampler.used);

          if (shader_texture_info.type == bound_texture->type_) {
            /* Bind texture and sampler if the bound texture matches the type expected by the
             * shader. */
            id<MTLTexture> tex = bound_texture->get_metal_handle();

            if (bool(shader_texture_info.stage_mask & ShaderStage::VERTEX)) {
              rps.bind_vertex_texture(tex, slot);
              rps.bind_vertex_sampler(bound_sampler, use_argument_buffer_for_samplers, slot);
            }

            if (bool(shader_texture_info.stage_mask & ShaderStage::FRAGMENT)) {
              rps.bind_fragment_texture(tex, slot);
              rps.bind_fragment_sampler(bound_sampler, use_argument_buffer_for_samplers, slot);
            }

            /* Texture state resolved, no need to bind dummy texture */
            bind_dummy_texture = false;
          }
          else {
            /* Texture type for bound texture (e.g. Texture2DArray) does not match what was
             * expected in the shader interface. This is a problem and we will need to bind
             * a dummy texture to ensure correct API usage. */
            MTL_LOG_WARNING(
                "(Shader '%s') Texture (%s) %p bound to slot %d is incompatible -- Wrong "
                "texture target type. (Expecting type %d, actual type %d) (binding "
                "name:'%s')(texture name:'%s')\n",
                shader_interface->get_name(),
                is_resource_sampler ? "TextureSampler" : "TextureImage",
                bound_texture,
                slot,
                shader_texture_info.type,
                bound_texture->type_,
                shader_interface->get_name_at_offset(shader_texture_info.name_offset),
                bound_texture->get_name());
          }
        }
        else {
          MTL_LOG_WARNING(
              "Shader '%s' expected texture (%s) to be bound to location %d (texture[[%d]]) -- No "
              "texture was "
              "bound. (name:'%s')\n",
              shader_interface->get_name(),
              is_resource_sampler ? "TextureSampler" : "TextureImage",
              location,
              slot,
              shader_interface->get_name_at_offset(shader_texture_info.name_offset));
        }

        /* Bind Dummy texture -- will temporarily resolve validation issues while incorrect formats
         * are provided -- as certain configurations may not need any binding. These issues should
         * be fixed in the high-level, if problems crop up. */
        if (bind_dummy_texture) {
          if (bool(shader_texture_info.stage_mask & ShaderStage::VERTEX)) {
            rps.bind_vertex_texture(
                get_dummy_texture(shader_texture_info.type, shader_texture_info.sampler_format)
                    ->get_metal_handle(),
                slot);

            /* Bind default sampler state. */
            rps.bind_vertex_sampler(default_binding, use_argument_buffer_for_samplers, slot);
          }
          if (bool(shader_texture_info.stage_mask & ShaderStage::FRAGMENT)) {
            rps.bind_fragment_texture(
                get_dummy_texture(shader_texture_info.type, shader_texture_info.sampler_format)
                    ->get_metal_handle(),
                slot);

            /* Bind default sampler state. */
            rps.bind_fragment_sampler(default_binding, use_argument_buffer_for_samplers, slot);
          }
        }
      }
      else {
        MTL_LOG_WARNING(
            "Shader %p expected texture (%s) to be bound to slot %d -- Slot exceeds the "
            "hardware/API limit of '%d'. (name:'%s')\n",
            this->pipeline_state.active_shader,
            is_resource_sampler ? "TextureSampler" : "TextureImage",
            slot,
            GPU_max_textures(),
            shader_interface->get_name_at_offset(shader_texture_info.name_offset));
      }
    }

    /* Construct and Bind argument buffer.
     * NOTE(Metal): Samplers use an argument buffer when the limit of 16 samplers is exceeded. */
    if (use_argument_buffer_for_samplers) {
#ifndef NDEBUG
      /* Debug check to validate each expected texture in the shader interface has a valid
       * sampler object bound to the context. We will need all of these to be valid
       * when constructing the sampler argument buffer. */
      for (const uint i : IndexRange(shader_interface->get_max_texture_index() + 1)) {
        const MTLShaderTexture &texture = shader_interface->get_texture(i);
        if (texture.used) {
          BLI_assert(this->samplers_.mtl_sampler[i] != nil);
        }
      }
#endif

      /* Check to ensure the buffer binding index for the argument buffer has been assigned.
       * This PSO property will be set if we expect to use argument buffers, and the shader
       * uses any amount of textures. */
      BLI_assert(vertex_arg_buffer_bind_index >= 0 || fragment_arg_buffer_bind_index >= 0);
      if (vertex_arg_buffer_bind_index >= 0 || fragment_arg_buffer_bind_index >= 0) {
        /* Offset binding index to be relative to the start of static uniform buffer binding slots.
         * The first N slots, prior to `pipeline_state_instance->base_uniform_buffer_index` are
         * used by vertex and index buffer bindings, and the number of buffers present will vary
         * between PSOs. */
        int arg_buffer_idx = (pipeline_state_instance->base_uniform_buffer_index +
                              vertex_arg_buffer_bind_index);
        assert(arg_buffer_idx < 32);
        id<MTLArgumentEncoder> argument_encoder = shader_interface->find_argument_encoder(
            arg_buffer_idx);
        if (argument_encoder == nil) {
          argument_encoder = [pipeline_state_instance->vert
              newArgumentEncoderWithBufferIndex:arg_buffer_idx];
          shader_interface->insert_argument_encoder(arg_buffer_idx, argument_encoder);
        }

        /* Generate or Fetch argument buffer sampler configuration.
         * NOTE(Metal): we need to base sampler counts off of the maximal texture
         * index. This is not the most optimal, but in practice, not a use-case
         * when argument buffers are required.
         * This is because with explicit texture indices, the binding indices
         * should match across draws, to allow the high-level to optimize bind-points. */
        gpu::MTLBuffer *encoder_buffer = nullptr;
        this->samplers_.num_samplers = shader_interface->get_max_texture_index() + 1;

        gpu::MTLBuffer **cached_smp_buffer_search = this->cached_sampler_buffers_.lookup_ptr(
            this->samplers_);
        if (cached_smp_buffer_search != nullptr) {
          encoder_buffer = *cached_smp_buffer_search;
        }
        else {
          /* Populate argument buffer with current global sampler bindings. */
          int size = [argument_encoder encodedLength];
          int alignment = max_uu([argument_encoder alignment], 256);
          int size_align_delta = (size % alignment);
          int aligned_alloc_size = ((alignment > 1) && (size_align_delta > 0)) ?
                                       size + (alignment - (size % alignment)) :
                                       size;

          /* Allocate buffer to store encoded sampler arguments. */
          encoder_buffer = MTLContext::get_global_memory_manager()->allocate(aligned_alloc_size,
                                                                             true);
          BLI_assert(encoder_buffer);
          BLI_assert(encoder_buffer->get_metal_buffer());
          [argument_encoder setArgumentBuffer:encoder_buffer->get_metal_buffer() offset:0];
          [argument_encoder
              setSamplerStates:this->samplers_.mtl_sampler
                     withRange:NSMakeRange(0, shader_interface->get_max_texture_index() + 1)];
          encoder_buffer->flush();

          /* Insert into cache. */
          this->cached_sampler_buffers_.add_new(this->samplers_, encoder_buffer);
        }

        BLI_assert(encoder_buffer != nullptr);
        int vert_buffer_index = (pipeline_state_instance->base_uniform_buffer_index +
                                 vertex_arg_buffer_bind_index);
        rps.bind_vertex_buffer(encoder_buffer->get_metal_buffer(), 0, vert_buffer_index);

        /* Fragment shader shares its argument buffer binding with the vertex shader, So no need to
         * re-encode. We can use the same argument buffer. */
        if (fragment_arg_buffer_bind_index >= 0) {
          BLI_assert(fragment_arg_buffer_bind_index);
          int frag_buffer_index = (pipeline_state_instance->base_uniform_buffer_index +
                                   fragment_arg_buffer_bind_index);
          rps.bind_fragment_buffer(encoder_buffer->get_metal_buffer(), 0, frag_buffer_index);
        }
      }
    }
  }
}

/* Texture binding variant for compute command encoder.
 * Ensure bound texture resources are bound to the active MTLComputeCommandEncoder. */
void MTLContext::ensure_texture_bindings(
    id<MTLComputeCommandEncoder> rec,
    MTLShaderInterface *shader_interface,
    const MTLComputePipelineStateInstance &pipeline_state_instance)
{
  BLI_assert(shader_interface != nil);
  BLI_assert(rec != nil);

  /* Fetch Render Pass state. */
  MTLComputeState &cs = this->main_command_buffer.get_compute_state();

  @autoreleasepool {
    int compute_arg_buffer_bind_index = -1;

    /* Argument buffers are used for samplers, when the limit of 16 is exceeded.
     * NOTE: Compute uses vertex argument for arg buffer bind index. */
    bool use_argument_buffer_for_samplers = shader_interface->uses_argument_buffer_for_samplers();
    compute_arg_buffer_bind_index = shader_interface->get_argument_buffer_bind_index(
        ShaderStage::COMPUTE);

    /* Loop through expected textures in shader interface and resolve bindings with currently
     * bound textures.. */
    for (const uint t : IndexRange(shader_interface->get_max_texture_index() + 1)) {
      /* Ensure the bound texture is compatible with the shader interface. If the
       * shader does not expect a texture to be bound for the current slot, we skip
       * binding.
       * NOTE: Global texture bindings may be left over from prior draw calls. */
      const MTLShaderTexture &shader_texture_info = shader_interface->get_texture(t);
      if (!shader_texture_info.used) {
        /* Skip unused binding points if explicit indices are specified. */
        continue;
      }

      /* Determine bind lookup table depending on whether an image binding or texture.
       * NOTE: Images and Texture Samplers share a binding table in Metal. */
      bool is_resource_sampler = shader_texture_info.is_texture_sampler;
      MTLTextureBinding(&resource_bind_table)[MTL_MAX_TEXTURE_SLOTS] =
          (is_resource_sampler) ? this->pipeline_state.texture_bindings :
                                  this->pipeline_state.image_bindings;

      /* Texture resource bind slot in shader `[[texture(n)]]`. */
      int slot = shader_texture_info.slot_index;
      /* Explicit bind location for texture. */
      int location = shader_texture_info.location;
      /* Default sampler. */
      MTLSamplerBinding default_binding = {true, DEFAULT_SAMPLER_STATE};

      if (slot >= 0 && slot < GPU_max_textures()) {
        bool bind_dummy_texture = true;
        if (resource_bind_table[location].used) {
          gpu::MTLTexture *bound_texture = resource_bind_table[location].texture_resource;
          MTLSamplerBinding &bound_sampler = (is_resource_sampler) ?
                                                 this->pipeline_state.sampler_bindings[location] :
                                                 default_binding;
          BLI_assert(bound_texture);
          BLI_assert(bound_sampler.used);

          if (shader_texture_info.type == bound_texture->type_) {
            /* Bind texture and sampler if the bound texture matches the type expected by the
             * shader. */
            id<MTLTexture> tex = bound_texture->get_metal_handle();

            if (bool(shader_texture_info.stage_mask & ShaderStage::COMPUTE)) {
              cs.bind_compute_texture(tex, slot);
              cs.bind_compute_sampler(bound_sampler, use_argument_buffer_for_samplers, slot);
            }

            /* Texture state resolved, no need to bind dummy texture */
            bind_dummy_texture = false;
          }
          else {
            /* Texture type for bound texture (e.g. Texture2DArray) does not match what was
             * expected in the shader interface. This is a problem and we will need to bind
             * a dummy texture to ensure correct API usage. */
            MTL_LOG_WARNING(
                "(Shader '%s') Texture (%s) %p bound to slot %d is incompatible -- Wrong "
                "texture target type. (Expecting type %d, actual type %d) (binding "
                "name:'%s')(texture name:'%s')\n",
                shader_interface->get_name(),
                is_resource_sampler ? "TextureSampler" : "TextureImage",
                bound_texture,
                slot,
                shader_texture_info.type,
                bound_texture->type_,
                shader_interface->get_name_at_offset(shader_texture_info.name_offset),
                bound_texture->get_name());
          }
        }
        else {
          MTL_LOG_WARNING(
              "Shader '%s' expected texture (%s) to be bound to location %d (texture[[%d]]) -- No "
              "texture was "
              "bound. (name:'%s')\n",
              shader_interface->get_name(),
              is_resource_sampler ? "TextureSampler" : "TextureImage",
              location,
              slot,
              shader_interface->get_name_at_offset(shader_texture_info.name_offset));
        }

        /* Bind Dummy texture -- will temporarily resolve validation issues while incorrect formats
         * are provided -- as certain configurations may not need any binding. These issues should
         * be fixed in the high-level, if problems crop up. */
        if (bind_dummy_texture) {
          if (bool(shader_texture_info.stage_mask & ShaderStage::COMPUTE)) {
            cs.bind_compute_texture(
                get_dummy_texture(shader_texture_info.type, shader_texture_info.sampler_format)
                    ->get_metal_handle(),
                slot);

            /* Bind default sampler state. */
            MTLSamplerBinding default_binding = {true, DEFAULT_SAMPLER_STATE};
            cs.bind_compute_sampler(default_binding, use_argument_buffer_for_samplers, slot);
          }
        }
      }
      else {
        MTL_LOG_WARNING(
            "Shader %p expected texture (%s) to be bound to slot %d -- Slot exceeds the "
            "hardware/API limit of '%d'. (name:'%s')\n",
            this->pipeline_state.active_shader,
            is_resource_sampler ? "TextureSampler" : "TextureImage",
            slot,
            GPU_max_textures(),
            shader_interface->get_name_at_offset(shader_texture_info.name_offset));
      }
    }

    /* Construct and Bind argument buffer.
     * NOTE(Metal): Samplers use an argument buffer when the limit of 16 samplers is exceeded. */
    if (use_argument_buffer_for_samplers) {
#ifndef NDEBUG
      /* Debug check to validate each expected texture in the shader interface has a valid
       * sampler object bound to the context. We will need all of these to be valid
       * when constructing the sampler argument buffer. */
      for (const uint i : IndexRange(shader_interface->get_max_texture_index() + 1)) {
        const MTLShaderTexture &texture = shader_interface->get_texture(i);
        if (texture.used) {
          BLI_assert(this->samplers_.mtl_sampler[i] != nil);
        }
      }
#endif

      /* Check to ensure the buffer binding index for the argument buffer has been assigned.
       * This PSO property will be set if we expect to use argument buffers, and the shader
       * uses any amount of textures. */
      BLI_assert(compute_arg_buffer_bind_index >= 0);
      if (compute_arg_buffer_bind_index >= 0) {
        /* Offset binding index to be relative to the start of static uniform buffer binding slots.
         * The first N slots, prior to `pipeline_state_instance->base_uniform_buffer_index` are
         * used by vertex and index buffer bindings, and the number of buffers present will vary
         * between PSOs. */
        int arg_buffer_idx = (pipeline_state_instance.base_uniform_buffer_index +
                              compute_arg_buffer_bind_index);
        assert(arg_buffer_idx < 32);
        id<MTLArgumentEncoder> argument_encoder = shader_interface->find_argument_encoder(
            arg_buffer_idx);
        if (argument_encoder == nil) {
          argument_encoder = [pipeline_state_instance.compute
              newArgumentEncoderWithBufferIndex:arg_buffer_idx];
          shader_interface->insert_argument_encoder(arg_buffer_idx, argument_encoder);
        }

        /* Generate or Fetch argument buffer sampler configuration.
         * NOTE(Metal): we need to base sampler counts off of the maximal texture
         * index. This is not the most optimal, but in practice, not a use-case
         * when argument buffers are required.
         * This is because with explicit texture indices, the binding indices
         * should match across draws, to allow the high-level to optimize bind-points. */
        gpu::MTLBuffer *encoder_buffer = nullptr;
        this->samplers_.num_samplers = shader_interface->get_max_texture_index() + 1;

        gpu::MTLBuffer **cached_smp_buffer_search = this->cached_sampler_buffers_.lookup_ptr(
            this->samplers_);
        if (cached_smp_buffer_search != nullptr) {
          encoder_buffer = *cached_smp_buffer_search;
        }
        else {
          /* Populate argument buffer with current global sampler bindings. */
          int size = [argument_encoder encodedLength];
          int alignment = max_uu([argument_encoder alignment], 256);
          int size_align_delta = (size % alignment);
          int aligned_alloc_size = ((alignment > 1) && (size_align_delta > 0)) ?
                                       size + (alignment - (size % alignment)) :
                                       size;

          /* Allocate buffer to store encoded sampler arguments. */
          encoder_buffer = MTLContext::get_global_memory_manager()->allocate(aligned_alloc_size,
                                                                             true);
          BLI_assert(encoder_buffer);
          BLI_assert(encoder_buffer->get_metal_buffer());
          [argument_encoder setArgumentBuffer:encoder_buffer->get_metal_buffer() offset:0];
          [argument_encoder
              setSamplerStates:this->samplers_.mtl_sampler
                     withRange:NSMakeRange(0, shader_interface->get_max_texture_index() + 1)];
          encoder_buffer->flush();

          /* Insert into cache. */
          this->cached_sampler_buffers_.add_new(this->samplers_, encoder_buffer);
        }

        BLI_assert(encoder_buffer != nullptr);
        int compute_buffer_index = (pipeline_state_instance.base_uniform_buffer_index +
                                    compute_arg_buffer_bind_index);
        cs.bind_compute_buffer(encoder_buffer->get_metal_buffer(), 0, compute_buffer_index);
      }
    }
  }
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

bool MTLContext::ensure_compute_pipeline_state()
{
  /* Verify if bound shader is valid and fetch MTLComputePipelineStateInstance. */
  /* Check if an active shader is bound. */
  if (!this->pipeline_state.active_shader) {
    MTL_LOG_WARNING("No Metal shader bound!\n");
    return false;
  }
  /* Also ensure active shader is valid. */
  if (!this->pipeline_state.active_shader->is_valid()) {
    MTL_LOG_WARNING(
        "Bound active shader is not valid (Missing/invalid implementation for Metal).\n", );
    return false;
  }
  /* Verify this is a compute shader. */

  /* Fetch shader interface. */
  MTLShaderInterface *shader_interface = this->pipeline_state.active_shader->get_interface();
  if (shader_interface == nullptr) {
    MTL_LOG_WARNING("Bound active shader does not have a valid shader interface!\n", );
    return false;
  }

  bool success = this->pipeline_state.active_shader->bake_compute_pipeline_state(this);
  const MTLComputePipelineStateInstance &compute_pso_inst =
      this->pipeline_state.active_shader->get_compute_pipeline_state();
  if (!success || compute_pso_inst.pso == nil) {
    MTL_LOG_WARNING("No valid compute PSO for compute dispatch!\n", );
    return false;
  }
  return true;
}

void MTLContext::compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len)
{
  /* Ensure all resources required by upcoming compute submission are correctly bound to avoid
   * out of bounds reads/writes. */
  if (!this->ensure_compute_pipeline_state()) {
    return;
  }

  /* Shader instance. */
  MTLShaderInterface *shader_interface = this->pipeline_state.active_shader->get_interface();
  const MTLComputePipelineStateInstance &compute_pso_inst =
      this->pipeline_state.active_shader->get_compute_pipeline_state();

  /* Begin compute encoder. */
  id<MTLComputeCommandEncoder> compute_encoder =
      this->main_command_buffer.ensure_begin_compute_encoder();
  BLI_assert(compute_encoder != nil);

  /* Bind PSO. */
  MTLComputeState &cs = this->main_command_buffer.get_compute_state();
  cs.bind_pso(compute_pso_inst.pso);

  /* Bind buffers. */
  this->ensure_uniform_buffer_bindings(compute_encoder, shader_interface, compute_pso_inst);
  /** Ensure resource bindings. */
  /* Texture Bindings. */
  /* We will iterate through all texture bindings on the context and determine if any of the
   * active slots match those in our shader interface. If so, textures will be bound. */
  if (shader_interface->get_total_textures() > 0) {
    this->ensure_texture_bindings(compute_encoder, shader_interface, compute_pso_inst);
  }

  /* Dispatch compute. */
  [compute_encoder dispatchThreadgroups:MTLSizeMake(max_ii(groups_x_len, 1),
                                                    max_ii(groups_y_len, 1),
                                                    max_ii(groups_z_len, 1))
                  threadsPerThreadgroup:MTLSizeMake(compute_pso_inst.threadgroup_x_len,
                                                    compute_pso_inst.threadgroup_y_len,
                                                    compute_pso_inst.threadgroup_z_len)];
}

void MTLContext::compute_dispatch_indirect(StorageBuf *indirect_buf)
{
  /* Ensure all resources required by upcoming compute submission are correctly bound. */
  if (this->ensure_compute_pipeline_state()) {
    /* Shader instance. */
    MTLShaderInterface *shader_interface = this->pipeline_state.active_shader->get_interface();
    const MTLComputePipelineStateInstance &compute_pso_inst =
        this->pipeline_state.active_shader->get_compute_pipeline_state();

    /* Begin compute encoder. */
    id<MTLComputeCommandEncoder> compute_encoder =
        this->main_command_buffer.ensure_begin_compute_encoder();
    BLI_assert(compute_encoder != nil);

    /* Bind PSO. */
    MTLComputeState &cs = this->main_command_buffer.get_compute_state();
    cs.bind_pso(compute_pso_inst.pso);

    /* Bind buffers. */
    this->ensure_uniform_buffer_bindings(compute_encoder, shader_interface, compute_pso_inst);
    /** Ensure resource bindings. */
    /* Texture Bindings. */
    /* We will iterate through all texture bindings on the context and determine if any of the
     * active slots match those in our shader interface. If so, textures will be bound. */
    if (shader_interface->get_total_textures() > 0) {
      this->ensure_texture_bindings(compute_encoder, shader_interface, compute_pso_inst);
    }

    /* Indirect Dispatch compute. */
    MTLStorageBuf *mtlssbo = static_cast<MTLStorageBuf *>(indirect_buf);
    id<MTLBuffer> mtl_indirect_buf = mtlssbo->get_metal_buffer();
    BLI_assert(mtl_indirect_buf != nil);
    if (mtl_indirect_buf == nil) {
      MTL_LOG_WARNING("Metal Indirect Compute dispatch storage buffer does not exist.\n");
      return;
    }

    /* Indirect Compute dispatch. */
    [compute_encoder
        dispatchThreadgroupsWithIndirectBuffer:mtl_indirect_buf
                          indirectBufferOffset:0
                         threadsPerThreadgroup:MTLSizeMake(compute_pso_inst.threadgroup_x_len,
                                                           compute_pso_inst.threadgroup_y_len,
                                                           compute_pso_inst.threadgroup_z_len)];
  }
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
    MTL_LOG_WARNING("Attempting to bind texture '%s' to invalid texture unit %d\n",
                    mtl_texture->get_name(),
                    texture_unit);
    BLI_assert(false);
    return;
  }

  MTLTextureBinding(
      &resource_bind_table)[MTL_MAX_TEXTURE_SLOTS] = (is_image) ?
                                                         this->pipeline_state.image_bindings :
                                                         this->pipeline_state.texture_bindings;

  /* Bind new texture. */
  resource_bind_table[texture_unit].texture_resource = mtl_texture;
  resource_bind_table[texture_unit].used = true;
  mtl_texture->is_bound_ = true;
}

void MTLContext::sampler_bind(MTLSamplerState sampler_state, uint sampler_unit)
{
  BLI_assert(this);
  if (sampler_unit < 0 || sampler_unit >= GPU_max_textures() ||
      sampler_unit >= MTL_MAX_SAMPLER_SLOTS)
  {
    MTL_LOG_WARNING("Attempting to bind sampler to invalid sampler unit %d\n", sampler_unit);
    BLI_assert(false);
    return;
  }

  /* Apply binding. */
  this->pipeline_state.sampler_bindings[sampler_unit] = {true, sampler_state};
}

void MTLContext::texture_unbind(gpu::MTLTexture *mtl_texture, bool is_image)
{
  BLI_assert(mtl_texture);

  MTLTextureBinding(
      &resource_bind_table)[MTL_MAX_TEXTURE_SLOTS] = (is_image) ?
                                                         this->pipeline_state.image_bindings :
                                                         this->pipeline_state.texture_bindings;

  /* Iterate through textures in state and unbind. */
  for (int i = 0; i < min_uu(GPU_max_textures(), MTL_MAX_TEXTURE_SLOTS); i++) {
    if (resource_bind_table[i].texture_resource == mtl_texture) {
      resource_bind_table[i].texture_resource = nullptr;
      resource_bind_table[i].used = false;
    }
  }

  /* Locally unbind texture. */
  mtl_texture->is_bound_ = false;
}

void MTLContext::texture_unbind_all(bool is_image)
{
  MTLTextureBinding(
      &resource_bind_table)[MTL_MAX_TEXTURE_SLOTS] = (is_image) ?
                                                         this->pipeline_state.image_bindings :
                                                         this->pipeline_state.texture_bindings;

  /* Iterate through context's bound textures. */
  for (int t = 0; t < min_uu(GPU_max_textures(), MTL_MAX_TEXTURE_SLOTS); t++) {
    if (resource_bind_table[t].used && resource_bind_table[t].texture_resource) {
      resource_bind_table[t].used = false;
      resource_bind_table[t].texture_resource = nullptr;
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
/** \name Swap-chain management and Metal presentation.
 * \{ */

void present(MTLRenderPassDescriptor *blit_descriptor,
             id<MTLRenderPipelineState> blit_pso,
             id<MTLTexture> swapchain_texture,
             id<CAMetalDrawable> drawable)
{

  MTLContext *ctx = static_cast<MTLContext *>(unwrap(GPU_context_active_get()));
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
  if (MTLContext::avg_drawable_latency_us > 185000) {
    perf_max_drawables = 1;
  }
  else if (MTLContext::avg_drawable_latency_us > 85000) {
    perf_max_drawables = 2;
  }

  while (MTLContext::max_drawables_in_flight > min_ii(perf_max_drawables, MTL_MAX_DRAWABLES)) {
    PIL_sleep_ms(2);
  }

  /* Present is submitted in its own CMD Buffer to ensure drawable reference released as early as
   * possible. This command buffer is separate as it does not utilize the global state
   * for rendering as the main context does. */
  id<MTLCommandBuffer> cmdbuf = [ctx->queue commandBuffer];
  MTLCommandBufferManager::num_active_cmd_bufs++;

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

  id<MTLCommandBuffer> cmd_buffer_ref = cmdbuf;
  [cmd_buffer_ref retain];

  /* Increment drawables in flight limiter. */
  MTLContext::max_drawables_in_flight++;
  std::chrono::time_point submission_time = std::chrono::high_resolution_clock::now();

  /* Increment free pool reference and decrement upon command buffer completion. */
  cmd_free_buffer_list->increment_reference();
  [cmdbuf addCompletedHandler:^(id<MTLCommandBuffer> cb) {
    /* Flag freed buffers associated with this CMD buffer as ready to be freed. */
    cmd_free_buffer_list->decrement_reference();
    [cmd_buffer_ref release];

    /* Decrement count */
    MTLCommandBufferManager::num_active_cmd_bufs--;
    MTL_LOG_INFO("[Metal] Active command buffers: %d\n",
                 MTLCommandBufferManager::num_active_cmd_bufs);

    /* Drawable count and latency management. */
    MTLContext::max_drawables_in_flight--;
    std::chrono::time_point completion_time = std::chrono::high_resolution_clock::now();
    int64_t microseconds_per_frame = std::chrono::duration_cast<std::chrono::microseconds>(
                                         completion_time - submission_time)
                                         .count();
    MTLContext::latency_resolve_average(microseconds_per_frame);

    MTL_LOG_INFO("Frame Latency: %f ms  (Rolling avg: %f ms  Drawables: %d)\n",
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

      @autoreleasepool {
        const char *stringAsChar = [[NSString stringWithFormat:@"%@", error] UTF8String];

        std::ofstream outfile;
        outfile.open("command_buffer_error.txt", std::fstream::out | std::fstream::app);
        outfile << stringAsChar;
        outfile.close();
      }
    }
    else {
      @autoreleasepool {
        NSString *str = @"Command buffer completed successfully!\n";
        const char *stringAsChar = [str UTF8String];

        std::ofstream outfile;
        outfile.open("command_buffer_error.txt", std::fstream::out | std::fstream::app);
        outfile << stringAsChar;
        outfile.close();
      }
    }
  }
}

/** \} */

}  // namespace blender::gpu
