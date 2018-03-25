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

typedef enum GPUFrameBufferBits{
	GPU_COLOR_BIT    = (1 << 0),
	GPU_DEPTH_BIT    = (1 << 1),
	GPU_STENCIL_BIT  = (1 << 2),
} GPUFrameBufferBits;

typedef struct GPUFrameBuffer GPUFrameBuffer;
typedef struct GPUOffScreen GPUOffScreen;
struct GPUTexture;

/* GPU Framebuffer
 * - this is a wrapper for an OpenGL framebuffer object (FBO). in practice
 *   multiple FBO's may be created, to get around limitations on the number
 *   of attached textures and the dimension requirements.
 * - after any of the GPU_framebuffer_* functions, GPU_framebuffer_restore must
 *   be called before rendering to the window framebuffer again */

void GPU_texture_bind_as_framebuffer(struct GPUTexture *tex);

GPUFrameBuffer *GPU_framebuffer_create(void);
bool GPU_framebuffer_texture_attach(GPUFrameBuffer *fb, struct GPUTexture *tex, int slot, int mip);
bool GPU_framebuffer_texture_layer_attach(
        GPUFrameBuffer *fb, struct GPUTexture *tex, int slot, int layer, int mip);
bool GPU_framebuffer_texture_cubeface_attach(
        GPUFrameBuffer *fb, struct GPUTexture *tex, int slot, int face, int mip);
void GPU_framebuffer_texture_detach(struct GPUTexture *tex);
void GPU_framebuffer_bind(GPUFrameBuffer *fb);
void GPU_framebuffer_slots_bind(GPUFrameBuffer *fb, int slot);
void GPU_framebuffer_texture_unbind(GPUFrameBuffer *fb, struct GPUTexture *tex);
void GPU_framebuffer_free(GPUFrameBuffer *fb);
bool GPU_framebuffer_check_valid(GPUFrameBuffer *fb, char err_out[256]);

/* internal use only */
unsigned int GPU_framebuffer_current_get(void);
void GPU_framebuffer_bind_no_save(GPUFrameBuffer *fb, int slot);

bool GPU_framebuffer_bound(GPUFrameBuffer *fb);
/* Framebuffer operations */

void GPU_framebuffer_viewport_set(GPUFrameBuffer *fb, int x, int y, int w, int h);

void GPU_framebuffer_restore(void);
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


void GPU_framebuffer_blit(
        GPUFrameBuffer *fb_read, int read_slot,
        GPUFrameBuffer *fb_write, int write_slot, bool use_depth, bool use_stencil);

void GPU_framebuffer_recursive_downsample(
        GPUFrameBuffer *fb, struct GPUTexture *tex, int num_iter,
        void (*callback)(void *userData, int level), void *userData);

/* GPU OffScreen
 * - wrapper around framebuffer and texture for simple offscreen drawing
 * - changes size if graphics card can't support it */

GPUOffScreen *GPU_offscreen_create(int width, int height, int samples,
        bool depth, bool high_bitdepth, char err_out[256]);
void GPU_offscreen_free(GPUOffScreen *ofs);
void GPU_offscreen_bind(GPUOffScreen *ofs, bool save);
void GPU_offscreen_unbind(GPUOffScreen *ofs, bool restore);
void GPU_offscreen_read_pixels(GPUOffScreen *ofs, int type, void *pixels);
int GPU_offscreen_width(const GPUOffScreen *ofs);
int GPU_offscreen_height(const GPUOffScreen *ofs);
struct GPUTexture *GPU_offscreen_color_texture(const GPUOffScreen *ofs);

void GPU_offscreen_viewport_data_get(
        GPUOffScreen *ofs,
        GPUFrameBuffer **r_fb, struct GPUTexture **r_color, struct GPUTexture **r_depth);

#ifdef __cplusplus
}
#endif

#endif  /* __GPU_FRAMEBUFFER_H__ */
