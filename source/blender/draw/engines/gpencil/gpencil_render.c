/*
 * Copyright 2017, Blender Foundation.
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
 * Contributor(s): Antonio Vazquez
 *
 */

/** \file blender/draw/engines/gpencil/gpencil_render.c
 *  \ingroup draw
 */
#include "BLI_rect.h"

#include "DRW_engine.h"
#include "DRW_render.h"

#include "BKE_camera.h"

#include "DNA_gpencil_types.h"

#include "DEG_depsgraph_query.h"

#include "draw_mode_engines.h"

#include "RE_pipeline.h"

#include "gpencil_engine.h"

/* Get pixel size for render
* This function uses the same calculation used for viewport, because if use
* camera pixelsize, the result is not correct.
*/
static float get_render_pixelsize(float persmat[4][4], int winx, int winy)
{
	float v1[3], v2[3];
	float len_px, len_sc;

	v1[0] = persmat[0][0];
	v1[1] = persmat[1][0];
	v1[2] = persmat[2][0];

	v2[0] = persmat[0][1];
	v2[1] = persmat[1][1];
	v2[2] = persmat[2][1];

	len_px = 2.0f / sqrtf(min_ff(len_squared_v3(v1), len_squared_v3(v2)));
	len_sc = (float)MAX2(winx, winy);

	return len_px / len_sc;
}

/* init render data */
void GPENCIL_render_init(GPENCIL_Data *ved, RenderEngine *engine, struct Depsgraph *depsgraph)
{
	GPENCIL_Data *vedata = (GPENCIL_Data *)ved;
	GPENCIL_StorageList *stl = vedata->stl;
	GPENCIL_FramebufferList *fbl = vedata->fbl;

	Scene *scene = DEG_get_evaluated_scene(depsgraph);
	const float *viewport_size = DRW_viewport_size_get();
	const int size[2] = { (int)viewport_size[0], (int)viewport_size[1] };

	/* In render mode the default framebuffer is not generated
	* because there is no viewport. So we need to manually create one
	* NOTE : use 32 bit format for precision in render mode.
	*/
	/* create multiframe framebuffer for AA */
	if (U.gpencil_multisamples > 0) {
		int rect_w = (int)viewport_size[0];
		int rect_h = (int)viewport_size[1];
		DRW_gpencil_multisample_ensure(vedata, rect_w, rect_h);
	}

	vedata->render_depth_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_DEPTH24_STENCIL8,
		&draw_engine_gpencil_type);
	vedata->render_color_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RGBA32F,
		&draw_engine_gpencil_type);
	GPU_framebuffer_ensure_config(&fbl->main, {
		GPU_ATTACHMENT_TEXTURE(vedata->render_depth_tx),
		GPU_ATTACHMENT_TEXTURE(vedata->render_color_tx)
		});

	/* Alloc transient data. */
	if (!stl->g_data) {
		stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
	}

	/* Set the pers & view matrix. */
	struct Object *camera = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));
	float frame = BKE_scene_frame_get(scene);
	RE_GetCameraWindow(engine->re, camera, frame, stl->storage->winmat);
	RE_GetCameraModelMatrix(engine->re, camera, stl->storage->viewinv);

	invert_m4_m4(stl->storage->viewmat, stl->storage->viewinv);
	mul_m4_m4m4(stl->storage->persmat, stl->storage->winmat, stl->storage->viewmat);
	invert_m4_m4(stl->storage->persinv, stl->storage->persmat);
	invert_m4_m4(stl->storage->wininv, stl->storage->winmat);

	DRW_viewport_matrix_override_set(stl->storage->persmat, DRW_MAT_PERS);
	DRW_viewport_matrix_override_set(stl->storage->persinv, DRW_MAT_PERSINV);
	DRW_viewport_matrix_override_set(stl->storage->winmat, DRW_MAT_WIN);
	DRW_viewport_matrix_override_set(stl->storage->wininv, DRW_MAT_WININV);
	DRW_viewport_matrix_override_set(stl->storage->viewmat, DRW_MAT_VIEW);
	DRW_viewport_matrix_override_set(stl->storage->viewinv, DRW_MAT_VIEWINV);

	/* calculate pixel size for render */
	stl->storage->render_pixsize = get_render_pixelsize(stl->storage->persmat, viewport_size[0], viewport_size[1]);
	/* INIT CACHE */
	GPENCIL_cache_init(vedata);
}

/* render all objects and select only grease pencil */
static void GPENCIL_render_cache(
	void *vedata, struct Object *ob,
	struct RenderEngine *UNUSED(engine), struct Depsgraph *UNUSED(depsgraph))
{
	if ((ob == NULL) || (DRW_check_object_visible_within_active_context(ob) == false)) {
		return;
	}

	if (ob->type == OB_GPENCIL) {
		GPENCIL_cache_populate(vedata, ob);
	}
}

/* TODO: Reuse Eevee code in shared module instead to duplicate here */
static void GPENCIL_render_update_viewvecs(float invproj[4][4], float winmat[4][4], float(*r_viewvecs)[4])
{
	/* view vectors for the corners of the view frustum.
	* Can be used to recreate the world space position easily */
	float view_vecs[4][4] = {
		{ -1.0f, -1.0f, -1.0f, 1.0f },
	{ 1.0f, -1.0f, -1.0f, 1.0f },
	{ -1.0f,  1.0f, -1.0f, 1.0f },
	{ -1.0f, -1.0f,  1.0f, 1.0f }
	};

	/* convert the view vectors to view space */
	const bool is_persp = (winmat[3][3] == 0.0f);
	for (int i = 0; i < 4; i++) {
		mul_project_m4_v3(invproj, view_vecs[i]);
		/* normalized trick see:
		* http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
		if (is_persp) {
			/* Divide XY by Z. */
			mul_v2_fl(view_vecs[i], 1.0f / view_vecs[i][2]);
		}
	}

	/**
	* If ortho : view_vecs[0] is the near-bottom-left corner of the frustum and
	*            view_vecs[1] is the vector going from the near-bottom-left corner to
	*            the far-top-right corner.
	* If Persp : view_vecs[0].xy and view_vecs[1].xy are respectively the bottom-left corner
	*            when Z = 1, and top-left corner if Z = 1.
	*            view_vecs[0].z the near clip distance and view_vecs[1].z is the (signed)
	*            distance from the near plane to the far clip plane.
	**/
	copy_v4_v4(r_viewvecs[0], view_vecs[0]);

	/* we need to store the differences */
	r_viewvecs[1][0] = view_vecs[1][0] - view_vecs[0][0];
	r_viewvecs[1][1] = view_vecs[2][1] - view_vecs[0][1];
	r_viewvecs[1][2] = view_vecs[3][2] - view_vecs[0][2];
}

/* Update view_vecs */
static void GPENCIL_render_update_vecs(GPENCIL_Data *vedata)
{
	GPENCIL_StorageList *stl = vedata->stl;

	float invproj[4][4], winmat[4][4];
	DRW_viewport_matrix_get(winmat, DRW_MAT_WIN);
	DRW_viewport_matrix_get(invproj, DRW_MAT_WININV);

	/* this is separated to keep function equal to Eevee for future reuse of same code */
	GPENCIL_render_update_viewvecs(invproj, winmat, stl->storage->view_vecs);
}

/* read z-depth render result */
static void GPENCIL_render_result_z(struct RenderLayer *rl, const char *viewname, GPENCIL_Data *vedata, const rcti *rect)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	GPENCIL_StorageList *stl = vedata->stl;

	if ((view_layer->passflag & SCE_PASS_Z) != 0) {
		RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_Z, viewname);

		GPU_framebuffer_read_depth(vedata->fbl->main, rect->xmin, rect->ymin, BLI_rcti_size_x(rect), BLI_rcti_size_y(rect), rp->rect);

		bool is_persp = DRW_viewport_is_persp_get();

		GPENCIL_render_update_vecs(vedata);

		/* Convert ogl depth [0..1] to view Z [near..far] */
		for (int i = 0; i < BLI_rcti_size_x(rect) * BLI_rcti_size_y(rect); i++) {
			if (rp->rect[i] == 1.0f) {
				rp->rect[i] = 1e10f; /* Background */
			}
			else {
				if (is_persp) {
					rp->rect[i] = rp->rect[i] * 2.0f - 1.0f;
					rp->rect[i] = stl->storage->winmat[3][2] / (rp->rect[i] + stl->storage->winmat[2][2]);
				}
				else {
					rp->rect[i] = -stl->storage->view_vecs[0][2] + rp->rect[i] * -stl->storage->view_vecs[1][2];
				}
			}
		}
	}
}

/* read combined render result */
static void GPENCIL_render_result_combined(struct RenderLayer *rl, const char *viewname, GPENCIL_Data *vedata, const rcti *rect)
{
	RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_COMBINED, viewname);
	GPENCIL_FramebufferList *fbl = ((GPENCIL_Data *)vedata)->fbl;

	GPU_framebuffer_bind(fbl->main);
	GPU_framebuffer_read_color(vedata->fbl->main, rect->xmin, rect->ymin, BLI_rcti_size_x(rect), BLI_rcti_size_y(rect), 4, 0, rp->rect);
}

/* helper to blend pixels */
static void blend_pixel(float src[4], float dst[4])
{
	float alpha = src[3];

	/* use blend: GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA */
	dst[0] = (src[0] * alpha) + (dst[0] * (1.0f - alpha));
	dst[1] = (src[1] * alpha) + (dst[1] * (1.0f - alpha));
	dst[2] = (src[2] * alpha) + (dst[2] * (1.0f - alpha));
}

/* render grease pencil to image */
void GPENCIL_render_to_image(void *vedata, RenderEngine *engine, struct RenderLayer *render_layer, const rcti *rect)
{
	const char *viewname = RE_GetActiveRenderView(engine->re);
	const DRWContextState *draw_ctx = DRW_context_state_get();
	int imgsize = BLI_rcti_size_x(rect) * BLI_rcti_size_y(rect);

	/* save previous render data */
	RenderPass *rpass_color_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_COMBINED, viewname);
	RenderPass *rpass_depth_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_Z, viewname);
	float *src_rect_color_data = NULL;
	float *src_rect_depth_data = NULL;
	if ((rpass_color_src) && (rpass_depth_src) && (rpass_color_src->rect) && (rpass_depth_src->rect)) {
		src_rect_color_data = MEM_dupallocN(rpass_color_src->rect);
		src_rect_depth_data = MEM_dupallocN(rpass_depth_src->rect);
	}
	else {
		/* TODO: put this message in a better place */
		printf("Warning: To render grease pencil, enable Combined and Z passes.\n");
	}

	GPENCIL_engine_init(vedata);
	GPENCIL_render_init(vedata, engine, draw_ctx->depsgraph);

	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	Object *camera = DEG_get_evaluated_object(draw_ctx->depsgraph, RE_GetCamera(engine->re));
	stl->storage->camera = camera; /* save current camera */

	GPENCIL_FramebufferList *fbl = ((GPENCIL_Data *)vedata)->fbl;
	if (fbl->main) {
		GPU_framebuffer_texture_attach(fbl->main, ((GPENCIL_Data *)vedata)->render_depth_tx, 0, 0);
		GPU_framebuffer_texture_attach(fbl->main, ((GPENCIL_Data *)vedata)->render_color_tx, 0, 0);
		/* clean first time the buffer */
		float clearcol[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		GPU_framebuffer_bind(fbl->main);
		GPU_framebuffer_clear_color_depth(fbl->main, clearcol, 1.0f);
	}

	/* loop all objects and draw */
	DRW_render_object_iter(vedata, engine, draw_ctx->depsgraph, GPENCIL_render_cache);

	GPENCIL_cache_finish(vedata);
	GPENCIL_draw_scene(vedata);

	/* combined data */
	GPENCIL_render_result_combined(render_layer, viewname, vedata, rect);
	/* z-depth data */
	GPENCIL_render_result_z(render_layer, viewname, vedata, rect);

	/* detach textures */
	 if (fbl->main) {
	 	GPU_framebuffer_texture_detach(fbl->main, ((GPENCIL_Data *)vedata)->render_depth_tx);
	 	GPU_framebuffer_texture_detach(fbl->main, ((GPENCIL_Data *)vedata)->render_color_tx);
	 }

	/* merge previous render image with new GP image */
	if (src_rect_color_data) {
		RenderPass *rpass_color_gp = RE_pass_find_by_name(render_layer, RE_PASSNAME_COMBINED, viewname);
		RenderPass *rpass_depth_gp = RE_pass_find_by_name(render_layer, RE_PASSNAME_Z, viewname);
		float *gp_rect_color_data = rpass_color_gp->rect;
		float *gp_rect_depth_data = rpass_depth_gp->rect;
		float *gp_pixel_rgba;
		float *gp_pixel_depth;
		float *src_pixel_rgba;
		float *src_pixel_depth;
		float tmp[4];

		for (int i = 0; i < imgsize; i++) {
			gp_pixel_rgba = &gp_rect_color_data[i * 4];
			gp_pixel_depth = &gp_rect_depth_data[i];

			src_pixel_rgba = &src_rect_color_data[i * 4];
			src_pixel_depth = &src_rect_depth_data[i];

			/* check grease pencil render transparency */
			if (gp_pixel_rgba[3] > 0.0f) {
				copy_v4_v4(tmp, gp_pixel_rgba);
				if (src_pixel_rgba[3] > 0.0f) {
					/* copy source color on back */
					copy_v4_v4(gp_pixel_rgba, src_pixel_rgba);
					/* check z-depth */
					if (gp_pixel_depth[0] > src_pixel_depth[0]) {
						/* copy source z-depth */
						gp_pixel_depth[0] = src_pixel_depth[0];
						/* blend gp render */
						blend_pixel(tmp, gp_pixel_rgba);
						/* blend object on top */
						blend_pixel(src_pixel_rgba, gp_pixel_rgba);
					}
					else {
						/* blend gp render */
						blend_pixel(tmp, gp_pixel_rgba);
					}
				}
			}
			else {
				copy_v4_v4(gp_pixel_rgba, src_pixel_rgba);
				gp_pixel_depth[0] = src_pixel_depth[0];
			}
		}

		/* free memory */
		MEM_SAFE_FREE(src_rect_color_data);
		MEM_SAFE_FREE(src_rect_depth_data);
	}
}
