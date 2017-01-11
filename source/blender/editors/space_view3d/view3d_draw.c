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

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_scene.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_unit.h"

#include "BLF_api.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_threads.h"

#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_screen.h"
#include "ED_transform.h"

#include "GPU_matrix.h"
#include "GPU_immediate.h"
#include "GPU_material.h"
#include "GPU_viewport.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RE_engine.h"

#include "WM_api.h"

#include "view3d_intern.h"  /* own include */

/* prototypes */
static void draw_all_objects(const bContext *C, ARegion *ar, const bool only_depth, const bool use_depth);

typedef struct DrawData {
	rcti border_rect;
	bool render_border;
	bool clip_border;
	bool is_render;
	GPUViewport *viewport;
} DrawData;

static void view3d_draw_data_init(const bContext *C, ARegion *ar, RegionView3D *rv3d, DrawData *draw_data)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);

	draw_data->is_render = (v3d->drawtype == OB_RENDER);

	draw_data->render_border = ED_view3d_calc_render_border(scene, v3d, ar, &draw_data->border_rect);
	draw_data->clip_border = (draw_data->render_border && !BLI_rcti_compare(&ar->drawrct, &draw_data->border_rect));

	draw_data->viewport = rv3d->viewport;
}

/* ******************** general functions ***************** */

static bool use_depth_doit(Scene *scene, View3D *v3d)
{
	if (v3d->drawtype > OB_WIRE)
		return true;

	/* special case (depth for wire color) */
	if (v3d->drawtype <= OB_WIRE) {
		if (scene->obedit && scene->obedit->type == OB_MESH) {
			Mesh *me = scene->obedit->data;
			if (me->drawflag & ME_DRAWEIGHT) {
				return true;
			}
		}
	}
	return false;
}

static bool use_depth(const bContext *C)
{
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = CTX_data_scene(C);
	return use_depth_doit(scene, v3d);
}

/**
 * \note keep this synced with #ED_view3d_mats_rv3d_backup/#ED_view3d_mats_rv3d_restore
 */
void ED_view3d_update_viewmat(Scene *scene, View3D *v3d, ARegion *ar, float viewmat[4][4], float winmat[4][4])
{
	RegionView3D *rv3d = ar->regiondata;


	/* setup window matrices */
	if (winmat)
		copy_m4_m4(rv3d->winmat, winmat);
	else
		view3d_winmatrix_set(ar, v3d, NULL);

	/* setup view matrix */
	if (viewmat)
		copy_m4_m4(rv3d->viewmat, viewmat);
	else
		view3d_viewmatrix_set(scene, v3d, rv3d);  /* note: calls BKE_object_where_is_calc for camera... */

	/* update utility matrices */
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);
	invert_m4_m4(rv3d->persinv, rv3d->persmat);
	invert_m4_m4(rv3d->viewinv, rv3d->viewmat);

	/* calculate GLSL view dependent values */

	/* store window coordinates scaling/offset */
	if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
		rctf cameraborder;
		ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &cameraborder, false);
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

static void view3d_main_region_setup_view(Scene *scene, View3D *v3d, ARegion *ar, float viewmat[4][4], float winmat[4][4])
{
	RegionView3D *rv3d = ar->regiondata;

	ED_view3d_update_viewmat(scene, v3d, ar, viewmat, winmat);

	/* set for opengl */
	/* TODO(merwin): transition to GPU_matrix API */
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(rv3d->winmat);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(rv3d->viewmat);
}

static bool view3d_stereo3d_active(const bContext *C, Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	wmWindow *win = CTX_wm_window(C);

	if ((scene->r.scemode & R_MULTIVIEW) == 0)
		return false;

	if (WM_stereo3d_enabled(win, true) == false)
		return false;

	if ((v3d->camera == NULL) || (v3d->camera->type != OB_CAMERA) || rv3d->persp != RV3D_CAMOB)
		return false;

	if (scene->r.views_format & SCE_VIEWS_FORMAT_MULTIVIEW) {
		if (v3d->stereo3d_camera == STEREO_MONO_ID)
			return false;

		return BKE_scene_multiview_is_stereo3d(&scene->r);
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
static void view3d_stereo3d_setup(Scene *scene, View3D *v3d, ARegion *ar)
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

		BLI_lock_thread(LOCK_VIEW3D);
		data->shiftx = BKE_camera_multiview_shift_x(&scene->r, v3d->camera, viewname);

		BKE_camera_multiview_view_matrix(&scene->r, v3d->camera, is_left, viewmat);
		view3d_main_region_setup_view(scene, v3d, ar, viewmat, NULL);

		data->shiftx = shiftx;
		BLI_unlock_thread(LOCK_VIEW3D);
	}
	else { /* SCE_VIEWS_FORMAT_MULTIVIEW */
		float viewmat[4][4];
		Object *view_ob = v3d->camera;
		Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);

		BLI_lock_thread(LOCK_VIEW3D);
		v3d->camera = camera;

		BKE_camera_multiview_view_matrix(&scene->r, camera, false, viewmat);
		view3d_main_region_setup_view(scene, v3d, ar, viewmat, NULL);

		v3d->camera = view_ob;
		BLI_unlock_thread(LOCK_VIEW3D);
	}
}

/* ******************** debug ***************** */

#define VIEW3D_DRAW_DEBUG 1
/* TODO: expand scope of this flag so UI reflects the underlying code */

#if VIEW3D_DRAW_DEBUG

static void view3d_draw_debug_store_depth(ARegion *UNUSED(ar), DrawData *draw_data)
{
	GPUViewport *viewport = draw_data->viewport;
	GLint viewport_size[4];
	glGetIntegerv(GL_VIEWPORT, viewport_size);

	const int x = viewport_size[0];
	const int y = viewport_size[1];
	const int w = viewport_size[2];
	const int h = viewport_size[3];

	if (GPU_viewport_debug_depth_is_valid(viewport)) {
		if ((GPU_viewport_debug_depth_width(viewport) != w) ||
			(GPU_viewport_debug_depth_height(viewport) != h))
		{
			GPU_viewport_debug_depth_free(viewport);
		}
	}

	if (!GPU_viewport_debug_depth_is_valid(viewport)) {
		char error[256];
		if (!GPU_viewport_debug_depth_create(viewport, w, h, error)) {
			fprintf(stderr, "Failed to create depth buffer for debug: %s\n", error);
			return;
		}
	}

	GPU_viewport_debug_depth_store(viewport, x, y);
}

static void view3d_draw_debug_post_solid(const bContext *C, ARegion *ar, DrawData *draw_data)
{
	View3D *v3d = CTX_wm_view3d(C);

	if ((v3d->tmp_compat_flag & V3D_DEBUG_SHOW_SCENE_DEPTH) != 0) {
		view3d_draw_debug_store_depth(ar, draw_data);
	}
}

static void view3d_draw_debug(const bContext *C, ARegion *ar, DrawData *draw_data)
{
	View3D *v3d = CTX_wm_view3d(C);

	if ((v3d->tmp_compat_flag & V3D_DEBUG_SHOW_COMBINED_DEPTH) != 0) {
		/* store */
		view3d_draw_debug_store_depth(ar, draw_data);
	}

	if (((v3d->tmp_compat_flag & V3D_DEBUG_SHOW_SCENE_DEPTH) != 0) ||
		((v3d->tmp_compat_flag & V3D_DEBUG_SHOW_COMBINED_DEPTH) != 0))
	{
		/* draw */
		if (GPU_viewport_debug_depth_is_valid(draw_data->viewport)) {
			GPU_viewport_debug_depth_draw(draw_data->viewport, v3d->debug.znear, v3d->debug.zfar);
		}
	}
	else {
		/* cleanup */
		GPU_viewport_debug_depth_free(draw_data->viewport);
	}
}

#endif /* VIEW3D_DRAW_DEBUG */

/* ******************** view border ***************** */

static void view3d_camera_border(
        const Scene *scene, const ARegion *ar, const View3D *v3d, const RegionView3D *rv3d,
        rctf *r_viewborder, const bool no_shift, const bool no_zoom)
{
	CameraParams params;
	rctf rect_view, rect_camera;

	/* get viewport viewplane */
	BKE_camera_params_init(&params);
	BKE_camera_params_from_view3d(&params, v3d, rv3d);
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
        const Scene *scene, const ARegion *ar, const View3D *v3d, const RegionView3D *rv3d,
        float r_size[2])
{
	rctf viewborder;

	view3d_camera_border(scene, ar, v3d, rv3d, &viewborder, true, true);
	r_size[0] = BLI_rctf_size_x(&viewborder);
	r_size[1] = BLI_rctf_size_y(&viewborder);
}

void ED_view3d_calc_camera_border(
        const Scene *scene, const ARegion *ar, const View3D *v3d, const RegionView3D *rv3d,
        rctf *r_viewborder, const bool no_shift)
{
	view3d_camera_border(scene, ar, v3d, rv3d, r_viewborder, no_shift, false);
}

static void drawviewborder_grid3(unsigned pos, float x1, float x2, float y1, float y2, float fac)
{
	float x3, y3, x4, y4;

	x3 = x1 + fac * (x2 - x1);
	y3 = y1 + fac * (y2 - y1);
	x4 = x1 + (1.0f - fac) * (x2 - x1);
	y4 = y1 + (1.0f - fac) * (y2 - y1);

	immBegin(GL_LINES, 8);
	immVertex2f(pos, x1, y3);
	immVertex2f(pos, x2, y3);

	immVertex2f(pos, x1, y4);
	immVertex2f(pos, x2, y4);

	immVertex2f(pos, x3, y1);
	immVertex2f(pos, x3, y2);

	immVertex2f(pos, x4, y1);
	immVertex2f(pos, x4, y2);
	immEnd();
}

/* harmonious triangle */
static void drawviewborder_triangle(unsigned pos, float x1, float x2, float y1, float y2, const char golden, const char dir)
{
	float ofs;
	float w = x2 - x1;
	float h = y2 - y1;

	immBegin(GL_LINES, 6);
	if (w > h) {
		if (golden) {
			ofs = w * (1.0f - (1.0f / 1.61803399f));
		}
		else {
			ofs = h * (h / w);
		}
		if (dir == 'B') SWAP(float, y1, y2);

		immVertex2f(pos, x1, y1);
		immVertex2f(pos, x2, y2);

		immVertex2f(pos, x2, y1);
		immVertex2f(pos, x1 + (w - ofs), y2);

		immVertex2f(pos, x1, y2);
		immVertex2f(pos, x1 + ofs, y1);
	}
	else {
		if (golden) {
			ofs = h * (1.0f - (1.0f / 1.61803399f));
		}
		else {
			ofs = w * (w / h);
		}
		if (dir == 'B') SWAP(float, x1, x2);

		immVertex2f(pos, x1, y1);
		immVertex2f(pos, x2, y2);

		immVertex2f(pos, x2, y1);
		immVertex2f(pos, x1, y1 + ofs);

		immVertex2f(pos, x1, y2);
		immVertex2f(pos, x2, y1 + (h - ofs));
	}
	immEnd();
}

static void drawviewborder(Scene *scene, ARegion *ar, View3D *v3d)
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

	ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &viewborder, false);
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

	/* use the same program for everything */
	unsigned pos = add_attrib(immVertexFormat(), "pos", COMP_F32, 2, KEEP_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	/* passepartout, specified in camera edit buttons */
	if (ca && (ca->flag & CAM_SHOWPASSEPARTOUT) && ca->passepartalpha > 0.000001f) {
		const float winx = (ar->winx + 1);
		const float winy = (ar->winy + 1);

		float alpha = 1.0f;

		if (ca->passepartalpha != 1.0f) {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
			alpha = ca->passepartalpha;
		}

		immUniformColor4f(0.0f, 0.0f, 0.0f, alpha);

		if (x1i > 0.0f)
			immRectf(pos, 0.0f, winy, x1i, 0.0f);
		if (x2i < winx)
			immRectf(pos, x2i, winy, winx, 0.0f);
		if (y2i < winy)
			immRectf(pos, x1i, winy, x2i, y2i);
		if (y2i > 0.0f)
			immRectf(pos, x1i, y1i, x2i, 0.0f);

		glDisable(GL_BLEND);
	}

	setlinestyle(0);

	immUniformThemeColor(TH_BACK);
	imm_draw_line_box(pos, x1i, y1i, x2i, y2i);

#ifdef VIEW3D_CAMERA_BORDER_HACK
	if (view3d_camera_border_hack_test == true) {
		immUniformColor3ubv(view3d_camera_border_hack_col);
		imm_draw_line_box(pos, x1i + 1, y1i + 1, x2i - 1, y2i - 1);
		view3d_camera_border_hack_test = false;
	}
#endif

	setlinestyle(3);

	/* outer line not to confuse with object selecton */
	if (v3d->flag2 & V3D_LOCK_CAMERA) {
		immUniformThemeColor(TH_REDALERT);
		imm_draw_line_box(pos, x1i - 1, y1i - 1, x2i + 1, y2i + 1);
	}

	immUniformThemeColor(TH_VIEW_OVERLAY);
	imm_draw_line_box(pos, x1i, y1i, x2i, y2i);

	/* border */
	if (scene->r.mode & R_BORDER) {
		float x3, y3, x4, y4;

		x3 = floorf(x1 + (scene->r.border.xmin * (x2 - x1))) - 1;
		y3 = floorf(y1 + (scene->r.border.ymin * (y2 - y1))) - 1;
		x4 = floorf(x1 + (scene->r.border.xmax * (x2 - x1))) + (U.pixelsize - 1);
		y4 = floorf(y1 + (scene->r.border.ymax * (y2 - y1))) + (U.pixelsize - 1);

		imm_cpack(0x4040FF);
		imm_draw_line_box(pos, x3, y3, x4, y4);
	}

	/* safety border */
	if (ca) {
		if (ca->dtx & CAM_DTX_CENTER) {
			float x3, y3;

			x3 = x1 + 0.5f * (x2 - x1);
			y3 = y1 + 0.5f * (y2 - y1);

			immUniformThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25f, 0);
			immBegin(GL_LINES, 4);

			immVertex2f(pos, x1, y3);
			immVertex2f(pos, x2, y3);

			immVertex2f(pos, x3, y1);
			immVertex2f(pos, x3, y2);

			immEnd();
		}

		if (ca->dtx & CAM_DTX_CENTER_DIAG) {

			immUniformThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25f, 0);
			immBegin(GL_LINES, 4);

			immVertex2f(pos, x1, y1);
			immVertex2f(pos, x2, y2);

			immVertex2f(pos, x1, y2);
			immVertex2f(pos, x2, y1);

			immEnd();
		}

		if (ca->dtx & CAM_DTX_THIRDS) {
			immUniformThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25f, 0);
			drawviewborder_grid3(pos, x1, x2, y1, y2, 1.0f / 3.0f);
		}

		if (ca->dtx & CAM_DTX_GOLDEN) {
			immUniformThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25f, 0);
			drawviewborder_grid3(pos, x1, x2, y1, y2, 1.0f - (1.0f / 1.61803399f));
		}

		if (ca->dtx & CAM_DTX_GOLDEN_TRI_A) {
			immUniformThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25f, 0);
			drawviewborder_triangle(pos, x1, x2, y1, y2, 0, 'A');
		}

		if (ca->dtx & CAM_DTX_GOLDEN_TRI_B) {
			immUniformThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25f, 0);
			drawviewborder_triangle(pos, x1, x2, y1, y2, 0, 'B');
		}

		if (ca->dtx & CAM_DTX_HARMONY_TRI_A) {
			immUniformThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25f, 0);
			drawviewborder_triangle(pos, x1, x2, y1, y2, 1, 'A');
		}

		if (ca->dtx & CAM_DTX_HARMONY_TRI_B) {
			immUniformThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25f, 0);
			drawviewborder_triangle(pos, x1, x2, y1, y2, 1, 'B');
		}

		if (ca->flag & CAM_SHOW_SAFE_MARGINS) {
			UI_draw_safe_areas(
			        pos, x1, x2, y1, y2,
			        scene->safe_areas.title,
			        scene->safe_areas.action);

			if (ca->flag & CAM_SHOW_SAFE_CENTER) {
				UI_draw_safe_areas(
				        pos, x1, x2, y1, y2,
				        scene->safe_areas.title_center,
				        scene->safe_areas.action_center);
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
			float color[4];
			UI_GetThemeColorShade4fv(TH_VIEW_OVERLAY, 100, color);
			UI_draw_roundbox_gl_mode(GL_LINE_LOOP, rect.xmin, rect.ymin, rect.xmax, rect.ymax, 2.0f, color);
		}
	}

	setlinestyle(0);

	/* camera name - draw in highlighted text color */
	if (ca && (ca->flag & CAM_SHOWNAME)) {
		UI_ThemeColor(TH_TEXT_HI);
		BLF_draw_default(
		        x1i, y1i - (0.7f * U.widget_unit), 0.0f,
		        v3d->camera->id.name + 2, sizeof(v3d->camera->id.name) - 2);
	}

	immUnbindProgram();
}

static void drawrenderborder(ARegion *ar, View3D *v3d)
{
	/* use the same program for everything */
	unsigned pos = add_attrib(immVertexFormat(), "pos", COMP_F32, 2, KEEP_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	glLineWidth(1.0f);
	setlinestyle(3);
	imm_cpack(0x4040FF);

	imm_draw_line_box(
	    pos, v3d->render_border.xmin * ar->winx, v3d->render_border.ymin * ar->winy,
	    v3d->render_border.xmax * ar->winx, v3d->render_border.ymax * ar->winy);

	setlinestyle(0);

	immUnbindProgram();
}

/* ******************** offline engine ***************** */

static bool view3d_draw_render_draw(const bContext *C, Scene *scene,
    ARegion *ar, View3D *UNUSED(v3d),
    bool clip_border, const rcti *border_rect)
{
	RegionView3D *rv3d = ar->regiondata;
	RenderEngineType *type;
	GLint scissor[4];

	/* create render engine */
	if (!rv3d->render_engine) {
		RenderEngine *engine;

		type = RE_engines_find(scene->r.engine);

		if (!(type->view_update && type->view_draw))
			return false;

		engine = RE_engine_create_ex(type, true);

		engine->tile_x = scene->r.tilex;
		engine->tile_y = scene->r.tiley;

		type->view_update(engine, C);

		rv3d->render_engine = engine;
	}

	/* background draw */
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	ED_region_pixelspace(ar);

	if (clip_border) {
		/* for border draw, we only need to clear a subset of the 3d view */
		if (border_rect->xmax > border_rect->xmin && border_rect->ymax > border_rect->ymin) {
			glGetIntegerv(GL_SCISSOR_BOX, scissor);
			glScissor(border_rect->xmin, border_rect->ymin,
				BLI_rcti_size_x(border_rect), BLI_rcti_size_y(border_rect));
		}
		else {
			return false;
		}
	}

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	/* don't change depth buffer */
	glClear(GL_COLOR_BUFFER_BIT); /* is this necessary? -- merwin */

	/* render result draw */
	type = rv3d->render_engine->type;
	type->view_draw(rv3d->render_engine, C);

	if (clip_border) {
		/* restore scissor as it was before */
		glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
	}

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	return true;
}

/* ******************** background plates ***************** */

static void view3d_draw_background_gradient(void)
{
	gpuMatrixBegin3D(); /* TODO: finish 2D API */

	glClear(GL_DEPTH_BUFFER_BIT);

	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
	unsigned color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);
	unsigned char col_hi[3], col_lo[3];

	immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

	UI_GetThemeColor3ubv(TH_LOW_GRAD, col_lo);
	UI_GetThemeColor3ubv(TH_HIGH_GRAD, col_hi);

	immBegin(GL_QUADS, 4);
	immAttrib3ubv(color, col_lo);
	immVertex2f(pos, -1.0f, -1.0f);
	immVertex2f(pos, 1.0f, -1.0f);

	immAttrib3ubv(color, col_hi);
	immVertex2f(pos, 1.0f, 1.0f);
	immVertex2f(pos, -1.0f, 1.0f);
	immEnd();

	immUnbindProgram();

	gpuMatrixEnd();
}

static void view3d_draw_background_none(void)
{
	if (UI_GetThemeValue(TH_SHOW_BACK_GRAD)) {
		view3d_draw_background_gradient();
	}
	else {
		UI_ThemeClearColorAlpha(TH_HIGH_GRAD, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
}

static void view3d_draw_background_world(Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	if (scene->world) {
		GPUMaterial *gpumat = GPU_material_world(scene, scene->world);

		/* calculate full shader for background */
		GPU_material_bind(gpumat, 1, 1, 1.0f, false, rv3d->viewmat, rv3d->viewinv, rv3d->viewcamtexcofac, (v3d->scenelock != 0));

		if (GPU_material_bound(gpumat)) {

			glClear(GL_DEPTH_BUFFER_BIT);

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
		}
		else {
			view3d_draw_background_none();
		}
	}
	else {
		view3d_draw_background_none();
	}
}

/* ******************** solid plates ***************** */

/**
 * Clear the buffer and draw the proper shader
 */
static void view3d_draw_background(const bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	/* Background functions do not read or write depth, but they do clear or completely
	 * overwrite color buffer. It's more efficient to clear color & depth in once call, so
	 * background functions do this even though they don't use depth.
	 */

	switch (v3d->debug.background) {
		case V3D_DEBUG_BACKGROUND_WORLD:
			view3d_draw_background_world(scene, v3d, rv3d);
			break;
		case V3D_DEBUG_BACKGROUND_GRADIENT:
			view3d_draw_background_gradient();
			break;
		case V3D_DEBUG_BACKGROUND_NONE:
		default:
			view3d_draw_background_none();
			break;
	}
}

/**
 *
 */
static void view3d_draw_render_solid_surfaces(const bContext *C, ARegion *ar, const bool UNUSED(run_screen_shaders))
{
	/* TODO viewport */
	draw_all_objects(C, ar, false, use_depth(C));
}

/**
 *
 */
static void view3d_draw_render_transparent_surfaces(const bContext *UNUSED(C))
{
	/* TODO viewport */
}

/**
 *
 */
static void view3d_draw_post_draw(const bContext *UNUSED(C))
{
	/* TODO viewport */
}

/* ******************** geometry overlay ***************** */

/**
 * Front/back wire frames
 */
static void view3d_draw_wire_plates(const bContext *UNUSED(C))
{
	/* TODO viewport */
}

/**
 * Special treatment for selected objects
 */
static void view3d_draw_outline_plates(const bContext *UNUSED(C))
{
	/* TODO viewport */
}

/* ******************** other elements ***************** */


#define DEBUG_GRID 0

static void gridline_range(double x0, double dx, double max, int* first_out, int* count_out)
{
	/* determine range of gridlines that appear in this Area -- similar calc but separate ranges for x & y
	* x0 is gridline 0, the axis in screen space
	* Area covers [0 .. max) pixels */

	int first = (int)ceil(-x0 / dx);
	int last = (int)floor((max - x0) / dx);

	if (first <= last) {
		*first_out = first;
		*count_out = last - first + 1;
	}
	else {
		*first_out = 0;
		*count_out = 0;
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

	VertexFormat* format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
	unsigned color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);

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

					immBegin(GL_LINES, gridline_ct * 2);
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
			if (dx >(GRID_MIN_PX_D * 10.0)) {  /* start blending in */
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

		immBegin(GL_LINES, gridline_ct * 2);

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

			VertexFormat* format = immVertexFormat();
			unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
			unsigned color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);

			immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

			immBegin(GL_LINES, vertex_ct);

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

			VertexFormat* format = immVertexFormat();
			unsigned pos = add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);
			unsigned color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);

			immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
			immBegin(GL_LINES, (show_axis_x + show_axis_y + show_axis_z) * 2);

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


static void view3d_draw_grid(const bContext *C, ARegion *ar)
{
	/* TODO viewport
	 * Missing is the flags to check whether to draw it
	 * for now now we are using the flags in v3d itself.
	 *
	 * Also for now always assume depth is there, so we
	 * draw on top of it.
	 */
	/**
	 * Calculate pixel-size factor once, is used for lamps and object centers.
	 * Used by #ED_view3d_pixel_size and typically not accessed directly.
	 *
	 * \note #BKE_camera_params_compute_viewplane' also calculates a pixel-size value,
	 * passed to #RE_SetPixelSize, in ortho mode this is compatible with this value,
	 * but in perspective mode its offset by the near-clip.
	 *
	 * 'RegionView3D.pixsize' is used for viewport drawing, not rendering.
	 */
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ar->regiondata;

	const bool draw_floor = (rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO);
	const char *grid_unit = NULL;

	/* ortho grid goes first, does not write to depth buffer and doesn't need depth test so it will override
	 * objects if done last
	 * needs to be done always, gridview is adjusted in drawgrid() now, but only for ortho views.
	 */
	rv3d->gridview = ED_view3d_grid_scale(scene, v3d, &grid_unit);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE); /* read & test depth, but don't alter it. TODO: separate UI depth buffer */

	if (!draw_floor) {
		ED_region_pixelspace(ar);
		*(&grid_unit) = NULL;  /* drawgrid need this to detect/affect smallest valid unit... */
		drawgrid(&scene->unit, ar, v3d, &grid_unit);

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(rv3d->winmat);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(rv3d->viewmat);
	}
	else {
		drawfloor(scene, v3d, &grid_unit, false);
	}

	glDisable(GL_DEPTH_TEST);
}

static bool is_cursor_visible(Scene *scene)
{
	Object *ob = OBACT;

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
			const Paint *p = BKE_paint_get_active(scene);

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

		VertexFormat* format = immVertexFormat();
		unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
		unsigned color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

		const int segments = 16;

		immBegin(GL_LINE_LOOP, segments);

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

		VertexFormat_clear(format);
		pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

		unsigned char crosshair_color[3];
		UI_GetThemeColor3ubv(TH_VIEW_OVERLAY, crosshair_color);
		immUniformColor3ubv(crosshair_color);

		immBegin(GL_LINES, 8);
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

static void draw_view_axis(RegionView3D *rv3d, rcti *rect)
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
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
	unsigned col = add_attrib(format, "color", COMP_U8, 4, NORMALIZE_INT_TO_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);
	immBegin(GL_LINES, 6);

	for (int axis_i = 0; axis_i < 3; axis_i++) {
		int i = axis_order[axis_i];

		immAttrib4ubv(col, axis_col[i]);
		immVertex2f(pos, startx, starty);
		immVertex2fv(pos, axis_pos[i]);
	}

	immEnd();
	immUnbindProgram();
	glDisable(GL_LINE_SMOOTH);

	/* draw axis names */
	for (int axis_i = 0; axis_i < 3; axis_i++) {
		int i = axis_order[axis_i];

		const char axis_text[2] = {'x' + i, '\0'};
		glColor4ubv(axis_col[i]); /* text shader still uses gl_Color */
		BLF_draw_default_ascii(axis_pos[i][0] + 2, axis_pos[i][1] + 2, 0.0f, axis_text, 1);
	}

	/* BLF_draw_default disabled blending for us */
}

#ifdef WITH_INPUT_NDOF
/* draw center and axis of rotation for ongoing 3D mouse navigation */
static void draw_rotation_guide(RegionView3D *rv3d)
{
	float o[3];    /* center of rotation */
	float end[3];  /* endpoints for drawing */

	GLubyte color[4] = {0, 108, 255, 255};  /* bright blue so it matches device LEDs */

	negate_v3_v3(o, rv3d->ofs);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPointSize(5.0f);
	glEnable(GL_POINT_SMOOTH);
	glDepthMask(GL_FALSE);  /* don't overwrite zbuf */

	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);
	unsigned col = add_attrib(format, "color", COMP_U8, 4, NORMALIZE_INT_TO_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);

	if (rv3d->rot_angle != 0.0f) {
		/* -- draw rotation axis -- */
		float scaled_axis[3];
		const float scale = rv3d->dist;
		mul_v3_v3fl(scaled_axis, rv3d->rot_axis, scale);


		immBegin(GL_LINE_STRIP, 3);
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

			immBegin(GL_LINE_LOOP, ROT_AXIS_DETAIL);
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

	/* -- draw rotation center -- */
	immBegin(GL_POINTS, 1);
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
	glDisable(GL_POINT_SMOOTH);
	glDepthMask(GL_TRUE);
}
#endif /* WITH_INPUT_NDOF */

/* ******************** non-meshes ***************** */

static void view3d_draw_non_mesh(
Scene *scene, Object *ob, Base *base, View3D *v3d,
RegionView3D *rv3d, const bool is_boundingbox, const unsigned char color[4])
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	/* multiply view with object matrix.
	* local viewmat and persmat, to calculate projections */
	ED_view3d_init_mats_rv3d_gl(ob, rv3d);

	switch (ob->type) {
		case OB_MESH:
		case OB_FONT:
		case OB_CURVE:
		case OB_SURF:
		case OB_MBALL:
			if (is_boundingbox) {
				draw_bounding_volume(ob, ob->boundtype);
			}
			break;
		case OB_EMPTY:
			drawaxes(rv3d->viewmatob, ob->empty_drawsize, ob->empty_drawtype, color);
			break;
		case OB_LAMP:
			drawlamp(v3d, rv3d, base, OB_SOLID, DRAW_CONSTCOLOR, color, ob == OBACT);
			break;
		case OB_CAMERA:
			drawcamera(scene, v3d, rv3d, base, DRAW_CONSTCOLOR, color);
			break;
		case OB_SPEAKER:
			drawspeaker(color);
			break;
		case OB_LATTICE:
			/* TODO */
			break;
		case OB_ARMATURE:
			/* TODO */
			break;
		default:
		/* TODO Viewport: handle the other cases*/
			break;
	}

	if (ob->rigidbody_object) {
		draw_rigidbody_shape(ob);
	}

	ED_view3d_clear_mats_rv3d(rv3d);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

/* ******************** info ***************** */

/**
* Render and camera border
*/
static void view3d_draw_border(const bContext *C, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	RegionView3D *rv3d = ar->regiondata;
	View3D *v3d = CTX_wm_view3d(C);

	if (rv3d->persp == RV3D_CAMOB) {
		drawviewborder(scene, ar, v3d);
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

/* ******************** view loop ***************** */

/**
 * Set the correct matrices
 */
static void view3d_draw_setup_view(const bContext *C, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ar->regiondata;

	/* setup the view matrix */
	if (view3d_stereo3d_active(C, scene, v3d, rv3d))
		view3d_stereo3d_setup(scene, v3d, ar);
	else
		view3d_main_region_setup_view(scene, v3d, ar, NULL, NULL);
}

static void draw_all_objects(const bContext *C, ARegion *ar, const bool only_depth, const bool use_depth)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);

	if (only_depth)
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	if (only_depth || use_depth) {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		v3d->zbuf = true;
	}

	for (Base *base = scene->base.first; base; base = base->next) {
		if (v3d->lay & base->lay) {
			/* dupli drawing */
			if (base->object->transflag & OB_DUPLI)
				draw_dupli_objects(scene, ar, v3d, base);

			draw_object(scene, ar, v3d, base, 0);
		}
	}

	if (only_depth)
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	if (only_depth || use_depth) {
		glDisable(GL_DEPTH_TEST);
		v3d->zbuf = false;
	}
}

/**
 * Draw only the scene depth buffer
 */
static void draw_depth_buffer(const bContext *C, ARegion *ar)
{
	draw_all_objects(C, ar, true, true);
}

/**
 * Required if the shaders need it or external engines
 * (e.g., Cycles requires depth buffer handled separately).
 */
static void view3d_draw_prerender_buffers(const bContext *C, ARegion *ar, DrawData *draw_data)
{
	View3D *v3d = CTX_wm_view3d(C);

	/* TODO viewport */
	if (draw_data->is_render && ((!draw_data->clip_border) || (v3d->drawtype <= OB_WIRE))) {
		draw_depth_buffer(C, ar);
	}
}

/**
 * Draw all the plates that will fill the RGBD buffer
 */
static void view3d_draw_solid_plates(const bContext *C, ARegion *ar, DrawData *draw_data)
{
	/* realtime plates */
	if ((!draw_data->is_render) || draw_data->clip_border) {
		view3d_draw_background(C);
		view3d_draw_render_solid_surfaces(C, ar, true);
		view3d_draw_render_transparent_surfaces(C);
		view3d_draw_post_draw(C);
	}

	/* offline plates*/
	if (draw_data->is_render) {
		Scene *scene = CTX_data_scene(C);
		View3D *v3d = CTX_wm_view3d(C);

		view3d_draw_render_draw(C, scene, ar, v3d, draw_data->clip_border, &draw_data->border_rect);
	}

#if VIEW3D_DRAW_DEBUG
	view3d_draw_debug_post_solid(C, ar, draw_data);
#endif
}

/**
 * Wires, outline, ...
 */
static void view3d_draw_geometry_overlay(const bContext *C)
{
	view3d_draw_wire_plates(C);
	view3d_draw_outline_plates(C);
}

/* drawing cameras, lamps, ... */
static void view3d_draw_non_meshes(const bContext *C, ARegion *ar)
{
	/* TODO viewport
	 * for now we draw them all, in the near future
	 * we filter them based on the plates/layers
	 */
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ar->regiondata;

	bool is_boundingbox = ((v3d->drawtype == OB_BOUNDBOX) ||
	                        ((v3d->drawtype == OB_RENDER) && (v3d->prev_drawtype == OB_BOUNDBOX)));

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	/* TODO Viewport
	 * we are already temporarily writing to zbuffer in draw_object()
	 * for now let's avoid writing again to zbuffer to prevent glitches
	 */

	for (Base *base = scene->base.first; base; base = base->next) {
		if (v3d->lay & base->lay) {
			Object *ob = base->object;

			unsigned char ob_wire_col[4];
			draw_object_wire_color(scene, base, ob_wire_col);
			view3d_draw_non_mesh(scene, ob, base, v3d, rv3d, is_boundingbox, ob_wire_col);
		}
	}

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
}

/**
* Parent lines, grid, ...
*/
static void view3d_draw_other_elements(const bContext *C, ARegion *ar)
{
	view3d_draw_grid(C, ar);

#ifdef WITH_INPUT_NDOF
	RegionView3D *rv3d = ar->regiondata;

	if ((U.ndof_flag & NDOF_SHOW_GUIDE) && ((rv3d->viewlock & RV3D_LOCKED) == 0) && (rv3d->persp != RV3D_CAMOB))
		/* TODO: draw something else (but not this) during fly mode */
		draw_rotation_guide(rv3d);
#endif
}

/**
 * Paint brushes, armatures, ...
 */
static void view3d_draw_tool_ui(const bContext *UNUSED(C))
{
	/* TODO viewport */
}

/**
 * Blueprint images
 */
static void view3d_draw_reference_images(const bContext *UNUSED(C))
{
	/* TODO viewport */
}

/**
* 3D manipulators
*/
static void view3d_draw_manipulator(const bContext *C)
{
	View3D *v3d = CTX_wm_view3d(C);
	v3d->zbuf = false;
	BIF_draw_manipulator(C);
}

/**
* Information drawn on top of the solid plates and composed data
*/
static void view3d_draw_region_info(const bContext *C, ARegion *ar)
{
	/* correct projection matrix */
	ED_region_pixelspace(ar);

	/* local coordinate visible rect inside region, to accomodate overlapping ui */
	rcti rect;
	ED_region_visible_rect(ar, &rect);

	view3d_draw_border(C, ar);
	view3d_draw_grease_pencil(C);

	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ar->regiondata;

	/* 3D cursor */
	if (is_cursor_visible(scene)) {
		drawcursor(scene, ar, v3d);
	}

	if (U.uiflag & USER_SHOW_ROTVIEWICON) {
		draw_view_axis(rv3d, &rect);
	}

	/* TODO viewport */
}

/**
 * This could run once per view, or even in parallel
 * for each of them. What is a "view"?
 * - a viewport with the camera elsewhere
 * - left/right stereo
 * - panorama / fisheye individual cubemap faces
 */
static void view3d_draw_view(const bContext *C, ARegion *ar, DrawData *draw_data)
{
	/* TODO - Technically this should be drawn to a few FBO, so we can handle
	 * compositing better, but for now this will get the ball rolling (dfelinto) */

	view3d_draw_setup_view(C, ar);
	view3d_draw_prerender_buffers(C, ar, draw_data);
	view3d_draw_solid_plates(C, ar, draw_data);
	view3d_draw_geometry_overlay(C);
	view3d_draw_non_meshes(C, ar);
	view3d_draw_other_elements(C, ar);
	view3d_draw_tool_ui(C);
	view3d_draw_reference_images(C);
	view3d_draw_manipulator(C);
	view3d_draw_region_info(C, ar);

#if VIEW3D_DRAW_DEBUG
	view3d_draw_debug(C, ar, draw_data);
#endif
}

void view3d_main_region_draw(const bContext *C, ARegion *ar)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ar->regiondata;

	if (IS_VIEWPORT_LEGACY(v3d)) {
		view3d_main_region_draw_legacy(C, ar);
		return;
	}

	if (!rv3d->viewport)
		rv3d->viewport = GPU_viewport_create();

	/* TODO viewport - there is so much to be done, in fact a lot will need to happen in the space_view3d.c
	 * before we even call the drawing routine, but let's move on for now (dfelinto)
	 * but this is a provisory way to start seeing things in the viewport */
	DrawData draw_data;
	view3d_draw_data_init(C, ar, rv3d, &draw_data);
	view3d_draw_view(C, ar, &draw_data);

	v3d->flag |= V3D_INVALID_BACKBUF;
}

/* ******************** legacy interface ***************** */
/**
 * This will be removed once the viewport gets replaced
 * meanwhile it should keep the old viewport working.
 */

void VP_legacy_drawcursor(Scene *scene, ARegion *ar, View3D *v3d)
{
	if (is_cursor_visible(scene)) {
		drawcursor(scene, ar, v3d);
	}
}

void VP_legacy_draw_view_axis(RegionView3D *rv3d, rcti *rect)
{
	draw_view_axis(rv3d, rect);
}

void VP_legacy_drawgrid(UnitSettings *unit, ARegion *ar, View3D *v3d, const char **grid_unit)
{
	drawgrid(unit, ar, v3d, grid_unit);
}

void VP_legacy_drawfloor(Scene *scene, View3D *v3d, const char **grid_unit, bool write_depth)
{
	drawfloor(scene, v3d, grid_unit, write_depth);
}

void VP_legacy_view3d_main_region_setup_view(Scene *scene, View3D *v3d, ARegion *ar, float viewmat[4][4], float winmat[4][4])
{
	view3d_main_region_setup_view(scene, v3d, ar, viewmat, winmat);
}

bool VP_legacy_view3d_stereo3d_active(const bContext *C, Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	return view3d_stereo3d_active(C, scene, v3d, rv3d);
}

void VP_legacy_view3d_stereo3d_setup(Scene *scene, View3D *v3d, ARegion *ar)
{
	view3d_stereo3d_setup(scene, v3d, ar);
}

bool VP_legacy_use_depth(Scene *scene, View3D *v3d)
{
	return use_depth_doit(scene, v3d);
}

void VP_drawviewborder(Scene *scene, ARegion *ar, View3D *v3d)
{
	drawviewborder(scene, ar, v3d);
}

void VP_drawrenderborder(ARegion *ar, View3D *v3d)
{
	drawrenderborder(ar, v3d);
}

void VP_view3d_draw_background_none(void)
{
	view3d_draw_background_none();
}

void VP_view3d_draw_background_world(Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	view3d_draw_background_world(scene, v3d, rv3d);
}
