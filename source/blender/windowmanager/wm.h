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

/** \file blender/windowmanager/wm.h
 *  \ingroup wm
 */

#ifndef __WM_H__
#define __WM_H__

struct wmWindow;
struct ReportList;

typedef struct wmPaintCursor {
	struct wmPaintCursor *next, *prev;

	void *customdata;
	
	int (*poll)(struct bContext *C);
	void (*draw)(bContext *C, int, int, void *customdata);
} wmPaintCursor;

extern void wm_close_and_free(bContext *C, wmWindowManager *);
extern void wm_close_and_free_all(bContext *C, ListBase *);

extern void wm_add_default(bContext *C);
extern void wm_clear_default_size(bContext *C);
			
			/* register to windowmanager for redo or macro */
void		wm_operator_register(bContext *C, wmOperator *op);

/* wm_operator.c, for init/exit */
void wm_operatortype_free(void);
void wm_operatortype_init(void);
void wm_window_keymap(wmKeyConfig *keyconf);

void wm_tweakevent_test(bContext *C, wmEvent *event, int action);

/* wm_gesture.c */
#define WM_LASSO_MIN_POINTS		1024
void wm_gesture_draw(struct wmWindow *win);
int wm_gesture_evaluate(wmGesture *gesture);
void wm_gesture_tag_redraw(bContext *C);

/* wm_jobs.c */
void wm_jobs_timer(const bContext *C, wmWindowManager *wm, wmTimer *wt);
void wm_jobs_timer_ended(wmWindowManager *wm, wmTimer *wt);

/* wm_files.c */
void wm_autosave_timer(const bContext *C, wmWindowManager *wm, wmTimer *wt);
void wm_autosave_timer_ended(wmWindowManager *wm);
void wm_autosave_delete(void);
void wm_autosave_read(bContext *C, struct ReportList *reports);
void wm_autosave_location(char *filepath);

/* wm_stereo.c */
void wm_method_draw_stereo3d(const bContext *C, wmWindow *win);
void wm_stereo3d_mouse_offset_apply(wmWindow *win, int *r_mouse_xy);
int wm_stereo3d_set_exec(bContext *C, wmOperator *op);
int wm_stereo3d_set_invoke(bContext *C, wmOperator *op, const wmEvent *event);
void wm_stereo3d_set_draw(bContext *C, wmOperator *op);
bool wm_stereo3d_set_check(bContext *C, wmOperator *op);
void wm_stereo3d_set_cancel(bContext *C, wmOperator *op);

/* init operator properties */
void wm_open_init_load_ui(wmOperator *op, bool use_prefs);
void wm_open_init_use_scripts(wmOperator *op, bool use_prefs);

/* hack to store circle select size - campbell, must replace with nice operator memory */
#define GESTURE_MEMORY

#ifdef GESTURE_MEMORY
extern int circle_select_size;
#endif

#endif /* __WM_H__ */

