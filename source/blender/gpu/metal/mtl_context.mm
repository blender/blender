/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_state.hh"

#include "DNA_userdef_types.h"

#include "GPU_capabilities.h"

using namespace blender;
using namespace blender::gpu;

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Memory Management
 * \{ */

bool MTLTemporaryBufferRange::requires_flush()
{
  /* We do not need to flush shared memory. */
  return this->options & MTLResourceStorageModeManaged;
}

void MTLTemporaryBufferRange::flush()
{
  if (this->requires_flush()) {
    BLI_assert(this->metal_buffer);
    BLI_assert((this->buffer_offset + this->size) <= [this->metal_buffer length]);
    BLI_assert(this->buffer_offset >= 0);
    [this->metal_buffer
        didModifyRange:NSMakeRange(this->buffer_offset, this->size - this->buffer_offset)];
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name MTLContext
 * \{ */

/* Placeholder functions */
MTLContext::MTLContext(void *ghost_window)
{
  /* Init debug. */
  debug::mtl_debug_init();

  /* Initialize command buffer state. */
  this->main_command_buffer.prepare(this);

  /* Frame management. */
  is_inside_frame_ = false;

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
  this->state_manager = new MTLStateManager(this);

  /* Initialize texture read/update structures. */
  this->get_texture_utils().init();

  /* Bound Samplers struct. */
  for (int i = 0; i < MTL_MAX_TEXTURE_SLOTS; i++) {
    samplers_.mtl_sampler[i] = nil;
    samplers_.mtl_sampler_flags[i] = DEFAULT_SAMPLER_STATE;
  }

  /* Initialize samplers. */
  for (uint i = 0; i < GPU_SAMPLER_MAX; i++) {
    MTLSamplerState state;
    state.state = static_cast<eGPUSamplerState>(i);
    sampler_state_cache_[i] = this->generate_sampler_from_state(state);
  }
}

MTLContext::~MTLContext()
{
  BLI_assert(this == reinterpret_cast<MTLContext *>(GPU_context_active_get()));
  /* Ensure rendering is complete command encoders/command buffers are freed. */
  if (MTLBackend::get()->is_inside_render_boundary()) {
    this->finish();

    /* End frame. */
    if (is_inside_frame_) {
      this->end_frame();
    }
  }
  /* Release update/blit shaders. */
  this->get_texture_utils().cleanup();

  /* Release Sampler States. */
  for (int i = 0; i < GPU_SAMPLER_MAX; i++) {
    if (sampler_state_cache_[i] != nil) {
      [sampler_state_cache_[i] release];
      sampler_state_cache_[i] = nil;
    }
  }
}

void MTLContext::begin_frame()
{
  BLI_assert(MTLBackend::get()->is_inside_render_boundary());
  if (is_inside_frame_) {
    return;
  }

  /* Begin Command buffer for next frame. */
  is_inside_frame_ = true;
}

void MTLContext::end_frame()
{
  BLI_assert(is_inside_frame_);

  /* Ensure pre-present work is committed. */
  this->flush();

  /* Increment frame counter. */
  is_inside_frame_ = false;
}

void MTLContext::check_error(const char *info)
{
  /* TODO(Metal): Implement. */
}

void MTLContext::activate(void)
{
  /* TODO(Metal): Implement. */
}
void MTLContext::deactivate(void)
{
  /* TODO(Metal): Implement. */
}

void MTLContext::flush(void)
{
  /* TODO(Metal): Implement. */
}
void MTLContext::finish(void)
{
  /* TODO(Metal): Implement. */
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
  if (!is_inside_frame_) {
    this->begin_frame();
  }

  /* Check whether a framebuffer is bound. */
  if (!this->active_fb) {
    BLI_assert(false && "No framebuffer is bound!");
    return this->main_command_buffer.get_active_render_command_encoder();
  }

  /* Ensure command buffer workload submissions are optimal --
   * Though do not split a batch mid-IMM recording */
  /* TODO(Metal): Add IMM Check once MTLImmediate has been implemented. */
  if (this->main_command_buffer.do_break_submission()/*&&
      !((MTLImmediate *)(this->imm))->imm_is_recording()*/) {
    this->flush();
  }

  /* Begin pass or perform a pass switch if the active framebuffer has been changed, or if the
   * framebuffer state has been modified (is_dirty). */
  if (!this->main_command_buffer.is_inside_render_pass() ||
      this->active_fb != this->main_command_buffer.get_active_framebuffer() ||
      this->main_command_buffer.get_active_framebuffer()->get_dirty()) {

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
    this->pipeline_state.active_shader = NULL;

    /* Clear bindings state. */
    for (int t = 0; t < GPU_max_textures(); t++) {
      this->pipeline_state.texture_bindings[t].used = false;
      this->pipeline_state.texture_bindings[t].texture_slot_index = t;
      this->pipeline_state.texture_bindings[t].texture_resource = NULL;
    }
    for (int s = 0; s < MTL_MAX_SAMPLER_SLOTS; s++) {
      this->pipeline_state.sampler_bindings[s].used = false;
    }
    for (int u = 0; u < MTL_MAX_UNIFORM_BUFFER_BINDINGS; u++) {
      this->pipeline_state.ubo_bindings[u].bound = false;
      this->pipeline_state.ubo_bindings[u].ubo = NULL;
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
/** \name Texture State Management
 * \{ */

void MTLContext::texture_bind(gpu::MTLTexture *mtl_texture, uint texture_unit)
{
  BLI_assert(this);
  BLI_assert(mtl_texture);

  if (texture_unit < 0 || texture_unit >= GPU_max_textures() ||
      texture_unit >= MTL_MAX_TEXTURE_SLOTS) {
    MTL_LOG_WARNING("Attempting to bind texture '%s' to invalid texture unit %d\n",
                    mtl_texture->get_name(),
                    texture_unit);
    BLI_assert(false);
    return;
  }

  /* Bind new texture. */
  this->pipeline_state.texture_bindings[texture_unit].texture_resource = mtl_texture;
  this->pipeline_state.texture_bindings[texture_unit].used = true;
  mtl_texture->is_bound_ = true;
}

void MTLContext::sampler_bind(MTLSamplerState sampler_state, uint sampler_unit)
{
  BLI_assert(this);
  if (sampler_unit < 0 || sampler_unit >= GPU_max_textures() ||
      sampler_unit >= MTL_MAX_SAMPLER_SLOTS) {
    MTL_LOG_WARNING("Attempting to bind sampler to invalid sampler unit %d\n", sampler_unit);
    BLI_assert(false);
    return;
  }

  /* Apply binding. */
  this->pipeline_state.sampler_bindings[sampler_unit] = {true, sampler_state};
}

void MTLContext::texture_unbind(gpu::MTLTexture *mtl_texture)
{
  BLI_assert(mtl_texture);

  /* Iterate through textures in state and unbind. */
  for (int i = 0; i < min_uu(GPU_max_textures(), MTL_MAX_TEXTURE_SLOTS); i++) {
    if (this->pipeline_state.texture_bindings[i].texture_resource == mtl_texture) {
      this->pipeline_state.texture_bindings[i].texture_resource = nullptr;
      this->pipeline_state.texture_bindings[i].used = false;
    }
  }

  /* Locally unbind texture. */
  mtl_texture->is_bound_ = false;
}

void MTLContext::texture_unbind_all()
{
  /* Iterate through context's bound textures. */
  for (int t = 0; t < min_uu(GPU_max_textures(), MTL_MAX_TEXTURE_SLOTS); t++) {
    if (this->pipeline_state.texture_bindings[t].used &&
        this->pipeline_state.texture_bindings[t].texture_resource) {

      this->pipeline_state.texture_bindings[t].used = false;
      this->pipeline_state.texture_bindings[t].texture_resource = nullptr;
    }
  }
}

id<MTLSamplerState> MTLContext::get_sampler_from_state(MTLSamplerState sampler_state)
{
  BLI_assert((uint)sampler_state >= 0 && ((uint)sampler_state) < GPU_SAMPLER_MAX);
  return sampler_state_cache_[(uint)sampler_state];
}

id<MTLSamplerState> MTLContext::generate_sampler_from_state(MTLSamplerState sampler_state)
{
  /* Check if sampler already exists for given state. */
  id<MTLSamplerState> st = sampler_state_cache_[(uint)sampler_state];
  if (st != nil) {
    return st;
  }
  else {
    MTLSamplerDescriptor *descriptor = [[MTLSamplerDescriptor alloc] init];
    descriptor.normalizedCoordinates = true;

    MTLSamplerAddressMode clamp_type = (sampler_state.state & GPU_SAMPLER_CLAMP_BORDER) ?
                                           MTLSamplerAddressModeClampToBorderColor :
                                           MTLSamplerAddressModeClampToEdge;
    descriptor.rAddressMode = (sampler_state.state & GPU_SAMPLER_REPEAT_R) ?
                                  MTLSamplerAddressModeRepeat :
                                  clamp_type;
    descriptor.sAddressMode = (sampler_state.state & GPU_SAMPLER_REPEAT_S) ?
                                  MTLSamplerAddressModeRepeat :
                                  clamp_type;
    descriptor.tAddressMode = (sampler_state.state & GPU_SAMPLER_REPEAT_T) ?
                                  MTLSamplerAddressModeRepeat :
                                  clamp_type;
    descriptor.borderColor = MTLSamplerBorderColorTransparentBlack;
    descriptor.minFilter = (sampler_state.state & GPU_SAMPLER_FILTER) ?
                               MTLSamplerMinMagFilterLinear :
                               MTLSamplerMinMagFilterNearest;
    descriptor.magFilter = (sampler_state.state & GPU_SAMPLER_FILTER) ?
                               MTLSamplerMinMagFilterLinear :
                               MTLSamplerMinMagFilterNearest;
    descriptor.mipFilter = (sampler_state.state & GPU_SAMPLER_MIPMAP) ?
                               MTLSamplerMipFilterLinear :
                               MTLSamplerMipFilterNotMipmapped;
    descriptor.lodMinClamp = -1000;
    descriptor.lodMaxClamp = 1000;
    float aniso_filter = max_ff(16, U.anisotropic_filter);
    descriptor.maxAnisotropy = (sampler_state.state & GPU_SAMPLER_MIPMAP) ? aniso_filter : 1;
    descriptor.compareFunction = (sampler_state.state & GPU_SAMPLER_COMPARE) ?
                                     MTLCompareFunctionLessEqual :
                                     MTLCompareFunctionAlways;
    descriptor.supportArgumentBuffers = true;

    id<MTLSamplerState> state = [this->device newSamplerStateWithDescriptor:descriptor];
    sampler_state_cache_[(uint)sampler_state] = state;

    BLI_assert(state != nil);
    [descriptor autorelease];
    return state;
  }
}

id<MTLSamplerState> MTLContext::get_default_sampler_state()
{
  if (default_sampler_state_ == nil) {
    default_sampler_state_ = this->get_sampler_from_state(DEFAULT_SAMPLER_STATE);
  }
  return default_sampler_state_;
}

/** \} */

}  // blender::gpu
