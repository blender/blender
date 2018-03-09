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

/** \file eevee_occlusion.c
 *  \ingroup draw_engine
 *
 * Implementation of the screen space Ground Truth Ambient Occlusion.
 */

#include "DRW_render.h"

#include "BLI_string_utils.h"

#include "DNA_anim_types.h"

#include "BKE_global.h" /* for G.debug_value */

#include "eevee_private.h"

static struct {
	/* Ground Truth Ambient Occlusion */
	struct GPUShader *gtao_sh;
	struct GPUShader *gtao_layer_sh;
	struct GPUShader *gtao_debug_sh;
	struct GPUTexture *src_depth;
} e_data = {NULL}; /* Engine data */

extern char datatoc_ambient_occlusion_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_effect_gtao_frag_glsl[];

static void eevee_create_shader_occlusion(void)
{
	char *frag_str = BLI_string_joinN(
	        datatoc_common_view_lib_glsl,
	        datatoc_common_uniforms_lib_glsl,
	        datatoc_bsdf_common_lib_glsl,
	        datatoc_ambient_occlusion_lib_glsl,
	        datatoc_effect_gtao_frag_glsl);

	e_data.gtao_sh = DRW_shader_create_fullscreen(frag_str, NULL);
	e_data.gtao_layer_sh = DRW_shader_create_fullscreen(frag_str, "#define LAYERED_DEPTH\n");
	e_data.gtao_debug_sh = DRW_shader_create_fullscreen(frag_str, "#define DEBUG_AO\n");

	MEM_freeN(frag_str);
}

int EEVEE_occlusion_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;

	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer,
	                                                        COLLECTION_MODE_NONE,
	                                                        RE_engine_id_BLENDER_EEVEE);

	if (BKE_collection_engine_property_value_get_bool(props, "gtao_enable")) {
		const float *viewport_size = DRW_viewport_size_get();

		/* Shaders */
		if (!e_data.gtao_sh) {
			eevee_create_shader_occlusion();
		}

		common_data->ao_dist = BKE_collection_engine_property_value_get_float(props, "gtao_distance");
		common_data->ao_factor = BKE_collection_engine_property_value_get_float(props, "gtao_factor");
		common_data->ao_quality = 1.0f - BKE_collection_engine_property_value_get_float(props, "gtao_quality");

		common_data->ao_settings = 1.0; /* USE_AO */
		if (BKE_collection_engine_property_value_get_bool(props, "gtao_use_bent_normals")) {
			common_data->ao_settings += 2.0; /* USE_BENT_NORMAL */
		}
		if (BKE_collection_engine_property_value_get_bool(props, "gtao_denoise")) {
			common_data->ao_settings += 4.0; /* USE_DENOISE */
		}

		common_data->ao_bounce_fac = (float)BKE_collection_engine_property_value_get_bool(props, "gtao_bounce");

		DRWFboTexture tex = {&txl->gtao_horizons, DRW_TEX_RGBA_8, 0};

		DRW_framebuffer_init(&fbl->gtao_fb, &draw_engine_eevee_type,
		                    (int)viewport_size[0], (int)viewport_size[1],
		                    &tex, 1);

		if (G.debug_value == 6) {
			DRWFboTexture tex_debug = {&stl->g_data->gtao_horizons_debug, DRW_TEX_RGBA_8, DRW_TEX_TEMP};

			DRW_framebuffer_init(&fbl->gtao_debug_fb, &draw_engine_eevee_type,
			                    (int)viewport_size[0], (int)viewport_size[1],
			                    &tex_debug, 1);
		}

		return EFFECT_GTAO | EFFECT_NORMAL_BUFFER;
	}

	/* Cleanup */
	DRW_TEXTURE_FREE_SAFE(txl->gtao_horizons);
	DRW_FRAMEBUFFER_FREE_SAFE(fbl->gtao_fb);
	common_data->ao_settings = 0.0f;

	return 0;
}

void EEVEE_occlusion_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_PassList *psl = vedata->psl;
	const float *viewport_size = DRW_viewport_size_get();

	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);

	if (BKE_collection_engine_property_value_get_bool(props, "gtao_enable")) {
		DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
		float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

		DRWFboTexture tex_data = {&txl->ao_accum, DRW_TEX_R_32, 0};
		DRW_framebuffer_init(&fbl->ao_accum_fb, &draw_engine_eevee_type, (int)viewport_size[0], (int)viewport_size[1],
		                     &tex_data, 1);

		/* Clear texture. */
		DRW_framebuffer_bind(fbl->ao_accum_fb);
		DRW_framebuffer_clear(true, false, false, clear, 0.0f);

		/* Accumulation pass */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE;
		psl->ao_accum_ps = DRW_pass_create("AO Accum", state);
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.gtao_debug_sh, psl->ao_accum_ps);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
		DRW_shgroup_uniform_buffer(grp, "maxzBuffer", &txl->maxzbuffer);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &dtxl->depth);
		DRW_shgroup_uniform_buffer(grp, "normalBuffer", &txl->ssr_normal_input);
		DRW_shgroup_uniform_buffer(grp, "horizonBuffer", &txl->gtao_horizons);
		DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
		DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
	}
	else {
		/* Cleanup to release memory */
		DRW_TEXTURE_FREE_SAFE(txl->ao_accum);
		DRW_FRAMEBUFFER_FREE_SAFE(fbl->ao_accum_fb);
	}
}

void EEVEE_occlusion_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	struct Gwn_Batch *quad = DRW_cache_fullscreen_quad_get();

	if ((effects->enabled_effects & EFFECT_GTAO) != 0) {
		/**  Occlusion algorithm overview
		 *
		 *  We separate the computation into 2 steps.
		 *
		 * - First we scan the neighborhood pixels to find the maximum horizon angle.
		 *   We save this angle in a RG8 array texture.
		 *
		 * - Then we use this angle to compute occlusion with the shading normal at
		 *   the shading stage. This let us do correct shadowing for each diffuse / specular
		 *   lobe present in the shader using the correct normal.
		 **/
		psl->ao_horizon_search = DRW_pass_create("GTAO Horizon Search", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.gtao_sh, psl->ao_horizon_search);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
		DRW_shgroup_uniform_buffer(grp, "maxzBuffer", &txl->maxzbuffer);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &effects->ao_src_depth);
		DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->ao_horizon_search_layer = DRW_pass_create("GTAO Horizon Search Layer", DRW_STATE_WRITE_COLOR);
		grp = DRW_shgroup_create(e_data.gtao_layer_sh, psl->ao_horizon_search_layer);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
		DRW_shgroup_uniform_buffer(grp, "maxzBuffer", &txl->maxzbuffer);
		DRW_shgroup_uniform_buffer(grp, "depthBufferLayered", &effects->ao_src_depth);
		DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
		DRW_shgroup_uniform_int(grp, "layer", &stl->effects->ao_depth_layer, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		if (G.debug_value == 6) {
			psl->ao_horizon_debug = DRW_pass_create("GTAO Horizon Debug", DRW_STATE_WRITE_COLOR);
			grp = DRW_shgroup_create(e_data.gtao_debug_sh, psl->ao_horizon_debug);
			DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
			DRW_shgroup_uniform_buffer(grp, "maxzBuffer", &txl->maxzbuffer);
			DRW_shgroup_uniform_buffer(grp, "depthBuffer", &dtxl->depth);
			DRW_shgroup_uniform_buffer(grp, "normalBuffer", &txl->ssr_normal_input);
			DRW_shgroup_uniform_buffer(grp, "horizonBuffer", &txl->gtao_horizons);
			DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
			DRW_shgroup_call_add(grp, quad, NULL);
		}
	}
}

void EEVEE_occlusion_compute(
        EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata, struct GPUTexture *depth_src, int layer)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if ((effects->enabled_effects & EFFECT_GTAO) != 0) {
		DRW_stats_group_start("GTAO Horizon Scan");
		effects->ao_src_depth = depth_src;
		effects->ao_depth_layer = layer;

		DRW_framebuffer_bind(fbl->gtao_fb);

		if (layer >= 0) {
			DRW_draw_pass(psl->ao_horizon_search_layer);
		}
		else {
			DRW_draw_pass(psl->ao_horizon_search);
		}

		/* Restore */
		DRW_framebuffer_bind(fbl->main);

		DRW_stats_group_end();
	}
}

void EEVEE_occlusion_draw_debug(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if (((effects->enabled_effects & EFFECT_GTAO) != 0) && (G.debug_value == 6)) {
		DRW_stats_group_start("GTAO Debug");

		DRW_framebuffer_texture_attach(fbl->gtao_debug_fb, stl->g_data->gtao_horizons_debug, 0, 0);
		DRW_framebuffer_bind(fbl->gtao_debug_fb);

		DRW_draw_pass(psl->ao_horizon_debug);

		/* Restore */
		DRW_framebuffer_texture_detach(stl->g_data->gtao_horizons_debug);
		DRW_framebuffer_bind(fbl->main);

		DRW_stats_group_end();
	}
}

void EEVEE_occlusion_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_PassList *psl = vedata->psl;

	if (fbl->ao_accum_fb != NULL) {
		DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

		/* Update the min_max/horizon buffers so the refracion materials appear in it. */
		EEVEE_create_minmax_buffer(vedata, dtxl->depth, -1);
		EEVEE_occlusion_compute(sldata, vedata, dtxl->depth, -1);

		DRW_framebuffer_bind(fbl->ao_accum_fb);
		DRW_draw_pass(psl->ao_accum_ps);

		/* Restore */
		DRW_framebuffer_bind(fbl->main);
	}
}

void EEVEE_occlusion_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.gtao_sh);
	DRW_SHADER_FREE_SAFE(e_data.gtao_layer_sh);
	DRW_SHADER_FREE_SAFE(e_data.gtao_debug_sh);
}
