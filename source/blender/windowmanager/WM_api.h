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
struct IDProperty;
struct wmEvent;
struct wmEventHandler;
struct wmGesture;
struct wmJob;
struct wmNotifier;
struct wmOperatorType;
struct wmOperator;
struct rcti;
struct PointerRNA;
struct EnumPropertyItem;
struct MenuType;
struct wmDropBox;
struct wmDrag;
struct ImBuf;

typedef struct wmJob wmJob;

/* general API */
void		WM_setprefsize		(int stax, int stay, int sizx, int sizy);

void		WM_init				(struct bContext *C, int argc, char **argv);
void		WM_exit				(struct bContext *C);
void		WM_main				(struct bContext *C);

void		WM_init_splash		(struct bContext *C);


void		WM_check			(struct bContext *C);

struct wmWindow	*WM_window_open	(struct bContext *C, struct rcti *rect);

			/* defines for 'type' WM_window_open_temp */
#define WM_WINDOW_RENDER		0
#define WM_WINDOW_USERPREFS		1
#define WM_WINDOW_FILESEL		2

void		WM_window_open_temp	(struct bContext *C, struct rcti *position, int type);



			/* files */
int			WM_read_homefile	(struct bContext *C, struct wmOperator *op);
int			WM_write_homefile	(struct bContext *C, struct wmOperator *op);
void		WM_read_file		(struct bContext *C, char *name, struct ReportList *reports);
void		WM_write_file		(struct bContext *C, char *target, int fileflags, struct ReportList *reports);
void		WM_read_autosavefile(struct bContext *C);
void		WM_autosave_init	(struct wmWindowManager *wm);

			/* mouse cursors */
void		WM_cursor_set		(struct wmWindow *win, int curs);
void		WM_cursor_modal		(struct wmWindow *win, int curs);
void		WM_cursor_restore	(struct wmWindow *win);
void		WM_cursor_wait		(int val);
void		WM_cursor_grab(struct wmWindow *win, int wrap, int hide, int *bounds);
void		WM_cursor_ungrab(struct wmWindow *win);
void		WM_timecursor		(struct wmWindow *win, int nr);

void		*WM_paint_cursor_activate(struct wmWindowManager *wm, int (*poll)(struct bContext *C), void (*draw)(struct bContext *C, int, int, void *customdata), void *customdata);
void		WM_paint_cursor_end(struct wmWindowManager *wm, void *handle);

void		WM_cursor_warp		(struct wmWindow *win, int x, int y);

			/* keyconfig and keymap */
wmKeyConfig *WM_keyconfig_add	(struct wmWindowManager *wm, char *idname);
void 		WM_keyconfig_free	(struct wmKeyConfig *keyconf);
void		WM_keyconfig_userdef(struct wmWindowManager *wm);

void		WM_keymap_init		(struct bContext *C);
void		WM_keymap_free		(struct wmKeyMap *keymap);

wmKeyMapItem *WM_keymap_verify_item(struct wmKeyMap *keymap, char *idname, int type, 
								 int val, int modifier, int keymodifier);
wmKeyMapItem *WM_keymap_add_item(struct wmKeyMap *keymap, char *idname, int type, 
								 int val, int modifier, int keymodifier);
wmKeyMapItem *WM_keymap_add_menu(struct wmKeyMap *keymap, char *idname, int type,
								 int val, int modifier, int keymodifier);

void         WM_keymap_remove_item(struct wmKeyMap *keymap, struct wmKeyMapItem *kmi);
char		 *WM_keymap_item_to_string(wmKeyMapItem *kmi, char *str, int len);

wmKeyMap	*WM_keymap_list_find(ListBase *lb, char *idname, int spaceid, int regionid);
wmKeyMap	*WM_keymap_find(struct wmKeyConfig *keyconf, char *idname, int spaceid, int regionid);
wmKeyMap	*WM_keymap_find_all(const struct bContext *C, char *idname, int spaceid, int regionid);
wmKeyMap	*WM_keymap_active(struct wmWindowManager *wm, struct wmKeyMap *keymap);
wmKeyMap	*WM_keymap_guess_opname(const struct bContext *C, char *opname);
int			 WM_keymap_user_init(struct wmWindowManager *wm, struct wmKeyMap *keymap);
wmKeyMap	*WM_keymap_copy_to_user(struct wmKeyMap *keymap);
void		WM_keymap_restore_to_default(struct wmKeyMap *keymap);
void		WM_keymap_properties_reset(struct wmKeyMapItem *kmi);
void		WM_keymap_restore_item_to_default(struct bContext *C, struct wmKeyMap *keymap, struct wmKeyMapItem *kmi);

wmKeyMapItem *WM_keymap_item_find_id(struct wmKeyMap *keymap, int id);
int			WM_keymap_item_compare(struct wmKeyMapItem *k1, struct wmKeyMapItem *k2);
int			WM_userdef_event_map(int kmitype);

wmKeyMap	*WM_modalkeymap_add(struct wmKeyConfig *keyconf, char *idname, struct EnumPropertyItem *items);
wmKeyMap	*WM_modalkeymap_get(struct wmKeyConfig *keyconf, char *idname);
wmKeyMapItem *WM_modalkeymap_add_item(struct wmKeyMap *km, int type, int val, int modifier, int keymodifier, int value);
void		WM_modalkeymap_assign(struct wmKeyMap *km, char *opname);

const char	*WM_key_event_string(short type);
int			WM_key_event_operator_id(const struct bContext *C, const char *opname, int opcontext, struct IDProperty *properties, int hotkey, struct wmKeyMap **keymap_r);
char		*WM_key_event_operator_string(const struct bContext *C, const char *opname, int opcontext, struct IDProperty *properties, char *str, int len);

			/* handlers */

struct wmEventHandler *WM_event_add_keymap_handler(ListBase *handlers, wmKeyMap *keymap);
						/* boundbox, optional subwindow boundbox for offset */
struct wmEventHandler *WM_event_add_keymap_handler_bb(ListBase *handlers, wmKeyMap *keymap, rcti *bb, rcti *swinbb);
						/* priority not implemented, it adds in begin */
struct wmEventHandler *WM_event_add_keymap_handler_priority(ListBase *handlers, wmKeyMap *keymap, int priority);

void		WM_event_remove_keymap_handler(ListBase *handlers, wmKeyMap *keymap);

struct wmEventHandler *WM_event_add_ui_handler(const struct bContext *C, ListBase *handlers,
			int (*func)(struct bContext *C, struct wmEvent *event, void *userdata),
			void (*remove)(struct bContext *C, void *userdata), void *userdata);
void		WM_event_remove_ui_handler(ListBase *handlers,
			int (*func)(struct bContext *C, struct wmEvent *event, void *userdata),
			void (*remove)(struct bContext *C, void *userdata), void *userdata);
void		WM_event_remove_area_handler(struct ListBase *handlers, void *area);

struct wmEventHandler *WM_event_add_modal_handler(struct bContext *C, struct wmOperator *op);
void		WM_event_remove_handlers(struct bContext *C, ListBase *handlers);

struct wmEventHandler *WM_event_add_dropbox_handler(ListBase *handlers, ListBase *dropboxes);

			/* mouse */
void		WM_event_add_mousemove(struct bContext *C);
int			WM_modal_tweak_exit(struct wmEvent *evt, int tweak_event);

			/* notifiers */
void		WM_event_add_notifier(const struct bContext *C, unsigned int type, void *reference);
void		WM_main_add_notifier(unsigned int type, void *reference);

void		wm_event_add		(struct wmWindow *win, struct wmEvent *event_to_add); /* XXX only for warning */

			/* at maximum, every timestep seconds it triggers event_type events */
struct wmTimer *WM_event_add_timer(struct wmWindowManager *wm, struct wmWindow *win, int event_type, double timestep);
void		WM_event_remove_timer(struct wmWindowManager *wm, struct wmWindow *win, struct wmTimer *timer);
void		WM_event_timer_sleep(struct wmWindowManager *wm, struct wmWindow *win, struct wmTimer *timer, int dosleep);

		/* operator api, default callbacks */
			/* invoke callback, uses enum property named "type" */
int			WM_menu_invoke			(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int			WM_enum_search_invoke(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
			/* invoke callback, confirm menu + exec */
int			WM_operator_confirm		(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
		/* invoke callback, file selector "path" unset + exec */
int			WM_operator_filesel		(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
			/* poll callback, context checks */
int			WM_operator_winactive	(struct bContext *C);
			/* invoke callback, exec + redo popup */
int			WM_operator_props_popup	(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int			WM_operator_redo_popup	(struct bContext *C, struct wmOperator *op);
void		WM_operator_ui_popup	(struct bContext *C, struct wmOperator *op, int width, int height);

int			WM_operator_confirm_message(struct bContext *C, struct wmOperator *op, char *message);

		/* operator api */
void		WM_operator_free		(struct wmOperator *op);
void		WM_operator_stack_clear(struct bContext *C);

struct wmOperatorType *WM_operatortype_find(const char *idnamem, int quiet);
struct wmOperatorType *WM_operatortype_exists(const char *idname);
struct wmOperatorType *WM_operatortype_first(void);
void		WM_operatortype_append	(void (*opfunc)(struct wmOperatorType*));
void		WM_operatortype_append_ptr	(void (*opfunc)(struct wmOperatorType*, void *), void *userdata);
void		WM_operatortype_append_macro_ptr	(void (*opfunc)(struct wmOperatorType*, void *), void *userdata);
int			WM_operatortype_remove(const char *idname);

struct wmOperatorType *WM_operatortype_append_macro(char *idname, char *name, int flag);
struct wmOperatorTypeMacro *WM_operatortype_macro_define(struct wmOperatorType *ot, const char *idname);


int			WM_operator_poll		(struct bContext *C, struct wmOperatorType *ot);
int			WM_operator_call		(struct bContext *C, struct wmOperator *op);
int			WM_operator_repeat		(struct bContext *C, struct wmOperator *op);
int         WM_operator_name_call	(struct bContext *C, const char *opstring, int context, struct PointerRNA *properties);
int			WM_operator_call_py(struct bContext *C, struct wmOperatorType *ot, int context, struct PointerRNA *properties, struct ReportList *reports);

void		WM_operator_properties_alloc(struct PointerRNA **ptr, struct IDProperty **properties, const char *opstring); /* used for keymap and macro items */
void		WM_operator_properties_create(struct PointerRNA *ptr, const char *opstring);
void		WM_operator_properties_create_ptr(struct PointerRNA *ptr, struct wmOperatorType *ot);
void		WM_operator_properties_free(struct PointerRNA *ptr);
void		WM_operator_properties_filesel(struct wmOperatorType *ot, int filter, short type);
void		WM_operator_properties_gesture_border(struct wmOperatorType *ot, int extend);
void		WM_operator_properties_select_all(struct wmOperatorType *ot);

/* MOVE THIS SOMEWHERE ELSE */
#define	SEL_TOGGLE		0
#define	SEL_SELECT		1
#define SEL_DESELECT	2
#define SEL_INVERT		3

		/* operator as a python command (resultuing string must be free'd) */
char		*WM_operator_pystring(struct bContext *C, struct wmOperatorType *ot, struct PointerRNA *opptr, int all_args);
void		WM_operator_bl_idname(char *to, const char *from);
void		WM_operator_py_idname(char *to, const char *from);

/* *************** menu types ******************** */
struct MenuType		*WM_menutype_find(const char *idname, int quiet);
int					WM_menutype_add(struct MenuType* mt);
void				WM_menutype_freelink(struct MenuType* mt);
void				WM_menutype_free(void);

			/* default operator callbacks for border/circle/lasso */
int			WM_border_select_invoke	(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int			WM_border_select_modal	(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int			WM_gesture_circle_invoke(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int			WM_gesture_circle_modal(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int			WM_gesture_lines_invoke(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int			WM_gesture_lines_modal(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int			WM_gesture_lasso_invoke(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int			WM_gesture_lasso_modal(struct bContext *C, struct wmOperator *op, struct wmEvent *event);

			/* default operator for arearegions, generates event */
void		WM_OT_tweak_gesture(struct wmOperatorType *ot);

			/* Gesture manager API */
struct wmGesture *WM_gesture_new(struct bContext *C, struct wmEvent *event, int type);
void		WM_gesture_end(struct bContext *C, struct wmGesture *gesture);
void		WM_gestures_remove(struct bContext *C);

			/* radial control operator */
int			WM_radial_control_invoke(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int			WM_radial_control_modal(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
void		WM_OT_radial_control_partial(struct wmOperatorType *ot);
void		WM_radial_control_string(struct wmOperator *op, char str[], int maxlen);

			/* fileselecting support */
void		WM_event_add_fileselect(struct bContext *C, struct wmOperator *op);
void		WM_event_fileselect_event(struct bContext *C, void *ophandle, int eventval);

			/* drag and drop */
struct wmDrag		*WM_event_start_drag(struct bContext *C, int icon, int type, void *poin, double value);
void				WM_event_drag_image(struct wmDrag *, struct ImBuf *, float scale, int sx, int sy);

struct wmDropBox	*WM_dropbox_add(ListBase *lb, const char *idname, int (*poll)(struct bContext *, struct wmDrag *, struct wmEvent *event),
						  void (*copy)(struct wmDrag *, struct wmDropBox *));
ListBase	*WM_dropboxmap_find(char *idname, int spaceid, int regionid);

			/* OpenGL wrappers, mimicking opengl syntax */
void		wmSubWindowSet			(struct wmWindow *win, int swinid);
void		wmSubWindowScissorSet	(struct wmWindow *win, int swinid, struct rcti *srct);

void		wmLoadMatrix		(float mat[][4]);
void		wmGetMatrix			(float mat[][4]);
void		wmMultMatrix		(float mat[][4]);
void		wmGetSingleMatrix	(float mat[][4]);
void		wmScale				(float x, float y, float z);
void		wmLoadIdentity		(void);		/* note: old name clear_view_mat */
void		wmPushMatrix		(void);		/* one level only */
void		wmPopMatrix			(void);		/* one level only */

void		wmFrustum			(float x1, float x2, float y1, float y2, float n, float f);
void		wmOrtho				(float x1, float x2, float y1, float y2, float n, float f);
void		wmOrtho2			(float x1, float x2, float y1, float y2);
void		wmOrthoPixelSpace	(void);

			/* utilities */
void		WM_set_framebuffer_index_color(int index);
int			WM_framebuffer_to_index(unsigned int col);

			/* threaded Jobs Manager */
#define WM_JOB_PRIORITY		1
#define WM_JOB_EXCL_RENDER	2

struct wmJob *WM_jobs_get(struct wmWindowManager *wm, struct wmWindow *win, void *owner, int flag);

int			WM_jobs_test(struct wmWindowManager *wm, void *owner);

void		WM_jobs_customdata(struct wmJob *, void *customdata, void (*free)(void *));
void		WM_jobs_timer(struct wmJob *, double timestep, unsigned int note, unsigned int endnote);
void		WM_jobs_callbacks(struct wmJob *, 
							  void (*startjob)(void *, short *, short *),
							  void (*initjob)(void *),
							  void (*update)(void *));

void		WM_jobs_start(struct wmWindowManager *wm, struct wmJob *);
void		WM_jobs_stop(struct wmWindowManager *wm, void *owner);
void		WM_jobs_stop_all(struct wmWindowManager *wm);

			/* clipboard */
char		*WM_clipboard_text_get(int selection);
void		WM_clipboard_text_set(char *buf, int selection);

#endif /* WM_API_H */

