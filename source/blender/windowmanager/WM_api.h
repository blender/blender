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
#ifndef __WM_API_H__
#define __WM_API_H__

/** \file WM_api.h
 *  \ingroup wm
 *
 *  \page wmpage windowmanager
 *  \section wmabout About windowmanager
 *  \ref wm handles events received from \ref GHOST and manages
 *  the screens, areas and input for Blender
 *  \section wmnote NOTE
 *  \todo document
 */

/* dna-savable wmStructs here */
#include "DNA_windowmanager_types.h"
#include "WM_keymap.h"
#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

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
struct PropertyRNA;
struct EnumPropertyItem;
struct MenuType;
struct wmDropBox;
struct wmDrag;
struct ImBuf;
struct ImageFormatData;
struct ARegion;
struct wmNDOFMotionData;

typedef struct wmJob wmJob;

/* general API */
void		WM_init_state_size_set		(int stax, int stay, int sizx, int sizy);
void		WM_init_state_fullscreen_set(void);
void		WM_init_state_normal_set(void);
void		WM_init_native_pixels(bool do_it);

void		WM_init				(struct bContext *C, int argc, const char **argv);
void		WM_exit_ext			(struct bContext *C, const bool do_python);

void		WM_exit				(struct bContext *C) ATTR_NORETURN;

void		WM_main				(struct bContext *C) ATTR_NORETURN;

bool 		WM_init_game		(struct bContext *C);
void		WM_init_splash		(struct bContext *C);


void		WM_check			(struct bContext *C);

struct wmWindow	*WM_window_open	(struct bContext *C, const struct rcti *rect);

int			WM_window_pixels_x		(struct wmWindow *win);
int			WM_window_pixels_y		(struct wmWindow *win);
bool		WM_window_is_fullscreen	(struct wmWindow *win);

/* defines for 'type' WM_window_open_temp */
#define WM_WINDOW_RENDER		0
#define WM_WINDOW_USERPREFS		1
// #define WM_WINDOW_FILESEL		2  // UNUSED

void		WM_window_open_temp	(struct bContext *C, struct rcti *position, int type);
			
			/* returns true if draw method is triple buffer */
bool		WM_is_draw_triple(struct wmWindow *win);



			/* files */
void		WM_file_autoexec_init(const char *filepath);
bool		WM_file_read(struct bContext *C, const char *filepath, struct ReportList *reports);
void		WM_autosave_init(struct wmWindowManager *wm);
void		WM_recover_last_session(struct bContext *C, struct ReportList *reports);

			/* mouse cursors */
void		WM_cursor_set(struct wmWindow *win, int curs);
void		WM_cursor_modal_set(struct wmWindow *win, int curs);
void		WM_cursor_modal_restore(struct wmWindow *win);
void		WM_cursor_wait		(bool val);
void		WM_cursor_grab_enable(struct wmWindow *win, bool wrap, bool hide, int bounds[4]);
void		WM_cursor_grab_disable(struct wmWindow *win, const int mouse_ungrab_xy[2]);
void		WM_cursor_time		(struct wmWindow *win, int nr);

void		*WM_paint_cursor_activate(struct wmWindowManager *wm,
                                      int (*poll)(struct bContext *C),
                                      void (*draw)(struct bContext *C, int, int, void *customdata),
                                      void *customdata);

void		WM_paint_cursor_end(struct wmWindowManager *wm, void *handle);
void		WM_paint_cursor_tag_redraw(struct wmWindow *win, struct ARegion *ar);

void		WM_cursor_warp		(struct wmWindow *win, int x, int y);
float		WM_cursor_pressure	(const struct wmWindow *win);

			/* event map */
int			WM_userdef_event_map(int kmitype);

			/* handlers */

struct wmEventHandler *WM_event_add_keymap_handler(ListBase *handlers, wmKeyMap *keymap);
						/* boundbox, optional subwindow boundbox for offset */
struct wmEventHandler *WM_event_add_keymap_handler_bb(ListBase *handlers, wmKeyMap *keymap, const rcti *bb, const rcti *swinbb);
						/* priority not implemented, it adds in begin */
struct wmEventHandler *WM_event_add_keymap_handler_priority(ListBase *handlers, wmKeyMap *keymap, int priority);

void		WM_event_remove_keymap_handler(ListBase *handlers, wmKeyMap *keymap);

typedef int (*wmUIHandlerFunc)(struct bContext *C, const struct wmEvent *event, void *userdata);
typedef void (*wmUIHandlerRemoveFunc)(struct bContext *C, void *userdata);

struct wmEventHandler *WM_event_add_ui_handler(
        const struct bContext *C, ListBase *handlers,
        wmUIHandlerFunc ui_handle, wmUIHandlerRemoveFunc ui_remove,
        void *userdata);
void WM_event_remove_ui_handler(
        ListBase *handlers,
        wmUIHandlerFunc ui_handle, wmUIHandlerRemoveFunc ui_remove,
        void *userdata, const bool postpone);
void WM_event_remove_area_handler(
        struct ListBase *handlers, void *area);
void WM_event_free_ui_handler_all(
        struct bContext *C, ListBase *handlers,
        wmUIHandlerFunc ui_handle, wmUIHandlerRemoveFunc ui_remove);

struct wmEventHandler *WM_event_add_modal_handler(struct bContext *C, struct wmOperator *op);
void		WM_event_remove_handlers(struct bContext *C, ListBase *handlers);

struct wmEventHandler *WM_event_add_dropbox_handler(ListBase *handlers, ListBase *dropboxes);

			/* mouse */
void		WM_event_add_mousemove(struct bContext *C);
bool        WM_modal_tweak_exit(const struct wmEvent *event, int tweak_event);

			/* notifiers */
void		WM_event_add_notifier(const struct bContext *C, unsigned int type, void *reference);
void		WM_main_add_notifier(unsigned int type, void *reference);
void		WM_main_remove_notifier_reference(const void *reference);

			/* reports */
void        WM_report(const struct bContext *C, ReportType type, const char *message);
void        WM_reportf(const struct bContext *C, ReportType type, const char *format, ...) ATTR_PRINTF_FORMAT(3, 4);

void		wm_event_add(struct wmWindow *win, const struct wmEvent *event_to_add);
void		wm_event_init_from_window(struct wmWindow *win, struct wmEvent *event);


			/* at maximum, every timestep seconds it triggers event_type events */
struct wmTimer *WM_event_add_timer(struct wmWindowManager *wm, struct wmWindow *win, int event_type, double timestep);
void		WM_event_remove_timer(struct wmWindowManager *wm, struct wmWindow *win, struct wmTimer *timer);
void        WM_event_timer_sleep(struct wmWindowManager *wm, struct wmWindow *win, struct wmTimer *timer, bool do_sleep);

		/* operator api, default callbacks */
			/* invoke callback, uses enum property named "type" */
void		WM_operator_view3d_unit_defaults(struct bContext *C, struct wmOperator *op);
int			WM_operator_smooth_viewtx_get(const struct wmOperator *op);
int			WM_menu_invoke			(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int			WM_enum_search_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
			/* invoke callback, confirm menu + exec */
int			WM_operator_confirm		(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
		/* invoke callback, file selector "filepath" unset + exec */
int			WM_operator_filesel		(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
bool        WM_operator_filesel_ensure_ext_imtype(wmOperator *op, const struct ImageFormatData *im_format);
			/* poll callback, context checks */
int			WM_operator_winactive	(struct bContext *C);
			/* invoke callback, exec + redo popup */
int			WM_operator_props_popup_confirm(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int			WM_operator_props_popup_call(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int			WM_operator_props_popup	(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int 		WM_operator_props_dialog_popup(struct bContext *C, struct wmOperator *op, int width, int height);
int			WM_operator_redo_popup	(struct bContext *C, struct wmOperator *op);
int			WM_operator_ui_popup	(struct bContext *C, struct wmOperator *op, int width, int height);

int         WM_operator_confirm_message_ex(struct bContext *C, struct wmOperator *op,
                                           const char *title, const int icon,
                                           const char *message);
int         WM_operator_confirm_message(struct bContext *C, struct wmOperator *op,
                                        const char *message);

		/* operator api */
void		WM_operator_free		(struct wmOperator *op);
void		WM_operator_type_set(struct wmOperator *op, struct wmOperatorType *ot);
void		WM_operator_stack_clear(struct wmWindowManager *wm);
void		WM_operator_handlers_clear(wmWindowManager *wm, struct wmOperatorType *ot);

struct wmOperatorType *WM_operatortype_find(const char *idname, bool quiet);
struct GHashIterator  *WM_operatortype_iter(void);
void		WM_operatortype_append(void (*opfunc)(struct wmOperatorType *));
void		WM_operatortype_append_ptr(void (*opfunc)(struct wmOperatorType *, void *), void *userdata);
void		WM_operatortype_append_macro_ptr(void (*opfunc)(struct wmOperatorType *, void *), void *userdata);
void        WM_operatortype_remove_ptr(struct wmOperatorType *ot);
bool        WM_operatortype_remove(const char *idname);

struct wmOperatorType *WM_operatortype_append_macro(const char *idname, const char *name, const char *description, int flag);
struct wmOperatorTypeMacro *WM_operatortype_macro_define(struct wmOperatorType *ot, const char *idname);


int			WM_operator_poll		(struct bContext *C, struct wmOperatorType *ot);
int			WM_operator_poll_context(struct bContext *C, struct wmOperatorType *ot, short context);
int         WM_operator_call_ex(struct bContext *C, struct wmOperator *op, const bool store);
int			WM_operator_call		(struct bContext *C, struct wmOperator *op);
int			WM_operator_call_notest(struct bContext *C, struct wmOperator *op);
int			WM_operator_repeat		(struct bContext *C, struct wmOperator *op);
bool        WM_operator_repeat_check(const struct bContext *C, struct wmOperator *op);
int         WM_operator_name_call_ptr(struct bContext *C, struct wmOperatorType *ot, short context, struct PointerRNA *properties);
int			WM_operator_name_call(struct bContext *C, const char *opstring, short context, struct PointerRNA *properties);
int			WM_operator_call_py(struct bContext *C, struct wmOperatorType *ot, short context, struct PointerRNA *properties, struct ReportList *reports, const bool is_undo);

void		WM_operator_properties_alloc(struct PointerRNA **ptr, struct IDProperty **properties, const char *opstring); /* used for keymap and macro items */
void		WM_operator_properties_sanitize(struct PointerRNA *ptr, const bool no_context); /* make props context sensitive or not */
bool        WM_operator_properties_default(struct PointerRNA *ptr, const bool do_update);
void        WM_operator_properties_reset(struct wmOperator *op);
void		WM_operator_properties_create(struct PointerRNA *ptr, const char *opstring);
void		WM_operator_properties_create_ptr(struct PointerRNA *ptr, struct wmOperatorType *ot);
void        WM_operator_properties_clear(struct PointerRNA *ptr);
void		WM_operator_properties_free(struct PointerRNA *ptr);
void		WM_operator_properties_filesel(struct wmOperatorType *ot, int filter, short type, short action, short flag, short display);
void        WM_operator_properties_border(struct wmOperatorType *ot);
void        WM_operator_properties_border_to_rcti(struct wmOperator *op, struct rcti *rect);
void        WM_operator_properties_border_to_rctf(struct wmOperator *op, rctf *rect);
void		WM_operator_properties_gesture_border(struct wmOperatorType *ot, bool extend);
void        WM_operator_properties_mouse_select(struct wmOperatorType *ot);
void		WM_operator_properties_gesture_straightline(struct wmOperatorType *ot, int cursor);
void		WM_operator_properties_select_all(struct wmOperatorType *ot);
void		WM_operator_properties_select_action(struct wmOperatorType *ot, int default_action);
void		WM_operator_properties_select_action_simple(struct wmOperatorType *ot, int default_action);

bool        WM_operator_check_ui_enabled(const struct bContext *C, const char *idname);
wmOperator *WM_operator_last_redo(const struct bContext *C);

bool        WM_operator_last_properties_init(struct wmOperator *op);
bool        WM_operator_last_properties_store(struct wmOperator *op);

/* MOVE THIS SOMEWHERE ELSE */
#define	SEL_TOGGLE		0
#define	SEL_SELECT		1
#define SEL_DESELECT	2
#define SEL_INVERT		3


/* flags for WM_operator_properties_filesel */
#define WM_FILESEL_RELPATH		(1 << 0)

#define WM_FILESEL_DIRECTORY	(1 << 1)
#define WM_FILESEL_FILENAME		(1 << 2)
#define WM_FILESEL_FILEPATH		(1 << 3)
#define WM_FILESEL_FILES		(1 << 4)


		/* operator as a python command (resultuing string must be freed) */
char		*WM_operator_pystring_ex(struct bContext *C, struct wmOperator *op,
                                     const bool all_args, const bool macro_args,
                                     struct wmOperatorType *ot, struct PointerRNA *opptr);
char		*WM_operator_pystring(struct bContext *C, struct wmOperator *op,
                                  const bool all_args, const bool macro_args);
bool         WM_operator_pystring_abbreviate(char *str, int str_len_max);
char		*WM_prop_pystring_assign(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int index);
void		WM_operator_bl_idname(char *to, const char *from);
void		WM_operator_py_idname(char *to, const char *from);

/* *************** uilist types ******************** */
void                WM_uilisttype_init(void);
struct uiListType  *WM_uilisttype_find(const char *idname, bool quiet);
bool                WM_uilisttype_add(struct uiListType *ult);
void                WM_uilisttype_freelink(struct uiListType *ult);
void                WM_uilisttype_free(void);

/* *************** menu types ******************** */
void                WM_menutype_init(void);
struct MenuType    *WM_menutype_find(const char *idname, bool quiet);
bool                WM_menutype_add(struct MenuType *mt);
void                WM_menutype_freelink(struct MenuType *mt);
void                WM_menutype_free(void);

			/* default operator callbacks for border/circle/lasso */
int			WM_border_select_invoke	(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int			WM_border_select_modal	(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
void		WM_border_select_cancel(struct bContext *C, struct wmOperator *op);
int			WM_gesture_circle_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int			WM_gesture_circle_modal(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
void		WM_gesture_circle_cancel(struct bContext *C, struct wmOperator *op);
int			WM_gesture_lines_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int			WM_gesture_lines_modal(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
void		WM_gesture_lines_cancel(struct bContext *C, struct wmOperator *op);
int			WM_gesture_lasso_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int			WM_gesture_lasso_modal(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
void		WM_gesture_lasso_cancel(struct bContext *C, struct wmOperator *op);
const int (*WM_gesture_lasso_path_to_array(struct bContext *C, struct wmOperator *op, int *mcords_tot))[2];
int			WM_gesture_straightline_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int			WM_gesture_straightline_modal(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
void		WM_gesture_straightline_cancel(struct bContext *C, struct wmOperator *op);

			/* Gesture manager API */
struct wmGesture *WM_gesture_new(struct bContext *C, const struct wmEvent *event, int type);
void		WM_gesture_end(struct bContext *C, struct wmGesture *gesture);
void		WM_gestures_remove(struct bContext *C);

			/* fileselecting support */
void		WM_event_add_fileselect(struct bContext *C, struct wmOperator *op);
void		WM_event_fileselect_event(struct wmWindowManager *wm, void *ophandle, int eventval);
#ifndef NDEBUG
void		WM_event_print(const struct wmEvent *event);
#endif

void		WM_operator_region_active_win_set(struct bContext *C);

			/* drag and drop */
struct wmDrag		*WM_event_start_drag(struct bContext *C, int icon, int type, void *poin, double value);
void				WM_event_drag_image(struct wmDrag *, struct ImBuf *, float scale, int sx, int sy);

struct wmDropBox	*WM_dropbox_add(ListBase *lb, const char *idname, int (*poll)(struct bContext *, struct wmDrag *, const struct wmEvent *event),
                                    void (*copy)(struct wmDrag *, struct wmDropBox *));
ListBase	*WM_dropboxmap_find(const char *idname, int spaceid, int regionid);

			/* Set a subwindow active in pixelspace view, with optional scissor subset */
void		wmSubWindowSet			(struct wmWindow *win, int swinid);
void		wmSubWindowScissorSet	(struct wmWindow *win, int swinid, const struct rcti *srct, bool srct_pad);

			/* OpenGL utilities with safety check + working in modelview matrix mode */
void		wmFrustum			(float x1, float x2, float y1, float y2, float n, float f);
void		wmOrtho				(float x1, float x2, float y1, float y2, float n, float f);
void		wmOrtho2			(float x1, float x2, float y1, float y2);

			/* utilities */
void		WM_framebuffer_index_set(int index);
int			WM_framebuffer_to_index(unsigned int col);

			/* threaded Jobs Manager */
enum {
	WM_JOB_PRIORITY     = (1 << 0),
	WM_JOB_EXCL_RENDER  = (1 << 1),
	WM_JOB_PROGRESS     = (1 << 2),
	WM_JOB_SUSPEND      = (1 << 3)
};

/* identifying jobs by owner alone is unreliable, this isnt saved, order can change (keep 0 for 'any') */
enum {
	WM_JOB_TYPE_ANY = 0,
	WM_JOB_TYPE_COMPOSITE,
	WM_JOB_TYPE_RENDER,
	WM_JOB_TYPE_RENDER_PREVIEW,  /* UI preview */
	WM_JOB_TYPE_SCREENCAST,
	WM_JOB_TYPE_OBJECT_SIM_OCEAN,
	WM_JOB_TYPE_OBJECT_SIM_FLUID,
	WM_JOB_TYPE_OBJECT_BAKE_TEXTURE,
	WM_JOB_TYPE_OBJECT_BAKE,
	WM_JOB_TYPE_FILESEL_THUMBNAIL,
	WM_JOB_TYPE_CLIP_BUILD_PROXY,
	WM_JOB_TYPE_CLIP_TRACK_MARKERS,
	WM_JOB_TYPE_CLIP_SOLVE_CAMERA,
	WM_JOB_TYPE_CLIP_PREFETCH,
	WM_JOB_TYPE_SEQ_BUILD_PROXY,
	/* add as needed, screencast, seq proxy build
	 * if having hard coded values is a problem */
};

struct wmJob *WM_jobs_get(struct wmWindowManager *wm, struct wmWindow *win, void *owner, const char *name, int flag, int job_type);

bool        WM_jobs_test(struct wmWindowManager *wm, void *owner, int job_type);
float		WM_jobs_progress(struct wmWindowManager *wm, void *owner);
char       *WM_jobs_name(struct wmWindowManager *wm, void *owner);
void       *WM_jobs_customdata(struct wmWindowManager *wm, void *owner);
void       *WM_jobs_customdata_from_type(struct wmWindowManager *wm, int job_type);

bool        WM_jobs_is_running(struct wmJob *);
void       *WM_jobs_customdata_get(struct wmJob *);
void        WM_jobs_customdata_set(struct wmJob *, void *customdata, void (*free)(void *));
void        WM_jobs_timer(struct wmJob *, double timestep, unsigned int note, unsigned int endnote);
void        WM_jobs_callbacks(struct wmJob *,
                              void (*startjob)(void *, short *, short *, float *),
                              void (*initjob)(void *),
                              void (*update)(void *),
                              void (*endjob)(void *));

void		WM_jobs_start(struct wmWindowManager *wm, struct wmJob *);
void		WM_jobs_stop(struct wmWindowManager *wm, void *owner, void *startjob);
void		WM_jobs_kill(struct wmWindowManager *wm, void *owner, void (*)(void *, short int *, short int *, float *));
void		WM_jobs_kill_all(struct wmWindowManager *wm);
void		WM_jobs_kill_all_except(struct wmWindowManager *wm, void *owner);
void		WM_jobs_kill_type(struct wmWindowManager *wm, void *owner, int job_type);

bool        WM_jobs_has_running(struct wmWindowManager *wm);

void		WM_job_main_thread_lock_acquire(struct wmJob *job);
void		WM_job_main_thread_lock_release(struct wmJob *job);

			/* clipboard */
char       *WM_clipboard_text_get(bool selection, int *r_len);
char       *WM_clipboard_text_get_firstline(bool selection, int *r_len);
void        WM_clipboard_text_set(const char *buf, bool selection);

			/* progress */
void		WM_progress_set(struct wmWindow *win, float progress);
void		WM_progress_clear(struct wmWindow *win);

			/* Draw (for screenshot) */
void		WM_redraw_windows(struct bContext *C);

void        WM_main_playanim(int argc, const char **argv);

/* debugging only, convenience function to write on crash */
bool write_crash_blend(void);

			/* Lock the interface for any communication */
void        WM_set_locked_interface(struct wmWindowManager *wm, bool lock);

void        WM_event_ndof_pan_get(const struct wmNDOFMotionData *ndof, float r_pan[3], const bool use_zoom);
void        WM_event_ndof_rotate_get(const struct wmNDOFMotionData *ndof, float r_rot[3]);

float       WM_event_ndof_to_axis_angle(const struct wmNDOFMotionData *ndof, float axis[3]);
void        WM_event_ndof_to_quat(const struct wmNDOFMotionData *ndof, float q[4]);

#ifdef __cplusplus
}
#endif

#endif /* __WM_API_H__ */

