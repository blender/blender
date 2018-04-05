/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_draw.c
 *  \ingroup spview3d
 */

#include <math.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_jitter_2d.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_scene.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_unit.h"

#include "BLF_api.h"

#include "BLT_translation.h"

#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "DRW_engine.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "DEG_depsgraph_query.h"

#include "GPU_draw.h"
#include "GPU_matrix.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_material.h"
#include "GPU_viewport.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RE_engine.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "view3d_intern.h"  /* own include */

/* ******************** general functions ***************** */

static bool use_depth_doit(View3D *v3d, Object *obedit)
{
	if (v3d->drawtype > OB_WIRE)
		return true;

	/* special case (depth for wire color) */
	if (v3d->drawtype <= OB_WIRE) {
		if (obedit && obedit->type == OB_MESH) {
			Mesh *me = obedit->data;
			if (me->drawflag & ME_DRAWEIGHT) {
				return true;
			}
		}
	}
	return false;
}

/**
 * \note keep this synced with #ED_view3d_mats_rv3d_backup/#ED_view3d_mats_rv3d_restore
 */
void ED_view3d_update_viewmat(
        const EvaluationContext *eval_ctx, Scene *scene, View3D *v3d, ARegion *ar,
        float viewmat[4][4], float winmat[4][4], const rcti *rect)
{
	const Depsgraph *depsgraph = eval_ctx->depsgraph;
	RegionView3D *rv3d = ar->regiondata;

	/* setup window matrices */
	if (winmat)
		copy_m4_m4(rv3d->winmat, winmat);
	else
		view3d_winmatrix_set(depsgraph, ar, v3d, rect);

	/* setup view matrix */
	if (viewmat) {
		copy_m4_m4(rv3d->viewmat, viewmat);
	}
	else {
		float rect_scale[2];
		if (rect) {
			rect_scale[0] = (float)BLI_rcti_size_x(rect) / (float)ar->winx;
			rect_scale[1] = (float)BLI_rcti_size_y(rect) / (float)ar->winy;
		}
		/* note: calls BKE_object_where_is_calc for camera... */
		view3d_viewmatrix_set(eval_ctx, scene, v3d, rv3d, rect ? rect_scale : NULL);
	}
	/* update utility matrices */
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);
	invert_m4_m4(rv3d->persinv, rv3d->persmat);
	invert_m4_m4(rv3d->viewinv, rv3d->viewmat);

	/* calculate GLSL view dependent values */

	/* store window coordinates scaling/offset */
	if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
		rctf cameraborder;
		ED_view3d_calc_camera_border(scene, eval_ctx->depsgraph, ar, v3d, rv3d, &cameraborder, false);
		rv3d->viewcamtexcofac[0] = (float)ar->winx / BLI_rctf_size_x(&cameraborder);
		rv3d->viewcamtexcofac[1] = (float)ar->winy / BLI_rctf_size_y(&cameraborder);

		rv3d->viewcamtexcofac[2] = -rv3d->viewcamtexcofac[0] * cameraborder.xmin / (float)ar->winx;
		rv3d->viewcamtexcofac[3] = -rv3d->viewcamtexcofac[1] * cameraborder.ymin / (float)ar->winy;
	}
	else {
		rv3d->viewcamtexcofac[0] = rv3d->viewcamtexcofac[1] = 1.0f;
		rv3d->viewcamtexcofac[2] = rv3d->viewcamtexcofac[3] = 0.0f;
	}

	/* calculate pixelsize factor once, is used for lamps and obcenters */
	{
		/* note:  '1.0f / len_v3(v1)'  replaced  'len_v3(rv3d->viewmat[0])'
		 * because of float point precision problems at large values [#23908] */
		float v1[3], v2[3];
		float len_px, len_sc;

		v1[0] = rv3d->persmat[0][0];
		v1[1] = rv3d->persmat[1][0];
		v1[2] = rv3d->persmat[2][0];

		v2[0] = rv3d->persmat[0][1];
		v2[1] = rv3d->persmat[1][1];
		v2[2] = rv3d->persmat[2][1];

		len_px = 2.0f / sqrtf(min_ff(len_squared_v3(v1), len_squared_v3(v2)));
		len_sc = (float)MAX2(ar->winx, ar->winy);

		rv3d->pixsize = len_px / len_sc;
	}
}

static void view3d_main_region_setup_view(
        const EvaluationContext *eval_ctx, Scene *scene,
        View3D *v3d, ARegion *ar, float viewmat[4][4], float winmat[4][4], const rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;

	ED_view3d_update_viewmat(eval_ctx, scene, v3d, ar, viewmat, winmat, rect);

	/* set for opengl */
	gpuLoadProjectionMatrix(rv3d->winmat);
	gpuLoadMatrix(rv3d->viewmat);
}

static bool view3d_stereo3d_active(wmWindow *win, Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	if ((scene->r.scemode & R_MULTIVIEW) == 0) {
		return false;
	}

	if ((v3d->camera == NULL) || (v3d->camera->type != OB_CAMERA) || rv3d->persp != RV3D_CAMOB) {
		return false;
	}

	switch (v3d->stereo3d_camera) {
		case STEREO_MONO_ID:
			return false;
			break;
		case STEREO_3D_ID:
			/* win will be NULL when calling this from the selection or draw loop. */
			if ((win == NULL) || (WM_stereo3d_enabled(win, true) == false)) {
				return false;
			}
			if (((scene->r.views_format & SCE_VIEWS_FORMAT_MULTIVIEW) != 0) &&
			    !BKE_scene_multiview_is_stereo3d(&scene->r))
			{
				return false;
			}
			break;
		/* We always need the stereo calculation for left and right cameras. */
		case STEREO_LEFT_ID:
		case STEREO_RIGHT_ID:
		default:
			break;
	}
	return true;
}


/* setup the view and win matrices for the multiview cameras
 *
 * unlike view3d_stereo3d_setup_offscreen, when view3d_stereo3d_setup is called
 * we have no winmatrix (i.e., projection matrix) defined at that time.
 * Since the camera and the camera shift are needed for the winmat calculation
 * we do a small hack to replace it temporarily so we don't need to change the
 * view3d)main_region_setup_view() code to account for that.
 */
static void view3d_stereo3d_setup(
        const EvaluationContext *eval_ctx, Scene *scene, View3D *v3d, ARegion *ar, const rcti *rect)
{
	bool is_left;
	const char *names[2] = { STEREO_LEFT_NAME, STEREO_RIGHT_NAME };
	const char *viewname;

	/* show only left or right camera */
	if (v3d->stereo3d_camera != STEREO_3D_ID)
		v3d->multiview_eye = v3d->stereo3d_camera;

	is_left = v3d->multiview_eye == STEREO_LEFT_ID;
	viewname = names[is_left ? STEREO_LEFT_ID : STEREO_RIGHT_ID];

	/* update the viewport matrices with the new camera */
	if (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D) {
		Camera *data;
		float viewmat[4][4];
		float shiftx;

		data = (Camera *)v3d->camera->data;
		shiftx = data->shiftx;

		BLI_thread_lock(LOCK_VIEW3D);
		data->shiftx = BKE_camera_multiview_shift_x(&scene->r, v3d->camera, viewname);

		BKE_camera_multiview_view_matrix(&scene->r, v3d->camera, is_left, viewmat);
		view3d_main_region_setup_view(eval_ctx, scene, v3d, ar, viewmat, NULL, rect);

		data->shiftx = shiftx;
		BLI_thread_unlock(LOCK_VIEW3D);
	}
	else { /* SCE_VIEWS_FORMAT_MULTIVIEW */
		float viewmat[4][4];
		Object *view_ob = v3d->camera;
		Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);

		BLI_thread_lock(LOCK_VIEW3D);
		v3d->camera = camera;

		BKE_camera_multiview_view_matrix(&scene->r, camera, false, viewmat);
		view3d_main_region_setup_view(eval_ctx, scene, v3d, ar, viewmat, NULL, rect);

		v3d->camera = view_ob;
		BLI_thread_unlock(LOCK_VIEW3D);
	}
}

/**
 * Set the correct matrices
 */
void ED_view3d_draw_setup_view(
        wmWindow *win, const EvaluationContext *eval_ctx, Scene *scene, ARegion *ar, View3D *v3d,
        float viewmat[4][4], float winmat[4][4], const rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;

	/* Setup the view matrix. */
	if (view3d_stereo3d_active(win, scene, v3d, rv3d)) {
		view3d_stereo3d_setup(eval_ctx, scene, v3d, ar, rect);
	}
	else {
		view3d_main_region_setup_view(eval_ctx, scene, v3d, ar, viewmat, winmat, rect);
	}
}

/* ******************** view border ***************** */

static void view3d_camera_border(
        const Scene *scene, const struct Depsgraph *depsgraph,
        const ARegion *ar, const View3D *v3d, const RegionView3D *rv3d,
        rctf *r_viewborder, const bool no_shift, const bool no_zoom)
{
	CameraParams params;
	rctf rect_view, rect_camera;

	/* get viewport viewplane */
	BKE_camera_params_init(&params);
	BKE_camera_params_from_view3d(&params, depsgraph, v3d, rv3d);
	if (no_zoom)
		params.zoom = 1.0f;
	BKE_camera_params_compute_viewplane(&params, ar->winx, ar->winy, 1.0f, 1.0f);
	rect_view = params.viewplane;

	/* get camera viewplane */
	BKE_camera_params_init(&params);
	/* fallback for non camera objects */
	params.clipsta = v3d->near;
	params.clipend = v3d->far;
	BKE_camera_params_from_object(&params, v3d->camera);
	if (no_shift) {
		params.shiftx = 0.0f;
		params.shifty = 0.0f;
	}
	BKE_camera_params_compute_viewplane(&params, scene->r.xsch, scene->r.ysch, scene->r.xasp, scene->r.yasp);
	rect_camera = params.viewplane;

	/* get camera border within viewport */
	r_viewborder->xmin = ((rect_camera.xmin - rect_view.xmin) / BLI_rctf_size_x(&rect_view)) * ar->winx;
	r_viewborder->xmax = ((rect_camera.xmax - rect_view.xmin) / BLI_rctf_size_x(&rect_view)) * ar->winx;
	r_viewborder->ymin = ((rect_camera.ymin - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) * ar->winy;
	r_viewborder->ymax = ((rect_camera.ymax - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) * ar->winy;
}

void ED_view3d_calc_camera_border_size(
        const Scene *scene, const Depsgraph *depsgraph,
        const ARegion *ar, const View3D *v3d, const RegionView3D *rv3d,
        float r_size[2])
{
	rctf viewborder;

	view3d_camera_border(scene, depsgraph, ar, v3d, rv3d, &viewborder, true, true);
	r_size[0] = BLI_rctf_size_x(&viewborder);
	r_size[1] = BLI_rctf_size_y(&viewborder);
}

void ED_view3d_calc_camera_border(
        const Scene *scene, const Depsgraph *depsgraph,
        const ARegion *ar, const View3D *v3d, const RegionView3D *rv3d,
        rctf *r_viewborder, const bool no_shift)
{
	view3d_camera_border(scene, depsgraph, ar, v3d, rv3d, r_viewborder, no_shift, false);
}

static void drawviewborder_grid3(uint shdr_pos, float x1, float x2, float y1, float y2, float fac)
{
	float x3, y3, x4, y4;

	x3 = x1 + fac * (x2 - x1);
	y3 = y1 + fac * (y2 - y1);
	x4 = x1 + (1.0f - fac) * (x2 - x1);
	y4 = y1 + (1.0f - fac) * (y2 - y1);

	immBegin(GWN_PRIM_LINES, 8);

	immVertex2f(shdr_pos, x1, y3);
	immVertex2f(shdr_pos, x2, y3);

	immVertex2f(shdr_pos, x1, y4);
	immVertex2f(shdr_pos, x2, y4);

	immVertex2f(shdr_pos, x3, y1);
	immVertex2f(shdr_pos, x3, y2);

	immVertex2f(shdr_pos, x4, y1);
	immVertex2f(shdr_pos, x4, y2);

	immEnd();
}

/* harmonious triangle */
static void drawviewborder_triangle(
        uint shdr_pos, float x1, float x2, float y1, float y2, const char golden, const char dir)
{
	float ofs;
	float w = x2 - x1;
	float h = y2 - y1;

	immBegin(GWN_PRIM_LINES, 6);

	if (w > h) {
		if (golden) {
			ofs = w * (1.0f - (1.0f / 1.61803399f));
		}
		else {
			ofs = h * (h / w);
		}
		if (dir == 'B') SWAP(float, y1, y2);

		immVertex2f(shdr_pos, x1, y1);
		immVertex2f(shdr_pos, x2, y2);

		immVertex2f(shdr_pos, x2, y1);
		immVertex2f(shdr_pos, x1 + (w - ofs), y2);

		immVertex2f(shdr_pos, x1, y2);
		immVertex2f(shdr_pos, x1 + ofs, y1);
	}
	else {
		if (golden) {
			ofs = h * (1.0f - (1.0f / 1.61803399f));
		}
		else {
			ofs = w * (w / h);
		}
		if (dir == 'B') SWAP(float, x1, x2);

		immVertex2f(shdr_pos, x1, y1);
		immVertex2f(shdr_pos, x2, y2);

		immVertex2f(shdr_pos, x2, y1);
		immVertex2f(shdr_pos, x1, y1 + ofs);

		immVertex2f(shdr_pos, x1, y2);
		immVertex2f(shdr_pos, x2, y1 + (h - ofs));
	}

	immEnd();
}

static void drawviewborder(Scene *scene, const Depsgraph *depsgraph, ARegion *ar, View3D *v3d)
{
	float x1, x2, y1, y2;
	float x1i, x2i, y1i, y2i;

	rctf viewborder;
	Camera *ca = NULL;
	RegionView3D *rv3d = ar->regiondata;

	if (v3d->camera == NULL)
		return;
	if (v3d->camera->type == OB_CAMERA)
		ca = v3d->camera->data;
	
	ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &viewborder, false);
	/* the offsets */
	x1 = viewborder.xmin;
	y1 = viewborder.ymin;
	x2 = viewborder.xmax;
	y2 = viewborder.ymax;
	
	glLineWidth(1.0f);

	/* apply offsets so the real 3D camera shows through */

	/* note: quite un-scientific but without this bit extra
	 * 0.0001 on the lower left the 2D border sometimes
	 * obscures the 3D camera border */
	/* note: with VIEW3D_CAMERA_BORDER_HACK defined this error isn't noticeable
	 * but keep it here in case we need to remove the workaround */
	x1i = (int)(x1 - 1.0001f);
	y1i = (int)(y1 - 1.0001f);
	x2i = (int)(x2 + (1.0f - 0.0001f));
	y2i = (int)(y2 + (1.0f - 0.0001f));

	uint shdr_pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	/* First, solid lines. */
	{
		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

		/* passepartout, specified in camera edit buttons */
		if (ca && (ca->flag & CAM_SHOWPASSEPARTOUT) && ca->passepartalpha > 0.000001f) {
			const float winx = (ar->winx + 1);
			const float winy = (ar->winy + 1);

			float alpha = 1.0f;

			if (ca->passepartalpha != 1.0f) {
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				glEnable(GL_BLEND);
				alpha = ca->passepartalpha;
			}

			immUniformColor4f(0.0f, 0.0f, 0.0f, alpha);

			if (x1i > 0.0f)
				immRectf(shdr_pos, 0.0f, winy, x1i, 0.0f);
			if (x2i < winx)
				immRectf(shdr_pos, x2i, winy, winx, 0.0f);
			if (y2i < winy)
				immRectf(shdr_pos, x1i, winy, x2i, y2i);
			if (y2i > 0.0f)
				immRectf(shdr_pos, x1i, y1i, x2i, 0.0f);

			glDisable(GL_BLEND);
		}

		immUniformThemeColor(TH_BACK);
		imm_draw_box_wire_2d(shdr_pos, x1i, y1i, x2i, y2i);

#ifdef VIEW3D_CAMERA_BORDER_HACK
		if (view3d_camera_border_hack_test == true) {
			immUniformColor3ubv(view3d_camera_border_hack_col);
			imm_draw_box_wire_2d(shdr_pos, x1i + 1, y1i + 1, x2i - 1, y2i - 1);
			view3d_camera_border_hack_test = false;
		}
#endif

		immUnbindProgram();
	}

	/* And now, the dashed lines! */
	immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

	{
		float viewport_size[4];
		glGetFloatv(GL_VIEWPORT, viewport_size);
		immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

		immUniform1i("num_colors", 0);  /* "simple" mode */
		immUniform1f("dash_width", 6.0f);
		immUniform1f("dash_factor", 0.5f);

		/* outer line not to confuse with object selection */
		if (v3d->flag2 & V3D_LOCK_CAMERA) {
			immUniformThemeColor(TH_REDALERT);
			imm_draw_box_wire_2d(shdr_pos, x1i - 1, y1i - 1, x2i + 1, y2i + 1);
		}

		immUniformThemeColor(TH_VIEW_OVERLAY);
		imm_draw_box_wire_2d(shdr_pos, x1i, y1i, x2i, y2i);
	}

	/* border */
	if (scene->r.mode & R_BORDER) {
		float x3, y3, x4, y4;

		x3 = floorf(x1 + (scene->r.border.xmin * (x2 - x1))) - 1;
		y3 = floorf(y1 + (scene->r.border.ymin * (y2 - y1))) - 1;
		x4 = floorf(x1 + (scene->r.border.xmax * (x2 - x1))) + (U.pixelsize - 1);
		y4 = floorf(y1 + (scene->r.border.ymax * (y2 - y1))) + (U.pixelsize - 1);

		immUniformColor3f(1.0f, 0.25f, 0.25f);
		imm_draw_box_wire_2d(shdr_pos, x3, y3, x4, y4);
	}

	/* safety border */
	if (ca) {
		immUniformThemeColorBlend(TH_VIEW_OVERLAY, TH_BACK, 0.25f);

		if (ca->dtx & CAM_DTX_CENTER) {
			float x3, y3;

			x3 = x1 + 0.5f * (x2 - x1);
			y3 = y1 + 0.5f * (y2 - y1);

			immBegin(GWN_PRIM_LINES, 4);

			immVertex2f(shdr_pos, x1, y3);
			immVertex2f(shdr_pos, x2, y3);

			immVertex2f(shdr_pos, x3, y1);
			immVertex2f(shdr_pos, x3, y2);

			immEnd();
		}

		if (ca->dtx & CAM_DTX_CENTER_DIAG) {
			immBegin(GWN_PRIM_LINES, 4);

			immVertex2f(shdr_pos, x1, y1);
			immVertex2f(shdr_pos, x2, y2);

			immVertex2f(shdr_pos, x1, y2);
			immVertex2f(shdr_pos, x2, y1);

			immEnd();
		}

		if (ca->dtx & CAM_DTX_THIRDS) {
			drawviewborder_grid3(shdr_pos, x1, x2, y1, y2, 1.0f / 3.0f);
		}

		if (ca->dtx & CAM_DTX_GOLDEN) {
			drawviewborder_grid3(shdr_pos, x1, x2, y1, y2, 1.0f - (1.0f / 1.61803399f));
		}

		if (ca->dtx & CAM_DTX_GOLDEN_TRI_A) {
			drawviewborder_triangle(shdr_pos, x1, x2, y1, y2, 0, 'A');
		}

		if (ca->dtx & CAM_DTX_GOLDEN_TRI_B) {
			drawviewborder_triangle(shdr_pos, x1, x2, y1, y2, 0, 'B');
		}

		if (ca->dtx & CAM_DTX_HARMONY_TRI_A) {
			drawviewborder_triangle(shdr_pos, x1, x2, y1, y2, 1, 'A');
		}

		if (ca->dtx & CAM_DTX_HARMONY_TRI_B) {
			drawviewborder_triangle(shdr_pos, x1, x2, y1, y2, 1, 'B');
		}

		if (ca->flag & CAM_SHOW_SAFE_MARGINS) {
			UI_draw_safe_areas(
			        shdr_pos, x1, x2, y1, y2,
			        scene->safe_areas.title, scene->safe_areas.action);

			if (ca->flag & CAM_SHOW_SAFE_CENTER) {
				UI_draw_safe_areas(
				        shdr_pos, x1, x2, y1, y2,
				        scene->safe_areas.title_center, scene->safe_areas.action_center);
			}
		}

		if (ca->flag & CAM_SHOWSENSOR) {
			/* determine sensor fit, and get sensor x/y, for auto fit we
			 * assume and square sensor and only use sensor_x */
			float sizex = scene->r.xsch * scene->r.xasp;
			float sizey = scene->r.ysch * scene->r.yasp;
			int sensor_fit = BKE_camera_sensor_fit(ca->sensor_fit, sizex, sizey);
			float sensor_x = ca->sensor_x;
			float sensor_y = (ca->sensor_fit == CAMERA_SENSOR_FIT_AUTO) ? ca->sensor_x : ca->sensor_y;

			/* determine sensor plane */
			rctf rect;

			if (sensor_fit == CAMERA_SENSOR_FIT_HOR) {
				float sensor_scale = (x2i - x1i) / sensor_x;
				float sensor_height = sensor_scale * sensor_y;

				rect.xmin = x1i;
				rect.xmax = x2i;
				rect.ymin = (y1i + y2i) * 0.5f - sensor_height * 0.5f;
				rect.ymax = rect.ymin + sensor_height;
			}
			else {
				float sensor_scale = (y2i - y1i) / sensor_y;
				float sensor_width = sensor_scale * sensor_x;

				rect.xmin = (x1i + x2i) * 0.5f - sensor_width * 0.5f;
				rect.xmax = rect.xmin + sensor_width;
				rect.ymin = y1i;
				rect.ymax = y2i;
			}

			/* draw */
			immUniformThemeColorShade(TH_VIEW_OVERLAY, 100);
			
			/* TODO Was using UI_draw_roundbox_4fv(false, rect.xmin, rect.ymin, rect.xmax, rect.ymax, 2.0f, color).
			 * We'll probably need a new imm_draw_line_roundbox_dashed dor that - though in practice the
			 * 2.0f round corner effect was nearly not visible anyway... */
			imm_draw_box_wire_2d(shdr_pos, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
		}
	}

	immUnbindProgram();
	/* end dashed lines */

	/* camera name - draw in highlighted text color */
	if (ca && (ca->flag & CAM_SHOWNAME)) {
		UI_FontThemeColor(BLF_default(), TH_TEXT_HI);
		BLF_draw_default(
		        x1i, y1i - (0.7f * U.widget_unit), 0.0f,
		        v3d->camera->id.name + 2, sizeof(v3d->camera->id.name) - 2);
	}
}

static void drawrenderborder(ARegion *ar, View3D *v3d)
{
	/* use the same program for everything */
	uint shdr_pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	glLineWidth(1.0f);

	immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

	float viewport_size[4];
	glGetFloatv(GL_VIEWPORT, viewport_size);
	immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

	immUniform1i("num_colors", 0);  /* "simple" mode */
	immUniform4f("color", 1.0f, 0.25f, 0.25f, 1.0f);
	immUniform1f("dash_width", 6.0f);
	immUniform1f("dash_factor", 0.5f);

	imm_draw_box_wire_2d(shdr_pos,
	                  v3d->render_border.xmin * ar->winx, v3d->render_border.ymin * ar->winy,
	                  v3d->render_border.xmax * ar->winx, v3d->render_border.ymax * ar->winy);

	immUnbindProgram();
}

void ED_view3d_draw_depth(
        const EvaluationContext *eval_ctx, struct Depsgraph *graph,
        ARegion *ar, View3D *v3d, bool alphaoverride)
{
	struct bThemeState theme_state;
	Scene *scene = DEG_get_evaluated_scene(graph);
	RegionView3D *rv3d = ar->regiondata;

	short zbuf = v3d->zbuf;
	short flag = v3d->flag;
	float glalphaclip = U.glalphaclip;
	int obcenter_dia = U.obcenter_dia;
	/* temp set drawtype to solid */
	/* Setting these temporarily is not nice */
	v3d->flag &= ~V3D_SELECT_OUTLINE;
	U.glalphaclip = alphaoverride ? 0.5f : glalphaclip; /* not that nice but means we wont zoom into billboards */
	U.obcenter_dia = 0;

	/* Tools may request depth outside of regular drawing code. */
	UI_Theme_Store(&theme_state);
	UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

	ED_view3d_draw_setup_view(NULL, eval_ctx, scene, ar, v3d, NULL, NULL, NULL);

	glClear(GL_DEPTH_BUFFER_BIT);

	if (rv3d->rflag & RV3D_CLIPPING) {
		ED_view3d_clipping_set(rv3d);
	}
	/* get surface depth without bias */
	rv3d->rflag |= RV3D_ZOFFSET_DISABLED;

	v3d->zbuf = true;
	glEnable(GL_DEPTH_TEST);

#ifdef WITH_OPENGL_LEGACY
	if (IS_VIEWPORT_LEGACY(vc->v3d)) {
		/* temp, calls into view3d_draw_legacy.c */
		ED_view3d_draw_depth_loop(scene, ar, v3d);
	}
	else
#endif /* WITH_OPENGL_LEGACY */
	{
		DRW_draw_depth_loop(graph, ar, v3d);
	}

	if (rv3d->rflag & RV3D_CLIPPING) {
		ED_view3d_clipping_disable();
	}
	rv3d->rflag &= ~RV3D_ZOFFSET_DISABLED;

	v3d->zbuf = zbuf;
	if (!v3d->zbuf) glDisable(GL_DEPTH_TEST);

	U.glalphaclip = glalphaclip;
	v3d->flag = flag;
	U.obcenter_dia = obcenter_dia;

	UI_Theme_Restore(&theme_state);
}

/* ******************** background plates ***************** */

static void view3d_draw_background_gradient(void)
{
	/* TODO: finish 2D API & draw background with that */

	Gwn_VertFormat *format = immVertexFormat();
	unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	unsigned int color = GWN_vertformat_attr_add(format, "color", GWN_COMP_U8, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
	unsigned char col_hi[3], col_lo[3];

	immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

	UI_GetThemeColor3ubv(TH_LOW_GRAD, col_lo);
	UI_GetThemeColor3ubv(TH_HIGH_GRAD, col_hi);

	immBegin(GWN_PRIM_TRI_FAN, 4);
	immAttrib3ubv(color, col_lo);
	immVertex2f(pos, -1.0f, -1.0f);
	immVertex2f(pos, 1.0f, -1.0f);

	immAttrib3ubv(color, col_hi);
	immVertex2f(pos, 1.0f, 1.0f);
	immVertex2f(pos, -1.0f, 1.0f);
	immEnd();

	immUnbindProgram();
}

static void view3d_draw_background_none(void)
{
	UI_ThemeClearColorAlpha(TH_HIGH_GRAD, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

static void view3d_draw_background_world(Scene *scene, RegionView3D *rv3d)
{
	if (scene->world) {
		GPUMaterial *gpumat = GPU_material_world(scene, scene->world);

		/* calculate full shader for background */
		GPU_material_bind(gpumat, 1, 1, 1.0f, false, rv3d->viewmat, rv3d->viewinv, rv3d->viewcamtexcofac);

		if (GPU_material_bound(gpumat)) {
			/* TODO viewport (dfelinto): GPU_material_bind relies on immediate mode,
			 * we can't get rid of the following code without a bigger refactor
			 * or we dropping this functionality. */

			glBegin(GL_TRIANGLE_STRIP);
			glVertex2f(-1.0f, -1.0f);
			glVertex2f(1.0f, -1.0f);
			glVertex2f(-1.0f, 1.0f);
			glVertex2f(1.0f, 1.0f);
			glEnd();

			GPU_material_unbind(gpumat);
			return;
		}
	}

	/* if any of the above fails */
	view3d_draw_background_none();
}

/* ******************** other elements ***************** */


#define DEBUG_GRID 0

static void gridline_range(double x0, double dx, double max, int *r_first, int *r_count)
{
	/* determine range of gridlines that appear in this Area -- similar calc but separate ranges for x & y
	 * x0 is gridline 0, the axis in screen space
	 * Area covers [0 .. max) pixels */

	int first = (int)ceil(-x0 / dx);
	int last = (int)floor((max - x0) / dx);

	if (first <= last) {
		*r_first = first;
		*r_count = last - first + 1;
	}
	else {
		*r_first = 0;
		*r_count = 0;
	}

#if DEBUG_GRID
	printf("   first %d * dx = %f\n", first, x0 + first * dx);
	printf("   last %d * dx = %f\n", last, x0 + last * dx);
	printf("   count = %d\n", *count_out);
#endif
}

static int gridline_count(ARegion *ar, double x0, double y0, double dx)
{
	/* x0 & y0 establish the "phase" of the grid within this 2D region
	 * dx is the frequency, shared by x & y directions
	 * pass in dx of smallest (highest precision) grid we want to draw */

#if DEBUG_GRID
	printf("  %s(%f, %f, dx:%f)\n", __FUNCTION__, x0, y0, dx);
#endif

	int first, x_ct, y_ct;

	gridline_range(x0, dx, ar->winx, &first, &x_ct);
	gridline_range(y0, dx, ar->winy, &first, &y_ct);

	int total_ct = x_ct + y_ct;

#if DEBUG_GRID
	printf("   %d + %d = %d gridlines\n", x_ct, y_ct, total_ct);
#endif

	return total_ct;
}

static bool drawgrid_draw(ARegion *ar, double x0, double y0, double dx, int skip_mod, unsigned pos, unsigned col, GLubyte col_value[3])
{
	/* skip every skip_mod lines relative to each axis; they will be overlaid by another drawgrid_draw
	 * always skip exact x0 & y0 axes; they will be drawn later in color
	 *
	 * set grid color once, just before the first line is drawn
	 * it's harmless to set same color for every line, or every vertex
	 * but if no lines are drawn, color must not be set! */

#if DEBUG_GRID
	printf("  %s(%f, %f, dx:%f, skip_mod:%d)\n", __FUNCTION__, x0, y0, dx, skip_mod);
#endif

	const float x_max = (float)ar->winx;
	const float y_max = (float)ar->winy;

	int first, ct;
	int x_ct = 0, y_ct = 0; /* count of lines actually drawn */
	int lines_skipped_for_next_unit = 0;

	/* draw vertical lines */
	gridline_range(x0, dx, x_max, &first, &ct);

	for (int i = first; i < first + ct; ++i) {
		if (i == 0)
			continue;
		else if (skip_mod && (i % skip_mod) == 0) {
			++lines_skipped_for_next_unit;
			continue;
		}

		if (x_ct == 0)
			immAttrib3ub(col, col_value[0], col_value[1], col_value[2]);

		float x = (float)(x0 + i * dx);
		immVertex2f(pos, x, 0.0f);
		immVertex2f(pos, x, y_max);
		++x_ct;
	}

	/* draw horizontal lines */
	gridline_range(y0, dx, y_max, &first, &ct);

	for (int i = first; i < first + ct; ++i) {
		if (i == 0)
			continue;
		else if (skip_mod && (i % skip_mod) == 0) {
			++lines_skipped_for_next_unit;
			continue;
		}

		if (x_ct + y_ct == 0)
			immAttrib3ub(col, col_value[0], col_value[1], col_value[2]);

		float y = (float)(y0 + i * dx);
		immVertex2f(pos, 0.0f, y);
		immVertex2f(pos, x_max, y);
		++y_ct;
	}

#if DEBUG_GRID
	int total_ct = x_ct + y_ct;
	printf("    %d + %d = %d gridlines drawn, %d skipped for next unit\n", x_ct, y_ct, total_ct, lines_skipped_for_next_unit);
#endif

	return lines_skipped_for_next_unit > 0;
}

#define GRID_MIN_PX_D 6.0
#define GRID_MIN_PX_F 6.0f

static void drawgrid(UnitSettings *unit, ARegion *ar, View3D *v3d, const char **grid_unit)
{
	RegionView3D *rv3d = ar->regiondata;

#if DEBUG_GRID
	printf("%s width %d, height %d\n", __FUNCTION__, ar->winx, ar->winy);
#endif

	double fx = rv3d->persmat[3][0];
	double fy = rv3d->persmat[3][1];
	double fw = rv3d->persmat[3][3];

	const double wx = 0.5 * ar->winx;  /* use double precision to avoid rounding errors */
	const double wy = 0.5 * ar->winy;

	double x = wx * fx / fw;
	double y = wy * fy / fw;

	double vec4[4] = { v3d->grid, v3d->grid, 0.0, 1.0 };
	mul_m4_v4d(rv3d->persmat, vec4);
	fx = vec4[0];
	fy = vec4[1];
	fw = vec4[3];

	double dx = fabs(x - wx * fx / fw);
	if (dx == 0) dx = fabs(y - wy * fy / fw);

	x += wx;
	y += wy;

	/* now x, y, and dx have their final values
	 * (x,y) is the world origin (0,0,0) mapped to Area-relative screen space
	 * dx is the distance in pixels between grid lines -- same for horiz or vert grid lines */

	glLineWidth(1.0f);

#if 0 /* TODO: write to UI/widget depth buffer, not scene depth */
	glDepthMask(GL_FALSE);  /* disable write in zbuffer */
#endif

	Gwn_VertFormat *format = immVertexFormat();
	unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	unsigned int color = GWN_vertformat_attr_add(format, "color", GWN_COMP_U8, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);

	immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

	unsigned char col[3], col2[3];
	UI_GetThemeColor3ubv(TH_GRID, col);

	if (unit->system) {
		const void *usys;
		int len;

		bUnit_GetSystem(unit->system, B_UNIT_LENGTH, &usys, &len);

		bool first = true;

		if (usys) {
			int i = len;
			while (i--) {
				double scalar = bUnit_GetScaler(usys, i);

				double dx_scalar = dx * scalar / (double)unit->scale_length;
				if (dx_scalar < (GRID_MIN_PX_D * 2.0)) {
					/* very very small grid items are less useful when dealing with units */
					continue;
				}

				if (first) {
					first = false;

					/* Store the smallest drawn grid size units name so users know how big each grid cell is */
					*grid_unit = bUnit_GetNameDisplay(usys, i);
					rv3d->gridview = (float)((scalar * (double)v3d->grid) / (double)unit->scale_length);

					int gridline_ct = gridline_count(ar, x, y, dx_scalar);
					if (gridline_ct == 0)
						goto drawgrid_cleanup; /* nothing to draw */

					immBegin(GWN_PRIM_LINES, gridline_ct * 2);
				}

				float blend_fac = 1.0f - ((GRID_MIN_PX_F * 2.0f) / (float)dx_scalar);
				/* tweak to have the fade a bit nicer */
				blend_fac = (blend_fac * blend_fac) * 2.0f;
				CLAMP(blend_fac, 0.3f, 1.0f);

				UI_GetThemeColorBlend3ubv(TH_HIGH_GRAD, TH_GRID, blend_fac, col2);

				const int skip_mod = (i == 0) ? 0 : (int)round(bUnit_GetScaler(usys, i - 1) / scalar);
#if DEBUG_GRID
				printf("%s %f, ", bUnit_GetNameDisplay(usys, i), scalar);
				if (i > 0)
					printf("next unit is %d times larger\n", skip_mod);
				else
					printf("largest unit\n");
#endif
				if (!drawgrid_draw(ar, x, y, dx_scalar, skip_mod, pos, color, col2))
					break;
			}
		}
	}
	else {
		const double sublines = v3d->gridsubdiv;
		const float  sublines_fl = v3d->gridsubdiv;

		int grids_to_draw = 2; /* first the faint fine grid, then the bold coarse grid */

		if (dx < GRID_MIN_PX_D) {
			rv3d->gridview *= sublines_fl;
			dx *= sublines;
			if (dx < GRID_MIN_PX_D) {
				rv3d->gridview *= sublines_fl;
				dx *= sublines;
				if (dx < GRID_MIN_PX_D) {
					rv3d->gridview *= sublines_fl;
					dx *= sublines;
					grids_to_draw = (dx < GRID_MIN_PX_D) ? 0 : 1;
				}
			}
		}
		else {
			if (dx > (GRID_MIN_PX_D * 10.0)) {  /* start blending in */
				rv3d->gridview /= sublines_fl;
				dx /= sublines;
				if (dx > (GRID_MIN_PX_D * 10.0)) {  /* start blending in */
					rv3d->gridview /= sublines_fl;
					dx /= sublines;
					if (dx > (GRID_MIN_PX_D * 10.0)) {
						grids_to_draw = 1;
					}
				}
			}
		}

		int gridline_ct = gridline_count(ar, x, y, dx);
		if (gridline_ct == 0)
			goto drawgrid_cleanup; /* nothing to draw */

		immBegin(GWN_PRIM_LINES, gridline_ct * 2);

		if (grids_to_draw == 2) {
			UI_GetThemeColorBlend3ubv(TH_HIGH_GRAD, TH_GRID, dx / (GRID_MIN_PX_D * 6.0), col2);
			if (drawgrid_draw(ar, x, y, dx, v3d->gridsubdiv, pos, color, col2))
				drawgrid_draw(ar, x, y, dx * sublines, 0, pos, color, col);
		}
		else if (grids_to_draw == 1) {
			drawgrid_draw(ar, x, y, dx, 0, pos, color, col);
		}
	}

	/* draw visible axes */
	/* horizontal line */
	if (0 <= y && y < ar->winy) {
		UI_make_axis_color(col, col2, ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT) ? 'Y' : 'X');
		immAttrib3ub(color, col2[0], col2[1], col2[2]);
		immVertex2f(pos, 0.0f, y);
		immVertex2f(pos, (float)ar->winx, y);
	}

	/* vertical line */
	if (0 <= x && x < ar->winx) {
		UI_make_axis_color(col, col2, ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM) ? 'Y' : 'Z');
		immAttrib3ub(color, col2[0], col2[1], col2[2]);
		immVertex2f(pos, x, 0.0f);
		immVertex2f(pos, x, (float)ar->winy);
	}

	immEnd();

drawgrid_cleanup:
	immUnbindProgram();

#if 0 /* depth write is left enabled above */
	glDepthMask(GL_TRUE);  /* enable write in zbuffer */
#endif
}

#undef DEBUG_GRID
#undef GRID_MIN_PX_D
#undef GRID_MIN_PX_F

static void drawfloor(Scene *scene, View3D *v3d, const char **grid_unit, bool write_depth)
{
	/* draw only if there is something to draw */
	if (v3d->gridflag & (V3D_SHOW_FLOOR | V3D_SHOW_X | V3D_SHOW_Y | V3D_SHOW_Z)) {
		/* draw how many lines?
		 * trunc(v3d->gridlines / 2) * 4
		 * + 2 for xy axes (possibly with special colors)
		 * + 1 for z axis (the only line not in xy plane)
		 * even v3d->gridlines are honored, odd rounded down */
		const int gridlines = v3d->gridlines / 2;
		const float grid_scale = ED_view3d_grid_scale(scene, v3d, grid_unit);
		const float grid = gridlines * grid_scale;

		const bool show_floor = (v3d->gridflag & V3D_SHOW_FLOOR) && gridlines >= 1;

		bool show_axis_x = v3d->gridflag & V3D_SHOW_X;
		bool show_axis_y = v3d->gridflag & V3D_SHOW_Y;
		bool show_axis_z = v3d->gridflag & V3D_SHOW_Z;

		unsigned char col_grid[3], col_axis[3];

		glLineWidth(1.0f);

		UI_GetThemeColor3ubv(TH_GRID, col_grid);

		if (!write_depth)
			glDepthMask(GL_FALSE);

		if (show_floor) {
			const unsigned vertex_ct = 2 * (gridlines * 4 + 2);
			const int sublines = v3d->gridsubdiv;

			unsigned char col_bg[3], col_grid_emphasise[3], col_grid_light[3];

			Gwn_VertFormat *format = immVertexFormat();
			unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
			unsigned int color = GWN_vertformat_attr_add(format, "color", GWN_COMP_U8, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);

			immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

			immBegin(GWN_PRIM_LINES, vertex_ct);

			/* draw normal grid lines */
			UI_GetColorPtrShade3ubv(col_grid, col_grid_light, 10);

			for (int a = 1; a <= gridlines; a++) {
				/* skip emphasised divider lines */
				if (a % sublines != 0) {
					const float line = a * grid_scale;

					immAttrib3ubv(color, col_grid_light);

					immVertex2f(pos, -grid, -line);
					immVertex2f(pos, +grid, -line);
					immVertex2f(pos, -grid, +line);
					immVertex2f(pos, +grid, +line);

					immVertex2f(pos, -line, -grid);
					immVertex2f(pos, -line, +grid);
					immVertex2f(pos, +line, -grid);
					immVertex2f(pos, +line, +grid);
				}
			}

			/* draw emphasised grid lines */
			UI_GetThemeColor3ubv(TH_BACK, col_bg);
			/* emphasise division lines lighter instead of darker, if background is darker than grid */
			UI_GetColorPtrShade3ubv(col_grid, col_grid_emphasise,
				(col_grid[0] + col_grid[1] + col_grid[2] + 30 >
				col_bg[0] + col_bg[1] + col_bg[2]) ? 20 : -10);

			if (sublines <= gridlines) {
				immAttrib3ubv(color, col_grid_emphasise);

				for (int a = sublines; a <= gridlines; a += sublines) {
					const float line = a * grid_scale;

					immVertex2f(pos, -grid, -line);
					immVertex2f(pos, +grid, -line);
					immVertex2f(pos, -grid, +line);
					immVertex2f(pos, +grid, +line);

					immVertex2f(pos, -line, -grid);
					immVertex2f(pos, -line, +grid);
					immVertex2f(pos, +line, -grid);
					immVertex2f(pos, +line, +grid);
				}
			}

			/* draw X axis */
			if (show_axis_x) {
				show_axis_x = false; /* drawing now, won't need to draw later */
				UI_make_axis_color(col_grid, col_axis, 'X');
				immAttrib3ubv(color, col_axis);
			}
			else
				immAttrib3ubv(color, col_grid_emphasise);

			immVertex2f(pos, -grid, 0.0f);
			immVertex2f(pos, +grid, 0.0f);

			/* draw Y axis */
			if (show_axis_y) {
				show_axis_y = false; /* drawing now, won't need to draw later */
				UI_make_axis_color(col_grid, col_axis, 'Y');
				immAttrib3ubv(color, col_axis);
			}
			else
				immAttrib3ubv(color, col_grid_emphasise);

			immVertex2f(pos, 0.0f, -grid);
			immVertex2f(pos, 0.0f, +grid);

			immEnd();
			immUnbindProgram();

			/* done with XY plane */
		}

		if (show_axis_x || show_axis_y || show_axis_z) {
			/* draw axis lines -- sometimes grid floor is off, other times we still need to draw the Z axis */

			Gwn_VertFormat *format = immVertexFormat();
			unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			unsigned int color = GWN_vertformat_attr_add(format, "color", GWN_COMP_U8, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);

			immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
			immBegin(GWN_PRIM_LINES, (show_axis_x + show_axis_y + show_axis_z) * 2);

			if (show_axis_x) {
				UI_make_axis_color(col_grid, col_axis, 'X');
				immAttrib3ubv(color, col_axis);
				immVertex3f(pos, -grid, 0.0f, 0.0f);
				immVertex3f(pos, +grid, 0.0f, 0.0f);
			}

			if (show_axis_y) {
				UI_make_axis_color(col_grid, col_axis, 'Y');
				immAttrib3ubv(color, col_axis);
				immVertex3f(pos, 0.0f, -grid, 0.0f);
				immVertex3f(pos, 0.0f, +grid, 0.0f);
			}

			if (show_axis_z) {
				UI_make_axis_color(col_grid, col_axis, 'Z');
				immAttrib3ubv(color, col_axis);
				immVertex3f(pos, 0.0f, 0.0f, -grid);
				immVertex3f(pos, 0.0f, 0.0f, +grid);
			}

			immEnd();
			immUnbindProgram();
		}

		if (!write_depth)
			glDepthMask(GL_TRUE);
	}
}

/** could move this elsewhere, but tied into #ED_view3d_grid_scale */
float ED_scene_grid_scale(Scene *scene, const char **grid_unit)
{
	/* apply units */
	if (scene->unit.system) {
		const void *usys;
		int len;

		bUnit_GetSystem(scene->unit.system, B_UNIT_LENGTH, &usys, &len);

		if (usys) {
			int i = bUnit_GetBaseUnit(usys);
			if (grid_unit)
				*grid_unit = bUnit_GetNameDisplay(usys, i);
			return (float)bUnit_GetScaler(usys, i) / scene->unit.scale_length;
		}
	}

	return 1.0f;
}

float ED_view3d_grid_scale(Scene *scene, View3D *v3d, const char **grid_unit)
{
	return v3d->grid * ED_scene_grid_scale(scene, grid_unit);
}

static bool is_cursor_visible(Scene *scene, ViewLayer *view_layer)
{
	if (U.app_flag & USER_APP_VIEW3D_HIDE_CURSOR) {
		return false;
	}

	Object *ob = OBACT(view_layer);

	/* don't draw cursor in paint modes, but with a few exceptions */
	if (ob && ob->mode & OB_MODE_ALL_PAINT) {
		/* exception: object is in weight paint and has deforming armature in pose mode */
		if (ob->mode & OB_MODE_WEIGHT_PAINT) {
			if (BKE_object_pose_armature_get(ob) != NULL) {
				return true;
			}
		}
		/* exception: object in texture paint mode, clone brush, use_clone_layer disabled */
		else if (ob->mode & OB_MODE_TEXTURE_PAINT) {
			const Paint *p = BKE_paint_get_active(scene, view_layer);

			if (p && p->brush && p->brush->imagepaint_tool == PAINT_TOOL_CLONE) {
				if ((scene->toolsettings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_CLONE) == 0) {
					return true;
				}
			}
		}

		/* no exception met? then don't draw cursor! */
		return false;
	}

	return true;
}

static void drawcursor(Scene *scene, ARegion *ar, View3D *v3d)
{
	int co[2];

	/* we don't want the clipping for cursor */
	if (ED_view3d_project_int_global(ar, ED_view3d_cursor3d_get(scene, v3d), co, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
		const float f5 = 0.25f * U.widget_unit;
		const float f10 = 0.5f * U.widget_unit;
		const float f20 = U.widget_unit;
		
		glLineWidth(1.0f);

		Gwn_VertFormat *format = immVertexFormat();
		unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		unsigned int color = GWN_vertformat_attr_add(format, "color", GWN_COMP_U8, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);

		immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

		const int segments = 16;

		immBegin(GWN_PRIM_LINE_LOOP, segments);

		for (int i = 0; i < segments; ++i) {
			float angle = 2 * M_PI * ((float)i / (float)segments);
			float x = co[0] + f10 * cosf(angle);
			float y = co[1] + f10 * sinf(angle);

			if (i % 2 == 0)
				immAttrib3ub(color, 255, 0, 0);
			else
				immAttrib3ub(color, 255, 255, 255);

			immVertex2f(pos, x, y);
		}
		immEnd();

		immUnbindProgram();

		GWN_vertformat_clear(format);
		pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

		unsigned char crosshair_color[3];
		UI_GetThemeColor3ubv(TH_VIEW_OVERLAY, crosshair_color);
		immUniformColor3ubv(crosshair_color);

		immBegin(GWN_PRIM_LINES, 8);
		immVertex2f(pos, co[0] - f20, co[1]);
		immVertex2f(pos, co[0] - f5, co[1]);
		immVertex2f(pos, co[0] + f5, co[1]);
		immVertex2f(pos, co[0] + f20, co[1]);
		immVertex2f(pos, co[0], co[1] - f20);
		immVertex2f(pos, co[0], co[1] - f5);
		immVertex2f(pos, co[0], co[1] + f5);
		immVertex2f(pos, co[0], co[1] + f20);
		immEnd();

		immUnbindProgram();
	}
}

static void draw_view_axis(RegionView3D *rv3d, const rcti *rect)
{
	const float k = U.rvisize * U.pixelsize;  /* axis size */
	const int bright = - 20 * (10 - U.rvibright);  /* axis alpha offset (rvibright has range 0-10) */

	const float startx = rect->xmin + k + 1.0f;  /* axis center in screen coordinates, x=y */
	const float starty = rect->ymin + k + 1.0f;

	float axis_pos[3][2];
	unsigned char axis_col[3][4];

	int axis_order[3] = {0, 1, 2};
	axis_sort_v3(rv3d->viewinv[2], axis_order);

	for (int axis_i = 0; axis_i < 3; axis_i++) {
		int i = axis_order[axis_i];

		/* get position of each axis tip on screen */
		float vec[3] = { 0.0f };
		vec[i] = 1.0f;
		mul_qt_v3(rv3d->viewquat, vec);
		axis_pos[i][0] = startx + vec[0] * k;
		axis_pos[i][1] = starty + vec[1] * k;

		/* get color of each axis */
		UI_GetThemeColorShade3ubv(TH_AXIS_X + i, bright, axis_col[i]); /* rgb */
		axis_col[i][3] = 255 * hypotf(vec[0], vec[1]); /* alpha */
	}

	/* draw axis lines */
	glLineWidth(2.0f);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	Gwn_VertFormat *format = immVertexFormat();
	unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	unsigned int col = GWN_vertformat_attr_add(format, "color", GWN_COMP_U8, 4, GWN_FETCH_INT_TO_FLOAT_UNIT);

	immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);
	immBegin(GWN_PRIM_LINES, 6);

	for (int axis_i = 0; axis_i < 3; axis_i++) {
		int i = axis_order[axis_i];

		immAttrib4ubv(col, axis_col[i]);
		immVertex2f(pos, startx, starty);
		immAttrib4ubv(col, axis_col[i]);
		immVertex2fv(pos, axis_pos[i]);
	}

	immEnd();
	immUnbindProgram();
	glDisable(GL_LINE_SMOOTH);

	/* draw axis names */
	for (int axis_i = 0; axis_i < 3; axis_i++) {
		int i = axis_order[axis_i];

		const char axis_text[2] = {'x' + i, '\0'};
		BLF_color4ubv(BLF_default(), axis_col[i]);
		BLF_draw_default_ascii(axis_pos[i][0] + 2, axis_pos[i][1] + 2, 0.0f, axis_text, 1);
	}
}

#ifdef WITH_INPUT_NDOF
/* draw center and axis of rotation for ongoing 3D mouse navigation */
static void UNUSED_FUNCTION(draw_rotation_guide)(RegionView3D *rv3d)
{
	float o[3];    /* center of rotation */
	float end[3];  /* endpoints for drawing */

	GLubyte color[4] = {0, 108, 255, 255};  /* bright blue so it matches device LEDs */

	negate_v3_v3(o, rv3d->ofs);

	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);  /* don't overwrite zbuf */

	Gwn_VertFormat *format = immVertexFormat();
	unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	unsigned int col = GWN_vertformat_attr_add(format, "color", GWN_COMP_U8, 4, GWN_FETCH_INT_TO_FLOAT_UNIT);

	immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);

	if (rv3d->rot_angle != 0.0f) {
		/* -- draw rotation axis -- */
		float scaled_axis[3];
		const float scale = rv3d->dist;
		mul_v3_v3fl(scaled_axis, rv3d->rot_axis, scale);


		immBegin(GWN_PRIM_LINE_STRIP, 3);
		color[3] = 0; /* more transparent toward the ends */
		immAttrib4ubv(col, color);
		add_v3_v3v3(end, o, scaled_axis);
		immVertex3fv(pos, end);

#if 0
		color[3] = 0.2f + fabsf(rv3d->rot_angle);  /* modulate opacity with angle */
		/* ^^ neat idea, but angle is frame-rate dependent, so it's usually close to 0.2 */
#endif

		color[3] = 127; /* more opaque toward the center */
		immAttrib4ubv(col, color);
		immVertex3fv(pos, o);

		color[3] = 0;
		immAttrib4ubv(col, color);
		sub_v3_v3v3(end, o, scaled_axis);
		immVertex3fv(pos, end);
		immEnd();
		
		/* -- draw ring around rotation center -- */
		{
#define     ROT_AXIS_DETAIL 13

			const float s = 0.05f * scale;
			const float step = 2.0f * (float)(M_PI / ROT_AXIS_DETAIL);

			float q[4];  /* rotate ring so it's perpendicular to axis */
			const int upright = fabsf(rv3d->rot_axis[2]) >= 0.95f;
			if (!upright) {
				const float up[3] = {0.0f, 0.0f, 1.0f};
				float vis_angle, vis_axis[3];

				cross_v3_v3v3(vis_axis, up, rv3d->rot_axis);
				vis_angle = acosf(dot_v3v3(up, rv3d->rot_axis));
				axis_angle_to_quat(q, vis_axis, vis_angle);
			}

			immBegin(GWN_PRIM_LINE_LOOP, ROT_AXIS_DETAIL);
			color[3] = 63; /* somewhat faint */
			immAttrib4ubv(col, color);
			float angle = 0.0f;
			for (int i = 0; i < ROT_AXIS_DETAIL; ++i, angle += step) {
				float p[3] = {s * cosf(angle), s * sinf(angle), 0.0f};

				if (!upright) {
					mul_qt_v3(q, p);
				}

				add_v3_v3(p, o);
				immVertex3fv(pos, p);
			}
			immEnd();

#undef      ROT_AXIS_DETAIL
		}

		color[3] = 255;  /* solid dot */
	}
	else
		color[3] = 127;  /* see-through dot */

	immUnbindProgram();

	/* -- draw rotation center -- */
	immBindBuiltinProgram(GPU_SHADER_3D_POINT_FIXED_SIZE_VARYING_COLOR);
	glPointSize(5.0f);
	immBegin(GWN_PRIM_POINTS, 1);
	immAttrib4ubv(col, color);
	immVertex3fv(pos, o);
	immEnd();
	immUnbindProgram();

#if 0
	/* find screen coordinates for rotation center, then draw pretty icon */
	mul_m4_v3(rv3d->persinv, rot_center);
	UI_icon_draw(rot_center[0], rot_center[1], ICON_NDOF_TURN);
	/* ^^ just playing around, does not work */
#endif

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}
#endif /* WITH_INPUT_NDOF */

/* ******************** info ***************** */

/**
* Render and camera border
*/
static void view3d_draw_border(const bContext *C, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	RegionView3D *rv3d = ar->regiondata;
	View3D *v3d = CTX_wm_view3d(C);

	if (rv3d->persp == RV3D_CAMOB) {
		drawviewborder(scene, depsgraph, ar, v3d);
	}
	else if (v3d->flag2 & V3D_RENDER_BORDER) {
		drawrenderborder(ar, v3d);
	}
}

/**
* Grease Pencil
*/
static void view3d_draw_grease_pencil(const bContext *UNUSED(C))
{
	/* TODO viewport */
}

/**
* Viewport Name
*/
static const char *view3d_get_name(View3D *v3d, RegionView3D *rv3d)
{
	const char *name = NULL;

	switch (rv3d->view) {
		case RV3D_VIEW_FRONT:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Front Ortho");
			else name = IFACE_("Front Persp");
			break;
		case RV3D_VIEW_BACK:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Back Ortho");
			else name = IFACE_("Back Persp");
			break;
		case RV3D_VIEW_TOP:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Top Ortho");
			else name = IFACE_("Top Persp");
			break;
		case RV3D_VIEW_BOTTOM:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Bottom Ortho");
			else name = IFACE_("Bottom Persp");
			break;
		case RV3D_VIEW_RIGHT:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Right Ortho");
			else name = IFACE_("Right Persp");
			break;
		case RV3D_VIEW_LEFT:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Left Ortho");
			else name = IFACE_("Left Persp");
			break;

		default:
			if (rv3d->persp == RV3D_CAMOB) {
				if ((v3d->camera) && (v3d->camera->type == OB_CAMERA)) {
					Camera *cam;
					cam = v3d->camera->data;
					if (cam->type == CAM_PERSP) {
						name = IFACE_("Camera Persp");
					}
					else if (cam->type == CAM_ORTHO) {
						name = IFACE_("Camera Ortho");
					}
					else {
						BLI_assert(cam->type == CAM_PANO);
						name = IFACE_("Camera Pano");
					}
				}
				else {
					name = IFACE_("Object as Camera");
				}
			}
			else {
				name = (rv3d->persp == RV3D_ORTHO) ? IFACE_("User Ortho") : IFACE_("User Persp");
			}
	}

	return name;
}

static void draw_viewport_name(ARegion *ar, View3D *v3d, const rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;
	const char *name = view3d_get_name(v3d, rv3d);
	/* increase size for unicode languages (Chinese in utf-8...) */
#ifdef WITH_INTERNATIONAL
	char tmpstr[96];
#else
	char tmpstr[32];
#endif

	if (v3d->localvd) {
		BLI_snprintf(tmpstr, sizeof(tmpstr), IFACE_("%s (Local)"), name);
		name = tmpstr;
	}

	UI_FontThemeColor(BLF_default(), TH_TEXT_HI);
#ifdef WITH_INTERNATIONAL
	BLF_draw_default(U.widget_unit + rect->xmin,  rect->ymax - U.widget_unit, 0.0f, name, sizeof(tmpstr));
#else
	BLF_draw_default_ascii(U.widget_unit + rect->xmin,  rect->ymax - U.widget_unit, 0.0f, name, sizeof(tmpstr));
#endif
}

/**
 * draw info beside axes in bottom left-corner:
 * framenum, object name, bone name (if available), marker name (if available)
 */

static void draw_selected_name(Scene *scene, Object *ob, rcti *rect)
{
	const int cfra = CFRA;
	const char *msg_pin = " (Pinned)";
	const char *msg_sep = " : ";

	const int font_id = BLF_default();

	char info[300];
	char *s = info;
	short offset = 1.5f * UI_UNIT_X + rect->xmin;

	s += sprintf(s, "(%d)", cfra);

	/*
	 * info can contain:
	 * - a frame (7 + 2)
	 * - 3 object names (MAX_NAME)
	 * - 2 BREAD_CRUMB_SEPARATORs (6)
	 * - a SHAPE_KEY_PINNED marker and a trailing '\0' (9+1) - translated, so give some room!
	 * - a marker name (MAX_NAME + 3)
	 */

	/* get name of marker on current frame (if available) */
	const char *markern = BKE_scene_find_marker_name(scene, cfra);

	/* check if there is an object */
	if (ob) {
		*s++ = ' ';
		s += BLI_strcpy_rlen(s, ob->id.name + 2);

		/* name(s) to display depends on type of object */
		if (ob->type == OB_ARMATURE) {
			bArmature *arm = ob->data;

			/* show name of active bone too (if possible) */
			if (arm->edbo) {
				if (arm->act_edbone) {
					s += BLI_strcpy_rlen(s, msg_sep);
					s += BLI_strcpy_rlen(s, arm->act_edbone->name);
				}
			}
			else if (ob->mode & OB_MODE_POSE) {
				if (arm->act_bone) {

					if (arm->act_bone->layer & arm->layer) {
						s += BLI_strcpy_rlen(s, msg_sep);
						s += BLI_strcpy_rlen(s, arm->act_bone->name);
					}
				}
			}
		}
		else if (ELEM(ob->type, OB_MESH, OB_LATTICE, OB_CURVE)) {
			/* try to display active bone and active shapekey too (if they exist) */

			if (ob->type == OB_MESH && ob->mode & OB_MODE_WEIGHT_PAINT) {
				Object *armobj = BKE_object_pose_armature_get(ob);
				if (armobj  && armobj->mode & OB_MODE_POSE) {
					bArmature *arm = armobj->data;
					if (arm->act_bone) {
						if (arm->act_bone->layer & arm->layer) {
							s += BLI_strcpy_rlen(s, msg_sep);
							s += BLI_strcpy_rlen(s, arm->act_bone->name);
						}
					}
				}
			}

			Key *key = BKE_key_from_object(ob);
			if (key) {
				KeyBlock *kb = BLI_findlink(&key->block, ob->shapenr - 1);
				if (kb) {
					s += BLI_strcpy_rlen(s, msg_sep);
					s += BLI_strcpy_rlen(s, kb->name);
					if (ob->shapeflag & OB_SHAPE_LOCK) {
						s += BLI_strcpy_rlen(s, IFACE_(msg_pin));
					}
				}
			}
		}

		/* color depends on whether there is a keyframe */
		if (id_frame_has_keyframe((ID *)ob, /* BKE_scene_frame_get(scene) */ (float)cfra, ANIMFILTER_KEYS_LOCAL))
			UI_FontThemeColor(font_id, TH_TIME_KEYFRAME);
		else if (ED_gpencil_has_keyframe_v3d(scene, ob, cfra))
			UI_FontThemeColor(font_id, TH_TIME_GP_KEYFRAME);
		else
			UI_FontThemeColor(font_id, TH_TEXT_HI);
	}
	else {
		/* no object */
		if (ED_gpencil_has_keyframe_v3d(scene, NULL, cfra))
			UI_FontThemeColor(font_id, TH_TIME_GP_KEYFRAME);
		else
			UI_FontThemeColor(font_id, TH_TEXT_HI);
	}

	if (markern) {
		s += sprintf(s, " <%s>", markern);
	}

	if (U.uiflag & USER_SHOW_ROTVIEWICON)
		offset = U.widget_unit + (U.rvisize * 2) + rect->xmin;

	BLF_draw_default(offset, 0.5f * U.widget_unit, 0.0f, info, sizeof(info));
}

/* ******************** view loop ***************** */

/**
* Information drawn on top of the solid plates and composed data
*/
void view3d_draw_region_info(const bContext *C, ARegion *ar, const int offset)
{
	RegionView3D *rv3d = ar->regiondata;
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = CTX_data_scene(C);
	wmWindowManager *wm = CTX_wm_manager(C);

	/* correct projection matrix */
	ED_region_pixelspace(ar);

	/* local coordinate visible rect inside region, to accomodate overlapping ui */
	rcti rect;
	ED_region_visible_rect(ar, &rect);

	/* Leave room for previously drawn info. */
	rect.ymax -= offset;

	view3d_draw_border(C, ar);
	view3d_draw_grease_pencil(C);

	if (U.uiflag & USER_SHOW_ROTVIEWICON) {
		draw_view_axis(rv3d, &rect);
	}

	if ((U.uiflag & USER_SHOW_FPS) && ED_screen_animation_no_scrub(wm)) {
		ED_scene_draw_fps(scene, &rect);
	}
	else if (U.uiflag & USER_SHOW_VIEWPORTNAME) {
		draw_viewport_name(ar, v3d, &rect);
	}

	if (U.uiflag & USER_DRAWVIEWINFO) {
		ViewLayer *view_layer = CTX_data_view_layer(C);
		Object *ob = OBACT(view_layer);
		draw_selected_name(scene, ob, &rect);
	}
#if 0 /* TODO */
	if (grid_unit) { /* draw below the viewport name */
		char numstr[32] = "";

		UI_FontThemeColor(BLF_default(), TH_TEXT_HI);
		if (v3d->grid != 1.0f) {
			BLI_snprintf(numstr, sizeof(numstr), "%s x %.4g", grid_unit, v3d->grid);
		}

		BLF_draw_default_ascii(rect.xmin + U.widget_unit,
		                       rect.ymax - (USER_SHOW_VIEWPORTNAME ? 2 * U.widget_unit : U.widget_unit), 0.0f,
		                       numstr[0] ? numstr : grid_unit, sizeof(numstr));
	}
#endif
}

static void view3d_draw_view(const bContext *C, ARegion *ar)
{
	EvaluationContext eval_ctx;
	CTX_data_eval_ctx(C, &eval_ctx);

	ED_view3d_draw_setup_view(CTX_wm_window(C), &eval_ctx, CTX_data_scene(C), ar, CTX_wm_view3d(C), NULL, NULL, NULL);

	/* Only 100% compliant on new spec goes bellow */
	DRW_draw_view(C);
}

void view3d_main_region_draw(const bContext *C, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	WorkSpace *workspace = CTX_wm_workspace(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ar->regiondata;
	ViewRender *view_render = BKE_viewrender_get(scene, workspace);
	RenderEngineType *type = RE_engines_find(view_render->engine_id);

	/* Provisory Blender Internal drawing */
	if (type->flag & RE_USE_LEGACY_PIPELINE) {
		view3d_main_region_draw_legacy(C, ar);
		return;
	}

	if (!rv3d->viewport) {
		rv3d->viewport = GPU_viewport_create();
	}

	GPU_viewport_bind(rv3d->viewport, &ar->winrct);
	view3d_draw_view(C, ar);
	GPU_viewport_unbind(rv3d->viewport);

	rcti rect = ar->winrct;
	BLI_rcti_translate(&rect, -ar->winrct.xmin, -ar->winrct.ymin);
	GPU_viewport_draw_to_screen(rv3d->viewport, &rect);

	GPU_free_images_old();
	GPU_pass_cache_garbage_collect();

	v3d->flag |= V3D_INVALID_BACKBUF;
}


/* -------------------------------------------------------------------- */

/** \name Offscreen Drawing
 * \{ */

static void view3d_stereo3d_setup_offscreen(
        const EvaluationContext *eval_ctx, Scene *scene, View3D *v3d, ARegion *ar,
        float winmat[4][4], const char *viewname)
{
	/* update the viewport matrices with the new camera */
	if (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D) {
		float viewmat[4][4];
		const bool is_left = STREQ(viewname, STEREO_LEFT_NAME);

		BKE_camera_multiview_view_matrix(&scene->r, v3d->camera, is_left, viewmat);
		view3d_main_region_setup_view(eval_ctx, scene, v3d, ar, viewmat, winmat, NULL);
	}
	else { /* SCE_VIEWS_FORMAT_MULTIVIEW */
		float viewmat[4][4];
		Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);

		BKE_camera_multiview_view_matrix(&scene->r, camera, false, viewmat);
		view3d_main_region_setup_view(eval_ctx, scene, v3d, ar, viewmat, winmat, NULL);
	}
}

void ED_view3d_draw_offscreen_init(const EvaluationContext *eval_ctx, Scene *scene, ViewLayer *view_layer, View3D *v3d)
{
	RenderEngineType *engine_type = eval_ctx->engine_type;
	if (engine_type->flag & RE_USE_LEGACY_PIPELINE) {
		/* shadow buffers, before we setup matrices */
		if (draw_glsl_material(scene, view_layer, NULL, v3d, v3d->drawtype)) {
			VP_deprecated_gpu_update_lamps_shadows_world(eval_ctx, scene, v3d);
		}
	}
}

/*
 * Function to clear the view
 */
static void view3d_main_region_clear(Scene *scene, View3D *v3d, ARegion *ar)
{
	glClear(GL_DEPTH_BUFFER_BIT);

	if (scene->world && (v3d->flag3 & V3D_SHOW_WORLD)) {
		VP_view3d_draw_background_world(scene, ar->regiondata);
	}
	else {
		VP_view3d_draw_background_none();
	}
}

/* ED_view3d_draw_offscreen_init should be called before this to initialize
 * stuff like shadow buffers
 */
void ED_view3d_draw_offscreen(
        const EvaluationContext *eval_ctx, Scene *scene, ViewLayer *view_layer,
        View3D *v3d, ARegion *ar, int winx, int winy,
        float viewmat[4][4], float winmat[4][4],
        bool do_bgpic, bool do_sky, bool UNUSED(is_persp), const char *viewname,
        GPUFXSettings *UNUSED(fx_settings),
        GPUOffScreen *ofs, GPUViewport *viewport)
{
	RegionView3D *rv3d = ar->regiondata;

	/* set temporary new size */
	int bwinx = ar->winx;
	int bwiny = ar->winy;
	rcti brect = ar->winrct;

	ar->winx = winx;
	ar->winy = winy;
	ar->winrct.xmin = 0;
	ar->winrct.ymin = 0;
	ar->winrct.xmax = winx;
	ar->winrct.ymax = winy;

	struct bThemeState theme_state;
	UI_Theme_Store(&theme_state);
	UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

	/* set flags */
	G.f |= G_RENDER_OGL;

	if ((v3d->flag2 & V3D_RENDER_SHADOW) == 0) {
		/* free images which can have changed on frame-change
		 * warning! can be slow so only free animated images - campbell */
		GPU_free_images_anim();
	}

	gpuPushProjectionMatrix();
	gpuLoadIdentity();
	gpuPushMatrix();
	gpuLoadIdentity();

	if ((viewname != NULL && viewname[0] != '\0') && (viewmat == NULL) && rv3d->persp == RV3D_CAMOB && v3d->camera)
		view3d_stereo3d_setup_offscreen(eval_ctx, scene, v3d, ar, winmat, viewname);
	else
		view3d_main_region_setup_view(eval_ctx, scene, v3d, ar, viewmat, winmat, NULL);

	Depsgraph *depsgraph = eval_ctx->depsgraph;

	/* main drawing call */
	RenderEngineType *engine_type = eval_ctx->engine_type;
	if (engine_type->flag & RE_USE_LEGACY_PIPELINE) {
		VP_deprecated_view3d_draw_objects(NULL, eval_ctx, scene, v3d, ar, NULL, do_bgpic, true);

		if ((v3d->flag2 & V3D_RENDER_SHADOW) == 0) {
			/* draw grease-pencil stuff */
			ED_region_pixelspace(ar);

			if (v3d->flag2 & V3D_SHOW_GPENCIL) {
				/* draw grease-pencil stuff - needed to get paint-buffer shown too (since it's 2D) */
				ED_gpencil_draw_view3d(NULL, scene, view_layer, depsgraph, v3d, ar, false);
			}

			/* freeing the images again here could be done after the operator runs, leaving for now */
			GPU_free_images_anim();
		}
	}
	else {
		DRW_draw_render_loop_offscreen(
		        depsgraph, eval_ctx->engine_type, ar, v3d,
		        do_sky, ofs, viewport);
	}

	/* restore size */
	ar->winx = bwinx;
	ar->winy = bwiny;
	ar->winrct = brect;

	gpuPopProjectionMatrix();
	gpuPopMatrix();

	UI_Theme_Restore(&theme_state);

	G.f &= ~G_RENDER_OGL;
}

/**
 * Utility func for ED_view3d_draw_offscreen
 *
 * \param ofs: Optional off-screen buffer, can be NULL.
 * (avoids re-creating when doing multiple GL renders).
 */
ImBuf *ED_view3d_draw_offscreen_imbuf(
        const EvaluationContext *eval_ctx, Scene *scene, ViewLayer *view_layer,
        View3D *v3d, ARegion *ar, int sizex, int sizey,
        unsigned int flag, unsigned int draw_flags,
        int alpha_mode, int samples, const char *viewname,
        /* output vars */
        GPUOffScreen *ofs, char err_out[256])
{
	const Depsgraph *depsgraph = eval_ctx->depsgraph;
	RegionView3D *rv3d = ar->regiondata;
	const bool draw_sky = (alpha_mode == R_ADDSKY);
	const bool draw_background = (draw_flags & V3D_OFSDRAW_USE_BACKGROUND);
	const bool use_full_sample = (draw_flags & V3D_OFSDRAW_USE_FULL_SAMPLE);

	/* view state */
	GPUFXSettings fx_settings = v3d->fx_settings;
	bool is_ortho = false;
	float winmat[4][4];

	if (ofs && ((GPU_offscreen_width(ofs) != sizex) || (GPU_offscreen_height(ofs) != sizey))) {
		/* sizes differ, can't reuse */
		ofs = NULL;
	}

	const bool own_ofs = (ofs == NULL);
	DRW_opengl_context_enable();

	if (own_ofs) {
		/* bind */
		ofs = GPU_offscreen_create(sizex, sizey, use_full_sample ? 0 : samples, true, false, err_out);
		if (ofs == NULL) {
			DRW_opengl_context_disable();
			return NULL;
		}
	}

	ED_view3d_draw_offscreen_init(eval_ctx, scene, view_layer, v3d);

	GPU_offscreen_bind(ofs, true);

	/* read in pixels & stamp */
	ImBuf *ibuf = IMB_allocImBuf(sizex, sizey, 32, flag);

	/* render 3d view */
	if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
		CameraParams params;
		Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);

		BKE_camera_params_init(&params);
		/* fallback for non camera objects */
		params.clipsta = v3d->near;
		params.clipend = v3d->far;
		BKE_camera_params_from_object(&params, camera);
		BKE_camera_multiview_params(&scene->r, &params, camera, viewname);
		BKE_camera_params_compute_viewplane(&params, sizex, sizey, scene->r.xasp, scene->r.yasp);
		BKE_camera_params_compute_matrix(&params);

		BKE_camera_to_gpu_dof(camera, &fx_settings);

		is_ortho = params.is_ortho;
		copy_m4_m4(winmat, params.winmat);
	}
	else {
		rctf viewplane;
		float clipsta, clipend;

		is_ortho = ED_view3d_viewplane_get(depsgraph, v3d, rv3d, sizex, sizey, &viewplane, &clipsta, &clipend, NULL);
		if (is_ortho) {
			orthographic_m4(winmat, viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, -clipend, clipend);
		}
		else {
			perspective_m4(winmat, viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, clipsta, clipend);
		}
	}

	if ((samples && use_full_sample) == 0) {
		/* Single-pass render, common case */
		ED_view3d_draw_offscreen(
		        eval_ctx, scene, view_layer, v3d, ar, sizex, sizey, NULL, winmat,
		        draw_background, draw_sky, !is_ortho, viewname,
		        &fx_settings, ofs, NULL);

		if (ibuf->rect_float) {
			GPU_offscreen_read_pixels(ofs, GL_FLOAT, ibuf->rect_float);
		}
		else if (ibuf->rect) {
			GPU_offscreen_read_pixels(ofs, GL_UNSIGNED_BYTE, ibuf->rect);
		}
	}
	else {
		/* Multi-pass render, use accumulation buffer & jitter for 'full' oversampling.
		 * Use because OpenGL may use a lower quality MSAA, and only over-sample edges. */
		static float jit_ofs[32][2];
		float winmat_jitter[4][4];
		float *rect_temp = (ibuf->rect_float) ? ibuf->rect_float : MEM_mallocN(sizex * sizey * sizeof(float[4]), "rect_temp");
		float *accum_buffer = MEM_mallocN(sizex * sizey * sizeof(float[4]), "accum_buffer");
		GPUViewport *viewport = GPU_viewport_create_from_offscreen(ofs);

		BLI_jitter_init(jit_ofs, samples);

		/* first sample buffer, also initializes 'rv3d->persmat' */
		ED_view3d_draw_offscreen(
		        eval_ctx, scene, view_layer, v3d, ar, sizex, sizey, NULL, winmat,
		        draw_background, draw_sky, !is_ortho, viewname,
		        &fx_settings, ofs, viewport);
		GPU_offscreen_read_pixels(ofs, GL_FLOAT, accum_buffer);

		/* skip the first sample */
		for (int j = 1; j < samples; j++) {
			copy_m4_m4(winmat_jitter, winmat);
			window_translate_m4(
			        winmat_jitter, rv3d->persmat,
			        (jit_ofs[j][0] * 2.0f) / sizex,
			        (jit_ofs[j][1] * 2.0f) / sizey);

			ED_view3d_draw_offscreen(
			        eval_ctx, scene, view_layer, v3d, ar, sizex, sizey, NULL, winmat_jitter,
			        draw_background, draw_sky, !is_ortho, viewname,
			        &fx_settings, ofs, viewport);
			GPU_offscreen_read_pixels(ofs, GL_FLOAT, rect_temp);

			unsigned int i = sizex * sizey * 4;
			while (i--) {
				accum_buffer[i] += rect_temp[i];
			}
		}

		{
			/* don't free data owned by 'ofs' */
			GPU_viewport_clear_from_offscreen(viewport);
			GPU_viewport_free(viewport);
		}

		if (ibuf->rect_float == NULL) {
			MEM_freeN(rect_temp);
		}

		if (ibuf->rect_float) {
			float *rect_float = ibuf->rect_float;
			unsigned int i = sizex * sizey * 4;
			while (i--) {
				rect_float[i] = accum_buffer[i] / samples;
			}
		}
		else {
			unsigned char *rect_ub = (unsigned char *)ibuf->rect;
			unsigned int i = sizex * sizey * 4;
			while (i--) {
				rect_ub[i] = (unsigned char)(255.0f * accum_buffer[i] / samples);
			}
		}

		MEM_freeN(accum_buffer);
	}

	/* unbind */
	GPU_offscreen_unbind(ofs, true);

	if (own_ofs) {
		GPU_offscreen_free(ofs);
	}

	DRW_opengl_context_disable();

	if (ibuf->rect_float && ibuf->rect)
		IMB_rect_from_float(ibuf);

	return ibuf;
}

/**
 * Creates own fake 3d views (wrapping #ED_view3d_draw_offscreen_imbuf)
 *
 * \param ofs: Optional off-screen buffer can be NULL.
 * (avoids re-creating when doing multiple GL renders).
 *
 * \note used by the sequencer
 */
ImBuf *ED_view3d_draw_offscreen_imbuf_simple(
        const EvaluationContext *eval_ctx, Scene *scene, ViewLayer *view_layer,
        Object *camera, int width, int height,
        unsigned int flag, unsigned int draw_flags, int drawtype,
        int alpha_mode, int samples, const char *viewname,
        GPUOffScreen *ofs, char err_out[256])
{
	View3D v3d = {NULL};
	ARegion ar = {NULL};
	RegionView3D rv3d = {{{0}}};

	/* connect data */
	v3d.regionbase.first = v3d.regionbase.last = &ar;
	ar.regiondata = &rv3d;
	ar.regiontype = RGN_TYPE_WINDOW;

	v3d.camera = camera;
	v3d.lay = scene->lay;
	v3d.drawtype = drawtype;
	v3d.flag2 = V3D_RENDER_OVERRIDE;

	if (draw_flags & V3D_OFSDRAW_USE_GPENCIL) {
		v3d.flag2 |= V3D_SHOW_GPENCIL;
	}
	if (draw_flags & V3D_OFSDRAW_USE_SOLID_TEX) {
		v3d.flag2 |= V3D_SOLID_TEX;
	}
	if (draw_flags & V3D_OFSDRAW_USE_BACKGROUND) {
		v3d.flag3 |= V3D_SHOW_WORLD;
	}
	if (draw_flags & V3D_OFSDRAW_USE_CAMERA_DOF) {
		if (camera->type == OB_CAMERA) {
			v3d.fx_settings.dof = &((Camera *)camera->data)->gpu_dof;
			v3d.fx_settings.fx_flag |= GPU_FX_FLAG_DOF;
		}
	}

	rv3d.persp = RV3D_CAMOB;

	copy_m4_m4(rv3d.viewinv, v3d.camera->obmat);
	normalize_m4(rv3d.viewinv);
	invert_m4_m4(rv3d.viewmat, rv3d.viewinv);

	{
		CameraParams params;
		Object *view_camera = BKE_camera_multiview_render(scene, v3d.camera, viewname);

		BKE_camera_params_init(&params);
		BKE_camera_params_from_object(&params, view_camera);
		BKE_camera_multiview_params(&scene->r, &params, view_camera, viewname);
		BKE_camera_params_compute_viewplane(&params, width, height, scene->r.xasp, scene->r.yasp);
		BKE_camera_params_compute_matrix(&params);

		copy_m4_m4(rv3d.winmat, params.winmat);
		v3d.near = params.clipsta;
		v3d.far = params.clipend;
		v3d.lens = params.lens;
	}

	mul_m4_m4m4(rv3d.persmat, rv3d.winmat, rv3d.viewmat);
	invert_m4_m4(rv3d.persinv, rv3d.viewinv);

	return ED_view3d_draw_offscreen_imbuf(
	        eval_ctx, scene, view_layer, &v3d, &ar, width, height, flag,
	        draw_flags, alpha_mode, samples, viewname, ofs, err_out);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Legacy Interface
 *
 * This will be removed once the viewport gets replaced
 * meanwhile it should keep the old viewport working.
 *
 * \{ */

void VP_legacy_drawcursor(Scene *scene, ViewLayer *view_layer, ARegion *ar, View3D *v3d)
{
	if (is_cursor_visible(scene, view_layer)) {
		drawcursor(scene, ar, v3d);
	}
}

void VP_legacy_draw_view_axis(RegionView3D *rv3d, const rcti *rect)
{
	draw_view_axis(rv3d, rect);
}

void VP_legacy_draw_viewport_name(ARegion *ar, View3D *v3d, const rcti *rect)
{
	draw_viewport_name(ar, v3d, rect);
}

void VP_legacy_draw_selected_name(Scene *scene, Object *ob, rcti *rect)
{
	draw_selected_name(scene, ob, rect);
}

void VP_legacy_drawgrid(UnitSettings *unit, ARegion *ar, View3D *v3d, const char **grid_unit)
{
	drawgrid(unit, ar, v3d, grid_unit);
}

void VP_legacy_drawfloor(Scene *scene, View3D *v3d, const char **grid_unit, bool write_depth)
{
	drawfloor(scene, v3d, grid_unit, write_depth);
}

void VP_legacy_view3d_main_region_setup_view(
        const EvaluationContext *eval_ctx, Scene *scene, View3D *v3d,
        ARegion *ar, float viewmat[4][4], float winmat[4][4])
{
	view3d_main_region_setup_view(eval_ctx, scene, v3d, ar, viewmat, winmat, NULL);
}

bool VP_legacy_view3d_stereo3d_active(wmWindow *win, Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	return view3d_stereo3d_active(win, scene, v3d, rv3d);
}

void VP_legacy_view3d_stereo3d_setup(const EvaluationContext *eval_ctx, Scene *scene, View3D *v3d, ARegion *ar)
{
	view3d_stereo3d_setup(eval_ctx, scene, v3d, ar, NULL);
}

bool VP_legacy_use_depth(View3D *v3d, Object *obedit)
{
	return use_depth_doit(v3d, obedit);
}

void VP_drawviewborder(Scene *scene, const struct Depsgraph *depsgraph, ARegion *ar, View3D *v3d)
{
	drawviewborder(scene, depsgraph, ar, v3d);
}

void VP_drawrenderborder(ARegion *ar, View3D *v3d)
{
	drawrenderborder(ar, v3d);
}

void VP_view3d_draw_background_none(void)
{
	if (UI_GetThemeValue(TH_SHOW_BACK_GRAD)) {
		view3d_draw_background_gradient();
	}
	else {
		view3d_draw_background_none();
	}
}

void VP_view3d_draw_background_world(Scene *scene, RegionView3D *rv3d)
{
	view3d_draw_background_world(scene, rv3d);
}

void VP_view3d_main_region_clear(Scene *scene, View3D *v3d, ARegion *ar)
{
	view3d_main_region_clear(scene, v3d, ar);
}

/** \} */
