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

/* Gather all screen space effects technique such as Bloom, Motion Blur, DoF, SSAO, SSR, ...
 */

/** \file eevee_effects.c
 *  \ingroup draw_engine
 */

#include "DRW_render.h"

#include "DNA_anim_types.h"
#include "DNA_view3d_types.h"

#include "BKE_object.h"
#include "BKE_animsys.h"

#include "eevee_private.h"
#include "GPU_texture.h"

typedef struct EEVEE_ProbeData {
	short probe_id, shadow_id;
} EEVEE_ProbeData;

/* TODO Option */
#define ENABLE_EFFECT_MOTION_BLUR 1

static struct {
	struct GPUShader *motion_blur_sh;
	struct GPUShader *tonemap_sh;
} e_data = {NULL}; /* Engine data */

extern char datatoc_effect_motion_blur_frag_glsl[];
extern char datatoc_tonemap_frag_glsl[];

void EEVEE_effects_init(EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;

	/* Ping Pong buffer */
	DRWFboTexture tex = {&txl->color_post, DRW_BUF_RGBA_16, DRW_TEX_FILTER};

	const float *viewport_size = DRW_viewport_size_get();
	DRW_framebuffer_init(&fbl->effect_fb,
	                    (int)viewport_size[0], (int)viewport_size[1],
	                    &tex, 1);

	if (!e_data.motion_blur_sh) {
		e_data.motion_blur_sh = DRW_shader_create_fullscreen(datatoc_effect_motion_blur_frag_glsl, NULL);
	}

	if (!e_data.tonemap_sh) {
		e_data.tonemap_sh = DRW_shader_create_fullscreen(datatoc_tonemap_frag_glsl, NULL);
	}

	if (!stl->effects) {
		stl->effects = MEM_callocN(sizeof(EEVEE_EffectsInfo), "EEVEE_EffectsInfo");
	}

	{
		/* Update Motion Blur Matrices */
		EEVEE_EffectsInfo *effects = stl->effects;
#if ENABLE_EFFECT_MOTION_BLUR
		const DRWContextState *draw_ctx = DRW_context_state_get();
		Scene *scene = draw_ctx->scene;
		View3D *v3d = draw_ctx->v3d;
		RegionView3D *rv3d = draw_ctx->rv3d;

		if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
			float ctime = BKE_scene_frame_get(scene);
			float past_obmat[4][4], future_obmat[4][4], winmat[4][4];

			DRW_viewport_matrix_get(winmat, DRW_MAT_WIN);

			/* HACK */
			Object cam_cpy;
			memcpy(&cam_cpy, v3d->camera, sizeof(cam_cpy));

			/* Past matrix */
			/* FIXME : This is a temporal solution that does not take care of parent animations */
			/* Recalc Anim manualy */
			BKE_animsys_evaluate_animdata(scene, &cam_cpy.id, cam_cpy.adt, ctime - 1.0, ADT_RECALC_ANIM);
			BKE_object_where_is_calc_time(scene, &cam_cpy, ctime - 1.0);

			normalize_m4_m4(past_obmat, cam_cpy.obmat);
			invert_m4(past_obmat);
			mul_m4_m4m4(effects->past_world_to_ndc, winmat, past_obmat);

#if 0       /* for future high quality blur */
			/* Future matrix */
			/* Recalc Anim manualy */
			BKE_animsys_evaluate_animdata(scene, &cam_cpy.id, cam_cpy.adt, ctime + 1.0, ADT_RECALC_ANIM);
			BKE_object_where_is_calc_time(scene, &cam_cpy, ctime + 1.0);

			normalize_m4_m4(past_obmat, cam_cpy.obmat);
			invert_m4(past_obmat);
			mul_m4_m4m4(effects->past_world_to_ndc, winmat, past_obmat);
#else
			UNUSED_VARS(future_obmat);
#endif

			/* Current matrix */
			DRW_viewport_matrix_get(effects->current_ndc_to_world, DRW_MAT_PERSINV);

			effects->blur_amount = 0.5f;
			effects->final_color = txl->color_post;
			effects->enabled_effects |= EFFECT_MOTION_BLUR;
		}
		else {
#endif /* ENABLE_EFFECT_MOTION_BLUR */
			effects->blur_amount = 0.0f;
			effects->final_color = txl->color;
			effects->enabled_effects &= ~EFFECT_MOTION_BLUR;
#if ENABLE_EFFECT_MOTION_BLUR
		}
#endif
	}
}

void EEVEE_effects_cache_init(EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	struct Batch *quad = DRW_cache_fullscreen_quad_get();

	{
		psl->motion_blur = DRW_pass_create("Motion Blur", DRW_STATE_WRITE_COLOR);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.motion_blur_sh, psl->motion_blur);
		DRW_shgroup_uniform_float(grp, "blurAmount", &effects->blur_amount, 1);
		DRW_shgroup_uniform_mat4(grp, "currInvViewProjMatrix", (float *)effects->current_ndc_to_world);
		DRW_shgroup_uniform_mat4(grp, "pastViewProjMatrix", (float *)effects->past_world_to_ndc);
		DRW_shgroup_uniform_buffer(grp, "colorBuffer", &txl->color, 0);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &dtxl->depth, 1);
		DRW_shgroup_call_add(grp, quad, NULL);
	}

	{
		/* Final pass : Map HDR color to LDR color.
		 * Write result to the default color buffer */
		psl->tonemap = DRW_pass_create("Tone Mapping", DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.tonemap_sh, psl->tonemap);
		DRW_shgroup_uniform_buffer(grp, "hdrColorBuf", &effects->final_color, 0);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
}

void EEVEE_draw_effects(EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	/* Default framebuffer and texture */
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	if ((effects->enabled_effects & EFFECT_MOTION_BLUR) != 0) {
		/* Motion Blur */
		DRW_framebuffer_bind(fbl->effect_fb);
		DRW_draw_pass(psl->motion_blur);
	}

	/* Restore default framebuffer */
	DRW_framebuffer_texture_detach(dtxl->depth);
	DRW_framebuffer_texture_attach(dfbl->default_fb, dtxl->depth, 0, 0);
	DRW_framebuffer_bind(dfbl->default_fb);

	/* Tonemapping */
	DRW_draw_pass(psl->tonemap);
}

void EEVEE_effects_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.tonemap_sh);
	DRW_SHADER_FREE_SAFE(e_data.motion_blur_sh);
}