/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

struct wmWindow;

#include "gizmo/wm_gizmo_wmapi.h"

struct wmPaintCursor {
  wmPaintCursor *next, *prev;

  void *customdata;

  bool (*poll)(bContext *C);
  void (*draw)(bContext *C, int, int, void *customdata);

  short space_type;
  short region_type;
};

/**
 * Cause a delayed #WM_exit()
 * call to avoid leaking memory when trying to exit from within operators.
 */
void wm_exit_schedule_delayed(const bContext *C);

/**
 * Context is allowed to be NULL, do not free wm itself `lib_id.cc`.
 */
extern void wm_close_and_free(bContext *C, wmWindowManager *);

/**
 * On startup, it adds all data, for matching.
 */
extern void wm_add_default(Main *bmain, bContext *C);
extern void wm_clear_default_size(bContext *C);

/* Register to window-manager for redo or macro. */

/**
 * Called on event handling by `event_system.c`.
 *
 * All operations get registered in the window-manager here.
 */
void wm_operator_register(bContext *C, wmOperator *op);

/* wm_operator.c, for init/exit */

void wm_operatortype_free();
/**
 * Called on initialize #WM_init().
 */
void wm_operatortype_init();
/**
 * Default key-map for windows and screens, only call once per WM.
 */
void wm_window_keymap(wmKeyConfig *keyconf);
void wm_operatortypes_register();

/* `wm_gesture.cc` */

/* Called in `wm_draw.cc`. */

void wm_gesture_draw(wmWindow *win);
/**
 * Use for line gesture.
 */
void wm_gesture_tag_redraw(wmWindow *win);

/* `wm_jobs.cc` */

/**
 * Hard-coded to event #TIMERJOBS.
 */
void wm_jobs_timer(wmWindowManager *wm, wmTimer *wt);
/**
 * Kill job entirely, also removes timer itself.
 */
void wm_jobs_timer_end(wmWindowManager *wm, wmTimer *wt);

/* wm_files.cc */

/**
 * Run the auto-save timer action.
 */
void wm_autosave_timer(Main *bmain, wmWindowManager *wm, wmTimer *wt);
void wm_autosave_timer_begin(wmWindowManager *wm);
void wm_autosave_timer_end(wmWindowManager *wm);
void wm_autosave_delete();

/* `wm_splash_screen.cc` */

void WM_OT_splash(wmOperatorType *ot);
void WM_OT_splash_about(wmOperatorType *ot);

/* `wm_stereo.cc` */

void wm_stereo3d_draw_sidebyside(wmWindow *win, int view);
void wm_stereo3d_draw_topbottom(wmWindow *win, int view);

/**
 * If needed, adjust \a r_mouse_xy
 * so that drawn cursor and handled mouse position are matching visually.
 */
void wm_stereo3d_mouse_offset_apply(wmWindow *win, int r_mouse_xy[2]);
int wm_stereo3d_set_exec(bContext *C, wmOperator *op);
int wm_stereo3d_set_invoke(bContext *C, wmOperator *op, const wmEvent *event);
void wm_stereo3d_set_draw(bContext *C, wmOperator *op);
bool wm_stereo3d_set_check(bContext *C, wmOperator *op);
void wm_stereo3d_set_cancel(bContext *C, wmOperator *op);

/**
 * Initialize operator properties.
 */
void wm_open_init_load_ui(wmOperator *op, bool use_prefs);
void wm_open_init_use_scripts(wmOperator *op, bool use_prefs);
