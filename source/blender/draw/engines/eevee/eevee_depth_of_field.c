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

/** \file eevee_depth_of_field.c
 *  \ingroup draw_engine
 *
 * Depth of field post process effect.
 */

#include "DRW_render.h"

#include "BLI_dynstr.h"
#include "BLI_rand.h"

#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_object_force.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_global.h" /* for G.debug_value */
#include "BKE_camera.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_animsys.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "eevee_private.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_texture.h"

#include "ED_screen.h"

static struct {
	/* Depth Of Field */
	struct GPUShader *dof_downsample_sh;
	struct GPUShader *dof_scatter_sh;
	struct GPUShader *dof_resolve_sh;
} e_data = {NULL}; /* Engine data */

extern char datatoc_effect_dof_vert_glsl[];
extern char datatoc_effect_dof_geom_glsl[];
extern char datatoc_effect_dof_frag_glsl[];

static void eevee_create_shader_depth_of_field(void)
{
	e_data.dof_downsample_sh = DRW_shader_create(datatoc_effect_dof_vert_glsl, NULL,
	                                             datatoc_effect_dof_frag_glsl, "#define STEP_DOWNSAMPLE\n");
	e_data.dof_scatter_sh = DRW_shader_create(datatoc_effect_dof_vert_glsl, NULL,
	                                          datatoc_effect_dof_frag_glsl, "#define STEP_SCATTER\n");
	e_data.dof_resolve_sh = DRW_shader_create(datatoc_effect_dof_vert_glsl, NULL,
	                                          datatoc_effect_dof_frag_glsl, "#define STEP_RESOLVE\n");
}

int EEVEE_depth_of_field_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;

	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);

	if (BKE_collection_engine_property_value_get_bool(props, "dof_enable")) {
		Scene *scene = draw_ctx->scene;
		View3D *v3d = draw_ctx->v3d;
		RegionView3D *rv3d = draw_ctx->rv3d;

		if (!e_data.dof_downsample_sh) {
			eevee_create_shader_depth_of_field();
		}

		if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
			const float *viewport_size = DRW_viewport_size_get();
			Camera *cam = (Camera *)v3d->camera->data;

			/* Retreive Near and Far distance */
			effects->dof_near_far[0] = -cam->clipsta;
			effects->dof_near_far[1] = -cam->clipend;

			int buffer_size[2] = {(int)viewport_size[0] / 2, (int)viewport_size[1] / 2};

			/* Reuse buffer from Bloom if available */
			/* WATCH IT : must have the same size */
			struct GPUTexture **dof_down_near;

			if ((effects->enabled_effects & EFFECT_BLOOM) != 0) {
				dof_down_near = &txl->bloom_downsample[0];
			}
			else {
				dof_down_near = &txl->dof_down_near;
			}

			/* Setup buffers */
			DRWFboTexture tex_down[3] = {
				{dof_down_near, DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER}, /* filter to not interfeer with bloom */
				{&txl->dof_down_far, DRW_TEX_RGB_11_11_10, 0},
				{&txl->dof_coc, DRW_TEX_RG_16, 0},
			};
			DRW_framebuffer_init(
			        &fbl->dof_down_fb, &draw_engine_eevee_type,
			        buffer_size[0], buffer_size[1], tex_down, 3);

			/* Go full 32bits for rendering and reduce the color artifacts. */
			DRWTextureFormat fb_format = DRW_state_is_image_render() ? DRW_TEX_RGBA_32 : DRW_TEX_RGBA_16;

			DRWFboTexture tex_scatter_far = {&txl->dof_far_blur, fb_format, DRW_TEX_FILTER};
			DRW_framebuffer_init(
			        &fbl->dof_scatter_far_fb, &draw_engine_eevee_type,
			        buffer_size[0], buffer_size[1], &tex_scatter_far, 1);

			DRWFboTexture tex_scatter_near = {&txl->dof_near_blur, fb_format, DRW_TEX_FILTER};
			DRW_framebuffer_init(
			        &fbl->dof_scatter_near_fb, &draw_engine_eevee_type,
			        buffer_size[0], buffer_size[1], &tex_scatter_near, 1);

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

			return EFFECT_DOF | EFFECT_POST_BUFFER;
		}
	}

	/* Cleanup to release memory */
	DRW_TEXTURE_FREE_SAFE(txl->dof_down_near);
	DRW_TEXTURE_FREE_SAFE(txl->dof_down_far);
	DRW_TEXTURE_FREE_SAFE(txl->dof_coc);
	DRW_TEXTURE_FREE_SAFE(txl->dof_far_blur);
	DRW_TEXTURE_FREE_SAFE(txl->dof_near_blur);
	DRW_FRAMEBUFFER_FREE_SAFE(fbl->dof_down_fb);
	DRW_FRAMEBUFFER_FREE_SAFE(fbl->dof_scatter_far_fb);
	DRW_FRAMEBUFFER_FREE_SAFE(fbl->dof_scatter_near_fb);

	return 0;
}

void EEVEE_depth_of_field_cache_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	if ((effects->enabled_effects & EFFECT_DOF) != 0) {
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
		struct Gwn_Batch *quad = DRW_cache_fullscreen_quad_get();

		psl->dof_down = DRW_pass_create("DoF Downsample", DRW_STATE_WRITE_COLOR);

		grp = DRW_shgroup_create(e_data.dof_downsample_sh, psl->dof_down);
		DRW_shgroup_uniform_buffer(grp, "colorBuffer", &effects->source_buffer);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &dtxl->depth);
		DRW_shgroup_uniform_vec2(grp, "nearFar", effects->dof_near_far, 1);
		DRW_shgroup_uniform_vec3(grp, "dofParams", effects->dof_params, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->dof_scatter = DRW_pass_create("DoF Scatter", DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE_FULL);

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

void EEVEE_depth_of_field_draw(EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

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
}

void EEVEE_depth_of_field_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.dof_downsample_sh);
	DRW_SHADER_FREE_SAFE(e_data.dof_scatter_sh);
	DRW_SHADER_FREE_SAFE(e_data.dof_resolve_sh);
}
