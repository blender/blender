/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/intern/draw_manager_framebuffer.c
 *  \ingroup draw
 */

#include "draw_manager.h"

GPUFrameBuffer *DRW_framebuffer_create(void)
{
	return GPU_framebuffer_create();
}

void DRW_framebuffer_init(
        GPUFrameBuffer **fb, void *engine_type, int width, int height,
        DRWFboTexture textures[MAX_FBO_TEX], int textures_len)
{
	BLI_assert(textures_len <= MAX_FBO_TEX);
	BLI_assert(width > 0 && height > 0);

	bool create_fb = false;
	int color_attachment = -1;

	if (!*fb) {
		*fb = GPU_framebuffer_create();
		create_fb = true;
	}

	for (int i = 0; i < textures_len; ++i) {
		int channels;
		bool is_depth;
		bool create_tex = false;
		GPUTextureFormat gpu_format;

		DRWFboTexture fbotex = textures[i];
		bool is_temp = (fbotex.flag & DRW_TEX_TEMP) != 0;

		drw_texture_get_format(fbotex.format, true, &gpu_format, &channels, &is_depth);

		if (!*fbotex.tex || is_temp) {
			/* Temp textures need to be queried each frame, others not. */
			if (is_temp) {
				*fbotex.tex = GPU_viewport_texture_pool_query(
				        DST.viewport, engine_type, width, height, channels, gpu_format);
			}
			else {
				*fbotex.tex = GPU_texture_create_2D_custom(
				        width, height, channels, gpu_format, NULL, NULL);
				create_tex = true;
			}
		}

		if (!is_depth) {
			++color_attachment;
		}

		if (create_fb || create_tex) {
			drw_texture_set_parameters(*fbotex.tex, fbotex.flag);
			GPU_framebuffer_texture_attach(*fb, *fbotex.tex, color_attachment, 0);
		}
	}

	if (create_fb && (textures_len > 0)) {
		if (!GPU_framebuffer_check_valid(*fb, NULL)) {
			printf("Error invalid framebuffer\n");
		}

		/* Detach temp textures */
		for (int i = 0; i < textures_len; ++i) {
			DRWFboTexture fbotex = textures[i];

			if ((fbotex.flag & DRW_TEX_TEMP) != 0) {
				GPU_framebuffer_texture_detach(*fbotex.tex);
			}
		}

		if (DST.default_framebuffer != NULL) {
			GPU_framebuffer_bind(DST.default_framebuffer);
		}
	}
}

void DRW_framebuffer_free(GPUFrameBuffer *fb)
{
	GPU_framebuffer_free(fb);
}

void DRW_framebuffer_bind(GPUFrameBuffer *fb)
{
	GPU_framebuffer_bind(fb);
}

void DRW_framebuffer_clear(bool color, bool depth, bool stencil, float clear_col[4], float clear_depth)
{
	if (color) {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(clear_col[0], clear_col[1], clear_col[2], clear_col[3]);
	}
	if (depth) {
		glDepthMask(GL_TRUE);
		glClearDepth(clear_depth);
	}
	if (stencil) {
		glStencilMask(0xFF);
	}
	glClear(((color) ? GL_COLOR_BUFFER_BIT : 0) |
	        ((depth) ? GL_DEPTH_BUFFER_BIT : 0) |
	        ((stencil) ? GL_STENCIL_BUFFER_BIT : 0));
}

void DRW_framebuffer_read_data(int x, int y, int w, int h, int channels, int slot, float *data)
{
	GLenum type;
	switch (channels) {
		case 1: type = GL_RED; break;
		case 2: type = GL_RG; break;
		case 3: type = GL_RGB; break;
		case 4: type = GL_RGBA;	break;
		default:
			BLI_assert(false && "wrong number of read channels");
			return;
	}
	glReadBuffer(GL_COLOR_ATTACHMENT0 + slot);
	glReadPixels(x, y, w, h, type, GL_FLOAT, data);
}

void DRW_framebuffer_read_depth(int x, int y, int w, int h, float *data)
{
	GLenum type = GL_DEPTH_COMPONENT;

	glReadBuffer(GL_COLOR_ATTACHMENT0); /* This is OK! */
	glReadPixels(x, y, w, h, type, GL_FLOAT, data);
}

void DRW_framebuffer_texture_attach(GPUFrameBuffer *fb, GPUTexture *tex, int slot, int mip)
{
	GPU_framebuffer_texture_attach(fb, tex, slot, mip);
}

void DRW_framebuffer_texture_layer_attach(GPUFrameBuffer *fb, GPUTexture *tex, int slot, int layer, int mip)
{
	GPU_framebuffer_texture_layer_attach(fb, tex, slot, layer, mip);
}

void DRW_framebuffer_cubeface_attach(GPUFrameBuffer *fb, GPUTexture *tex, int slot, int face, int mip)
{
	GPU_framebuffer_texture_cubeface_attach(fb, tex, slot, face, mip);
}

void DRW_framebuffer_texture_detach(GPUTexture *tex)
{
	GPU_framebuffer_texture_detach(tex);
}

void DRW_framebuffer_blit(GPUFrameBuffer *fb_read, GPUFrameBuffer *fb_write, bool depth, bool stencil)
{
	GPU_framebuffer_blit(fb_read, 0, fb_write, 0, depth, stencil);
}

void DRW_framebuffer_recursive_downsample(
        GPUFrameBuffer *fb, GPUTexture *tex, int num_iter,
        void (*callback)(void *userData, int level), void *userData)
{
	GPU_framebuffer_recursive_downsample(fb, tex, num_iter, callback, userData);
}

void DRW_framebuffer_viewport_size(GPUFrameBuffer *UNUSED(fb_read), int x, int y, int w, int h)
{
	glViewport(x, y, w, h);
}
