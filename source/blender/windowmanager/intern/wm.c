/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Internal functions for managing UI registerable types (operator, UI and menu types).
 *
 * Also Blender's main event loop (WM_main).
 */

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include <stddef.h>
#include <string.h>

#include "BLI_ghash.h"
#include "BLI_sys_types.h"

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_draw.h"
#include "wm_event_system.h"
#include "wm_window.h"
#ifdef WITH_XR_OPENXR
#  include "wm_xr.h"
#endif

#include "BKE_undo_system.h"
#include "ED_screen.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#  include "BPY_extern_run.h"
#endif

#include "BLO_read_write.h"

/* ****************************************************** */

static void window_manager_free_data(ID *id)
{
  wm_close_and_free(NULL, (wmWindowManager *)id);
}

static void window_manager_foreach_id(ID *id, LibraryForeachIDData *data)
{
  wmWindowManager *wm = (wmWindowManager *)id;

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, win->scene, IDWALK_CB_USER_ONE);

    /* This pointer can be NULL during old files reading, better be safe than sorry. */
    if (win->workspace_hook != NULL) {
      ID *workspace = (ID *)BKE_workspace_active_get(win->workspace_hook);
      BKE_lib_query_foreachid_process(data, &workspace, IDWALK_CB_USER);
      /* Allow callback to set a different workspace. */
      BKE_workspace_active_set(win->workspace_hook, (WorkSpace *)workspace);
      if (BKE_lib_query_foreachid_iter_stop(data)) {
        return;
      }

      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, win->unpinned_scene, IDWALK_CB_NOP);
    }

    if (BKE_lib_query_foreachid_process_flags_get(data) & IDWALK_INCLUDE_UI) {
      LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
        BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data,
                                                BKE_screen_foreach_id_screen_area(data, area));
      }
    }
  }

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(
      data, wm->xr.session_settings.base_pose_object, IDWALK_CB_USER_ONE);
}

static void write_wm_xr_data(BlendWriter *writer, wmXrData *xr_data)
{
  BKE_screen_view3d_shading_blend_write(writer, &xr_data->session_settings.shading);
}

static void window_manager_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  wmWindowManager *wm = (wmWindowManager *)id;

  BLO_write_id_struct(writer, wmWindowManager, id_address, &wm->id);
  BKE_id_blend_write(writer, &wm->id);
  write_wm_xr_data(writer, &wm->xr);

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    /* update deprecated screen member (for so loading in 2.7x uses the correct screen) */
    win->screen = BKE_workspace_active_screen_get(win->workspace_hook);

    BLO_write_struct(writer, wmWindow, win);
    BLO_write_struct(writer, WorkSpaceInstanceHook, win->workspace_hook);
    BLO_write_struct(writer, Stereo3dFormat, win->stereo3d_format);

    BKE_screen_area_map_blend_write(writer, &win->global_areas);

    /* data is written, clear deprecated data again */
    win->screen = NULL;
  }
}

static void direct_link_wm_xr_data(BlendDataReader *reader, wmXrData *xr_data)
{
  BKE_screen_view3d_shading_blend_read_data(reader, &xr_data->session_settings.shading);
}

static void window_manager_blend_read_data(BlendDataReader *reader, ID *id)
{
  wmWindowManager *wm = (wmWindowManager *)id;

  id_us_ensure_real(&wm->id);
  BLO_read_list(reader, &wm->windows);

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    BLO_read_data_address(reader, &win->parent);

    WorkSpaceInstanceHook *hook = win->workspace_hook;
    BLO_read_data_address(reader, &win->workspace_hook);

    /* This will be NULL for any pre-2.80 blend file. */
    if (win->workspace_hook != NULL) {
      /* We need to restore a pointer to this later when reading workspaces,
       * so store in global oldnew-map.
       * Note that this is only needed for versioning of older .blend files now. */
      BLO_read_data_globmap_add(reader, hook, win->workspace_hook);
      /* Cleanup pointers to data outside of this data-block scope. */
      win->workspace_hook->act_layout = NULL;
      win->workspace_hook->temp_workspace_store = NULL;
      win->workspace_hook->temp_layout_store = NULL;
    }

    BKE_screen_area_map_blend_read_data(reader, &win->global_areas);

    win->ghostwin = NULL;
    win->gpuctx = NULL;
    win->eventstate = NULL;
    win->event_last_handled = NULL;
    win->cursor_keymap_status = NULL;
#if defined(WIN32) || defined(__APPLE__)
    win->ime_data = NULL;
#endif

    BLI_listbase_clear(&win->event_queue);
    BLI_listbase_clear(&win->handlers);
    BLI_listbase_clear(&win->modalhandlers);
    BLI_listbase_clear(&win->gesture);

    win->active = 0;

    win->cursor = 0;
    win->lastcursor = 0;
    win->modalcursor = 0;
    win->grabcursor = 0;
    win->addmousemove = true;
    win->event_queue_check_click = 0;
    win->event_queue_check_drag = 0;
    win->event_queue_check_drag_handled = 0;
    win->event_queue_consecutive_gesture_type = 0;
    win->event_queue_consecutive_gesture_data = NULL;
    BLO_read_data_address(reader, &win->stereo3d_format);

    /* Multi-view always fallback to anaglyph at file opening
     * otherwise quad-buffer saved files can break Blender. */
    if (win->stereo3d_format) {
      win->stereo3d_format->display_mode = S3D_DISPLAY_ANAGLYPH;
    }
  }

  direct_link_wm_xr_data(reader, &wm->xr);

  BLI_listbase_clear(&wm->timers);
  BLI_listbase_clear(&wm->operators);
  BLI_listbase_clear(&wm->paintcursors);
  BLI_listbase_clear(&wm->notifier_queue);
  wm->notifier_queue_set = NULL;
  BKE_reports_init(&wm->reports, RPT_STORE);

  BLI_listbase_clear(&wm->keyconfigs);
  wm->defaultconf = NULL;
  wm->addonconf = NULL;
  wm->userconf = NULL;
  wm->undo_stack = NULL;

  wm->message_bus = NULL;

  wm->xr.runtime = NULL;

  BLI_listbase_clear(&wm->jobs);
  BLI_listbase_clear(&wm->drags);

  wm->windrawable = NULL;
  wm->winactive = NULL;
  wm->init_flag = 0;
  wm->op_undo_depth = 0;
  wm->is_interface_locked = 0;
}

static void lib_link_wm_xr_data(BlendLibReader *reader, ID *parent_id, wmXrData *xr_data)
{
  BLO_read_id_address(reader, parent_id, &xr_data->session_settings.base_pose_object);
}

static void lib_link_workspace_instance_hook(BlendLibReader *reader,
                                             WorkSpaceInstanceHook *hook,
                                             ID *id)
{
  WorkSpace *workspace = BKE_workspace_active_get(hook);
  BLO_read_id_address(reader, id, &workspace);

  BKE_workspace_active_set(hook, workspace);
}

static void window_manager_blend_read_lib(BlendLibReader *reader, ID *id)
{
  wmWindowManager *wm = (wmWindowManager *)id;

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (win->workspace_hook) { /* NULL for old files */
      lib_link_workspace_instance_hook(reader, win->workspace_hook, id);
    }
    BLO_read_id_address(reader, id, &win->scene);
    /* deprecated, but needed for versioning (will be NULL'ed then) */
    BLO_read_id_address(reader, id, &win->screen);

    /* The unpinned scene is a UI->Scene-data pointer, and should be NULL'ed on linking (like
     * WorkSpace.pin_scene). But the WindowManager ID (owning the window) is never linked. */
    BLI_assert(!ID_IS_LINKED(id));
    BLO_read_id_address(reader, id, &win->unpinned_scene);

    LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
      BKE_screen_area_blend_read_lib(reader, id, area);
    }

    lib_link_wm_xr_data(reader, id, &wm->xr);
  }
}

IDTypeInfo IDType_ID_WM = {
    .id_code = ID_WM,
    .id_filter = FILTER_ID_WM,
    .main_listbase_index = INDEX_ID_WM,
    .struct_size = sizeof(wmWindowManager),
    .name = "WindowManager",
    .name_plural = "window_managers",
    .translation_context = BLT_I18NCONTEXT_ID_WINDOWMANAGER,
    .flags = IDTYPE_FLAGS_NO_COPY | IDTYPE_FLAGS_NO_LIBLINKING | IDTYPE_FLAGS_NO_ANIMDATA |
             IDTYPE_FLAGS_NO_MEMFILE_UNDO,
    .asset_type_info = NULL,

    .init_data = NULL,
    .copy_data = NULL,
    .free_data = window_manager_free_data,
    .make_local = NULL,
    .foreach_id = window_manager_foreach_id,
    .foreach_cache = NULL,
    .foreach_path = NULL,
    .owner_pointer_get = NULL,

    .blend_write = window_manager_blend_write,
    .blend_read_data = window_manager_blend_read_data,
    .blend_read_lib = window_manager_blend_read_lib,
    .blend_read_expand = NULL,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

#define MAX_OP_REGISTERED 32

void WM_operator_free(wmOperator *op)
{

#ifdef WITH_PYTHON
  if (op->py_instance) {
    /* Do this first in case there are any __del__ functions or similar that use properties. */
    BPY_DECREF_RNA_INVALIDATE(op->py_instance);
  }
#endif

  if (op->ptr) {
    op->properties = op->ptr->data;
    MEM_freeN(op->ptr);
  }

  if (op->properties) {
    IDP_FreeProperty(op->properties);
  }

  if (op->reports && (op->reports->flag & RPT_FREE)) {
    BKE_reports_clear(op->reports);
    MEM_freeN(op->reports);
  }

  if (op->macro.first) {
    wmOperator *opm, *opmnext;
    for (opm = op->macro.first; opm; opm = opmnext) {
      opmnext = opm->next;
      WM_operator_free(opm);
    }
  }

  MEM_freeN(op);
}

void WM_operator_free_all_after(wmWindowManager *wm, wmOperator *op)
{
  op = op->next;
  while (op != NULL) {
    wmOperator *op_next = op->next;
    BLI_remlink(&wm->operators, op);
    WM_operator_free(op);
    op = op_next;
  }
}

void WM_operator_type_set(wmOperator *op, wmOperatorType *ot)
{
  /* Not supported for Python. */
  BLI_assert(op->py_instance == NULL);

  op->type = ot;
  op->ptr->type = ot->srna;

  /* Ensure compatible properties. */
  if (op->properties) {
    PointerRNA ptr;
    WM_operator_properties_create_ptr(&ptr, ot);

    WM_operator_properties_default(&ptr, false);

    if (ptr.data) {
      IDP_SyncGroupTypes(op->properties, ptr.data, true);
    }

    WM_operator_properties_free(&ptr);
  }
}

static void wm_reports_free(wmWindowManager *wm)
{
  BKE_reports_clear(&wm->reports);
  WM_event_remove_timer(wm, NULL, wm->reports.reporttimer);
}

void wm_operator_register(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  int tot = 0;

  BLI_addtail(&wm->operators, op);

  /* Only count registered operators. */
  while (op) {
    wmOperator *op_prev = op->prev;
    if (op->type->flag & OPTYPE_REGISTER) {
      tot += 1;
    }
    if (tot > MAX_OP_REGISTERED) {
      BLI_remlink(&wm->operators, op);
      WM_operator_free(op);
    }
    op = op_prev;
  }

  /* So the console is redrawn. */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO_REPORT, NULL);
  WM_event_add_notifier(C, NC_WM | ND_HISTORY, NULL);
}

void WM_operator_stack_clear(wmWindowManager *wm)
{
  wmOperator *op;

  while ((op = BLI_pophead(&wm->operators))) {
    WM_operator_free(op);
  }

  WM_main_add_notifier(NC_WM | ND_HISTORY, NULL);
}

void WM_operator_handlers_clear(wmWindowManager *wm, wmOperatorType *ot)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    ListBase *lb[2] = {&win->handlers, &win->modalhandlers};
    for (int i = 0; i < ARRAY_SIZE(lb); i++) {
      LISTBASE_FOREACH (wmEventHandler *, handler_base, lb[i]) {
        if (handler_base->type == WM_HANDLER_TYPE_OP) {
          wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
          if (handler->op && handler->op->type == ot) {
            /* don't run op->cancel because it needs the context,
             * assume whoever unregisters the operator will cleanup */
            handler->head.flag |= WM_HANDLER_DO_FREE;
            WM_operator_free(handler->op);
            handler->op = NULL;
          }
        }
      }
    }
  }
}

/* ****************************************** */

void WM_keyconfig_reload(bContext *C)
{
  if (CTX_py_init_get(C) && !G.background) {
#ifdef WITH_PYTHON
    BPY_run_string_eval(C, (const char *[]){"bpy", NULL}, "bpy.utils.keyconfig_init()");
#endif
  }
}

void WM_keyconfig_init(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* Create standard key configs. */
  if (wm->defaultconf == NULL) {
    /* Keep lowercase to match the preset filename. */
    wm->defaultconf = WM_keyconfig_new(wm, WM_KEYCONFIG_STR_DEFAULT, false);
  }
  if (wm->addonconf == NULL) {
    wm->addonconf = WM_keyconfig_new(wm, WM_KEYCONFIG_STR_DEFAULT " addon", false);
  }
  if (wm->userconf == NULL) {
    wm->userconf = WM_keyconfig_new(wm, WM_KEYCONFIG_STR_DEFAULT " user", false);
  }

  /* Initialize only after python init is done, for keymaps that use python operators. */
  if (CTX_py_init_get(C) && (wm->init_flag & WM_INIT_FLAG_KEYCONFIG) == 0) {
    /* create default key config, only initialize once,
     * it's persistent across sessions */
    if (!(wm->defaultconf->flag & KEYCONF_INIT_DEFAULT)) {
      wm_window_keymap(wm->defaultconf);
      ED_spacetypes_keymap(wm->defaultconf);

      WM_keyconfig_reload(C);

      wm->defaultconf->flag |= KEYCONF_INIT_DEFAULT;
    }

    /* Harmless, but no need to update in background mode. */
    if (!G.background) {
      WM_keyconfig_update_tag(NULL, NULL);
    }
    WM_keyconfig_update(wm);

    wm->init_flag |= WM_INIT_FLAG_KEYCONFIG;
  }
}

void WM_check(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);

  /* WM context. */
  if (wm == NULL) {
    wm = bmain->wm.first;
    CTX_wm_manager_set(C, wm);
  }

  if (wm == NULL || BLI_listbase_is_empty(&wm->windows)) {
    return;
  }

  /* Run before loading the keyconfig. */
  if (wm->message_bus == NULL) {
    wm->message_bus = WM_msgbus_create();
  }

  if (!G.background) {
    /* Case: file-read. */
    if ((wm->init_flag & WM_INIT_FLAG_WINDOW) == 0) {
      WM_keyconfig_init(C);
      WM_file_autosave_init(wm);
    }

    /* Case: no open windows at all, for old file reads. */
    wm_window_ghostwindows_ensure(wm);
  }

  /* Case: file-read. */
  /* NOTE: this runs in background mode to set the screen context cb. */
  if ((wm->init_flag & WM_INIT_FLAG_WINDOW) == 0) {
    ED_screens_init(bmain, wm);
    wm->init_flag |= WM_INIT_FLAG_WINDOW;
  }
}

void wm_clear_default_size(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* WM context. */
  if (wm == NULL) {
    wm = CTX_data_main(C)->wm.first;
    CTX_wm_manager_set(C, wm);
  }

  if (wm == NULL || BLI_listbase_is_empty(&wm->windows)) {
    return;
  }

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    win->sizex = 0;
    win->sizey = 0;
    win->posx = 0;
    win->posy = 0;
  }
}

void wm_add_default(Main *bmain, bContext *C)
{
  wmWindowManager *wm = BKE_libblock_alloc(bmain, ID_WM, "WinMan", 0);
  wmWindow *win;
  bScreen *screen = CTX_wm_screen(C); /* XXX from file read hrmf */
  WorkSpace *workspace;
  WorkSpaceLayout *layout = BKE_workspace_layout_find_global(bmain, screen, &workspace);

  CTX_wm_manager_set(C, wm);
  win = wm_window_new(bmain, wm, NULL, false);
  win->scene = CTX_data_scene(C);
  STRNCPY(win->view_layer_name, CTX_data_view_layer(C)->name);
  BKE_workspace_active_set(win->workspace_hook, workspace);
  BKE_workspace_active_layout_set(win->workspace_hook, win->winid, workspace, layout);
  screen->winid = win->winid;

  wm->winactive = win;
  wm->file_saved = 1;
  wm_window_make_drawable(wm, win);
}

void wm_close_and_free(bContext *C, wmWindowManager *wm)
{
  if (wm->autosavetimer) {
    wm_autosave_timer_end(wm);
  }

#ifdef WITH_XR_OPENXR
  /* May send notifier, so do before freeing notifier queue. */
  wm_xr_exit(wm);
#endif

  wmWindow *win;
  while ((win = BLI_pophead(&wm->windows))) {
    /* Prevent draw clear to use screen. */
    BKE_workspace_active_set(win->workspace_hook, NULL);
    wm_window_free(C, wm, win);
  }

  wmOperator *op;
  while ((op = BLI_pophead(&wm->operators))) {
    WM_operator_free(op);
  }

  wmKeyConfig *keyconf;
  while ((keyconf = BLI_pophead(&wm->keyconfigs))) {
    WM_keyconfig_free(keyconf);
  }

  BLI_freelistN(&wm->notifier_queue);
  if (wm->notifier_queue_set) {
    BLI_gset_free(wm->notifier_queue_set, NULL);
    wm->notifier_queue_set = NULL;
  }

  if (wm->message_bus != NULL) {
    WM_msgbus_destroy(wm->message_bus);
  }

#ifdef WITH_PYTHON
  BPY_callback_wm_free(wm);
#endif
  BLI_freelistN(&wm->paintcursors);

  WM_drag_free_list(&wm->drags);

  wm_reports_free(wm);

  /* NOTE(@ideasman42): typically timers are associated with windows and timers will have been
   * freed when the windows are removed. However timers can be created which don't have windows
   * and in this case it's necessary to free them on exit, see: #109953. */
  WM_event_timers_free_all(wm);

  if (wm->undo_stack) {
    BKE_undosys_stack_destroy(wm->undo_stack);
    wm->undo_stack = NULL;
  }

  if (C && CTX_wm_manager(C) == wm) {
    CTX_wm_manager_set(C, NULL);
  }
}

void WM_main(bContext *C)
{
  /* Single refresh before handling events.
   * This ensures we don't run operators before the depsgraph has been evaluated. */
  wm_event_do_refresh_wm_and_depsgraph(C);

  while (1) {

    /* Get events from ghost, handle window events, add to window queues. */
    wm_window_process_events(C);

    /* Per window, all events to the window, screen, area and region handlers. */
    wm_event_do_handlers(C);

    /* Events have left notes about changes, we handle and cache it. */
    wm_event_do_notifiers(C);

    /* Execute cached changes draw. */
    wm_draw_update(C);
  }
}
