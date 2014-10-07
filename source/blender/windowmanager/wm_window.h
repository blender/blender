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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/wm_window.h
 *  \ingroup wm
 */


#ifndef __WM_WINDOW_H__
#define __WM_WINDOW_H__

struct bScreen;
struct wmOperator;

/* *************** internal api ************** */
void		wm_ghost_init			(bContext *C);
void		wm_ghost_exit(void);

void wm_get_screensize(int *width_r, int *height_r);
void wm_get_desktopsize(int *width_r, int *height_r);

wmWindow	*wm_window_new			(bContext *C);
void		wm_window_free			(bContext *C, wmWindowManager *wm, wmWindow *win);
void		wm_window_close			(bContext *C, wmWindowManager *wm, wmWindow *win);

void		wm_window_title				(wmWindowManager *wm, wmWindow *win);
void		wm_window_add_ghostwindows	(wmWindowManager *wm);
void		wm_window_process_events	(const bContext *C);
void		wm_window_process_events_nosleep(void);

void		wm_window_make_drawable(wmWindowManager *wm, wmWindow *win);

void		wm_window_raise			(wmWindow *win);
void		wm_window_lower			(wmWindow *win);
void		wm_window_set_size		(wmWindow *win, int width, int height);
void		wm_window_get_position	(wmWindow *win, int *posx_r, int *posy_r);
void		wm_window_swap_buffers	(wmWindow *win);
void		wm_window_set_swap_interval(wmWindow *win, int interval);
bool		wm_window_get_swap_interval(wmWindow *win, int *intervalOut);

float		wm_window_pixelsize(wmWindow *win);

void		wm_get_cursor_position	(wmWindow *win, int *x, int *y);

wmWindow	*wm_window_copy			(bContext *C, wmWindow *winorig);

void		wm_window_testbreak		(void);

/* *************** window operators ************** */
int			wm_window_duplicate_exec(bContext *C, struct wmOperator *op);
int			wm_window_fullscreen_toggle_exec(bContext *C, struct wmOperator *op);

/* Initial (unmaximized) size to start with for
 * systems that can't find it for themselves (X11).
 * Clamped by real desktop limits */
#define WM_WIN_INIT_SIZE_X 1800
#define WM_WIN_INIT_SIZE_Y 1000
#define WM_WIN_INIT_PAD 40

#endif /* __WM_WINDOW_H__ */

