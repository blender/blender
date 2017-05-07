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
#define ENABLE_EFFECT_BLOOM 1

static struct {
	struct GPUShader *motion_blur_sh;

	/* Bloom */
	struct GPUShader *bloom_blit_sh[2];
	struct GPUShader *bloom_downsample_sh[2];
	struct GPUShader *bloom_upsample_sh[2];
	struct GPUShader *bloom_resolve_sh[2];

	struct GPUShader *tonemap_sh;
} e_data = {NULL}; /* Engine data */

extern char datatoc_effect_motion_blur_frag_glsl[];
extern char datatoc_effect_bloom_frag_glsl[];
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

	if (!e_data.bloom_blit_sh[0]) {
		e_data.bloom_blit_sh[0] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl, "#define STEP_BLIT\n");
		e_data.bloom_blit_sh[1] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl, "#define STEP_BLIT\n"
		                                                                                       "#define HIGH_QUALITY\n");

		e_data.bloom_downsample_sh[0] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl, "#define STEP_DOWNSAMPLE\n");
		e_data.bloom_downsample_sh[1] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl, "#define STEP_DOWNSAMPLE\n"
		                                                                                             "#define HIGH_QUALITY\n");

		e_data.bloom_upsample_sh[0] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl, "#define STEP_UPSAMPLE\n");
		e_data.bloom_upsample_sh[1] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl, "#define STEP_UPSAMPLE\n"
		                                                                                           "#define HIGH_QUALITY\n");

		e_data.bloom_resolve_sh[0] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl, "#define STEP_RESOLVE\n");
		e_data.bloom_resolve_sh[1] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl, "#define STEP_RESOLVE\n"
		                                                                                          "#define HIGH_QUALITY\n");
	}

	if (!e_data.tonemap_sh) {
		e_data.tonemap_sh = DRW_shader_create_fullscreen(datatoc_tonemap_frag_glsl, NULL);
	}

	if (!stl->effects) {
		stl->effects = MEM_callocN(sizeof(EEVEE_EffectsInfo), "EEVEE_EffectsInfo");
		stl->effects->enabled_effects = 0;
	}

#if ENABLE_EFFECT_MOTION_BLUR
	{
		/* Update Motion Blur Matrices */
		EEVEE_EffectsInfo *effects = stl->effects;
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
			effects->enabled_effects |= EFFECT_MOTION_BLUR;
		}
	}
#endif /* ENABLE_EFFECT_MOTION_BLUR */

	{
		/* Bloom */
		EEVEE_EffectsInfo *effects = stl->effects;
		int blitsize[2], texsize[2];

		/* Blit Buffer */
		effects->source_texel_size[0] = 1.0f / viewport_size[0];
		effects->source_texel_size[1] = 1.0f / viewport_size[1];

		blitsize[0] = (int)viewport_size[0];
		blitsize[1] = (int)viewport_size[1];

		effects->blit_texel_size[0] = 1.0f / (float)blitsize[0];
		effects->blit_texel_size[1] = 1.0f / (float)blitsize[1];

		DRWFboTexture tex_blit = {&txl->bloom_blit, DRW_BUF_RGBA_16, DRW_TEX_FILTER};
		DRW_framebuffer_init(&fbl->bloom_blit_fb,
		                    (int)blitsize[0], (int)blitsize[1],
		                    &tex_blit, 1);

		/* Parameters */
		/* TODO UI Options */
		float threshold = 0.8f;
		float knee = 0.5f;
		float intensity = 0.8f;
		float radius = 8.5f;

		/* determine the iteration count */
		const float minDim = (float)MIN2(blitsize[0], blitsize[1]);
		const float maxIter = (radius - 8.0f) + log(minDim) / log(2);
		const int maxIterInt = effects->bloom_iteration_ct = (int)maxIter;

		CLAMP(effects->bloom_iteration_ct, 1, MAX_BLOOM_STEP);

		effects->bloom_sample_scale = 0.5f + maxIter - (float)maxIterInt;
		effects->bloom_curve_threshold[0] = threshold - knee;
		effects->bloom_curve_threshold[1] = knee * 2.0f;
		effects->bloom_curve_threshold[2] = 0.25f / knee;
		effects->bloom_curve_threshold[3] = threshold;
		effects->bloom_intensity = intensity;

		/* Downsample buffers */
		copy_v2_v2_int(texsize, blitsize);
		for (int i = 0; i < effects->bloom_iteration_ct; ++i) {
			texsize[0] /= 2; texsize[1] /= 2;
			texsize[0] = MAX2(texsize[0], 2);
			texsize[1] = MAX2(texsize[1], 2);

			effects->downsamp_texel_size[i][0] = 1.0f / (float)texsize[0];
			effects->downsamp_texel_size[i][1] = 1.0f / (float)texsize[1];

			DRWFboTexture tex_bloom = {&txl->bloom_downsample[i], DRW_BUF_RGBA_16, DRW_TEX_FILTER};
			DRW_framebuffer_init(&fbl->bloom_down_fb[i],
			                    (int)texsize[0], (int)texsize[1],
			                    &tex_bloom, 1);
		}

		/* Upsample buffers */
		copy_v2_v2_int(texsize, blitsize);
		for (int i = 0; i < effects->bloom_iteration_ct - 1; ++i) {
			texsize[0] /= 2; texsize[1] /= 2;
			texsize[0] = MAX2(texsize[0], 2);
			texsize[1] = MAX2(texsize[1], 2);

			DRWFboTexture tex_bloom = {&txl->bloom_upsample[i], DRW_BUF_RGBA_16, DRW_TEX_FILTER};
			DRW_framebuffer_init(&fbl->bloom_accum_fb[i],
			                    (int)texsize[0], (int)texsize[1],
			                    &tex_bloom, 1);
		}

		effects->enabled_effects |= EFFECT_BLOOM;
	}
}

static DRWShadingGroup *eevee_create_bloom_pass(const char *name, EEVEE_EffectsInfo *effects, struct GPUShader *sh, DRWPass **pass, bool upsample)
{
	struct Batch *quad = DRW_cache_fullscreen_quad_get();

	*pass = DRW_pass_create(name, DRW_STATE_WRITE_COLOR);

	DRWShadingGroup *grp = DRW_shgroup_create(sh, *pass);
	DRW_shgroup_call_add(grp, quad, NULL);
	DRW_shgroup_uniform_buffer(grp, "sourceBuffer", &effects->unf_source_buffer, 0);
	DRW_shgroup_uniform_vec2(grp, "sourceBufferTexelSize", effects->unf_source_texel_size, 1);
	if (upsample) {
		DRW_shgroup_uniform_buffer(grp, "baseBuffer", &effects->unf_base_buffer, 1);
		DRW_shgroup_uniform_float(grp, "sampleScale", &effects->bloom_sample_scale, 1);
	}

	return grp;
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
		/**  Bloom algorithm
		 *
		 * Overview :
		 * - Downsample the color buffer doing a small blur during each step.
		 * - Accumulate bloom color using previously downsampled color buffers
		 *   and do an upsample blur for each new accumulated layer.
		 * - Finally add accumulation buffer onto the source color buffer.
		 *
		 *  [1/1] is original copy resolution (can be half or quater res for performance)
		 *
		 *                                [DOWNSAMPLE CHAIN]                      [UPSAMPLE CHAIN]
		 *
		 *  Source Color ── [Blit] ──>  Bright Color Extract [1/1]                  Final Color
		 *                                        |                                      Λ
		 *                                [Downsample First]       Source Color ─> + [Resolve]
		 *                                        v                                      |
		 *                              Color Downsampled [1/2] ────────────> + Accumulation Buffer [1/2]
		 *                                        |                                      Λ
		 *                                       ───                                    ───
		 *                                      Repeat                                 Repeat
		 *                                       ───                                    ───
		 *                                        v                                      |
		 *                              Color Downsampled [1/N-1] ──────────> + Accumulation Buffer [1/N-1]
		 *                                        |                                      Λ
		 *                                   [Downsample]                            [Upsample]
		 *                                        v                                      |
		 *                              Color Downsampled [1/N] ─────────────────────────┘
		 **/
		DRWShadingGroup *grp;
		const bool use_highres = true;
		const bool use_antiflicker = true;
		eevee_create_bloom_pass("Bloom Downsample First", effects, e_data.bloom_downsample_sh[use_antiflicker], &psl->bloom_downsample_first, false);
		eevee_create_bloom_pass("Bloom Downsample", effects, e_data.bloom_downsample_sh[0], &psl->bloom_downsample, false);
		eevee_create_bloom_pass("Bloom Upsample", effects, e_data.bloom_upsample_sh[use_highres], &psl->bloom_upsample, true);
		grp = eevee_create_bloom_pass("Bloom Blit", effects, e_data.bloom_blit_sh[use_antiflicker], &psl->bloom_blit, false);
		DRW_shgroup_uniform_vec4(grp, "curveThreshold", effects->bloom_curve_threshold, 1);
		grp = eevee_create_bloom_pass("Bloom Resolve", effects, e_data.bloom_resolve_sh[use_highres], &psl->bloom_resolve, true);
		DRW_shgroup_uniform_float(grp, "bloomIntensity", &effects->bloom_intensity, 1);
	}

	{
		/* Final pass : Map HDR color to LDR color.
		 * Write result to the default color buffer */
		psl->tonemap = DRW_pass_create("Tone Mapping", DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.tonemap_sh, psl->tonemap);
		DRW_shgroup_uniform_buffer(grp, "hdrColorBuf", &effects->source_buffer, 0);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
}

/* Ping pong between 2 buffers */
static void eevee_effect_framebuffer_bind(EEVEE_Data *vedata)
{
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_EffectsInfo *effects = vedata->stl->effects;

	DRW_framebuffer_bind(effects->target_buffer);

	if (effects->source_buffer == txl->color) {
		effects->source_buffer = txl->color_post;
		effects->target_buffer = fbl->main;
	}
	else {
		effects->source_buffer = txl->color;
		effects->target_buffer = fbl->effect_fb;
	}
}

void EEVEE_draw_effects(EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	/* Default framebuffer and texture */
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	/* Init pointers */
	effects->source_buffer = txl->color; /* latest updated texture */
	effects->target_buffer = fbl->effect_fb; /* next target to render to */

	/* Motion Blur */
	if ((effects->enabled_effects & EFFECT_MOTION_BLUR) != 0) {
		eevee_effect_framebuffer_bind(vedata);
		DRW_draw_pass(psl->motion_blur);
	}

	/* Bloom */
	if ((effects->enabled_effects & EFFECT_BLOOM) != 0) {
		struct GPUTexture *last;

		/* Extract bright pixels */
		copy_v2_v2(effects->unf_source_texel_size, effects->source_texel_size);
		effects->unf_source_buffer = effects->source_buffer;

		DRW_framebuffer_bind(fbl->bloom_blit_fb);
		DRW_draw_pass(psl->bloom_blit);

		/* Downsample */
		copy_v2_v2(effects->unf_source_texel_size, effects->blit_texel_size);
		effects->unf_source_buffer = txl->bloom_blit;

		DRW_framebuffer_bind(fbl->bloom_down_fb[0]);
		DRW_draw_pass(psl->bloom_downsample_first);

		last = txl->bloom_downsample[0];

		for (int i = 1; i < effects->bloom_iteration_ct; ++i) {
			copy_v2_v2(effects->unf_source_texel_size, effects->downsamp_texel_size[i-1]);
			effects->unf_source_buffer = last;

			DRW_framebuffer_bind(fbl->bloom_down_fb[i]);
			DRW_draw_pass(psl->bloom_downsample);

			/* Used in next loop */
			last = txl->bloom_downsample[i];
		}

		/* Upsample and accumulate */
		for (int i = effects->bloom_iteration_ct - 2; i >= 0; --i) {
			copy_v2_v2(effects->unf_source_texel_size, effects->downsamp_texel_size[i]);
			effects->unf_source_buffer = txl->bloom_downsample[i];
			effects->unf_base_buffer = last;

			DRW_framebuffer_bind(fbl->bloom_accum_fb[i]);
			DRW_draw_pass(psl->bloom_upsample);

			last = txl->bloom_upsample[i];
		}

		/* Resolve */
		copy_v2_v2(effects->unf_source_texel_size, effects->downsamp_texel_size[0]);
		effects->unf_source_buffer = last;
		effects->unf_base_buffer = effects->source_buffer;

		eevee_effect_framebuffer_bind(vedata);
		DRW_draw_pass(psl->bloom_resolve);
	}

	/* Restore default framebuffer */
	DRW_framebuffer_texture_detach(dtxl->depth);
	DRW_framebuffer_texture_attach(dfbl->default_fb, dtxl->depth, 0, 0);
	DRW_framebuffer_bind(dfbl->default_fb);

	/* Tonemapping */
	/* TODO : use OCIO */
	DRW_draw_pass(psl->tonemap);
}

void EEVEE_effects_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.tonemap_sh);
	DRW_SHADER_FREE_SAFE(e_data.motion_blur_sh);

	DRW_SHADER_FREE_SAFE(e_data.bloom_blit_sh[0]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_downsample_sh[0]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_upsample_sh[0]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_resolve_sh[0]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_blit_sh[1]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_downsample_sh[1]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_upsample_sh[1]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_resolve_sh[1]);
}