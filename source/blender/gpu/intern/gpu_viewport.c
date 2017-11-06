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
#include "BLI_mempool.h"

#include "DNA_vec_types.h"
#include "DNA_userdef_types.h"

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

/* Maximum number of simultaneous engine enabled at the same time.
 * Setting it lower than the real number will do lead to
 * higher VRAM usage due to sub-efficient buffer reuse. */
#define MAX_ENGINE_BUFFER_SHARING 5

typedef struct ViewportTempTexture {
	struct ViewportTempTexture *next, *prev;
	void *user[MAX_ENGINE_BUFFER_SHARING];
	GPUTexture *texture;
} ViewportTempTexture;

struct GPUViewport {
	float pad[4];

	/* debug */
	GPUTexture *debug_depth;
	int size[2];

	int samples;
	int flag;

	ListBase data;  /* ViewportEngineData wrapped in LinkData */
	unsigned int data_hash;  /* If hash mismatch we free all ViewportEngineData in this viewport */

	DefaultFramebufferList *fbl;
	DefaultTextureList *txl;

	ViewportMemoryPool vmempool; /* Used for rendering data structure. */

	ListBase tex_pool;  /* ViewportTempTexture list : Temporary textures shared across draw engines */
};

enum {
	DO_UPDATE = (1 << 0),
};

static void gpu_viewport_buffers_free(FramebufferList *fbl, int fbl_len, TextureList *txl, int txl_len);
static void gpu_viewport_storage_free(StorageList *stl, int stl_len);
static void gpu_viewport_passes_free(PassList *psl, int psl_len);
static void gpu_viewport_texture_pool_free(GPUViewport *viewport);

void GPU_viewport_tag_update(GPUViewport *viewport)
{
	viewport->flag |= DO_UPDATE;
}

bool GPU_viewport_do_update(GPUViewport *viewport)
{
	bool ret = (viewport->flag & DO_UPDATE);
	viewport->flag &= ~DO_UPDATE;
	return ret;
}

GPUViewport *GPU_viewport_create(void)
{
	GPUViewport *viewport = MEM_callocN(sizeof(GPUViewport), "GPUViewport");
	viewport->fbl = MEM_callocN(sizeof(DefaultFramebufferList), "FramebufferList");
	viewport->txl = MEM_callocN(sizeof(DefaultTextureList), "TextureList");

	viewport->size[0] = viewport->size[1] = -1;

	return viewport;
}

GPUViewport *GPU_viewport_create_from_offscreen(struct GPUOffScreen *ofs)
{
	GPUViewport *viewport = GPU_viewport_create();
	GPU_offscreen_viewport_data_get(ofs, &viewport->fbl->default_fb, &viewport->txl->color, &viewport->txl->depth);
	viewport->size[0] = GPU_offscreen_width(ofs);
	viewport->size[1] = GPU_offscreen_height(ofs);
	return viewport;
}
/**
 * Clear vars assigned from offscreen, so we don't free data owned by `GPUOffScreen`.
 */
void GPU_viewport_clear_from_offscreen(GPUViewport *viewport)
{
	viewport->fbl->default_fb = NULL;
	viewport->txl->color = NULL;
	viewport->txl->depth = NULL;
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

static void gpu_viewport_engines_data_free(GPUViewport *viewport)
{
	int fbl_len, txl_len, psl_len, stl_len;

	LinkData *next;
	for (LinkData *link = viewport->data.first; link; link = next) {
		next = link->next;
		ViewportEngineData *data = link->data;
		DRW_engine_viewport_data_size_get(data->engine_type, &fbl_len, &txl_len, &psl_len, &stl_len);

		gpu_viewport_buffers_free(data->fbl, fbl_len, data->txl, txl_len);
		gpu_viewport_passes_free(data->psl, psl_len);
		gpu_viewport_storage_free(data->stl, stl_len);

		MEM_freeN(data->fbl);
		MEM_freeN(data->txl);
		MEM_freeN(data->psl);
		MEM_freeN(data->stl);

		/* We could handle this in the DRW module */
		if (data->text_draw_cache) {
			extern void DRW_text_cache_destroy(struct DRWTextStore *dt);
			DRW_text_cache_destroy(data->text_draw_cache);
			data->text_draw_cache = NULL;
		}

		MEM_freeN(data);

		BLI_remlink(&viewport->data, link);
		MEM_freeN(link);
	}

	gpu_viewport_texture_pool_free(viewport);
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

ViewportMemoryPool *GPU_viewport_mempool_get(GPUViewport *viewport)
{
	return &viewport->vmempool;
}

void *GPU_viewport_framebuffer_list_get(GPUViewport *viewport)
{
	return viewport->fbl;
}

void *GPU_viewport_texture_list_get(GPUViewport *viewport)
{
	return viewport->txl;
}

void GPU_viewport_size_get(const GPUViewport *viewport, int size[2])
{
	size[0] = viewport->size[0];
	size[1] = viewport->size[1];
}

/**
 * Special case, this is needed for when we have a viewport without a frame-buffer output
 * (occlusion queries for eg) but still need to set the size since it may be used for other calculations.
 */
void GPU_viewport_size_set(GPUViewport *viewport, const int size[2])
{
	viewport->size[0] = size[0];
	viewport->size[1] = size[1];
}

/**
 * Try to find a texture coresponding to params into the texture pool.
 * If no texture was found, create one and add it to the pool.
 */
GPUTexture *GPU_viewport_texture_pool_query(GPUViewport *viewport, void *engine, int width, int height, int channels, int format)
{
	GPUTexture *tex;

	for (ViewportTempTexture *tmp_tex = viewport->tex_pool.first; tmp_tex; tmp_tex = tmp_tex->next) {
		if ((GPU_texture_width(tmp_tex->texture) == width) &&
		    (GPU_texture_height(tmp_tex->texture) == height) &&
		    (GPU_texture_format(tmp_tex->texture) == format))
		{
			/* Search if the engine is not already using this texture */
			for (int i = 0; i < MAX_ENGINE_BUFFER_SHARING; ++i) {
				if (tmp_tex->user[i] == engine) {
					break;
				}

				if (tmp_tex->user[i] == NULL) {
					tmp_tex->user[i] = engine;
					return tmp_tex->texture;
				}
			}
		}
	}

	tex = GPU_texture_create_2D_custom(width, height, channels, format, NULL, NULL);

	ViewportTempTexture *tmp_tex = MEM_callocN(sizeof(ViewportTempTexture), "ViewportTempTexture");
	tmp_tex->texture = tex;
	tmp_tex->user[0] = engine;

	BLI_addtail(&viewport->tex_pool, tmp_tex);

	return tex;
}

static void gpu_viewport_texture_pool_clear_users(GPUViewport *viewport)
{
	ViewportTempTexture *tmp_tex_next;

	for (ViewportTempTexture *tmp_tex = viewport->tex_pool.first; tmp_tex; tmp_tex = tmp_tex_next) {
		tmp_tex_next = tmp_tex->next;
		bool no_user = true;
		for (int i = 0; i < MAX_ENGINE_BUFFER_SHARING; ++i) {
			if (tmp_tex->user[i] != NULL) {
				tmp_tex->user[i] = NULL;
				no_user = false;
			}
		}

		if (no_user) {
			GPU_texture_free(tmp_tex->texture);
			BLI_freelinkN(&viewport->tex_pool, tmp_tex);
		}
	}
}

static void gpu_viewport_texture_pool_free(GPUViewport *viewport)
{
	for (ViewportTempTexture *tmp_tex = viewport->tex_pool.first; tmp_tex; tmp_tex = tmp_tex->next) {
		GPU_texture_free(tmp_tex->texture);
	}

	BLI_freelistN(&viewport->tex_pool);
}

bool GPU_viewport_engines_data_validate(GPUViewport *viewport, unsigned int hash)
{
	bool dirty = false;

	if (viewport->data_hash != hash) {
		gpu_viewport_engines_data_free(viewport);
		dirty = true;
	}

	viewport->data_hash = hash;

	return dirty;
}

void GPU_viewport_cache_release(GPUViewport *viewport)
{
	for (LinkData *link = viewport->data.first; link; link = link->next) {
		ViewportEngineData *data = link->data;
		int psl_len;
		DRW_engine_viewport_data_size_get(data->engine_type, NULL, NULL, &psl_len, NULL);
		gpu_viewport_passes_free(data->psl, psl_len);
	}
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
		if (rect_w != viewport->size[0] || rect_h != viewport->size[1] || U.ogl_multisamples != viewport->samples) {
			gpu_viewport_buffers_free(
			        (FramebufferList *)viewport->fbl, default_fbl_len,
			        (TextureList *)viewport->txl, default_txl_len);

			for (LinkData *link = viewport->data.first; link; link = link->next) {
				ViewportEngineData *data = link->data;
				DRW_engine_viewport_data_size_get(data->engine_type, &fbl_len, &txl_len, NULL, NULL);
				gpu_viewport_buffers_free(data->fbl, fbl_len, data->txl, txl_len);
			}

			gpu_viewport_texture_pool_free(viewport);
		}
	}

	gpu_viewport_texture_pool_clear_users(viewport);

	/* Multisample Buffer */
	if (U.ogl_multisamples > 0) {
		if (!dfbl->default_fb) {
			bool ok = true;
			viewport->samples = U.ogl_multisamples;

			dfbl->multisample_fb = GPU_framebuffer_create();
			if (!dfbl->multisample_fb) {
				ok = false;
				goto cleanup_multisample;
			}

			/* Color */
			dtxl->multisample_color = GPU_texture_create_2D_multisample(rect_w, rect_h, NULL, U.ogl_multisamples, NULL);
			if (!dtxl->multisample_color) {
				ok = false;
				goto cleanup_multisample;
			}

			if (!GPU_framebuffer_texture_attach(dfbl->multisample_fb, dtxl->multisample_color, 0, 0)) {
				ok = false;
				goto cleanup_multisample;
			}

			/* Depth */
			dtxl->multisample_depth = GPU_texture_create_depth_multisample(rect_w, rect_h, U.ogl_multisamples, NULL);

			if (!dtxl->multisample_depth) {
				ok = false;
				goto cleanup_multisample;
			}

			if (!GPU_framebuffer_texture_attach(dfbl->multisample_fb, dtxl->multisample_depth, 0, 0)) {
				ok = false;
				goto cleanup_multisample;
			}
			else if (!GPU_framebuffer_check_valid(dfbl->multisample_fb, NULL)) {
				ok = false;
				goto cleanup_multisample;
			}

cleanup_multisample:
			if (!ok) {
				GPU_viewport_free(viewport);
				MEM_freeN(viewport);
				return;
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
		dtxl->color = GPU_texture_create_2D(rect_w, rect_h, NULL, NULL);
		if (!dtxl->color) {
			ok = false;
			goto cleanup;
		}

		if (!GPU_framebuffer_texture_attach(dfbl->default_fb, dtxl->color, 0, 0)) {
			ok = false;
			goto cleanup;
		}

		/* Depth */
		dtxl->depth = GPU_texture_create_depth(rect_w, rect_h, NULL);

		if (dtxl->depth) {
			/* Define texture parameters */
			GPU_texture_bind(dtxl->depth, 0);
			GPU_texture_compare_mode(dtxl->depth, false);
			GPU_texture_filter_mode(dtxl->depth, true);
			GPU_texture_unbind(dtxl->depth);
		}
		else {
			ok = false;
			goto cleanup;
		}

		if (!GPU_framebuffer_texture_attach(dfbl->default_fb, dtxl->depth, 0, 0)) {
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

	Gwn_VertFormat *format = immVertexFormat();
	unsigned int texcoord = GWN_vertformat_attr_add(format, "texCoord", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_MODULATE_ALPHA);
	GPU_texture_bind(color, 0);

	immUniform1i("image", 0); /* default GL_TEXTURE0 unit */

	immBegin(GWN_PRIM_TRI_STRIP, 4);

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
		GPU_framebuffer_texture_unbind(NULL, NULL);
		GPU_framebuffer_restore();

		glEnable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);

		/* This might be bandwidth limiting */
		draw_ofs_to_screen(viewport);
	}
}

static void gpu_viewport_buffers_free(
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

static void gpu_viewport_storage_free(StorageList *stl, int stl_len)
{
	for (int i = 0; i < stl_len; i++) {
		void *storage = stl->storage[i];
		if (storage) {
			MEM_freeN(storage);
			stl->storage[i] = NULL;
		}
	}
}

static void gpu_viewport_passes_free(PassList *psl, int psl_len)
{
	for (int i = 0; i < psl_len; i++) {
		struct DRWPass *pass = psl->passes[i];
		if (pass) {
			DRW_pass_free(pass);
			psl->passes[i] = NULL;
		}
	}
}

void GPU_viewport_free(GPUViewport *viewport)
{
	gpu_viewport_engines_data_free(viewport);

	gpu_viewport_buffers_free(
	        (FramebufferList *)viewport->fbl, default_fbl_len,
	        (TextureList *)viewport->txl, default_txl_len);

	gpu_viewport_texture_pool_free(viewport);

	MEM_freeN(viewport->fbl);
	MEM_freeN(viewport->txl);

	if (viewport->vmempool.calls != NULL) {
		BLI_mempool_destroy(viewport->vmempool.calls);
	}
	if (viewport->vmempool.calls_generate != NULL) {
		BLI_mempool_destroy(viewport->vmempool.calls_generate);
	}
	if (viewport->vmempool.calls_dynamic != NULL) {
		BLI_mempool_destroy(viewport->vmempool.calls_dynamic);
	}
	if (viewport->vmempool.shgroups != NULL) {
		BLI_mempool_destroy(viewport->vmempool.shgroups);
	}
	if (viewport->vmempool.uniforms != NULL) {
		BLI_mempool_destroy(viewport->vmempool.uniforms);
	}
	if (viewport->vmempool.attribs != NULL) {
		BLI_mempool_destroy(viewport->vmempool.attribs);
	}
	if (viewport->vmempool.passes != NULL) {
		BLI_mempool_destroy(viewport->vmempool.passes);
	}

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

	Gwn_VertFormat *format = immVertexFormat();
	unsigned int texcoord = GWN_vertformat_attr_add(format, "texCoord", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_DEPTH);

	GPU_texture_bind(viewport->debug_depth, 0);

	immUniform1f("znear", znear);
	immUniform1f("zfar", zfar);
	immUniform1i("image", 0); /* default GL_TEXTURE0 unit */

	immBegin(GWN_PRIM_TRI_STRIP, 4);

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
