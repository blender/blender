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
  /* We do not need to flush shared memory */
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

  /* Initialize Metal modules. */
  this->state_manager = new MTLStateManager(this);

  /* TODO(Metal): Implement. */
}

MTLContext::~MTLContext()
{
  /* TODO(Metal): Implement. */
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

id<MTLCommandBuffer> MTLContext::get_active_command_buffer()
{
  /* TODO(Metal): Implement. */
  return nil;
}

/* Render Pass State and Management */
void MTLContext::begin_render_pass()
{
  /* TODO(Metal): Implement. */
}
void MTLContext::end_render_pass()
{
  /* TODO(Metal): Implement. */
}

bool MTLContext::is_render_pass_active()
{
  /* TODO(Metal): Implement. */
  return false;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture State Management
 * \{ */

void MTLContext::texture_bind(gpu::MTLTexture *mtl_texture, unsigned int texture_unit)
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

void MTLContext::sampler_bind(MTLSamplerState sampler_state, unsigned int sampler_unit)
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
  BLI_assert((unsigned int)sampler_state >= 0 && ((unsigned int)sampler_state) < GPU_SAMPLER_MAX);
  return this->sampler_state_cache_[(unsigned int)sampler_state];
}

id<MTLSamplerState> MTLContext::generate_sampler_from_state(MTLSamplerState sampler_state)
{
  /* Check if sampler already exists for given state. */
  id<MTLSamplerState> st = this->sampler_state_cache_[(unsigned int)sampler_state];
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
    this->sampler_state_cache_[(unsigned int)sampler_state] = state;

    BLI_assert(state != nil);
    [descriptor autorelease];
    return state;
  }
}

id<MTLSamplerState> MTLContext::get_default_sampler_state()
{
  if (this->default_sampler_state_ == nil) {
    this->default_sampler_state_ = this->get_sampler_from_state(DEFAULT_SAMPLER_STATE);
  }
  return this->default_sampler_state_;
}

/** \} */

}  // blender::gpu
