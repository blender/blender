/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation. All rights reserved. */
#pragma once

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
#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"
#include "DNA_windowmanager_types.h"
#include "WM_keymap.h"
#include "WM_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct AssetHandle;
struct AssetLibraryReference;
struct GHashIterator;
struct GPUViewport;
struct ID;
struct IDProperty;
struct IDRemapper;
struct ImBuf;
struct ImageFormatData;
struct Main;
struct MenuType;
struct PointerRNA;
struct PropertyRNA;
struct ScrArea;
struct SelectPick_Params;
struct View3D;
struct ViewLayer;
struct bContext;
struct rcti;
struct wmDrag;
struct wmDropBox;
struct wmEvent;
struct wmEventHandler_Keymap;
struct wmEventHandler_UI;
struct wmGenericUserData;
struct wmGesture;
struct wmJob;
struct wmOperator;
struct wmOperatorType;
struct wmPaintCursor;
struct wmTabletData;

#ifdef WITH_INPUT_NDOF
struct wmNDOFMotionData;
#endif

#ifdef WITH_XR_OPENXR
struct wmXrRuntimeData;
#endif

typedef struct wmGizmo wmGizmo;
typedef struct wmGizmoMap wmGizmoMap;
typedef struct wmGizmoMapType wmGizmoMapType;
typedef struct wmJob wmJob;

/* General API. */

/**
 * Used for setting app-template from the command line:
 * - non-empty string: overrides.
 * - empty string: override, using no app template.
 * - NULL: clears override.
 */
void WM_init_state_app_template_set(const char *app_template);
const char *WM_init_state_app_template_get(void);

/**
 * Called when no ghost system was initialized.
 */
void WM_init_state_size_set(int stax, int stay, int sizx, int sizy);
/**
 * For border-less and border windows set from command-line.
 */
void WM_init_state_fullscreen_set(void);
void WM_init_state_normal_set(void);
void WM_init_state_maximized_set(void);
void WM_init_state_start_with_console_set(bool value);
void WM_init_window_focus_set(bool do_it);
void WM_init_native_pixels(bool do_it);
void WM_init_tablet_api(void);

/**
 * Initialize Blender and load the startup file & preferences
 * (only called once).
 */
void WM_init(struct bContext *C, int argc, const char **argv);
/**
 * \note doesn't run exit() call #WM_exit() for that.
 */
void WM_exit_ex(struct bContext *C, bool do_python);

/**
 * \brief Main exit function to close Blender ordinarily.
 * \note Use #wm_exit_schedule_delayed() to close Blender from an operator.
 * Might leak memory otherwise.
 */
void WM_exit(struct bContext *C) ATTR_NORETURN;

void WM_main(struct bContext *C) ATTR_NORETURN;

void WM_init_splash(struct bContext *C);

void WM_init_opengl(void);

void WM_check(struct bContext *C);
void WM_reinit_gizmomap_all(struct Main *bmain);

/**
 * Needed for cases when operators are re-registered
 * (when operator type pointers are stored).
 */
void WM_script_tag_reload(void);

wmWindow *WM_window_find_under_cursor(wmWindow *win, const int mval[2], int r_mval[2]);
void WM_window_pixel_sample_read(const wmWindowManager *wm,
                                 const wmWindow *win,
                                 const int pos[2],
                                 float r_col[3]);

uint *WM_window_pixels_read(struct wmWindowManager *wm, struct wmWindow *win, int r_size[2]);

/**
 * Support for native pixel size
 *
 * \note macOS retina opens window in size X, but it has up to 2 x more pixels.
 */
int WM_window_pixels_x(const struct wmWindow *win);
int WM_window_pixels_y(const struct wmWindow *win);
/**
 * Get boundaries usable by all window contents, including global areas.
 */
void WM_window_rect_calc(const struct wmWindow *win, struct rcti *r_rect);
/**
 * Get boundaries usable by screen-layouts, excluding global areas.
 * \note Depends on #U.dpi_fac. Should that be outdated, call #WM_window_set_dpi first.
 */
void WM_window_screen_rect_calc(const struct wmWindow *win, struct rcti *r_rect);
bool WM_window_is_fullscreen(const struct wmWindow *win);
bool WM_window_is_maximized(const struct wmWindow *win);

/**
 * Some editor data may need to be synced with scene data (3D View camera and layers).
 * This function ensures data is synced for editors
 * in visible work-spaces and their visible layouts.
 */
void WM_windows_scene_data_sync(const ListBase *win_lb, struct Scene *scene) ATTR_NONNULL();
struct Scene *WM_windows_scene_get_from_screen(const struct wmWindowManager *wm,
                                               const struct bScreen *screen)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
struct ViewLayer *WM_windows_view_layer_get_from_screen(const struct wmWindowManager *wm,
                                                        const struct bScreen *screen)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
struct WorkSpace *WM_windows_workspace_get_from_screen(const wmWindowManager *wm,
                                                       const struct bScreen *screen)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

struct Scene *WM_window_get_active_scene(const struct wmWindow *win)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
/**
 * \warning Only call outside of area/region loops.
 */
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
/**
 * Get the active screen of the active workspace in \a win.
 */
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

/* #WM_window_open alignment */
typedef enum eWindowAlignment {
  WIN_ALIGN_ABSOLUTE = 0,
  WIN_ALIGN_LOCATION_CENTER,
  WIN_ALIGN_PARENT_CENTER,
} eWindowAlignment;

/**
 * \param space_type: SPACE_VIEW3D, SPACE_INFO, ... (eSpace_Type)
 * \param toplevel: Not a child owned by other windows. A peer of main window.
 * \param dialog: whether this should be made as a dialog-style window
 * \param temp: whether this is considered a short-lived window
 * \param alignment: how this window is positioned relative to its parent
 * \return the window or NULL in case of failure.
 */
struct wmWindow *WM_window_open(struct bContext *C,
                                const char *title,
                                int x,
                                int y,
                                int sizex,
                                int sizey,
                                int space_type,
                                bool toplevel,
                                bool dialog,
                                bool temp,
                                eWindowAlignment alignment);

void WM_window_set_dpi(const wmWindow *win);

bool WM_stereo3d_enabled(struct wmWindow *win, bool only_fullscreen_test);

/* wm_files.c */

void WM_file_autoexec_init(const char *filepath);
bool WM_file_read(struct bContext *C, const char *filepath, struct ReportList *reports);
void WM_file_autosave_init(struct wmWindowManager *wm);
bool WM_file_recover_last_session(struct bContext *C, struct ReportList *reports);
void WM_file_tag_modified(void);

/**
 * \note `scene` (and related `view_layer` and `v3d`) pointers may be NULL,
 * in which case no instantiation of linked objects, collections etc. will be performed.
 */
struct ID *WM_file_link_datablock(struct Main *bmain,
                                  struct Scene *scene,
                                  struct ViewLayer *view_layer,
                                  struct View3D *v3d,
                                  const char *filepath,
                                  short id_code,
                                  const char *id_name,
                                  int flag);
/**
 * \note `scene` (and related `view_layer` and `v3d`) pointers may be NULL,
 * in which case no instantiation of appended objects, collections etc. will be performed.
 */
struct ID *WM_file_append_datablock(struct Main *bmain,
                                    struct Scene *scene,
                                    struct ViewLayer *view_layer,
                                    struct View3D *v3d,
                                    const char *filepath,
                                    short id_code,
                                    const char *id_name,
                                    int flag);
void WM_lib_reload(struct Library *lib, struct bContext *C, struct ReportList *reports);

/* Mouse cursors. */

void WM_cursor_set(struct wmWindow *win, int curs);
bool WM_cursor_set_from_tool(struct wmWindow *win, const ScrArea *area, const ARegion *region);
void WM_cursor_modal_set(struct wmWindow *win, int val);
void WM_cursor_modal_restore(struct wmWindow *win);
/**
 * To allow usage all over, we do entire WM.
 */
void WM_cursor_wait(bool val);
/**
 * \param bounds: can be NULL
 */
void WM_cursor_grab_enable(struct wmWindow *win, int wrap, bool hide, int bounds[4]);
void WM_cursor_grab_disable(struct wmWindow *win, const int mouse_ungrab_xy[2]);
/**
 * After this you can call restore too.
 */
void WM_cursor_time(struct wmWindow *win, int nr);

struct wmPaintCursor *WM_paint_cursor_activate(
    short space_type,
    short region_type,
    bool (*poll)(struct bContext *C),
    void (*draw)(struct bContext *C, int, int, void *customdata),
    void *customdata);

bool WM_paint_cursor_end(struct wmPaintCursor *handle);
void WM_paint_cursor_remove_by_type(struct wmWindowManager *wm,
                                    void *draw_fn,
                                    void (*free)(void *));
void WM_paint_cursor_tag_redraw(struct wmWindow *win, struct ARegion *region);

/**
 * This function requires access to the GHOST_SystemHandle (g_system).
 */
void WM_cursor_warp(struct wmWindow *win, int x, int y);
/**
 * Set x, y to values we can actually position the cursor to.
 */
void WM_cursor_compatible_xy(wmWindow *win, int *x, int *y);

/* Handlers. */

typedef bool (*EventHandlerPoll)(const ARegion *region, const struct wmEvent *event);
struct wmEventHandler_Keymap *WM_event_add_keymap_handler(ListBase *handlers, wmKeyMap *keymap);
struct wmEventHandler_Keymap *WM_event_add_keymap_handler_poll(ListBase *handlers,
                                                               wmKeyMap *keymap,
                                                               EventHandlerPoll poll);
struct wmEventHandler_Keymap *WM_event_add_keymap_handler_v2d_mask(ListBase *handlers,
                                                                   wmKeyMap *keymap);
/**
 * \note Priorities not implemented yet, for time being just insert in begin of list.
 */
struct wmEventHandler_Keymap *WM_event_add_keymap_handler_priority(ListBase *handlers,
                                                                   wmKeyMap *keymap,
                                                                   int priority);

typedef struct wmEventHandler_KeymapResult {
  wmKeyMap *keymaps[3];
  int keymaps_len;
} wmEventHandler_KeymapResult;

typedef void(wmEventHandler_KeymapDynamicFn)(wmWindowManager *wm,
                                             struct wmWindow *win,
                                             struct wmEventHandler_Keymap *handler,
                                             struct wmEventHandler_KeymapResult *km_result);

void WM_event_get_keymap_from_toolsystem_with_gizmos(struct wmWindowManager *wm,
                                                     struct wmWindow *win,
                                                     struct wmEventHandler_Keymap *handler,
                                                     wmEventHandler_KeymapResult *km_result);
void WM_event_get_keymap_from_toolsystem(struct wmWindowManager *wm,
                                         struct wmWindow *win,
                                         struct wmEventHandler_Keymap *handler,
                                         wmEventHandler_KeymapResult *km_result);

struct wmEventHandler_Keymap *WM_event_add_keymap_handler_dynamic(
    ListBase *handlers, wmEventHandler_KeymapDynamicFn *keymap_fn, void *user_data);

void WM_event_remove_keymap_handler(ListBase *handlers, wmKeyMap *keymap);

void WM_event_set_keymap_handler_post_callback(struct wmEventHandler_Keymap *handler,
                                               void(keymap_tag)(wmKeyMap *keymap,
                                                                wmKeyMapItem *kmi,
                                                                void *user_data),
                                               void *user_data);
void WM_event_get_keymaps_from_handler(wmWindowManager *wm,
                                       struct wmWindow *win,
                                       struct wmEventHandler_Keymap *handler,
                                       struct wmEventHandler_KeymapResult *km_result);

wmKeyMapItem *WM_event_match_keymap_item(struct bContext *C,
                                         wmKeyMap *keymap,
                                         const struct wmEvent *event);

wmKeyMapItem *WM_event_match_keymap_item_from_handlers(struct bContext *C,
                                                       struct wmWindowManager *wm,
                                                       struct wmWindow *win,
                                                       struct ListBase *handlers,
                                                       const struct wmEvent *event);

typedef int (*wmUIHandlerFunc)(struct bContext *C, const struct wmEvent *event, void *userdata);
typedef void (*wmUIHandlerRemoveFunc)(struct bContext *C, void *userdata);

struct wmEventHandler_UI *WM_event_add_ui_handler(const struct bContext *C,
                                                  ListBase *handlers,
                                                  wmUIHandlerFunc handle_fn,
                                                  wmUIHandlerRemoveFunc remove_fn,
                                                  void *user_data,
                                                  char flag);

/**
 * Return the first modal operator of type \a ot or NULL.
 */
wmOperator *WM_operator_find_modal_by_type(wmWindow *win, const wmOperatorType *ot);

/**
 * \param postpone: Enable for `win->modalhandlers`,
 * this is in a running for () loop in wm_handlers_do().
 */
void WM_event_remove_ui_handler(ListBase *handlers,
                                wmUIHandlerFunc handle_fn,
                                wmUIHandlerRemoveFunc remove_fn,
                                void *user_data,
                                bool postpone);
void WM_event_remove_area_handler(struct ListBase *handlers, void *area);
void WM_event_free_ui_handler_all(struct bContext *C,
                                  ListBase *handlers,
                                  wmUIHandlerFunc handle_fn,
                                  wmUIHandlerRemoveFunc remove_fn);

struct wmEventHandler_Op *WM_event_add_modal_handler(struct bContext *C, struct wmOperator *op);
/**
 * Modal handlers store a pointer to an area which might be freed while the handler runs.
 * Use this function to NULL all handler pointers to \a old_area.
 */
void WM_event_modal_handler_area_replace(wmWindow *win,
                                         const struct ScrArea *old_area,
                                         struct ScrArea *new_area);
/**
 * Modal handlers store a pointer to a region which might be freed while the handler runs.
 * Use this function to NULL all handler pointers to \a old_region.
 */
void WM_event_modal_handler_region_replace(wmWindow *win,
                                           const struct ARegion *old_region,
                                           struct ARegion *new_region);

/**
 * Called on exit or remove area, only here call cancel callback.
 */
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
void WM_event_add_mousemove(wmWindow *win);

#ifdef WITH_INPUT_NDOF
/* 3D mouse */
void WM_ndof_deadzone_set(float deadzone);
#endif
/* notifiers */
void WM_event_add_notifier_ex(struct wmWindowManager *wm,
                              const struct wmWindow *win,
                              unsigned int type,
                              void *reference);
void WM_event_add_notifier(const struct bContext *C, unsigned int type, void *reference);
void WM_main_add_notifier(unsigned int type, void *reference);
/**
 * Clear notifiers by reference, Used so listeners don't act on freed data.
 */
void WM_main_remove_notifier_reference(const void *reference);
void WM_main_remap_editor_id_reference(const struct IDRemapper *mappings);

/* reports */
/**
 * Show the report in the info header.
 */
void WM_report_banner_show(void);
/**
 * Hide all currently displayed banners and abort their timer.
 */
void WM_report_banners_cancel(struct Main *bmain);
void WM_report(eReportType type, const char *message);
void WM_reportf(eReportType type, const char *format, ...) ATTR_PRINTF_FORMAT(2, 3);

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
/**
 * To (de)activate running timers temporary.
 */
void WM_event_timer_sleep(struct wmWindowManager *wm,
                          struct wmWindow *win,
                          struct wmTimer *timer,
                          bool do_sleep);

/* Operator API, default callbacks. */

/**
 * Helper to get select and tweak-transform to work conflict free and as desired. See
 * #WM_operator_properties_generic_select() for details.
 *
 * To be used together with #WM_generic_select_invoke() and
 * #WM_operator_properties_generic_select().
 */
int WM_generic_select_modal(struct bContext *C,
                            struct wmOperator *op,
                            const struct wmEvent *event);
/**
 * Helper to get select and tweak-transform to work conflict free and as desired. See
 * #WM_operator_properties_generic_select() for details.
 *
 * To be used together with #WM_generic_select_modal() and
 * #WM_operator_properties_generic_select().
 */
int WM_generic_select_invoke(struct bContext *C,
                             struct wmOperator *op,
                             const struct wmEvent *event);
void WM_operator_view3d_unit_defaults(struct bContext *C, struct wmOperator *op);
int WM_operator_smooth_viewtx_get(const struct wmOperator *op);
/**
 * Invoke callback, uses enum property named "type".
 */
int WM_menu_invoke_ex(struct bContext *C, struct wmOperator *op, wmOperatorCallContext opcontext);
int WM_menu_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
/**
 * Call an existent menu. The menu can be created in C or Python.
 */
void WM_menu_name_call(struct bContext *C, const char *menu_name, short context);
/**
 * Similar to #WM_enum_search_invoke, but draws previews. Also, this can't
 * be used as invoke callback directly since it needs additional info.
 */
int WM_enum_search_invoke_previews(struct bContext *C,
                                   struct wmOperator *op,
                                   short prv_cols,
                                   short prv_rows);
int WM_enum_search_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
/**
 * Invoke callback, confirm menu + exec.
 */
int WM_operator_confirm(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int WM_operator_confirm_or_exec(struct bContext *C,
                                struct wmOperator *op,
                                const struct wmEvent *event);
/**
 * Invoke callback, file selector "filepath" unset + exec.
 *
 * #wmOperatorType.invoke, opens file-select if path property not set, otherwise executes.
 */
int WM_operator_filesel(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
bool WM_operator_filesel_ensure_ext_imtype(wmOperator *op,
                                           const struct ImageFormatData *im_format);
/** Callback for #wmOperatorType.poll */
bool WM_operator_winactive(struct bContext *C);
/**
 * Invoke callback, exec + redo popup.
 *
 * Same as #WM_operator_props_popup but don't use operator redo.
 * just wraps #WM_operator_props_dialog_popup.
 */
int WM_operator_props_popup_confirm(struct bContext *C,
                                    struct wmOperator *op,
                                    const struct wmEvent *event);
/**
 * Same as #WM_operator_props_popup but call the operator first,
 * This way - the button values correspond to the result of the operator.
 * Without this, first access to a button will make the result jump, see T32452.
 */
int WM_operator_props_popup_call(struct bContext *C,
                                 struct wmOperator *op,
                                 const struct wmEvent *event);
int WM_operator_props_popup(struct bContext *C,
                            struct wmOperator *op,
                            const struct wmEvent *event);
int WM_operator_props_dialog_popup(struct bContext *C, struct wmOperator *op, int width);
int WM_operator_redo_popup(struct bContext *C, struct wmOperator *op);
int WM_operator_ui_popup(struct bContext *C, struct wmOperator *op, int width);

/**
 * Can't be used as an invoke directly, needs message arg (can be NULL).
 */
int WM_operator_confirm_message_ex(struct bContext *C,
                                   struct wmOperator *op,
                                   const char *title,
                                   int icon,
                                   const char *message,
                                   wmOperatorCallContext opcontext);
int WM_operator_confirm_message(struct bContext *C, struct wmOperator *op, const char *message);

/* Operator API. */

void WM_operator_free(struct wmOperator *op);
void WM_operator_free_all_after(wmWindowManager *wm, struct wmOperator *op);
/**
 * Use with extreme care!
 * Properties, custom-data etc - must be compatible.
 *
 * \param op: Operator to assign the type to.
 * \param ot: Operator type to assign.
 */
void WM_operator_type_set(struct wmOperator *op, struct wmOperatorType *ot);
void WM_operator_stack_clear(struct wmWindowManager *wm);
/**
 * This function is needed in the case when an addon id disabled
 * while a modal operator it defined is running.
 */
void WM_operator_handlers_clear(wmWindowManager *wm, struct wmOperatorType *ot);

bool WM_operator_poll(struct bContext *C, struct wmOperatorType *ot);
bool WM_operator_poll_context(struct bContext *C, struct wmOperatorType *ot, short context);
/**
 * For running operators with frozen context (modal handlers, menus).
 *
 * \param store: Store properties for re-use when an operator has finished
 * (unless #PROP_SKIP_SAVE is set).
 *
 * \warning do not use this within an operator to call itself! T29537.
 */
int WM_operator_call_ex(struct bContext *C, struct wmOperator *op, bool store);
int WM_operator_call(struct bContext *C, struct wmOperator *op);
/**
 * This is intended to be used when an invoke operator wants to call exec on itself
 * and is basically like running op->type->exec() directly, no poll checks no freeing,
 * since we assume whoever called invoke will take care of that
 */
int WM_operator_call_notest(struct bContext *C, struct wmOperator *op);
/**
 * Execute this operator again, put here so it can share above code
 */
int WM_operator_repeat(struct bContext *C, struct wmOperator *op);
int WM_operator_repeat_last(struct bContext *C, struct wmOperator *op);
/**
 * \return true if #WM_operator_repeat can run.
 * Simple check for now but may become more involved.
 * To be sure the operator can run call `WM_operator_poll(C, op->type)` also, since this call
 * checks if #WM_operator_repeat() can run at all, not that it WILL run at any time.
 */
bool WM_operator_repeat_check(const struct bContext *C, struct wmOperator *op);
bool WM_operator_is_repeat(const struct bContext *C, const struct wmOperator *op);

bool WM_operator_name_poll(struct bContext *C, const char *opstring);
/**
 * Invokes operator in context.
 *
 * \param event: Optionally pass in an event to use when context uses one of the
 * `WM_OP_INVOKE_*` values. When left unset the #wmWindow.eventstate will be used,
 * this can cause problems for operators that read the events type - for example,
 * storing the key that was pressed so as to be able to detect it's release.
 * In these cases it's necessary to forward the current event being handled.
 */
int WM_operator_name_call_ptr(struct bContext *C,
                              struct wmOperatorType *ot,
                              wmOperatorCallContext context,
                              struct PointerRNA *properties,
                              const wmEvent *event);
/** See #WM_operator_name_call_ptr */
int WM_operator_name_call(struct bContext *C,
                          const char *opstring,
                          wmOperatorCallContext context,
                          struct PointerRNA *properties,
                          const wmEvent *event);
int WM_operator_name_call_with_properties(struct bContext *C,
                                          const char *opstring,
                                          wmOperatorCallContext context,
                                          struct IDProperty *properties,
                                          const wmEvent *event);
/**
 * Similar to #WM_operator_name_call called with #WM_OP_EXEC_DEFAULT context.
 *
 * - #wmOperatorType is used instead of operator name since python already has the operator type.
 * - `poll()` must be called by python before this runs.
 * - reports can be passed to this function (so python can report them as exceptions).
 */
int WM_operator_call_py(struct bContext *C,
                        struct wmOperatorType *ot,
                        wmOperatorCallContext context,
                        struct PointerRNA *properties,
                        struct ReportList *reports,
                        bool is_undo);

void WM_operator_name_call_ptr_with_depends_on_cursor(struct bContext *C,
                                                      wmOperatorType *ot,
                                                      wmOperatorCallContext opcontext,
                                                      PointerRNA *properties,
                                                      const wmEvent *event,
                                                      const char *drawstr);

/**
 * Similar to the function above except its uses ID properties used for key-maps and macros.
 */
void WM_operator_properties_alloc(struct PointerRNA **ptr,
                                  struct IDProperty **properties,
                                  const char *opstring);

/**
 * Make props context sensitive or not.
 */
void WM_operator_properties_sanitize(struct PointerRNA *ptr, bool no_context);

/**
 * Set all props to their default.
 *
 * \param do_update: Only update un-initialized props.
 *
 * \note There's nothing specific to operators here.
 * This could be made a general function.
 */
bool WM_operator_properties_default(struct PointerRNA *ptr, bool do_update);
/**
 * Remove all props without #PROP_SKIP_SAVE.
 */
void WM_operator_properties_reset(struct wmOperator *op);
void WM_operator_properties_create(struct PointerRNA *ptr, const char *opstring);
void WM_operator_properties_create_ptr(struct PointerRNA *ptr, struct wmOperatorType *ot);
void WM_operator_properties_clear(struct PointerRNA *ptr);
void WM_operator_properties_free(struct PointerRNA *ptr);

bool WM_operator_check_ui_empty(struct wmOperatorType *ot);
/**
 * Return false, if the UI should be disabled.
 */
bool WM_operator_check_ui_enabled(const struct bContext *C, const char *idname);

IDProperty *WM_operator_last_properties_ensure_idprops(struct wmOperatorType *ot);
void WM_operator_last_properties_ensure(struct wmOperatorType *ot, struct PointerRNA *ptr);
wmOperator *WM_operator_last_redo(const struct bContext *C);
/**
 * Use for drag & drop a path or name with operators invoke() function.
 * Returns null if no operator property is set to identify the file or ID to use.
 */
ID *WM_operator_drop_load_path(struct bContext *C, struct wmOperator *op, short idcode);

bool WM_operator_last_properties_init(struct wmOperator *op);
bool WM_operator_last_properties_store(struct wmOperator *op);

/* wm_operator_props.c */

void WM_operator_properties_confirm_or_exec(struct wmOperatorType *ot);

/** Flags for #WM_operator_properties_filesel. */
typedef enum eFileSel_Flag {
  WM_FILESEL_RELPATH = 1 << 0,
  WM_FILESEL_DIRECTORY = 1 << 1,
  WM_FILESEL_FILENAME = 1 << 2,
  WM_FILESEL_FILEPATH = 1 << 3,
  WM_FILESEL_FILES = 1 << 4,
  /** Show the properties sidebar by default. */
  WM_FILESEL_SHOW_PROPS = 1 << 5,
} eFileSel_Flag;
ENUM_OPERATORS(eFileSel_Flag, WM_FILESEL_SHOW_PROPS)

/** Action for #WM_operator_properties_filesel. */
typedef enum eFileSel_Action {
  FILE_OPENFILE = 0,
  FILE_SAVE = 1,
} eFileSel_Action;

/**
 * Default properties for file-select.
 */
void WM_operator_properties_filesel(struct wmOperatorType *ot,
                                    int filter,
                                    short type,
                                    eFileSel_Action action,
                                    eFileSel_Flag flag,
                                    short display,
                                    short sort);

/**
 * Tries to pass \a id to an operator via either a "session_uuid" or a "name" property defined in
 * the properties of \a ptr. The former is preferred, since it works properly with linking and
 * library overrides (which may both result in multiple IDs with the same name and type).
 *
 * Also see #WM_operator_properties_id_lookup() and
 * #WM_operator_properties_id_lookup_from_name_or_session_uuid()
 */
void WM_operator_properties_id_lookup_set_from_id(PointerRNA *ptr, const ID *id);
/**
 * Tries to find an ID in \a bmain. There needs to be either a "session_uuid" int or "name" string
 * property defined and set. The former has priority. See #WM_operator_properties_id_lookup() for a
 * helper to add the properties.
 */
struct ID *WM_operator_properties_id_lookup_from_name_or_session_uuid(struct Main *bmain,
                                                                      PointerRNA *ptr,
                                                                      enum ID_Type type);
/**
 * Check if either the "session_uuid" or "name" property is set inside \a ptr. If this is the case
 * the ID can be looked up by #WM_operator_properties_id_lookup_from_name_or_session_uuid().
 */
bool WM_operator_properties_id_lookup_is_set(PointerRNA *ptr);
/**
 * Adds "name" and "session_uuid" properties so the caller can tell the operator which ID to act
 * on. See #WM_operator_properties_id_lookup_from_name_or_session_uuid(). Both properties will be
 * hidden in the UI and not be saved over consecutive operator calls.
 *
 * \note New operators should probably use "session_uuid" only (set \a add_name_prop to #false),
 * since this works properly with linked data and/or library overrides (in both cases, multiple IDs
 * with the same name and type may be present). The "name" property is only kept to not break
 * compatibility with old scripts using some previously existing operators.
 */
void WM_operator_properties_id_lookup(wmOperatorType *ot, const bool add_name_prop);

/**
 * Disable using cursor position,
 * use when view operators are initialized from buttons.
 */
void WM_operator_properties_use_cursor_init(struct wmOperatorType *ot);
void WM_operator_properties_border(struct wmOperatorType *ot);
void WM_operator_properties_border_to_rcti(struct wmOperator *op, struct rcti *rect);
void WM_operator_properties_border_to_rctf(struct wmOperator *op, rctf *rect);
/**
 * Use with #WM_gesture_box_invoke
 */
void WM_operator_properties_gesture_box_ex(struct wmOperatorType *ot, bool deselect, bool extend);
void WM_operator_properties_gesture_box(struct wmOperatorType *ot);
void WM_operator_properties_gesture_box_select(struct wmOperatorType *ot);
void WM_operator_properties_gesture_box_zoom(struct wmOperatorType *ot);
/**
 * Use with #WM_gesture_lasso_invoke
 */
void WM_operator_properties_gesture_lasso(struct wmOperatorType *ot);
/**
 * Use with #WM_gesture_straightline_invoke
 */
void WM_operator_properties_gesture_straightline(struct wmOperatorType *ot, int cursor);
/**
 * Use with #WM_gesture_circle_invoke
 */
void WM_operator_properties_gesture_circle(struct wmOperatorType *ot);
/**
 * See #ED_select_pick_params_from_operator to initialize parameters defined here.
 */
void WM_operator_properties_mouse_select(struct wmOperatorType *ot);
void WM_operator_properties_select_all(struct wmOperatorType *ot);
void WM_operator_properties_select_action(struct wmOperatorType *ot,
                                          int default_action,
                                          bool hide_gui);
/**
 * Only #SELECT / #DESELECT.
 */
void WM_operator_properties_select_action_simple(struct wmOperatorType *ot,
                                                 int default_action,
                                                 bool hide_gui);
/**
 * Use for all select random operators.
 * Adds properties: percent, seed, action.
 */
void WM_operator_properties_select_random(struct wmOperatorType *ot);
int WM_operator_properties_select_random_seed_increment_get(wmOperator *op);
void WM_operator_properties_select_operation(struct wmOperatorType *ot);
/**
 * \note Some tools don't support XOR/AND.
 */
void WM_operator_properties_select_operation_simple(struct wmOperatorType *ot);
void WM_operator_properties_select_walk_direction(struct wmOperatorType *ot);
/**
 * Selecting and tweaking items are overlapping operations. Getting both to work without conflicts
 * requires special care. See
 * https://wiki.blender.org/wiki/Human_Interface_Guidelines/Selection#Select-tweaking for the
 * desired behavior.
 *
 * For default click selection (with no modifier keys held), the select operators can do the
 * following:
 * - On a mouse press on an unselected item, change selection and finish immediately after.
 *   This sends an undo push and allows transform to take over should a click-drag event be caught.
 * - On a mouse press on a selected item, don't change selection state, but start modal execution
 *   of the operator. Idea is that we wait with deselecting other items until we know that the
 *   intention wasn't to tweak (mouse press+drag) all selected items.
 * - If a click-drag is recognized before the release event happens, cancel the operator,
 *   so that transform can take over and no undo-push is sent.
 * - If the release event occurs rather than a click-drag one,
 *   deselect all items but the one under the cursor, and finish the modal operator.
 *
 * This utility, together with #WM_generic_select_invoke() and #WM_generic_select_modal() should
 * help getting the wanted behavior to work. Most generic logic should be handled in these, so that
 * the select operators only have to care for the case dependent handling.
 *
 * Every select operator has slightly different requirements, e.g. sequencer strip selection
 * also needs to account for handle selection. This should be the baseline behavior though.
 */
void WM_operator_properties_generic_select(struct wmOperatorType *ot);

struct CheckerIntervalParams {
  int nth; /* bypass when set to zero */
  int skip;
  int offset;
};
/**
 * \param nth_can_disable: Enable if we want to be able to select no interval at all.
 */
void WM_operator_properties_checker_interval(struct wmOperatorType *ot, bool nth_can_disable);
void WM_operator_properties_checker_interval_from_op(struct wmOperator *op,
                                                     struct CheckerIntervalParams *op_params);
bool WM_operator_properties_checker_interval_test(const struct CheckerIntervalParams *op_params,
                                                  int depth);

/**
 * Operator as a Python command (resulting string must be freed).
 *
 * Print a string representation of the operator,
 * with the arguments that it runs so Python can run it again.
 *
 * When calling from an existing #wmOperator, better to use simple version:
 * `WM_operator_pystring(C, op);`
 *
 * \note Both \a op and \a opptr may be `NULL` (\a op is only used for macro operators).
 */
char *WM_operator_pystring_ex(struct bContext *C,
                              struct wmOperator *op,
                              bool all_args,
                              bool macro_args,
                              struct wmOperatorType *ot,
                              struct PointerRNA *opptr);
char *WM_operator_pystring(struct bContext *C,
                           struct wmOperator *op,
                           bool all_args,
                           bool macro_args);
/**
 * \return true if the string was shortened.
 */
bool WM_operator_pystring_abbreviate(char *str, int str_len_max);
char *WM_prop_pystring_assign(struct bContext *C,
                              struct PointerRNA *ptr,
                              struct PropertyRNA *prop,
                              int index);
/**
 * Convert: `some.op` -> `SOME_OT_op` or leave as-is.
 * \return the length of `dst`.
 */
size_t WM_operator_bl_idname(char *dst, const char *src) ATTR_NONNULL(1, 2);
/**
 * Convert: `SOME_OT_op` -> `some.op` or leave as-is.
 * \return the length of `dst`.
 */
size_t WM_operator_py_idname(char *dst, const char *src) ATTR_NONNULL(1, 2);
/**
 * Sanity check to ensure #WM_operator_bl_idname won't fail.
 * \returns true when there are no problems with \a idname, otherwise report an error.
 */
bool WM_operator_py_idname_ok_or_report(struct ReportList *reports,
                                        const char *classname,
                                        const char *idname);
/**
 * Calculate the path to `ptr` from context `C`, or return NULL if it can't be calculated.
 */
char *WM_context_path_resolve_property_full(const struct bContext *C,
                                            const PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            int index);
char *WM_context_path_resolve_full(struct bContext *C, const PointerRNA *ptr);

/* wm_operator_type.c */

struct wmOperatorType *WM_operatortype_find(const char *idname, bool quiet);
/**
 * \note Caller must free.
 */
void WM_operatortype_iter(struct GHashIterator *ghi);
void WM_operatortype_append(void (*opfunc)(struct wmOperatorType *));
void WM_operatortype_append_ptr(void (*opfunc)(struct wmOperatorType *, void *), void *userdata);
void WM_operatortype_append_macro_ptr(void (*opfunc)(struct wmOperatorType *, void *),
                                      void *userdata);
/**
 * Called on initialize WM_exit().
 */
void WM_operatortype_remove_ptr(struct wmOperatorType *ot);
bool WM_operatortype_remove(const char *idname);
/**
 * Remove memory of all previously executed tools.
 */
void WM_operatortype_last_properties_clear_all(void);

void WM_operatortype_idname_visit_for_search(const struct bContext *C,
                                             PointerRNA *ptr,
                                             PropertyRNA *prop,
                                             const char *edit_text,
                                             StringPropertySearchVisitFunc visit_fn,
                                             void *visit_user_data);

/**
 * Tag all operator-properties of \a ot defined after calling this, until
 * the next #WM_operatortype_props_advanced_end call (if available), with
 * #OP_PROP_TAG_ADVANCED. Previously defined ones properties not touched.
 *
 * Calling this multiple times without a call to #WM_operatortype_props_advanced_end,
 * all calls after the first one are ignored. Meaning all proprieties defined after the
 * first call are tagged as advanced.
 *
 * This doesn't do the actual tagging, #WM_operatortype_props_advanced_end does which is
 * called for all operators during registration (see #wm_operatortype_append__end).
 */
void WM_operatortype_props_advanced_begin(struct wmOperatorType *ot);
/**
 * Tags all operator-properties of \a ot defined since the first
 * #WM_operatortype_props_advanced_begin call,
 * or the last #WM_operatortype_props_advanced_end call, with #OP_PROP_TAG_ADVANCED.
 *
 * \note This is called for all operators during registration (see #wm_operatortype_append__end).
 * So it does not need to be explicitly called in operator-type definition.
 */
void WM_operatortype_props_advanced_end(struct wmOperatorType *ot);

#define WM_operatortype_prop_tag(property, tags) \
  { \
    CHECK_TYPE(tags, eOperatorPropTags); \
    RNA_def_property_tags(prop, tags); \
  } \
  (void)0

/**
 * \note Names have to be static for now.
 */
struct wmOperatorType *WM_operatortype_append_macro(const char *idname,
                                                    const char *name,
                                                    const char *description,
                                                    int flag);
struct wmOperatorTypeMacro *WM_operatortype_macro_define(struct wmOperatorType *ot,
                                                         const char *idname);

const char *WM_operatortype_name(struct wmOperatorType *ot, struct PointerRNA *properties);
char *WM_operatortype_description(struct bContext *C,
                                  struct wmOperatorType *ot,
                                  struct PointerRNA *properties);
/**
 * Use when we want a label, preferring the description.
 */
char *WM_operatortype_description_or_name(struct bContext *C,
                                          struct wmOperatorType *ot,
                                          struct PointerRNA *properties);

/* wm_operator_utils.c */

/**
 * Allow an operator with only and execute function to run modally,
 * re-doing the action, using vertex coordinate store/restore instead of operator undo.
 */
void WM_operator_type_modal_from_exec_for_object_edit_coords(struct wmOperatorType *ot);

/* wm_uilist_type.c */

/**
 * Called on initialize #WM_init()
 */
void WM_uilisttype_init(void);
struct uiListType *WM_uilisttype_find(const char *idname, bool quiet);
bool WM_uilisttype_add(struct uiListType *ult);
void WM_uilisttype_remove_ptr(struct Main *bmain, struct uiListType *ult);
void WM_uilisttype_free(void);

/**
 * The "full" list-ID is an internal name used for storing and identifying a list. It is built like
 * this:
 * "{uiListType.idname}_{list_id}", whereby "list_id" is an optional parameter passed to
 * `UILayout.template_list()`. If it is not set, the full list-ID is just "{uiListType.idname}_".
 *
 * Note that whenever the Python API refers to the list-ID, it's the short, "non-full" one it
 * passed to `UILayout.template_list()`. C code can query that through
 * #WM_uilisttype_list_id_get().
 */
void WM_uilisttype_to_full_list_id(const struct uiListType *ult,
                                   const char *list_id,
                                   char r_full_list_id[]);
/**
 * Get the "non-full" list-ID, see #WM_uilisttype_to_full_list_id() for details.
 *
 * \note Assumes `uiList.list_id` was set using #WM_uilisttype_to_full_list_id()!
 */
const char *WM_uilisttype_list_id_get(const struct uiListType *ult, struct uiList *list);

/* wm_menu_type.c */

/**
 * \note Called on initialize #WM_init().
 */
void WM_menutype_init(void);
struct MenuType *WM_menutype_find(const char *idname, bool quiet);
void WM_menutype_iter(struct GHashIterator *ghi);
bool WM_menutype_add(struct MenuType *mt);
void WM_menutype_freelink(struct MenuType *mt);
void WM_menutype_free(void);
bool WM_menutype_poll(struct bContext *C, struct MenuType *mt);

void WM_menutype_idname_visit_for_search(const struct bContext *C,
                                         struct PointerRNA *ptr,
                                         struct PropertyRNA *prop,
                                         const char *edit_text,
                                         StringPropertySearchVisitFunc visit_fn,
                                         void *visit_user_data);

/* wm_panel_type.c */

/**
 * Called on initialize #WM_init().
 */
void WM_paneltype_init(void);
void WM_paneltype_clear(void);
struct PanelType *WM_paneltype_find(const char *idname, bool quiet);
bool WM_paneltype_add(struct PanelType *pt);
void WM_paneltype_remove(struct PanelType *pt);

void WM_paneltype_idname_visit_for_search(const struct bContext *C,
                                          struct PointerRNA *ptr,
                                          struct PropertyRNA *prop,
                                          const char *edit_text,
                                          StringPropertySearchVisitFunc visit_fn,
                                          void *visit_user_data);

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
/**
 * helper function, we may want to add options for conversion to view space
 *
 * caller must free.
 */
const int (*WM_gesture_lasso_path_to_array(struct bContext *C,
                                           struct wmOperator *op,
                                           int *mcoords_len))[2];

int WM_gesture_straightline_invoke(struct bContext *C,
                                   struct wmOperator *op,
                                   const struct wmEvent *event);
/**
 * This invoke callback starts the straight-line gesture with a viewport preview to the right side
 * of the line.
 */
int WM_gesture_straightline_active_side_invoke(struct bContext *C,
                                               struct wmOperator *op,
                                               const struct wmEvent *event);
/**
 * This modal callback calls exec once per mouse move event while the gesture is active with the
 * updated line start and end values, so it can be used for tools that have a real time preview
 * (like a gradient updating in real time over the mesh).
 */
int WM_gesture_straightline_modal(struct bContext *C,
                                  struct wmOperator *op,
                                  const struct wmEvent *event);
/**
 * This modal one-shot callback only calls exec once after the gesture finishes without any updates
 * during the gesture execution. Should be used for operations that are intended to be applied once
 * without real time preview (like a trimming tool that only applies the bisect operation once
 * after finishing the gesture as the bisect operation is too heavy to be computed in real time for
 * a preview).
 */
int WM_gesture_straightline_oneshot_modal(struct bContext *C,
                                          struct wmOperator *op,
                                          const struct wmEvent *event);
void WM_gesture_straightline_cancel(struct bContext *C, struct wmOperator *op);

/* Gesture manager API */

/**
 * Context checked on having screen, window and area.
 */
struct wmGesture *WM_gesture_new(struct wmWindow *window,
                                 const struct ARegion *region,
                                 const struct wmEvent *event,
                                 int type);
void WM_gesture_end(struct wmWindow *win, struct wmGesture *gesture);
void WM_gestures_remove(struct wmWindow *win);
void WM_gestures_free_all(struct wmWindow *win);
bool WM_gesture_is_modal_first(const struct wmGesture *gesture);

/* File-selecting support. */

/**
 * The idea here is to keep a handler alive on window queue, owning the operator.
 * The file window can send event to make it execute, thus ensuring
 * executing happens outside of lower level queues, with UI refreshed.
 * Should also allow multi-window solutions.
 */
void WM_event_add_fileselect(struct bContext *C, struct wmOperator *op);
void WM_event_fileselect_event(struct wmWindowManager *wm, void *ophandle, int eventval);

/**
 * Sets the active region for this space from the context.
 *
 * \see #BKE_area_find_region_active_win
 */
void WM_operator_region_active_win_set(struct bContext *C);

/**
 * Only finish + pass through for press events (allowing press-tweak).
 */
int WM_operator_flag_only_pass_through_on_press(int retval, const struct wmEvent *event);

/* Drag and drop. */

/**
 * Note that the pointer should be valid allocated and not on stack.
 */
struct wmDrag *WM_event_start_drag(
    struct bContext *C, int icon, int type, void *poin, double value, unsigned int flags);
void WM_event_drag_image(struct wmDrag *, struct ImBuf *, float scale);
void WM_drag_free(struct wmDrag *drag);
void WM_drag_data_free(int dragtype, void *poin);
void WM_drag_free_list(struct ListBase *lb);
struct wmDropBox *WM_dropbox_add(
    ListBase *lb,
    const char *idname,
    bool (*poll)(struct bContext *, struct wmDrag *, const struct wmEvent *event),
    void (*copy)(struct bContext *, struct wmDrag *, struct wmDropBox *),
    void (*cancel)(struct Main *, struct wmDrag *, struct wmDropBox *),
    WMDropboxTooltipFunc tooltip);
void WM_drag_draw_item_name_fn(struct bContext *C,
                               struct wmWindow *win,
                               struct wmDrag *drag,
                               const int xy[2]);
void WM_drag_draw_default_fn(struct bContext *C,
                             struct wmWindow *win,
                             struct wmDrag *drag,
                             const int xy[2]);
/**
 * `spaceid` / `regionid` are zero for window drop maps.
 */
ListBase *WM_dropboxmap_find(const char *idname, int spaceid, int regionid);

/* ID drag and drop */

/**
 * \param flag_extra: Additional linking flags (from #eFileSel_Params_Flag).
 */
ID *WM_drag_asset_id_import(wmDragAsset *asset_drag, int flag_extra);
bool WM_drag_asset_will_import_linked(const wmDrag *drag);
void WM_drag_add_local_ID(struct wmDrag *drag, struct ID *id, struct ID *from_parent);
struct ID *WM_drag_get_local_ID(const struct wmDrag *drag, short idcode);
struct ID *WM_drag_get_local_ID_from_event(const struct wmEvent *event, short idcode);
/**
 * Check if the drag data is either a local ID or an external ID asset of type \a idcode.
 */
bool WM_drag_is_ID_type(const struct wmDrag *drag, int idcode);

/**
 * \note Does not store \a asset in any way, so it's fine to pass a temporary.
 */
wmDragAsset *WM_drag_create_asset_data(const struct AssetHandle *asset,
                                       struct AssetMetaData *metadata,
                                       const char *path,
                                       int import_type);
struct wmDragAsset *WM_drag_get_asset_data(const struct wmDrag *drag, int idcode);
struct AssetMetaData *WM_drag_get_asset_meta_data(const struct wmDrag *drag, int idcode);
/**
 * When dragging a local ID, return that. Otherwise, if dragging an asset-handle, link or append
 * that depending on what was chosen by the drag-box (currently append only in fact).
 *
 * Use #WM_drag_free_imported_drag_ID() as cancel callback of the drop-box, so that the asset
 * import is rolled back if the drop operator fails.
 */
struct ID *WM_drag_get_local_ID_or_import_from_asset(const struct wmDrag *drag, int idcode);

/**
 * \brief Free asset ID imported for canceled drop.
 *
 * If the asset was imported (linked/appended) using #WM_drag_get_local_ID_or_import_from_asset()`
 * (typically via a #wmDropBox.copy() callback), we want the ID to be removed again if the drop
 * operator cancels.
 * This is for use as #wmDropBox.cancel() callback.
 */
void WM_drag_free_imported_drag_ID(struct Main *bmain,
                                   struct wmDrag *drag,
                                   struct wmDropBox *drop);

struct wmDragAssetCatalog *WM_drag_get_asset_catalog_data(const struct wmDrag *drag);

/**
 * \note Does not store \a asset in any way, so it's fine to pass a temporary.
 */
void WM_drag_add_asset_list_item(wmDrag *drag,
                                 const struct bContext *C,
                                 const struct AssetLibraryReference *asset_library_ref,
                                 const struct AssetHandle *asset);
const ListBase *WM_drag_asset_list_get(const wmDrag *drag);

const char *WM_drag_get_item_name(struct wmDrag *drag);

/* Set OpenGL viewport and scissor */
void wmViewport(const struct rcti *winrct);
void wmPartialViewport(rcti *drawrct, const rcti *winrct, const rcti *partialrct);
void wmWindowViewport(struct wmWindow *win);

/* OpenGL utilities with safety check */
void wmOrtho2(float x1, float x2, float y1, float y2);
/* use for conventions (avoid hard-coded offsets all over) */

/**
 * Default pixel alignment for regions.
 */
void wmOrtho2_region_pixelspace(const struct ARegion *region);
void wmOrtho2_pixelspace(float x, float y);
void wmGetProjectionMatrix(float mat[4][4], const struct rcti *winrct);

/* threaded Jobs Manager */
enum {
  WM_JOB_PRIORITY = (1 << 0),
  WM_JOB_EXCL_RENDER = (1 << 1),
  WM_JOB_PROGRESS = (1 << 2),
};

/**
 * Identifying jobs by owner alone is unreliable, this isn't saved,
 * order can change (keep 0 for 'any').
 */
enum {
  WM_JOB_TYPE_ANY = 0,
  WM_JOB_TYPE_COMPOSITE,
  WM_JOB_TYPE_RENDER,
  WM_JOB_TYPE_RENDER_PREVIEW, /* UI preview */
  /** Job for the UI to load previews from the file system (uses OS thumbnail cache). */
  WM_JOB_TYPE_LOAD_PREVIEW, /* UI preview */
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
  WM_JOB_TYPE_QUADRIFLOW_REMESH,
  WM_JOB_TYPE_TRACE_IMAGE,
  WM_JOB_TYPE_LINEART,
  WM_JOB_TYPE_SEQ_DRAW_THUMBNAIL,
  WM_JOB_TYPE_SEQ_DRAG_DROP_PREVIEW,
  /* add as needed, bake, seq proxy build
   * if having hard coded values is a problem */
};

/**
 * \return current job or adds new job, but doesn't run it.
 *
 * \note every owner only gets a single job,
 * adding a new one will stop running job and when stopped it starts the new one.
 */
struct wmJob *WM_jobs_get(struct wmWindowManager *wm,
                          struct wmWindow *win,
                          const void *owner,
                          const char *name,
                          int flag,
                          int job_type);

/**
 * Returns true if job runs, for UI (progress) indicators.
 */
bool WM_jobs_test(const struct wmWindowManager *wm, const void *owner, int job_type);
float WM_jobs_progress(const struct wmWindowManager *wm, const void *owner);
const char *WM_jobs_name(const struct wmWindowManager *wm, const void *owner);
/**
 * Time that job started.
 */
double WM_jobs_starttime(const struct wmWindowManager *wm, const void *owner);
void *WM_jobs_customdata_from_type(struct wmWindowManager *wm, const void *owner, int job_type);

bool WM_jobs_is_running(const struct wmJob *wm_job);
bool WM_jobs_is_stopped(const wmWindowManager *wm, const void *owner);
void *WM_jobs_customdata_get(struct wmJob *);
void WM_jobs_customdata_set(struct wmJob *, void *customdata, void (*free)(void *));
void WM_jobs_timer(struct wmJob *, double timestep, unsigned int note, unsigned int endnote);
void WM_jobs_delay_start(struct wmJob *, double delay_time);

typedef void (*wm_jobs_start_callback)(void *custom_data,
                                       short *stop,
                                       short *do_update,
                                       float *progress);
void WM_jobs_callbacks(struct wmJob *,
                       wm_jobs_start_callback startjob,
                       void (*initjob)(void *),
                       void (*update)(void *),
                       void (*endjob)(void *));

void WM_jobs_callbacks_ex(wmJob *wm_job,
                          wm_jobs_start_callback startjob,
                          void (*initjob)(void *),
                          void (*update)(void *),
                          void (*endjob)(void *),
                          void (*completed)(void *),
                          void (*canceled)(void *));

/**
 * If job running, the same owner gave it a new job.
 * if different owner starts existing startjob, it suspends itself
 */
void WM_jobs_start(struct wmWindowManager *wm, struct wmJob *);
/**
 * Signal job(s) from this owner or callback to stop, timer is required to get handled.
 */
void WM_jobs_stop(struct wmWindowManager *wm, const void *owner, void *startjob);
/**
 * Actually terminate thread and job timer.
 */
void WM_jobs_kill(struct wmWindowManager *wm,
                  void *owner,
                  void (*)(void *, short int *, short int *, float *));
/**
 * Wait until every job ended.
 */
void WM_jobs_kill_all(struct wmWindowManager *wm);
/**
 * Wait until every job ended, except for one owner (used in undo to keep screen job alive).
 */
void WM_jobs_kill_all_except(struct wmWindowManager *wm, const void *owner);
void WM_jobs_kill_type(struct wmWindowManager *wm, const void *owner, int job_type);

bool WM_jobs_has_running(const struct wmWindowManager *wm);
bool WM_jobs_has_running_type(const struct wmWindowManager *wm, int job_type);

void WM_job_main_thread_lock_acquire(struct wmJob *job);
void WM_job_main_thread_lock_release(struct wmJob *job);

/* Clipboard. */

/**
 * Return text from the clipboard.
 *
 * \note Caller needs to check for valid utf8 if this is a requirement.
 */
char *WM_clipboard_text_get(bool selection, int *r_len);
/**
 * Convenience function for pasting to areas of Blender which don't support newlines.
 */
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

void WM_draw_region_viewport_ensure(struct ARegion *region, short space_type);
void WM_draw_region_viewport_bind(struct ARegion *region);
void WM_draw_region_viewport_unbind(struct ARegion *region);

/* Region drawing */

void WM_draw_region_free(struct ARegion *region, bool hide);
struct GPUViewport *WM_draw_region_get_viewport(struct ARegion *region);
struct GPUViewport *WM_draw_region_get_bound_viewport(struct ARegion *region);

void WM_main_playanim(int argc, const char **argv);

/**
 * Debugging only, convenience function to write on crash.
 * Convenient to save a blend file from a debugger.
 */
bool write_crash_blend(void);

/**
 * Lock the interface for any communication.
 */
void WM_set_locked_interface(struct wmWindowManager *wm, bool lock);

void WM_event_tablet_data_default_set(struct wmTabletData *tablet_data);

/**
 * For testing only, see #G_FLAG_EVENT_SIMULATE.
 */
struct wmEvent *WM_event_add_simulate(struct wmWindow *win, const struct wmEvent *event_to_add);

const char *WM_window_cursor_keymap_status_get(const struct wmWindow *win,
                                               int button_index,
                                               int type_index);
void WM_window_cursor_keymap_status_refresh(struct bContext *C, struct wmWindow *win);

void WM_window_status_area_tag_redraw(struct wmWindow *win);
/**
 * Similar to #BKE_screen_area_map_find_area_xy and related functions,
 * use here since the area is stored in the window manager.
 */
struct ScrArea *WM_window_status_area_find(struct wmWindow *win, struct bScreen *screen);
bool WM_window_modal_keymap_status_draw(struct bContext *C,
                                        struct wmWindow *win,
                                        struct uiLayout *layout);

/* wm_event_query.c */

/**
 * For debugging only, getting inspecting events manually is tedious.
 */
void WM_event_print(const struct wmEvent *event);

/**
 * For modal callbacks, check configuration for how to interpret exit when dragging.
 */
bool WM_event_is_modal_drag_exit(const struct wmEvent *event,
                                 short init_event_type,
                                 short init_event_val);
bool WM_event_is_last_mousemove(const struct wmEvent *event);
bool WM_event_is_mouse_drag(const struct wmEvent *event);
bool WM_event_is_mouse_drag_or_press(const wmEvent *event);
int WM_event_drag_direction(const wmEvent *event);

/**
 * Detect motion between selection (callers should only use this for selection picking),
 * typically mouse press/click events.
 *
 * \param mval: Region relative coordinates, call with (-1, -1) resets the last cursor location.
 * \returns True when there was motion since last called.
 *
 * NOTE(@campbellbarton): The logic used here isn't foolproof.
 * It's possible that users move the cursor past #WM_EVENT_CURSOR_MOTION_THRESHOLD then back to
 * a position within the threshold (between mouse clicks).
 * In practice users never reported this since the threshold is very small (a few pixels).
 * To prevent the unlikely case of values matching from another region,
 * changing regions resets this value to (-1, -1).
 */
bool WM_cursor_test_motion_and_update(const int mval[2]) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;

int WM_event_drag_threshold(const struct wmEvent *event);
bool WM_event_drag_test(const struct wmEvent *event, const int prev_xy[2]);
bool WM_event_drag_test_with_delta(const struct wmEvent *event, const int delta[2]);
void WM_event_drag_start_mval(const wmEvent *event, const ARegion *region, int r_mval[2]);
void WM_event_drag_start_mval_fl(const wmEvent *event, const ARegion *region, float r_mval[2]);
void WM_event_drag_start_xy(const wmEvent *event, int r_xy[2]);

/**
 * Event map that takes preferences into account.
 */
int WM_userdef_event_map(int kmitype);
/**
 * Use so we can check if 'wmEvent.type' is released in modal operators.
 *
 * An alternative would be to add a 'wmEvent.type_nokeymap'... or similar.
 */
int WM_userdef_event_type_from_keymap_type(int kmitype);

#ifdef WITH_INPUT_NDOF
void WM_event_ndof_pan_get(const struct wmNDOFMotionData *ndof, float r_pan[3], bool use_zoom);
void WM_event_ndof_rotate_get(const struct wmNDOFMotionData *ndof, float r_rot[3]);

float WM_event_ndof_to_axis_angle(const struct wmNDOFMotionData *ndof, float axis[3]);
void WM_event_ndof_to_quat(const struct wmNDOFMotionData *ndof, float q[4]);
#endif /* WITH_INPUT_NDOF */

#ifdef WITH_XR_OPENXR
bool WM_event_is_xr(const struct wmEvent *event);
#endif

/**
 * If this is a tablet event, return tablet pressure and set `*pen_flip`
 * to 1 if the eraser tool is being used, 0 otherwise.
 */
float WM_event_tablet_data(const struct wmEvent *event, int *pen_flip, float tilt[2]);
bool WM_event_is_tablet(const struct wmEvent *event);

int WM_event_absolute_delta_x(const struct wmEvent *event);
int WM_event_absolute_delta_y(const struct wmEvent *event);

#ifdef WITH_INPUT_IME
bool WM_event_is_ime_switch(const struct wmEvent *event);
#endif

/* wm_tooltip.c */

typedef struct ARegion *(*wmTooltipInitFn)(struct bContext *C,
                                           struct ARegion *region,
                                           int *pass,
                                           double *r_pass_delay,
                                           bool *r_exit_on_event);

void WM_tooltip_immediate_init(struct bContext *C,
                               struct wmWindow *win,
                               struct ScrArea *area,
                               struct ARegion *region,
                               wmTooltipInitFn init);
void WM_tooltip_timer_init_ex(struct bContext *C,
                              struct wmWindow *win,
                              struct ScrArea *area,
                              struct ARegion *region,
                              wmTooltipInitFn init,
                              double delay);
void WM_tooltip_timer_init(struct bContext *C,
                           struct wmWindow *win,
                           struct ScrArea *area,
                           struct ARegion *region,
                           wmTooltipInitFn init);
void WM_tooltip_timer_clear(struct bContext *C, struct wmWindow *win);
void WM_tooltip_clear(struct bContext *C, struct wmWindow *win);
void WM_tooltip_init(struct bContext *C, struct wmWindow *win);
void WM_tooltip_refresh(struct bContext *C, struct wmWindow *win);
double WM_tooltip_time_closed(void);

/* wm_utils.c */

struct wmGenericCallback *WM_generic_callback_steal(struct wmGenericCallback *callback);
void WM_generic_callback_free(struct wmGenericCallback *callback);

void WM_generic_user_data_free(struct wmGenericUserData *wm_userdata);

bool WM_region_use_viewport(struct ScrArea *area, struct ARegion *region);

#ifdef WITH_XR_OPENXR
/* wm_xr_session.c */

/**
 * Check if the XR-Session was triggered.
 * If an error happened while trying to start a session, this returns false too.
 */
bool WM_xr_session_exists(const wmXrData *xr);
/**
 * Check if the session is running, according to the OpenXR definition.
 */
bool WM_xr_session_is_ready(const wmXrData *xr);
struct wmXrSessionState *WM_xr_session_state_handle_get(const wmXrData *xr);
struct ScrArea *WM_xr_session_area_get(const wmXrData *xr);
void WM_xr_session_base_pose_reset(wmXrData *xr);
bool WM_xr_session_state_viewer_pose_location_get(const wmXrData *xr, float r_location[3]);
bool WM_xr_session_state_viewer_pose_rotation_get(const wmXrData *xr, float r_rotation[4]);
bool WM_xr_session_state_viewer_pose_matrix_info_get(const wmXrData *xr,
                                                     float r_viewmat[4][4],
                                                     float *r_focal_len);
bool WM_xr_session_state_controller_grip_location_get(const wmXrData *xr,
                                                      unsigned int subaction_idx,
                                                      float r_location[3]);
bool WM_xr_session_state_controller_grip_rotation_get(const wmXrData *xr,
                                                      unsigned int subaction_idx,
                                                      float r_rotation[4]);
bool WM_xr_session_state_controller_aim_location_get(const wmXrData *xr,
                                                     unsigned int subaction_idx,
                                                     float r_location[3]);
bool WM_xr_session_state_controller_aim_rotation_get(const wmXrData *xr,
                                                     unsigned int subaction_idx,
                                                     float r_rotation[4]);
bool WM_xr_session_state_nav_location_get(const wmXrData *xr, float r_location[3]);
void WM_xr_session_state_nav_location_set(wmXrData *xr, const float location[3]);
bool WM_xr_session_state_nav_rotation_get(const wmXrData *xr, float r_rotation[4]);
void WM_xr_session_state_nav_rotation_set(wmXrData *xr, const float rotation[4]);
bool WM_xr_session_state_nav_scale_get(const wmXrData *xr, float *r_scale);
void WM_xr_session_state_nav_scale_set(wmXrData *xr, float scale);
void WM_xr_session_state_navigation_reset(struct wmXrSessionState *state);

struct ARegionType *WM_xr_surface_controller_region_type_get(void);

/* wm_xr_actions.c */

/* XR action functions to be called pre-XR session start.
 * NOTE: The "destroy" functions can also be called post-session start. */

bool WM_xr_action_set_create(wmXrData *xr, const char *action_set_name);
void WM_xr_action_set_destroy(wmXrData *xr, const char *action_set_name);
bool WM_xr_action_create(wmXrData *xr,
                         const char *action_set_name,
                         const char *action_name,
                         eXrActionType type,
                         const ListBase *user_paths,
                         struct wmOperatorType *ot,
                         struct IDProperty *op_properties,
                         const char *haptic_name,
                         const int64_t *haptic_duration,
                         const float *haptic_frequency,
                         const float *haptic_amplitude,
                         eXrOpFlag op_flag,
                         eXrActionFlag action_flag,
                         eXrHapticFlag haptic_flag);
void WM_xr_action_destroy(wmXrData *xr, const char *action_set_name, const char *action_name);
bool WM_xr_action_binding_create(wmXrData *xr,
                                 const char *action_set_name,
                                 const char *action_name,
                                 const char *profile_path,
                                 const ListBase *user_paths,
                                 const ListBase *component_paths,
                                 const float *float_thresholds,
                                 const eXrAxisFlag *axis_flags,
                                 const struct wmXrPose *poses);
void WM_xr_action_binding_destroy(wmXrData *xr,
                                  const char *action_set_name,
                                  const char *action_name,
                                  const char *profile_path);

/**
 * If action_set_name is NULL, then all action sets will be treated as active.
 */
bool WM_xr_active_action_set_set(wmXrData *xr, const char *action_set_name, bool delayed);

bool WM_xr_controller_pose_actions_set(wmXrData *xr,
                                       const char *action_set_name,
                                       const char *grip_action_name,
                                       const char *aim_action_name);

/**
 * XR action functions to be called post-XR session start.
 */
bool WM_xr_action_state_get(const wmXrData *xr,
                            const char *action_set_name,
                            const char *action_name,
                            const char *subaction_path,
                            struct wmXrActionState *r_state);
bool WM_xr_haptic_action_apply(wmXrData *xr,
                               const char *action_set_name,
                               const char *action_name,
                               const char *subaction_path,
                               const int64_t *duration,
                               const float *frequency,
                               const float *amplitude);
void WM_xr_haptic_action_stop(wmXrData *xr,
                              const char *action_set_name,
                              const char *action_name,
                              const char *subaction_path);

/* wm_xr_actionmap.c */

XrActionMap *WM_xr_actionmap_new(struct wmXrRuntimeData *runtime,
                                 const char *name,
                                 bool replace_existing);
/**
 * Ensure unique name among all action maps.
 */
void WM_xr_actionmap_ensure_unique(struct wmXrRuntimeData *runtime, XrActionMap *actionmap);
XrActionMap *WM_xr_actionmap_add_copy(struct wmXrRuntimeData *runtime, XrActionMap *am_src);
bool WM_xr_actionmap_remove(struct wmXrRuntimeData *runtime, XrActionMap *actionmap);
XrActionMap *WM_xr_actionmap_find(struct wmXrRuntimeData *runtime, const char *name);
void WM_xr_actionmap_clear(XrActionMap *actionmap);
void WM_xr_actionmaps_clear(struct wmXrRuntimeData *runtime);
ListBase *WM_xr_actionmaps_get(struct wmXrRuntimeData *runtime);
short WM_xr_actionmap_active_index_get(const struct wmXrRuntimeData *runtime);
void WM_xr_actionmap_active_index_set(struct wmXrRuntimeData *runtime, short idx);
short WM_xr_actionmap_selected_index_get(const struct wmXrRuntimeData *runtime);
void WM_xr_actionmap_selected_index_set(struct wmXrRuntimeData *runtime, short idx);

XrActionMapItem *WM_xr_actionmap_item_new(XrActionMap *actionmap,
                                          const char *name,
                                          bool replace_existing);
/**
 * Ensure unique name among all action map items.
 */
void WM_xr_actionmap_item_ensure_unique(XrActionMap *actionmap, XrActionMapItem *ami);
XrActionMapItem *WM_xr_actionmap_item_add_copy(XrActionMap *actionmap, XrActionMapItem *ami_src);
bool WM_xr_actionmap_item_remove(XrActionMap *actionmap, XrActionMapItem *ami);
XrActionMapItem *WM_xr_actionmap_item_find(XrActionMap *actionmap, const char *name);
/**
 * Similar to #wm_xr_actionmap_item_properties_set()
 * but checks for the #eXrActionType and #wmOperatorType having changed.
 */
void WM_xr_actionmap_item_properties_update_ot(XrActionMapItem *ami);

XrActionMapBinding *WM_xr_actionmap_binding_new(XrActionMapItem *ami,
                                                const char *name,
                                                bool replace_existing);
/**
 * Ensure unique name among all action map bindings.
 */
void WM_xr_actionmap_binding_ensure_unique(XrActionMapItem *ami, XrActionMapBinding *amb);
XrActionMapBinding *WM_xr_actionmap_binding_add_copy(XrActionMapItem *ami,
                                                     XrActionMapBinding *amb_src);
bool WM_xr_actionmap_binding_remove(XrActionMapItem *ami, XrActionMapBinding *amb);
XrActionMapBinding *WM_xr_actionmap_binding_find(XrActionMapItem *ami, const char *name);
#endif /* WITH_XR_OPENXR */

#ifdef __cplusplus
}
#endif
