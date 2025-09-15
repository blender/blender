/* SPDX-FileCopyrightText: 2007 Blender Authors
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

#include <cstring>

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_message.hh"
#include "WM_types.hh"
#include "wm.hh"
#include "wm_draw.hh"
#include "wm_event_system.hh"
#include "wm_window.hh"
#ifdef WITH_XR_OPENXR
#  include "wm_xr.hh"
#endif

#include "BKE_undo_system.hh"
#include "ED_screen.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#  include "BPY_extern_run.hh"
#endif

#include "BLO_read_write.hh"

/* ****************************************************** */

static void window_manager_free_data(ID *id)
{
  wm_close_and_free(nullptr, (wmWindowManager *)id);
}

static void window_manager_foreach_id(ID *id, LibraryForeachIDData *data)
{
  wmWindowManager *wm = reinterpret_cast<wmWindowManager *>(id);
  const int flag = BKE_lib_query_foreachid_process_flags_get(data);

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, win->scene, IDWALK_CB_USER_ONE);

    /* This pointer can be nullptr during old files reading. */
    if (win->workspace_hook != nullptr) {
      ID *workspace = (ID *)BKE_workspace_active_get(win->workspace_hook);
      BKE_lib_query_foreachid_process(data, &workspace, IDWALK_CB_USER);
      /* Allow callback to set a different workspace. */
      BKE_workspace_active_set(win->workspace_hook, (WorkSpace *)workspace);
      if (BKE_lib_query_foreachid_iter_stop(data)) {
        return;
      }
    }

    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, win->unpinned_scene, IDWALK_CB_NOP);

    if (flag & IDWALK_INCLUDE_UI) {
      LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
        BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data,
                                                BKE_screen_foreach_id_screen_area(data, area));
      }
    }

    if (flag & IDWALK_DO_DEPRECATED_POINTERS) {
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, win->screen, IDWALK_CB_NOP);
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

  wm->runtime = nullptr;

  BLO_write_id_struct(writer, wmWindowManager, id_address, &wm->id);
  BKE_id_blend_write(writer, &wm->id);
  write_wm_xr_data(writer, &wm->xr);

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    /* Update deprecated screen member (for so loading in 2.7x uses the correct screen). */
    win->screen = BKE_workspace_active_screen_get(win->workspace_hook);

    BLO_write_struct(writer, wmWindow, win);
    BLO_write_struct(writer, WorkSpaceInstanceHook, win->workspace_hook);
    BLO_write_struct(writer, Stereo3dFormat, win->stereo3d_format);

    BKE_screen_area_map_blend_write(writer, &win->global_areas);

    /* Data is written, clear deprecated data again. */
    win->screen = nullptr;
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
  BLO_read_struct_list(reader, wmWindow, &wm->windows);

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    BLO_read_struct(reader, wmWindow, &win->parent);

    WorkSpaceInstanceHook *hook = win->workspace_hook;
    BLO_read_struct(reader, WorkSpaceInstanceHook, &win->workspace_hook);

    /* This will be nullptr for any pre-2.80 blend file. */
    if (win->workspace_hook != nullptr) {
      /* We need to restore a pointer to this later when reading workspaces,
       * so store in global oldnew-map.
       * Note that this is only needed for versioning of older .blend files now. */
      BLO_read_data_globmap_add(reader, hook, win->workspace_hook);
      /* Cleanup pointers to data outside of this data-block scope. */
      win->workspace_hook->act_layout = nullptr;
      win->workspace_hook->temp_workspace_store = nullptr;
      win->workspace_hook->temp_layout_store = nullptr;
    }

    BKE_screen_area_map_blend_read_data(reader, &win->global_areas);

    win->ghostwin = nullptr;
    win->gpuctx = nullptr;
    win->eventstate = nullptr;
    win->eventstate_prev_press_time_ms = 0;
    win->event_last_handled = nullptr;
    win->cursor_keymap_status = nullptr;

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
    win->event_queue_consecutive_gesture_type = EVENT_NONE;
    win->event_queue_consecutive_gesture_data = nullptr;
    BLO_read_struct(reader, Stereo3dFormat, &win->stereo3d_format);

    /* Multi-view always falls back to anaglyph at file opening
     * otherwise quad-buffer saved files can break Blender. */
    if (win->stereo3d_format) {
      win->stereo3d_format->display_mode = S3D_DISPLAY_ANAGLYPH;
    }
    win->runtime = MEM_new<blender::bke::WindowRuntime>(__func__);
  }

  direct_link_wm_xr_data(reader, &wm->xr);

  wm->xr.runtime = nullptr;

  wm->init_flag = 0;
  wm->op_undo_depth = 0;
  wm->extensions_updates = WM_EXTENSIONS_UPDATE_UNSET;
  wm->extensions_blocked = 0;

  BLI_assert(wm->runtime == nullptr);
  wm->runtime = MEM_new<blender::bke::WindowManagerRuntime>(__func__);
}

static void window_manager_blend_read_after_liblink(BlendLibReader *reader, ID *id)
{
  wmWindowManager *wm = reinterpret_cast<wmWindowManager *>(id);

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
      BKE_screen_area_blend_read_after_liblink(reader, id, area);
    }
  }
}

IDTypeInfo IDType_ID_WM = {
    /*id_code*/ wmWindowManager::id_type,
    /*id_filter*/ FILTER_ID_WM,
    /*dependencies_id_types*/ FILTER_ID_SCE | FILTER_ID_WS,
    /*main_listbase_index*/ INDEX_ID_WM,
    /*struct_size*/ sizeof(wmWindowManager),
    /*name*/ "WindowManager",
    /*name_plural*/ N_("window_managers"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_WINDOWMANAGER,
    /*flags*/ IDTYPE_FLAGS_NO_COPY | IDTYPE_FLAGS_NO_LIBLINKING | IDTYPE_FLAGS_NO_ANIMDATA |
        IDTYPE_FLAGS_NO_MEMFILE_UNDO | IDTYPE_FLAGS_NEVER_UNUSED,
    /*asset_type_info*/ nullptr,

    /*init_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*free_data*/ window_manager_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ window_manager_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ window_manager_blend_write,
    /*blend_read_data*/ window_manager_blend_read_data,
    /*blend_read_after_liblink*/ window_manager_blend_read_after_liblink,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
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
    op->properties = static_cast<IDProperty *>(op->ptr->data);
    MEM_delete(op->ptr);
  }

  if (op->properties) {
    IDP_FreeProperty(op->properties);
  }

  if (op->reports && (op->reports->flag & RPT_FREE)) {
    BKE_reports_free(op->reports);
    MEM_freeN(op->reports);
  }

  if (op->macro.first) {
    wmOperator *opm, *opmnext;
    for (opm = static_cast<wmOperator *>(op->macro.first); opm; opm = opmnext) {
      opmnext = opm->next;
      WM_operator_free(opm);
    }
  }

  MEM_freeN(op);
}

void WM_operator_free_all_after(wmWindowManager *wm, wmOperator *op)
{
  op = op->next;
  while (op != nullptr) {
    wmOperator *op_next = op->next;
    BLI_remlink(&wm->runtime->operators, op);
    WM_operator_free(op);
    op = op_next;
  }
}

void WM_operator_type_set(wmOperator *op, wmOperatorType *ot)
{
  /* Not supported for Python. */
  BLI_assert(op->py_instance == nullptr);

  op->type = ot;
  op->ptr->type = ot->srna;

  /* Ensure compatible properties. */
  if (op->properties) {
    PointerRNA ptr;
    WM_operator_properties_create_ptr(&ptr, ot);

    WM_operator_properties_default(&ptr, false);

    if (ptr.data) {
      IDP_SyncGroupTypes(op->properties, static_cast<const IDProperty *>(ptr.data), true);
    }

    WM_operator_properties_free(&ptr);
  }
}

static void wm_reports_free(wmWindowManager *wm)
{
  WM_event_timer_remove(wm, nullptr, wm->runtime->reports.reporttimer);
}

void wm_operator_register(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  int tot = 0;

  BLI_addtail(&wm->runtime->operators, op);

  /* Only count registered operators. */
  while (op) {
    wmOperator *op_prev = op->prev;
    if (op->type->flag & OPTYPE_REGISTER) {
      tot += 1;
    }
    if (tot > MAX_OP_REGISTERED) {
      BLI_remlink(&wm->runtime->operators, op);
      WM_operator_free(op);
    }
    op = op_prev;
  }

  /* So the console is redrawn. */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO_REPORT, nullptr);
  WM_event_add_notifier(C, NC_WM | ND_HISTORY, nullptr);
}

void WM_operator_stack_clear(wmWindowManager *wm)
{
  while (wmOperator *op = static_cast<wmOperator *>(BLI_pophead(&wm->runtime->operators))) {
    WM_operator_free(op);
  }

  WM_main_add_notifier(NC_WM | ND_HISTORY, nullptr);
}

void WM_operator_handlers_clear(wmWindowManager *wm, wmOperatorType *ot)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      switch (area->spacetype) {
        case SPACE_FILE: {
          SpaceFile *sfile = static_cast<SpaceFile *>(area->spacedata.first);
          if (sfile->op && sfile->op->type == ot) {
            /* Freed as part of the handler. */
            sfile->op = nullptr;
          }
          break;
        }
      }
    }
  }

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    ListBase *lb[2] = {&win->handlers, &win->modalhandlers};
    for (int i = 0; i < ARRAY_SIZE(lb); i++) {
      LISTBASE_FOREACH (wmEventHandler *, handler_base, lb[i]) {
        if (handler_base->type == WM_HANDLER_TYPE_OP) {
          wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
          if (handler->op && handler->op->type == ot) {
            /* Don't run op->cancel because it needs the context,
             * assume whoever unregisters the operator will cleanup. */
            handler->head.flag |= WM_HANDLER_DO_FREE;
            WM_operator_free(handler->op);
            handler->op = nullptr;
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
    const char *imports[] = {"bpy", nullptr};
    BPY_run_string_eval(C, imports, "bpy.utils.keyconfig_init()");
#endif
  }
}

void WM_keyconfig_init(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* Create standard key configuration. */
  if (wm->runtime->defaultconf == nullptr) {
    /* Keep lowercase to match the preset filename. */
    wm->runtime->defaultconf = WM_keyconfig_new(wm, WM_KEYCONFIG_STR_DEFAULT, false);
  }
  if (wm->runtime->addonconf == nullptr) {
    wm->runtime->addonconf = WM_keyconfig_new(wm, WM_KEYCONFIG_STR_DEFAULT " addon", false);
  }
  if (wm->runtime->userconf == nullptr) {
    wm->runtime->userconf = WM_keyconfig_new(wm, WM_KEYCONFIG_STR_DEFAULT " user", false);
  }

  /* Initialize only after python init is done, for keymaps that use python operators. */
  if (CTX_py_init_get(C) && (wm->init_flag & WM_INIT_FLAG_KEYCONFIG) == 0) {
    /* Create default key config, only initialize once,
     * it's persistent across sessions. */
    if (!(wm->runtime->defaultconf->flag & KEYCONF_INIT_DEFAULT)) {
      wm_window_keymap(wm->runtime->defaultconf);
      ED_spacetypes_keymap(wm->runtime->defaultconf);

      WM_keyconfig_reload(C);

      wm->runtime->defaultconf->flag |= KEYCONF_INIT_DEFAULT;
    }

    /* Harmless, but no need to update in background mode. */
    if (!G.background) {
      WM_keyconfig_update_tag(nullptr, nullptr);
    }
    /* Don't call #WM_keyconfig_update here because add-ons have not yet been registered yet. */

    wm->init_flag |= WM_INIT_FLAG_KEYCONFIG;
  }
}

void WM_check(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);

  /* WM context. */
  if (wm == nullptr) {
    wm = static_cast<wmWindowManager *>(bmain->wm.first);
    CTX_wm_manager_set(C, wm);
  }

  if (wm == nullptr || BLI_listbase_is_empty(&wm->windows)) {
    return;
  }

  /* Run before loading the keyconfig. */
  if (wm->runtime->message_bus == nullptr) {
    wm->runtime->message_bus = WM_msgbus_create();
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
    ED_screens_init(C, bmain, wm);
    wm->init_flag |= WM_INIT_FLAG_WINDOW;
  }
}

void wm_clear_default_size(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* WM context. */
  if (wm == nullptr) {
    wm = static_cast<wmWindowManager *>(CTX_data_main(C)->wm.first);
    CTX_wm_manager_set(C, wm);
  }

  if (wm == nullptr || BLI_listbase_is_empty(&wm->windows)) {
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
  wmWindowManager *wm = static_cast<wmWindowManager *>(
      BKE_libblock_alloc(bmain, ID_WM, "WinMan", 0));
  wmWindow *win;
  bScreen *screen = CTX_wm_screen(C); /* XXX: from file read hrmf. */
  WorkSpace *workspace;
  WorkSpaceLayout *layout = BKE_workspace_layout_find_global(bmain, screen, &workspace);

  CTX_wm_manager_set(C, wm);
  win = wm_window_new(bmain, wm, nullptr, false);
  win->scene = CTX_data_scene(C);
  STRNCPY_UTF8(win->view_layer_name, CTX_data_view_layer(C)->name);
  BKE_workspace_active_set(win->workspace_hook, workspace);
  BKE_workspace_active_layout_set(win->workspace_hook, win->winid, workspace, layout);
  screen->winid = win->winid;

  wm->runtime = MEM_new<blender::bke::WindowManagerRuntime>(__func__);
  wm->runtime->winactive = win;
  wm->file_saved = 1;
  wm_window_make_drawable(wm, win);
}

static void wm_xr_data_free(wmWindowManager *wm)
{
  /* NOTE: this also runs when built without `WITH_XR_OPENXR`.
   * It's necessary to prevent leaks when XR data is created or loaded into non XR builds.
   * This can occur when Python reads all properties (see the `bl_rna_paths` test). */

  /* Note that non-runtime data in `wm->xr` is freed as part of freeing the window manager. */
  if (wm->xr.session_settings.shading.prop) {
    IDP_FreeProperty(wm->xr.session_settings.shading.prop);
    wm->xr.session_settings.shading.prop = nullptr;
  }
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
  wm_xr_data_free(wm);

  while (wmWindow *win = static_cast<wmWindow *>(BLI_pophead(&wm->windows))) {
    /* Prevent draw clear to use screen. */
    BKE_workspace_active_set(win->workspace_hook, nullptr);
    wm_window_free(C, wm, win);
  }

#ifdef WITH_PYTHON
  BPY_callback_wm_free(wm);
#endif

  wm_reports_free(wm);

  if (C && CTX_wm_manager(C) == wm) {
    CTX_wm_manager_set(C, nullptr);
  }

  MEM_delete(wm->runtime);
}

void WM_main(bContext *C)
{
  /* Single refresh before handling events.
   * This ensures we don't run operators before the depsgraph has been evaluated. */
  wm_event_do_refresh_wm_and_depsgraph(C);

  while (true) {

    /* Get events from ghost, handle window events, add to window queues. */
    wm_window_events_process(C);

    /* Per window, all events to the window, screen, area and region handlers. */
    wm_event_do_handlers(C);

    /* Events have left notes about changes, we handle and cache it. */
    wm_event_do_notifiers(C);

    /* Execute cached changes draw. */
    wm_draw_update(C);
  }
}
