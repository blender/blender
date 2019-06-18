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
#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#include "GPU_batch.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "gpu_private.h"
#include "gpu_context_private.h"

typedef enum {
  GPU_FB_DEPTH_ATTACHMENT = 0,
  GPU_FB_DEPTH_STENCIL_ATTACHMENT,
  GPU_FB_COLOR_ATTACHMENT0,
  GPU_FB_COLOR_ATTACHMENT1,
  GPU_FB_COLOR_ATTACHMENT2,
  GPU_FB_COLOR_ATTACHMENT3,
  GPU_FB_COLOR_ATTACHMENT4,
  /* Number of maximum output slots.
   * We support 5 outputs for now (usually we wouldn't need more to preserve fill rate). */
  /* Keep in mind that GL max is GL_MAX_DRAW_BUFFERS and is at least 8, corresponding to
   * the maximum number of COLOR attachments specified by glDrawBuffers. */
  GPU_FB_MAX_ATTACHEMENT,
} GPUAttachmentType;

#define GPU_FB_MAX_COLOR_ATTACHMENT (GPU_FB_MAX_ATTACHEMENT - GPU_FB_COLOR_ATTACHMENT0)

#define GPU_FB_DIRTY_DRAWBUFFER (1 << 15)

#define GPU_FB_ATTACHEMENT_IS_DIRTY(flag, type) ((flag & (1 << type)) != 0)
#define GPU_FB_ATTACHEMENT_SET_DIRTY(flag, type) (flag |= (1 << type))

struct GPUFrameBuffer {
  GPUContext *ctx;
  GLuint object;
  GPUAttachment attachments[GPU_FB_MAX_ATTACHEMENT];
  uint16_t dirty_flag;
  int width, height;
  bool multisample;
  /* TODO Check that we always use the right context when binding
   * (FBOs are not shared across ogl contexts). */
  // void *ctx;
};

static GLenum convert_attachment_type_to_gl(GPUAttachmentType type)
{
  static const GLenum table[] = {
      [GPU_FB_DEPTH_ATTACHMENT] = GL_DEPTH_ATTACHMENT,
      [GPU_FB_DEPTH_STENCIL_ATTACHMENT] = GL_DEPTH_STENCIL_ATTACHMENT,
      [GPU_FB_COLOR_ATTACHMENT0] = GL_COLOR_ATTACHMENT0,
      [GPU_FB_COLOR_ATTACHMENT1] = GL_COLOR_ATTACHMENT1,
      [GPU_FB_COLOR_ATTACHMENT2] = GL_COLOR_ATTACHMENT2,
      [GPU_FB_COLOR_ATTACHMENT3] = GL_COLOR_ATTACHMENT3,
      [GPU_FB_COLOR_ATTACHMENT4] = GL_COLOR_ATTACHMENT4,
  };
  return table[type];
}

static GPUAttachmentType attachment_type_from_tex(GPUTexture *tex, int slot)
{
  switch (GPU_texture_format(tex)) {
    case GPU_DEPTH_COMPONENT32F:
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH_COMPONENT16:
      return GPU_FB_DEPTH_ATTACHMENT;
    case GPU_DEPTH24_STENCIL8:
    case GPU_DEPTH32F_STENCIL8:
      return GPU_FB_DEPTH_STENCIL_ATTACHMENT;
    default:
      return GPU_FB_COLOR_ATTACHMENT0 + slot;
  }
}

static GLenum convert_buffer_bits_to_gl(eGPUFrameBufferBits bits)
{
  GLbitfield mask = 0;
  mask |= (bits & GPU_DEPTH_BIT) ? GL_DEPTH_BUFFER_BIT : 0;
  mask |= (bits & GPU_STENCIL_BIT) ? GL_STENCIL_BUFFER_BIT : 0;
  mask |= (bits & GPU_COLOR_BIT) ? GL_COLOR_BUFFER_BIT : 0;
  return mask;
}

static GPUTexture *framebuffer_get_depth_tex(GPUFrameBuffer *fb)
{
  if (fb->attachments[GPU_FB_DEPTH_ATTACHMENT].tex) {
    return fb->attachments[GPU_FB_DEPTH_ATTACHMENT].tex;
  }
  else {
    return fb->attachments[GPU_FB_DEPTH_STENCIL_ATTACHMENT].tex;
  }
}

static GPUTexture *framebuffer_get_color_tex(GPUFrameBuffer *fb, int slot)
{
  return fb->attachments[GPU_FB_COLOR_ATTACHMENT0 + slot].tex;
}

static void gpu_print_framebuffer_error(GLenum status, char err_out[256])
{
  const char *format = "GPUFrameBuffer: framebuffer status %s\n";
  const char *err = "unknown";

#define FORMAT_STATUS(X) \
  case GL_FRAMEBUFFER_##X: \
    err = "GL_FRAMEBUFFER_" #X; \
    break;

  switch (status) {
    /* success */
    FORMAT_STATUS(COMPLETE);
    /* errors shared by OpenGL desktop & ES */
    FORMAT_STATUS(INCOMPLETE_ATTACHMENT);
    FORMAT_STATUS(INCOMPLETE_MISSING_ATTACHMENT);
    FORMAT_STATUS(UNSUPPORTED);
#if 0 /* for OpenGL ES only */
    FORMAT_STATUS(INCOMPLETE_DIMENSIONS);
#else /* for desktop GL only */
    FORMAT_STATUS(INCOMPLETE_DRAW_BUFFER);
    FORMAT_STATUS(INCOMPLETE_READ_BUFFER);
    FORMAT_STATUS(INCOMPLETE_MULTISAMPLE);
    FORMAT_STATUS(UNDEFINED);
#endif
  }

#undef FORMAT_STATUS

  if (err_out) {
    BLI_snprintf(err_out, 256, format, err);
  }
  else {
    fprintf(stderr, format, err);
  }
}

void gpu_framebuffer_module_init(void)
{
}

void gpu_framebuffer_module_exit(void)
{
}

GPUFrameBuffer *GPU_framebuffer_active_get(void)
{
  GPUContext *ctx = GPU_context_active_get();
  if (ctx) {
    return gpu_context_active_framebuffer_get(ctx);
  }
  else {
    return 0;
  }
}

static void gpu_framebuffer_current_set(GPUFrameBuffer *fb)
{
  GPUContext *ctx = GPU_context_active_get();
  if (ctx) {
    gpu_context_active_framebuffer_set(ctx, fb);
  }
}

/* GPUFrameBuffer */

GPUFrameBuffer *GPU_framebuffer_create(void)
{
  /* We generate the FB object later at first use in order to
   * create the framebuffer in the right opengl context. */
  return MEM_callocN(sizeof(GPUFrameBuffer), "GPUFrameBuffer");
}

static void gpu_framebuffer_init(GPUFrameBuffer *fb)
{
  fb->object = GPU_fbo_alloc();
  fb->ctx = GPU_context_active_get();
  gpu_context_add_framebuffer(fb->ctx, fb);
}

void GPU_framebuffer_free(GPUFrameBuffer *fb)
{
  for (GPUAttachmentType type = 0; type < GPU_FB_MAX_ATTACHEMENT; type++) {
    if (fb->attachments[type].tex != NULL) {
      GPU_framebuffer_texture_detach(fb, fb->attachments[type].tex);
    }
  }

  if (fb->object != 0) {
    /* This restores the framebuffer if it was bound */
    GPU_fbo_free(fb->object, fb->ctx);
    gpu_context_remove_framebuffer(fb->ctx, fb);
  }

  if (GPU_framebuffer_active_get() == fb) {
    gpu_framebuffer_current_set(NULL);
  }

  MEM_freeN(fb);
}

/* ---------- Attach ----------- */

static void gpu_framebuffer_texture_attach_ex(
    GPUFrameBuffer *fb, GPUTexture *tex, int slot, int layer, int mip)
{
  if (slot >= GPU_FB_MAX_COLOR_ATTACHMENT) {
    fprintf(stderr,
            "Attaching to index %d framebuffer slot unsupported. "
            "Use at most %d\n",
            slot,
            GPU_FB_MAX_COLOR_ATTACHMENT);
    return;
  }

  GPUAttachmentType type = attachment_type_from_tex(tex, slot);
  GPUAttachment *attachment = &fb->attachments[type];

  if ((attachment->tex == tex) && (attachment->mip == mip) && (attachment->layer == layer)) {
    return; /* Exact same texture already bound here. */
  }
  else if (attachment->tex != NULL) {
    GPU_framebuffer_texture_detach(fb, attachment->tex);
  }

  if (attachment->tex == NULL) {
    GPU_texture_attach_framebuffer(tex, fb, type);
  }

  attachment->tex = tex;
  attachment->mip = mip;
  attachment->layer = layer;
  GPU_FB_ATTACHEMENT_SET_DIRTY(fb->dirty_flag, type);
}

void GPU_framebuffer_texture_attach(GPUFrameBuffer *fb, GPUTexture *tex, int slot, int mip)
{
  gpu_framebuffer_texture_attach_ex(fb, tex, slot, -1, mip);
}

void GPU_framebuffer_texture_layer_attach(
    GPUFrameBuffer *fb, GPUTexture *tex, int slot, int layer, int mip)
{
  /* NOTE: We could support 1D ARRAY texture. */
  BLI_assert(GPU_texture_target(tex) == GL_TEXTURE_2D_ARRAY);
  gpu_framebuffer_texture_attach_ex(fb, tex, slot, layer, mip);
}

void GPU_framebuffer_texture_cubeface_attach(
    GPUFrameBuffer *fb, GPUTexture *tex, int slot, int face, int mip)
{
  BLI_assert(GPU_texture_cube(tex));
  gpu_framebuffer_texture_attach_ex(fb, tex, slot, face, mip);
}

/* ---------- Detach ----------- */

void GPU_framebuffer_texture_detach_slot(GPUFrameBuffer *fb, GPUTexture *tex, int type)
{
  GPUAttachment *attachment = &fb->attachments[type];

  if (attachment->tex != tex) {
    fprintf(stderr,
            "Warning, attempting to detach Texture %p from framebuffer %p "
            "but texture is not attached.\n",
            tex,
            fb);
    return;
  }

  attachment->tex = NULL;
  GPU_FB_ATTACHEMENT_SET_DIRTY(fb->dirty_flag, type);
}

void GPU_framebuffer_texture_detach(GPUFrameBuffer *fb, GPUTexture *tex)
{
  GPUAttachmentType type = GPU_texture_detach_framebuffer(tex, fb);
  GPU_framebuffer_texture_detach_slot(fb, tex, type);
}

/* ---------- Config (Attach & Detach) ----------- */

/**
 * First GPUAttachment in *config is always the depth/depth_stencil buffer.
 * Following GPUAttachments are color buffers.
 * Setting GPUAttachment.mip to -1 will leave the texture in this slot.
 * Setting GPUAttachment.tex to NULL will detach the texture in this slot.
 */
void GPU_framebuffer_config_array(GPUFrameBuffer *fb, const GPUAttachment *config, int config_len)
{
  if (config[0].tex) {
    BLI_assert(GPU_texture_depth(config[0].tex));
    gpu_framebuffer_texture_attach_ex(fb, config[0].tex, 0, config[0].layer, config[0].mip);
  }
  else if (config[0].mip == -1) {
    /* Leave texture attached */
  }
  else if (fb->attachments[GPU_FB_DEPTH_ATTACHMENT].tex != NULL) {
    GPU_framebuffer_texture_detach(fb, fb->attachments[GPU_FB_DEPTH_ATTACHMENT].tex);
  }
  else if (fb->attachments[GPU_FB_DEPTH_STENCIL_ATTACHMENT].tex != NULL) {
    GPU_framebuffer_texture_detach(fb, fb->attachments[GPU_FB_DEPTH_STENCIL_ATTACHMENT].tex);
  }

  int slot = 0;
  for (int i = 1; i < config_len; ++i, ++slot) {
    if (config[i].tex != NULL) {
      BLI_assert(GPU_texture_depth(config[i].tex) == false);
      gpu_framebuffer_texture_attach_ex(fb, config[i].tex, slot, config[i].layer, config[i].mip);
    }
    else if (config[i].mip != -1) {
      GPUTexture *tex = framebuffer_get_color_tex(fb, slot);
      if (tex != NULL) {
        GPU_framebuffer_texture_detach(fb, tex);
      }
    }
  }
}

/* ---------- Bind / Restore ----------- */

static void gpu_framebuffer_attachment_attach(GPUAttachment *attach, GPUAttachmentType attach_type)
{
  int tex_bind = GPU_texture_opengl_bindcode(attach->tex);
  GLenum gl_attachment = convert_attachment_type_to_gl(attach_type);

  if (attach->layer > -1) {
    if (GPU_texture_cube(attach->tex)) {
      glFramebufferTexture2D(GL_FRAMEBUFFER,
                             gl_attachment,
                             GL_TEXTURE_CUBE_MAP_POSITIVE_X + attach->layer,
                             tex_bind,
                             attach->mip);
    }
    else {
      glFramebufferTextureLayer(
          GL_FRAMEBUFFER, gl_attachment, tex_bind, attach->mip, attach->layer);
    }
  }
  else {
    glFramebufferTexture(GL_FRAMEBUFFER, gl_attachment, tex_bind, attach->mip);
  }
}

static void gpu_framebuffer_attachment_detach(GPUAttachment *UNUSED(attachment),
                                              GPUAttachmentType attach_type)
{
  GLenum gl_attachment = convert_attachment_type_to_gl(attach_type);
  glFramebufferTexture(GL_FRAMEBUFFER, gl_attachment, 0, 0);
}

static void gpu_framebuffer_update_attachments(GPUFrameBuffer *fb)
{
  GLenum gl_attachments[GPU_FB_MAX_COLOR_ATTACHMENT];
  int numslots = 0;

  BLI_assert(GPU_framebuffer_active_get() == fb);

  /* Update attachments */
  for (GPUAttachmentType type = 0; type < GPU_FB_MAX_ATTACHEMENT; ++type) {

    if (type >= GPU_FB_COLOR_ATTACHMENT0) {
      if (fb->attachments[type].tex) {
        gl_attachments[numslots] = convert_attachment_type_to_gl(type);
      }
      else {
        gl_attachments[numslots] = GL_NONE;
      }
      numslots++;
    }

    if (GPU_FB_ATTACHEMENT_IS_DIRTY(fb->dirty_flag, type) == false) {
      continue;
    }
    else if (fb->attachments[type].tex != NULL) {
      gpu_framebuffer_attachment_attach(&fb->attachments[type], type);

      fb->multisample = (GPU_texture_samples(fb->attachments[type].tex) > 0);
      fb->width = GPU_texture_width(fb->attachments[type].tex);
      fb->height = GPU_texture_height(fb->attachments[type].tex);
    }
    else {
      gpu_framebuffer_attachment_detach(&fb->attachments[type], type);
    }
  }
  fb->dirty_flag = 0;

  /* Update draw buffers (color targets)
   * This state is saved in the FBO */
  if (numslots) {
    glDrawBuffers(numslots, gl_attachments);
  }
  else {
    glDrawBuffer(GL_NONE);
  }
}

/**
 * Hack to solve the problem of some bugged AMD GPUs (see `GPU_unused_fb_slot_workaround`).
 * If there is an empty color slot between the color slots,
 * all textures after this slot are apparently skipped/discarded.
 */
static void gpu_framebuffer_update_attachments_and_fill_empty_slots(GPUFrameBuffer *fb)
{
  GLenum gl_attachments[GPU_FB_MAX_COLOR_ATTACHMENT];
  int dummy_tex = 0;

  BLI_assert(GPU_framebuffer_active_get() == fb);

  /* Update attachments */
  for (GPUAttachmentType type = GPU_FB_MAX_ATTACHEMENT; type--;) {
    GPUTexture *tex = fb->attachments[type].tex;

    if (type >= GPU_FB_COLOR_ATTACHMENT0) {
      int slot = type - GPU_FB_COLOR_ATTACHMENT0;
      if (tex != NULL || (dummy_tex != 0)) {
        gl_attachments[slot] = convert_attachment_type_to_gl(type);

        if (dummy_tex == 0) {
          dummy_tex = GPU_texture_opengl_bindcode(tex);
        }
      }
      else {
        gl_attachments[slot] = GL_NONE;
      }
    }
    else {
      dummy_tex = 0;
    }

    if ((dummy_tex != 0) && tex == NULL) {
      /* Fill empty slot */
      glFramebufferTexture(GL_FRAMEBUFFER, convert_attachment_type_to_gl(type), dummy_tex, 0);
    }
    else if (GPU_FB_ATTACHEMENT_IS_DIRTY(fb->dirty_flag, type)) {
      if (tex != NULL) {
        gpu_framebuffer_attachment_attach(&fb->attachments[type], type);

        fb->multisample = (GPU_texture_samples(tex) > 0);
        fb->width = GPU_texture_width(tex);
        fb->height = GPU_texture_height(tex);
      }
      else {
        gpu_framebuffer_attachment_detach(&fb->attachments[type], type);
      }
    }
  }
  fb->dirty_flag = 0;

  /* Update draw buffers (color targets)
   * This state is saved in the FBO */
  glDrawBuffers(GPU_FB_MAX_COLOR_ATTACHMENT, gl_attachments);
}

#define FRAMEBUFFER_STACK_DEPTH 16

static struct {
  GPUFrameBuffer *framebuffers[FRAMEBUFFER_STACK_DEPTH];
  uint top;
} FrameBufferStack = {{0}};

static void gpuPushFrameBuffer(GPUFrameBuffer *fbo)
{
  BLI_assert(FrameBufferStack.top < FRAMEBUFFER_STACK_DEPTH);
  FrameBufferStack.framebuffers[FrameBufferStack.top] = fbo;
  FrameBufferStack.top++;
}

static GPUFrameBuffer *gpuPopFrameBuffer(void)
{
  BLI_assert(FrameBufferStack.top > 0);
  FrameBufferStack.top--;
  return FrameBufferStack.framebuffers[FrameBufferStack.top];
}

#undef FRAMEBUFFER_STACK_DEPTH

void GPU_framebuffer_bind(GPUFrameBuffer *fb)
{
  if (fb->object == 0) {
    gpu_framebuffer_init(fb);
  }

  if (GPU_framebuffer_active_get() != fb) {
    glBindFramebuffer(GL_FRAMEBUFFER, fb->object);
  }

  gpu_framebuffer_current_set(fb);

  if (fb->dirty_flag != 0) {
    if (GPU_unused_fb_slot_workaround()) {
      /* XXX: Please AMD, fix this. */
      gpu_framebuffer_update_attachments_and_fill_empty_slots(fb);
    }
    else {
      gpu_framebuffer_update_attachments(fb);
    }
  }

  /* TODO manually check for errors? */
#if 0
  char err_out[256];
  if (!GPU_framebuffer_check_valid(fb, err_out)) {
    printf("Invalid %s\n", err_out);
  }
#endif

  if (fb->multisample) {
    glEnable(GL_MULTISAMPLE);
  }

  glViewport(0, 0, fb->width, fb->height);
}

void GPU_framebuffer_restore(void)
{
  if (GPU_framebuffer_active_get() != NULL) {
    glBindFramebuffer(GL_FRAMEBUFFER, GPU_framebuffer_default());
    gpu_framebuffer_current_set(NULL);
  }
}

bool GPU_framebuffer_bound(GPUFrameBuffer *fb)
{
  return (fb == GPU_framebuffer_active_get()) && (fb->object != 0);
}

bool GPU_framebuffer_check_valid(GPUFrameBuffer *fb, char err_out[256])
{
  if (!GPU_framebuffer_bound(fb)) {
    GPU_framebuffer_bind(fb);
  }

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

  if (status != GL_FRAMEBUFFER_COMPLETE) {
    GPU_framebuffer_restore();
    gpu_print_framebuffer_error(status, err_out);
    return false;
  }

  return true;
}

/* ---------- Framebuffer Operations ----------- */

#define CHECK_FRAMEBUFFER_IS_BOUND(_fb) \
  BLI_assert(GPU_framebuffer_bound(_fb)); \
  UNUSED_VARS_NDEBUG(_fb); \
  ((void)0)

/* Needs to be done after binding. */
void GPU_framebuffer_viewport_set(GPUFrameBuffer *fb, int x, int y, int w, int h)
{
  CHECK_FRAMEBUFFER_IS_BOUND(fb);

  glViewport(x, y, w, h);
}

void GPU_framebuffer_clear(GPUFrameBuffer *fb,
                           eGPUFrameBufferBits buffers,
                           const float clear_col[4],
                           float clear_depth,
                           uint clear_stencil)
{
  CHECK_FRAMEBUFFER_IS_BOUND(fb);

  if (buffers & GPU_COLOR_BIT) {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(clear_col[0], clear_col[1], clear_col[2], clear_col[3]);
  }
  if (buffers & GPU_DEPTH_BIT) {
    glDepthMask(GL_TRUE);
    glClearDepth(clear_depth);
  }
  if (buffers & GPU_STENCIL_BIT) {
    glStencilMask(0xFF);
    glClearStencil(clear_stencil);
  }

  GLbitfield mask = convert_buffer_bits_to_gl(buffers);
  glClear(mask);
}

void GPU_framebuffer_read_depth(GPUFrameBuffer *fb, int x, int y, int w, int h, float *data)
{
  CHECK_FRAMEBUFFER_IS_BOUND(fb);

  GLenum type = GL_DEPTH_COMPONENT;
  glReadBuffer(GL_COLOR_ATTACHMENT0); /* This is OK! */
  glReadPixels(x, y, w, h, type, GL_FLOAT, data);
}

void GPU_framebuffer_read_color(
    GPUFrameBuffer *fb, int x, int y, int w, int h, int channels, int slot, float *data)
{
  CHECK_FRAMEBUFFER_IS_BOUND(fb);

  GLenum type;
  switch (channels) {
    case 1:
      type = GL_RED;
      break;
    case 2:
      type = GL_RG;
      break;
    case 3:
      type = GL_RGB;
      break;
    case 4:
      type = GL_RGBA;
      break;
    default:
      BLI_assert(false && "wrong number of read channels");
      return;
  }
  glReadBuffer(GL_COLOR_ATTACHMENT0 + slot);
  glReadPixels(x, y, w, h, type, GL_FLOAT, data);
}

/* read_slot and write_slot are only used for color buffers. */
void GPU_framebuffer_blit(GPUFrameBuffer *fb_read,
                          int read_slot,
                          GPUFrameBuffer *fb_write,
                          int write_slot,
                          eGPUFrameBufferBits blit_buffers)
{
  BLI_assert(blit_buffers != 0);

  GPUFrameBuffer *prev_fb = GPU_framebuffer_active_get();

  /* Framebuffers must be up to date. This simplify this function. */
  if (fb_read->dirty_flag != 0 || fb_read->object == 0) {
    GPU_framebuffer_bind(fb_read);
  }
  if (fb_write->dirty_flag != 0 || fb_write->object == 0) {
    GPU_framebuffer_bind(fb_write);
  }

  const bool do_color = (blit_buffers & GPU_COLOR_BIT);
  const bool do_depth = (blit_buffers & GPU_DEPTH_BIT);
  const bool do_stencil = (blit_buffers & GPU_STENCIL_BIT);

  GPUTexture *read_tex = ((do_depth || do_stencil) ?
                              framebuffer_get_depth_tex(fb_read) :
                              framebuffer_get_color_tex(fb_read, read_slot));
  GPUTexture *write_tex = ((do_depth || do_stencil) ?
                               framebuffer_get_depth_tex(fb_write) :
                               framebuffer_get_color_tex(fb_write, read_slot));

  if (do_depth) {
    BLI_assert(GPU_texture_depth(read_tex) && GPU_texture_depth(write_tex));
    BLI_assert(GPU_texture_format(read_tex) == GPU_texture_format(write_tex));
  }
  if (do_stencil) {
    BLI_assert(GPU_texture_stencil(read_tex) && GPU_texture_stencil(write_tex));
    BLI_assert(GPU_texture_format(read_tex) == GPU_texture_format(write_tex));
  }
  if (GPU_texture_samples(write_tex) != 0 || GPU_texture_samples(read_tex) != 0) {
    /* Can only blit multisample textures to another texture of the same size. */
    BLI_assert((fb_read->width == fb_write->width) && (fb_read->height == fb_write->height));
  }

  glBindFramebuffer(GL_READ_FRAMEBUFFER, fb_read->object);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_write->object);

  if (do_color) {
    glReadBuffer(GL_COLOR_ATTACHMENT0 + read_slot);
    glDrawBuffer(GL_COLOR_ATTACHMENT0 + write_slot);
    /* XXX we messed with the glDrawBuffer, this will reset the
     * glDrawBuffers the next time we bind fb_write. */
    fb_write->dirty_flag = GPU_FB_DIRTY_DRAWBUFFER;
  }

  GLbitfield mask = convert_buffer_bits_to_gl(blit_buffers);

  glBlitFramebuffer(0,
                    0,
                    fb_read->width,
                    fb_read->height,
                    0,
                    0,
                    fb_write->width,
                    fb_write->height,
                    mask,
                    GL_NEAREST);

  /* Restore previous framebuffer */
  if (fb_write == prev_fb) {
    GPU_framebuffer_bind(fb_write); /* To update drawbuffers */
  }
  else if (prev_fb) {
    glBindFramebuffer(GL_FRAMEBUFFER, prev_fb->object);
    gpu_framebuffer_current_set(prev_fb);
  }
  else {
    glBindFramebuffer(GL_FRAMEBUFFER, GPU_framebuffer_default());
    gpu_framebuffer_current_set(NULL);
  }
}

/**
 * Use this if you need to custom down-sample your texture and use the previous mip level as input.
 * This function only takes care of the correct texture handling.
 * It execute the callback for each texture level.
 */
void GPU_framebuffer_recursive_downsample(GPUFrameBuffer *fb,
                                          int max_lvl,
                                          void (*callback)(void *userData, int level),
                                          void *userData)
{
  /* Framebuffer must be up to date and bound. This simplify this function. */
  if (GPU_framebuffer_active_get() != fb || fb->dirty_flag != 0 || fb->object == 0) {
    GPU_framebuffer_bind(fb);
  }
  /* HACK: We make the framebuffer appear not bound in order to
   * not trigger any error in GPU_texture_bind().  */
  GPUFrameBuffer *prev_fb = GPU_framebuffer_active_get();
  gpu_framebuffer_current_set(NULL);

  int levels = floor(log2(max_ii(fb->width, fb->height)));
  max_lvl = min_ii(max_lvl, levels);

  int i;
  int current_dim[2] = {fb->width, fb->height};
  for (i = 1; i < max_lvl + 1; i++) {
    /* calculate next viewport size */
    current_dim[0] = max_ii(current_dim[0] / 2, 1);
    current_dim[1] = max_ii(current_dim[1] / 2, 1);

    for (GPUAttachmentType type = 0; type < GPU_FB_MAX_ATTACHEMENT; ++type) {
      if (fb->attachments[type].tex != NULL) {
        /* Some Intel HDXXX have issue with rendering to a mipmap that is below
         * the texture GL_TEXTURE_MAX_LEVEL. So even if it not correct, in this case
         * we allow GL_TEXTURE_MAX_LEVEL to be one level lower. In practice it does work! */
        int next_lvl = (GPU_mip_render_workaround()) ? i : i - 1;
        /* bind next level for rendering but first restrict fetches only to previous level */
        GPUTexture *tex = fb->attachments[type].tex;
        GPU_texture_bind(tex, 0);
        glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_BASE_LEVEL, i - 1);
        glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_MAX_LEVEL, next_lvl);
        GPU_texture_unbind(tex);
        /* copy attachment and replace miplevel. */
        GPUAttachment attachment = fb->attachments[type];
        attachment.mip = i;
        gpu_framebuffer_attachment_attach(&attachment, type);
      }
    }

    BLI_assert(GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glViewport(0, 0, current_dim[0], current_dim[1]);
    callback(userData, i);

    if (current_dim[0] == 1 && current_dim[1] == 1) {
      break;
    }
  }

  for (GPUAttachmentType type = 0; type < GPU_FB_MAX_ATTACHEMENT; ++type) {
    if (fb->attachments[type].tex != NULL) {
      /* reset mipmap level range */
      GPUTexture *tex = fb->attachments[type].tex;
      GPU_texture_bind(tex, 0);
      glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_BASE_LEVEL, 0);
      glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_MAX_LEVEL, i - 1);
      GPU_texture_unbind(tex);
      /* Reattach original level */
      /* NOTE: This is not necessary but this makes the FBO config
       *       remain in sync with the GPUFrameBuffer config. */
      gpu_framebuffer_attachment_attach(&fb->attachments[type], type);
    }
  }

  gpu_framebuffer_current_set(prev_fb);
}

/* GPUOffScreen */

#define MAX_CTX_FB_LEN 3

struct GPUOffScreen {
  struct {
    GPUContext *ctx;
    GPUFrameBuffer *fb;
  } framebuffers[MAX_CTX_FB_LEN];

  GPUTexture *color;
  GPUTexture *depth;
};

/* Returns the correct framebuffer for the current context. */
static GPUFrameBuffer *gpu_offscreen_fb_get(GPUOffScreen *ofs)
{
  GPUContext *ctx = GPU_context_active_get();
  BLI_assert(ctx);

  for (int i = 0; i < MAX_CTX_FB_LEN; i++) {
    if (ofs->framebuffers[i].fb == NULL) {
      ofs->framebuffers[i].ctx = ctx;
      GPU_framebuffer_ensure_config(
          &ofs->framebuffers[i].fb,
          {GPU_ATTACHMENT_TEXTURE(ofs->depth), GPU_ATTACHMENT_TEXTURE(ofs->color)});
    }

    if (ofs->framebuffers[i].ctx == ctx) {
      return ofs->framebuffers[i].fb;
    }
  }

  /* List is full, this should never happen or
   * it might just slow things down if it happens
   * regulary. In this case we just empty the list
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
    int width, int height, int samples, bool depth, bool high_bitdepth, char err_out[256])
{
  GPUOffScreen *ofs;

  ofs = MEM_callocN(sizeof(GPUOffScreen), "GPUOffScreen");

  /* Sometimes areas can have 0 height or width and this will
   * create a 1D texture which we don't want. */
  height = max_ii(1, height);
  width = max_ii(1, width);

  ofs->color = GPU_texture_create_2d_multisample(
      width, height, (high_bitdepth) ? GPU_RGBA16F : GPU_RGBA8, NULL, samples, err_out);

  if (depth) {
    ofs->depth = GPU_texture_create_2d_multisample(
        width, height, GPU_DEPTH24_STENCIL8, NULL, samples, err_out);
  }

  if ((depth && !ofs->depth) || !ofs->color) {
    GPU_offscreen_free(ofs);
    return NULL;
  }

  gpuPushAttr(GPU_VIEWPORT_BIT);

  GPUFrameBuffer *fb = gpu_offscreen_fb_get(ofs);

  /* check validity at the very end! */
  if (!GPU_framebuffer_check_valid(fb, err_out)) {
    GPU_offscreen_free(ofs);
    gpuPopAttr();
    return NULL;
  }

  GPU_framebuffer_restore();

  gpuPopAttr();

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
    gpuPushAttr(GPU_SCISSOR_BIT | GPU_VIEWPORT_BIT);
    GPUFrameBuffer *fb = GPU_framebuffer_active_get();
    gpuPushFrameBuffer(fb);
  }
  glDisable(GL_SCISSOR_TEST);
  GPUFrameBuffer *ofs_fb = gpu_offscreen_fb_get(ofs);
  GPU_framebuffer_bind(ofs_fb);
}

void GPU_offscreen_unbind(GPUOffScreen *UNUSED(ofs), bool restore)
{
  GPUFrameBuffer *fb = NULL;

  if (restore) {
    gpuPopAttr();
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
  const int w = GPU_texture_width(ofs->color);
  const int h = GPU_texture_height(ofs->color);

  GPUFrameBuffer *ofs_fb = gpu_offscreen_fb_get(ofs);

  glBindFramebuffer(GL_READ_FRAMEBUFFER, ofs_fb->object);
  GLenum status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);

  if (status == GL_FRAMEBUFFER_COMPLETE) {
    glBlitFramebuffer(0, 0, w, h, x, y, x + w, y + h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }
  else {
    gpu_print_framebuffer_error(status, NULL);
  }

  glBindFramebuffer(GL_READ_FRAMEBUFFER, GPU_framebuffer_default());
}

void GPU_offscreen_read_pixels(GPUOffScreen *ofs, int type, void *pixels)
{
  const int w = GPU_texture_width(ofs->color);
  const int h = GPU_texture_height(ofs->color);

  BLI_assert(type == GL_UNSIGNED_BYTE || type == GL_FLOAT);

  if (GPU_texture_target(ofs->color) == GL_TEXTURE_2D_MULTISAMPLE) {
    /* For a multi-sample texture,
     * we need to create an intermediate buffer to blit to,
     * before its copied using 'glReadPixels' */
    GLuint fbo_blit = 0;
    GLuint tex_blit = 0;

    /* create texture for new 'fbo_blit' */
    glGenTextures(1, &tex_blit);
    glBindTexture(GL_TEXTURE_2D, tex_blit);
    glTexImage2D(
        GL_TEXTURE_2D, 0, (type == GL_FLOAT) ? GL_RGBA16F : GL_RGBA8, w, h, 0, GL_RGBA, type, 0);

    /* write into new single-sample buffer */
    glGenFramebuffers(1, &fbo_blit);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_blit);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_blit, 0);

    GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      goto finally;
    }

    /* perform the copy */
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    /* read the results */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_blit);
    glReadPixels(0, 0, w, h, GL_RGBA, type, pixels);

    /* restore the original frame-bufer */
    GPUFrameBuffer *ofs_fb = gpu_offscreen_fb_get(ofs);
    glBindFramebuffer(GL_FRAMEBUFFER, ofs_fb->object);

  finally:
    /* cleanup */
    glDeleteTextures(1, &tex_blit);
    glDeleteFramebuffers(1, &fbo_blit);
  }
  else {
    glReadPixels(0, 0, w, h, GL_RGBA, type, pixels);
  }
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

void GPU_clear_color(float red, float green, float blue, float alpha)
{
  glClearColor(red, green, blue, alpha);
}

void GPU_clear_depth(float depth)
{
  glClearDepth(depth);
}

void GPU_clear(eGPUFrameBufferBits flags)
{
  glClear(convert_buffer_bits_to_gl(flags));
}
