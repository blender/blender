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
 */


#include "DNA_screen_types.h"
#include "DNA_vec_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_scanfill.h"   /* lasso tessellation */
#include "BLI_utildefines.h"
#include "BLI_lasso.h"

#include "BKE_context.h"


#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_event_system.h"
#include "wm_subwindow.h"
#include "wm_draw.h"


#include "BIF_gl.h"
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
	
	wm_subwindow_getorigin(window, gesture->swinid, &sx, &sy);
	
	if (ELEM5(type, WM_GESTURE_RECT, WM_GESTURE_CROSS_RECT, WM_GESTURE_TWEAK,
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
		if (ABS(dx) + ABS(dy) > U.tweak_threshold) {
			int theta = (int)floor(4.0f * atan2f((float)dy, (float)dx) / (float)M_PI + 0.5f);
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

static void wm_gesture_draw_rect(wmGesture *gt)
{
	rcti *rect = (rcti *)gt->customdata;
	
	glEnable(GL_BLEND);
	glColor4f(1.0, 1.0, 1.0, 0.05);
	glBegin(GL_QUADS);
	glVertex2s(rect->xmax, rect->ymin);
	glVertex2s(rect->xmax, rect->ymax);
	glVertex2s(rect->xmin, rect->ymax);
	glVertex2s(rect->xmin, rect->ymin);
	glEnd();
	glDisable(GL_BLEND);
	
	glEnable(GL_LINE_STIPPLE);
	glColor3ub(96, 96, 96);
	glLineStipple(1, 0xCCCC);
	sdrawbox(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
	glColor3ub(255, 255, 255);
	glLineStipple(1, 0x3333);
	sdrawbox(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
	glDisable(GL_LINE_STIPPLE);
}

static void wm_gesture_draw_line(wmGesture *gt)
{
	rcti *rect = (rcti *)gt->customdata;
	
	glEnable(GL_LINE_STIPPLE);
	glColor3ub(96, 96, 96);
	glLineStipple(1, 0xAAAA);
	sdrawline(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
	glColor3ub(255, 255, 255);
	glLineStipple(1, 0x5555);
	sdrawline(rect->xmin, rect->ymin, rect->xmax, rect->ymax);

	glDisable(GL_LINE_STIPPLE);
	
}

static void wm_gesture_draw_circle(wmGesture *gt)
{
	rcti *rect = (rcti *)gt->customdata;

	glTranslatef((float)rect->xmin, (float)rect->ymin, 0.0f);

	glEnable(GL_BLEND);
	glColor4f(1.0, 1.0, 1.0, 0.05);
	glutil_draw_filled_arc(0.0, M_PI * 2.0, rect->xmax, 40);
	glDisable(GL_BLEND);
	
	glEnable(GL_LINE_STIPPLE);
	glColor3ub(96, 96, 96);
	glLineStipple(1, 0xAAAA);
	glutil_draw_lined_arc(0.0, M_PI * 2.0, rect->xmax, 40);
	glColor3ub(255, 255, 255);
	glLineStipple(1, 0x5555);
	glutil_draw_lined_arc(0.0, M_PI * 2.0, rect->xmax, 40);
	
	glDisable(GL_LINE_STIPPLE);
	glTranslatef(-rect->xmin, -rect->ymin, 0.0f);
	
}

struct LassoFillData {
	unsigned int *px;
	int width;
};

static void draw_filled_lasso_px_cb(int x, int y, void *user_data)
{
	struct LassoFillData *data = user_data;
	unsigned char *col = (unsigned char *)&(data->px[(y * data->width) + x]);
	col[0] = col[1] = col[2] = 0xff;
	col[3] = 0x10;
}

static void draw_filled_lasso(wmWindow *win, wmGesture *gt)
{
	short *lasso = (short *)gt->customdata;
	const int tot = gt->points;
	int (*moves)[2] = MEM_mallocN(sizeof(*moves) * (tot + 1), __func__);
	int i;
	rcti rect;
	rcti rect_win;

	for (i = 0; i < tot; i++, lasso += 2) {
		moves[i][0] = lasso[0];
		moves[i][1] = lasso[1];
	}

	BLI_lasso_boundbox(&rect, (const int (*)[2])moves, tot);

	wm_subwindow_getrect(win, gt->swinid, &rect_win);
	BLI_rcti_translate(&rect, rect_win.xmin, rect_win.ymin);
	BLI_rcti_isect(&rect_win, &rect, &rect);
	BLI_rcti_translate(&rect, -rect_win.xmin, -rect_win.ymin);

	/* highly unlikely this will fail, but could crash if (tot == 0) */
	if (BLI_rcti_is_empty(&rect) == false) {
		const int w = BLI_rcti_size_x(&rect);
		const int h = BLI_rcti_size_y(&rect);
		unsigned int *pixel_buf = MEM_callocN(sizeof(*pixel_buf) * w * h, __func__);
		struct LassoFillData lasso_fill_data = {pixel_buf, w};

		fill_poly_v2i_n(
		       rect.xmin, rect.ymin, rect.xmax, rect.ymax,
		       (const int (*)[2])moves, tot,
		       draw_filled_lasso_px_cb, &lasso_fill_data);

		glEnable(GL_BLEND);
		// glColor4f(1.0, 1.0, 1.0, 0.05);

		glRasterPos2f(rect.xmin, rect.ymin);

		glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixel_buf);

		glDisable(GL_BLEND);
		MEM_freeN(pixel_buf);
	}

	MEM_freeN(moves);
}


static void wm_gesture_draw_lasso(wmWindow *win, wmGesture *gt, bool filled)
{
	short *lasso = (short *)gt->customdata;
	int i;

	if (filled) {
		draw_filled_lasso(win, gt);
	}

	glEnable(GL_LINE_STIPPLE);
	glColor3ub(96, 96, 96);
	glLineStipple(1, 0xAAAA);
	glBegin(GL_LINE_STRIP);
	for (i = 0; i < gt->points; i++, lasso += 2)
		glVertex2sv(lasso);
	if (gt->type == WM_GESTURE_LASSO)
		glVertex2sv((short *)gt->customdata);
	glEnd();
	
	glColor3ub(255, 255, 255);
	glLineStipple(1, 0x5555);
	glBegin(GL_LINE_STRIP);
	lasso = (short *)gt->customdata;
	for (i = 0; i < gt->points; i++, lasso += 2)
		glVertex2sv(lasso);
	if (gt->type == WM_GESTURE_LASSO)
		glVertex2sv((short *)gt->customdata);
	glEnd();
	
	glDisable(GL_LINE_STIPPLE);
	
}

static void wm_gesture_draw_cross(wmWindow *win, wmGesture *gt)
{
	rcti *rect = (rcti *)gt->customdata;
	const int winsize_x = WM_window_pixels_x(win);
	const int winsize_y = WM_window_pixels_y(win);

	glEnable(GL_LINE_STIPPLE);
	glColor3ub(96, 96, 96);
	glLineStipple(1, 0xCCCC);
	sdrawline(rect->xmin - winsize_x, rect->ymin, rect->xmin + winsize_x, rect->ymin);
	sdrawline(rect->xmin, rect->ymin - winsize_y, rect->xmin, rect->ymin + winsize_y);
	
	glColor3ub(255, 255, 255);
	glLineStipple(1, 0x3333);
	sdrawline(rect->xmin - winsize_x, rect->ymin, rect->xmin + winsize_x, rect->ymin);
	sdrawline(rect->xmin, rect->ymin - winsize_y, rect->xmin, rect->ymin + winsize_y);
	glDisable(GL_LINE_STIPPLE);
}

/* called in wm_draw.c */
void wm_gesture_draw(wmWindow *win)
{
	wmGesture *gt = (wmGesture *)win->gesture.first;
	
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
		screen->do_draw_gesture = TRUE;

	wm_tag_redraw_overlay(win, ar);
}
