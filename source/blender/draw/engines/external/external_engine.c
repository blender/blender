/*
 * Copyright 2017, Blender Foundation.
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

/** \file external_engine.c
 *  \ingroup draw_engine
 *
 * Base engine for external render engines.
 * We use it for depth and non-mesh objects.
 */

#include "DRW_render.h"

#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_main.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "GPU_glew.h"
#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_viewport.h"

#include "external_engine.h"
/* Shaders */

#define EXTERNAL_ENGINE "BLENDER_EXTERNAL"

/* *********** LISTS *********** */

/* GPUViewport.storage
 * Is freed everytime the viewport engine changes */
typedef struct EXTERNAL_Storage {
	int dummy;
} EXTERNAL_Storage;

typedef struct EXTERNAL_StorageList {
	struct EXTERNAL_Storage *storage;
	struct EXTERNAL_PrivateData *g_data;
} EXTERNAL_StorageList;

typedef struct EXTERNAL_FramebufferList {
	struct GPUFrameBuffer *default_fb;
} EXTERNAL_FramebufferList;

typedef struct EXTERNAL_TextureList {
	/* default */
	struct GPUTexture *depth;
} EXTERNAL_TextureList;

typedef struct EXTERNAL_PassList {
	struct DRWPass *depth_pass;
} EXTERNAL_PassList;

typedef struct EXTERNAL_Data {
	void *engine_type;
	EXTERNAL_FramebufferList *fbl;
	EXTERNAL_TextureList *txl;
	EXTERNAL_PassList *psl;
	EXTERNAL_StorageList *stl;
	char info[GPU_INFO_SIZE];
} EXTERNAL_Data;

/* *********** STATIC *********** */

static struct {
	/* Depth Pre Pass */
	struct GPUShader *depth_sh;
} e_data = {NULL}; /* Engine data */

typedef struct EXTERNAL_PrivateData {
	DRWShadingGroup *depth_shgrp;
} EXTERNAL_PrivateData; /* Transient data */

/* Functions */

static void external_engine_init(void *UNUSED(vedata))
{
	/* Depth prepass */
	if (!e_data.depth_sh) {
		e_data.depth_sh = DRW_shader_create_3D_depth_only();
	}
}

static void external_cache_init(void *vedata)
{
	EXTERNAL_PassList *psl = ((EXTERNAL_Data *)vedata)->psl;
	EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	/* Depth Pass */
	{
		psl->depth_pass = DRW_pass_create("Depth Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);
		stl->g_data->depth_shgrp = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass);
	}
}

static void external_cache_populate(void *vedata, Object *ob)
{
	EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;

	if (!DRW_object_is_renderable(ob))
		return;

	struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
	if (geom) {
		/* Depth Prepass */
		DRW_shgroup_call_add(stl->g_data->depth_shgrp, geom, ob->obmat);
	}
}

static void external_cache_finish(void *UNUSED(vedata))
{
}

static void external_draw_scene_do(void *vedata)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene = draw_ctx->scene;
	RegionView3D *rv3d = draw_ctx->rv3d;
	ARegion *ar = draw_ctx->ar;
	RenderEngineType *type;

	DRW_state_reset_ex(DRW_STATE_DEFAULT & ~DRW_STATE_DEPTH_LESS_EQUAL);

	/* Create render engine. */
	if (!rv3d->render_engine) {
		RenderEngineType *engine_type = draw_ctx->engine_type;

		if (!(engine_type->view_update && engine_type->view_draw)) {
			return;
		}

		RenderEngine *engine = RE_engine_create_ex(engine_type, true);
		engine->tile_x = scene->r.tilex;
		engine->tile_y = scene->r.tiley;
		engine_type->view_update(engine, draw_ctx->evil_C);
		rv3d->render_engine = engine;
	}

	/* Rendered draw. */
	GPU_matrix_push_projection();
	ED_region_pixelspace(ar);

	/* Render result draw. */
	type = rv3d->render_engine->type;
	type->view_draw(rv3d->render_engine, draw_ctx->evil_C);

	GPU_matrix_pop_projection();

	/* Set render info. */
	EXTERNAL_Data *data = vedata;
	if (rv3d->render_engine->text[0] != '\0') {
		BLI_strncpy(data->info, rv3d->render_engine->text, sizeof(data->info));
	}
	else {
		data->info[0] = '\0';
	}
}

static void external_draw_scene(void *vedata)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	EXTERNAL_PassList *psl = ((EXTERNAL_Data *)vedata)->psl;

	/* Will be NULL during OpenGL render.
	 * OpenGL render is used for quick preview (thumbnails or sequencer preview)
	 * where using the rendering engine to preview doesn't make so much sense. */
	if (draw_ctx->evil_C) {
		external_draw_scene_do(vedata);
	}
	DRW_draw_pass(psl->depth_pass);
}

static void external_engine_free(void)
{
	/* All shaders are builtin. */
}

static const DrawEngineDataSize external_data_size = DRW_VIEWPORT_DATA_SIZE(EXTERNAL_Data);

DrawEngineType draw_engine_external_type = {
	NULL, NULL,
	N_("External"),
	&external_data_size,
	&external_engine_init,
	&external_engine_free,
	&external_cache_init,
	&external_cache_populate,
	&external_cache_finish,
	NULL,
	&external_draw_scene,
	NULL,
	NULL,
	NULL,
};

/* Note: currently unused, we should not register unless we want to see this when debugging the view. */

RenderEngineType DRW_engine_viewport_external_type = {
	NULL, NULL,
	EXTERNAL_ENGINE, N_("External"), RE_INTERNAL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	&draw_engine_external_type,
	{NULL, NULL, NULL}
};

#undef EXTERNAL_ENGINE
