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

/** \file workbench_effect_taa.c
 *  \ingroup draw_engine
 */


#include "workbench_private.h"
#include "BLI_jitter_2d.h"

static struct {
	struct GPUShader *effect_taa_sh;
	float jitter_8[8][2];
	float jitter_16[16][2];
	float jitter_32[32][2];
} e_data = {NULL};

extern char datatoc_workbench_effect_taa_frag_glsl[];


static void workbench_taa_jitter_init_order(float (*table)[2], int num)
{
	BLI_jitter_init(table, num);

	/* find closest element to center */
	int closest_index = 0;
	float closest_squared_distance = 1.0f;

	for (int index = 0; index < num; index++) {
		const float squared_dist = SQUARE(table[index][0]) + SQUARE(table[index][1]);
		if (squared_dist < closest_squared_distance) {
			closest_squared_distance = squared_dist;
			closest_index = index;
		}
	}

	/* move jitter table so that closest sample is in center */
	for (int index = 0; index < num; index++) {
		sub_v2_v2(table[index], table[closest_index]);
		mul_v2_fl(table[index], 2.0f);
	}

	/* swap center sample to the start of the table */
	if (closest_index != 0) {
		swap_v2_v2(table[0], table[closest_index]);
	}

	/* sort list based on furtest distance with previous */
	for (int i = 0; i < num - 2; i++) {
		float f_squared_dist = 0.0;
		int f_index = i;
		for (int j = i + 1; j < num; j++) {
			const float squared_dist = SQUARE(table[i][0] - table[j][0]) + SQUARE(table[i][1] - table[j][1]);
			if (squared_dist > f_squared_dist) {
				f_squared_dist = squared_dist;
				f_index = j;
			}
		}
		swap_v2_v2(table[i + 1], table[f_index]);
	}
}


static void workbench_taa_jitter_init(void)
{
	workbench_taa_jitter_init_order(e_data.jitter_8, 8);
	workbench_taa_jitter_init_order(e_data.jitter_16, 16);
	workbench_taa_jitter_init_order(e_data.jitter_32, 32);
}

int workbench_taa_calculate_num_iterations(WORKBENCH_Data *vedata)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	int result = 1;
	if (TAA_ENABLED(wpd)) {
		if (DRW_state_is_image_render()) {
			const Scene *scene = DRW_context_state_get()->scene;
			result = (scene->r.mode & R_OSA) ? scene->r.osa : 1;
		}
		else if (IN_RANGE_INCL(
		            wpd->user_preferences->gpu_viewport_quality,
		            GPU_VIEWPORT_QUALITY_TAA8, GPU_VIEWPORT_QUALITY_TAA16))
		{
			result = 8;
		}
		else if (IN_RANGE_INCL(
		            wpd->user_preferences->gpu_viewport_quality,
		            GPU_VIEWPORT_QUALITY_TAA16, GPU_VIEWPORT_QUALITY_TAA32))
		{
			result = 16;
		}
		else {
			result = 32;
		}
	}
	return result;
}

void workbench_taa_engine_init(WORKBENCH_Data *vedata)
{
	WORKBENCH_EffectInfo *effect_info = vedata->stl->effects;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	RegionView3D *rv3d = draw_ctx->rv3d;

	if (e_data.effect_taa_sh == NULL) {
		e_data.effect_taa_sh = DRW_shader_create_fullscreen(datatoc_workbench_effect_taa_frag_glsl, NULL);
		workbench_taa_jitter_init();
	}

	/* reset complete drawing when navigating. */
	if (effect_info->jitter_index != 0) {
		if (rv3d && rv3d->rflag & RV3D_NAVIGATING) {
			effect_info->jitter_index = 0;
		}
	}

	if (effect_info->view_updated) {
		effect_info->jitter_index = 0;
		effect_info->view_updated = false;
	}

	{
		float view[4][4];
		float win[4][4];
		DRW_viewport_matrix_get(view, DRW_MAT_VIEW);
		DRW_viewport_matrix_get(win, DRW_MAT_WIN);
		mul_m4_m4m4(effect_info->curr_mat, view, win);
		if (!equals_m4m4(effect_info->curr_mat, effect_info->last_mat)) {
			effect_info->jitter_index = 0;
		}
	}
}

void workbench_taa_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.effect_taa_sh);
}


DRWPass *workbench_taa_create_pass(WORKBENCH_Data *vedata, GPUTexture **color_buffer_tx)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_TextureList *txl = vedata->txl;
	WORKBENCH_EffectInfo *effect_info = stl->effects;
	WORKBENCH_FramebufferList *fbl = vedata->fbl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	/*
	 * jitter_index is not updated yet. This will be done in during draw phase.
	 * so for now it is inversed.
	 */
	int previous_jitter_index = effect_info->jitter_index;

	{
		DRW_texture_ensure_fullscreen_2D(&txl->history_buffer_tx, GPU_RGBA16F, 0);
		DRW_texture_ensure_fullscreen_2D(&txl->depth_buffer_tx, GPU_DEPTH24_STENCIL8, 0);
	}

	{
		GPU_framebuffer_ensure_config(&fbl->effect_taa_fb, {
			GPU_ATTACHMENT_NONE,
			GPU_ATTACHMENT_TEXTURE(txl->history_buffer_tx),
		});
		GPU_framebuffer_ensure_config(&fbl->depth_buffer_fb, {
			GPU_ATTACHMENT_TEXTURE(txl->depth_buffer_tx),
		});
	}


	DRWPass *pass = DRW_pass_create("Effect TAA", DRW_STATE_WRITE_COLOR);
	DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_taa_sh, pass);
	DRW_shgroup_uniform_texture_ref(grp, "colorBuffer", color_buffer_tx);
	DRW_shgroup_uniform_texture_ref(grp, "historyBuffer", &txl->history_buffer_tx);
	DRW_shgroup_uniform_float(grp, "mixFactor", &effect_info->taa_mix_factor, 1);
	DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);

	/*
	 * Set the offset for the cavity shader so every iteration different
	 * samples will be selected
	 */
	wpd->ssao_params[3] = previous_jitter_index;

	return pass;
}

void workbench_taa_draw_scene_start(WORKBENCH_Data *vedata)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_EffectInfo *effect_info = stl->effects;
	const float *viewport_size = DRW_viewport_size_get();
	int num_samples = 8;
	float (*samples)[2];
	float mix_factor;

	num_samples = workbench_taa_calculate_num_iterations(vedata);
	switch (num_samples) {
		default:
		case 8:
			samples = e_data.jitter_8;
			break;
		case 16:
			samples = e_data.jitter_16;
			break;
		case 32:
			samples = e_data.jitter_32;
			break;
	}

	mix_factor = 1.0f / (effect_info->jitter_index + 1);

	const int bitmask = num_samples - 1;
	const int jitter_index = effect_info->jitter_index;
	const float *transform_offset = samples[jitter_index];
	effect_info->jitter_index = (jitter_index + 1) & bitmask;

	/* construct new matrices from transform delta */
	float viewmat[4][4];
	float persmat[4][4];
	DRW_viewport_matrix_get(viewmat, DRW_MAT_VIEW);
	DRW_viewport_matrix_get(persmat, DRW_MAT_PERS);
	DRW_viewport_matrix_get(effect_info->override_winmat, DRW_MAT_WIN);

	window_translate_m4(
	        effect_info->override_winmat, persmat,
	        transform_offset[0] / viewport_size[0],
	        transform_offset[1] / viewport_size[1]);

	mul_m4_m4m4(effect_info->override_persmat, effect_info->override_winmat, viewmat);
	invert_m4_m4(effect_info->override_persinv, effect_info->override_persmat);
	invert_m4_m4(effect_info->override_wininv, effect_info->override_winmat);

	DRW_viewport_matrix_override_set(effect_info->override_persmat, DRW_MAT_PERS);
	DRW_viewport_matrix_override_set(effect_info->override_persinv, DRW_MAT_PERSINV);
	DRW_viewport_matrix_override_set(effect_info->override_winmat,  DRW_MAT_WIN);
	DRW_viewport_matrix_override_set(effect_info->override_wininv,  DRW_MAT_WININV);

	/* weight the mix factor by the jitter index */
	effect_info->taa_mix_factor = mix_factor;
}

void workbench_taa_draw_scene_end(WORKBENCH_Data *vedata)
{
	/*
	 * If first frame than the offset is 0.0 and its depth is the depth buffer to use
	 * for the rest of the draw engines. We store it in a persistent buffer.
	 *
	 * If it is not the first frame we copy the persistent buffer back to the
	 * default depth buffer
	 */
	const WORKBENCH_StorageList *stl = vedata->stl;
	const WORKBENCH_FramebufferList *fbl = vedata->fbl;
	const DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	WORKBENCH_EffectInfo *effect_info = stl->effects;

	if (effect_info->jitter_index == 1) {
		GPU_framebuffer_blit(dfbl->depth_only_fb, 0, fbl->depth_buffer_fb, 0, GPU_DEPTH_BIT);
	}
	else {
		GPU_framebuffer_blit(fbl->depth_buffer_fb, 0, dfbl->depth_only_fb, 0, GPU_DEPTH_BIT);
	}

	GPU_framebuffer_blit(dfbl->color_only_fb, 0, fbl->effect_taa_fb, 0, GPU_COLOR_BIT);

	if (!DRW_state_is_image_render()) {
		DRW_viewport_matrix_override_unset_all();
	}

	copy_m4_m4(effect_info->last_mat, effect_info->curr_mat);
	if (effect_info->jitter_index != 0 && !DRW_state_is_image_render()) {
		DRW_viewport_request_redraw();
	}
}

void workbench_taa_view_updated(WORKBENCH_Data *vedata)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	if (stl) {
		WORKBENCH_EffectInfo *effect_info = stl->effects;
		if (effect_info) {
			effect_info->view_updated = true;
		}
	}
}
