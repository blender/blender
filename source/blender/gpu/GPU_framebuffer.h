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
int GPU_framebuffer_texture_attach(GPUFrameBuffer *fb, struct GPUTexture *tex, int slot, char err_out[256]);
void GPU_framebuffer_texture_detach(struct GPUTexture *tex);
void GPU_framebuffer_slots_bind(GPUFrameBuffer *fb, int slot);
void GPU_framebuffer_texture_unbind(GPUFrameBuffer *fb, struct GPUTexture *tex);
void GPU_framebuffer_free(GPUFrameBuffer *fb);
bool GPU_framebuffer_check_valid(GPUFrameBuffer *fb, char err_out[256]);

void GPU_framebuffer_bind_no_save(GPUFrameBuffer *fb, int slot);

bool GPU_framebuffer_bound(GPUFrameBuffer *fb);

void GPU_framebuffer_restore(void);
void GPU_framebuffer_blur(
        GPUFrameBuffer *fb, struct GPUTexture *tex,
        GPUFrameBuffer *blurfb, struct GPUTexture *blurtex);

/* GPU OffScreen
 * - wrapper around framebuffer and texture for simple offscreen drawing
 * - changes size if graphics card can't support it */

GPUOffScreen *GPU_offscreen_create(int width, int height, int samples, char err_out[256]);
void GPU_offscreen_free(GPUOffScreen *ofs);
void GPU_offscreen_bind(GPUOffScreen *ofs, bool save);
void GPU_offscreen_unbind(GPUOffScreen *ofs, bool restore);
void GPU_offscreen_read_pixels(GPUOffScreen *ofs, int type, void *pixels);
int GPU_offscreen_width(const GPUOffScreen *ofs);
int GPU_offscreen_height(const GPUOffScreen *ofs);
int GPU_offscreen_color_texture(const GPUOffScreen *ofs);

#ifdef __cplusplus
}
#endif

#endif  /* __GPU_FRAMEBUFFER_H__ */
