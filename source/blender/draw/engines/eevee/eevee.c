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

/** \file eevee.c
 *  \ingroup DNA
 */

#include "DRW_render.h"

#include "BLI_dynstr.h"

#include "eevee.h"
#include "eevee_private.h"

#define EEVEE_ENGINE "BLENDER_EEVEE"

/* *********** STATIC *********** */
static struct {
	struct GPUShader *default_lit;
	struct GPUShader *depth_sh;
	struct GPUShader *tonemap;
} e_data = {NULL}; /* Engine data */

extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_direct_lib_glsl[];
extern char datatoc_lit_surface_frag_glsl[];
extern char datatoc_lit_surface_vert_glsl[];
extern char datatoc_tonemap_frag_glsl[];

/* *********** FUNCTIONS *********** */

static void EEVEE_engine_init(void *vedata)
{
	EEVEE_TextureList *txl = ((EEVEE_Data *)vedata)->txl;
	EEVEE_FramebufferList *fbl = ((EEVEE_Data *)vedata)->fbl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

	DRWFboTexture tex = {&txl->color, DRW_BUF_RGBA_16};

	float *viewport_size = DRW_viewport_size_get();
	DRW_framebuffer_init(&fbl->main,
	                    (int)viewport_size[0], (int)viewport_size[1],
	                    &tex, 1);

	if (!e_data.default_lit) {
		e_data.depth_sh = DRW_shader_create_3D_depth_only();
	}

	if (!e_data.default_lit) {
		char *lib_str = NULL;

		DynStr *ds_vert = BLI_dynstr_new();
		BLI_dynstr_append(ds_vert, datatoc_bsdf_common_lib_glsl);
		BLI_dynstr_append(ds_vert, datatoc_bsdf_direct_lib_glsl);
		lib_str = BLI_dynstr_get_cstring(ds_vert);
		BLI_dynstr_free(ds_vert);

		e_data.default_lit = DRW_shader_create_with_lib(datatoc_lit_surface_vert_glsl, NULL, datatoc_lit_surface_frag_glsl, lib_str, "#define MAX_LIGHT 128\n");

		MEM_freeN(lib_str);
	}

	if (!e_data.tonemap) {
		e_data.tonemap = DRW_shader_create_fullscreen(datatoc_tonemap_frag_glsl, NULL);
	}

	if (stl->lights_info == NULL)
		EEVEE_lights_init(stl);

	// EEVEE_lights_update(stl);
}

static void EEVEE_cache_init(void *vedata)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_TextureList *txl = ((EEVEE_Data *)vedata)->txl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(g_data), "g_data");
	}

	{
		psl->depth_pass = DRW_pass_create("Depth Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
		stl->g_data->depth_shgrp = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass);

		stl->g_data->depth_shgrp_select = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass);
		DRW_shgroup_state_set(stl->g_data->depth_shgrp_select, DRW_STATE_WRITE_STENCIL_SELECT);

		stl->g_data->depth_shgrp_active = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass);
		DRW_shgroup_state_set(stl->g_data->depth_shgrp_active, DRW_STATE_WRITE_STENCIL_ACTIVE);

		psl->depth_pass_cull = DRW_pass_create("Depth Pass Cull", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_CULL_BACK);
		stl->g_data->depth_shgrp_cull = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass_cull);

		stl->g_data->depth_shgrp_cull_select = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass_cull);
		DRW_shgroup_state_set(stl->g_data->depth_shgrp_cull_select, DRW_STATE_WRITE_STENCIL_SELECT);

		stl->g_data->depth_shgrp_cull_active = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass_cull);
		DRW_shgroup_state_set(stl->g_data->depth_shgrp_cull_active, DRW_STATE_WRITE_STENCIL_ACTIVE);
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_EQUAL;
		psl->pass = DRW_pass_create("Default Light Pass", state);

		stl->g_data->default_lit_grp = DRW_shgroup_create(e_data.default_lit, psl->pass);
		DRW_shgroup_uniform_block(stl->g_data->default_lit_grp, "light_block", stl->lights_ubo, 0);
		DRW_shgroup_uniform_int(stl->g_data->default_lit_grp, "light_count", &stl->lights_info->light_count, 1);
	}

	{
		/* Final pass : Map HDR color to LDR color.
		 * Write result to the default color buffer */
		psl->tonemap = DRW_pass_create("Tone Mapping", DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.tonemap, psl->tonemap);
		DRW_shgroup_uniform_buffer(grp, "hdrColorBuf", &txl->color, 0);

		struct Batch *geom = DRW_cache_fullscreen_quad_get();
		DRW_shgroup_call_add(grp, geom, NULL);
	}

	EEVEE_lights_cache_init(stl);
}

static void EEVEE_cache_populate(void *vedata, Object *ob)
{
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

	if (ob->type == OB_MESH) {
		CollectionEngineSettings *ces_mode_ob = BKE_object_collection_engine_get(ob, COLLECTION_MODE_OBJECT, "");
		bool do_cull = BKE_collection_engine_property_value_get_bool(ces_mode_ob, "show_backface_culling");
		struct Batch *geom = DRW_cache_surface_get(ob);

		/* Depth Prepass */
		/* waiting for proper flag */
		// if ((ob->base_flag & BASE_ACTIVE) != 0)
			// DRW_shgroup_call_add((do_cull) ? depth_shgrp_cull_active : depth_shgrp_active, geom, ob->obmat);
		if ((ob->base_flag & BASE_SELECTED) != 0)
			DRW_shgroup_call_add((do_cull) ? stl->g_data->depth_shgrp_cull_select : stl->g_data->depth_shgrp_select, geom, ob->obmat);
		else
			DRW_shgroup_call_add((do_cull) ? stl->g_data->depth_shgrp_cull : stl->g_data->depth_shgrp, geom, ob->obmat);

		DRW_shgroup_call_add(stl->g_data->default_lit_grp, geom, ob->obmat);
	}
	else if (ob->type == OB_LAMP) {
		EEVEE_lights_cache_add(stl, ob);
	}
}

static void EEVEE_cache_finish(void *vedata)
{
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

	EEVEE_lights_cache_finish(stl);
}

static void EEVEE_draw_scene(void *vedata)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_FramebufferList *fbl = ((EEVEE_Data *)vedata)->fbl;

	/* Default framebuffer and texture */
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	/* Attach depth to the hdr buffer and bind it */	
	DRW_framebuffer_texture_detach(dtxl->depth);
	DRW_framebuffer_texture_attach(fbl->main, dtxl->depth, 0);
	DRW_framebuffer_bind(fbl->main);

	/* Clear Depth */
	/* TODO do background */
	float clearcol[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	DRW_framebuffer_clear(true, true, true, clearcol, 1.0f);

	DRW_draw_pass(psl->depth_pass);
	DRW_draw_pass(psl->depth_pass_cull);
	DRW_draw_pass(psl->pass);

	/* Restore default framebuffer */
	DRW_framebuffer_texture_detach(dtxl->depth);
	DRW_framebuffer_texture_attach(dfbl->default_fb, dtxl->depth, 0);
	DRW_framebuffer_bind(dfbl->default_fb);

	DRW_draw_pass(psl->tonemap);
}

static void EEVEE_engine_free(void)
{
	if (e_data.default_lit)
		DRW_shader_free(e_data.default_lit);
	if (e_data.tonemap)
		DRW_shader_free(e_data.tonemap);
}

static void EEVEE_collection_settings_create(RenderEngine *UNUSED(engine), CollectionEngineSettings *ces)
{
	BLI_assert(ces);
	// BKE_collection_engine_property_add_int(ces, "high_quality_sphere_lamps", false);
}

DrawEngineType draw_engine_eevee_type = {
	NULL, NULL,
	N_("Eevee"),
	&EEVEE_engine_init,
	&EEVEE_engine_free,
	&EEVEE_cache_init,
	&EEVEE_cache_populate,
	&EEVEE_cache_finish,
	&EEVEE_draw_scene,
	NULL//&EEVEE_draw_scene
};

RenderEngineType viewport_eevee_type = {
	NULL, NULL,
	EEVEE_ENGINE, N_("Eevee"), RE_INTERNAL | RE_USE_OGL_PIPELINE,
	NULL, NULL, NULL, NULL, NULL, NULL, &EEVEE_collection_settings_create,
	&draw_engine_eevee_type,
	{NULL, NULL, NULL}
};


#undef EEVEE_ENGINE
