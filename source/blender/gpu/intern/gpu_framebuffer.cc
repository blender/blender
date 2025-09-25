/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "GPU_capabilities.hh"
#include "GPU_texture.hh"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_texture_private.hh"

#include "gpu_framebuffer_private.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Constructor / Destructor
 * \{ */

FrameBuffer::FrameBuffer(const char *name)
{
  if (name) {
    STRNCPY(name_, name);
  }
  else {
    name_[0] = '\0';
  }
  /* Force config on first use. */
  dirty_attachments_ = true;
  dirty_state_ = true;

  for (GPUAttachment &attachment : attachments_) {
    attachment.tex = nullptr;
    attachment.mip = -1;
    attachment.layer = -1;
  }
}

FrameBuffer::~FrameBuffer()
{
  for (GPUAttachment &attachment : attachments_) {
    if (attachment.tex != nullptr) {
      reinterpret_cast<Texture *>(attachment.tex)->detach_from(this);
    }
  }

#ifndef GPU_NO_USE_PY_REFERENCES
  if (this->py_ref) {
    *this->py_ref = nullptr;
  }
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attachments Management
 * \{ */

void FrameBuffer::attachment_set(GPUAttachmentType type, const GPUAttachment &new_attachment)
{
  if (new_attachment.mip == -1) {
    return; /* GPU_ATTACHMENT_LEAVE */
  }

  if (type >= GPU_FB_MAX_ATTACHMENT) {
    fprintf(stderr,
            "GPUFramebuffer: Error: Trying to attach texture to type %d but maximum slot is %d.\n",
            type - GPU_FB_COLOR_ATTACHMENT0,
            GPU_FB_MAX_COLOR_ATTACHMENT);
    return;
  }

  if (new_attachment.tex) {
    if (new_attachment.layer > 0) {
      BLI_assert(GPU_texture_is_cube(new_attachment.tex) ||
                 GPU_texture_is_array(new_attachment.tex));
    }
    if (GPU_texture_has_stencil_format(new_attachment.tex)) {
      BLI_assert(ELEM(type, GPU_FB_DEPTH_STENCIL_ATTACHMENT));
    }
    else if (GPU_texture_has_depth_format(new_attachment.tex)) {
      BLI_assert(ELEM(type, GPU_FB_DEPTH_ATTACHMENT));
    }
  }

  GPUAttachment &attachment = attachments_[type];

  set_color_attachment_bit(type, new_attachment.tex != nullptr);

  if (attachment.tex == new_attachment.tex && attachment.layer == new_attachment.layer &&
      attachment.mip == new_attachment.mip)
  {
    return; /* Exact same texture already bound here. */
  }
  /* Unbind previous and bind new. */
  /* TODO(fclem): cleanup the casts. */
  if (attachment.tex) {
    reinterpret_cast<Texture *>(attachment.tex)->detach_from(this);
  }

  /* Might be null if this is for unbinding. */
  if (new_attachment.tex) {
    reinterpret_cast<Texture *>(new_attachment.tex)->attach_to(this, type);
  }
  else {
    /* GPU_ATTACHMENT_NONE */
  }

  attachment = new_attachment;
  dirty_attachments_ = true;
}

void FrameBuffer::attachment_remove(GPUAttachmentType type)
{
  attachments_[type] = GPU_ATTACHMENT_NONE;
  dirty_attachments_ = true;
  set_color_attachment_bit(type, false);
}

void FrameBuffer::subpass_transition(const GPUAttachmentState depth_attachment_state,
                                     Span<GPUAttachmentState> color_attachment_states)
{
  /* NOTE: Depth is not supported as input attachment because the Metal API doesn't support it and
   * because depth is not compatible with the framebuffer fetch implementation. */
  BLI_assert(depth_attachment_state != GPU_ATTACHMENT_READ);

  if (!attachments_[GPU_FB_DEPTH_ATTACHMENT].tex &&
      !attachments_[GPU_FB_DEPTH_STENCIL_ATTACHMENT].tex)
  {
    BLI_assert(depth_attachment_state == GPU_ATTACHMENT_IGNORE);
  }

  BLI_assert(color_attachment_states.size() <= GPU_FB_MAX_COLOR_ATTACHMENT);
  for (int i : IndexRange(GPU_FB_MAX_COLOR_ATTACHMENT)) {
    GPUAttachmentType type = GPU_FB_COLOR_ATTACHMENT0 + i;
    if (this->attachments_[type].tex) {
      BLI_assert(i < color_attachment_states.size());
      set_color_attachment_bit(type, color_attachment_states[i] == GPU_ATTACHMENT_WRITE);
    }
    else {
      BLI_assert(i >= color_attachment_states.size() ||
                 color_attachment_states[i] == GPU_ATTACHMENT_IGNORE);
    }
  }

  subpass_transition_impl(depth_attachment_state, color_attachment_states);
}

void FrameBuffer::load_store_config_array(const GPULoadStore *load_store_actions, uint actions_len)
{
  /* Follows attachment structure of GPU_framebuffer_config_array/GPU_framebuffer_ensure_config */
  const GPULoadStore &depth_action = load_store_actions[0];
  Span<GPULoadStore> color_attachment_actions(load_store_actions + 1, actions_len - 1);
  BLI_assert(color_attachment_actions.size() <= GPU_FB_MAX_COLOR_ATTACHMENT);

  if (!attachments_[GPU_FB_DEPTH_ATTACHMENT].tex &&
      !attachments_[GPU_FB_DEPTH_STENCIL_ATTACHMENT].tex)
  {
    BLI_assert(depth_action.load_action == GPU_LOADACTION_DONT_CARE &&
               depth_action.store_action == GPU_STOREACTION_DONT_CARE);
  }

  if (this->attachments_[GPU_FB_DEPTH_STENCIL_ATTACHMENT].tex) {
    this->attachment_set_loadstore_op(GPU_FB_DEPTH_STENCIL_ATTACHMENT, depth_action);
  }

  if (this->attachments_[GPU_FB_DEPTH_ATTACHMENT].tex) {
    this->attachment_set_loadstore_op(GPU_FB_DEPTH_ATTACHMENT, depth_action);
  }

  for (int i : IndexRange(GPU_FB_MAX_COLOR_ATTACHMENT)) {
    GPUAttachmentType type = GPU_FB_COLOR_ATTACHMENT0 + i;
    if (this->attachments_[type].tex) {
      BLI_assert(i < color_attachment_actions.size());
      this->attachment_set_loadstore_op(type, color_attachment_actions[i]);
    }
    else {
      BLI_assert(i >= color_attachment_actions.size() ||
                 (color_attachment_actions[i].load_action == GPU_LOADACTION_DONT_CARE &&
                  color_attachment_actions[i].store_action == GPU_STOREACTION_DONT_CARE));
    }
  }
}

uint FrameBuffer::get_bits_per_pixel()
{
  uint total_bits = 0;
  for (GPUAttachment &attachment : attachments_) {
    Texture *tex = reinterpret_cast<Texture *>(attachment.tex);
    if (tex != nullptr) {
      int bits = to_bytesize(tex->format_get()) * to_component_len(tex->format_get());
      total_bits += bits;
    }
  }
  return total_bits;
}

/** \} */

}  // namespace blender::gpu

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

using namespace blender;
using namespace blender::gpu;

gpu::FrameBuffer *GPU_framebuffer_create(const char *name)
{
  /* We generate the FB object later at first use in order to
   * create the frame-buffer in the right opengl context. */
  return GPUBackend::get()->framebuffer_alloc(name);
}

void GPU_framebuffer_free(gpu::FrameBuffer *fb)
{
  delete fb;
}

const char *GPU_framebuffer_get_name(gpu::FrameBuffer *fb)
{
  return fb->name_get();
}

/* ---------- Binding ----------- */

void GPU_framebuffer_bind(gpu::FrameBuffer *fb)
{
  const bool enable_srgb = true;
  /* Disable custom loadstore and bind. */
  fb->set_use_explicit_loadstore(false);
  fb->bind(enable_srgb);
}

void GPU_framebuffer_bind_loadstore(gpu::FrameBuffer *fb,
                                    const GPULoadStore *load_store_actions,
                                    uint actions_len)
{
  const bool enable_srgb = true;
  /* Bind with explicit loadstore state */
  fb->set_use_explicit_loadstore(true);
  fb->bind(enable_srgb);

  /* Update load store */
  fb->load_store_config_array(load_store_actions, actions_len);
}

void GPU_framebuffer_subpass_transition_array(gpu::FrameBuffer *fb,
                                              const GPUAttachmentState *attachment_states,
                                              uint attachment_len)
{
  fb->subpass_transition(attachment_states[0],
                         Span<GPUAttachmentState>(attachment_states + 1, attachment_len - 1));
}

void GPU_framebuffer_bind_no_srgb(gpu::FrameBuffer *fb)
{
  const bool enable_srgb = false;
  fb->bind(enable_srgb);
}

void GPU_backbuffer_bind(GPUBackBuffer back_buffer_type)
{
  Context *ctx = Context::get();

  if (back_buffer_type == GPU_BACKBUFFER_LEFT) {
    ctx->back_left->bind(false);
  }
  else {
    ctx->back_right->bind(false);
  }
}

void GPU_framebuffer_restore()
{
  Context::get()->back_left->bind(false);
}

gpu::FrameBuffer *GPU_framebuffer_active_get()
{
  Context *ctx = Context::get();
  return ctx ? ctx->active_fb : nullptr;
}

gpu::FrameBuffer *GPU_framebuffer_back_get()
{
  Context *ctx = Context::get();
  return ctx ? ctx->back_left : nullptr;
}

bool GPU_framebuffer_bound(gpu::FrameBuffer *gpu_fb)
{
  return (gpu_fb == GPU_framebuffer_active_get());
}

/* ---------- Attachment Management ----------- */

bool GPU_framebuffer_check_valid(gpu::FrameBuffer *gpu_fb, char err_out[256])
{
  return gpu_fb->check(err_out);
}

static void gpu_framebuffer_texture_attach_ex(gpu::FrameBuffer *gpu_fb,
                                              GPUAttachment attachment,
                                              int slot)
{
  Texture *tex = reinterpret_cast<Texture *>(attachment.tex);
  GPUAttachmentType type = tex->attachment_type(slot);
  gpu_fb->attachment_set(type, attachment);
}

void GPU_framebuffer_texture_attach(gpu::FrameBuffer *fb,
                                    blender::gpu::Texture *tex,
                                    int slot,
                                    int mip)
{
  GPUAttachment attachment = GPU_ATTACHMENT_TEXTURE_MIP(tex, mip);
  gpu_framebuffer_texture_attach_ex(fb, attachment, slot);
}

void GPU_framebuffer_texture_layer_attach(
    gpu::FrameBuffer *fb, blender::gpu::Texture *tex, int slot, int layer, int mip)
{
  GPUAttachment attachment = GPU_ATTACHMENT_TEXTURE_LAYER_MIP(tex, layer, mip);
  gpu_framebuffer_texture_attach_ex(fb, attachment, slot);
}

void GPU_framebuffer_texture_cubeface_attach(
    gpu::FrameBuffer *fb, blender::gpu::Texture *tex, int slot, int face, int mip)
{
  GPUAttachment attachment = GPU_ATTACHMENT_TEXTURE_CUBEFACE_MIP(tex, face, mip);
  gpu_framebuffer_texture_attach_ex(fb, attachment, slot);
}

void GPU_framebuffer_texture_detach(gpu::FrameBuffer *fb, blender::gpu::Texture *tex)
{
  tex->detach_from(fb);
}

void GPU_framebuffer_config_array(gpu::FrameBuffer *fb,
                                  const GPUAttachment *config,
                                  int config_len)
{
  const GPUAttachment &depth_attachment = config[0];
  Span<GPUAttachment> color_attachments(config + 1, config_len - 1);

  if (depth_attachment.mip == -1) {
    /* GPU_ATTACHMENT_LEAVE */
  }
  else if (depth_attachment.tex == nullptr) {
    /* GPU_ATTACHMENT_NONE: Need to clear both targets. */
    fb->attachment_set(GPU_FB_DEPTH_STENCIL_ATTACHMENT, depth_attachment);
    fb->attachment_set(GPU_FB_DEPTH_ATTACHMENT, depth_attachment);
  }
  else {
    GPUAttachmentType type = GPU_texture_has_stencil_format(depth_attachment.tex) ?
                                 GPU_FB_DEPTH_STENCIL_ATTACHMENT :
                                 GPU_FB_DEPTH_ATTACHMENT;
    fb->attachment_set(type, depth_attachment);
  }

  GPUAttachmentType type = GPU_FB_COLOR_ATTACHMENT0;
  for (const GPUAttachment &attachment : color_attachments) {
    fb->attachment_set(type, attachment);
    ++type;
  }
}

void GPU_framebuffer_default_size(gpu::FrameBuffer *gpu_fb, int width, int height)
{
  gpu_fb->default_size_set(width, height);
}

/* ---------- Viewport & Scissor Region ----------- */

void GPU_framebuffer_viewport_set(gpu::FrameBuffer *gpu_fb, int x, int y, int width, int height)
{
  int viewport_rect[4] = {x, y, width, height};
  gpu_fb->viewport_set(viewport_rect);
}

void GPU_framebuffer_multi_viewports_set(gpu::FrameBuffer *gpu_fb,
                                         const int viewport_rects[GPU_MAX_VIEWPORTS][4])
{
  gpu_fb->viewport_multi_set(viewport_rects);
}

void GPU_framebuffer_viewport_get(gpu::FrameBuffer *gpu_fb, int r_viewport[4])
{
  gpu_fb->viewport_get(r_viewport);
}

void GPU_framebuffer_viewport_reset(gpu::FrameBuffer *gpu_fb)
{
  gpu_fb->viewport_reset();
}

/* ---------- Frame-buffer Operations ----------- */

void GPU_framebuffer_clear(gpu::FrameBuffer *gpu_fb,
                           GPUFrameBufferBits buffers,
                           const float clear_col[4],
                           float clear_depth,
                           uint clear_stencil)
{
  BLI_assert_msg(gpu_fb->get_use_explicit_loadstore() == false,
                 "Using GPU_framebuffer_clear_* functions in conjunction with custom load-store "
                 "state via GPU_framebuffer_bind_ex is invalid.");
  gpu_fb->clear(buffers, clear_col, clear_depth, clear_stencil);
}

void GPU_framebuffer_clear_color(gpu::FrameBuffer *fb, const float clear_col[4])
{
  GPU_framebuffer_clear(fb, GPU_COLOR_BIT, clear_col, 0.0f, 0x00);
}

void GPU_framebuffer_clear_depth(gpu::FrameBuffer *fb, float clear_depth)
{
  GPU_framebuffer_clear(fb, GPU_DEPTH_BIT, nullptr, clear_depth, 0x00);
}

void GPU_framebuffer_clear_color_depth(gpu::FrameBuffer *fb,
                                       const float clear_col[4],
                                       float clear_depth)
{
  GPU_framebuffer_clear(fb, GPU_COLOR_BIT | GPU_DEPTH_BIT, clear_col, clear_depth, 0x00);
}

void GPU_framebuffer_clear_stencil(gpu::FrameBuffer *fb, uint clear_stencil)
{
  GPU_framebuffer_clear(fb, GPU_STENCIL_BIT, nullptr, 0.0f, clear_stencil);
}

void GPU_framebuffer_clear_depth_stencil(gpu::FrameBuffer *fb,
                                         float clear_depth,
                                         uint clear_stencil)
{
  GPU_framebuffer_clear(fb, GPU_DEPTH_BIT | GPU_STENCIL_BIT, nullptr, clear_depth, clear_stencil);
}

void GPU_framebuffer_clear_color_depth_stencil(gpu::FrameBuffer *fb,
                                               const float clear_col[4],
                                               float clear_depth,
                                               uint clear_stencil)
{
  GPU_framebuffer_clear(
      fb, GPU_COLOR_BIT | GPU_DEPTH_BIT | GPU_STENCIL_BIT, clear_col, clear_depth, clear_stencil);
}

void GPU_framebuffer_multi_clear(gpu::FrameBuffer *fb, const float (*clear_colors)[4])
{
  BLI_assert_msg(fb->get_use_explicit_loadstore() == false,
                 "Using GPU_framebuffer_clear_* functions in conjunction with custom load-store "
                 "state via GPU_framebuffer_bind_ex is invalid.");
  fb->clear_multi(clear_colors);
}

void GPU_clear_color(float red, float green, float blue, float alpha)
{
  BLI_assert_msg(Context::get()->active_fb->get_use_explicit_loadstore() == false,
                 "Using GPU_framebuffer_clear_* functions in conjunction with custom load-store "
                 "state via GPU_framebuffer_bind_ex is invalid.");
  float clear_col[4] = {red, green, blue, alpha};
  Context::get()->active_fb->clear(GPU_COLOR_BIT, clear_col, 0.0f, 0x0);
}

void GPU_clear_depth(float depth)
{
  BLI_assert_msg(Context::get()->active_fb->get_use_explicit_loadstore() == false,
                 "Using GPU_framebuffer_clear_* functions in conjunction with custom load-store "
                 "state via GPU_framebuffer_bind_ex is invalid.");
  float clear_col[4] = {0};
  Context::get()->active_fb->clear(GPU_DEPTH_BIT, clear_col, depth, 0x0);
}

void GPU_framebuffer_read_depth(
    gpu::FrameBuffer *fb, int x, int y, int w, int h, eGPUDataFormat format, void *data)
{
  int rect[4] = {x, y, w, h};
  fb->read(GPU_DEPTH_BIT, format, rect, 1, 1, data);
}

void GPU_framebuffer_read_color(gpu::FrameBuffer *fb,
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
  fb->read(GPU_COLOR_BIT, format, rect, channels, slot, data);
}

void GPU_frontbuffer_read_color(
    int x, int y, int w, int h, int channels, eGPUDataFormat format, void *data)
{
  int rect[4] = {x, y, w, h};
  Context::get()->front_left->read(GPU_COLOR_BIT, format, rect, channels, 0, data);
}

/* TODO(fclem): port as texture operation. */
void GPU_framebuffer_blit(gpu::FrameBuffer *fb_read,
                          int read_slot,
                          gpu::FrameBuffer *fb_write,
                          int write_slot,
                          GPUFrameBufferBits blit_buffers)
{
  BLI_assert(blit_buffers != 0);

  FrameBuffer *prev_fb = Context::get()->active_fb;

#ifndef NDEBUG
  blender::gpu::Texture *read_tex, *write_tex;
  if (blit_buffers & (GPU_DEPTH_BIT | GPU_STENCIL_BIT)) {
    read_tex = fb_read->depth_tex();
    write_tex = fb_write->depth_tex();
  }
  else {
    read_tex = fb_read->color_tex(read_slot);
    write_tex = fb_write->color_tex(write_slot);
  }

  if (blit_buffers & GPU_DEPTH_BIT) {
    BLI_assert(GPU_texture_has_depth_format(read_tex) && GPU_texture_has_depth_format(write_tex));
    BLI_assert(GPU_texture_format(read_tex) == GPU_texture_format(write_tex));
  }
  if (blit_buffers & GPU_STENCIL_BIT) {
    BLI_assert(GPU_texture_has_stencil_format(read_tex) &&
               GPU_texture_has_stencil_format(write_tex));
    BLI_assert(GPU_texture_format(read_tex) == GPU_texture_format(write_tex));
  }
#endif

  fb_read->blit_to(blit_buffers, read_slot, fb_write, write_slot, 0, 0);

  /* FIXME(@fclem): sRGB is not saved. */
  prev_fb->bind(true);
}

#ifndef GPU_NO_USE_PY_REFERENCES
void **GPU_framebuffer_py_reference_get(gpu::FrameBuffer *fb)
{
  return fb->py_ref;
}

void GPU_framebuffer_py_reference_set(gpu::FrameBuffer *fb, void **py_ref)
{
  BLI_assert(py_ref == nullptr || fb->py_ref == nullptr);
  fb->py_ref = py_ref;
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame-Buffer Stack
 *
 * Keeps track of frame-buffer binding operation to restore previously bound frame-buffers.
 * \{ */

#define FRAMEBUFFER_STACK_DEPTH 16

static struct {
  gpu::FrameBuffer *framebuffers[FRAMEBUFFER_STACK_DEPTH];
  uint top;
} FrameBufferStack = {{nullptr}};

void GPU_framebuffer_push(gpu::FrameBuffer *fb)
{
  BLI_assert(FrameBufferStack.top < FRAMEBUFFER_STACK_DEPTH);
  FrameBufferStack.framebuffers[FrameBufferStack.top] = fb;
  FrameBufferStack.top++;
}

gpu::FrameBuffer *GPU_framebuffer_pop()
{
  BLI_assert(FrameBufferStack.top > 0);
  FrameBufferStack.top--;
  return FrameBufferStack.framebuffers[FrameBufferStack.top];
}

uint GPU_framebuffer_stack_level_get()
{
  return FrameBufferStack.top;
}

#undef FRAMEBUFFER_STACK_DEPTH

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUOffScreen
 *
 * Container that holds a frame-buffer and its textures.
 * Might be bound to multiple contexts.
 * \{ */

struct GPUOffScreen {
  constexpr static int MAX_CTX_FB_LEN = 3;

  struct {
    Context *ctx;
    gpu::FrameBuffer *fb;
  } framebuffers[MAX_CTX_FB_LEN];

  blender::gpu::Texture *color;
  blender::gpu::Texture *depth;
};

/**
 * Returns the correct frame-buffer for the current context.
 */
static gpu::FrameBuffer *gpu_offscreen_fb_get(GPUOffScreen *ofs)
{
  Context *ctx = Context::get();
  BLI_assert(ctx);

  for (auto &framebuffer : ofs->framebuffers) {
    if (framebuffer.fb == nullptr) {
      framebuffer.ctx = ctx;
      GPU_framebuffer_ensure_config(&framebuffer.fb,
                                    {
                                        GPU_ATTACHMENT_TEXTURE(ofs->depth),
                                        GPU_ATTACHMENT_TEXTURE(ofs->color),
                                    });
    }

    if (framebuffer.ctx == ctx) {
      return framebuffer.fb;
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

  for (auto &framebuffer : ofs->framebuffers) {
    GPU_framebuffer_free(framebuffer.fb);
    framebuffer.fb = nullptr;
  }

  return gpu_offscreen_fb_get(ofs);
}

GPUOffScreen *GPU_offscreen_create(int width,
                                   int height,
                                   bool with_depth_buffer,
                                   blender::gpu::TextureFormat format,
                                   eGPUTextureUsage usage,
                                   bool clear,
                                   char err_out[256])
{
  GPUOffScreen *ofs = MEM_callocN<GPUOffScreen>(__func__);

  /* Sometimes areas can have 0 height or width and this will
   * create a 1D texture which we don't want. */
  height = max_ii(1, height);
  width = max_ii(1, width);

  /* Always add GPU_TEXTURE_USAGE_ATTACHMENT for convenience. */
  usage |= GPU_TEXTURE_USAGE_ATTACHMENT;

  ofs->color = GPU_texture_create_2d("ofs_color", width, height, 1, format, usage, nullptr);

  if (with_depth_buffer) {
    /* Format view flag is needed by Workbench Volumes to read the stencil view. */
    eGPUTextureUsage depth_usage = usage | GPU_TEXTURE_USAGE_FORMAT_VIEW;
    ofs->depth = GPU_texture_create_2d("ofs_depth",
                                       width,
                                       height,
                                       1,
                                       blender::gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8,
                                       depth_usage,
                                       nullptr);
  }

  if ((with_depth_buffer && !ofs->depth) || !ofs->color) {
    const char error[] = "blender::gpu::Texture: Texture allocation failed.";
    if (err_out) {
      BLI_strncpy(err_out, error, 256);
    }
    else {
      fprintf(stderr, "%s", error);
    }
    GPU_offscreen_free(ofs);
    return nullptr;
  }

  gpu::FrameBuffer *fb = gpu_offscreen_fb_get(ofs);

  /* check validity at the very end! */
  if (!GPU_framebuffer_check_valid(fb, err_out)) {
    GPU_offscreen_free(ofs);
    return nullptr;
  }

  if (clear) {
    float const clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float clear_depth = 0.0f;
    GPU_framebuffer_bind(fb);
    if (with_depth_buffer) {
      GPU_framebuffer_clear_color_depth(fb, clear_color, clear_depth);
    }
    else {
      GPU_framebuffer_clear_color(fb, clear_color);
    }
  }

  GPU_framebuffer_restore();
  return ofs;
}

void GPU_offscreen_free(GPUOffScreen *offscreen)
{
  for (auto &framebuffer : offscreen->framebuffers) {
    if (framebuffer.fb) {
      GPU_framebuffer_free(framebuffer.fb);
    }
  }
  if (offscreen->color) {
    GPU_texture_free(offscreen->color);
  }
  if (offscreen->depth) {
    GPU_texture_free(offscreen->depth);
  }

  MEM_freeN(offscreen);
}

void GPU_offscreen_bind(GPUOffScreen *offscreen, bool save)
{
  if (save) {
    gpu::FrameBuffer *fb = GPU_framebuffer_active_get();
    GPU_framebuffer_push(fb);
  }
  gpu_offscreen_fb_get(offscreen)->bind(false);
}

void GPU_offscreen_unbind(GPUOffScreen * /*offscreen*/, bool restore)
{
  gpu::FrameBuffer *fb = nullptr;
  if (restore) {
    fb = GPU_framebuffer_pop();
  }

  if (fb) {
    GPU_framebuffer_bind(fb);
  }
  else {
    GPU_framebuffer_restore();
  }
}

void GPU_offscreen_draw_to_screen(GPUOffScreen *offscreen, int x, int y)
{
  Context *ctx = Context::get();
  FrameBuffer *ofs_fb = gpu_offscreen_fb_get(offscreen);
  ofs_fb->blit_to(GPU_COLOR_BIT, 0, ctx->active_fb, 0, x, y);
}

void GPU_offscreen_read_color_region(
    GPUOffScreen *offscreen, eGPUDataFormat format, int x, int y, int w, int h, void *r_data)
{
  BLI_assert(ELEM(format, GPU_DATA_UBYTE, GPU_DATA_FLOAT));
  BLI_assert(x >= 0 && y >= 0 && w > 0 && h > 0);
  BLI_assert(x + w <= GPU_texture_width(offscreen->color));
  BLI_assert(y + h <= GPU_texture_height(offscreen->color));

  gpu::FrameBuffer *ofs_fb = gpu_offscreen_fb_get(offscreen);
  GPU_framebuffer_read_color(ofs_fb, x, y, w, h, 4, 0, format, r_data);
}

void GPU_offscreen_read_color(GPUOffScreen *offscreen, eGPUDataFormat format, void *r_data)
{
  BLI_assert(ELEM(format, GPU_DATA_UBYTE, GPU_DATA_FLOAT));

  const int w = GPU_texture_width(offscreen->color);
  const int h = GPU_texture_height(offscreen->color);

  GPU_offscreen_read_color_region(offscreen, format, 0, 0, w, h, r_data);
}

int GPU_offscreen_width(const GPUOffScreen *offscreen)
{
  return GPU_texture_width(offscreen->color);
}

int GPU_offscreen_height(const GPUOffScreen *offscreen)
{
  return GPU_texture_height(offscreen->color);
}

blender::gpu::Texture *GPU_offscreen_color_texture(const GPUOffScreen *offscreen)
{
  return offscreen->color;
}

blender::gpu::TextureFormat GPU_offscreen_format(const GPUOffScreen *offscreen)
{
  return GPU_texture_format(offscreen->color);
}

void GPU_offscreen_viewport_data_get(GPUOffScreen *offscreen,
                                     gpu::FrameBuffer **r_fb,
                                     blender::gpu::Texture **r_color,
                                     blender::gpu::Texture **r_depth)
{
  *r_fb = gpu_offscreen_fb_get(offscreen);
  *r_color = offscreen->color;
  *r_depth = offscreen->depth;
}

/** \} */
