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

typedef struct wmPaintCursor {
	struct wmPaintCursor *next, *prev;

	void *customdata;
	
	int (*poll)(struct bContext *C);
	void (*draw)(bContext *C, int, int, void *customdata);
} wmPaintCursor;

extern void wm_close_and_free(bContext *C, wmWindowManager *);
extern void wm_close_and_free_all(bContext *C, ListBase *);

extern void wm_add_default(bContext *C);
extern void wm_check(bContext *C);
extern void wm_clear_default_size(bContext *C);
			
			/* register to windowmanager for redo or macro */
void		wm_operator_register(bContext *C, wmOperator *op);

extern void wm_report_free(wmReport *report);

/* wm_operator.c, for init/exit */
void wm_operatortype_free(void);
void wm_operatortype_init(void);
void wm_window_keymap(wmWindowManager *wm);

void wm_tweakevent_test(bContext *C, wmEvent *event, int action);

/* wm_gesture.c */
#define WM_LASSO_MAX_POINTS		1024
void wm_gesture_draw(struct wmWindow *win);
int wm_gesture_evaluate(bContext *C, wmGesture *gesture);
void wm_gesture_tag_redraw(bContext *C);

/* wm_jobs.h */
void WM_OT_jobs_timer(struct wmOperatorType *ot);
void wm_jobs_timer_ended(wmWindowManager *wm, wmTimer *wt);

#endif /* WM_H */

