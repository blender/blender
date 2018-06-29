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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): 2007 Blender Foundation (refactor)
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 *
 * Subwindow opengl handling.
 * BTW: subwindows open/close in X11 are way too slow, tried it, and choose for my own system... (ton)
 *
 */

/** \file blender/windowmanager/intern/wm_subwindow.c
 *  \ingroup wm
 *
 * Internal subwindows used for OpenGL state, used for regions and screens.
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_windowmanager_types.h"
#include "DNA_screen_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BIF_gl.h"

#include "GPU_extensions.h"
#include "GPU_basic_shader.h"

#include "WM_api.h"
#include "wm_subwindow.h"

/**
 * \note #wmSubWindow stored in #wmWindow but not exposed outside this C file,
 * it seems a bit redundant (area regions can store it too, but we keep it
 * because we can store all kind of future opengl fanciness here.
 *
 * We use indices and array because:
 * - index has safety, no pointers from this C file hanging around
 * - fast lookups of indices with array, list would give overhead
 * - old code used it this way...
 * - keep option open to have 2 screens using same window
 */

typedef struct wmSubWindow {
	struct wmSubWindow *next, *prev;

	rcti winrct;
	int swinid;
} wmSubWindow;


/* ******************* open, free, set, get data ******************** */

/* not subwindow itself */
static void wm_subwindow_free(wmSubWindow *UNUSED(swin))
{
	/* future fancy stuff */
}

void wm_subwindows_free(wmWindow *win)
{
	wmSubWindow *swin;

	for (swin = win->subwindows.first; swin; swin = swin->next)
		wm_subwindow_free(swin);

	BLI_freelistN(&win->subwindows);
}


int wm_subwindow_get_id(wmWindow *win)
{
	if (win->curswin)
		return win->curswin->swinid;
	return 0;
}

static wmSubWindow *swin_from_swinid(wmWindow *win, int swinid)
{
	wmSubWindow *swin;

	for (swin = win->subwindows.first; swin; swin = swin->next)
		if (swin->swinid == swinid)
			break;
	return swin;
}


static void wm_swin_size_get(wmSubWindow *swin, int *x, int *y)
{
	*x = BLI_rcti_size_x(&swin->winrct) + 1;
	*y = BLI_rcti_size_y(&swin->winrct) + 1;
}
void wm_subwindow_size_get(wmWindow *win, int swinid, int *x, int *y)
{
	wmSubWindow *swin = swin_from_swinid(win, swinid);

	if (swin) {
		wm_swin_size_get(swin, x, y);
	}
}


static void wm_swin_origin_get(wmSubWindow *swin, int *x, int *y)
{
	*x = swin->winrct.xmin;
	*y = swin->winrct.ymin;
}
void wm_subwindow_origin_get(wmWindow *win, int swinid, int *x, int *y)
{
	wmSubWindow *swin = swin_from_swinid(win, swinid);

	if (swin) {
		wm_swin_origin_get(swin, x, y);
	}
}


static void wm_swin_matrix_get(wmWindow *win, wmSubWindow *swin, float mat[4][4])
{
	/* used by UI, should find a better way to get the matrix there */
	if (swin->swinid == win->screen->mainwin) {
		int width, height;

		wm_swin_size_get(swin, &width, &height);
		orthographic_m4(mat, -GLA_PIXEL_OFS, (float)width - GLA_PIXEL_OFS, -GLA_PIXEL_OFS, (float)height - GLA_PIXEL_OFS, -100, 100);
	}
	else {
		glGetFloatv(GL_PROJECTION_MATRIX, (float *)mat);
	}
}
void wm_subwindow_matrix_get(wmWindow *win, int swinid, float mat[4][4])
{
	wmSubWindow *swin = swin_from_swinid(win, swinid);

	if (swin) {
		wm_swin_matrix_get(win, swin, mat);
	}
}


static void wm_swin_rect_get(wmSubWindow *swin, rcti *r_rect)
{
	*r_rect = swin->winrct;
}
void wm_subwindow_rect_get(wmWindow *win, int swinid, rcti *r_rect)
{
	wmSubWindow *swin = swin_from_swinid(win, swinid);

	if (swin) {
		wm_swin_rect_get(swin, r_rect);
	}
}


static void wm_swin_rect_set(wmSubWindow *swin, const rcti *rect)
{
	swin->winrct = *rect;
}
void wm_subwindow_rect_set(wmWindow *win, int swinid, const rcti *rect)
{
	wmSubWindow *swin = swin_from_swinid(win, swinid);

	if (swin) {
		wm_swin_rect_set(swin, rect);
	}
}


/* always sets pixel-precise 2D window/view matrices */
/* coords is in whole pixels. xmin = 15, xmax = 16: means window is 2 pix big */
int wm_subwindow_open(wmWindow *win, const rcti *winrct, bool activate)
{
	wmSubWindow *swin;
	int width, height;
	int freewinid = 1;

	for (swin = win->subwindows.first; swin; swin = swin->next)
		if (freewinid <= swin->swinid)
			freewinid = swin->swinid + 1;

	win->curswin = swin = MEM_callocN(sizeof(wmSubWindow), "swinopen");
	BLI_addtail(&win->subwindows, swin);

	swin->swinid = freewinid;
	swin->winrct = *winrct;

	if (activate) {
		/* and we appy it all right away */
		wmSubWindowSet(win, swin->swinid);

		/* extra service */
		wm_swin_size_get(swin, &width, &height);
		wmOrtho2_pixelspace(width, height);
		glLoadIdentity();
	}

	return swin->swinid;
}

void wm_subwindow_close(wmWindow *win, int swinid)
{
	wmSubWindow *swin = swin_from_swinid(win, swinid);

	if (swin) {
		if (swin == win->curswin)
			win->curswin = NULL;
		wm_subwindow_free(swin);
		BLI_remlink(&win->subwindows, swin);
		MEM_freeN(swin);
	}
	else {
		printf("%s: Internal error, bad winid: %d\n", __func__, swinid);
	}
}

/* pixels go from 0-99 for a 100 pixel window */
void wm_subwindow_position(wmWindow *win, int swinid, const rcti *winrct, bool activate)
{
	wmSubWindow *swin = swin_from_swinid(win, swinid);

	if (swin) {
		const int winsize_x = WM_window_pixels_x(win);
		const int winsize_y = WM_window_pixels_y(win);

		int width, height;

		swin->winrct = *winrct;

		/* CRITICAL, this clamping ensures that
		 * the viewport never goes outside the screen
		 * edges (assuming the x, y coords aren't
		 *        outside). This caused a hardware lock
		 * on Matrox cards if it happens.
		 *
		 * Really Blender should never _ever_ try
		 * to do such a thing, but just to be safe
		 * clamp it anyway (or fix the bScreen
		 * scaling routine, and be damn sure you
		 * fixed it). - zr  (2001!)
		 */

		if (swin->winrct.xmax > winsize_x)
			swin->winrct.xmax = winsize_x;
		if (swin->winrct.ymax > winsize_y)
			swin->winrct.ymax = winsize_y;

		if (activate) {
			/* extra service */
			wmSubWindowSet(win, swinid);
			wm_swin_size_get(swin, &width, &height);
			wmOrtho2_pixelspace(width, height);
		}
	}
	else {
		printf("%s: Internal error, bad winid: %d\n", __func__, swinid);
	}
}

/* ---------------- WM versions of OpenGL style API calls ------------------------ */
/* ----------------- exported in WM_api.h ------------------------------------------------------ */

/* internal state, no threaded opengl! XXX */
static wmWindow *_curwindow = NULL;
static wmSubWindow *_curswin = NULL;

void wmSubWindowScissorSet(wmWindow *win, int swinid, const rcti *srct, bool srct_pad)
{
	int width, height;
	_curswin = swin_from_swinid(win, swinid);

	if (_curswin == NULL) {
		printf("%s %d: doesn't exist\n", __func__, swinid);
		return;
	}

	win->curswin = _curswin;
	_curwindow = win;

	width  = BLI_rcti_size_x(&_curswin->winrct) + 1;
	height = BLI_rcti_size_y(&_curswin->winrct) + 1;
	glViewport(_curswin->winrct.xmin, _curswin->winrct.ymin, width, height);

	if (srct) {
		int scissor_width  = BLI_rcti_size_x(srct);
		int scissor_height = BLI_rcti_size_y(srct);

		/* typically a single pixel doesn't matter,
		 * but one pixel offset is noticeable with viewport border render */
		if (srct_pad) {
			scissor_width  += 1;
			scissor_height += 1;
		}

		glScissor(srct->xmin, srct->ymin, scissor_width, scissor_height);
	}
	else
		glScissor(_curswin->winrct.xmin, _curswin->winrct.ymin, width, height);

	wmOrtho2_pixelspace(width, height);
	glLoadIdentity();

	glFlush();
}

/* enable the WM versions of opengl calls */
void wmSubWindowSet(wmWindow *win, int swinid)
{
	wmSubWindowScissorSet(win, swinid, NULL, true);
}

void wmFrustum(float x1, float x2, float y1, float y2, float n, float f)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(x1, x2, y1, y2, n, f);
	glMatrixMode(GL_MODELVIEW);
}

void wmOrtho(float x1, float x2, float y1, float y2, float n, float f)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(x1, x2, y1, y2, n, f);

	glMatrixMode(GL_MODELVIEW);
}

void wmOrtho2(float x1, float x2, float y1, float y2)
{
	/* prevent opengl from generating errors */
	if (x1 == x2) x2 += 1.0f;
	if (y1 == y2) y2 += 1.0f;

	wmOrtho(x1, x2, y1, y2, -100, 100);
}

static void wmOrtho2_offset(const float x, const float y, const float ofs)
{
	wmOrtho2(ofs, x + ofs, ofs, y + ofs);
}

/**
 * default pixel alignment.
 */
void wmOrtho2_region_pixelspace(const ARegion *ar)
{
	wmOrtho2_offset(ar->winx, ar->winy, -0.01f);
}

void wmOrtho2_pixelspace(const float x, const float y)
{
	wmOrtho2_offset(x, y, -GLA_PIXEL_OFS);
}

/* ********** END MY WINDOW ************** */
