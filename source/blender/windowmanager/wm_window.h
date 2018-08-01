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

struct EnumPropertyItem;
struct wmEvent;
struct wmOperator;
struct PointerRNA;
struct PropertyRNA;

/* *************** internal api ************** */
void		wm_ghost_init			(bContext *C);
void		wm_ghost_exit(void);

void wm_get_screensize(int *r_width, int *r_height);
void wm_get_desktopsize(int *r_width, int *r_height);

wmWindow	*wm_window_new			(bContext *C, wmWindow *parent);
wmWindow	*wm_window_copy			(bContext *C, wmWindow *win_src, const bool duplicate_layout, const bool child);
wmWindow	*wm_window_copy_test	(bContext *C, wmWindow *win_src, const bool duplicate_layout, const bool child);
void		wm_window_free			(bContext *C, wmWindowManager *wm, wmWindow *win);
void		wm_window_close			(bContext *C, wmWindowManager *wm, wmWindow *win);

void		wm_window_title				(wmWindowManager *wm, wmWindow *win);
void		wm_window_ghostwindows_ensure(wmWindowManager *wm);
void		wm_window_ghostwindows_remove_invalid(bContext *C, wmWindowManager *wm);
void		wm_window_process_events	(const bContext *C);
void		wm_window_process_events_nosleep(void);

void		wm_window_clear_drawable(wmWindowManager *wm);
void		wm_window_make_drawable(wmWindowManager *wm, wmWindow *win);
void		wm_window_reset_drawable(void);

void		wm_window_raise			(wmWindow *win);
void		wm_window_lower			(wmWindow *win);
void		wm_window_set_size		(wmWindow *win, int width, int height);
void		wm_window_get_position	(wmWindow *win, int *r_pos_x, int *r_pos_y);
void		wm_window_swap_buffers	(wmWindow *win);
void		wm_window_set_swap_interval(wmWindow *win, int interval);
bool		wm_window_get_swap_interval(wmWindow *win, int *intervalOut);

void		wm_get_cursor_position			(wmWindow *win, int *x, int *y);
void		wm_cursor_position_from_ghost	(wmWindow *win, int *x, int *y);
void		wm_cursor_position_to_ghost		(wmWindow *win, int *x, int *y);

void		wm_window_testbreak		(void);

#ifdef WITH_INPUT_IME
void		wm_window_IME_begin	(wmWindow *win, int x, int y, int w, int h, bool complete);
void		wm_window_IME_end	(wmWindow *win);
#endif

/* *************** window operators ************** */
int			wm_window_close_exec(bContext *C, struct wmOperator *op);
int			wm_window_fullscreen_toggle_exec(bContext *C, struct wmOperator *op);
void		wm_quit_with_optional_confirmation_prompt(bContext *C, wmWindow *win) ATTR_NONNULL();

int			wm_window_new_exec(bContext *C, struct wmOperator *op);
int			wm_window_new_main_exec(bContext *C, struct wmOperator *op);

/* Initial (unmaximized) size to start with for
 * systems that can't find it for themselves (X11).
 * Clamped by real desktop limits */
#define WM_WIN_INIT_SIZE_X 1800
#define WM_WIN_INIT_SIZE_Y 1000
#define WM_WIN_INIT_PAD 40

#endif /* __WM_WINDOW_H__ */
