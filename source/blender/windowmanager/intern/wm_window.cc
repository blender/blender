/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved. 2007 Blender Authors.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Window management, wrap GHOST.
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include <fmt/format.h>

#include "CLG_log.h"

#include "DNA_listBase.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "MEM_guardedalloc.h"

#include "GHOST_C-api.h"

#include "BLI_enum_flags.hh"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_system.h"
#include "BLI_time.h"

#include "BLT_translation.hh"

#include "BKE_blender_version.h"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_icons.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"
#include "BKE_wm_runtime.hh"
#include "BKE_workspace.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_types.hh"
#include "wm.hh"
#include "wm_draw.hh"
#include "wm_event_system.hh"
#include "wm_files.hh"
#include "wm_window.hh"
#include "wm_window_private.hh"
#ifdef WITH_XR_OPENXR
#  include "wm_xr.hh"
#endif

#include "ED_anim_api.hh"
#include "ED_fileselect.hh"
#include "ED_render.hh"
#include "ED_scene.hh"
#include "ED_screen.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"

#include "BLF_api.hh"
#include "GPU_capabilities.hh"
#include "GPU_context.hh"
#include "GPU_framebuffer.hh"
#include "GPU_init_exit.hh"

#include "UI_resources.hh"

/* For assert. */
#ifndef NDEBUG
#  include "BLI_threads.h"
#endif

/* The global to talk to GHOST. */
static GHOST_SystemHandle g_system = nullptr;
#if !(defined(WIN32) || defined(__APPLE__))
static const char *g_system_backend_id = nullptr;
#endif

static CLG_LogRef LOG_GHOST_SYSTEM = {"ghost.system"};

enum eWinOverrideFlag {
  WIN_OVERRIDE_GEOM = (1 << 0),
  WIN_OVERRIDE_WINSTATE = (1 << 1),
};
ENUM_OPERATORS(eWinOverrideFlag)

#define GHOST_WINDOW_STATE_DEFAULT GHOST_kWindowStateMaximized

/**
 * Override defaults or startup file when #eWinOverrideFlag is set.
 * These values are typically set by command line arguments.
 */
static struct WMInitStruct {
  /**
   * Window geometry:
   * - Defaults to the main screen-size.
   * - May be set by the `--window-geometry` argument,
   *   which also forces these values to be used by setting #WIN_OVERRIDE_GEOM.
   * - When #wmWindow::size_x is zero, these values are used as a fallback,
   *   needed so the #BLENDER_STARTUP_FILE loads at the size of the users main-screen
   *   instead of the size stored in the factory startup.
   *   Otherwise the window geometry saved in the blend-file is used and these values are ignored.
   */
  blender::int2 size;
  blender::int2 start;

  GHOST_TWindowState windowstate = GHOST_WINDOW_STATE_DEFAULT;
  eWinOverrideFlag override_flag;

  bool window_frame = true;
  bool window_focus = true;
  bool native_pixels = true;
} wm_init_state;

/* -------------------------------------------------------------------- */
/** \name Modifier Constants
 * \{ */

static const struct {
  uint8_t flag;
  GHOST_TKey ghost_key_pair[2];
  GHOST_TModifierKey ghost_mask_pair[2];
} g_modifier_table[] = {
    {KM_SHIFT,
     {GHOST_kKeyLeftShift, GHOST_kKeyRightShift},
     {GHOST_kModifierKeyLeftShift, GHOST_kModifierKeyRightShift}},
    {KM_CTRL,
     {GHOST_kKeyLeftControl, GHOST_kKeyRightControl},
     {GHOST_kModifierKeyLeftControl, GHOST_kModifierKeyRightControl}},
    {KM_ALT,
     {GHOST_kKeyLeftAlt, GHOST_kKeyRightAlt},
     {GHOST_kModifierKeyLeftAlt, GHOST_kModifierKeyRightAlt}},
    {KM_OSKEY,
     {GHOST_kKeyLeftOS, GHOST_kKeyRightOS},
     {GHOST_kModifierKeyLeftOS, GHOST_kModifierKeyRightOS}},
    {KM_HYPER,
     {GHOST_kKeyLeftHyper, GHOST_kKeyRightHyper},
     {GHOST_kModifierKeyLeftHyper, GHOST_kModifierKeyRightHyper}},
};

enum ModSide {
  MOD_SIDE_LEFT = 0,
  MOD_SIDE_RIGHT = 1,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Open
 * \{ */

static void wm_window_set_drawable(wmWindowManager *wm, wmWindow *win, bool activate);
static bool wm_window_timers_process(const bContext *C, int *sleep_us_p);
static uint8_t wm_ghost_modifier_query(const enum ModSide side);

bool wm_get_screensize(int r_size[2])
{
  uint32_t uiwidth, uiheight;
  if (GHOST_GetMainDisplayDimensions(g_system, &uiwidth, &uiheight) == GHOST_kFailure) {
    return false;
  }
  r_size[0] = uiwidth;
  r_size[1] = uiheight;
  return true;
}

bool wm_get_desktopsize(int r_size[2])
{
  uint32_t uiwidth, uiheight;
  if (GHOST_GetAllDisplayDimensions(g_system, &uiwidth, &uiheight) == GHOST_kFailure) {
    return false;
  }
  r_size[0] = uiwidth;
  r_size[1] = uiheight;
  return true;
}

/** Keeps size within monitor bounds. */
static void wm_window_check_size(rcti *rect)
{
  blender::int2 scr_size;
  if (wm_get_screensize(scr_size)) {
    if (BLI_rcti_size_x(rect) > scr_size[0]) {
      BLI_rcti_resize_x(rect, scr_size[0]);
    }
    if (BLI_rcti_size_y(rect) > scr_size[1]) {
      BLI_rcti_resize_y(rect, scr_size[1]);
    }
  }
}

static void wm_ghostwindow_destroy(wmWindowManager *wm, wmWindow *win)
{
  if (UNLIKELY(!win->ghostwin)) {
    return;
  }

  /* Prevents non-drawable state of main windows (bugs #22967,
   * #25071 and possibly #22477 too). Always clear it even if
   * this window was not the drawable one, because we mess with
   * drawing context to discard the GW context. */
  wm_window_clear_drawable(wm);

  if (win == wm->runtime->winactive) {
    wm->runtime->winactive = nullptr;
  }

  /* We need this window's GPU context active to discard it. */
  GHOST_ActivateWindowDrawingContext(static_cast<GHOST_WindowHandle>(win->ghostwin));
  GPU_context_active_set(static_cast<GPUContext *>(win->gpuctx));

  /* Delete local GPU context. */
  GPU_context_discard(static_cast<GPUContext *>(win->gpuctx));

  GHOST_DisposeWindow(g_system, static_cast<GHOST_WindowHandle>(win->ghostwin));
  win->ghostwin = nullptr;
  win->gpuctx = nullptr;
}

void wm_window_free(bContext *C, wmWindowManager *wm, wmWindow *win)
{
  /* Update context. */
  if (C) {
    WM_event_remove_handlers(C, &win->handlers);
    WM_event_remove_handlers(C, &win->modalhandlers);

    if (CTX_wm_window(C) == win) {
      CTX_wm_window_set(C, nullptr);
    }
  }

  BKE_screen_area_map_free(&win->global_areas);

  /* End running jobs, a job end also removes its timer. */
  LISTBASE_FOREACH_MUTABLE (wmTimer *, wt, &wm->runtime->timers) {
    if (wt->flags & WM_TIMER_TAGGED_FOR_REMOVAL) {
      continue;
    }
    if (wt->win == win && wt->event_type == TIMERJOBS) {
      wm_jobs_timer_end(wm, wt);
    }
  }

  /* Timer removing, need to call this API function. */
  LISTBASE_FOREACH_MUTABLE (wmTimer *, wt, &wm->runtime->timers) {
    if (wt->flags & WM_TIMER_TAGGED_FOR_REMOVAL) {
      continue;
    }
    if (wt->win == win) {
      WM_event_timer_remove(wm, win, wt);
    }
  }
  wm_window_timers_delete_removed(wm);

  if (win->eventstate) {
    MEM_freeN(win->eventstate);
  }
  if (win->event_last_handled) {
    MEM_freeN(win->event_last_handled);
  }
  if (win->event_queue_consecutive_gesture_data) {
    WM_event_consecutive_data_free(win);
  }

  if (win->cursor_keymap_status) {
    MEM_freeN(win->cursor_keymap_status);
  }

  WM_gestures_free_all(win);

  wm_event_free_all(win);

  wm_ghostwindow_destroy(wm, win);

  BKE_workspace_instance_hook_free(G_MAIN, win->workspace_hook);
  MEM_freeN(win->stereo3d_format);

  MEM_delete(win->runtime);
  MEM_freeN(win);
}

static int find_free_winid(wmWindowManager *wm)
{
  int id = 1;

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (id <= win->winid) {
      id = win->winid + 1;
    }
  }
  return id;
}

wmWindow *wm_window_new(const Main *bmain, wmWindowManager *wm, wmWindow *parent, bool dialog)
{
  wmWindow *win = MEM_callocN<wmWindow>("window");

  BLI_addtail(&wm->windows, win);
  win->winid = find_free_winid(wm);

  /* Dialogs may have a child window as parent. Otherwise, a child must not be a parent too. */
  win->parent = (!dialog && parent && parent->parent) ? parent->parent : parent;
  win->stereo3d_format = MEM_callocN<Stereo3dFormat>("Stereo 3D Format (window)");
  win->workspace_hook = BKE_workspace_instance_hook_create(bmain, win->winid);
  win->runtime = MEM_new<blender::bke::WindowRuntime>(__func__);

  return win;
}

wmWindow *wm_window_copy(Main *bmain,
                         wmWindowManager *wm,
                         wmWindow *win_src,
                         const bool duplicate_layout,
                         const bool child)
{
  const bool is_dialog = GHOST_IsDialogWindow(static_cast<GHOST_WindowHandle>(win_src->ghostwin));
  wmWindow *win_parent = (child) ? win_src : win_src->parent;
  wmWindow *win_dst = wm_window_new(bmain, wm, win_parent, is_dialog);
  WorkSpace *workspace = WM_window_get_active_workspace(win_src);
  WorkSpaceLayout *layout_old = WM_window_get_active_layout(win_src);

  win_dst->posx = win_src->posx + 10;
  win_dst->posy = win_src->posy;
  win_dst->sizex = win_src->sizex;
  win_dst->sizey = win_src->sizey;

  win_dst->scene = win_src->scene;
  STRNCPY_UTF8(win_dst->view_layer_name, win_src->view_layer_name);
  BKE_workspace_active_set(win_dst->workspace_hook, workspace);
  WorkSpaceLayout *layout_new = duplicate_layout ? ED_workspace_layout_duplicate(
                                                       bmain, workspace, layout_old, win_dst) :
                                                   layout_old;
  BKE_workspace_active_layout_set(win_dst->workspace_hook, win_dst->winid, workspace, layout_new);

  *win_dst->stereo3d_format = *win_src->stereo3d_format;

  return win_dst;
}

wmWindow *wm_window_copy_test(bContext *C,
                              wmWindow *win_src,
                              const bool duplicate_layout,
                              const bool child)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);

  wmWindow *win_dst = wm_window_copy(bmain, wm, win_src, duplicate_layout, child);

  WM_check(C);

  if (win_dst->ghostwin) {
    WM_event_add_notifier_ex(wm, CTX_wm_window(C), NC_WINDOW | NA_ADDED, nullptr);
    return win_dst;
  }
  wm_window_close(C, wm, win_dst);
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Quit Confirmation Dialog
 * \{ */

static void wm_save_file_on_quit_dialog_callback(bContext *C, void * /*user_data*/)
{
  wm_exit_schedule_delayed(C);
}

/**
 * Call the confirm dialog on quitting. It's displayed in the context window so
 * caller should set it as desired.
 */
static void wm_confirm_quit(bContext *C)
{
  wmGenericCallback *action = MEM_callocN<wmGenericCallback>(__func__);
  action->exec = wm_save_file_on_quit_dialog_callback;
  wm_close_file_dialog(C, action);
}

void wm_quit_with_optional_confirmation_prompt(bContext *C, wmWindow *win)
{
  wmWindow *win_ctx = CTX_wm_window(C);

  /* The popup will be displayed in the context window which may not be set
   * here (this function gets called outside of normal event handling loop). */
  CTX_wm_window_set(C, win);

  if (U.uiflag & USER_SAVE_PROMPT) {
    if (wm_file_or_session_data_has_unsaved_changes(CTX_data_main(C), CTX_wm_manager(C)) &&
        !G.background)
    {
      wm_window_raise(win);
      wm_confirm_quit(C);
    }
    else {
      wm_exit_schedule_delayed(C);
    }
  }
  else {
    wm_exit_schedule_delayed(C);
  }

  CTX_wm_window_set(C, win_ctx);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Close
 * \{ */

static rctf *stored_window_bounds(eSpace_Type space_type)
{
  if (space_type == SPACE_IMAGE) {
    return &U.stored_bounds.image;
  }
  if (space_type == SPACE_USERPREF) {
    return &U.stored_bounds.userpref;
  }
  if (space_type == SPACE_GRAPH) {
    return &U.stored_bounds.graph;
  }
  if (space_type == SPACE_INFO) {
    return &U.stored_bounds.info;
  }
  if (space_type == SPACE_OUTLINER) {
    return &U.stored_bounds.outliner;
  }
  if (space_type == SPACE_FILE) {
    return &U.stored_bounds.file;
  }

  return nullptr;
}

void wm_window_close(bContext *C, wmWindowManager *wm, wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);

  if (screen->temp && BLI_listbase_is_single(&screen->areabase) && !WM_window_is_maximized(win)) {
    ScrArea *area = static_cast<ScrArea *>(screen->areabase.first);
    rctf *stored_bounds = stored_window_bounds(eSpace_Type(area->spacetype));

    if (stored_bounds) {
      /* Get DPI and scale from parent window, if there is one. */
      WM_window_dpi_set_userdef(win->parent ? win->parent : win);
      const float f = GHOST_GetNativePixelSize(static_cast<GHOST_WindowHandle>(win->ghostwin));
      stored_bounds->xmin = float(win->posx) * f / UI_SCALE_FAC;
      stored_bounds->xmax = stored_bounds->xmin + float(win->sizex) * f / UI_SCALE_FAC;
      stored_bounds->ymin = float(win->posy) * f / UI_SCALE_FAC;
      stored_bounds->ymax = stored_bounds->ymin + float(win->sizey) * f / UI_SCALE_FAC;
      /* Tag user preferences as dirty. */
      U.runtime.is_dirty = true;
    }
  }

  wmWindow *win_other;

  /* First check if there is another main window remaining. */
  for (win_other = static_cast<wmWindow *>(wm->windows.first); win_other;
       win_other = win_other->next)
  {
    if (win_other != win && win_other->parent == nullptr && !WM_window_is_temp_screen(win_other)) {
      break;
    }
  }

  if (win->parent == nullptr && win_other == nullptr) {
    wm_quit_with_optional_confirmation_prompt(C, win);
    return;
  }

  /* Close child windows. */
  LISTBASE_FOREACH_MUTABLE (wmWindow *, iter_win, &wm->windows) {
    if (iter_win->parent == win) {
      wm_window_close(C, wm, iter_win);
    }
  }

  WorkSpace *workspace = WM_window_get_active_workspace(win);
  WorkSpaceLayout *layout = BKE_workspace_active_layout_get(win->workspace_hook);

  BLI_remlink(&wm->windows, win);

  CTX_wm_window_set(C, win); /* Needed by handlers. */
  WM_event_remove_handlers(C, &win->handlers);

  WM_event_remove_handlers(C, &win->modalhandlers);

  /* For regular use this will _never_ be nullptr,
   * however we may be freeing an improperly initialized window. */
  if (screen) {
    ED_screen_exit(C, win, screen);
  }
  const bool is_single_editor = !WM_window_is_main_top_level(win) &&
                                (screen && BLI_listbase_is_single(&screen->areabase));

  wm_window_free(C, wm, win);

  /* If temp screen, delete it after window free (it stops jobs that can access it).
   * Also delete windows with single editor. If required, they are easy to restore, see: !132978.
   */
  if ((screen && screen->temp) || is_single_editor) {
    Main *bmain = CTX_data_main(C);

    BLI_assert(BKE_workspace_layout_screen_get(layout) == screen);
    BKE_workspace_layout_remove(bmain, workspace, layout);
    WM_event_add_notifier(C, NC_SCREEN | ND_LAYOUTDELETE, nullptr);
  }

  WM_main_add_notifier(NC_WINDOW | NA_REMOVED, nullptr);
}

/**
 * Construct the title text for `win`.
 * The window may *not* have been created, any calls depending on `win->ghostwin` are forbidden.
 *
 * \param window_filepath_fn: When non `nullopt` the title text does not need to contain
 * the file-path (typically based on #WM_CAPABILITY_WINDOW_PATH).
 */
static std::string wm_window_title_text(
    wmWindowManager *wm,
    wmWindow *win,
    std::optional<blender::FunctionRef<void(const char *)>> window_filepath_fn)
{
  if (win->parent || WM_window_is_temp_screen(win)) {
    /* Not a main window. */
    bScreen *screen = WM_window_get_active_screen(win);
    const bool is_single = screen && BLI_listbase_is_single(&screen->areabase);
    ScrArea *area = (screen) ? static_cast<ScrArea *>(screen->areabase.first) : nullptr;
    if (is_single && area && area->spacetype != SPACE_EMPTY) {
      return IFACE_(ED_area_name(area).c_str());
    }
    return "Blender";
  }

  /* This path may contain invalid UTF8 byte sequences on UNIX systems,
   * use `filepath` for display which is sanitized as needed. */
  const char *filepath_as_bytes = BKE_main_blendfile_path_from_global();

  char _filepath_utf8_buf[FILE_MAX];
  /* Allow non-UTF8 characters on systems that support it.
   *
   * On Wayland, invalid UTF8 characters will disconnect
   * from the server - exiting immediately. */
  const char *filepath = (OS_MAC || OS_WINDOWS) ?
                             filepath_as_bytes :
                             BLI_str_utf8_invalid_substitute_if_needed(filepath_as_bytes,
                                                                       strlen(filepath_as_bytes),
                                                                       '?',
                                                                       _filepath_utf8_buf,
                                                                       sizeof(_filepath_utf8_buf));

  const char *filename = BLI_path_basename(filepath);
  const bool has_filepath = filepath[0] != '\0';
  const bool native_filepath_display = (window_filepath_fn != std::nullopt);
  if (native_filepath_display) {
    (*window_filepath_fn)(filepath_as_bytes);
  }
  const bool include_filepath = has_filepath && (filepath != filename) && !native_filepath_display;

  /* File saved state. */
  std::string win_title = wm->file_saved ? "" : "* ";

  /* File name. Show the file extension if the full file path is not included in the title. */
  if (include_filepath) {
    const size_t filename_no_ext_len = BLI_path_extension_or_end(filename) - filename;
    win_title.append(filename, filename_no_ext_len);
  }
  else if (has_filepath) {
    win_title.append(filename);
  }
  /* New / Unsaved file default title. Shows "Untitled" on macOS following the Apple HIGs. */
  else {
#ifdef __APPLE__
    win_title.append(IFACE_("Untitled"));
#else
    win_title.append(IFACE_("(Unsaved)"));
#endif
  }

  if (G_MAIN->recovered) {
    win_title.append(IFACE_(" (Recovered)"));
  }

  if (include_filepath) {
    bool add_filepath = true;
    if ((OS_MAC || OS_WINDOWS) == 0) {
      /* Notes:
       * - Relies on the `filepath_as_bytes` & `filepath` being aligned and the same length.
       *   If that changes (if we implement surrogate escape for example)
       *   then the substitution would need to be performed before validating UTF8.
       * - This file-path is already normalized
       *   so there is no need to use a comparison that normalizes both.
       *
       * See !141059 for more general support for "My Documents", "Downloads" etc,
       * this also caches the result, which doesn't seem necessary at the moment. */
      if (const char *home_dir = BLI_dir_home()) {
        size_t home_dir_len = strlen(home_dir);
        /* Strip trailing slash (if it exists). */
        while (home_dir_len && home_dir[home_dir_len - 1] == SEP) {
          home_dir_len--;
        }
        if ((home_dir_len > 0) && BLI_path_ncmp(home_dir, filepath_as_bytes, home_dir_len) == 0) {
          if (filepath_as_bytes[home_dir_len] == SEP) {
            win_title.append(fmt::format(" [~{}]", filepath + home_dir_len));
            add_filepath = false;
          }
        }
      }
    }
    if (add_filepath) {
      win_title.append(fmt::format(" [{}]", filepath));
    }
  }

  win_title.append(fmt::format(" - Blender {}", BKE_blender_version_string()));

  return win_title;
}

static void wm_window_title_state_refresh(wmWindowManager *wm, wmWindow *win)
{
  GHOST_WindowHandle handle = static_cast<GHOST_WindowHandle>(win->ghostwin);

  /* Informs GHOST of unsaved changes to set the window modified visual indicator (macOS)
   * and to give a hint of unsaved changes for a user warning mechanism in case of OS application
   * terminate request (e.g., OS Shortcut Alt+F4, Command+Q, (...) or session end). */
  GHOST_SetWindowModifiedState(handle, !wm->file_saved);
}

void WM_window_title_set(wmWindow *win, const char *title)
{
  if (win->ghostwin == nullptr) {
    return;
  }

  GHOST_WindowHandle handle = static_cast<GHOST_WindowHandle>(win->ghostwin);
  GHOST_SetTitle(handle, title);
}

void WM_window_title_refresh(wmWindowManager *wm, wmWindow *win)
{
  if (win->ghostwin == nullptr) {
    return;
  }

  GHOST_WindowHandle handle = static_cast<GHOST_WindowHandle>(win->ghostwin);
  auto window_filepath_fn = (WM_capabilities_flag() & WM_CAPABILITY_WINDOW_PATH) ?
                                std::optional([&handle](const char *filepath) {
                                  GHOST_SetPath(handle, filepath);
                                }) :
                                std::nullopt;
  std::string win_title = wm_window_title_text(wm, win, window_filepath_fn);
  GHOST_SetTitle(handle, win_title.c_str());
  wm_window_title_state_refresh(wm, win);
}

void WM_window_dpi_set_userdef(const wmWindow *win)
{
  float auto_dpi = GHOST_GetDPIHint(static_cast<GHOST_WindowHandle>(win->ghostwin));

  /* Clamp auto DPI to 96, since our font/interface drawing does not work well
   * with lower sizes. The main case we are interested in supporting is higher
   * DPI. If a smaller UI is desired it is still possible to adjust UI scale. */
  auto_dpi = max_ff(auto_dpi, 96.0f);

  /* Lazily init UI scale size, preserving backwards compatibility by
   * computing UI scale from ratio of previous DPI and auto DPI. */
  if (U.ui_scale == 0) {
    int virtual_pixel = (U.virtual_pixel == VIRTUAL_PIXEL_NATIVE) ? 1 : 2;

    if (U.dpi == 0) {
      U.ui_scale = virtual_pixel;
    }
    else {
      U.ui_scale = (virtual_pixel * U.dpi * 96.0f) / (auto_dpi * 72.0f);
    }

    CLAMP(U.ui_scale, 0.25f, 4.0f);
  }

  /* Blender's UI drawing assumes DPI 72 as a good default following macOS
   * while Windows and Linux use DPI 96. GHOST assumes a default 96 so we
   * remap the DPI to Blender's convention. */
  auto_dpi *= GHOST_GetNativePixelSize(static_cast<GHOST_WindowHandle>(win->ghostwin));
  U.dpi = auto_dpi * U.ui_scale * (72.0 / 96.0f);

  /* Automatically set larger pixel size for high DPI. */
  int pixelsize = max_ii(1, (U.dpi / 64));
  /* User adjustment for pixel size. */
  pixelsize = max_ii(1, pixelsize + U.ui_line_width);

  /* Set user preferences globals for drawing, and for forward compatibility. */
  U.pixelsize = pixelsize;
  U.virtual_pixel = (pixelsize == 1) ? VIRTUAL_PIXEL_NATIVE : VIRTUAL_PIXEL_DOUBLE;
  U.scale_factor = U.dpi / 72.0f;
  U.inv_scale_factor = 1.0f / U.scale_factor;

  /* Widget unit is 20 pixels at 1X scale. This consists of 18 user-scaled units plus
   * left and right borders of line-width (pixel-size). */
  U.widget_unit = int(roundf(18.0f * U.scale_factor)) + (2 * pixelsize);
}

float WM_window_dpi_get_scale(const wmWindow *win)
{
  GHOST_WindowHandle win_handle = static_cast<GHOST_WindowHandle>(win->ghostwin);
  const uint16_t dpi_base = 96;
  const uint16_t dpi_fixed = std::max<uint16_t>(dpi_base, GHOST_GetDPIHint(win_handle));
  float dpi = float(dpi_fixed);
  if (OS_MAC) {
    dpi *= GHOST_GetNativePixelSize(win_handle);
  }
  return dpi / float(dpi_base);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Decoration Style
 * \{ */

eWM_WindowDecorationStyleFlag WM_window_decoration_style_flags_get(const wmWindow *win)
{
  const GHOST_TWindowDecorationStyleFlags ghost_style_flags = GHOST_GetWindowDecorationStyleFlags(
      static_cast<GHOST_WindowHandle>(win->ghostwin));

  eWM_WindowDecorationStyleFlag wm_style_flags = WM_WINDOW_DECORATION_STYLE_NONE;

  if (ghost_style_flags & GHOST_kDecorationColoredTitleBar) {
    wm_style_flags |= WM_WINDOW_DECORATION_STYLE_COLORED_TITLEBAR;
  }

  return wm_style_flags;
}

void WM_window_decoration_style_flags_set(const wmWindow *win,
                                          eWM_WindowDecorationStyleFlag style_flags)
{
  BLI_assert(WM_capabilities_flag() & WM_CAPABILITY_WINDOW_DECORATION_STYLES);
  uint ghost_style_flags = GHOST_kDecorationNone;

  if (style_flags & WM_WINDOW_DECORATION_STYLE_COLORED_TITLEBAR) {
    ghost_style_flags |= GHOST_kDecorationColoredTitleBar;
  }

  GHOST_SetWindowDecorationStyleFlags(
      static_cast<GHOST_WindowHandle>(win->ghostwin),
      static_cast<GHOST_TWindowDecorationStyleFlags>(ghost_style_flags));
}

static void wm_window_decoration_style_set_from_theme(const wmWindow *win, const bScreen *screen)
{
  /* Set the decoration style settings from the current theme colors.
   * NOTE: screen may be null. In which case, only the window is used as a theme provider. */
  GHOST_WindowDecorationStyleSettings decoration_settings = {};

  /* Colored TitleBar Decoration. */
  /* For main windows, use the top-bar color. */
  if (WM_window_is_main_top_level(win)) {
    UI_SetTheme(SPACE_TOPBAR, RGN_TYPE_HEADER);
  }
  /* For single editor floating windows, use the editor header color. */
  else if (screen && BLI_listbase_is_single(&screen->areabase)) {
    const ScrArea *main_area = static_cast<ScrArea *>(screen->areabase.first);
    UI_SetTheme(main_area->spacetype, RGN_TYPE_HEADER);
  }
  /* For floating window with multiple editors/areas, use the default space color. */
  else {
    UI_SetTheme(0, RGN_TYPE_WINDOW);
  }

  float titlebar_bg_color[3];
  UI_GetThemeColor3fv(TH_BACK, titlebar_bg_color);
  copy_v3_v3(decoration_settings.colored_titlebar_bg_color, titlebar_bg_color);

  GHOST_SetWindowDecorationStyleSettings(static_cast<GHOST_WindowHandle>(win->ghostwin),
                                         decoration_settings);
}

void WM_window_decoration_style_apply(const wmWindow *win, const bScreen *screen)
{
  BLI_assert(WM_capabilities_flag() & WM_CAPABILITY_WINDOW_DECORATION_STYLES);
  wm_window_decoration_style_set_from_theme(win, screen);
  GHOST_ApplyWindowDecorationStyle(static_cast<GHOST_WindowHandle>(win->ghostwin));
}

/**
 * When windows are activated, simulate modifier press/release to match the current state of
 * held modifier keys, see #40317.
 *
 * NOTE(@ideasman42): There is a bug in Windows11 where Alt-Tab sends an Alt-press event
 * to the window after it's deactivated, this means window de-activation is not a fool-proof
 * way of ensuring modifier keys are cleared for inactive windows. So any event added to an
 * inactive window must run #wm_window_update_eventstate_modifiers first to ensure no modifier
 * keys are held. See: #105277.
 */
static void wm_window_update_eventstate_modifiers(wmWindowManager *wm,
                                                  wmWindow *win,
                                                  const uint64_t event_time_ms)
{
  const uint8_t keymodifier_sided[2] = {
      wm_ghost_modifier_query(MOD_SIDE_LEFT),
      wm_ghost_modifier_query(MOD_SIDE_RIGHT),
  };
  const uint8_t keymodifier = keymodifier_sided[0] | keymodifier_sided[1];
  const uint8_t keymodifier_eventstate = win->eventstate->modifier;
  if (keymodifier != keymodifier_eventstate) {
    GHOST_TEventKeyData kdata{};
    kdata.key = GHOST_kKeyUnknown;
    kdata.utf8_buf[0] = '\0';
    kdata.is_repeat = false;
    for (int i = 0; i < ARRAY_SIZE(g_modifier_table); i++) {
      if (keymodifier_eventstate & g_modifier_table[i].flag) {
        if ((keymodifier & g_modifier_table[i].flag) == 0) {
          for (int side = 0; side < 2; side++) {
            if ((keymodifier_sided[side] & g_modifier_table[i].flag) == 0) {
              kdata.key = g_modifier_table[i].ghost_key_pair[side];
              wm_event_add_ghostevent(wm, win, GHOST_kEventKeyUp, &kdata, event_time_ms);
              /* Only ever send one release event
               * (currently releasing multiple isn't needed and only confuses logic). */
              break;
            }
          }
        }
      }
      else {
        if (keymodifier & g_modifier_table[i].flag) {
          for (int side = 0; side < 2; side++) {
            if (keymodifier_sided[side] & g_modifier_table[i].flag) {
              kdata.key = g_modifier_table[i].ghost_key_pair[side];
              wm_event_add_ghostevent(wm, win, GHOST_kEventKeyDown, &kdata, event_time_ms);
            }
          }
        }
      }
    }
  }
}

/**
 * When the window is de-activated, release all held modifiers.
 *
 * Needed so events generated over unfocused (non-active) windows don't have modifiers held.
 * Since modifier press/release events aren't send to unfocused windows it's best to assume
 * modifiers are not pressed. This means when modifiers *are* held, events will incorrectly
 * reported as not being held. Since this is standard behavior for Linux/MS-Window,
 * opt to use this.
 *
 * NOTE(@ideasman42): Events generated for non-active windows are rare,
 * this happens when using the mouse-wheel over an unfocused window, see: #103722.
 */
static void wm_window_update_eventstate_modifiers_clear(wmWindowManager *wm,
                                                        wmWindow *win,
                                                        const uint64_t event_time_ms)
{
  /* Release all held modifiers before de-activating the window. */
  if (win->eventstate->modifier != 0) {
    const uint8_t keymodifier_eventstate = win->eventstate->modifier;
    const uint8_t keymodifier_l = wm_ghost_modifier_query(MOD_SIDE_LEFT);
    const uint8_t keymodifier_r = wm_ghost_modifier_query(MOD_SIDE_RIGHT);
    /* NOTE(@ideasman42): when non-zero, there are modifiers held in
     * `win->eventstate` which are not considered held by the GHOST internal state.
     * While this should not happen, it's important all modifier held in event-state
     * receive release events. Without this, so any events generated while the window
     * is *not* active will have modifiers held. */
    const uint8_t keymodifier_unhandled = keymodifier_eventstate &
                                          ~(keymodifier_l | keymodifier_r);
    const uint8_t keymodifier_sided[2] = {
        uint8_t(keymodifier_l | keymodifier_unhandled),
        keymodifier_r,
    };
    GHOST_TEventKeyData kdata{};
    kdata.key = GHOST_kKeyUnknown;
    kdata.utf8_buf[0] = '\0';
    kdata.is_repeat = false;
    for (int i = 0; i < ARRAY_SIZE(g_modifier_table); i++) {
      if (keymodifier_eventstate & g_modifier_table[i].flag) {
        for (int side = 0; side < 2; side++) {
          if ((keymodifier_sided[side] & g_modifier_table[i].flag) == 0) {
            kdata.key = g_modifier_table[i].ghost_key_pair[side];
            wm_event_add_ghostevent(wm, win, GHOST_kEventKeyUp, &kdata, event_time_ms);
          }
        }
      }
    }
  }
}

static void wm_window_update_eventstate(wmWindow *win)
{
  /* Update mouse position when a window is activated. */
  int xy[2];
  if (wm_cursor_position_get(win, &xy[0], &xy[1])) {
    copy_v2_v2_int(win->eventstate->xy, xy);
  }
}

static void wm_window_ensure_eventstate(wmWindow *win)
{
  if (win->eventstate) {
    return;
  }

  win->eventstate = MEM_callocN<wmEvent>("window event state");
  wm_window_update_eventstate(win);
}

static bool wm_window_update_size_position(wmWindow *win);

/* Belongs to below. */
static void wm_window_ghostwindow_add(wmWindowManager *wm,
                                      const char *title,
                                      wmWindow *win,
                                      bool is_dialog)
{
  /* A new window is created when page-flip mode is required for a window. */
  GHOST_GPUSettings gpu_settings = {0};
  if (win->stereo3d_format->display_mode == S3D_DISPLAY_PAGEFLIP) {
    gpu_settings.flags |= GHOST_gpuStereoVisual;
  }

  if (G.debug & G_DEBUG_GPU) {
    gpu_settings.flags |= GHOST_gpuDebugContext;
  }

  GPUBackendType gpu_backend = GPU_backend_type_selection_get();
  gpu_settings.context_type = wm_ghost_drawing_context_type(gpu_backend);
  gpu_settings.preferred_device.index = U.gpu_preferred_index;
  gpu_settings.preferred_device.vendor_id = U.gpu_preferred_vendor_id;
  gpu_settings.preferred_device.device_id = U.gpu_preferred_device_id;
  if (GPU_backend_vsync_is_overridden()) {
    gpu_settings.flags |= GHOST_gpuVSyncIsOverridden;
    gpu_settings.vsync = GHOST_TVSyncModes(GPU_backend_vsync_get());
  }

  int posx = 0;
  int posy = 0;

  if (WM_capabilities_flag() & WM_CAPABILITY_WINDOW_POSITION) {
    blender::int2 scr_size;
    if (wm_get_desktopsize(scr_size)) {
      posx = win->posx;
      posy = (scr_size[1] - win->posy - win->sizey);
    }
  }

  /* Clear drawable so we can set the new window. */
  wmWindow *prev_windrawable = wm->runtime->windrawable;
  wm_window_clear_drawable(wm);

  GHOST_WindowHandle ghostwin = GHOST_CreateWindow(
      g_system,
      static_cast<GHOST_WindowHandle>((win->parent) ? win->parent->ghostwin : nullptr),
      title,
      posx,
      posy,
      win->sizex,
      win->sizey,
      (GHOST_TWindowState)win->windowstate,
      is_dialog,
      gpu_settings);

  if (ghostwin) {
    win->gpuctx = GPU_context_create(ghostwin, nullptr);
    GPU_render_begin();

    /* Needed so we can detect the graphics card below. */
    GPU_init();

    /* Set window as drawable upon creation. Note this has already been
     * it has already been activated by GHOST_CreateWindow. */
    wm_window_set_drawable(wm, win, false);

    win->ghostwin = ghostwin;
    GHOST_SetWindowUserData(ghostwin, win); /* Pointer back. */

    wm_window_ensure_eventstate(win);

    /* Store actual window size in blender window. */
    /* WIN32: gives undefined window size when minimized. */
    if (GHOST_GetWindowState(static_cast<GHOST_WindowHandle>(win->ghostwin)) !=
        GHOST_kWindowStateMinimized)
    {
      wm_window_update_size_position(win);
    }

#ifndef __APPLE__
    /* Set the state here, so minimized state comes up correct on windows. */
    if (wm_init_state.window_focus) {
      GHOST_SetWindowState(ghostwin, (GHOST_TWindowState)win->windowstate);
    }
#endif

    /* Get the window background color from the current theme. Using the top-bar header
     * background theme color to match with the colored title-bar decoration style. */
    float window_bg_color[3];
    UI_SetTheme(SPACE_TOPBAR, RGN_TYPE_HEADER);
    UI_GetThemeColor3fv(TH_BACK, window_bg_color);

    /* Until screens get drawn, draw a default background using the window theme color. */
    wm_window_swap_buffer_acquire(win);
    GPU_clear_color(window_bg_color[0], window_bg_color[1], window_bg_color[2], 1.0f);

    /* Needed here, because it's used before it reads #UserDef. */
    WM_window_dpi_set_userdef(win);

    wm_window_swap_buffer_release(win);

    /* Clear double buffer to avoids flickering of new windows on certain drivers, see #97600. */
    GPU_clear_color(window_bg_color[0], window_bg_color[1], window_bg_color[2], 1.0f);

    GPU_render_end();
  }
  else {
    wm_window_set_drawable(wm, prev_windrawable, false);
  }
}

static void wm_window_ghostwindow_ensure(wmWindowManager *wm, wmWindow *win, bool is_dialog)
{
  bool new_window = false;
  char win_filepath[FILE_MAX];
  win_filepath[0] = '\0';

  if (win->ghostwin == nullptr) {
    new_window = true;
    if ((win->sizex == 0) || (wm_init_state.override_flag & WIN_OVERRIDE_GEOM)) {
      win->posx = wm_init_state.start[0];
      win->posy = wm_init_state.start[1];
      win->sizex = wm_init_state.size[0];
      win->sizey = wm_init_state.size[1];

      if (wm_init_state.override_flag & WIN_OVERRIDE_GEOM) {
        win->windowstate = GHOST_kWindowStateNormal;
        wm_init_state.override_flag &= ~WIN_OVERRIDE_GEOM;
      }
      else {
        win->windowstate = GHOST_WINDOW_STATE_DEFAULT;
      }
    }

    if (wm_init_state.override_flag & WIN_OVERRIDE_WINSTATE) {
      win->windowstate = wm_init_state.windowstate;
      wm_init_state.override_flag &= ~WIN_OVERRIDE_WINSTATE;
    }

    /* Without this, cursor restore may fail, see: #45456. */
    if (win->cursor == 0) {
      win->cursor = WM_CURSOR_DEFAULT;
    }

    /* As the window has not yet been created: #GHOST_SetPath cannot be called yet.
     * Use this callback to store the file-path path which is used later in this function
     * after the window has been created. */
    auto window_filepath_fn = (WM_capabilities_flag() & WM_CAPABILITY_WINDOW_PATH) ?
                                  std::optional([&win_filepath](const char *filepath) {
                                    STRNCPY_UTF8(win_filepath, filepath);
                                  }) :
                                  std::nullopt;
    std::string win_title = wm_window_title_text(wm, win, window_filepath_fn);
    wm_window_ghostwindow_add(wm, win_title.c_str(), win, is_dialog);
  }

  if (win->ghostwin != nullptr) {
    /* If we have no `ghostwin` this is a buggy window that should be removed.
     * However we still need to initialize it correctly so the screen doesn't hang. */

    /* Happens after file-read. */
    wm_window_ensure_eventstate(win);

    WM_window_dpi_set_userdef(win);

    if (WM_capabilities_flag() & WM_CAPABILITY_WINDOW_DECORATION_STYLES) {
      /* Only decoration style we have for now. */
      WM_window_decoration_style_flags_set(win, WM_WINDOW_DECORATION_STYLE_COLORED_TITLEBAR);
      WM_window_decoration_style_apply(win);
    }
  }

  /* Add key-map handlers (1 handler for all keys in map!). */
  wmKeyMap *keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Window", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&win->handlers, keymap);

  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Screen", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&win->handlers, keymap);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Screen Editing", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&win->modalhandlers, keymap);

  /* Add drop boxes. */
  {
    ListBase *lb = WM_dropboxmap_find("Window", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_dropbox_handler(&win->handlers, lb);
  }

  if (new_window) {
    if (win->ghostwin != nullptr) {
      if (win_filepath[0]) {
        GHOST_WindowHandle handle = static_cast<GHOST_WindowHandle>(win->ghostwin);
        GHOST_SetPath(handle, win_filepath);
      }
      wm_window_title_state_refresh(wm, win);
    }
  }
  else {
    WM_window_title_refresh(wm, win);
  }

  /* Add top-bar. */
  ED_screen_global_areas_refresh(win);
}

void wm_window_ghostwindows_ensure(wmWindowManager *wm)
{
  BLI_assert(G.background == false);

  /* No command-line prefsize? then we set this.
   * Note that these values will be used only
   * when there is no startup.blend yet.
   */
  if (wm_init_state.size[0] == 0) {
    if (UNLIKELY(!wm_get_screensize(wm_init_state.size))) {
      /* Use fallback values. */
      wm_init_state.size = blender::int2(0);
    }

    /* NOTE: this isn't quite correct, active screen maybe offset 1000s if PX,
     * we'd need a #wm_get_screensize like function that gives offset,
     * in practice the window manager will likely move to the correct monitor. */
    wm_init_state.start = blender::int2(0);
  }

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    wm_window_ghostwindow_ensure(wm, win, false);
  }
}

void wm_window_ghostwindows_remove_invalid(bContext *C, wmWindowManager *wm)
{
  BLI_assert(G.background == false);

  LISTBASE_FOREACH_MUTABLE (wmWindow *, win, &wm->windows) {
    if (win->ghostwin == nullptr) {
      wm_window_close(C, wm, win);
    }
  }
}

/* Update window size and position based on data from GHOST window. */
static bool wm_window_update_size_position(wmWindow *win)
{
  GHOST_RectangleHandle client_rect = GHOST_GetClientBounds(
      static_cast<GHOST_WindowHandle>(win->ghostwin));
  int l, t, r, b;
  GHOST_GetRectangle(client_rect, &l, &t, &r, &b);

  GHOST_DisposeRectangle(client_rect);

  int sizex = r - l;
  int sizey = b - t;

  int posx = 0;
  int posy = 0;

  if (WM_capabilities_flag() & WM_CAPABILITY_WINDOW_POSITION) {
    blender::int2 scr_size;
    if (wm_get_desktopsize(scr_size)) {
      posx = l;
      posy = scr_size[1] - t - win->sizey;
    }
  }

  if (win->sizex != sizex || win->sizey != sizey || win->posx != posx || win->posy != posy) {
    win->sizex = sizex;
    win->sizey = sizey;
    win->posx = posx;
    win->posy = posy;
    return true;
  }
  return false;
}

wmWindow *WM_window_open(bContext *C,
                         const char *title,
                         const rcti *rect_unscaled,
                         int space_type,
                         bool toplevel,
                         bool dialog,
                         bool temp,
                         eWindowAlignment alignment,
                         void (*area_setup_fn)(bScreen *screen, ScrArea *area, void *user_data),
                         void *area_setup_user_data)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win_prev = CTX_wm_window(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int x = rect_unscaled->xmin;
  int y = rect_unscaled->ymin;
  /* Duplicated windows are created at Area size, so duplicated
   * minimized areas can init at 2 pixels high before being
   * resized at the end of window creation. Therefore minimums. */
  int sizex = std::max(BLI_rcti_size_x(rect_unscaled), 200);
  int sizey = std::max(BLI_rcti_size_y(rect_unscaled), 150);
  rcti rect;

  const float native_pixel_size = GHOST_GetNativePixelSize(
      static_cast<GHOST_WindowHandle>(win_prev->ghostwin));
  /* Convert to native OS window coordinates. */
  rect.xmin = x / native_pixel_size;
  rect.ymin = y / native_pixel_size;
  sizex /= native_pixel_size;
  sizey /= native_pixel_size;

  if (alignment == WIN_ALIGN_LOCATION_CENTER) {
    /* Window centered around x,y location. */
    rect.xmin += win_prev->posx - (sizex / 2);
    rect.ymin += win_prev->posy - (sizey / 2);
  }
  else if (alignment == WIN_ALIGN_PARENT_CENTER) {
    /* Centered within parent. X,Y as offsets from there. */
    rect.xmin += win_prev->posx + ((win_prev->sizex - sizex) / 2);
    rect.ymin += win_prev->posy + ((win_prev->sizey - sizey) / 2);
  }
  else if (alignment == WIN_ALIGN_ABSOLUTE) {
    /* Positioned absolutely in desktop coordinates. */
  }

  rect.xmax = rect.xmin + sizex;
  rect.ymax = rect.ymin + sizey;

  /* Changes rect to fit within desktop. */
  wm_window_check_size(&rect);

  /* Reuse temporary windows when they share the same single area. */
  wmWindow *win = nullptr;
  if (temp) {
    LISTBASE_FOREACH (wmWindow *, win_iter, &wm->windows) {
      const bScreen *screen = WM_window_get_active_screen(win_iter);
      if (screen && screen->temp && BLI_listbase_is_single(&screen->areabase)) {
        ScrArea *area = static_cast<ScrArea *>(screen->areabase.first);
        if (space_type == (area->butspacetype ? area->butspacetype : area->spacetype)) {
          win = win_iter;
          break;
        }
      }
    }
  }

  /* Add new window? */
  if (win == nullptr) {
    win = wm_window_new(bmain, wm, toplevel ? nullptr : win_prev, dialog);
    win->posx = rect.xmin;
    win->posy = rect.ymin;
    win->sizex = BLI_rcti_size_x(&rect);
    win->sizey = BLI_rcti_size_y(&rect);
    *win->stereo3d_format = *win_prev->stereo3d_format;
  }

  bScreen *screen = WM_window_get_active_screen(win);

  if (WM_window_get_active_workspace(win) == nullptr) {
    WorkSpace *workspace = WM_window_get_active_workspace(win_prev);
    BKE_workspace_active_set(win->workspace_hook, workspace);
  }

  if (screen == nullptr) {
    /* Add new screen layout. */
    WorkSpace *workspace = WM_window_get_active_workspace(win);
    WorkSpaceLayout *layout = ED_workspace_layout_add(bmain, workspace, win, "temp");

    screen = BKE_workspace_layout_screen_get(layout);
    WM_window_set_active_layout(win, workspace, layout);
  }

  /* Set scene and view layer to match original window. */
  STRNCPY_UTF8(win->view_layer_name, view_layer->name);
  if (WM_window_get_active_scene(win) != scene) {
    /* No need to refresh the tool-system as the window has not yet finished being setup. */
    ED_screen_scene_change(C, win, scene, false);
  }

  screen->temp = temp;

  /* Make window active, and validate/resize. */
  CTX_wm_window_set(C, win);
  const bool new_window = (win->ghostwin == nullptr);

  if (area_setup_fn) {
    /* When the caller is setting up the area, it should always be empty
     * because it's expected the callback sets the type. */
    BLI_assert(space_type == SPACE_EMPTY);
    /* NOTE(@ideasman42): passing in a callback to setup the `area` is admittedly awkward.
     * This is done so #ED_screen_refresh has a valid area to initialize,
     * otherwise it will attempt to make the empty area usable via #ED_area_init.
     * While refreshing the window could be postponed this makes the state of the
     * window less predictable to the caller. */
    ScrArea *area = static_cast<ScrArea *>(screen->areabase.first);
    area_setup_fn(screen, area, area_setup_user_data);
    CTX_wm_area_set(C, area);
  }
  else if (space_type != SPACE_EMPTY) {
    /* Ensure it shows the right space-type editor. */
    ScrArea *area = static_cast<ScrArea *>(screen->areabase.first);
    CTX_wm_area_set(C, area);
    ED_area_newspace(C, area, space_type, false);
  }

  if (new_window) {
    wm_window_ghostwindow_ensure(wm, win, dialog);
  }
  WM_check(C);

  /* It's possible `win->ghostwin == nullptr`.
   * instead of attempting to cleanup here (in a half finished state),
   * finish setting up the screen, then free it at the end of the function,
   * to avoid having to take into account a partially-created window.
   */
  ED_screen_change(C, screen);

  if (!new_window) {
    /* Set size in GHOST window and then update size and position from GHOST,
     * in case they where changed by GHOST to fit the monitor/screen. */
    wm_window_set_size(win, win->sizex, win->sizey);
    wm_window_update_size_position(win);
  }

  /* Refresh screen dimensions, after the effective window size is known. */
  ED_screen_refresh(C, wm, win);

  if (win->ghostwin) {
    wm_window_raise(win);
    if (title) {
      WM_window_title_set(win, title);
    }
    else {
      WM_window_title_refresh(wm, win);
    }
    return win;
  }

  /* Very unlikely! but opening a new window can fail. */
  wm_window_close(C, wm, win);
  CTX_wm_window_set(C, win_prev);

  return nullptr;
}

wmWindow *WM_window_open_temp(bContext *C, const char *title, int space_type, bool dialog)
{
  rcti rect;
  WM_window_dpi_set_userdef(CTX_wm_window(C));
  eWindowAlignment align;
  rctf *stored_bounds = stored_window_bounds(eSpace_Type(space_type));
  const bool bounds_valid = (stored_bounds && (BLI_rctf_size_x(stored_bounds) > 150.0f) &&
                             (BLI_rctf_size_y(stored_bounds) > 100.0f));
  const bool mm_placement = WM_capabilities_flag() & WM_CAPABILITY_MULTIMONITOR_PLACEMENT;

  if (bounds_valid && mm_placement) {
    rect.xmin = int(stored_bounds->xmin * UI_SCALE_FAC);
    rect.ymin = int(stored_bounds->ymin * UI_SCALE_FAC);
    rect.xmax = int(stored_bounds->xmax * UI_SCALE_FAC);
    rect.ymax = int(stored_bounds->ymax * UI_SCALE_FAC);
    align = WIN_ALIGN_ABSOLUTE;
  }
  else {
    wmWindow *win_cur = CTX_wm_window(C);
    const int width = int((bounds_valid ? BLI_rctf_size_x(stored_bounds) : 800.0f) * UI_SCALE_FAC);
    const int height = int((bounds_valid ? BLI_rctf_size_y(stored_bounds) : 600.0f) *
                           UI_SCALE_FAC);
    /* Use eventstate, not event from _invoke, so this can be called through exec(). */
    const wmEvent *event = win_cur->eventstate;
    rect.xmin = event->xy[0];
    rect.ymin = event->xy[1];
    rect.xmax = event->xy[0] + width;
    rect.ymax = event->xy[1] + height;
    align = WIN_ALIGN_LOCATION_CENTER;
  }

  wmWindow *win = WM_window_open(
      C, title, &rect, space_type, false, dialog, true, align, nullptr, nullptr);

  return win;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operators
 * \{ */

wmOperatorStatus wm_window_close_exec(bContext *C, wmOperator * /*op*/)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  wm_window_close(C, wm, win);
  return OPERATOR_FINISHED;
}

wmOperatorStatus wm_window_new_exec(bContext *C, wmOperator *op)
{
  wmWindow *win_src = CTX_wm_window(C);
  ScrArea *area = BKE_screen_find_big_area(CTX_wm_screen(C), SPACE_TYPE_ANY, 0);
  const rcti window_rect = {
      /*xmin*/ 0,
      /*xmax*/ int(win_src->sizex * 0.95f),
      /*ymin*/ 0,
      /*ymax*/ int(win_src->sizey * 0.9f),
  };

  bool ok = (WM_window_open(C,
                            nullptr,
                            &window_rect,
                            area->spacetype,
                            false,
                            false,
                            false,
                            WIN_ALIGN_PARENT_CENTER,
                            nullptr,
                            nullptr) != nullptr);

  if (!ok) {
    BKE_report(op->reports, RPT_ERROR, "Failed to create window");
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

wmOperatorStatus wm_window_new_main_exec(bContext *C, wmOperator *op)
{
  wmWindow *win_src = CTX_wm_window(C);

  bool ok = (wm_window_copy_test(C, win_src, true, false) != nullptr);
  if (!ok) {
    BKE_report(op->reports, RPT_ERROR, "Failed to create window");
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

wmOperatorStatus wm_window_fullscreen_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  wmWindow *window = CTX_wm_window(C);

  if (G.background) {
    return OPERATOR_CANCELLED;
  }

  GHOST_TWindowState state = GHOST_GetWindowState(
      static_cast<GHOST_WindowHandle>(window->ghostwin));
  if (state != GHOST_kWindowStateFullScreen) {
    GHOST_SetWindowState(static_cast<GHOST_WindowHandle>(window->ghostwin),
                         GHOST_kWindowStateFullScreen);
  }
  else {
    GHOST_SetWindowState(static_cast<GHOST_WindowHandle>(window->ghostwin),
                         GHOST_kWindowStateNormal);
  }

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Events
 * \{ */

void wm_cursor_position_from_ghost_client_coords(wmWindow *win, int *x, int *y)
{
  float fac = GHOST_GetNativePixelSize(static_cast<GHOST_WindowHandle>(win->ghostwin));
  *x *= fac;

  *y = (win->sizey - 1) - *y;
  *y *= fac;
}

void wm_cursor_position_to_ghost_client_coords(wmWindow *win, int *x, int *y)
{
  float fac = GHOST_GetNativePixelSize(static_cast<GHOST_WindowHandle>(win->ghostwin));

  *x /= fac;
  *y /= fac;
  *y = win->sizey - *y - 1;
}

void wm_cursor_position_from_ghost_screen_coords(wmWindow *win, int *x, int *y)
{
  GHOST_ScreenToClient(static_cast<GHOST_WindowHandle>(win->ghostwin), *x, *y, x, y);
  wm_cursor_position_from_ghost_client_coords(win, x, y);
}

void wm_cursor_position_to_ghost_screen_coords(wmWindow *win, int *x, int *y)
{
  wm_cursor_position_to_ghost_client_coords(win, x, y);
  GHOST_ClientToScreen(static_cast<GHOST_WindowHandle>(win->ghostwin), *x, *y, x, y);
}

bool wm_cursor_position_get(wmWindow *win, int *r_x, int *r_y)
{
  if (UNLIKELY(G.f & G_FLAG_EVENT_SIMULATE)) {
    *r_x = win->eventstate->xy[0];
    *r_y = win->eventstate->xy[1];
    return true;
  }

  if (GHOST_GetCursorPosition(
          g_system, static_cast<GHOST_WindowHandle>(win->ghostwin), r_x, r_y) == GHOST_kSuccess)
  {
    wm_cursor_position_from_ghost_client_coords(win, r_x, r_y);
    return true;
  }

  return false;
}

/** Check if specified modifier key type is pressed. */
static uint8_t wm_ghost_modifier_query(const enum ModSide side)
{
  uint8_t result = 0;
  for (int i = 0; i < ARRAY_SIZE(g_modifier_table); i++) {
    bool val = false;
    GHOST_GetModifierKeyState(g_system, g_modifier_table[i].ghost_mask_pair[side], &val);
    if (val) {
      result |= g_modifier_table[i].flag;
    }
  }
  return result;
}

static void wm_window_set_drawable(wmWindowManager *wm, wmWindow *win, bool activate)
{
  BLI_assert(ELEM(wm->runtime->windrawable, nullptr, win));

  wm->runtime->windrawable = win;
  if (activate) {
    GHOST_ActivateWindowDrawingContext(static_cast<GHOST_WindowHandle>(win->ghostwin));
  }
  GPU_context_active_set(static_cast<GPUContext *>(win->gpuctx));
}

void wm_window_clear_drawable(wmWindowManager *wm)
{
  if (wm->runtime->windrawable) {
    wm->runtime->windrawable = nullptr;
  }
}

void wm_window_make_drawable(wmWindowManager *wm, wmWindow *win)
{
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());

  if (win != wm->runtime->windrawable && win->ghostwin) {
    // win->lmbut = 0; /* Keeps hanging when mouse-pressed while other window opened. */
    wm_window_clear_drawable(wm);

    if (G.debug & G_DEBUG_EVENTS) {
      printf("%s: set drawable %d\n", __func__, win->winid);
    }

    wm_window_set_drawable(wm, win, true);
  }

  if (win->ghostwin) {
    /* This can change per window. */
    WM_window_dpi_set_userdef(win);
  }
}

void wm_window_reset_drawable()
{
  BLI_assert(BLI_thread_is_main());
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());
  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);

  if (wm == nullptr) {
    return;
  }
  wmWindow *win = wm->runtime->windrawable;

  if (win && win->ghostwin) {
    wm_window_clear_drawable(wm);
    wm_window_set_drawable(wm, win, true);
  }
}

#ifndef NDEBUG
/**
 * Time-stamp validation that uses basic heuristics to warn about bad time-stamps.
 * Issues here should be resolved in GHOST.
 */
static void ghost_event_proc_timestamp_warning(GHOST_EventHandle ghost_event)
{
  /* NOTE: The following time constants can be tweaked if they're reporting false positives. */

  /* The reference event time-stamp must have happened in this time-frame. */
  constexpr uint64_t event_time_ok_ms = 1000;
  /* The current event time-stamp must be outside this time-frame to be considered an error. */
  constexpr uint64_t event_time_error_ms = 5000;

  static uint64_t event_ms_ref_last = std::numeric_limits<uint64_t>::max();
  const uint64_t event_ms = GHOST_GetEventTime(ghost_event);
  const uint64_t event_ms_ref = event_ms_ref_last;

  /* Assign first (allow early returns). */
  event_ms_ref_last = event_ms;

  if (event_ms_ref == std::numeric_limits<uint64_t>::max()) {
    return;
  }
  /* Check the events are recent enough to be used for testing. */
  const uint64_t now_ms = GHOST_GetMilliSeconds(g_system);
  /* Ensure the reference time occurred in the last #event_time_ok_ms.
   * If not, the reference time it's self may be a bad time-stamp. */
  if (event_ms_ref < event_time_error_ms || (event_ms_ref < (now_ms - event_time_ok_ms)) ||
      (event_ms_ref > (now_ms + event_time_ok_ms)))
  {
    /* Skip, the reference time not recent enough to be used. */
    return;
  }

  /* NOTE: Regarding time-stamps from the future.
   * Generally this shouldn't happen but may do depending on the kinds of events.
   * Different input methods may detect and trigger events in a way that wont ensure
   * monotonic event times, so only consider this an error for large time deltas. */
  double time_delta = 0.0;
  if (event_ms < (event_ms_ref - event_time_error_ms)) {
    /* New event time is after (to be expected). */
    time_delta = double(now_ms - event_ms) / -1000.0;
  }
  else if (event_ms > (event_ms_ref + event_time_error_ms)) {
    /* New event time is before (unexpected but not an error). */
    time_delta = double(event_ms - now_ms) / 1000.0;
  }
  else {
    /* Time is in range. */
    return;
  }

  const char *time_unit = "seconds";
  const struct {
    const char *unit;
    double scale;
  } unit_table[] = {{"minutes", 60}, {"hours", 60}, {"days", 24}, {"weeks", 7}, {"years", 52}};
  for (int i = 0; i < ARRAY_SIZE(unit_table); i++) {
    if (std::abs(time_delta) <= unit_table[i].scale) {
      break;
    }
    time_delta /= unit_table[i].scale;
    time_unit = unit_table[i].unit;
  }

  CLOG_INFO_NOCHECK(WM_LOG_EVENTS,
                    "GHOST: suspicious time-stamp from far in the %s: %.2f %s, "
                    "absolute value is %" PRIu64 ", current time is %" PRIu64 ", for type %d\n",
                    time_delta < 0.0f ? "past" : "future",
                    std::abs(time_delta),
                    time_unit,
                    event_ms,
                    now_ms,
                    int(GHOST_GetEventType(ghost_event)));
}
#endif /* !NDEBUG */

/**
 * Called by ghost, here we handle events for windows themselves or send to event system.
 *
 * Mouse coordinate conversion happens here.
 */
static bool ghost_event_proc(GHOST_EventHandle ghost_event, GHOST_TUserDataPtr C_void_ptr)
{
  bContext *C = static_cast<bContext *>(C_void_ptr);
  wmWindowManager *wm = CTX_wm_manager(C);
  GHOST_TEventType type = GHOST_GetEventType(ghost_event);

  GHOST_WindowHandle ghostwin = GHOST_GetEventWindow(ghost_event);

#ifndef NDEBUG
  ghost_event_proc_timestamp_warning(ghost_event);
#endif

  if (type == GHOST_kEventQuitRequest) {
    /* Find an active window to display quit dialog in. */
    wmWindow *win;
    if (ghostwin && GHOST_ValidWindow(g_system, ghostwin)) {
      win = static_cast<wmWindow *>(GHOST_GetWindowUserData(ghostwin));
    }
    else {
      win = wm->runtime->winactive;
    }

    /* Display quit dialog or quit immediately. */
    if (win) {
      wm_quit_with_optional_confirmation_prompt(C, win);
    }
    else {
      wm_exit_schedule_delayed(C);
    }
    return true;
  }

  GHOST_TEventDataPtr data = GHOST_GetEventData(ghost_event);
  const uint64_t event_time_ms = GHOST_GetEventTime(ghost_event);

  /* Ghost now can call this function for life resizes,
   * but it should return if WM didn't initialize yet.
   * Can happen on file read (especially full size window). */
  if ((wm->init_flag & WM_INIT_FLAG_WINDOW) == 0) {
    return true;
  }
  if (!ghostwin) {
    /* XXX: should be checked, why are we getting an event here, and what is it? */
    puts("<!> event has no window");
    return true;
  }
  if (!GHOST_ValidWindow(g_system, ghostwin)) {
    /* XXX: should be checked, why are we getting an event here, and what is it? */
    puts("<!> event has invalid window");
    return true;
  }

  wmWindow *win = static_cast<wmWindow *>(GHOST_GetWindowUserData(ghostwin));

  switch (type) {
    case GHOST_kEventWindowDeactivate: {
      wm_window_update_eventstate_modifiers_clear(wm, win, event_time_ms);

      wm_event_add_ghostevent(wm, win, type, data, event_time_ms);
      win->active = 0;
      break;
    }
    case GHOST_kEventWindowActivate: {
      /* Ensure the event state matches modifiers (window was inactive). */
      wm_window_update_eventstate_modifiers(wm, win, event_time_ms);

      /* Entering window, update mouse position (without sending an event). */
      wm_window_update_eventstate(win);

      /* No context change! `C->wm->runtime->windrawable` is drawable, or for area queues. */
      wm->runtime->winactive = win;
      win->active = 1;

      /* Zero the `keymodifier`, it hangs on hotkeys that open windows otherwise. */
      win->eventstate->keymodifier = EVENT_NONE;

      win->addmousemove = 1; /* Enables highlighted buttons. */

      wm_window_make_drawable(wm, win);

      /* NOTE(@sergey): Window might be focused by mouse click in configuration of window manager
       * when focus is not following mouse
       * click could have been done on a button and depending on window manager settings
       * click would be passed to blender or not, but in any case button under cursor
       * should be activated, so at max next click on button without moving mouse
       * would trigger its handle function
       * currently it seems to be common practice to generate new event for, but probably
       * we'll need utility function for this?
       */
      wmEvent event;
      wm_event_init_from_window(win, &event);
      event.type = MOUSEMOVE;
      event.val = KM_NOTHING;
      copy_v2_v2_int(event.prev_xy, event.xy);
      event.flag = eWM_EventFlag(0);

      WM_event_add(win, &event);

      break;
    }
    case GHOST_kEventWindowClose: {
      wm_window_close(C, wm, win);
      break;
    }
    case GHOST_kEventWindowUpdate: {
      if (G.debug & G_DEBUG_EVENTS) {
        printf("%s: ghost redraw %d\n", __func__, win->winid);
      }

      wm_window_make_drawable(wm, win);
      WM_event_add_notifier_ex(wm, win, NC_WINDOW, nullptr);

      break;
    }
    case GHOST_kEventWindowUpdateDecor: {
      if (G.debug & G_DEBUG_EVENTS) {
        printf("%s: ghost redraw decor %d\n", __func__, win->winid);
      }

      wm_window_make_drawable(wm, win);
#if 0
        /* NOTE(@ideasman42): Ideally we could swap-buffers to avoid a full redraw.
         * however this causes window flickering on resize with LIBDECOR under WAYLAND. */
        wm_window_swap_buffer_release(win);
#else
      WM_event_add_notifier_ex(wm, win, NC_WINDOW, nullptr);
#endif

      break;
    }
    case GHOST_kEventWindowSize:
    case GHOST_kEventWindowMove: {
      GHOST_TWindowState state = GHOST_GetWindowState(
          static_cast<GHOST_WindowHandle>(win->ghostwin));
      win->windowstate = state;

      WM_window_dpi_set_userdef(win);

      /* WIN32: gives undefined window size when minimized. */
      if (state != GHOST_kWindowStateMinimized) {
        /*
         * Ghost sometimes send size or move events when the window hasn't changed.
         * One case of this is using COMPIZ on Linux.
         * To alleviate the problem we ignore all such event here.
         *
         * It might be good to eventually do that at GHOST level, but that is for another time.
         */
        if (wm_window_update_size_position(win)) {
          const bScreen *screen = WM_window_get_active_screen(win);

          /* Debug prints. */
          if (G.debug & G_DEBUG_EVENTS) {
            const char *state_str;
            state = GHOST_GetWindowState(static_cast<GHOST_WindowHandle>(win->ghostwin));

            if (state == GHOST_kWindowStateNormal) {
              state_str = "normal";
            }
            else if (state == GHOST_kWindowStateMinimized) {
              state_str = "minimized";
            }
            else if (state == GHOST_kWindowStateMaximized) {
              state_str = "maximized";
            }
            else if (state == GHOST_kWindowStateFullScreen) {
              state_str = "full-screen";
            }
            else {
              state_str = "<unknown>";
            }

            printf("%s: window %d state = %s\n", __func__, win->winid, state_str);

            if (type != GHOST_kEventWindowSize) {
              printf("win move event pos %d %d size %d %d\n",
                     win->posx,
                     win->posy,
                     win->sizex,
                     win->sizey);
            }
          }

          wm_window_make_drawable(wm, win);
          BKE_icon_changed(screen->id.icon_id);
          WM_event_add_notifier_ex(wm, win, NC_SCREEN | NA_EDITED, nullptr);
          WM_event_add_notifier_ex(wm, win, NC_WINDOW | NA_EDITED, nullptr);

#if defined(__APPLE__) || defined(WIN32)
          /* MACOS and WIN32 don't return to the main-loop while resize. */
          int dummy_sleep_ms = 0;
          wm_window_timers_process(C, &dummy_sleep_ms);
          wm_event_do_handlers(C);
          wm_event_do_notifiers(C);
          wm_draw_update(C);
#endif
        }
      }
      break;
    }

    case GHOST_kEventWindowDPIHintChanged: {
      WM_window_dpi_set_userdef(win);
      /* Font's are stored at each DPI level, without this we can easy load 100's of fonts. */
      BLF_cache_clear();

      WM_main_add_notifier(NC_WINDOW, nullptr);             /* Full redraw. */
      WM_main_add_notifier(NC_SCREEN | NA_EDITED, nullptr); /* Refresh region sizes. */
      break;
    }

    case GHOST_kEventOpenMainFile: {
      const char *path = static_cast<const char *>(data);

      if (path) {
        wmOperatorType *ot = WM_operatortype_find("WM_OT_open_mainfile", false);
        /* Operator needs a valid window in context, ensures it is correctly set. */
        CTX_wm_window_set(C, win);

        PointerRNA props_ptr;
        WM_operator_properties_create_ptr(&props_ptr, ot);
        RNA_string_set(&props_ptr, "filepath", path);
        RNA_boolean_set(&props_ptr, "display_file_selector", false);
        WM_operator_name_call_ptr(
            C, ot, blender::wm::OpCallContext::InvokeDefault, &props_ptr, nullptr);
        WM_operator_properties_free(&props_ptr);

        CTX_wm_window_set(C, nullptr);
      }
      break;
    }
    case GHOST_kEventDraggingDropDone: {
      const GHOST_TEventDragnDropData *ddd = static_cast<const GHOST_TEventDragnDropData *>(data);

      /* Ensure the event state matches modifiers (window was inactive). */
      wm_window_update_eventstate_modifiers(wm, win, event_time_ms);
      /* Entering window, update mouse position (without sending an event). */
      wm_window_update_eventstate(win);

      wmEvent event;
      wm_event_init_from_window(win, &event); /* Copy last state, like mouse coords. */

      /* Activate region. */
      event.type = MOUSEMOVE;
      event.val = KM_NOTHING;
      copy_v2_v2_int(event.prev_xy, event.xy);

      copy_v2_v2_int(event.xy, &ddd->x);
      wm_cursor_position_from_ghost_screen_coords(win, &event.xy[0], &event.xy[1]);

      /* The values from #wm_window_update_eventstate may not match (under WAYLAND they don't)
       * Write this into the event state. */
      copy_v2_v2_int(win->eventstate->xy, event.xy);

      event.flag = eWM_EventFlag(0);

      /* No context change! `C->wm->runtime->windrawable` is drawable, or for area queues. */
      wm->runtime->winactive = win;
      win->active = 1;

      WM_event_add(win, &event);

      /* Make blender drop event with custom data pointing to wm drags. */
      event.type = EVT_DROP;
      event.val = KM_RELEASE;
      event.custom = EVT_DATA_DRAGDROP;
      event.customdata = &wm->runtime->drags;
      event.customdata_free = true;

      WM_event_add(win, &event);

      // printf("Drop detected\n");

      /* Add drag data to wm for paths. */

      if (ddd->dataType == GHOST_kDragnDropTypeFilenames) {
        const GHOST_TStringArray *stra = static_cast<const GHOST_TStringArray *>(ddd->data);

        if (stra->count) {
          CLOG_INFO(WM_LOG_EVENTS, "Drop %d files:", stra->count);
          for (const char *path : blender::Span((char **)stra->strings, stra->count)) {
            CLOG_INFO(WM_LOG_EVENTS, "%s", path);
          }
          /* Try to get icon type from extension of the first path. */
          int icon = ED_file_extension_icon((char *)stra->strings[0]);
          wmDragPath *path_data = WM_drag_create_path_data(
              blender::Span((char **)stra->strings, stra->count));
          WM_event_start_drag(C, icon, WM_DRAG_PATH, path_data, WM_DRAG_NOP);
          /* Void pointer should point to string, it makes a copy. */
        }
      }
      else if (ddd->dataType == GHOST_kDragnDropTypeString) {
        /* Drop an arbitrary string. */
        std::string *str = MEM_new<std::string>(__func__, static_cast<const char *>(ddd->data));
        WM_event_start_drag(C, ICON_NONE, WM_DRAG_STRING, str, WM_DRAG_FREE_DATA);
      }

      break;
    }
    case GHOST_kEventNativeResolutionChange: {
      /* Only update if the actual pixel size changes. */
      float prev_pixelsize = U.pixelsize;
      WM_window_dpi_set_userdef(win);

      if (U.pixelsize != prev_pixelsize) {
        BKE_icon_changed(WM_window_get_active_screen(win)->id.icon_id);

        /* Close all popups since they are positioned with the pixel
         * size baked in and it's difficult to correct them. */
        CTX_wm_window_set(C, win);
        UI_popup_handlers_remove_all(C, &win->modalhandlers);
        CTX_wm_window_set(C, nullptr);

        wm_window_make_drawable(wm, win);

        WM_event_add_notifier_ex(wm, win, NC_SCREEN | NA_EDITED, nullptr);
        WM_event_add_notifier_ex(wm, win, NC_WINDOW | NA_EDITED, nullptr);
      }

      break;
    }
    case GHOST_kEventButtonDown:
    case GHOST_kEventButtonUp: {
      if (win->active == 0) {
        /* Entering window, update cursor/tablet state & modifiers.
         * (ghost sends win-activate *after* the mouse-click in window!). */
        wm_window_update_eventstate_modifiers(wm, win, event_time_ms);
        wm_window_update_eventstate(win);
      }

      wm_event_add_ghostevent(wm, win, type, data, event_time_ms);
      break;
    }
    default: {
      wm_event_add_ghostevent(wm, win, type, data, event_time_ms);
      break;
    }
  }

  return true;
}

/**
 * This timer system only gives maximum 1 timer event per redraw cycle,
 * to prevent queues to get overloaded.
 * - Timer handlers should check for delta to decide if they just update, or follow real time.
 * - Timer handlers can also set duration to match frames passed.
 *
 * \param sleep_us_p: The number of microseconds to sleep which may be reduced by this function
 * to account for timers that would run during the anticipated sleep period.
 */
static bool wm_window_timers_process(const bContext *C, int *sleep_us_p)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  const double time = BLI_time_now_seconds();
  bool has_event = false;

  const int sleep_us = *sleep_us_p;
  /* The nearest time an active timer is scheduled to run. */
  double ntime_min = DBL_MAX;

  /* Mutable in case the timer gets removed. */
  LISTBASE_FOREACH_MUTABLE (wmTimer *, wt, &wm->runtime->timers) {
    if (wt->flags & WM_TIMER_TAGGED_FOR_REMOVAL) {
      continue;
    }
    if (wt->sleep == true) {
      continue;
    }

    /* Future timer, update nearest time & skip. */
    if (wt->time_next >= time) {
      if ((has_event == false) && (sleep_us != 0)) {
        /* The timer is not ready to run but may run shortly. */
        ntime_min = std::min(wt->time_next, ntime_min);
      }
      continue;
    }

    wt->time_delta = time - wt->time_last;
    wt->time_duration += wt->time_delta;
    wt->time_last = time;

    wt->time_next = wt->time_start;
    if (wt->time_step != 0.0f) {
      wt->time_next += wt->time_step * ceil(wt->time_duration / wt->time_step);
    }

    if (wt->event_type == TIMERJOBS) {
      wm_jobs_timer(wm, wt);
    }
    else if (wt->event_type == TIMERAUTOSAVE) {
      wm_autosave_timer(bmain, wm, wt);
    }
    else if (wt->event_type == TIMERNOTIFIER) {
      WM_main_add_notifier(POINTER_AS_UINT(wt->customdata), nullptr);
    }
    else if (wmWindow *win = wt->win) {
      wmEvent event;
      wm_event_init_from_window(win, &event);

      event.type = wt->event_type;
      event.val = KM_NOTHING;
      event.keymodifier = EVENT_NONE;
      event.flag = eWM_EventFlag(0);
      event.custom = EVT_DATA_TIMER;
      event.customdata = wt;
      WM_event_add(win, &event);

      has_event = true;
    }
  }

  if ((has_event == false) && (sleep_us != 0) && (ntime_min != DBL_MAX)) {
    /* Clamp the sleep time so next execution runs earlier (if necessary).
     * Use `ceil` so the timer is guarantee to be ready to run (not always the case with rounding).
     * Even though using `floor` or `round` is more responsive,
     * it causes CPU intensive loops that may run until the timer is reached, see: #111579. */
    const double microseconds = 1000000.0;
    const double sleep_sec = double(sleep_us) / microseconds;
    const double sleep_sec_next = ntime_min - time;

    if (sleep_sec_next < sleep_sec) {
      *sleep_us_p = int(std::ceil(sleep_sec_next * microseconds));
    }
  }

  /* Effectively delete all timers marked for removal. */
  wm_window_timers_delete_removed(wm);

  return has_event;
}

void wm_window_events_process(const bContext *C)
{
  BLI_assert(BLI_thread_is_main());
  GPU_render_begin();

  bool has_event = GHOST_ProcessEvents(g_system, false); /* `false` is no wait. */

  if (has_event) {
    GHOST_DispatchEvents(g_system);
  }

  /* When there is no event, sleep 5 milliseconds not to use too much CPU when idle. */
  const int sleep_us_default = 5000;
  int sleep_us = has_event ? 0 : sleep_us_default;
  has_event |= wm_window_timers_process(C, &sleep_us);
#ifdef WITH_XR_OPENXR
  /* XR events don't use the regular window queues. So here we don't only trigger
   * processing/dispatching but also handling. */
  has_event |= wm_xr_events_handle(CTX_wm_manager(C));
#endif
  GPU_render_end();

  /* Skip sleeping when simulating events so tests don't idle unnecessarily as simulated
   * events are typically generated from a timer that runs in the main loop. */
  if ((has_event == false) && (sleep_us != 0) && !(G.f & G_FLAG_EVENT_SIMULATE)) {
    BLI_time_sleep_precise_us(sleep_us);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ghost Init/Exit
 * \{ */

void wm_ghost_init(bContext *C)
{
  if (g_system) {
    return;
  }

  BLI_assert(C != nullptr);
  BLI_assert_msg(!G.background, "Use wm_ghost_init_background instead");

  GHOST_EventConsumerHandle consumer;

  consumer = GHOST_CreateEventConsumer(ghost_event_proc, C);

  GHOST_SetBacktraceHandler((GHOST_TBacktraceFn)BLI_system_backtrace);
  GHOST_UseWindowFrame(wm_init_state.window_frame);

  g_system = GHOST_CreateSystem();
  GPU_backend_ghost_system_set(g_system);

  if (UNLIKELY(g_system == nullptr)) {
    /* GHOST will have reported the back-ends that failed to load. */
    CLOG_STR_ERROR(&LOG_GHOST_SYSTEM, "Unable to initialize GHOST, exiting!");
    /* This will leak memory, it's preferable to crashing. */
    exit(EXIT_FAILURE);
  }
#if !(defined(WIN32) || defined(__APPLE__))
  g_system_backend_id = GHOST_SystemBackend();
#endif

  GHOST_Debug debug = {0};
  if (G.debug & G_DEBUG_GHOST) {
    debug.flags |= GHOST_kDebugDefault;
  }
  if (G.debug & G_DEBUG_WINTAB) {
    debug.flags |= GHOST_kDebugWintab;
  }
  GHOST_SystemInitDebug(g_system, debug);

  GHOST_AddEventConsumer(g_system, consumer);

  if (wm_init_state.native_pixels) {
    GHOST_UseNativePixels();
  }

  GHOST_UseWindowFocus(wm_init_state.window_focus);
}

void wm_ghost_init_background()
{
  /* TODO: move this to `wm_init_exit.cc`. */

  if (g_system) {
    return;
  }

  GHOST_SetBacktraceHandler((GHOST_TBacktraceFn)BLI_system_backtrace);

  g_system = GHOST_CreateSystemBackground();
  GPU_backend_ghost_system_set(g_system);

  GHOST_Debug debug = {0};
  if (G.debug & G_DEBUG_GHOST) {
    debug.flags |= GHOST_kDebugDefault;
  }
  GHOST_SystemInitDebug(g_system, debug);
}

void wm_ghost_exit()
{
  if (g_system) {
    GHOST_DisposeSystem(g_system);
  }
  g_system = nullptr;
}

const char *WM_ghost_backend()
{
#if !(defined(WIN32) || defined(__APPLE__))
  return g_system_backend_id ? g_system_backend_id : "NONE";
#else
  /* While this could be supported, at the moment it's only needed with GHOST X11/WAYLAND
   * to check which was selected and the API call may be removed after that's no longer needed.
   * Use dummy values to prevent this being used on other systems. */
  return g_system ? "DEFAULT" : "NONE";
#endif
}

GHOST_TDrawingContextType wm_ghost_drawing_context_type(const GPUBackendType gpu_backend)
{
  switch (gpu_backend) {
    case GPU_BACKEND_NONE:
      return GHOST_kDrawingContextTypeNone;
    case GPU_BACKEND_ANY:
    case GPU_BACKEND_OPENGL:
#ifdef WITH_OPENGL_BACKEND
      return GHOST_kDrawingContextTypeOpenGL;
#endif
      BLI_assert_unreachable();
      return GHOST_kDrawingContextTypeNone;
    case GPU_BACKEND_VULKAN:
#ifdef WITH_VULKAN_BACKEND
      return GHOST_kDrawingContextTypeVulkan;
#endif
      BLI_assert_unreachable();
      return GHOST_kDrawingContextTypeNone;
    case GPU_BACKEND_METAL:
#ifdef WITH_METAL_BACKEND
      return GHOST_kDrawingContextTypeMetal;
#endif
      BLI_assert_unreachable();
      return GHOST_kDrawingContextTypeNone;
  }

  /* Avoid control reaches end of non-void function compilation warning, which could be promoted
   * to error. */
  BLI_assert_unreachable();
  return GHOST_kDrawingContextTypeNone;
}

void wm_test_gpu_backend_fallback(bContext *C)
{
  if (!bool(G.f & G_FLAG_GPU_BACKEND_FALLBACK)) {
    return;
  }

  /* Have we already shown a message during this Blender session. */
  if (bool(G.f & G_FLAG_GPU_BACKEND_FALLBACK_QUIET)) {
    return;
  }
  G.f |= G_FLAG_GPU_BACKEND_FALLBACK_QUIET;

  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = static_cast<wmWindow *>((wm->runtime->winactive) ? wm->runtime->winactive :
                                                                     wm->windows.first);

  if (win) {
    /* We want this warning on the Main window, not a child window even if active. See #118765. */
    if (win->parent) {
      win = win->parent;
    }

    wmWindow *prevwin = CTX_wm_window(C);
    CTX_wm_window_set(C, win);
    std::string message = RPT_("Updating GPU drivers may solve this issue.");
    message += RPT_(
        "The graphics backend can be changed in the System section of the Preferences.");
    UI_alert(C,
             RPT_("Failed to load using Vulkan, using OpenGL instead."),
             message,
             ALERT_ICON_ERROR,
             false);
    CTX_wm_window_set(C, prevwin);
  }
}

eWM_CapabilitiesFlag WM_capabilities_flag()
{
  static eWM_CapabilitiesFlag flag = eWM_CapabilitiesFlag(0);
  if (flag != 0) {
    return flag;
  }
  flag |= WM_CAPABILITY_INITIALIZED;

  /* NOTE(@ideasman42): Regarding tests.
   * Some callers of this function may run from tests where GHOST's hasn't been initialized.
   * In such cases it may be necessary to check `!G.background` which is acceptable in most cases.
   * At time of writing this is the case for `bl_animation_keyframing`.
   *
   * While this function *could* early-exit when in background mode, don't do this as GHOST
   * may be initialized in background mode for GPU rendering and in this case we may want to
   * query GHOST/GPU related capabilities. */

  const GHOST_TCapabilityFlag ghost_flag = GHOST_GetCapabilities();
  if (ghost_flag & GHOST_kCapabilityCursorWarp) {
    flag |= WM_CAPABILITY_CURSOR_WARP;
  }
  if (ghost_flag & GHOST_kCapabilityWindowPosition) {
    flag |= WM_CAPABILITY_WINDOW_POSITION;
  }
  if (ghost_flag & GHOST_kCapabilityClipboardPrimary) {
    flag |= WM_CAPABILITY_CLIPBOARD_PRIMARY;
  }
  if (ghost_flag & GHOST_kCapabilityGPUReadFrontBuffer) {
    flag |= WM_CAPABILITY_GPU_FRONT_BUFFER_READ;
  }
  if (ghost_flag & GHOST_kCapabilityClipboardImage) {
    flag |= WM_CAPABILITY_CLIPBOARD_IMAGE;
  }
  if (ghost_flag & GHOST_kCapabilityDesktopSample) {
    flag |= WM_CAPABILITY_DESKTOP_SAMPLE;
  }
  if (ghost_flag & GHOST_kCapabilityInputIME) {
    flag |= WM_CAPABILITY_INPUT_IME;
  }
  if (ghost_flag & GHOST_kCapabilityTrackpadPhysicalDirection) {
    flag |= WM_CAPABILITY_TRACKPAD_PHYSICAL_DIRECTION;
  }
  if (ghost_flag & GHOST_kCapabilityWindowDecorationStyles) {
    flag |= WM_CAPABILITY_WINDOW_DECORATION_STYLES;
  }
  if (ghost_flag & GHOST_kCapabilityKeyboardHyperKey) {
    flag |= WM_CAPABILITY_KEYBOARD_HYPER_KEY;
  }
  if (ghost_flag & GHOST_kCapabilityCursorRGBA) {
    flag |= WM_CAPABILITY_CURSOR_RGBA;
  }
  if (ghost_flag & GHOST_kCapabilityCursorGenerator) {
    flag |= WM_CAPABILITY_CURSOR_GENERATOR;
  }
  if (ghost_flag & GHOST_kCapabilityMultiMonitorPlacement) {
    flag |= WM_CAPABILITY_MULTIMONITOR_PLACEMENT;
  }
  if (ghost_flag & GHOST_kCapabilityWindowPath) {
    flag |= WM_CAPABILITY_WINDOW_PATH;
  }
  return flag;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Timer
 * \{ */

void WM_event_timer_sleep(wmWindowManager *wm, wmWindow * /*win*/, wmTimer *timer, bool do_sleep)
{
  /* Extra security check. */
  if (BLI_findindex(&wm->runtime->timers, timer) == -1) {
    return;
  }
  /* It's disputable if this is needed, when tagged for removal,
   * the sleep value won't be used anyway. */
  if (timer->flags & WM_TIMER_TAGGED_FOR_REMOVAL) {
    return;
  }
  timer->sleep = do_sleep;
}

wmTimer *WM_event_timer_add(wmWindowManager *wm,
                            wmWindow *win,
                            const wmEventType event_type,
                            const double time_step)
{
  BLI_assert(ISTIMER(event_type));

  wmTimer *wt = MEM_callocN<wmTimer>("window timer");
  BLI_assert(time_step >= 0.0f);

  wt->event_type = event_type;
  wt->time_last = BLI_time_now_seconds();
  wt->time_next = wt->time_last + time_step;
  wt->time_start = wt->time_last;
  wt->time_step = time_step;
  wt->win = win;

  BLI_addtail(&wm->runtime->timers, wt);

  return wt;
}

wmTimer *WM_event_timer_add_notifier(wmWindowManager *wm,
                                     wmWindow *win,
                                     const uint type,
                                     const double time_step)
{
  wmTimer *wt = MEM_callocN<wmTimer>("window timer");
  BLI_assert(time_step >= 0.0f);

  wt->event_type = TIMERNOTIFIER;
  wt->time_last = BLI_time_now_seconds();
  wt->time_next = wt->time_last + time_step;
  wt->time_start = wt->time_last;
  wt->time_step = time_step;
  wt->win = win;
  wt->customdata = POINTER_FROM_UINT(type);
  wt->flags |= WM_TIMER_NO_FREE_CUSTOM_DATA;

  BLI_addtail(&wm->runtime->timers, wt);

  return wt;
}

void wm_window_timers_delete_removed(wmWindowManager *wm)
{
  LISTBASE_FOREACH_MUTABLE (wmTimer *, wt, &wm->runtime->timers) {
    if ((wt->flags & WM_TIMER_TAGGED_FOR_REMOVAL) == 0) {
      continue;
    }

    /* Actual removal and freeing of the timer. */
    BLI_remlink(&wm->runtime->timers, wt);
    MEM_freeN(wt);
  }
}

void WM_event_timer_free_data(wmTimer *timer)
{
  if (timer->customdata != nullptr && (timer->flags & WM_TIMER_NO_FREE_CUSTOM_DATA) == 0) {
    MEM_freeN(timer->customdata);
    timer->customdata = nullptr;
  }
}

void WM_event_timer_remove(wmWindowManager *wm, wmWindow * /*win*/, wmTimer *timer)
{
  /* Extra security check. */
  if (BLI_findindex(&wm->runtime->timers, timer) == -1) {
    return;
  }

  timer->flags |= WM_TIMER_TAGGED_FOR_REMOVAL;

  /* Clear existing references to the timer. */
  if (wm->runtime->reports.reporttimer == timer) {
    wm->runtime->reports.reporttimer = nullptr;
  }
  /* There might be events in queue with this timer as customdata. */
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    LISTBASE_FOREACH (wmEvent *, event, &win->runtime->event_queue) {
      if (event->customdata == timer) {
        event->customdata = nullptr;
        event->type = EVENT_NONE; /* Timer users customdata, don't want `nullptr == nullptr`. */
      }
    }
  }

  /* Immediately free `customdata` if requested, so that invalid usages of that data after
   * calling `WM_event_timer_remove` can be easily spotted (through ASAN errors e.g.). */
  WM_event_timer_free_data(timer);
}

void WM_event_timer_remove_notifier(wmWindowManager *wm, wmWindow *win, wmTimer *timer)
{
  timer->customdata = nullptr;
  WM_event_timer_remove(wm, win, timer);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clipboard Wrappers
 *
 * GHOST function wrappers that support a "fake" clipboard used when simulating events.
 * This is useful user actions can be simulated while the system is in use without the system's
 * clipboard getting overwritten.
 * \{ */

struct {
  char *buffers[2];
} *g_wm_clipboard_text_simulate = nullptr;

void wm_clipboard_free()
{
  if (g_wm_clipboard_text_simulate == nullptr) {
    return;
  }
  for (int i = 0; i < ARRAY_SIZE(g_wm_clipboard_text_simulate->buffers); i++) {
    char *buf = g_wm_clipboard_text_simulate->buffers[i];
    if (buf) {
      MEM_freeN(buf);
    }
  }
  MEM_freeN(g_wm_clipboard_text_simulate);
  g_wm_clipboard_text_simulate = nullptr;
}

static char *wm_clipboard_text_get_impl(bool selection)
{
  if (UNLIKELY(G.f & G_FLAG_EVENT_SIMULATE)) {
    if (g_wm_clipboard_text_simulate == nullptr) {
      return nullptr;
    }
    const char *buf_src = g_wm_clipboard_text_simulate->buffers[int(selection)];
    if (buf_src == nullptr) {
      return nullptr;
    }
    size_t size = strlen(buf_src) + 1;
    char *buf = static_cast<char *>(malloc(size));
    memcpy(buf, buf_src, size);
    return buf;
  }

  return GHOST_getClipboard(selection);
}

static void wm_clipboard_text_set_impl(const char *buf, bool selection)
{
  if (UNLIKELY(G.f & G_FLAG_EVENT_SIMULATE)) {
    if (g_wm_clipboard_text_simulate == nullptr) {
      g_wm_clipboard_text_simulate =
          MEM_callocN<std::remove_pointer_t<decltype(g_wm_clipboard_text_simulate)>>(__func__);
    }
    char **buf_src_p = &(g_wm_clipboard_text_simulate->buffers[int(selection)]);
    MEM_SAFE_FREE(*buf_src_p);
    *buf_src_p = BLI_strdup(buf);
    return;
  }

  GHOST_putClipboard(buf, selection);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clipboard
 * \{ */

static char *wm_clipboard_text_get_ex(bool selection,
                                      int *r_len,
                                      const bool ensure_utf8,
                                      const bool firstline)
{
  if (G.background) {
    *r_len = 0;
    return nullptr;
  }

  char *buf = wm_clipboard_text_get_impl(selection);
  if (!buf) {
    *r_len = 0;
    return nullptr;
  }

  int buf_len = strlen(buf);

  if (ensure_utf8) {
    /* TODO(@ideasman42): It would be good if unexpected byte sequences could be interpreted
     * instead of stripped - so mixed in characters (typically Latin1) aren't ignored.
     * Check on how Python bytes this, see: #PyC_UnicodeFromBytesAndSize,
     * there are clever ways to handle this although they increase the size of the buffer. */
    buf_len -= BLI_str_utf8_invalid_strip(buf, buf_len);
  }

  /* Always convert from `\r\n` to `\n`. */
  char *newbuf = MEM_malloc_arrayN<char>(size_t(buf_len + 1), __func__);
  char *p2 = newbuf;

  if (firstline) {
    /* Will return an over-allocated value in the case there are newlines. */
    for (char *p = buf; *p; p++) {
      if (!ELEM(*p, '\n', '\r')) {
        *(p2++) = *p;
      }
      else {
        break;
      }
    }
  }
  else {
    for (char *p = buf; *p; p++) {
      if (*p != '\r') {
        *(p2++) = *p;
      }
    }
  }

  *p2 = '\0';

  free(buf); /* GHOST uses regular malloc. */

  *r_len = (p2 - newbuf);

  return newbuf;
}

char *WM_clipboard_text_get(bool selection, bool ensure_utf8, int *r_len)
{
  return wm_clipboard_text_get_ex(selection, r_len, ensure_utf8, false);
}

char *WM_clipboard_text_get_firstline(bool selection, bool ensure_utf8, int *r_len)
{
  return wm_clipboard_text_get_ex(selection, r_len, ensure_utf8, true);
}

void WM_clipboard_text_set(const char *buf, bool selection)
{
  if (!G.background) {
#ifdef _WIN32
    /* Do conversion from `\n` to `\r\n` on Windows. */
    const char *p;
    char *p2, *newbuf;
    int newlen = 0;

    for (p = buf; *p; p++) {
      if (*p == '\n') {
        newlen += 2;
      }
      else {
        newlen++;
      }
    }

    newbuf = MEM_calloc_arrayN<char>(newlen + 1, "WM_clipboard_text_set");

    for (p = buf, p2 = newbuf; *p; p++, p2++) {
      if (*p == '\n') {
        *(p2++) = '\r';
        *p2 = '\n';
      }
      else {
        *p2 = *p;
      }
    }
    *p2 = '\0';

    wm_clipboard_text_set_impl(newbuf, selection);
    MEM_freeN(newbuf);
#else
    wm_clipboard_text_set_impl(buf, selection);
#endif
  }
}

bool WM_clipboard_image_available()
{
  if (G.background) {
    return false;
  }
  return bool(GHOST_hasClipboardImage());
}

ImBuf *WM_clipboard_image_get()
{
  if (G.background) {
    return nullptr;
  }

  int width, height;

  uint8_t *rgba = (uint8_t *)GHOST_getClipboardImage(&width, &height);
  if (!rgba) {
    return nullptr;
  }

  ImBuf *ibuf = IMB_allocFromBuffer(rgba, nullptr, width, height, 4);
  free(rgba);

  return ibuf;
}

bool WM_clipboard_image_set_byte_buffer(ImBuf *ibuf)
{
  if (G.background) {
    return false;
  }
  if (ibuf->byte_buffer.data == nullptr) {
    return false;
  }

  bool success = bool(GHOST_putClipboardImage((uint *)ibuf->byte_buffer.data, ibuf->x, ibuf->y));

  return success;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Progress Bar
 * \{ */

void WM_progress_set(wmWindow *win, float progress)
{
  /* In background mode we may have windows, but not actual GHOST windows. */
  if (win->ghostwin) {
    GHOST_SetProgressBar(static_cast<GHOST_WindowHandle>(win->ghostwin), progress);
  }
}

void WM_progress_clear(wmWindow *win)
{
  if (win->ghostwin) {
    GHOST_EndProgressBar(static_cast<GHOST_WindowHandle>(win->ghostwin));
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Position/Size (internal)
 * \{ */

void wm_window_set_size(wmWindow *win, int width, int height)
{
  GHOST_SetClientSize(static_cast<GHOST_WindowHandle>(win->ghostwin), width, height);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Depth (Raise/Lower)
 * \{ */

void wm_window_lower(wmWindow *win)
{
  GHOST_SetWindowOrder(static_cast<GHOST_WindowHandle>(win->ghostwin), GHOST_kWindowOrderBottom);
}

void wm_window_raise(wmWindow *win)
{
  /* Restore window if minimized. */
  if (GHOST_GetWindowState(static_cast<GHOST_WindowHandle>(win->ghostwin)) ==
      GHOST_kWindowStateMinimized)
  {
    GHOST_SetWindowState(static_cast<GHOST_WindowHandle>(win->ghostwin), GHOST_kWindowStateNormal);
  }
  GHOST_SetWindowOrder(static_cast<GHOST_WindowHandle>(win->ghostwin), GHOST_kWindowOrderTop);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Buffers
 * \{ */

void wm_window_swap_buffer_acquire(wmWindow *win)
{
  GHOST_SwapWindowBufferAcquire(static_cast<GHOST_WindowHandle>(win->ghostwin));
}

void wm_window_swap_buffer_release(wmWindow *win)
{
  GHOST_SwapWindowBufferRelease(static_cast<GHOST_WindowHandle>(win->ghostwin));
}

void wm_window_set_swap_interval(wmWindow *win, int interval)
{
  GHOST_SetSwapInterval(static_cast<GHOST_WindowHandle>(win->ghostwin), interval);
}

bool wm_window_get_swap_interval(wmWindow *win, int *r_interval)
{
  return GHOST_GetSwapInterval(static_cast<GHOST_WindowHandle>(win->ghostwin), r_interval);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Window Utility
 * \{ */

wmWindow *WM_window_find_under_cursor(wmWindow *win,
                                      const int event_xy[2],
                                      int r_event_xy_other[2])
{
  if ((WM_capabilities_flag() & WM_CAPABILITY_WINDOW_POSITION) == 0) {
    /* Window positions are unsupported, so this function can't work as intended.
     * Perform the bare minimum, return the active window if the event is within it. */
    rcti rect;
    WM_window_rect_calc(win, &rect);
    if (!BLI_rcti_isect_pt_v(&rect, event_xy)) {
      return nullptr;
    }
    copy_v2_v2_int(r_event_xy_other, event_xy);
    return win;
  }

  int temp_xy[2];
  copy_v2_v2_int(temp_xy, event_xy);
  wm_cursor_position_to_ghost_screen_coords(win, &temp_xy[0], &temp_xy[1]);

  GHOST_WindowHandle ghostwin = GHOST_GetWindowUnderCursor(g_system, temp_xy[0], temp_xy[1]);

  if (!ghostwin) {
    return nullptr;
  }

  wmWindow *win_other = static_cast<wmWindow *>(GHOST_GetWindowUserData(ghostwin));
  wm_cursor_position_from_ghost_screen_coords(win_other, &temp_xy[0], &temp_xy[1]);
  copy_v2_v2_int(r_event_xy_other, temp_xy);
  return win_other;
}

wmWindow *WM_window_find_by_area(wmWindowManager *wm, const ScrArea *area)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *sc = WM_window_get_active_screen(win);
    if (BLI_findindex(&sc->areabase, area) != -1) {
      return win;
    }
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Initial Window State API
 * \{ */

void WM_init_state_size_set(int stax, int stay, int sizx, int sizy)
{
  wm_init_state.start = blender::int2(stax, stay); /* Left hand bottom position. */
  wm_init_state.size = blender::int2(std::max(sizx, 640), std::max(sizy, 480));
  wm_init_state.override_flag |= WIN_OVERRIDE_GEOM;
}

void WM_init_state_fullscreen_set()
{
  wm_init_state.windowstate = GHOST_kWindowStateFullScreen;
  wm_init_state.override_flag |= WIN_OVERRIDE_WINSTATE;
}

void WM_init_state_normal_set()
{
  wm_init_state.windowstate = GHOST_kWindowStateNormal;
  wm_init_state.override_flag |= WIN_OVERRIDE_WINSTATE;
}

void WM_init_state_maximized_set()
{
  wm_init_state.windowstate = GHOST_kWindowStateMaximized;
  wm_init_state.override_flag |= WIN_OVERRIDE_WINSTATE;
}

bool WM_init_window_frame_get()
{
  return wm_init_state.window_frame;
}

void WM_init_window_frame_set(bool do_it)
{
  wm_init_state.window_frame = do_it;
}

void WM_init_window_focus_set(bool do_it)
{
  wm_init_state.window_focus = do_it;
}

void WM_init_native_pixels(bool do_it)
{
  wm_init_state.native_pixels = do_it;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor API
 * \{ */

void WM_init_input_devices()
{
  if (UNLIKELY(!g_system)) {
    return;
  }

  GHOST_SetMultitouchGestures(g_system, (U.uiflag & USER_NO_MULTITOUCH_GESTURES) == 0);

  switch (U.tablet_api) {
    case USER_TABLET_NATIVE:
      GHOST_SetTabletAPI(g_system, GHOST_kTabletWinPointer);
      break;
    case USER_TABLET_WINTAB:
      GHOST_SetTabletAPI(g_system, GHOST_kTabletWintab);
      break;
    case USER_TABLET_AUTOMATIC:
    default:
      GHOST_SetTabletAPI(g_system, GHOST_kTabletAutomatic);
      break;
  }
}

void WM_cursor_warp(wmWindow *win, int x, int y)
{
  /* This function requires access to the GHOST_SystemHandle (`g_system`). */

  if (!(win && win->ghostwin)) {
    return;
  }

  int oldx = x, oldy = y;

  wm_cursor_position_to_ghost_client_coords(win, &x, &y);
  GHOST_SetCursorPosition(g_system, static_cast<GHOST_WindowHandle>(win->ghostwin), x, y);

  win->eventstate->prev_xy[0] = oldx;
  win->eventstate->prev_xy[1] = oldy;

  win->eventstate->xy[0] = oldx;
  win->eventstate->xy[1] = oldy;
}

uint WM_cursor_preferred_logical_size()
{
  return GHOST_GetCursorPreferredLogicalSize(g_system);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Size (public)
 * \{ */

int WM_window_native_pixel_x(const wmWindow *win)
{
  const float fac = GHOST_GetNativePixelSize(static_cast<GHOST_WindowHandle>(win->ghostwin));

  return int(fac * float(win->sizex));
}
int WM_window_native_pixel_y(const wmWindow *win)
{
  const float fac = GHOST_GetNativePixelSize(static_cast<GHOST_WindowHandle>(win->ghostwin));

  return int(fac * float(win->sizey));
}

blender::int2 WM_window_native_pixel_size(const wmWindow *win)
{
  const float fac = GHOST_GetNativePixelSize(static_cast<GHOST_WindowHandle>(win->ghostwin));

  return blender::int2(int(fac * float(win->sizex)), int(fac * float(win->sizey)));
}

void WM_window_native_pixel_coords(const wmWindow *win, int *x, int *y)
{
  const float fac = GHOST_GetNativePixelSize(static_cast<GHOST_WindowHandle>(win->ghostwin));

  *x *= fac;
  *y *= fac;
}

void WM_window_rect_calc(const wmWindow *win, rcti *r_rect)
{
  const blender::int2 win_size = WM_window_native_pixel_size(win);
  BLI_rcti_init(r_rect, 0, win_size[0], 0, win_size[1]);
}
void WM_window_screen_rect_calc(const wmWindow *win, rcti *r_rect)
{
  rcti window_rect, screen_rect;

  WM_window_rect_calc(win, &window_rect);
  screen_rect = window_rect;

  /* Subtract global areas from screen rectangle. */
  LISTBASE_FOREACH (ScrArea *, global_area, &win->global_areas.areabase) {
    int height = ED_area_global_size_y(global_area) - 1;

    if (global_area->global->flag & GLOBAL_AREA_IS_HIDDEN) {
      continue;
    }

    switch (global_area->global->align) {
      case GLOBAL_AREA_ALIGN_TOP:
        screen_rect.ymax -= height;
        break;
      case GLOBAL_AREA_ALIGN_BOTTOM:
        screen_rect.ymin += height;
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  }

  BLI_assert(BLI_rcti_is_valid(&screen_rect));

  *r_rect = screen_rect;
}

bool WM_window_is_fullscreen(const wmWindow *win)
{
  return win->windowstate == GHOST_kWindowStateFullScreen;
}

bool WM_window_is_maximized(const wmWindow *win)
{
  return win->windowstate == GHOST_kWindowStateMaximized;
}

bool WM_window_is_main_top_level(const wmWindow *win)
{
  /**
   * Return whether the window is a main/top-level window. In which case it is expected to contain
   * global areas (top-bar/status-bar).
   */
  const bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);
  if ((win->parent != nullptr) || screen->temp) {
    return false;
  }
  return true;
}

bool WM_window_support_hdr_color(const wmWindow *win)
{
  return GPU_hdr_support() && win->ghostwin &&
         GHOST_WindowGetHDRInfo(static_cast<GHOST_WindowHandle>(win->ghostwin)).hdr_enabled;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Screen/Scene/WorkSpaceViewLayer API
 * \{ */

void WM_windows_scene_data_sync(const ListBase *win_lb, Scene *scene)
{
  LISTBASE_FOREACH (wmWindow *, win, win_lb) {
    if (WM_window_get_active_scene(win) == scene) {
      ED_workspace_scene_data_sync(win->workspace_hook, scene);
    }
  }
}

Scene *WM_windows_scene_get_from_screen(const wmWindowManager *wm, const bScreen *screen)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (WM_window_get_active_screen(win) == screen) {
      return WM_window_get_active_scene(win);
    }
  }

  return nullptr;
}

ViewLayer *WM_windows_view_layer_get_from_screen(const wmWindowManager *wm, const bScreen *screen)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (WM_window_get_active_screen(win) == screen) {
      return WM_window_get_active_view_layer(win);
    }
  }

  return nullptr;
}

WorkSpace *WM_windows_workspace_get_from_screen(const wmWindowManager *wm, const bScreen *screen)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (WM_window_get_active_screen(win) == screen) {
      return WM_window_get_active_workspace(win);
    }
  }
  return nullptr;
}

Scene *WM_window_get_active_scene(const wmWindow *win)
{
  return win->scene;
}

void WM_window_set_active_scene(Main *bmain, bContext *C, wmWindow *win, Scene *scene)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win_parent = (win->parent) ? win->parent : win;
  bool changed = false;

  /* Set scene in parent and its child windows. */
  if (win_parent->scene != scene) {
    ED_screen_scene_change(C, win_parent, scene, true);
    changed = true;
  }

  LISTBASE_FOREACH (wmWindow *, win_child, &wm->windows) {
    if (win_child->parent == win_parent && win_child->scene != scene) {
      ED_screen_scene_change(C, win_child, scene, true);
      changed = true;
    }
  }

  if (changed) {
    /* Update depsgraph and renderers for scene change. */
    ViewLayer *view_layer = WM_window_get_active_view_layer(win_parent);
    ED_scene_change_update(bmain, scene, view_layer);

    /* Complete redraw. */
    WM_event_add_notifier(C, NC_WINDOW, nullptr);
  }
}

ViewLayer *WM_window_get_active_view_layer(const wmWindow *win)
{
  Scene *scene = WM_window_get_active_scene(win);
  if (scene == nullptr) {
    return nullptr;
  }

  ViewLayer *view_layer = BKE_view_layer_find(scene, win->view_layer_name);
  if (view_layer) {
    return view_layer;
  }

  view_layer = BKE_view_layer_default_view(scene);
  if (view_layer) {
    WM_window_set_active_view_layer((wmWindow *)win, view_layer);
  }

  return view_layer;
}

void WM_window_set_active_view_layer(wmWindow *win, ViewLayer *view_layer)
{
  BLI_assert(BKE_view_layer_find(WM_window_get_active_scene(win), view_layer->name) != nullptr);
  Main *bmain = G_MAIN;

  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  wmWindow *win_parent = (win->parent) ? win->parent : win;

  /* Set view layer in parent and child windows. */
  LISTBASE_FOREACH (wmWindow *, win_iter, &wm->windows) {
    if ((win_iter == win_parent) || (win_iter->parent == win_parent)) {
      STRNCPY_UTF8(win_iter->view_layer_name, view_layer->name);
      bScreen *screen = BKE_workspace_active_screen_get(win_iter->workspace_hook);
      ED_render_view_layer_changed(bmain, screen);
    }
  }
}

void WM_window_ensure_active_view_layer(wmWindow *win)
{
  /* Update layer name is correct after scene changes, load without UI, etc. */
  Scene *scene = WM_window_get_active_scene(win);

  if (scene && BKE_view_layer_find(scene, win->view_layer_name) == nullptr) {
    ViewLayer *view_layer = BKE_view_layer_default_view(scene);
    STRNCPY_UTF8(win->view_layer_name, view_layer->name);
  }
}

WorkSpace *WM_window_get_active_workspace(const wmWindow *win)
{
  return BKE_workspace_active_get(win->workspace_hook);
}

void WM_window_set_active_workspace(bContext *C, wmWindow *win, WorkSpace *workspace)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win_parent = (win->parent) ? win->parent : win;

  ED_workspace_change(workspace, C, wm, win);

  LISTBASE_FOREACH (wmWindow *, win_child, &wm->windows) {
    if (win_child->parent == win_parent) {
      bScreen *screen = WM_window_get_active_screen(win_child);
      /* Don't change temporary screens, they only serve a single purpose. */
      if (screen->temp) {
        continue;
      }
      ED_workspace_change(workspace, C, wm, win_child);
    }
  }
}

WorkSpaceLayout *WM_window_get_active_layout(const wmWindow *win)
{
  const WorkSpace *workspace = WM_window_get_active_workspace(win);
  return (LIKELY(workspace != nullptr) ? BKE_workspace_active_layout_get(win->workspace_hook) :
                                         nullptr);
}
void WM_window_set_active_layout(wmWindow *win, WorkSpace *workspace, WorkSpaceLayout *layout)
{
  BKE_workspace_active_layout_set(win->workspace_hook, win->winid, workspace, layout);
}

bScreen *WM_window_get_active_screen(const wmWindow *win)
{
  const WorkSpace *workspace = WM_window_get_active_workspace(win);
  /* May be null in rare cases like closing Blender. */
  return (LIKELY(workspace != nullptr) ? BKE_workspace_active_screen_get(win->workspace_hook) :
                                         nullptr);
}
void WM_window_set_active_screen(wmWindow *win, WorkSpace *workspace, bScreen *screen)
{
  BKE_workspace_active_screen_set(win->workspace_hook, win->winid, workspace, screen);
}

bool WM_window_is_temp_screen(const wmWindow *win)
{
  const bScreen *screen = WM_window_get_active_screen(win);
  return (screen && screen->temp != 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window IME API
 * \{ */

#ifdef WITH_INPUT_IME
void wm_window_IME_begin(wmWindow *win, int x, int y, int w, int h, bool complete)
{
  /* NOTE: Keep in mind #wm_window_IME_begin is also used to reposition the IME window. */

  BLI_assert(win);
  if ((WM_capabilities_flag() & WM_CAPABILITY_INPUT_IME) == 0) {
    return;
  }

  /* Convert to native OS window coordinates. */
  float fac = GHOST_GetNativePixelSize(static_cast<GHOST_WindowHandle>(win->ghostwin));
  x /= fac;
  y /= fac;
  GHOST_BeginIME(
      static_cast<GHOST_WindowHandle>(win->ghostwin), x, win->sizey - y, w, h, complete);
}

void wm_window_IME_end(wmWindow *win)
{
  if ((WM_capabilities_flag() & WM_CAPABILITY_INPUT_IME) == 0) {
    return;
  }

  BLI_assert(win);
  /* NOTE(@ideasman42): on WAYLAND and Windows a call to "begin" must be closed by an "end" call.
   * Even if no IME events were generated (which assigned `ime_data`).
   * TODO: check if #GHOST_EndIME can run on APPLE without causing problems. */
#  ifdef __APPLE__
  BLI_assert(win->runtime->ime_data);
#  endif
  GHOST_EndIME(static_cast<GHOST_WindowHandle>(win->ghostwin));
  MEM_delete(win->runtime->ime_data);
  win->runtime->ime_data = nullptr;
  win->runtime->ime_data_is_composing = false;
}
#endif /* WITH_INPUT_IME */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Direct GPU Context Management
 * \{ */

void *WM_system_gpu_context_create()
{
  /* On Windows there is a problem creating contexts that share resources (almost any object,
   * including legacy display lists, but also textures) with a context which is current in another
   * thread. This is a documented and behavior of both `::wglCreateContextAttribsARB()` and
   * `::wglShareLists()`.
   *
   * Other platforms might successfully share resources from context which is active somewhere
   * else, but to keep our code behave the same on all platform we expect contexts to only be
   * created from the main thread. */

  BLI_assert(BLI_thread_is_main());
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());

  GHOST_GPUSettings gpu_settings = {0};
  const GPUBackendType gpu_backend = GPU_backend_type_selection_get();
  gpu_settings.context_type = wm_ghost_drawing_context_type(gpu_backend);
  if (G.debug & G_DEBUG_GPU) {
    gpu_settings.flags |= GHOST_gpuDebugContext;
  }
  gpu_settings.preferred_device.index = U.gpu_preferred_index;
  gpu_settings.preferred_device.vendor_id = U.gpu_preferred_vendor_id;
  gpu_settings.preferred_device.device_id = U.gpu_preferred_device_id;
  if (GPU_backend_vsync_is_overridden()) {
    gpu_settings.flags |= GHOST_gpuVSyncIsOverridden;
    gpu_settings.vsync = GHOST_TVSyncModes(GPU_backend_vsync_get());
  }

  return GHOST_CreateGPUContext(g_system, gpu_settings);
}

void WM_system_gpu_context_dispose(void *context)
{
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());
  GHOST_DisposeGPUContext(g_system, (GHOST_ContextHandle)context);
}

void WM_system_gpu_context_activate(void *context)
{
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());
  GHOST_ActivateGPUContext((GHOST_ContextHandle)context);
}

void WM_system_gpu_context_release(void *context)
{
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());
  GHOST_ReleaseGPUContext((GHOST_ContextHandle)context);
}

void WM_ghost_show_message_box(const char *title,
                               const char *message,
                               const char *help_label,
                               const char *continue_label,
                               const char *link,
                               GHOST_DialogOptions dialog_options)
{
  BLI_assert(g_system);
  GHOST_ShowMessageBox(g_system, title, message, help_label, continue_label, link, dialog_options);
}

/** \} */
