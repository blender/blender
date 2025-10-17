/* SPDX-FileCopyrightText: 2005 Blender Authors
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
 * When a `gpu::Texture` is attached to a `GPUFramebuffer` a reference is created.
 * Deleting either does not require any unbinding.
 *
 * A `GPUOffScreen` is a convenience type that holds a `GPUFramebuffer` and its associated
 * `gpu::Texture`s. It is useful for quick drawing surface configuration.
 */

#pragma once

#include "GPU_common_types.hh"
#include "GPU_texture.hh"

#include "BLI_enum_flags.hh"

namespace blender::gpu {
class Texture;
class FrameBuffer;
}  // namespace blender::gpu

enum GPUFrameBufferBits {
  GPU_COLOR_BIT = (1 << 0),
  GPU_DEPTH_BIT = (1 << 1),
  GPU_STENCIL_BIT = (1 << 2),
};

ENUM_OPERATORS(GPUFrameBufferBits)

/* Guaranteed by the spec and is never greater than 16 on any hardware or implementation. */
constexpr static int GPU_MAX_VIEWPORTS = 16;

struct GPUAttachment {
  blender::gpu::Texture *tex;
  int layer, mip;
};

/* -------------------------------------------------------------------- */
/** \name Creation
 * \{ */

/**
 * Create a #FrameBuffer object. It is not configured and not bound to a specific context until
 * `GPU_framebuffer_bind()` is called.
 */
blender::gpu::FrameBuffer *GPU_framebuffer_create(const char *name);

/**
 * Returns the current context active framebuffer.
 * Return nullptr if no context is active.
 */
blender::gpu::FrameBuffer *GPU_framebuffer_active_get();

/**
 * Returns the default (back-left) frame-buffer. It will always exists even if it's just a dummy.
 * Return nullptr if no context is active.
 */
blender::gpu::FrameBuffer *GPU_framebuffer_back_get();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free
 * \{ */

/**
 * Create a #FrameBuffer object. It is not configured and not bound to a specific context until
 * `GPU_framebuffer_bind()` is called.
 */
void GPU_framebuffer_free(blender::gpu::FrameBuffer *fb);

#define GPU_FRAMEBUFFER_FREE_SAFE(fb) \
  do { \
    if (fb != nullptr) { \
      GPU_framebuffer_free(fb); \
      fb = nullptr; \
    } \
  } while (0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

enum GPUBackBuffer {
  /** Default framebuffer of a window. Always available. */
  GPU_BACKBUFFER_LEFT = 0,
  /** Right buffer of a window. Only available if window was created using stereo-view. */
  GPU_BACKBUFFER_RIGHT,
};

/**
 * Binds the active context's window frame-buffer.
 * Note that `GPU_BACKBUFFER_RIGHT` is only available if the window was created using stereo-view.
 */
void GPU_backbuffer_bind(GPUBackBuffer back_buffer_type);

/**
 * Binds a #FrameBuffer making it the active framebuffer for all geometry rendering.
 */
void GPU_framebuffer_bind(blender::gpu::FrameBuffer *fb);

/**
 * Same as `GPU_framebuffer_bind` but do not enable the SRGB transform.
 */
void GPU_framebuffer_bind_no_srgb(blender::gpu::FrameBuffer *fb);

/**
 * Binds back the active context's default frame-buffer.
 * Equivalent to `GPU_backbuffer_bind(GPU_BACKBUFFER_LEFT)`.
 */
void GPU_framebuffer_restore();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Advanced binding control
 * \{ */

struct GPULoadStore {
  GPULoadOp load_action;
  GPUStoreOp store_action;
  float clear_value[4];
};

/* Empty bind point. */
#define NULL_ATTACHMENT_COLOR {0.0, 0.0, 0.0, 0.0}
#define NULL_LOAD_STORE \
  {GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_DONT_CARE, NULL_ATTACHMENT_COLOR}

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
 * NOTE: Using GPU_framebuffer_clear_* functions in conjunction with a custom load-store
 * configuration is invalid. Instead, utilize GPU_LOADACTION_CLEAR and provide a clear color as
 * the third parameter in `GPULoadStore action`.
 *
 * For Color attachments: `{GPU_LOADACTION_CLEAR, GPU_STOREACTION_STORE, {Rf, Gf, Bf, Af}}`
 * For Depth attachments: `{GPU_LOADACTION_CLEAR, GPU_STOREACTION_STORE, {Df}}`
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
void GPU_framebuffer_bind_loadstore(blender::gpu::FrameBuffer *fb,
                                    const GPULoadStore *load_store_actions,
                                    uint load_store_actions_len);
#define GPU_framebuffer_bind_ex(_fb, ...) \
  { \
    GPULoadStore actions[] = __VA_ARGS__; \
    GPU_framebuffer_bind_loadstore(_fb, actions, (sizeof(actions) / sizeof(GPULoadStore))); \
  }

/**
 * Sub-pass config array matches attachment structure of `GPU_framebuffer_config_array`.
 * This allows to explicitly specify attachment state within the next sub-pass.
 * This enables a number of bandwidth optimizations specially on Tile Based Deferred Renderers
 * where the attachments can be kept into tile memory and used in place for later sub-passes.
 *
 * IMPORTANT: When using this, the framebuffer initial state is undefined. A sub-pass transition
 * need to be issued before any draw-call.
 *
 * Example:
 * \code{.c}
 * GPU_framebuffer_bind_loadstore(&fb, {
 *         GPU_ATTACHMENT_WRITE,  // must be depth buffer
 *         GPU_ATTACHMENT_READ,   // Color attachment 0
 *         GPU_ATTACHMENT_IGNORE, // Color attachment 1
 *         GPU_ATTACHMENT_WRITE}  // Color attachment 2
 * })
 * \endcode
 *
 * \note Excess attachments will have no effect as long as they are GPU_ATTACHMENT_IGNORE.
 */
void GPU_framebuffer_subpass_transition_array(blender::gpu::FrameBuffer *fb,
                                              const GPUAttachmentState *attachment_states,
                                              uint attachment_len);

#define GPU_framebuffer_subpass_transition(_fb, ...) \
  { \
    GPUAttachmentState actions[] = __VA_ARGS__; \
    GPU_framebuffer_subpass_transition_array( \
        _fb, actions, (sizeof(actions) / sizeof(GPUAttachmentState))); \
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
    if (*(_fb) == nullptr) { \
      *(_fb) = GPU_framebuffer_create(#_fb); \
    } \
    GPUAttachment config[] = __VA_ARGS__; \
    GPU_framebuffer_config_array(*(_fb), config, (sizeof(config) / sizeof(GPUAttachment))); \
  } while (0)

/**
 * First #GPUAttachment in *config is always the depth/depth_stencil buffer.
 * Following #GPUAttachments are color buffers.
 * Setting #GPUAttachment.mip to -1 will leave the texture in this slot.
 * Setting #GPUAttachment.tex to nullptr will detach the texture in this slot.
 */
void GPU_framebuffer_config_array(blender::gpu::FrameBuffer *fb,
                                  const GPUAttachment *config,
                                  int config_len);

/** Empty bind point. */
#define GPU_ATTACHMENT_NONE \
  { \
      nullptr, \
      -1, \
      0, \
  }
/** Leave currently bound texture in this slot. DEPRECATED: Specify all textures for clarity. */
#define GPU_ATTACHMENT_LEAVE \
  { \
      nullptr, \
      -1, \
      -1, \
  }
/** Bind the first mip level of a texture (all layers). */
#define GPU_ATTACHMENT_TEXTURE(_texture) \
  { \
      _texture, \
      -1, \
      0, \
  }
/** Bind the \a _mip level of a texture (all layers). */
#define GPU_ATTACHMENT_TEXTURE_MIP(_texture, _mip) \
  { \
      _texture, \
      -1, \
      _mip, \
  }
/** Bind the \a _layer layer of the first mip level of a texture. */
#define GPU_ATTACHMENT_TEXTURE_LAYER(_texture, _layer) \
  { \
      _texture, \
      _layer, \
      0, \
  }
/** Bind the \a _layer layer of the \a _mip level of a texture. */
#define GPU_ATTACHMENT_TEXTURE_LAYER_MIP(_texture, _layer, _mip) \
  { \
      _texture, \
      _layer, \
      _mip, \
  }

/** NOTE: The cube-face variants are equivalent to the layer ones but give better semantic. */

/** Bind the first mip level of a cube-map \a _face texture. */
#define GPU_ATTACHMENT_TEXTURE_CUBEFACE(_texture, _face) \
  { \
      _texture, \
      _face, \
      0, \
  }
/** Bind the \a _mip level of a cube-map \a _face texture. */
#define GPU_ATTACHMENT_TEXTURE_CUBEFACE_MIP(_texture, _face, _mip) \
  { \
      _texture, \
      _face, \
      _mip, \
  }

/**
 * Attach an entire texture mip level to a #FrameBuffer.
 * Changes will only take effect next time `GPU_framebuffer_bind()` is called.
 * \a slot is the color texture slot to bind this texture to. Must be 0 if it is the depth texture.
 * \a mip is the mip level of this texture to attach to the framebuffer.
 * DEPRECATED: Prefer using multiple #FrameBuffer with different configurations with
 * `GPU_framebuffer_config_array()`.
 */
void GPU_framebuffer_texture_attach(blender::gpu::FrameBuffer *fb,
                                    blender::gpu::Texture *texture,
                                    int slot,
                                    int mip);

/**
 * Attach a single layer of an array texture mip level to a #FrameBuffer.
 * Changes will only take effect next time `GPU_framebuffer_bind()` is called.
 * \a slot is the color texture slot to bind this texture to. Must be 0 if it is the depth texture.
 * \a layer is the layer of this array texture to attach to the framebuffer.
 * \a mip is the mip level of this texture to attach to the framebuffer.
 * DEPRECATED: Prefer using multiple #FrameBuffer with different configurations with
 * `GPU_framebuffer_config_array()`.
 */
void GPU_framebuffer_texture_layer_attach(
    blender::gpu::FrameBuffer *fb, blender::gpu::Texture *texture, int slot, int layer, int mip);

/**
 * Attach a single cube-face of an cube-map texture mip level to a #FrameBuffer.
 * Changes will only take effect next time `GPU_framebuffer_bind()` is called.
 * \a slot is the color texture slot to bind this texture to. Must be 0 if it is the depth texture.
 * \a face is the cube-face of this cube-map texture to attach to the framebuffer.
 * \a mip is the mip level of this texture to attach to the framebuffer.
 * DEPRECATED: Prefer using multiple #FrameBuffer with different configurations with
 * `GPU_framebuffer_config_array()`.
 */
void GPU_framebuffer_texture_cubeface_attach(
    blender::gpu::FrameBuffer *fb, blender::gpu::Texture *texture, int slot, int face, int mip);

/**
 * Detach a texture from a #FrameBuffer. The texture must be attached.
 * Changes will only take effect next time `GPU_framebuffer_bind()` is called.
 * DEPRECATED: Prefer using multiple #FrameBuffer with different configurations with
 * `GPU_framebuffer_config_array()`.
 */
void GPU_framebuffer_texture_detach(blender::gpu::FrameBuffer *fb, blender::gpu::Texture *texture);

/**
 * Checks a framebuffer current configuration for errors.
 * Checks for texture size mismatch, incompatible attachment, incomplete textures etc...
 * \note This binds the framebuffer to the active context.
 * \a err_out is an error output buffer.
 * \return false if the framebuffer is invalid.
 */
bool GPU_framebuffer_check_valid(blender::gpu::FrameBuffer *fb, char err_out[256]);

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
void GPU_framebuffer_default_size(blender::gpu::FrameBuffer *fb, int width, int height);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal state
 * \{ */

/**
 * Set the viewport offset and size.
 * These are reset to the original dimensions explicitly (using `GPU_framebuffer_viewport_reset()`)
 * or when binding the frame-buffer after modifying its attachments.
 *
 * \note Viewport and scissor size is stored per frame-buffer.
 * \note Setting a singular viewport will only change the state of the first viewport.
 * \note Must be called after first bind.
 */
void GPU_framebuffer_viewport_set(
    blender::gpu::FrameBuffer *fb, int x, int y, int width, int height);

/**
 * Similar to `GPU_framebuffer_viewport_set()` but specify the bounds of all 16 viewports.
 * By default geometry renders only to the first viewport. That can be changed by setting
 * `gpu_ViewportIndex` in the vertex.
 *
 * \note Viewport and scissor size is stored per frame-buffer.
 * \note Must be called after first bind.
 */
void GPU_framebuffer_multi_viewports_set(blender::gpu::FrameBuffer *gpu_fb,
                                         const int viewport_rects[GPU_MAX_VIEWPORTS][4]);

/**
 * Return the viewport offset and size in a int quadruple: (x, y, width, height).
 * \note Viewport and scissor size is stored per frame-buffer.
 */
void GPU_framebuffer_viewport_get(blender::gpu::FrameBuffer *fb, int r_viewport[4]);

/**
 * Reset a frame-buffer viewport bounds to its attachment(s) size.
 * \note Viewport and scissor size is stored per frame-buffer.
 */
void GPU_framebuffer_viewport_reset(blender::gpu::FrameBuffer *fb);

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
void GPU_framebuffer_clear(blender::gpu::FrameBuffer *fb,
                           GPUFrameBufferBits buffers,
                           const float clear_col[4],
                           float clear_depth,
                           unsigned int clear_stencil);

/**
 * Clear all color attachment textures with the value \a clear_col .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_color(blender::gpu::FrameBuffer *fb, const float clear_col[4]);

/**
 * Clear the depth attachment texture with the value \a clear_depth .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_depth(blender::gpu::FrameBuffer *fb, float clear_depth);

/**
 * Clear the stencil attachment with the value \a clear_stencil .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_stencil(blender::gpu::FrameBuffer *fb, uint clear_stencil);

/**
 * Clear all color attachment textures with the value \a clear_col and the depth attachment texture
 * with the value \a clear_depth .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_color_depth(blender::gpu::FrameBuffer *fb,
                                       const float clear_col[4],
                                       float clear_depth);

/**
 * Clear the depth attachment texture with the value \a clear_depth and the stencil attachment with
 * the value \a clear_stencil .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_depth_stencil(blender::gpu::FrameBuffer *fb,
                                         float clear_depth,
                                         uint clear_stencil);
/**
 * Clear the depth attachment texture with the value \a clear_depth , the stencil attachment with
 * the value \a clear_stencil and all the color attachments with the value \a clear_col .
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_clear_color_depth_stencil(blender::gpu::FrameBuffer *fb,
                                               const float clear_col[4],
                                               float clear_depth,
                                               uint clear_stencil);

/**
 * Clear each color attachment texture attached to this frame-buffer with a different color.
 * IMPORTANT: The size of `clear_colors` must match the number of color attachments.
 * \note `GPU_write_mask`, and stencil test do not affect this command.
 * \note Viewport and scissor regions affect this command but are not efficient nor recommended.
 */
void GPU_framebuffer_multi_clear(blender::gpu::FrameBuffer *fb, const float (*clear_colors)[4]);

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

const char *GPU_framebuffer_get_name(blender::gpu::FrameBuffer *fb);

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
void **GPU_framebuffer_py_reference_get(blender::gpu::FrameBuffer *fb);
void GPU_framebuffer_py_reference_set(blender::gpu::FrameBuffer *fb, void **py_ref);
#endif

/**
 * Keep a stack of bound frame-buffer to allow scoped binding of frame-buffer in python.
 * This is also used by #GPUOffScreen to save/restore the current frame-buffers.
 * \note This isn't thread safe.
 */
/* TODO(fclem): This has nothing to do with the GPU module and should be move to the pyGPU module.
 */
void GPU_framebuffer_push(blender::gpu::FrameBuffer *fb);
blender::gpu::FrameBuffer *GPU_framebuffer_pop();
uint GPU_framebuffer_stack_level_get();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deprecated
 * \{ */

/**
 * Return true if \a framebuffer is the active framebuffer of the active context.
 * \note return false if no context is active.
 * \note this is undefined behavior if \a framebuffer is `nullptr`.
 * DEPRECATED: Kept only because of Python GPU API. */
bool GPU_framebuffer_bound(blender::gpu::FrameBuffer *fb);

/**
 * Read a region of the framebuffer depth attachment and copy it to \a r_data .
 * The pixel data will be converted to \a data_format but it needs to be compatible with the
 * attachment type. DEPRECATED: Prefer using `GPU_texture_read()`.
 */
void GPU_framebuffer_read_depth(blender::gpu::FrameBuffer *fb,
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
void GPU_framebuffer_read_color(blender::gpu::FrameBuffer *fb,
                                int x,
                                int y,
                                int width,
                                int height,
                                int channels,
                                int slot,
                                eGPUDataFormat data_format,
                                void *r_data);

/**
 * Read the color of the window screen as it is currently displayed (so the previously rendered
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
void GPU_framebuffer_blit(blender::gpu::FrameBuffer *fb_read,
                          int read_slot,
                          blender::gpu::FrameBuffer *fb_write,
                          int write_slot,
                          GPUFrameBufferBits blit_buffers);

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU OffScreen
 *
 * A `GPUOffScreen` is a convenience type that holds a `GPUFramebuffer` and its associated
 * `blender::gpu::Texture`s. It is useful for quick drawing surface configuration.
 * NOTE: They are still limited by the same single context limitation as #FrameBuffer.
 * \{ */

struct GPUOffScreen;

/**
 * Create a #GPUOffScreen with attachment size of \a width by \a height pixels.
 * If \a with_depth_buffer is true, a depth buffer attachment will also be created.
 * \a format is the format of the color buffer.
 * If \a clear is true, the color and depth buffer attachments will be cleared.
 * If \a err_out is not `nullptr` it will be use to write any configuration error message..
 * \note This function binds the framebuffer to the active context.
 * \note `GPU_TEXTURE_USAGE_ATTACHMENT` is added to the usage parameter by default.
 */
GPUOffScreen *GPU_offscreen_create(int width,
                                   int height,
                                   bool with_depth_buffer,
                                   blender::gpu::TextureFormat format,
                                   eGPUTextureUsage usage,
                                   bool clear,
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
 * Read the whole color texture of the #GPUOffScreen.
 * The pixel data will be converted to \a data_format but it needs to be compatible with the
 * attachment type.
 * IMPORTANT: \a r_data must be big enough for all pixels in \a data_format.
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
blender::gpu::Texture *GPU_offscreen_color_texture(const GPUOffScreen *offscreen);

/**
 * Return the texture format of a #GPUOffScreen.
 */
blender::gpu::TextureFormat GPU_offscreen_format(const GPUOffScreen *offscreen);

/**
 * Return the internals of a #GPUOffScreen. Does not give ownership.
 * \note only to be used by viewport code!
 */
void GPU_offscreen_viewport_data_get(GPUOffScreen *offscreen,
                                     blender::gpu::FrameBuffer **r_fb,
                                     blender::gpu::Texture **r_color,
                                     blender::gpu::Texture **r_depth);

/** \} */
