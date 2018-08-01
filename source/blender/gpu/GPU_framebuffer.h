/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_framebuffer.h
 *  \ingroup gpu
 */

#ifndef __GPU_FRAMEBUFFER_H__
#define __GPU_FRAMEBUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

struct GPUTexture;

typedef struct GPUAttachment {
	struct GPUTexture *tex;
	int mip, layer;
} GPUAttachment;

typedef enum GPUFrameBufferBits {
	GPU_COLOR_BIT    = (1 << 0),
	GPU_DEPTH_BIT    = (1 << 1),
	GPU_STENCIL_BIT  = (1 << 2),
} GPUFrameBufferBits;

typedef struct GPUFrameBuffer GPUFrameBuffer;
typedef struct GPUOffScreen GPUOffScreen;

/* GPU Framebuffer
 * - this is a wrapper for an OpenGL framebuffer object (FBO). in practice
 *   multiple FBO's may be created, to get around limitations on the number
 *   of attached textures and the dimension requirements.
 * - actual FBO creation & config is deferred until GPU_framebuffer_bind or
 *   GPU_framebuffer_check_valid to allow creation & config while another
 *   opengl context is bound (since FBOs are not shared between ogl contexts).
 */

GPUFrameBuffer *GPU_framebuffer_create(void);
void GPU_framebuffer_free(GPUFrameBuffer *fb);
void GPU_framebuffer_bind(GPUFrameBuffer *fb);
void GPU_framebuffer_restore(void);

bool GPU_framebuffer_bound(GPUFrameBuffer *fb);
bool GPU_framebuffer_check_valid(GPUFrameBuffer *fb, char err_out[256]);

GPUFrameBuffer *GPU_framebuffer_active_get(void);

#define GPU_FRAMEBUFFER_FREE_SAFE(fb) do { \
	if (fb != NULL) { \
		GPU_framebuffer_free(fb); \
		fb = NULL; \
	} \
} while (0)

/* Framebuffer setup : You need to call GPU_framebuffer_bind for theses
 * to be effective. */

void GPU_framebuffer_texture_attach(
        GPUFrameBuffer *fb, struct GPUTexture *tex, int slot, int mip);
void GPU_framebuffer_texture_layer_attach(
        GPUFrameBuffer *fb, struct GPUTexture *tex, int slot, int layer, int mip);
void GPU_framebuffer_texture_cubeface_attach(
        GPUFrameBuffer *fb, struct GPUTexture *tex, int slot, int face, int mip);
void GPU_framebuffer_texture_detach(GPUFrameBuffer *fb, struct GPUTexture *tex);
void GPU_framebuffer_texture_detach_slot(
        GPUFrameBuffer *fb, struct GPUTexture *tex, int type);

/**
 * How to use GPU_framebuffer_ensure_config().
 *
 * Example :
 * GPU_framebuffer_ensure_config(&fb, {
 *         GPU_ATTACHMENT_TEXTURE(depth), // must be depth buffer
 *         GPU_ATTACHMENT_TEXTURE(tex1),
 *         GPU_ATTACHMENT_TEXTURE_CUBEFACE(tex2, 0),
 *         GPU_ATTACHMENT_TEXTURE_LAYER_MIP(tex2, 0, 0)
 * })
 *
 * Note : Unspecified attachements (i.e: those beyond the last
 *        GPU_ATTACHMENT_* in GPU_framebuffer_ensure_config list)
 *        are left unchanged.
 * Note : Make sure that the dimensions of your textures matches
 *        otherwise you will have an invalid framebuffer error.
 **/
#define GPU_framebuffer_ensure_config(_fb, ...) do { \
	if (*(_fb) == NULL) { \
		*(_fb) = GPU_framebuffer_create(); \
	} \
	GPUAttachment config[] = __VA_ARGS__; \
	GPU_framebuffer_config_array(*(_fb), config, (sizeof(config) / sizeof(GPUAttachment))); \
} while (0)

void GPU_framebuffer_config_array(GPUFrameBuffer *fb, const GPUAttachment *config, int config_len);

#define GPU_ATTACHMENT_NONE \
        {.tex = NULL, .layer = -1, .mip = 0}
#define GPU_ATTACHMENT_LEAVE \
        {.tex = NULL, .layer = -1, .mip = -1}
#define GPU_ATTACHMENT_TEXTURE(_tex) \
        {.tex = _tex, .layer = -1, .mip = 0}
#define GPU_ATTACHMENT_TEXTURE_MIP(_tex, _mip) \
        {.tex = _tex, .layer = -1, .mip = _mip}
#define GPU_ATTACHMENT_TEXTURE_LAYER(_tex, _layer) \
        {.tex = _tex, .layer = _layer, .mip = 0}
#define GPU_ATTACHMENT_TEXTURE_LAYER_MIP(_tex, _layer, _mip) \
        {.tex = _tex, .layer = _layer, .mip = _mip}
#define GPU_ATTACHMENT_TEXTURE_CUBEFACE(_tex, _face) \
        {.tex = _tex, .layer = _face, .mip = 0}
#define GPU_ATTACHMENT_TEXTURE_CUBEFACE_MIP(_tex, _face, _mip) \
        {.tex = _tex, .layer = _face, .mip = _mip}

/* Framebuffer operations */

void GPU_framebuffer_viewport_set(GPUFrameBuffer *fb, int x, int y, int w, int h);

void GPU_framebuffer_clear(
        GPUFrameBuffer *fb, GPUFrameBufferBits buffers,
        const float clear_col[4], float clear_depth, unsigned int clear_stencil);

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

void GPU_framebuffer_read_depth(GPUFrameBuffer *fb, int x, int y, int w, int h, float *data);
void GPU_framebuffer_read_color(
        GPUFrameBuffer *fb, int x, int y, int w, int h, int channels, int slot, float *data);

void GPU_framebuffer_blit(
        GPUFrameBuffer *fb_read, int read_slot,
        GPUFrameBuffer *fb_write, int write_slot,
        GPUFrameBufferBits blit_buffers);

void GPU_framebuffer_recursive_downsample(
        GPUFrameBuffer *fb, int max_lvl,
        void (*callback)(void *userData, int level), void *userData);

/* GPU OffScreen
 * - wrapper around framebuffer and texture for simple offscreen drawing
 */

GPUOffScreen *GPU_offscreen_create(
        int width, int height, int samples,
        bool depth, bool high_bitdepth, char err_out[256]);
void GPU_offscreen_free(GPUOffScreen *ofs);
void GPU_offscreen_bind(GPUOffScreen *ofs, bool save);
void GPU_offscreen_unbind(GPUOffScreen *ofs, bool restore);
void GPU_offscreen_read_pixels(GPUOffScreen *ofs, int type, void *pixels);
void GPU_offscreen_draw_to_screen(GPUOffScreen *ofs, int x, int y);
int GPU_offscreen_width(const GPUOffScreen *ofs);
int GPU_offscreen_height(const GPUOffScreen *ofs);
struct GPUTexture *GPU_offscreen_color_texture(const GPUOffScreen *ofs);

void GPU_offscreen_viewport_data_get(
        GPUOffScreen *ofs,
        GPUFrameBuffer **r_fb, struct GPUTexture **r_color, struct GPUTexture **r_depth);

void GPU_clear_color(float red, float green, float blue, float alpha);
void GPU_clear(GPUFrameBufferBits flags);

#ifdef __cplusplus
}
#endif

#endif  /* __GPU_FRAMEBUFFER_H__ */
