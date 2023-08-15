/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
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

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"
#include "DNA_windowmanager_types.h"
#include "WM_keymap.hh"
#include "WM_types.hh"

struct ARegion;
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
struct View3D;
struct ViewLayer;
struct bContext;
struct rcti;
struct WorkSpace;
struct WorkSpaceLayout;
struct wmDrag;
struct wmDropBox;
struct wmEvent;
struct wmEventHandler_Dropbox;
struct wmEventHandler_Keymap;
struct wmEventHandler_Op;
struct wmEventHandler_UI;
struct wmGenericUserData;
struct wmGesture;
struct wmGizmo;
struct wmGizmoMap;
struct wmGizmoMapType;
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
struct wmXrSessionState;
#endif

namespace blender::asset_system {
class AssetRepresentation;
}

/* General API. */

/**
 * Used for setting app-template from the command line:
 * - non-empty string: overrides.
 * - empty string: override, using no app template.
 * - NULL: clears override.
 */
void WM_init_state_app_template_set(const char *app_template);
const char *WM_init_state_app_template_get();

/**
 * Called when no ghost system was initialized.
 */
void WM_init_state_size_set(int stax, int stay, int sizx, int sizy);
/**
 * For border-less and border windows set from command-line.
 */
void WM_init_state_fullscreen_set();
void WM_init_state_normal_set();
void WM_init_state_maximized_set();
void WM_init_state_start_with_console_set(bool value);
void WM_init_window_focus_set(bool do_it);
void WM_init_native_pixels(bool do_it);
void WM_init_input_devices();

/**
 * Initialize Blender and load the startup file & preferences
 * (only called once).
 */
void WM_init(bContext *C, int argc, const char **argv);
/**
 * Main exit function (implementation).
 *
 * \note Unlike #WM_exit this does not call `exit()`,
 * the caller is responsible for this.
 *
 * \param C: The context or null, a null context implies `do_user_exit_actions == false` &
 * prevents some editor-exit operations from running.
 * \param do_python: Free all data associated with Blender's Python integration.
 * Also exit the Python interpreter (unless `WITH_PYTHON_MODULE` is enabled).
 * \param do_user_exit_actions: When enabled perform actions associated with a user
 * having been using Blender then exiting. Actions such as writing the auto-save
 * and writing any changes to preferences.
 * Set to false in background mode or when exiting because of failed command line argument parsing.
 * In general automated actions where the user isn't making changes should pass in false too.
 */
void WM_exit_ex(bContext *C, bool do_python, bool do_user_exit_actions);

/**
 * Main exit function to close Blender ordinarily.
 *
 * \note Use #wm_exit_schedule_delayed() to close Blender from an operator.
 * Might leak memory otherwise.
 *
 * \param exit_code: Passed to #exit, typically #EXIT_SUCCESS or #EXIT_FAILURE should be used.
 * With failure being used for an early exit when parsing command line arguments fails.
 * Note that any exit-code besides #EXIT_SUCCESS calls #WM_exit_ex with its `do_user_exit_actions`
 * argument set to false.
 */
void WM_exit(bContext *C, int exit_code) ATTR_NORETURN;

void WM_main(bContext *C) ATTR_NORETURN;

/**
 * Show the splash screen as needed on startup.
 *
 * The splash may not show depending on a file being loaded and user preferences.
 */
void WM_init_splash_on_startup(bContext *C);
/**
 * Show the splash screen.
 */
void WM_init_splash(bContext *C);

void WM_init_gpu();

/**
 * Return an identifier for the underlying GHOST implementation.
 * \warning Use of this function should be limited & never for compatibility checks.
 * see: #GHOST_ISystem::getSystemBackend for details.
 */
const char *WM_ghost_backend();

enum eWM_CapabilitiesFlag {
  /** Ability to warp the cursor (set its location). */
  WM_CAPABILITY_CURSOR_WARP = (1 << 0),
  /** Ability to access window positions & move them. */
  WM_CAPABILITY_WINDOW_POSITION = (1 << 1),
  /**
   * The windowing system supports a separate primary clipboard
   * (typically set when interactively selecting text).
   */
  WM_CAPABILITY_PRIMARY_CLIPBOARD = (1 << 2),
  /**
   * Reading from the back-buffer is supported.
   */
  WM_CAPABILITY_GPU_FRONT_BUFFER_READ = (1 << 3),
  /** Ability to copy/paste system clipboard images. */
  WM_CAPABILITY_CLIPBOARD_IMAGES = (1 << 4),
  /** The initial value, indicates the value needs to be set by inspecting GHOST. */
  WM_CAPABILITY_INITIALIZED = (1 << 31),
};
ENUM_OPERATORS(eWM_CapabilitiesFlag, WM_CAPABILITY_CLIPBOARD_IMAGES)

eWM_CapabilitiesFlag WM_capabilities_flag();

void WM_check(bContext *C);
void WM_reinit_gizmomap_all(Main *bmain);

/**
 * Needed for cases when operators are re-registered
 * (when operator type pointers are stored).
 */
void WM_script_tag_reload();

wmWindow *WM_window_find_under_cursor(wmWindow *win, const int mval[2], int r_mval[2]);

/**
 * Knowing the area, return its screen.
 * \note This should typically be avoided, only use when the context is not available.
 */
wmWindow *WM_window_find_by_area(wmWindowManager *wm, const ScrArea *area);

/**
 * Read pixels from the front-buffer (fast).
 *
 * \note Internally this depends on the front-buffer state,
 * for a slower but more reliable method of reading pixels,
 * use #WM_window_pixels_read_from_offscreen.
 * Fast pixel access may be preferred for file-save thumbnails.
 *
 * \warning Drawing (swap-buffers) immediately before calling this function causes
 * the front-buffer state to be invalid under some EGL configurations.
 */
uint8_t *WM_window_pixels_read_from_frontbuffer(const wmWindowManager *wm,
                                                const wmWindow *win,
                                                int r_size[2]);
/** A version of #WM_window_pixels_read_from_frontbuffer that samples a pixel at `pos`. */
void WM_window_pixels_read_sample_from_frontbuffer(const wmWindowManager *wm,
                                                   const wmWindow *win,
                                                   const int pos[2],
                                                   float r_col[3]);

/**
 * Draw the window & read pixels from an off-screen buffer
 * (slower than #WM_window_pixels_read_from_frontbuffer).
 *
 * \note This is needed because the state of the front-buffer may be damaged
 * (see in-line code comments for details).
 */
uint8_t *WM_window_pixels_read_from_offscreen(bContext *C, wmWindow *win, int r_size[2]);
/** A version of #WM_window_pixels_read_from_offscreen that samples a pixel at `pos`. */
bool WM_window_pixels_read_sample_from_offscreen(bContext *C,
                                                 wmWindow *win,
                                                 const int pos[2],
                                                 float r_col[3]);

/**
 * Read from the screen.
 *
 * \note Use off-screen drawing when front-buffer reading is not supported.
 */
uint8_t *WM_window_pixels_read(bContext *C, wmWindow *win, int r_size[2]);
/**
 * Read a single pixel from the screen.
 *
 * \note Use off-screen drawing when front-buffer reading is not supported.
 */
bool WM_window_pixels_read_sample(bContext *C, wmWindow *win, const int pos[2], float r_col[3]);

/**
 * Support for native pixel size
 *
 * \note macOS retina opens window in size X, but it has up to 2 x more pixels.
 */
int WM_window_pixels_x(const wmWindow *win);
int WM_window_pixels_y(const wmWindow *win);
/**
 * Get boundaries usable by all window contents, including global areas.
 */
void WM_window_rect_calc(const wmWindow *win, rcti *r_rect);
/**
 * Get boundaries usable by screen-layouts, excluding global areas.
 * \note Depends on #UI_SCALE_FAC. Should that be outdated, call #WM_window_set_dpi first.
 */
void WM_window_screen_rect_calc(const wmWindow *win, rcti *r_rect);
bool WM_window_is_fullscreen(const wmWindow *win);
bool WM_window_is_maximized(const wmWindow *win);

/**
 * Some editor data may need to be synced with scene data (3D View camera and layers).
 * This function ensures data is synced for editors
 * in visible work-spaces and their visible layouts.
 */
void WM_windows_scene_data_sync(const ListBase *win_lb, Scene *scene) ATTR_NONNULL();
Scene *WM_windows_scene_get_from_screen(const wmWindowManager *wm, const bScreen *screen)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
ViewLayer *WM_windows_view_layer_get_from_screen(const wmWindowManager *wm, const bScreen *screen)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
WorkSpace *WM_windows_workspace_get_from_screen(const wmWindowManager *wm, const bScreen *screen)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

Scene *WM_window_get_active_scene(const wmWindow *win) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
/**
 * \warning Only call outside of area/region loops.
 */
void WM_window_set_active_scene(Main *bmain, bContext *C, wmWindow *win, Scene *scene_new)
    ATTR_NONNULL();
WorkSpace *WM_window_get_active_workspace(const wmWindow *win)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void WM_window_set_active_workspace(bContext *C, wmWindow *win, WorkSpace *workspace)
    ATTR_NONNULL(1);
WorkSpaceLayout *WM_window_get_active_layout(const wmWindow *win)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void WM_window_set_active_layout(wmWindow *win, WorkSpace *workspace, WorkSpaceLayout *layout)
    ATTR_NONNULL(1);
/**
 * Get the active screen of the active workspace in \a win.
 */
bScreen *WM_window_get_active_screen(const wmWindow *win) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void WM_window_set_active_screen(wmWindow *win, WorkSpace *workspace, bScreen *screen)
    ATTR_NONNULL(1);

ViewLayer *WM_window_get_active_view_layer(const wmWindow *win)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
void WM_window_set_active_view_layer(wmWindow *win, ViewLayer *view_layer) ATTR_NONNULL(1);
void WM_window_ensure_active_view_layer(wmWindow *win) ATTR_NONNULL(1);

bool WM_window_is_temp_screen(const wmWindow *win) ATTR_WARN_UNUSED_RESULT;

void *WM_system_gpu_context_create();
void WM_system_gpu_context_dispose(void *context);
void WM_system_gpu_context_activate(void *context);
void WM_system_gpu_context_release(void *context);

/* #WM_window_open alignment */
enum eWindowAlignment {
  WIN_ALIGN_ABSOLUTE = 0,
  WIN_ALIGN_LOCATION_CENTER,
  WIN_ALIGN_PARENT_CENTER,
};

/**
 * \param rect: Position & size of the window.
 * \param space_type: #SPACE_VIEW3D, #SPACE_INFO, ... (#eSpace_Type).
 * \param toplevel: Not a child owned by other windows. A peer of main window.
 * \param dialog: whether this should be made as a dialog-style window
 * \param temp: whether this is considered a short-lived window
 * \param alignment: how this window is positioned relative to its parent
 * \param area_setup_fn: An optional callback which can be used to initialize the area
 * before it's initialized. When set, `space_type` should be #SPACE_EMTPY,
 * so the setup function can take a blank area and initialize it.
 * \param area_setup_user_data: User data argument passed to `area_setup_fn`.
 * \return the window or NULL in case of failure.
 */
wmWindow *WM_window_open(bContext *C,
                         const char *title,
                         const rcti *rect_unscaled,
                         int space_type,
                         bool toplevel,
                         bool dialog,
                         bool temp,
                         eWindowAlignment alignment,
                         void (*area_setup_fn)(bScreen *screen, ScrArea *area, void *user_data),
                         void *area_setup_user_data) ATTR_NONNULL(1, 2, 3);

void WM_window_set_dpi(const wmWindow *win);

bool WM_stereo3d_enabled(wmWindow *win, bool only_fullscreen_test);

/* wm_files.cc */

void WM_file_autoexec_init(const char *filepath);
bool WM_file_read(bContext *C, const char *filepath, ReportList *reports);
void WM_file_autosave_init(wmWindowManager *wm);
bool WM_file_recover_last_session(bContext *C, ReportList *reports);
void WM_file_tag_modified();

/**
 * \note `scene` (and related `view_layer` and `v3d`) pointers may be NULL,
 * in which case no instantiation of linked objects, collections etc. will be performed.
 */
ID *WM_file_link_datablock(Main *bmain,
                           Scene *scene,
                           ViewLayer *view_layer,
                           View3D *v3d,
                           const char *filepath,
                           short id_code,
                           const char *id_name,
                           int flag);
/**
 * \note `scene` (and related `view_layer` and `v3d`) pointers may be NULL,
 * in which case no instantiation of appended objects, collections etc. will be performed.
 */
ID *WM_file_append_datablock(Main *bmain,
                             Scene *scene,
                             ViewLayer *view_layer,
                             View3D *v3d,
                             const char *filepath,
                             short id_code,
                             const char *id_name,
                             int flag);
void WM_lib_reload(Library *lib, bContext *C, ReportList *reports);

/* Mouse cursors. */

void WM_cursor_set(wmWindow *win, int curs);
bool WM_cursor_set_from_tool(wmWindow *win, const ScrArea *area, const ARegion *region);
void WM_cursor_modal_set(wmWindow *win, int val);
void WM_cursor_modal_restore(wmWindow *win);
/**
 * To allow usage all over, we do entire WM.
 */
void WM_cursor_wait(bool val);
/**
 * Enable cursor grabbing, optionally hiding the cursor and wrapping cursor-motion
 * within a sub-region of the window.
 *
 * \param wrap: an enum (#WM_CURSOR_WRAP_NONE, #WM_CURSOR_WRAP_XY... etc).
 * \param wrap_region: Window-relative region for cursor wrapping (when `wrap` is
 * #WM_CURSOR_WRAP_XY). When NULL, the window bounds are used for wrapping.
 *
 * \note The current grab state can be accessed by #wmWindowManager.grabcursor although.
 */
void WM_cursor_grab_enable(wmWindow *win,
                           eWM_CursorWrapAxis wrap,
                           const rcti *wrap_region,
                           bool hide);
void WM_cursor_grab_disable(wmWindow *win, const int mouse_ungrab_xy[2]);
/**
 * After this you can call restore too.
 */
void WM_cursor_time(wmWindow *win, int nr);

wmPaintCursor *WM_paint_cursor_activate(short space_type,
                                        short region_type,
                                        bool (*poll)(bContext *C),
                                        void (*draw)(bContext *C, int, int, void *customdata),
                                        void *customdata);

bool WM_paint_cursor_end(wmPaintCursor *handle);
void WM_paint_cursor_remove_by_type(wmWindowManager *wm, void *draw_fn, void (*free)(void *));
void WM_paint_cursor_tag_redraw(wmWindow *win, ARegion *region);

/**
 * Set the cursor location in window coordinates (compatible with #wmEvent.xy).
 *
 * \note Some platforms don't support this, check: #WM_CAPABILITY_WINDOW_POSITION
 * before relying on this functionality.
 */
void WM_cursor_warp(wmWindow *win, int x, int y);

/* Handlers. */

enum eWM_EventHandlerFlag {
  /** After this handler all others are ignored. */
  WM_HANDLER_BLOCKING = (1 << 0),
  /** Handler accepts double key press events. */
  WM_HANDLER_ACCEPT_DBL_CLICK = (1 << 1),

  /* Internal. */
  /** Handler tagged to be freed in #wm_handlers_do(). */
  WM_HANDLER_DO_FREE = (1 << 7),
};
ENUM_OPERATORS(eWM_EventHandlerFlag, WM_HANDLER_DO_FREE)

using EventHandlerPoll = bool (*)(const ARegion *region, const wmEvent *event);
wmEventHandler_Keymap *WM_event_add_keymap_handler(ListBase *handlers, wmKeyMap *keymap);
wmEventHandler_Keymap *WM_event_add_keymap_handler_poll(ListBase *handlers,
                                                        wmKeyMap *keymap,
                                                        EventHandlerPoll poll);
wmEventHandler_Keymap *WM_event_add_keymap_handler_v2d_mask(ListBase *handlers, wmKeyMap *keymap);
/**
 * \note Priorities not implemented yet, for time being just insert in begin of list.
 */
wmEventHandler_Keymap *WM_event_add_keymap_handler_priority(ListBase *handlers,
                                                            wmKeyMap *keymap,
                                                            int priority);

struct wmEventHandler_KeymapResult {
  wmKeyMap *keymaps[3];
  int keymaps_len;
};

using wmEventHandler_KeymapDynamicFn = void (*)(wmWindowManager *wm,
                                                wmWindow *win,
                                                wmEventHandler_Keymap *handler,
                                                wmEventHandler_KeymapResult *km_result);

void WM_event_get_keymap_from_toolsystem_with_gizmos(wmWindowManager *wm,
                                                     wmWindow *win,
                                                     wmEventHandler_Keymap *handler,
                                                     wmEventHandler_KeymapResult *km_result);
void WM_event_get_keymap_from_toolsystem(wmWindowManager *wm,
                                         wmWindow *win,
                                         wmEventHandler_Keymap *handler,
                                         wmEventHandler_KeymapResult *km_result);

wmEventHandler_Keymap *WM_event_add_keymap_handler_dynamic(
    ListBase *handlers, wmEventHandler_KeymapDynamicFn keymap_fn, void *user_data);

void WM_event_remove_keymap_handler(ListBase *handlers, wmKeyMap *keymap);

void WM_event_set_keymap_handler_post_callback(wmEventHandler_Keymap *handler,
                                               void(keymap_tag)(wmKeyMap *keymap,
                                                                wmKeyMapItem *kmi,
                                                                void *user_data),
                                               void *user_data);
void WM_event_get_keymaps_from_handler(wmWindowManager *wm,
                                       wmWindow *win,
                                       wmEventHandler_Keymap *handler,
                                       wmEventHandler_KeymapResult *km_result);

wmKeyMapItem *WM_event_match_keymap_item(bContext *C, wmKeyMap *keymap, const wmEvent *event);

wmKeyMapItem *WM_event_match_keymap_item_from_handlers(
    bContext *C, wmWindowManager *wm, wmWindow *win, ListBase *handlers, const wmEvent *event);

bool WM_event_match(const wmEvent *winevent, const wmKeyMapItem *kmi);

using wmUIHandlerFunc = int (*)(bContext *C, const wmEvent *event, void *userdata);
using wmUIHandlerRemoveFunc = void (*)(bContext *C, void *userdata);

wmEventHandler_UI *WM_event_add_ui_handler(const bContext *C,
                                           ListBase *handlers,
                                           wmUIHandlerFunc handle_fn,
                                           wmUIHandlerRemoveFunc remove_fn,
                                           void *user_data,
                                           eWM_EventHandlerFlag flag);

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
void WM_event_remove_area_handler(ListBase *handlers, void *area);
void WM_event_free_ui_handler_all(bContext *C,
                                  ListBase *handlers,
                                  wmUIHandlerFunc handle_fn,
                                  wmUIHandlerRemoveFunc remove_fn);

/**
 * Add a modal handler to `win`, `area` and `region` may optionally be NULL.
 */
wmEventHandler_Op *WM_event_add_modal_handler_ex(wmWindow *win,
                                                 ScrArea *area,
                                                 ARegion *region,
                                                 wmOperator *op) ATTR_NONNULL(1, 4);
wmEventHandler_Op *WM_event_add_modal_handler(bContext *C, wmOperator *op) ATTR_NONNULL(1, 2);
/**
 * Modal handlers store a pointer to an area which might be freed while the handler runs.
 * Use this function to NULL all handler pointers to \a old_area.
 */
void WM_event_modal_handler_area_replace(wmWindow *win,
                                         const ScrArea *old_area,
                                         ScrArea *new_area);
/**
 * Modal handlers store a pointer to a region which might be freed while the handler runs.
 * Use this function to NULL all handler pointers to \a old_region.
 */
void WM_event_modal_handler_region_replace(wmWindow *win,
                                           const ARegion *old_region,
                                           ARegion *new_region);

/**
 * Called on exit or remove area, only here call cancel callback.
 */
void WM_event_remove_handlers(bContext *C, ListBase *handlers);

wmEventHandler_Dropbox *WM_event_add_dropbox_handler(ListBase *handlers, ListBase *dropboxes);

/* mouse */
void WM_event_add_mousemove(wmWindow *win);

#ifdef WITH_INPUT_NDOF
/* 3D mouse */
void WM_ndof_deadzone_set(float deadzone);
#endif
/* notifiers */
void WM_event_add_notifier_ex(wmWindowManager *wm,
                              const wmWindow *win,
                              unsigned int type,
                              void *reference);
void WM_event_add_notifier(const bContext *C, unsigned int type, void *reference);
void WM_main_add_notifier(unsigned int type, void *reference);
/**
 * Clear notifiers by reference, Used so listeners don't act on freed data.
 */
void WM_main_remove_notifier_reference(const void *reference);
void WM_main_remap_editor_id_reference(const IDRemapper *mappings);

/* reports */
/**
 * Show the report in the info header.
 * \param win: When NULL, a best-guess is used.
 */
void WM_report_banner_show(wmWindowManager *wm, wmWindow *win) ATTR_NONNULL(1);
/**
 * Hide all currently displayed banners and abort their timer.
 */
void WM_report_banners_cancel(Main *bmain);
void WM_report(eReportType type, const char *message);
void WM_reportf(eReportType type, const char *format, ...) ATTR_PRINTF_FORMAT(2, 3);

wmEvent *wm_event_add_ex(wmWindow *win,
                         const wmEvent *event_to_add,
                         const wmEvent *event_to_add_after) ATTR_NONNULL(1, 2);
wmEvent *wm_event_add(wmWindow *win, const wmEvent *event_to_add) ATTR_NONNULL(1, 2);

void wm_event_init_from_window(wmWindow *win, wmEvent *event);

/* at maximum, every timestep seconds it triggers event_type events */
wmTimer *WM_event_timer_add(wmWindowManager *wm, wmWindow *win, int event_type, double timestep);
wmTimer *WM_event_timer_add_notifier(wmWindowManager *wm,
                                     wmWindow *win,
                                     unsigned int type,
                                     double timestep);

void WM_event_timer_free_data(wmTimer *timer);
/**
 * Free all timers immediately.
 *
 * \note This should only be used on-exit,
 * in all other cases timers should be tagged for removal by #WM_event_timer_remove.
 */
void WM_event_timers_free_all(wmWindowManager *wm);

/** Mark the given `timer` to be removed, actual removal and deletion is deferred and handled
 * internally by the window manager code. */
void WM_event_timer_remove(wmWindowManager *wm, wmWindow *win, wmTimer *timer);
void WM_event_timer_remove_notifier(wmWindowManager *wm, wmWindow *win, wmTimer *timer);
/**
 * To (de)activate running timers temporary.
 */
void WM_event_timer_sleep(wmWindowManager *wm, wmWindow *win, wmTimer *timer, bool do_sleep);

/* Operator API, default callbacks. */

/**
 * Helper to get select and tweak-transform to work conflict free and as desired. See
 * #WM_operator_properties_generic_select() for details.
 *
 * To be used together with #WM_generic_select_invoke() and
 * #WM_operator_properties_generic_select().
 */
int WM_generic_select_modal(bContext *C, wmOperator *op, const wmEvent *event);
/**
 * Helper to get select and tweak-transform to work conflict free and as desired. See
 * #WM_operator_properties_generic_select() for details.
 *
 * To be used together with #WM_generic_select_modal() and
 * #WM_operator_properties_generic_select().
 */
int WM_generic_select_invoke(bContext *C, wmOperator *op, const wmEvent *event);
void WM_operator_view3d_unit_defaults(bContext *C, wmOperator *op);
int WM_operator_smooth_viewtx_get(const wmOperator *op);
/**
 * Invoke callback, uses enum property named "type".
 */
int WM_menu_invoke_ex(bContext *C, wmOperator *op, wmOperatorCallContext opcontext);
int WM_menu_invoke(bContext *C, wmOperator *op, const wmEvent *event);
/**
 * Call an existent menu. The menu can be created in C or Python.
 */
void WM_menu_name_call(bContext *C, const char *menu_name, short context);
/**
 * Similar to #WM_enum_search_invoke, but draws previews. Also, this can't
 * be used as invoke callback directly since it needs additional info.
 */
int WM_enum_search_invoke_previews(bContext *C, wmOperator *op, short prv_cols, short prv_rows);
int WM_enum_search_invoke(bContext *C, wmOperator *op, const wmEvent *event);
/**
 * Invoke callback, confirm menu + exec.
 */
int WM_operator_confirm(bContext *C, wmOperator *op, const wmEvent *event);
int WM_operator_confirm_or_exec(bContext *C, wmOperator *op, const wmEvent *event);
/**
 * Invoke callback, file selector "filepath" unset + exec.
 *
 * #wmOperatorType.invoke, opens file-select if path property not set, otherwise executes.
 */
int WM_operator_filesel(bContext *C, wmOperator *op, const wmEvent *event);
bool WM_operator_filesel_ensure_ext_imtype(wmOperator *op, const ImageFormatData *im_format);
/** Callback for #wmOperatorType.poll */
bool WM_operator_winactive(bContext *C);
/**
 * Invoke callback, exec + redo popup.
 *
 * Same as #WM_operator_props_popup but don't use operator redo.
 * just wraps #WM_operator_props_dialog_popup.
 */
int WM_operator_props_popup_confirm(bContext *C, wmOperator *op, const wmEvent *event);
/**
 * Same as #WM_operator_props_popup but call the operator first,
 * This way - the button values correspond to the result of the operator.
 * Without this, first access to a button will make the result jump, see #32452.
 */
int WM_operator_props_popup_call(bContext *C, wmOperator *op, const wmEvent *event);
int WM_operator_props_popup(bContext *C, wmOperator *op, const wmEvent *event);
int WM_operator_props_dialog_popup(bContext *C, wmOperator *op, int width);
int WM_operator_redo_popup(bContext *C, wmOperator *op);
int WM_operator_ui_popup(bContext *C, wmOperator *op, int width);

/**
 * Can't be used as an invoke directly, needs message arg (can be NULL).
 */
int WM_operator_confirm_message_ex(bContext *C,
                                   wmOperator *op,
                                   const char *title,
                                   int icon,
                                   const char *message,
                                   wmOperatorCallContext opcontext);
int WM_operator_confirm_message(bContext *C, wmOperator *op, const char *message);

/* Operator API. */

void WM_operator_free(wmOperator *op);
void WM_operator_free_all_after(wmWindowManager *wm, wmOperator *op);
/**
 * Use with extreme care!
 * Properties, custom-data etc - must be compatible.
 *
 * \param op: Operator to assign the type to.
 * \param ot: Operator type to assign.
 */
void WM_operator_type_set(wmOperator *op, wmOperatorType *ot);
void WM_operator_stack_clear(wmWindowManager *wm);
/**
 * This function is needed in the case when an addon id disabled
 * while a modal operator it defined is running.
 */
void WM_operator_handlers_clear(wmWindowManager *wm, wmOperatorType *ot);

bool WM_operator_poll(bContext *C, wmOperatorType *ot);
bool WM_operator_poll_context(bContext *C, wmOperatorType *ot, short context);
/**
 * For running operators with frozen context (modal handlers, menus).
 *
 * \param store: Store properties for re-use when an operator has finished
 * (unless #PROP_SKIP_SAVE is set).
 *
 * \warning do not use this within an operator to call itself! #29537.
 */
int WM_operator_call_ex(bContext *C, wmOperator *op, bool store);
int WM_operator_call(bContext *C, wmOperator *op);
/**
 * This is intended to be used when an invoke operator wants to call exec on itself
 * and is basically like running op->type->exec() directly, no poll checks no freeing,
 * since we assume whoever called invoke will take care of that
 */
int WM_operator_call_notest(bContext *C, wmOperator *op);
/**
 * Execute this operator again, put here so it can share above code
 */
int WM_operator_repeat(bContext *C, wmOperator *op);
int WM_operator_repeat_last(bContext *C, wmOperator *op);
/**
 * \return true if #WM_operator_repeat can run.
 * Simple check for now but may become more involved.
 * To be sure the operator can run call `WM_operator_poll(C, op->type)` also, since this call
 * checks if #WM_operator_repeat() can run at all, not that it WILL run at any time.
 */
bool WM_operator_repeat_check(const bContext *C, wmOperator *op);
bool WM_operator_is_repeat(const bContext *C, const wmOperator *op);

bool WM_operator_name_poll(bContext *C, const char *opstring);
/**
 * Invokes operator in context.
 *
 * \param event: Optionally pass in an event to use when context uses one of the
 * `WM_OP_INVOKE_*` values. When left unset the #wmWindow.eventstate will be used,
 * this can cause problems for operators that read the events type - for example,
 * storing the key that was pressed so as to be able to detect its release.
 * In these cases it's necessary to forward the current event being handled.
 */
int WM_operator_name_call_ptr(bContext *C,
                              wmOperatorType *ot,
                              wmOperatorCallContext context,
                              PointerRNA *properties,
                              const wmEvent *event);
/** See #WM_operator_name_call_ptr */
int WM_operator_name_call(bContext *C,
                          const char *opstring,
                          wmOperatorCallContext context,
                          PointerRNA *properties,
                          const wmEvent *event);
int WM_operator_name_call_with_properties(bContext *C,
                                          const char *opstring,
                                          wmOperatorCallContext context,
                                          IDProperty *properties,
                                          const wmEvent *event);
/**
 * Similar to #WM_operator_name_call called with #WM_OP_EXEC_DEFAULT context.
 *
 * - #wmOperatorType is used instead of operator name since python already has the operator type.
 * - `poll()` must be called by python before this runs.
 * - reports can be passed to this function (so python can report them as exceptions).
 */
int WM_operator_call_py(bContext *C,
                        wmOperatorType *ot,
                        wmOperatorCallContext context,
                        PointerRNA *properties,
                        ReportList *reports,
                        bool is_undo);

void WM_operator_name_call_ptr_with_depends_on_cursor(bContext *C,
                                                      wmOperatorType *ot,
                                                      wmOperatorCallContext opcontext,
                                                      PointerRNA *properties,
                                                      const wmEvent *event,
                                                      const char *drawstr);

/**
 * Similar to the function above except its uses ID properties used for key-maps and macros.
 */
void WM_operator_properties_alloc(PointerRNA **ptr, IDProperty **properties, const char *opstring);

/**
 * Make props context sensitive or not.
 */
void WM_operator_properties_sanitize(PointerRNA *ptr, bool no_context);

/**
 * Set all props to their default.
 *
 * \param do_update: Only update un-initialized props.
 *
 * \note There's nothing specific to operators here.
 * This could be made a general function.
 */
bool WM_operator_properties_default(PointerRNA *ptr, bool do_update);
/**
 * Remove all props without #PROP_SKIP_SAVE.
 */
void WM_operator_properties_reset(wmOperator *op);
void WM_operator_properties_create(PointerRNA *ptr, const char *opstring);
void WM_operator_properties_create_ptr(PointerRNA *ptr, wmOperatorType *ot);
void WM_operator_properties_clear(PointerRNA *ptr);
void WM_operator_properties_free(PointerRNA *ptr);

bool WM_operator_check_ui_empty(wmOperatorType *ot);
/**
 * Return false, if the UI should be disabled.
 */
bool WM_operator_check_ui_enabled(const bContext *C, const char *idname);

IDProperty *WM_operator_last_properties_ensure_idprops(wmOperatorType *ot);
void WM_operator_last_properties_ensure(wmOperatorType *ot, PointerRNA *ptr);
wmOperator *WM_operator_last_redo(const bContext *C);
/**
 * Use for drag & drop a path or name with operators invoke() function.
 * Returns null if no operator property is set to identify the file or ID to use.
 */
ID *WM_operator_drop_load_path(bContext *C, wmOperator *op, short idcode);

bool WM_operator_last_properties_init(wmOperator *op);
bool WM_operator_last_properties_store(wmOperator *op);

/* `wm_operator_props.cc` */

void WM_operator_properties_confirm_or_exec(wmOperatorType *ot);

/** Flags for #WM_operator_properties_filesel. */
enum eFileSel_Flag {
  WM_FILESEL_RELPATH = 1 << 0,
  WM_FILESEL_DIRECTORY = 1 << 1,
  WM_FILESEL_FILENAME = 1 << 2,
  WM_FILESEL_FILEPATH = 1 << 3,
  WM_FILESEL_FILES = 1 << 4,
  /** Show the properties sidebar by default. */
  WM_FILESEL_SHOW_PROPS = 1 << 5,
};
ENUM_OPERATORS(eFileSel_Flag, WM_FILESEL_SHOW_PROPS)

/** Action for #WM_operator_properties_filesel. */
enum eFileSel_Action {
  FILE_OPENFILE = 0,
  FILE_SAVE = 1,
};

/**
 * Default properties for file-select.
 */
void WM_operator_properties_filesel(wmOperatorType *ot,
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
ID *WM_operator_properties_id_lookup_from_name_or_session_uuid(Main *bmain,
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
void WM_operator_properties_use_cursor_init(wmOperatorType *ot);
void WM_operator_properties_border(wmOperatorType *ot);
void WM_operator_properties_border_to_rcti(wmOperator *op, rcti *rect);
void WM_operator_properties_border_to_rctf(wmOperator *op, rctf *rect);
/**
 * Use with #WM_gesture_box_invoke
 */
void WM_operator_properties_gesture_box_ex(wmOperatorType *ot, bool deselect, bool extend);
void WM_operator_properties_gesture_box(wmOperatorType *ot);
void WM_operator_properties_gesture_box_select(wmOperatorType *ot);
void WM_operator_properties_gesture_box_zoom(wmOperatorType *ot);
/**
 * Use with #WM_gesture_lasso_invoke
 */
void WM_operator_properties_gesture_lasso(wmOperatorType *ot);
/**
 * Use with #WM_gesture_straightline_invoke
 */
void WM_operator_properties_gesture_straightline(wmOperatorType *ot, int cursor);
/**
 * Use with #WM_gesture_circle_invoke
 */
void WM_operator_properties_gesture_circle(wmOperatorType *ot);
/**
 * See #ED_select_pick_params_from_operator to initialize parameters defined here.
 */
void WM_operator_properties_mouse_select(wmOperatorType *ot);
void WM_operator_properties_select_all(wmOperatorType *ot);
void WM_operator_properties_select_action(wmOperatorType *ot, int default_action, bool hide_gui);
/**
 * Only for select/de-select.
 */
void WM_operator_properties_select_action_simple(wmOperatorType *ot,
                                                 int default_action,
                                                 bool hide_gui);
/**
 * Use for all select random operators.
 * Adds properties: percent, seed, action.
 */
void WM_operator_properties_select_random(wmOperatorType *ot);
int WM_operator_properties_select_random_seed_increment_get(wmOperator *op);
void WM_operator_properties_select_operation(wmOperatorType *ot);
/**
 * \note Some tools don't support XOR/AND.
 */
void WM_operator_properties_select_operation_simple(wmOperatorType *ot);
void WM_operator_properties_select_walk_direction(wmOperatorType *ot);
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
void WM_operator_properties_generic_select(wmOperatorType *ot);

struct CheckerIntervalParams {
  int nth; /* bypass when set to zero */
  int skip;
  int offset;
};
/**
 * \param nth_can_disable: Enable if we want to be able to select no interval at all.
 */
void WM_operator_properties_checker_interval(wmOperatorType *ot, bool nth_can_disable);
void WM_operator_properties_checker_interval_from_op(wmOperator *op,
                                                     CheckerIntervalParams *op_params);
bool WM_operator_properties_checker_interval_test(const CheckerIntervalParams *op_params,
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
char *WM_operator_pystring_ex(bContext *C,
                              wmOperator *op,
                              bool all_args,
                              bool macro_args,
                              wmOperatorType *ot,
                              PointerRNA *opptr);
char *WM_operator_pystring(bContext *C, wmOperator *op, bool all_args, bool macro_args);
/**
 * \return true if the string was shortened.
 */
bool WM_operator_pystring_abbreviate(char *str, int str_len_max);
char *WM_prop_pystring_assign(bContext *C, PointerRNA *ptr, PropertyRNA *prop, int index);
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
bool WM_operator_py_idname_ok_or_report(ReportList *reports,
                                        const char *classname,
                                        const char *idname);
/**
 * Calculate the path to `ptr` from context `C`, or return NULL if it can't be calculated.
 */
char *WM_context_path_resolve_property_full(const bContext *C,
                                            const PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            int index);
char *WM_context_path_resolve_full(bContext *C, const PointerRNA *ptr);

/* `wm_operator_type.cc` */

wmOperatorType *WM_operatortype_find(const char *idname, bool quiet);
/**
 * \note Caller must free.
 */
void WM_operatortype_iter(GHashIterator *ghi);
void WM_operatortype_append(void (*opfunc)(wmOperatorType *));
void WM_operatortype_append_ptr(void (*opfunc)(wmOperatorType *, void *), void *userdata);
void WM_operatortype_append_macro_ptr(void (*opfunc)(wmOperatorType *, void *), void *userdata);
/**
 * Called on initialize WM_exit().
 */
void WM_operatortype_remove_ptr(wmOperatorType *ot);
bool WM_operatortype_remove(const char *idname);
/**
 * Remove memory of all previously executed tools.
 */
void WM_operatortype_last_properties_clear_all();

void WM_operatortype_idname_visit_for_search(const bContext *C,
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
void WM_operatortype_props_advanced_begin(wmOperatorType *ot);
/**
 * Tags all operator-properties of \a ot defined since the first
 * #WM_operatortype_props_advanced_begin call,
 * or the last #WM_operatortype_props_advanced_end call, with #OP_PROP_TAG_ADVANCED.
 *
 * \note This is called for all operators during registration (see #wm_operatortype_append__end).
 * So it does not need to be explicitly called in operator-type definition.
 */
void WM_operatortype_props_advanced_end(wmOperatorType *ot);

#define WM_operatortype_prop_tag(property, tags) \
  { \
    CHECK_TYPE(tags, eOperatorPropTags); \
    RNA_def_property_tags(prop, tags); \
  } \
  (void)0

/**
 * \note Names have to be static for now.
 */
wmOperatorType *WM_operatortype_append_macro(const char *idname,
                                             const char *name,
                                             const char *description,
                                             int flag);
wmOperatorTypeMacro *WM_operatortype_macro_define(wmOperatorType *ot, const char *idname);

std::string WM_operatortype_name(wmOperatorType *ot, PointerRNA *properties);
std::string WM_operatortype_description(bContext *C, wmOperatorType *ot, PointerRNA *properties);
/**
 * Use when we want a label, preferring the description.
 */
std::string WM_operatortype_description_or_name(bContext *C,
                                                wmOperatorType *ot,
                                                PointerRNA *properties);

/* `wm_operator_utils.cc` */

/**
 * Allow an operator with only and execute function to run modally,
 * re-doing the action, using vertex coordinate store/restore instead of operator undo.
 */
void WM_operator_type_modal_from_exec_for_object_edit_coords(wmOperatorType *ot);

/* `wm_uilist_type.cc` */

/**
 * Called on initialize #WM_init()
 */
void WM_uilisttype_init();
uiListType *WM_uilisttype_find(const char *idname, bool quiet);
bool WM_uilisttype_add(uiListType *ult);
void WM_uilisttype_remove_ptr(Main *bmain, uiListType *ult);
void WM_uilisttype_free();

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
void WM_uilisttype_to_full_list_id(const uiListType *ult,
                                   const char *list_id,
                                   char r_full_list_id[]);
/**
 * Get the "non-full" list-ID, see #WM_uilisttype_to_full_list_id() for details.
 *
 * \note Assumes `uiList.list_id` was set using #WM_uilisttype_to_full_list_id()!
 */
const char *WM_uilisttype_list_id_get(const uiListType *ult, uiList *list);

/* `wm_menu_type.cc` */

/**
 * \note Called on initialize #WM_init().
 */
void WM_menutype_init();
MenuType *WM_menutype_find(const char *idname, bool quiet);
void WM_menutype_iter(GHashIterator *ghi);
bool WM_menutype_add(MenuType *mt);
void WM_menutype_freelink(MenuType *mt);
void WM_menutype_free();
bool WM_menutype_poll(bContext *C, MenuType *mt);

void WM_menutype_idname_visit_for_search(const bContext *C,
                                         PointerRNA *ptr,
                                         PropertyRNA *prop,
                                         const char *edit_text,
                                         StringPropertySearchVisitFunc visit_fn,
                                         void *visit_user_data);

/* `wm_panel_type.cc` */

/**
 * Called on initialize #WM_init().
 */
void WM_paneltype_init();
void WM_paneltype_clear();
PanelType *WM_paneltype_find(const char *idname, bool quiet);
bool WM_paneltype_add(PanelType *pt);
void WM_paneltype_remove(PanelType *pt);

void WM_paneltype_idname_visit_for_search(const bContext *C,
                                          PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          const char *edit_text,
                                          StringPropertySearchVisitFunc visit_fn,
                                          void *visit_user_data);

/* `wm_gesture_ops.cc` */

int WM_gesture_box_invoke(bContext *C, wmOperator *op, const wmEvent *event);
int WM_gesture_box_modal(bContext *C, wmOperator *op, const wmEvent *event);
void WM_gesture_box_cancel(bContext *C, wmOperator *op);
int WM_gesture_circle_invoke(bContext *C, wmOperator *op, const wmEvent *event);
int WM_gesture_circle_modal(bContext *C, wmOperator *op, const wmEvent *event);
void WM_gesture_circle_cancel(bContext *C, wmOperator *op);
int WM_gesture_lines_invoke(bContext *C, wmOperator *op, const wmEvent *event);
int WM_gesture_lines_modal(bContext *C, wmOperator *op, const wmEvent *event);
void WM_gesture_lines_cancel(bContext *C, wmOperator *op);
int WM_gesture_lasso_invoke(bContext *C, wmOperator *op, const wmEvent *event);
int WM_gesture_lasso_modal(bContext *C, wmOperator *op, const wmEvent *event);
void WM_gesture_lasso_cancel(bContext *C, wmOperator *op);
/**
 * helper function, we may want to add options for conversion to view space
 *
 * caller must free.
 */
const int (*WM_gesture_lasso_path_to_array(bContext *C, wmOperator *op, int *mcoords_len))[2];

int WM_gesture_straightline_invoke(bContext *C, wmOperator *op, const wmEvent *event);
/**
 * This invoke callback starts the straight-line gesture with a viewport preview to the right side
 * of the line.
 */
int WM_gesture_straightline_active_side_invoke(bContext *C, wmOperator *op, const wmEvent *event);
/**
 * This modal callback calls exec once per mouse move event while the gesture is active with the
 * updated line start and end values, so it can be used for tools that have a real time preview
 * (like a gradient updating in real time over the mesh).
 */
int WM_gesture_straightline_modal(bContext *C, wmOperator *op, const wmEvent *event);
/**
 * This modal one-shot callback only calls exec once after the gesture finishes without any updates
 * during the gesture execution. Should be used for operations that are intended to be applied once
 * without real time preview (like a trimming tool that only applies the bisect operation once
 * after finishing the gesture as the bisect operation is too heavy to be computed in real time for
 * a preview).
 */
int WM_gesture_straightline_oneshot_modal(bContext *C, wmOperator *op, const wmEvent *event);
void WM_gesture_straightline_cancel(bContext *C, wmOperator *op);

/* Gesture manager API */

/**
 * Context checked on having screen, window and area.
 */
wmGesture *WM_gesture_new(wmWindow *window, const ARegion *region, const wmEvent *event, int type);
void WM_gesture_end(wmWindow *win, wmGesture *gesture);
void WM_gestures_remove(wmWindow *win);
void WM_gestures_free_all(wmWindow *win);
bool WM_gesture_is_modal_first(const wmGesture *gesture);

/* File-selecting support. */

/**
 * The idea here is to keep a handler alive on window queue, owning the operator.
 * The file window can send event to make it execute, thus ensuring
 * executing happens outside of lower level queues, with UI refreshed.
 * Should also allow multi-window solutions.
 */
void WM_event_add_fileselect(bContext *C, wmOperator *op);
void WM_event_fileselect_event(wmWindowManager *wm, void *ophandle, int eventval);

/* Event consecutive data. */

/** Return a borrowed reference to the custom-data. */
void *WM_event_consecutive_data_get(wmWindow *win, const char *id);
/** Set the custom-data (and own the pointer), free with #MEM_freeN. */
void WM_event_consecutive_data_set(wmWindow *win, const char *id, void *custom_data);
/** Clear and free the consecutive custom-data. */
void WM_event_consecutive_data_free(wmWindow *win);

/**
 * Sets the active region for this space from the context.
 *
 * \see #BKE_area_find_region_active_win
 */
void WM_operator_region_active_win_set(bContext *C);

/**
 * Indented for use in a selection (picking) operators #wmOperatorType::invoke callback
 * to implement click-drag, where the initial click selects and the drag action
 * grabs or performs box-select (for example).
 *
 * - In this case, returning `OPERATOR_FINISHED` causes the PRESS event
 *   to be handled and prevents further CLICK (on release) or DRAG (on cursor motion)
 *   from being generated & handled.
 *
 * - Returning `OPERATOR_FINISHED | OPERATOR_PASS_THROUGH` allows for CLICK/DRAG but only makes
 *   sense if the event's value is PRESS. If the operator was already mapped to a CLICK/DRAG event,
 *   a single CLICK/DRAG could invoke multiple operators.
 *
 * This function handles the details of checking the operator return value,
 * clearing #OPERATOR_PASS_THROUGH when the #wmEvent::val is not #KM_PRESS.
 *
 * \note Combining selection with other actions should only be used
 * in situations where selecting doesn't change the visibility of other items.
 * Since it means for example click-drag to box select could hide-show elements the user
 * intended to box-select. In this case it's preferred to select on CLICK instead of PRESS
 * (see the Outliner use of click-drag).
 */
int WM_operator_flag_only_pass_through_on_press(int retval, const wmEvent *event);

/* Drag and drop. */

/**
 * Start dragging immediately with the given data.
 * Note that \a poin should be valid allocated and not on stack.
 */
void WM_event_start_drag(
    bContext *C, int icon, eWM_DragDataType type, void *poin, double value, unsigned int flags);
/**
 * Create and fill the dragging data, but don't start dragging just yet (unlike
 * #WM_event_start_drag()). Must be followed up by #WM_event_start_prepared_drag(), otherwise the
 * returned pointer will leak memory.
 *
 * Note that \a poin should be valid allocated and not on stack.
 */
wmDrag *WM_drag_data_create(
    bContext *C, int icon, eWM_DragDataType type, void *poin, double value, unsigned int flags);
/**
 * Invoke dragging using the given \a drag data.
 */
void WM_event_start_prepared_drag(bContext *C, wmDrag *drag);
void WM_event_drag_image(wmDrag *, const ImBuf *, float scale);
void WM_drag_free(wmDrag *drag);
void WM_drag_data_free(eWM_DragDataType dragtype, void *poin);
void WM_drag_free_list(ListBase *lb);
wmDropBox *WM_dropbox_add(ListBase *lb,
                          const char *idname,
                          bool (*poll)(bContext *, wmDrag *, const wmEvent *event),
                          void (*copy)(bContext *, wmDrag *, wmDropBox *),
                          void (*cancel)(Main *, wmDrag *, wmDropBox *),
                          WMDropboxTooltipFunc tooltip);
void WM_drag_draw_item_name_fn(bContext *C, wmWindow *win, wmDrag *drag, const int xy[2]);
void WM_drag_draw_default_fn(bContext *C, wmWindow *win, wmDrag *drag, const int xy[2]);
/**
 * `spaceid` / `regionid` are zero for window drop maps.
 */
ListBase *WM_dropboxmap_find(const char *idname, int spaceid, int regionid);

/* ID drag and drop */

/**
 * \param flag_extra: Additional linking flags (from #eFileSel_Params_Flag).
 */
ID *WM_drag_asset_id_import(const bContext *C, wmDragAsset *asset_drag, int flag_extra);
bool WM_drag_asset_will_import_linked(const wmDrag *drag);
void WM_drag_add_local_ID(wmDrag *drag, ID *id, ID *from_parent);
ID *WM_drag_get_local_ID(const wmDrag *drag, short idcode);
ID *WM_drag_get_local_ID_from_event(const wmEvent *event, short idcode);
/**
 * Check if the drag data is either a local ID or an external ID asset of type \a idcode.
 */
bool WM_drag_is_ID_type(const wmDrag *drag, int idcode);

/**
 * \note Does not store \a asset in any way, so it's fine to pass a temporary.
 */
wmDragAsset *WM_drag_create_asset_data(const blender::asset_system::AssetRepresentation *asset,
                                       int /* #eAssetImportMethod */ import_type);

wmDragAsset *WM_drag_get_asset_data(const wmDrag *drag, int idcode);
AssetMetaData *WM_drag_get_asset_meta_data(const wmDrag *drag, int idcode);
/**
 * When dragging a local ID, return that. Otherwise, if dragging an asset-handle, link or append
 * that depending on what was chosen by the drag-box (currently append only in fact).
 *
 * Use #WM_drag_free_imported_drag_ID() as cancel callback of the drop-box, so that the asset
 * import is rolled back if the drop operator fails.
 */
ID *WM_drag_get_local_ID_or_import_from_asset(const bContext *C, const wmDrag *drag, int idcode);

/**
 * \brief Free asset ID imported for canceled drop.
 *
 * If the asset was imported (linked/appended) using #WM_drag_get_local_ID_or_import_from_asset()`
 * (typically via a #wmDropBox.copy() callback), we want the ID to be removed again if the drop
 * operator cancels.
 * This is for use as #wmDropBox.cancel() callback.
 */
void WM_drag_free_imported_drag_ID(Main *bmain, wmDrag *drag, wmDropBox *drop);

wmDragAssetCatalog *WM_drag_get_asset_catalog_data(const wmDrag *drag);

/**
 * \note Does not store \a asset in any way, so it's fine to pass a temporary.
 */
void WM_drag_add_asset_list_item(wmDrag *drag,
                                 const blender::asset_system::AssetRepresentation *asset);

const ListBase *WM_drag_asset_list_get(const wmDrag *drag);

const char *WM_drag_get_item_name(wmDrag *drag);

/* Path drag and drop. */
/**
 * \param path: The path to drag. Value will be copied into the drag data so the passed string may
 *              be destructed.
 */
wmDragPath *WM_drag_create_path_data(const char *path);
const char *WM_drag_get_path(const wmDrag *drag);
/**
 * Note that even though the enum return type uses bit-flags, this should never have multiple
 * type-bits set, so `ELEM()` like comparison is possible.
 */
int /* eFileSel_File_Types */ WM_drag_get_path_file_type(const wmDrag *drag);

/* Set OpenGL viewport and scissor */
void wmViewport(const rcti *winrct);
void wmPartialViewport(rcti *drawrct, const rcti *winrct, const rcti *partialrct);
void wmWindowViewport(wmWindow *win);

/* OpenGL utilities with safety check */
void wmOrtho2(float x1, float x2, float y1, float y2);
/* use for conventions (avoid hard-coded offsets all over) */

/**
 * Default pixel alignment for regions.
 */
void wmOrtho2_region_pixelspace(const ARegion *region);
void wmOrtho2_pixelspace(float x, float y);
void wmGetProjectionMatrix(float mat[4][4], const rcti *winrct);

/* threaded Jobs Manager */
enum eWM_JobFlag {
  WM_JOB_PRIORITY = (1 << 0),
  WM_JOB_EXCL_RENDER = (1 << 1),
  WM_JOB_PROGRESS = (1 << 2),
};
ENUM_OPERATORS(eWM_JobFlag, WM_JOB_PROGRESS);

/**
 * Identifying jobs by owner alone is unreliable, this isn't saved,
 * order can change (keep 0 for 'any').
 */
enum eWM_JobType {
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
  WM_JOB_TYPE_CALCULATE_SIMULATION_NODES,
  WM_JOB_TYPE_BAKE_SIMULATION_NODES,
  WM_JOB_TYPE_UV_PACK,
  /* add as needed, bake, seq proxy build
   * if having hard coded values is a problem */
};

/**
 * \return current job or adds new job, but doesn't run it.
 *
 * \note every owner only gets a single job,
 * adding a new one will stop running job and when stopped it starts the new one.
 */
wmJob *WM_jobs_get(wmWindowManager *wm,
                   wmWindow *win,
                   const void *owner,
                   const char *name,
                   eWM_JobFlag flag,
                   eWM_JobType job_type);

/**
 * Returns true if job runs, for UI (progress) indicators.
 */
bool WM_jobs_test(const wmWindowManager *wm, const void *owner, int job_type);
float WM_jobs_progress(const wmWindowManager *wm, const void *owner);
const char *WM_jobs_name(const wmWindowManager *wm, const void *owner);
/**
 * Time that job started.
 */
double WM_jobs_starttime(const wmWindowManager *wm, const void *owner);
void *WM_jobs_customdata_from_type(wmWindowManager *wm, const void *owner, int job_type);

bool WM_jobs_is_running(const wmJob *wm_job);
bool WM_jobs_is_stopped(const wmWindowManager *wm, const void *owner);
void *WM_jobs_customdata_get(wmJob *);
void WM_jobs_customdata_set(wmJob *, void *customdata, void (*free)(void *));
void WM_jobs_timer(wmJob *, double timestep, unsigned int note, unsigned int endnote);
void WM_jobs_delay_start(wmJob *, double delay_time);

using wm_jobs_start_callback = void (*)(void *custom_data,
                                        bool *stop,
                                        bool *do_update,
                                        float *progress);
void WM_jobs_callbacks(wmJob *,
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
void WM_jobs_start(wmWindowManager *wm, wmJob *);
/**
 * Signal job(s) from this owner or callback to stop, timer is required to get handled.
 */
void WM_jobs_stop(wmWindowManager *wm, const void *owner, void *startjob);
/**
 * Actually terminate thread and job timer.
 */
void WM_jobs_kill(wmWindowManager *wm, void *owner, void (*)(void *, bool *, bool *, float *));
/**
 * Wait until every job ended.
 */
void WM_jobs_kill_all(wmWindowManager *wm);
/**
 * Wait until every job ended, except for one owner (used in undo to keep screen job alive).
 */
void WM_jobs_kill_all_except(wmWindowManager *wm, const void *owner);
void WM_jobs_kill_type(wmWindowManager *wm, const void *owner, int job_type);

bool WM_jobs_has_running(const wmWindowManager *wm);
bool WM_jobs_has_running_type(const wmWindowManager *wm, int job_type);

void WM_job_main_thread_lock_acquire(wmJob *job);
void WM_job_main_thread_lock_release(wmJob *job);

/* Clipboard. */

/**
 * Return text from the clipboard.
 * \param selection: Use the "primary" clipboard, see: #WM_CAPABILITY_PRIMARY_CLIPBOARD.
 * \param ensure_utf8: Ensure the resulting string does not contain invalid UTF8 encoding.
 */
char *WM_clipboard_text_get(bool selection, bool ensure_utf8, int *r_len);
/**
 * Convenience function for pasting to areas of Blender which don't support newlines.
 */
char *WM_clipboard_text_get_firstline(bool selection, bool ensure_utf8, int *r_len);

void WM_clipboard_text_set(const char *buf, bool selection);

/**
 * Returns true if the clipboard contains an image.
 */
bool WM_clipboard_image_available();

/**
 * Get image data from the Clipboard
 * \param r_width: the returned image width in pixels.
 * \param r_height: the returned image height in pixels.
 * \return pointer uint array in RGBA byte order. Caller must free.
 */
ImBuf *WM_clipboard_image_get();

/**
 * Put image data to the Clipboard
 * \param rgba: uint array in RGBA byte order.
 * \param width: the image width in pixels.
 * \param height: the image height in pixels.
 */
bool WM_clipboard_image_set(ImBuf *ibuf);

/* progress */

void WM_progress_set(wmWindow *win, float progress);
void WM_progress_clear(wmWindow *win);

/* Draw (for screenshot) */

void *WM_draw_cb_activate(wmWindow *win, void (*draw)(const wmWindow *, void *), void *customdata);
void WM_draw_cb_exit(wmWindow *win, void *handle);
void WM_redraw_windows(bContext *C);

void WM_draw_region_viewport_ensure(Scene *scene, ARegion *region, short space_type);
void WM_draw_region_viewport_bind(ARegion *region);
void WM_draw_region_viewport_unbind(ARegion *region);

/* Region drawing */

void WM_draw_region_free(ARegion *region, bool hide);
GPUViewport *WM_draw_region_get_viewport(ARegion *region);
GPUViewport *WM_draw_region_get_bound_viewport(ARegion *region);

void WM_main_playanim(int argc, const char **argv);

/**
 * Debugging only, convenience function to write on crash.
 * Convenient to save a blend file from a debugger.
 */
bool write_crash_blend();

/**
 * Lock the interface for any communication.
 */
void WM_set_locked_interface(wmWindowManager *wm, bool lock);

void WM_event_tablet_data_default_set(wmTabletData *tablet_data);

/**
 * For testing only, see #G_FLAG_EVENT_SIMULATE.
 */
wmEvent *WM_event_add_simulate(wmWindow *win, const wmEvent *event_to_add);

const char *WM_window_cursor_keymap_status_get(const wmWindow *win,
                                               int button_index,
                                               int type_index);
void WM_window_cursor_keymap_status_refresh(bContext *C, wmWindow *win);

void WM_window_status_area_tag_redraw(wmWindow *win);
/**
 * Similar to #BKE_screen_area_map_find_area_xy and related functions,
 * use here since the area is stored in the window manager.
 */
ScrArea *WM_window_status_area_find(wmWindow *win, bScreen *screen);
bool WM_window_modal_keymap_status_draw(bContext *C, wmWindow *win, uiLayout *layout);

/* `wm_event_query.cc` */

/**
 * For debugging only, getting inspecting events manually is tedious.
 */
void WM_event_print(const wmEvent *event);

/**
 * For modal callbacks, check configuration for how to interpret exit when dragging.
 */
bool WM_event_is_modal_drag_exit(const wmEvent *event,
                                 short init_event_type,
                                 short init_event_val);
bool WM_event_is_mouse_drag(const wmEvent *event);
bool WM_event_is_mouse_drag_or_press(const wmEvent *event);
int WM_event_drag_direction(const wmEvent *event);
char WM_event_utf8_to_ascii(const wmEvent *event) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;

/**
 * Detect motion between selection (callers should only use this for selection picking),
 * typically mouse press/click events.
 *
 * \param mval: Region relative coordinates, call with (-1, -1) resets the last cursor location.
 * \returns True when there was motion since last called.
 *
 * NOTE(@ideasman42): The logic used here isn't foolproof.
 * It's possible that users move the cursor past #WM_EVENT_CURSOR_MOTION_THRESHOLD then back to
 * a position within the threshold (between mouse clicks).
 * In practice users never reported this since the threshold is very small (a few pixels).
 * To prevent the unlikely case of values matching from another region,
 * changing regions resets this value to (-1, -1).
 */
bool WM_cursor_test_motion_and_update(const int mval[2]) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;

/**
 * Return true if this event type is a candidate for being flagged as consecutive.
 *
 * See: #WM_EVENT_IS_CONSECUTIVE doc-string.
 */
bool WM_event_consecutive_gesture_test(const wmEvent *event);
/**
 * Return true if this event should break the chain of consecutive gestures.
 * Practically all intentional user input should, key presses or button clicks.
 */
bool WM_event_consecutive_gesture_test_break(const wmWindow *win, const wmEvent *event);

int WM_event_drag_threshold(const wmEvent *event);
bool WM_event_drag_test(const wmEvent *event, const int prev_xy[2]);
bool WM_event_drag_test_with_delta(const wmEvent *event, const int delta[2]);
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
void WM_event_ndof_pan_get(const wmNDOFMotionData *ndof, float r_pan[3], bool use_zoom);
void WM_event_ndof_rotate_get(const wmNDOFMotionData *ndof, float r_rot[3]);

float WM_event_ndof_to_axis_angle(const wmNDOFMotionData *ndof, float axis[3]);
void WM_event_ndof_to_quat(const wmNDOFMotionData *ndof, float q[4]);
#endif /* WITH_INPUT_NDOF */

#ifdef WITH_XR_OPENXR
bool WM_event_is_xr(const wmEvent *event);
#endif

/**
 * If this is a tablet event, return tablet pressure and set `*pen_flip`
 * to 1 if the eraser tool is being used, 0 otherwise.
 */
float WM_event_tablet_data(const wmEvent *event, int *pen_flip, float tilt[2]);
bool WM_event_is_tablet(const wmEvent *event);

int WM_event_absolute_delta_x(const wmEvent *event);
int WM_event_absolute_delta_y(const wmEvent *event);

#ifdef WITH_INPUT_IME
bool WM_event_is_ime_switch(const wmEvent *event);
#endif

/* `wm_tooltip.cc` */

using wmTooltipInitFn = ARegion *(*)(bContext *C,
                                     ARegion *region,
                                     int *pass,
                                     double *r_pass_delay,
                                     bool *r_exit_on_event);

void WM_tooltip_immediate_init(
    bContext *C, wmWindow *win, ScrArea *area, ARegion *region, wmTooltipInitFn init);
void WM_tooltip_timer_init_ex(bContext *C,
                              wmWindow *win,
                              ScrArea *area,
                              ARegion *region,
                              wmTooltipInitFn init,
                              double delay);
void WM_tooltip_timer_init(
    bContext *C, wmWindow *win, ScrArea *area, ARegion *region, wmTooltipInitFn init);
void WM_tooltip_timer_clear(bContext *C, wmWindow *win);
void WM_tooltip_clear(bContext *C, wmWindow *win);
void WM_tooltip_init(bContext *C, wmWindow *win);
void WM_tooltip_refresh(bContext *C, wmWindow *win);
double WM_tooltip_time_closed();

/* `wm_utils.cc` */

wmGenericCallback *WM_generic_callback_steal(wmGenericCallback *callback);
void WM_generic_callback_free(wmGenericCallback *callback);

void WM_generic_user_data_free(wmGenericUserData *wm_userdata);

bool WM_region_use_viewport(ScrArea *area, ARegion *region);

#ifdef WITH_XR_OPENXR
/* `wm_xr_session.cc` */

/**
 * Check if the XR-Session was triggered.
 * If an error happened while trying to start a session, this returns false too.
 */
bool WM_xr_session_exists(const wmXrData *xr);
/**
 * Check if the session is running, according to the OpenXR definition.
 */
bool WM_xr_session_is_ready(const wmXrData *xr);
wmXrSessionState *WM_xr_session_state_handle_get(const wmXrData *xr);
ScrArea *WM_xr_session_area_get(const wmXrData *xr);
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
void WM_xr_session_state_navigation_reset(wmXrSessionState *state);

ARegionType *WM_xr_surface_controller_region_type_get();

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
                         wmOperatorType *ot,
                         IDProperty *op_properties,
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
                                 const wmXrPose *poses);
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
                            wmXrActionState *r_state);
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

/* `wm_xr_actionmap.cc` */

XrActionMap *WM_xr_actionmap_new(wmXrRuntimeData *runtime,
                                 const char *name,
                                 bool replace_existing);
/**
 * Ensure unique name among all action maps.
 */
void WM_xr_actionmap_ensure_unique(wmXrRuntimeData *runtime, XrActionMap *actionmap);
XrActionMap *WM_xr_actionmap_add_copy(wmXrRuntimeData *runtime, XrActionMap *am_src);
bool WM_xr_actionmap_remove(wmXrRuntimeData *runtime, XrActionMap *actionmap);
XrActionMap *WM_xr_actionmap_find(wmXrRuntimeData *runtime, const char *name);
void WM_xr_actionmap_clear(XrActionMap *actionmap);
void WM_xr_actionmaps_clear(wmXrRuntimeData *runtime);
ListBase *WM_xr_actionmaps_get(wmXrRuntimeData *runtime);
short WM_xr_actionmap_active_index_get(const wmXrRuntimeData *runtime);
void WM_xr_actionmap_active_index_set(wmXrRuntimeData *runtime, short idx);
short WM_xr_actionmap_selected_index_get(const wmXrRuntimeData *runtime);
void WM_xr_actionmap_selected_index_set(wmXrRuntimeData *runtime, short idx);

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
