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
#include "DNA_camera_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_camera.h"
#include "BKE_object.h"
#include "BKE_animsys.h"
#include "BKE_screen.h"

#include "eevee_private.h"
#include "GPU_texture.h"

typedef struct EEVEE_LightProbeData {
	short probe_id, shadow_id;
} EEVEE_LightProbeData;

/* TODO Option */
#define ENABLE_EFFECT_MOTION_BLUR 1
#define ENABLE_EFFECT_BLOOM 1
#define ENABLE_EFFECT_DOF 1

static struct {
	/* Downsample Depth */
	struct GPUShader *minmaxz_downlevel_sh;
	struct GPUShader *minmaxz_downdepth_sh;
	struct GPUShader *minmaxz_copydepth_sh;

	/* Motion Blur */
	struct GPUShader *motion_blur_sh;

	/* Bloom */
	struct GPUShader *bloom_blit_sh[2];
	struct GPUShader *bloom_downsample_sh[2];
	struct GPUShader *bloom_upsample_sh[2];
	struct GPUShader *bloom_resolve_sh[2];

	/* Depth Of Field */
	struct GPUShader *dof_downsample_sh;
	struct GPUShader *dof_scatter_sh;
	struct GPUShader *dof_resolve_sh;

	struct GPUTexture *minmmaxz_depth_src;
} e_data = {NULL}; /* Engine data */

extern char datatoc_effect_minmaxz_frag_glsl[];
extern char datatoc_effect_motion_blur_frag_glsl[];
extern char datatoc_effect_bloom_frag_glsl[];
extern char datatoc_effect_dof_vert_glsl[];
extern char datatoc_effect_dof_geom_glsl[];
extern char datatoc_effect_dof_frag_glsl[];
extern char datatoc_tonemap_frag_glsl[];

static void eevee_motion_blur_camera_get_matrix_at_time(
        Scene *scene, ARegion *ar, RegionView3D *rv3d, View3D *v3d, Object *camera, float time, float r_mat[4][4])
{
	float obmat[4][4];

	/* HACK */
	Object cam_cpy; Camera camdata_cpy;
	memcpy(&cam_cpy, camera, sizeof(cam_cpy));
	memcpy(&camdata_cpy, camera->data, sizeof(camdata_cpy));
	cam_cpy.data = &camdata_cpy;

	/* Past matrix */
	/* FIXME : This is a temporal solution that does not take care of parent animations */
	/* Recalc Anim manualy */
	BKE_animsys_evaluate_animdata(scene, &cam_cpy.id, cam_cpy.adt, time, ADT_RECALC_ALL);
	BKE_animsys_evaluate_animdata(scene, &camdata_cpy.id, camdata_cpy.adt, time, ADT_RECALC_ALL);
	BKE_object_where_is_calc_time(scene, &cam_cpy, time);

	/* Compute winmat */
	CameraParams params;
	BKE_camera_params_init(&params);

	/* copy of BKE_camera_params_from_view3d */
	{
		params.lens = v3d->lens;
		params.clipsta = v3d->near;
		params.clipend = v3d->far;

		/* camera view */
		BKE_camera_params_from_object(&params, &cam_cpy);

		params.zoom = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom);

		params.offsetx = 2.0f * rv3d->camdx * params.zoom;
		params.offsety = 2.0f * rv3d->camdy * params.zoom;

		params.shiftx *= params.zoom;
		params.shifty *= params.zoom;

		params.zoom = CAMERA_PARAM_ZOOM_INIT_CAMOB / params.zoom;
	}

	BKE_camera_params_compute_viewplane(&params, ar->winx, ar->winy, 1.0f, 1.0f);
	BKE_camera_params_compute_matrix(&params);

	/* FIXME Should be done per view (MULTIVIEW) */
	normalize_m4_m4(obmat, cam_cpy.obmat);
	invert_m4(obmat);
	mul_m4_m4m4(r_mat, params.winmat, obmat);
}

void EEVEE_effects_init(EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects;

	const DRWContextState *draw_ctx = DRW_context_state_get();
	SceneLayer *scene_layer = draw_ctx->sl;
	Scene *scene = draw_ctx->scene;
	View3D *v3d = draw_ctx->v3d;
	RegionView3D *rv3d = draw_ctx->rv3d;
	ARegion *ar = draw_ctx->ar;
	IDProperty *props = BKE_scene_layer_engine_evaluated_get(scene_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);

	const float *viewport_size = DRW_viewport_size_get();

	/* Shaders */
	if (!e_data.motion_blur_sh) {
		e_data.minmaxz_downlevel_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, NULL);
		e_data.minmaxz_downdepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define INPUT_DEPTH\n");
		e_data.minmaxz_copydepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define INPUT_DEPTH\n"
		                                                                                             "#define COPY_DEPTH\n");

		e_data.motion_blur_sh = DRW_shader_create_fullscreen(datatoc_effect_motion_blur_frag_glsl, NULL);

		e_data.dof_downsample_sh = DRW_shader_create(datatoc_effect_dof_vert_glsl, NULL,
		                                                datatoc_effect_dof_frag_glsl, "#define STEP_DOWNSAMPLE\n");
		e_data.dof_scatter_sh = DRW_shader_create(datatoc_effect_dof_vert_glsl, NULL,
		                                             datatoc_effect_dof_frag_glsl, "#define STEP_SCATTER\n");
		e_data.dof_resolve_sh = DRW_shader_create(datatoc_effect_dof_vert_glsl, NULL,
		                                             datatoc_effect_dof_frag_glsl, "#define STEP_RESOLVE\n");

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

	if (!stl->effects) {
		stl->effects = MEM_callocN(sizeof(EEVEE_EffectsInfo), "EEVEE_EffectsInfo");
	}

	effects = stl->effects;

	int enabled_effects = 0;

#if ENABLE_EFFECT_MOTION_BLUR
	if (BKE_collection_engine_property_value_get_bool(props, "motion_blur_enable")) {
		/* Update Motion Blur Matrices */
		if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
			float persmat[4][4];
			float ctime = BKE_scene_frame_get(scene);
			float delta = BKE_collection_engine_property_value_get_float(props, "motion_blur_shutter");

			/* Current matrix */
			eevee_motion_blur_camera_get_matrix_at_time(scene, ar, rv3d, v3d, v3d->camera, ctime, effects->current_ndc_to_world);

			/* Viewport Matrix */
			DRW_viewport_matrix_get(persmat, DRW_MAT_PERS);

			/* Only continue if camera is not being keyed */
			if (compare_m4m4(persmat, effects->current_ndc_to_world, 0.0001f)) {

				/* Past matrix */
				eevee_motion_blur_camera_get_matrix_at_time(scene, ar, rv3d, v3d, v3d->camera, ctime - delta, effects->past_world_to_ndc);

#if 0       /* for future high quality blur */
				/* Future matrix */
				eevee_motion_blur_camera_get_matrix_at_time(scene, ar, rv3d, v3d, v3d->camera, ctime + delta, effects->future_world_to_ndc);
#endif
				invert_m4(effects->current_ndc_to_world);

				effects->motion_blur_samples = BKE_collection_engine_property_value_get_int(props, "motion_blur_samples");
				enabled_effects |= EFFECT_MOTION_BLUR;
			}
		}
	}
#endif /* ENABLE_EFFECT_MOTION_BLUR */

#if ENABLE_EFFECT_BLOOM
	if (BKE_collection_engine_property_value_get_bool(props, "bloom_enable")) {
		/* Bloom */
		int blitsize[2], texsize[2];

		/* Blit Buffer */
		effects->source_texel_size[0] = 1.0f / viewport_size[0];
		effects->source_texel_size[1] = 1.0f / viewport_size[1];

		blitsize[0] = (int)viewport_size[0];
		blitsize[1] = (int)viewport_size[1];

		effects->blit_texel_size[0] = 1.0f / (float)blitsize[0];
		effects->blit_texel_size[1] = 1.0f / (float)blitsize[1];

		DRWFboTexture tex_blit = {&txl->bloom_blit, DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER};
		DRW_framebuffer_init(&fbl->bloom_blit_fb, &draw_engine_eevee_type,
		                    (int)blitsize[0], (int)blitsize[1],
		                    &tex_blit, 1);

		/* Parameters */
		float threshold = BKE_collection_engine_property_value_get_float(props, "bloom_threshold");
		float knee = BKE_collection_engine_property_value_get_float(props, "bloom_knee");
		float intensity = BKE_collection_engine_property_value_get_float(props, "bloom_intensity");
		float radius = BKE_collection_engine_property_value_get_float(props, "bloom_radius");

		/* determine the iteration count */
		const float minDim = (float)MIN2(blitsize[0], blitsize[1]);
		const float maxIter = (radius - 8.0f) + log(minDim) / log(2);
		const int maxIterInt = effects->bloom_iteration_ct = (int)maxIter;

		CLAMP(effects->bloom_iteration_ct, 1, MAX_BLOOM_STEP);

		effects->bloom_sample_scale = 0.5f + maxIter - (float)maxIterInt;
		effects->bloom_curve_threshold[0] = threshold - knee;
		effects->bloom_curve_threshold[1] = knee * 2.0f;
		effects->bloom_curve_threshold[2] = 0.25f / max_ff(1e-5f, knee);
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

			DRWFboTexture tex_bloom = {&txl->bloom_downsample[i], DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER};
			DRW_framebuffer_init(&fbl->bloom_down_fb[i], &draw_engine_eevee_type,
			                    (int)texsize[0], (int)texsize[1],
			                    &tex_bloom, 1);
		}

		/* Upsample buffers */
		copy_v2_v2_int(texsize, blitsize);
		for (int i = 0; i < effects->bloom_iteration_ct - 1; ++i) {
			texsize[0] /= 2; texsize[1] /= 2;
			texsize[0] = MAX2(texsize[0], 2);
			texsize[1] = MAX2(texsize[1], 2);

			DRWFboTexture tex_bloom = {&txl->bloom_upsample[i], DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER};
			DRW_framebuffer_init(&fbl->bloom_accum_fb[i], &draw_engine_eevee_type,
			                    (int)texsize[0], (int)texsize[1],
			                    &tex_bloom, 1);
		}

		enabled_effects |= EFFECT_BLOOM;
	}
#endif /* ENABLE_EFFECT_BLOOM */

#if ENABLE_EFFECT_DOF
	if (BKE_collection_engine_property_value_get_bool(props, "dof_enable")) {
		/* Depth Of Field */
		if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
			Camera *cam = (Camera *)v3d->camera->data;

			/* Retreive Near and Far distance */
			effects->dof_near_far[0] = -cam->clipsta;
			effects->dof_near_far[1] = -cam->clipend;

			int buffer_size[2] = {(int)viewport_size[0] / 2, (int)viewport_size[1] / 2};

			struct GPUTexture **dof_down_near = &txl->dof_down_near;
			bool fb_reset = false;

			/* Reuse buffer from Bloom if available */
			/* WATCH IT : must have the same size */
			if ((enabled_effects & EFFECT_BLOOM) != 0) {
				dof_down_near = &txl->bloom_downsample[0]; /* should always exists */
				if ((effects->enabled_effects & EFFECT_BLOOM) == 0) {
					fb_reset = true;
				}
			}
			else if ((effects->enabled_effects & EFFECT_BLOOM) != 0) {
				fb_reset = true;
			}

			/* if framebuffer config must be changed */
			if (fb_reset && (fbl->dof_down_fb != NULL)) {
				DRW_framebuffer_free(fbl->dof_down_fb);
				fbl->dof_down_fb = NULL;
			}

			/* Setup buffers */
			DRWFboTexture tex_down[3] = {{dof_down_near, DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER}, /* filter to not interfeer with bloom */
			                             {&txl->dof_down_far, DRW_TEX_RGB_11_11_10, 0},
			                             {&txl->dof_coc, DRW_TEX_RG_16, 0}};
			DRW_framebuffer_init(&fbl->dof_down_fb, &draw_engine_eevee_type, buffer_size[0], buffer_size[1], tex_down, 3);

			DRWFboTexture tex_scatter_far = {&txl->dof_far_blur, DRW_TEX_RGBA_16, DRW_TEX_FILTER};
			DRW_framebuffer_init(&fbl->dof_scatter_far_fb, &draw_engine_eevee_type, buffer_size[0], buffer_size[1], &tex_scatter_far, 1);

			DRWFboTexture tex_scatter_near = {&txl->dof_near_blur, DRW_TEX_RGBA_16, DRW_TEX_FILTER};
			DRW_framebuffer_init(&fbl->dof_scatter_near_fb, &draw_engine_eevee_type, buffer_size[0], buffer_size[1], &tex_scatter_near, 1);

			/* Parameters */
			/* TODO UI Options */
			float fstop = cam->gpu_dof.fstop;
			float blades = cam->gpu_dof.num_blades;
			float rotation = cam->gpu_dof.rotation;
			float ratio = 1.0f / cam->gpu_dof.ratio;
			float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
			float focus_dist = BKE_camera_object_dof_distance(v3d->camera);
			float focal_len = cam->lens;

			UNUSED_VARS(rotation, ratio);

			/* this is factor that converts to the scene scale. focal length and sensor are expressed in mm
			 * unit.scale_length is how many meters per blender unit we have. We want to convert to blender units though
			 * because the shader reads coordinates in world space, which is in blender units.
			 * Note however that focus_distance is already in blender units and shall not be scaled here (see T48157). */
			float scale = (scene->unit.system) ? scene->unit.scale_length : 1.0f;
			float scale_camera = 0.001f / scale;
			/* we want radius here for the aperture number  */
			float aperture = 0.5f * scale_camera * focal_len / fstop;
			float focal_len_scaled = scale_camera * focal_len;
			float sensor_scaled = scale_camera * sensor;

			effects->dof_params[0] = aperture * fabsf(focal_len_scaled / (focus_dist - focal_len_scaled));
			effects->dof_params[1] = -focus_dist;
			effects->dof_params[2] = viewport_size[0] / (rv3d->viewcamtexcofac[0] * sensor_scaled);
			effects->dof_bokeh[0] = blades;
			effects->dof_bokeh[1] = rotation;
			effects->dof_bokeh[2] = ratio;
			effects->dof_bokeh[3] = BKE_collection_engine_property_value_get_float(props, "bokeh_max_size");

			enabled_effects |= EFFECT_DOF;
		}
	}
#endif /* ENABLE_EFFECT_DOF */

	effects->enabled_effects = enabled_effects;

	/* Only allocate if at least one effect is activated */
	if (effects->enabled_effects != 0) {
		/* Ping Pong buffer */
		DRWFboTexture tex = {&txl->color_post, DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER};

		DRW_framebuffer_init(&fbl->effect_fb, &draw_engine_eevee_type,
		                    (int)viewport_size[0], (int)viewport_size[1],
		                    &tex, 1);
	}

	{
		/* Ambient Occlusion*/
		stl->effects->ao_dist = BKE_collection_engine_property_value_get_float(props, "gtao_distance");
		stl->effects->ao_samples = BKE_collection_engine_property_value_get_int(props, "gtao_samples");
		stl->effects->ao_factor = BKE_collection_engine_property_value_get_float(props, "gtao_factor");
	}

	/* MinMax Pyramid */
	/* TODO reduce precision */
	DRWFboTexture tex = {&stl->g_data->minmaxz, DRW_TEX_RG_32, DRW_TEX_MIPMAP | DRW_TEX_TEMP};

	DRW_framebuffer_init(&fbl->minmaxz_fb, &draw_engine_eevee_type,
	                    (int)viewport_size[0] / 2, (int)viewport_size[1] / 2,
	                    &tex, 1);

}

static DRWShadingGroup *eevee_create_bloom_pass(const char *name, EEVEE_EffectsInfo *effects, struct GPUShader *sh, DRWPass **pass, bool upsample)
{
	struct Gwn_Batch *quad = DRW_cache_fullscreen_quad_get();

	*pass = DRW_pass_create(name, DRW_STATE_WRITE_COLOR);

	DRWShadingGroup *grp = DRW_shgroup_create(sh, *pass);
	DRW_shgroup_call_add(grp, quad, NULL);
	DRW_shgroup_uniform_buffer(grp, "sourceBuffer", &effects->unf_source_buffer);
	DRW_shgroup_uniform_vec2(grp, "sourceBufferTexelSize", effects->unf_source_texel_size, 1);
	if (upsample) {
		DRW_shgroup_uniform_buffer(grp, "baseBuffer", &effects->unf_base_buffer);
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

	struct Gwn_Batch *quad = DRW_cache_fullscreen_quad_get();

	{
		psl->minmaxz_downlevel = DRW_pass_create("HiZ Down Level", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.minmaxz_downlevel_sh, psl->minmaxz_downlevel);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &stl->g_data->minmaxz);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->minmaxz_downdepth = DRW_pass_create("HiZ Down Depth", DRW_STATE_WRITE_COLOR);
		grp = DRW_shgroup_create(e_data.minmaxz_downdepth_sh, psl->minmaxz_downdepth);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.minmmaxz_depth_src);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->minmaxz_copydepth = DRW_pass_create("HiZ Copy Depth", DRW_STATE_WRITE_COLOR);
		grp = DRW_shgroup_create(e_data.minmaxz_copydepth_sh, psl->minmaxz_copydepth);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.minmmaxz_depth_src);
		DRW_shgroup_call_add(grp, quad, NULL);
	}

	{
		psl->motion_blur = DRW_pass_create("Motion Blur", DRW_STATE_WRITE_COLOR);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.motion_blur_sh, psl->motion_blur);
		DRW_shgroup_uniform_int(grp, "samples", &effects->motion_blur_samples, 1);
		DRW_shgroup_uniform_mat4(grp, "currInvViewProjMatrix", (float *)effects->current_ndc_to_world);
		DRW_shgroup_uniform_mat4(grp, "pastViewProjMatrix", (float *)effects->past_world_to_ndc);
		DRW_shgroup_uniform_buffer(grp, "colorBuffer", &effects->source_buffer);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &dtxl->depth);
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
		/**  Depth of Field algorithm
		 *
		 * Overview :
		 * - Downsample the color buffer into 2 buffers weighted with
		 *   CoC values. Also output CoC into a texture.
		 * - Shoot quads for every pixel and expand it depending on the CoC.
		 *   Do one pass for near Dof and one pass for far Dof.
		 * - Finally composite the 2 blurred buffers with the original render.
		 **/
		DRWShadingGroup *grp;

		psl->dof_down = DRW_pass_create("DoF Downsample", DRW_STATE_WRITE_COLOR);

		grp = DRW_shgroup_create(e_data.dof_downsample_sh, psl->dof_down);
		DRW_shgroup_uniform_buffer(grp, "colorBuffer", &effects->source_buffer);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &dtxl->depth);
		DRW_shgroup_uniform_vec2(grp, "nearFar", effects->dof_near_far, 1);
		DRW_shgroup_uniform_vec3(grp, "dofParams", effects->dof_params, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->dof_scatter = DRW_pass_create("DoF Scatter", DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE);

		/* This create an empty batch of N triangles to be positioned
		 * by the vertex shader 0.4ms against 6ms with instancing */
		const float *viewport_size = DRW_viewport_size_get();
		const int sprite_ct = ((int)viewport_size[0]/2) * ((int)viewport_size[1]/2); /* brackets matters */
		grp = DRW_shgroup_empty_tri_batch_create(e_data.dof_scatter_sh, psl->dof_scatter, sprite_ct);

		DRW_shgroup_uniform_buffer(grp, "colorBuffer", &effects->unf_source_buffer);
		DRW_shgroup_uniform_buffer(grp, "cocBuffer", &txl->dof_coc);
		DRW_shgroup_uniform_vec2(grp, "layerSelection", effects->dof_layer_select, 1);
		DRW_shgroup_uniform_vec4(grp, "bokehParams", effects->dof_bokeh, 1);

		psl->dof_resolve = DRW_pass_create("DoF Resolve", DRW_STATE_WRITE_COLOR);

		grp = DRW_shgroup_create(e_data.dof_resolve_sh, psl->dof_resolve);
		DRW_shgroup_uniform_buffer(grp, "colorBuffer", &effects->source_buffer);
		DRW_shgroup_uniform_buffer(grp, "nearBuffer", &txl->dof_near_blur);
		DRW_shgroup_uniform_buffer(grp, "farBuffer", &txl->dof_far_blur);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &dtxl->depth);
		DRW_shgroup_uniform_vec2(grp, "nearFar", effects->dof_near_far, 1);
		DRW_shgroup_uniform_vec3(grp, "dofParams", effects->dof_params, 1);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
}

#define SWAP_BUFFERS() {                           \
	if (effects->source_buffer == txl->color) {    \
		effects->source_buffer = txl->color_post;  \
		effects->target_buffer = fbl->main;        \
	}                                              \
	else {                                         \
		effects->source_buffer = txl->color;       \
		effects->target_buffer = fbl->effect_fb;   \
	}                                              \
} ((void)0)

static void minmax_downsample_cb(void *vedata, int UNUSED(level))
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	DRW_draw_pass(psl->minmaxz_downlevel);
}

void EEVEE_create_minmax_buffer(EEVEE_Data *vedata, GPUTexture *depth_src)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;

	e_data.minmmaxz_depth_src = depth_src;

	/* Copy depth buffer to minmax texture top level */
	DRW_framebuffer_texture_attach(fbl->minmaxz_fb, stl->g_data->minmaxz, 0, 0);
	DRW_framebuffer_bind(fbl->minmaxz_fb);
	DRW_draw_pass(psl->minmaxz_downdepth);
	DRW_framebuffer_texture_detach(stl->g_data->minmaxz);

	/* Create lower levels */
	DRW_framebuffer_recursive_downsample(fbl->minmaxz_fb, stl->g_data->minmaxz, 6, &minmax_downsample_cb, vedata);
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

	/* Detach depth for effects to use it */
	DRW_framebuffer_texture_detach(dtxl->depth);

	/* Motion Blur */
	if ((effects->enabled_effects & EFFECT_MOTION_BLUR) != 0) {
		DRW_framebuffer_bind(effects->target_buffer);
		DRW_draw_pass(psl->motion_blur);
		SWAP_BUFFERS();
	}

	/* Depth Of Field */
	if ((effects->enabled_effects & EFFECT_DOF) != 0) {
		float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};

		/* Downsample */
		DRW_framebuffer_bind(fbl->dof_down_fb);
		DRW_draw_pass(psl->dof_down);

		/* Scatter Far */
		effects->unf_source_buffer = txl->dof_down_far;
		copy_v2_fl2(effects->dof_layer_select, 0.0f, 1.0f);
		DRW_framebuffer_bind(fbl->dof_scatter_far_fb);
		DRW_framebuffer_clear(true, false, false, clear_col, 0.0f);
		DRW_draw_pass(psl->dof_scatter);

		/* Scatter Near */
		if ((effects->enabled_effects & EFFECT_BLOOM) != 0) {
			/* Reuse bloom half res buffer */
			effects->unf_source_buffer = txl->bloom_downsample[0];
		}
		else {
			effects->unf_source_buffer = txl->dof_down_near;
		}
		copy_v2_fl2(effects->dof_layer_select, 1.0f, 0.0f);
		DRW_framebuffer_bind(fbl->dof_scatter_near_fb);
		DRW_framebuffer_clear(true, false, false, clear_col, 0.0f);
		DRW_draw_pass(psl->dof_scatter);

		/* Resolve */
		DRW_framebuffer_bind(effects->target_buffer);
		DRW_draw_pass(psl->dof_resolve);
		SWAP_BUFFERS();
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

		DRW_framebuffer_bind(effects->target_buffer);
		DRW_draw_pass(psl->bloom_resolve);
		SWAP_BUFFERS();
	}

	/* Restore default framebuffer */
	DRW_framebuffer_texture_attach(dfbl->default_fb, dtxl->depth, 0, 0);
	DRW_framebuffer_bind(dfbl->default_fb);

	/* Tonemapping */
	DRW_transform_to_display(effects->source_buffer);
}

void EEVEE_effects_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.minmaxz_downlevel_sh);
	DRW_SHADER_FREE_SAFE(e_data.minmaxz_downdepth_sh);
	DRW_SHADER_FREE_SAFE(e_data.minmaxz_copydepth_sh);

	DRW_SHADER_FREE_SAFE(e_data.motion_blur_sh);
	DRW_SHADER_FREE_SAFE(e_data.dof_downsample_sh);
	DRW_SHADER_FREE_SAFE(e_data.dof_scatter_sh);
	DRW_SHADER_FREE_SAFE(e_data.dof_resolve_sh);

	DRW_SHADER_FREE_SAFE(e_data.bloom_blit_sh[0]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_downsample_sh[0]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_upsample_sh[0]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_resolve_sh[0]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_blit_sh[1]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_downsample_sh[1]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_upsample_sh[1]);
	DRW_SHADER_FREE_SAFE(e_data.bloom_resolve_sh[1]);
}