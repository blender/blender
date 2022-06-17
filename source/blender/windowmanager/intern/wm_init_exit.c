/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup wm
 *
 * Manage initializing resources and correctly shutting down.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_genfile.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_timer.h"
#include "BLI_utildefines.h"

#include "BLO_undofile.h"
#include "BLO_writefile.h"

#include "BKE_blender.h"
#include "BKE_blendfile.h"
#include "BKE_callbacks.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_keyconfig.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_mball_tessellate.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sound.h"
#include "BKE_vfont.h"

#include "BKE_addon.h"
#include "BKE_appdir.h"
#include "BKE_mask.h"     /* free mask clipboard */
#include "BKE_material.h" /* BKE_material_copybuf_clear */
#include "BKE_studiolight.h"
#include "BKE_subdiv.h"
#include "BKE_tracking.h" /* free tracking clipboard */

#include "RE_engine.h"
#include "RE_pipeline.h" /* RE_ free stuff */

#include "SEQ_clipboard.h" /* free seq clipboard */

#include "IMB_thumbs.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#  include "BPY_extern_python.h"
#  include "BPY_extern_run.h"
#endif

#include "GHOST_C-api.h"
#include "GHOST_Path-api.h"

#include "RNA_define.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_cursors.h"
#include "wm_event_system.h"
#include "wm_files.h"
#include "wm_platform_support.h"
#include "wm_surface.h"
#include "wm_window.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_asset.h"
#include "ED_gpencil.h"
#include "ED_keyframes_edit.h"
#include "ED_keyframing.h"
#include "ED_node.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_undo.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "BLF_api.h"
#include "BLT_lang.h"
#include "UI_interface.h"
#include "UI_resources.h"

#include "GPU_context.h"
#include "GPU_init_exit.h"
#include "GPU_material.h"

#include "COM_compositor.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DRW_engine.h"

CLG_LOGREF_DECLARE_GLOBAL(WM_LOG_OPERATORS, "wm.operator");
CLG_LOGREF_DECLARE_GLOBAL(WM_LOG_HANDLERS, "wm.handler");
CLG_LOGREF_DECLARE_GLOBAL(WM_LOG_EVENTS, "wm.event");
CLG_LOGREF_DECLARE_GLOBAL(WM_LOG_KEYMAPS, "wm.keymap");
CLG_LOGREF_DECLARE_GLOBAL(WM_LOG_TOOLS, "wm.tool");
CLG_LOGREF_DECLARE_GLOBAL(WM_LOG_MSGBUS_PUB, "wm.msgbus.pub");
CLG_LOGREF_DECLARE_GLOBAL(WM_LOG_MSGBUS_SUB, "wm.msgbus.sub");

static void wm_init_reports(bContext *C)
{
  ReportList *reports = CTX_wm_reports(C);

  BLI_assert(!reports || BLI_listbase_is_empty(&reports->list));

  BKE_reports_init(reports, RPT_STORE);
}
static void wm_free_reports(wmWindowManager *wm)
{
  BKE_reports_clear(&wm->reports);
}

static bool wm_start_with_console = false;

void WM_init_state_start_with_console_set(bool value)
{
  wm_start_with_console = value;
}

/**
 * Since we cannot know in advance if we will require the draw manager
 * context when starting blender in background mode (specially true with
 * scripts) we defer the ghost initialization the most as possible
 * so that it does not break anything that can run in headless mode (as in
 * without display server attached).
 */
static bool opengl_is_init = false;

void WM_init_opengl(void)
{
  /* Must be called only once. */
  BLI_assert(opengl_is_init == false);

  if (G.background) {
    /* Ghost is still not initialized elsewhere in background mode. */
    wm_ghost_init(NULL);
  }

  /* Needs to be first to have an OpenGL context bound. */
  DRW_opengl_context_create();

  GPU_init();

  GPU_pass_cache_init();

  opengl_is_init = true;
}

static void sound_jack_sync_callback(Main *bmain, int mode, double time)
{
  /* Ugly: Blender doesn't like it when the animation is played back during rendering. */
  if (G.is_rendering) {
    return;
  }

  wmWindowManager *wm = bmain->wm.first;

  LISTBASE_FOREACH (wmWindow *, window, &wm->windows) {
    Scene *scene = WM_window_get_active_scene(window);
    if ((scene->audio.flag & AUDIO_SYNC) == 0) {
      continue;
    }
    ViewLayer *view_layer = WM_window_get_active_view_layer(window);
    Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer);
    if (depsgraph == NULL) {
      continue;
    }
    BKE_sound_lock();
    Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
    BKE_sound_jack_scene_update(scene_eval, mode, time);
    BKE_sound_unlock();
  }
}

void WM_init(bContext *C, int argc, const char **argv)
{

  if (!G.background) {
    wm_ghost_init(C); /* NOTE: it assigns C to ghost! */
    wm_init_cursor_data();
    BKE_sound_jack_sync_callback_set(sound_jack_sync_callback);
  }

  GHOST_CreateSystemPaths();

  BKE_addon_pref_type_init();
  BKE_keyconfig_pref_type_init();

  wm_operatortype_init();
  wm_operatortypes_register();

  WM_paneltype_init(); /* Lookup table only. */
  WM_menutype_init();
  WM_uilisttype_init();
  wm_gizmotype_init();
  wm_gizmogrouptype_init();

  ED_undosys_type_init();

  BKE_library_callback_free_notifier_reference_set(WM_main_remove_notifier_reference);
  BKE_region_callback_free_gizmomap_set(wm_gizmomap_remove);
  BKE_region_callback_refresh_tag_gizmomap_set(WM_gizmomap_tag_refresh);
  BKE_library_callback_remap_editor_id_reference_set(WM_main_remap_editor_id_reference);
  BKE_spacedata_callback_id_remap_set(ED_spacedata_id_remap_single);
  DEG_editors_set_update_cb(ED_render_id_flush_update, ED_render_scene_update);

  ED_spacetypes_init();

  ED_node_init_butfuncs();

  BLF_init();

  BLT_lang_init();
  /* Must call first before doing any `.blend` file reading,
   * since versioning code may create new IDs. See T57066. */
  BLT_lang_set(NULL);

  /* Init icons before reading .blend files for preview icons, which can
   * get triggered by the depsgraph. This is also done in background mode
   * for scripts that do background processing with preview icons. */
  BKE_icons_init(BIFICONID_LAST);

  /* Reports can't be initialized before the window-manager,
   * but keep before file reading, since that may report errors */
  wm_init_reports(C);

  WM_msgbus_types_init();

  /* Studio-lights needs to be init before we read the home-file,
   * otherwise the versioning cannot find the default studio-light. */
  BKE_studiolight_init();

  BLI_assert((G.fileflags & G_FILE_NO_UI) == 0);

  /**
   * NOTE(@campbellbarton): Startup file and order of initialization.
   *
   * Loading #BLENDER_STARTUP_FILE, #BLENDER_USERPREF_FILE, starting Python and other sub-systems,
   * have inter-dependencies, for example.
   *
   * - Some sub-systems depend on the preferences (initializing icons depend on the theme).
   * - Add-ons depends on the preferences to know what has been enabled.
   * - Add-ons depends on the window-manger to register their key-maps.
   * - Evaluating the startup file depends on Python for animation-drivers (see T89046).
   * - Starting Python depends on the startup file so key-maps can be added in the window-manger.
   *
   * Loading preferences early, then application subsystems and finally the startup data would
   * simplify things if it weren't for key-maps being part of the window-manager
   * which is blend file data.
   * Creating a dummy window-manager early, or moving the key-maps into the preferences
   * would resolve this and may be worth looking into long-term, see: D12184 for details.
   */
  struct wmFileReadPost_Params *params_file_read_post = NULL;
  wm_homefile_read_ex(C,
                      &(const struct wmHomeFileRead_Params){
                          .use_data = true,
                          .use_userdef = true,
                          .use_factory_settings = G.factory_startup,
                          .use_empty_data = false,
                          .filepath_startup_override = NULL,
                          .app_template_override = WM_init_state_app_template_get(),
                      },
                      NULL,
                      &params_file_read_post);

  /* NOTE: leave `G_MAIN->filepath` set to an empty string since this
   * matches behavior after loading a new file. */
  BLI_assert(G_MAIN->filepath[0] == '\0');

  /* Call again to set from preferences. */
  BLT_lang_set(NULL);

  /* For file-system. Called here so can include user preference paths if needed. */
  ED_file_init();

  /* That one is generated on demand, we need to be sure it's clear on init. */
  IMB_thumb_clear_translations();

  if (!G.background) {

#ifdef WITH_INPUT_NDOF
    /* Sets 3D mouse dead-zone. */
    WM_ndof_deadzone_set(U.ndof_deadzone);
#endif
    WM_init_opengl();

    if (!WM_platform_support_perform_checks()) {
      /* No attempt to avoid memory leaks here. */
      exit(-1);
    }

    UI_init();
  }

  BKE_subdiv_init();

  ED_spacemacros_init();

#ifdef WITH_PYTHON
  BPY_python_start(C, argc, argv);
  BPY_python_reset(C);
#else
  UNUSED_VARS(argc, argv);
#endif

  if (!G.background) {
    if (wm_start_with_console) {
      GHOST_setConsoleWindowState(GHOST_kConsoleWindowStateShow);
    }
    else {
      GHOST_setConsoleWindowState(GHOST_kConsoleWindowStateHideForNonConsoleLaunch);
    }
  }

  BKE_material_copybuf_clear();
  ED_render_clear_mtex_copybuf();

  wm_history_file_read();

  BLI_strncpy(G.lib, BKE_main_blendfile_path_from_global(), sizeof(G.lib));

  wm_homefile_read_post(C, params_file_read_post);
}

void WM_init_splash(bContext *C)
{
  if ((U.uiflag & USER_SPLASH_DISABLE) == 0) {
    wmWindowManager *wm = CTX_wm_manager(C);
    wmWindow *prevwin = CTX_wm_window(C);

    if (wm->windows.first) {
      CTX_wm_window_set(C, wm->windows.first);
      WM_operator_name_call(C, "WM_OT_splash", WM_OP_INVOKE_DEFAULT, NULL, NULL);
      CTX_wm_window_set(C, prevwin);
    }
  }
}

/* free strings of open recent files */
static void free_openrecent(void)
{
  LISTBASE_FOREACH (RecentFile *, recent, &G.recent_files) {
    MEM_freeN(recent->filepath);
  }

  BLI_freelistN(&(G.recent_files));
}

#ifdef WIN32
/* Read console events until there is a key event. Also returns on any error. */
static void wait_for_console_key(void)
{
  HANDLE hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);

  if (!ELEM(hConsoleInput, NULL, INVALID_HANDLE_VALUE) && FlushConsoleInputBuffer(hConsoleInput)) {
    for (;;) {
      INPUT_RECORD buffer;
      DWORD ignored;

      if (!ReadConsoleInput(hConsoleInput, &buffer, 1, &ignored)) {
        break;
      }

      if (buffer.EventType == KEY_EVENT) {
        break;
      }
    }
  }
}
#endif

static int wm_exit_handler(bContext *C, const wmEvent *event, void *userdata)
{
  WM_exit(C);

  UNUSED_VARS(event, userdata);
  return WM_UI_HANDLER_BREAK;
}

void wm_exit_schedule_delayed(const bContext *C)
{
  /* What we do here is a little bit hacky, but quite simple and doesn't require bigger
   * changes: Add a handler wrapping WM_exit() to cause a delayed call of it. */

  wmWindow *win = CTX_wm_window(C);

  /* Use modal UI handler for now.
   * Could add separate WM handlers or so, but probably not worth it. */
  WM_event_add_ui_handler(C, &win->modalhandlers, wm_exit_handler, NULL, NULL, 0);
  WM_event_add_mousemove(win); /* ensure handler actually gets called */
}

void WM_exit_ex(bContext *C, const bool do_python)
{
  wmWindowManager *wm = C ? CTX_wm_manager(C) : NULL;

  /* first wrap up running stuff, we assume only the active WM is running */
  /* modal handlers are on window level freed, others too? */
  /* NOTE: same code copied in `wm_files.c`. */
  if (C && wm) {
    if (!G.background) {
      struct MemFile *undo_memfile = wm->undo_stack ?
                                         ED_undosys_stack_memfile_get_active(wm->undo_stack) :
                                         NULL;
      if (undo_memfile != NULL) {
        /* save the undo state as quit.blend */
        Main *bmain = CTX_data_main(C);
        char filepath[FILE_MAX];
        bool has_edited;
        const int fileflags = G.fileflags & ~G_FILE_COMPRESS;

        BLI_join_dirfile(filepath, sizeof(filepath), BKE_tempdir_base(), BLENDER_QUIT_FILE);

        has_edited = ED_editors_flush_edits(bmain);

        if ((has_edited &&
             BLO_write_file(
                 bmain, filepath, fileflags, &(const struct BlendFileWriteParams){0}, NULL)) ||
            (BLO_memfile_write_file(undo_memfile, filepath))) {
          printf("Saved session recovery to '%s'\n", filepath);
        }
      }
    }

    WM_jobs_kill_all(wm);

    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      CTX_wm_window_set(C, win); /* needed by operator close callbacks */
      WM_event_remove_handlers(C, &win->handlers);
      WM_event_remove_handlers(C, &win->modalhandlers);
      ED_screen_exit(C, win, WM_window_get_active_screen(win));
    }

    if (!G.background) {
      if ((U.pref_flag & USER_PREF_FLAG_SAVE) && ((G.f & G_FLAG_USERPREF_NO_SAVE_ON_EXIT) == 0)) {
        if (U.runtime.is_dirty) {
          BKE_blendfile_userdef_write_all(NULL);
        }
      }
      /* Free the callback data used on file-open
       * (will be set when a recover operation has run). */
      wm_test_autorun_revert_action_set(NULL, NULL);
    }
  }

#if defined(WITH_PYTHON) && !defined(WITH_PYTHON_MODULE)
  /* Without this, we there isn't a good way to manage false-positive resource leaks
   * where a #PyObject references memory allocated with guarded-alloc, T71362.
   *
   * This allows add-ons to free resources when unregistered (which is good practice anyway).
   *
   * Don't run this code when built as a Python module as this runs when Python is in the
   * process of shutting down, where running a snippet like this will crash, see T82675.
   * Instead use the `atexit` module, installed by #BPY_python_start */
  BPY_run_string_eval(C, (const char *[]){"addon_utils", NULL}, "addon_utils.disable_all()");
#endif

  BLI_timer_free();

  WM_paneltype_clear();

  BKE_addon_pref_type_free();
  BKE_keyconfig_pref_type_free();
  BKE_materials_exit();

  wm_operatortype_free();
  wm_surfaces_free();
  wm_dropbox_free();
  WM_menutype_free();

  /* all non-screen and non-space stuff editors did, like editmode */
  if (C) {
    Main *bmain = CTX_data_main(C);
    ED_editors_exit(bmain, true);
  }

  ED_undosys_type_free();

  free_openrecent();

  BKE_mball_cubeTable_free();

  /* render code might still access databases */
  RE_FreeAllRender();
  RE_engines_exit();

  ED_preview_free_dbase(); /* frees a Main dbase, before BKE_blender_free! */
  ED_preview_restart_queue_free();
  ED_assetlist_storage_exit();

  if (wm) {
    /* Before BKE_blender_free! - since the ListBases get freed there. */
    wm_free_reports(wm);
  }

  SEQ_clipboard_free(); /* sequencer.c */
  BKE_tracking_clipboard_free();
  BKE_mask_clipboard_free();
  BKE_vfont_clipboard_free();
  BKE_node_clipboard_free();

#ifdef WITH_COMPOSITOR
  COM_deinitialize();
#endif

  BKE_subdiv_exit();

  if (opengl_is_init) {
    BKE_image_free_unused_gpu_textures();
  }

  BKE_blender_free(); /* blender.c, does entire library and spacetypes */
                      //  BKE_material_copybuf_free();

  /* Free the GPU subdivision data after the database to ensure that subdivision structs used by
   * the modifiers were garbage collected. */
  if (opengl_is_init) {
    DRW_subdiv_free();
  }

  ANIM_fcurves_copybuf_free();
  ANIM_drivers_copybuf_free();
  ANIM_driver_vars_copybuf_free();
  ANIM_fmodifiers_copybuf_free();
  ED_gpencil_anim_copybuf_free();
  ED_gpencil_strokes_copybuf_free();

  /* free gizmo-maps after freeing blender,
   * so no deleted data get accessed during cleaning up of areas. */
  wm_gizmomaptypes_free();
  wm_gizmogrouptype_free();
  wm_gizmotype_free();
  /* Same for UI-list types. */
  WM_uilisttype_free();

  BLF_exit();

  if (opengl_is_init) {
    DRW_opengl_context_enable_ex(false);
    GPU_pass_cache_free();
    GPU_exit();
    DRW_opengl_context_disable_ex(false);
    DRW_opengl_context_destroy();
  }

  BLT_lang_free();

  ANIM_keyingset_infos_exit();

  //  free_txt_data();

#ifdef WITH_PYTHON
  /* option not to close python so we can use 'atexit' */
  if (do_python && ((C == NULL) || CTX_py_init_get(C))) {
    /* NOTE: (old note)
     * before BKE_blender_free so Python's garbage-collection happens while library still exists.
     * Needed at least for a rare crash that can happen in python-drivers.
     *
     * Update for Blender 2.5, move after #BKE_blender_free because Blender now holds references
     * to #PyObject's so #Py_DECREF'ing them after Python ends causes bad problems every time
     * the python-driver bug can be fixed if it happens again we can deal with it then. */
    BPY_python_end();
  }
#else
  (void)do_python;
#endif

  ED_file_exit(); /* for fsmenu */

  UI_exit();
  BKE_blender_userdef_data_free(&U, false);

  RNA_exit(); /* should be after BPY_python_end so struct python slots are cleared */

  GPU_backend_exit();

  wm_ghost_exit();

  CTX_free(C);

  GHOST_DisposeSystemPaths();

  DNA_sdna_current_free();

  BLI_threadapi_exit();
  BLI_task_scheduler_exit();

  /* No need to call this early, rather do it late so that other
   * pieces of Blender using sound may exit cleanly, see also T50676. */
  BKE_sound_exit();

  BKE_appdir_exit();
  CLG_exit();

  BKE_blender_atexit();

  wm_autosave_delete();

  BKE_tempdir_session_purge();
}

void WM_exit(bContext *C)
{
  WM_exit_ex(C, true);

  printf("\nBlender quit\n");

#ifdef WIN32
  /* ask user to press a key when in debug mode */
  if (G.debug & G_DEBUG) {
    printf("Press any key to exit . . .\n\n");
    wait_for_console_key();
  }
#endif

  exit(G.is_break == true);
}

void WM_script_tag_reload(void)
{
  UI_interface_tag_script_reload();
}
