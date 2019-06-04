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
#ifndef __WM_API_H__
#define __WM_API_H__

/** \file
 * \ingroup wm
 *
 * \page wmpage windowmanager
 * \section wmabout About windowmanager
 * \ref wm handles events received from \ref GHOST and manages
 * the screens, areas and input for Blender
 * \section wmnote NOTE
 * \todo document
 */

/* dna-savable wmStructs here */
#include "DNA_windowmanager_types.h"
#include "WM_keymap.h"
#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct GHashIterator;
struct GPUViewport;
struct IDProperty;
struct ImBuf;
struct ImageFormatData;
struct Main;
struct MenuType;
struct PointerRNA;
struct PropertyRNA;
struct ScrArea;
struct ViewLayer;
struct bContext;
struct bToolRef_Runtime;
struct rcti;
struct wmDrag;
struct wmDropBox;
struct wmEvent;
struct wmEventHandler;
struct wmEventHandler_Keymap;
struct wmEventHandler_UI;
struct wmGenericUserData;
struct wmGesture;
struct wmJob;
struct wmMsgSubscribeKey;
struct wmMsgSubscribeValue;
struct wmOperator;
struct wmOperatorType;
struct wmPaintCursor;

#ifdef WITH_INPUT_NDOF
struct wmNDOFMotionData;
#endif

typedef struct wmGizmo wmGizmo;
typedef struct wmGizmoMap wmGizmoMap;
typedef struct wmGizmoMapType wmGizmoMapType;
typedef struct wmJob wmJob;

/* general API */
void WM_init_state_app_template_set(const char *app_template);
const char *WM_init_state_app_template_get(void);

void WM_init_state_size_set(int stax, int stay, int sizx, int sizy);
void WM_init_state_fullscreen_set(void);
void WM_init_state_normal_set(void);
void WM_init_state_maximized_set(void);
void WM_init_state_start_with_console_set(bool value);
void WM_init_window_focus_set(bool do_it);
void WM_init_native_pixels(bool do_it);
void WM_init_tablet_api(void);

void WM_init(struct bContext *C, int argc, const char **argv);
void WM_exit_ext(struct bContext *C, const bool do_python);

void WM_exit(struct bContext *C) ATTR_NORETURN;

void WM_main(struct bContext *C) ATTR_NORETURN;

void WM_init_splash(struct bContext *C);

void WM_init_opengl(struct Main *bmain);

void WM_check(struct bContext *C);
void WM_reinit_gizmomap_all(struct Main *bmain);

int WM_window_pixels_x(const struct wmWindow *win);
int WM_window_pixels_y(const struct wmWindow *win);
void WM_window_rect_calc(const struct wmWindow *win, struct rcti *r_rect);
void WM_window_screen_rect_calc(const struct wmWindow *win, struct rcti *r_rect);
bool WM_window_is_fullscreen(struct wmWindow *win);

void WM_windows_scene_data_sync(const ListBase *win_lb, struct Scene *scene) ATTR_NONNULL();
struct Scene *WM_windows_scene_get_from_screen(const struct wmWindowManager *wm,
                                               const struct bScreen *screen)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
struct WorkSpace *WM_windows_workspace_get_from_screen(const wmWindowManager *wm,
                                                       const struct bScreen *screen)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

struct Scene *WM_window_get_active_scene(const struct wmWindow *win)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void WM_window_set_active_scene(struct Main *bmain,
                                struct bContext *C,
                                struct wmWindow *win,
                                struct Scene *scene_new) ATTR_NONNULL();
struct WorkSpace *WM_window_get_active_workspace(const struct wmWindow *win)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void WM_window_set_active_workspace(struct bContext *C,
                                    struct wmWindow *win,
                                    struct WorkSpace *workspace) ATTR_NONNULL(1);
struct WorkSpaceLayout *WM_window_get_active_layout(const struct wmWindow *win)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void WM_window_set_active_layout(struct wmWindow *win,
                                 struct WorkSpace *workspace,
                                 struct WorkSpaceLayout *layout) ATTR_NONNULL(1);
struct bScreen *WM_window_get_active_screen(const struct wmWindow *win)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void WM_window_set_active_screen(struct wmWindow *win,
                                 struct WorkSpace *workspace,
                                 struct bScreen *screen) ATTR_NONNULL(1);

struct ViewLayer *WM_window_get_active_view_layer(const struct wmWindow *win)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
void WM_window_set_active_view_layer(struct wmWindow *win, struct ViewLayer *view_layer)
    ATTR_NONNULL(1);
void WM_window_ensure_active_view_layer(struct wmWindow *win) ATTR_NONNULL(1);

bool WM_window_is_temp_screen(const struct wmWindow *win) ATTR_WARN_UNUSED_RESULT;

void *WM_opengl_context_create(void);
void WM_opengl_context_dispose(void *context);
void WM_opengl_context_activate(void *context);
void WM_opengl_context_release(void *context);

/* defines for 'type' WM_window_open_temp */
enum {
  WM_WINDOW_RENDER = 1,
  WM_WINDOW_USERPREFS,
  WM_WINDOW_DRIVERS,
  // WM_WINDOW_FILESEL // UNUSED
};

struct wmWindow *WM_window_open(struct bContext *C, const struct rcti *rect);
struct wmWindow *WM_window_open_temp(
    struct bContext *C, int x, int y, int sizex, int sizey, int type);
void WM_window_set_dpi(wmWindow *win);

bool WM_stereo3d_enabled(struct wmWindow *win, bool only_fullscreen_test);

/* files */
void WM_file_autoexec_init(const char *filepath);
bool WM_file_read(struct bContext *C, const char *filepath, struct ReportList *reports);
void WM_autosave_init(struct wmWindowManager *wm);
void WM_recover_last_session(struct bContext *C, struct ReportList *reports);
void WM_file_tag_modified(void);

void WM_lib_reload(struct Library *lib, struct bContext *C, struct ReportList *reports);

/* mouse cursors */
void WM_cursor_set(struct wmWindow *win, int curs);
bool WM_cursor_set_from_tool(struct wmWindow *win, const ScrArea *sa, const ARegion *ar);
void WM_cursor_modal_set(struct wmWindow *win, int curs);
void WM_cursor_modal_restore(struct wmWindow *win);
void WM_cursor_wait(bool val);
void WM_cursor_grab_enable(struct wmWindow *win, int wrap, bool hide, int bounds[4]);
void WM_cursor_grab_disable(struct wmWindow *win, const int mouse_ungrab_xy[2]);
void WM_cursor_time(struct wmWindow *win, int nr);

struct wmPaintCursor *WM_paint_cursor_activate(
    struct wmWindowManager *wm,
    short space_type,
    short region_type,
    bool (*poll)(struct bContext *C),
    void (*draw)(struct bContext *C, int, int, void *customdata),
    void *customdata);

bool WM_paint_cursor_end(struct wmWindowManager *wm, struct wmPaintCursor *handle);
void WM_paint_cursor_tag_redraw(struct wmWindow *win, struct ARegion *ar);

void WM_cursor_warp(struct wmWindow *win, int x, int y);
void WM_cursor_compatible_xy(wmWindow *win, int *x, int *y);
float WM_cursor_pressure(const struct wmWindow *win);

/* event map */
int WM_userdef_event_map(int kmitype);
int WM_userdef_event_type_from_keymap_type(int kmitype);

/* handlers */

typedef bool (*EventHandlerPoll)(const ARegion *ar, const struct wmEvent *event);
struct wmEventHandler_Keymap *WM_event_add_keymap_handler(ListBase *handlers, wmKeyMap *keymap);
struct wmEventHandler_Keymap *WM_event_add_keymap_handler_poll(ListBase *handlers,
                                                               wmKeyMap *keymap,
                                                               EventHandlerPoll poll);
struct wmEventHandler_Keymap *WM_event_add_keymap_handler_v2d_mask(ListBase *handlers,
                                                                   wmKeyMap *keymap);
/* priority not implemented, it adds in begin */
struct wmEventHandler_Keymap *WM_event_add_keymap_handler_priority(ListBase *handlers,
                                                                   wmKeyMap *keymap,
                                                                   int priority);

typedef struct wmKeyMap *(wmEventHandler_KeymapDynamicFn)(
    wmWindowManager *wm, struct wmEventHandler_Keymap *handler)ATTR_WARN_UNUSED_RESULT;

struct wmKeyMap *WM_event_get_keymap_from_toolsystem(struct wmWindowManager *wm,
                                                     struct wmEventHandler_Keymap *handler);

struct wmEventHandler_Keymap *WM_event_add_keymap_handler_dynamic(
    ListBase *handlers, wmEventHandler_KeymapDynamicFn *keymap_fn, void *user_data);

void WM_event_remove_keymap_handler(ListBase *handlers, wmKeyMap *keymap);

void WM_event_set_keymap_handler_post_callback(struct wmEventHandler_Keymap *handler,
                                               void(keymap_tag)(wmKeyMap *keymap,
                                                                wmKeyMapItem *kmi,
                                                                void *user_data),
                                               void *user_data);
wmKeyMap *WM_event_get_keymap_from_handler(wmWindowManager *wm,
                                           struct wmEventHandler_Keymap *handler);

typedef int (*wmUIHandlerFunc)(struct bContext *C, const struct wmEvent *event, void *userdata);
typedef void (*wmUIHandlerRemoveFunc)(struct bContext *C, void *userdata);

struct wmEventHandler_UI *WM_event_add_ui_handler(const struct bContext *C,
                                                  ListBase *handlers,
                                                  wmUIHandlerFunc ui_handle,
                                                  wmUIHandlerRemoveFunc ui_remove,
                                                  void *userdata,
                                                  const char flag);
void WM_event_remove_ui_handler(ListBase *handlers,
                                wmUIHandlerFunc ui_handle,
                                wmUIHandlerRemoveFunc ui_remove,
                                void *userdata,
                                const bool postpone);
void WM_event_remove_area_handler(struct ListBase *handlers, void *area);
void WM_event_free_ui_handler_all(struct bContext *C,
                                  ListBase *handlers,
                                  wmUIHandlerFunc ui_handle,
                                  wmUIHandlerRemoveFunc ui_remove);

struct wmEventHandler_Op *WM_event_add_modal_handler(struct bContext *C, struct wmOperator *op);
void WM_event_modal_handler_area_replace(wmWindow *win,
                                         const struct ScrArea *old_area,
                                         struct ScrArea *new_area);
void WM_event_modal_handler_region_replace(wmWindow *win,
                                           const struct ARegion *old_region,
                                           struct ARegion *new_region);

void WM_event_remove_handlers(struct bContext *C, ListBase *handlers);

/* handler flag */
enum {
  WM_HANDLER_BLOCKING = (1 << 0),         /* after this handler all others are ignored */
  WM_HANDLER_ACCEPT_DBL_CLICK = (1 << 1), /* handler accepts double key press events */

  /* internal */
  WM_HANDLER_DO_FREE = (1 << 7), /* handler tagged to be freed in wm_handlers_do() */
};

struct wmEventHandler_Dropbox *WM_event_add_dropbox_handler(ListBase *handlers,
                                                            ListBase *dropboxes);

/* mouse */
void WM_event_add_mousemove(const struct bContext *C);
bool WM_event_is_modal_tweak_exit(const struct wmEvent *event, int tweak_event);
bool WM_event_is_last_mousemove(const struct wmEvent *event);

#ifdef WITH_INPUT_NDOF
/* 3D mouse */
void WM_ndof_deadzone_set(float deadzone);
#endif
/* notifiers */
void WM_event_add_notifier(const struct bContext *C, unsigned int type, void *reference);
void WM_main_add_notifier(unsigned int type, void *reference);
void WM_main_remove_notifier_reference(const void *reference);
void WM_main_remap_editor_id_reference(struct ID *old_id, struct ID *new_id);

/* reports */
void WM_report_banner_show(void);
void WM_report(ReportType type, const char *message);
void WM_reportf(ReportType type, const char *format, ...) ATTR_PRINTF_FORMAT(2, 3);

struct wmEvent *wm_event_add_ex(struct wmWindow *win,
                                const struct wmEvent *event_to_add,
                                const struct wmEvent *event_to_add_after) ATTR_NONNULL(1, 2);
struct wmEvent *wm_event_add(struct wmWindow *win, const struct wmEvent *event_to_add)
    ATTR_NONNULL(1, 2);

void wm_event_init_from_window(struct wmWindow *win, struct wmEvent *event);

/* at maximum, every timestep seconds it triggers event_type events */
struct wmTimer *WM_event_add_timer(struct wmWindowManager *wm,
                                   struct wmWindow *win,
                                   int event_type,
                                   double timestep);
struct wmTimer *WM_event_add_timer_notifier(struct wmWindowManager *wm,
                                            struct wmWindow *win,
                                            unsigned int type,
                                            double timestep);
void WM_event_remove_timer(struct wmWindowManager *wm,
                           struct wmWindow *win,
                           struct wmTimer *timer);
void WM_event_remove_timer_notifier(struct wmWindowManager *wm,
                                    struct wmWindow *win,
                                    struct wmTimer *timer);
void WM_event_timer_sleep(struct wmWindowManager *wm,
                          struct wmWindow *win,
                          struct wmTimer *timer,
                          bool do_sleep);

/* operator api, default callbacks */
/* invoke callback, uses enum property named "type" */
void WM_operator_view3d_unit_defaults(struct bContext *C, struct wmOperator *op);
int WM_operator_smooth_viewtx_get(const struct wmOperator *op);
int WM_menu_invoke_ex(struct bContext *C, struct wmOperator *op, int opcontext);
int WM_menu_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
void WM_menu_name_call(struct bContext *C, const char *menu_name, short context);
int WM_enum_search_invoke_previews(struct bContext *C,
                                   struct wmOperator *op,
                                   short prv_cols,
                                   short prv_rows);
int WM_enum_search_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
/* invoke callback, confirm menu + exec */
int WM_operator_confirm(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int WM_operator_confirm_or_exec(struct bContext *C,
                                struct wmOperator *op,
                                const struct wmEvent *event);
/* invoke callback, file selector "filepath" unset + exec */
int WM_operator_filesel(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
bool WM_operator_filesel_ensure_ext_imtype(wmOperator *op,
                                           const struct ImageFormatData *im_format);
/* poll callback, context checks */
bool WM_operator_winactive(struct bContext *C);
/* invoke callback, exec + redo popup */
int WM_operator_props_popup_confirm(struct bContext *C,
                                    struct wmOperator *op,
                                    const struct wmEvent *event);
int WM_operator_props_popup_call(struct bContext *C,
                                 struct wmOperator *op,
                                 const struct wmEvent *event);
int WM_operator_props_popup(struct bContext *C,
                            struct wmOperator *op,
                            const struct wmEvent *event);
int WM_operator_props_dialog_popup(struct bContext *C,
                                   struct wmOperator *op,
                                   int width,
                                   int height);
int WM_operator_redo_popup(struct bContext *C, struct wmOperator *op);
int WM_operator_ui_popup(struct bContext *C, struct wmOperator *op, int width, int height);

int WM_operator_confirm_message_ex(struct bContext *C,
                                   struct wmOperator *op,
                                   const char *title,
                                   const int icon,
                                   const char *message,
                                   const short opcontext);
int WM_operator_confirm_message(struct bContext *C, struct wmOperator *op, const char *message);

/* operator api */
void WM_operator_free(struct wmOperator *op);
void WM_operator_free_all_after(wmWindowManager *wm, struct wmOperator *op);
void WM_operator_type_set(struct wmOperator *op, struct wmOperatorType *ot);
void WM_operator_stack_clear(struct wmWindowManager *wm);
void WM_operator_handlers_clear(wmWindowManager *wm, struct wmOperatorType *ot);

bool WM_operator_poll(struct bContext *C, struct wmOperatorType *ot);
bool WM_operator_poll_context(struct bContext *C, struct wmOperatorType *ot, short context);
int WM_operator_call_ex(struct bContext *C, struct wmOperator *op, const bool store);
int WM_operator_call(struct bContext *C, struct wmOperator *op);
int WM_operator_call_notest(struct bContext *C, struct wmOperator *op);
int WM_operator_repeat(struct bContext *C, struct wmOperator *op);
int WM_operator_repeat_interactive(struct bContext *C, struct wmOperator *op);
bool WM_operator_repeat_check(const struct bContext *C, struct wmOperator *op);
bool WM_operator_is_repeat(const struct bContext *C, const struct wmOperator *op);
int WM_operator_name_call_ptr(struct bContext *C,
                              struct wmOperatorType *ot,
                              short context,
                              struct PointerRNA *properties);
int WM_operator_name_call(struct bContext *C,
                          const char *opstring,
                          short context,
                          struct PointerRNA *properties);
int WM_operator_name_call_with_properties(struct bContext *C,
                                          const char *opstring,
                                          short context,
                                          struct IDProperty *properties);
int WM_operator_call_py(struct bContext *C,
                        struct wmOperatorType *ot,
                        short context,
                        struct PointerRNA *properties,
                        struct ReportList *reports,
                        const bool is_undo);

void WM_operator_properties_alloc(struct PointerRNA **ptr,
                                  struct IDProperty **properties,
                                  const char *opstring); /* used for keymap and macro items */
void WM_operator_properties_sanitize(
    struct PointerRNA *ptr, const bool no_context); /* make props context sensitive or not */
bool WM_operator_properties_default(struct PointerRNA *ptr, const bool do_update);
void WM_operator_properties_reset(struct wmOperator *op);
void WM_operator_properties_create(struct PointerRNA *ptr, const char *opstring);
void WM_operator_properties_create_ptr(struct PointerRNA *ptr, struct wmOperatorType *ot);
void WM_operator_properties_clear(struct PointerRNA *ptr);
void WM_operator_properties_free(struct PointerRNA *ptr);

bool WM_operator_check_ui_empty(struct wmOperatorType *ot);
bool WM_operator_check_ui_enabled(const struct bContext *C, const char *idname);

IDProperty *WM_operator_last_properties_ensure_idprops(struct wmOperatorType *ot);
void WM_operator_last_properties_ensure(struct wmOperatorType *ot, struct PointerRNA *ptr);
wmOperator *WM_operator_last_redo(const struct bContext *C);
ID *WM_operator_drop_load_path(struct bContext *C, struct wmOperator *op, const short idcode);

bool WM_operator_last_properties_init(struct wmOperator *op);
bool WM_operator_last_properties_store(struct wmOperator *op);

/* wm_operator_props.c */
void WM_operator_properties_confirm_or_exec(struct wmOperatorType *ot);
void WM_operator_properties_filesel(struct wmOperatorType *ot,
                                    int filter,
                                    short type,
                                    short action,
                                    short flag,
                                    short display,
                                    short sort);
void WM_operator_properties_use_cursor_init(struct wmOperatorType *ot);
void WM_operator_properties_border(struct wmOperatorType *ot);
void WM_operator_properties_border_to_rcti(struct wmOperator *op, struct rcti *rect);
void WM_operator_properties_border_to_rctf(struct wmOperator *op, rctf *rect);
void WM_operator_properties_gesture_box_ex(struct wmOperatorType *ot, bool deselect, bool extend);
void WM_operator_properties_gesture_box(struct wmOperatorType *ot);
void WM_operator_properties_gesture_box_select(struct wmOperatorType *ot);
void WM_operator_properties_gesture_box_zoom(struct wmOperatorType *ot);
void WM_operator_properties_gesture_lasso(struct wmOperatorType *ot);
void WM_operator_properties_gesture_straightline(struct wmOperatorType *ot, int cursor);
void WM_operator_properties_gesture_circle(struct wmOperatorType *ot);
void WM_operator_properties_mouse_select(struct wmOperatorType *ot);
void WM_operator_properties_select_all(struct wmOperatorType *ot);
void WM_operator_properties_select_action(struct wmOperatorType *ot,
                                          int default_action,
                                          bool hide_gui);
void WM_operator_properties_select_action_simple(struct wmOperatorType *ot,
                                                 int default_action,
                                                 bool hide_gui);
void WM_operator_properties_select_random(struct wmOperatorType *ot);
int WM_operator_properties_select_random_seed_increment_get(wmOperator *op);
void WM_operator_properties_select_operation(struct wmOperatorType *ot);
void WM_operator_properties_select_operation_simple(struct wmOperatorType *ot);
struct CheckerIntervalParams {
  int nth; /* bypass when set to zero */
  int skip;
  int offset;
};
void WM_operator_properties_checker_interval(struct wmOperatorType *ot, bool nth_can_disable);
void WM_operator_properties_checker_interval_from_op(struct wmOperator *op,
                                                     struct CheckerIntervalParams *op_params);
bool WM_operator_properties_checker_interval_test(const struct CheckerIntervalParams *op_params,
                                                  int depth);

/* flags for WM_operator_properties_filesel */
#define WM_FILESEL_RELPATH (1 << 0)

#define WM_FILESEL_DIRECTORY (1 << 1)
#define WM_FILESEL_FILENAME (1 << 2)
#define WM_FILESEL_FILEPATH (1 << 3)
#define WM_FILESEL_FILES (1 << 4)

/* operator as a python command (resultuing string must be freed) */
char *WM_operator_pystring_ex(struct bContext *C,
                              struct wmOperator *op,
                              const bool all_args,
                              const bool macro_args,
                              struct wmOperatorType *ot,
                              struct PointerRNA *opptr);
char *WM_operator_pystring(struct bContext *C,
                           struct wmOperator *op,
                           const bool all_args,
                           const bool macro_args);
bool WM_operator_pystring_abbreviate(char *str, int str_len_max);
char *WM_prop_pystring_assign(struct bContext *C,
                              struct PointerRNA *ptr,
                              struct PropertyRNA *prop,
                              int index);
void WM_operator_bl_idname(char *to, const char *from);
void WM_operator_py_idname(char *to, const char *from);
bool WM_operator_py_idname_ok_or_report(struct ReportList *reports,
                                        const char *classname,
                                        const char *idname);
const char *WM_context_member_from_ptr(struct bContext *C, const struct PointerRNA *ptr);

/* wm_operator_type.c */
struct wmOperatorType *WM_operatortype_find(const char *idname, bool quiet);
void WM_operatortype_iter(struct GHashIterator *ghi);
void WM_operatortype_append(void (*opfunc)(struct wmOperatorType *));
void WM_operatortype_append_ptr(void (*opfunc)(struct wmOperatorType *, void *), void *userdata);
void WM_operatortype_append_macro_ptr(void (*opfunc)(struct wmOperatorType *, void *),
                                      void *userdata);
void WM_operatortype_remove_ptr(struct wmOperatorType *ot);
bool WM_operatortype_remove(const char *idname);
void WM_operatortype_last_properties_clear_all(void);
void WM_operatortype_props_advanced_begin(struct wmOperatorType *ot);
void WM_operatortype_props_advanced_end(struct wmOperatorType *ot);

#define WM_operatortype_prop_tag(property, tags) \
  { \
    CHECK_TYPE(tags, eOperatorPropTags); \
    RNA_def_property_tags(prop, tags); \
  } \
  (void)0

struct wmOperatorType *WM_operatortype_append_macro(const char *idname,
                                                    const char *name,
                                                    const char *description,
                                                    int flag);
struct wmOperatorTypeMacro *WM_operatortype_macro_define(struct wmOperatorType *ot,
                                                         const char *idname);

const char *WM_operatortype_name(struct wmOperatorType *ot, struct PointerRNA *properties);

/* wm_uilist_type.c */
void WM_uilisttype_init(void);
struct uiListType *WM_uilisttype_find(const char *idname, bool quiet);
bool WM_uilisttype_add(struct uiListType *ult);
void WM_uilisttype_freelink(struct uiListType *ult);
void WM_uilisttype_free(void);

/* wm_menu_type.c */
void WM_menutype_init(void);
struct MenuType *WM_menutype_find(const char *idname, bool quiet);
bool WM_menutype_add(struct MenuType *mt);
void WM_menutype_freelink(struct MenuType *mt);
void WM_menutype_free(void);
bool WM_menutype_poll(struct bContext *C, struct MenuType *mt);

/* wm_panel_type.c */
void WM_paneltype_init(void);
void WM_paneltype_clear(void);
struct PanelType *WM_paneltype_find(const char *idname, bool quiet);
bool WM_paneltype_add(struct PanelType *mt);
void WM_paneltype_remove(struct PanelType *mt);

/* wm_gesture_ops.c */
int WM_gesture_box_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int WM_gesture_box_modal(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
void WM_gesture_box_cancel(struct bContext *C, struct wmOperator *op);
int WM_gesture_circle_invoke(struct bContext *C,
                             struct wmOperator *op,
                             const struct wmEvent *event);
int WM_gesture_circle_modal(struct bContext *C,
                            struct wmOperator *op,
                            const struct wmEvent *event);
void WM_gesture_circle_cancel(struct bContext *C, struct wmOperator *op);
int WM_gesture_lines_invoke(struct bContext *C,
                            struct wmOperator *op,
                            const struct wmEvent *event);
int WM_gesture_lines_modal(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
void WM_gesture_lines_cancel(struct bContext *C, struct wmOperator *op);
int WM_gesture_lasso_invoke(struct bContext *C,
                            struct wmOperator *op,
                            const struct wmEvent *event);
int WM_gesture_lasso_modal(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
void WM_gesture_lasso_cancel(struct bContext *C, struct wmOperator *op);
const int (*WM_gesture_lasso_path_to_array(struct bContext *C,
                                           struct wmOperator *op,
                                           int *mcords_tot))[2];
int WM_gesture_straightline_invoke(struct bContext *C,
                                   struct wmOperator *op,
                                   const struct wmEvent *event);
int WM_gesture_straightline_modal(struct bContext *C,
                                  struct wmOperator *op,
                                  const struct wmEvent *event);
void WM_gesture_straightline_cancel(struct bContext *C, struct wmOperator *op);

/* Gesture manager API */
struct wmGesture *WM_gesture_new(struct bContext *C, const struct wmEvent *event, int type);
void WM_gesture_end(struct bContext *C, struct wmGesture *gesture);
void WM_gestures_remove(struct bContext *C);
bool WM_gesture_is_modal_first(const struct wmGesture *gesture);

/* fileselecting support */
void WM_event_add_fileselect(struct bContext *C, struct wmOperator *op);
void WM_event_fileselect_event(struct wmWindowManager *wm, void *ophandle, int eventval);
int WM_event_modifier_flag(const struct wmEvent *event);
void WM_event_print(const struct wmEvent *event);

void WM_operator_region_active_win_set(struct bContext *C);

int WM_event_drag_threshold(const struct wmEvent *event);
bool WM_event_drag_test(const struct wmEvent *event, const int prev_xy[2]);
bool WM_event_drag_test_with_delta(const struct wmEvent *event, const int delta[2]);

/* drag and drop */
struct wmDrag *WM_event_start_drag(
    struct bContext *C, int icon, int type, void *poin, double value, unsigned int flags);
void WM_event_drag_image(struct wmDrag *, struct ImBuf *, float scale, int sx, int sy);
void WM_drag_free(struct wmDrag *drag);
void WM_drag_free_list(struct ListBase *lb);

struct wmDropBox *WM_dropbox_add(
    ListBase *lb,
    const char *idname,
    bool (*poll)(struct bContext *, struct wmDrag *, const struct wmEvent *event, const char **),
    void (*copy)(struct wmDrag *, struct wmDropBox *));
ListBase *WM_dropboxmap_find(const char *idname, int spaceid, int regionid);

/* ID drag and drop */
void WM_drag_add_ID(struct wmDrag *drag, struct ID *id, struct ID *from_parent);
struct ID *WM_drag_ID(const struct wmDrag *drag, short idcode);
struct ID *WM_drag_ID_from_event(const struct wmEvent *event, short idcode);

/* Set OpenGL viewport and scissor */
void wmViewport(const struct rcti *rect);
void wmPartialViewport(rcti *drawrct, const rcti *winrct, const rcti *partialrct);
void wmWindowViewport(struct wmWindow *win);

/* OpenGL utilities with safety check */
void wmOrtho2(float x1, float x2, float y1, float y2);
/* use for conventions (avoid hard-coded offsets all over) */
void wmOrtho2_region_pixelspace(const struct ARegion *ar);
void wmOrtho2_pixelspace(const float x, const float y);
void wmGetProjectionMatrix(float mat[4][4], const struct rcti *winrct);

/* threaded Jobs Manager */
enum {
  WM_JOB_PRIORITY = (1 << 0),
  WM_JOB_EXCL_RENDER = (1 << 1),
  WM_JOB_PROGRESS = (1 << 2),
  WM_JOB_SUSPEND = (1 << 3),
};

/** Identifying jobs by owner alone is unreliable, this isnt saved,
 * order can change (keep 0 for 'any'). */
enum {
  WM_JOB_TYPE_ANY = 0,
  WM_JOB_TYPE_COMPOSITE,
  WM_JOB_TYPE_RENDER,
  WM_JOB_TYPE_RENDER_PREVIEW, /* UI preview */
  WM_JOB_TYPE_OBJECT_SIM_OCEAN,
  WM_JOB_TYPE_OBJECT_SIM_FLUID,
  WM_JOB_TYPE_OBJECT_BAKE_TEXTURE,
  WM_JOB_TYPE_OBJECT_BAKE,
  WM_JOB_TYPE_FILESEL_READDIR,
  WM_JOB_TYPE_CLIP_BUILD_PROXY,
  WM_JOB_TYPE_CLIP_TRACK_MARKERS,
  WM_JOB_TYPE_CLIP_SOLVE_CAMERA,
  WM_JOB_TYPE_CLIP_PREFETCH,
  WM_JOB_TYPE_SEQ_BUILD_PROXY,
  WM_JOB_TYPE_SEQ_BUILD_PREVIEW,
  WM_JOB_TYPE_POINTCACHE,
  WM_JOB_TYPE_DPAINT_BAKE,
  WM_JOB_TYPE_ALEMBIC,
  WM_JOB_TYPE_SHADER_COMPILATION,
  WM_JOB_TYPE_STUDIOLIGHT,
  WM_JOB_TYPE_LIGHT_BAKE,
  WM_JOB_TYPE_FSMENU_BOOKMARK_VALIDATE,
  /* add as needed, screencast, seq proxy build
   * if having hard coded values is a problem */
};

struct wmJob *WM_jobs_get(struct wmWindowManager *wm,
                          struct wmWindow *win,
                          void *owner,
                          const char *name,
                          int flag,
                          int job_type);

bool WM_jobs_test(struct wmWindowManager *wm, void *owner, int job_type);
float WM_jobs_progress(struct wmWindowManager *wm, void *owner);
char *WM_jobs_name(struct wmWindowManager *wm, void *owner);
double WM_jobs_starttime(struct wmWindowManager *wm, void *owner);
void *WM_jobs_customdata(struct wmWindowManager *wm, void *owner);
void *WM_jobs_customdata_from_type(struct wmWindowManager *wm, int job_type);

bool WM_jobs_is_running(struct wmJob *);
bool WM_jobs_is_stopped(wmWindowManager *wm, void *owner);
void *WM_jobs_customdata_get(struct wmJob *);
void WM_jobs_customdata_set(struct wmJob *, void *customdata, void (*free)(void *));
void WM_jobs_timer(struct wmJob *, double timestep, unsigned int note, unsigned int endnote);
void WM_jobs_callbacks(struct wmJob *,
                       void (*startjob)(void *, short *, short *, float *),
                       void (*initjob)(void *),
                       void (*update)(void *),
                       void (*endjob)(void *));

void WM_jobs_start(struct wmWindowManager *wm, struct wmJob *);
void WM_jobs_stop(struct wmWindowManager *wm, void *owner, void *startjob);
void WM_jobs_kill(struct wmWindowManager *wm,
                  void *owner,
                  void (*)(void *, short int *, short int *, float *));
void WM_jobs_kill_all(struct wmWindowManager *wm);
void WM_jobs_kill_all_except(struct wmWindowManager *wm, void *owner);
void WM_jobs_kill_type(struct wmWindowManager *wm, void *owner, int job_type);

bool WM_jobs_has_running(struct wmWindowManager *wm);

void WM_job_main_thread_lock_acquire(struct wmJob *job);
void WM_job_main_thread_lock_release(struct wmJob *job);

/* clipboard */
char *WM_clipboard_text_get(bool selection, int *r_len);
char *WM_clipboard_text_get_firstline(bool selection, int *r_len);
void WM_clipboard_text_set(const char *buf, bool selection);

/* progress */
void WM_progress_set(struct wmWindow *win, float progress);
void WM_progress_clear(struct wmWindow *win);

/* Draw (for screenshot) */
void *WM_draw_cb_activate(struct wmWindow *win,
                          void (*draw)(const struct wmWindow *, void *),
                          void *customdata);
void WM_draw_cb_exit(struct wmWindow *win, void *handle);
void WM_redraw_windows(struct bContext *C);

/* Region drawing */
void WM_draw_region_free(struct ARegion *ar);
struct GPUViewport *WM_draw_region_get_viewport(struct ARegion *ar, int view);
struct GPUViewport *WM_draw_region_get_bound_viewport(struct ARegion *ar);

void WM_main_playanim(int argc, const char **argv);

/* debugging only, convenience function to write on crash */
bool write_crash_blend(void);

/* Lock the interface for any communication */
void WM_set_locked_interface(struct wmWindowManager *wm, bool lock);

#ifdef WITH_INPUT_NDOF
void WM_event_ndof_pan_get(const struct wmNDOFMotionData *ndof,
                           float r_pan[3],
                           const bool use_zoom);
void WM_event_ndof_rotate_get(const struct wmNDOFMotionData *ndof, float r_rot[3]);

float WM_event_ndof_to_axis_angle(const struct wmNDOFMotionData *ndof, float axis[3]);
void WM_event_ndof_to_quat(const struct wmNDOFMotionData *ndof, float q[4]);
#endif /* WITH_INPUT_NDOF */

float WM_event_tablet_data(const struct wmEvent *event, int *pen_flip, float tilt[2]);
bool WM_event_is_tablet(const struct wmEvent *event);

#ifdef WITH_INPUT_IME
bool WM_event_is_ime_switch(const struct wmEvent *event);
#endif

/* For testing only 'G_FLAG_EVENT_SIMULATE' */
struct wmEvent *WM_event_add_simulate(struct wmWindow *win, const struct wmEvent *event_to_add);

const char *WM_window_cursor_keymap_status_get(const struct wmWindow *win,
                                               int button_index,
                                               int type_index);
void WM_window_cursor_keymap_status_refresh(struct bContext *C, struct wmWindow *win);

void WM_window_status_area_tag_redraw(struct wmWindow *win);
struct ScrArea *WM_window_status_area_find(struct wmWindow *win, struct bScreen *sc);
bool WM_window_modal_keymap_status_draw(struct bContext *C,
                                        struct wmWindow *win,
                                        struct uiLayout *layout);

/* wm_tooltip.c */
typedef struct ARegion *(*wmTooltipInitFn)(struct bContext *C,
                                           struct ARegion *ar,
                                           int *pass,
                                           double *r_pass_delay,
                                           bool *r_exit_on_event);

void WM_tooltip_immediate_init(struct bContext *C,
                               struct wmWindow *win,
                               struct ARegion *ar,
                               wmTooltipInitFn init);
void WM_tooltip_timer_init_ex(struct bContext *C,
                              struct wmWindow *win,
                              struct ARegion *ar,
                              wmTooltipInitFn init,
                              double delay);
void WM_tooltip_timer_init(struct bContext *C,
                           struct wmWindow *win,
                           struct ARegion *ar,
                           wmTooltipInitFn init);
void WM_tooltip_timer_clear(struct bContext *C, struct wmWindow *win);
void WM_tooltip_clear(struct bContext *C, struct wmWindow *win);
void WM_tooltip_init(struct bContext *C, struct wmWindow *win);
void WM_tooltip_refresh(struct bContext *C, struct wmWindow *win);
double WM_tooltip_time_closed(void);

/* wm_utils.c */
struct wmGenericCallback *WM_generic_callback_steal(struct wmGenericCallback *callback);
void WM_generic_callback_free(struct wmGenericCallback *callback);

void WM_generic_user_data_free(struct wmGenericUserData *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __WM_API_H__ */
