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

#include "eevee.h"
#include "eevee_private.h"

#define EEVEE_ENGINE "BLENDER_EEVEE"

/* *********** STATIC *********** */
static struct {
	struct GPUShader *default_lit;
	struct GPUShader *tonemap;
} e_data = {NULL}; /* Engine data */

static struct {
	DRWShadingGroup *default_lit_grp;
	EEVEE_Data *vedata;
} g_data = {NULL}; /* Transient data */

extern char datatoc_lit_surface_frag_glsl[];
extern char datatoc_lit_surface_vert_glsl[];
extern char datatoc_tonemap_frag_glsl[];

/* *********** FUNCTIONS *********** */

static void EEVEE_engine_init(void)
{
	g_data.vedata = DRW_viewport_engine_data_get(EEVEE_ENGINE);
	EEVEE_TextureList *txl = g_data.vedata->txl;
	EEVEE_FramebufferList *fbl = g_data.vedata->fbl;
	EEVEE_StorageList *stl = g_data.vedata->stl;

	DRWFboTexture tex = {&txl->color, DRW_BUF_RGBA_32};

	float *viewport_size = DRW_viewport_size_get();
	DRW_framebuffer_init(&fbl->main,
	                    (int)viewport_size[0], (int)viewport_size[1],
	                    &tex, 1);

	if (!e_data.default_lit) {
		e_data.default_lit = DRW_shader_create(datatoc_lit_surface_vert_glsl, NULL, datatoc_lit_surface_frag_glsl, "#define MAX_LIGHT 128\n");
	}

	if (!e_data.tonemap) {
		e_data.tonemap = DRW_shader_create_fullscreen(datatoc_tonemap_frag_glsl, NULL);
	}
	UNUSED_VARS(stl);

	if (stl->lights_info == NULL)
		EEVEE_lights_init(stl);

	// EEVEE_lights_update(stl);
}

static void EEVEE_cache_init(void)
{
	g_data.vedata = DRW_viewport_engine_data_get(EEVEE_ENGINE);
	EEVEE_PassList *psl = g_data.vedata->psl;
	EEVEE_TextureList *txl = g_data.vedata->txl;
	EEVEE_StorageList *stl = g_data.vedata->stl;

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
		psl->pass = DRW_pass_create("Default Light Pass", state);

		g_data.default_lit_grp = DRW_shgroup_create(e_data.default_lit, psl->pass);
		DRW_shgroup_uniform_block(g_data.default_lit_grp, "light_block", stl->lights_ubo, 0);
		DRW_shgroup_uniform_int(g_data.default_lit_grp, "light_count", &stl->lights_info->light_count, 1);
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

static void EEVEE_cache_populate(Object *ob)
{
	EEVEE_StorageList *stl = g_data.vedata->stl;

	if (ob->type == OB_MESH) {
		struct Batch *geom = DRW_cache_surface_get(ob);

		DRW_shgroup_call_add(g_data.default_lit_grp, geom, ob->obmat);
	}
	else if (ob->type == OB_LAMP) {
		EEVEE_lights_cache_add(stl, ob);
	}
}

static void EEVEE_cache_finish(void)
{
	EEVEE_StorageList *stl = g_data.vedata->stl;

	EEVEE_lights_cache_finish(stl);
}

static void EEVEE_draw_scene(void)
{
	EEVEE_Data *ved = DRW_viewport_engine_data_get(EEVEE_ENGINE);
	EEVEE_PassList *psl = ved->psl;
	EEVEE_FramebufferList *fbl = ved->fbl;

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
	DRW_framebuffer_clear(true, true, clearcol, 1.0f);

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
	N_("Clay"),
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
