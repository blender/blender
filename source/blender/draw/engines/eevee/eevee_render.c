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

/** \file eevee_render.c
 *  \ingroup draw_engine
 */

/**
 * Render functions for final render outputs.
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_node_types.h"

#include "BLI_rand.h"
#include "BLI_rect.h"

#include "DEG_depsgraph_query.h"

#include "GPU_framebuffer.h"
#include "GPU_glew.h"

#include "RE_pipeline.h"

#include "eevee_private.h"

void EEVEE_render_init(EEVEE_Data *ved, RenderEngine *engine, struct Depsgraph *depsgraph)
{
	EEVEE_Data *vedata = (EEVEE_Data *)ved;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();
	Scene *scene = DEG_get_evaluated_scene(depsgraph);
	const float *viewport_size = DRW_viewport_size_get();

	/* Init default FB and render targets:
	 * In render mode the default framebuffer is not generated
	 * because there is no viewport. So we need to manually create it or
	 * not use it. For code clarity we just allocate it make use of it. */
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	/* NOTE : use 32 bit format for precision in render mode. */
	DRWFboTexture dtex = {&dtxl->depth, DRW_TEX_DEPTH_24_STENCIL_8, 0};
	DRW_framebuffer_init(&dfbl->default_fb, &draw_engine_eevee_type,
	                     (int)viewport_size[0], (int)viewport_size[1],
	                     &dtex, 1);

	DRWFboTexture tex = {&txl->color, DRW_TEX_RGBA_32, DRW_TEX_FILTER | DRW_TEX_MIPMAP};
	DRW_framebuffer_init(&fbl->main, &draw_engine_eevee_type,
	                     (int)viewport_size[0], (int)viewport_size[1],
	                     &tex, 1);

	/* Alloc transient data. */
	if (!stl->g_data) {
		stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
	}
	EEVEE_PrivateData *g_data = stl->g_data;
	g_data->background_alpha = DRW_state_draw_background() ? 1.0f : 0.0f;
	g_data->valid_double_buffer = 0;

	/* Alloc common ubo data. */
	if (sldata->common_ubo == NULL) {
		sldata->common_ubo = DRW_uniformbuffer_create(sizeof(sldata->common_data), &sldata->common_data);
	}
	if (sldata->clip_ubo == NULL) {
		sldata->clip_ubo = DRW_uniformbuffer_create(sizeof(sldata->clip_data), &sldata->clip_data);
	}

	/* Set the pers & view matrix. */
	struct Object *camera = RE_GetCamera(engine->re);
	float frame = BKE_scene_frame_get(scene);
	RE_GetCameraWindow(engine->re, camera, frame, g_data->winmat);
	RE_GetCameraModelMatrix(engine->re, camera, g_data->viewinv);

	invert_m4_m4(g_data->viewmat, g_data->viewinv);
	mul_m4_m4m4(g_data->persmat, g_data->winmat, g_data->viewmat);
	invert_m4_m4(g_data->persinv, g_data->persmat);
	invert_m4_m4(g_data->wininv, g_data->winmat);

	DRW_viewport_matrix_override_set(g_data->persmat, DRW_MAT_PERS);
	DRW_viewport_matrix_override_set(g_data->persinv, DRW_MAT_PERSINV);
	DRW_viewport_matrix_override_set(g_data->winmat, DRW_MAT_WIN);
	DRW_viewport_matrix_override_set(g_data->wininv, DRW_MAT_WININV);
	DRW_viewport_matrix_override_set(g_data->viewmat, DRW_MAT_VIEW);
	DRW_viewport_matrix_override_set(g_data->viewinv, DRW_MAT_VIEWINV);

	/* EEVEE_effects_init needs to go first for TAA */
	EEVEE_effects_init(sldata, vedata, camera);
	EEVEE_materials_init(sldata, stl, fbl);
	EEVEE_lights_init(sldata);
	EEVEE_lightprobes_init(sldata, vedata);

	/* INIT CACHE */
	EEVEE_bloom_cache_init(sldata, vedata);
	EEVEE_depth_of_field_cache_init(sldata, vedata);
	EEVEE_effects_cache_init(sldata, vedata);
	EEVEE_lightprobes_cache_init(sldata, vedata);
	EEVEE_lights_cache_init(sldata, vedata);
	EEVEE_materials_cache_init(sldata, vedata);
	EEVEE_motion_blur_cache_init(sldata, vedata);
	EEVEE_occlusion_cache_init(sldata, vedata);
	EEVEE_screen_raytrace_cache_init(sldata, vedata);
	EEVEE_subsurface_cache_init(sldata, vedata);
	EEVEE_temporal_sampling_cache_init(sldata, vedata);
	EEVEE_volumes_cache_init(sldata, vedata);
}

void EEVEE_render_cache(
        void *vedata, struct Object *ob,
        struct RenderEngine *UNUSED(engine), struct Depsgraph *UNUSED(depsgraph))
{
	EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();

	if (DRW_check_object_visible_within_active_context(ob) == false) {
		return;
	}

	if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT)) {
		EEVEE_materials_cache_populate(vedata, sldata, ob);

		const bool cast_shadow = true;

		if (cast_shadow) {
			EEVEE_lights_cache_shcaster_object_add(sldata, ob);
		}
	}
	else if (ob->type == OB_LIGHTPROBE) {
		EEVEE_lightprobes_cache_add(sldata, ob);
	}
	else if (ob->type == OB_LAMP) {
		EEVEE_lights_cache_add(sldata, ob);
	}
}

static void eevee_render_result_combined(
        RenderLayer *rl, const char *viewname, const rcti *rect,
        EEVEE_Data *vedata, EEVEE_ViewLayerData *UNUSED(sldata))
{
	RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_COMBINED, viewname);

	DRW_framebuffer_bind(vedata->stl->effects->final_fb);
	DRW_framebuffer_read_data(rect->xmin, rect->ymin, BLI_rcti_size_x(rect), BLI_rcti_size_y(rect), 4, 0, rp->rect);
}

static void eevee_render_result_subsurface(
        RenderLayer *rl, const char *viewname, const rcti *rect,
        EEVEE_Data *vedata, EEVEE_ViewLayerData *UNUSED(sldata), int render_samples)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;

	if (vedata->fbl->sss_accum_fb == NULL) {
		/* SSS is not enabled. */
		return;
	}

	if ((view_layer->passflag & SCE_PASS_SUBSURFACE_COLOR) != 0) {
		RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_SUBSURFACE_COLOR, viewname);

		DRW_framebuffer_bind(vedata->fbl->sss_accum_fb);
		DRW_framebuffer_read_data(rect->xmin, rect->ymin, BLI_rcti_size_x(rect), BLI_rcti_size_y(rect), 3, 1, rp->rect);

		/* This is the accumulated color. Divide by the number of samples. */
		for (int i = 0; i < rp->rectx * rp->recty * 3; i++) {
			rp->rect[i] /= (float)render_samples;
		}
	}

	if ((view_layer->passflag & SCE_PASS_SUBSURFACE_DIRECT) != 0) {
		RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_SUBSURFACE_DIRECT, viewname);

		DRW_framebuffer_bind(vedata->fbl->sss_accum_fb);
		DRW_framebuffer_read_data(rect->xmin, rect->ymin, BLI_rcti_size_x(rect), BLI_rcti_size_y(rect), 3, 0, rp->rect);

		/* This is the accumulated color. Divide by the number of samples. */
		for (int i = 0; i < rp->rectx * rp->recty * 3; i++) {
			rp->rect[i] /= (float)render_samples;
		}
	}

	if ((view_layer->passflag & SCE_PASS_SUBSURFACE_INDIRECT) != 0) {
		/* Do nothing as all the lighting is in the direct pass.
		 * TODO : Separate Direct from indirect lighting. */
	}
}

static void eevee_render_result_normal(
        RenderLayer *rl, const char *viewname, const rcti *rect,
        EEVEE_Data *vedata, EEVEE_ViewLayerData *UNUSED(sldata))
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_PrivateData *g_data = stl->g_data;

	/* Only read the center texel. */
	if (stl->effects->taa_current_sample > 1)
		return;

	if ((view_layer->passflag & SCE_PASS_NORMAL) != 0) {
		RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_NORMAL, viewname);

		DRW_framebuffer_read_data(rect->xmin, rect->ymin, BLI_rcti_size_x(rect), BLI_rcti_size_y(rect), 3, 1, rp->rect);

		/* Convert Eevee encoded normals to Blender normals. */
		for (int i = 0; i < rp->rectx * rp->recty * 3; i += 3) {
			if (rp->rect[i] == 0.0f && rp->rect[i + 1] == 0.0f) {
				/* If normal is not correct then do not produce NANs.  */
				continue;
			}

			float fenc[2];
			fenc[0] = rp->rect[i+0] * 4.0f - 2.0f;
			fenc[1] = rp->rect[i+1] * 4.0f - 2.0f;

			float f = dot_v2v2(fenc, fenc);
			float g = sqrtf(1.0f - f / 4.0f);

			rp->rect[i + 0] = fenc[0] * g;
			rp->rect[i + 1] = fenc[1] * g;
			rp->rect[i + 2] = 1.0f - f / 2.0f;

			mul_mat3_m4_v3(g_data->viewinv, &rp->rect[i]);
		}
	}
}

static void eevee_render_result_z(
        RenderLayer *rl, const char *viewname, const rcti *rect,
        EEVEE_Data *vedata, EEVEE_ViewLayerData *sldata)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_PrivateData *g_data = stl->g_data;

	/* Only read the center texel. */
	if (stl->effects->taa_current_sample > 1)
		return;

	if ((view_layer->passflag & SCE_PASS_Z) != 0) {
		RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_Z, viewname);

		DRW_framebuffer_read_depth(rect->xmin, rect->ymin, BLI_rcti_size_x(rect), BLI_rcti_size_y(rect), rp->rect);

		bool is_persp = DRW_viewport_is_persp_get();

		/* Convert ogl depth [0..1] to view Z [near..far] */
		for (int i = 0; i < rp->rectx * rp->recty; ++i) {
			if (rp->rect[i] == 1.0f ) {
				rp->rect[i] = 1e10f; /* Background */
			}
			else {
				if (is_persp) {
					rp->rect[i] = rp->rect[i] * 2.0f - 1.0f;
					rp->rect[i] = g_data->winmat[3][2] / (rp->rect[i] + g_data->winmat[2][2]);
				}
				else {
					rp->rect[i] = -common_data->view_vecs[0][2] + rp->rect[i] * -common_data->view_vecs[1][2];
				}
			}
		}
	}
}

static void eevee_render_result_mist(
        RenderLayer *rl, const char *viewname, const rcti *rect,
        EEVEE_Data *vedata, EEVEE_ViewLayerData *UNUSED(sldata), int render_samples)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;

	if ((view_layer->passflag & SCE_PASS_MIST) != 0) {
		RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_MIST, viewname);

		DRW_framebuffer_bind(vedata->fbl->mist_accum_fb);
		DRW_framebuffer_read_data(rect->xmin, rect->ymin, BLI_rcti_size_x(rect), BLI_rcti_size_y(rect), 1, 0, rp->rect);

		/* This is the accumulated color. Divide by the number of samples. */
		for (int i = 0; i < rp->rectx * rp->recty; i++) {
			rp->rect[i] /= (float)render_samples;
		}
	}
}

static void eevee_render_result_occlusion(
        RenderLayer *rl, const char *viewname, const rcti *rect,
        EEVEE_Data *vedata, EEVEE_ViewLayerData *UNUSED(sldata), int render_samples)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;

	if (vedata->fbl->ao_accum_fb == NULL) {
		/* AO is not enabled. */
		return;
	}

	if ((view_layer->passflag & SCE_PASS_AO) != 0) {
		RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_AO, viewname);

		DRW_framebuffer_bind(vedata->fbl->ao_accum_fb);
		DRW_framebuffer_read_data(rect->xmin, rect->ymin, BLI_rcti_size_x(rect), BLI_rcti_size_y(rect), 3, 0, rp->rect);

		/* This is the accumulated color. Divide by the number of samples. */
		for (int i = 0; i < rp->rectx * rp->recty * 3; i += 3) {
			rp->rect[i] = rp->rect[i + 1] = rp->rect[i+2] = min_ff(1.0f, rp->rect[i] / (float)render_samples);
		}
	}
}

static void eevee_render_draw_background(EEVEE_Data *vedata)
{
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_PassList *psl = vedata->psl;

	/* Prevent background to write to data buffers.
	 * NOTE : This also make sure the textures are bound
	 *        to the right double buffer. */
	if (txl->ssr_normal_input != NULL) {
		DRW_framebuffer_texture_detach(txl->ssr_normal_input);
	}
	if (txl->ssr_specrough_input != NULL) {
		DRW_framebuffer_texture_detach(txl->ssr_specrough_input);
	}
	DRW_framebuffer_bind(fbl->main);

	DRW_draw_pass(psl->background_pass);

	if (txl->ssr_normal_input != NULL) {
		DRW_framebuffer_texture_attach(fbl->main, txl->ssr_normal_input, 1, 0);
	}
	if (txl->ssr_specrough_input != NULL) {
		DRW_framebuffer_texture_attach(fbl->main, txl->ssr_specrough_input, 2, 0);
	}
	DRW_framebuffer_bind(fbl->main);
}

void EEVEE_render_draw(EEVEE_Data *vedata, RenderEngine *engine, RenderLayer *rl, const rcti *rect)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	const char *viewname = RE_GetActiveRenderView(engine->re);
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
	EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();
	EEVEE_PrivateData *g_data = stl->g_data;

	/* FINISH CACHE */
	EEVEE_materials_cache_finish(vedata);
	EEVEE_lights_cache_finish(sldata);
	EEVEE_lightprobes_cache_finish(sldata, vedata);

	/* Sort transparents before the loop. */
	DRW_pass_sort_shgroup_z(psl->transparent_pass);

	/* Push instances attribs to the GPU. */
	DRW_render_instance_buffer_finish();

	if ((view_layer->passflag & (SCE_PASS_SUBSURFACE_COLOR |
	                             SCE_PASS_SUBSURFACE_DIRECT |
	                             SCE_PASS_SUBSURFACE_INDIRECT)) != 0)
	{
		EEVEE_subsurface_output_init(sldata, vedata);
	}

	if ((view_layer->passflag & SCE_PASS_MIST) != 0) {
		EEVEE_mist_output_init(sldata, vedata);
	}

	if ((view_layer->passflag & SCE_PASS_AO) != 0) {
		EEVEE_occlusion_output_init(sldata, vedata);
	}

	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);
	unsigned int tot_sample = BKE_collection_engine_property_value_get_int(props, "taa_render_samples");
	unsigned int render_samples = 0;

	if (RE_engine_test_break(engine)) {
		return;
	}

	while (render_samples < tot_sample && !RE_engine_test_break(engine)) {
		float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		unsigned int primes[3] = {2, 3, 7};
		double offset[3] = {0.0, 0.0, 0.0};
		double r[3];

		/* Restore winmat before jittering again. */
		copy_m4_m4(stl->effects->overide_winmat, g_data->winmat);
		/* Copy previous persmat to UBO data */
		copy_m4_m4(sldata->common_data.prev_persmat, stl->effects->prev_persmat);

		BLI_halton_3D(primes, offset, stl->effects->taa_current_sample, r);
		EEVEE_update_noise(psl, fbl, r);
		EEVEE_temporal_sampling_matrices_calc(stl->effects, g_data->viewmat, g_data->persmat, r);
		EEVEE_volumes_set_jitter(sldata, stl->effects->taa_current_sample - 1);
		EEVEE_materials_init(sldata, stl, fbl);

		/* Set matrices. */
		DRW_viewport_matrix_override_set(stl->effects->overide_persmat, DRW_MAT_PERS);
		DRW_viewport_matrix_override_set(stl->effects->overide_persinv, DRW_MAT_PERSINV);
		DRW_viewport_matrix_override_set(stl->effects->overide_winmat, DRW_MAT_WIN);
		DRW_viewport_matrix_override_set(stl->effects->overide_wininv, DRW_MAT_WININV);
		DRW_viewport_matrix_override_set(g_data->viewmat, DRW_MAT_VIEW);
		DRW_viewport_matrix_override_set(g_data->viewinv, DRW_MAT_VIEWINV);

		/* Refresh Probes */
		while (EEVEE_lightprobes_all_probes_ready(sldata, vedata) == false) {
			EEVEE_lightprobes_refresh(sldata, vedata);
		}
		EEVEE_lightprobes_refresh_planar(sldata, vedata);
		DRW_uniformbuffer_update(sldata->common_ubo, &sldata->common_data);

		/* Refresh Shadows */
		EEVEE_draw_shadows(sldata, psl);

		DRW_framebuffer_texture_detach(dtxl->depth);
		DRW_framebuffer_texture_attach(fbl->main, dtxl->depth, 0, 0);
		DRW_framebuffer_bind(fbl->main);
		DRW_framebuffer_clear(true, true, true, clear_col, 1.0f);
		/* Depth prepass */
		DRW_draw_pass(psl->depth_pass);
		DRW_draw_pass(psl->depth_pass_cull);
		/* Create minmax texture */
		EEVEE_create_minmax_buffer(vedata, dtxl->depth, -1);
		EEVEE_occlusion_compute(sldata, vedata, dtxl->depth, -1);
		EEVEE_volumes_compute(sldata, vedata);
		/* Shading pass */
		eevee_render_draw_background(vedata);
		DRW_framebuffer_bind(fbl->main);
		EEVEE_draw_default_passes(psl);
		DRW_draw_pass(psl->material_pass);
		EEVEE_subsurface_data_render(sldata, vedata);
		/* Effects pre-transparency */
		EEVEE_subsurface_compute(sldata, vedata);
		EEVEE_reflection_compute(sldata, vedata);
		EEVEE_refraction_compute(sldata, vedata);
		/* Opaque refraction */
		DRW_draw_pass(psl->refract_depth_pass);
		DRW_draw_pass(psl->refract_depth_pass_cull);
		DRW_draw_pass(psl->refract_pass);
		/* Subsurface output */
		EEVEE_subsurface_output_accumulate(sldata, vedata);
		/* Occlusion output */
		EEVEE_occlusion_output_accumulate(sldata, vedata);
		/* Result NORMAL */
		eevee_render_result_normal(rl, viewname, rect, vedata, sldata);
		/* Volumetrics Resolve Opaque */
		EEVEE_volumes_resolve(sldata, vedata);
		/* Mist output */
		EEVEE_mist_output_accumulate(sldata, vedata);
		/* Transparent */
		DRW_draw_pass(psl->transparent_pass);
		/* Result Z */
		eevee_render_result_z(rl, viewname, rect, vedata, sldata);
		/* Post Process */
		EEVEE_draw_effects(sldata, vedata);

		RE_engine_update_progress(engine, (float)(render_samples++) / (float)tot_sample);
	}

	eevee_render_result_combined(rl, viewname, rect, vedata, sldata);
	eevee_render_result_subsurface(rl, viewname, rect, vedata, sldata, render_samples);
	eevee_render_result_mist(rl, viewname, rect, vedata, sldata, render_samples);
	eevee_render_result_occlusion(rl, viewname, rect, vedata, sldata, render_samples);
}

void EEVEE_render_update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer)
{
	int type;

	RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_COMBINED, 4, "RGBA", SOCK_RGBA);

#define CHECK_PASS(name, channels, chanid) \
	if (view_layer->passflag & (SCE_PASS_ ## name)) { \
		if (channels == 4) type = SOCK_RGBA; \
		else if (channels == 3) type = SOCK_VECTOR; \
		else type = SOCK_FLOAT; \
		RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_ ## name, channels, chanid, type); \
	}

	CHECK_PASS(Z,           1, "Z");
	CHECK_PASS(MIST,        1, "Z");
	CHECK_PASS(NORMAL,      3, "XYZ");
	CHECK_PASS(AO,          3, "RGB");
	CHECK_PASS(SUBSURFACE_COLOR,     3, "RGB");
	CHECK_PASS(SUBSURFACE_DIRECT,    3, "RGB");

#undef CHECK_PASS
}
