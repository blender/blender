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

/** \file workbench_effect_aa.c
 *  \ingroup draw_engine
 */

#include "workbench_private.h"


void workbench_aa_create_pass(WORKBENCH_Data *vedata, GPUTexture **tx)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_EffectInfo *effect_info = stl->effects;

	if (TAA_ENABLED(wpd)) {
		psl->effect_aa_pass = workbench_taa_create_pass(vedata, tx);
	}
	else if (FXAA_ENABLED(wpd)) {
		psl->effect_aa_pass = workbench_fxaa_create_pass(tx);
		effect_info->jitter_index = 0;
	}
	else {
		psl->effect_aa_pass = NULL;
	}
}

static void workspace_aa_draw_transform(GPUTexture *tx)
{
	if (DRW_state_is_image_render()) {
		/* Linear result for render. */
		DRW_transform_none(tx);
	}
	else {
		/* Display space result for viewport. */
		DRW_transform_to_display(tx);
	}
}

void workbench_aa_draw_pass(WORKBENCH_Data *vedata, GPUTexture *tx)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	WORKBENCH_FramebufferList *fbl = vedata->fbl;
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_EffectInfo *effect_info = stl->effects;

	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	if (FXAA_ENABLED(wpd)) {
		GPU_framebuffer_bind(fbl->effect_fb);
		workspace_aa_draw_transform(tx);
		GPU_framebuffer_bind(dfbl->color_only_fb);
		DRW_draw_pass(psl->effect_aa_pass);
	}
	else if (TAA_ENABLED(wpd)) {
		/*
		 * when drawing the first TAA frame, we transform directly to the
		 * color_only_fb as the TAA shader is just performing a direct copy.
		 * the workbench_taa_draw_screen_end will fill the history buffer
		 * for the other iterations.
		 */
		if (effect_info->jitter_index == 1) {
			GPU_framebuffer_bind(dfbl->color_only_fb);
			workspace_aa_draw_transform(tx);
		}
		else {
			GPU_framebuffer_bind(fbl->effect_fb);
			workspace_aa_draw_transform(tx);
			GPU_framebuffer_bind(dfbl->color_only_fb);
			DRW_draw_pass(psl->effect_aa_pass);
		}
		workbench_taa_draw_scene_end(vedata);
	}
	else {
		GPU_framebuffer_bind(dfbl->color_only_fb);
		workspace_aa_draw_transform(tx);
	}
}
