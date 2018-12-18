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

/** \file blender/draw/engines/gpencil/gpencil_engine.c
 *  \ingroup draw
 */
#include "DRW_engine.h"
#include "DRW_render.h"

#include "BKE_camera.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_gpencil.h"
#include "BKE_shader_fx.h"

#include "DNA_gpencil_types.h"
#include "DNA_view3d_types.h"

#include "draw_mode_engines.h"

#include "UI_resources.h"

#include "GPU_texture.h"

#include "gpencil_engine.h"

#include "DEG_depsgraph_query.h"

#include "ED_screen.h"
#include "ED_gpencil.h"

#include "WM_api.h"

extern char datatoc_gpencil_fill_vert_glsl[];
extern char datatoc_gpencil_fill_frag_glsl[];
extern char datatoc_gpencil_stroke_vert_glsl[];
extern char datatoc_gpencil_stroke_geom_glsl[];
extern char datatoc_gpencil_stroke_frag_glsl[];
extern char datatoc_gpencil_zdepth_mix_frag_glsl[];
extern char datatoc_gpencil_simple_mix_frag_glsl[];
extern char datatoc_gpencil_point_vert_glsl[];
extern char datatoc_gpencil_point_geom_glsl[];
extern char datatoc_gpencil_point_frag_glsl[];
extern char datatoc_gpencil_background_frag_glsl[];
extern char datatoc_gpencil_paper_frag_glsl[];
extern char datatoc_gpencil_edit_point_vert_glsl[];
extern char datatoc_gpencil_edit_point_geom_glsl[];
extern char datatoc_gpencil_edit_point_frag_glsl[];
extern char datatoc_gpencil_blend_frag_glsl[];

/* *********** STATIC *********** */
static GPENCIL_e_data e_data = {NULL}; /* Engine data */

/* *********** FUNCTIONS *********** */

/* create a multisample buffer if not present */
void DRW_gpencil_multisample_ensure(GPENCIL_Data *vedata, int rect_w, int rect_h)
{
	GPENCIL_FramebufferList *fbl = vedata->fbl;
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	GPENCIL_TextureList *txl = ((GPENCIL_Data *)vedata)->txl;

	short samples = stl->storage->multisamples;

	if (samples > 0) {
		if (!fbl->multisample_fb) {
			fbl->multisample_fb = GPU_framebuffer_create();
			if (fbl->multisample_fb) {
				if (txl->multisample_color == NULL) {
					txl->multisample_color = GPU_texture_create_2D_multisample(
					        rect_w, rect_h, GPU_RGBA16F, NULL, samples, NULL);
				}
				if (txl->multisample_depth == NULL) {
					txl->multisample_depth = GPU_texture_create_2D_multisample(
					        rect_w, rect_h, GPU_DEPTH_COMPONENT24, NULL, samples, NULL);
				}
				GPU_framebuffer_ensure_config(
				        &fbl->multisample_fb, {
				            GPU_ATTACHMENT_TEXTURE(txl->multisample_depth),
				            GPU_ATTACHMENT_TEXTURE(txl->multisample_color)
				        });
			}
		}
	}
}

static void GPENCIL_create_framebuffers(void *vedata)
{
	GPENCIL_FramebufferList *fbl = ((GPENCIL_Data *)vedata)->fbl;
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;

	/* Go full 32bits for rendering */
	GPUTextureFormat fb_format = DRW_state_is_image_render() ? GPU_RGBA32F : GPU_RGBA16F;

	if (DRW_state_is_fbo()) {
		const float *viewport_size = DRW_viewport_size_get();
		const int size[2] = { (int)viewport_size[0], (int)viewport_size[1] };

		/* create multiframe framebuffer for AA */
		if ((stl->storage->framebuffer_flag & GP_FRAMEBUFFER_MULTISAMPLE) &&
		    (stl->storage->multisamples > 0))
		{
			DRW_gpencil_multisample_ensure(vedata, size[0], size[1]);
		}

		/* Framebufers for basic object drawing */
		if (stl->storage->framebuffer_flag & GP_FRAMEBUFFER_BASIC) {
			/* temp textures for ping-pong buffers */
			e_data.temp_depth_tx_a = DRW_texture_pool_query_2D(
			        size[0], size[1], GPU_DEPTH_COMPONENT24,
			        &draw_engine_gpencil_type);
			e_data.temp_color_tx_a = DRW_texture_pool_query_2D(
			        size[0], size[1], fb_format,
			        &draw_engine_gpencil_type);
			GPU_framebuffer_ensure_config(
				&fbl->temp_fb_a, {
					GPU_ATTACHMENT_TEXTURE(e_data.temp_depth_tx_a),
					GPU_ATTACHMENT_TEXTURE(e_data.temp_color_tx_a)
				});

			e_data.temp_depth_tx_b = DRW_texture_pool_query_2D(
			        size[0], size[1], GPU_DEPTH_COMPONENT24,
			        &draw_engine_gpencil_type);
			e_data.temp_color_tx_b = DRW_texture_pool_query_2D(
			        size[0], size[1], fb_format,
			        &draw_engine_gpencil_type);
			GPU_framebuffer_ensure_config(
				&fbl->temp_fb_b, {
					GPU_ATTACHMENT_TEXTURE(e_data.temp_depth_tx_b),
					GPU_ATTACHMENT_TEXTURE(e_data.temp_color_tx_b)
				});

			/* used for FX effects and Layer blending */
			e_data.temp_depth_tx_fx = DRW_texture_pool_query_2D(
			        size[0], size[1], GPU_DEPTH_COMPONENT24,
			        &draw_engine_gpencil_type);
			e_data.temp_color_tx_fx = DRW_texture_pool_query_2D(
			        size[0], size[1], fb_format,
			        &draw_engine_gpencil_type);
			GPU_framebuffer_ensure_config(
				&fbl->temp_fb_fx, {
					GPU_ATTACHMENT_TEXTURE(e_data.temp_depth_tx_fx),
					GPU_ATTACHMENT_TEXTURE(e_data.temp_color_tx_fx),
				});
		}

		/* background framebuffer to speed up drawing process (always 16 bits) */
		if (stl->storage->framebuffer_flag & GP_FRAMEBUFFER_DRAW) {
			e_data.background_depth_tx = DRW_texture_pool_query_2D(
			        size[0], size[1], GPU_DEPTH_COMPONENT24,
			        &draw_engine_gpencil_type);
			e_data.background_color_tx = DRW_texture_pool_query_2D(
			        size[0], size[1], GPU_RGBA32F,
			        &draw_engine_gpencil_type);
			GPU_framebuffer_ensure_config(
				&fbl->background_fb, {
					GPU_ATTACHMENT_TEXTURE(e_data.background_depth_tx),
					GPU_ATTACHMENT_TEXTURE(e_data.background_color_tx)
				});
		}
	}
}

static void GPENCIL_create_shaders(void)
{
	/* normal fill shader */
	if (!e_data.gpencil_fill_sh) {
		e_data.gpencil_fill_sh = DRW_shader_create(
		        datatoc_gpencil_fill_vert_glsl, NULL,
		        datatoc_gpencil_fill_frag_glsl, NULL);
	}

	/* normal stroke shader using geometry to display lines (line mode) */
	if (!e_data.gpencil_stroke_sh) {
		e_data.gpencil_stroke_sh = DRW_shader_create(
		        datatoc_gpencil_stroke_vert_glsl,
		        datatoc_gpencil_stroke_geom_glsl,
		        datatoc_gpencil_stroke_frag_glsl,
		        NULL);
	}

	/* dot/rectangle mode for normal strokes using geometry */
	if (!e_data.gpencil_point_sh) {
		e_data.gpencil_point_sh = DRW_shader_create(
		        datatoc_gpencil_point_vert_glsl,
		        datatoc_gpencil_point_geom_glsl,
		        datatoc_gpencil_point_frag_glsl,
		        NULL);
	}
	/* used for edit points or strokes with one point only */
	if (!e_data.gpencil_edit_point_sh) {
		e_data.gpencil_edit_point_sh = DRW_shader_create(
		        datatoc_gpencil_edit_point_vert_glsl,
		        datatoc_gpencil_edit_point_geom_glsl,
		        datatoc_gpencil_edit_point_frag_glsl, NULL);
	}

	/* used for edit lines for edit modes */
	if (!e_data.gpencil_line_sh) {
		e_data.gpencil_line_sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_FLAT_COLOR);
	}

	/* used to filling during drawing */
	if (!e_data.gpencil_drawing_fill_sh) {
		e_data.gpencil_drawing_fill_sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_SMOOTH_COLOR);
	}

	/* full screen for mix zdepth*/
	if (!e_data.gpencil_fullscreen_sh) {
		e_data.gpencil_fullscreen_sh = DRW_shader_create_fullscreen(datatoc_gpencil_zdepth_mix_frag_glsl, NULL);
	}
	if (!e_data.gpencil_simple_fullscreen_sh) {
		e_data.gpencil_simple_fullscreen_sh = DRW_shader_create_fullscreen(datatoc_gpencil_simple_mix_frag_glsl, NULL);
	}

	/* blend */
	if (!e_data.gpencil_blend_fullscreen_sh) {
		e_data.gpencil_blend_fullscreen_sh = DRW_shader_create_fullscreen(datatoc_gpencil_blend_frag_glsl, NULL);
	}

	/* shaders for use when drawing */
	if (!e_data.gpencil_background_sh) {
		e_data.gpencil_background_sh = DRW_shader_create_fullscreen(datatoc_gpencil_background_frag_glsl, NULL);
	}
	if (!e_data.gpencil_paper_sh) {
		e_data.gpencil_paper_sh = DRW_shader_create_fullscreen(datatoc_gpencil_paper_frag_glsl, NULL);
	}
}

void GPENCIL_engine_init(void *vedata)
{
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	/* init storage */
	if (!stl->storage) {
		stl->storage = MEM_callocN(sizeof(GPENCIL_Storage), "GPENCIL_Storage");

		/* unit matrix */
		unit_m4(stl->storage->unit_matrix);
	}

	stl->storage->multisamples = U.gpencil_multisamples;

	/* create shaders */
	GPENCIL_create_shaders();
	GPENCIL_create_fx_shaders(&e_data);

	/* blank texture used if no texture defined for fill shader */
	if (!e_data.gpencil_blank_texture) {
		float rect[16][16][4] = {{{0.0f}}};
		e_data.gpencil_blank_texture = DRW_texture_create_2D(16, 16, GPU_RGBA8, DRW_TEX_FILTER, (float *)rect);
	}
}

static void GPENCIL_engine_free(void)
{
	/* only free custom shaders, builtin shaders are freed in blender close */
	DRW_SHADER_FREE_SAFE(e_data.gpencil_fill_sh);
	DRW_SHADER_FREE_SAFE(e_data.gpencil_stroke_sh);
	DRW_SHADER_FREE_SAFE(e_data.gpencil_point_sh);
	DRW_SHADER_FREE_SAFE(e_data.gpencil_edit_point_sh);
	DRW_SHADER_FREE_SAFE(e_data.gpencil_fullscreen_sh);
	DRW_SHADER_FREE_SAFE(e_data.gpencil_simple_fullscreen_sh);
	DRW_SHADER_FREE_SAFE(e_data.gpencil_blend_fullscreen_sh);
	DRW_SHADER_FREE_SAFE(e_data.gpencil_background_sh);
	DRW_SHADER_FREE_SAFE(e_data.gpencil_paper_sh);

	DRW_TEXTURE_FREE_SAFE(e_data.gpencil_blank_texture);

	GPU_BATCH_DISCARD_SAFE(e_data.batch_buffer_stroke);
	MEM_SAFE_FREE(e_data.batch_buffer_stroke);

	GPU_BATCH_DISCARD_SAFE(e_data.batch_buffer_fill);
	MEM_SAFE_FREE(e_data.batch_buffer_fill);

	GPU_BATCH_DISCARD_SAFE(e_data.batch_buffer_ctrlpoint);
	MEM_SAFE_FREE(e_data.batch_buffer_ctrlpoint);

	GPU_BATCH_DISCARD_SAFE(e_data.batch_grid);
	MEM_SAFE_FREE(e_data.batch_grid);

	/* effects */
	GPENCIL_delete_fx_shaders(&e_data);
}

void GPENCIL_cache_init(void *vedata)
{
	GPENCIL_PassList *psl = ((GPENCIL_Data *)vedata)->psl;
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene = draw_ctx->scene;
	View3D *v3d = draw_ctx->v3d;

	/* Special handling for when active object is GP object (e.g. for draw mode) */
	Object *obact = draw_ctx->obact;
	bGPdata *obact_gpd = NULL;
	MaterialGPencilStyle *gp_style = NULL;

	if (obact && (obact->type == OB_GPENCIL) && (obact->data)) {
		obact_gpd = (bGPdata *)obact->data;
		gp_style = BKE_material_gpencil_settings_get(obact, obact->actcol);
	}

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(g_data), "g_data");
		stl->storage->xray = GP_XRAY_FRONT; /* used for drawing */
		stl->storage->stroke_style = GP_STYLE_STROKE_STYLE_SOLID; /* used for drawing */
	}
	stl->storage->tonemapping = 0;

	stl->g_data->shgrps_edit_line = NULL;
	stl->g_data->shgrps_edit_point = NULL;

	if (!stl->shgroups) {
		/* Alloc maximum size because count strokes is very slow and can be very complex due onion skinning.
		 */
		stl->shgroups = MEM_mallocN(sizeof(GPENCIL_shgroup) * GPENCIL_MAX_SHGROUPS, "GPENCIL_shgroup");
	}

	/* init gp objects cache */
	stl->g_data->gp_cache_used = 0;
	stl->g_data->gp_cache_size = 0;
	stl->g_data->gp_object_cache = NULL;

	{
		/* Stroke pass */
		psl->stroke_pass = DRW_pass_create(
		        "GPencil Stroke Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS | DRW_STATE_BLEND);
		stl->storage->shgroup_id = 0;

		/* edit pass */
		psl->edit_pass = DRW_pass_create(
		        "GPencil Edit Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND);

		/* detect if playing animation */
		if (draw_ctx->evil_C) {

			bool playing = ED_screen_animation_playing(CTX_wm_manager(draw_ctx->evil_C)) != NULL;
			if (playing != stl->storage->is_playing) {
				stl->storage->reset_cache = true;
			}
			stl->storage->is_playing = playing;
		}
		else {
			stl->storage->is_playing = false;
			stl->storage->reset_cache = false;
		}
		/* save render state */
		stl->storage->is_render = DRW_state_is_image_render();
		stl->storage->is_mat_preview = (bool)stl->storage->is_render && STREQ(scene->id.name + 2, "preview");

		if (obact_gpd) {
			/* for some reason, when press play there is a delay in the animation flag check
			 * and this produces errors. To be sure, we set cache as dirty because the frame
			 * is changing.
			 */
			if (stl->storage->is_playing == true) {
				obact_gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
			}
			/* if render, set as dirty to update all data */
			else if (stl->storage->is_render == true) {
				obact_gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
			}
		}

		/* save simplify flags (can change while drawing, so it's better to save) */
		stl->storage->simplify_fill = GP_SIMPLIFY_FILL(scene, stl->storage->is_playing);
		stl->storage->simplify_modif = GP_SIMPLIFY_MODIF(scene, stl->storage->is_playing);
		stl->storage->simplify_fx = GP_SIMPLIFY_FX(scene, stl->storage->is_playing);
		stl->storage->simplify_blend = GP_SIMPLIFY_BLEND(scene, stl->storage->is_playing);

		/* save pixsize */
		stl->storage->pixsize = DRW_viewport_pixelsize_get();
		if ((!DRW_state_is_opengl_render()) && (stl->storage->is_render)) {
			stl->storage->pixsize = &stl->storage->render_pixsize;
		}

		/* detect if painting session */
		if ((obact_gpd) &&
		    (obact_gpd->flag & GP_DATA_STROKE_PAINTMODE) &&
		    (stl->storage->is_playing == false))
		{
			/* need the original to avoid cow overhead while drawing */
			bGPdata *gpd_orig = (bGPdata *)DEG_get_original_id(&obact_gpd->id);
			if (((gpd_orig->runtime.sbuffer_sflag & GP_STROKE_ERASER) == 0) &&
			    (gpd_orig->runtime.sbuffer_size > 0) &&
			    ((gpd_orig->flag & GP_DATA_STROKE_POLYGON) == 0))
			{
				stl->g_data->session_flag |= GP_DRW_PAINT_PAINTING;
			}
			else {
				stl->g_data->session_flag = GP_DRW_PAINT_IDLE;
			}
		}
		else {
			/* if not drawing mode */
			stl->g_data->session_flag = GP_DRW_PAINT_HOLD;
		}

		if (gp_style) {
			stl->storage->stroke_style = gp_style->stroke_style;
			stl->storage->color_type = GPENCIL_COLOR_SOLID;
			if (gp_style->stroke_style == GP_STYLE_STROKE_STYLE_TEXTURE) {
				stl->storage->color_type = GPENCIL_COLOR_TEXTURE;
				if (gp_style->flag & GP_STYLE_STROKE_PATTERN) {
					stl->storage->color_type = GPENCIL_COLOR_PATTERN;
				}
			}
		}
		else {
			stl->storage->stroke_style = GP_STYLE_STROKE_STYLE_SOLID;
			stl->storage->color_type = GPENCIL_COLOR_SOLID;
		}

		/* drawing buffer pass for drawing the stroke that is being drawing by the user. The data
		 * is stored in sbuffer
		 */
		psl->drawing_pass = DRW_pass_create(
		        "GPencil Drawing Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);

		/* full screen pass to combine the result with default framebuffer */
		struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();
		psl->mix_pass = DRW_pass_create(
		        "GPencil Mix Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
		DRWShadingGroup *mix_shgrp = DRW_shgroup_create(e_data.gpencil_fullscreen_sh, psl->mix_pass);
		DRW_shgroup_call_add(mix_shgrp, quad, NULL);
		DRW_shgroup_uniform_texture_ref(mix_shgrp, "strokeColor", &e_data.input_color_tx);
		DRW_shgroup_uniform_texture_ref(mix_shgrp, "strokeDepth", &e_data.input_depth_tx);
		DRW_shgroup_uniform_int(mix_shgrp, "tonemapping", &stl->storage->tonemapping, 1);

		/* mix pass no blend used to copy between passes. A separated pass is required
		 * because if mix_pass is used, the acumulation of blend degrade the colors.
		 *
		 * This pass is used too to take the snapshot used for background_pass. This image
		 * will be used as the background while the user is drawing.
		 */
		psl->mix_pass_noblend = DRW_pass_create(
		        "GPencil Mix Pass no blend",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
		DRWShadingGroup *mix_shgrp_noblend = DRW_shgroup_create(e_data.gpencil_fullscreen_sh, psl->mix_pass_noblend);
		DRW_shgroup_call_add(mix_shgrp_noblend, quad, NULL);
		DRW_shgroup_uniform_texture_ref(mix_shgrp_noblend, "strokeColor", &e_data.input_color_tx);
		DRW_shgroup_uniform_texture_ref(mix_shgrp_noblend, "strokeDepth", &e_data.input_depth_tx);
		DRW_shgroup_uniform_int(mix_shgrp_noblend, "tonemapping", &stl->storage->tonemapping, 1);

		/* Painting session pass (used only to speedup while the user is drawing )
		 * This pass is used to show the snapshot of the current grease pencil strokes captured
		 * when the user starts to draw (see comments above).
		 * In this way, the previous strokes don't need to be redraw and the drawing process
		 * is far to agile.
		 */
		psl->background_pass = DRW_pass_create(
		        "GPencil Background Painting Session Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
		DRWShadingGroup *background_shgrp = DRW_shgroup_create(e_data.gpencil_background_sh, psl->background_pass);
		DRW_shgroup_call_add(background_shgrp, quad, NULL);
		DRW_shgroup_uniform_texture_ref(background_shgrp, "strokeColor", &e_data.background_color_tx);
		DRW_shgroup_uniform_texture_ref(background_shgrp, "strokeDepth", &e_data.background_depth_tx);

		/* pass for drawing paper (only if viewport)
		 * In render, the v3d is null so the paper is disabled
		 * The paper is way to isolate the drawing in complex scene and to have a cleaner
		 * drawing area.
		 */
		if (v3d) {
			psl->paper_pass = DRW_pass_create(
			        "GPencil Paper Pass",
			        DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND);
			DRWShadingGroup *paper_shgrp = DRW_shgroup_create(e_data.gpencil_paper_sh, psl->paper_pass);
			DRW_shgroup_call_add(paper_shgrp, quad, NULL);
			DRW_shgroup_uniform_vec3(paper_shgrp, "color", v3d->shading.background_color, 1);
			DRW_shgroup_uniform_float(paper_shgrp, "opacity", &v3d->overlay.gpencil_paper_opacity, 1);
		}

		/* grid pass */
		if (v3d) {
			psl->grid_pass = DRW_pass_create(
			        "GPencil Grid Pass",
			        DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
			stl->g_data->shgrps_grid = DRW_shgroup_create(e_data.gpencil_line_sh, psl->grid_pass);
		}

		/* blend layers pass */
		psl->blend_pass = DRW_pass_create(
			"GPencil Blend Layers Pass",
			DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
		DRWShadingGroup *blend_shgrp = DRW_shgroup_create(e_data.gpencil_blend_fullscreen_sh, psl->blend_pass);
		DRW_shgroup_call_add(blend_shgrp, quad, NULL);
		DRW_shgroup_uniform_texture_ref(blend_shgrp, "strokeColor", &e_data.temp_color_tx_a);
		DRW_shgroup_uniform_texture_ref(blend_shgrp, "strokeDepth", &e_data.temp_depth_tx_a);
		DRW_shgroup_uniform_texture_ref(blend_shgrp, "blendColor", &e_data.temp_color_tx_fx);
		DRW_shgroup_uniform_texture_ref(blend_shgrp, "blendDepth", &e_data.temp_depth_tx_fx);
		DRW_shgroup_uniform_int(blend_shgrp, "mode", &stl->storage->blend_mode, 1);
		DRW_shgroup_uniform_int(blend_shgrp, "clamp_layer", &stl->storage->clamp_layer, 1);
		DRW_shgroup_uniform_float(blend_shgrp, "blend_opacity", &stl->storage->blend_opacity, 1);
		DRW_shgroup_uniform_int(mix_shgrp, "tonemapping", &stl->storage->tonemapping, 1);

		/* create effects passes */
		if (!stl->storage->simplify_fx) {
			GPENCIL_create_fx_passes(psl);
		}
	}
}

static void gpencil_add_draw_data(void *vedata, Object *ob)
{
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	bGPdata *gpd = (bGPdata *)ob->data;
	const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);

	int i = stl->g_data->gp_cache_used - 1;
	tGPencilObjectCache *cache_ob = &stl->g_data->gp_object_cache[i];

	if (!cache_ob->is_dup_ob) {
		/* fill shading groups */
		if (!is_multiedit) {
			DRW_gpencil_populate_datablock(&e_data, vedata, ob, cache_ob);
		}
		else {
			DRW_gpencil_populate_multiedit(&e_data, vedata, ob, cache_ob);
		}
	}

	/* FX passses */
	cache_ob->has_fx = false;
	if ((!stl->storage->simplify_fx) &&
	    (BKE_shaderfx_has_gpencil(ob)))
	{
		cache_ob->has_fx = true;
		if ((!stl->storage->simplify_fx) && (!is_multiedit)) {
			DRW_gpencil_fx_prepare(&e_data, vedata, cache_ob);
		}
	}

}

void GPENCIL_cache_populate(void *vedata, Object *ob)
{
	/* object must be visible */
	if (!(DRW_object_visibility_in_active_context(ob) & OB_VISIBLE_SELF)) {
		return;
	}

	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene = draw_ctx->scene;
	ToolSettings *ts = scene->toolsettings;
	View3D *v3d = draw_ctx->v3d;

	if (ob->type == OB_GPENCIL && ob->data) {
		bGPdata *gpd = (bGPdata *)ob->data;

		/* enable multisample and basic framebuffer creation */
		stl->storage->framebuffer_flag |= GP_FRAMEBUFFER_MULTISAMPLE;
		stl->storage->framebuffer_flag |= GP_FRAMEBUFFER_BASIC;

		/* when start/stop animation the cache must be set as dirty to reset all data */
		if (stl->storage->reset_cache) {
			gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
			stl->storage->reset_cache = false;
		}

		if ((stl->g_data->session_flag & GP_DRW_PAINT_READY) == 0) {

			/* save gp objects for drawing later */
			stl->g_data->gp_object_cache = gpencil_object_cache_add(
				stl->g_data->gp_object_cache, ob,
				&stl->g_data->gp_cache_size, &stl->g_data->gp_cache_used);

			/* load drawing data */
			gpencil_add_draw_data(vedata, ob);
		}

		/* draw current painting strokes
		* (only if region is equal to originated paint region)
		*/
		if ((draw_ctx->obact == ob) &&
		    ((gpd->runtime.ar == NULL) || (gpd->runtime.ar == draw_ctx->ar)))
		{
			DRW_gpencil_populate_buffer_strokes(&e_data, vedata, ts, ob);
		}

		/* grid */
		if ((v3d) &&
		    ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) &&
		    (v3d->gp_flag & V3D_GP_SHOW_GRID) &&
		    (ob->type == OB_GPENCIL) && (ob == draw_ctx->obact))
		{
			GPU_BATCH_DISCARD_SAFE(e_data.batch_grid);
			MEM_SAFE_FREE(e_data.batch_grid);

			e_data.batch_grid = DRW_gpencil_get_grid(ob);

			/* define grid orientation */
			if (ts->gp_sculpt.lock_axis != GP_LOCKAXIS_VIEW) {
				copy_m4_m4(stl->storage->grid_matrix, ob->obmat);
			}
			else {
				/* align always to view */
				invert_m4_m4(stl->storage->grid_matrix, draw_ctx->rv3d->viewmat);
				/* copy ob location */
				copy_v3_v3(stl->storage->grid_matrix[3], ob->obmat[3]);
			}

			DRW_shgroup_call_add(
			        stl->g_data->shgrps_grid,
			        e_data.batch_grid,
			        stl->storage->grid_matrix);
		}
	}
}

void GPENCIL_cache_finish(void *vedata)
{
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;

	/* draw particles */
	DRW_gpencil_populate_particles(&e_data, vedata);

	if (stl->g_data->session_flag & (GP_DRW_PAINT_IDLE | GP_DRW_PAINT_FILLING)) {
		stl->storage->framebuffer_flag |= GP_FRAMEBUFFER_DRAW;
	}

	/* create framebuffers */
	GPENCIL_create_framebuffers(vedata);

}

/* helper function to sort inverse gpencil objects using qsort */
static int gpencil_object_cache_compare_zdepth(const void *a1, const void *a2)
{
	const tGPencilObjectCache *ps1 = a1, *ps2 = a2;

	if (ps1->zdepth < ps2->zdepth) return 1;
	else if (ps1->zdepth > ps2->zdepth) return -1;

	return 0;
}

/* prepare a texture with full viewport screenshot for fast drawing */
static void gpencil_prepare_fast_drawing(
        GPENCIL_StorageList *stl, DefaultFramebufferList *dfbl,
        GPENCIL_FramebufferList *fbl, DRWPass *pass,
        const float clearcol[4])
{
	if (stl->g_data->session_flag & (GP_DRW_PAINT_IDLE | GP_DRW_PAINT_FILLING)) {
		GPU_framebuffer_bind(fbl->background_fb);
		/* clean only in first loop cycle */
		if (stl->g_data->session_flag & GP_DRW_PAINT_IDLE) {
			GPU_framebuffer_clear_color_depth(fbl->background_fb, clearcol, 1.0f);
			stl->g_data->session_flag = GP_DRW_PAINT_FILLING;
		}
		/* repeat pass to fill temp texture */
		DRW_draw_pass(pass);
		/* set default framebuffer again */
		GPU_framebuffer_bind(dfbl->default_fb);
	}
}

static void gpencil_free_obj_runtime(GPENCIL_StorageList *stl)
{
	/* reset all cache flags */
	for (int i = 0; i < stl->g_data->gp_cache_used; i++) {
		tGPencilObjectCache *cache_ob = &stl->g_data->gp_object_cache[i];
		bGPdata *gpd = cache_ob->gpd;
		gpd->flag &= ~GP_DATA_CACHE_IS_DIRTY;

		/* free shgrp array */
		cache_ob->tot_layers = 0;
		MEM_SAFE_FREE(cache_ob->shgrp_array);
	}

	/* free the cache itself */
	MEM_SAFE_FREE(stl->g_data->gp_object_cache);
}

static void gpencil_draw_pass_range(
	GPENCIL_FramebufferList *fbl, GPENCIL_StorageList *stl,
	GPENCIL_PassList *psl, GPENCIL_TextureList *txl,
	GPUFrameBuffer *fb,
	DRWShadingGroup *init_shgrp, DRWShadingGroup *end_shgrp, bool multi)
{
	if (init_shgrp == NULL) {
		return;
	}

	/* previews don't use AA */
	if ((!stl->storage->is_mat_preview) && (multi)) {
		MULTISAMPLE_GP_SYNC_ENABLE(stl->storage->multisamples, fbl);
	}

	DRW_draw_pass_subset(
		psl->stroke_pass, init_shgrp, end_shgrp);

	if ((!stl->storage->is_mat_preview) && (multi)) {
		MULTISAMPLE_GP_SYNC_DISABLE(stl->storage->multisamples, fbl, fb, txl);
	}

}

/* draw scene */
void GPENCIL_draw_scene(void *ved)
{
	GPENCIL_Data *vedata = (GPENCIL_Data *)ved;
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;

	GPENCIL_PassList *psl = ((GPENCIL_Data *)vedata)->psl;
	GPENCIL_FramebufferList *fbl = ((GPENCIL_Data *)vedata)->fbl;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	GPENCIL_TextureList *txl = ((GPENCIL_Data *)vedata)->txl;

	tGPencilObjectCache *cache_ob;
	tGPencilObjectCache_shgrp *array_elm = NULL;
	DRWShadingGroup *init_shgrp = NULL;
	DRWShadingGroup *end_shgrp = NULL;

	const float clearcol[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	const DRWContextState *draw_ctx = DRW_context_state_get();
	View3D *v3d = draw_ctx->v3d;
	Object *obact = draw_ctx->obact;
	const bool playing = stl->storage->is_playing;
	const bool is_render = stl->storage->is_render;
	bGPdata *gpd_act = (obact) && (obact->type == OB_GPENCIL) ? (bGPdata *)obact->data : NULL;
	const bool is_edit = GPENCIL_ANY_EDIT_MODE(gpd_act);

	/* paper pass to display a comfortable area to draw over complex scenes with geometry */
	if ((!is_render) && (obact) && (obact->type == OB_GPENCIL)) {
		if (((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) &&
		    (v3d->gp_flag & V3D_GP_SHOW_PAPER))
		{
			DRW_draw_pass(psl->paper_pass);
		}
	}

	/* if we have a painting session, we use fast viewport drawing method */
	if ((!is_render) && (stl->g_data->session_flag & GP_DRW_PAINT_PAINTING)) {
		GPU_framebuffer_bind(dfbl->default_fb);

		MULTISAMPLE_GP_SYNC_ENABLE(stl->storage->multisamples, fbl);

		DRW_draw_pass(psl->background_pass);
		DRW_draw_pass(psl->drawing_pass);

		MULTISAMPLE_GP_SYNC_DISABLE(stl->storage->multisamples, fbl, dfbl->default_fb, txl);

		/* free memory */
		gpencil_free_obj_runtime(stl);

		/* grid pass */
		if ((!is_render) && (obact) && (obact->type == OB_GPENCIL)) {
			if (((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) &&
			    (v3d->gp_flag & V3D_GP_SHOW_GRID))
			{
				DRW_draw_pass(psl->grid_pass);
			}
		}

		return;
	}

	if (DRW_state_is_fbo()) {

		/* Draw all pending objects */
		if (stl->g_data->gp_cache_used > 0) {

			/* sort by zdepth */
			qsort(stl->g_data->gp_object_cache, stl->g_data->gp_cache_used,
			      sizeof(tGPencilObjectCache), gpencil_object_cache_compare_zdepth);

			for (int i = 0; i < stl->g_data->gp_cache_used; i++) {
				cache_ob = &stl->g_data->gp_object_cache[i];
				bGPdata *gpd = cache_ob->gpd;
				init_shgrp = NULL;
				/* Render stroke in separated framebuffer */
				GPU_framebuffer_bind(fbl->temp_fb_a);
				GPU_framebuffer_clear_color_depth(fbl->temp_fb_a, clearcol, 1.0f);

				/* Stroke Pass:
				 * draw only a subset that usually starts with a fill and ends with stroke
				 */
				bool use_blend = false;
				if (cache_ob->tot_layers > 0) {
					for (int e = 0; e < cache_ob->tot_layers; e++) {
						bool is_last = e == cache_ob->tot_layers - 1 ? true : false;
						array_elm = &cache_ob->shgrp_array[e];

						if (((array_elm->mode == eGplBlendMode_Normal) &&
						     (!use_blend) && (!array_elm->clamp_layer)) ||
						    (e == 0))
						{
							if (init_shgrp == NULL) {
								init_shgrp = array_elm->init_shgrp;
							}
							end_shgrp = array_elm->end_shgrp;
						}
						else {
							use_blend = true;
							/* draw pending groups */
							gpencil_draw_pass_range(
								fbl, stl, psl, txl, fbl->temp_fb_a,
								init_shgrp, end_shgrp, is_last);

							/* draw current group in separated texture */
							init_shgrp = array_elm->init_shgrp;
							end_shgrp = array_elm->end_shgrp;

							GPU_framebuffer_bind(fbl->temp_fb_fx);
							GPU_framebuffer_clear_color_depth(fbl->temp_fb_fx, clearcol, 1.0f);
							gpencil_draw_pass_range(
							        fbl, stl, psl, txl, fbl->temp_fb_fx,
							        init_shgrp, end_shgrp,
							        is_last);

							/* Blend A texture and FX texture */
							GPU_framebuffer_bind(fbl->temp_fb_b);
							GPU_framebuffer_clear_color_depth(fbl->temp_fb_b, clearcol, 1.0f);
							stl->storage->blend_mode = array_elm->mode;
							stl->storage->clamp_layer = (int)array_elm->clamp_layer;
							stl->storage->blend_opacity = array_elm->blend_opacity;
							stl->storage->tonemapping = stl->storage->is_render ? 1 : 0;
							DRW_draw_pass(psl->blend_pass);
							stl->storage->tonemapping = 0;

							/* Copy B texture to A texture to follow loop */
							e_data.input_depth_tx = e_data.temp_depth_tx_b;
							e_data.input_color_tx = e_data.temp_color_tx_b;

							GPU_framebuffer_bind(fbl->temp_fb_a);
							GPU_framebuffer_clear_color_depth(fbl->temp_fb_a, clearcol, 1.0f);
							DRW_draw_pass(psl->mix_pass_noblend);

							/* prepare next group */
							init_shgrp = NULL;
						}

					}
					/* last group */
					gpencil_draw_pass_range(
					        fbl, stl, psl, txl, fbl->temp_fb_a,
					        init_shgrp, end_shgrp,
					        true);
				}

				/* Current buffer drawing */
				if ((!is_render) && (cache_ob->is_dup_ob == false)) {
					DRW_draw_pass(psl->drawing_pass);
				}
				/* fx passes */
				if (cache_ob->has_fx == true) {
					stl->storage->tonemapping = 0;
					DRW_gpencil_fx_draw(&e_data, vedata, cache_ob);
				}

				e_data.input_depth_tx = e_data.temp_depth_tx_a;
				e_data.input_color_tx = e_data.temp_color_tx_a;

				/* Combine with scene buffer */
				if ((!is_render) || (fbl->main == NULL)) {
					GPU_framebuffer_bind(dfbl->default_fb);
				}
				else {
					GPU_framebuffer_bind(fbl->main);
				}
				/* tonemapping */
				stl->storage->tonemapping = stl->storage->is_render ? 1 : 0;

				DRW_draw_pass(psl->mix_pass);

				/* prepare for fast drawing */
				if (!is_render) {
					if (!playing) {
						gpencil_prepare_fast_drawing(stl, dfbl, fbl, psl->mix_pass_noblend, clearcol);
					}
				}
				else {
					/* if render, the cache must be dirty for next loop */
					gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
				}
			}
			/* edit points */
			if ((!is_render) && (!playing) && (is_edit)) {
				DRW_draw_pass(psl->edit_pass);
			}
		}
		/* grid pass */
		if ((!is_render) && (obact) && (obact->type == OB_GPENCIL)) {
			if (((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) &&
			    (v3d->gp_flag & V3D_GP_SHOW_GRID))
			{
				DRW_draw_pass(psl->grid_pass);
			}
		}
	}
	/* free memory */
	gpencil_free_obj_runtime(stl);

	/* reset  */
	if (DRW_state_is_fbo()) {
		/* attach again default framebuffer */
		if (!is_render) {
			GPU_framebuffer_bind(dfbl->default_fb);
		}

		/* the temp texture is ready. Now we can use fast screen drawing */
		if (stl->g_data->session_flag & GP_DRW_PAINT_FILLING) {
			stl->g_data->session_flag = GP_DRW_PAINT_READY;
		}
	}
}

static const DrawEngineDataSize GPENCIL_data_size = DRW_VIEWPORT_DATA_SIZE(GPENCIL_Data);

DrawEngineType draw_engine_gpencil_type = {
	NULL, NULL,
	N_("GpencilMode"),
	&GPENCIL_data_size,
	&GPENCIL_engine_init,
	&GPENCIL_engine_free,
	&GPENCIL_cache_init,
	&GPENCIL_cache_populate,
	&GPENCIL_cache_finish,
	NULL,
	&GPENCIL_draw_scene,
	NULL,
	NULL,
	&GPENCIL_render_to_image,
};
