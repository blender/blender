/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef WM_H
#define WM_H

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

extern void wm_report_free(wmReport *report);

/* wm_operator.c, for init/exit */
void wm_operatortype_free(void);
void wm_operatortype_init(void);
void wm_window_keymap(wmKeyConfig *keyconf);

void wm_tweakevent_test(bContext *C, wmEvent *event, int action);

/* wm_gesture.c */
#define WM_LASSO_MAX_POINTS		1024
void wm_gesture_draw(struct wmWindow *win);
int wm_gesture_evaluate(bContext *C, wmGesture *gesture);
void wm_gesture_tag_redraw(bContext *C);

/* wm_jobs.c */
void wm_jobs_timer(const bContext *C, wmWindowManager *wm, wmTimer *wt);
void wm_jobs_timer_ended(wmWindowManager *wm, wmTimer *wt);

/* wm_files.c */
void wm_autosave_timer(const bContext *C, wmWindowManager *wm, wmTimer *wt);
void wm_autosave_timer_ended(wmWindowManager *wm);
void wm_autosave_delete(void);
void wm_autosave_read(bContext *C, struct ReportList *reports);
void wm_autosave_location(char *filename);

/* hack to store circle select size - campbell, must replace with nice operator memory */
#define GESTURE_MEMORY

#ifdef GESTURE_MEMORY
extern int circle_select_size;
#endif

#endif /* WM_H */

