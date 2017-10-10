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

#include "BKE_object.h"

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

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
	}
	stl->g_data->background_alpha = 1.0f;
	stl->g_data->valid_double_buffer = (txl->color_double_buffer != NULL);

	DRWFboTexture tex = {&txl->color, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

	const float *viewport_size = DRW_viewport_size_get();
	DRW_framebuffer_init(&fbl->main, &draw_engine_eevee_type,
	                    (int)viewport_size[0], (int)viewport_size[1],
	                    &tex, 1);

	/* EEVEE_effects_init needs to go first for TAA */
	EEVEE_effects_init(sldata, vedata);

	EEVEE_materials_init(stl);
	EEVEE_lights_init(sldata);
	EEVEE_lightprobes_init(sldata, vedata);

	if (stl->effects->taa_current_sample > 1) {
		/* XXX otherwise it would break the other engines. */
		DRW_viewport_matrix_override_unset(DRW_MAT_PERS);
		DRW_viewport_matrix_override_unset(DRW_MAT_PERSINV);
		DRW_viewport_matrix_override_unset(DRW_MAT_WIN);
		DRW_viewport_matrix_override_unset(DRW_MAT_WININV);
	}
}

static void EEVEE_cache_init(void *vedata)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_SceneLayerData *sldata = EEVEE_scene_layer_data_get();

	EEVEE_materials_cache_init(vedata);
	EEVEE_lights_cache_init(sldata, psl);
	EEVEE_lightprobes_cache_init(sldata, vedata);
	EEVEE_effects_cache_init(sldata, vedata);
}

static void EEVEE_cache_populate(void *vedata, Object *ob)
{
	EEVEE_SceneLayerData *sldata = EEVEE_scene_layer_data_get();

	const DRWContextState *draw_ctx = DRW_context_state_get();
	const bool is_active = (ob == draw_ctx->obact);
	if (is_active) {
		if (DRW_object_is_mode_shade(ob) == true) {
			return;
		}
	}

	if (ELEM(ob->type, OB_MESH)) {
		if (!BKE_object_is_visible(ob)) {
			return;
		}

		EEVEE_materials_cache_populate(vedata, sldata, ob);

		const bool cast_shadow = true;

		if (cast_shadow) {
			if ((ob->base_flag & BASE_FROMDUPLI) != 0) {
				/* TODO: Special case for dupli objects because we cannot save the object pointer. */
			}
			else {
				BLI_addtail(&sldata->shadow_casters, BLI_genericNodeN(ob));
				EEVEE_ObjectEngineData *oedata = EEVEE_object_data_get(ob);
				oedata->need_update = ((ob->deg_update_flag & DEG_RUNTIME_DATA_UPDATE) != 0);
			}
		}
	}
	else if (ob->type == OB_LIGHTPROBE) {
		if ((ob->base_flag & BASE_FROMDUPLI) != 0) {
			/* TODO: Special case for dupli objects because we cannot save the object pointer. */
		}
		else {
			EEVEE_lightprobes_cache_add(sldata, ob);
		}
	}
	else if (ob->type == OB_LAMP) {
		if ((ob->base_flag & BASE_FROMDUPLI) != 0) {
			/* TODO: Special case for dupli objects because we cannot save the object pointer. */
		}
		else {
			EEVEE_lights_cache_add(sldata, ob);
		}
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
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
	EEVEE_FramebufferList *fbl = ((EEVEE_Data *)vedata)->fbl;
	EEVEE_SceneLayerData *sldata = EEVEE_scene_layer_data_get();

	/* Default framebuffer and texture */
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	/* Number of iteration: needed for all temporal effect (SSR, TAA)
	 * when using opengl render. */
	int loop_ct = DRW_state_is_image_render() ? 4 : 1;

	static float rand = 0.0f;

	/* XXX temp for denoising render. TODO plug number of samples here */
	if (DRW_state_is_image_render()) {
		rand += 1.0f / 16.0f;
		rand = rand - floorf(rand);

		/* Set jitter offset */
		EEVEE_update_util_texture(rand);
	}
	else if (((stl->effects->enabled_effects & EFFECT_TAA) != 0) && (stl->effects->taa_current_sample > 1)) {
		double r;
		BLI_halton_1D(2, 0.0, stl->effects->taa_current_sample - 1, &r);

		/* Set jitter offset */
		/* PERF This is killing perf ! */
		EEVEE_update_util_texture((float)r);
	}

	while (loop_ct--) {

		/* Refresh Probes */
		DRW_stats_group_start("Probes Refresh");
		EEVEE_lightprobes_refresh(sldata, vedata);
		DRW_stats_group_end();

		/* Refresh shadows */
		DRW_stats_group_start("Shadows");
		EEVEE_draw_shadows(sldata, psl);
		DRW_stats_group_end();

		/* Attach depth to the hdr buffer and bind it */
		DRW_framebuffer_texture_detach(dtxl->depth);
		DRW_framebuffer_texture_attach(fbl->main, dtxl->depth, 0, 0);
		DRW_framebuffer_bind(fbl->main);
		DRW_framebuffer_clear(false, true, false, NULL, 1.0f);

		if (((stl->effects->enabled_effects & EFFECT_TAA) != 0) && stl->effects->taa_current_sample > 1) {
			DRW_viewport_matrix_override_set(stl->effects->overide_persmat, DRW_MAT_PERS);
			DRW_viewport_matrix_override_set(stl->effects->overide_persinv, DRW_MAT_PERSINV);
			DRW_viewport_matrix_override_set(stl->effects->overide_winmat, DRW_MAT_WIN);
			DRW_viewport_matrix_override_set(stl->effects->overide_wininv, DRW_MAT_WININV);
		}

		/* Depth prepass */
		DRW_stats_group_start("Prepass");
		DRW_draw_pass(psl->depth_pass);
		DRW_draw_pass(psl->depth_pass_cull);
		DRW_stats_group_end();

		/* Create minmax texture */
		DRW_stats_group_start("Main MinMax buffer");
		EEVEE_create_minmax_buffer(vedata, dtxl->depth, -1);
		DRW_stats_group_end();

		/* Compute GTAO Horizons */
		EEVEE_effects_do_gtao(sldata, vedata);

		/* Restore main FB */
		DRW_framebuffer_bind(fbl->main);

		/* Shading pass */
		DRW_stats_group_start("Shading");
		DRW_draw_pass(psl->background_pass);
		EEVEE_draw_default_passes(psl);
		DRW_draw_pass(psl->material_pass);
		DRW_stats_group_end();

		/* Screen Space Reflections */
		DRW_stats_group_start("SSR");
		EEVEE_effects_do_ssr(sldata, vedata);
		DRW_stats_group_end();

		DRW_draw_pass(psl->probe_display);

		/* Prepare Refraction */
		EEVEE_effects_do_refraction(sldata, vedata);

		/* Restore main FB */
		DRW_framebuffer_bind(fbl->main);

		/* Opaque refraction */
		DRW_stats_group_start("Opaque Refraction");
		DRW_draw_pass(psl->refract_depth_pass);
		DRW_draw_pass(psl->refract_depth_pass_cull);
		DRW_draw_pass(psl->refract_pass);
		DRW_stats_group_end();

		/* Transparent */
		DRW_pass_sort_shgroup_z(psl->transparent_pass);
		DRW_stats_group_start("Transparent");
		DRW_draw_pass(psl->transparent_pass);
		DRW_stats_group_end();

		/* Volumetrics */
		DRW_stats_group_start("Volumetrics");
		EEVEE_effects_do_volumetrics(sldata, vedata);
		DRW_stats_group_end();

		/* Post Process */
		DRW_stats_group_start("Post FX");
		EEVEE_draw_effects(vedata);
		DRW_stats_group_end();

		if (stl->effects->taa_current_sample > 1) {
			DRW_viewport_matrix_override_unset(DRW_MAT_PERS);
			DRW_viewport_matrix_override_unset(DRW_MAT_PERSINV);
			DRW_viewport_matrix_override_unset(DRW_MAT_WIN);
			DRW_viewport_matrix_override_unset(DRW_MAT_WININV);
		}
	}

	stl->g_data->view_updated = false;
}

static void EEVEE_view_update(void *vedata)
{
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
	if (stl->g_data) {
		stl->g_data->view_updated = true;
	}
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

	BKE_collection_engine_property_add_int(props, "gi_diffuse_bounces", 3);
	BKE_collection_engine_property_add_int(props, "gi_cubemap_resolution", 512);

	BKE_collection_engine_property_add_int(props, "taa_samples", 8);

	BKE_collection_engine_property_add_bool(props, "ssr_enable", false);
	BKE_collection_engine_property_add_bool(props, "ssr_refraction", false);
	BKE_collection_engine_property_add_bool(props, "ssr_halfres", true);
	BKE_collection_engine_property_add_int(props, "ssr_ray_count", 1);
	BKE_collection_engine_property_add_float(props, "ssr_quality", 0.25f);
	BKE_collection_engine_property_add_float(props, "ssr_max_roughness", 0.5f);
	BKE_collection_engine_property_add_float(props, "ssr_thickness", 0.2f);
	BKE_collection_engine_property_add_float(props, "ssr_border_fade", 0.075f);
	BKE_collection_engine_property_add_float(props, "ssr_firefly_fac", 10.0f);

	BKE_collection_engine_property_add_bool(props, "volumetric_enable", false);
	BKE_collection_engine_property_add_float(props, "volumetric_start", 0.1f);
	BKE_collection_engine_property_add_float(props, "volumetric_end", 100.0f);
	BKE_collection_engine_property_add_int(props, "volumetric_samples", 64);
	BKE_collection_engine_property_add_float(props, "volumetric_sample_distribution", 0.8f);
	BKE_collection_engine_property_add_bool(props, "volumetric_lights", true);
	BKE_collection_engine_property_add_float(props, "volumetric_light_clamp", 0.0f);
	BKE_collection_engine_property_add_bool(props, "volumetric_shadows", false);
	BKE_collection_engine_property_add_int(props, "volumetric_shadow_samples", 16);
	BKE_collection_engine_property_add_bool(props, "volumetric_colored_transmittance", true);

	BKE_collection_engine_property_add_bool(props, "gtao_enable", false);
	BKE_collection_engine_property_add_bool(props, "gtao_use_bent_normals", true);
	BKE_collection_engine_property_add_bool(props, "gtao_denoise", true);
	BKE_collection_engine_property_add_bool(props, "gtao_bounce", true);
	BKE_collection_engine_property_add_float(props, "gtao_distance", 0.2f);
	BKE_collection_engine_property_add_float(props, "gtao_factor", 1.0f);
	BKE_collection_engine_property_add_float(props, "gtao_quality", 0.25f);
	BKE_collection_engine_property_add_int(props, "gtao_samples", 2);

	BKE_collection_engine_property_add_bool(props, "dof_enable", false);
	BKE_collection_engine_property_add_float(props, "bokeh_max_size", 100.0f);
	BKE_collection_engine_property_add_float(props, "bokeh_threshold", 1.0f);

	float default_bloom_color[3] = {1.0f, 1.0f, 1.0f};
	BKE_collection_engine_property_add_bool(props, "bloom_enable", false);
	BKE_collection_engine_property_add_float_array(props, "bloom_color", default_bloom_color, 3);
	BKE_collection_engine_property_add_float(props, "bloom_threshold", 0.8f);
	BKE_collection_engine_property_add_float(props, "bloom_knee", 0.5f);
	BKE_collection_engine_property_add_float(props, "bloom_intensity", 0.8f);
	BKE_collection_engine_property_add_float(props, "bloom_radius", 6.5f);
	BKE_collection_engine_property_add_float(props, "bloom_clamp", 1.0f);

	BKE_collection_engine_property_add_bool(props, "motion_blur_enable", false);
	BKE_collection_engine_property_add_int(props, "motion_blur_samples", 8);
	BKE_collection_engine_property_add_float(props, "motion_blur_shutter", 1.0f);

	BKE_collection_engine_property_add_int(props, "shadow_method", SHADOW_ESM);
	BKE_collection_engine_property_add_int(props, "shadow_size", 512);
	BKE_collection_engine_property_add_bool(props, "shadow_high_bitdepth", false);
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
	NULL, //&EEVEE_draw_scene
	&EEVEE_view_update,
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
