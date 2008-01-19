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
#ifndef WM_API_H
#define WM_API_H

/* dna-savable wmStructs here */
#include "DNA_windowmanager_types.h"

struct bContext;
struct wmEvent;
struct wmEventHandler;
struct wmGesture;

			/* general API */
void		WM_setprefsize		(int stax, int stay, int sizx, int sizy);

void		WM_init				(struct bContext *C);
void		WM_exit				(struct bContext *C);
void		WM_main				(struct bContext *C);

			/* files */
int			WM_read_homefile	(struct bContext *C, int from_memory);
int			WM_write_homefile	(struct bContext *C, struct wmOperator *op);
void		WM_read_file		(struct bContext *C, char *name);
void		WM_write_file		(struct bContext *C, char *target);
void		WM_read_autosavefile(struct bContext *C);
void		WM_write_autosave	(struct bContext *C);

			/* mouse cursors */
void		WM_init_cursor_data	(void);
void		WM_set_cursor		(struct bContext *C, int curs);
void		WM_waitcursor		(struct bContext *C, int val);
void		WM_timecursor		(struct bContext *C, int nr);

			/* keymap and handlers */
void		WM_keymap_set_item	(ListBase *lb, char *idname, short type, 
								 short val, int modifier, short keymodifier);
void		WM_keymap_verify_item(ListBase *lb, char *idname, short type, 
								 short val, int modifier, short keymodifier);
void		WM_keymap_add_item	(ListBase *lb, char *idname, short type, 
								 short val, int modifier, short keymodifier);
struct wmEventHandler *WM_event_add_keymap_handler(ListBase *keymap, ListBase *handlers);
struct wmEventHandler *WM_event_add_modal_handler(ListBase *handlers, wmOperator *op);
void		WM_event_remove_modal_handler(ListBase *handlers, wmOperator *op);

void		WM_event_add_notifier(wmWindowManager *wm, wmWindow *window,
					int swinid, int type,
					int value, void *data);


			/* operator api, default callbacks */
			/* confirm menu + exec */
int			WM_operator_confirm		(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
			/* context checks */
int			WM_operator_winactive	(struct bContext *C);

			/* operator api */
wmOperatorType *WM_operatortype_find(const char *idname);
void		WM_operatortypelist_append(ListBase *lb);

/* 
 * Operator property api
 *
 * Some notes to take care:
 *
 * All the OP_set_* functions append a new property to the operator,
 * if the property already exist, just replace it with the new
 * value in other case make a new property and append it.
 *
 * The OP_get_string function is a "special case", this function
 * return a pointer to property data, so don't change/resize/free
 * the string, because probably we get a segfault.
 * I really think that is better duplicate the string, so we are
 * really sure that the property data don't change.
 *
 * OP_get_int/float/array return 0 on success (found the property)
 * or != 0 if can't found the property in the operator.
 * The property value are store in the "value" pointer.
 *
 * Both array function copy the property data into the "array"
 * pointer, but you need init the len pointer to the "array" size.
 *
 * For example:
 *	int vec[] = { 1, 2, 3, 4 };
 *	OP_set_int_array (op, "vector", vec, 4);
 *
 *	...
 *
 *	short len;
 *	int vec[4];
 *	len= 4; <---- set the size!!
 *	OP_get_int_array (op, "vector", vec, &len);
 */
void OP_set_int(wmOperator *op, char *name, int value);
void OP_set_float(wmOperator *op, char *name, float value);
void OP_set_string(wmOperator *op, char *name, char *str);
void OP_set_int_array(wmOperator *op, char *name, int *array, short len);
void OP_set_float_array(wmOperator *op, char *name, float *array, short len);

int OP_get_int(wmOperator *op, char *name, int *value);
int OP_get_float(wmOperator *op, char *name, float *value);
char *OP_get_string(wmOperator *op, char *name);
int OP_get_int_array(wmOperator *op, char *name, int *array, short *len);
int OP_get_float_array(wmOperator *op, char *name, float *array, short *len);

/*
 * Need call this function in the "exit callback"
 * of the operator, but only if you use the property system.
 **/
void OP_free_property(wmOperator *op);

			/* Gesture manager API */
struct wmGesture *WM_gesture_new(int type);
struct wmGesture *WM_gesture_dup(struct wmGesture *from);
void WM_gesture_send(wmWindow *win, struct wmGesture *gesture);

			/* OpenGL wrappers, mimicing opengl syntax */
void		wmLoadMatrix		(wmWindow *win, float mat[][4]);
void		wmGetMatrix			(wmWindow *win, float mat[][4]);
void		wmMultMatrix		(wmWindow *win, float mat[][4]);
void		wmGetSingleMatrix	(wmWindow *win, float mat[][4]);
void		wmScale				(wmWindow *win, float x, float y, float z);
void		wmLoadIdentity		(wmWindow *win);	/* note: old name clear_view_mat */

void		wmFrustum			(wmWindow *win, float x1, float x2, float y1, float y2, float n, float f);
void		wmOrtho				(wmWindow *win, float x1, float x2, float y1, float y2, float n, float f);
void		wmOrtho2			(wmWindow *win, float x1, float x2, float y1, float y2);


#endif /* WM_API_H */

