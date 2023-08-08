/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * A `GPUFramebuffer` is a wrapper for a frame-buffer object (FBO) from the underlying graphic API.
 *
 * A `GPUFramebuffer` is limited to one context and thus cannot be shared across different
 * contexts. In the case this is needed, one must recreate the same `GPUFramebuffer` in each
 * context.
 *
 * Note that actual FBO creation & config is deferred until `GPU_framebuffer_bind` or
 * `GPU_framebuffer_check_valid` is called. This means the context the `GPUFramebuffer` is bound
 * with is the one active when `GPU_framebuffer_bind` is called.
 *
 * When a `GPUTexture` is attached to a `GPUFramebuffer` a reference is created. Deleting either
 * does not require any unbinding.
 *
 * A `GPUOffScreen` is a convenience type that holds a `GPUFramebuffer` and its associated
 * `GPUTexture`s. It is useful for quick drawing surface configuration.
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

/* Guaranteed by the spec and is never greater than 16 on any hardware or implementation. */
#define GPU_MAX_VIEWPORTS 16

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GPUAttachment {
  GPUTexture *tex;
  int layer, mip;
} GPUAttachment;

/** Opaque type hiding blender::gpu::FrameBuffer. */
typedef struct GPUFrameBuffer GPUFrameBuffer;

/* -------------------------------------------------------------------- */
/** \name Creation
 * \{ */

/**
 * Create a #GPUFrameBuffer object. It is not configured and not bound to a specific context until
 * `GPU_framebuffer_bind()` is called.
 */
GPUFrameBuffer *GPU_framebuffer_create(const char *name);

/**
 * Returns the current context active framebuffer.
 * Return nullptr if no context is active.
 */
GPUFrameBuffer *GPU_framebuffer_active_get(void);

/**
 * Returns the default (back-left) frame-buffer. It will always exists even if it's just a dummy.
 * Return nullptr if no context is active.
 */
GPUFrameBuffer *GPU_framebuffer_back_get(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free
 * \{ */

/**
 * Create a #GPUFrameBuffer object. It is not configured and not bound to a specific context until
 * `GPU_framebuffer_bind()` is called.
 */
void GPU_framebuffer_free(GPUFrameBuffer *framebuffer);

#define GPU_FRAMEBUFFER_FREE_SAFE(fb) \
  do { \
    if (fb != NULL) { \
      GPU_framebuffer_free(fb); \
      fb = NULL; \
    } \
  } while (0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

typedef enum eGPUBackBuffer {
  /** Default framebuffer of a window. Always available. */
  GPU_BACKBUFFER_LEFT = 0,
  /** Right buffer of a window. Only available if window was created using stereo-view. */
  GPU_BACKBUFFER_RIGHT,
} eGPUBackBuffer;

/**
 * Binds the active context's window frame-buffer.
 * Note that `GPU_BACKBUFFER_RIGHT` is only available if the window was created using stereo-view.
 */
void GPU_backbuffer_bind(eGPUBackBuffer back_buffer_type);

/**
 * Binds a #GPUFrameBuffer making it the active framebuffer for all geometry rendering.
 */
void GPU_framebuffer_bind(GPUFrameBuffer *framebuffer);

/**
 * Same as `GPU_framebuffer_bind` but do not enable the SRGB transform.
 */
void GPU_framebuffer_bind_no_srgb(GPUFrameBuffer *framebuffer);

/**
 * Binds back the active context's default frame-buffer.
 * Equivalent to `GPU_backbuffer_bind(GPU_BACKBUFFER_LEFT)`.
 */
void GPU_framebuffer_restore(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Advanced binding control
 * \{ */

typedef struct GPULoadStore {
  eGPULoadOp load_action;
  eGPUStoreOp store_action;
} GPULoadStore;

/* Empty bind point. */
#define NULL_LOAD_STORE \
  { \
    GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_DONT_CARE \
  }

/**
 * Load store config array (load_store_actions) matches attachment structure of
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
 * \endcode
 */
void GPU_framebuffer_bind_loadstore(GPUFrameBuffer *framebuffer,
                                    const GPULoadStore *load_store_actions,
                                    uint load_store_actions_len);
#define GPU_framebuffer_bind_ex(_fb, ...) \
  { \
    GPULoadStore actions[] = __VA_ARGS__; \
    GPU_framebuffer_bind_loadstore(_fb, actions, (sizeof(actions) / sizeof(GPULoadStore))); \
  }

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attachments
 * \{ */

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
 * \endcode
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
void GPU_framebuffer_config_array(GPUFrameBuffer *framebuffer,
                                  const GPUAttachment *config,
                                  int config_len);

/** Empty bind point. */
#define GPU_ATTACHMENT_NONE \
  { \
    NULL, -1, 0, \
  }
/** Leave currently bound texture in this slot. DEPRECATED: Specify all textures for clarity. */
#define GPU_ATTACHMENT_LEAVE \
  { \
    NULL, -1, -1, \
  }
/** Bind the first mip level of a texture (all layers). */
#define GPU_ATTACHMENT_TEXTURE(_texture) \
  { \
    _texture, -1, 0, \
  }
/** Bind the \a _mip level of a texture (all layers). */
#define GPU_ATTACHMENT_TEXTURE_MIP(_texture, _mip) \
  { \
    _texture, -1, _mip, \
  }
/** Bind the \a _layer layer of the first mip level of a texture. */
#define GPU_ATTACHMENT_TEXTURE_LAYER(_texture, _layer) \
  { \
    _texture, _layer, 0, \
  }
/** Bind the \a _layer layer of the \a _mip level of a texture. */
#define GPU_ATTACHMENT_TEXTURE_LAYER_MIP(_texture, _layer, _mip) \
  { \
    _texture, _layer, _mip, \
  }

/** NOTE: The cube-face variants are equivalent to the layer ones but give better semantic. */

/** Bind the first mip level of a cube-map \a _face texture. */
#define GPU_ATTACHMENT_TEXTURE_CUBEFACE(_texture, _face) \
  { \
    _texture, _face, 0, \
  }
/** Bind the \a _mip level of a cube-map \a _face texture. */
#define GPU_ATTACHMENT_TEXTURE_CUBEFACE_MIP(_texture, _face, _mip) \
  { \
    _texture, _face, _mip, \
  }

/**
 * Attach an entire texture mip level to a #GPUFrameBuffer.
 * Changes will only take effect next time `GPU_framebuffer_bind()` is called.
 * \a slot is the color texture slot to bind this texture to. Must be 0 if it is the depth texture.
 * \a mip is the mip level of this texture to attach to the framebuffer.
 * DEPRECATED: Prefer using multiple #GPUFrameBuffer with different configurations with
 * `GPU_framebuffer_config_array()`.
 */
void GPU_framebuffer_texture_attach(GPUFrameBuffer *framebuffer,
                                    GPUTexture *texture,
                                    int slot,
                                    int mip);

/**
 * Attach a single layer of an array texture mip level to a #GPUFrameBuffer.
 * Changes will only take effect next time `GPU_framebuffer_bind()` is called.
 * \a slot is the color texture slot to bind this texture to. Must be 0 if it is the depth texture.
 * \a layer is the layer of this array texture to attach to the framebuffer.
 * \a mip is the mip level of this texture to attach to the framebuffer.
 * DEPRECATED: Prefer using multiple #GPUFrameBuffer with different configurations with
 * `GPU_framebuffer_config_array()`.
 */
void GPU_framebuffer_texture_layer_attach(
    GPUFrameBuffer *framebuffer, GPUTexture *texture, int slot, int layer, int mip);

/**
 * Attach a single cube-face of an cube-map texture mip level to a #GPUFrameBuffer.
 * Changes will only take effect next time `GPU_framebuffer_bind()` is called.
 * \a slot is the color texture slot to bind this texture to. Must be 0 if it is the depth texture.
 * \a face is the cube-face of this cube-map texture to attach to the framebuffer.
 * \a mip is the mip level of this texture to attach to the framebuffer.
 * DEPRECATED: Prefer using multiple #GPUFrameBuffer with different configurations with
 * `GPU_framebuffer_config_array()`.
 */
void GPU_framebuffer_texture_cubeface_attach(
    GPUFrameBuffer *framebuffer, GPUTexture *texture, int slot, int face, int mip);

/**
 * Detach a texture from a #GPUFrameBuffer. The texture must be attached.
 * Changes will only take effect next time `GPU_framebuffer_bind()` is called.
 * DEPRECATED: Prefer using multiple #GPUFrameBuffer with different configurations with
 * `GPU_framebuffer_config_array()`.
 */
void GPU_framebuffer_texture_detach(GPUFrameBuffer *framebuffer, GPUTexture *texture);

/**
 * Checks a framebuffer current configuration for errors.
 * Checks for texture size mismatch, incompatible attachment, incomplete textures etc...
 * \note This binds the framebuffer to the active context.
 * \a err_out is an error output buffer.
 * \return false if the framebuffer is invalid.
 */
bool GPU_framebuffer_check_valid(GPUFrameBuffer *framebuffer, char err_out[256]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Empty frame-buffer
 *
 * An empty frame-buffer is a frame-buffer with no attachments. This allow to rasterize geometry
 * without creating any dummy attachments and write some computation results using other means
 * (SSBOs, Images).
 * \{ */

/**
 * Default size is used if the frame-buffer contains no attachments.
 * It needs to be re-specified each time an attachment is added.
 */
void GPU_framebuffer_default_size(GPUFrameBuffer *framebuffer, int width, int height);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal state
 * \{ */

/**
 * Set a the viewport offset and size.
 * These are reset to the original dimensions explicitly (using `GPU_framebuffer_viewport_reset()`)
 * or when binding the frame-buffer after modifying its attachments.
 *
 * \note Viewport and scissor size is stored per frame-buffer.
 * \note Setting a singular viewport will only change the state of the first viewport.
 * \note Must be called after first bind.
 */
void GPU_framebuffer_viewport_set(
    GPUFrameBuffer *framebuffer, int x, int y, int width, int height);

/**
 * Similar to `GPU_framebuffer_viewport_set()` but specify the bounds of all 16 viewports.
 * By default geometry renders only to the first viewport. That can be changed by setting
 * `gpu_ViewportIndex` in the vertex.
 *
 * \note Viewport and scissor size is stored per frame-buffer.
 * \note Must be called after first bind.
 */
void GPU_framebuffer_multi_viewports_set(GPUFrameBuffer *gpu_fb,
                                         const int viewport_rects[GPU_MAX_VIEWPORTS][4]);

/**
 * Return the viewport offset and size in a int quadruple: (x, y, width, height).
 * \note Viewport and scissor size is stored per frame-buffer.
 */
void GPU_framebuffer_viewport_get(GPUFrameBuffer *framebuffer, int r_viewport[4]);

/**
 * Reset a frame-buffer viewport bounds to its attachment(s) size.
 * \note Viewport and scissor size is stored per frame-buffer.
 */
void GPU_framebuffer_viewport_reset(GPUFrameBuffer *framebuffer);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clearing
 * \{ */

/**
 * Clear the frame-buffer attachments.
 * \a buffers controls the types of attachments to clear. Setting GPU_COLOR_BIT will clear *all*
 * the color attachment.
 * Each attachment gets cleared to the value of its type:
 * - color attachments gets cleared to \a clear_col .
 * - depth attachment gets cleared to \a clear_depth .
 * - stencil attachment gets cleared to \a clear_stencil .
 *
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear(GPUFrameBuffer *framebuffer,
                           eGPUFrameBufferBits buffers,
                           const float clear_col[4],
                           float clear_depth,
                           unsigned int clear_stencil);

/**
 * Clear all color attachment textures with the value \a clear_col .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_color(GPUFrameBuffer *fb, const float clear_col[4]);

/**
 * Clear the depth attachment texture with the value \a clear_depth .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_depth(GPUFrameBuffer *fb, float clear_depth);

/**
 * Clear the stencil attachment with the value \a clear_stencil .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_stencil(GPUFrameBuffer *fb, uint clear_stencil);

/**
 * Clear all color attachment textures with the value \a clear_col and the depth attachment texture
 * with the value \a clear_depth .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_color_depth(GPUFrameBuffer *fb,
                                       const float clear_col[4],
                                       float clear_depth);

/**
 * Clear the depth attachment texture with the value \a clear_depth and the stencil attachment with
 * the value \a clear_stencil .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_depth_stencil(GPUFrameBuffer *fb,
                                         float clear_depth,
                                         uint clear_stencil);
/**
 * Clear the depth attachment texture with the value \a clear_depth , the stencil attachment with
 * the value \a clear_stencil and all the color attachments with the value \a clear_col .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_color_depth_stencil(GPUFrameBuffer *fb,
                                               const float clear_col[4],
                                               float clear_depth,
                                               uint clear_stencil);

/**
 * Clear each color attachment texture attached to this frame-buffer with a different color.
 * IMPORTANT: The size of `clear_colors` must match the number of color attachments.
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_multi_clear(GPUFrameBuffer *framebuffer, const float (*clear_colors)[4]);

/**
 * Clear all color attachment textures of the active frame-buffer with the given red, green, blue,
 * alpha values.
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 * DEPRECATED: Use `GPU_framebuffer_clear_color` with explicit frame-buffer.
 */
void GPU_clear_color(float red, float green, float blue, float alpha);

/**
 * Clear the depth attachment texture of the active frame-buffer with the given depth value.
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 * DEPRECATED: Use `GPU_framebuffer_clear_color` with explicit frame-buffer.
 */
void GPU_clear_depth(float depth);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debugging introspection API.
 * \{ */

const char *GPU_framebuffer_get_name(GPUFrameBuffer *framebuffer);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Python API & meta-data
 *
 * These are not intrinsic properties of a frame-buffer but they are stored inside the
 * gpu::FrameBuffer structure for tracking purpose.
 * \{ */

/**
 * Reference of a pointer that needs to be cleaned when deallocating the frame-buffer.
 * Points to #BPyGPUFrameBuffer.fb
 */
#ifndef GPU_NO_USE_PY_REFERENCES
void **GPU_framebuffer_py_reference_get(GPUFrameBuffer *framebuffer);
void GPU_framebuffer_py_reference_set(GPUFrameBuffer *framebuffer, void **py_ref);
#endif

/**
 * Keep a stack of bound frame-buffer to allow scoped binding of frame-buffer in python.
 * This is also used by #GPUOffScreen to save/restore the current frame-buffers.
 * \note This isn't thread safe.
 */
/* TODO(fclem): This has nothing to do with the GPU module and should be move to the pyGPU module.
 */
void GPU_framebuffer_push(GPUFrameBuffer *framebuffer);
GPUFrameBuffer *GPU_framebuffer_pop(void);
uint GPU_framebuffer_stack_level_get(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deprecated
 * \{ */

/**
 * Return true if \a framebuffer is the active framebuffer of the active context.
 * \note return false if no context is active.
 * \note this is undefined behavior if \a framebuffer is `nullptr`.
 * DEPRECATED: Kept only because of Python GPU API. */
bool GPU_framebuffer_bound(GPUFrameBuffer *framebuffer);

/**
 * Read a region of the framebuffer depth attachment and copy it to \a r_data .
 * The pixel data will be converted to \a data_format but it needs to be compatible with the
 * attachment type. DEPRECATED: Prefer using `GPU_texture_read()`.
 */
void GPU_framebuffer_read_depth(GPUFrameBuffer *framebuffer,
                                int x,
                                int y,
                                int width,
                                int height,
                                eGPUDataFormat data_format,
                                void *r_data);

/**
 * Read a region of a framebuffer color attachment and copy it to \a r_data .
 * The pixel data will be converted to \a data_format but it needs to be compatible with the
 * attachment type. DEPRECATED: Prefer using `GPU_texture_read()`.
 */
void GPU_framebuffer_read_color(GPUFrameBuffer *framebuffer,
                                int x,
                                int y,
                                int width,
                                int height,
                                int channels,
                                int slot,
                                eGPUDataFormat data_format,
                                void *r_data);

/**
 * Read a the color of the window screen as it is currently displayed (so the previously rendered
 * back-buffer).
 * DEPRECATED: This isn't even working correctly on some implementation.
 * TODO: Emulate this by doing some slow texture copy on the backend side or try to read the areas
 * offscreen textures directly.
 */
void GPU_frontbuffer_read_color(
    int x, int y, int width, int height, int channels, eGPUDataFormat data_format, void *r_data);

/**
 * Copy the content of \a fb_read attachments to the \a fb_read attachments.
 * The attachments types are chosen by \a blit_buffers .
 * Only one color buffer can by copied at a time and its index is chosen by \a read_slot and \a
 * write_slot.
 * The source and destination frame-buffers dimensions have to match.
 * DEPRECATED: Prefer using `GPU_texture_copy()`.
 */
void GPU_framebuffer_blit(GPUFrameBuffer *fb_read,
                          int read_slot,
                          GPUFrameBuffer *fb_write,
                          int write_slot,
                          eGPUFrameBufferBits blit_buffers);

/**
 * Call \a per_level_callback after binding each framebuffer attachment mip level
 * up until \a max_level .
 * Each attachment texture sampler mip range is set to not overlap the currently processed level.
 * This is used for generating custom mip-map chains where each level needs access to the one
 * above.
 * DEPRECATED: Prefer using a compute shader with arbitrary imageLoad/Store for this purpose
 * as it is clearer and likely faster with optimizations.
 */
void GPU_framebuffer_recursive_downsample(GPUFrameBuffer *framebuffer,
                                          int max_level,
                                          void (*per_level_callback)(void *user_data, int level),
                                          void *user_data);

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU OffScreen
 *
 * A `GPUOffScreen` is a convenience type that holds a `GPUFramebuffer` and its associated
 * `GPUTexture`s. It is useful for quick drawing surface configuration.
 * NOTE: They are still limited by the same single context limitation as #GPUFrameBuffer.
 * \{ */

typedef struct GPUOffScreen GPUOffScreen;

/**
 * Create a #GPUOffScreen with attachment size of \a width by \a height pixels.
 * If \a with_depth_buffer is true, a depth buffer attachment will also be created.
 * \a format is the format of the color buffer.
 * If \a err_out is not `nullptr` it will be use to write any configuration error message..
 * \note This function binds the framebuffer to the active context.
 * \note `GPU_TEXTURE_USAGE_ATTACHMENT` is added to the usage parameter by default.
 */
GPUOffScreen *GPU_offscreen_create(int width,
                                   int height,
                                   bool with_depth_buffer,
                                   eGPUTextureFormat format,
                                   eGPUTextureUsage usage,
                                   char err_out[256]);

/**
 * Free a #GPUOffScreen.
 */
void GPU_offscreen_free(GPUOffScreen *offscreen);

/**
 * Unbind a #GPUOffScreen from a #GPUContext.
 * If \a save is true, it will save the currently bound framebuffer into a stack.
 */
/* TODO(fclem): Remove the `save` parameter and always save/restore. */
void GPU_offscreen_bind(GPUOffScreen *offscreen, bool save);

/**
 * Unbind a #GPUOffScreen from a #GPUContext.
 * If \a restore is true, it will restore the previously bound framebuffer. If false, it will bind
 * the window back-buffer.
 */
/* TODO(fclem): Remove the `save` parameter and always save/restore. */
void GPU_offscreen_unbind(GPUOffScreen *offscreen, bool restore);

/**
 * Read the whole color texture of the a #GPUOffScreen.
 * The pixel data will be converted to \a data_format but it needs to be compatible with the
 * attachment type.
 * IMPORTANT: \a r_data must be big enough for all pixels in \a data_format .
 */
void GPU_offscreen_read_color(GPUOffScreen *offscreen, eGPUDataFormat data_format, void *r_data);
/**
 * A version of #GPU_offscreen_read_color that reads into a region.
 */
void GPU_offscreen_read_color_region(
    GPUOffScreen *offscreen, eGPUDataFormat data_format, int x, int y, int w, int h, void *r_data);

/**
 * Blit the offscreen color texture to the active framebuffer at the `(x, y)` location.
 */
void GPU_offscreen_draw_to_screen(GPUOffScreen *offscreen, int x, int y);

/**
 * Return the width of a #GPUOffScreen.
 */
int GPU_offscreen_width(const GPUOffScreen *offscreen);

/**
 * Return the height of a #GPUOffScreen.
 */
int GPU_offscreen_height(const GPUOffScreen *offscreen);

/**
 * Return the color texture of a #GPUOffScreen. Does not give ownership.
 * \note only to be used by viewport code!
 */
struct GPUTexture *GPU_offscreen_color_texture(const GPUOffScreen *offscreen);

/**
 * Return the internals of a #GPUOffScreen. Does not give ownership.
 * \note only to be used by viewport code!
 */
void GPU_offscreen_viewport_data_get(GPUOffScreen *offscreen,
                                     GPUFrameBuffer **r_fb,
                                     struct GPUTexture **r_color,
                                     struct GPUTexture **r_depth);

/** \} */

#ifdef __cplusplus
}
#endif
