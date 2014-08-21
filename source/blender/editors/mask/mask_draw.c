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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Campbell Barton,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mask/mask_draw.c
 *  \ingroup edmask
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_task.h"

#include "BKE_context.h"
#include "BKE_mask.h"

#include "DNA_mask_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"   /* SELECT */
#include "DNA_space_types.h"

#include "ED_clip.h"
#include "ED_mask.h"  /* own include */
#include "ED_space_api.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "mask_intern.h"  /* own include */

static void mask_spline_color_get(MaskLayer *masklay, MaskSpline *spline, const bool is_sel,
                                  unsigned char r_rgb[4])
{
	if (is_sel) {
		if (masklay->act_spline == spline) {
			r_rgb[0] = r_rgb[1] = r_rgb[2] = 255;
		}
		else {
			r_rgb[0] = 255;
			r_rgb[1] = r_rgb[2] = 0;
		}
	}
	else {
		r_rgb[0] = 128;
		r_rgb[1] = r_rgb[2] = 0;
	}

	r_rgb[3] = 255;
}

static void mask_spline_feather_color_get(MaskLayer *UNUSED(masklay), MaskSpline *UNUSED(spline), const bool is_sel,
                                          unsigned char r_rgb[4])
{
	if (is_sel) {
		r_rgb[1] = 255;
		r_rgb[0] = r_rgb[2] = 0;
	}
	else {
		r_rgb[1] = 128;
		r_rgb[0] = r_rgb[2] = 0;
	}

	r_rgb[3] = 255;
}

#if 0
static void draw_spline_parents(MaskLayer *UNUSED(masklay), MaskSpline *spline)
{
	int i;
	MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

	if (!spline->tot_point)
		return;

	glColor3ub(0, 0, 0);
	glEnable(GL_LINE_STIPPLE);
	glLineStipple(1, 0xAAAA);

	glBegin(GL_LINES);

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &points_array[i];
		BezTriple *bezt = &point->bezt;

		if (point->parent.id) {
			glVertex2f(bezt->vec[1][0],
			           bezt->vec[1][1]);

			glVertex2f(bezt->vec[1][0] - point->parent.offset[0],
			           bezt->vec[1][1] - point->parent.offset[1]);
		}
	}

	glEnd();

	glDisable(GL_LINE_STIPPLE);
}
#endif

static void mask_point_undistort_pos(SpaceClip *sc, float r_co[2], const float co[2])
{
	BKE_mask_coord_to_movieclip(sc->clip, &sc->user, r_co, co);
	ED_clip_point_undistorted_pos(sc, r_co, r_co);
	BKE_mask_coord_from_movieclip(sc->clip, &sc->user, r_co, r_co);
}

static void draw_circle(const float x, const float y,
                        const float size, const bool fill,
                        const float xscale, const float yscale)
{
	static GLuint wire_displist = 0;
	static GLuint fill_displist = 0;
	GLuint displist = fill ? fill_displist : wire_displist;

	/* Initialize round circle shape. */
	if (displist == 0) {
		GLUquadricObj *qobj;

		displist = glGenLists(1);
		glNewList(displist, GL_COMPILE);

		qobj = gluNewQuadric();
		gluQuadricDrawStyle(qobj, fill ? GLU_FILL : GLU_SILHOUETTE);
		gluDisk(qobj, 0,  0.7, 8, 1);
		gluDeleteQuadric(qobj);

		glEndList();

		if (fill) {
			fill_displist = displist;
		}
		else {
			wire_displist = displist;
		}
	}

	glPushMatrix();
	glTranslatef(x, y, 0.0f);
	glScalef(1.0f / xscale * size, 1.0f / yscale * size, 1.0f);
	glCallList(displist);
	glPopMatrix();
}

static void draw_single_handle(const MaskLayer *mask_layer, const MaskSplinePoint *point,
                               const eMaskWhichHandle which_handle, const int draw_type,
                               const float handle_size, const float xscale, const float yscale,
                               const float point_pos[2], const float handle_pos[2])
{
	const BezTriple *bezt = &point->bezt;
	char handle_type;

	if (which_handle == MASK_WHICH_HANDLE_STICK || which_handle == MASK_WHICH_HANDLE_LEFT) {
		handle_type = bezt->h1;
	}
	else {
		handle_type = bezt->h2;
	}

	if (handle_type == HD_VECT) {
		return;
	}

	/* this could be split into its own loop */
	if (draw_type == MASK_DT_OUTLINE) {
		const unsigned char rgb_gray[4] = {0x60, 0x60, 0x60, 0xff};
		glLineWidth(3);
		glColor4ubv(rgb_gray);
		glBegin(GL_LINES);
		glVertex2fv(point_pos);
		glVertex2fv(handle_pos);
		glEnd();
		glLineWidth(1);
	}

	switch (handle_type) {
		case HD_FREE:
			UI_ThemeColor(TH_HANDLE_FREE);
			break;
		case HD_AUTO:
			UI_ThemeColor(TH_HANDLE_AUTO);
			break;
		case HD_ALIGN:
		case HD_ALIGN_DOUBLESIDE:
			UI_ThemeColor(TH_HANDLE_ALIGN);
			break;
	}

	glBegin(GL_LINES);
	glVertex2fv(point_pos);
	glVertex2fv(handle_pos);
	glEnd();

	/* draw handle points */
	if (MASKPOINT_ISSEL_HANDLE(point, which_handle)) {
		if (point == mask_layer->act_point)
			glColor3f(1.0f, 1.0f, 1.0f);
		else
			UI_ThemeColor(TH_HANDLE_VERTEX_SELECT);
	}
	else {
		UI_ThemeColor(TH_HANDLE_VERTEX);
	}

	draw_circle(handle_pos[0], handle_pos[1], handle_size, false, xscale, yscale);
}

/* return non-zero if spline is selected */
static void draw_spline_points(const bContext *C, MaskLayer *masklay, MaskSpline *spline,
                               const char draw_flag, const char draw_type,
                               const float xscale, const float yscale)
{
	const bool is_spline_sel = (spline->flag & SELECT) && (masklay->restrictflag & MASK_RESTRICT_SELECT) == 0;
	const bool is_smooth = (draw_flag & MASK_DRAWFLAG_SMOOTH) != 0;

	unsigned char rgb_spline[4];
	MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);
	SpaceClip *sc = CTX_wm_space_clip(C);
	bool undistort = false;

	int i, handle_size, tot_feather_point;
	float (*feather_points)[2], (*fp)[2];
	float min[2], max[2];

	if (!spline->tot_point)
		return;

	if (sc)
		undistort = sc->clip && (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT) != 0;

	/* TODO, add this to sequence editor */
	handle_size = UI_GetThemeValuef(TH_HANDLE_VERTEX_SIZE) * U.pixelsize;

	glPointSize(handle_size);

	mask_spline_color_get(masklay, spline, is_spline_sel, rgb_spline);

	/* feather points */
	feather_points = fp = BKE_mask_spline_feather_points(spline, &tot_feather_point);
	for (i = 0; i < spline->tot_point; i++) {

		/* watch it! this is intentionally not the deform array, only check for sel */
		MaskSplinePoint *point = &spline->points[i];

		int j;

		for (j = 0; j <= point->tot_uw; j++) {
			float feather_point[2];
			bool sel = false;

			copy_v2_v2(feather_point, *fp);

			if (undistort)
				mask_point_undistort_pos(sc, feather_point, feather_point);

			if (j == 0) {
				sel = MASKPOINT_ISSEL_ANY(point);
			}
			else {
				sel = (point->uw[j - 1].flag & SELECT) != 0;
			}

			if (sel) {
				if (point == masklay->act_point)
					glColor3f(1.0f, 1.0f, 1.0f);
				else
					UI_ThemeColor(TH_HANDLE_VERTEX_SELECT);
			}
			else {
				UI_ThemeColor(TH_HANDLE_VERTEX);
			}

			glBegin(GL_POINTS);
			glVertex2fv(feather_point);
			glEnd();

			fp++;
		}
	}
	MEM_freeN(feather_points);

	if (is_smooth) {
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	/* control points */
	INIT_MINMAX2(min, max);
	for (i = 0; i < spline->tot_point; i++) {

		/* watch it! this is intentionally not the deform array, only check for sel */
		MaskSplinePoint *point = &spline->points[i];
		MaskSplinePoint *point_deform = &points_array[i];
		BezTriple *bezt = &point_deform->bezt;

		float vert[2];

		copy_v2_v2(vert, bezt->vec[1]);

		if (undistort) {
			mask_point_undistort_pos(sc, vert, vert);
		}

		/* draw handle segment */
		if (BKE_mask_point_handles_mode_get(point) == MASK_HANDLE_MODE_STICK) {
			float handle[2];
			BKE_mask_point_handle(point_deform, MASK_WHICH_HANDLE_STICK, handle);
			if (undistort) {
				mask_point_undistort_pos(sc, handle, handle);
			}
			draw_single_handle(masklay, point, MASK_WHICH_HANDLE_STICK,
			                   draw_type, handle_size, xscale, yscale, vert, handle);
		}
		else {
			float handle_left[2], handle_right[2];
			BKE_mask_point_handle(point_deform, MASK_WHICH_HANDLE_LEFT, handle_left);
			BKE_mask_point_handle(point_deform, MASK_WHICH_HANDLE_RIGHT, handle_right);
			if (undistort) {
				mask_point_undistort_pos(sc, handle_left, handle_left);
				mask_point_undistort_pos(sc, handle_left, handle_left);
			}
			draw_single_handle(masklay, point, MASK_WHICH_HANDLE_LEFT,
			                   draw_type, handle_size, xscale, yscale, vert, handle_left);
			draw_single_handle(masklay, point, MASK_WHICH_HANDLE_RIGHT,
			                   draw_type, handle_size, xscale, yscale, vert, handle_right);
		}

		/* draw CV point */
		if (MASKPOINT_ISSEL_KNOT(point)) {
			if (point == masklay->act_point)
				glColor3f(1.0f, 1.0f, 1.0f);
			else
				UI_ThemeColor(TH_HANDLE_VERTEX_SELECT);
		}
		else
			UI_ThemeColor(TH_HANDLE_VERTEX);

		glBegin(GL_POINTS);
		glVertex2fv(vert);
		glEnd();

		minmax_v2v2_v2(min, max, vert);
	}

	if (is_spline_sel) {
		float x = (min[0] + max[0]) / 2.0f;
		float y = (min[1] + max[1]) / 2.0f;
		/* TODO(sergey): Remove hardcoded colors. */
		if (masklay->act_spline == spline) {
			glColor3ub(255, 255, 255);
		}
		else {
			glColor3ub(255, 255, 0);
		}

		draw_circle(x, y, 6.0f, true, xscale, yscale);

		glColor3ub(0, 0, 0);
		draw_circle(x, y, 6.0f, false, xscale, yscale);
	}

	glPointSize(1.0f);

	if (is_smooth) {
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}
}

/* #define USE_XOR */

static void mask_color_active_tint(unsigned char r_rgb[4], const unsigned char rgb[4], const bool is_active)
{
	if (!is_active) {
		r_rgb[0] = (unsigned char)((((int)(rgb[0])) + 128) / 2);
		r_rgb[1] = (unsigned char)((((int)(rgb[1])) + 128) / 2);
		r_rgb[2] = (unsigned char)((((int)(rgb[2])) + 128) / 2);
		r_rgb[3] = rgb[3];
	}
	else {
		*(unsigned int *)r_rgb = *(const unsigned int *)rgb;
	}
}

static void mask_draw_curve_type(const bContext *C, MaskSpline *spline, float (*orig_points)[2], int tot_point,
                                 const bool is_feather, const bool is_smooth, const bool is_active,
                                 const unsigned char rgb_spline[4], const char draw_type)
{
	const int draw_method = (spline->flag & MASK_SPLINE_CYCLIC) ? GL_LINE_LOOP : GL_LINE_STRIP;
	const unsigned char rgb_black[4] = {0x00, 0x00, 0x00, 0xff};
//	const unsigned char rgb_white[4] = {0xff, 0xff, 0xff, 0xff};
	unsigned char rgb_tmp[4];
	SpaceClip *sc = CTX_wm_space_clip(C);
	float (*points)[2] = orig_points;

	if (sc) {
		int undistort = sc->clip && sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT;

		if (undistort) {
			int i;

			points = MEM_callocN(2 * tot_point * sizeof(float), "undistorthed mask curve");

			for (i = 0; i < tot_point; i++) {
				mask_point_undistort_pos(sc, points[i], orig_points[i]);
			}
		}
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, points);

	switch (draw_type) {

		case MASK_DT_OUTLINE:
			glLineWidth(3);

			mask_color_active_tint(rgb_tmp, rgb_black, is_active);
			glColor4ubv(rgb_tmp);

			glDrawArrays(draw_method, 0, tot_point);

			glLineWidth(1);
			mask_color_active_tint(rgb_tmp, rgb_spline, is_active);
			glColor4ubv(rgb_tmp);
			glDrawArrays(draw_method, 0, tot_point);

			break;

		case MASK_DT_DASH:
		default:
			glEnable(GL_LINE_STIPPLE);

#ifdef USE_XOR
			glEnable(GL_COLOR_LOGIC_OP);
			glLogicOp(GL_OR);
#endif
			mask_color_active_tint(rgb_tmp, rgb_spline, is_active);
			glColor4ubv(rgb_tmp);
			glLineStipple(3, 0xaaaa);
			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(2, GL_FLOAT, 0, points);
			glDrawArrays(draw_method, 0, tot_point);

#ifdef USE_XOR
			glDisable(GL_COLOR_LOGIC_OP);
#endif
			mask_color_active_tint(rgb_tmp, rgb_black, is_active);
			glColor4ubv(rgb_tmp);
			glLineStipple(3, 0x5555);
			glDrawArrays(draw_method, 0, tot_point);

			glDisable(GL_LINE_STIPPLE);
			break;


		case MASK_DT_BLACK:
		case MASK_DT_WHITE:
			if (draw_type == MASK_DT_BLACK) { rgb_tmp[0] = rgb_tmp[1] = rgb_tmp[2] = 0;   }
			else                            { rgb_tmp[0] = rgb_tmp[1] = rgb_tmp[2] = 255; }
			/* alpha values seem too low but gl draws many points that compensate for it */
			if (is_feather) { rgb_tmp[3] = 64; }
			else            { rgb_tmp[3] = 128; }

			if (is_feather) {
				rgb_tmp[0] = (unsigned char)(((short)rgb_tmp[0] + (short)rgb_spline[0]) / 2);
				rgb_tmp[1] = (unsigned char)(((short)rgb_tmp[1] + (short)rgb_spline[1]) / 2);
				rgb_tmp[2] = (unsigned char)(((short)rgb_tmp[2] + (short)rgb_spline[2]) / 2);
			}

			if (is_smooth == false && is_feather) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			mask_color_active_tint(rgb_tmp, rgb_tmp, is_active);
			glColor4ubv(rgb_tmp);

			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(2, GL_FLOAT, 0, points);
			glDrawArrays(draw_method, 0, tot_point);

			if (is_smooth == false && is_feather) {
				glDisable(GL_BLEND);
			}

			break;
	}

	glDisableClientState(GL_VERTEX_ARRAY);

	if (points != orig_points)
		MEM_freeN(points);
}

static void draw_spline_curve(const bContext *C, MaskLayer *masklay, MaskSpline *spline,
                              const char draw_flag, const char draw_type,
                              const bool is_active,
                              const int width, const int height)
{
	const unsigned int resol = max_ii(BKE_mask_spline_feather_resolution(spline, width, height),
	                                  BKE_mask_spline_resolution(spline, width, height));

	unsigned char rgb_tmp[4];

	const bool is_spline_sel = (spline->flag & SELECT) && (masklay->restrictflag & MASK_RESTRICT_SELECT) == 0;
	const bool is_smooth = (draw_flag & MASK_DRAWFLAG_SMOOTH) != 0;
	const bool is_fill = (spline->flag & MASK_SPLINE_NOFILL) == 0;

	unsigned int tot_diff_point;
	float (*diff_points)[2];

	unsigned int tot_feather_point;
	float (*feather_points)[2];

	diff_points = BKE_mask_spline_differentiate_with_resolution(spline, &tot_diff_point, resol);

	if (!diff_points)
		return;

	if (is_smooth) {
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	feather_points = BKE_mask_spline_feather_differentiated_points_with_resolution(spline, &tot_feather_point, resol, (is_fill != false));

	/* draw feather */
	mask_spline_feather_color_get(masklay, spline, is_spline_sel, rgb_tmp);
	mask_draw_curve_type(C, spline, feather_points, tot_feather_point,
	                     true, is_smooth, is_active,
	                     rgb_tmp, draw_type);

	if (!is_fill) {

		const float *fp   = &diff_points[0][0];
		float *fp_feather = &feather_points[0][0];
		float tvec[2];
		int i;

		BLI_assert(tot_diff_point == tot_feather_point);

		for (i = 0; i < tot_diff_point; i++, fp += 2, fp_feather += 2) {
			sub_v2_v2v2(tvec, fp, fp_feather);
			add_v2_v2v2(fp_feather, fp, tvec);
		}

		/* same as above */
		mask_draw_curve_type(C, spline, feather_points, tot_feather_point,
		                     true, is_smooth, is_active,
		                     rgb_tmp, draw_type);
	}

	MEM_freeN(feather_points);

	/* draw main curve */
	mask_spline_color_get(masklay, spline, is_spline_sel, rgb_tmp);
	mask_draw_curve_type(C, spline, diff_points, tot_diff_point,
	                     false, is_smooth, is_active,
	                     rgb_tmp, draw_type);
	MEM_freeN(diff_points);

	if (draw_flag & MASK_DRAWFLAG_SMOOTH) {
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}

	(void)draw_type;
}

static void draw_masklays(const bContext *C, Mask *mask, const char draw_flag, const char draw_type,
                          const int width, const int height, const float xscale, const float yscale)
{
	MaskLayer *masklay;
	int i;

	for (masklay = mask->masklayers.first, i = 0; masklay; masklay = masklay->next, i++) {
		MaskSpline *spline;
		const bool is_active = (i == mask->masklay_act);

		if (masklay->restrictflag & MASK_RESTRICT_VIEW) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {

			/* draw curve itself first... */
			draw_spline_curve(C, masklay, spline, draw_flag, draw_type, is_active, width, height);

//			draw_spline_parents(masklay, spline);

			if (!(masklay->restrictflag & MASK_RESTRICT_SELECT)) {
				/* ...and then handles over the curve so they're nicely visible */
				draw_spline_points(C, masklay, spline, draw_flag, draw_type, xscale, yscale);
			}

			/* show undeform for testing */
			if (0) {
				void *back = spline->points_deform;

				spline->points_deform = NULL;
				draw_spline_curve(C, masklay, spline, draw_flag, draw_type, is_active, width, height);
//				draw_spline_parents(masklay, spline);
				draw_spline_points(C, masklay, spline, draw_flag, draw_type, xscale, yscale);
				spline->points_deform = back;
			}
		}
	}
}

void ED_mask_draw(const bContext *C,
                  const char draw_flag, const char draw_type)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);

	Mask *mask = CTX_data_edit_mask(C);
	int width, height;
	float aspx, aspy;
	float xscale, yscale;

	if (!mask)
		return;

	ED_mask_get_size(sa, &width, &height);
	ED_mask_get_aspect(sa, ar, &aspx, &aspy);
	UI_view2d_scale_get(&ar->v2d, &xscale, &yscale);

	draw_masklays(C, mask, draw_flag, draw_type, width, height, xscale * aspx, yscale * aspy);
}

typedef struct ThreadedMaskRasterizeState {
	MaskRasterHandle *handle;
	float *buffer;
	int width, height;
} ThreadedMaskRasterizeState;

typedef struct ThreadedMaskRasterizeData {
	int start_scanline;
	int num_scanlines;
} ThreadedMaskRasterizeData;

static void mask_rasterize_func(TaskPool *pool, void *taskdata, int UNUSED(threadid))
{
	ThreadedMaskRasterizeState *state = (ThreadedMaskRasterizeState *) BLI_task_pool_userdata(pool);
	ThreadedMaskRasterizeData *data = (ThreadedMaskRasterizeData *) taskdata;
	int scanline;
	const float x_inv = 1.0f / (float)state->width;
	const float y_inv = 1.0f / (float)state->height;
	const float x_px_ofs = x_inv * 0.5f;
	const float y_px_ofs = y_inv * 0.5f;

	for (scanline = 0; scanline < data->num_scanlines; scanline++) {
		float xy[2];
		int x, y = data->start_scanline + scanline;

		xy[1] = ((float)y * y_inv) + y_px_ofs;

		for (x = 0; x < state->width; x++) {
			int index = y * state->width + x;

			xy[0] = ((float)x * x_inv) + x_px_ofs;

			state->buffer[index] = BKE_maskrasterize_handle_sample(state->handle, xy);
		}
	}
}

static float *threaded_mask_rasterize(Mask *mask, const int width, const int height)
{
	TaskScheduler *task_scheduler = BLI_task_scheduler_get();
	TaskPool *task_pool;
	MaskRasterHandle *handle;
	ThreadedMaskRasterizeState state;
	float *buffer;
	int i, num_threads = BLI_task_scheduler_num_threads(task_scheduler), scanlines_per_thread;

	buffer = MEM_mallocN(sizeof(float) * height * width, "rasterized mask buffer");

	/* Initialize rasterization handle. */
	handle = BKE_maskrasterize_handle_new();
	BKE_maskrasterize_handle_init(handle, mask, width, height, true, true, true);

	state.handle = handle;
	state.buffer = buffer;
	state.width = width;
	state.height = height;

	task_pool = BLI_task_pool_create(task_scheduler, &state);

	scanlines_per_thread = height / num_threads;
	for (i = 0; i < num_threads; i++) {
		ThreadedMaskRasterizeData *data = MEM_mallocN(sizeof(ThreadedMaskRasterizeData),
		                                                "threaded mask rasterize data");

		data->start_scanline = i * scanlines_per_thread;

		if (i < num_threads - 1) {
			data->num_scanlines = scanlines_per_thread;
		}
		else {
			data->num_scanlines = height - data->start_scanline;
		}

		BLI_task_pool_push(task_pool, mask_rasterize_func, data, true, TASK_PRIORITY_LOW);
	}

	/* work and wait until tasks are done */
	BLI_task_pool_work_and_wait(task_pool);

	/* Free memory. */
	BLI_task_pool_free(task_pool);
	BKE_maskrasterize_handle_free(handle);

	return buffer;
}

/* sets up the opengl context.
 * width, height are to match the values from ED_mask_get_size() */
void ED_mask_draw_region(Mask *mask, ARegion *ar,
                         const char draw_flag, const char draw_type, const char overlay_mode,
                         const int width_i, const int height_i,  /* convert directly into aspect corrected vars */
                         const float aspx, const float aspy,
                         const bool do_scale_applied, const bool do_draw_cb,
                         float stabmat[4][4], /* optional - only used by clip */
                         const bContext *C    /* optional - only used when do_post_draw is set or called from clip editor */
                         )
{
	struct View2D *v2d = &ar->v2d;

	/* aspect always scales vertically in movie and image spaces */
	const float width = width_i, height = (float)height_i * (aspy / aspx);

	int x, y;
	/* int w, h; */
	float zoomx, zoomy;

	/* frame image */
	float maxdim;
	float xofs, yofs;

	/* find window pixel coordinates of origin */
	UI_view2d_view_to_region(&ar->v2d, 0.0f, 0.0f, &x, &y);


	/* w = BLI_rctf_size_x(&v2d->tot); */
	/* h = BLI_rctf_size_y(&v2d->tot); */


	zoomx = (float)(BLI_rcti_size_x(&ar->winrct) + 1) / BLI_rctf_size_x(&ar->v2d.cur);
	zoomy = (float)(BLI_rcti_size_y(&ar->winrct) + 1) / BLI_rctf_size_y(&ar->v2d.cur);

	if (do_scale_applied) {
		zoomx /= width;
		zoomy /= height;
	}

	x += v2d->tot.xmin * zoomx;
	y += v2d->tot.ymin * zoomy;

	/* frame the image */
	maxdim = max_ff(width, height);
	if (width == height) {
		xofs = yofs = 0;
	}
	else if (width < height) {
		xofs = ((height - width) / -2.0f) * zoomx;
		yofs = 0.0f;
	}
	else { /* (width > height) */
		xofs = 0.0f;
		yofs = ((width - height) / -2.0f) * zoomy;
	}

	if (draw_flag & MASK_DRAWFLAG_OVERLAY) {
		float *buffer = threaded_mask_rasterize(mask, width, height);
		int format;

		if (overlay_mode == MASK_OVERLAY_ALPHACHANNEL) {
			glColor3f(1.0f, 1.0f, 1.0f);
			format = GL_LUMINANCE;
		}
		else {
			/* More blending types could be supported in the future. */
			glEnable(GL_BLEND);
			glBlendFunc(GL_DST_COLOR, GL_SRC_ALPHA);
			format = GL_ALPHA;
		}

		glPushMatrix();
		glTranslatef(x, y, 0);
		glScalef(zoomx, zoomy, 0);
		if (stabmat) {
			glMultMatrixf(stabmat);
		}
		glaDrawPixelsTex(0.0f, 0.0f, width, height, format, GL_FLOAT, GL_NEAREST, buffer);
		glPopMatrix();

		if (overlay_mode != MASK_OVERLAY_ALPHACHANNEL) {
			glDisable(GL_BLEND);
		}

		MEM_freeN(buffer);
	}

	/* apply transformation so mask editing tools will assume drawing from the origin in normalized space */
	glPushMatrix();

	if (stabmat) {
		glMultMatrixf(stabmat);
	}

	glTranslatef(x + xofs, y + yofs, 0);
	glScalef(maxdim * zoomx, maxdim * zoomy, 0);

	if (do_draw_cb) {
		ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);
	}

	/* draw! */
	draw_masklays(C, mask, draw_flag, draw_type, width, height, maxdim * zoomx, maxdim * zoomy);

	if (do_draw_cb) {
		ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);
	}

	glPopMatrix();
}

void ED_mask_draw_frames(Mask *mask, ARegion *ar, const int cfra, const int sfra, const int efra)
{
	const float framelen = ar->winx / (float)(efra - sfra + 1);

	MaskLayer *masklay = BKE_mask_layer_active(mask);

	glBegin(GL_LINES);
	glColor4ub(255, 175, 0, 255);

	if (masklay) {
		MaskLayerShape *masklay_shape;

		for (masklay_shape = masklay->splines_shapes.first;
		     masklay_shape;
		     masklay_shape = masklay_shape->next)
		{
			int frame = masklay_shape->frame;

			/* draw_keyframe(i, CFRA, sfra, framelen, 1); */
			int height = (frame == cfra) ? 22 : 10;
			int x = (frame - sfra) * framelen;
			glVertex2i(x, 0);
			glVertex2i(x, height);
		}
	}

	glEnd();
}
