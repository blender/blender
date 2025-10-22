/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.hh"

#include "BLI_math_base.h"
#include "BLI_math_bits.h"

#include "GPU_capabilities.hh"

#include "gl_context.hh"
#include "gl_framebuffer.hh"
#include "gl_texture.hh"

#include "gl_state.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name GLStateManager
 * \{ */

GLStateManager::GLStateManager()
{
  /* Set other states that never change. */
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  glEnable(GL_MULTISAMPLE);

  glDisable(GL_DITHER);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  /* Takes precedence over #GL_PRIMITIVE_RESTART.
   * Sets restart index correctly following the IBO type. */
  glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

  /* Limits. */
  glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, line_width_range_);

  /* Force update using default state. */
  current_ = ~state;
  current_mutable_ = ~mutable_state;
  set_state(state);
  set_mutable_state(mutable_state);
}

void GLStateManager::apply_state()
{
  this->set_state(this->state);
  this->set_mutable_state(this->mutable_state);
  this->texture_bind_apply();
  this->image_bind_apply();

  /* This is needed by gpu_py_offscreen. */
  active_fb->apply_state();
};

void GLStateManager::force_state()
{
  /* Little exception for clip distances since they need to keep the old count correct. */
  uint32_t clip_distances = current_.clip_distances;
  current_ = ~this->state;
  current_.clip_distances = clip_distances;
  current_mutable_ = ~this->mutable_state;
  this->set_state(this->state);
  this->set_mutable_state(this->mutable_state);
};

void GLStateManager::set_state(const GPUState &state)
{
  GPUState changed = state ^ current_;

  if (changed.blend != 0) {
    set_blend((GPUBlend)state.blend);
  }
  if (changed.write_mask != 0) {
    set_write_mask((GPUWriteMask)state.write_mask);
  }
  if (changed.depth_test != 0) {
    set_depth_test((GPUDepthTest)state.depth_test);
  }
  if (changed.stencil_test != 0 || changed.stencil_op != 0) {
    set_stencil_test((GPUStencilTest)state.stencil_test, (GPUStencilOp)state.stencil_op);
    set_stencil_mask((GPUStencilTest)state.stencil_test, mutable_state);
  }
  if (changed.clip_distances != 0) {
    set_clip_distances(state.clip_distances, current_.clip_distances);
  }
  if (changed.culling_test != 0) {
    set_backface_culling((GPUFaceCullTest)state.culling_test);
  }
  if (changed.logic_op_xor != 0) {
    set_logic_op(state.logic_op_xor);
  }
  if (changed.invert_facing != 0) {
    set_facing(state.invert_facing);
  }
  if (changed.provoking_vert != 0) {
    set_provoking_vert((GPUProvokingVertex)state.provoking_vert);
  }
  if (changed.shadow_bias != 0) {
    set_shadow_bias(state.shadow_bias);
  }
  if (changed.clip_control != 0) {
    set_clip_control(state.clip_control);
  }

  /* TODO: remove. */
  if (changed.polygon_smooth) {
    if (state.polygon_smooth) {
      glEnable(GL_POLYGON_SMOOTH);
    }
    else {
      glDisable(GL_POLYGON_SMOOTH);
    }
  }
  if (changed.line_smooth) {
    if (state.line_smooth) {
      glEnable(GL_LINE_SMOOTH);
    }
    else {
      glDisable(GL_LINE_SMOOTH);
    }
  }

  current_ = state;
}

void GLStateManager::set_mutable_state(const GPUStateMutable &state)
{
  GPUStateMutable changed = state ^ current_mutable_;

  /* TODO: remove, should be uniform. */
  if (float_as_uint(changed.point_size) != 0) {
    if (state.point_size > 0.0f) {
      glEnable(GL_PROGRAM_POINT_SIZE);
    }
    else {
      glDisable(GL_PROGRAM_POINT_SIZE);
      glPointSize(fabsf(state.point_size));
    }
  }

  if (float_as_uint(changed.line_width) != 0) {
    /* TODO: remove, should use wide line shader. */
    glLineWidth(clamp_f(state.line_width, line_width_range_[0], line_width_range_[1]));
  }

  if (float_as_uint(changed.depth_range[0]) != 0 || float_as_uint(changed.depth_range[1]) != 0) {
    /* TODO: remove, should modify the projection matrix instead. */
    glDepthRange(UNPACK2(state.depth_range));
  }

  if (changed.stencil_compare_mask != 0 || changed.stencil_reference != 0 ||
      changed.stencil_write_mask != 0)
  {
    set_stencil_mask((GPUStencilTest)current_.stencil_test, state);
  }

  current_mutable_ = state;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name State set functions
 * \{ */

void GLStateManager::set_write_mask(const GPUWriteMask value)
{
  glDepthMask((value & GPU_WRITE_DEPTH) != 0);
  glColorMask((value & GPU_WRITE_RED) != 0,
              (value & GPU_WRITE_GREEN) != 0,
              (value & GPU_WRITE_BLUE) != 0,
              (value & GPU_WRITE_ALPHA) != 0);

  if (value == GPU_WRITE_NONE) {
    glEnable(GL_RASTERIZER_DISCARD);
  }
  else {
    glDisable(GL_RASTERIZER_DISCARD);
  }
}

void GLStateManager::set_depth_test(const GPUDepthTest value)
{
  GLenum func;
  switch (value) {
    case GPU_DEPTH_LESS:
      func = GL_LESS;
      break;
    case GPU_DEPTH_LESS_EQUAL:
      func = GL_LEQUAL;
      break;
    case GPU_DEPTH_EQUAL:
      func = GL_EQUAL;
      break;
    case GPU_DEPTH_GREATER:
      func = GL_GREATER;
      break;
    case GPU_DEPTH_GREATER_EQUAL:
      func = GL_GEQUAL;
      break;
    case GPU_DEPTH_ALWAYS:
    default:
      func = GL_ALWAYS;
      break;
  }

  if (value != GPU_DEPTH_NONE) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(func);
  }
  else {
    glDisable(GL_DEPTH_TEST);
  }
}

void GLStateManager::set_stencil_test(const GPUStencilTest test, const GPUStencilOp operation)
{
  switch (operation) {
    case GPU_STENCIL_OP_REPLACE:
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
      break;
    case GPU_STENCIL_OP_COUNT_DEPTH_PASS:
      glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
      glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
      break;
    case GPU_STENCIL_OP_COUNT_DEPTH_FAIL:
      glStencilOpSeparate(GL_BACK, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
      glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
      break;
    case GPU_STENCIL_OP_NONE:
    default:
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  }

  if (test != GPU_STENCIL_NONE) {
    glEnable(GL_STENCIL_TEST);
  }
  else {
    glDisable(GL_STENCIL_TEST);
  }
}

void GLStateManager::set_stencil_mask(const GPUStencilTest test, const GPUStateMutable &state)
{
  GLenum func;
  switch (test) {
    case GPU_STENCIL_NEQUAL:
      func = GL_NOTEQUAL;
      break;
    case GPU_STENCIL_EQUAL:
      func = GL_EQUAL;
      break;
    case GPU_STENCIL_ALWAYS:
      func = GL_ALWAYS;
      break;
    case GPU_STENCIL_NONE:
    default:
      glStencilMask(0x00);
      glStencilFunc(GL_ALWAYS, 0x00, 0x00);
      return;
  }

  glStencilMask(state.stencil_write_mask);
  glStencilFunc(func, state.stencil_reference, state.stencil_compare_mask);
}

void GLStateManager::set_clip_distances(const int new_dist_len, const int old_dist_len)
{
  for (int i = 0; i < new_dist_len; i++) {
    glEnable(GL_CLIP_DISTANCE0 + i);
  }
  for (int i = new_dist_len; i < old_dist_len; i++) {
    glDisable(GL_CLIP_DISTANCE0 + i);
  }
}

void GLStateManager::set_logic_op(const bool enable)
{
  if (enable) {
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_XOR);
  }
  else {
    glDisable(GL_COLOR_LOGIC_OP);
  }
}

void GLStateManager::set_facing(const bool invert)
{
  glFrontFace((invert) ? GL_CW : GL_CCW);
}

void GLStateManager::set_backface_culling(const GPUFaceCullTest test)
{
  if (test != GPU_CULL_NONE) {
    glEnable(GL_CULL_FACE);
    glCullFace((test == GPU_CULL_FRONT) ? GL_FRONT : GL_BACK);
  }
  else {
    glDisable(GL_CULL_FACE);
  }
}

void GLStateManager::set_provoking_vert(const GPUProvokingVertex vert)
{
  GLenum value = (vert == GPU_VERTEX_FIRST) ? GL_FIRST_VERTEX_CONVENTION :
                                              GL_LAST_VERTEX_CONVENTION;
  glProvokingVertex(value);
}

void GLStateManager::set_shadow_bias(const bool enable)
{
  if (enable) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_POLYGON_OFFSET_LINE);
    /* 2.0 Seems to be the lowest possible slope bias that works in every case. */
    glPolygonOffset(2.0f, 1.0f);
  }
  else {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
  }
}

void GLStateManager::set_clip_control(const bool enable)
{
  if (enable) {
    /* Match Vulkan and Metal by default. */
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
  }
  else {
    glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
  }
}

void GLStateManager::set_blend(const GPUBlend value)
{
  /**
   * Factors to the equation.
   * SRC is fragment shader output.
   * DST is frame-buffer color.
   * final.rgb = SRC.rgb * src_rgb + DST.rgb * dst_rgb;
   * final.a = SRC.a * src_alpha + DST.a * dst_alpha;
   */
  GLenum src_rgb, src_alpha, dst_rgb, dst_alpha;
  switch (value) {
    default:
    case GPU_BLEND_ALPHA: {
      src_rgb = GL_SRC_ALPHA;
      dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
      src_alpha = GL_ONE;
      dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
      break;
    }
    case GPU_BLEND_ALPHA_PREMULT: {
      src_rgb = GL_ONE;
      dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
      src_alpha = GL_ONE;
      dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
      break;
    }
    case GPU_BLEND_ADDITIVE: {
      /* Do not let alpha accumulate but pre-multiply the source RGB by it. */
      src_rgb = GL_SRC_ALPHA;
      dst_rgb = GL_ONE;
      src_alpha = GL_ZERO;
      dst_alpha = GL_ONE;
      break;
    }
    /* Factors are not use in min or max mode, but avoid uninitialized values. */;
    case GPU_BLEND_MIN:
    case GPU_BLEND_MAX:
    case GPU_BLEND_SUBTRACT:
    case GPU_BLEND_ADDITIVE_PREMULT: {
      /* Let alpha accumulate. */
      src_rgb = GL_ONE;
      dst_rgb = GL_ONE;
      src_alpha = GL_ONE;
      dst_alpha = GL_ONE;
      break;
    }
    case GPU_BLEND_MULTIPLY: {
      src_rgb = GL_DST_COLOR;
      dst_rgb = GL_ZERO;
      src_alpha = GL_DST_ALPHA;
      dst_alpha = GL_ZERO;
      break;
    }
    case GPU_BLEND_INVERT: {
      src_rgb = GL_ONE_MINUS_DST_COLOR;
      dst_rgb = GL_ZERO;
      src_alpha = GL_ZERO;
      dst_alpha = GL_ONE;
      break;
    }
    case GPU_BLEND_OIT: {
      src_rgb = GL_ONE;
      dst_rgb = GL_ONE;
      src_alpha = GL_ZERO;
      dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
      break;
    }
    case GPU_BLEND_BACKGROUND: {
      src_rgb = GL_ONE_MINUS_DST_ALPHA;
      dst_rgb = GL_SRC_ALPHA;
      src_alpha = GL_ZERO;
      dst_alpha = GL_SRC_ALPHA;
      break;
    }
    case GPU_BLEND_ALPHA_UNDER_PREMUL: {
      src_rgb = GL_ONE_MINUS_DST_ALPHA;
      dst_rgb = GL_ONE;
      src_alpha = GL_ONE_MINUS_DST_ALPHA;
      dst_alpha = GL_ONE;
      break;
    }
    case GPU_BLEND_CUSTOM: {
      src_rgb = GL_ONE;
      dst_rgb = GL_SRC1_COLOR;
      src_alpha = GL_ONE;
      dst_alpha = GL_SRC1_ALPHA;
      break;
    }
    case GPU_BLEND_OVERLAY_MASK_FROM_ALPHA: {
      src_rgb = GL_ZERO;
      dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
      src_alpha = GL_ZERO;
      dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
      break;
    }
  }

  if (value == GPU_BLEND_MIN) {
    glBlendEquation(GL_MIN);
  }
  else if (value == GPU_BLEND_MAX) {
    glBlendEquation(GL_MAX);
  }
  else if (value == GPU_BLEND_SUBTRACT) {
    glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
  }
  else {
    glBlendEquation(GL_FUNC_ADD);
  }

  /* Always set the blend function. This avoid a rendering error when blending is disabled but
   * GPU_BLEND_CUSTOM was used just before and the frame-buffer is using more than 1 color target.
   */
  glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
  if (value != GPU_BLEND_NONE) {
    glEnable(GL_BLEND);
  }
  else {
    glDisable(GL_BLEND);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture State Management
 * \{ */

void GLStateManager::texture_bind(Texture *tex_, GPUSamplerState sampler_state, int unit)
{
  BLI_assert(unit < GPU_max_textures());
  GLTexture *tex = static_cast<GLTexture *>(tex_);
  if (G.debug & G_DEBUG_GPU) {
    tex->check_feedback_loop();
  }
  /* Eliminate redundant binds. */
  if ((textures_[unit] == tex->tex_id_) &&
      (samplers_[unit] == GLTexture::get_sampler(sampler_state)))
  {
    return;
  }
  targets_[unit] = tex->target_;
  textures_[unit] = tex->tex_id_;
  samplers_[unit] = GLTexture::get_sampler(sampler_state);
  tex->is_bound_ = true;
  dirty_texture_binds_ |= 1ULL << unit;
}

void GLStateManager::texture_bind_temp(GLTexture *tex)
{
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(tex->target_, tex->tex_id_);
  /* Will reset the first texture that was originally bound to slot 0 back before drawing. */
  dirty_texture_binds_ |= 1ULL;
  /* NOTE: This might leave this texture attached to this target even after update.
   * In practice it is not causing problems as we have incorrect binding detection
   * at higher level. */
}

void GLStateManager::texture_unbind(Texture *tex_)
{
  GLTexture *tex = static_cast<GLTexture *>(tex_);
  if (!tex->is_bound_) {
    return;
  }

  GLuint tex_id = tex->tex_id_;
  for (int i = 0; i < ARRAY_SIZE(textures_); i++) {
    if (textures_[i] == tex_id) {
      textures_[i] = 0;
      samplers_[i] = 0;
      dirty_texture_binds_ |= 1ULL << i;
    }
  }
  tex->is_bound_ = false;
}

void GLStateManager::texture_unbind_all()
{
  for (int i = 0; i < ARRAY_SIZE(textures_); i++) {
    if (textures_[i] != 0) {
      textures_[i] = 0;
      samplers_[i] = 0;
      dirty_texture_binds_ |= 1ULL << i;
    }
  }
  this->texture_bind_apply();
}

void GLStateManager::texture_bind_apply()
{
  if (dirty_texture_binds_ == 0) {
    return;
  }
  uint64_t dirty_bind = dirty_texture_binds_;
  dirty_texture_binds_ = 0;

  int first = bitscan_forward_uint64(dirty_bind);
  int last = 64 - bitscan_reverse_uint64(dirty_bind);
  int count = last - first;

  if (GLContext::multi_bind_support) {
    glBindTextures(first, count, textures_ + first);
    glBindSamplers(first, count, samplers_ + first);
  }
  else {
    for (int unit = first; unit < last; unit++) {
      if ((dirty_bind >> unit) & 1UL) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(targets_[unit], textures_[unit]);
        glBindSampler(unit, samplers_[unit]);
      }
    }
  }
}

void GLStateManager::texture_unpack_row_length_set(uint len)
{
  glPixelStorei(GL_UNPACK_ROW_LENGTH, len);
}

uint64_t GLStateManager::bound_texture_slots()
{
  uint64_t bound_slots = 0;
  for (int i = 0; i < ARRAY_SIZE(textures_); i++) {
    if (textures_[i] != 0) {
      bound_slots |= 1ULL << i;
    }
  }
  return bound_slots;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Binding (from image load store)
 * \{ */

void GLStateManager::image_bind(Texture *tex_, int unit)
{
  /* Minimum support is 8 image in the fragment shader. No image for other stages. */
  BLI_assert(unit < 8);
  GLTexture *tex = static_cast<GLTexture *>(tex_);
  if (G.debug & G_DEBUG_GPU) {
    tex->check_feedback_loop();
  }
  images_[unit] = tex->tex_id_;
  formats_[unit] = to_gl_internal_format(tex->format_);
  image_formats[unit] = TextureWriteFormat(tex->format_get());
  tex->is_bound_image_ = true;
  dirty_image_binds_ |= 1ULL << unit;
}

void GLStateManager::image_unbind(Texture *tex_)
{
  GLTexture *tex = static_cast<GLTexture *>(tex_);
  if (!tex->is_bound_image_) {
    return;
  }

  GLuint tex_id = tex->tex_id_;
  for (int i = 0; i < ARRAY_SIZE(images_); i++) {
    if (images_[i] == tex_id) {
      images_[i] = 0;
      image_formats[i] = TextureWriteFormat::Invalid;
      dirty_image_binds_ |= 1ULL << i;
    }
  }
  tex->is_bound_image_ = false;
}

void GLStateManager::image_unbind_all()
{
  for (int i = 0; i < ARRAY_SIZE(images_); i++) {
    if (images_[i] != 0) {
      images_[i] = 0;
      dirty_image_binds_ |= 1ULL << i;
    }
  }
  image_formats.fill(TextureWriteFormat::Invalid);
  this->image_bind_apply();
}

void GLStateManager::image_bind_apply()
{
  if (dirty_image_binds_ == 0) {
    return;
  }
  uint32_t dirty_bind = dirty_image_binds_;
  dirty_image_binds_ = 0;

  int first = bitscan_forward_uint(dirty_bind);
  int last = 32 - bitscan_reverse_uint(dirty_bind);
  int count = last - first;

  if (GLContext::multi_bind_image_support) {
    glBindImageTextures(first, count, images_ + first);
  }
  else {
    for (int unit = first; unit < last; unit++) {
      if ((dirty_bind >> unit) & 1UL) {
        glBindImageTexture(unit, images_[unit], 0, GL_TRUE, 0, GL_READ_WRITE, formats_[unit]);
      }
    }
  }
}

uint8_t GLStateManager::bound_image_slots()
{
  uint8_t bound_slots = 0;
  for (int i = 0; i < ARRAY_SIZE(images_); i++) {
    if (images_[i] != 0) {
      bound_slots |= 1ULL << i;
    }
  }
  return bound_slots;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Memory barrier
 * \{ */

void GLStateManager::issue_barrier(GPUBarrier barrier_bits)
{
  glMemoryBarrier(to_gl(barrier_bits));
}

GLFence::~GLFence()
{
  if (gl_sync_ != nullptr) {
    glDeleteSync(gl_sync_);
    gl_sync_ = nullptr;
  }
}

void GLFence::signal()
{
  /* If fence is already signaled, create a newly signaled fence primitive. */
  if (gl_sync_) {
    glDeleteSync(gl_sync_);
  }

  gl_sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  signalled_ = true;
}

void GLFence::wait()
{
  /* Do not wait if fence does not yet exist. */
  if (gl_sync_ == nullptr) {
    return;
  }
  glWaitSync(gl_sync_, 0, GL_TIMEOUT_IGNORED);
  signalled_ = false;
}
/** \} */

}  // namespace blender::gpu
