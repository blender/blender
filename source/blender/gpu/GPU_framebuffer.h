/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * GPU Frame-buffer
 * - this is a wrapper for an OpenGL framebuffer object (FBO). in practice
 *   multiple FBO's may be created.
 * - actual FBO creation & config is deferred until GPU_framebuffer_bind or
 *   GPU_framebuffer_check_valid to allow creation & config while another
 *   opengl context is bound (since FBOs are not shared between ogl contexts).
 */

#pragma once

#include "GPU_common_types.h"
#include "GPU_texture.h"

typedef enum eGPUFrameBufferBits {
  GPU_COLOR_BIT = (1 << 0),
  GPU_DEPTH_BIT = (1 << 1),
  GPU_STENCIL_BIT = (1 << 2),
} eGPUFrameBufferBits;

ENUM_OPERATORS(eGPUFrameBufferBits, GPU_STENCIL_BIT)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GPUAttachment {
  struct GPUTexture *tex;
  int layer, mip;
} GPUAttachment;

typedef enum eGPUBackBuffer {
  GPU_BACKBUFFER_LEFT = 0,
  GPU_BACKBUFFER_RIGHT,
} eGPUBackBuffer;

/** Opaque type hiding blender::gpu::FrameBuffer. */
typedef struct GPUFrameBuffer GPUFrameBuffer;

typedef struct GPUOffScreen GPUOffScreen;

GPUFrameBuffer *GPU_framebuffer_create(const char *name);
void GPU_framebuffer_free(GPUFrameBuffer *fb);
void GPU_framebuffer_bind(GPUFrameBuffer *fb);
/**
 * Workaround for binding a SRGB frame-buffer without doing the SRGB transform.
 */
void GPU_framebuffer_bind_no_srgb(GPUFrameBuffer *fb);
void GPU_framebuffer_restore(void);

/* Advanced binding control. */
typedef struct GPULoadStore {
  eGPULoadOp load_action;
  eGPUStoreOp store_action;
} GPULoadStore;
#define NULL_LOAD_STORE \
  { \
    GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_DONT_CARE \
  }

/* Load store config array (load_store_actions) matches attachment structure of
 * GPU_framebuffer_config_array. This allows us to explicitly specify whether attachment data needs
 * to be loaded and stored on a per-attachment basis. This enables a number of bandwidth
 * optimizations:
 *  - No need to load contents if subsequent work is over-writing every pixel.
 *  - No need to store attachments whose contents are not used beyond this pass e.g. depth buffer.
 *  - State can be customized at bind-time rather than applying to the frame-buffer object as a
 * whole.
 *
 * Example:
 * \code{.c}
 * GPU_framebuffer_bind_loadstore(&fb, {
 *         {GPU_LOADACTION_LOAD, GPU_STOREACTION_DONT_CARE} // must be depth buffer
 *         {GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE}, // Color attachment 0
 *         {GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_STORE}, // Color attachment 1
 *         {GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_STORE} // Color attachment 2
 * })
 * \encode
 */
void GPU_framebuffer_bind_loadstore(GPUFrameBuffer *fb,
                                    const GPULoadStore *load_store_actions,
                                    uint actions_len);
#define GPU_framebuffer_bind_ex(_fb, ...) \
  { \
    GPULoadStore actions[] = __VA_ARGS__; \
    GPU_framebuffer_bind_loadstore(_fb, actions, (sizeof(actions) / sizeof(GPULoadStore))); \
  }

bool GPU_framebuffer_bound(GPUFrameBuffer *fb);
bool GPU_framebuffer_check_valid(GPUFrameBuffer *fb, char err_out[256]);

GPUFrameBuffer *GPU_framebuffer_active_get(void);
/**
 * Returns the default frame-buffer. Will always exists even if it's just a dummy.
 */
GPUFrameBuffer *GPU_framebuffer_back_get(void);

#define GPU_FRAMEBUFFER_FREE_SAFE(fb) \
  do { \
    if (fb != NULL) { \
      GPU_framebuffer_free(fb); \
      fb = NULL; \
    } \
  } while (0)

/* Frame-buffer setup: You need to call #GPU_framebuffer_bind for these
 * to be effective. */

void GPU_framebuffer_texture_attach_ex(GPUFrameBuffer *gpu_fb, GPUAttachment attachment, int slot);
void GPU_framebuffer_texture_detach(GPUFrameBuffer *fb, struct GPUTexture *tex);

/**
 * How to use #GPU_framebuffer_ensure_config().
 *
 * Example:
 * \code{.c}
 * GPU_framebuffer_ensure_config(&fb, {
 *         GPU_ATTACHMENT_TEXTURE(depth), // must be depth buffer
 *         GPU_ATTACHMENT_TEXTURE(tex1),
 *         GPU_ATTACHMENT_TEXTURE_CUBEFACE(tex2, 0),
 *         GPU_ATTACHMENT_TEXTURE_LAYER_MIP(tex2, 0, 0)
 * })
 * \encode
 *
 * \note Unspecified attachments (i.e: those beyond the last
 * GPU_ATTACHMENT_* in GPU_framebuffer_ensure_config list) are left unchanged.
 *
 * \note Make sure that the dimensions of your textures matches
 * otherwise you will have an invalid framebuffer error.
 */
#define GPU_framebuffer_ensure_config(_fb, ...) \
  do { \
    if (*(_fb) == NULL) { \
      *(_fb) = GPU_framebuffer_create(#_fb); \
    } \
    GPUAttachment config[] = __VA_ARGS__; \
    GPU_framebuffer_config_array(*(_fb), config, (sizeof(config) / sizeof(GPUAttachment))); \
  } while (0)

/**
 * First #GPUAttachment in *config is always the depth/depth_stencil buffer.
 * Following #GPUAttachments are color buffers.
 * Setting #GPUAttachment.mip to -1 will leave the texture in this slot.
 * Setting #GPUAttachment.tex to NULL will detach the texture in this slot.
 */
void GPU_framebuffer_config_array(GPUFrameBuffer *fb, const GPUAttachment *config, int config_len);

#define GPU_ATTACHMENT_NONE \
  { \
    NULL, -1, 0, \
  }
#define GPU_ATTACHMENT_LEAVE \
  { \
    NULL, -1, -1, \
  }
#define GPU_ATTACHMENT_TEXTURE(_tex) \
  { \
    _tex, -1, 0, \
  }
#define GPU_ATTACHMENT_TEXTURE_MIP(_tex, _mip) \
  { \
    _tex, -1, _mip, \
  }
#define GPU_ATTACHMENT_TEXTURE_LAYER(_tex, _layer) \
  { \
    _tex, _layer, 0, \
  }
#define GPU_ATTACHMENT_TEXTURE_LAYER_MIP(_tex, _layer, _mip) \
  { \
    _tex, _layer, _mip, \
  }
#define GPU_ATTACHMENT_TEXTURE_CUBEFACE(_tex, _face) \
  { \
    _tex, _face, 0, \
  }
#define GPU_ATTACHMENT_TEXTURE_CUBEFACE_MIP(_tex, _face, _mip) \
  { \
    _tex, _face, _mip, \
  }

void GPU_framebuffer_texture_attach(GPUFrameBuffer *fb, GPUTexture *tex, int slot, int mip);
void GPU_framebuffer_texture_layer_attach(
    GPUFrameBuffer *fb, GPUTexture *tex, int slot, int layer, int mip);
void GPU_framebuffer_texture_cubeface_attach(
    GPUFrameBuffer *fb, GPUTexture *tex, int slot, int face, int mip);

/* Frame-buffer operations. */

/**
 * Viewport and scissor size is stored per frame-buffer.
 * It is only reset to its original dimensions explicitly OR when binding the frame-buffer after
 * modifying its attachments.
 */
void GPU_framebuffer_viewport_set(GPUFrameBuffer *fb, int x, int y, int w, int h);
void GPU_framebuffer_viewport_get(GPUFrameBuffer *fb, int r_viewport[4]);
/**
 * Reset to its attachment(s) size.
 */
void GPU_framebuffer_viewport_reset(GPUFrameBuffer *fb);

void GPU_framebuffer_clear(GPUFrameBuffer *fb,
                           eGPUFrameBufferBits buffers,
                           const float clear_col[4],
                           float clear_depth,
                           unsigned int clear_stencil);

#define GPU_framebuffer_clear_color(fb, col) \
  GPU_framebuffer_clear(fb, GPU_COLOR_BIT, col, 0.0f, 0x00)

#define GPU_framebuffer_clear_depth(fb, depth) \
  GPU_framebuffer_clear(fb, GPU_DEPTH_BIT, NULL, depth, 0x00)

#define GPU_framebuffer_clear_color_depth(fb, col, depth) \
  GPU_framebuffer_clear(fb, GPU_COLOR_BIT | GPU_DEPTH_BIT, col, depth, 0x00)

#define GPU_framebuffer_clear_stencil(fb, stencil) \
  GPU_framebuffer_clear(fb, GPU_STENCIL_BIT, NULL, 0.0f, stencil)

#define GPU_framebuffer_clear_depth_stencil(fb, depth, stencil) \
  GPU_framebuffer_clear(fb, GPU_DEPTH_BIT | GPU_STENCIL_BIT, NULL, depth, stencil)

#define GPU_framebuffer_clear_color_depth_stencil(fb, col, depth, stencil) \
  GPU_framebuffer_clear(fb, GPU_COLOR_BIT | GPU_DEPTH_BIT | GPU_STENCIL_BIT, col, depth, stencil)

/**
 * Clear all textures attached to this frame-buffer with a different color.
 */
void GPU_framebuffer_multi_clear(GPUFrameBuffer *fb, const float (*clear_cols)[4]);

void GPU_framebuffer_read_depth(
    GPUFrameBuffer *fb, int x, int y, int w, int h, eGPUDataFormat format, void *data);
void GPU_framebuffer_read_color(GPUFrameBuffer *fb,
                                int x,
                                int y,
                                int w,
                                int h,
                                int channels,
                                int slot,
                                eGPUDataFormat format,
                                void *data);

/**
 * Read_slot and write_slot are only used for color buffers.
 */
void GPU_framebuffer_blit(GPUFrameBuffer *fb_read,
                          int read_slot,
                          GPUFrameBuffer *fb_write,
                          int write_slot,
                          eGPUFrameBufferBits blit_buffers);

void GPU_framebuffer_recursive_downsample(GPUFrameBuffer *fb,
                                          int max_lvl,
                                          void (*callback)(void *userData, int level),
                                          void *userData);

#ifndef GPU_NO_USE_PY_REFERENCES
void **GPU_framebuffer_py_reference_get(GPUFrameBuffer *gpu_fb);
void GPU_framebuffer_py_reference_set(GPUFrameBuffer *gpu_fb, void **py_ref);
#endif

void GPU_framebuffer_push(GPUFrameBuffer *fb);
GPUFrameBuffer *GPU_framebuffer_pop(void);
uint GPU_framebuffer_stack_level_get(void);

/* GPU OffScreen
 * - wrapper around frame-buffer and texture for simple off-screen drawing
 */

GPUOffScreen *GPU_offscreen_create(
    int width, int height, bool depth, eGPUTextureFormat format, char err_out[256]);
void GPU_offscreen_free(GPUOffScreen *ofs);
void GPU_offscreen_bind(GPUOffScreen *ofs, bool save);
void GPU_offscreen_unbind(GPUOffScreen *ofs, bool restore);
void GPU_offscreen_read_pixels(GPUOffScreen *ofs, eGPUDataFormat format, void *pixels);
void GPU_offscreen_draw_to_screen(GPUOffScreen *ofs, int x, int y);
int GPU_offscreen_width(const GPUOffScreen *ofs);
int GPU_offscreen_height(const GPUOffScreen *ofs);
struct GPUTexture *GPU_offscreen_color_texture(const GPUOffScreen *ofs);

/**
 * \note only to be used by viewport code!
 */
void GPU_offscreen_viewport_data_get(GPUOffScreen *ofs,
                                     GPUFrameBuffer **r_fb,
                                     struct GPUTexture **r_color,
                                     struct GPUTexture **r_depth);

void GPU_clear_color(float red, float green, float blue, float alpha);
void GPU_clear_depth(float depth);

void GPU_frontbuffer_read_pixels(
    int x, int y, int w, int h, int channels, eGPUDataFormat format, void *data);

void GPU_backbuffer_bind(eGPUBackBuffer buffer);

#ifdef __cplusplus
}
#endif
