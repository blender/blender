/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * User level access for blend file read/write, file-history and user-preferences
 * (including relevant operators).
 */

/* Placed up here because of crappy WINSOCK stuff. */
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <fcntl.h> /* For open flags (#O_BINARY, #O_RDONLY). */

#ifdef WIN32
/* Need to include windows.h so _WIN32_IE is defined. */
#  include <windows.h>
#  ifndef _WIN32_IE
/* Minimal requirements for SHGetSpecialFolderPath on MINGW MSVC has this defined already. */
#    define _WIN32_IE 0x0400
#  endif
/* For #SHGetSpecialFolderPath, has to be done before `BLI_winstuff.h`
 * because 'near' is disabled through `BLI_windstuff.h`. */
#  include "BLI_winstuff.h"
#  include <shlobj.h>
#endif

#include <fmt/format.h>

#include "MEM_CacheLimiterC-Api.h"
#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_filereader.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_time.h"
#include "BLI_memory_cache.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_system.h"
#include "BLI_threads.h"
#include "BLI_time.h"
#include "BLI_timer.h"
#include "BLI_utildefines.h"
#include BLI_SYSTEM_PID_H

#include "BLO_readfile.hh"
#include "BLT_translation.hh"

#include "BLF_api.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "AS_asset_library.hh"

#ifndef WITH_CYCLES
#  include "BKE_addon.h"
#endif
#include "BKE_appdir.hh"
#include "BKE_autoexec.hh"
#include "BKE_blender.hh"
#include "BKE_blender_version.h"
#include "BKE_blendfile.hh"
#include "BKE_callbacks.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"
#include "BKE_node.hh"
#include "BKE_packedFile.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_sound.hh"
#include "BKE_undo_system.hh"
#include "BKE_workspace.hh"

#include "BLO_writefile.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"
#include "IMB_thumbs.hh"

#include "ED_asset.hh"
#include "ED_datafiles.h"
#include "ED_fileselect.hh"
#include "ED_image.hh"
#include "ED_outliner.hh"
#include "ED_render.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"
#include "ED_util.hh"
#include "ED_view3d.hh"
#include "ED_view3d_offscreen.hh"

#include "NOD_composite.hh"

#include "GHOST_C-api.h"
#include "GHOST_Path-api.hh"

#include "GPU_context.hh"

#include "SEQ_sequencer.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

/* Only to report a missing engine. */
#include "RE_engine.h"

#ifdef WITH_PYTHON
#  include "BPY_extern_python.hh"
#  include "BPY_extern_run.hh"
#endif

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "wm.hh"
#include "wm_event_system.hh"
#include "wm_files.hh"
#include "wm_window.hh"

#include "CLG_log.h"

static RecentFile *wm_file_history_find(const char *filepath);
static void wm_history_file_free(RecentFile *recent);
static void wm_history_files_free();
static void wm_history_file_update();
static void wm_history_file_write();

static void wm_test_autorun_revert_action_exec(bContext *C);

static CLG_LogRef LOG = {"blend"};

/**
 * Fast-path for down-scaling byte buffers.
 *
 * NOTE(@ideasman42) Support alternate logic for scaling byte buffers for
 * thumbnails which doesn't use the higher quality box-filtered floating point math.
 * This may be removed if similar performance can be achieved from other scale methods,
 * especially in debug mode - which could cause file saving to be unreasonably slow
 * (taking seconds just down-scaling the thumbnail).
 */
#define USE_THUMBNAIL_FAST_DOWNSCALE

/* -------------------------------------------------------------------- */
/** \name Misc Utility Functions
 * \{ */

void WM_file_tag_modified()
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
  if (wm->file_saved) {
    wm->file_saved = 0;
    /* Notifier that data changed, for save-over warning or header. */
    WM_main_add_notifier(NC_WM | ND_DATACHANGED, nullptr);
  }
}

bool wm_file_or_session_data_has_unsaved_changes(const Main *bmain, const wmWindowManager *wm)
{
  return !wm->file_saved || ED_image_should_save_modified(bmain) ||
         AS_asset_library_has_any_unsaved_catalogs();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Matching for File Reading
 * \{ */

/**
 * To be able to read files without windows closing, opening, moving
 * we try to prepare for worst case:
 * - active window gets active screen from file
 * - restoring the screens from non-active windows
 * Best case is all screens match, in that case they get assigned to proper window.
 */

/**
 * Clear several WM/UI runtime data that would make later complex WM handling impossible.
 *
 * Return data should be cleared by #wm_file_read_setup_wm_finalize. */
static BlendFileReadWMSetupData *wm_file_read_setup_wm_init(bContext *C,
                                                            Main *bmain,
                                                            const bool is_read_homefile)
{
  using namespace blender;
  BLI_assert(BLI_listbase_count_at_most(&bmain->wm, 2) <= 1);
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  BlendFileReadWMSetupData *wm_setup_data = MEM_callocN<BlendFileReadWMSetupData>(__func__);
  wm_setup_data->is_read_homefile = is_read_homefile;
  /* This info is not always known yet when this function is called. */
  wm_setup_data->is_factory_startup = false;

  if (wm == nullptr) {
    return wm_setup_data;
  }

  /* First wrap up running stuff.
   *
   * Code copied from `wm_init_exit.cc`. */
  WM_jobs_kill_all(wm);

  wmWindow *active_win = CTX_wm_window(C);
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    CTX_wm_window_set(C, win); /* Needed by operator close callbacks. */
    WM_event_remove_handlers(C, &win->handlers);
    WM_event_remove_handlers(C, &win->modalhandlers);
    ED_screen_exit(C, win, WM_window_get_active_screen(win));
  }
  /* Reset active window. */
  CTX_wm_window_set(C, active_win);

  /* NOTE(@ideasman42): Clear the message bus so it's always cleared on file load.
   * Otherwise it's cleared when "Load UI" is set (see #USER_FILENOUI and #wm_close_and_free).
   * However it's _not_ cleared when the UI is kept. This complicates use from add-ons
   * which can re-register subscribers on file-load. To support this use case,
   * it's best to have predictable behavior - always clear. */
  if (wm->runtime->message_bus != nullptr) {
    WM_msgbus_destroy(wm->runtime->message_bus);
    wm->runtime->message_bus = nullptr;
  }

  /* XXX Hack! We have to clear context popup-region here, because removing all
   * #wmWindow::modalhandlers above frees the active menu (at least, in the 'startup splash' case),
   * causing use-after-free error in later handling of the button callbacks in UI code
   * (see #ui_apply_but_funcs_after()).
   * Tried solving this by always nullptr-ing context's menu when setting wm/win/etc.,
   * but it broke popups refreshing (see #47632),
   * so for now just handling this specific case here. */
  CTX_wm_region_popup_set(C, nullptr);

  ED_editors_exit(bmain, true);

  /* Asset loading is done by the UI/editors and they keep pointers into it. So make sure to clear
   * it after UI/editors. */
  ed::asset::list::storage_exit();
  AS_asset_libraries_exit();

  /* NOTE: `wm_setup_data->old_wm` cannot be set here, as this pointer may be swapped with the
   * newly read one in `setup_app_data` process (See #swap_wm_data_for_blendfile). */

  return wm_setup_data;
}

static void wm_file_read_setup_wm_substitute_old_window(wmWindowManager *oldwm,
                                                        wmWindowManager *wm,
                                                        wmWindow *oldwin,
                                                        wmWindow *win)
{
  win->ghostwin = oldwin->ghostwin;
  win->gpuctx = oldwin->gpuctx;
  win->active = oldwin->active;
  if (win->active) {
    wm->runtime->winactive = win;
  }
  if (oldwm->runtime->windrawable == oldwin) {
    oldwm->runtime->windrawable = nullptr;
    wm->runtime->windrawable = win;
  }

  /* File loading in background mode still calls this. */
  if (!G.background) {
    /* Pointer back. */
    GHOST_SetWindowUserData(static_cast<GHOST_WindowHandle>(win->ghostwin), win);
  }

  oldwin->ghostwin = nullptr;
  oldwin->gpuctx = nullptr;

  win->eventstate = oldwin->eventstate;
  win->event_last_handled = oldwin->event_last_handled;
  oldwin->eventstate = nullptr;
  oldwin->event_last_handled = nullptr;

  /* Ensure proper screen re-scaling. */
  win->sizex = oldwin->sizex;
  win->sizey = oldwin->sizey;
  win->posx = oldwin->posx;
  win->posy = oldwin->posy;
}

/**
 * Support loading older files without multiple windows (pre 2.5),
 * in this case the #bScreen from the users file should be used but the current
 * windows (from `current_wm_list` are kept).
 *
 * As the original file did not have multiple windows, duplicate the layout into each window.
 * An alternative solution could also be to close all windows except the first however this is
 * enough of a corner case that it's the current behavior is acceptable.
 */
static void wm_file_read_setup_wm_keep_old(const bContext *C,
                                           Main *bmain,
                                           BlendFileReadWMSetupData *wm_setup_data,
                                           wmWindowManager *wm,
                                           const bool load_ui)
{
  /* This data is not needed here, besides detecting that old WM has been kept (in caller code).
   * Since `old_wm` is kept, do not free it, just clear the pointer as clean-up. */
  wm_setup_data->old_wm = nullptr;

  if (!load_ui) {
    /* When loading without UI (i.e. keeping existing UI), no matching needed.
     *
     * The other UI data (workspaces, layouts, screens) has also been re-used from old Main, and
     * newly read one from file has already been discarded in #setup_app_data. */
    return;
  }

  /* Old WM is being reused, but other UI data (workspaces, layouts, screens) comes from the new
   * file, so the WM needs to be updated to use these. */
  bScreen *screen = CTX_wm_screen(C);
  if (screen != nullptr) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      WorkSpace *workspace;

      WorkSpaceLayout *layout_ref = BKE_workspace_layout_find_global(bmain, screen, &workspace);
      BKE_workspace_active_set(win->workspace_hook, workspace);
      win->scene = CTX_data_scene(C);

      /* All windows get active screen from file. */
      if (screen->winid == 0) {
        WM_window_set_active_screen(win, workspace, screen);
      }
      else {
#if 0
        /* NOTE(@ideasman42): The screen referenced from the window has been freed,
         * see: #107525. */
        WorkSpaceLayout *layout_ref = WM_window_get_active_layout(win);
#endif
        WorkSpaceLayout *layout_new = ED_workspace_layout_duplicate(
            bmain, workspace, layout_ref, win);

        WM_window_set_active_layout(win, workspace, layout_new);
      }

      bScreen *win_screen = WM_window_get_active_screen(win);
      win_screen->winid = win->winid;
    }
  }
}

static void wm_file_read_setup_wm_use_new(bContext *C,
                                          Main * /*bmain*/,
                                          BlendFileReadWMSetupData *wm_setup_data,
                                          wmWindowManager *wm)
{
  wmWindowManager *old_wm = wm_setup_data->old_wm;

  wm->op_undo_depth = old_wm->op_undo_depth;

  /* Move existing key configurations into the new WM. */
  wm->runtime->keyconfigs = old_wm->runtime->keyconfigs;
  wm->runtime->addonconf = old_wm->runtime->addonconf;
  wm->runtime->defaultconf = old_wm->runtime->defaultconf;
  wm->runtime->userconf = old_wm->runtime->userconf;

  BLI_listbase_clear(&old_wm->runtime->keyconfigs);
  old_wm->runtime->addonconf = nullptr;
  old_wm->runtime->defaultconf = nullptr;
  old_wm->runtime->userconf = nullptr;

  /* Ensure new keymaps are made, and space types are set. */
  wm->init_flag = 0;
  wm->runtime->winactive = nullptr;

  /* Clearing drawable of old WM before deleting any context to avoid clearing the wrong wm. */
  wm_window_clear_drawable(old_wm);

  bool has_match = false;
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    LISTBASE_FOREACH (wmWindow *, old_win, &old_wm->windows) {
      if (old_win->winid == win->winid) {
        has_match = true;

        wm_file_read_setup_wm_substitute_old_window(old_wm, wm, old_win, win);
      }
    }
  }
  /* Ensure that at least one window is kept open so we don't lose the context, see #42303. */
  if (!has_match) {
    wm_file_read_setup_wm_substitute_old_window(old_wm,
                                                wm,
                                                static_cast<wmWindow *>(old_wm->windows.first),
                                                static_cast<wmWindow *>(wm->windows.first));
  }

  wm_setup_data->old_wm = nullptr;
  wm_close_and_free(C, old_wm);
  /* Don't handle user counts as this is only ever called once #G_MAIN has already been freed via
   * #BKE_main_free so any access to ID's referenced by the window-manager (from ID properties)
   * will crash. See: #100703. */
  BKE_libblock_free_data(&old_wm->id, false);
  BKE_libblock_free_data_py(&old_wm->id);
  MEM_freeN(old_wm);
}

/**
 * Finalize setting up the WM for the newly read file, transferring GHOST windows from the old WM
 * if needed, updating other UI data, etc. And free the old WM if any.
 *
 * Counterpart of #wm_file_read_setup_wm_init.
 */
static void wm_file_read_setup_wm_finalize(bContext *C,
                                           Main *bmain,
                                           BlendFileReadWMSetupData *wm_setup_data)
{
  BLI_assert(BLI_listbase_count_at_most(&bmain->wm, 2) <= 1);
  BLI_assert(wm_setup_data != nullptr);
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);

  /* If reading factory startup file, and there was no previous WM, clear the size of the windows
   * in newly read WM so that they get resized to occupy the whole available space on current
   * monitor.
   */
  if (wm_setup_data->is_read_homefile && wm_setup_data->is_factory_startup &&
      wm_setup_data->old_wm == nullptr)
  {
    wm_clear_default_size(C);
  }

  if (wm == nullptr) {
    /* Add a default WM in case none exists in newly read main (should only happen when opening
     * an old pre-2.5 .blend file at startup). */
    wm_add_default(bmain, C);
  }
  else if (wm_setup_data->old_wm != nullptr) {
    if (wm_setup_data->old_wm == wm) {
      /* Old WM was kept, update it with new workspaces/layouts/screens read from file.
       *
       * Happens when not loading UI, or when the newly read file has no WM (pre-2.5 files). */
      wm_file_read_setup_wm_keep_old(
          C, bmain, wm_setup_data, wm, (G.fileflags & G_FILE_NO_UI) == 0);
    }
    else {
      /* Using new WM from read file, try to keep current GHOST windows, transfer keymaps, etc.,
       * from old WM.
       *
       * Also takes care of clearing old WM data (temporarily stored in `wm_setup_data->old_wm`).
       */
      wm_file_read_setup_wm_use_new(C, bmain, wm_setup_data, wm);
    }
  }
  /* Else just using the new WM read from file, nothing to do. */
  BLI_assert(wm_setup_data->old_wm == nullptr);
  MEM_delete(wm_setup_data);

  /* UI Updates. */
  /* Flag local View3D's to check and exit if they are empty. */
  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype == SPACE_VIEW3D) {
          View3D *v3d = reinterpret_cast<View3D *>(sl);
          if (v3d->localvd) {
            v3d->localvd->runtime.flag |= V3D_RUNTIME_LOCAL_MAYBE_EMPTY;
          }
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preferences Initialization & Versioning
 * \{ */

static void wm_gpu_backend_override_from_userdef()
{
  /* Check if GPU backend is already set from the command line arguments. The command line
   * arguments have higher priority than user preferences. */
  if (GPU_backend_type_selection_is_overridden()) {
    return;
  }

  GPU_backend_type_selection_set_override(GPUBackendType(U.gpu_backend));
}

/**
 * In case #UserDef was read, re-initialize values that depend on it.
 */
static void wm_init_userdef(Main *bmain)
{
  /* Not versioning, just avoid errors. */
#ifndef WITH_CYCLES
  BKE_addon_remove_safe(&U.addons, "cycles");
#endif

  UI_init_userdef();

  /* Needed so loading a file from the command line respects user-pref #26156. */
  SET_FLAG_FROM_TEST(G.fileflags, U.flag & USER_FILENOUI, G_FILE_NO_UI);

  /* Set the python auto-execute setting from user prefs. */
  /* Enabled by default, unless explicitly enabled in the command line which overrides. */
  if ((G.f & G_FLAG_SCRIPT_OVERRIDE_PREF) == 0) {
    SET_FLAG_FROM_TEST(G.f, (U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0, G_FLAG_SCRIPT_AUTOEXEC);
  }

  /* Only reset "offline mode" if they weren't passes via command line arguments. */
  if ((G.f & G_FLAG_INTERNET_OVERRIDE_PREF_ANY) == 0) {
    SET_FLAG_FROM_TEST(G.f, U.flag & USER_INTERNET_ALLOW, G_FLAG_INTERNET_ALLOW);
  }

  const int64_t cache_limit = int64_t(U.memcachelimit) * 1024 * 1024;
  MEM_CacheLimiter_set_maximum(cache_limit);
  blender::memory_cache::set_approximate_size_limit(cache_limit);

  BKE_sound_init(bmain);

  /* Update the temporary directory from the preferences or fall back to the system default. */
  BKE_tempdir_init(U.tempdir);

  /* Update input device preference. */
  WM_init_input_devices();

  BLO_sanitize_experimental_features_userpref_blend(&U);

  wm_gpu_backend_override_from_userdef();
  GPU_backend_type_selection_detect();
}

/* Return codes. */
#define BKE_READ_EXOTIC_FAIL_PATH -3   /* File format is not supported. */
#define BKE_READ_EXOTIC_FAIL_FORMAT -2 /* File format is not supported. */
#define BKE_READ_EXOTIC_FAIL_OPEN -1   /* Can't open the file. */
#define BKE_READ_EXOTIC_OK_BLEND 0     /* `.blend` file. */
#if 0
#  define BKE_READ_EXOTIC_OK_OTHER 1 /* Other supported formats. */
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Exotic File Formats
 *
 * Currently only supports `.blend` files,
 * we could support registering other file formats and their loaders.
 * \{ */

/* Intended to check for non-blender formats but for now it only reads blends. */
static int wm_read_exotic(const char *filepath)
{
  /* Make sure we're not trying to read a directory. */

  int filepath_len = strlen(filepath);
  if (filepath_len > 0 && ELEM(filepath[filepath_len - 1], '/', '\\')) {
    return BKE_READ_EXOTIC_FAIL_PATH;
  }

  /* Open the file. */
  const int filedes = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (filedes == -1) {
    return BKE_READ_EXOTIC_FAIL_OPEN;
  }

  FileReader *rawfile = BLI_filereader_new_file(filedes);
  if (rawfile == nullptr) {
    return BKE_READ_EXOTIC_FAIL_OPEN;
  }

  /* Read the header (7 bytes are enough to identify all known types). */
  char header[7];
  if (rawfile->read(rawfile, header, sizeof(header)) != sizeof(header)) {
    rawfile->close(rawfile);
    return BKE_READ_EXOTIC_FAIL_FORMAT;
  }
  rawfile->seek(rawfile, 0, SEEK_SET);

  /* Check for uncompressed `.blend`. */
  if (STREQLEN(header, "BLENDER", 7)) {
    rawfile->close(rawfile);
    return BKE_READ_EXOTIC_OK_BLEND;
  }

  /* Check for compressed `.blend`. */
  FileReader *compressed_file = nullptr;
  if (BLI_file_magic_is_gzip(header)) {
    /* In earlier versions of Blender (before 3.0), compressed files used `Gzip` instead of `Zstd`.
     * While these files will no longer be written, there still needs to be reading support. */
    compressed_file = BLI_filereader_new_gzip(rawfile);
  }
  else if (BLI_file_magic_is_zstd(header)) {
    compressed_file = BLI_filereader_new_zstd(rawfile);
  }

  /* If a compression signature matches,
   * try decompressing the start and check if it's a `.blend`. */
  if (compressed_file != nullptr) {
    size_t len = compressed_file->read(compressed_file, header, sizeof(header));
    compressed_file->close(compressed_file);
    if (len == sizeof(header) && STREQLEN(header, "BLENDER", 7)) {
      return BKE_READ_EXOTIC_OK_BLEND;
    }
  }
  else {
    rawfile->close(rawfile);
  }

  /* Add check for future file formats here. */

  return BKE_READ_EXOTIC_FAIL_FORMAT;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Blend-File Shared Utilities
 * \{ */

void WM_file_autoexec_init(const char *filepath)
{
  if (G.f & G_FLAG_SCRIPT_OVERRIDE_PREF) {
    return;
  }

  if (G.f & G_FLAG_SCRIPT_AUTOEXEC) {
    char dirpath[FILE_MAX];
    BLI_path_split_dir_part(filepath, dirpath, sizeof(dirpath));
    if (BKE_autoexec_match(dirpath)) {
      G.f &= ~G_FLAG_SCRIPT_AUTOEXEC;
    }
  }
}

void wm_file_read_report(Main *bmain, wmWindow *win)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  ReportList *reports = &wm->runtime->reports;
  bool found = false;
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (scene->r.engine[0] &&
        BLI_findstring(&R_engines, scene->r.engine, offsetof(RenderEngineType, idname)) == nullptr)
    {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Engine '%s' not available for scene '%s' (an add-on may need to be installed "
                  "or enabled)",
                  scene->r.engine,
                  scene->id.name + 2);
      found = true;
    }
  }

  if (found) {
    if (!G.background) {
      WM_report_banner_show(wm, win);
    }
  }
}

/**
 * Logic shared between #WM_file_read & #wm_homefile_read,
 * call before loading a file.
 * \note In the case of #WM_file_read the file may fail to load.
 * Change here shouldn't cause user-visible changes in that case.
 */
static void wm_file_read_pre(bool use_data, bool /*use_userdef*/)
{
  if (use_data) {
    BLI_timer_on_file_load();
  }

  /* Always do this as both startup and preferences may have loaded in many font's
   * at a different zoom level to the file being loaded. */
  UI_view2d_zoom_cache_reset();

  ED_preview_restart_queue_free();
}

/**
 * Parameters for #wm_file_read_post, also used for deferred initialization.
 */
struct wmFileReadPost_Params {
  uint use_data : 1;
  uint use_userdef : 1;

  uint is_startup_file : 1;
  uint is_factory_startup : 1;
  uint reset_app_template : 1;

  /* Used by #wm_homefile_read_post. */
  uint success : 1;
  uint is_alloc : 1;
  uint is_first_time : 1;
};

/**
 * Logic shared between #WM_file_read & #wm_homefile_read,
 * updates to make after reading a file.
 */
static void wm_file_read_post(bContext *C,
                              const char *filepath,
                              const wmFileReadPost_Params *params)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  const bool use_data = params->use_data;
  const bool use_userdef = params->use_userdef;
  const bool is_startup_file = params->is_startup_file;
  const bool is_factory_startup = params->is_factory_startup;
  const bool reset_app_template = params->reset_app_template;

  bool addons_loaded = false;

  if (use_data) {
    if (!G.background) {
      /* Remove windows which failed to be added via #WM_check. */
      wm_window_ghostwindows_remove_invalid(C, wm);
    }
    CTX_wm_window_set(C, static_cast<wmWindow *>(wm->windows.first));
  }

#ifdef WITH_PYTHON
  if (is_startup_file) {
    /* The following block handles data & preferences being reloaded
     * which requires resetting some internal variables. */
    if (!params->is_first_time) {
      BLI_assert(CTX_py_init_get(C));
      bool reset_all = use_userdef;
      if (use_userdef || reset_app_template) {
        /* Only run when we have a template path found. */
        if (BKE_appdir_app_template_any()) {
          const char *imports[] = {"bl_app_template_utils", nullptr};
          BPY_run_string_eval(C, imports, "bl_app_template_utils.reset()");
          reset_all = true;
        }
      }
      if (reset_all) {
        const char *imports[] = {"bpy", "addon_utils", nullptr};
        BPY_run_string_exec(
            C,
            imports,
            /* Refresh scripts as the preferences may have changed the user-scripts path.
             *
             * This is needed when loading settings from the previous version,
             * otherwise the script path stored in the preferences would be ignored. */
            "bpy.utils.refresh_script_paths()\n"
            /* Sync add-ons, these may have changed from the defaults. */
            "addon_utils.reset_all()");
      }
      if (use_data) {
        BPY_python_reset(C);
      }
      addons_loaded = true;
    }
  }
  else {
    /* Run any texts that were loaded in and flagged as modules. */
    if (use_data) {
      BPY_python_reset(C);
    }
    addons_loaded = true;
  }
#else
  UNUSED_VARS(is_startup_file, reset_app_template);
#endif /* WITH_PYTHON */

  Main *bmain = CTX_data_main(C);

  if (use_userdef) {
    if (is_factory_startup) {
      BKE_callback_exec_null(bmain, BKE_CB_EVT_LOAD_FACTORY_USERDEF_POST);
    }
  }

  if (is_factory_startup && BLT_translate_new_dataname()) {
    /* Translate workspace names. */
    LISTBASE_FOREACH_MUTABLE (WorkSpace *, workspace, &bmain->workspaces) {
      BKE_libblock_rename(
          *bmain, workspace->id, CTX_DATA_(BLT_I18NCONTEXT_ID_WORKSPACE, workspace->id.name + 2));
    }
  }

  if (use_data) {
    /* Important to do before nulling the context. */
    BKE_callback_exec_null(bmain, BKE_CB_EVT_VERSION_UPDATE);

    /* Load-post must run before evaluating drivers & depsgraph, see: #109720.
     * On failure, the caller handles #BKE_CB_EVT_LOAD_POST_FAIL. */
    if (params->success) {
      BKE_callback_exec_string(bmain, BKE_CB_EVT_LOAD_POST, filepath);
    }

    if (is_factory_startup) {
      BKE_callback_exec_null(bmain, BKE_CB_EVT_LOAD_FACTORY_STARTUP_POST);
    }
  }

  if (use_data) {
    WM_operatortype_last_properties_clear_all();

    /* After load post, so for example the driver namespace can be filled
     * before evaluating the depsgraph. */
    if (!G.background || (G.fileflags & G_BACKGROUND_NO_DEPSGRAPH) == 0) {
      wm_event_do_depsgraph(C, true);
    }

    ED_editors_init(C);

    /* Add-ons are disabled when loading the startup file, so the Render Layer node in compositor
     * node trees might be wrong due to missing render engines that are available as add-ons, like
     * Cycles. So we need to update compositor node trees after reading the file when add-ons are
     * now loaded. */
    if (is_startup_file) {
      FOREACH_NODETREE_BEGIN (bmain, node_tree, owner_id) {
        if (node_tree->type == NTREE_COMPOSIT) {
          ntreeCompositUpdateRLayers(node_tree);
        }
      }
      FOREACH_NODETREE_END;
    }

#if 1
    WM_event_add_notifier(C, NC_WM | ND_FILEREAD, nullptr);
    /* Clear static filtered asset tree caches. */
    WM_event_add_notifier(C, NC_ASSET | ND_ASSET_LIST_READING, nullptr);
#else
    WM_msg_publish_static(CTX_wm_message_bus(C), WM_MSG_STATICTYPE_FILE_READ);
#endif
  }

  /* Report any errors.
   * Currently disabled if add-ons aren't yet loaded. */
  if (addons_loaded) {
    wm_file_read_report(bmain, static_cast<wmWindow *>(wm->windows.first));
  }

  if (use_data) {
    if (!G.background) {
      if (wm->runtime->undo_stack == nullptr) {
        wm->runtime->undo_stack = BKE_undosys_stack_create();
      }
      else {
        BKE_undosys_stack_clear(wm->runtime->undo_stack);
      }
      BKE_undosys_stack_init_from_main(wm->runtime->undo_stack, bmain);
      BKE_undosys_stack_init_from_context(wm->runtime->undo_stack, C);
    }
  }

  if (use_data) {
    if (!G.background) {
      /* In background mode this makes it hard to load
       * a blend file and do anything since the screen
       * won't be set to a valid value again. */
      CTX_wm_window_set(C, nullptr); /* Exits queues. */

      /* Ensure auto-run action is not used from a previous blend file load. */
      wm_test_autorun_revert_action_set(nullptr, nullptr);

      /* Ensure tools are registered. */
      WM_toolsystem_init(C);
    }
  }
}

static void wm_read_callback_pre_wrapper(bContext *C, const char *filepath)
{
  /* NOTE: either #BKE_CB_EVT_LOAD_POST or #BKE_CB_EVT_LOAD_POST_FAIL must run.
   * Runs at the end of this function, don't return beforehand. */
  BKE_callback_exec_string(CTX_data_main(C), BKE_CB_EVT_LOAD_PRE, filepath);
}

static void wm_read_callback_post_wrapper(bContext *C, const char *filepath, const bool success)
{
  Main *bmain = CTX_data_main(C);
  /* Temporarily set the window context as this was once supported, see: #107759.
   * If the window is already set, don't change it. */
  bool has_window = CTX_wm_window(C) != nullptr;
  if (!has_window) {
    wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
    wmWindow *win = static_cast<wmWindow *>(wm->windows.first);
    CTX_wm_window_set(C, win);
  }

  /* On success: #BKE_CB_EVT_LOAD_POST runs from #wm_file_read_post. */
  if (success == false) {
    BKE_callback_exec_string(bmain, BKE_CB_EVT_LOAD_POST_FAIL, filepath);
  }

  /* This function should leave the window null when the function entered. */
  if (!has_window) {
    CTX_wm_window_set(C, nullptr);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Main Blend-File API
 * \{ */

static void file_read_reports_finalize(BlendFileReadReport *bf_reports)
{
  double duration_whole_minutes, duration_whole_seconds;
  double duration_libraries_minutes, duration_libraries_seconds;
  double duration_lib_override_minutes, duration_lib_override_seconds;
  double duration_lib_override_resync_minutes, duration_lib_override_resync_seconds;
  double duration_lib_override_recursive_resync_minutes,
      duration_lib_override_recursive_resync_seconds;

  BLI_math_time_seconds_decompose(bf_reports->duration.whole,
                                  nullptr,
                                  nullptr,
                                  &duration_whole_minutes,
                                  &duration_whole_seconds,
                                  nullptr);
  BLI_math_time_seconds_decompose(bf_reports->duration.libraries,
                                  nullptr,
                                  nullptr,
                                  &duration_libraries_minutes,
                                  &duration_libraries_seconds,
                                  nullptr);
  BLI_math_time_seconds_decompose(bf_reports->duration.lib_overrides,
                                  nullptr,
                                  nullptr,
                                  &duration_lib_override_minutes,
                                  &duration_lib_override_seconds,
                                  nullptr);
  BLI_math_time_seconds_decompose(bf_reports->duration.lib_overrides_resync,
                                  nullptr,
                                  nullptr,
                                  &duration_lib_override_resync_minutes,
                                  &duration_lib_override_resync_seconds,
                                  nullptr);
  BLI_math_time_seconds_decompose(bf_reports->duration.lib_overrides_recursive_resync,
                                  nullptr,
                                  nullptr,
                                  &duration_lib_override_recursive_resync_minutes,
                                  &duration_lib_override_recursive_resync_seconds,
                                  nullptr);

  CLOG_INFO(
      &LOG, "Blender file read in %.0fm%.2fs", duration_whole_minutes, duration_whole_seconds);
  CLOG_INFO(&LOG,
            " * Loading libraries: %.0fm%.2fs",
            duration_libraries_minutes,
            duration_libraries_seconds);
  CLOG_INFO(&LOG,
            " * Applying overrides: %.0fm%.2fs",
            duration_lib_override_minutes,
            duration_lib_override_seconds);
  CLOG_INFO(&LOG,
            " * Resyncing overrides: %.0fm%.2fs (%d root overrides), including recursive "
            "resyncs: %.0fm%.2fs)",
            duration_lib_override_resync_minutes,
            duration_lib_override_resync_seconds,
            bf_reports->count.resynced_lib_overrides,
            duration_lib_override_recursive_resync_minutes,
            duration_lib_override_recursive_resync_seconds);

  if (bf_reports->resynced_lib_overrides_libraries_count != 0) {
    for (LinkNode *node_lib = bf_reports->resynced_lib_overrides_libraries; node_lib != nullptr;
         node_lib = node_lib->next)
    {
      Library *library = static_cast<Library *>(node_lib->link);
      BKE_reportf(bf_reports->reports,
                  RPT_INFO,
                  "Library \"%s\" needs overrides resync",
                  library->filepath);
    }
  }

  if (bf_reports->count.missing_libraries != 0 || bf_reports->count.missing_linked_id != 0) {
    BKE_reportf(bf_reports->reports,
                RPT_WARNING,
                "%d libraries and %d linked data-blocks are missing (including %d ObjectData), "
                "please check the Info and Outliner editors for details",
                bf_reports->count.missing_libraries,
                bf_reports->count.missing_linked_id,
                bf_reports->count.missing_obdata);
  }
  else {
    if (bf_reports->count.missing_obdata != 0) {
      CLOG_WARN(&LOG,
                "%d local ObjectData are reported to be missing, this should never happen",
                bf_reports->count.missing_obdata);
    }
  }

  if (bf_reports->resynced_lib_overrides_libraries_count != 0) {
    BKE_reportf(bf_reports->reports,
                RPT_WARNING,
                "%d libraries have overrides needing resync (auto resynced in %.0fm%.2fs), "
                "please check the Info editor for details",
                bf_reports->resynced_lib_overrides_libraries_count,
                duration_lib_override_recursive_resync_minutes,
                duration_lib_override_recursive_resync_seconds);
  }

  if (bf_reports->count.proxies_to_lib_overrides_success != 0 ||
      bf_reports->count.proxies_to_lib_overrides_failures != 0)
  {
    BKE_reportf(bf_reports->reports,
                RPT_WARNING,
                "Proxies have been removed from Blender (%d proxies were automatically converted "
                "to library overrides, %d proxies could not be converted and were cleared). "
                "Consider re-saving any library .blend file with the newest Blender version",
                bf_reports->count.proxies_to_lib_overrides_success,
                bf_reports->count.proxies_to_lib_overrides_failures);
  }

  if (bf_reports->count.sequence_strips_skipped != 0) {
    BKE_reportf(bf_reports->reports,
                RPT_ERROR,
                "%d sequence strips were not read because they were in a channel larger than %d",
                bf_reports->count.sequence_strips_skipped,
                blender::seq::MAX_CHANNELS);
  }

  BLI_linklist_free(bf_reports->resynced_lib_overrides_libraries, nullptr);
  bf_reports->resynced_lib_overrides_libraries = nullptr;

  if (bf_reports->pre_animato_file_loaded) {
    BKE_report(
        bf_reports->reports,
        RPT_WARNING,
        "Loaded a pre-2.50 blend file, animation data has not been loaded. Open & save the file "
        "with Blender v4.5 to convert animation data.");
  }
}

bool WM_file_read(bContext *C,
                  const char *filepath,
                  const bool use_scripts_autoexec_check,
                  ReportList *reports)
{
  /* Assume automated tasks with background, don't write recent file list. */
  const bool do_history_file_update = (G.background == false) &&
                                      (CTX_wm_manager(C)->op_undo_depth == 0);
  bool success = false;

  const bool use_data = true;
  const bool use_userdef = false;

  /* NOTE: a matching #wm_read_callback_post_wrapper must be called. */
  wm_read_callback_pre_wrapper(C, filepath);

  Main *bmain = CTX_data_main(C);

  /* So we can get the error message. */
  errno = 0;

  WM_cursor_wait(true);

  /* First try to append data from exotic file formats. */
  /* It throws error box when file doesn't exist and returns -1. */
  /* NOTE(ton): it should set some error message somewhere. */
  const int retval = wm_read_exotic(filepath);

  /* We didn't succeed, now try to read Blender file. */
  if (retval == BKE_READ_EXOTIC_OK_BLEND) {
    BlendFileReadParams params{};
    params.is_startup = false;
    /* Loading preferences when the user intended to load a regular file is a security
     * risk, because the excluded path list is also loaded. Further it's just confusing
     * if a user loads a file and various preferences change. */
    params.skip_flags = BLO_READ_SKIP_USERDEF;

    BlendFileReadReport bf_reports{};
    bf_reports.reports = reports;
    bf_reports.duration.whole = BLI_time_now_seconds();
    BlendFileData *bfd = BKE_blendfile_read(filepath, &params, &bf_reports);
    if (bfd != nullptr) {
      wm_file_read_pre(use_data, use_userdef);

      /* Close any user-loaded fonts. */
      BLF_reset_fonts();

      /* Put WM into a stable state for post-readfile processes (kill jobs, removes event handlers,
       * message bus, and so on). */
      BlendFileReadWMSetupData *wm_setup_data = wm_file_read_setup_wm_init(C, bmain, false);

      /* This flag is initialized by the operator but overwritten on read.
       * need to re-enable it here else drivers and registered scripts won't work. */
      const int G_f_orig = G.f;

      /* Frees the current main and replaces it with the new one read from file. */
      BKE_blendfile_read_setup_readfile(
          C, bfd, &params, wm_setup_data, &bf_reports, false, nullptr);
      bmain = CTX_data_main(C);

      /* Finalize handling of WM, using the read WM and/or the current WM depending on things like
       * whether the UI is loaded from the .blend file or not, etc. */
      wm_file_read_setup_wm_finalize(C, bmain, wm_setup_data);

      if (G.f != G_f_orig) {
        const int flags_keep = G_FLAG_ALL_RUNTIME;
        G.f &= G_FLAG_ALL_READFILE;
        G.f = (G.f & ~flags_keep) | (G_f_orig & flags_keep);
      }

      /* Set by the `use_scripts` property on file load.
       * If this was not set, then it should be calculated based on the file-path.
       * Note that this uses `bmain->filepath` and not `filepath`, necessary when
       * recovering the last session, where the file-path can be #BLENDER_QUIT_FILE. */
      if (use_scripts_autoexec_check) {
        WM_file_autoexec_init(bmain->filepath);
      }

      WM_check(C); /* Opens window(s), checks keymaps. */

      if (do_history_file_update) {
        wm_history_file_update();
      }

      wmFileReadPost_Params read_file_post_params{};
      read_file_post_params.use_data = use_data;
      read_file_post_params.use_userdef = use_userdef;
      read_file_post_params.is_startup_file = false;
      read_file_post_params.is_factory_startup = false;
      read_file_post_params.reset_app_template = false;
      read_file_post_params.success = true;
      read_file_post_params.is_alloc = false;
      wm_file_read_post(C, filepath, &read_file_post_params);

      bf_reports.duration.whole = BLI_time_now_seconds() - bf_reports.duration.whole;
      file_read_reports_finalize(&bf_reports);

      success = true;
    }
  }
#if 0
  else if (retval == BKE_READ_EXOTIC_OK_OTHER) {
    BKE_undo_write(C, "Import file");
  }
#endif
  else if (retval == BKE_READ_EXOTIC_FAIL_OPEN) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot read file \"%s\": %s",
                filepath,
                errno ? strerror(errno) : RPT_("unable to open the file"));
  }
  else if (retval == BKE_READ_EXOTIC_FAIL_FORMAT) {
    BKE_reportf(reports, RPT_ERROR, "File format is not supported in file \"%s\"", filepath);
  }
  else if (retval == BKE_READ_EXOTIC_FAIL_PATH) {
    BKE_reportf(reports, RPT_ERROR, "File path \"%s\" invalid", filepath);
  }
  else {
    BKE_reportf(reports, RPT_ERROR, "Unknown error loading \"%s\"", filepath);
    BLI_assert_msg(0, "invalid 'retval'");
  }

  /* NOTE: even if the file fails to load, keep the file in the "Recent Files" list.
   * This is done because failure to load could be caused by the file-system being
   * temporarily offline, see: #127825. */

  WM_cursor_wait(false);

  wm_read_callback_post_wrapper(C, filepath, success);

  BLI_assert(BKE_main_namemap_validate(*CTX_data_main(C)));

  return success;
}

static struct {
  char app_template[64];
  bool override;
} wm_init_state_app_template = {{0}};

void WM_init_state_app_template_set(const char *app_template)
{
  if (app_template) {
    STRNCPY(wm_init_state_app_template.app_template, app_template);
    wm_init_state_app_template.override = true;
  }
  else {
    wm_init_state_app_template.app_template[0] = '\0';
    wm_init_state_app_template.override = false;
  }
}

const char *WM_init_state_app_template_get()
{
  return wm_init_state_app_template.override ? wm_init_state_app_template.app_template : nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Startup & Preferences Blend-File API
 * \{ */

void wm_homefile_read_ex(bContext *C,
                         const wmHomeFileRead_Params *params_homefile,
                         ReportList *reports,
                         wmFileReadPost_Params **r_params_file_read_post)
{
  /* NOTE: unlike #WM_file_read, don't set the wait cursor when reading the home-file.
   * While technically both are reading a file and could use the wait cursor,
   * avoid doing so for the following reasons.
   *
   * - When loading blend with a file (command line or external file browser)
   *   the home-file is read before the file being loaded.
   *   Toggling the wait cursor twice causes the cursor to flicker which looks like a glitch.
   * - In practice it's not that useful as users tend not to set scenes with slow loading times
   *   as their startup.
   */

/* UNUSED, keep as this may be needed later & the comment below isn't self evident. */
#if 0
  /* Context does not always have valid main pointer here. */
  Main *bmain = G_MAIN;
#endif
  bool success = false;

  /* May be enabled, when the user configuration doesn't exist. */
  const bool use_data = params_homefile->use_data;
  const bool use_userdef = params_homefile->use_userdef;
  bool use_factory_settings = params_homefile->use_factory_settings;
  /* Currently this only impacts preferences as it doesn't make much sense to keep the default
   * startup open in the case the app-template doesn't happen to define its own startup.
   * Unlike preferences where we might want to only reset the app-template part of the preferences
   * so as not to reset the preferences for all other Blender instances, see: #96427. */
  const bool use_factory_settings_app_template_only =
      params_homefile->use_factory_settings_app_template_only;
  const bool use_empty_data = params_homefile->use_empty_data;
  const char *filepath_startup_override = params_homefile->filepath_startup_override;
  const char *app_template_override = params_homefile->app_template_override;

  bool filepath_startup_is_factory = true;
  char filepath_startup[FILE_MAX];
  char filepath_userdef[FILE_MAX];

  /* When 'app_template' is set:
   * `{BLENDER_USER_CONFIG}/{app_template}`. */
  char app_template_system[FILE_MAX];
  /* When 'app_template' is set:
   * `{BLENDER_SYSTEM_SCRIPTS}/startup/bl_app_templates_system/{app_template}`. */
  char app_template_config[FILE_MAX];

  eBLOReadSkip skip_flags = eBLOReadSkip(0);

  if (use_data == false) {
    skip_flags |= BLO_READ_SKIP_DATA;
  }
  if (use_userdef == false) {
    skip_flags |= BLO_READ_SKIP_USERDEF;
  }

  /* True if we load startup.blend from memory
   * or use app-template startup.blend which the user hasn't saved. */
  bool is_factory_startup = true;

  const char *app_template = nullptr;
  bool update_defaults = false;

  /* Current Main is not always available in context here. */
  Main *bmain = G_MAIN;

  if (filepath_startup_override != nullptr) {
    /* Pass. */
  }
  else if (app_template_override) {
    /* This may be clearing the current template by setting to an empty string. */
    app_template = app_template_override;
  }
  else if (!use_factory_settings && U.app_template[0]) {
    app_template = U.app_template;
  }

  const bool reset_app_template = ((!app_template && U.app_template[0]) ||
                                   (app_template && !STREQ(app_template, U.app_template)));

  /* Options exclude each other. */
  BLI_assert((use_factory_settings && filepath_startup_override) == 0);

  if ((G.f & G_FLAG_SCRIPT_OVERRIDE_PREF) == 0) {
    SET_FLAG_FROM_TEST(G.f, (U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0, G_FLAG_SCRIPT_AUTOEXEC);
  }

  if (use_data) {
    if (reset_app_template) {
      /* Always load UI when switching to another template. */
      G.fileflags &= ~G_FILE_NO_UI;
    }
  }

  if (use_userdef || reset_app_template) {
#ifdef WITH_PYTHON
    /* This only runs once Blender has already started. */
    if (!params_homefile->is_first_time) {
      BLI_assert(CTX_py_init_get(C));
      /* This is restored by 'wm_file_read_post', disable before loading any preferences
       * so an add-on can read their own preferences when un-registering,
       * and use new preferences if/when re-registering, see #67577.
       *
       * Note that this fits into 'wm_file_read_pre' function but gets messy
       * since we need to know if 'reset_app_template' is true. */
      const char *imports[] = {"addon_utils", nullptr};
      BPY_run_string_eval(C, imports, "addon_utils.disable_all()");
    }
#endif /* WITH_PYTHON */
  }

  if (use_data) {
    /* NOTE: a matching #wm_read_callback_post_wrapper must be called.
     * This runs from #wm_homefile_read_post. */
    wm_read_callback_pre_wrapper(C, "");
  }

  /* For regular file loading this only runs after the file is successfully read.
   * In the case of the startup file, the in-memory startup file is used as a fallback
   * so we know this will work if all else fails. */
  wm_file_read_pre(use_data, use_userdef);

  BlendFileReadWMSetupData *wm_setup_data = nullptr;
  if (use_data) {
    /* Put WM into a stable state for post-readfile processes (kill jobs, removes event handlers,
     * message bus, and so on). */
    wm_setup_data = wm_file_read_setup_wm_init(C, bmain, true);
  }

  filepath_startup[0] = '\0';
  filepath_userdef[0] = '\0';
  app_template_system[0] = '\0';
  app_template_config[0] = '\0';

  const std::optional<std::string> cfgdir = BKE_appdir_folder_id(BLENDER_USER_CONFIG, nullptr);
  if (!use_factory_settings) {
    if (cfgdir.has_value()) {
      BLI_path_join(
          filepath_startup, sizeof(filepath_startup), cfgdir->c_str(), BLENDER_STARTUP_FILE);
      filepath_startup_is_factory = false;
      if (use_userdef) {
        BLI_path_join(
            filepath_userdef, sizeof(filepath_startup), cfgdir->c_str(), BLENDER_USERPREF_FILE);
      }
    }
    else {
      use_factory_settings = true;
    }

    if (filepath_startup_override) {
      STRNCPY(filepath_startup, filepath_startup_override);
      filepath_startup_is_factory = false;
    }
  }

  /* Load preferences before `startup.blend`. */
  if (use_userdef) {
    if (use_factory_settings_app_template_only) {
      /* Use the current preferences as-is (only load in the app_template preferences). */
      skip_flags |= BLO_READ_SKIP_USERDEF;
    }
    else if (!use_factory_settings && BLI_exists(filepath_userdef)) {
      UserDef *userdef = BKE_blendfile_userdef_read(filepath_userdef, nullptr);
      if (userdef != nullptr) {
        CLOG_INFO(&LOG, "Read prefs: \"%s\"", filepath_userdef);

        BKE_blender_userdef_data_set_and_free(userdef);
        userdef = nullptr;

        skip_flags |= BLO_READ_SKIP_USERDEF;
      }
    }
  }

  if ((app_template != nullptr) && (app_template[0] != '\0')) {
    if (!BKE_appdir_app_template_id_search(
            app_template, app_template_system, sizeof(app_template_system)))
    {
      /* Can safely continue with code below, just warn it's not found. */
      BKE_reportf(reports, RPT_WARNING, "Application Template \"%s\" not found", app_template);
    }

    /* Insert template name into startup file. */

    /* Note that the path is being set even when `use_factory_settings == true`
     * this is done so we can load a templates factory-settings. */
    if (!use_factory_settings && cfgdir.has_value()) {
      BLI_path_join(
          app_template_config, sizeof(app_template_config), cfgdir->c_str(), app_template);
      BLI_path_join(
          filepath_startup, sizeof(filepath_startup), app_template_config, BLENDER_STARTUP_FILE);
      filepath_startup_is_factory = false;
      if (BLI_access(filepath_startup, R_OK) != 0) {
        filepath_startup[0] = '\0';
      }
    }
    else {
      filepath_startup[0] = '\0';
    }

    if (filepath_startup[0] == '\0') {
      BLI_path_join(
          filepath_startup, sizeof(filepath_startup), app_template_system, BLENDER_STARTUP_FILE);
      filepath_startup_is_factory = true;

      /* Update defaults only for system templates. */
      update_defaults = true;
    }
  }

  if (!use_factory_settings || (filepath_startup[0] != '\0')) {
    if (BLI_access(filepath_startup, R_OK) == 0) {
      BlendFileReadParams params{};
      params.is_startup = true;
      params.is_factory_settings = use_factory_settings;
      params.skip_flags = skip_flags | BLO_READ_SKIP_USERDEF;
      BlendFileReadReport bf_reports{};
      bf_reports.reports = reports;
      BlendFileData *bfd = BKE_blendfile_read(filepath_startup, &params, &bf_reports);

      if (bfd != nullptr) {
        CLOG_INFO(&LOG, "Read startup: \"%s\"", filepath_startup);

        /* Frees the current main and replaces it with the new one read from file. */
        BKE_blendfile_read_setup_readfile(C,
                                          bfd,
                                          &params,
                                          wm_setup_data,
                                          &bf_reports,
                                          update_defaults && use_data,
                                          app_template);
        success = true;
        bmain = CTX_data_main(C);
      }
    }
    if (success) {
      is_factory_startup = filepath_startup_is_factory;
    }
  }

  if (use_userdef) {
    if ((skip_flags & BLO_READ_SKIP_USERDEF) == 0) {
      UserDef *userdef_default = BKE_blendfile_userdef_from_defaults();
      BKE_blender_userdef_data_set_and_free(userdef_default);
      skip_flags |= BLO_READ_SKIP_USERDEF;
    }
  }

  if (success == false && filepath_startup_override && reports) {
    /* We can not return from here because wm is already reset. */
    BKE_reportf(reports, RPT_ERROR, "Could not read \"%s\"", filepath_startup_override);
  }

  bool loaded_factory_settings = false;
  if (success == false) {
    BlendFileReadParams read_file_params{};
    read_file_params.is_startup = true;
    read_file_params.is_factory_settings = use_factory_settings;
    read_file_params.skip_flags = skip_flags;
    BlendFileData *bfd = BKE_blendfile_read_from_memory(
        datatoc_startup_blend, datatoc_startup_blend_size, &read_file_params, nullptr);
    if (bfd != nullptr) {
      BlendFileReadReport read_report{};
      /* Frees the current main and replaces it with the new one read from file. */
      BKE_blendfile_read_setup_readfile(
          C, bfd, &read_file_params, wm_setup_data, &read_report, true, nullptr);
      success = true;
      loaded_factory_settings = true;
      bmain = CTX_data_main(C);
    }
  }

  if (use_empty_data) {
    BKE_blendfile_read_make_empty(C);
  }

  /* Load template preferences,
   * unlike regular preferences we only use some of the settings,
   * see: #BKE_blender_userdef_set_app_template. */
  if (app_template_system[0] != '\0') {
    char temp_path[FILE_MAX];
    temp_path[0] = '\0';
    if (!use_factory_settings) {
      BLI_path_join(temp_path, sizeof(temp_path), app_template_config, BLENDER_USERPREF_FILE);
      if (BLI_access(temp_path, R_OK) != 0) {
        temp_path[0] = '\0';
      }
    }

    if (temp_path[0] == '\0') {
      BLI_path_join(temp_path, sizeof(temp_path), app_template_system, BLENDER_USERPREF_FILE);
    }

    if (use_userdef) {
      UserDef *userdef_template = nullptr;
      /* Just avoids missing file warning. */
      if (BLI_exists(temp_path)) {
        userdef_template = BKE_blendfile_userdef_read(temp_path, nullptr);
        if (userdef_template) {
          CLOG_INFO(&LOG, "Read prefs from app-template: \"%s\"", temp_path);
        }
      }
      if (userdef_template == nullptr) {
        /* We need to have preferences load to overwrite preferences from previous template. */
        userdef_template = BKE_blendfile_userdef_from_defaults();
      }
      if (userdef_template) {
        BKE_blender_userdef_app_template_data_set_and_free(userdef_template);
        userdef_template = nullptr;
      }
    }
  }

  if (app_template_override) {
    STRNCPY(U.app_template, app_template_override);
  }

  if (use_userdef) {
    /* Check userdef before open window, keymaps etc. */
    wm_init_userdef(bmain);
  }

  if (use_data) {
    /* Finalize handling of WM, using the read WM and/or the current WM depending on things like
     * whether the UI is loaded from the .blend file or not, etc. */
    wm_setup_data->is_factory_startup = loaded_factory_settings;
    wm_file_read_setup_wm_finalize(C, bmain, wm_setup_data);
  }

  if (use_userdef) {
    /* Clear keymaps because the current default keymap may have been initialized
     * from user preferences, which have been reset. */
    LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
      if (wm->runtime->defaultconf) {
        wm->runtime->defaultconf->flag &= ~KEYCONF_INIT_DEFAULT;
      }
    }
  }

  if (use_data) {
    WM_check(C); /* Opens window(s), checks keymaps. */

    bmain->filepath[0] = '\0';
  }

  {
    wmFileReadPost_Params params_file_read_post{};
    params_file_read_post.use_data = use_data;
    params_file_read_post.use_userdef = use_userdef;
    params_file_read_post.is_startup_file = true;
    params_file_read_post.is_factory_startup = is_factory_startup;
    params_file_read_post.is_first_time = params_homefile->is_first_time;

    params_file_read_post.reset_app_template = reset_app_template;

    params_file_read_post.success = success;
    params_file_read_post.is_alloc = false;

    if (r_params_file_read_post == nullptr) {
      wm_homefile_read_post(C, &params_file_read_post);
    }
    else {
      params_file_read_post.is_alloc = true;
      *r_params_file_read_post = MEM_mallocN<wmFileReadPost_Params>(__func__);
      **r_params_file_read_post = params_file_read_post;

      /* Match #wm_file_read_post which leaves the window cleared too. */
      CTX_wm_window_set(C, nullptr);
    }
  }
}

void wm_homefile_read(bContext *C,
                      const wmHomeFileRead_Params *params_homefile,
                      ReportList *reports)
{
  wm_homefile_read_ex(C, params_homefile, reports, nullptr);
}

void wm_homefile_read_post(bContext *C, const wmFileReadPost_Params *params_file_read_post)
{
  const char *filepath = "";
  wm_file_read_post(C, filepath, params_file_read_post);

  if (params_file_read_post->use_data) {
    wm_read_callback_post_wrapper(C, filepath, params_file_read_post->success);
  }

  if (params_file_read_post->is_alloc) {
    MEM_freeN(params_file_read_post);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend-File History API
 * \{ */

void wm_history_file_read()
{
  const std::optional<std::string> cfgdir = BKE_appdir_folder_id(BLENDER_USER_CONFIG, nullptr);
  if (!cfgdir.has_value()) {
    return;
  }

  char filepath[FILE_MAX];
  LinkNode *l;
  int num;

  BLI_path_join(filepath, sizeof(filepath), cfgdir->c_str(), BLENDER_HISTORY_FILE);

  LinkNode *lines = BLI_file_read_as_lines(filepath);

  wm_history_files_free();

  /* Read list of recent opened files from #BLENDER_HISTORY_FILE to memory. */
  for (l = lines, num = 0; l && (num < U.recent_files); l = l->next) {
    const char *line = static_cast<const char *>(l->link);
    /* Don't check if files exist, causes slow startup for remote/external drives. */
    if (line[0]) {
      RecentFile *recent = MEM_mallocN<RecentFile>("RecentFile");
      BLI_addtail(&(G.recent_files), recent);
      recent->filepath = BLI_strdup(line);
      num++;
    }
  }

  BLI_file_free_lines(lines);
}

static RecentFile *wm_history_file_new(const char *filepath)
{
  RecentFile *recent = MEM_mallocN<RecentFile>("RecentFile");
  recent->filepath = BLI_strdup(filepath);
  return recent;
}

static void wm_history_file_free(RecentFile *recent)
{
  BLI_assert(BLI_findindex(&G.recent_files, recent) != -1);
  MEM_freeN(recent->filepath);
  BLI_freelinkN(&G.recent_files, recent);
}

static void wm_history_files_free()
{
  LISTBASE_FOREACH_MUTABLE (RecentFile *, recent, &G.recent_files) {
    wm_history_file_free(recent);
  }
}

static RecentFile *wm_file_history_find(const char *filepath)
{
  return static_cast<RecentFile *>(
      BLI_findstring_ptr(&G.recent_files, filepath, offsetof(RecentFile, filepath)));
}

/**
 * Write #BLENDER_HISTORY_FILE as-is, without checking the environment
 * (that's handled by #wm_history_file_update).
 */
static void wm_history_file_write()
{
  char filepath[FILE_MAX];
  FILE *fp;

  /* Will be nullptr in background mode. */
  const std::optional<std::string> user_config_dir = BKE_appdir_folder_id_create(
      BLENDER_USER_CONFIG, nullptr);
  if (!user_config_dir.has_value()) {
    return;
  }

  BLI_path_join(filepath, sizeof(filepath), user_config_dir->c_str(), BLENDER_HISTORY_FILE);

  fp = BLI_fopen(filepath, "w");
  if (fp) {
    LISTBASE_FOREACH (RecentFile *, recent, &G.recent_files) {
      fprintf(fp, "%s\n", recent->filepath);
    }
    fclose(fp);
  }
}

/**
 * Run after saving a file to refresh the #BLENDER_HISTORY_FILE list.
 */
static void wm_history_file_update()
{
  RecentFile *recent;
  const char *blendfile_path = BKE_main_blendfile_path_from_global();

  /* No write history for recovered startup files. */
  if (blendfile_path[0] == '\0') {
    return;
  }

  recent = static_cast<RecentFile *>(G.recent_files.first);
  /* Refresh #BLENDER_HISTORY_FILE of recent opened files, when current file was changed. */
  if (!(recent) || (BLI_path_cmp(recent->filepath, blendfile_path) != 0)) {

    recent = wm_file_history_find(blendfile_path);
    if (recent) {
      BLI_remlink(&G.recent_files, recent);
    }
    else {
      RecentFile *recent_next;
      for (recent = static_cast<RecentFile *>(BLI_findlink(&G.recent_files, U.recent_files - 1));
           recent;
           recent = recent_next)
      {
        recent_next = recent->next;
        wm_history_file_free(recent);
      }
      recent = wm_history_file_new(blendfile_path);
    }

    /* Add current file to the beginning of list. */
    BLI_addhead(&(G.recent_files), recent);

    /* Write current file to #BLENDER_HISTORY_FILE. */
    wm_history_file_write();

    /* Also update most recent files on system. */
    GHOST_addToSystemRecentFiles(blendfile_path);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Thumbnail Generation: Screen-Shot / Camera View
 *
 * Thumbnail Sizes
 * ===============
 *
 * - `PREVIEW_RENDER_LARGE_HEIGHT * 2` is used to render a large thumbnail,
 *   giving some over-sampling when scaled down:
 *
 * - There are two outputs for this thumbnail:
 *
 *   - An image is saved to the thumbnail cache, sized at #PREVIEW_RENDER_LARGE_HEIGHT.
 *
 *   - A smaller thumbnail is stored in the `.blend` file itself, sized at #BLEN_THUMB_SIZE.
 *     The size is kept small to prevent thumbnails bloating the size of `.blend` files.
 *
 *     The thumbnail will be extracted if the file is shared or the local thumbnail cache
 *     is cleared. see: `blendthumb_extract.cc` for logic that extracts the thumbnail.
 *
 * \{ */

#ifdef USE_THUMBNAIL_FAST_DOWNSCALE
static uint8_t *blend_file_thumb_fast_downscale(const uint8_t *src_rect,
                                                const int src_size[2],
                                                const int dst_size[2])
{
  /* NOTE: this is a faster alternative to #IMBScaleFilter::Box which is
   * especially slow in debug builds, normally debug performance isn't a
   * consideration however it's slow enough to get in the way of development.
   * In release builds this gives ~1.4x speedup. */

  /* Scaling using a box-filter where each box uses an integer-rounded region.
   * Accept a slightly lower quality scale as this is only for thumbnails.
   * In practice the result is visually indistinguishable.
   *
   * Technically the color accumulation *could* overflow (creating some invalid pixels),
   * however this would require the source image to be larger than
   * 65,535 pixels squared (when scaling down to 256x256).
   * As the source input is a screenshot or a small camera render created for the thumbnail,
   * this isn't a concern. */

  BLI_assert(dst_size[0] <= src_size[0] && dst_size[1] <= src_size[1]);
  uint8_t *dst_rect = MEM_malloc_arrayN<uint8_t>(size_t(4 * dst_size[0] * dst_size[1]), __func__);

  /* A row, the width of the destination to accumulate pixel values into
   * before writing into the image. */
  uint32_t *accum_row = MEM_calloc_arrayN<uint32_t>(size_t(dst_size[0] * 4), __func__);

#  ifndef NDEBUG
  /* Assert that samples are calculated correctly. */
  uint64_t sample_count_all = 0;
#  endif

  const uint32_t src_size_x = src_size[0];
  const uint32_t src_size_y = src_size[1];

  const uint32_t dst_size_x = dst_size[0];
  const uint32_t dst_size_y = dst_size[1];
  const uint8_t *src_px = src_rect;

  uint32_t src_y = 0;
  for (uint32_t dst_y = 0; dst_y < dst_size_y; dst_y++) {
    const uint32_t src_y_beg = src_y;
    const uint32_t src_y_end = ((dst_y + 1) * src_size_y) / dst_size_y;
    for (; src_y < src_y_end; src_y++) {
      uint32_t *accum = accum_row;
      uint32_t src_x = 0;
      for (uint32_t dst_x = 0; dst_x < dst_size_x; dst_x++, accum += 4) {
        const uint32_t src_x_end = ((dst_x + 1) * src_size_x) / dst_size_x;
        for (; src_x < src_x_end; src_x++) {
          accum[0] += uint32_t(src_px[0]);
          accum[1] += uint32_t(src_px[1]);
          accum[2] += uint32_t(src_px[2]);
          accum[3] += uint32_t(src_px[3]);
          src_px += 4;
        }
        BLI_assert(src_x == src_x_end);
      }
      BLI_assert(accum == accum_row + (4 * dst_size[0]));
    }

    uint32_t *accum = accum_row;
    uint8_t *dst_px = dst_rect + ((dst_y * dst_size_x) * 4);
    uint32_t src_x_beg = 0;
    const uint32_t span_y = src_y_end - src_y_beg;
    for (uint32_t dst_x = 0; dst_x < dst_size_x; dst_x++) {
      const uint32_t src_x_end = ((dst_x + 1) * src_size_x) / dst_size_x;
      const uint32_t span_x = src_x_end - src_x_beg;

      const uint32_t sample_count = span_x * span_y;
      dst_px[0] = uint8_t(accum[0] / sample_count);
      dst_px[1] = uint8_t(accum[1] / sample_count);
      dst_px[2] = uint8_t(accum[2] / sample_count);
      dst_px[3] = uint8_t(accum[3] / sample_count);
      accum[0] = accum[1] = accum[2] = accum[3] = 0;
      accum += 4;
      dst_px += 4;

      src_x_beg = src_x_end;
#  ifndef NDEBUG
      sample_count_all += sample_count;
#  endif
    }
  }
  BLI_assert(src_px == src_rect + (sizeof(uint8_t[4]) * src_size[0] * src_size[1]));
  BLI_assert(sample_count_all == size_t(src_size[0]) * size_t(src_size[1]));

  MEM_freeN(accum_row);
  return dst_rect;
}
#endif /* USE_THUMBNAIL_FAST_DOWNSCALE */

static blender::int2 blend_file_thumb_clamp_size(const int size[2], const int limit)
{
  blender::int2 result;
  if (size[0] > size[1]) {
    result.x = limit;
    result.y = max_ii(1, int((float(size[1]) / float(size[0])) * limit));
  }
  else {
    result.x = max_ii(1, int((float(size[0]) / float(size[1])) * limit));
    result.y = limit;
  }
  return result;
}

/**
 * Screen-shot the active window.
 */
static ImBuf *blend_file_thumb_from_screenshot(bContext *C, BlendThumbnail **r_thumb)
{
  *r_thumb = nullptr;

  wmWindow *win = CTX_wm_window(C);
  if (G.background || (win == nullptr)) {
    return nullptr;
  }

  /* The window to capture should be a main window (without parent). */
  while (win->parent) {
    win = win->parent;
  }

  wmWindowManager *wm = CTX_wm_manager(C);
  int win_size[2];
  /* NOTE: always read from front-buffer as drawing a window can cause problems while saving,
   * even if this means the thumbnail from the screen-shot fails to be created, see: #98462. */
  ImBuf *ibuf = nullptr;

  if (uint8_t *buffer = WM_window_pixels_read_from_frontbuffer(wm, win, win_size)) {
    const blender::int2 thumb_size_2x = blend_file_thumb_clamp_size(win_size, BLEN_THUMB_SIZE * 2);
    const blender::int2 thumb_size = blend_file_thumb_clamp_size(win_size, BLEN_THUMB_SIZE);

#ifdef USE_THUMBNAIL_FAST_DOWNSCALE
    if ((thumb_size_2x[0] <= win_size[0]) && (thumb_size_2x[1] <= win_size[1])) {
      uint8_t *rect_2x = blend_file_thumb_fast_downscale(buffer, win_size, thumb_size_2x);
      uint8_t *rect = blend_file_thumb_fast_downscale(rect_2x, thumb_size_2x, thumb_size);

      MEM_freeN(buffer);
      ibuf = IMB_allocFromBufferOwn(rect_2x, nullptr, thumb_size_2x.x, thumb_size_2x.y, 24);

      BlendThumbnail *thumb = BKE_main_thumbnail_from_buffer(nullptr, rect, thumb_size);
      MEM_freeN(rect);
      *r_thumb = thumb;
    }
    else
#endif /* USE_THUMBNAIL_FAST_DOWNSCALE */
    {
      ibuf = IMB_allocFromBufferOwn(buffer, nullptr, win_size[0], win_size[1], 24);
      BLI_assert(ibuf != nullptr); /* Never expected to fail. */

      /* File-system thumbnail image can be 256x256. */
      IMB_scale(ibuf, thumb_size_2x.x, thumb_size_2x.y, IMBScaleFilter::Box, false);

      /* Thumbnail inside blend should be 128x128. */
      ImBuf *thumb_ibuf = IMB_dupImBuf(ibuf);
      IMB_scale(thumb_ibuf, thumb_size.x, thumb_size.y, IMBScaleFilter::Box, false);

      BlendThumbnail *thumb = BKE_main_thumbnail_from_imbuf(nullptr, thumb_ibuf);
      IMB_freeImBuf(thumb_ibuf);
      *r_thumb = thumb;
    }
  }

  if (ibuf) {
    /* Save metadata for quick access. */
    char version_str[10];
    SNPRINTF(version_str, "%d.%01d", BLENDER_VERSION / 100, BLENDER_VERSION % 100);
    IMB_metadata_ensure(&ibuf->metadata);
    IMB_metadata_set_field(ibuf->metadata, "Thumb::Blender::Version", version_str);
  }

  /* Must be freed by caller. */
  return ibuf;
}

/**
 * Render the current scene with the active camera.
 *
 * \param screen: can be nullptr.
 */
static ImBuf *blend_file_thumb_from_camera(const bContext *C,
                                           Scene *scene,
                                           bScreen *screen,
                                           BlendThumbnail **r_thumb)
{
  *r_thumb = nullptr;

  /* Scene can be nullptr if running a script at startup and calling the save operator. */
  if (G.background || scene == nullptr) {
    return nullptr;
  }

  /* Will be scaled down, but gives some nice oversampling. */
  ImBuf *ibuf;
  BlendThumbnail *thumb;
  wmWindowManager *wm = CTX_wm_manager(C);
  const float pixelsize_old = U.pixelsize;
  wmWindow *windrawable_old = wm->runtime->windrawable;
  char err_out[256] = "unknown";

  /* Screen if no camera found. */
  ScrArea *area = nullptr;
  ARegion *region = nullptr;
  View3D *v3d = nullptr;

  if (screen != nullptr) {
    area = BKE_screen_find_big_area(screen, SPACE_VIEW3D, 0);
    if (area) {
      v3d = static_cast<View3D *>(area->spacedata.first);
      region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
    }
  }

  if (scene->camera == nullptr && v3d == nullptr) {
    return nullptr;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* Note that with scaling, this ends up being 0.5,
   * as it's a thumbnail, we don't need object centers and friends to be 1:1 size. */
  U.pixelsize = 1.0f;

  if (scene->camera) {
    ibuf = ED_view3d_draw_offscreen_imbuf_simple(depsgraph,
                                                 scene,
                                                 (v3d) ? &v3d->shading : nullptr,
                                                 (v3d) ? eDrawType(v3d->shading.type) : OB_SOLID,
                                                 scene->camera,
                                                 PREVIEW_RENDER_LARGE_HEIGHT * 2,
                                                 PREVIEW_RENDER_LARGE_HEIGHT * 2,
                                                 IB_byte_data,
                                                 (v3d) ? V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS :
                                                         V3D_OFSDRAW_NONE,
                                                 R_ALPHAPREMUL,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 err_out);
  }
  else {
    ibuf = ED_view3d_draw_offscreen_imbuf(depsgraph,
                                          scene,
                                          OB_SOLID,
                                          v3d,
                                          region,
                                          PREVIEW_RENDER_LARGE_HEIGHT * 2,
                                          PREVIEW_RENDER_LARGE_HEIGHT * 2,
                                          IB_byte_data,
                                          R_ALPHAPREMUL,
                                          nullptr,
                                          true,
                                          nullptr,
                                          nullptr,
                                          err_out);
  }

  U.pixelsize = pixelsize_old;

  /* Reset to old drawable. */
  if (windrawable_old) {
    wm_window_make_drawable(wm, windrawable_old);
  }
  else {
    wm_window_clear_drawable(wm);
  }

  if (ibuf) {
    /* Dirty oversampling. */
    ImBuf *thumb_ibuf;
    thumb_ibuf = IMB_dupImBuf(ibuf);

    /* Save metadata for quick access. */
    char version_str[10];
    SNPRINTF(version_str, "%d.%01d", BLENDER_VERSION / 100, BLENDER_VERSION % 100);
    IMB_metadata_ensure(&ibuf->metadata);
    IMB_metadata_set_field(ibuf->metadata, "Thumb::Blender::Version", version_str);

    /* BLEN_THUMB_SIZE is size of thumbnail inside blend file: 128x128. */
    IMB_scale(thumb_ibuf, BLEN_THUMB_SIZE, BLEN_THUMB_SIZE, IMBScaleFilter::Box, false);
    thumb = BKE_main_thumbnail_from_imbuf(nullptr, thumb_ibuf);
    IMB_freeImBuf(thumb_ibuf);
    /* Thumbnail saved to file-system should be 256x256. */
    IMB_scale(ibuf,
              PREVIEW_RENDER_LARGE_HEIGHT,
              PREVIEW_RENDER_LARGE_HEIGHT,
              IMBScaleFilter::Box,
              false);
  }
  else {
    /* '*r_thumb' needs to stay nullptr to prevent a bad thumbnail from being handled. */
    CLOG_WARN(&LOG, "failed to create thumbnail: %s", err_out);
    thumb = nullptr;
  }

  /* Must be freed by caller. */
  *r_thumb = thumb;

  return ibuf;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Write Main Blend-File (internal)
 * \{ */

bool write_crash_blend()
{
  char filepath[FILE_MAX];

  STRNCPY(filepath, BKE_main_blendfile_path_from_global());
  BLI_path_extension_replace(filepath, sizeof(filepath), "_crash.blend");
  BlendFileWriteParams params{};
  const bool success = BLO_write_file(G_MAIN, filepath, G.fileflags, &params, nullptr);
  printf("%s: \"%s\"\n", success ? "written" : "failed", filepath);
  return success;
}

/**
 * Helper to check if file `filepath` can be written.
 * \return true if it can, otherwise report an error and return false.
 */
static bool wm_file_write_check_with_report_on_failure(Main *bmain,
                                                       const char *filepath,
                                                       ReportList *reports)
{
  const int filepath_len = strlen(filepath);
  if (filepath_len == 0) {
    BKE_report(reports, RPT_ERROR, "Path is empty, cannot save");
    return false;
  }

  if (filepath_len >= FILE_MAX) {
    BKE_report(reports, RPT_ERROR, "Path too long, cannot save");
    return false;
  }

  if (bmain->is_asset_edit_file &&
      blender::StringRef(filepath).endswith(BLENDER_ASSET_FILE_SUFFIX))
  {
    BKE_report(reports, RPT_ERROR, "Cannot overwrite files that are managed by the asset system");
    return false;
  }

  LISTBASE_FOREACH (Library *, li, &bmain->libraries) {
    if (BLI_path_cmp(li->runtime->filepath_abs, filepath) == 0) {
      BKE_reportf(reports, RPT_ERROR, "Cannot overwrite used library '%.240s'", filepath);
      return false;
    }
  }

  return true;
}

/**
 * \see #wm_homefile_write_exec wraps #BLO_write_file in a similar way.
 */
static bool wm_file_write(bContext *C,
                          const char *filepath,
                          int fileflags,
                          eBLO_WritePathRemap remap_mode,
                          bool use_save_as_copy,
                          ReportList *reports)
{
  Main *bmain = CTX_data_main(C);
  BlendThumbnail *thumb = nullptr, *main_thumb = nullptr;
  ImBuf *ibuf_thumb = nullptr;

  /* NOTE: used to replace the file extension (to ensure `.blend`),
   * no need to now because the operator ensures,
   * its handy for scripts to save to a predefined name without blender editing it. */

  if (!wm_file_write_check_with_report_on_failure(bmain, filepath, reports)) {
    return false;
  }

  /* Call pre-save callbacks before writing preview,
   * that way you can generate custom file thumbnail. */

  /* NOTE: either #BKE_CB_EVT_SAVE_POST or #BKE_CB_EVT_SAVE_POST_FAIL must run.
   * Runs at the end of this function, don't return beforehand. */
  BKE_callback_exec_string(bmain, BKE_CB_EVT_SAVE_PRE, filepath);

  /* Check if file write permission is OK. */
  if (const int st_mode = BLI_exists(filepath)) {
    bool ok = true;

    if (!BLI_file_is_writable(filepath)) {
      BKE_reportf(
          reports, RPT_ERROR, "Cannot save blend file, path \"%s\" is not writable", filepath);
      ok = false;
    }
    else if (S_ISDIR(st_mode)) {
      /* While the UI mostly prevents this, it's possible to save to an existing
       * directory from Python because the path is used unmodified.
       * Besides it being unlikely the user wants to replace a directory,
       * the file versioning logic (to create `*.blend1` files)
       * would rename the directory with a `1` suffix, see #134101. */
      BKE_reportf(
          reports, RPT_ERROR, "Cannot save blend file, path \"%s\" is a directory", filepath);
      ok = false;
    }

    if (!ok) {
      BKE_callback_exec_string(bmain, BKE_CB_EVT_SAVE_POST_FAIL, filepath);
      return false;
    }
  }

  blender::ed::asset::pre_save_assets(bmain);

  /* Enforce full override check/generation on file save. */
  BKE_lib_override_library_main_operations_create(bmain, true, nullptr);

  /* NOTE: Ideally we would call `WM_redraw_windows` here to remove any open menus.
   * But we can crash if saving from a script, see #92704 & #97627.
   * Just checking `!G.background && BLI_thread_is_main()` is not sufficient to fix this.
   * Additionally some EGL configurations don't support reading the front-buffer
   * immediately after drawing, see: #98462. In that case off-screen drawing is necessary. */

  /* Don't forget not to return without! */
  WM_cursor_wait(true);

  if (U.file_preview_type != USER_FILE_PREVIEW_NONE) {
    /* Blend file thumbnail.
     *
     * - Save before exiting edit-mode, otherwise evaluated-mesh for shared data gets corrupted.
     *   See #27765.
     * - Main can store a `.blend` thumbnail,
     *   useful for background-mode or thumbnail customization.
     */
    main_thumb = thumb = bmain->blen_thumb;
    if (thumb != nullptr) {
      /* In case we are given a valid thumbnail data, just generate image from it. */
      ibuf_thumb = BKE_main_thumbnail_to_imbuf(nullptr, thumb);
    }
    else if (BLI_thread_is_main()) {
      int file_preview_type = U.file_preview_type;

      if (file_preview_type == USER_FILE_PREVIEW_AUTO) {
        Scene *scene = CTX_data_scene(C);
        bScreen *screen = CTX_wm_screen(C);
        bool do_render = (scene != nullptr && scene->camera != nullptr && screen != nullptr &&
                          (BKE_screen_find_big_area(screen, SPACE_VIEW3D, 0) != nullptr));
        file_preview_type = do_render ? USER_FILE_PREVIEW_CAMERA : USER_FILE_PREVIEW_SCREENSHOT;
      }

      switch (file_preview_type) {
        case USER_FILE_PREVIEW_SCREENSHOT: {
          ibuf_thumb = blend_file_thumb_from_screenshot(C, &thumb);
          break;
        }
        case USER_FILE_PREVIEW_CAMERA: {
          ibuf_thumb = blend_file_thumb_from_camera(
              C, CTX_data_scene(C), CTX_wm_screen(C), &thumb);
          break;
        }
        default:
          BLI_assert_unreachable();
      }
    }
  }

  /* Operator now handles overwrite checks. */

  if (G.fileflags & G_FILE_AUTOPACK) {
    BKE_packedfile_pack_all(bmain, reports, false);
  }

  ED_editors_flush_edits(bmain);

  /* XXX(ton): temp solution to solve bug, real fix coming. */
  bmain->recovered = false;

  BlendFileWriteParams blend_write_params{};
  blend_write_params.remap_mode = remap_mode;
  blend_write_params.use_save_versions = true;
  blend_write_params.use_save_as_copy = use_save_as_copy;
  blend_write_params.thumb = thumb;

  const bool success = BLO_write_file(bmain, filepath, fileflags, &blend_write_params, reports);

  if (success) {
    const bool do_history_file_update = (G.background == false) &&
                                        (CTX_wm_manager(C)->op_undo_depth == 0);

    if (use_save_as_copy == false) {
      STRNCPY(bmain->filepath, filepath); /* Is guaranteed current file. */
    }

    SET_FLAG_FROM_TEST(G.fileflags, fileflags & G_FILE_COMPRESS, G_FILE_COMPRESS);

    /* Prevent background mode scripts from clobbering history. */
    if (do_history_file_update) {
      wm_history_file_update();
    }

    /* Run this function after because the file can't be written before the blend is. */
    if (ibuf_thumb) {
      IMB_thumb_delete(filepath, THB_FAIL); /* Without this a failed thumb overrides. */
      ibuf_thumb = IMB_thumb_create(filepath, THB_LARGE, THB_SOURCE_BLEND, ibuf_thumb);
    }
  }

  BKE_callback_exec_string(
      bmain, success ? BKE_CB_EVT_SAVE_POST : BKE_CB_EVT_SAVE_POST_FAIL, filepath);

  if (ibuf_thumb) {
    IMB_freeImBuf(ibuf_thumb);
  }
  if (thumb && thumb != main_thumb) {
    MEM_freeN(thumb);
  }

  WM_cursor_wait(false);

  return success;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto-Save API
 * \{ */

static void wm_autosave_location(char filepath[FILE_MAX])
{
  const int pid = abs(getpid());
  char filename[1024];

  /* Normally there is no need to check for this to be nullptr,
   * however this runs on exit when it may be cleared. */
  Main *bmain = G_MAIN;
  const char *blendfile_path = bmain ? BKE_main_blendfile_path(bmain) : nullptr;

  if (blendfile_path && (blendfile_path[0] != '\0')) {
    const char *basename = BLI_path_basename(blendfile_path);
    int len = strlen(basename) - 6;
    SNPRINTF(filename, "%.*s_%d_autosave.blend", len, basename, pid);
  }
  else {
    SNPRINTF(filename, "%d_autosave.blend", pid);
  }

  const char *tempdir_base = BKE_tempdir_base();
  BLI_path_join(filepath, FILE_MAX, tempdir_base, filename);
}

static bool wm_autosave_write_try(Main *bmain, wmWindowManager *wm)
{
  if (wm->file_saved) {
    /* When file is already saved, skip creating an auto-save file, see: #146003 */
    return true;
  }

  char filepath[FILE_MAX];
  wm_autosave_location(filepath);

  /* Technically, we could always just save here, but that would cause performance regressions
   * compared to when the #MemFile undo step was used for saving undo-steps. So for now just skip
   * auto-save when we are in a mode where auto-save wouldn't have worked previously anyway. This
   * check can be removed once the performance regressions have been solved. */
  if (ED_undosys_stack_memfile_get_if_active(wm->runtime->undo_stack) != nullptr) {
    WM_autosave_write(wm, bmain);
    return true;
  }
  if ((U.uiflag & USER_GLOBALUNDO) == 0) {
    WM_autosave_write(wm, bmain);
    return true;
  }
  /* Can't auto-save with MemFile right now, try again later. */
  return false;
}

bool WM_autosave_is_scheduled(wmWindowManager *wm)
{
  return wm->autosave_scheduled;
}

void WM_autosave_write(wmWindowManager *wm, Main *bmain)
{
  ED_editors_flush_edits(bmain);

  char filepath[FILE_MAX];
  wm_autosave_location(filepath);
  /* Save as regular blend file with recovery information and always compress them, see: !132685.
   */
  const int fileflags = G.fileflags | G_FILE_RECOVER_WRITE | G_FILE_COMPRESS;

  /* Error reporting into console. */
  BlendFileWriteParams params{};
  BLO_write_file(bmain, filepath, fileflags, &params, nullptr);

  /* Restart auto-save timer. */
  wm_autosave_timer_end(wm);
  wm_autosave_timer_begin(wm);

  wm->autosave_scheduled = false;
}

static void wm_autosave_timer_begin_ex(wmWindowManager *wm, double timestep)
{
  wm_autosave_timer_end(wm);

  if (U.flag & USER_AUTOSAVE) {
    wm->autosavetimer = WM_event_timer_add(wm, nullptr, TIMERAUTOSAVE, timestep);
  }
}

void wm_autosave_timer_begin(wmWindowManager *wm)
{
  wm_autosave_timer_begin_ex(wm, U.savetime * 60.0);
}

void wm_autosave_timer_end(wmWindowManager *wm)
{
  if (wm->autosavetimer) {
    WM_event_timer_remove(wm, nullptr, wm->autosavetimer);
    wm->autosavetimer = nullptr;
  }
}

void WM_file_autosave_init(wmWindowManager *wm)
{
  wm_autosave_timer_begin(wm);
}

void wm_autosave_timer(Main *bmain, wmWindowManager *wm, wmTimer * /*wt*/)
{
  wm_autosave_timer_end(wm);

  /* If a modal operator is running, don't autosave because we might not be in
   * a valid state to save. But try again in 10ms. */
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
      if (handler_base->type == WM_HANDLER_TYPE_OP) {
        wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
        if (handler->op) {
          wm_autosave_timer_begin_ex(wm, 0.01);
          return;
        }
      }
    }
  }

  wm->autosave_scheduled = false;
  if (!wm_autosave_write_try(bmain, wm)) {
    wm->autosave_scheduled = true;
  }
  /* Restart the timer after file write, just in case file write takes a long time. */
  wm_autosave_timer_begin(wm);
}

void wm_autosave_delete()
{
  char filepath[FILE_MAX];

  wm_autosave_location(filepath);

  if (BLI_exists(filepath)) {
    char filepath_quit[FILE_MAX];
    BLI_path_join(filepath_quit, sizeof(filepath_quit), BKE_tempdir_base(), BLENDER_QUIT_FILE);

    /* For global undo; remove temporarily saved file, otherwise rename. */
    if (U.uiflag & USER_GLOBALUNDO) {
      BLI_delete(filepath, false, false);
    }
    else {
      BLI_rename_overwrite(filepath, filepath_quit);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Operator Properties
 * \{ */

/** Use for loading factory startup & preferences. */
static void read_factory_reset_props(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* So it's possible to reset app-template settings without resetting other defaults. */
  prop = RNA_def_boolean(ot->srna,
                         "use_factory_startup_app_template_only",
                         false,
                         "Factory Startup App-Template Only",
                         "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Initialize `WM_OT_open_*` Properties
 *
 * Check if load_ui was set by the caller.
 * Fall back to user preference when file flags not specified.
 *
 * \{ */

void wm_open_init_load_ui(wmOperator *op, bool use_prefs)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "load_ui");
  if (!RNA_property_is_set(op->ptr, prop)) {
    bool value = use_prefs ? ((U.flag & USER_FILENOUI) == 0) : ((G.fileflags & G_FILE_NO_UI) == 0);

    RNA_property_boolean_set(op->ptr, prop, value);
  }
}

bool wm_open_init_use_scripts(wmOperator *op, bool use_prefs)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_scripts");
  bool use_scripts_autoexec_check = false;
  if (!RNA_property_is_set(op->ptr, prop)) {
    /* Use #G_FLAG_SCRIPT_AUTOEXEC rather than the userpref because this means if
     * the flag has been disabled from the command line, then opening
     * from the menu won't enable this setting. */
    bool value = use_prefs ? ((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0) :
                             ((G.f & G_FLAG_SCRIPT_AUTOEXEC) != 0);

    RNA_property_boolean_set(op->ptr, prop, value);
    use_scripts_autoexec_check = true;
  }
  return use_scripts_autoexec_check;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Startup File Save Operator
 * \{ */

/**
 * \see #wm_file_write wraps #BLO_write_file in a similar way.
 * \return success.
 */
static wmOperatorStatus wm_homefile_write_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  char filepath[FILE_MAX];
  int fileflags;

  const char *app_template = U.app_template[0] ? U.app_template : nullptr;
  const std::optional<std::string> cfgdir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG,
                                                                        app_template);
  if (!cfgdir.has_value()) {
    BKE_report(op->reports, RPT_ERROR, "Unable to create user config path");
    return OPERATOR_CANCELLED;
  }

  /* NOTE: either #BKE_CB_EVT_SAVE_POST or #BKE_CB_EVT_SAVE_POST_FAIL must run.
   * Runs at the end of this function, don't return beforehand. */
  BKE_callback_exec_string(bmain, BKE_CB_EVT_SAVE_PRE, "");
  blender::ed::asset::pre_save_assets(bmain);

  /* Check current window and close it if temp. */
  if (win && WM_window_is_temp_screen(win)) {
    wm_window_close(C, wm, win);
  }

  /* Update keymaps in user preferences. */
  WM_keyconfig_update(wm);

  BLI_path_join(filepath, sizeof(filepath), cfgdir->c_str(), BLENDER_STARTUP_FILE);

  CLOG_INFO_NOCHECK(&LOG, "Writing startup file: \"%s\" ", filepath);

  ED_editors_flush_edits(bmain);

  /* Force save as regular blend file. */
  fileflags = G.fileflags & ~G_FILE_COMPRESS;

  BlendFileWriteParams blend_write_params{};
  /* Make all paths absolute when saving the startup file.
   * On load the `G.main->filepath` will be empty so the paths
   * won't have a base for resolving the relative paths. */
  blend_write_params.remap_mode = BLO_WRITE_PATH_REMAP_ABSOLUTE;
  /* Don't apply any path changes to the current blend file. */
  blend_write_params.use_save_as_copy = true;

  const bool success = BLO_write_file(
      bmain, filepath, fileflags, &blend_write_params, op->reports);

  BKE_callback_exec_string(bmain, success ? BKE_CB_EVT_SAVE_POST : BKE_CB_EVT_SAVE_POST_FAIL, "");

  if (success) {
    BKE_report(op->reports, RPT_INFO, "Startup file saved");
    return OPERATOR_FINISHED;
  }
  CLOG_WARN(&LOG, "Failed to write startup file");
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus wm_homefile_write_invoke(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent * /*event*/)
{
  if (!U.app_template[0]) {
    return WM_operator_confirm_ex(C,
                                  op,
                                  IFACE_("Overwrite Startup File"),
                                  IFACE_("Blender will start next time as it is now."),
                                  IFACE_("Overwrite"),
                                  ALERT_ICON_QUESTION,
                                  false);
  }

  /* A different message if this is overriding a specific template startup file. */
  char display_name[FILE_MAX];
  BLI_path_to_display_name(display_name, sizeof(display_name), IFACE_(U.app_template));
  std::string message = fmt::format(
      fmt::runtime(IFACE_("Template \"{}\" will start next time as it is now.")),
      IFACE_(display_name));
  return WM_operator_confirm_ex(C,
                                op,
                                IFACE_("Overwrite Template Startup File"),
                                message.c_str(),
                                IFACE_("Overwrite"),
                                ALERT_ICON_QUESTION,
                                false);
}

void WM_OT_save_homefile(wmOperatorType *ot)
{
  ot->name = "Save Startup File";
  ot->idname = "WM_OT_save_homefile";
  ot->description = "Make the current file the default startup file";

  ot->invoke = wm_homefile_write_invoke;
  ot->exec = wm_homefile_write_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Write Preferences Operator
 * \{ */

/* Only save the prefs block. operator entry. */
static wmOperatorStatus wm_userpref_write_exec(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* Update keymaps in user preferences. */
  WM_keyconfig_update(wm);

  const bool success = BKE_blendfile_userdef_write_all(op->reports);

  return success ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void WM_OT_save_userpref(wmOperatorType *ot)
{
  ot->name = "Save Preferences";
  ot->idname = "WM_OT_save_userpref";
  ot->description = "Make the current preferences default";

  ot->invoke = WM_operator_confirm;
  ot->exec = wm_userpref_write_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Preferences Operator
 * \{ */

/**
 * When reading preferences, there are some exceptions for values which are reset.
 */
static void wm_userpref_read_exceptions(UserDef *userdef_curr, const UserDef *userdef_prev)
{
#define USERDEF_RESTORE(member) \
  { \
    userdef_curr->member = userdef_prev->member; \
  } \
  ((void)0)

  /* Current visible preferences category. */
  USERDEF_RESTORE(space_data.section_active);

#undef USERDEF_RESTORE
}

static void rna_struct_update_when_changed(bContext *C,
                                           Main *bmain,
                                           PointerRNA *ptr_a,
                                           PointerRNA *ptr_b)
{
  CollectionPropertyIterator iter;
  PropertyRNA *iterprop = RNA_struct_iterator_property(ptr_a->type);
  BLI_assert(ptr_a->type == ptr_b->type);
  RNA_property_collection_begin(ptr_a, iterprop, &iter);
  for (; iter.valid; RNA_property_collection_next(&iter)) {
    PropertyRNA *prop = static_cast<PropertyRNA *>(iter.ptr.data);
    if (STREQ(RNA_property_identifier(prop), "rna_type")) {
      continue;
    }
    switch (RNA_property_type(prop)) {
      case PROP_POINTER: {
        PointerRNA ptr_sub_a = RNA_property_pointer_get(ptr_a, prop);
        PointerRNA ptr_sub_b = RNA_property_pointer_get(ptr_b, prop);
        rna_struct_update_when_changed(C, bmain, &ptr_sub_a, &ptr_sub_b);
        break;
      }
      case PROP_COLLECTION:
        /* Don't handle collections. */
        break;
      default: {
        if (!RNA_property_equals(bmain, ptr_a, ptr_b, prop, RNA_EQ_STRICT)) {
          RNA_property_update(C, ptr_b, prop);
        }
      }
    }
  }
  RNA_property_collection_end(&iter);
}

static void wm_userpref_update_when_changed(bContext *C,
                                            Main *bmain,
                                            UserDef *userdef_prev,
                                            UserDef *userdef_curr)
{
  PointerRNA ptr_a = RNA_pointer_create_discrete(nullptr, &RNA_Preferences, userdef_prev);
  PointerRNA ptr_b = RNA_pointer_create_discrete(nullptr, &RNA_Preferences, userdef_curr);
  const bool is_dirty = userdef_curr->runtime.is_dirty;

  rna_struct_update_when_changed(C, bmain, &ptr_a, &ptr_b);

  WM_reinit_gizmomap_all(bmain);
  WM_keyconfig_reload(C);

  userdef_curr->runtime.is_dirty = is_dirty;
}

static wmOperatorStatus wm_userpref_read_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const bool use_data = false;
  const bool use_userdef = true;
  const bool use_factory_settings = STREQ(op->type->idname, "WM_OT_read_factory_userpref");
  const bool use_factory_settings_app_template_only =
      (use_factory_settings && RNA_boolean_get(op->ptr, "use_factory_startup_app_template_only"));

  BKE_callback_exec_null(bmain, BKE_CB_EVT_EXTENSION_REPOS_UPDATE_PRE);

  UserDef U_backup = blender::dna::shallow_copy(U);

  wmHomeFileRead_Params read_homefile_params{};
  read_homefile_params.use_data = use_data;
  read_homefile_params.use_userdef = use_userdef;
  read_homefile_params.use_factory_settings = use_factory_settings;
  read_homefile_params.use_factory_settings_app_template_only =
      use_factory_settings_app_template_only;
  read_homefile_params.use_empty_data = false;
  read_homefile_params.filepath_startup_override = nullptr;
  read_homefile_params.app_template_override = WM_init_state_app_template_get();
  wm_homefile_read(C, &read_homefile_params, op->reports);

  wm_userpref_read_exceptions(&U, &U_backup);
  SET_FLAG_FROM_TEST(G.f, use_factory_settings, G_FLAG_USERPREF_NO_SAVE_ON_EXIT);

  wm_userpref_update_when_changed(C, bmain, &U_backup, &U);

  if (use_factory_settings) {
    U.runtime.is_dirty = true;
  }

  BKE_callback_exec_null(bmain, BKE_CB_EVT_EXTENSION_REPOS_UPDATE_POST);

  /* Needed to recalculate UI scaling values (eg, #UserDef.inv_scale_factor). */
  wm_window_clear_drawable(static_cast<wmWindowManager *>(bmain->wm.first));

  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

void WM_OT_read_userpref(wmOperatorType *ot)
{
  ot->name = "Load Preferences";
  ot->idname = "WM_OT_read_userpref";
  ot->description = "Load last saved preferences";

  ot->invoke = WM_operator_confirm;
  ot->exec = wm_userpref_read_exec;
}

static wmOperatorStatus wm_userpref_read_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  std::string title;

  const bool template_only = U.app_template[0] &&
                             RNA_boolean_get(op->ptr, "use_factory_startup_app_template_only");

  if (template_only) {
    char display_name[FILE_MAX];
    BLI_path_to_display_name(display_name, sizeof(display_name), IFACE_(U.app_template));
    title = fmt::format(fmt::runtime(IFACE_("Load Factory \"{}\" Preferences.")),
                        IFACE_(display_name));
  }
  else {
    title = IFACE_("Load Factory Blender Preferences");
  }

  return WM_operator_confirm_ex(
      C,
      op,
      title.c_str(),
      IFACE_("To make changes to Preferences permanent, use \"Save Preferences\""),
      IFACE_("Load"),
      ALERT_ICON_WARNING,
      false);
}

void WM_OT_read_factory_userpref(wmOperatorType *ot)
{
  ot->name = "Load Factory Preferences";
  ot->idname = "WM_OT_read_factory_userpref";
  ot->description =
      "Load factory default preferences. "
      "To make changes to preferences permanent, use \"Save Preferences\"";

  ot->invoke = wm_userpref_read_invoke;
  ot->exec = wm_userpref_read_exec;

  read_factory_reset_props(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read File History Operator
 * \{ */

static wmOperatorStatus wm_history_file_read_exec(bContext * /*C*/, wmOperator * /*op*/)
{
  ED_file_read_bookmarks();
  wm_history_file_read();
  return OPERATOR_FINISHED;
}

void WM_OT_read_history(wmOperatorType *ot)
{
  ot->name = "Reload History File";
  ot->idname = "WM_OT_read_history";
  ot->description = "Reloads history and bookmarks";

  ot->invoke = WM_operator_confirm;
  ot->exec = wm_history_file_read_exec;

  /* This operator is only used for loading settings from a previous blender install. */
  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read Startup & Preferences Operator
 *
 * Both #WM_OT_read_homefile & #WM_OT_read_factory_settings.
 * \{ */

static wmOperatorStatus wm_homefile_read_exec(bContext *C, wmOperator *op)
{
  const bool use_factory_startup_and_userdef = STREQ(op->type->idname,
                                                     "WM_OT_read_factory_settings");
  const bool use_factory_settings = use_factory_startup_and_userdef ||
                                    RNA_boolean_get(op->ptr, "use_factory_startup");
  const bool use_factory_settings_app_template_only =
      (use_factory_startup_and_userdef &&
       RNA_boolean_get(op->ptr, "use_factory_startup_app_template_only"));

  bool use_userdef = false;
  char filepath_buf[FILE_MAX];
  const char *filepath = nullptr;
  UserDef U_backup = blender::dna::shallow_copy(U);

  if (!use_factory_settings) {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "filepath");

    /* This can be used when loading of a start-up file should only change
     * the scene content but keep the blender UI as it is. */
    wm_open_init_load_ui(op, true);
    SET_FLAG_FROM_TEST(G.fileflags, !RNA_boolean_get(op->ptr, "load_ui"), G_FILE_NO_UI);

    if (RNA_property_is_set(op->ptr, prop)) {
      RNA_property_string_get(op->ptr, prop, filepath_buf);
      filepath = filepath_buf;
      if (BLI_access(filepath, R_OK)) {
        BKE_reportf(
            op->reports, RPT_ERROR, "Cannot read alternative start-up file: \"%s\"", filepath);
        return OPERATOR_CANCELLED;
      }
    }
  }
  else {
    if (use_factory_startup_and_userdef) {
      /* Always load UI for factory settings (prefs will re-init). */
      G.fileflags &= ~G_FILE_NO_UI;
      /* Always load preferences with factory settings. */
      use_userdef = true;
    }
  }

  /* Close any user-loaded fonts. */
  BLF_reset_fonts();

  char app_template_buf[sizeof(U.app_template)];
  const char *app_template;
  PropertyRNA *prop_app_template = RNA_struct_find_property(op->ptr, "app_template");
  const bool use_splash = !use_factory_settings && RNA_boolean_get(op->ptr, "use_splash");
  const bool use_empty_data = RNA_boolean_get(op->ptr, "use_empty");

  if (prop_app_template && RNA_property_is_set(op->ptr, prop_app_template)) {
    RNA_property_string_get(op->ptr, prop_app_template, app_template_buf);
    app_template = app_template_buf;

    if (!use_factory_settings) {
      /* Always load preferences when switching templates with own preferences. */
      use_userdef = BKE_appdir_app_template_has_userpref(app_template) ||
                    BKE_appdir_app_template_has_userpref(U.app_template);
    }

    /* Turn override off, since we're explicitly loading a different app-template. */
    WM_init_state_app_template_set(nullptr);
  }
  else {
    /* Normally nullptr, only set when overriding from the command-line. */
    app_template = WM_init_state_app_template_get();
  }

  if (use_userdef) {
    BKE_callback_exec_null(CTX_data_main(C), BKE_CB_EVT_EXTENSION_REPOS_UPDATE_PRE);
  }

  wmHomeFileRead_Params read_homefile_params{};
  read_homefile_params.use_data = true;
  read_homefile_params.use_userdef = use_userdef;
  read_homefile_params.use_factory_settings = use_factory_settings;
  read_homefile_params.use_factory_settings_app_template_only =
      use_factory_settings_app_template_only;
  read_homefile_params.use_empty_data = use_empty_data;
  read_homefile_params.filepath_startup_override = filepath;
  read_homefile_params.app_template_override = app_template;
  wm_homefile_read(C, &read_homefile_params, op->reports);

  if (use_splash) {
    WM_init_splash(C);
  }

  if (use_userdef) {
    wm_userpref_read_exceptions(&U, &U_backup);
    SET_FLAG_FROM_TEST(G.f, use_factory_settings, G_FLAG_USERPREF_NO_SAVE_ON_EXIT);

    if (use_factory_settings) {
      U.runtime.is_dirty = true;
    }
  }

  if (use_userdef) {
    BKE_callback_exec_null(CTX_data_main(C), BKE_CB_EVT_EXTENSION_REPOS_UPDATE_POST);
  }

  if (G.fileflags & G_FILE_NO_UI) {
    ED_outliner_select_sync_from_all_tag(C);
  }

  return OPERATOR_FINISHED;
}

static void wm_homefile_read_after_dialog_callback(bContext *C, void *user_data)
{
  WM_operator_name_call_with_properties(C,
                                        "WM_OT_read_homefile",
                                        blender::wm::OpCallContext::ExecDefault,
                                        (IDProperty *)user_data,
                                        nullptr);
}

static wmOperatorStatus wm_homefile_read_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  if (wm_operator_close_file_dialog_if_needed(C, op, wm_homefile_read_after_dialog_callback)) {
    return OPERATOR_INTERFACE;
  }
  return wm_homefile_read_exec(C, op);
}

static void read_homefile_props(wmOperatorType *ot)
{
  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna, "app_template", "Template", sizeof(U.app_template), "", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "use_empty",
      false,
      "Empty",
      "After loading, remove everything except scenes, windows, and workspaces. This makes it "
      "possible to load the startup file with its scene configuration and window layout intact, "
      "but no objects, materials, animations, ...");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void WM_OT_read_homefile(wmOperatorType *ot)
{
  PropertyRNA *prop;
  ot->name = "Reload Start-Up File";
  ot->idname = "WM_OT_read_homefile";
  ot->description = "Open the default file";

  ot->invoke = wm_homefile_read_invoke;
  ot->exec = wm_homefile_read_exec;

  prop = RNA_def_string_file_path(ot->srna,
                                  "filepath",
                                  nullptr,
                                  FILE_MAX,
                                  "File Path",
                                  "Path to an alternative start-up file");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  /* So scripts can use an alternative start-up file without the UI. */
  prop = RNA_def_boolean(
      ot->srna, "load_ui", true, "Load UI", "Load user interface setup from the .blend file");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  /* So the splash can be kept open after loading a file (for templates). */
  prop = RNA_def_boolean(ot->srna, "use_splash", false, "Splash", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  /* So scripts can load factory-startup without resetting preferences
   * (which has other implications such as reloading all add-ons).
   * Match naming for `--factory-startup` command line argument. */
  prop = RNA_def_boolean(ot->srna,
                         "use_factory_startup",
                         false,
                         "Factory Startup",
                         "Load the default ('factory startup') blend file. "
                         "This is independent of the normal start-up file that the user can save");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  read_factory_reset_props(ot);

  read_homefile_props(ot);

  /* Omit poll to run in background mode. */
}

static wmOperatorStatus wm_read_factory_settings_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent * /*event*/)
{
  const bool unsaved = wm_file_or_session_data_has_unsaved_changes(CTX_data_main(C),
                                                                   CTX_wm_manager(C));
  std::string title;
  const bool template_only = U.app_template[0] &&
                             RNA_boolean_get(op->ptr, "use_factory_startup_app_template_only");

  if (template_only) {
    char display_name[FILE_MAX];
    BLI_path_to_display_name(display_name, sizeof(display_name), IFACE_(U.app_template));
    title = fmt::format(fmt::runtime(IFACE_("Load Factory \"{}\" Startup File and Preferences")),
                        IFACE_(display_name));
  }
  else {
    title = IFACE_("Load Factory Default Startup File and Preferences");
  }

  return WM_operator_confirm_ex(
      C,
      op,
      title.c_str(),
      unsaved ? IFACE_("To make changes to Preferences permanent, use \"Save Preferences\".\n"
                       "Warning: Your file is unsaved! Proceeding will abandon your changes.") :
                IFACE_("To make changes to Preferences permanent, use \"Save Preferences\"."),
      IFACE_("Load"),
      ALERT_ICON_WARNING,
      false);
}

void WM_OT_read_factory_settings(wmOperatorType *ot)
{
  ot->name = "Load Factory Settings";
  ot->idname = "WM_OT_read_factory_settings";
  ot->description =
      "Load factory default startup file and preferences. "
      "To make changes permanent, use \"Save Startup File\" and \"Save Preferences\"";

  ot->invoke = wm_read_factory_settings_invoke;
  ot->exec = wm_homefile_read_exec;
  /* Omit poll to run in background mode. */

  read_factory_reset_props(ot);

  read_homefile_props(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Open Main .blend File Utilities
 * \{ */

/**
 * Wrap #WM_file_read, shared by file reading operators.
 */
static bool wm_file_read_opwrap(bContext *C,
                                const char *filepath,
                                const bool use_scripts_autoexec_check,
                                ReportList *reports)
{
  /* XXX: wm in context is not set correctly after #WM_file_read -> crash. */
  /* Do it before for now, but is this correct with multiple windows? */
  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  const bool success = WM_file_read(C, filepath, use_scripts_autoexec_check, reports);

  return success;
}

/* Generic operator state utilities. */

static void create_operator_state(wmOperatorType *ot, int first_state)
{
  PropertyRNA *prop = RNA_def_int(
      ot->srna, "state", first_state, INT32_MIN, INT32_MAX, "State", "", INT32_MIN, INT32_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static int get_operator_state(wmOperator *op)
{
  return RNA_int_get(op->ptr, "state");
}

static void set_next_operator_state(wmOperator *op, int state)
{
  RNA_int_set(op->ptr, "state", state);
}

struct OperatorDispatchTarget {
  int state;
  wmOperatorStatus (*run)(bContext *C, wmOperator *op);
};

static wmOperatorStatus operator_state_dispatch(bContext *C,
                                                wmOperator *op,
                                                OperatorDispatchTarget *targets)
{
  int state = get_operator_state(op);
  for (int i = 0; targets[i].run; i++) {
    OperatorDispatchTarget target = targets[i];
    if (target.state == state) {
      return target.run(C, op);
    }
  }
  BLI_assert_unreachable();
  return OPERATOR_CANCELLED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Open Main .blend File Operator
 * \{ */

enum {
  OPEN_MAINFILE_STATE_DISCARD_CHANGES,
  OPEN_MAINFILE_STATE_SELECT_FILE_PATH,
  OPEN_MAINFILE_STATE_OPEN,
};

static wmOperatorStatus wm_open_mainfile_dispatch(bContext *C, wmOperator *op);

static void wm_open_mainfile_after_dialog_callback(bContext *C, void *user_data)
{
  WM_operator_name_call_with_properties(C,
                                        "WM_OT_open_mainfile",
                                        blender::wm::OpCallContext::InvokeDefault,
                                        (IDProperty *)user_data,
                                        nullptr);
}

static wmOperatorStatus wm_open_mainfile__discard_changes_exec(bContext *C, wmOperator *op)
{
  if (RNA_boolean_get(op->ptr, "display_file_selector")) {
    set_next_operator_state(op, OPEN_MAINFILE_STATE_SELECT_FILE_PATH);
  }
  else {
    set_next_operator_state(op, OPEN_MAINFILE_STATE_OPEN);
  }

  if (wm_operator_close_file_dialog_if_needed(C, op, wm_open_mainfile_after_dialog_callback)) {
    return OPERATOR_INTERFACE;
  }
  return wm_open_mainfile_dispatch(C, op);
}

static wmOperatorStatus wm_open_mainfile__select_file_path_exec(bContext *C, wmOperator *op)
{
  set_next_operator_state(op, OPEN_MAINFILE_STATE_OPEN);

  Main *bmain = CTX_data_main(C);
  const char *blendfile_path = BKE_main_blendfile_path(bmain);

  if (CTX_wm_window(C) == nullptr) {
    /* In rare cases this could happen, when trying to invoke in background
     * mode on load for example. Don't use poll for this because exec()
     * can still run without a window. */
    BKE_report(op->reports, RPT_ERROR, "Context window not set");
    return OPERATOR_CANCELLED;
  }

  /* If possible, get the name of the most recently used `.blend` file. */
  if (G.recent_files.first) {
    RecentFile *recent = static_cast<RecentFile *>(G.recent_files.first);
    blendfile_path = recent->filepath;
  }

  RNA_string_set(op->ptr, "filepath", blendfile_path);
  wm_open_init_load_ui(op, true);
  const bool use_scripts_autoexec_check = wm_open_init_use_scripts(op, true);
  UNUSED_VARS(use_scripts_autoexec_check); /* The user can set this in the UI. */
  op->customdata = nullptr;

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus wm_open_mainfile__open(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  bool success;

  RNA_string_get(op->ptr, "filepath", filepath);
  BLI_path_canonicalize_native(filepath, sizeof(filepath));

  /* For file opening, also print in console for warnings, not only errors. */
  BKE_report_print_level_set(op->reports, RPT_WARNING);

  /* Re-use last loaded setting so we can reload a file without changing. */
  wm_open_init_load_ui(op, false);
  const bool use_scripts_autoexec_check = wm_open_init_use_scripts(op, false);

  SET_FLAG_FROM_TEST(G.fileflags, !RNA_boolean_get(op->ptr, "load_ui"), G_FILE_NO_UI);
  SET_FLAG_FROM_TEST(G.f, RNA_boolean_get(op->ptr, "use_scripts"), G_FLAG_SCRIPT_AUTOEXEC);
  success = wm_file_read_opwrap(C, filepath, use_scripts_autoexec_check, op->reports);

  if (success) {
    if (G.fileflags & G_FILE_NO_UI) {
      ED_outliner_select_sync_from_all_tag(C);
    }
    ED_view3d_local_collections_reset(C, (G.fileflags & G_FILE_NO_UI) != 0);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static OperatorDispatchTarget wm_open_mainfile_dispatch_targets[] = {
    {OPEN_MAINFILE_STATE_DISCARD_CHANGES, wm_open_mainfile__discard_changes_exec},
    {OPEN_MAINFILE_STATE_SELECT_FILE_PATH, wm_open_mainfile__select_file_path_exec},
    {OPEN_MAINFILE_STATE_OPEN, wm_open_mainfile__open},
    {0, nullptr},
};

static wmOperatorStatus wm_open_mainfile_dispatch(bContext *C, wmOperator *op)
{
  return operator_state_dispatch(C, op, wm_open_mainfile_dispatch_targets);
}

static wmOperatorStatus wm_open_mainfile_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  return wm_open_mainfile_dispatch(C, op);
}

static wmOperatorStatus wm_open_mainfile_exec(bContext *C, wmOperator *op)
{
  return wm_open_mainfile__open(C, op);
}

static std::string wm_open_mainfile_get_description(bContext * /*C*/,
                                                    wmOperatorType * /*ot*/,
                                                    PointerRNA *ptr)
{
  if (!RNA_struct_property_is_set(ptr, "filepath")) {
    return "";
  }

  char filepath[FILE_MAX];
  RNA_string_get(ptr, "filepath", filepath);

  BLI_stat_t stats;
  if (BLI_stat(filepath, &stats) == -1) {
    return fmt::format("{}\n\n{}", filepath, TIP_("File Not Found"));
  }

  /* Date. */
  char date_str[FILELIST_DIRENTRY_DATE_LEN];
  char time_str[FILELIST_DIRENTRY_TIME_LEN];
  bool is_today, is_yesterday;
  BLI_filelist_entry_datetime_to_string(
      nullptr, int64_t(stats.st_mtime), false, time_str, date_str, &is_today, &is_yesterday);
  if (is_today || is_yesterday) {
    STRNCPY(date_str, is_today ? TIP_("Today") : TIP_("Yesterday"));
  }

  /* Size. */
  char size_str[FILELIST_DIRENTRY_SIZE_LEN];
  BLI_filelist_entry_size_to_string(nullptr, uint64_t(stats.st_size), false, size_str);

  return fmt::format("{}\n\n{}: {} {}\n{}: {}",
                     filepath,
                     TIP_("Modified"),
                     date_str,
                     time_str,
                     TIP_("Size"),
                     size_str);
}

/* Currently fits in a pointer. */
struct FileRuntime {
  bool is_untrusted;
};
BLI_STATIC_ASSERT(sizeof(FileRuntime) <= sizeof(void *), "Struct must not exceed pointer size");

static bool wm_open_mainfile_check(bContext * /*C*/, wmOperator *op)
{
  FileRuntime *file_info = (FileRuntime *)&op->customdata;
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_scripts");
  bool is_untrusted = false;
  char filepath[FILE_MAX];
  char *lslash;

  RNA_string_get(op->ptr, "filepath", filepath);

  /* Get the directory. */
  lslash = (char *)BLI_path_slash_rfind(filepath);
  if (lslash) {
    *(lslash + 1) = '\0';
  }

  if ((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0) {
    if (BKE_autoexec_match(filepath) == true) {
      RNA_property_boolean_set(op->ptr, prop, false);
      is_untrusted = true;
    }
  }

  if (file_info) {
    file_info->is_untrusted = is_untrusted;
  }

  return is_untrusted;
}

static void wm_open_mainfile_ui(bContext * /*C*/, wmOperator *op)
{
  FileRuntime *file_info = (FileRuntime *)&op->customdata;
  uiLayout *layout = op->layout;
  const char *autoexec_text;

  layout->prop(op->ptr, "load_ui", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  uiLayout *col = &layout->column(false);
  if (file_info->is_untrusted) {
    autoexec_text = IFACE_("Trusted Source [Untrusted Path]");
    col->active_set(false);
    col->enabled_set(false);
  }
  else {
    autoexec_text = IFACE_("Trusted Source");
  }

  col->prop(op->ptr, "use_scripts", UI_ITEM_NONE, autoexec_text, ICON_NONE);
}

static void wm_open_mainfile_def_property_use_scripts(wmOperatorType *ot)
{
  RNA_def_boolean(ot->srna,
                  "use_scripts",
                  false,
                  "Trusted Source",
                  "Allow .blend file to execute scripts automatically, default available from "
                  "system preferences");
}

void WM_OT_open_mainfile(wmOperatorType *ot)
{
  ot->name = "Open";
  ot->idname = "WM_OT_open_mainfile";
  ot->description = "Open a Blender file";
  ot->get_description = wm_open_mainfile_get_description;

  ot->invoke = wm_open_mainfile_invoke;
  ot->exec = wm_open_mainfile_exec;
  ot->check = wm_open_mainfile_check;
  ot->ui = wm_open_mainfile_ui;
  /* Omit window poll so this can work in background mode. */

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  RNA_def_boolean(
      ot->srna, "load_ui", true, "Load UI", "Load user interface setup in the .blend file");

  wm_open_mainfile_def_property_use_scripts(ot);

  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "display_file_selector", true, "Display File Selector", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  create_operator_state(ot, OPEN_MAINFILE_STATE_DISCARD_CHANGES);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reload (revert) Main .blend File Operator
 * \{ */

static wmOperatorStatus wm_revert_mainfile_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent * /*event*/)
{
  std::string message = IFACE_("Any unsaved changes will be lost.");
  if (ED_image_should_save_modified(CTX_data_main(C))) {
    message += "\n";
    message += IFACE_("Warning: There are unsaved external image(s).");
  }

  return WM_operator_confirm_ex(C,
                                op,
                                IFACE_("Revert to the Saved File"),
                                message.c_str(),
                                IFACE_("Revert"),
                                ALERT_ICON_WARNING,
                                false);
}

static wmOperatorStatus wm_revert_mainfile_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  bool success;
  char filepath[FILE_MAX];

  const bool use_scripts_autoexec_check = wm_open_init_use_scripts(op, false);

  SET_FLAG_FROM_TEST(G.f, RNA_boolean_get(op->ptr, "use_scripts"), G_FLAG_SCRIPT_AUTOEXEC);

  STRNCPY(filepath, BKE_main_blendfile_path(bmain));
  success = wm_file_read_opwrap(C, filepath, use_scripts_autoexec_check, op->reports);

  if (success) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static bool wm_revert_mainfile_poll(bContext * /*C*/)
{
  const char *blendfile_path = BKE_main_blendfile_path_from_global();
  return (blendfile_path[0] != '\0');
}

void WM_OT_revert_mainfile(wmOperatorType *ot)
{
  ot->name = "Revert";
  ot->idname = "WM_OT_revert_mainfile";
  ot->description = "Reload the saved file";

  ot->invoke = wm_revert_mainfile_invoke;
  ot->exec = wm_revert_mainfile_exec;
  ot->poll = wm_revert_mainfile_poll;

  wm_open_mainfile_def_property_use_scripts(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recover Last Session Operator
 * \{ */

bool WM_file_recover_last_session(bContext *C,
                                  const bool use_scripts_autoexec_check,
                                  ReportList *reports)
{
  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), BKE_tempdir_base(), BLENDER_QUIT_FILE);
  G.fileflags |= G_FILE_RECOVER_READ;
  const bool success = wm_file_read_opwrap(C, filepath, use_scripts_autoexec_check, reports);
  G.fileflags &= ~G_FILE_RECOVER_READ;
  return success;
}

static wmOperatorStatus wm_recover_last_session_impl(bContext *C,
                                                     wmOperator *op,
                                                     const bool use_scripts_autoexec_check)
{
  SET_FLAG_FROM_TEST(G.f, RNA_boolean_get(op->ptr, "use_scripts"), G_FLAG_SCRIPT_AUTOEXEC);
  if (WM_file_recover_last_session(C, use_scripts_autoexec_check, op->reports)) {
    if (!G.background) {
      wmOperatorType *ot = op->type;
      PointerRNA *props_ptr = MEM_new<PointerRNA>(__func__);
      WM_operator_properties_create_ptr(props_ptr, ot);
      RNA_boolean_set(props_ptr, "use_scripts", true);
      wm_test_autorun_revert_action_set(ot, props_ptr);
    }
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus wm_recover_last_session_exec(bContext *C, wmOperator *op)
{
  const bool use_scripts_autoexec_check = wm_open_init_use_scripts(op, true);
  return wm_recover_last_session_impl(C, op, use_scripts_autoexec_check);
}

static void wm_recover_last_session_after_dialog_callback(bContext *C, void *user_data)
{
  WM_operator_name_call_with_properties(C,
                                        "WM_OT_recover_last_session",
                                        blender::wm::OpCallContext::ExecDefault,
                                        (IDProperty *)user_data,
                                        nullptr);
}

static wmOperatorStatus wm_recover_last_session_invoke(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent * /*event*/)
{
  /* Keep the current setting instead of using the preferences since a file selector
   * doesn't give us the option to change the setting. */
  const bool use_scripts_autoexec_check = wm_open_init_use_scripts(op, false);

  if (wm_operator_close_file_dialog_if_needed(
          C, op, wm_recover_last_session_after_dialog_callback))
  {
    return OPERATOR_INTERFACE;
  }
  return wm_recover_last_session_impl(C, op, use_scripts_autoexec_check);
}

void WM_OT_recover_last_session(wmOperatorType *ot)
{
  ot->name = "Recover Last Session";
  ot->idname = "WM_OT_recover_last_session";
  ot->description = "Open the last closed file (\"" BLENDER_QUIT_FILE "\")";

  ot->invoke = wm_recover_last_session_invoke;
  ot->exec = wm_recover_last_session_exec;

  wm_open_mainfile_def_property_use_scripts(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto-Save Main .blend File Operator
 * \{ */

static wmOperatorStatus wm_recover_auto_save_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  bool success;

  RNA_string_get(op->ptr, "filepath", filepath);
  BLI_path_canonicalize_native(filepath, sizeof(filepath));

  const bool use_scripts_autoexec_check = wm_open_init_use_scripts(op, true);
  SET_FLAG_FROM_TEST(G.f, RNA_boolean_get(op->ptr, "use_scripts"), G_FLAG_SCRIPT_AUTOEXEC);

  G.fileflags |= G_FILE_RECOVER_READ;

  success = wm_file_read_opwrap(C, filepath, use_scripts_autoexec_check, op->reports);

  G.fileflags &= ~G_FILE_RECOVER_READ;

  if (success) {
    if (!G.background) {
      wmOperatorType *ot = op->type;
      PointerRNA *props_ptr = MEM_new<PointerRNA>(__func__);
      WM_operator_properties_create_ptr(props_ptr, ot);
      RNA_boolean_set(props_ptr, "use_scripts", true);
      wm_test_autorun_revert_action_set(ot, props_ptr);
    }
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus wm_recover_auto_save_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent * /*event*/)
{
  char filepath[FILE_MAX];

  wm_autosave_location(filepath);
  RNA_string_set(op->ptr, "filepath", filepath);
  const bool use_scripts_autoexec_check = wm_open_init_use_scripts(op, true);
  UNUSED_VARS(use_scripts_autoexec_check); /* The user can set this in the UI. */
  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void WM_OT_recover_auto_save(wmOperatorType *ot)
{
  ot->name = "Recover Auto Save";
  ot->idname = "WM_OT_recover_auto_save";
  ot->description = "Open an automatically saved file to recover it";

  ot->invoke = wm_recover_auto_save_invoke;
  ot->exec = wm_recover_auto_save_exec;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_VERTICALDISPLAY,
                                 FILE_SORT_TIME);

  wm_open_mainfile_def_property_use_scripts(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Save Main .blend File Operator
 *
 * Both #WM_OT_save_as_mainfile & #WM_OT_save_mainfile.
 * \{ */

static void wm_filepath_default(const Main *bmain, char *filepath)
{
  if (bmain->filepath[0] == '\0') {
    char filename_untitled[FILE_MAXFILE];
    /* While a filename need not be UTF8, at this point the constructed name should be UTF8. */
    SNPRINTF_UTF8(filename_untitled, "%s.blend", DATA_("Untitled"));
    BLI_path_filename_ensure(filepath, FILE_MAX, filename_untitled);
  }
}

static void save_set_compress(wmOperator *op)
{
  PropertyRNA *prop;

  prop = RNA_struct_find_property(op->ptr, "compress");
  if (!RNA_property_is_set(op->ptr, prop)) {
    const char *blendfile_path = BKE_main_blendfile_path_from_global();
    if (blendfile_path[0] != '\0') { /* Keep flag for existing file. */
      RNA_property_boolean_set(op->ptr, prop, (G.fileflags & G_FILE_COMPRESS) != 0);
    }
    else { /* Use userdef for new file. */
      RNA_property_boolean_set(op->ptr, prop, (U.flag & USER_FILECOMPRESS) != 0);
    }
  }
}

static void save_set_filepath(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  PropertyRNA *prop;
  char filepath[FILE_MAX];

  prop = RNA_struct_find_property(op->ptr, "filepath");
  if (!RNA_property_is_set(op->ptr, prop)) {
    const char *blendfile_path = BKE_main_blendfile_path(bmain);
    /* If not saved before, get the name of the most recently used `.blend` file. */
    if ((blendfile_path[0] == '\0') && G.recent_files.first) {
      RecentFile *recent = static_cast<RecentFile *>(G.recent_files.first);
      STRNCPY(filepath, recent->filepath);
    }
    else {
      STRNCPY(filepath, blendfile_path);
    }

    /* For convenience when using "Save As" on asset system files:
     * Replace `.asset.blend` extension with just `.blend`.
     * Asset system files must not be overridden (except by the asset system),
     * there are further checks to prevent this entirely. */
    if (bmain->is_asset_edit_file &&
        blender::StringRef(filepath).endswith(BLENDER_ASSET_FILE_SUFFIX))
    {
      filepath[strlen(filepath) - strlen(BLENDER_ASSET_FILE_SUFFIX)] = '\0';
      BLI_path_extension_ensure(filepath, FILE_MAX, ".blend");
    }

    wm_filepath_default(bmain, filepath);
    RNA_property_string_set(op->ptr, prop, filepath);
  }
}

static wmOperatorStatus wm_save_as_mainfile_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent * /*event*/)
{

  save_set_compress(op);
  save_set_filepath(C, op);

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "relative_remap");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_boolean_set(op->ptr, prop, (U.flag & USER_RELPATHS));
  }

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* Function used for #WM_OT_save_mainfile too. */
static wmOperatorStatus wm_save_as_mainfile_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  char filepath[FILE_MAX];
  const bool is_save_as = (op->type->invoke == wm_save_as_mainfile_invoke);
  const bool use_save_as_copy = is_save_as && RNA_boolean_get(op->ptr, "copy");

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "incremental");
  const bool is_incremental = prop ? RNA_property_boolean_get(op->ptr, prop) : false;

  /* We could expose all options to the users however in most cases remapping
   * existing relative paths is a good default.
   * Users can manually make their paths relative & absolute if they wish. */
  const eBLO_WritePathRemap remap_mode = RNA_boolean_get(op->ptr, "relative_remap") ?
                                             BLO_WRITE_PATH_REMAP_RELATIVE :
                                             BLO_WRITE_PATH_REMAP_NONE;
  save_set_compress(op);

  const bool is_filepath_set = RNA_struct_property_is_set(op->ptr, "filepath");
  if (is_filepath_set) {
    RNA_string_get(op->ptr, "filepath", filepath);
    BLI_path_canonicalize_native(filepath, sizeof(filepath));
  }
  else {
    STRNCPY(filepath, BKE_main_blendfile_path(bmain));
  }

  if (filepath[0] == '\0') {
    BKE_report(op->reports,
               RPT_ERROR,
               "Unable to save an unsaved file with an empty or unset \"filepath\" property");
    return OPERATOR_CANCELLED;
  }

  if ((is_save_as == false) && is_incremental) {
    char head[FILE_MAXFILE], tail[FILE_MAXFILE];
    ushort digits;
    int num = BLI_path_sequence_decode(filepath, head, sizeof(head), tail, sizeof(tail), &digits);
    /* Numbers greater than INT_MAX return 0, resulting in always appending "1" to the name. */
    if (num == 0 && digits == 0) {
      /* This does nothing if there are no numbers at the end of the head. */
      BLI_str_rstrip_digits(head);
    }

    const int tries_limit = 1000;
    int tries = 0;
    bool in_use = true;
    do {
      num++;
      tries++;
      BLI_path_sequence_encode(filepath, sizeof(filepath), head, tail, digits, num);
      in_use = BLI_exists(filepath);
    } while (in_use && tries < tries_limit && num < INT_MAX);
    if (in_use) {
      BKE_report(op->reports, RPT_ERROR, "Unable to find an available incremented file name");
      return OPERATOR_CANCELLED;
    }
  }

  const int fileflags_orig = G.fileflags;
  int fileflags = G.fileflags;

  /* Set compression flag. */
  SET_FLAG_FROM_TEST(fileflags, RNA_boolean_get(op->ptr, "compress"), G_FILE_COMPRESS);

  const bool success = wm_file_write(
      C, filepath, fileflags, remap_mode, use_save_as_copy, op->reports);

  if ((op->flag & OP_IS_INVOKE) == 0) {
    /* OP_IS_INVOKE is set when the operator is called from the GUI.
     * If it is not set, the operator is called from a script and
     * shouldn't influence G.fileflags. */
    G.fileflags = fileflags_orig;
  }

  if (success == false) {
    return OPERATOR_CANCELLED;
  }

  const char *filename = BLI_path_basename(filepath);

  if (is_incremental) {
    BKE_reportf(op->reports, RPT_INFO, "Saved incremental as \"%s\"", filename);
  }
  else if (is_save_as) {
    /* use_save_as_copy depends upon is_save_as. */
    if (use_save_as_copy) {
      BKE_reportf(op->reports, RPT_INFO, "Saved copy as \"%s\"", filename);
    }
    else {
      BKE_reportf(op->reports, RPT_INFO, "Saved as \"%s\"", filename);
    }
  }
  else {
    BKE_reportf(op->reports, RPT_INFO, "Saved \"%s\"", filename);
  }

  if (!use_save_as_copy) {
    /* If saved file is the active one, there are technically no more compatibility issues, the
     * file on disk now matches the currently opened data version-wise. */
    bmain->has_forward_compatibility_issues = false;
    bmain->colorspace.is_missing_opencolorio_config = false;

    /* If saved file is the active one, notify WM so that saved status and window title can be
     * updated. */
    WM_event_add_notifier(C, NC_WM | ND_FILESAVE, nullptr);
    if (wmWindowManager *wm = CTX_wm_manager(C)) {
      /* Restart auto-save timer to avoid unnecessary unexpected freezing (because of auto-save)
       * when often saving manually. */
      wm_autosave_timer_end(wm);
      wm_autosave_timer_begin(wm);
      wm->autosave_scheduled = false;
    }
  }

  if (!is_save_as && RNA_boolean_get(op->ptr, "exit")) {
    wm_exit_schedule_delayed(C);
  }

  return OPERATOR_FINISHED;
}

static bool wm_save_mainfile_check(bContext * /*C*/, wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  if (!BKE_blendfile_extension_check(filepath)) {
    /* NOTE(@ideasman42): some users would prefer #BLI_path_extension_replace(),
     * we have had some nitpicking bug reports about this.
     * Always adding the extension as users may use '.' as part of the file-name. */
    BLI_path_extension_ensure(filepath, FILE_MAX, ".blend");
    RNA_string_set(op->ptr, "filepath", filepath);
    return true;
  }
  return false;
}

static std::string wm_save_as_mainfile_get_name(wmOperatorType *ot, PointerRNA *ptr)
{
  if (RNA_boolean_get(ptr, "copy")) {
    return CTX_IFACE_(ot->translation_context, "Save Copy");
  }
  return "";
}

static std::string wm_save_as_mainfile_get_description(bContext * /*C*/,
                                                       wmOperatorType * /*ot*/,
                                                       PointerRNA *ptr)
{
  if (RNA_boolean_get(ptr, "copy")) {
    return BLI_strdup(TIP_(
        "Save the current file in the desired location but do not make the saved file active"));
  }
  return "";
}

void WM_OT_save_as_mainfile(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Save As";
  ot->idname = "WM_OT_save_as_mainfile";
  ot->description = "Save the current file in the desired location";

  ot->invoke = wm_save_as_mainfile_invoke;
  ot->exec = wm_save_as_mainfile_exec;
  ot->get_name = wm_save_as_mainfile_get_name;
  ot->get_description = wm_save_as_mainfile_get_description;
  ot->check = wm_save_mainfile_check;
  /* Omit window poll so this can work in background mode. */

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
  RNA_def_boolean(ot->srna, "compress", false, "Compress", "Write compressed .blend file");
  RNA_def_boolean(ot->srna,
                  "relative_remap",
                  true,
                  "Remap Relative",
                  "Remap relative paths when saving to a different directory");
  prop = RNA_def_boolean(
      ot->srna,
      "copy",
      false,
      "Save Copy",
      "Save a copy of the actual working state but does not make saved file active");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static wmOperatorStatus wm_save_mainfile_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  wmOperatorStatus ret;

  /* Cancel if no active window. */
  if (CTX_wm_window(C) == nullptr) {
    return OPERATOR_CANCELLED;
  }

  save_set_compress(op);
  save_set_filepath(C, op);

  /* If we're saving for the first time and prefer relative paths -
   * any existing paths will be absolute,
   * enable the option to remap paths to avoid confusion, see: #37240. */
  const char *blendfile_path = BKE_main_blendfile_path_from_global();
  if ((blendfile_path[0] == '\0') && (U.flag & USER_RELPATHS)) {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "relative_remap");
    if (!RNA_property_is_set(op->ptr, prop)) {
      RNA_property_boolean_set(op->ptr, prop, true);
    }
  }

  if (blendfile_path[0] != '\0') {
    if (BKE_main_needs_overwrite_confirm(CTX_data_main(C))) {
      wm_save_file_overwrite_dialog(C, op);
      ret = OPERATOR_INTERFACE;
    }
    else {
      ret = wm_save_as_mainfile_exec(C, op);
    }
  }
  else {
    WM_event_add_fileselect(C, op);
    ret = OPERATOR_RUNNING_MODAL;
  }

  return ret;
}

static std::string wm_save_mainfile_get_description(bContext * /*C*/,
                                                    wmOperatorType * /*ot*/,
                                                    PointerRNA *ptr)
{
  if (RNA_boolean_get(ptr, "incremental")) {
    return TIP_(
        "Save the current Blender file with a numerically incremented name that does not "
        "overwrite any existing files");
  }
  return "";
}

void WM_OT_save_mainfile(wmOperatorType *ot)
{
  ot->name = "Save Blender File";
  ot->idname = "WM_OT_save_mainfile";
  ot->description = "Save the current Blender file";

  ot->invoke = wm_save_mainfile_invoke;
  ot->exec = wm_save_as_mainfile_exec;
  ot->check = wm_save_mainfile_check;
  ot->get_description = wm_save_mainfile_get_description;
  /* Omit window poll so this can work in background mode. */

  PropertyRNA *prop;
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
  RNA_def_boolean(ot->srna, "compress", false, "Compress", "Write compressed .blend file");
  RNA_def_boolean(ot->srna,
                  "relative_remap",
                  false,
                  "Remap Relative",
                  "Remap relative paths when saving to a different directory");

  prop = RNA_def_boolean(ot->srna, "exit", false, "Exit", "Exit Blender after saving");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "incremental",
                         false,
                         "Incremental",
                         "Save the current Blender file with a numerically incremented name that "
                         "does not overwrite any existing files");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Recent Files List Operator
 * \{ */

enum ClearRecentInclude { CLEAR_RECENT_ALL, CLEAR_RECENT_MISSING };

static const EnumPropertyItem prop_clear_recent_types[] = {
    {CLEAR_RECENT_ALL, "ALL", 0, "All Items", ""},
    {CLEAR_RECENT_MISSING, "MISSING", 0, "Items Not Found", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus wm_clear_recent_files_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event)
{
  return WM_operator_props_popup_confirm_ex(
      C, op, event, IFACE_("Clear Recent Files List"), IFACE_("Remove"));
}

static wmOperatorStatus wm_clear_recent_files_exec(bContext * /*C*/, wmOperator *op)
{
  ClearRecentInclude include = static_cast<ClearRecentInclude>(RNA_enum_get(op->ptr, "remove"));

  if (include == CLEAR_RECENT_ALL) {
    wm_history_files_free();
  }
  else if (include == CLEAR_RECENT_MISSING) {
    LISTBASE_FOREACH_MUTABLE (RecentFile *, recent, &G.recent_files) {
      if (!BLI_exists(recent->filepath)) {
        wm_history_file_free(recent);
      }
    }
  }

  wm_history_file_write();

  return OPERATOR_FINISHED;
}

static void wm_clear_recent_files_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  layout->separator();
  layout->prop(op->ptr, "remove", UI_ITEM_R_TOGGLE, std::nullopt, ICON_NONE);
  layout->separator();
}

void WM_OT_clear_recent_files(wmOperatorType *ot)
{
  ot->name = "Clear Recent Files List";
  ot->idname = "WM_OT_clear_recent_files";
  ot->description = "Clear the recent files list";

  ot->invoke = wm_clear_recent_files_invoke;
  ot->exec = wm_clear_recent_files_exec;
  ot->ui = wm_clear_recent_files_ui;

  /* flags */
  ot->flag = OPTYPE_REGISTER;

  /* props */
  ot->prop = RNA_def_enum(
      ot->srna, "remove", prop_clear_recent_types, CLEAR_RECENT_ALL, "Remove", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto Script Execution Warning Dialog
 * \{ */

static void wm_block_autorun_warning_ignore(bContext *C, void *arg_block, void * /*arg*/)
{
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, static_cast<uiBlock *>(arg_block));

  /* Free the data as it's no longer needed. */
  wm_test_autorun_revert_action_set(nullptr, nullptr);
}

static void wm_block_autorun_warning_reload_with_scripts(bContext *C, uiBlock *block)
{
  wmWindow *win = CTX_wm_window(C);

  UI_popup_block_close(C, win, block);

  /* Save user preferences for permanent execution. */
  if ((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0) {
    WM_operator_name_call(
        C, "WM_OT_save_userpref", blender::wm::OpCallContext::ExecDefault, nullptr, nullptr);
  }

  /* Load file again with scripts enabled.
   * The reload is necessary to allow scripts to run when the files loads. */
  wm_test_autorun_revert_action_exec(C);
}

static void wm_block_autorun_warning_enable_scripts(bContext *C, uiBlock *block)
{
  wmWindow *win = CTX_wm_window(C);
  Main *bmain = CTX_data_main(C);

  UI_popup_block_close(C, win, block);

  /* Save user preferences for permanent execution. */
  if ((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0) {
    WM_operator_name_call(
        C, "WM_OT_save_userpref", blender::wm::OpCallContext::ExecDefault, nullptr, nullptr);
  }

  /* Force a full refresh, but without reloading the file. */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    BKE_scene_free_depsgraph_hash(scene);
  }
}

/* Build the auto-run warning dialog UI. */
static uiBlock *block_create_autorun_warning(bContext *C, ARegion *region, void * /*arg1*/)
{
  const char *blendfile_path = BKE_main_blendfile_path_from_global();
  wmWindowManager *wm = CTX_wm_manager(C);

  uiBlock *block = UI_block_begin(
      C, region, "autorun_warning_popup", blender::ui::EmbossType::Emboss);
  UI_block_flag_enable(
      block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_LOOP | UI_BLOCK_NO_WIN_CLIP | UI_BLOCK_NUMSELECT);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
  UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);

  const char *title = RPT_(
      "For security reasons, automatic execution of Python scripts "
      "in this file was disabled:");
  const char *message = RPT_("This may lead to unexpected behavior");
  const char *checkbox_text = RPT_("Permanently allow execution of scripts");

  /* Measure strings to find the longest. */
  const uiStyle *style = UI_style_get_dpi();
  UI_fontstyle_set(&style->widget);
  int text_width = int(BLF_width(style->widget.uifont_id, title, BLF_DRAW_STR_DUMMY_MAX));
  text_width = std::max(text_width,
                        int(BLF_width(style->widget.uifont_id, message, BLF_DRAW_STR_DUMMY_MAX)));
  text_width = std::max(
      text_width,
      int(BLF_width(style->widget.uifont_id, checkbox_text, BLF_DRAW_STR_DUMMY_MAX) +
          (UI_SCALE_FAC * 25.0f)));

  const int dialog_width = std::max(int(400.0f * UI_SCALE_FAC),
                                    text_width + int(style->columnspace * 2.5));
  const short icon_size = 40 * UI_SCALE_FAC;
  uiLayout *layout = uiItemsAlertBox(
      block, style, dialog_width + icon_size, ALERT_ICON_ERROR, icon_size);

  /* Title and explanation text. */
  uiLayout *col = &layout->column(true);
  uiItemL_ex(col, title, ICON_NONE, true, false);
  uiItemL_ex(col, G.autoexec_fail, ICON_NONE, false, true);
  col->label(message, ICON_NONE);

  layout->separator();

  PointerRNA pref_ptr = RNA_pointer_create_discrete(nullptr, &RNA_PreferencesFilePaths, &U);
  layout->prop(&pref_ptr, "use_scripts_auto_execute", UI_ITEM_NONE, checkbox_text, ICON_NONE);

  layout->separator(2.0f);

  /* Buttons. */
  uiBut *but;
  uiLayout *split = &layout->split(0.0f, true);
  split->scale_y_set(1.2f);

  /* Empty space. */
  col = &split->column(false);
  col->separator();

  col = &split->column(false);

  /* Allow reload if we have a saved file.
   * Otherwise just enable scripts and reset the depsgraphs. */
  if ((blendfile_path[0] != '\0') && wm->file_saved) {
    but = uiDefIconTextBut(block,
                           ButType::But,
                           0,
                           ICON_NONE,
                           IFACE_("Allow Execution"),
                           0,
                           0,
                           50,
                           UI_UNIT_Y,
                           nullptr,
                           TIP_("Reload file with execution of Python scripts enabled"));
    UI_but_func_set(
        but, [block](bContext &C) { wm_block_autorun_warning_reload_with_scripts(&C, block); });
  }
  else {
    but = uiDefIconTextBut(block,
                           ButType::But,
                           0,
                           ICON_NONE,
                           IFACE_("Allow Execution"),
                           0,
                           0,
                           50,
                           UI_UNIT_Y,
                           nullptr,
                           TIP_("Enable scripts"));
    UI_but_func_set(but,
                    [block](bContext &C) { wm_block_autorun_warning_enable_scripts(&C, block); });
  }
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);

  col = &split->column(false);
  but = uiDefIconTextBut(block,
                         ButType::But,
                         0,
                         ICON_NONE,
                         IFACE_("Ignore"),
                         0,
                         0,
                         50,
                         UI_UNIT_Y,
                         nullptr,
                         TIP_("Continue using file without Python scripts"));
  UI_but_func_set(but, wm_block_autorun_warning_ignore, block, nullptr);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  UI_but_flag_enable(but, UI_BUT_ACTIVE_DEFAULT);

  UI_block_bounds_set_centered(block, 14 * UI_SCALE_FAC);

  return block;
}

/**
 * Store the action needed if the user needs to reload the file with Python scripts enabled.
 *
 * When left to nullptr, this is simply revert.
 * When loading files through the recover auto-save or session,
 * we need to revert using other operators.
 */
static struct {
  wmOperatorType *ot;
  PointerRNA *ptr;
} wm_test_autorun_revert_action_data = {nullptr, nullptr};

void wm_test_autorun_revert_action_set(wmOperatorType *ot, PointerRNA *ptr)
{
  BLI_assert(!G.background);
  wm_test_autorun_revert_action_data.ot = nullptr;
  if (wm_test_autorun_revert_action_data.ptr != nullptr) {
    WM_operator_properties_free(wm_test_autorun_revert_action_data.ptr);
    MEM_delete(wm_test_autorun_revert_action_data.ptr);
    wm_test_autorun_revert_action_data.ptr = nullptr;
  }
  wm_test_autorun_revert_action_data.ot = ot;
  wm_test_autorun_revert_action_data.ptr = ptr;
}

void wm_test_autorun_revert_action_exec(bContext *C)
{
  wmOperatorType *ot = wm_test_autorun_revert_action_data.ot;
  PointerRNA *ptr = wm_test_autorun_revert_action_data.ptr;

  /* Use regular revert. */
  if (ot == nullptr) {
    ot = WM_operatortype_find("WM_OT_revert_mainfile", false);
    ptr = MEM_new<PointerRNA>(__func__);
    WM_operator_properties_create_ptr(ptr, ot);
    RNA_boolean_set(ptr, "use_scripts", true);

    /* Set state, so it's freed correctly. */
    wm_test_autorun_revert_action_set(ot, ptr);
  }

  WM_operator_name_call_ptr(C, ot, blender::wm::OpCallContext::ExecDefault, ptr, nullptr);
  wm_test_autorun_revert_action_set(nullptr, nullptr);
}

void wm_test_autorun_warning(bContext *C)
{
  /* Test if any auto-execution of scripts failed. */
  if ((G.f & G_FLAG_SCRIPT_AUTOEXEC_FAIL) == 0) {
    return;
  }

  /* Only show the warning once. */
  if (G.f & G_FLAG_SCRIPT_AUTOEXEC_FAIL_QUIET) {
    return;
  }

  G.f |= G_FLAG_SCRIPT_AUTOEXEC_FAIL_QUIET;

  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = (wm->runtime->winactive) ? wm->runtime->winactive :
                                             static_cast<wmWindow *>(wm->windows.first);

  if (win) {
    /* We want this warning on the Main window, not a child window even if active. See #118765. */
    if (win->parent) {
      win = win->parent;
    }

    wmWindow *prevwin = CTX_wm_window(C);
    CTX_wm_window_set(C, win);
    UI_popup_block_invoke(C, block_create_autorun_warning, nullptr, nullptr);
    CTX_wm_window_set(C, prevwin);
  }
}

void wm_test_foreign_file_warning(bContext *C)
{
  if (!G_MAIN->is_read_invalid) {
    return;
  }

  G_MAIN->is_read_invalid = false;

  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = (wm->runtime->winactive) ? wm->runtime->winactive :
                                             static_cast<wmWindow *>(wm->windows.first);

  if (win) {
    /* We want this warning on the Main window, not a child window even if active. See #118765. */
    if (win->parent) {
      win = win->parent;
    }

    wmWindow *prevwin = CTX_wm_window(C);
    CTX_wm_window_set(C, win);
    UI_alert(C,
             RPT_("Unable to Load File"),
             RPT_("The file specified is not a valid Blend document."),
             ALERT_ICON_ERROR,
             false);

    CTX_wm_window_set(C, prevwin);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Save File Forward Compatibility Dialog
 * \{ */

static void free_post_file_close_action(void *arg)
{
  wmGenericCallback *action = (wmGenericCallback *)arg;
  WM_generic_callback_free(action);
}

static void wm_free_operator_properties_callback(void *user_data)
{
  IDProperty *properties = (IDProperty *)user_data;
  IDP_FreeProperty(properties);
}

static const char *save_file_overwrite_dialog_name = "save_file_overwrite_popup";

static void file_overwrite_detailed_info_show(uiLayout *parent_layout, Main *bmain)
{
  uiLayout *layout = &parent_layout->column(true);
  /* Trick to make both lines of text below close enough to look like they are part of a same
   * block. */
  layout->scale_y_set(0.70f);

  if (bmain->has_forward_compatibility_issues) {
    char writer_ver_str[16];
    char current_ver_str[16];
    if (bmain->versionfile == BLENDER_VERSION) {
      BKE_blender_version_blendfile_string_from_values(
          writer_ver_str, sizeof(writer_ver_str), bmain->versionfile, bmain->subversionfile);
      BKE_blender_version_blendfile_string_from_values(
          current_ver_str, sizeof(current_ver_str), BLENDER_FILE_VERSION, BLENDER_FILE_SUBVERSION);
    }
    else {
      BKE_blender_version_blendfile_string_from_values(
          writer_ver_str, sizeof(writer_ver_str), bmain->versionfile, -1);
      BKE_blender_version_blendfile_string_from_values(
          current_ver_str, sizeof(current_ver_str), BLENDER_VERSION, -1);
    }

    char message_line1[256];
    char message_line2[256];
    SNPRINTF(message_line1,
             RPT_("This file was saved by a newer version of Blender (%s)."),
             writer_ver_str);
    SNPRINTF(message_line2,
             RPT_("Saving it with this Blender (%s) may cause loss of data."),
             current_ver_str);
    layout->label(message_line1, ICON_NONE);
    layout->label(message_line2, ICON_NONE);
  }

  if (bmain->is_asset_edit_file) {
    if (bmain->has_forward_compatibility_issues) {
      layout->separator(1.4f);
    }

    layout->label(RPT_("This file is managed by the Blender asset system. It can only be"),
                  ICON_NONE);
    layout->label(RPT_("saved as a new, regular file."), ICON_NONE);
  }

  if (bmain->colorspace.is_missing_opencolorio_config) {
    if (bmain->is_asset_edit_file || bmain->has_forward_compatibility_issues) {
      layout->separator(1.4f);
    }
    layout->label(
        RPT_("Displays, views or color spaces in this file were missing and have been changed."),
        ICON_NONE);
    layout->label(RPT_("Saving it with this OpenColorIO configuration may cause loss of data."),
                  ICON_NONE);
  }
}

static void save_file_overwrite_cancel(bContext *C, void *arg_block, void * /*arg_data*/)
{
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, static_cast<uiBlock *>(arg_block));
}

static void save_file_overwrite_cancel_button(uiBlock *block, wmGenericCallback *post_action)
{
  uiBut *but = uiDefIconTextBut(
      block, ButType::But, 0, ICON_NONE, IFACE_("Cancel"), 0, 0, 0, UI_UNIT_Y, nullptr, "");
  UI_but_func_set(but, save_file_overwrite_cancel, block, post_action);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
}

static void save_file_overwrite_confirm(bContext *C, void *arg_block, void *arg_data)
{
  wmWindow *win = CTX_wm_window(C);

  /* Re-use operator properties as defined for the initial "Save" operator,
   * which triggered this "Forward Compatibility" popup. */
  wmGenericCallback *callback = WM_generic_callback_steal(
      static_cast<wmGenericCallback *>(arg_data));

  /* Needs to be done after stealing the callback data above, otherwise it would cause a
   * use-after-free. */
  UI_popup_block_close(C, win, static_cast<uiBlock *>(arg_block));

  PointerRNA operator_propptr = {};
  PointerRNA *operator_propptr_p = &operator_propptr;
  IDProperty *operator_idproperties = static_cast<IDProperty *>(callback->user_data);
  WM_operator_properties_alloc(&operator_propptr_p, &operator_idproperties, "WM_OT_save_mainfile");

  WM_operator_name_call(C,
                        "WM_OT_save_mainfile",
                        blender::wm::OpCallContext::ExecDefault,
                        operator_propptr_p,
                        nullptr);

  WM_generic_callback_free(callback);
}

static void save_file_overwrite_confirm_button(uiBlock *block, wmGenericCallback *post_action)
{
  uiBut *but = uiDefIconTextBut(
      block, ButType::But, 0, ICON_NONE, IFACE_("Overwrite"), 0, 0, 0, UI_UNIT_Y, nullptr, "");
  UI_but_func_set(but, save_file_overwrite_confirm, block, post_action);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  UI_but_flag_enable(but, UI_BUT_REDALERT);
}

static void save_file_overwrite_saveas(bContext *C, void *arg_block, void * /*arg_data*/)
{
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, static_cast<uiBlock *>(arg_block));

  WM_operator_name_call(
      C, "WM_OT_save_as_mainfile", blender::wm::OpCallContext::InvokeDefault, nullptr, nullptr);
}

static void save_file_overwrite_saveas_button(uiBlock *block, wmGenericCallback *post_action)
{
  uiBut *but = uiDefIconTextBut(
      block, ButType::But, 0, ICON_NONE, IFACE_("Save As..."), 0, 0, 0, UI_UNIT_Y, nullptr, "");
  UI_but_func_set(but, save_file_overwrite_saveas, block, post_action);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  UI_but_flag_enable(but, UI_BUT_ACTIVE_DEFAULT);
}

static uiBlock *block_create_save_file_overwrite_dialog(bContext *C, ARegion *region, void *arg1)
{
  wmGenericCallback *post_action = static_cast<wmGenericCallback *>(arg1);
  Main *bmain = CTX_data_main(C);

  uiBlock *block = UI_block_begin(
      C, region, save_file_overwrite_dialog_name, blender::ui::EmbossType::Emboss);
  UI_block_flag_enable(
      block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_LOOP | UI_BLOCK_NO_WIN_CLIP | UI_BLOCK_NUMSELECT);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  uiLayout *layout = uiItemsAlertBox(block, 44, ALERT_ICON_WARNING);

  /* Title. */
  if (bmain->has_forward_compatibility_issues) {
    if (bmain->is_asset_edit_file) {
      uiItemL_ex(layout,
                 RPT_("Cannot overwrite asset system files. Save as new file"),
                 ICON_NONE,
                 true,
                 false);
      uiItemL_ex(layout, RPT_("with an older Blender version?"), ICON_NONE, true, false);
    }
    else {
      uiItemL_ex(
          layout, RPT_("Overwrite file with an older Blender version?"), ICON_NONE, true, false);
    }
  }
  else if (bmain->is_asset_edit_file) {
    uiItemL_ex(layout,
               RPT_("Cannot overwrite asset system files. Save as new file?"),
               ICON_NONE,
               true,
               false);
  }
  else if (!bmain->colorspace.is_missing_opencolorio_config) {
    BLI_assert_unreachable();
  }

  if (bmain->colorspace.is_missing_opencolorio_config) {
    uiItemL_ex(layout,
               RPT_("Overwrite file with current OpenColorIO configuration?"),
               ICON_NONE,
               true,
               false);
  }

  /* Filename. */
  const char *blendfile_path = BKE_main_blendfile_path(CTX_data_main(C));
  char filename[FILE_MAX];
  if (blendfile_path[0] != '\0') {
    BLI_path_split_file_part(blendfile_path, filename, sizeof(filename));
  }
  else {
    /* While a filename need not be UTF8, at this point the constructed name should be UTF8. */
    SNPRINTF_UTF8(filename, "%s.blend", DATA_("Untitled"));
    /* Since this dialog should only be shown when re-saving an existing file, current filepath
     * should never be empty. */
    BLI_assert_unreachable();
  }
  layout->label(filename, ICON_NONE);

  /* Detailed message info. */
  file_overwrite_detailed_info_show(layout, bmain);

  layout->separator(4.0f);

  /* Buttons. */

  uiLayout *split = &layout->split(0.3f, true);
  split->scale_y_set(1.2f);

  split->column(false);
  /* Asset files don't actually allow overriding. */
  const bool allow_overwrite = !bmain->is_asset_edit_file;
  if (allow_overwrite) {
    save_file_overwrite_confirm_button(block, post_action);
  }

  uiLayout *split_right = &split->split(0.1f, true);

  split_right->column(false);
  /* Empty space. */

  split_right->column(false);
  save_file_overwrite_cancel_button(block, post_action);

  split_right->column(false);
  save_file_overwrite_saveas_button(block, post_action);

  UI_block_bounds_set_centered(block, 14 * UI_SCALE_FAC);
  return block;
}

void wm_save_file_overwrite_dialog(bContext *C, wmOperator *op)
{
  if (!UI_popup_block_name_exists(CTX_wm_screen(C), save_file_overwrite_dialog_name)) {
    wmGenericCallback *callback = MEM_callocN<wmGenericCallback>(__func__);
    callback->exec = nullptr;
    callback->user_data = IDP_CopyProperty(op->properties);
    callback->free_user_data = wm_free_operator_properties_callback;

    UI_popup_block_invoke(
        C, block_create_save_file_overwrite_dialog, callback, free_post_file_close_action);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Close File Dialog
 * \{ */

static char save_images_when_file_is_closed = true;

static void wm_block_file_close_cancel(bContext *C, void *arg_block, void * /*arg_data*/)
{
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, static_cast<uiBlock *>(arg_block));
}

static void wm_block_file_close_discard(bContext *C, void *arg_block, void *arg_data)
{
  wmGenericCallback *callback = WM_generic_callback_steal((wmGenericCallback *)arg_data);

  /* Close the popup before executing the callback. Otherwise
   * the popup might be closed by the callback, which will lead
   * to a crash. */
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, static_cast<uiBlock *>(arg_block));

  callback->exec(C, callback->user_data);
  WM_generic_callback_free(callback);
}

static void wm_block_file_close_save(bContext *C, void *arg_block, void *arg_data)
{
  const Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  wmGenericCallback *callback = WM_generic_callback_steal((wmGenericCallback *)arg_data);
  bool execute_callback = true;

  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, static_cast<uiBlock *>(arg_block));

  int modified_images_count = ED_image_save_all_modified_info(CTX_data_main(C), nullptr);
  if (modified_images_count > 0 && save_images_when_file_is_closed) {
    if (ED_image_should_save_modified(bmain)) {
      ReportList *reports = CTX_wm_reports(C);
      ED_image_save_all_modified(C, reports);
      WM_report_banner_show(wm, win);
    }
    else {
      execute_callback = false;
    }
  }

  bool file_has_been_saved_before = BKE_main_blendfile_path(bmain)[0] != '\0';

  if (file_has_been_saved_before) {
    if (bmain->has_forward_compatibility_issues || bmain->colorspace.is_missing_opencolorio_config)
    {
      /* Need to invoke to get the file-browser and choose where to save the new file.
       * This also makes it impossible to keep on going with current operation, which is why
       * callback cannot be executed anymore.
       *
       * This is the same situation as what happens when the file has never been saved before
       * (outer `else` statement, below). */
      WM_operator_name_call(C,
                            "WM_OT_save_as_mainfile",
                            blender::wm::OpCallContext::InvokeDefault,
                            nullptr,
                            nullptr);
      execute_callback = false;
    }
    else {
      const wmOperatorStatus status = WM_operator_name_call(
          C, "WM_OT_save_mainfile", blender::wm::OpCallContext::ExecDefault, nullptr, nullptr);
      if (status & OPERATOR_CANCELLED) {
        execute_callback = false;
      }
    }
  }
  else {
    WM_operator_name_call(
        C, "WM_OT_save_mainfile", blender::wm::OpCallContext::InvokeDefault, nullptr, nullptr);
    execute_callback = false;
  }

  if (execute_callback) {
    callback->exec(C, callback->user_data);
  }
  WM_generic_callback_free(callback);
}

static void wm_block_file_close_cancel_button(uiBlock *block, wmGenericCallback *post_action)
{
  uiBut *but = uiDefIconTextBut(
      block, ButType::But, 0, ICON_NONE, IFACE_("Cancel"), 0, 0, 0, UI_UNIT_Y, nullptr, "");
  UI_but_func_set(but, wm_block_file_close_cancel, block, post_action);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
}

static void wm_block_file_close_discard_button(uiBlock *block, wmGenericCallback *post_action)
{
  uiBut *but = uiDefIconTextBut(
      block, ButType::But, 0, ICON_NONE, IFACE_("Don't Save"), 0, 0, 0, UI_UNIT_Y, nullptr, "");
  UI_but_func_set(but, wm_block_file_close_discard, block, post_action);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
}

static void wm_block_file_close_save_button(uiBlock *block,
                                            wmGenericCallback *post_action,
                                            const bool needs_overwrite_confirm)
{
  uiBut *but = uiDefIconTextBut(
      block,
      ButType::But,
      0,
      ICON_NONE,
      /* Forward compatibility issues force using 'save as' operator instead of 'save' one. */
      needs_overwrite_confirm ? IFACE_("Save As...") : IFACE_("Save"),
      0,
      0,
      0,
      UI_UNIT_Y,
      nullptr,
      "");
  UI_but_func_set(but, wm_block_file_close_save, block, post_action);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  UI_but_flag_enable(but, UI_BUT_ACTIVE_DEFAULT);
}

static const char *close_file_dialog_name = "file_close_popup";

static void save_catalogs_when_file_is_closed_set_fn(bContext * /*C*/, void *arg1, void * /*arg2*/)
{
  char *save_catalogs_when_file_is_closed = static_cast<char *>(arg1);
  blender::ed::asset::catalogs_set_save_catalogs_when_file_is_saved(
      *save_catalogs_when_file_is_closed != 0);
}

static uiBlock *block_create__close_file_dialog(bContext *C, ARegion *region, void *arg1)
{
  using namespace blender;
  wmGenericCallback *post_action = (wmGenericCallback *)arg1;
  Main *bmain = CTX_data_main(C);

  uiBlock *block = UI_block_begin(
      C, region, close_file_dialog_name, blender::ui::EmbossType::Emboss);
  UI_block_flag_enable(
      block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_LOOP | UI_BLOCK_NO_WIN_CLIP | UI_BLOCK_NUMSELECT);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  uiLayout *layout = uiItemsAlertBox(
      block, (bmain->colorspace.is_missing_opencolorio_config) ? 44 : 34, ALERT_ICON_QUESTION);

  const bool needs_overwrite_confirm = BKE_main_needs_overwrite_confirm(bmain);

  /* Title. */
  uiItemL_ex(layout, RPT_("Save changes before closing?"), ICON_NONE, true, false);

  /* Filename. */
  const char *blendfile_path = BKE_main_blendfile_path(CTX_data_main(C));
  char filename[FILE_MAX];
  if (blendfile_path[0] != '\0') {
    BLI_path_split_file_part(blendfile_path, filename, sizeof(filename));
  }
  else {
    /* While a filename need not be UTF8, at this point the constructed name should be UTF8. */
    SNPRINTF_UTF8(filename, "%s.blend", DATA_("Untitled"));
  }
  layout->label(filename, ICON_NONE);

  /* Potential forward compatibility issues message. */
  if (needs_overwrite_confirm) {
    file_overwrite_detailed_info_show(layout, bmain);
  }

  /* Image Saving Warnings. */
  ReportList reports;
  BKE_reports_init(&reports, RPT_STORE);
  uint modified_images_count = ED_image_save_all_modified_info(bmain, &reports);

  LISTBASE_FOREACH (Report *, report, &reports.list) {
    uiLayout *row = &layout->column(false);
    row->scale_y_set(0.6f);
    row->separator();

    /* Error messages created in ED_image_save_all_modified_info() can be long,
     * but are made to separate into two parts at first colon between text and paths.
     */
    char *message = BLI_strdupn(report->message, report->len);
    char *path_info = strstr(message, ": ");
    if (path_info) {
      /* Terminate message string at colon. */
      path_info[1] = '\0';
      /* Skip over the ": ". */
      path_info += 2;
    }
    uiItemL_ex(row, message, ICON_NONE, false, true);
    if (path_info) {
      uiItemL_ex(row, path_info, ICON_NONE, false, true);
    }
    MEM_freeN(message);
  }

  /* Used to determine if extra separators are needed. */
  bool has_extra_checkboxes = false;

  /* Modified Images Checkbox. */
  if (modified_images_count > 0) {
    char message[64];
    SNPRINTF(message, RPT_("Save %u modified image(s)"), modified_images_count);
    /* Only the first checkbox should get extra separation. */
    if (!has_extra_checkboxes) {
      layout->separator();
    }
    uiDefButBitC(block,
                 ButType::Checkbox,
                 1,
                 0,
                 message,
                 0,
                 0,
                 0,
                 UI_UNIT_Y,
                 &save_images_when_file_is_closed,
                 0,
                 0,
                 "");
    has_extra_checkboxes = true;
  }

  if (AS_asset_library_has_any_unsaved_catalogs()) {
    static char save_catalogs_when_file_is_closed;

    save_catalogs_when_file_is_closed = ed::asset::catalogs_get_save_catalogs_when_file_is_saved();

    /* Only the first checkbox should get extra separation. */
    if (!has_extra_checkboxes) {
      layout->separator();
    }
    uiBut *but = uiDefButBitC(block,
                              ButType::Checkbox,
                              1,
                              0,
                              "Save modified asset catalogs",
                              0,
                              0,
                              0,
                              UI_UNIT_Y,
                              &save_catalogs_when_file_is_closed,
                              0,
                              0,
                              "");
    UI_but_func_set(but,
                    save_catalogs_when_file_is_closed_set_fn,
                    &save_catalogs_when_file_is_closed,
                    nullptr);
    has_extra_checkboxes = true;
  }

  BKE_reports_free(&reports);

  layout->separator(2.0f);

  /* Buttons. */
#ifdef _WIN32
  const bool windows_layout = true;
#else
  const bool windows_layout = false;
#endif

  if (windows_layout) {
    /* Windows standard layout. */

    uiLayout *split = &layout->split(0.0f, true);
    split->scale_y_set(1.2f);

    split->column(false);
    wm_block_file_close_save_button(block, post_action, needs_overwrite_confirm);

    split->column(false);
    wm_block_file_close_discard_button(block, post_action);

    split->column(false);
    wm_block_file_close_cancel_button(block, post_action);
  }
  else {
    /* Non-Windows layout (macOS and Linux). */

    uiLayout *split = &layout->split(0.3f, true);
    split->scale_y_set(1.2f);

    split->column(false);
    wm_block_file_close_discard_button(block, post_action);

    uiLayout *split_right = &split->split(0.1f, true);

    split_right->column(false);
    /* Empty space. */

    split_right->column(false);
    wm_block_file_close_cancel_button(block, post_action);

    split_right->column(false);
    wm_block_file_close_save_button(block, post_action, needs_overwrite_confirm);
  }

  UI_block_bounds_set_centered(block, 14 * UI_SCALE_FAC);
  return block;
}

void wm_close_file_dialog(bContext *C, wmGenericCallback *post_action)
{
  if (!UI_popup_block_name_exists(CTX_wm_screen(C), close_file_dialog_name)) {
    UI_popup_block_invoke(
        C, block_create__close_file_dialog, post_action, free_post_file_close_action);
  }
  else {
    WM_generic_callback_free(post_action);
  }
}

bool wm_operator_close_file_dialog_if_needed(bContext *C,
                                             wmOperator *op,
                                             wmGenericCallbackFn post_action_fn)
{
  if (U.uiflag & USER_SAVE_PROMPT &&
      wm_file_or_session_data_has_unsaved_changes(CTX_data_main(C), CTX_wm_manager(C)))
  {
    wmGenericCallback *callback = MEM_callocN<wmGenericCallback>(__func__);
    callback->exec = post_action_fn;
    callback->user_data = IDP_CopyProperty(op->properties);
    callback->free_user_data = wm_free_operator_properties_callback;
    wm_close_file_dialog(C, callback);
    return true;
  }

  return false;
}

/** \} */
