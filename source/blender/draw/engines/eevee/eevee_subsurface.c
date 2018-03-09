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

/** \file eevee_subsurface.c
 *  \ingroup draw_engine
 *
 * Screen space subsurface scattering technique.
 */

#include "DRW_render.h"

#include "BLI_string_utils.h"

#include "eevee_private.h"
#include "GPU_texture.h"

static struct {
	struct GPUShader *sss_sh[4];
} e_data = {NULL}; /* Engine data */

extern char datatoc_common_view_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_effect_subsurface_frag_glsl[];

static void eevee_create_shader_subsurface(void)
{
	char *frag_str = BLI_string_joinN(
	        datatoc_common_view_lib_glsl,
	        datatoc_common_uniforms_lib_glsl,
	        datatoc_effect_subsurface_frag_glsl);

	e_data.sss_sh[0] = DRW_shader_create_fullscreen(frag_str, "#define FIRST_PASS\n");
	e_data.sss_sh[1] = DRW_shader_create_fullscreen(frag_str, "#define SECOND_PASS\n");
	e_data.sss_sh[2] = DRW_shader_create_fullscreen(frag_str, "#define SECOND_PASS\n"
	                                                          "#define USE_SEP_ALBEDO\n");
	e_data.sss_sh[3] = DRW_shader_create_fullscreen(frag_str, "#define SECOND_PASS\n"
	                                                          "#define USE_SEP_ALBEDO\n"
	                                                          "#define RESULT_ACCUM\n");

	MEM_freeN(frag_str);
}

int EEVEE_subsurface_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	const float *viewport_size = DRW_viewport_size_get();

	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);

	if (BKE_collection_engine_property_value_get_bool(props, "sss_enable")) {
		effects->sss_sample_count = 1 + BKE_collection_engine_property_value_get_int(props, "sss_samples") * 2;
		effects->sss_separate_albedo = BKE_collection_engine_property_value_get_bool(props, "sss_separate_albedo");
		common_data->sss_jitter_threshold = BKE_collection_engine_property_value_get_float(props, "sss_jitter_threshold");

		/* Force separate albedo for final render */
		if (DRW_state_is_image_render()) {
			effects->sss_separate_albedo = true;
		}

		/* Shaders */
		if (!e_data.sss_sh[0]) {
			eevee_create_shader_subsurface();
		}

		/* NOTE : we need another stencil because the stencil buffer is on the same texture
		 * as the depth buffer we are sampling from. This could be avoided if the stencil is
		 * a separate texture but that needs OpenGL 4.4 or ARB_texture_stencil8.
		 * OR OpenGL 4.3 / ARB_ES3_compatibility if using a renderbuffer instead */
		DRWFboTexture texs[2] = {{&txl->sss_stencil, DRW_TEX_DEPTH_24_STENCIL_8, 0},
		                         {&txl->sss_blur, DRW_TEX_RGBA_16, DRW_TEX_FILTER}};

		DRW_framebuffer_init(&fbl->sss_blur_fb, &draw_engine_eevee_type, (int)viewport_size[0], (int)viewport_size[1],
		                     texs, 2);

		DRWFboTexture tex_data = {&txl->sss_data, DRW_TEX_RGBA_16, DRW_TEX_FILTER};
		DRW_framebuffer_init(&fbl->sss_clear_fb, &draw_engine_eevee_type, (int)viewport_size[0], (int)viewport_size[1],
		                     &tex_data, 1);

		if (effects->sss_separate_albedo) {
			if (txl->sss_albedo == NULL) {
				txl->sss_albedo = DRW_texture_create_2D((int)viewport_size[0], (int)viewport_size[1],
				                                        DRW_TEX_RGB_11_11_10, 0, NULL);
			}
		}
		else {
			/* Cleanup to release memory */
			DRW_TEXTURE_FREE_SAFE(txl->sss_albedo);
		}
		return EFFECT_SSS;
	}

	/* Cleanup to release memory */
	DRW_TEXTURE_FREE_SAFE(txl->sss_albedo);
	DRW_TEXTURE_FREE_SAFE(txl->sss_data);
	DRW_TEXTURE_FREE_SAFE(txl->sss_blur);
	DRW_TEXTURE_FREE_SAFE(txl->sss_stencil);
	DRW_FRAMEBUFFER_FREE_SAFE(fbl->sss_blur_fb);
	DRW_FRAMEBUFFER_FREE_SAFE(fbl->sss_clear_fb);

	return 0;
}

static void set_shgrp_stencil(void *UNUSED(userData), DRWShadingGroup *shgrp)
{
	DRW_shgroup_stencil_mask(shgrp, 255);
}

void EEVEE_subsurface_output_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	const float *viewport_size = DRW_viewport_size_get();

	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);

	if (BKE_collection_engine_property_value_get_bool(props, "sss_enable")) {
		float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

		DRWFboTexture tex_data[2] = {{&txl->sss_dir_accum, DRW_TEX_RGBA_16, 0},
		                             {&txl->sss_col_accum, DRW_TEX_RGBA_16, 0}};
		DRW_framebuffer_init(&fbl->sss_accum_fb, &draw_engine_eevee_type, (int)viewport_size[0], (int)viewport_size[1],
		                     tex_data, 2);

		/* Clear texture. */
		DRW_framebuffer_bind(fbl->sss_accum_fb);
		DRW_framebuffer_clear(true, false, false, clear, 0.0f);

		/* Make the opaque refraction pass mask the sss. */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES |
		                 DRW_STATE_WIRE | DRW_STATE_WRITE_STENCIL;
		DRW_pass_state_set(vedata->psl->refract_pass, state);
		DRW_pass_foreach_shgroup(vedata->psl->refract_pass, &set_shgrp_stencil, NULL);
	}
	else {
		/* Cleanup to release memory */
		DRW_TEXTURE_FREE_SAFE(txl->sss_dir_accum);
		DRW_TEXTURE_FREE_SAFE(txl->sss_col_accum);
		DRW_FRAMEBUFFER_FREE_SAFE(fbl->sss_accum_fb);
	}
}

void EEVEE_subsurface_cache_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if ((effects->enabled_effects & EFFECT_SSS) != 0) {
		/** Screen Space SubSurface Scattering overview
		 * TODO
		 */
		psl->sss_blur_ps = DRW_pass_create("Blur Horiz", DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_EQUAL);

		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE | DRW_STATE_STENCIL_EQUAL;
		psl->sss_resolve_ps = DRW_pass_create("Blur Vert", state);
		psl->sss_accum_ps = DRW_pass_create("Resolve Accum", state);
	}
}

void EEVEE_subsurface_add_pass(
        EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, unsigned int sss_id, struct GPUUniformBuffer *sss_profile)
{
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;
	struct Gwn_Batch *quad = DRW_cache_fullscreen_quad_get();

	DRWShadingGroup *grp = DRW_shgroup_create(e_data.sss_sh[0], psl->sss_blur_ps);
	DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
	DRW_shgroup_uniform_buffer(grp, "depthBuffer", &dtxl->depth);
	DRW_shgroup_uniform_buffer(grp, "sssData", &txl->sss_data);
	DRW_shgroup_uniform_block(grp, "sssProfile", sss_profile);
	DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
	DRW_shgroup_stencil_mask(grp, sss_id);
	DRW_shgroup_call_add(grp, quad, NULL);

	struct GPUShader *sh = (effects->sss_separate_albedo) ? e_data.sss_sh[2] : e_data.sss_sh[1];
	grp = DRW_shgroup_create(sh, psl->sss_resolve_ps);
	DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
	DRW_shgroup_uniform_buffer(grp, "depthBuffer", &dtxl->depth);
	DRW_shgroup_uniform_buffer(grp, "sssData", &txl->sss_blur);
	DRW_shgroup_uniform_block(grp, "sssProfile", sss_profile);
	DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
	DRW_shgroup_stencil_mask(grp, sss_id);
	DRW_shgroup_call_add(grp, quad, NULL);

	if (effects->sss_separate_albedo) {
		DRW_shgroup_uniform_buffer(grp, "sssAlbedo", &txl->sss_albedo);
	}

	if (DRW_state_is_image_render()) {
		grp = DRW_shgroup_create(e_data.sss_sh[3], psl->sss_accum_ps);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &dtxl->depth);
		DRW_shgroup_uniform_buffer(grp, "sssData", &txl->sss_blur);
		DRW_shgroup_uniform_buffer(grp, "sssAlbedo", &txl->sss_albedo);
		DRW_shgroup_uniform_block(grp, "sssProfile", sss_profile);
		DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
		DRW_shgroup_stencil_mask(grp, sss_id);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
}

void EEVEE_subsurface_data_render(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if ((effects->enabled_effects & EFFECT_SSS) != 0) {
		float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		/* Clear sss_data texture only... can this be done in a more clever way? */
		DRW_framebuffer_bind(fbl->sss_clear_fb);
		DRW_framebuffer_clear(true, false, false, clear, 0.0f);


		DRW_framebuffer_texture_detach(txl->sss_data);
		if ((effects->enabled_effects & EFFECT_NORMAL_BUFFER) != 0) {
			DRW_framebuffer_texture_detach(txl->ssr_normal_input);
		}
		if ((effects->enabled_effects & EFFECT_SSR) != 0) {
			DRW_framebuffer_texture_detach(txl->ssr_specrough_input);
		}

		/* Start at slot 1 because slot 0 is txl->color */
		int tex_slot = 1;
		DRW_framebuffer_texture_attach(fbl->main, txl->sss_data, tex_slot++, 0);
		if (effects->sss_separate_albedo) {
			DRW_framebuffer_texture_attach(fbl->main, txl->sss_albedo, tex_slot++, 0);
		}
		if ((effects->enabled_effects & EFFECT_NORMAL_BUFFER) != 0) {
			DRW_framebuffer_texture_attach(fbl->main, txl->ssr_normal_input, tex_slot++, 0);
		}
		if ((effects->enabled_effects & EFFECT_SSR) != 0) {
			DRW_framebuffer_texture_attach(fbl->main, txl->ssr_specrough_input, tex_slot++, 0);
		}
		DRW_framebuffer_bind(fbl->main);

		DRW_draw_pass(psl->sss_pass);

		/* Restore */
		DRW_framebuffer_texture_detach(txl->sss_data);
		if (effects->sss_separate_albedo) {
			DRW_framebuffer_texture_detach(txl->sss_albedo);
		}
		if ((effects->enabled_effects & EFFECT_NORMAL_BUFFER) != 0) {
			DRW_framebuffer_texture_detach(txl->ssr_normal_input);
		}
		if ((effects->enabled_effects & EFFECT_SSR) != 0) {
			DRW_framebuffer_texture_detach(txl->ssr_specrough_input);
		}

		DRW_framebuffer_texture_attach(fbl->sss_clear_fb, txl->sss_data, 0, 0);
		if ((effects->enabled_effects & EFFECT_NORMAL_BUFFER) != 0) {
			DRW_framebuffer_texture_attach(fbl->main, txl->ssr_normal_input, 1, 0);
		}
		if ((effects->enabled_effects & EFFECT_SSR) != 0) {
			DRW_framebuffer_texture_attach(fbl->main, txl->ssr_specrough_input, 2, 0);
		}
	}
}

void EEVEE_subsurface_compute(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if ((effects->enabled_effects & EFFECT_SSS) != 0) {
		float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

		DRW_stats_group_start("SSS");

		/* Copy stencil channel, could be avoided (see EEVEE_subsurface_init) */
		DRW_framebuffer_blit(fbl->main, fbl->sss_blur_fb, false, true);

		DRW_framebuffer_texture_detach(dtxl->depth);

		/* 1. horizontal pass */
		DRW_framebuffer_bind(fbl->sss_blur_fb);
		DRW_framebuffer_clear(true, false, false, clear, 0.0f);
		DRW_draw_pass(psl->sss_blur_ps);

		/* 2. vertical pass + Resolve */
		DRW_framebuffer_texture_detach(txl->sss_stencil);
		if ((effects->enabled_effects & EFFECT_NORMAL_BUFFER) != 0) {
			DRW_framebuffer_texture_detach(txl->ssr_normal_input);
		}
		if ((effects->enabled_effects & EFFECT_SSR) != 0) {
			DRW_framebuffer_texture_detach(txl->ssr_specrough_input);
		}
		DRW_framebuffer_texture_attach(fbl->main, txl->sss_stencil, 0, 0);
		DRW_framebuffer_bind(fbl->main);
		DRW_draw_pass(psl->sss_resolve_ps);

		/* Restore */
		DRW_framebuffer_texture_detach(txl->sss_stencil);
		DRW_framebuffer_texture_attach(fbl->sss_blur_fb, txl->sss_stencil, 0, 0);
		DRW_framebuffer_texture_attach(fbl->main, dtxl->depth, 0, 0);
		if ((effects->enabled_effects & EFFECT_NORMAL_BUFFER) != 0) {
			DRW_framebuffer_texture_attach(fbl->main, txl->ssr_normal_input, 1, 0);
		}
		if ((effects->enabled_effects & EFFECT_SSR) != 0) {
			DRW_framebuffer_texture_attach(fbl->main, txl->ssr_specrough_input, 2, 0);
		}

		DRW_stats_group_end();
	}
}

void EEVEE_subsurface_output_accumulate(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if (((effects->enabled_effects & EFFECT_SSS) != 0) && (fbl->sss_accum_fb != NULL)) {
		/* Copy stencil channel, could be avoided (see EEVEE_subsurface_init) */
		DRW_framebuffer_blit(fbl->main, fbl->sss_blur_fb, false, true);

		/* Only do vertical pass + Resolve */
		DRW_framebuffer_texture_detach(txl->sss_stencil);
		DRW_framebuffer_texture_attach(fbl->sss_accum_fb, txl->sss_stencil, 0, 0);
		DRW_framebuffer_bind(fbl->sss_accum_fb);
		DRW_draw_pass(psl->sss_accum_ps);

		/* Restore */
		DRW_framebuffer_texture_detach(txl->sss_stencil);
		DRW_framebuffer_texture_attach(fbl->sss_blur_fb, txl->sss_stencil, 0, 0);
		DRW_framebuffer_bind(fbl->main);
	}
}

void EEVEE_subsurface_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.sss_sh[0]);
	DRW_SHADER_FREE_SAFE(e_data.sss_sh[1]);
	DRW_SHADER_FREE_SAFE(e_data.sss_sh[2]);
	DRW_SHADER_FREE_SAFE(e_data.sss_sh[3]);
}
