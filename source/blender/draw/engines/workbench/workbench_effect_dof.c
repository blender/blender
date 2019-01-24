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

/** \file workbench_effect_dof.c
 *  \ingroup draw_engine
 */

#include "workbench_private.h"

#include "BKE_camera.h"
#include "DEG_depsgraph_query.h"

#include "DNA_camera_types.h"

/* *********** STATIC *********** */
static struct {
	struct GPUShader *effect_dof_prepare_sh;
	struct GPUShader *effect_dof_flatten_v_sh;
	struct GPUShader *effect_dof_flatten_h_sh;
	struct GPUShader *effect_dof_dilate_v_sh;
	struct GPUShader *effect_dof_dilate_h_sh;
	struct GPUShader *effect_dof_blur1_sh;
	struct GPUShader *effect_dof_blur2_sh;
	struct GPUShader *effect_dof_resolve_sh;
} e_data = {NULL};

/* Shaders */
extern char datatoc_workbench_effect_dof_frag_glsl[];

/* *********** Functions *********** */
void workbench_dof_engine_init(WORKBENCH_Data *vedata, Object *camera)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	WORKBENCH_FramebufferList *fbl = vedata->fbl;

	if (camera == NULL) {
		wpd->dof_enabled = false;
		return;
	}

	if (e_data.effect_dof_prepare_sh == NULL) {
		e_data.effect_dof_prepare_sh = DRW_shader_create_fullscreen(
		        datatoc_workbench_effect_dof_frag_glsl,
		        "#define PREPARE\n");

		e_data.effect_dof_flatten_v_sh = DRW_shader_create_fullscreen(
		        datatoc_workbench_effect_dof_frag_glsl,
		        "#define FLATTEN_VERTICAL\n");

		e_data.effect_dof_flatten_h_sh = DRW_shader_create_fullscreen(
		        datatoc_workbench_effect_dof_frag_glsl,
		        "#define FLATTEN_HORIZONTAL\n");

		e_data.effect_dof_dilate_v_sh = DRW_shader_create_fullscreen(
		        datatoc_workbench_effect_dof_frag_glsl,
		        "#define DILATE_VERTICAL\n");

		e_data.effect_dof_dilate_h_sh = DRW_shader_create_fullscreen(
		        datatoc_workbench_effect_dof_frag_glsl,
		        "#define DILATE_HORIZONTAL\n");

		e_data.effect_dof_blur1_sh = DRW_shader_create_fullscreen(
		        datatoc_workbench_effect_dof_frag_glsl,
		        "#define BLUR1\n");

		e_data.effect_dof_blur2_sh = DRW_shader_create_fullscreen(
		        datatoc_workbench_effect_dof_frag_glsl,
		        "#define BLUR2\n");

		e_data.effect_dof_resolve_sh = DRW_shader_create_fullscreen(
		        datatoc_workbench_effect_dof_frag_glsl,
		        "#define RESOLVE\n");
	}

	const float *full_size = DRW_viewport_size_get();
	int size[2] = {full_size[0] / 2, full_size[1] / 2};
	/* NOTE: We Ceil here in order to not miss any edge texel if using a NPO2 texture.  */
	int shrink_h_size[2] = {ceilf(size[0] / 8.0f), size[1]};
	int shrink_w_size[2] = {shrink_h_size[0], ceilf(size[1] / 8.0f)};

	wpd->half_res_col_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_R11F_G11F_B10F, &draw_engine_workbench_solid);
	wpd->dof_blur_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_R11F_G11F_B10F, &draw_engine_workbench_solid);
	wpd->coc_halfres_tx  = DRW_texture_pool_query_2D(size[0], size[1], GPU_RG8, &draw_engine_workbench_solid);
	wpd->coc_temp_tx     = DRW_texture_pool_query_2D(shrink_h_size[0], shrink_h_size[1], GPU_RG8, &draw_engine_workbench_solid);
	wpd->coc_tiles_tx[0] = DRW_texture_pool_query_2D(shrink_w_size[0], shrink_w_size[1], GPU_RG8, &draw_engine_workbench_solid);
	wpd->coc_tiles_tx[1] = DRW_texture_pool_query_2D(shrink_w_size[0], shrink_w_size[1], GPU_RG8, &draw_engine_workbench_solid);

	GPU_framebuffer_ensure_config(&fbl->dof_downsample_fb, {
		GPU_ATTACHMENT_NONE,
		GPU_ATTACHMENT_TEXTURE(wpd->half_res_col_tx),
		GPU_ATTACHMENT_TEXTURE(wpd->coc_halfres_tx),
	});
	GPU_framebuffer_ensure_config(&fbl->dof_coc_tile_h_fb, {
		GPU_ATTACHMENT_NONE,
		GPU_ATTACHMENT_TEXTURE(wpd->coc_temp_tx),
	});
	GPU_framebuffer_ensure_config(&fbl->dof_coc_tile_v_fb, {
		GPU_ATTACHMENT_NONE,
		GPU_ATTACHMENT_TEXTURE(wpd->coc_tiles_tx[0]),
	});
	GPU_framebuffer_ensure_config(&fbl->dof_coc_dilate_fb, {
		GPU_ATTACHMENT_NONE,
		GPU_ATTACHMENT_TEXTURE(wpd->coc_tiles_tx[1]),
	});
	GPU_framebuffer_ensure_config(&fbl->dof_blur1_fb, {
		GPU_ATTACHMENT_NONE,
		GPU_ATTACHMENT_TEXTURE(wpd->dof_blur_tx),
	});
	GPU_framebuffer_ensure_config(&fbl->dof_blur2_fb, {
		GPU_ATTACHMENT_NONE,
		GPU_ATTACHMENT_TEXTURE(wpd->half_res_col_tx),
	});

	{
		const DRWContextState *draw_ctx = DRW_context_state_get();
		const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);
		RegionView3D *rv3d = draw_ctx->rv3d;
		Camera *cam = (Camera *)camera->data;

		/* Parameters */
		/* TODO UI Options */
		float fstop = cam->gpu_dof.fstop;
		float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
		float focus_dist = BKE_camera_object_dof_distance(camera);
		float focal_len = cam->lens;

		/* TODO(fclem) deduplicate with eevee */

		/* this is factor that converts to the scene scale. focal length and sensor are expressed in mm
		 * unit.scale_length is how many meters per blender unit we have. We want to convert to blender units though
		 * because the shader reads coordinates in world space, which is in blender units.
		 * Note however that focus_distance is already in blender units and shall not be scaled here (see T48157). */
		float scale = (scene_eval->unit.system) ? scene_eval->unit.scale_length : 1.0f;
		float scale_camera = 0.001f / scale;
		/* we want radius here for the aperture number  */
		float aperture = 0.5f * scale_camera * focal_len / fstop;
		float focal_len_scaled = scale_camera * focal_len;
		float sensor_scaled = scale_camera * sensor;

		if (rv3d != NULL) {
			sensor_scaled *= rv3d->viewcamtexcofac[0];
		}

		wpd->dof_aperturesize = aperture * fabsf(focal_len_scaled / (focus_dist - focal_len_scaled));
		wpd->dof_distance = -focus_dist;
		wpd->dof_invsensorsize = full_size[0] / sensor_scaled;

		wpd->dof_near_far[0] = -cam->clipsta;
		wpd->dof_near_far[1] = -cam->clipend;
	}

	wpd->dof_enabled = true;
}

void workbench_dof_create_pass(WORKBENCH_Data *vedata, GPUTexture **dof_input)
{
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();

	if (!wpd->dof_enabled) {
		return;
	}

	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	psl->dof_down_ps = DRW_pass_create("DoF DownSample", DRW_STATE_WRITE_COLOR);
	psl->dof_flatten_h_ps = DRW_pass_create("DoF Flatten Coc H", DRW_STATE_WRITE_COLOR);
	psl->dof_flatten_v_ps = DRW_pass_create("DoF Flatten Coc V", DRW_STATE_WRITE_COLOR);
	psl->dof_dilate_h_ps = DRW_pass_create("DoF Dilate Coc H", DRW_STATE_WRITE_COLOR);
	psl->dof_dilate_v_ps = DRW_pass_create("DoF Dilate Coc V", DRW_STATE_WRITE_COLOR);
	psl->dof_blur1_ps = DRW_pass_create("DoF Blur 1", DRW_STATE_WRITE_COLOR);
	psl->dof_blur2_ps = DRW_pass_create("DoF Blur 2", DRW_STATE_WRITE_COLOR);
	psl->dof_resolve_ps = DRW_pass_create("DoF Resolve", DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND);

	{
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_prepare_sh, psl->dof_down_ps);
		DRW_shgroup_uniform_texture_ref(grp, "sceneColorTex", dof_input);
		DRW_shgroup_uniform_texture(grp, "sceneDepthTex", dtxl->depth);
		DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
		DRW_shgroup_uniform_vec3(grp, "dofParams", &wpd->dof_aperturesize, 1);
		DRW_shgroup_uniform_vec2(grp, "nearFar", wpd->dof_near_far, 1);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
	{
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_flatten_h_sh, psl->dof_flatten_h_ps);
		DRW_shgroup_uniform_texture(grp, "inputCocTex", wpd->coc_halfres_tx);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
	{
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_flatten_v_sh, psl->dof_flatten_v_ps);
		DRW_shgroup_uniform_texture(grp, "inputCocTex", wpd->coc_temp_tx);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
	{
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_dilate_v_sh, psl->dof_dilate_v_ps);
		DRW_shgroup_uniform_texture(grp, "inputCocTex", wpd->coc_tiles_tx[0]);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
	{
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_dilate_h_sh, psl->dof_dilate_h_ps);
		DRW_shgroup_uniform_texture(grp, "inputCocTex", wpd->coc_tiles_tx[1]);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
	{
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_blur1_sh, psl->dof_blur1_ps);
		DRW_shgroup_uniform_texture(grp, "inputCocTex", wpd->coc_halfres_tx);
		DRW_shgroup_uniform_texture(grp, "maxCocTilesTex", wpd->coc_tiles_tx[0]);
		DRW_shgroup_uniform_texture(grp, "halfResColorTex", wpd->half_res_col_tx);
		DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
	{
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_blur2_sh, psl->dof_blur2_ps);
		DRW_shgroup_uniform_texture(grp, "inputCocTex", wpd->coc_halfres_tx);
		DRW_shgroup_uniform_texture(grp, "blurTex", wpd->dof_blur_tx);
		DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
	{
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_dof_resolve_sh, psl->dof_resolve_ps);
		DRW_shgroup_uniform_texture(grp, "halfResColorTex", wpd->half_res_col_tx);
		DRW_shgroup_uniform_texture(grp, "sceneDepthTex", dtxl->depth);
		DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
		DRW_shgroup_uniform_vec3(grp, "dofParams", &wpd->dof_aperturesize, 1);
		DRW_shgroup_uniform_vec2(grp, "nearFar", wpd->dof_near_far, 1);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
}

void workbench_dof_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.effect_dof_prepare_sh);
	DRW_SHADER_FREE_SAFE(e_data.effect_dof_flatten_v_sh);
	DRW_SHADER_FREE_SAFE(e_data.effect_dof_flatten_h_sh);
	DRW_SHADER_FREE_SAFE(e_data.effect_dof_dilate_v_sh);
	DRW_SHADER_FREE_SAFE(e_data.effect_dof_dilate_h_sh);
	DRW_SHADER_FREE_SAFE(e_data.effect_dof_blur1_sh);
	DRW_SHADER_FREE_SAFE(e_data.effect_dof_blur2_sh);
	DRW_SHADER_FREE_SAFE(e_data.effect_dof_resolve_sh);
}

void workbench_dof_draw_pass(WORKBENCH_Data *vedata)
{
	WORKBENCH_FramebufferList *fbl = vedata->fbl;
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_PrivateData *wpd = stl->g_data;

	if (!wpd->dof_enabled) {
		return;
	}

	GPU_framebuffer_bind(fbl->dof_downsample_fb);
	DRW_draw_pass(psl->dof_down_ps);

	GPU_framebuffer_bind(fbl->dof_coc_tile_h_fb);
	DRW_draw_pass(psl->dof_flatten_h_ps);

	GPU_framebuffer_bind(fbl->dof_coc_tile_v_fb);
	DRW_draw_pass(psl->dof_flatten_v_ps);

	GPU_framebuffer_bind(fbl->dof_coc_dilate_fb);
	DRW_draw_pass(psl->dof_dilate_v_ps);

	GPU_framebuffer_bind(fbl->dof_coc_tile_v_fb);
	DRW_draw_pass(psl->dof_dilate_h_ps);

	GPU_framebuffer_bind(fbl->dof_blur1_fb);
	DRW_draw_pass(psl->dof_blur1_ps);

	GPU_framebuffer_bind(fbl->dof_blur2_fb);
	DRW_draw_pass(psl->dof_blur2_ps);

	GPU_framebuffer_bind(fbl->color_only_fb);
	DRW_draw_pass(psl->dof_resolve_ps);
}
