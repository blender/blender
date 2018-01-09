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

#include "BLI_dynstr.h"

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
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_effect_gtao_frag_glsl[];

static void eevee_create_shader_occlusion(void)
{
	DynStr *ds_frag = BLI_dynstr_new();
	BLI_dynstr_append(ds_frag, datatoc_bsdf_common_lib_glsl);
	BLI_dynstr_append(ds_frag, datatoc_ambient_occlusion_lib_glsl);
	BLI_dynstr_append(ds_frag, datatoc_effect_gtao_frag_glsl);
	char *frag_str = BLI_dynstr_get_cstring(ds_frag);
	BLI_dynstr_free(ds_frag);

	e_data.gtao_sh = DRW_shader_create_fullscreen(frag_str, NULL);
	e_data.gtao_layer_sh = DRW_shader_create_fullscreen(frag_str, "#define LAYERED_DEPTH\n");
	e_data.gtao_debug_sh = DRW_shader_create_fullscreen(frag_str, "#define DEBUG_AO\n");

	MEM_freeN(frag_str);
}

int EEVEE_occlusion_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;

	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);

	if (BKE_collection_engine_property_value_get_bool(props, "gtao_enable")) {
		const float *viewport_size = DRW_viewport_size_get();

		/* Shaders */
		if (!e_data.gtao_sh) {
			eevee_create_shader_occlusion();
		}

		effects->ao_dist = BKE_collection_engine_property_value_get_float(props, "gtao_distance");
		effects->ao_factor = BKE_collection_engine_property_value_get_float(props, "gtao_factor");
		effects->ao_quality = 1.0f - BKE_collection_engine_property_value_get_float(props, "gtao_quality");
		effects->ao_samples = BKE_collection_engine_property_value_get_int(props, "gtao_samples");
		effects->ao_samples_inv = 1.0f / effects->ao_samples;

		effects->ao_settings = 1.0; /* USE_AO */
		if (BKE_collection_engine_property_value_get_bool(props, "gtao_use_bent_normals")) {
			effects->ao_settings += 2.0; /* USE_BENT_NORMAL */
		}
		if (BKE_collection_engine_property_value_get_bool(props, "gtao_denoise")) {
			effects->ao_settings += 4.0; /* USE_DENOISE */
		}

		effects->ao_bounce_fac = (float)BKE_collection_engine_property_value_get_bool(props, "gtao_bounce");

		effects->ao_texsize[0] = ((int)viewport_size[0]);
		effects->ao_texsize[1] = ((int)viewport_size[1]);

		/* Round up to multiple of 2 */
		if ((effects->ao_texsize[0] & 0x1) != 0) {
			effects->ao_texsize[0] += 1;
		}
		if ((effects->ao_texsize[1] & 0x1) != 0) {
			effects->ao_texsize[1] += 1;
		}

		CLAMP(effects->ao_samples, 1, 32);

		if (effects->hori_tex_layers != effects->ao_samples) {
			DRW_TEXTURE_FREE_SAFE(txl->gtao_horizons);
		}

		if (txl->gtao_horizons == NULL) {
			effects->hori_tex_layers = effects->ao_samples;
			txl->gtao_horizons = DRW_texture_create_2D_array((int)viewport_size[0], (int)viewport_size[1], effects->hori_tex_layers, DRW_TEX_RG_8, 0, NULL);
		}

		DRWFboTexture tex = {&txl->gtao_horizons, DRW_TEX_RG_8, 0};

		DRW_framebuffer_init(&fbl->gtao_fb, &draw_engine_eevee_type,
		                    effects->ao_texsize[0], effects->ao_texsize[1],
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
	effects->ao_settings = 0.0f;

	return 0;
}

void EEVEE_occlusion_cache_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
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
		DRW_shgroup_uniform_buffer(grp, "maxzBuffer", &txl->maxzbuffer);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &effects->ao_src_depth);
		DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
		DRW_shgroup_uniform_vec2(grp, "mipRatio[0]", (float *)stl->g_data->mip_ratio, 10);
		DRW_shgroup_uniform_vec4(grp, "aoParameters[0]", &stl->effects->ao_dist, 2);
		DRW_shgroup_uniform_float(grp, "sampleNbr", &stl->effects->ao_sample_nbr, 1);
		DRW_shgroup_uniform_ivec2(grp, "aoHorizonTexSize", (int *)stl->effects->ao_texsize, 1);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->ao_horizon_search_layer = DRW_pass_create("GTAO Horizon Search Layer", DRW_STATE_WRITE_COLOR);
		grp = DRW_shgroup_create(e_data.gtao_layer_sh, psl->ao_horizon_search_layer);
		DRW_shgroup_uniform_buffer(grp, "maxzBuffer", &txl->maxzbuffer);
		DRW_shgroup_uniform_buffer(grp, "depthBufferLayered", &effects->ao_src_depth);
		DRW_shgroup_uniform_int(grp, "layer", &stl->effects->ao_depth_layer, 1);
		DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
		DRW_shgroup_uniform_vec2(grp, "mipRatio[0]", (float *)stl->g_data->mip_ratio, 10);
		DRW_shgroup_uniform_vec4(grp, "aoParameters[0]", &stl->effects->ao_dist, 2);
		DRW_shgroup_uniform_float(grp, "sampleNbr", &stl->effects->ao_sample_nbr, 1);
		DRW_shgroup_uniform_ivec2(grp, "aoHorizonTexSize", (int *)stl->effects->ao_texsize, 1);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
		DRW_shgroup_call_add(grp, quad, NULL);

		if (G.debug_value == 6) {
			psl->ao_horizon_debug = DRW_pass_create("GTAO Horizon Debug", DRW_STATE_WRITE_COLOR);
			grp = DRW_shgroup_create(e_data.gtao_debug_sh, psl->ao_horizon_debug);
			DRW_shgroup_uniform_buffer(grp, "maxzBuffer", &txl->maxzbuffer);
			DRW_shgroup_uniform_buffer(grp, "depthBuffer", &dtxl->depth);
			DRW_shgroup_uniform_buffer(grp, "normalBuffer", &txl->ssr_normal_input);
			DRW_shgroup_uniform_buffer(grp, "horizonBuffer", &txl->gtao_horizons);
			DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
			DRW_shgroup_uniform_vec2(grp, "mipRatio[0]", (float *)stl->g_data->mip_ratio, 10);
			DRW_shgroup_uniform_vec4(grp, "aoParameters[0]", &stl->effects->ao_dist, 2);
			DRW_shgroup_uniform_ivec2(grp, "aoHorizonTexSize", (int *)stl->effects->ao_texsize, 1);
			DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
			DRW_shgroup_call_add(grp, quad, NULL);
		}
	}
}

void EEVEE_occlusion_compute(
        EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata, struct GPUTexture *depth_src, int layer)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if ((effects->enabled_effects & EFFECT_GTAO) != 0) {
		DRW_stats_group_start("GTAO Horizon Scan");
		effects->ao_src_depth = depth_src;
		effects->ao_depth_layer = layer;

		for (effects->ao_sample_nbr = 0.0;
		     effects->ao_sample_nbr < effects->ao_samples;
		     ++effects->ao_sample_nbr)
		{
			DRW_framebuffer_texture_detach(txl->gtao_horizons);
			DRW_framebuffer_texture_layer_attach(fbl->gtao_fb, txl->gtao_horizons, 0, (int)effects->ao_sample_nbr, 0);
			DRW_framebuffer_bind(fbl->gtao_fb);

			if (layer >= 0) {
				DRW_draw_pass(psl->ao_horizon_search_layer);
			}
			else {
				DRW_draw_pass(psl->ao_horizon_search);
			}
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

void EEVEE_occlusion_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.gtao_sh);
	DRW_SHADER_FREE_SAFE(e_data.gtao_layer_sh);
	DRW_SHADER_FREE_SAFE(e_data.gtao_debug_sh);
}
