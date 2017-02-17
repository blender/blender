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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_viewport.c
 *  \ingroup gpu
 *
 * System that manages viewport drawing.
 */

#include <string.h>

#include "BLI_rect.h"
#include "BLI_string.h"

#include "DNA_vec_types.h"

#include "GPU_framebuffer.h"
#include "GPU_glew.h"
#include "GPU_immediate.h"
#include "GPU_texture.h"
#include "GPU_viewport.h"

#include "DRW_engine.h"

#include "MEM_guardedalloc.h"

struct GPUViewport {
	float pad[4];

	/* debug */
	GPUTexture *debug_depth;
	int size[2];

	/* Viewport Buffer Storage */
	FramebufferList *fbl;
	TextureList *txl;
	PassList *psl;
	StorageList *stl;

	char engine_name[32];

	/* Mode storage */
	FramebufferList *fbl_mode;
	TextureList *txl_mode;
	PassList *psl_mode;
	StorageList *stl_mode;
	int mode;
};

static void GPU_viewport_buffers_free(FramebufferList *fbl, TextureList *txl);
static void GPU_viewport_storage_free(StorageList *stl);
static void GPU_viewport_passes_free(PassList *psl);

GPUViewport *GPU_viewport_create(void)
{
	GPUViewport *viewport = MEM_callocN(sizeof(GPUViewport), "GPUViewport");
	viewport->fbl = MEM_callocN(sizeof(FramebufferList), "FramebufferList");
	viewport->txl = MEM_callocN(sizeof(TextureList), "TextureList");
	viewport->psl = MEM_callocN(sizeof(PassList), "PassList");
	viewport->stl = MEM_callocN(sizeof(StorageList), "StorageList");
	viewport->fbl_mode = MEM_callocN(sizeof(FramebufferList), "FramebufferList");
	viewport->txl_mode = MEM_callocN(sizeof(TextureList), "TextureList");
	viewport->psl_mode = MEM_callocN(sizeof(PassList), "PassList");
	viewport->stl_mode = MEM_callocN(sizeof(StorageList), "StorageList");
	viewport->size[0] = viewport->size[1] = -1;

	return viewport;
}

void GPU_viewport_get_engine_data(GPUViewport *viewport, FramebufferList **fbs, TextureList **txs, PassList **pss, StorageList **str)
{
	*fbs = viewport->fbl;
	*txs = viewport->txl;
	*pss = viewport->psl;
	*str = viewport->stl;
}

void GPU_viewport_get_mode_data(GPUViewport *viewport, FramebufferList **fbs, TextureList **txs, PassList **pss, StorageList **str)
{
	*fbs = viewport->fbl_mode;
	*txs = viewport->txl_mode;
	*pss = viewport->psl_mode;
	*str = viewport->stl_mode;
}

void GPU_viewport_bind(GPUViewport *viewport, const rcti *rect, const char *engine, int mode)
{
	DefaultFramebufferList *dfbl = (DefaultFramebufferList *)viewport->fbl;
	DefaultTextureList *dtxl = (DefaultTextureList *)viewport->txl;

	/* add one pixel because of scissor test */
	int rect_w = BLI_rcti_size_x(rect) + 1, rect_h = BLI_rcti_size_y(rect) + 1;

#ifndef WITH_VIEWPORT_CACHE_TEST
	/* TODO for testing only, we need proper cache invalidation */
	GPU_viewport_passes_free(viewport->psl);
	GPU_viewport_passes_free(viewport->psl_mode);
#endif

	if (!STREQ(engine, viewport->engine_name)) {
		GPU_viewport_storage_free(viewport->stl);
		GPU_viewport_buffers_free(viewport->fbl, viewport->txl);

		BLI_strncpy(viewport->engine_name, engine, 32);
	}

	if (mode != viewport->mode) {
		GPU_viewport_buffers_free(viewport->fbl_mode, viewport->txl_mode);
		GPU_viewport_passes_free(viewport->psl_mode);
		GPU_viewport_storage_free(viewport->stl_mode);

		viewport->mode = mode;
	}

	if (dfbl->default_fb) {
		if (rect_w != viewport->size[0] || rect_h != viewport->size[1]) {
			GPU_viewport_buffers_free(viewport->fbl, viewport->txl);
			GPU_viewport_buffers_free(viewport->fbl_mode, viewport->txl_mode);
		}
	}

	if (!dfbl->default_fb) {
		bool ok = true;
		viewport->size[0] = rect_w;
		viewport->size[1] = rect_h;

		dfbl->default_fb = GPU_framebuffer_create();
		if (!dfbl->default_fb) {
			ok = false;
			goto cleanup;
		}

		/* Color */
		/* No multi samples for now */
		dtxl->color = GPU_texture_create_2D(rect_w, rect_h, NULL, NULL);
		if (!dtxl->color) {
			ok = false;
			goto cleanup;
		}

		if (!GPU_framebuffer_texture_attach(dfbl->default_fb, dtxl->color, 0)) {
			ok = false;
			goto cleanup;
		}

		/* Depth */
		dtxl->depth = GPU_texture_create_depth(rect_w, rect_h, NULL);
		if (!dtxl->depth) {
			ok = false;
			goto cleanup;
		}
		else if (!GPU_framebuffer_texture_attach(dfbl->default_fb, dtxl->depth, 0)) {
			ok = false;
			goto cleanup;
		}
		else if (!GPU_framebuffer_check_valid(dfbl->default_fb, NULL)) {
			ok = false;
			goto cleanup;
		}

cleanup:
		if (!ok) {
			GPU_viewport_free(viewport);
			MEM_freeN(viewport);
			return;
		}

		GPU_framebuffer_restore();
	}

	GPU_framebuffer_slots_bind(dfbl->default_fb, 0);
}

static void draw_ofs_to_screen(GPUViewport *viewport)
{
	DefaultTextureList *dtxl = (DefaultTextureList *)viewport->txl;

	GPUTexture *color = dtxl->color;

	const float w = (float)GPU_texture_width(color);
	const float h = (float)GPU_texture_height(color);

	VertexFormat *format = immVertexFormat();
	unsigned texcoord = add_attrib(format, "texCoord", GL_FLOAT, 2, KEEP_FLOAT);
	unsigned pos = add_attrib(format, "pos", GL_FLOAT, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_MODULATE_ALPHA);

	GPU_texture_bind(color, 0);

	immUniform1i("image", 0); /* default GL_TEXTURE0 unit */

	immBegin(GL_QUADS, 4);

	immAttrib2f(texcoord, 0.0f, 0.0f);
	immVertex2f(pos, 0.0f, 0.0f);

	immAttrib2f(texcoord, 1.0f, 0.0f);
	immVertex2f(pos, w, 0.0f);

	immAttrib2f(texcoord, 1.0f, 1.0f);
	immVertex2f(pos, w, h);

	immAttrib2f(texcoord, 0.0f, 1.0f);
	immVertex2f(pos, 0.0f, h);

	immEnd();

	GPU_texture_unbind(color);

	immUnbindProgram();
}

void GPU_viewport_unbind(GPUViewport *viewport)
{
	DefaultFramebufferList *dfbl = (DefaultFramebufferList *)viewport->fbl;

	if (dfbl->default_fb) {
		GPU_framebuffer_texture_unbind(NULL, NULL);
		GPU_framebuffer_restore();

		glEnable(GL_SCISSOR_TEST);

		/* This might be bandwidth limiting */
		draw_ofs_to_screen(viewport);
	}
}

static void GPU_viewport_buffers_free(FramebufferList *fbl, TextureList *txl)
{
	int i;
	for (i = MAX_BUFFERS - 1; i > -1; --i) {
		GPUFrameBuffer *fb = fbl->framebuffers[i];
		if (fb) {
			GPU_framebuffer_free(fb);
			fbl->framebuffers[i] = NULL;
		}
	}
	for (i = MAX_TEXTURES - 1; i > -1; --i) {
		GPUTexture *tex = txl->textures[i];
		if (tex) {
			GPU_texture_free(tex);
			txl->textures[i] = NULL;
		}
	}
}

static void GPU_viewport_storage_free(StorageList *stl)
{
	for (int i = MAX_STORAGE - 1; i > -1; --i) {
		void *storage = stl->storage[i];
		if (storage) {
			MEM_freeN(storage);
			stl->storage[i] = NULL;
		}
	}
}

static void GPU_viewport_passes_free(PassList *psl)
{
	for (int i = MAX_PASSES - 1; i > -1; --i) {
		struct DRWPass *pass = psl->passes[i];
		if (pass) {
			DRW_pass_free(pass);
			MEM_freeN(pass);
			psl->passes[i] = NULL;
		}
	}
}

void GPU_viewport_free(GPUViewport *viewport)
{
	GPU_viewport_debug_depth_free(viewport);

	GPU_viewport_buffers_free(viewport->fbl, viewport->txl);
	GPU_viewport_passes_free(viewport->psl);
	GPU_viewport_storage_free(viewport->stl);

	GPU_viewport_buffers_free(viewport->fbl_mode, viewport->txl_mode);
	GPU_viewport_passes_free(viewport->psl_mode);
	GPU_viewport_storage_free(viewport->stl_mode);

	MEM_freeN(viewport->fbl);
	MEM_freeN(viewport->txl);
	MEM_freeN(viewport->psl);
	MEM_freeN(viewport->stl);

	MEM_freeN(viewport->fbl_mode);
	MEM_freeN(viewport->txl_mode);
	MEM_freeN(viewport->psl_mode);
	MEM_freeN(viewport->stl_mode);
}

/****************** debug ********************/

bool GPU_viewport_debug_depth_create(GPUViewport *viewport, int width, int height, char err_out[256])
{
	viewport->debug_depth = GPU_texture_create_2D_custom(width, height, 4, GPU_RGBA16F, NULL, err_out);
	return (viewport->debug_depth != NULL);
}

void GPU_viewport_debug_depth_free(GPUViewport *viewport)
{
	if (viewport->debug_depth != NULL) {
		MEM_freeN(viewport->debug_depth);
		viewport->debug_depth = NULL;
	}
}

void GPU_viewport_debug_depth_store(GPUViewport *viewport, const int x, const int y)
{
	const int w = GPU_texture_width(viewport->debug_depth);
	const int h = GPU_texture_height(viewport->debug_depth);

	GPU_texture_bind(viewport->debug_depth, 0);
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, x, y, w, h, 0);
	GPU_texture_unbind(viewport->debug_depth);
}

void GPU_viewport_debug_depth_draw(GPUViewport *viewport, const float znear, const float zfar)
{
	const float w = (float)GPU_texture_width(viewport->debug_depth);
	const float h = (float)GPU_texture_height(viewport->debug_depth);

	VertexFormat *format = immVertexFormat();
	unsigned texcoord = add_attrib(format, "texCoord", GL_FLOAT, 2, KEEP_FLOAT);
	unsigned pos = add_attrib(format, "pos", GL_FLOAT, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_DEPTH);

	GPU_texture_bind(viewport->debug_depth, 0);

	immUniform1f("znear", znear);
	immUniform1f("zfar", zfar);
	immUniform1i("image", 0); /* default GL_TEXTURE0 unit */

	immBegin(GL_QUADS, 4);

	immAttrib2f(texcoord, 0.0f, 0.0f);
	immVertex2f(pos, 0.0f, 0.0f);

	immAttrib2f(texcoord, 1.0f, 0.0f);
	immVertex2f(pos, w, 0.0f);

	immAttrib2f(texcoord, 1.0f, 1.0f);
	immVertex2f(pos, w, h);

	immAttrib2f(texcoord, 0.0f, 1.0f);
	immVertex2f(pos, 0.0f, h);

	immEnd();

	GPU_texture_unbind(viewport->debug_depth);

	immUnbindProgram();
}

int GPU_viewport_debug_depth_width(const GPUViewport *viewport)
{
	return GPU_texture_width(viewport->debug_depth);
}

int GPU_viewport_debug_depth_height(const GPUViewport *viewport)
{
	return GPU_texture_height(viewport->debug_depth);
}

bool GPU_viewport_debug_depth_is_valid(GPUViewport *viewport)
{
	return viewport->debug_depth != NULL;
}
