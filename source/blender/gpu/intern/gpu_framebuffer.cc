/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "GPU_batch.h"
#include "GPU_extensions.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_private.h"
#include "gpu_texture_private.hh"

#include "gpu_framebuffer_private.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Constructor / Destructor
 * \{ */

FrameBuffer::FrameBuffer(const char *name)
{
  if (name) {
    BLI_strncpy(name_, name, sizeof(name_));
  }
  else {
    name_[0] = '\0';
  }
  /* Force config on first use. */
  dirty_attachments_ = true;
  dirty_state_ = true;

  for (int i = 0; i < ARRAY_SIZE(attachments_); i++) {
    attachments_[i].tex = NULL;
    attachments_[i].mip = -1;
    attachments_[i].layer = -1;
  }
}

FrameBuffer::~FrameBuffer()
{
  GPUFrameBuffer *gpu_fb = reinterpret_cast<GPUFrameBuffer *>(this);
  for (int i = 0; i < ARRAY_SIZE(attachments_); i++) {
    if (attachments_[i].tex != NULL) {
      GPU_texture_detach_framebuffer(attachments_[i].tex, gpu_fb);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attachments managment
 * \{ */

void FrameBuffer::attachment_set(GPUAttachmentType type, const GPUAttachment &new_attachment)
{
  if (new_attachment.mip == -1) {
    return; /* GPU_ATTACHMENT_LEAVE */
  }

  if (type >= GPU_FB_MAX_ATTACHEMENT) {
    fprintf(stderr,
            "GPUFramebuffer: Error: Trying to attach texture to type %d but maximum slot is %d.\n",
            type - GPU_FB_COLOR_ATTACHMENT0,
            GPU_FB_MAX_COLOR_ATTACHMENT);
    return;
  }

  if (new_attachment.tex) {
    if (new_attachment.layer > 0) {
      BLI_assert(ELEM(GPU_texture_target(new_attachment.tex),
                      GL_TEXTURE_2D_ARRAY,
                      GL_TEXTURE_CUBE_MAP,
                      GL_TEXTURE_CUBE_MAP_ARRAY_ARB));
    }
    if (GPU_texture_stencil(new_attachment.tex)) {
      BLI_assert(ELEM(type, GPU_FB_DEPTH_STENCIL_ATTACHMENT));
    }
    else if (GPU_texture_depth(new_attachment.tex)) {
      BLI_assert(ELEM(type, GPU_FB_DEPTH_ATTACHMENT));
    }
  }

  GPUAttachment &attachment = attachments_[type];

  if (attachment.tex == new_attachment.tex && attachment.layer == new_attachment.layer &&
      attachment.mip == new_attachment.mip) {
    return; /* Exact same texture already bound here. */
  }
  /* Unbind previous and bind new. */
  /* TODO(fclem) cleanup the casts. */
  if (attachment.tex) {
    GPU_texture_detach_framebuffer(attachment.tex, reinterpret_cast<GPUFrameBuffer *>(this));
  }

  attachment = new_attachment;

  /* Might be null if this is for unbinding. */
  if (attachment.tex) {
    GPU_texture_attach_framebuffer(attachment.tex, reinterpret_cast<GPUFrameBuffer *>(this), type);
  }
  else {
    /* GPU_ATTACHMENT_NONE */
  }

  dirty_attachments_ = true;
}

void FrameBuffer::recursive_downsample(int max_lvl,
                                       void (*callback)(void *userData, int level),
                                       void *userData)
{
  GPUContext *ctx = GPU_context_active_get();
  /* Bind to make sure the framebuffer is up to date. */
  this->bind(true);

  if (width_ == 1 && height_ == 1) {
    return;
  }
  /* HACK: Make the framebuffer appear not bound to avoid assert in GPU_texture_bind. */
  ctx->active_fb = NULL;

  int levels = floor(log2(max_ii(width_, height_)));
  max_lvl = min_ii(max_lvl, levels);

  int current_dim[2] = {width_, height_};
  int mip_lvl;
  for (mip_lvl = 1; mip_lvl < max_lvl + 1; mip_lvl++) {
    /* calculate next viewport size */
    current_dim[0] = max_ii(current_dim[0] / 2, 1);
    current_dim[1] = max_ii(current_dim[1] / 2, 1);
    /* Replace attaached miplevel for each attachement. */
    for (int att = 0; att < ARRAY_SIZE(attachments_); att++) {
      GPUTexture *tex = attachments_[att].tex;
      if (tex != NULL) {
        /* Some Intel HDXXX have issue with rendering to a mipmap that is below
         * the texture GL_TEXTURE_MAX_LEVEL. So even if it not correct, in this case
         * we allow GL_TEXTURE_MAX_LEVEL to be one level lower. In practice it does work! */
        int map_lvl = (GPU_mip_render_workaround()) ? mip_lvl : (mip_lvl - 1);
        /* Restrict fetches only to previous level. */
        GPU_texture_bind(tex, 0);
        glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_BASE_LEVEL, mip_lvl - 1);
        glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_MAX_LEVEL, map_lvl);
        GPU_texture_unbind(tex);
        /* Bind next level. */
        attachments_[att].mip = mip_lvl;
      }
    }
    /* Update the internal attachments and viewport size. */
    dirty_attachments_ = true;
    this->bind(true);
    /* HACK: Make the framebuffer appear not bound to avoid assert in GPU_texture_bind. */
    ctx->active_fb = NULL;

    callback(userData, mip_lvl);

    /* This is the last mipmap level. Exit loop without incrementing mip_lvl. */
    if (current_dim[0] == 1 && current_dim[1] == 1) {
      break;
    }
  }

  for (int att = 0; att < ARRAY_SIZE(attachments_); att++) {
    if (attachments_[att].tex != NULL) {
      /* Reset mipmap level range. */
      GPUTexture *tex = attachments_[att].tex;
      GPU_texture_bind(tex, 0);
      glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_BASE_LEVEL, 0);
      glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_MAX_LEVEL, mip_lvl);
      GPU_texture_unbind(tex);
      /* Reset base level. NOTE: might not be the one bound at the start of this function. */
      attachments_[att].mip = 0;
    }
  }
  dirty_attachments_ = true;
}

/** \} */

}  // namespace blender::gpu

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

using namespace blender;
using namespace blender::gpu;

GPUFrameBuffer *GPU_framebuffer_create(const char *name)
{
  /* We generate the FB object later at first use in order to
   * create the framebuffer in the right opengl context. */
  return (GPUFrameBuffer *)GPUBackend::get()->framebuffer_alloc(name);
}

void GPU_framebuffer_free(GPUFrameBuffer *gpu_fb)
{
  delete reinterpret_cast<FrameBuffer *>(gpu_fb);
}

/* ---------- Binding ----------- */

void GPU_framebuffer_bind(GPUFrameBuffer *gpu_fb)
{
  FrameBuffer *fb = reinterpret_cast<FrameBuffer *>(gpu_fb);
  const bool enable_srgb = true;
  fb->bind(enable_srgb);
}

/* Workaround for binding a srgb framebuffer without doing the srgb transform. */
void GPU_framebuffer_bind_no_srgb(GPUFrameBuffer *gpu_fb)
{
  FrameBuffer *fb = reinterpret_cast<FrameBuffer *>(gpu_fb);
  const bool enable_srgb = false;
  fb->bind(enable_srgb);
}

/* For stereo rendering. */
void GPU_backbuffer_bind(eGPUBackBuffer buffer)
{
  GPUContext *ctx = GPU_context_active_get();

  if (buffer == GPU_BACKBUFFER_LEFT) {
    ctx->back_left->bind(false);
  }
  else {
    ctx->back_right->bind(false);
  }
}

void GPU_framebuffer_restore(void)
{
  GPU_context_active_get()->back_left->bind(false);
}

GPUFrameBuffer *GPU_framebuffer_active_get(void)
{
  GPUContext *ctx = GPU_context_active_get();
  return reinterpret_cast<GPUFrameBuffer *>(ctx ? ctx->active_fb : NULL);
}

/* Returns the default framebuffer. Will always exists even if it's just a dummy. */
GPUFrameBuffer *GPU_framebuffer_back_get(void)
{
  GPUContext *ctx = GPU_context_active_get();
  return reinterpret_cast<GPUFrameBuffer *>(ctx ? ctx->back_left : NULL);
}

bool GPU_framebuffer_bound(GPUFrameBuffer *gpu_fb)
{
  return (gpu_fb == GPU_framebuffer_active_get());
}

/* ---------- Attachment Management ----------- */

bool GPU_framebuffer_check_valid(GPUFrameBuffer *gpu_fb, char err_out[256])
{
  return reinterpret_cast<FrameBuffer *>(gpu_fb)->check(err_out);
}

void GPU_framebuffer_texture_attach_ex(GPUFrameBuffer *gpu_fb, GPUAttachment attachement, int slot)
{
  GPUAttachmentType type = blender::gpu::Texture::attachment_type(attachement.tex, slot);
  reinterpret_cast<FrameBuffer *>(gpu_fb)->attachment_set(type, attachement);
}

void GPU_framebuffer_texture_attach(GPUFrameBuffer *fb, GPUTexture *tex, int slot, int mip)
{
  GPUAttachment attachement = GPU_ATTACHMENT_TEXTURE_MIP(tex, mip);
  GPU_framebuffer_texture_attach_ex(fb, attachement, slot);
}

void GPU_framebuffer_texture_layer_attach(
    GPUFrameBuffer *fb, GPUTexture *tex, int slot, int layer, int mip)
{
  GPUAttachment attachement = GPU_ATTACHMENT_TEXTURE_LAYER_MIP(tex, layer, mip);
  GPU_framebuffer_texture_attach_ex(fb, attachement, slot);
}

void GPU_framebuffer_texture_cubeface_attach(
    GPUFrameBuffer *fb, GPUTexture *tex, int slot, int face, int mip)
{
  GPUAttachment attachement = GPU_ATTACHMENT_TEXTURE_CUBEFACE_MIP(tex, face, mip);
  GPU_framebuffer_texture_attach_ex(fb, attachement, slot);
}

void GPU_framebuffer_texture_detach(GPUFrameBuffer *gpu_fb, GPUTexture *tex)
{
  GPUAttachment attachement = GPU_ATTACHMENT_NONE;
  int type = GPU_texture_framebuffer_attachement_get(tex, gpu_fb);
  if (type != -1) {
    reinterpret_cast<FrameBuffer *>(gpu_fb)->attachment_set((GPUAttachmentType)type, attachement);
  }
  else {
    BLI_assert(!"Error: Texture: Framebuffer is not attached");
  }
}

/**
 * First GPUAttachment in *config is always the depth/depth_stencil buffer.
 * Following GPUAttachments are color buffers.
 * Setting GPUAttachment.mip to -1 will leave the texture in this slot.
 * Setting GPUAttachment.tex to NULL will detach the texture in this slot.
 */
void GPU_framebuffer_config_array(GPUFrameBuffer *gpu_fb,
                                  const GPUAttachment *config,
                                  int config_len)
{
  FrameBuffer *fb = reinterpret_cast<FrameBuffer *>(gpu_fb);

  const GPUAttachment &depth_attachement = config[0];
  Span<GPUAttachment> color_attachments(config + 1, config_len - 1);

  if (depth_attachement.mip == -1) {
    /* GPU_ATTACHMENT_LEAVE */
  }
  else if (depth_attachement.tex == NULL) {
    /* GPU_ATTACHMENT_NONE: Need to clear both targets. */
    fb->attachment_set(GPU_FB_DEPTH_STENCIL_ATTACHMENT, depth_attachement);
    fb->attachment_set(GPU_FB_DEPTH_ATTACHMENT, depth_attachement);
  }
  else {
    GPUAttachmentType type = GPU_texture_stencil(depth_attachement.tex) ?
                                 GPU_FB_DEPTH_STENCIL_ATTACHMENT :
                                 GPU_FB_DEPTH_ATTACHMENT;
    fb->attachment_set(type, depth_attachement);
  }

  GPUAttachmentType type = GPU_FB_COLOR_ATTACHMENT0;
  for (const GPUAttachment &attachement : color_attachments) {
    fb->attachment_set(type, attachement);
    ++type;
  }
}

/* ---------- Viewport & Scissor Region ----------- */

/* Viewport and scissor size is stored per framebuffer.
 * It is only reset to its original dimensions explicitely OR when binding the framebuffer after
 * modifiying its attachments. */
void GPU_framebuffer_viewport_set(GPUFrameBuffer *gpu_fb, int x, int y, int width, int height)
{
  int viewport_rect[4] = {x, y, width, height};
  reinterpret_cast<FrameBuffer *>(gpu_fb)->viewport_set(viewport_rect);
}

void GPU_framebuffer_viewport_get(GPUFrameBuffer *gpu_fb, int r_viewport[4])
{
  reinterpret_cast<FrameBuffer *>(gpu_fb)->viewport_get(r_viewport);
}

/* Reset to its attachement(s) size. */
void GPU_framebuffer_viewport_reset(GPUFrameBuffer *gpu_fb)
{
  reinterpret_cast<FrameBuffer *>(gpu_fb)->viewport_reset();
}

/* ---------- Framebuffer Operations ----------- */

void GPU_framebuffer_clear(GPUFrameBuffer *gpu_fb,
                           eGPUFrameBufferBits buffers,
                           const float clear_col[4],
                           float clear_depth,
                           uint clear_stencil)
{
  reinterpret_cast<FrameBuffer *>(gpu_fb)->clear(buffers, clear_col, clear_depth, clear_stencil);
}

/* Clear all textures attached to this framebuffer with a different color. */
void GPU_framebuffer_multi_clear(GPUFrameBuffer *gpu_fb, const float (*clear_cols)[4])
{
  reinterpret_cast<FrameBuffer *>(gpu_fb)->clear_multi(clear_cols);
}

void GPU_clear_color(float red, float green, float blue, float alpha)
{
  float clear_col[4] = {red, green, blue, alpha};
  GPU_context_active_get()->active_fb->clear(GPU_COLOR_BIT, clear_col, 0.0f, 0x0);
}

void GPU_clear_depth(float depth)
{
  float clear_col[4] = {0};
  GPU_context_active_get()->active_fb->clear(GPU_DEPTH_BIT, clear_col, depth, 0x0);
}

void GPU_framebuffer_read_depth(GPUFrameBuffer *gpu_fb, int x, int y, int w, int h, float *data)
{
  int rect[4] = {x, y, w, h};
  reinterpret_cast<FrameBuffer *>(gpu_fb)->read(GPU_DEPTH_BIT, GPU_DATA_FLOAT, rect, 1, 1, data);
}

void GPU_framebuffer_read_color(GPUFrameBuffer *gpu_fb,
                                int x,
                                int y,
                                int w,
                                int h,
                                int channels,
                                int slot,
                                eGPUDataFormat format,
                                void *data)
{
  int rect[4] = {x, y, w, h};
  reinterpret_cast<FrameBuffer *>(gpu_fb)->read(GPU_COLOR_BIT, format, rect, channels, slot, data);
}

/* TODO(fclem) rename to read_color. */
void GPU_frontbuffer_read_pixels(
    int x, int y, int w, int h, int channels, eGPUDataFormat format, void *data)
{
  int rect[4] = {x, y, w, h};
  GPU_context_active_get()->front_left->read(GPU_COLOR_BIT, format, rect, channels, 0, data);
}

/* read_slot and write_slot are only used for color buffers. */
/* TODO(fclem) port as texture operation. */
void GPU_framebuffer_blit(GPUFrameBuffer *gpufb_read,
                          int read_slot,
                          GPUFrameBuffer *gpufb_write,
                          int write_slot,
                          eGPUFrameBufferBits blit_buffers)
{
  FrameBuffer *fb_read = reinterpret_cast<FrameBuffer *>(gpufb_read);
  FrameBuffer *fb_write = reinterpret_cast<FrameBuffer *>(gpufb_write);
  BLI_assert(blit_buffers != 0);

  FrameBuffer *prev_fb = GPU_context_active_get()->active_fb;

#ifndef NDEBUG
  GPUTexture *read_tex, *write_tex;
  if (blit_buffers & (GPU_DEPTH_BIT | GPU_STENCIL_BIT)) {
    read_tex = fb_read->depth_tex();
    write_tex = fb_write->depth_tex();
  }
  else {
    read_tex = fb_read->color_tex(read_slot);
    write_tex = fb_write->color_tex(write_slot);
  }

  if (blit_buffers & GPU_DEPTH_BIT) {
    BLI_assert(GPU_texture_depth(read_tex) && GPU_texture_depth(write_tex));
    BLI_assert(GPU_texture_format(read_tex) == GPU_texture_format(write_tex));
  }
  if (blit_buffers & GPU_STENCIL_BIT) {
    BLI_assert(GPU_texture_stencil(read_tex) && GPU_texture_stencil(write_tex));
    BLI_assert(GPU_texture_format(read_tex) == GPU_texture_format(write_tex));
  }
  if (GPU_texture_samples(write_tex) != 0 || GPU_texture_samples(read_tex) != 0) {
    /* Can only blit multisample textures to another texture of the same size. */
    BLI_assert((GPU_texture_width(write_tex) == GPU_texture_width(read_tex)) &&
               (GPU_texture_height(write_tex) == GPU_texture_height(read_tex)));
  }
#endif

  fb_read->blit_to(blit_buffers, read_slot, fb_write, write_slot, 0, 0);

  /* FIXME(fclem) sRGB is not saved. */
  prev_fb->bind(true);
}

/**
 * Use this if you need to custom down-sample your texture and use the previous mip level as
 * input. This function only takes care of the correct texture handling. It execute the callback
 * for each texture level.
 */
void GPU_framebuffer_recursive_downsample(GPUFrameBuffer *gpu_fb,
                                          int max_lvl,
                                          void (*callback)(void *userData, int level),
                                          void *userData)
{
  reinterpret_cast<FrameBuffer *>(gpu_fb)->recursive_downsample(max_lvl, callback, userData);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUOffScreen
 *
 * Container that holds a framebuffer and its textures.
 * Might be bound to multiple contexts.
 * \{ */

#define FRAMEBUFFER_STACK_DEPTH 16

static struct {
  GPUFrameBuffer *framebuffers[FRAMEBUFFER_STACK_DEPTH];
  uint top;
} FrameBufferStack = {{0}};

static void gpuPushFrameBuffer(GPUFrameBuffer *fb)
{
  BLI_assert(FrameBufferStack.top < FRAMEBUFFER_STACK_DEPTH);
  FrameBufferStack.framebuffers[FrameBufferStack.top] = fb;
  FrameBufferStack.top++;
}

static GPUFrameBuffer *gpuPopFrameBuffer(void)
{
  BLI_assert(FrameBufferStack.top > 0);
  FrameBufferStack.top--;
  return FrameBufferStack.framebuffers[FrameBufferStack.top];
}

#undef FRAMEBUFFER_STACK_DEPTH

#define MAX_CTX_FB_LEN 3

struct GPUOffScreen {
  struct {
    GPUContext *ctx;
    GPUFrameBuffer *fb;
  } framebuffers[MAX_CTX_FB_LEN];

  GPUTexture *color;
  GPUTexture *depth;

  /** Saved state of the previously bound framebuffer. */
  /* TODO(fclem) This is quite hacky and a proper fix would be to
   * put these states directly inside the GPUFrambuffer.
   * But we don't have a GPUFramebuffer for the default framebuffer yet. */
  int saved_viewport[4];
  int saved_scissor[4];
};

/* Returns the correct framebuffer for the current context. */
static GPUFrameBuffer *gpu_offscreen_fb_get(GPUOffScreen *ofs)
{
  GPUContext *ctx = GPU_context_active_get();
  BLI_assert(ctx);

  for (int i = 0; i < MAX_CTX_FB_LEN; i++) {
    if (ofs->framebuffers[i].fb == NULL) {
      ofs->framebuffers[i].ctx = ctx;
      GPU_framebuffer_ensure_config(&ofs->framebuffers[i].fb,
                                    {
                                        GPU_ATTACHMENT_TEXTURE(ofs->depth),
                                        GPU_ATTACHMENT_TEXTURE(ofs->color),
                                    });
    }

    if (ofs->framebuffers[i].ctx == ctx) {
      return ofs->framebuffers[i].fb;
    }
  }

  /* List is full, this should never happen or
   * it might just slow things down if it happens
   * regularly. In this case we just empty the list
   * and start over. This is most likely never going
   * to happen under normal usage. */
  BLI_assert(0);
  printf(
      "Warning: GPUOffscreen used in more than 3 GPUContext. "
      "This may create performance drop.\n");

  for (int i = 0; i < MAX_CTX_FB_LEN; i++) {
    GPU_framebuffer_free(ofs->framebuffers[i].fb);
    ofs->framebuffers[i].fb = NULL;
  }

  return gpu_offscreen_fb_get(ofs);
}

GPUOffScreen *GPU_offscreen_create(
    int width, int height, bool depth, bool high_bitdepth, char err_out[256])
{
  GPUOffScreen *ofs = (GPUOffScreen *)MEM_callocN(sizeof(GPUOffScreen), __func__);

  /* Sometimes areas can have 0 height or width and this will
   * create a 1D texture which we don't want. */
  height = max_ii(1, height);
  width = max_ii(1, width);

  ofs->color = GPU_texture_create_2d(
      width, height, (high_bitdepth) ? GPU_RGBA16F : GPU_RGBA8, NULL, err_out);

  if (depth) {
    ofs->depth = GPU_texture_create_2d(width, height, GPU_DEPTH24_STENCIL8, NULL, err_out);
  }

  if ((depth && !ofs->depth) || !ofs->color) {
    GPU_offscreen_free(ofs);
    return NULL;
  }

  GPUFrameBuffer *fb = gpu_offscreen_fb_get(ofs);

  /* check validity at the very end! */
  if (!GPU_framebuffer_check_valid(fb, err_out)) {
    GPU_offscreen_free(ofs);
    return NULL;
  }
  GPU_framebuffer_restore();
  return ofs;
}

void GPU_offscreen_free(GPUOffScreen *ofs)
{
  for (int i = 0; i < MAX_CTX_FB_LEN; i++) {
    if (ofs->framebuffers[i].fb) {
      GPU_framebuffer_free(ofs->framebuffers[i].fb);
    }
  }
  if (ofs->color) {
    GPU_texture_free(ofs->color);
  }
  if (ofs->depth) {
    GPU_texture_free(ofs->depth);
  }

  MEM_freeN(ofs);
}

void GPU_offscreen_bind(GPUOffScreen *ofs, bool save)
{
  if (save) {
    GPU_scissor_get(ofs->saved_scissor);
    GPU_viewport_size_get_i(ofs->saved_viewport);

    GPUFrameBuffer *fb = GPU_framebuffer_active_get();
    gpuPushFrameBuffer(reinterpret_cast<GPUFrameBuffer *>(fb));
  }
  GPUFrameBuffer *ofs_fb = gpu_offscreen_fb_get(ofs);
  GPU_framebuffer_bind(ofs_fb);
  glDisable(GL_FRAMEBUFFER_SRGB);
  GPU_scissor_test(false);
  GPU_shader_set_framebuffer_srgb_target(false);
}

void GPU_offscreen_unbind(GPUOffScreen *ofs, bool restore)
{
  GPUFrameBuffer *fb = NULL;

  if (restore) {
    GPU_scissor(UNPACK4(ofs->saved_scissor));
    GPU_viewport(UNPACK4(ofs->saved_viewport));
    fb = gpuPopFrameBuffer();
  }

  if (fb) {
    GPU_framebuffer_bind(fb);
  }
  else {
    GPU_framebuffer_restore();
  }
}

void GPU_offscreen_draw_to_screen(GPUOffScreen *ofs, int x, int y)
{
  GPUContext *ctx = GPU_context_active_get();
  FrameBuffer *ofs_fb = reinterpret_cast<FrameBuffer *>(gpu_offscreen_fb_get(ofs));
  ofs_fb->blit_to(GPU_COLOR_BIT, 0, ctx->active_fb, 0, x, y);
}

void GPU_offscreen_read_pixels(GPUOffScreen *ofs, eGPUDataFormat format, void *pixels)
{
  BLI_assert(ELEM(format, GPU_DATA_UNSIGNED_BYTE, GPU_DATA_FLOAT));

  const int w = GPU_texture_width(ofs->color);
  const int h = GPU_texture_height(ofs->color);

  GPUFrameBuffer *ofs_fb = gpu_offscreen_fb_get(ofs);
  GPU_framebuffer_read_color(ofs_fb, 0, 0, w, h, 4, 0, format, pixels);
}

int GPU_offscreen_width(const GPUOffScreen *ofs)
{
  return GPU_texture_width(ofs->color);
}

int GPU_offscreen_height(const GPUOffScreen *ofs)
{
  return GPU_texture_height(ofs->color);
}

GPUTexture *GPU_offscreen_color_texture(const GPUOffScreen *ofs)
{
  return ofs->color;
}

/* only to be used by viewport code! */
void GPU_offscreen_viewport_data_get(GPUOffScreen *ofs,
                                     GPUFrameBuffer **r_fb,
                                     GPUTexture **r_color,
                                     GPUTexture **r_depth)
{
  *r_fb = gpu_offscreen_fb_get(ofs);
  *r_color = ofs->color;
  *r_depth = ofs->depth;
}

/** \} */