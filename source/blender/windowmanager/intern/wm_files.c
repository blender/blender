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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 *
 * User level access for blend file read/write, file-history and user-preferences
 * (including relevant operators).
 */

/* placed up here because of crappy
 * winsock stuff.
 */
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "zlib.h" /* wm_read_exotic() */

#ifdef WIN32
/* Need to include windows.h so _WIN32_IE is defined. */
#  include <windows.h>
#  ifndef _WIN32_IE
/* Minimal requirements for SHGetSpecialFolderPath on MINGW MSVC has this defined already. */
#    define _WIN32_IE 0x0400
#  endif
/* For SHGetSpecialFolderPath, has to be done before BLI_winstuff
 * because 'near' is disabled through BLI_windstuff */
#  include <shlobj.h>
#  include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"
#include "MEM_CacheLimiterC-Api.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"
#include "BLI_threads.h"
#include "BLI_callbacks.h"
#include "BLI_system.h"
#include BLI_SYSTEM_PID_H

#include "BLT_translation.h"

#include "BLF_api.h"

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BKE_appdir.h"
#include "BKE_autoexec.h"
#include "BKE_blender.h"
#include "BKE_blendfile.h"
#include "BKE_blender_undo.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_report.h"
#include "BKE_sound.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_undo_system.h"
#include "BKE_workspace.h"

#include "BLO_readfile.h"
#include "BLO_writefile.h"
#include "BLO_undofile.h" /* to save from an undo memfile */

#include "RNA_access.h"
#include "RNA_define.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "ED_datafiles.h"
#include "ED_fileselect.h"
#include "ED_image.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_util.h"
#include "ED_undo.h"

#include "GHOST_C-api.h"
#include "GHOST_Path-api.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_draw.h"

/* only to report a missing engine */
#include "RE_engine.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"
#include "WM_toolsystem.h"

#include "wm.h"
#include "wm_files.h"
#include "wm_window.h"
#include "wm_event_system.h"

static RecentFile *wm_file_history_find(const char *filepath);
static void wm_history_file_free(RecentFile *recent);
static void wm_history_file_update(void);
static void wm_history_file_write(void);

/* To be able to read files without windows closing, opening, moving
 * we try to prepare for worst case:
 * - active window gets active screen from file
 * - restoring the screens from non-active windows
 * Best case is all screens match, in that case they get assigned to proper window
 */
static void wm_window_match_init(bContext *C, ListBase *wmlist)
{
  wmWindowManager *wm;
  wmWindow *win, *active_win;

  *wmlist = G_MAIN->wm;
  BLI_listbase_clear(&G_MAIN->wm);

  active_win = CTX_wm_window(C);

  /* first wrap up running stuff */
  /* code copied from wm_init_exit.c */
  for (wm = wmlist->first; wm; wm = wm->id.next) {

    WM_jobs_kill_all(wm);

    for (win = wm->windows.first; win; win = win->next) {

      CTX_wm_window_set(C, win); /* needed by operator close callbacks */
      WM_event_remove_handlers(C, &win->handlers);
      WM_event_remove_handlers(C, &win->modalhandlers);
      ED_screen_exit(C, win, WM_window_get_active_screen(win));
    }
  }

  /* reset active window */
  CTX_wm_window_set(C, active_win);

  /* XXX Hack! We have to clear context menu here, because removing all modalhandlers
   * above frees the active menu (at least, in the 'startup splash' case),
   * causing use-after-free error in later handling of the button callbacks in UI code
   * (see ui_apply_but_funcs_after()).
   * Tried solving this by always NULL-ing context's menu when setting wm/win/etc.,
   * but it broke popups refreshing (see T47632),
   * so for now just handling this specific case here. */
  CTX_wm_menu_set(C, NULL);

  ED_editors_exit(G_MAIN, true);
}

static void wm_window_substitute_old(wmWindowManager *oldwm,
                                     wmWindowManager *wm,
                                     wmWindow *oldwin,
                                     wmWindow *win)
{
  win->ghostwin = oldwin->ghostwin;
  win->gpuctx = oldwin->gpuctx;
  win->active = oldwin->active;
  if (win->active) {
    wm->winactive = win;
  }
  if (oldwm->windrawable == oldwin) {
    oldwm->windrawable = NULL;
    wm->windrawable = win;
  }

  /* File loading in background mode still calls this. */
  if (!G.background) {
    /* Pointer back. */
    GHOST_SetWindowUserData(win->ghostwin, win);
  }

  oldwin->ghostwin = NULL;
  oldwin->gpuctx = NULL;

  win->eventstate = oldwin->eventstate;
  oldwin->eventstate = NULL;

  /* ensure proper screen rescaling */
  win->sizex = oldwin->sizex;
  win->sizey = oldwin->sizey;
  win->posx = oldwin->posx;
  win->posy = oldwin->posy;
}

static void wm_window_match_keep_current_wm(const bContext *C,
                                            ListBase *current_wm_list,
                                            const bool load_ui,
                                            ListBase *r_new_wm_list)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = current_wm_list->first;
  bScreen *screen = NULL;

  /* match oldwm to new dbase, only old files */
  wm->initialized &= ~WM_WINDOW_IS_INITIALIZED;

  /* when loading without UI, no matching needed */
  if (load_ui && (screen = CTX_wm_screen(C))) {
    for (wmWindow *win = wm->windows.first; win; win = win->next) {
      WorkSpace *workspace;

      BKE_workspace_layout_find_global(bmain, screen, &workspace);
      BKE_workspace_active_set(win->workspace_hook, workspace);
      win->scene = CTX_data_scene(C);

      /* all windows get active screen from file */
      if (screen->winid == 0) {
        WM_window_set_active_screen(win, workspace, screen);
      }
      else {
        WorkSpaceLayout *layout_old = WM_window_get_active_layout(win);
        WorkSpaceLayout *layout_new = ED_workspace_layout_duplicate(
            bmain, workspace, layout_old, win);

        WM_window_set_active_layout(win, workspace, layout_new);
      }

      bScreen *win_screen = WM_window_get_active_screen(win);
      win_screen->winid = win->winid;
    }
  }

  *r_new_wm_list = *current_wm_list;
}

static void wm_window_match_replace_by_file_wm(bContext *C,
                                               ListBase *current_wm_list,
                                               ListBase *readfile_wm_list,
                                               ListBase *r_new_wm_list)
{
  wmWindowManager *oldwm = current_wm_list->first;
  wmWindowManager *wm = readfile_wm_list->first; /* will become our new WM */
  bool has_match = false;

  /* this code could move to setup_appdata */

  /* preserve key configurations in new wm, to preserve their keymaps */
  wm->keyconfigs = oldwm->keyconfigs;
  wm->addonconf = oldwm->addonconf;
  wm->defaultconf = oldwm->defaultconf;
  wm->userconf = oldwm->userconf;

  BLI_listbase_clear(&oldwm->keyconfigs);
  oldwm->addonconf = NULL;
  oldwm->defaultconf = NULL;
  oldwm->userconf = NULL;

  /* ensure making new keymaps and set space types */
  wm->initialized = 0;
  wm->winactive = NULL;

  /* Clearing drawable of before deleting any context
   * to avoid clearing the wrong wm. */
  wm_window_clear_drawable(oldwm);

  /* only first wm in list has ghostwins */
  for (wmWindow *win = wm->windows.first; win; win = win->next) {
    for (wmWindow *oldwin = oldwm->windows.first; oldwin; oldwin = oldwin->next) {
      if (oldwin->winid == win->winid) {
        has_match = true;

        wm_window_substitute_old(oldwm, wm, oldwin, win);
      }
    }
  }
  /* make sure at least one window is kept open so we don't lose the context, check T42303 */
  if (!has_match) {
    wm_window_substitute_old(oldwm, wm, oldwm->windows.first, wm->windows.first);
  }

  wm_close_and_free_all(C, current_wm_list);

  *r_new_wm_list = *readfile_wm_list;
}

/**
 * Match old WM with new, 4 cases:
 * 1) No current WM, no WM in file: Make new default.
 * 2) No current WM, but WM in file: Keep current WM, do nothing else.
 * 3) Current WM, but not in file: Keep current WM, update windows with screens from file.
 * 4) Current WM, and WM in file: Try to keep current GHOST windows, use WM from file.
 *
 * \param r_new_wm_list: Return argument for the wm list to be used from now on.
 */
static void wm_window_match_do(bContext *C,
                               ListBase *current_wm_list,
                               ListBase *readfile_wm_list,
                               ListBase *r_new_wm_list)
{
  if (BLI_listbase_is_empty(current_wm_list)) {
    /* case 1 */
    if (BLI_listbase_is_empty(readfile_wm_list)) {
      Main *bmain = CTX_data_main(C);
      /* Neither current, no newly read file have a WM -> add the default one. */
      wm_add_default(bmain, C);
      *r_new_wm_list = bmain->wm;
    }
    /* case 2 */
    else {
      *r_new_wm_list = *readfile_wm_list;
    }
  }
  else {
    /* case 3 */
    if (BLI_listbase_is_empty(readfile_wm_list)) {
      /* We've read file without wm, keep current one entirely alive.
       * Happens when reading pre 2.5 files (no WM back then) */
      wm_window_match_keep_current_wm(
          C, current_wm_list, (G.fileflags & G_FILE_NO_UI) == 0, r_new_wm_list);
    }
    /* case 4 */
    else {
      wm_window_match_replace_by_file_wm(C, current_wm_list, readfile_wm_list, r_new_wm_list);
    }
  }
}

/* in case UserDef was read, we re-initialize all, and do versioning */
static void wm_init_userdef(Main *bmain)
{
  /* versioning is here */
  UI_init_userdef(bmain);

  /* needed so loading a file from the command line respects user-pref [#26156] */
  SET_FLAG_FROM_TEST(G.fileflags, U.flag & USER_FILENOUI, G_FILE_NO_UI);

  /* set the python auto-execute setting from user prefs */
  /* enabled by default, unless explicitly enabled in the command line which overrides */
  if ((G.f & G_FLAG_SCRIPT_OVERRIDE_PREF) == 0) {
    SET_FLAG_FROM_TEST(G.f, (U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0, G_FLAG_SCRIPT_AUTOEXEC);
  }

  MEM_CacheLimiter_set_maximum(((size_t)U.memcachelimit) * 1024 * 1024);
  BKE_sound_init(bmain);

  /* update tempdir from user preferences */
  BKE_tempdir_init(U.tempdir);
}

/* return codes */
#define BKE_READ_EXOTIC_FAIL_PATH -3   /* file format is not supported */
#define BKE_READ_EXOTIC_FAIL_FORMAT -2 /* file format is not supported */
#define BKE_READ_EXOTIC_FAIL_OPEN -1   /* Can't open the file */
#define BKE_READ_EXOTIC_OK_BLEND 0     /* .blend file */
#if 0
#  define BKE_READ_EXOTIC_OK_OTHER 1 /* other supported formats */
#endif

/* intended to check for non-blender formats but for now it only reads blends */
static int wm_read_exotic(const char *name)
{
  int len;
  gzFile gzfile;
  char header[7];
  int retval;

  /* make sure we're not trying to read a directory.... */

  len = strlen(name);
  if (len > 0 && ELEM(name[len - 1], '/', '\\')) {
    retval = BKE_READ_EXOTIC_FAIL_PATH;
  }
  else {
    gzfile = BLI_gzopen(name, "rb");
    if (gzfile == NULL) {
      retval = BKE_READ_EXOTIC_FAIL_OPEN;
    }
    else {
      len = gzread(gzfile, header, sizeof(header));
      gzclose(gzfile);
      if (len == sizeof(header) && STREQLEN(header, "BLENDER", 7)) {
        retval = BKE_READ_EXOTIC_OK_BLEND;
      }
      else {
        /* We may want to support loading other file formats
         * from their header bytes or file extension.
         * This used to be supported in the code below and may be added
         * back at some point. */
#if 0
        WM_cursor_wait(true);

        if (is_foo_format(name)) {
          read_foo(name);
          retval = BKE_READ_EXOTIC_OK_OTHER;
        }
        else
#endif
        {
          retval = BKE_READ_EXOTIC_FAIL_FORMAT;
        }
#if 0
        WM_cursor_wait(false);
#endif
      }
    }
  }

  return retval;
}

void WM_file_autoexec_init(const char *filepath)
{
  if (G.f & G_FLAG_SCRIPT_OVERRIDE_PREF) {
    return;
  }

  if (G.f & G_FLAG_SCRIPT_AUTOEXEC) {
    char path[FILE_MAX];
    BLI_split_dir_part(filepath, path, sizeof(path));
    if (BKE_autoexec_match(path)) {
      G.f &= ~G_FLAG_SCRIPT_AUTOEXEC;
    }
  }
}

void wm_file_read_report(bContext *C, Main *bmain)
{
  ReportList *reports = NULL;
  Scene *sce;

  for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
    if (sce->r.engine[0] &&
        BLI_findstring(&R_engines, sce->r.engine, offsetof(RenderEngineType, idname)) == NULL) {
      if (reports == NULL) {
        reports = CTX_wm_reports(C);
      }

      BKE_reportf(reports,
                  RPT_ERROR,
                  "Engine '%s' not available for scene '%s' (an add-on may need to be installed "
                  "or enabled)",
                  sce->r.engine,
                  sce->id.name + 2);
    }
  }

  if (reports) {
    if (!G.background) {
      WM_report_banner_show();
    }
  }
}

/**
 * Logic shared between #WM_file_read & #wm_homefile_read,
 * updates to make after reading a file.
 */
static void wm_file_read_post(bContext *C,
                              const bool is_startup_file,
                              const bool is_factory_startup,
                              const bool use_data,
                              const bool use_userdef,
                              const bool reset_app_template)
{
  bool addons_loaded = false;
  wmWindowManager *wm = CTX_wm_manager(C);

  if (use_data) {
    if (!G.background) {
      /* remove windows which failed to be added via WM_check */
      wm_window_ghostwindows_remove_invalid(C, wm);
    }
    CTX_wm_window_set(C, wm->windows.first);
  }

#ifdef WITH_PYTHON
  if (is_startup_file) {
    /* possible python hasn't been initialized */
    if (CTX_py_init_get(C)) {
      if (reset_app_template) {
        /* Only run when we have a template path found. */
        if (BKE_appdir_app_template_any()) {
          BPY_execute_string(
              C, (const char *[]){"bl_app_template_utils", NULL}, "bl_app_template_utils.reset()");
        }
        /* sync addons, these may have changed from the defaults */
        BPY_execute_string(C, (const char *[]){"addon_utils", NULL}, "addon_utils.reset_all()");
      }
      if (use_data) {
        BPY_python_reset(C);
      }
      addons_loaded = true;
    }
  }
  else {
    /* run any texts that were loaded in and flagged as modules */
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
      BLI_callback_exec(bmain, NULL, BLI_CB_EVT_LOAD_FACTORY_USERDEF_POST);
    }
  }

  if (use_data) {
    /* important to do before NULL'ing the context */
    BLI_callback_exec(bmain, NULL, BLI_CB_EVT_VERSION_UPDATE);
    BLI_callback_exec(bmain, NULL, BLI_CB_EVT_LOAD_POST);
    if (is_factory_startup) {
      BLI_callback_exec(bmain, NULL, BLI_CB_EVT_LOAD_FACTORY_STARTUP_POST);
    }
  }

  if (use_data) {
    WM_operatortype_last_properties_clear_all();

    /* After load post, so for example the driver namespace can be filled
     * before evaluating the depsgraph. */
    wm_event_do_depsgraph(C, true);

    ED_editors_init(C);

#if 1
    WM_event_add_notifier(C, NC_WM | ND_FILEREAD, NULL);
#else
    WM_msg_publish_static(CTX_wm_message_bus(C), WM_MSG_STATICTYPE_FILE_READ);
#endif
  }

  /* report any errors.
   * currently disabled if addons aren't yet loaded */
  if (addons_loaded) {
    wm_file_read_report(C, bmain);
  }

  if (use_data) {
    if (!G.background) {
      if (wm->undo_stack == NULL) {
        wm->undo_stack = BKE_undosys_stack_create();
      }
      else {
        BKE_undosys_stack_clear(wm->undo_stack);
      }
      BKE_undosys_stack_init_from_main(wm->undo_stack, bmain);
      BKE_undosys_stack_init_from_context(wm->undo_stack, C);
    }
  }

  if (use_data) {
    if (!G.background) {
      /* in background mode this makes it hard to load
       * a blend file and do anything since the screen
       * won't be set to a valid value again */
      CTX_wm_window_set(C, NULL); /* exits queues */

      /* Ensure tools are registered. */
      WM_toolsystem_init(C);
    }
  }
}

bool WM_file_read(bContext *C, const char *filepath, ReportList *reports)
{
  /* assume automated tasks with background, don't write recent file list */
  const bool do_history = (G.background == false) && (CTX_wm_manager(C)->op_undo_depth == 0);
  bool success = false;

  /* so we can get the error message */
  errno = 0;

  WM_cursor_wait(1);

  BLI_callback_exec(CTX_data_main(C), NULL, BLI_CB_EVT_LOAD_PRE);

  UI_view2d_zoom_cache_reset();

  /* first try to append data from exotic file formats... */
  /* it throws error box when file doesn't exist and returns -1 */
  /* note; it should set some error message somewhere... (ton) */
  const int retval = wm_read_exotic(filepath);

  /* we didn't succeed, now try to read Blender file */
  if (retval == BKE_READ_EXOTIC_OK_BLEND) {
    const int G_f_orig = G.f;
    ListBase wmbase;

    /* put aside screens to match with persistent windows later */
    /* also exit screens and editors */
    wm_window_match_init(C, &wmbase);

    /* confusing this global... */
    G.relbase_valid = 1;
    success = BKE_blendfile_read(
        C,
        filepath,
        /* Loading preferences when the user intended to load a regular file is a security risk,
         * because the excluded path list is also loaded.
         * Further it's just confusing if a user loads a file and various preferences change. */
        &(const struct BlendFileReadParams){
            .is_startup = false,
            .skip_flags = BLO_READ_SKIP_USERDEF,
        },
        reports);

    /* BKE_file_read sets new Main into context. */
    Main *bmain = CTX_data_main(C);

    /* when loading startup.blend's, we can be left with a blank path */
    if (BKE_main_blendfile_path(bmain)[0] != '\0') {
      G.save_over = 1;
    }
    else {
      G.save_over = 0;
      G.relbase_valid = 0;
    }

    /* this flag is initialized by the operator but overwritten on read.
     * need to re-enable it here else drivers + registered scripts wont work. */
    if (G.f != G_f_orig) {
      const int flags_keep = G_FLAG_ALL_RUNTIME;
      G.f &= G_FLAG_ALL_READFILE;
      G.f = (G.f & ~flags_keep) | (G_f_orig & flags_keep);
    }

    /* match the read WM with current WM */
    wm_window_match_do(C, &wmbase, &bmain->wm, &bmain->wm);
    WM_check(C); /* opens window(s), checks keymaps */

    if (success) {
      if (do_history) {
        wm_history_file_update();
      }
    }

    const bool use_data = true;
    const bool use_userdef = false;
    wm_file_read_post(C, false, false, use_data, use_userdef, false);
  }
#if 0
  else if (retval == BKE_READ_EXOTIC_OK_OTHER) {
    BKE_undo_write(C, "Import file");
  }
#endif
  else if (retval == BKE_READ_EXOTIC_FAIL_OPEN) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot read file '%s': %s",
                filepath,
                errno ? strerror(errno) : TIP_("unable to open the file"));
  }
  else if (retval == BKE_READ_EXOTIC_FAIL_FORMAT) {
    BKE_reportf(reports, RPT_ERROR, "File format is not supported in file '%s'", filepath);
  }
  else if (retval == BKE_READ_EXOTIC_FAIL_PATH) {
    BKE_reportf(reports, RPT_ERROR, "File path '%s' invalid", filepath);
  }
  else {
    BKE_reportf(reports, RPT_ERROR, "Unknown error loading '%s'", filepath);
    BLI_assert(!"invalid 'retval'");
  }

  if (success == false) {
    /* remove from recent files list */
    if (do_history) {
      RecentFile *recent = wm_file_history_find(filepath);
      if (recent) {
        wm_history_file_free(recent);
        wm_history_file_write();
      }
    }
  }

  WM_cursor_wait(0);

  return success;
}

static struct {
  char app_template[64];
  bool override;
} wm_init_state_app_template = {{0}};

/**
 * Used for setting app-template from the command line:
 * - non-empty string: overrides.
 * - empty string: override, using no app template.
 * - NULL: clears override.
 */
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

const char *WM_init_state_app_template_get(void)
{
  return wm_init_state_app_template.override ? wm_init_state_app_template.app_template : NULL;
}

/**
 * Called on startup, (context entirely filled with NULLs)
 * or called for 'New File' both startup.blend and userpref.blend are checked.
 *
 * \param use_factory_settings:
 * Ignore on-disk startup file, use bundled ``datatoc_startup_blend`` instead.
 * Used for "Restore Factory Settings".
 *
 * \param use_userdef: Load factory settings as well as startup file.
 * Disabled for "File New" we don't want to reload preferences.
 *
 * \param filepath_startup_override:
 * Optional path pointing to an alternative blend file (may be NULL).
 *
 * \param app_template_override:
 * Template to use instead of the template defined in user-preferences.
 * When not-null, this is written into the user preferences.
 */
void wm_homefile_read(bContext *C,
                      ReportList *reports,
                      bool use_factory_settings,
                      bool use_empty_data,
                      bool use_data,
                      bool use_userdef,
                      const char *filepath_startup_override,
                      const char *app_template_override,
                      bool *r_is_factory_startup)
{
  Main *bmain = G_MAIN; /* Context does not always have valid main pointer here... */
  ListBase wmbase;
  bool success = false;

  bool filepath_startup_is_factory = true;
  char filepath_startup[FILE_MAX];
  char filepath_userdef[FILE_MAX];

  /* When 'app_template' is set:
   * '{BLENDER_USER_CONFIG}/{app_template}' */
  char app_template_system[FILE_MAX];
  /* When 'app_template' is set:
   * '{BLENDER_SYSTEM_SCRIPTS}/startup/bl_app_templates_system/{app_template}' */
  char app_template_config[FILE_MAX];

  eBLOReadSkip skip_flags = 0;

  if (use_data == false) {
    skip_flags |= BLO_READ_SKIP_DATA;
  }
  if (use_userdef == false) {
    skip_flags |= BLO_READ_SKIP_USERDEF;
  }

  /* True if we load startup.blend from memory
   * or use app-template startup.blend which the user hasn't saved. */
  bool is_factory_startup = true;

  /* options exclude eachother */
  BLI_assert((use_factory_settings && filepath_startup_override) == 0);

  if ((G.f & G_FLAG_SCRIPT_OVERRIDE_PREF) == 0) {
    SET_FLAG_FROM_TEST(G.f, (U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0, G_FLAG_SCRIPT_AUTOEXEC);
  }

  if (use_data) {
    BLI_callback_exec(CTX_data_main(C), NULL, BLI_CB_EVT_LOAD_PRE);

    G.relbase_valid = 0;

    /* put aside screens to match with persistent windows later */
    wm_window_match_init(C, &wmbase);
  }

  UI_view2d_zoom_cache_reset();

  filepath_startup[0] = '\0';
  filepath_userdef[0] = '\0';
  app_template_system[0] = '\0';
  app_template_config[0] = '\0';

  const char *const cfgdir = BKE_appdir_folder_id(BLENDER_USER_CONFIG, NULL);
  if (!use_factory_settings) {
    if (cfgdir) {
      BLI_path_join(
          filepath_startup, sizeof(filepath_startup), cfgdir, BLENDER_STARTUP_FILE, NULL);
      filepath_startup_is_factory = false;
      if (use_userdef) {
        BLI_path_join(
            filepath_userdef, sizeof(filepath_startup), cfgdir, BLENDER_USERPREF_FILE, NULL);
      }
    }
    else {
      use_factory_settings = true;
    }

    if (filepath_startup_override) {
      BLI_strncpy(filepath_startup, filepath_startup_override, FILE_MAX);
      filepath_startup_is_factory = false;
    }
  }

  /* load preferences before startup.blend */
  if (use_userdef) {
    if (!use_factory_settings && BLI_exists(filepath_userdef)) {
      UserDef *userdef = BKE_blendfile_userdef_read(filepath_userdef, NULL);
      if (userdef != NULL) {
        BKE_blender_userdef_data_set_and_free(userdef);
        userdef = NULL;

        skip_flags |= BLO_READ_SKIP_USERDEF;
        printf("Read prefs: %s\n", filepath_userdef);
      }
    }
  }

  const char *app_template = NULL;
  bool update_defaults = false;
  bool reset_app_template = false;

  if (filepath_startup_override != NULL) {
    /* pass */
  }
  else if (app_template_override) {
    /* This may be clearing the current template by setting to an empty string. */
    app_template = app_template_override;
  }
  else if (!use_factory_settings && U.app_template[0]) {
    app_template = U.app_template;
  }

  if ((!app_template && U.app_template[0]) ||
      (app_template && !STREQ(app_template, U.app_template))) {
    /* Always load UI when switching to another template. */
    G.fileflags &= ~G_FILE_NO_UI;
    reset_app_template = true;
  }

  if ((app_template != NULL) && (app_template[0] != '\0')) {
    if (!BKE_appdir_app_template_id_search(
            app_template, app_template_system, sizeof(app_template_system))) {
      /* Can safely continue with code below, just warn it's not found. */
      BKE_reportf(reports, RPT_WARNING, "Application Template '%s' not found", app_template);
    }

    /* Insert template name into startup file. */

    /* note that the path is being set even when 'use_factory_settings == true'
     * this is done so we can load a templates factory-settings */
    if (!use_factory_settings) {
      BLI_path_join(app_template_config, sizeof(app_template_config), cfgdir, app_template, NULL);
      BLI_path_join(filepath_startup,
                    sizeof(filepath_startup),
                    app_template_config,
                    BLENDER_STARTUP_FILE,
                    NULL);
      filepath_startup_is_factory = false;
      if (BLI_access(filepath_startup, R_OK) != 0) {
        filepath_startup[0] = '\0';
      }
    }
    else {
      filepath_startup[0] = '\0';
    }

    if (filepath_startup[0] == '\0') {
      BLI_path_join(filepath_startup,
                    sizeof(filepath_startup),
                    app_template_system,
                    BLENDER_STARTUP_FILE,
                    NULL);
      filepath_startup_is_factory = true;

      /* Update defaults only for system templates. */
      update_defaults = true;
    }
  }

  if (!use_factory_settings || (filepath_startup[0] != '\0')) {
    if (BLI_access(filepath_startup, R_OK) == 0) {
      success = BKE_blendfile_read(C,
                                   filepath_startup,
                                   &(const struct BlendFileReadParams){
                                       .is_startup = true,
                                       .skip_flags = skip_flags | BLO_READ_SKIP_USERDEF,
                                   },
                                   NULL);
    }
    if (BLI_listbase_is_empty(&U.themes)) {
      if (G.debug & G_DEBUG) {
        printf("\nNote: No (valid) '%s' found, fall back to built-in default.\n\n",
               filepath_startup);
      }
      success = false;
    }
    if (success) {
      if (update_defaults) {
        if (use_data) {
          BLO_update_defaults_startup_blend(CTX_data_main(C), app_template);
        }
      }
      is_factory_startup = filepath_startup_is_factory;
    }
  }

  if (success == false && filepath_startup_override && reports) {
    /* We can not return from here because wm is already reset */
    BKE_reportf(reports, RPT_ERROR, "Could not read '%s'", filepath_startup_override);
  }

  if (success == false) {
    if (use_userdef) {
      if ((skip_flags & BLO_READ_SKIP_USERDEF) == 0) {
        UserDef *userdef_default = BKE_blendfile_userdef_from_defaults();
        BKE_blender_userdef_app_template_data_set_and_free(userdef_default);
        skip_flags &= ~BLO_READ_SKIP_USERDEF;
      }
    }

    success = BKE_blendfile_read_from_memory(C,
                                             datatoc_startup_blend,
                                             datatoc_startup_blend_size,
                                             true,
                                             &(const struct BlendFileReadParams){
                                                 .is_startup = true,
                                                 .skip_flags = skip_flags,
                                             },
                                             NULL);

    if (use_data && BLI_listbase_is_empty(&wmbase)) {
      wm_clear_default_size(C);
    }
  }

  if (use_empty_data) {
    BKE_blendfile_read_make_empty(C);
  }

  /* Load template preferences,
   * unlike regular preferences we only use some of the settings,
   * see: BKE_blender_userdef_set_app_template */
  if (app_template_system[0] != '\0') {
    char temp_path[FILE_MAX];
    temp_path[0] = '\0';
    if (!use_factory_settings) {
      BLI_path_join(
          temp_path, sizeof(temp_path), app_template_config, BLENDER_USERPREF_FILE, NULL);
      if (BLI_access(temp_path, R_OK) != 0) {
        temp_path[0] = '\0';
      }
    }

    if (temp_path[0] == '\0') {
      BLI_path_join(
          temp_path, sizeof(temp_path), app_template_system, BLENDER_USERPREF_FILE, NULL);
    }

    if (use_userdef) {
      UserDef *userdef_template = NULL;
      /* just avoids missing file warning */
      if (BLI_exists(temp_path)) {
        userdef_template = BKE_blendfile_userdef_read(temp_path, NULL);
      }
      if (userdef_template == NULL) {
        /* we need to have preferences load to overwrite preferences from previous template */
        userdef_template = BKE_blendfile_userdef_from_defaults();
      }
      if (userdef_template) {
        BKE_blender_userdef_app_template_data_set_and_free(userdef_template);
        userdef_template = NULL;
      }
    }
  }

  if (app_template_override) {
    BLI_strncpy(U.app_template, app_template_override, sizeof(U.app_template));
  }

  if (use_data) {
    /* Prevent buggy files that had G_FILE_RELATIVE_REMAP written out by mistake.
     * Screws up autosaves otherwise can remove this eventually,
     * only in a 2.53 and older, now its not written. */
    G.fileflags &= ~G_FILE_RELATIVE_REMAP;
  }

  bmain = CTX_data_main(C);

  if (use_userdef) {
    /* check userdef before open window, keymaps etc */
    wm_init_userdef(bmain);
    reset_app_template = true;
  }

  if (use_data) {
    /* match the read WM with current WM */
    wm_window_match_do(C, &wmbase, &bmain->wm, &bmain->wm);
  }

  if (use_userdef) {
    /*  Clear keymaps because the current default keymap may have been initialized
     *  from user preferences, which have been reset. */
    for (wmWindowManager *wm = bmain->wm.first; wm; wm = wm->id.next) {
      if (wm->defaultconf) {
        wm->defaultconf->flag &= ~KEYCONF_INIT_DEFAULT;
      }
    }
  }

  if (use_data) {
    WM_check(C); /* opens window(s), checks keymaps */

    bmain->name[0] = '\0';

    /* start with save preference untitled.blend */
    G.save_over = 0;
  }

  wm_file_read_post(C, true, is_factory_startup, use_data, use_userdef, reset_app_template);

  if (r_is_factory_startup) {
    *r_is_factory_startup = is_factory_startup;
  }
}

/** \name WM History File API
 * \{ */

void wm_history_file_read(void)
{
  char name[FILE_MAX];
  LinkNode *l, *lines;
  struct RecentFile *recent;
  const char *line;
  int num;
  const char *const cfgdir = BKE_appdir_folder_id(BLENDER_USER_CONFIG, NULL);

  if (!cfgdir) {
    return;
  }

  BLI_make_file_string("/", name, cfgdir, BLENDER_HISTORY_FILE);

  lines = BLI_file_read_as_lines(name);

  BLI_listbase_clear(&G.recent_files);

  /* read list of recent opened files from recent-files.txt to memory */
  for (l = lines, num = 0; l && (num < U.recent_files); l = l->next) {
    line = l->link;
    /* don't check if files exist, causes slow startup for remote/external drives */
    if (line[0]) {
      recent = (RecentFile *)MEM_mallocN(sizeof(RecentFile), "RecentFile");
      BLI_addtail(&(G.recent_files), recent);
      recent->filepath = BLI_strdup(line);
      num++;
    }
  }

  BLI_file_free_lines(lines);
}

static RecentFile *wm_history_file_new(const char *filepath)
{
  RecentFile *recent = MEM_mallocN(sizeof(RecentFile), "RecentFile");
  recent->filepath = BLI_strdup(filepath);
  return recent;
}

static void wm_history_file_free(RecentFile *recent)
{
  BLI_assert(BLI_findindex(&G.recent_files, recent) != -1);
  MEM_freeN(recent->filepath);
  BLI_freelinkN(&G.recent_files, recent);
}

static RecentFile *wm_file_history_find(const char *filepath)
{
  return BLI_findstring_ptr(&G.recent_files, filepath, offsetof(RecentFile, filepath));
}

/**
 * Write #BLENDER_HISTORY_FILE as-is, without checking the environment
 * (that's handled by #wm_history_file_update).
 */
static void wm_history_file_write(void)
{
  const char *user_config_dir;
  char name[FILE_MAX];
  FILE *fp;

  /* will be NULL in background mode */
  user_config_dir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL);
  if (!user_config_dir) {
    return;
  }

  BLI_make_file_string("/", name, user_config_dir, BLENDER_HISTORY_FILE);

  fp = BLI_fopen(name, "w");
  if (fp) {
    struct RecentFile *recent;
    for (recent = G.recent_files.first; recent; recent = recent->next) {
      fprintf(fp, "%s\n", recent->filepath);
    }
    fclose(fp);
  }
}

/**
 * Run after saving a file to refresh the #BLENDER_HISTORY_FILE list.
 */
static void wm_history_file_update(void)
{
  RecentFile *recent;
  const char *blendfile_name = BKE_main_blendfile_path_from_global();

  /* no write history for recovered startup files */
  if (blendfile_name[0] == '\0') {
    return;
  }

  recent = G.recent_files.first;
  /* refresh recent-files.txt of recent opened files, when current file was changed */
  if (!(recent) || (BLI_path_cmp(recent->filepath, blendfile_name) != 0)) {

    recent = wm_file_history_find(blendfile_name);
    if (recent) {
      BLI_remlink(&G.recent_files, recent);
    }
    else {
      RecentFile *recent_next;
      for (recent = BLI_findlink(&G.recent_files, U.recent_files - 1); recent;
           recent = recent_next) {
        recent_next = recent->next;
        wm_history_file_free(recent);
      }
      recent = wm_history_file_new(blendfile_name);
    }

    /* add current file to the beginning of list */
    BLI_addhead(&(G.recent_files), recent);

    /* write current file to recent-files.txt */
    wm_history_file_write();

    /* also update most recent files on System */
    GHOST_addToSystemRecentFiles(blendfile_name);
  }
}

/** \} */

/* screen can be NULL */
static ImBuf *blend_file_thumb(const bContext *C,
                               Scene *scene,
                               bScreen *screen,
                               BlendThumbnail **thumb_pt)
{
  /* will be scaled down, but gives some nice oversampling */
  ImBuf *ibuf;
  BlendThumbnail *thumb;
  char err_out[256] = "unknown";

  /* screen if no camera found */
  ScrArea *sa = NULL;
  ARegion *ar = NULL;
  View3D *v3d = NULL;

  /* In case we are given a valid thumbnail data, just generate image from it. */
  if (*thumb_pt) {
    thumb = *thumb_pt;
    return BKE_main_thumbnail_to_imbuf(NULL, thumb);
  }

  /* scene can be NULL if running a script at startup and calling the save operator */
  if (G.background || scene == NULL) {
    return NULL;
  }

  if ((scene->camera == NULL) && (screen != NULL)) {
    sa = BKE_screen_find_big_area(screen, SPACE_VIEW3D, 0);
    ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
    if (ar) {
      v3d = sa->spacedata.first;
    }
  }

  if (scene->camera == NULL && v3d == NULL) {
    return NULL;
  }

  /* gets scaled to BLEN_THUMB_SIZE */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  if (scene->camera) {
    ibuf = ED_view3d_draw_offscreen_imbuf_simple(depsgraph,
                                                 scene,
                                                 NULL,
                                                 OB_SOLID,
                                                 scene->camera,
                                                 BLEN_THUMB_SIZE * 2,
                                                 BLEN_THUMB_SIZE * 2,
                                                 IB_rect,
                                                 V3D_OFSDRAW_NONE,
                                                 R_ALPHAPREMUL,
                                                 0,
                                                 NULL,
                                                 NULL,
                                                 err_out);
  }
  else {
    ibuf = ED_view3d_draw_offscreen_imbuf(depsgraph,
                                          scene,
                                          OB_SOLID,
                                          v3d,
                                          ar,
                                          BLEN_THUMB_SIZE * 2,
                                          BLEN_THUMB_SIZE * 2,
                                          IB_rect,
                                          R_ALPHAPREMUL,
                                          0,
                                          NULL,
                                          NULL,
                                          err_out);
  }

  if (ibuf) {
    float aspect = (scene->r.xsch * scene->r.xasp) / (scene->r.ysch * scene->r.yasp);

    /* dirty oversampling */
    IMB_scaleImBuf(ibuf, BLEN_THUMB_SIZE, BLEN_THUMB_SIZE);

    /* add pretty overlay */
    IMB_thumb_overlay_blend(ibuf->rect, ibuf->x, ibuf->y, aspect);

    thumb = BKE_main_thumbnail_from_imbuf(NULL, ibuf);
  }
  else {
    /* '*thumb_pt' needs to stay NULL to prevent a bad thumbnail from being handled */
    fprintf(stderr, "blend_file_thumb failed to create thumbnail: %s\n", err_out);
    thumb = NULL;
  }

  /* must be freed by caller */
  *thumb_pt = thumb;

  return ibuf;
}

/* easy access from gdb */
bool write_crash_blend(void)
{
  char path[FILE_MAX];
  int fileflags = G.fileflags & ~(G_FILE_HISTORY); /* don't do file history on crash file */

  BLI_strncpy(path, BKE_main_blendfile_path_from_global(), sizeof(path));
  BLI_path_extension_replace(path, sizeof(path), "_crash.blend");
  if (BLO_write_file(G_MAIN, path, fileflags, NULL, NULL)) {
    printf("written: %s\n", path);
    return 1;
  }
  else {
    printf("failed: %s\n", path);
    return 0;
  }
}

/**
 * \see #wm_homefile_write_exec wraps #BLO_write_file in a similar way.
 */
static bool wm_file_write(bContext *C, const char *filepath, int fileflags, ReportList *reports)
{
  Main *bmain = CTX_data_main(C);
  Library *li;
  int len;
  int ok = false;
  BlendThumbnail *thumb, *main_thumb;
  ImBuf *ibuf_thumb = NULL;

  len = strlen(filepath);

  if (len == 0) {
    BKE_report(reports, RPT_ERROR, "Path is empty, cannot save");
    return ok;
  }

  if (len >= FILE_MAX) {
    BKE_report(reports, RPT_ERROR, "Path too long, cannot save");
    return ok;
  }

  /* Check if file write permission is ok */
  if (BLI_exists(filepath) && !BLI_file_is_writable(filepath)) {
    BKE_reportf(reports, RPT_ERROR, "Cannot save blend file, path '%s' is not writable", filepath);
    return ok;
  }

  /* note: used to replace the file extension (to ensure '.blend'),
   * no need to now because the operator ensures,
   * its handy for scripts to save to a predefined name without blender editing it */

  /* send the OnSave event */
  for (li = bmain->libraries.first; li; li = li->id.next) {
    if (BLI_path_cmp(li->filepath, filepath) == 0) {
      BKE_reportf(reports, RPT_ERROR, "Cannot overwrite used library '%.240s'", filepath);
      return ok;
    }
  }

  /* Call pre-save callbacks befores writing preview,
   * that way you can generate custom file thumbnail. */
  BLI_callback_exec(bmain, NULL, BLI_CB_EVT_SAVE_PRE);

  /* blend file thumbnail */
  /* Save before exit_editmode, otherwise derivedmeshes for shared data corrupt T27765. */
  /* Main now can store a '.blend' thumbnail, useful for background mode
   * or thumbnail customization. */
  main_thumb = thumb = bmain->blen_thumb;
  if ((U.flag & USER_SAVE_PREVIEWS) && BLI_thread_is_main()) {
    ibuf_thumb = blend_file_thumb(C, CTX_data_scene(C), CTX_wm_screen(C), &thumb);
  }

  /* operator now handles overwrite checks */

  if (G.fileflags & G_FILE_AUTOPACK) {
    BKE_packedfile_pack_all(bmain, reports, false);
  }

  /* don't forget not to return without! */
  WM_cursor_wait(1);

  ED_editors_flush_edits(bmain, false);

  fileflags |= G_FILE_HISTORY; /* write file history */

  /* first time saving */
  /* XXX temp solution to solve bug, real fix coming (ton) */
  if ((BKE_main_blendfile_path(bmain)[0] == '\0') && !(fileflags & G_FILE_SAVE_COPY)) {
    BLI_strncpy(bmain->name, filepath, sizeof(bmain->name));
  }

  /* XXX temp solution to solve bug, real fix coming (ton) */
  bmain->recovered = 0;

  if (BLO_write_file(CTX_data_main(C), filepath, fileflags, reports, thumb)) {
    const bool do_history = (G.background == false) && (CTX_wm_manager(C)->op_undo_depth == 0);

    if (!(fileflags & G_FILE_SAVE_COPY)) {
      G.relbase_valid = 1;
      BLI_strncpy(bmain->name, filepath, sizeof(bmain->name)); /* is guaranteed current file */

      G.save_over = 1; /* disable untitled.blend convention */
    }

    SET_FLAG_FROM_TEST(G.fileflags, fileflags & G_FILE_COMPRESS, G_FILE_COMPRESS);

    /* prevent background mode scripts from clobbering history */
    if (do_history) {
      wm_history_file_update();
    }

    BLI_callback_exec(bmain, NULL, BLI_CB_EVT_SAVE_POST);

    /* run this function after because the file cant be written before the blend is */
    if (ibuf_thumb) {
      IMB_thumb_delete(filepath, THB_FAIL); /* without this a failed thumb overrides */
      ibuf_thumb = IMB_thumb_create(filepath, THB_LARGE, THB_SOURCE_BLEND, ibuf_thumb);
    }

    /* Success. */
    ok = true;
  }

  if (ibuf_thumb) {
    IMB_freeImBuf(ibuf_thumb);
  }
  if (thumb && thumb != main_thumb) {
    MEM_freeN(thumb);
  }

  WM_cursor_wait(0);

  return ok;
}

/************************ autosave ****************************/

void wm_autosave_location(char *filepath)
{
  const int pid = abs(getpid());
  char path[1024];
#ifdef WIN32
  const char *savedir;
#endif

  if (G_MAIN && G.relbase_valid) {
    const char *basename = BLI_path_basename(BKE_main_blendfile_path_from_global());
    int len = strlen(basename) - 6;
    BLI_snprintf(path, sizeof(path), "%.*s_%d_autosave.blend", len, basename, pid);
  }
  else {
    BLI_snprintf(path, sizeof(path), "%d_autosave.blend", pid);
  }

#ifdef WIN32
  /* XXX Need to investigate how to handle default location of '/tmp/'
   * This is a relative directory on Windows, and it may be
   * found. Example:
   * Blender installed on D:\ drive, D:\ drive has D:\tmp\
   * Now, BLI_exists() will find '/tmp/' exists, but
   * BLI_make_file_string will create string that has it most likely on C:\
   * through get_default_root().
   * If there is no C:\tmp autosave fails. */
  if (!BLI_exists(BKE_tempdir_base())) {
    savedir = BKE_appdir_folder_id_create(BLENDER_USER_AUTOSAVE, NULL);
    BLI_make_file_string("/", filepath, savedir, path);
    return;
  }
#endif

  BLI_make_file_string("/", filepath, BKE_tempdir_base(), path);
}

void WM_autosave_init(wmWindowManager *wm)
{
  wm_autosave_timer_ended(wm);

  if (U.flag & USER_AUTOSAVE) {
    wm->autosavetimer = WM_event_add_timer(wm, NULL, TIMERAUTOSAVE, U.savetime * 60.0);
  }
}

void wm_autosave_timer(const bContext *C, wmWindowManager *wm, wmTimer *UNUSED(wt))
{
  char filepath[FILE_MAX];

  WM_event_remove_timer(wm, NULL, wm->autosavetimer);

  /* if a modal operator is running, don't autosave, but try again in 10 seconds */
  for (wmWindow *win = wm->windows.first; win; win = win->next) {
    LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
      if (handler_base->type == WM_HANDLER_TYPE_OP) {
        wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
        if (handler->op) {
          wm->autosavetimer = WM_event_add_timer(wm, NULL, TIMERAUTOSAVE, 10.0);
          if (G.debug) {
            printf("Skipping auto-save, modal operator running, retrying in ten seconds...\n");
          }
          return;
        }
      }
    }
  }

  wm_autosave_location(filepath);

  if (U.uiflag & USER_GLOBALUNDO) {
    /* fast save of last undobuffer, now with UI */
    struct MemFile *memfile = ED_undosys_stack_memfile_get_active(wm->undo_stack);
    if (memfile) {
      BLO_memfile_write_file(memfile, filepath);
    }
  }
  else {
    /*  save as regular blend file */
    Main *bmain = CTX_data_main(C);
    int fileflags = G.fileflags & ~(G_FILE_COMPRESS | G_FILE_HISTORY);

    ED_editors_flush_edits(bmain, false);

    /* Error reporting into console */
    BLO_write_file(bmain, filepath, fileflags, NULL, NULL);
  }
  /* do timer after file write, just in case file write takes a long time */
  wm->autosavetimer = WM_event_add_timer(wm, NULL, TIMERAUTOSAVE, U.savetime * 60.0);
}

void wm_autosave_timer_ended(wmWindowManager *wm)
{
  if (wm->autosavetimer) {
    WM_event_remove_timer(wm, NULL, wm->autosavetimer);
    wm->autosavetimer = NULL;
  }
}

void wm_autosave_delete(void)
{
  char filename[FILE_MAX];

  wm_autosave_location(filename);

  if (BLI_exists(filename)) {
    char str[FILE_MAX];
    BLI_make_file_string("/", str, BKE_tempdir_base(), BLENDER_QUIT_FILE);

    /* if global undo; remove tempsave, otherwise rename */
    if (U.uiflag & USER_GLOBALUNDO) {
      BLI_delete(filename, false, false);
    }
    else {
      BLI_rename(filename, str);
    }
  }
}

void wm_autosave_read(bContext *C, ReportList *reports)
{
  char filename[FILE_MAX];

  wm_autosave_location(filename);
  WM_file_read(C, filename, reports);
}

/** \name Initialize WM_OT_open_xxx properties
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

void wm_open_init_use_scripts(wmOperator *op, bool use_prefs)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_scripts");
  if (!RNA_property_is_set(op->ptr, prop)) {
    /* use G_FLAG_SCRIPT_AUTOEXEC rather than the userpref because this means if
     * the flag has been disabled from the command line, then opening
     * from the menu wont enable this setting. */
    bool value = use_prefs ? ((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0) :
                             ((G.f & G_FLAG_SCRIPT_AUTOEXEC) != 0);

    RNA_property_boolean_set(op->ptr, prop, value);
  }
}

/** \} */

void WM_file_tag_modified(void)
{
  wmWindowManager *wm = G_MAIN->wm.first;
  if (wm->file_saved) {
    wm->file_saved = 0;
    /* notifier that data changed, for save-over warning or header */
    WM_main_add_notifier(NC_WM | ND_DATACHANGED, NULL);
  }
}

/** \name Preferences/startup save & load.
 *
 * \{ */

/**
 * \see #wm_file_write wraps #BLO_write_file in a similar way.
 * \return success.
 */
static int wm_homefile_write_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  char filepath[FILE_MAX];
  int fileflags;

  const char *app_template = U.app_template[0] ? U.app_template : NULL;
  const char *const cfgdir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, app_template);
  if (cfgdir == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Unable to create user config path");
    return OPERATOR_CANCELLED;
  }

  BLI_callback_exec(bmain, NULL, BLI_CB_EVT_SAVE_PRE);

  /* check current window and close it if temp */
  if (win && WM_window_is_temp_screen(win)) {
    wm_window_close(C, wm, win);
  }

  /* update keymaps in user preferences */
  WM_keyconfig_update(wm);

  BLI_path_join(filepath, sizeof(filepath), cfgdir, BLENDER_STARTUP_FILE, NULL);

  printf("Writing homefile: '%s' ", filepath);

  ED_editors_flush_edits(bmain, false);

  /*  force save as regular blend file */
  fileflags = G.fileflags & ~(G_FILE_COMPRESS | G_FILE_HISTORY);

  if (BLO_write_file(bmain, filepath, fileflags, op->reports, NULL) == 0) {
    printf("fail\n");
    return OPERATOR_CANCELLED;
  }

  printf("ok\n");

  G.save_over = 0;

  BLI_callback_exec(bmain, NULL, BLI_CB_EVT_SAVE_POST);

  return OPERATOR_FINISHED;
}

void WM_OT_save_homefile(wmOperatorType *ot)
{
  ot->name = "Save Startup File";
  ot->idname = "WM_OT_save_homefile";
  ot->description = "Make the current file the default .blend file";

  ot->invoke = WM_operator_confirm;
  ot->exec = wm_homefile_write_exec;
}

static int wm_userpref_autoexec_add_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
  bPathCompare *path_cmp = MEM_callocN(sizeof(bPathCompare), "bPathCompare");
  BLI_addtail(&U.autoexec_paths, path_cmp);
  U.runtime.is_dirty = true;
  return OPERATOR_FINISHED;
}

void WM_OT_userpref_autoexec_path_add(wmOperatorType *ot)
{
  ot->name = "Add Autoexec Path";
  ot->idname = "WM_OT_userpref_autoexec_path_add";
  ot->description = "Add path to exclude from autoexecution";

  ot->exec = wm_userpref_autoexec_add_exec;

  ot->flag = OPTYPE_INTERNAL;
}

static int wm_userpref_autoexec_remove_exec(bContext *UNUSED(C), wmOperator *op)
{
  const int index = RNA_int_get(op->ptr, "index");
  bPathCompare *path_cmp = BLI_findlink(&U.autoexec_paths, index);
  if (path_cmp) {
    BLI_freelinkN(&U.autoexec_paths, path_cmp);
    U.runtime.is_dirty = true;
  }
  return OPERATOR_FINISHED;
}

void WM_OT_userpref_autoexec_path_remove(wmOperatorType *ot)
{
  ot->name = "Remove Autoexec Path";
  ot->idname = "WM_OT_userpref_autoexec_path_remove";
  ot->description = "Remove path to exclude from autoexecution";

  ot->exec = wm_userpref_autoexec_remove_exec;

  ot->flag = OPTYPE_INTERNAL;

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, 1000);
}

/* Only save the prefs block. operator entry */
static int wm_userpref_write_exec(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* Update keymaps in user preferences. */
  WM_keyconfig_update(wm);

  const bool ok = BKE_blendfile_userdef_write_all(op->reports);

  return ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void WM_OT_save_userpref(wmOperatorType *ot)
{
  ot->name = "Save Preferences";
  ot->idname = "WM_OT_save_userpref";
  ot->description = "Make the current preferences default";

  ot->invoke = WM_operator_confirm;
  ot->exec = wm_userpref_write_exec;
}

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
  USERDEF_RESTORE(userpref);

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
    PropertyRNA *prop = iter.ptr.data;
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
  PointerRNA ptr_a, ptr_b;
  RNA_pointer_create(NULL, &RNA_Preferences, userdef_prev, &ptr_a);
  RNA_pointer_create(NULL, &RNA_Preferences, userdef_curr, &ptr_b);
  const bool is_dirty = userdef_curr->runtime.is_dirty;

  rna_struct_update_when_changed(C, bmain, &ptr_a, &ptr_b);

#ifdef WITH_PYTHON
  BPY_execute_string(C, (const char *[]){"addon_utils", NULL}, "addon_utils.reset_all()");
#endif

  WM_keyconfig_reload(C);
  userdef_curr->runtime.is_dirty = is_dirty;
}

static int wm_userpref_read_exec(bContext *C, wmOperator *op)
{
  const bool use_data = false;
  const bool use_userdef = true;
  const bool use_factory_settings = STREQ(op->type->idname, "WM_OT_read_factory_userpref");

  UserDef U_backup = U;

  wm_homefile_read(C,
                   op->reports,
                   use_factory_settings,
                   false,
                   use_data,
                   use_userdef,
                   NULL,
                   WM_init_state_app_template_get(),
                   NULL);

  wm_userpref_read_exceptions(&U, &U_backup);
  SET_FLAG_FROM_TEST(G.f, use_factory_settings, G_FLAG_USERPREF_NO_SAVE_ON_EXIT);

  Main *bmain = CTX_data_main(C);

  wm_userpref_update_when_changed(C, bmain, &U_backup, &U);

  if (use_factory_settings) {
    U.runtime.is_dirty = true;
  }

  /* Needed to recalculate UI scaling values (eg, #UserDef.inv_dpi_fac). */
  wm_window_clear_drawable(bmain->wm.first);

  WM_event_add_notifier(C, NC_WINDOW, NULL);

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

void WM_OT_read_factory_userpref(wmOperatorType *ot)
{
  ot->name = "Load Factory Preferences";
  ot->idname = "WM_OT_read_factory_userpref";
  ot->description =
      "Load factory default preferences. "
      "To make changes to preferences permanent, use \"Save Preferences\"";

  ot->invoke = WM_operator_confirm;
  ot->exec = wm_userpref_read_exec;
}

static int wm_history_file_read_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
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

  /* this operator is only used for loading settings from a previous blender install */
  ot->flag = OPTYPE_INTERNAL;
}

static int wm_homefile_read_exec(bContext *C, wmOperator *op)
{
  const bool use_factory_settings = (STREQ(op->type->idname, "WM_OT_read_factory_settings"));
  bool use_userdef = false;
  char filepath_buf[FILE_MAX];
  const char *filepath = NULL;
  UserDef U_backup = U;

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
            op->reports, RPT_ERROR, "Can't read alternative start-up file: '%s'", filepath);
        return OPERATOR_CANCELLED;
      }
    }
  }
  else {
    /* always load UI for factory settings (prefs will re-init) */
    G.fileflags &= ~G_FILE_NO_UI;
    /* Always load preferences with factory settings. */
    use_userdef = true;
  }

  char app_template_buf[sizeof(U.app_template)];
  const char *app_template;
  PropertyRNA *prop_app_template = RNA_struct_find_property(op->ptr, "app_template");
  const bool use_splash = !use_factory_settings && RNA_boolean_get(op->ptr, "use_splash");
  const bool use_empty_data = RNA_boolean_get(op->ptr, "use_empty");

  if (prop_app_template && RNA_property_is_set(op->ptr, prop_app_template)) {
    RNA_property_string_get(op->ptr, prop_app_template, app_template_buf);
    app_template = app_template_buf;

    /* Always load preferences when switching templates with own preferences. */
    use_userdef = BKE_appdir_app_template_has_userpref(app_template) ||
                  BKE_appdir_app_template_has_userpref(U.app_template);

    /* Turn override off, since we're explicitly loading a different app-template. */
    WM_init_state_app_template_set(NULL);
  }
  else {
    /* Normally NULL, only set when overriding from the command-line. */
    app_template = WM_init_state_app_template_get();
  }

  bool use_data = true;
  wm_homefile_read(C,
                   op->reports,
                   use_factory_settings,
                   use_empty_data,
                   use_data,
                   use_userdef,
                   filepath,
                   app_template,
                   NULL);
  if (use_splash) {
    WM_init_splash(C);
  }

  if (use_userdef) {
    wm_userpref_read_exceptions(&U, &U_backup);
    SET_FLAG_FROM_TEST(G.f, use_factory_settings, G_FLAG_USERPREF_NO_SAVE_ON_EXIT);
  }

  return OPERATOR_FINISHED;
}

static void wm_homefile_read_after_dialog_callback(bContext *C, void *user_data)
{
  WM_operator_name_call_with_properties(
      C, "WM_OT_read_homefile", WM_OP_EXEC_DEFAULT, (IDProperty *)user_data);
}

static void wm_free_operator_properties_callback(void *user_data)
{
  IDProperty *properties = (IDProperty *)user_data;
  IDP_FreeProperty(properties);
}

static int wm_homefile_read_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (U.uiflag & USER_SAVE_PROMPT && wm_file_or_image_is_modified(C)) {
    wmGenericCallback *callback = MEM_callocN(sizeof(*callback), __func__);
    callback->exec = wm_homefile_read_after_dialog_callback;
    callback->user_data = IDP_CopyProperty(op->properties);
    callback->free_user_data = wm_free_operator_properties_callback;
    wm_close_file_dialog(C, callback);
    return OPERATOR_INTERFACE;
  }
  else {
    return wm_homefile_read_exec(C, op);
  }
}

static void read_homefile_props(wmOperatorType *ot)
{
  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna, "app_template", "Template", sizeof(U.app_template), "", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "use_empty", false, "Empty", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void WM_OT_read_homefile(wmOperatorType *ot)
{
  PropertyRNA *prop;
  ot->name = "Reload Start-Up File";
  ot->idname = "WM_OT_read_homefile";
  ot->description = "Open the default file (doesn't save the current file)";

  ot->invoke = wm_homefile_read_invoke;
  ot->exec = wm_homefile_read_exec;

  prop = RNA_def_string_file_path(
      ot->srna, "filepath", NULL, FILE_MAX, "File Path", "Path to an alternative start-up file");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  /* So scripts can use an alternative start-up file without the UI */
  prop = RNA_def_boolean(
      ot->srna, "load_ui", true, "Load UI", "Load user interface setup from the .blend file");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  /* So the splash can be kept open after loading a file (for templates). */
  prop = RNA_def_boolean(ot->srna, "use_splash", false, "Splash", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  read_homefile_props(ot);

  /* omit poll to run in background mode */
}

void WM_OT_read_factory_settings(wmOperatorType *ot)
{
  ot->name = "Load Factory Settings";
  ot->idname = "WM_OT_read_factory_settings";
  ot->description =
      "Load factory default startup file and preferences. "
      "To make changes permanent, use \"Save Startup File\" and \"Save Preferences\"";

  ot->invoke = WM_operator_confirm;
  ot->exec = wm_homefile_read_exec;

  read_homefile_props(ot);
  /* omit poll to run in background mode */
}

/** \} */

/** \name Open main .blend file.
 *
 * \{ */

/**
 * Wrap #WM_file_read, shared by file reading operators.
 */
static bool wm_file_read_opwrap(bContext *C,
                                const char *filepath,
                                ReportList *reports,
                                const bool autoexec_init)
{
  bool success;

  /* XXX wm in context is not set correctly after WM_file_read -> crash */
  /* do it before for now, but is this correct with multiple windows? */
  WM_event_add_notifier(C, NC_WINDOW, NULL);

  if (autoexec_init) {
    WM_file_autoexec_init(filepath);
  }

  success = WM_file_read(C, filepath, reports);

  return success;
}

/* Generic operator state utilities
 *********************************************/

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

typedef struct OperatorDispatchTarget {
  int state;
  int (*run)(bContext *C, wmOperator *op);
} OperatorDispatchTarget;

static int operator_state_dispatch(bContext *C, wmOperator *op, OperatorDispatchTarget *targets)
{
  int state = get_operator_state(op);
  for (int i = 0; targets[i].run; i++) {
    OperatorDispatchTarget target = targets[i];
    if (target.state == state) {
      return target.run(C, op);
    }
  }
  BLI_assert(false);
  return OPERATOR_CANCELLED;
}

/* Open Mainfile operator
 ********************************************/

enum {
  OPEN_MAINFILE_STATE_DISCARD_CHANGES,
  OPEN_MAINFILE_STATE_SELECT_FILE_PATH,
  OPEN_MAINFILE_STATE_OPEN,
};

static int wm_open_mainfile_dispatch(bContext *C, wmOperator *op);

static void wm_open_mainfile_after_dialog_callback(bContext *C, void *user_data)
{
  WM_operator_name_call_with_properties(
      C, "WM_OT_open_mainfile", WM_OP_INVOKE_DEFAULT, (IDProperty *)user_data);
}

static int wm_open_mainfile__discard_changes(bContext *C, wmOperator *op)
{
  if (RNA_boolean_get(op->ptr, "display_file_selector")) {
    set_next_operator_state(op, OPEN_MAINFILE_STATE_SELECT_FILE_PATH);
  }
  else {
    set_next_operator_state(op, OPEN_MAINFILE_STATE_OPEN);
  }

  if (U.uiflag & USER_SAVE_PROMPT && wm_file_or_image_is_modified(C)) {
    wmGenericCallback *callback = MEM_callocN(sizeof(*callback), __func__);
    callback->exec = wm_open_mainfile_after_dialog_callback;
    callback->user_data = IDP_CopyProperty(op->properties);
    callback->free_user_data = wm_free_operator_properties_callback;
    wm_close_file_dialog(C, callback);
    return OPERATOR_INTERFACE;
  }
  else {
    return wm_open_mainfile_dispatch(C, op);
  }
}

static int wm_open_mainfile__select_file_path(bContext *C, wmOperator *op)
{
  set_next_operator_state(op, OPEN_MAINFILE_STATE_OPEN);

  Main *bmain = CTX_data_main(C);
  const char *openname = BKE_main_blendfile_path(bmain);

  if (CTX_wm_window(C) == NULL) {
    /* in rare cases this could happen, when trying to invoke in background
     * mode on load for example. Don't use poll for this because exec()
     * can still run without a window */
    BKE_report(op->reports, RPT_ERROR, "Context window not set");
    return OPERATOR_CANCELLED;
  }

  /* if possible, get the name of the most recently used .blend file */
  if (G.recent_files.first) {
    struct RecentFile *recent = G.recent_files.first;
    openname = recent->filepath;
  }

  RNA_string_set(op->ptr, "filepath", openname);
  wm_open_init_load_ui(op, true);
  wm_open_init_use_scripts(op, true);
  op->customdata = NULL;

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int wm_open_mainfile__open(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  bool success;

  RNA_string_get(op->ptr, "filepath", filepath);

  /* re-use last loaded setting so we can reload a file without changing */
  wm_open_init_load_ui(op, false);
  wm_open_init_use_scripts(op, false);

  if (RNA_boolean_get(op->ptr, "load_ui")) {
    G.fileflags &= ~G_FILE_NO_UI;
  }
  else {
    G.fileflags |= G_FILE_NO_UI;
  }

  if (RNA_boolean_get(op->ptr, "use_scripts")) {
    G.f |= G_FLAG_SCRIPT_AUTOEXEC;
  }
  else {
    G.f &= ~G_FLAG_SCRIPT_AUTOEXEC;
  }

  success = wm_file_read_opwrap(C, filepath, op->reports, !(G.f & G_FLAG_SCRIPT_AUTOEXEC));

  /* for file open also popup for warnings, not only errors */
  BKE_report_print_level_set(op->reports, RPT_WARNING);

  if (success) {
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

static OperatorDispatchTarget wm_open_mainfile_dispatch_targets[] = {
    {OPEN_MAINFILE_STATE_DISCARD_CHANGES, wm_open_mainfile__discard_changes},
    {OPEN_MAINFILE_STATE_SELECT_FILE_PATH, wm_open_mainfile__select_file_path},
    {OPEN_MAINFILE_STATE_OPEN, wm_open_mainfile__open},
    {0, NULL},
};

static int wm_open_mainfile_dispatch(bContext *C, wmOperator *op)
{
  return operator_state_dispatch(C, op, wm_open_mainfile_dispatch_targets);
}

static int wm_open_mainfile_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  return wm_open_mainfile_dispatch(C, op);
}

static int wm_open_mainfile_exec(bContext *C, wmOperator *op)
{
  return wm_open_mainfile__open(C, op);
}

/* currently fits in a pointer */
struct FileRuntime {
  bool is_untrusted;
};

static bool wm_open_mainfile_check(bContext *UNUSED(C), wmOperator *op)
{
  struct FileRuntime *file_info = (struct FileRuntime *)&op->customdata;
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_scripts");
  bool is_untrusted = false;
  char path[FILE_MAX];
  char *lslash;

  RNA_string_get(op->ptr, "filepath", path);

  /* get the dir */
  lslash = (char *)BLI_last_slash(path);
  if (lslash) {
    *(lslash + 1) = '\0';
  }

  if ((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0) {
    if (BKE_autoexec_match(path) == true) {
      RNA_property_boolean_set(op->ptr, prop, false);
      is_untrusted = true;
    }
  }

  if (file_info) {
    file_info->is_untrusted = is_untrusted;
  }

  return is_untrusted;
}

static void wm_open_mainfile_ui(bContext *UNUSED(C), wmOperator *op)
{
  struct FileRuntime *file_info = (struct FileRuntime *)&op->customdata;
  uiLayout *layout = op->layout;
  uiLayout *col = op->layout;
  const char *autoexec_text;

  uiItemR(layout, op->ptr, "load_ui", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  if (file_info->is_untrusted) {
    autoexec_text = IFACE_("Trusted Source [Untrusted Path]");
    uiLayoutSetActive(col, false);
    uiLayoutSetEnabled(col, false);
  }
  else {
    autoexec_text = IFACE_("Trusted Source");
  }

  uiItemR(col, op->ptr, "use_scripts", 0, autoexec_text, ICON_NONE);
}

void WM_OT_open_mainfile(wmOperatorType *ot)
{
  ot->name = "Open Blender File";
  ot->idname = "WM_OT_open_mainfile";
  ot->description = "Open a Blender file";

  ot->invoke = wm_open_mainfile_invoke;
  ot->exec = wm_open_mainfile_exec;
  ot->check = wm_open_mainfile_check;
  ot->ui = wm_open_mainfile_ui;
  /* omit window poll so this can work in background mode */

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);

  RNA_def_boolean(
      ot->srna, "load_ui", true, "Load UI", "Load user interface setup in the .blend file");
  RNA_def_boolean(ot->srna,
                  "use_scripts",
                  true,
                  "Trusted Source",
                  "Allow .blend file to execute scripts automatically, default available from "
                  "system preferences");

  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "display_file_selector", true, "Display File Selector", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  create_operator_state(ot, OPEN_MAINFILE_STATE_DISCARD_CHANGES);
}

/** \} */

/** \name Reload (revert) main .blend file.
 *
 * \{ */

static int wm_revert_mainfile_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  bool success;
  char filepath[FILE_MAX];

  wm_open_init_use_scripts(op, false);

  if (RNA_boolean_get(op->ptr, "use_scripts")) {
    G.f |= G_FLAG_SCRIPT_AUTOEXEC;
  }
  else {
    G.f &= ~G_FLAG_SCRIPT_AUTOEXEC;
  }

  BLI_strncpy(filepath, BKE_main_blendfile_path(bmain), sizeof(filepath));
  success = wm_file_read_opwrap(C, filepath, op->reports, !(G.f & G_FLAG_SCRIPT_AUTOEXEC));

  if (success) {
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

static bool wm_revert_mainfile_poll(bContext *UNUSED(C))
{
  return G.relbase_valid;
}

void WM_OT_revert_mainfile(wmOperatorType *ot)
{
  ot->name = "Revert";
  ot->idname = "WM_OT_revert_mainfile";
  ot->description = "Reload the saved file";
  ot->invoke = WM_operator_confirm;

  RNA_def_boolean(ot->srna,
                  "use_scripts",
                  true,
                  "Trusted Source",
                  "Allow .blend file to execute scripts automatically, default available from "
                  "system preferences");

  ot->exec = wm_revert_mainfile_exec;
  ot->poll = wm_revert_mainfile_poll;
}

/** \} */

/** \name Recover last session & auto-save.
 *
 * \{ */

void WM_recover_last_session(bContext *C, ReportList *reports)
{
  char filepath[FILE_MAX];

  BLI_make_file_string("/", filepath, BKE_tempdir_base(), BLENDER_QUIT_FILE);
  /* if reports==NULL, it's called directly without operator, we add a quick check here */
  if (reports || BLI_exists(filepath)) {
    G.fileflags |= G_FILE_RECOVER;

    wm_file_read_opwrap(C, filepath, reports, true);

    G.fileflags &= ~G_FILE_RECOVER;

    /* XXX bad global... fixme */
    Main *bmain = CTX_data_main(C);
    if (BKE_main_blendfile_path(bmain)[0] != '\0') {
      G.file_loaded = 1; /* prevents splash to show */
    }
    else {
      G.relbase_valid = 0;
      G.save_over = 0; /* start with save preference untitled.blend */
    }
  }
}

static int wm_recover_last_session_exec(bContext *C, wmOperator *op)
{
  WM_recover_last_session(C, op->reports);
  return OPERATOR_FINISHED;
}

void WM_OT_recover_last_session(wmOperatorType *ot)
{
  ot->name = "Recover Last Session";
  ot->idname = "WM_OT_recover_last_session";
  ot->description = "Open the last closed file (\"" BLENDER_QUIT_FILE "\")";
  ot->invoke = WM_operator_confirm;

  ot->exec = wm_recover_last_session_exec;
}

static int wm_recover_auto_save_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  bool success;

  RNA_string_get(op->ptr, "filepath", filepath);

  G.fileflags |= G_FILE_RECOVER;

  success = wm_file_read_opwrap(C, filepath, op->reports, true);

  G.fileflags &= ~G_FILE_RECOVER;

  if (success) {
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

static int wm_recover_auto_save_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  char filename[FILE_MAX];

  wm_autosave_location(filename);
  RNA_string_set(op->ptr, "filepath", filename);
  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void WM_OT_recover_auto_save(wmOperatorType *ot)
{
  ot->name = "Recover Auto Save";
  ot->idname = "WM_OT_recover_auto_save";
  ot->description = "Open an automatically saved file to recover it";

  ot->exec = wm_recover_auto_save_exec;
  ot->invoke = wm_recover_auto_save_invoke;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_LONGDISPLAY,
                                 FILE_SORT_TIME);
}

/** \} */

/** \name Save main .blend file.
 *
 * \{ */

static void wm_filepath_default(char *filepath)
{
  if (G.save_over == false) {
    BLI_ensure_filename(filepath, FILE_MAX, "untitled.blend");
  }
}

static void save_set_compress(wmOperator *op)
{
  PropertyRNA *prop;

  prop = RNA_struct_find_property(op->ptr, "compress");
  if (!RNA_property_is_set(op->ptr, prop)) {
    if (G.save_over) { /* keep flag for existing file */
      RNA_property_boolean_set(op->ptr, prop, (G.fileflags & G_FILE_COMPRESS) != 0);
    }
    else { /* use userdef for new file */
      RNA_property_boolean_set(op->ptr, prop, (U.flag & USER_FILECOMPRESS) != 0);
    }
  }
}

static void save_set_filepath(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  PropertyRNA *prop;
  char name[FILE_MAX];

  prop = RNA_struct_find_property(op->ptr, "filepath");
  if (!RNA_property_is_set(op->ptr, prop)) {
    /* if not saved before, get the name of the most recently used .blend file */
    if (BKE_main_blendfile_path(bmain)[0] == '\0' && G.recent_files.first) {
      struct RecentFile *recent = G.recent_files.first;
      BLI_strncpy(name, recent->filepath, FILE_MAX);
    }
    else {
      BLI_strncpy(name, bmain->name, FILE_MAX);
    }

    wm_filepath_default(name);
    RNA_property_string_set(op->ptr, prop, name);
  }
}

static int wm_save_as_mainfile_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{

  save_set_compress(op);
  save_set_filepath(C, op);

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* function used for WM_OT_save_mainfile too */
static int wm_save_as_mainfile_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  char path[FILE_MAX];
  const bool is_save_as = (op->type->invoke == wm_save_as_mainfile_invoke);

  save_set_compress(op);

  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    RNA_string_get(op->ptr, "filepath", path);
  }
  else {
    BLI_strncpy(path, BKE_main_blendfile_path(bmain), FILE_MAX);
    wm_filepath_default(path);
  }

  const int fileflags_orig = G.fileflags;
  int fileflags = G.fileflags & ~G_FILE_USERPREFS;

  /* set compression flag */
  SET_FLAG_FROM_TEST(fileflags, RNA_boolean_get(op->ptr, "compress"), G_FILE_COMPRESS);
  SET_FLAG_FROM_TEST(fileflags, RNA_boolean_get(op->ptr, "relative_remap"), G_FILE_RELATIVE_REMAP);
  SET_FLAG_FROM_TEST(
      fileflags,
      (RNA_struct_property_is_set(op->ptr, "copy") && RNA_boolean_get(op->ptr, "copy")),
      G_FILE_SAVE_COPY);

  const bool ok = wm_file_write(C, path, fileflags, op->reports);

  if ((op->flag & OP_IS_INVOKE) == 0) {
    /* OP_IS_INVOKE is set when the operator is called from the GUI.
     * If it is not set, the operator is called from a script and
     * shouldn't influence G.fileflags. */
    G.fileflags = fileflags_orig;
  }

  if (ok == false) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_WM | ND_FILESAVE, NULL);

  if (!is_save_as && RNA_boolean_get(op->ptr, "exit")) {
    wm_exit_schedule_delayed(C);
  }

  return OPERATOR_FINISHED;
}

/* function used for WM_OT_save_mainfile too */
static bool blend_save_check(bContext *UNUSED(C), wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  if (!BLO_has_bfile_extension(filepath)) {
    /* some users would prefer BLI_path_extension_replace(),
     * we keep getting nitpicking bug reports about this - campbell */
    BLI_path_extension_ensure(filepath, FILE_MAX, ".blend");
    RNA_string_set(op->ptr, "filepath", filepath);
    return true;
  }
  return false;
}

void WM_OT_save_as_mainfile(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Save As Blender File";
  ot->idname = "WM_OT_save_as_mainfile";
  ot->description = "Save the current file in the desired location";

  ot->invoke = wm_save_as_mainfile_invoke;
  ot->exec = wm_save_as_mainfile_exec;
  ot->check = blend_save_check;
  /* omit window poll so this can work in background mode */

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);
  RNA_def_boolean(ot->srna, "compress", false, "Compress", "Write compressed .blend file");
  RNA_def_boolean(ot->srna,
                  "relative_remap",
                  true,
                  "Remap Relative",
                  "Make paths relative when saving to a different directory");
  prop = RNA_def_boolean(
      ot->srna,
      "copy",
      false,
      "Save Copy",
      "Save a copy of the actual working state but does not make saved file active");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int wm_save_mainfile_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  int ret;

  /* cancel if no active window */
  if (CTX_wm_window(C) == NULL) {
    return OPERATOR_CANCELLED;
  }

  save_set_compress(op);
  save_set_filepath(C, op);

  /* if we're saving for the first time and prefer relative paths -
   * any existing paths will be absolute,
   * enable the option to remap paths to avoid confusion T37240. */
  if ((G.relbase_valid == false) && (U.flag & USER_RELPATHS)) {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "relative_remap");
    if (!RNA_property_is_set(op->ptr, prop)) {
      RNA_property_boolean_set(op->ptr, prop, true);
    }
  }

  if (G.save_over) {
    char path[FILE_MAX];

    RNA_string_get(op->ptr, "filepath", path);
    ret = wm_save_as_mainfile_exec(C, op);
    /* Without this there is no feedback the file was saved. */
    BKE_reportf(op->reports, RPT_INFO, "Saved \"%s\"", BLI_path_basename(path));
  }
  else {
    WM_event_add_fileselect(C, op);
    ret = OPERATOR_RUNNING_MODAL;
  }

  return ret;
}

void WM_OT_save_mainfile(wmOperatorType *ot)
{
  ot->name = "Save Blender File";
  ot->idname = "WM_OT_save_mainfile";
  ot->description = "Save the current Blender file";

  ot->invoke = wm_save_mainfile_invoke;
  ot->exec = wm_save_as_mainfile_exec;
  ot->check = blend_save_check;
  /* omit window poll so this can work in background mode */

  PropertyRNA *prop;
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);
  RNA_def_boolean(ot->srna, "compress", false, "Compress", "Write compressed .blend file");
  RNA_def_boolean(ot->srna,
                  "relative_remap",
                  false,
                  "Remap Relative",
                  "Make paths relative when saving to a different directory");

  prop = RNA_def_boolean(ot->srna, "exit", false, "Exit", "Exit Blender after saving");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/** \name Auto-execution of scripts warning popup
 *
 * \{ */

static void wm_block_autorun_warning_ignore(bContext *C, void *arg_block, void *UNUSED(arg))
{
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, arg_block);
}

static void wm_block_autorun_warning_reload_with_scripts(bContext *C,
                                                         void *arg_block,
                                                         void *UNUSED(arg))
{
  wmWindow *win = CTX_wm_window(C);

  UI_popup_block_close(C, win, arg_block);

  /* Save user preferences for permanent execution. */
  if ((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0) {
    WM_operator_name_call(C, "WM_OT_save_userpref", WM_OP_EXEC_DEFAULT, NULL);
  }

  /* Load file again with scripts enabled.
   * The reload is necessary to allow scripts to run when the files loads. */
  wmOperatorType *ot = WM_operatortype_find("WM_OT_revert_mainfile", false);

  PointerRNA props_ptr;
  WM_operator_properties_create_ptr(&props_ptr, ot);
  RNA_boolean_set(&props_ptr, "use_scripts", true);
  WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &props_ptr);
  WM_operator_properties_free(&props_ptr);
}

static void wm_block_autorun_warning_enable_scripts(bContext *C,
                                                    void *arg_block,
                                                    void *UNUSED(arg))
{
  wmWindow *win = CTX_wm_window(C);
  Main *bmain = CTX_data_main(C);

  UI_popup_block_close(C, win, arg_block);

  /* Save user preferences for permanent execution. */
  if ((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0) {
    WM_operator_name_call(C, "WM_OT_save_userpref", WM_OP_EXEC_DEFAULT, NULL);
  }

  /* Force a full refresh, but without reloading the file. */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    BKE_scene_free_depsgraph_hash(scene);
  }
}

/* Build the autorun warning dialog UI */
static uiBlock *block_create_autorun_warning(struct bContext *C,
                                             struct ARegion *ar,
                                             void *UNUSED(arg1))
{
  wmWindowManager *wm = CTX_wm_manager(C);
  uiStyle *style = UI_style_get();
  uiBlock *block = UI_block_begin(C, ar, "autorun_warning_popup", UI_EMBOSS);

  UI_block_flag_enable(
      block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_LOOP | UI_BLOCK_NO_WIN_CLIP | UI_BLOCK_NUMSELECT);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
  UI_block_emboss_set(block, UI_EMBOSS);

  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_PANEL,
                                     10,
                                     2,
                                     U.widget_unit * 24,
                                     U.widget_unit * 6,
                                     0,
                                     style);

  /* Text and some vertical space */
  uiLayout *col = uiLayoutColumn(layout, true);
  uiItemL(col,
          TIP_("For security reasons, automatic execution of Python scripts in this file was "
               "disabled:"),
          ICON_ERROR);
  uiLayout *sub = uiLayoutRow(col, true);
  uiLayoutSetRedAlert(sub, true);
  uiItemL(sub, G.autoexec_fail, ICON_BLANK1);
  uiItemL(col, TIP_("This may lead to unexpected behavior"), ICON_BLANK1);

  uiItemS(layout);

  PointerRNA pref_ptr;
  RNA_pointer_create(NULL, &RNA_PreferencesFilePaths, &U, &pref_ptr);
  uiItemR(layout,
          &pref_ptr,
          "use_scripts_auto_execute",
          0,
          TIP_("Permanently allow execution of scripts"),
          ICON_NONE);

  uiItemS(layout);

  /* Buttons */
  uiBut *but;
  uiLayout *split = uiLayoutSplit(layout, 0.0f, true);
  col = uiLayoutColumn(split, false);

  /* Allow reload if we have a saved file.
   * Otherwise just enable scripts and reset the depsgraphs. */
  if (G.relbase_valid && wm->file_saved) {
    but = uiDefIconTextBut(block,
                           UI_BTYPE_BUT,
                           0,
                           ICON_NONE,
                           IFACE_("Allow Execution"),
                           0,
                           0,
                           50,
                           UI_UNIT_Y,
                           NULL,
                           0,
                           0,
                           0,
                           0,
                           TIP_("Reload file with execution of Python scripts enabled"));
    UI_but_func_set(but, wm_block_autorun_warning_reload_with_scripts, block, NULL);
  }
  else {
    but = uiDefIconTextBut(block,
                           UI_BTYPE_BUT,
                           0,
                           ICON_NONE,
                           IFACE_("Allow Execution"),
                           0,
                           0,
                           50,
                           UI_UNIT_Y,
                           NULL,
                           0,
                           0,
                           0,
                           0,
                           TIP_("Enable scripts"));
    UI_but_func_set(but, wm_block_autorun_warning_enable_scripts, block, NULL);
  }

  /* empty space between buttons */
  col = uiLayoutColumn(split, false);
  uiItemS(col);

  col = uiLayoutColumn(split, 1);
  but = uiDefIconTextBut(block,
                         UI_BTYPE_BUT,
                         0,
                         ICON_NONE,
                         IFACE_("Ignore"),
                         0,
                         0,
                         50,
                         UI_UNIT_Y,
                         NULL,
                         0,
                         0,
                         0,
                         0,
                         TIP_("Continue using file without Python scripts"));
  UI_but_func_set(but, wm_block_autorun_warning_ignore, block, NULL);

  UI_block_bounds_set_centered(block, 10);

  return block;
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
  wmWindow *win = (wm->winactive) ? wm->winactive : wm->windows.first;

  if (win) {
    wmWindow *prevwin = CTX_wm_window(C);
    CTX_wm_window_set(C, win);
    UI_popup_block_invoke(C, block_create_autorun_warning, NULL, NULL);
    CTX_wm_window_set(C, prevwin);
  }
}

/* Close File Dialog
 *************************************/

static char save_images_when_file_is_closed = true;

static void wm_block_file_close_cancel(bContext *C, void *arg_block, void *UNUSED(arg_data))
{
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, arg_block);
}

static void wm_block_file_close_discard(bContext *C, void *arg_block, void *arg_data)
{
  wmGenericCallback *callback = WM_generic_callback_steal((wmGenericCallback *)arg_data);

  /* Close the popup before executing the callback. Otherwise
   * the popup might be closed by the callback, which will lead
   * to a crash. */
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, arg_block);

  callback->exec(C, callback->user_data);
  WM_generic_callback_free(callback);
}

static void wm_block_file_close_save(bContext *C, void *arg_block, void *arg_data)
{
  wmGenericCallback *callback = WM_generic_callback_steal((wmGenericCallback *)arg_data);
  bool execute_callback = true;

  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, arg_block);

  int modified_images_count = ED_image_save_all_modified_info(C, NULL);
  if (modified_images_count > 0 && save_images_when_file_is_closed) {
    if (ED_image_should_save_modified(C)) {
      ReportList *reports = CTX_wm_reports(C);
      ED_image_save_all_modified(C, reports);
      WM_report_banner_show();
    }
    else {
      execute_callback = false;
    }
  }

  Main *bmain = CTX_data_main(C);
  bool file_has_been_saved_before = BKE_main_blendfile_path(bmain)[0] != '\0';

  if (file_has_been_saved_before) {
    WM_operator_name_call(C, "WM_OT_save_mainfile", WM_OP_EXEC_DEFAULT, NULL);
  }
  else {
    WM_operator_name_call(C, "WM_OT_save_mainfile", WM_OP_INVOKE_DEFAULT, NULL);
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
      block, UI_BTYPE_BUT, 0, 0, IFACE_("Cancel"), 0, 0, 0, UI_UNIT_Y, 0, 0, 0, 0, 0, "");
  UI_but_func_set(but, wm_block_file_close_cancel, block, post_action);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
}

static void wm_block_file_close_discard_button(uiBlock *block, wmGenericCallback *post_action)
{
  uiBut *but = uiDefIconTextBut(
      block, UI_BTYPE_BUT, 0, 0, IFACE_("Discard Changes"), 0, 0, 0, UI_UNIT_Y, 0, 0, 0, 0, 0, "");
  UI_but_func_set(but, wm_block_file_close_discard, block, post_action);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
}

static void wm_block_file_close_save_button(uiBlock *block, wmGenericCallback *post_action)
{
  uiBut *but = uiDefIconTextBut(
      block, UI_BTYPE_BUT, 0, 0, IFACE_("Save"), 0, 0, 0, UI_UNIT_Y, 0, 0, 0, 0, 0, "");
  UI_but_func_set(but, wm_block_file_close_save, block, post_action);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  UI_but_flag_enable(but, UI_BUT_ACTIVE_DEFAULT);
}

static const char *close_file_dialog_name = "file_close_popup";

static uiBlock *block_create__close_file_dialog(struct bContext *C, struct ARegion *ar, void *arg1)
{
  wmGenericCallback *post_action = (wmGenericCallback *)arg1;
  Main *bmain = CTX_data_main(C);

  uiStyle *style = UI_style_get();
  uiBlock *block = UI_block_begin(C, ar, close_file_dialog_name, UI_EMBOSS);

  UI_block_flag_enable(
      block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_LOOP | UI_BLOCK_NO_WIN_CLIP | UI_BLOCK_NUMSELECT);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
  UI_block_emboss_set(block, UI_EMBOSS);

  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_PANEL,
                                     10,
                                     2,
                                     U.widget_unit * 24,
                                     U.widget_unit * 6,
                                     0,
                                     style);

  /* Title */
  bool blend_file_is_saved = BKE_main_blendfile_path(bmain)[0] != '\0';
  if (blend_file_is_saved) {
    uiItemL(layout, "This file has unsaved changes.", ICON_INFO);
  }
  else {
    uiItemL(layout, "This file has not been saved yet.", ICON_INFO);
  }

  /* Image Saving */
  ReportList reports;
  BKE_reports_init(&reports, RPT_STORE);
  uint modified_images_count = ED_image_save_all_modified_info(C, &reports);

  if (modified_images_count > 0) {
    char message[64];
    BLI_snprintf(message,
                 sizeof(message),
                 (modified_images_count == 1) ? "Save %u modified image" :
                                                "Save %u modified images",
                 modified_images_count);
    uiDefButBitC(block,
                 UI_BTYPE_CHECKBOX,
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
                 0,
                 0,
                 "");
  }

  LISTBASE_FOREACH (Report *, report, &reports.list) {
    uiItemL(layout, report->message, ICON_ERROR);
  }

  BKE_reports_clear(&reports);

  uiItemL(layout, "", ICON_NONE);

  /* Buttons */
#ifdef _WIN32
  const bool windows_layout = true;
#else
  const bool windows_layout = false;
#endif

  uiLayout *split = uiLayoutSplit(layout, 0.0f, true);

  if (windows_layout) {
    /* Windows standard layout. */
    uiLayout *col = uiLayoutColumn(split, false);
    uiItemS(col);

    col = uiLayoutColumn(split, false);
    wm_block_file_close_save_button(block, post_action);

    col = uiLayoutColumn(split, false);
    wm_block_file_close_discard_button(block, post_action);

    col = uiLayoutColumn(split, false);
    wm_block_file_close_cancel_button(block, post_action);
  }
  else {
    /* macOS and Linux standard layout. */
    uiLayout *col = uiLayoutColumn(split, false);
    wm_block_file_close_discard_button(block, post_action);

    col = uiLayoutColumn(split, false);
    uiItemS(col);

    col = uiLayoutColumn(split, false);
    wm_block_file_close_cancel_button(block, post_action);

    col = uiLayoutColumn(split, false);
    wm_block_file_close_save_button(block, post_action);
  }

  UI_block_bounds_set_centered(block, 10);
  return block;
}

static void free_post_file_close_action(void *arg)
{
  wmGenericCallback *action = (wmGenericCallback *)arg;
  WM_generic_callback_free(action);
}

void wm_close_file_dialog(bContext *C, wmGenericCallback *post_action)
{
  if (!UI_popup_block_name_exists(C, close_file_dialog_name)) {
    UI_popup_block_invoke(
        C, block_create__close_file_dialog, post_action, free_post_file_close_action);
  }
  else {
    WM_generic_callback_free(post_action);
  }
}

bool wm_file_or_image_is_modified(const bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  return !wm->file_saved || ED_image_should_save_modified(C);
}

/** \} */
