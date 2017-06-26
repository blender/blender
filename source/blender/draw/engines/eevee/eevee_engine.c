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

/** \file eevee_engine.c
 *  \ingroup draw_engine
 */

#include "DNA_world_types.h"
#include "DRW_render.h"

#include "BLI_dynstr.h"
#include "BLI_rand.h"

#include "GPU_material.h"
#include "GPU_glew.h"

#include "eevee_engine.h"
#include "eevee_private.h"

#define EEVEE_ENGINE "BLENDER_EEVEE"

extern GlobalsUboStorage ts;

/* *********** FUNCTIONS *********** */

static void EEVEE_engine_init(void *ved)
{
	EEVEE_Data *vedata = (EEVEE_Data *)ved;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
	EEVEE_SceneLayerData *sldata = EEVEE_scene_layer_data_get();

	DRWFboTexture tex = {&txl->color, DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER};

	const float *viewport_size = DRW_viewport_size_get();
	DRW_framebuffer_init(&fbl->main, &draw_engine_eevee_type,
	                    (int)viewport_size[0], (int)viewport_size[1],
	                    &tex, 1);

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}
	stl->g_data->background_alpha = 1.0f;

	EEVEE_materials_init();
	EEVEE_lights_init(sldata);
	EEVEE_lightprobes_init(sldata, vedata);
	EEVEE_effects_init(vedata);
}

static void EEVEE_cache_init(void *vedata)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_SceneLayerData *sldata = EEVEE_scene_layer_data_get();

	EEVEE_materials_cache_init(vedata);
	EEVEE_lights_cache_init(sldata, psl);
	EEVEE_lightprobes_cache_init(sldata, vedata);
	EEVEE_effects_cache_init(vedata);
}

static void EEVEE_cache_populate(void *vedata, Object *ob)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_SceneLayerData *sldata = EEVEE_scene_layer_data_get();

	const DRWContextState *draw_ctx = DRW_context_state_get();
	const bool is_active = (ob == draw_ctx->obact);
	if (is_active) {
		if (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)) {
			return;
		}
	}

	struct Gwn_Batch *geom = DRW_cache_object_surface_get(ob);
	if (geom) {
		EEVEE_materials_cache_populate(vedata, sldata, ob, geom);

		const bool cast_shadow = true;

		if (cast_shadow) {
			EEVEE_lights_cache_shcaster_add(sldata, psl, geom, ob->obmat);
			BLI_addtail(&sldata->shadow_casters, BLI_genericNodeN(ob));
			EEVEE_ObjectEngineData *oedata = EEVEE_object_data_get(ob);
			oedata->need_update = ((ob->deg_update_flag & DEG_RUNTIME_DATA_UPDATE) != 0);
		}
	}
	else if (ob->type == OB_LIGHTPROBE) {
		EEVEE_lightprobes_cache_add(sldata, ob);
	}
	else if (ob->type == OB_LAMP) {
		EEVEE_lights_cache_add(sldata, ob);
	}
}

static void EEVEE_cache_finish(void *vedata)
{
	EEVEE_SceneLayerData *sldata = EEVEE_scene_layer_data_get();

	EEVEE_materials_cache_finish(vedata);
	EEVEE_lights_cache_finish(sldata);
	EEVEE_lightprobes_cache_finish(sldata, vedata);
}

static void EEVEE_draw_scene(void *vedata)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_FramebufferList *fbl = ((EEVEE_Data *)vedata)->fbl;
	EEVEE_SceneLayerData *sldata = EEVEE_scene_layer_data_get();

	/* Default framebuffer and texture */
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	/* Refresh shadows */
	EEVEE_draw_shadows(sldata, psl);

	/* Refresh Probes */
	EEVEE_lightprobes_refresh(sldata, vedata);

	/* Attach depth to the hdr buffer and bind it */	
	DRW_framebuffer_texture_detach(dtxl->depth);
	DRW_framebuffer_texture_attach(fbl->main, dtxl->depth, 0, 0);
	DRW_framebuffer_bind(fbl->main);
	DRW_framebuffer_clear(false, true, false, NULL, 1.0f);

	DRW_draw_pass(psl->background_pass);

	/* Depth prepass */
	DRW_draw_pass(psl->depth_pass);
	DRW_draw_pass(psl->depth_pass_cull);

	/* Create minmax texture */
	EEVEE_create_minmax_buffer(vedata, dtxl->depth);

	/* Restore main FB */
	DRW_framebuffer_bind(fbl->main);

	/* Shading pass */
	DRW_draw_pass(psl->probe_display);
	EEVEE_draw_default_passes(psl);
	DRW_draw_pass(psl->material_pass);

	/* Post Process */
	EEVEE_draw_effects(vedata);
}

static void EEVEE_engine_free(void)
{
	EEVEE_materials_free();
	EEVEE_effects_free();
	EEVEE_lights_free();
	EEVEE_lightprobes_free();
}

static void EEVEE_layer_collection_settings_create(RenderEngine *UNUSED(engine), IDProperty *props)
{
	BLI_assert(props &&
	           props->type == IDP_GROUP &&
	           props->subtype == IDP_GROUP_SUB_ENGINE_RENDER);
	// BKE_collection_engine_property_add_int(props, "high_quality_sphere_lamps", false);
	UNUSED_VARS_NDEBUG(props);
}

static void EEVEE_scene_layer_settings_create(RenderEngine *UNUSED(engine), IDProperty *props)
{
	BLI_assert(props &&
	           props->type == IDP_GROUP &&
	           props->subtype == IDP_GROUP_SUB_ENGINE_RENDER);

	BKE_collection_engine_property_add_bool(props, "gtao_enable", false);
	BKE_collection_engine_property_add_bool(props, "gtao_use_bent_normals", true);
	BKE_collection_engine_property_add_float(props, "gtao_distance", 0.2f);
	BKE_collection_engine_property_add_float(props, "gtao_factor", 1.0f);
	BKE_collection_engine_property_add_int(props, "gtao_samples", 2);

	BKE_collection_engine_property_add_bool(props, "dof_enable", false);
	BKE_collection_engine_property_add_float(props, "bokeh_max_size", 100.0f);
	BKE_collection_engine_property_add_float(props, "bokeh_threshold", 1.0f);

	BKE_collection_engine_property_add_bool(props, "bloom_enable", false);
	BKE_collection_engine_property_add_float(props, "bloom_threshold", 0.8f);
	BKE_collection_engine_property_add_float(props, "bloom_knee", 0.5f);
	BKE_collection_engine_property_add_float(props, "bloom_intensity", 0.8f);
	BKE_collection_engine_property_add_float(props, "bloom_radius", 6.5f);

	BKE_collection_engine_property_add_bool(props, "motion_blur_enable", false);
	BKE_collection_engine_property_add_int(props, "motion_blur_samples", 8);
	BKE_collection_engine_property_add_float(props, "motion_blur_shutter", 1.0f);
}

static const DrawEngineDataSize EEVEE_data_size = DRW_VIEWPORT_DATA_SIZE(EEVEE_Data);

DrawEngineType draw_engine_eevee_type = {
	NULL, NULL,
	N_("Eevee"),
	&EEVEE_data_size,
	&EEVEE_engine_init,
	&EEVEE_engine_free,
	&EEVEE_cache_init,
	&EEVEE_cache_populate,
	&EEVEE_cache_finish,
	&EEVEE_draw_scene,
	NULL//&EEVEE_draw_scene
};

RenderEngineType DRW_engine_viewport_eevee_type = {
	NULL, NULL,
	EEVEE_ENGINE, N_("Eevee"), RE_INTERNAL | RE_USE_SHADING_NODES,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	&EEVEE_layer_collection_settings_create, &EEVEE_scene_layer_settings_create,
	&draw_engine_eevee_type,
	{NULL, NULL, NULL}
};


#undef EEVEE_ENGINE
