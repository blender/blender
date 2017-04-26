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

#include "DRW_render.h"

#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_main.h"

#include "GPU_shader.h"

#include "select_engine.h"
/* Shaders */

#define SELECT_ENGINE "BLENDER_SELECT"

/* we may want this later? */
#define USE_DEPTH

/* *********** LISTS *********** */

/* GPUViewport.storage
 * Is freed everytime the viewport engine changes */
typedef struct SELECT_Storage {
	int dummy;
} SELECT_Storage;

typedef struct SELECT_StorageList {
	struct SELECT_Storage *storage;
	struct g_data *g_data;
} SELECT_StorageList;

typedef struct SELECT_FramebufferList {
	/* default */
	struct GPUFrameBuffer *default_fb;
	/* engine specific */
#ifdef USE_DEPTH
	struct GPUFrameBuffer *dupli_depth;
#endif
} SELECT_FramebufferList;

typedef struct SELECT_TextureList {
	/* default */
	struct GPUTexture *color;
#ifdef USE_DEPTH
	struct GPUTexture *depth;
	/* engine specific */
	struct GPUTexture *depth_dup;
#endif
} SELECT_TextureList;

typedef struct SELECT_PassList {
#ifdef USE_DEPTH
	struct DRWPass *depth_pass;
#endif
	struct DRWPass *color_pass;
	struct g_data *g_data;
} SELECT_PassList;

typedef struct SELECT_Data {
	void *engine_type;
	SELECT_FramebufferList *fbl;
	SELECT_TextureList *txl;
	SELECT_PassList *psl;
	SELECT_StorageList *stl;
} SELECT_Data;

/* *********** STATIC *********** */

static struct {
#ifdef USE_DEPTH
	/* Depth Pre Pass */
	struct GPUShader *depth_sh;
#endif
	/* Shading Pass */
	struct GPUShader *color_sh;
} e_data = {NULL}; /* Engine data */

typedef struct g_data {
	DRWShadingGroup *depth_shgrp;
	DRWShadingGroup *depth_shgrp_select;
	DRWShadingGroup *depth_shgrp_active;
	DRWShadingGroup *depth_shgrp_cull;
	DRWShadingGroup *depth_shgrp_cull_select;
	DRWShadingGroup *depth_shgrp_cull_active;
} g_data; /* Transient data */

/* Functions */

static void SELECT_engine_init(void *vedata)
{
	SELECT_StorageList *stl = ((SELECT_Data *)vedata)->stl;
	SELECT_TextureList *txl = ((SELECT_Data *)vedata)->txl;
	SELECT_FramebufferList *fbl = ((SELECT_Data *)vedata)->fbl;

	/* Depth prepass */
	if (!e_data.depth_sh) {
		e_data.depth_sh = DRW_shader_create_3D_depth_only();
	}

	/* Shading pass */
	if (!e_data.color_sh) {
		e_data.color_sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	}

	if (!stl->storage) {
		stl->storage = MEM_callocN(sizeof(SELECT_Storage), "SELECT_Storage");
	}

	if (DRW_viewport_is_fbo()) {
		const float *viewport_size = DRW_viewport_size_get();
		DRWFboTexture tex = {&txl->depth_dup, DRW_BUF_DEPTH_24, 0};
		DRW_framebuffer_init(&fbl->dupli_depth,
		                     (int)viewport_size[0], (int)viewport_size[1],
		                     &tex, 1);
	}
}

static void SELECT_cache_init(void *vedata)
{
	SELECT_PassList *psl = ((SELECT_Data *)vedata)->psl;
	SELECT_StorageList *stl = ((SELECT_Data *)vedata)->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(g_data), "g_data");
	}

#ifdef USE_DEPTH
	/* Depth Pass */
	{
		psl->depth_pass = DRW_pass_create("Depth Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
		stl->g_data->depth_shgrp = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass);
	}
#endif

	/* Color Pass */
	{
		psl->color_pass = DRW_pass_create("Color Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);
	}
}

static void SELECT_cache_populate(void *vedata, Object *ob)
{
	SELECT_PassList *psl = ((SELECT_Data *)vedata)->psl;
	SELECT_StorageList *stl = ((SELECT_Data *)vedata)->stl;

	if (!DRW_is_object_renderable(ob))
		return;

	struct Batch *geom = DRW_cache_object_surface_get(ob);
	if (geom) {
		IDProperty *ces_mode_ob = BKE_object_collection_engine_get(ob, COLLECTION_MODE_OBJECT, "");
		bool do_cull = BKE_collection_engine_property_value_get_bool(ces_mode_ob, "show_backface_culling");

		/* Depth Prepass */
		DRW_shgroup_call_add((do_cull) ? stl->g_data->depth_shgrp_cull : stl->g_data->depth_shgrp, geom, ob->obmat);

		/* Shading */
		DRWShadingGroup *color_shgrp = DRW_shgroup_create(e_data.color_sh, psl->color_pass);

		DRW_shgroup_call_add(color_shgrp, geom, ob->obmat);
	}
}

static void SELECT_cache_finish(void *vedata)
{
	SELECT_StorageList *stl = ((SELECT_Data *)vedata)->stl;

	UNUSED_VARS(stl);
}

static void SELECT_draw_scene(void *vedata)
{

	SELECT_PassList *psl = ((SELECT_Data *)vedata)->psl;
	SELECT_FramebufferList *fbl = ((SELECT_Data *)vedata)->fbl;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

#ifdef USE_DEPTH
	/* Pass 1 : Depth pre-pass */
	DRW_draw_pass(psl->depth_pass);

	/* Pass 2 : Duplicate depth */
	/* Unless we go for deferred shading we need this to avoid manual depth test and artifacts */
	if (DRW_viewport_is_fbo()) {
		DRW_framebuffer_blit(dfbl->default_fb, fbl->dupli_depth, true);
	}
#endif

	/* Pass 3 : Shading */
	DRW_draw_pass(psl->color_pass);
}

static void SELECT_engine_free(void)
{
	/* all shaders are builtin */
}

static const DrawEngineDataSize SELECT_data_size = DRW_VIEWPORT_DATA_SIZE(SELECT_Data);

DrawEngineType draw_engine_select_type = {
	NULL, NULL,
	N_("SelectID"),
	&SELECT_data_size,
	&SELECT_engine_init,
	&SELECT_engine_free,
	&SELECT_cache_init,
	&SELECT_cache_populate,
	&SELECT_cache_finish,
	NULL,
	&SELECT_draw_scene
};

RenderEngineType viewport_select_type = {
	NULL, NULL,
	SELECT_ENGINE, N_("SelectID"), RE_INTERNAL | RE_USE_OGL_PIPELINE,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	&draw_engine_select_type,
	{NULL, NULL, NULL}
};


#undef SELECT_ENGINE
