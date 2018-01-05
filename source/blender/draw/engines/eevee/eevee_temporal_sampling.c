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

/** \file eevee_temporal_sampling.c
 *  \ingroup draw_engine
 *
 * Temporal super sampling technique
 */

#include "DRW_render.h"

#include "BLI_rand.h"

#include "eevee_private.h"
#include "GPU_texture.h"

static struct {
	/* Temporal Anti Aliasing */
	struct GPUShader *taa_resolve_sh;
} e_data = {NULL}; /* Engine data */

extern char datatoc_effect_temporal_aa_glsl[];

static void eevee_create_shader_temporal_sampling(void)
{
	e_data.taa_resolve_sh = DRW_shader_create_fullscreen(datatoc_effect_temporal_aa_glsl, NULL);
}

int EEVEE_temporal_sampling_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;

	/* Reset for each "redraw". When rendering using ogl render,
	 * we accumulate the redraw inside the drawing loop in eevee_draw_background().
	 * But we do NOT accumulate between "redraw" (as in full draw manager drawloop)
	 * because the opengl render already does that. */
	effects->taa_render_sample = 1;

	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);

	if ((BKE_collection_engine_property_value_get_int(props, "taa_samples") != 1 &&
	    /* FIXME the motion blur camera evaluation is tagging view_updated
	     * thus making the TAA always reset and never stopping rendering. */
	    (effects->enabled_effects & EFFECT_MOTION_BLUR) == 0) ||
	    DRW_state_is_image_render())
	{
		const float *viewport_size = DRW_viewport_size_get();
		float persmat[4][4], viewmat[4][4];

		if (!e_data.taa_resolve_sh) {
			eevee_create_shader_temporal_sampling();
		}

		/* Until we support reprojection, we need to make sure
		 * that the history buffer contains correct information. */
		bool view_is_valid = stl->g_data->valid_double_buffer;

		view_is_valid = view_is_valid && (stl->g_data->view_updated == false);

		effects->taa_total_sample = BKE_collection_engine_property_value_get_int(props, "taa_samples");
		MAX2(effects->taa_total_sample, 0);

		DRW_viewport_matrix_get(persmat, DRW_MAT_PERS);
		DRW_viewport_matrix_get(viewmat, DRW_MAT_VIEW);
		DRW_viewport_matrix_get(effects->overide_winmat, DRW_MAT_WIN);
		/* The view is jittered by the oglrenderer. So avoid testing in this case. */
		if (!DRW_state_is_image_render()) {
			view_is_valid = view_is_valid && compare_m4m4(persmat, effects->prev_drw_persmat, FLT_MIN);
			copy_m4_m4(effects->prev_drw_persmat, persmat);
		}

		/* Prevent ghosting from probe data. */
		view_is_valid = view_is_valid && (effects->prev_drw_support == DRW_state_draw_support());
		effects->prev_drw_support = DRW_state_draw_support();

		if (((effects->taa_total_sample == 0) || (effects->taa_current_sample < effects->taa_total_sample)) ||
		    DRW_state_is_image_render())
		{
			if (view_is_valid) {
				/* OGL render already jitter the camera. */
				if (!DRW_state_is_image_render()) {
					effects->taa_current_sample += 1;

					double ht_point[2];
					double ht_offset[2] = {0.0, 0.0};
					unsigned int ht_primes[2] = {2, 3};

					BLI_halton_2D(ht_primes, ht_offset, effects->taa_current_sample - 1, ht_point);

					window_translate_m4(
					        effects->overide_winmat, persmat,
					        ((float)(ht_point[0]) * 2.0f - 1.0f) / viewport_size[0],
					        ((float)(ht_point[1]) * 2.0f - 1.0f) / viewport_size[1]);

					mul_m4_m4m4(effects->overide_persmat, effects->overide_winmat, viewmat);
					invert_m4_m4(effects->overide_persinv, effects->overide_persmat);
					invert_m4_m4(effects->overide_wininv, effects->overide_winmat);

					DRW_viewport_matrix_override_set(effects->overide_persmat, DRW_MAT_PERS);
					DRW_viewport_matrix_override_set(effects->overide_persinv, DRW_MAT_PERSINV);
					DRW_viewport_matrix_override_set(effects->overide_winmat, DRW_MAT_WIN);
					DRW_viewport_matrix_override_set(effects->overide_wininv, DRW_MAT_WININV);
				}
			}
			else {
				effects->taa_current_sample = 1;
			}
		}
		else {
			effects->taa_current_sample = 1;
		}

		DRWFboTexture tex_double_buffer = {&txl->depth_double_buffer, DRW_TEX_DEPTH_24_STENCIL_8, 0};

		DRW_framebuffer_init(&fbl->depth_double_buffer_fb, &draw_engine_eevee_type,
		                    (int)viewport_size[0], (int)viewport_size[1],
		                    &tex_double_buffer, 1);

		return EFFECT_TAA | EFFECT_DOUBLE_BUFFER | EFFECT_POST_BUFFER;
	}

	/* Cleanup to release memory */
	DRW_TEXTURE_FREE_SAFE(txl->depth_double_buffer);
	DRW_FRAMEBUFFER_FREE_SAFE(fbl->depth_double_buffer_fb);

	return 0;
}

void EEVEE_temporal_sampling_cache_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if ((effects->enabled_effects & EFFECT_TAA) != 0) {
		psl->taa_resolve = DRW_pass_create("Temporal AA Resolve", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.taa_resolve_sh, psl->taa_resolve);

		DRW_shgroup_uniform_buffer(grp, "historyBuffer", &txl->color_double_buffer);
		DRW_shgroup_uniform_buffer(grp, "colorBuffer", &txl->color);
		DRW_shgroup_uniform_float(grp, "alpha", &effects->taa_alpha, 1);
		DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
	}
}

void EEVEE_temporal_sampling_draw(EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if ((effects->enabled_effects & EFFECT_TAA) != 0) {
		if (effects->taa_current_sample != 1) {
			if (DRW_state_is_image_render()) {
				/* See EEVEE_temporal_sampling_init() for more details. */
				effects->taa_alpha = 1.0f / (float)(effects->taa_render_sample);
			}
			else {
				effects->taa_alpha = 1.0f / (float)(effects->taa_current_sample);
			}

			DRW_framebuffer_bind(fbl->effect_fb);
			DRW_draw_pass(psl->taa_resolve);

			/* Restore the depth from sample 1. */
			DRW_framebuffer_blit(fbl->depth_double_buffer_fb, fbl->main, true, false);

			/* Special Swap */
			SWAP(struct GPUFrameBuffer *, fbl->effect_fb, fbl->double_buffer);
			SWAP(GPUTexture *, txl->color_post, txl->color_double_buffer);
			effects->swap_double_buffer = false;
			effects->source_buffer = txl->color_double_buffer;
			effects->target_buffer = fbl->main;
		}
		else {
			/* Save the depth buffer for the next frame.
			 * This saves us from doing anything special
			 * in the other mode engines. */
			DRW_framebuffer_blit(fbl->main, fbl->depth_double_buffer_fb, true, false);
		}

		/* Make each loop count when doing a render. */
		if (DRW_state_is_image_render()) {
			effects->taa_render_sample += 1;
			effects->taa_current_sample += 1;
		}
		else {
			if ((effects->taa_total_sample == 0) ||
			    (effects->taa_current_sample < effects->taa_total_sample))
			{
				DRW_viewport_request_redraw();
			}
		}
	}

}

void EEVEE_temporal_sampling_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.taa_resolve_sh);
}
