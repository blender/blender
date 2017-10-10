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
#include "DNA_world_types.h"

#include "BKE_global.h" /* for G.debug_value */
#include "BKE_camera.h"
#include "BKE_object.h"
#include "BKE_animsys.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "BLI_dynstr.h"
#include "BLI_rand.h"

#include "eevee_private.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_texture.h"

#define SHADER_DEFINES \
	"#define EEVEE_ENGINE\n" \
	"#define MAX_PROBE " STRINGIFY(MAX_PROBE) "\n" \
	"#define MAX_GRID " STRINGIFY(MAX_GRID) "\n" \
	"#define MAX_PLANAR " STRINGIFY(MAX_PLANAR) "\n"

typedef struct EEVEE_LightProbeData {
	short probe_id, shadow_id;
} EEVEE_LightProbeData;

/* SSR shader variations */
enum {
	SSR_SAMPLES      = (1 << 0) | (1 << 1),
	SSR_RESOLVE      = (1 << 2),
	SSR_FULL_TRACE   = (1 << 3),
	SSR_MAX_SHADER   = (1 << 4),
};

static struct {
	/* Downsample Depth */
	struct GPUShader *minz_downlevel_sh;
	struct GPUShader *maxz_downlevel_sh;
	struct GPUShader *minz_downdepth_sh;
	struct GPUShader *maxz_downdepth_sh;
	struct GPUShader *minz_downdepth_layer_sh;
	struct GPUShader *maxz_downdepth_layer_sh;
	struct GPUShader *minz_copydepth_sh;
	struct GPUShader *maxz_copydepth_sh;

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

	/* Volumetric */
	struct GPUShader *volumetric_upsample_sh;

	/* Screen Space Reflection */
	struct GPUShader *ssr_sh[SSR_MAX_SHADER];

	/* Simple Downsample */
	struct GPUShader *downsample_sh;
	struct GPUShader *downsample_cube_sh;

	/* Ground Truth Ambient Occlusion */
	struct GPUShader *gtao_sh;
	struct GPUShader *gtao_debug_sh;

	/* Temporal Anti Aliasing */
	struct GPUShader *taa_resolve_sh;

	/* Theses are just references, not actually allocated */
	struct GPUTexture *depth_src;
	struct GPUTexture *color_src;

	int depth_src_layer;
	float cube_texel_size;
} e_data = {NULL}; /* Engine data */

extern char datatoc_ambient_occlusion_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];
extern char datatoc_octahedron_lib_glsl[];
extern char datatoc_effect_temporal_aa_glsl[];
extern char datatoc_effect_ssr_frag_glsl[];
extern char datatoc_effect_minmaxz_frag_glsl[];
extern char datatoc_effect_motion_blur_frag_glsl[];
extern char datatoc_effect_bloom_frag_glsl[];
extern char datatoc_effect_dof_vert_glsl[];
extern char datatoc_effect_dof_geom_glsl[];
extern char datatoc_effect_dof_frag_glsl[];
extern char datatoc_effect_downsample_frag_glsl[];
extern char datatoc_effect_downsample_cube_frag_glsl[];
extern char datatoc_effect_gtao_frag_glsl[];
extern char datatoc_lightprobe_lib_glsl[];
extern char datatoc_lightprobe_vert_glsl[];
extern char datatoc_lightprobe_geom_glsl[];
extern char datatoc_raytrace_lib_glsl[];
extern char datatoc_tonemap_frag_glsl[];
extern char datatoc_volumetric_frag_glsl[];

static void eevee_motion_blur_camera_get_matrix_at_time(
        const bContext *C, Scene *scene, ARegion *ar, RegionView3D *rv3d, View3D *v3d, Object *camera, float time, float r_mat[4][4])
{
	EvaluationContext eval_ctx;
	float obmat[4][4];

	/* HACK */
	Object cam_cpy; Camera camdata_cpy;
	memcpy(&cam_cpy, camera, sizeof(cam_cpy));
	memcpy(&camdata_cpy, camera->data, sizeof(camdata_cpy));
	cam_cpy.data = &camdata_cpy;

	CTX_data_eval_ctx(C, &eval_ctx);

	/* Past matrix */
	/* FIXME : This is a temporal solution that does not take care of parent animations */
	/* Recalc Anim manualy */
	BKE_animsys_evaluate_animdata(scene, &cam_cpy.id, cam_cpy.adt, time, ADT_RECALC_ALL);
	BKE_animsys_evaluate_animdata(scene, &camdata_cpy.id, camdata_cpy.adt, time, ADT_RECALC_ALL);
	BKE_object_where_is_calc_time(&eval_ctx, scene, &cam_cpy, time);

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

static struct GPUShader *eevee_effects_ssr_shader_get(int options)
{
	if (e_data.ssr_sh[options] == NULL) {
		DynStr *ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_bsdf_common_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_bsdf_sampling_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_octahedron_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_lightprobe_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_ambient_occlusion_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_raytrace_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_effect_ssr_frag_glsl);
		char *ssr_shader_str = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		int samples = (SSR_SAMPLES & options) + 1;

		DynStr *ds_defines = BLI_dynstr_new();
		BLI_dynstr_appendf(ds_defines, SHADER_DEFINES);
		BLI_dynstr_appendf(ds_defines, "#define RAY_COUNT %d\n", samples);
		if (options & SSR_RESOLVE) {
			BLI_dynstr_appendf(ds_defines, "#define STEP_RESOLVE\n");
		}
		else {
			BLI_dynstr_appendf(ds_defines, "#define STEP_RAYTRACE\n");
		}
		if (options & SSR_FULL_TRACE) {
			BLI_dynstr_appendf(ds_defines, "#define FULLRES\n");
		}
		char *ssr_define_str = BLI_dynstr_get_cstring(ds_defines);
		BLI_dynstr_free(ds_defines);

		e_data.ssr_sh[options] = DRW_shader_create_fullscreen(ssr_shader_str, ssr_define_str);

		MEM_freeN(ssr_shader_str);
		MEM_freeN(ssr_define_str);
	}

	return e_data.ssr_sh[options];
}

void EEVEE_effects_init(EEVEE_SceneLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects;

	const DRWContextState *draw_ctx = DRW_context_state_get();
	SceneLayer *scene_layer = draw_ctx->scene_layer;
	Scene *scene = draw_ctx->scene;
	View3D *v3d = draw_ctx->v3d;
	RegionView3D *rv3d = draw_ctx->rv3d;
	ARegion *ar = draw_ctx->ar;
	IDProperty *props = BKE_scene_layer_engine_evaluated_get(scene_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);

	const float *viewport_size = DRW_viewport_size_get();

	/* Shaders */
	if (!e_data.motion_blur_sh) {
		DynStr *ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_bsdf_common_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_ambient_occlusion_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_effect_gtao_frag_glsl);
		char *frag_str = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		e_data.gtao_sh = DRW_shader_create_fullscreen(frag_str, NULL);
		e_data.gtao_debug_sh = DRW_shader_create_fullscreen(frag_str, "#define DEBUG_AO\n");

		MEM_freeN(frag_str);

		e_data.taa_resolve_sh = DRW_shader_create_fullscreen(datatoc_effect_temporal_aa_glsl, NULL);

		e_data.downsample_sh = DRW_shader_create_fullscreen(datatoc_effect_downsample_frag_glsl, NULL);
		e_data.downsample_cube_sh = DRW_shader_create(datatoc_lightprobe_vert_glsl,
		                                              datatoc_lightprobe_geom_glsl,
		                                              datatoc_effect_downsample_cube_frag_glsl, NULL);

		e_data.volumetric_upsample_sh = DRW_shader_create_fullscreen(datatoc_volumetric_frag_glsl, "#define STEP_UPSAMPLE\n");

		e_data.minz_downlevel_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MIN_PASS\n");
		e_data.maxz_downlevel_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MAX_PASS\n");
		e_data.minz_downdepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MIN_PASS\n"
		                                                                                          "#define INPUT_DEPTH\n");
		e_data.maxz_downdepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MAX_PASS\n"
		                                                                                          "#define INPUT_DEPTH\n");
		e_data.minz_downdepth_layer_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MIN_PASS\n"
		                                                                                                "#define LAYERED\n"
		                                                                                                "#define INPUT_DEPTH\n");
		e_data.maxz_downdepth_layer_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MAX_PASS\n"
		                                                                                                "#define LAYERED\n"
		                                                                                                "#define INPUT_DEPTH\n");
		e_data.minz_copydepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MIN_PASS\n"
		                                                                                          "#define INPUT_DEPTH\n"
		                                                                                          "#define COPY_DEPTH\n");
		e_data.maxz_copydepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MAX_PASS\n"
		                                                                                          "#define INPUT_DEPTH\n"
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

	if (BKE_collection_engine_property_value_get_bool(props, "motion_blur_enable") && (draw_ctx->evil_C != NULL)) {
		/* Update Motion Blur Matrices */
		if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
			float persmat[4][4];
			float ctime = BKE_scene_frame_get(scene);
			float delta = BKE_collection_engine_property_value_get_float(props, "motion_blur_shutter");

			/* Current matrix */
			eevee_motion_blur_camera_get_matrix_at_time(draw_ctx->evil_C, scene, ar, rv3d, v3d, v3d->camera, ctime, effects->current_ndc_to_world);

			/* Viewport Matrix */
			DRW_viewport_matrix_get(persmat, DRW_MAT_PERS);

			/* Only continue if camera is not being keyed */
			if (compare_m4m4(persmat, effects->current_ndc_to_world, 0.0001f)) {

				/* Past matrix */
				eevee_motion_blur_camera_get_matrix_at_time(draw_ctx->evil_C, scene, ar, rv3d, v3d, v3d->camera, ctime - delta, effects->past_world_to_ndc);

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
		const float *color = BKE_collection_engine_property_value_get_float_array(props, "bloom_color");
		float radius = BKE_collection_engine_property_value_get_float(props, "bloom_radius");
		effects->bloom_clamp = BKE_collection_engine_property_value_get_float(props, "bloom_clamp");

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

		mul_v3_v3fl(effects->bloom_color, color, intensity);

		/* Downsample buffers */
		copy_v2_v2_int(texsize, blitsize);
		for (int i = 0; i < effects->bloom_iteration_ct; ++i) {
			texsize[0] /= 2; texsize[1] /= 2;

			if (GPU_type_matches(GPU_DEVICE_AMD_VEGA, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE)) {
				texsize[0] = MAX2(texsize[0], 17);
				texsize[1] = MAX2(texsize[1], 17);
			}
			else {
				texsize[0] = MAX2(texsize[0], 2);
				texsize[1] = MAX2(texsize[1], 2);
			}

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

			if (GPU_type_matches(GPU_DEVICE_AMD_VEGA, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE)) {
				texsize[0] = MAX2(texsize[0], 17);
				texsize[1] = MAX2(texsize[1], 17);
			}
			else {
				texsize[0] = MAX2(texsize[0], 2);
				texsize[1] = MAX2(texsize[1], 2);
			}

			DRWFboTexture tex_bloom = {&txl->bloom_upsample[i], DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER};
			DRW_framebuffer_init(&fbl->bloom_accum_fb[i], &draw_engine_eevee_type,
			                    (int)texsize[0], (int)texsize[1],
			                    &tex_bloom, 1);
		}

		enabled_effects |= EFFECT_BLOOM;
	}

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

	if (BKE_collection_engine_property_value_get_int(props, "taa_samples") != 1) {
		float persmat[4][4], viewmat[4][4];

		enabled_effects |= EFFECT_TAA | EFFECT_DOUBLE_BUFFER;

		/* Until we support reprojection, we need to make sure
		 * that the history buffer contains correct information. */
		bool view_is_valid = stl->g_data->valid_double_buffer;

		view_is_valid = view_is_valid && (stl->g_data->view_updated == false);

		effects->taa_total_sample = BKE_collection_engine_property_value_get_int(props, "taa_samples");
		MAX2(effects->taa_total_sample, 0);

		DRW_viewport_matrix_get(persmat, DRW_MAT_PERS);
		DRW_viewport_matrix_get(viewmat, DRW_MAT_VIEW);
		DRW_viewport_matrix_get(effects->overide_winmat, DRW_MAT_WIN);
		view_is_valid = view_is_valid && compare_m4m4(persmat, effects->prev_drw_persmat, FLT_MIN);
		copy_m4_m4(effects->prev_drw_persmat, persmat);

		/* Prevent ghosting from probe data. */
		view_is_valid = view_is_valid && (effects->prev_drw_support == DRW_state_draw_support());
		effects->prev_drw_support = DRW_state_draw_support();

		if (view_is_valid &&
		    ((effects->taa_total_sample == 0) ||
		     (effects->taa_current_sample < effects->taa_total_sample)))
		{
			effects->taa_current_sample += 1;

			effects->taa_alpha = 1.0f / (float)(effects->taa_current_sample);

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
		else {
			effects->taa_current_sample = 1;
		}

		DRWFboTexture tex_double_buffer = {&txl->depth_double_buffer, DRW_TEX_DEPTH_24};

		DRW_framebuffer_init(&fbl->depth_double_buffer_fb, &draw_engine_eevee_type,
		                    (int)viewport_size[0], (int)viewport_size[1],
		                    &tex_double_buffer, 1);
	}
	else {
		/* Cleanup to release memory */
		DRW_TEXTURE_FREE_SAFE(txl->depth_double_buffer);
		DRW_FRAMEBUFFER_FREE_SAFE(fbl->depth_double_buffer_fb);
	}


	effects->enabled_effects = enabled_effects;

	/* Only allocate if at least one effect is activated */
	if (effects->enabled_effects != 0) {
		/* Ping Pong buffer */
		DRWFboTexture tex = {&txl->color_post, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

		DRW_framebuffer_init(&fbl->effect_fb, &draw_engine_eevee_type,
		                    (int)viewport_size[0], (int)viewport_size[1],
		                    &tex, 1);
	}

	if (BKE_collection_engine_property_value_get_bool(props, "gtao_enable")) {
		/* Ambient Occlusion*/
		effects->enabled_effects |= EFFECT_GTAO;

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
	}
	else {
		/* Cleanup */
		DRW_TEXTURE_FREE_SAFE(txl->gtao_horizons);
		DRW_FRAMEBUFFER_FREE_SAFE(fbl->gtao_fb);
		effects->ao_settings = 0.0f;
	}

	/* MinMax Pyramid */
	DRWFboTexture texmin = {&stl->g_data->minzbuffer, DRW_TEX_DEPTH_24, DRW_TEX_MIPMAP | DRW_TEX_TEMP};

	DRW_framebuffer_init(&fbl->downsample_fb, &draw_engine_eevee_type,
	                    (int)viewport_size[0] / 2, (int)viewport_size[1] / 2,
	                    &texmin, 1);

	/* Cannot define 2 depth texture for one framebuffer. So allocate ourself. */
	if (txl->maxzbuffer == NULL) {
		txl->maxzbuffer = DRW_texture_create_2D((int)viewport_size[0] / 2, (int)viewport_size[1] / 2, DRW_TEX_DEPTH_24, DRW_TEX_MIPMAP, NULL);
	}

	/* Compute Mipmap texel alignement. */
	for (int i = 0; i < 10; ++i) {
		float mip_size[2] = {viewport_size[0], viewport_size[1]};
		for (int j = 0; j < i; ++j) {
			mip_size[0] = floorf(fmaxf(1.0f, mip_size[0] / 2.0f));
			mip_size[1] = floorf(fmaxf(1.0f, mip_size[1] / 2.0f));
		}
		stl->g_data->mip_ratio[i][0] = viewport_size[0] / (mip_size[0] * powf(2.0f, floorf(log2f(floorf(viewport_size[0] / mip_size[0])))));
		stl->g_data->mip_ratio[i][1] = viewport_size[1] / (mip_size[1] * powf(2.0f, floorf(log2f(floorf(viewport_size[1] / mip_size[1])))));
	}

	if (BKE_collection_engine_property_value_get_bool(props, "volumetric_enable")) {
		World *wo = scene->world;

		/* TODO: this will not be the case if we support object volumetrics */
		if ((wo != NULL) && (wo->use_nodes) && (wo->nodetree != NULL)) {
			effects->enabled_effects |= EFFECT_VOLUMETRIC;

			if (sldata->volumetrics == NULL) {
				sldata->volumetrics = MEM_callocN(sizeof(EEVEE_VolumetricsInfo), "EEVEE_VolumetricsInfo");
			}

			EEVEE_VolumetricsInfo *volumetrics = sldata->volumetrics;
			bool last_use_colored_transmit = volumetrics->use_colored_transmit; /* Save to compare */

			volumetrics->integration_start = BKE_collection_engine_property_value_get_float(props, "volumetric_start");
			volumetrics->integration_end = BKE_collection_engine_property_value_get_float(props, "volumetric_end");

			if (DRW_viewport_is_persp_get()) {
				/* Negate */
				volumetrics->integration_start = -volumetrics->integration_start;
				volumetrics->integration_end = -volumetrics->integration_end;
			}
			else {
				const float clip_start = stl->g_data->viewvecs[0][2];
				const float clip_end = stl->g_data->viewvecs[1][2];
				volumetrics->integration_start = min_ff(volumetrics->integration_end, clip_start);
				volumetrics->integration_end = max_ff(-volumetrics->integration_end, clip_end);
			}

			volumetrics->sample_distribution = BKE_collection_engine_property_value_get_float(props, "volumetric_sample_distribution");
			volumetrics->integration_step_count = (float)BKE_collection_engine_property_value_get_int(props, "volumetric_samples");
			volumetrics->shadow_step_count = (float)BKE_collection_engine_property_value_get_int(props, "volumetric_shadow_samples");
			volumetrics->light_clamp = BKE_collection_engine_property_value_get_float(props, "volumetric_light_clamp");

			/* Disable clamp if equal to 0. */
			if (volumetrics->light_clamp == 0.0) {
				volumetrics->light_clamp = FLT_MAX;
			}

			volumetrics->use_lights = BKE_collection_engine_property_value_get_bool(props, "volumetric_lights");
			volumetrics->use_volume_shadows = BKE_collection_engine_property_value_get_bool(props, "volumetric_shadows");
			volumetrics->use_colored_transmit = BKE_collection_engine_property_value_get_bool(props, "volumetric_colored_transmittance");

			if (last_use_colored_transmit != volumetrics->use_colored_transmit) {
				if (fbl->volumetric_fb != NULL) {
					DRW_framebuffer_free(fbl->volumetric_fb);
					fbl->volumetric_fb = NULL;
				}
			}

			/* Integration result buffer(s) */
			if (volumetrics->use_colored_transmit == false) {
				/* Monocromatic transmittance in alpha */
				DRWFboTexture tex_vol = {&stl->g_data->volumetric, DRW_TEX_RGBA_16, DRW_TEX_MIPMAP | DRW_TEX_FILTER | DRW_TEX_TEMP};

				DRW_framebuffer_init(&fbl->volumetric_fb, &draw_engine_eevee_type,
				                    (int)viewport_size[0] / 2, (int)viewport_size[1] / 2,
				                    &tex_vol, 1);
			}
			else {
				/* Transmittance is separated, No need for alpha and DRW_TEX_RGB_11_11_10 gives the same vram usage */
				/* Hint ! Could reuse this for transparency! */
				DRWFboTexture tex_vol[2] = {{&stl->g_data->volumetric, DRW_TEX_RGB_11_11_10, DRW_TEX_MIPMAP | DRW_TEX_FILTER | DRW_TEX_TEMP},
				                            {&stl->g_data->volumetric_transmit, DRW_TEX_RGB_11_11_10, DRW_TEX_MIPMAP | DRW_TEX_FILTER | DRW_TEX_TEMP}};

				DRW_framebuffer_init(&fbl->volumetric_fb, &draw_engine_eevee_type,
				                    (int)viewport_size[0] / 2, (int)viewport_size[1] / 2,
				                    tex_vol, 2);
			}
		}
	}

	/* Compute pixel size, (shared with contact shadows) */
	copy_v2_v2(effects->ssr_pixelsize, viewport_size);
	invert_v2(effects->ssr_pixelsize);

	if (BKE_collection_engine_property_value_get_bool(props, "ssr_enable")) {
		effects->enabled_effects |= EFFECT_SSR;

		if (BKE_collection_engine_property_value_get_bool(props, "ssr_refraction")) {
			effects->enabled_effects |= EFFECT_REFRACT;

			DRWFboTexture tex = {&txl->refract_color, DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

			DRW_framebuffer_init(&fbl->refract_fb, &draw_engine_eevee_type, (int)viewport_size[0], (int)viewport_size[1], &tex, 1);
		}

		/* Enable double buffering to be able to read previous frame color */
		effects->enabled_effects |= EFFECT_DOUBLE_BUFFER;

		effects->ssr_ray_count = BKE_collection_engine_property_value_get_int(props, "ssr_ray_count");
		effects->reflection_trace_full = !BKE_collection_engine_property_value_get_bool(props, "ssr_halfres");
		effects->ssr_use_normalization = BKE_collection_engine_property_value_get_bool(props, "ssr_normalize_weight");
		effects->ssr_quality = 1.0f - BKE_collection_engine_property_value_get_float(props, "ssr_quality");
		effects->ssr_thickness = BKE_collection_engine_property_value_get_float(props, "ssr_thickness");
		effects->ssr_border_fac = BKE_collection_engine_property_value_get_float(props, "ssr_border_fade");
		effects->ssr_firefly_fac = BKE_collection_engine_property_value_get_float(props, "ssr_firefly_fac");
		effects->ssr_max_roughness = BKE_collection_engine_property_value_get_float(props, "ssr_max_roughness");

		if (effects->ssr_firefly_fac < 1e-8f) {
			effects->ssr_firefly_fac = FLT_MAX;
		}

		/* Important, can lead to breakage otherwise. */
		CLAMP(effects->ssr_ray_count, 1, 4);

		const int divisor = (effects->reflection_trace_full) ? 1 : 2;
		int tracing_res[2] = {(int)viewport_size[0] / divisor, (int)viewport_size[1] / divisor};
		const bool high_qual_input = true; /* TODO dither low quality input */

		/* MRT for the shading pass in order to output needed data for the SSR pass. */
		/* TODO create one texture layer per lobe */
		if (txl->ssr_specrough_input == NULL) {
			DRWTextureFormat specrough_format = (high_qual_input) ? DRW_TEX_RGBA_16 : DRW_TEX_RGBA_8;
			txl->ssr_specrough_input = DRW_texture_create_2D((int)viewport_size[0], (int)viewport_size[1], specrough_format, 0, NULL);
		}

		/* Reattach textures to the right buffer (because we are alternating between buffers) */
		/* TODO multiple FBO per texture!!!! */
		DRW_framebuffer_texture_detach(txl->ssr_specrough_input);
		DRW_framebuffer_texture_attach(fbl->main, txl->ssr_specrough_input, 2, 0);

		/* Raytracing output */
		/* TODO try integer format for hit coord to increase precision */
		DRWFboTexture tex_output[4] = {{&stl->g_data->ssr_hit_output[0], DRW_TEX_RGBA_16, DRW_TEX_TEMP},
		                               {&stl->g_data->ssr_hit_output[1], DRW_TEX_RGBA_16, DRW_TEX_TEMP},
		                               {&stl->g_data->ssr_hit_output[2], DRW_TEX_RGBA_16, DRW_TEX_TEMP},
		                               {&stl->g_data->ssr_hit_output[3], DRW_TEX_RGBA_16, DRW_TEX_TEMP}};

		DRW_framebuffer_init(&fbl->screen_tracing_fb, &draw_engine_eevee_type, tracing_res[0], tracing_res[1], tex_output, effects->ssr_ray_count);
	}
	else {
		/* Cleanup to release memory */
		DRW_TEXTURE_FREE_SAFE(txl->ssr_specrough_input);
		DRW_FRAMEBUFFER_FREE_SAFE(fbl->screen_tracing_fb);
		for (int i = 0; i < 4; ++i) {
			stl->g_data->ssr_hit_output[i] = NULL;
		}
	}

	/* Normal buffer for deferred passes. */
	if ((((effects->enabled_effects & EFFECT_GTAO) != 0) && G.debug_value == 6) ||
	    ((effects->enabled_effects & EFFECT_SSR) != 0))
	{
		if (txl->ssr_normal_input == NULL) {
			DRWTextureFormat nor_format = DRW_TEX_RG_16;
			txl->ssr_normal_input = DRW_texture_create_2D((int)viewport_size[0], (int)viewport_size[1], nor_format, 0, NULL);
		}

		/* Reattach textures to the right buffer (because we are alternating between buffers) */
		/* TODO multiple FBO per texture!!!! */
		DRW_framebuffer_texture_detach(txl->ssr_normal_input);
		DRW_framebuffer_texture_attach(fbl->main, txl->ssr_normal_input, 1, 0);
	}
	else {
		/* Cleanup to release memory */
		DRW_TEXTURE_FREE_SAFE(txl->ssr_normal_input);
	}

	/* Setup double buffer so we can access last frame as it was before post processes */
	if ((effects->enabled_effects & EFFECT_DOUBLE_BUFFER) != 0) {
		DRWFboTexture tex_double_buffer = {&txl->color_double_buffer, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

		DRW_framebuffer_init(&fbl->double_buffer, &draw_engine_eevee_type,
		                    (int)viewport_size[0], (int)viewport_size[1],
		                    &tex_double_buffer, 1);
	}
	else {
		/* Cleanup to release memory */
		DRW_TEXTURE_FREE_SAFE(txl->color_double_buffer);
		DRW_FRAMEBUFFER_FREE_SAFE(fbl->double_buffer);
	}
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

void EEVEE_effects_cache_init(EEVEE_SceneLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	struct Gwn_Batch *quad = DRW_cache_fullscreen_quad_get();

	if ((effects->enabled_effects & EFFECT_TAA) != 0) {
		psl->taa_resolve = DRW_pass_create("Temporal AA Resolve", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.taa_resolve_sh, psl->taa_resolve);

		DRW_shgroup_uniform_buffer(grp, "historyBuffer", &txl->color_double_buffer);
		DRW_shgroup_uniform_buffer(grp, "colorBuffer", &txl->color);
		DRW_shgroup_uniform_float(grp, "alpha", &effects->taa_alpha, 1);
		DRW_shgroup_call_add(grp, quad, NULL);
	}

	if ((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) {
		const DRWContextState *draw_ctx = DRW_context_state_get();
		Scene *scene = draw_ctx->scene;
		struct World *wo = scene->world; /* Already checked non NULL */
		EEVEE_VolumetricsInfo *volumetrics = sldata->volumetrics;

		struct GPUMaterial *mat = EEVEE_material_world_volume_get(
		        scene, wo, volumetrics->use_lights, volumetrics->use_volume_shadows,
		        false, volumetrics->use_colored_transmit, sldata->lamps->shadow_method);

		psl->volumetric_integrate_ps = DRW_pass_create("Volumetric Integration", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_material_create(mat, psl->volumetric_integrate_ps);

		if (grp != NULL) {
			DRW_shgroup_uniform_buffer(grp, "depthFull", &e_data.depth_src);
			DRW_shgroup_uniform_buffer(grp, "shadowTexture", &sldata->shadow_pool);
			DRW_shgroup_uniform_buffer(grp, "irradianceGrid", &sldata->irradiance_pool);
			DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
			DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
			DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
			DRW_shgroup_uniform_int(grp, "light_count", &sldata->lamps->num_light, 1);
			DRW_shgroup_uniform_int(grp, "grid_count", &sldata->probes->num_render_grid, 1);
			DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
			DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
			DRW_shgroup_uniform_vec2(grp, "volume_start_end", &sldata->volumetrics->integration_start, 1);
			DRW_shgroup_uniform_vec4(grp, "volume_samples_clamp", &sldata->volumetrics->integration_step_count, 1);
			DRW_shgroup_call_add(grp, quad, NULL);

			if (volumetrics->use_colored_transmit == false) { /* Monochromatic transmittance */
				psl->volumetric_resolve_ps = DRW_pass_create("Volumetric Resolve", DRW_STATE_WRITE_COLOR | DRW_STATE_TRANSMISSION);
				grp = DRW_shgroup_create(e_data.volumetric_upsample_sh, psl->volumetric_resolve_ps);
				DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
				DRW_shgroup_uniform_buffer(grp, "depthFull", &e_data.depth_src);
				DRW_shgroup_uniform_buffer(grp, "volumetricBuffer", &stl->g_data->volumetric);
				DRW_shgroup_call_add(grp, quad, NULL);
			}
			else {
				psl->volumetric_resolve_transmit_ps = DRW_pass_create("Volumetric Transmittance Resolve", DRW_STATE_WRITE_COLOR | DRW_STATE_MULTIPLY);
				grp = DRW_shgroup_create(e_data.volumetric_upsample_sh, psl->volumetric_resolve_transmit_ps);
				DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
				DRW_shgroup_uniform_buffer(grp, "depthFull", &e_data.depth_src);
				DRW_shgroup_uniform_buffer(grp, "volumetricBuffer", &stl->g_data->volumetric_transmit);
				DRW_shgroup_call_add(grp, quad, NULL);

				psl->volumetric_resolve_ps = DRW_pass_create("Volumetric Resolve", DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE);
				grp = DRW_shgroup_create(e_data.volumetric_upsample_sh, psl->volumetric_resolve_ps);
				DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
				DRW_shgroup_uniform_buffer(grp, "depthFull", &e_data.depth_src);
				DRW_shgroup_uniform_buffer(grp, "volumetricBuffer", &stl->g_data->volumetric);
				DRW_shgroup_call_add(grp, quad, NULL);
			}
		}
		else {
			/* Compilation failled */
			effects->enabled_effects &= ~EFFECT_VOLUMETRIC;
		}
	}

	if ((effects->enabled_effects & EFFECT_SSR) != 0) {
		int options = (effects->reflection_trace_full) ? SSR_FULL_TRACE : 0;
		options |= (effects->ssr_ray_count - 1);

		struct GPUShader *trace_shader = eevee_effects_ssr_shader_get(options);
		struct GPUShader *resolve_shader = eevee_effects_ssr_shader_get(SSR_RESOLVE | options);

		psl->ssr_raytrace = DRW_pass_create("SSR Raytrace", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_create(trace_shader, psl->ssr_raytrace);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_uniform_buffer(grp, "normalBuffer", &txl->ssr_normal_input);
		DRW_shgroup_uniform_buffer(grp, "specroughBuffer", &txl->ssr_specrough_input);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
		DRW_shgroup_uniform_buffer(grp, "maxzBuffer", &txl->maxzbuffer);
		DRW_shgroup_uniform_buffer(grp, "minzBuffer", &stl->g_data->minzbuffer);
		DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
		DRW_shgroup_uniform_vec2(grp, "mipRatio[0]", (float *)stl->g_data->mip_ratio, 10);
		DRW_shgroup_uniform_vec4(grp, "ssrParameters", &effects->ssr_quality, 1);
		DRW_shgroup_uniform_int(grp, "planar_count", &sldata->probes->num_planar, 1);
		DRW_shgroup_uniform_float(grp, "maxRoughness", &effects->ssr_max_roughness, 1);
		DRW_shgroup_uniform_buffer(grp, "planarDepth", &vedata->txl->planar_depth);
		DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->ssr_resolve = DRW_pass_create("SSR Resolve", DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE);
		grp = DRW_shgroup_create(resolve_shader, psl->ssr_resolve);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_uniform_buffer(grp, "normalBuffer", &txl->ssr_normal_input);
		DRW_shgroup_uniform_buffer(grp, "specroughBuffer", &txl->ssr_specrough_input);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
		DRW_shgroup_uniform_buffer(grp, "prevColorBuffer", &txl->color_double_buffer);
		DRW_shgroup_uniform_mat4(grp, "PastViewProjectionMatrix", (float *)stl->g_data->prev_persmat);
		DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
		DRW_shgroup_uniform_int(grp, "planar_count", &sldata->probes->num_planar, 1);
		DRW_shgroup_uniform_int(grp, "probe_count", &sldata->probes->num_render_cube, 1);
		DRW_shgroup_uniform_vec2(grp, "mipRatio[0]", (float *)stl->g_data->mip_ratio, 10);
		DRW_shgroup_uniform_float(grp, "borderFadeFactor", &effects->ssr_border_fac, 1);
		DRW_shgroup_uniform_float(grp, "maxRoughness", &effects->ssr_max_roughness, 1);
		DRW_shgroup_uniform_float(grp, "lodCubeMax", &sldata->probes->lod_cube_max, 1);
		DRW_shgroup_uniform_float(grp, "lodPlanarMax", &sldata->probes->lod_planar_max, 1);
		DRW_shgroup_uniform_float(grp, "fireflyFactor", &effects->ssr_firefly_fac, 1);
		DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
		DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
		DRW_shgroup_uniform_buffer(grp, "probeCubes", &sldata->probe_pool);
		DRW_shgroup_uniform_buffer(grp, "probePlanars", &vedata->txl->planar_pool);
		DRW_shgroup_uniform_buffer(grp, "hitBuffer0", &stl->g_data->ssr_hit_output[0]);
		if (effects->ssr_ray_count > 1) {
			DRW_shgroup_uniform_buffer(grp, "hitBuffer1", &stl->g_data->ssr_hit_output[1]);
		}
		if (effects->ssr_ray_count > 2) {
			DRW_shgroup_uniform_buffer(grp, "hitBuffer2", &stl->g_data->ssr_hit_output[2]);
		}
		if (effects->ssr_ray_count > 3) {
			DRW_shgroup_uniform_buffer(grp, "hitBuffer3", &stl->g_data->ssr_hit_output[3]);
		}

		DRW_shgroup_uniform_vec4(grp, "aoParameters[0]", &effects->ao_dist, 2);
		if (effects->use_ao) {
			DRW_shgroup_uniform_buffer(grp, "horizonBuffer", &vedata->txl->gtao_horizons);
			DRW_shgroup_uniform_ivec2(grp, "aoHorizonTexSize", (int *)vedata->stl->effects->ao_texsize, 1);
		}
		else {
			/* Use shadow_pool as fallback to avoid sampling problem on certain platform, see: T52593 */
			DRW_shgroup_uniform_buffer(grp, "horizonBuffer", &sldata->shadow_pool);
		}

		DRW_shgroup_call_add(grp, quad, NULL);
	}

	{
		psl->color_downsample_ps = DRW_pass_create("Downsample", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.downsample_sh, psl->color_downsample_ps);
		DRW_shgroup_uniform_buffer(grp, "source", &e_data.color_src);
		DRW_shgroup_uniform_float(grp, "fireflyFactor", &effects->ssr_firefly_fac, 1);
		DRW_shgroup_call_add(grp, quad, NULL);
	}

	{
		static int zero = 0;
		psl->color_downsample_cube_ps = DRW_pass_create("Downsample Cube", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_instance_create(e_data.downsample_cube_sh, psl->color_downsample_cube_ps, quad);
		DRW_shgroup_uniform_buffer(grp, "source", &e_data.color_src);
		DRW_shgroup_uniform_float(grp, "texelSize", &e_data.cube_texel_size, 1);
		DRW_shgroup_uniform_int(grp, "Layer", &zero, 1);
		for (int i = 0; i < 6; ++i)
			DRW_shgroup_call_dynamic_add_empty(grp);
	}

	{
		/* Perform min/max downsample */
		psl->minz_downlevel_ps = DRW_pass_create("HiZ Min Down Level", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.minz_downlevel_sh, psl->minz_downlevel_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &stl->g_data->minzbuffer);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->maxz_downlevel_ps = DRW_pass_create("HiZ Max Down Level", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.maxz_downlevel_sh, psl->maxz_downlevel_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &txl->maxzbuffer);
		DRW_shgroup_call_add(grp, quad, NULL);

		/* Copy depth buffer to halfres top level of HiZ */
		psl->minz_downdepth_ps = DRW_pass_create("HiZ Min Copy Depth Halfres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.minz_downdepth_sh, psl->minz_downdepth_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->maxz_downdepth_ps = DRW_pass_create("HiZ Max Copy Depth Halfres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.maxz_downdepth_sh, psl->maxz_downdepth_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->minz_downdepth_layer_ps = DRW_pass_create("HiZ Min Copy DepthLayer Halfres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.minz_downdepth_layer_sh, psl->minz_downdepth_layer_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_uniform_int(grp, "depthLayer", &e_data.depth_src_layer, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->maxz_downdepth_layer_ps = DRW_pass_create("HiZ Max Copy DepthLayer Halfres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.maxz_downdepth_layer_sh, psl->maxz_downdepth_layer_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_uniform_int(grp, "depthLayer", &e_data.depth_src_layer, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		/* Copy depth buffer to halfres top level of HiZ */
		psl->minz_copydepth_ps = DRW_pass_create("HiZ Min Copy Depth Fullres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.minz_copydepth_sh, psl->minz_copydepth_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->maxz_copydepth_ps = DRW_pass_create("HiZ Max Copy Depth Fullres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.maxz_copydepth_sh, psl->maxz_copydepth_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_call_add(grp, quad, NULL);
	}

	{
		psl->ao_horizon_search = DRW_pass_create("GTAO Horizon Search", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.gtao_sh, psl->ao_horizon_search);
		DRW_shgroup_uniform_buffer(grp, "maxzBuffer", &txl->maxzbuffer);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
		DRW_shgroup_uniform_vec2(grp, "mipRatio[0]", (float *)stl->g_data->mip_ratio, 10);
		DRW_shgroup_uniform_vec4(grp, "aoParameters[0]", &stl->effects->ao_dist, 2);
		DRW_shgroup_uniform_float(grp, "sampleNbr", &stl->effects->ao_sample_nbr, 1);
		DRW_shgroup_uniform_ivec2(grp, "aoHorizonTexSize", (int *)stl->effects->ao_texsize, 1);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->ao_horizon_debug = DRW_pass_create("GTAO Horizon Debug", DRW_STATE_WRITE_COLOR);
		grp = DRW_shgroup_create(e_data.gtao_debug_sh, psl->ao_horizon_debug);
		DRW_shgroup_uniform_buffer(grp, "maxzBuffer", &txl->maxzbuffer);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_uniform_buffer(grp, "normalBuffer", &txl->ssr_normal_input);
		DRW_shgroup_uniform_buffer(grp, "horizonBuffer", &txl->gtao_horizons);
		DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
		DRW_shgroup_uniform_vec2(grp, "mipRatio[0]", (float *)stl->g_data->mip_ratio, 10);
		DRW_shgroup_uniform_vec4(grp, "aoParameters[0]", &stl->effects->ao_dist, 2);
		DRW_shgroup_uniform_ivec2(grp, "aoHorizonTexSize", (int *)stl->effects->ao_texsize, 1);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
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
		DRW_shgroup_uniform_float(grp, "clampIntensity", &effects->bloom_clamp, 1);
		grp = eevee_create_bloom_pass("Bloom Resolve", effects, e_data.bloom_resolve_sh[use_highres], &psl->bloom_resolve, true);
		DRW_shgroup_uniform_vec3(grp, "bloomColor", effects->bloom_color, 1);
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
		const int sprite_ct = ((int)viewport_size[0] / 2) * ((int)viewport_size[1] / 2); /* brackets matters */
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

static void min_downsample_cb(void *vedata, int UNUSED(level))
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	DRW_draw_pass(psl->minz_downlevel_ps);
}

static void max_downsample_cb(void *vedata, int UNUSED(level))
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	DRW_draw_pass(psl->maxz_downlevel_ps);
}

static void simple_downsample_cb(void *vedata, int UNUSED(level))
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	DRW_draw_pass(psl->color_downsample_ps);
}

static void simple_downsample_cube_cb(void *vedata, int level)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	e_data.cube_texel_size = (float)(1 << level) / (float)GPU_texture_width(e_data.color_src);
	DRW_draw_pass(psl->color_downsample_cube_ps);
}

void EEVEE_create_minmax_buffer(EEVEE_Data *vedata, GPUTexture *depth_src, int layer)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;

	e_data.depth_src = depth_src;
	e_data.depth_src_layer = layer;

	DRW_stats_group_start("Min buffer");
	/* Copy depth buffer to min texture top level */
	DRW_framebuffer_texture_attach(fbl->downsample_fb, stl->g_data->minzbuffer, 0, 0);
	DRW_framebuffer_bind(fbl->downsample_fb);
	if (layer >= 0) {
		DRW_draw_pass(psl->minz_downdepth_layer_ps);
	}
	else {
		DRW_draw_pass(psl->minz_downdepth_ps);
	}
	DRW_framebuffer_texture_detach(stl->g_data->minzbuffer);

	/* Create lower levels */
	DRW_framebuffer_recursive_downsample(fbl->downsample_fb, stl->g_data->minzbuffer, 8, &min_downsample_cb, vedata);
	DRW_stats_group_end();

	DRW_stats_group_start("Max buffer");
	/* Copy depth buffer to max texture top level */
	DRW_framebuffer_texture_attach(fbl->downsample_fb, txl->maxzbuffer, 0, 0);
	DRW_framebuffer_bind(fbl->downsample_fb);
	if (layer >= 0) {
		DRW_draw_pass(psl->maxz_downdepth_layer_ps);
	}
	else {
		DRW_draw_pass(psl->maxz_downdepth_ps);
	}
	DRW_framebuffer_texture_detach(txl->maxzbuffer);

	/* Create lower levels */
	DRW_framebuffer_recursive_downsample(fbl->downsample_fb, txl->maxzbuffer, 8, &max_downsample_cb, vedata);
	DRW_stats_group_end();
}

/**
 * Simple downsampling algorithm. Reconstruct mip chain up to mip level.
 **/
void EEVEE_downsample_buffer(EEVEE_Data *vedata, struct GPUFrameBuffer *fb_src, GPUTexture *texture_src, int level)
{
	e_data.color_src = texture_src;

	DRW_stats_group_start("Downsample buffer");
	/* Create lower levels */
	DRW_framebuffer_recursive_downsample(fb_src, texture_src, level, &simple_downsample_cb, vedata);
	DRW_stats_group_end();
}

/**
 * Simple downsampling algorithm for cubemap. Reconstruct mip chain up to mip level.
 **/
void EEVEE_downsample_cube_buffer(EEVEE_Data *vedata, struct GPUFrameBuffer *fb_src, GPUTexture *texture_src, int level)
{
	e_data.color_src = texture_src;

	DRW_stats_group_start("Downsample Cube buffer");
	/* Create lower levels */
	DRW_framebuffer_recursive_downsample(fb_src, texture_src, level, &simple_downsample_cube_cb, vedata);
	DRW_stats_group_end();
}

void EEVEE_effects_do_volumetrics(EEVEE_SceneLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if ((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) {
		DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

		e_data.depth_src = dtxl->depth;

		/* Compute volumetric integration at halfres. */
		DRW_framebuffer_texture_attach(fbl->volumetric_fb, stl->g_data->volumetric, 0, 0);
		if (sldata->volumetrics->use_colored_transmit) {
			DRW_framebuffer_texture_attach(fbl->volumetric_fb, stl->g_data->volumetric_transmit, 1, 0);
		}
		DRW_framebuffer_bind(fbl->volumetric_fb);
		DRW_draw_pass(psl->volumetric_integrate_ps);

		/* Resolve at fullres */
		DRW_framebuffer_texture_detach(dtxl->depth);
		DRW_framebuffer_bind(fbl->main);
		if (sldata->volumetrics->use_colored_transmit) {
			DRW_draw_pass(psl->volumetric_resolve_transmit_ps);
		}
		DRW_draw_pass(psl->volumetric_resolve_ps);

		/* Restore */
		DRW_framebuffer_texture_attach(fbl->main, dtxl->depth, 0, 0);
		DRW_framebuffer_texture_detach(stl->g_data->volumetric);
		if (sldata->volumetrics->use_colored_transmit) {
			DRW_framebuffer_texture_detach(stl->g_data->volumetric_transmit);
		}

		/* Rebind main buffer after attach/detach operations */
		DRW_framebuffer_bind(fbl->main);
	}
}

void EEVEE_effects_do_refraction(EEVEE_SceneLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if ((effects->enabled_effects & EFFECT_REFRACT) != 0) {
		DRW_framebuffer_texture_attach(fbl->refract_fb, txl->refract_color, 0, 0);
		DRW_framebuffer_blit(fbl->main, fbl->refract_fb, false);
		EEVEE_downsample_buffer(vedata, fbl->downsample_fb, txl->refract_color, 9);
	}
}

void EEVEE_effects_do_ssr(EEVEE_SceneLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if (((effects->enabled_effects & EFFECT_SSR) != 0) && stl->g_data->valid_double_buffer) {
		DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
		e_data.depth_src = dtxl->depth;

		for (int i = 0; i < effects->ssr_ray_count; ++i) {
			DRW_framebuffer_texture_attach(fbl->screen_tracing_fb, stl->g_data->ssr_hit_output[i], i, 0);
		}
		DRW_framebuffer_bind(fbl->screen_tracing_fb);

		/* Raytrace. */
		DRW_draw_pass(psl->ssr_raytrace);

		for (int i = 0; i < effects->ssr_ray_count; ++i) {
			DRW_framebuffer_texture_detach(stl->g_data->ssr_hit_output[i]);
		}

		EEVEE_downsample_buffer(vedata, fbl->downsample_fb, txl->color_double_buffer, 9);

		/* Resolve at fullres */
		DRW_framebuffer_texture_detach(dtxl->depth);
		DRW_framebuffer_texture_detach(txl->ssr_normal_input);
		DRW_framebuffer_texture_detach(txl->ssr_specrough_input);
		DRW_framebuffer_bind(fbl->main);
		DRW_draw_pass(psl->ssr_resolve);

		/* Restore */
		DRW_framebuffer_texture_attach(fbl->main, dtxl->depth, 0, 0);
		DRW_framebuffer_texture_attach(fbl->main, txl->ssr_normal_input, 1, 0);
		DRW_framebuffer_texture_attach(fbl->main, txl->ssr_specrough_input, 2, 0);
	}

	if ((effects->enabled_effects & EFFECT_GTAO) != 0 && G.debug_value == 6) {
		/* GTAO Debug */
		DRW_framebuffer_texture_attach(fbl->gtao_debug_fb, stl->g_data->gtao_horizons_debug, 0, 0);
		DRW_framebuffer_bind(fbl->gtao_debug_fb);

		DRW_draw_pass(psl->ao_horizon_debug);

		/* Restore */
		DRW_framebuffer_texture_detach(stl->g_data->gtao_horizons_debug);
	}

	DRW_framebuffer_bind(fbl->main);
}

void EEVEE_effects_do_gtao(EEVEE_SceneLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if ((effects->enabled_effects & EFFECT_GTAO) != 0) {
		DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
		e_data.depth_src = dtxl->depth;

		DRW_stats_group_start("GTAO Horizon Scan");
		for (effects->ao_sample_nbr = 0.0;
		     effects->ao_sample_nbr < effects->ao_samples;
		     ++effects->ao_sample_nbr)
		{
			DRW_framebuffer_texture_detach(txl->gtao_horizons);
			DRW_framebuffer_texture_layer_attach(fbl->gtao_fb, txl->gtao_horizons, 0, (int)effects->ao_sample_nbr, 0);
			DRW_framebuffer_bind(fbl->gtao_fb);

			DRW_draw_pass(psl->ao_horizon_search);
		}
		DRW_stats_group_end();

		/* Restore */
		DRW_framebuffer_bind(fbl->main);
	}
}

#define SWAP_DOUBLE_BUFFERS() {                                       \
	if (swap_double_buffer) {                                         \
		SWAP(struct GPUFrameBuffer *, fbl->main, fbl->double_buffer); \
		SWAP(GPUTexture *, txl->color, txl->color_double_buffer);     \
		swap_double_buffer = false;                                   \
	}                                                                 \
} ((void)0)

#define SWAP_BUFFERS() {                           \
	if (effects->target_buffer != fbl->main) {     \
		SWAP_DOUBLE_BUFFERS();                     \
		effects->source_buffer = txl->color_post;  \
		effects->target_buffer = fbl->main;        \
	}                                              \
	else {                                         \
		SWAP_DOUBLE_BUFFERS();                     \
		effects->source_buffer = txl->color;       \
		effects->target_buffer = fbl->effect_fb;   \
	}                                              \
} ((void)0)

void EEVEE_draw_effects(EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	/* only once per frame after the first post process */
	bool swap_double_buffer = ((effects->enabled_effects & EFFECT_DOUBLE_BUFFER) != 0);

	/* Default framebuffer and texture */
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	/* Init pointers */
	effects->source_buffer = txl->color; /* latest updated texture */
	effects->target_buffer = fbl->effect_fb; /* next target to render to */

	/* Temporal Anti-Aliasing */
	/* MUST COME FIRST. */
	if ((effects->enabled_effects & EFFECT_TAA) != 0) {
		if (effects->taa_current_sample != 1) {
			DRW_framebuffer_bind(fbl->effect_fb);
			DRW_draw_pass(psl->taa_resolve);

			/* Restore the depth from sample 1. */
			DRW_framebuffer_blit(fbl->depth_double_buffer_fb, fbl->main, true);

			/* Special Swap */
			SWAP(struct GPUFrameBuffer *, fbl->effect_fb, fbl->double_buffer);
			SWAP(GPUTexture *, txl->color_post, txl->color_double_buffer);
			swap_double_buffer = false;
			effects->source_buffer = txl->color_double_buffer;
			effects->target_buffer = fbl->main;
		}
		else {
			/* Save the depth buffer for the next frame.
			 * This saves us from doing anything special
			 * in the other mode engines. */
			DRW_framebuffer_blit(fbl->main, fbl->depth_double_buffer_fb, true);
		}

		if ((effects->taa_total_sample == 0) ||
		    (effects->taa_current_sample < effects->taa_total_sample))
		{
			DRW_viewport_request_redraw();
		}
	}

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
			copy_v2_v2(effects->unf_source_texel_size, effects->downsamp_texel_size[i - 1]);
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

	/* Debug : Ouput buffer to view. */
	if ((G.debug_value > 0) && (G.debug_value <= 6)) {
		switch (G.debug_value) {
			case 1:
				if (txl->maxzbuffer) DRW_transform_to_display(txl->maxzbuffer);
				break;
			case 2:
				if (stl->g_data->ssr_hit_output[0]) DRW_transform_to_display(stl->g_data->ssr_hit_output[0]);
				break;
			case 3:
				if (txl->ssr_normal_input) DRW_transform_to_display(txl->ssr_normal_input);
				break;
			case 4:
				if (txl->ssr_specrough_input) DRW_transform_to_display(txl->ssr_specrough_input);
				break;
			case 5:
				if (txl->color_double_buffer) DRW_transform_to_display(txl->color_double_buffer);
				break;
			case 6:
				if (stl->g_data->gtao_horizons_debug) DRW_transform_to_display(stl->g_data->gtao_horizons_debug);
				break;
			default:
				break;
		}
	}

	/* If no post processes is enabled, buffers are still not swapped, do it now. */
	SWAP_DOUBLE_BUFFERS();

	if (!stl->g_data->valid_double_buffer &&
	    ((effects->enabled_effects & EFFECT_DOUBLE_BUFFER) != 0) &&
	    (DRW_state_is_image_render() == false))
	{
		/* If history buffer is not valid request another frame.
		 * This fix black reflections on area resize. */
		DRW_viewport_request_redraw();
	}

	/* Record pers matrix for the next frame. */
	DRW_viewport_matrix_get(stl->g_data->prev_persmat, DRW_MAT_PERS);

	/* Update double buffer status if render mode. */
	if (DRW_state_is_image_render()) {
		stl->g_data->valid_double_buffer = (txl->color_double_buffer != NULL);
	}
}

void EEVEE_effects_free(void)
{
	for (int i = 0; i < SSR_MAX_SHADER; ++i) {
		DRW_SHADER_FREE_SAFE(e_data.ssr_sh[i]);
	}
	DRW_SHADER_FREE_SAFE(e_data.downsample_sh);
	DRW_SHADER_FREE_SAFE(e_data.downsample_cube_sh);

	DRW_SHADER_FREE_SAFE(e_data.taa_resolve_sh);

	DRW_SHADER_FREE_SAFE(e_data.gtao_sh);
	DRW_SHADER_FREE_SAFE(e_data.gtao_debug_sh);

	DRW_SHADER_FREE_SAFE(e_data.volumetric_upsample_sh);

	DRW_SHADER_FREE_SAFE(e_data.minz_downlevel_sh);
	DRW_SHADER_FREE_SAFE(e_data.maxz_downlevel_sh);
	DRW_SHADER_FREE_SAFE(e_data.minz_downdepth_sh);
	DRW_SHADER_FREE_SAFE(e_data.maxz_downdepth_sh);
	DRW_SHADER_FREE_SAFE(e_data.minz_downdepth_layer_sh);
	DRW_SHADER_FREE_SAFE(e_data.maxz_downdepth_layer_sh);
	DRW_SHADER_FREE_SAFE(e_data.minz_copydepth_sh);
	DRW_SHADER_FREE_SAFE(e_data.maxz_copydepth_sh);

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
