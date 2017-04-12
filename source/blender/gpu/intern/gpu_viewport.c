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

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "DNA_vec_types.h"

#include "BKE_global.h"

#include "GPU_framebuffer.h"
#include "GPU_glew.h"
#include "GPU_immediate.h"
#include "GPU_texture.h"
#include "GPU_viewport.h"

#include "DRW_engine.h"

#include "MEM_guardedalloc.h"

static const int default_fbl_len = (sizeof(DefaultFramebufferList)) / sizeof(void *);
static const int default_txl_len = (sizeof(DefaultTextureList)) / sizeof(void *);

struct GPUViewport {
	float pad[4];

	/* debug */
	GPUTexture *debug_depth;
	int size[2];

	ListBase data;  /* ViewportEngineData wrapped in LinkData */
	unsigned int data_hash;  /* If hash mismatch we free all ViewportEngineData in this viewport */

	DefaultFramebufferList *fbl;
	DefaultTextureList *txl;
};

static void GPU_viewport_buffers_free(FramebufferList *fbl, int fbl_len, TextureList *txl, int txl_len);
static void GPU_viewport_storage_free(StorageList *stl, int stl_len);
static void GPU_viewport_passes_free(PassList *psl, int psl_len);

GPUViewport *GPU_viewport_create(void)
{
	GPUViewport *viewport = MEM_callocN(sizeof(GPUViewport), "GPUViewport");
	viewport->fbl = MEM_callocN(sizeof(DefaultFramebufferList), "FramebufferList");
	viewport->txl = MEM_callocN(sizeof(DefaultTextureList), "TextureList");

	viewport->size[0] = viewport->size[1] = -1;

	return viewport;
}

void *GPU_viewport_engine_data_create(GPUViewport *viewport, void *engine_type)
{
	LinkData *ld = MEM_callocN(sizeof(LinkData), "LinkData");
	ViewportEngineData *data = MEM_callocN(sizeof(ViewportEngineData), "ViewportEngineData");
	int fbl_len, txl_len, psl_len, stl_len;

	DRW_engine_viewport_data_size_get(engine_type, &fbl_len, &txl_len, &psl_len, &stl_len);

	data->engine_type = engine_type;

	data->fbl = MEM_callocN((sizeof(void *) * fbl_len) + sizeof(FramebufferList), "FramebufferList");
	data->txl = MEM_callocN((sizeof(void *) * txl_len) + sizeof(TextureList), "TextureList");
	data->psl = MEM_callocN((sizeof(void *) * psl_len) + sizeof(PassList), "PassList");
	data->stl = MEM_callocN((sizeof(void *) * stl_len) + sizeof(StorageList), "StorageList");

	ld->data = data;
	BLI_addtail(&viewport->data, ld);

	return data;
}

static void GPU_viewport_engines_data_free(GPUViewport *viewport)
{
	int fbl_len, txl_len, psl_len, stl_len;

	LinkData *next;
	for (LinkData *link = viewport->data.first; link; link = next) {
		next = link->next;
		ViewportEngineData *data = link->data;
		DRW_engine_viewport_data_size_get(data->engine_type, &fbl_len, &txl_len, &psl_len, &stl_len);

		GPU_viewport_buffers_free(data->fbl, fbl_len, data->txl, txl_len);
		GPU_viewport_passes_free(data->psl, psl_len);
		GPU_viewport_storage_free(data->stl, stl_len);

		MEM_freeN(data->fbl);
		MEM_freeN(data->txl);
		MEM_freeN(data->psl);
		MEM_freeN(data->stl);

		MEM_freeN(data);

		BLI_remlink(&viewport->data, link);
		MEM_freeN(link);
	}
}

void *GPU_viewport_engine_data_get(GPUViewport *viewport, void *engine_type)
{
	for (LinkData *link = viewport->data.first; link; link = link->next) {
		ViewportEngineData *vdata = link->data;
		if (vdata->engine_type == engine_type) {
			return vdata;
		}
	}
	return NULL;
}

void *GPU_viewport_framebuffer_list_get(GPUViewport *viewport)
{
	return viewport->fbl;
}

void *GPU_viewport_texture_list_get(GPUViewport *viewport)
{
	return viewport->txl;
}

void GPU_viewport_size_get(GPUViewport *viewport, int *size)
{
	size[0] = viewport->size[0];
	size[1] = viewport->size[1];
}

bool GPU_viewport_cache_validate(GPUViewport *viewport, unsigned int hash)
{
	bool dirty = false;

	/* TODO for testing only, we need proper cache invalidation */
	if (G.debug_value != 666 && G.debug_value != 667) {
		for (LinkData *link = viewport->data.first; link; link = link->next) {
			ViewportEngineData *data = link->data;
			int psl_len;
			DRW_engine_viewport_data_size_get(data->engine_type, NULL, NULL, &psl_len, NULL);
			GPU_viewport_passes_free(data->psl, psl_len);
		}
		dirty = true;
	}

	if (viewport->data_hash != hash) {
		GPU_viewport_engines_data_free(viewport);
		dirty = true;
	}

	viewport->data_hash = hash;

	return dirty;
}

void GPU_viewport_bind(GPUViewport *viewport, const rcti *rect)
{
	DefaultFramebufferList *dfbl = viewport->fbl;
	DefaultTextureList *dtxl = viewport->txl;
	int fbl_len, txl_len;

	/* add one pixel because of scissor test */
	int rect_w = BLI_rcti_size_x(rect) + 1;
	int rect_h = BLI_rcti_size_y(rect) + 1;

	if (dfbl->default_fb) {
		if (rect_w != viewport->size[0] || rect_h != viewport->size[1]) {
			GPU_viewport_buffers_free(
			        (FramebufferList *)viewport->fbl, default_fbl_len,
			        (TextureList *)viewport->txl, default_txl_len);

			for (LinkData *link = viewport->data.first; link; link = link->next) {
				ViewportEngineData *data = link->data;
				DRW_engine_viewport_data_size_get(data->engine_type, &fbl_len, &txl_len, NULL, NULL);
				GPU_viewport_buffers_free(data->fbl, fbl_len, data->txl, txl_len);
			}
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
	DefaultTextureList *dtxl = viewport->txl;

	GPUTexture *color = dtxl->color;

	const float w = (float)GPU_texture_width(color);
	const float h = (float)GPU_texture_height(color);

	VertexFormat *format = immVertexFormat();
	unsigned int texcoord = VertexFormat_add_attrib(format, "texCoord", COMP_F32, 2, KEEP_FLOAT);
	unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_MODULATE_ALPHA);
	GPU_texture_bind(color, 0);

	immUniform1i("image", 0); /* default GL_TEXTURE0 unit */

	immBegin(PRIM_TRIANGLE_STRIP, 4);

	immAttrib2f(texcoord, 0.0f, 0.0f);
	immVertex2f(pos, 0.0f, 0.0f);

	immAttrib2f(texcoord, 1.0f, 0.0f);
	immVertex2f(pos, w, 0.0f);

	immAttrib2f(texcoord, 0.0f, 1.0f);
	immVertex2f(pos, 0.0f, h);

	immAttrib2f(texcoord, 1.0f, 1.0f);
	immVertex2f(pos, w, h);

	immEnd();

	GPU_texture_unbind(color);

	immUnbindProgram();
}

void GPU_viewport_unbind(GPUViewport *viewport)
{
	DefaultFramebufferList *dfbl = viewport->fbl;

	if (dfbl->default_fb) {
		GPU_framebuffer_texture_unbind(dfbl->default_fb, NULL);
		GPU_framebuffer_restore();

		glEnable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);

		/* This might be bandwidth limiting */
		draw_ofs_to_screen(viewport);
	}
}

static void GPU_viewport_buffers_free(
        FramebufferList *fbl, int fbl_len,
        TextureList *txl, int txl_len)
{
	for (int i = 0; i < fbl_len; i++) {
		GPUFrameBuffer *fb = fbl->framebuffers[i];
		if (fb) {
			GPU_framebuffer_free(fb);
			fbl->framebuffers[i] = NULL;
		}
	}
	for (int i = 0; i < txl_len; i++) {
		GPUTexture *tex = txl->textures[i];
		if (tex) {
			GPU_texture_free(tex);
			txl->textures[i] = NULL;
		}
	}
}

static void GPU_viewport_storage_free(StorageList *stl, int stl_len)
{
	for (int i = 0; i < stl_len; i++) {
		void *storage = stl->storage[i];
		if (storage) {
			MEM_freeN(storage);
			stl->storage[i] = NULL;
		}
	}
}

static void GPU_viewport_passes_free(PassList *psl, int psl_len)
{
	for (int i = 0; i < psl_len; i++) {
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
	GPU_viewport_engines_data_free(viewport);

	GPU_viewport_buffers_free(
	        (FramebufferList *)viewport->fbl, default_fbl_len,
	        (TextureList *)viewport->txl, default_txl_len);

	MEM_freeN(viewport->fbl);
	MEM_freeN(viewport->txl);

	GPU_viewport_debug_depth_free(viewport);
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
	unsigned int texcoord = VertexFormat_add_attrib(format, "texCoord", COMP_F32, 2, KEEP_FLOAT);
	unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_DEPTH);

	GPU_texture_bind(viewport->debug_depth, 0);

	immUniform1f("znear", znear);
	immUniform1f("zfar", zfar);
	immUniform1i("image", 0); /* default GL_TEXTURE0 unit */

	immBegin(PRIM_TRIANGLE_STRIP, 4);

	immAttrib2f(texcoord, 0.0f, 0.0f);
	immVertex2f(pos, 0.0f, 0.0f);

	immAttrib2f(texcoord, 1.0f, 0.0f);
	immVertex2f(pos, w, 0.0f);

	immAttrib2f(texcoord, 0.0f, 1.0f);
	immVertex2f(pos, 0.0f, h);

	immAttrib2f(texcoord, 1.0f, 1.0f);
	immVertex2f(pos, w, h);

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
