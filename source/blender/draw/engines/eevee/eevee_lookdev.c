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

/** \file eevee_lookdev.c
 *  \ingroup draw_engine
 */
#include "DRW_render.h"

#include "BKE_camera.h"
#include "BKE_studiolight.h"

#include "DNA_screen_types.h"
#include "DNA_world_types.h"

#include "ED_screen.h"

#include "eevee_private.h"

void EEVEE_lookdev_cache_init(
        EEVEE_Data *vedata, DRWShadingGroup **grp, GPUShader *shader, DRWPass *pass,
        World *world, EEVEE_LightProbesInfo *pinfo)
{
	EEVEE_StorageList *stl = vedata->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	View3D *v3d = draw_ctx->v3d;
	if (LOOK_DEV_MODE_ENABLED(v3d)) {
		StudioLight *sl = BKE_studiolight_find(v3d->shading.studio_light, STUDIOLIGHT_ORIENTATION_WORLD);
		if ((sl->flag & STUDIOLIGHT_ORIENTATION_WORLD)) {
			struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();
			GPUTexture *tex;

			*grp = DRW_shgroup_create(shader, pass);
			axis_angle_to_mat3_single(stl->g_data->studiolight_matrix, 'Z', v3d->shading.studiolight_rot_z);
			DRW_shgroup_uniform_mat3(*grp, "StudioLightMatrix", stl->g_data->studiolight_matrix);

			DRW_shgroup_uniform_vec3(*grp, "color", &world->horr, 1);
			DRW_shgroup_uniform_float(*grp, "backgroundAlpha", &stl->g_data->background_alpha, 1);
			DRW_shgroup_call_add(*grp, geom, NULL);
			if (!pinfo) {
				/* Do not fadeout when doing probe rendering, only when drawing the background */
				DRW_shgroup_uniform_float(*grp, "studioLightBackground", &v3d->shading.studiolight_background, 1);

				BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EQUIRECTANGULAR_IRRADIANCE_GPUTEXTURE);
				tex = sl->equirectangular_irradiance_gputexture;
			}
			else {
				BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EQUIRECTANGULAR_RADIANCE_GPUTEXTURE);
				tex = sl->equirectangular_radiance_gputexture;
			}
			DRW_shgroup_uniform_texture(*grp, "image", tex);

			/* Do we need to recalc the lightprobes? */
			if (pinfo &&
			    ((pinfo->studiolight_index != sl->index) ||
			     (pinfo->studiolight_rot_z != v3d->shading.studiolight_rot_z)))
			{
				pinfo->update_world |= PROBE_UPDATE_ALL;
				pinfo->studiolight_index = sl->index;
				pinfo->studiolight_rot_z = v3d->shading.studiolight_rot_z;
				pinfo->prev_wo_sh_compiled = false;
				pinfo->prev_world = NULL;
			}
		}
	}
}

void EEVEE_lookdev_draw_background(EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
	EEVEE_EffectsInfo *effects = stl->effects;
	EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

	const DRWContextState *draw_ctx = DRW_context_state_get();

	if (psl->lookdev_pass && LOOK_DEV_OVERLAY_ENABLED(draw_ctx->v3d)) {
		DRW_stats_group_start("Look Dev");
		CameraParams params;
		BKE_camera_params_init(&params);
		View3D *v3d = draw_ctx->v3d;
		RegionView3D *rv3d = draw_ctx->rv3d;
		ARegion *ar = draw_ctx->ar;

		BKE_camera_params_from_view3d(&params, draw_ctx->depsgraph, v3d, rv3d);
		params.is_ortho = true;
		params.ortho_scale = 3.0f;
		params.zoom = CAMERA_PARAM_ZOOM_INIT_PERSP;
		params.offsetx = 0.0f;
		params.offsety = 0.0f;
		params.shiftx = 0.0f;
		params.shifty = 0.0f;
		params.clipsta = 0.001f;
		params.clipend = 20.0f;
		BKE_camera_params_compute_viewplane(&params, ar->winx, ar->winy, 1.0f, 1.0f);
		BKE_camera_params_compute_matrix(&params);

		const float *viewport_size = DRW_viewport_size_get();
		rcti rect;
		ED_region_visible_rect(draw_ctx->ar, &rect);
		int viewport_inset_x = viewport_size[0] / 4;
		int viewport_inset_y = viewport_size[1] / 4;

		EEVEE_CommonUniformBuffer *common = &sldata->common_data;
		common->la_num_light = 0;
		common->prb_num_planar = 0;
		common->prb_num_render_cube = 1;
		common->prb_num_render_grid = 1;
		common->ao_dist = 0.0f;
		common->ao_factor = 0.0f;
		common->ao_settings = 0.0f;
		DRW_uniformbuffer_update(sldata->common_ubo, common);

		/* override matrices */
		float winmat[4][4];
		float winmat_inv[4][4];
		copy_m4_m4(winmat, params.winmat);
		invert_m4_m4(winmat_inv, winmat);
		DRW_viewport_matrix_override_set(winmat, DRW_MAT_WIN);
		DRW_viewport_matrix_override_set(winmat_inv, DRW_MAT_WININV);
		float viewmat[4][4];
		DRW_viewport_matrix_get(viewmat, DRW_MAT_VIEW);
		float persmat[4][4];
		float persmat_inv[4][4];
		mul_m4_m4m4(persmat, winmat, viewmat);
		invert_m4_m4(persmat_inv, persmat);
		DRW_viewport_matrix_override_set(persmat, DRW_MAT_PERS);
		DRW_viewport_matrix_override_set(persmat_inv, DRW_MAT_PERSINV);

		GPUFrameBuffer *fb = effects->final_fb;
		GPU_framebuffer_bind(fb);
		GPU_framebuffer_viewport_set(fb, rect.xmax - viewport_inset_x, 0, viewport_inset_x, viewport_inset_y);
		DRW_draw_pass(psl->lookdev_pass);

		fb = dfbl->depth_only_fb;
		GPU_framebuffer_bind(fb);
		GPU_framebuffer_viewport_set(fb, rect.xmax - viewport_inset_x, 0, viewport_inset_x, viewport_inset_y);
		DRW_draw_pass(psl->lookdev_pass);

		DRW_viewport_matrix_override_unset_all();
		DRW_stats_group_end();
	}
}
