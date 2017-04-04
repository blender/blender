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

/** \file blender/windowmanager/intern/wm_gesture.c
 *  \ingroup wm
 *
 * Gestures (cursor motions) creating, evaluating and drawing, shared between operators.
 */

#include "DNA_screen_types.h"
#include "DNA_vec_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap_draw_2d.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_lasso.h"

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_subwindow.h"
#include "wm_draw.h"

#include "GPU_immediate.h"

#include "BIF_glutil.h"


/* context checked on having screen, window and area */
wmGesture *WM_gesture_new(bContext *C, const wmEvent *event, int type)
{
	wmGesture *gesture = MEM_callocN(sizeof(wmGesture), "new gesture");
	wmWindow *window = CTX_wm_window(C);
	ARegion *ar = CTX_wm_region(C);
	int sx, sy;
	
	BLI_addtail(&window->gesture, gesture);
	
	gesture->type = type;
	gesture->event_type = event->type;
	gesture->swinid = ar->swinid;    /* means only in area-region context! */
	
	wm_subwindow_origin_get(window, gesture->swinid, &sx, &sy);
	
	if (ELEM(type, WM_GESTURE_RECT, WM_GESTURE_CROSS_RECT, WM_GESTURE_TWEAK,
	          WM_GESTURE_CIRCLE, WM_GESTURE_STRAIGHTLINE))
	{
		rcti *rect = MEM_callocN(sizeof(rcti), "gesture rect new");
		
		gesture->customdata = rect;
		rect->xmin = event->x - sx;
		rect->ymin = event->y - sy;
		if (type == WM_GESTURE_CIRCLE) {
#ifdef GESTURE_MEMORY
			rect->xmax = circle_select_size;
#else
			rect->xmax = 25;    // XXX temp
#endif
		}
		else {
			rect->xmax = event->x - sx;
			rect->ymax = event->y - sy;
		}
	}
	else if (ELEM(type, WM_GESTURE_LINES, WM_GESTURE_LASSO)) {
		short *lasso;
		gesture->customdata = lasso = MEM_callocN(2 * sizeof(short) * WM_LASSO_MIN_POINTS, "lasso points");
		lasso[0] = event->x - sx;
		lasso[1] = event->y - sy;
		gesture->points = 1;
		gesture->size = WM_LASSO_MIN_POINTS;
	}
	
	return gesture;
}

void WM_gesture_end(bContext *C, wmGesture *gesture)
{
	wmWindow *win = CTX_wm_window(C);
	
	if (win->tweak == gesture)
		win->tweak = NULL;
	BLI_remlink(&win->gesture, gesture);
	MEM_freeN(gesture->customdata);
	if (gesture->userdata) {
		MEM_freeN(gesture->userdata);
	}
	MEM_freeN(gesture);
}

void WM_gestures_remove(bContext *C)
{
	wmWindow *win = CTX_wm_window(C);
	
	while (win->gesture.first)
		WM_gesture_end(C, win->gesture.first);
}


/* tweak and line gestures */
int wm_gesture_evaluate(wmGesture *gesture)
{
	if (gesture->type == WM_GESTURE_TWEAK) {
		rcti *rect = gesture->customdata;
		int dx = BLI_rcti_size_x(rect);
		int dy = BLI_rcti_size_y(rect);
		if (abs(dx) + abs(dy) > U.tweak_threshold) {
			int theta = iroundf(4.0f * atan2f((float)dy, (float)dx) / (float)M_PI);
			int val = EVT_GESTURE_W;

			if (theta == 0) val = EVT_GESTURE_E;
			else if (theta == 1) val = EVT_GESTURE_NE;
			else if (theta == 2) val = EVT_GESTURE_N;
			else if (theta == 3) val = EVT_GESTURE_NW;
			else if (theta == -1) val = EVT_GESTURE_SE;
			else if (theta == -2) val = EVT_GESTURE_S;
			else if (theta == -3) val = EVT_GESTURE_SW;
			
#if 0
			/* debug */
			if (val == 1) printf("tweak north\n");
			if (val == 2) printf("tweak north-east\n");
			if (val == 3) printf("tweak east\n");
			if (val == 4) printf("tweak south-east\n");
			if (val == 5) printf("tweak south\n");
			if (val == 6) printf("tweak south-west\n");
			if (val == 7) printf("tweak west\n");
			if (val == 8) printf("tweak north-west\n");
#endif
			return val;
		}
	}
	return 0;
}


/* ******************* gesture draw ******************* */

static void wm_gesture_draw_line(wmGesture *gt)
{
	rcti *rect = (rcti *)gt->customdata;
	
	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
	unsigned line_origin = add_attrib(format, "line_origin", COMP_F32, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_COLOR);

	immUniform4f("color1", 0.4f, 0.4f, 0.4f, 1.0f);
	immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
	immUniform1f("dash_width", 8.0f);
	immUniform1f("dash_width_on", 4.0f);

	float xmin = (float)rect->xmin;
	float ymin = (float)rect->ymin;

	immBegin(PRIM_LINES, 2);

	immAttrib2f(line_origin, xmin, ymin);
	immVertex2f(pos, xmin, ymin);
	immAttrib2f(line_origin, xmin, ymin);
	immVertex2f(pos, (float)rect->xmax, (float)rect->ymax);

	immEnd();

	immUnbindProgram();
}

static void imm_draw_line_box_dashed(unsigned pos, unsigned line_origin, float x1, float y1, float x2, float y2)
{
	immBegin(PRIM_LINES, 8);
	immAttrib2f(line_origin, x1, y1);
	immVertex2f(pos, x1, y1);
	immVertex2f(pos, x1, y2);
	immAttrib2f(line_origin, x1, y2);
	immVertex2f(pos, x1, y2);
	immVertex2f(pos, x2, y2);
	immAttrib2f(line_origin, x2, y1);
	immVertex2f(pos, x2, y2);
	immVertex2f(pos, x2, y1);
	immAttrib2f(line_origin, x1, y1);
	immVertex2f(pos, x2, y1);
	immVertex2f(pos, x1, y1);
	immEnd();
}

static void wm_gesture_draw_rect(wmGesture *gt)
{
	rcti *rect = (rcti *)gt->customdata;

	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	
	glEnable(GL_BLEND);

	immUniform4f("color", 1.0f, 1.0f, 1.0f, 0.05f);

	immBegin(GL_QUADS, 4);

	immVertex2f(pos, (float)rect->xmax, (float)rect->ymin);
	immVertex2f(pos, (float)rect->xmax, (float)rect->ymax);
	immVertex2f(pos, (float)rect->xmin, (float)rect->ymax);
	immVertex2f(pos, (float)rect->xmin, (float)rect->ymin);

	immEnd();

	immUnbindProgram();

	glDisable(GL_BLEND);

	format = immVertexFormat();
	pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
	unsigned line_origin = add_attrib(format, "line_origin", COMP_F32, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_COLOR);

	immUniform4f("color1", 0.4f, 0.4f, 0.4f, 1.0f);
	immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
	immUniform1f("dash_width", 8.0f);
	immUniform1f("dash_width_on", 4.0f);
	
	imm_draw_line_box_dashed(pos, line_origin,
		(float)rect->xmin, (float)rect->ymin, (float)rect->xmax, (float)rect->ymax);

	immUnbindProgram();

	// wm_gesture_draw_line(gt); // draws a diagonal line in the lined box to test wm_gesture_draw_line
}

static void imm_draw_lined_dashed_circle(unsigned pos, unsigned line_origin, float x, float y, float rad, int nsegments)
{
	float xpos, ypos;

	xpos = x + rad;
	ypos = y;

	immBegin(PRIM_LINES, nsegments * 2);

	for (int i = 1; i <= nsegments; ++i) {
		float angle = 2 * M_PI * ((float)i / (float)nsegments);

		immAttrib2f(line_origin, xpos, ypos);
		immVertex2f(pos, xpos, ypos);

		xpos = x + rad * cosf(angle);
		ypos = y + rad * sinf(angle);

		immVertex2f(pos, xpos, ypos);
	}

	immEnd();
}

static void wm_gesture_draw_circle(wmGesture *gt)
{
	rcti *rect = (rcti *)gt->customdata;

	glEnable(GL_BLEND);

	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	immUniformColor4f(1.0, 1.0, 1.0, 0.05);
	imm_draw_filled_circle(pos, (float)rect->xmin, (float)rect->ymin, (float)rect->xmax, 40);

	immUnbindProgram();

	glDisable(GL_BLEND);
	
	format = immVertexFormat();
	pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
	unsigned line_origin = add_attrib(format, "line_origin", COMP_F32, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_COLOR);

	immUniform4f("color1", 0.4f, 0.4f, 0.4f, 1.0f);
	immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
	immUniform1f("dash_width", 4.0f);
	immUniform1f("dash_width_on", 2.0f);
	
	imm_draw_lined_dashed_circle(pos, line_origin,
		(float)rect->xmin, (float)rect->ymin, (float)rect->xmax, 40);

	immUnbindProgram();
}

struct LassoFillData {
	unsigned char *px;
	int width;
};

static void draw_filled_lasso_px_cb(int x, int x_end, int y, void *user_data)
{
	struct LassoFillData *data = user_data;
	unsigned char *col = &(data->px[(y * data->width) + x]);
	memset(col, 0x10, x_end - x);
}

static void draw_filled_lasso(wmWindow *win, wmGesture *gt)
{
	const short *lasso = (short *)gt->customdata;
	const int tot = gt->points;
	int (*moves)[2] = MEM_mallocN(sizeof(*moves) * (tot + 1), __func__);
	int i;
	rcti rect;
	rcti rect_win;
	float red[4] = {1.0f, 0.0f, 0.0f, 0.0f};

	for (i = 0; i < tot; i++, lasso += 2) {
		moves[i][0] = lasso[0];
		moves[i][1] = lasso[1];
	}

	BLI_lasso_boundbox(&rect, (const int (*)[2])moves, tot);

	wm_subwindow_rect_get(win, gt->swinid, &rect_win);
	BLI_rcti_translate(&rect, rect_win.xmin, rect_win.ymin);
	BLI_rcti_isect(&rect_win, &rect, &rect);
	BLI_rcti_translate(&rect, -rect_win.xmin, -rect_win.ymin);

	/* highly unlikely this will fail, but could crash if (tot == 0) */
	if (BLI_rcti_is_empty(&rect) == false) {
		const int w = BLI_rcti_size_x(&rect);
		const int h = BLI_rcti_size_y(&rect);
		unsigned char *pixel_buf = MEM_callocN(sizeof(*pixel_buf) * w * h, __func__);
		struct LassoFillData lasso_fill_data = {pixel_buf, w};

		BLI_bitmap_draw_2d_poly_v2i_n(
		       rect.xmin, rect.ymin, rect.xmax, rect.ymax,
		       (const int (*)[2])moves, tot,
		       draw_filled_lasso_px_cb, &lasso_fill_data);

		/* Additive Blending */
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);

		GLint unpack_alignment;
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		GPUShader *shader = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR);
		GPU_shader_bind(shader);
		GPU_shader_uniform_vector(shader, GPU_shader_get_uniform(shader, "shuffle"), 4, 1, red);

		immDrawPixelsTex(rect.xmin, rect.ymin, w, h, GL_RED, GL_UNSIGNED_BYTE, GL_NEAREST, pixel_buf, 1.0f, 1.0f, NULL);

		GPU_shader_unbind();

		glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);

		MEM_freeN(pixel_buf);

		glDisable(GL_BLEND);
	}

	MEM_freeN(moves);
}


static void wm_gesture_draw_lasso(wmWindow *win, wmGesture *gt, bool filled)
{
	const short *lasso = (short *)gt->customdata;
	int i, numverts;
	float x, y;

	if (filled) {
		draw_filled_lasso(win, gt);
	}

	numverts = gt->points;
	if (gt->type == WM_GESTURE_LASSO) {
		numverts++;
	}

	/* Nothing to drawe, do early output. */
	if (numverts < 2) {
		return;
	}

	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
	unsigned line_origin = add_attrib(format, "line_origin", COMP_F32, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_COLOR);

	immUniform4f("color1", 0.4f, 0.4f, 0.4f, 1.0f);
	immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
	immUniform1f("dash_width", 2.0f);
	immUniform1f("dash_width_on", 1.0f);

	immBegin(PRIM_LINE_STRIP, numverts);

	for (i = 0; i < gt->points; i++, lasso += 2) {

		/* get line_origin coordinates only from the first vertex of each line */
		if (!(i % 2)) {
			x = (float)lasso[0];
			y = (float)lasso[1];
		}

		immAttrib2f(line_origin, x, y);
		immVertex2f(pos, (float)lasso[0], (float)lasso[1]);
	}

	if (gt->type == WM_GESTURE_LASSO) {
		immAttrib2f(line_origin, x, y);
		lasso = (short *)gt->customdata;
		immVertex2f(pos, (float)lasso[0], (float)lasso[1]);
	}

	immEnd();

	immUnbindProgram();
}

static void wm_gesture_draw_cross(wmWindow *win, wmGesture *gt)
{
	rcti *rect = (rcti *)gt->customdata;
	const int winsize_x = WM_window_pixels_x(win);
	const int winsize_y = WM_window_pixels_y(win);

	float x1, x2, y1, y2;

	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
	unsigned line_origin = add_attrib(format, "line_origin", COMP_F32, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_COLOR);

	immUniform4f("color1", 0.4f, 0.4f, 0.4f, 1.0f);
	immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
	immUniform1f("dash_width", 8.0f);
	immUniform1f("dash_width_on", 4.0f);

	immBegin(PRIM_LINES, 4);

	x1 = (float)(rect->xmin - winsize_x);
	y1 = (float)rect->ymin;
	x2 = (float)(rect->xmin + winsize_x);
	y2 = y1;

	immAttrib2f(line_origin, x1, y1);
	immVertex2f(pos, x1, y1);
	immAttrib2f(line_origin, x1, y1);
	immVertex2f(pos, x2, y2);

	x1 = (float)rect->xmin;
	y1 = (float)(rect->ymin - winsize_y);
	x2 = x1;
	y2 = (float)(rect->ymin + winsize_y);

	immAttrib2f(line_origin, x1, y1);
	immVertex2f(pos, x1, y1);
	immAttrib2f(line_origin, x1, y1);
	immVertex2f(pos, x2, y2);

	immEnd();

	immUnbindProgram();
}

/* called in wm_draw.c */
void wm_gesture_draw(wmWindow *win)
{
	wmGesture *gt = (wmGesture *)win->gesture.first;

	glLineWidth(1.0f);
	for (; gt; gt = gt->next) {
		/* all in subwindow space */
		wmSubWindowSet(win, gt->swinid);
		
		if (gt->type == WM_GESTURE_RECT)
			wm_gesture_draw_rect(gt);
//		else if (gt->type == WM_GESTURE_TWEAK)
//			wm_gesture_draw_line(gt);
		else if (gt->type == WM_GESTURE_CIRCLE)
			wm_gesture_draw_circle(gt);
		else if (gt->type == WM_GESTURE_CROSS_RECT) {
			if (gt->mode == 1)
				wm_gesture_draw_rect(gt);
			else
				wm_gesture_draw_cross(win, gt);
		}
		else if (gt->type == WM_GESTURE_LINES)
			wm_gesture_draw_lasso(win, gt, false);
		else if (gt->type == WM_GESTURE_LASSO)
			wm_gesture_draw_lasso(win, gt, true);
		else if (gt->type == WM_GESTURE_STRAIGHTLINE)
			wm_gesture_draw_line(gt);
	}
}

void wm_gesture_tag_redraw(bContext *C)
{
	wmWindow *win = CTX_wm_window(C);
	bScreen *screen = CTX_wm_screen(C);
	ARegion *ar = CTX_wm_region(C);
	
	if (screen)
		screen->do_draw_gesture = true;

	wm_tag_redraw_overlay(win, ar);
}
