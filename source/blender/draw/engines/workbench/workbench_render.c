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

/** \file workbench_render.c
 *  \ingroup draw_engine
 *
 * Render functions for final render output.
 */

#include "BLI_rect.h"

#include "BKE_report.h"

#include "DRW_render.h"

#include "GPU_shader.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RE_pipeline.h"

#include "workbench_private.h"

static void workbench_render_deferred_cache(
        void *vedata, struct Object *ob,
        struct RenderEngine *UNUSED(engine), struct Depsgraph *UNUSED(depsgraph))
{
	workbench_deferred_solid_cache_populate(vedata, ob);
}

static void workbench_render_forward_cache(
        void *vedata, struct Object *ob,
        struct RenderEngine *UNUSED(engine), struct Depsgraph *UNUSED(depsgraph))
{
	workbench_forward_cache_populate(vedata, ob);
}

static void workbench_render_matrices_init(RenderEngine *engine, Depsgraph *depsgraph)
{
	/* TODO(sergey): Shall render hold pointer to an evaluated camera instead? */
	Scene *scene = DEG_get_evaluated_scene(depsgraph);
	struct Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));
	float frame = BKE_scene_frame_get(scene);

	/* Set the persective, view and window matrix. */
	float winmat[4][4], wininv[4][4];
	float viewmat[4][4], viewinv[4][4];
	float persmat[4][4], persinv[4][4];

	RE_GetCameraWindow(engine->re, ob_camera_eval, frame, winmat);
	RE_GetCameraModelMatrix(engine->re, ob_camera_eval, viewinv);

	invert_m4_m4(viewmat, viewinv);
	mul_m4_m4m4(persmat, winmat, viewmat);
	invert_m4_m4(persinv, persmat);
	invert_m4_m4(wininv, winmat);

	DRW_viewport_matrix_override_set(persmat, DRW_MAT_PERS);
	DRW_viewport_matrix_override_set(persinv, DRW_MAT_PERSINV);
	DRW_viewport_matrix_override_set(winmat, DRW_MAT_WIN);
	DRW_viewport_matrix_override_set(wininv, DRW_MAT_WININV);
	DRW_viewport_matrix_override_set(viewmat, DRW_MAT_VIEW);
	DRW_viewport_matrix_override_set(viewinv, DRW_MAT_VIEWINV);
}

static bool workbench_render_framebuffers_init(void)
{
	/* For image render, allocate own buffers because we don't have a viewport. */
	const float *viewport_size = DRW_viewport_size_get();
	const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
	dtxl->color = GPU_texture_create_2D(size[0], size[1], GPU_RGBA8, NULL, NULL);
	dtxl->depth = GPU_texture_create_2D(size[0], size[1], GPU_DEPTH24_STENCIL8, NULL, NULL);

	if (!(dtxl->depth && dtxl->color)) {
		return false;
	}

	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

	GPU_framebuffer_ensure_config(&dfbl->default_fb, {
		GPU_ATTACHMENT_TEXTURE(dtxl->depth),
		GPU_ATTACHMENT_TEXTURE(dtxl->color)
	});

	GPU_framebuffer_ensure_config(&dfbl->depth_only_fb, {
		GPU_ATTACHMENT_TEXTURE(dtxl->depth),
		GPU_ATTACHMENT_NONE
	});

	GPU_framebuffer_ensure_config(&dfbl->color_only_fb, {
		GPU_ATTACHMENT_NONE,
		GPU_ATTACHMENT_TEXTURE(dtxl->color)
	});

	bool ok = true;
	ok = ok && GPU_framebuffer_check_valid(dfbl->default_fb, NULL);
	ok = ok && GPU_framebuffer_check_valid(dfbl->color_only_fb, NULL);
	ok = ok && GPU_framebuffer_check_valid(dfbl->depth_only_fb, NULL);

	return ok;
}

static void workbench_render_framebuffers_finish(void)
{
}

void workbench_render(WORKBENCH_Data *data, RenderEngine *engine, RenderLayer *render_layer, const rcti *rect)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const Scene *scene = draw_ctx->scene;
	Depsgraph *depsgraph = draw_ctx->depsgraph;
	workbench_render_matrices_init(engine, depsgraph);

	if (!workbench_render_framebuffers_init()) {
		RE_engine_report(engine, RPT_ERROR, "Failed to allocate OpenGL buffers");
		return;
	}

	const bool deferred = (scene->display.shading.flag & V3D_SHADING_XRAY) == 0;

	if (deferred)
	{
		/* Init engine. */
		workbench_deferred_engine_init(data);

		/* Init objects. */
		workbench_deferred_cache_init(data);
		DRW_render_object_iter(data, engine, depsgraph, workbench_render_deferred_cache);
		workbench_deferred_cache_finish(data);
		DRW_render_instance_buffer_finish();

		/* Draw. */
		int num_samples = workbench_taa_calculate_num_iterations(data);
		for (int sample = 0; sample < num_samples; sample++) {
			if (RE_engine_test_break(engine)) {
				break;
			}

			workbench_deferred_draw_background(data);
			workbench_deferred_draw_scene(data);
		}

		workbench_deferred_draw_finish(data);
	}
	else {
		/* Init engine. */
		workbench_forward_engine_init(data);

		/* Init objects. */
		workbench_forward_cache_init(data);
		DRW_render_object_iter(data, engine, depsgraph, workbench_render_forward_cache);
		workbench_forward_cache_finish(data);
		DRW_render_instance_buffer_finish();

		/* Draw. */
		int num_samples = workbench_taa_calculate_num_iterations(data);
		for (int sample = 0; sample < num_samples; sample++) {
			if (RE_engine_test_break(engine)) {
				break;
			}

			workbench_forward_draw_background(data);
			workbench_forward_draw_scene(data);
		}

		workbench_forward_draw_finish(data);
	}

	/* Write render output. */
	const char *viewname = RE_GetActiveRenderView(engine->re);
	RenderPass *rp = RE_pass_find_by_name(render_layer, RE_PASSNAME_COMBINED, viewname);

	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	GPU_framebuffer_bind(dfbl->color_only_fb);
	GPU_framebuffer_read_color(dfbl->color_only_fb,
	                           rect->xmin, rect->ymin,
	                           BLI_rcti_size_x(rect), BLI_rcti_size_y(rect),
	                           4, 0, rp->rect);

	workbench_render_framebuffers_finish();
}

void workbench_render_update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer)
{
	RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_COMBINED, 4, "RGBA", SOCK_RGBA);
}
