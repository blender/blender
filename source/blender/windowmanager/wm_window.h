/*
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
 */

/** \file
 * \ingroup wm
 */

#pragma once

struct wmOperator;

#ifdef __cplusplus
extern "C" {
#endif

/* *************** internal api ************** */

/**
 * \note #bContext can be null in background mode because we don't
 * need to event handling.
 */
void wm_ghost_init(bContext *C);
void wm_ghost_exit(void);

/**
 * This one should correctly check for apple top header...
 * done for Cocoa: returns window contents (and not frame) max size.
 */
void wm_get_screensize(int *r_width, int *r_height);
/**
 * Size of all screens (desktop), useful since the mouse is bound by this.
 */
void wm_get_desktopsize(int *r_width, int *r_height);

/**
 * Don't change context itself.
 */
wmWindow *wm_window_new(const struct Main *bmain,
                        wmWindowManager *wm,
                        wmWindow *parent,
                        bool dialog);
/**
 * Part of `wm_window.c` API.
 */
wmWindow *wm_window_copy(
    struct Main *bmain, wmWindowManager *wm, wmWindow *win_src, bool duplicate_layout, bool child);
/**
 * A higher level version of copy that tests the new window can be added.
 * (called from the operator directly).
 */
wmWindow *wm_window_copy_test(bContext *C, wmWindow *win_src, bool duplicate_layout, bool child);
/**
 * Including window itself.
 * \param C: can be NULL.
 * \note #ED_screen_exit should have been called.
 */
void wm_window_free(bContext *C, wmWindowManager *wm, wmWindow *win);
/**
 * This is event from ghost, or exit-Blender operator.
 */
void wm_window_close(bContext *C, wmWindowManager *wm, wmWindow *win);

void wm_window_title(wmWindowManager *wm, wmWindow *win);
/**
 * Initialize #wmWindow without `ghostwin`, open these and clear.
 *
 * Window size is read from window, if 0 it uses prefsize
 * called in #WM_check, also initialize stuff after file read.
 *
 * \warning After running, `win->ghostwin` can be NULL in rare cases
 * (where OpenGL driver fails to create a context for eg).
 * We could remove them with #wm_window_ghostwindows_remove_invalid
 * but better not since caller may continue to use.
 * Instead, caller needs to handle the error case and cleanup.
 */
void wm_window_ghostwindows_ensure(wmWindowManager *wm);
/**
 * Call after #wm_window_ghostwindows_ensure or #WM_check
 * (after loading a new file) in the unlikely event a window couldn't be created.
 */
void wm_window_ghostwindows_remove_invalid(bContext *C, wmWindowManager *wm);
void wm_window_process_events(const bContext *C);

void wm_window_clear_drawable(wmWindowManager *wm);
void wm_window_make_drawable(wmWindowManager *wm, wmWindow *win);
/**
 * Reset active the current window opengl drawing context.
 */
void wm_window_reset_drawable(void);

void wm_window_raise(wmWindow *win);
void wm_window_lower(wmWindow *win);
void wm_window_set_size(wmWindow *win, int width, int height);
void wm_window_get_position(wmWindow *win, int *r_pos_x, int *r_pos_y);
/**
 * \brief Push rendered buffer to the screen.
 */
void wm_window_swap_buffers(wmWindow *win);
void wm_window_set_swap_interval(wmWindow *win, int interval);
bool wm_window_get_swap_interval(wmWindow *win, int *intervalOut);

void wm_cursor_position_get(wmWindow *win, int *r_x, int *r_y);
void wm_cursor_position_from_ghost(wmWindow *win, int *r_x, int *r_y);
void wm_cursor_position_to_ghost(wmWindow *win, int *x, int *y);

#ifdef WITH_INPUT_IME
void wm_window_IME_begin(wmWindow *win, int x, int y, int w, int h, bool complete);
void wm_window_IME_end(wmWindow *win);
#endif

/* *************** window operators ************** */

int wm_window_close_exec(bContext *C, struct wmOperator *op);
/**
 * Full-screen operator callback.
 */
int wm_window_fullscreen_toggle_exec(bContext *C, struct wmOperator *op);
/**
 * Call the quit confirmation prompt or exit directly if needed. The use can
 * still cancel via the confirmation popup. Also, this may not quit Blender
 * immediately, but rather schedule the closing.
 *
 * \param win: The window to show the confirmation popup/window in.
 */
void wm_quit_with_optional_confirmation_prompt(bContext *C, wmWindow *win) ATTR_NONNULL();

int wm_window_new_exec(bContext *C, struct wmOperator *op);
int wm_window_new_main_exec(bContext *C, struct wmOperator *op);

void wm_test_autorun_revert_action_set(struct wmOperatorType *ot, struct PointerRNA *ptr);
void wm_test_autorun_warning(bContext *C);

#ifdef __cplusplus
}
#endif
