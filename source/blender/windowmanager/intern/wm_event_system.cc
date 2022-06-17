/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup wm
 *
 * Handle events and notifiers from GHOST input (mouse, keyboard, tablet, NDOF).
 *
 * Also some operator reports utility functions.
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "GHOST_C-api.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_math.h"
#include "BLI_timer.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "BKE_sound.h"

#include "BLT_translation.h"

#include "ED_asset.h"
#include "ED_fileselect.h"
#include "ED_info.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_undo.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "PIL_time.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_event_system.h"
#include "wm_event_types.h"
#include "wm_surface.h"
#include "wm_window.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

/**
 * When a gizmo is highlighted and uses click/drag events,
 * this prevents mouse button press events from being passed through to other key-maps
 * which would obscure those events.
 *
 * This allows gizmos that only use drag to co-exist with tools that use click.
 *
 * Without tools using press events which would prevent click/drag events getting to the gizmos.
 *
 * This is not a fool proof solution since it's possible the gizmo operators would pass
 * through these events when called, see: T65479.
 */
#define USE_GIZMO_MOUSE_PRIORITY_HACK

static void wm_notifier_clear(wmNotifier *note);

static int wm_operator_call_internal(bContext *C,
                                     wmOperatorType *ot,
                                     PointerRNA *properties,
                                     ReportList *reports,
                                     const wmOperatorCallContext context,
                                     const bool poll_only,
                                     const wmEvent *event);

static bool wm_operator_check_locked_interface(bContext *C, wmOperatorType *ot);
static wmEvent *wm_event_add_mousemove_to_head(wmWindow *win);
static void wm_operator_free_for_fileselect(wmOperator *file_operator);

static void wm_event_state_update_and_click_set_ex(wmEvent *event,
                                                   wmEvent *event_state,
                                                   const bool is_keyboard,
                                                   const bool check_double_click);

/* -------------------------------------------------------------------- */
/** \name Event Management
 * \{ */

wmEvent *wm_event_add_ex(wmWindow *win,
                         const wmEvent *event_to_add,
                         const wmEvent *event_to_add_after)
{
  wmEvent *event = MEM_new<wmEvent>(__func__);

  *event = *event_to_add;

  if (event_to_add_after == nullptr) {
    BLI_addtail(&win->event_queue, event);
  }
  else {
    /* NOTE: strictly speaking this breaks const-correctness,
     * however we're only changing 'next' member. */
    BLI_insertlinkafter(&win->event_queue, (void *)event_to_add_after, event);
  }
  return event;
}

wmEvent *wm_event_add(wmWindow *win, const wmEvent *event_to_add)
{
  return wm_event_add_ex(win, event_to_add, nullptr);
}

wmEvent *WM_event_add_simulate(wmWindow *win, const wmEvent *event_to_add)
{
  if ((G.f & G_FLAG_EVENT_SIMULATE) == 0) {
    BLI_assert_unreachable();
    return nullptr;
  }
  wmEvent *event = wm_event_add(win, event_to_add);

  /* Logic for setting previous value is documented on the #wmEvent struct,
   * see #wm_event_add_ghostevent for the implementation of logic this follows. */
  copy_v2_v2_int(win->eventstate->xy, event->xy);

  if (event->type == MOUSEMOVE) {
    copy_v2_v2_int(win->eventstate->prev_xy, win->eventstate->xy);
    copy_v2_v2_int(event->prev_xy, win->eventstate->xy);
  }
  else if (ISKEYBOARD_OR_BUTTON(event->type)) {
    wm_event_state_update_and_click_set_ex(event, win->eventstate, ISKEYBOARD(event->type), false);
  }
  return event;
}

static void wm_event_custom_free(wmEvent *event)
{
  if ((event->customdata && event->customdata_free) == 0) {
    return;
  }

  /* NOTE: pointer to #ListBase struct elsewhere. */
  if (event->custom == EVT_DATA_DRAGDROP) {
    ListBase *lb = static_cast<ListBase *>(event->customdata);
    WM_drag_free_list(lb);
  }
  else {
    MEM_freeN(event->customdata);
  }
}

static void wm_event_custom_clear(wmEvent *event)
{
  event->custom = 0;
  event->customdata = nullptr;
  event->customdata_free = false;
}

void wm_event_free(wmEvent *event)
{
#ifndef NDEBUG
  /* Don't use assert here because it's fairly harmless in most cases,
   * more an issue of correctness, something we should avoid in general. */
  if ((event->flag & WM_EVENT_IS_REPEAT) && !ISKEYBOARD(event->type)) {
    printf("%s: 'is_repeat=true' for non-keyboard event, this should not happen.\n", __func__);
    WM_event_print(event);
  }
  if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE) && (event->val != KM_NOTHING)) {
    printf("%s: 'val != NOTHING' for a cursor motion event, this should not happen.\n", __func__);
    WM_event_print(event);
  }
#endif

  wm_event_custom_free(event);

  MEM_freeN(event);
}

/** A version of #wm_event_free that holds the last handled event. */
static void wm_event_free_last_handled(wmWindow *win, wmEvent *event)
{
  /* Don't rely on this pointer being valid,
   * callers should behave as if the memory has been freed.
   * As this function should be interchangeable with #wm_event_free. */
#ifndef NDEBUG
  {
    wmEvent *event_copy = static_cast<wmEvent *>(MEM_dupallocN(event));
    MEM_freeN(event);
    event = event_copy;
  }
#endif

  if (win->event_last_handled) {
    wm_event_free(win->event_last_handled);
  }
  /* Don't store custom data in the last handled event as we don't have control how long this event
   * will be stored and the referenced data may become invalid (also it's not needed currently). */
  wm_event_custom_free(event);
  wm_event_custom_clear(event);
  win->event_last_handled = event;
}

static void wm_event_free_last(wmWindow *win)
{
  wmEvent *event = static_cast<wmEvent *>(BLI_poptail(&win->event_queue));
  if (event != nullptr) {
    wm_event_free(event);
  }
}

void wm_event_free_all(wmWindow *win)
{
  wmEvent *event;
  while ((event = static_cast<wmEvent *>(BLI_pophead(&win->event_queue)))) {
    wm_event_free(event);
  }
}

void wm_event_init_from_window(wmWindow *win, wmEvent *event)
{
  *event = *(win->eventstate);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Notifiers & Listeners
 * \{ */

static bool wm_test_duplicate_notifier(const wmWindowManager *wm, uint type, void *reference)
{
  LISTBASE_FOREACH (wmNotifier *, note, &wm->notifier_queue) {
    if ((note->category | note->data | note->subtype | note->action) == type &&
        note->reference == reference) {
      return true;
    }
  }

  return false;
}

void WM_event_add_notifier_ex(wmWindowManager *wm, const wmWindow *win, uint type, void *reference)
{
  if (wm_test_duplicate_notifier(wm, type, reference)) {
    return;
  }

  wmNotifier *note = MEM_cnew<wmNotifier>(__func__);

  BLI_addtail(&wm->notifier_queue, note);

  note->window = win;

  note->category = type & NOTE_CATEGORY;
  note->data = type & NOTE_DATA;
  note->subtype = type & NOTE_SUBTYPE;
  note->action = type & NOTE_ACTION;

  note->reference = reference;
}

/* XXX: in future, which notifiers to send to other windows? */
void WM_event_add_notifier(const bContext *C, uint type, void *reference)
{
  WM_event_add_notifier_ex(CTX_wm_manager(C), CTX_wm_window(C), type, reference);
}

void WM_main_add_notifier(unsigned int type, void *reference)
{
  Main *bmain = G_MAIN;
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);

  if (!wm || wm_test_duplicate_notifier(wm, type, reference)) {
    return;
  }

  wmNotifier *note = MEM_cnew<wmNotifier>(__func__);

  BLI_addtail(&wm->notifier_queue, note);

  note->category = type & NOTE_CATEGORY;
  note->data = type & NOTE_DATA;
  note->subtype = type & NOTE_SUBTYPE;
  note->action = type & NOTE_ACTION;

  note->reference = reference;
}

void WM_main_remove_notifier_reference(const void *reference)
{
  Main *bmain = G_MAIN;
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);

  if (wm) {
    LISTBASE_FOREACH_MUTABLE (wmNotifier *, note, &wm->notifier_queue) {
      if (note->reference == reference) {
        /* Don't remove because this causes problems for #wm_event_do_notifiers
         * which may be looping on the data (deleting screens). */
        wm_notifier_clear(note);
      }
    }

    /* Remap instead. */
#if 0
    if (wm->message_bus) {
      WM_msg_id_remove(wm->message_bus, reference);
    }
#endif
  }
}

static void wm_main_remap_assetlist(ID *old_id, ID *new_id, void *UNUSED(user_data))
{
  ED_assetlist_storage_id_remap(old_id, new_id);
}

static void wm_main_remap_msgbus_notify(ID *old_id, ID *new_id, void *user_data)
{
  wmMsgBus *mbus = static_cast<wmMsgBus *>(user_data);
  if (new_id != nullptr) {
    WM_msg_id_update(mbus, old_id, new_id);
  }
  else {
    WM_msg_id_remove(mbus, old_id);
  }
}

void WM_main_remap_editor_id_reference(const IDRemapper *mappings)
{
  Main *bmain = G_MAIN;

  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        ED_spacedata_id_remap(area, sl, mappings);
      }
    }
  }

  BKE_id_remapper_iter(mappings, wm_main_remap_assetlist, nullptr);

  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  if (wm && wm->message_bus) {
    BKE_id_remapper_iter(mappings, wm_main_remap_msgbus_notify, wm->message_bus);
  }
}

static void wm_notifier_clear(wmNotifier *note)
{
  /* nullptr the entire notifier, only leaving (`next`, `prev`) members intact. */
  memset(((char *)note) + sizeof(Link), 0, sizeof(*note) - sizeof(Link));
}

void wm_event_do_depsgraph(bContext *C, bool is_after_open_file)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  /* The whole idea of locked interface is to prevent viewport and whatever thread from
   * modifying the same data. Because of this, we can not perform dependency graph update. */
  if (wm->is_interface_locked) {
    return;
  }
  /* Combine data-masks so one window doesn't disable UV's in another T26448. */
  CustomData_MeshMasks win_combine_v3d_datamask = {0};
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    const Scene *scene = WM_window_get_active_scene(win);
    const bScreen *screen = WM_window_get_active_screen(win);

    ED_view3d_screen_datamask(C, scene, screen, &win_combine_v3d_datamask);
  }
  /* Update all the dependency graphs of visible view layers. */
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    Scene *scene = WM_window_get_active_scene(win);
    ViewLayer *view_layer = WM_window_get_active_view_layer(win);
    Main *bmain = CTX_data_main(C);
    /* Copied to set's in scene_update_tagged_recursive() */
    scene->customdata_mask = win_combine_v3d_datamask;
    /* XXX, hack so operators can enforce data-masks T26482, GPU render. */
    CustomData_MeshMasks_update(&scene->customdata_mask, &scene->customdata_mask_modal);
    /* TODO(sergey): For now all dependency graphs which are evaluated from
     * workspace are considered active. This will work all fine with "locked"
     * view layer and time across windows. This is to be granted separately,
     * and for until then we have to accept ambiguities when object is shared
     * across visible view layers and has overrides on it. */
    Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);
    if (is_after_open_file) {
      DEG_graph_tag_on_visible_update(depsgraph, true);
    }
    DEG_make_active(depsgraph);
    BKE_scene_graph_update_tagged(depsgraph, bmain);
  }

  wm_surfaces_do_depsgraph(C);
}

void wm_event_do_refresh_wm_and_depsgraph(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  /* Cached: editor refresh callbacks now, they get context. */
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(win);

    CTX_wm_window_set(C, win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->do_refresh) {
        CTX_wm_area_set(C, area);
        ED_area_do_refresh(C, area);
      }
    }
  }

  wm_event_do_depsgraph(C, false);

  CTX_wm_window_set(C, nullptr);
}

static void wm_event_execute_timers(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (UNLIKELY(wm == nullptr)) {
    return;
  }

  /* Set the first window as context, so that there is some minimal context. This avoids crashes
   * when calling code that assumes that there is always a window in the context (which many
   * operators do). */
  CTX_wm_window_set(C, static_cast<wmWindow *>(wm->windows.first));
  BLI_timer_execute();
  CTX_wm_window_set(C, nullptr);
}

void wm_event_do_notifiers(bContext *C)
{
  /* Run the timer before assigning `wm` in the unlikely case a timer loads a file, see T80028. */
  wm_event_execute_timers(C);

  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == nullptr) {
    return;
  }

  /* Disable? - Keep for now since its used for window level notifiers. */
#if 1
  /* Cache & catch WM level notifiers, such as frame change, scene/screen set. */
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    Scene *scene = WM_window_get_active_scene(win);
    bool do_anim = false;
    bool clear_info_stats = false;

    CTX_wm_window_set(C, win);

    LISTBASE_FOREACH_MUTABLE (wmNotifier *, note, &wm->notifier_queue) {
      if (note->category == NC_WM) {
        if (ELEM(note->data, ND_FILEREAD, ND_FILESAVE)) {
          wm->file_saved = 1;
          wm_window_title(wm, win);
        }
        else if (note->data == ND_DATACHANGED) {
          wm_window_title(wm, win);
        }
        else if (note->data == ND_UNDO) {
          ED_preview_restart_queue_work(C);
        }
      }
      if (note->window == win) {
        if (note->category == NC_SCREEN) {
          if (note->data == ND_WORKSPACE_SET) {
            WorkSpace *ref_ws = static_cast<WorkSpace *>(note->reference);

            UI_popup_handlers_remove_all(C, &win->modalhandlers);

            WM_window_set_active_workspace(C, win, ref_ws);
            if (G.debug & G_DEBUG_EVENTS) {
              printf("%s: Workspace set %p\n", __func__, note->reference);
            }
          }
          else if (note->data == ND_WORKSPACE_DELETE) {
            WorkSpace *workspace = static_cast<WorkSpace *>(note->reference);

            ED_workspace_delete(
                workspace, CTX_data_main(C), C, wm); /* XXX: hum, think this over! */
            if (G.debug & G_DEBUG_EVENTS) {
              printf("%s: Workspace delete %p\n", __func__, workspace);
            }
          }
          else if (note->data == ND_LAYOUTBROWSE) {
            bScreen *ref_screen = BKE_workspace_layout_screen_get(
                static_cast<WorkSpaceLayout *>(note->reference));

            /* Free popup handlers only T35434. */
            UI_popup_handlers_remove_all(C, &win->modalhandlers);

            ED_screen_change(C, ref_screen); /* XXX: hum, think this over! */
            if (G.debug & G_DEBUG_EVENTS) {
              printf("%s: screen set %p\n", __func__, note->reference);
            }
          }
          else if (note->data == ND_LAYOUTDELETE) {
            WorkSpace *workspace = WM_window_get_active_workspace(win);
            WorkSpaceLayout *layout = static_cast<WorkSpaceLayout *>(note->reference);

            ED_workspace_layout_delete(workspace, layout, C); /* XXX: hum, think this over! */
            if (G.debug & G_DEBUG_EVENTS) {
              printf("%s: screen delete %p\n", __func__, note->reference);
            }
          }
        }
      }

      if (note->window == win ||
          (note->window == nullptr && (ELEM(note->reference, nullptr, scene)))) {
        if (note->category == NC_SCENE) {
          if (note->data == ND_FRAME) {
            do_anim = true;
          }
        }
      }
      if (ELEM(note->category, NC_SCENE, NC_OBJECT, NC_GEOM, NC_WM)) {
        clear_info_stats = true;
      }
    }

    if (clear_info_stats) {
      /* Only do once since adding notifiers is slow when there are many. */
      ViewLayer *view_layer = CTX_data_view_layer(C);
      ED_info_stats_clear(wm, view_layer);
      WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO, nullptr);
    }

    if (do_anim) {

      /* XXX: quick frame changes can cause a crash if frame-change and rendering
       * collide (happens on slow scenes), BKE_scene_graph_update_for_newframe can be called
       * twice which can depsgraph update the same object at once. */
      if (G.is_rendering == false) {
        /* Depsgraph gets called, might send more notifiers. */
        Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
        ED_update_for_newframe(CTX_data_main(C), depsgraph);
      }
    }
  }

  /* The notifiers are sent without context, to keep it clean. */
  wmNotifier *note;
  while ((note = static_cast<wmNotifier *>(BLI_pophead(&wm->notifier_queue)))) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      Scene *scene = WM_window_get_active_scene(win);
      bScreen *screen = WM_window_get_active_screen(win);
      WorkSpace *workspace = WM_window_get_active_workspace(win);

      /* Filter out notifiers. */
      if (note->category == NC_SCREEN && note->reference && note->reference != screen &&
          note->reference != workspace && note->reference != WM_window_get_active_layout(win)) {
        /* Pass. */
      }
      else if (note->category == NC_SCENE && note->reference && note->reference != scene) {
        /* Pass. */
      }
      else {
        /* XXX context in notifiers? */
        CTX_wm_window_set(C, win);

#  if 0
        printf("notifier win %d screen %s cat %x\n",
               win->winid,
               win->screen->id.name + 2,
               note->category);
#  endif
        ED_screen_do_listen(C, note);

        LISTBASE_FOREACH (ARegion *, region, &screen->regionbase) {
          wmRegionListenerParams region_params{};
          region_params.window = win;
          region_params.area = nullptr;
          region_params.region = region;
          region_params.scene = scene;
          region_params.notifier = note;

          ED_region_do_listen(&region_params);
        }

        ED_screen_areas_iter (win, screen, area) {
          if ((note->category == NC_SPACE) && note->reference) {
            /* Filter out notifiers sent to other spaces. RNA sets the reference to the owning ID
             * though, the screen, so let notifiers through that reference the entire screen. */
            if (!ELEM(note->reference, area->spacedata.first, screen, scene)) {
              continue;
            }
          }
          wmSpaceTypeListenerParams area_params{};
          area_params.window = win;
          area_params.area = area;
          area_params.notifier = note;
          area_params.scene = scene;
          ED_area_do_listen(&area_params);
          LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
            wmRegionListenerParams region_params{};
            region_params.window = win;
            region_params.area = area;
            region_params.region = region;
            region_params.scene = scene;
            region_params.notifier = note;
            ED_region_do_listen(&region_params);
          }
        }
      }
    }

    MEM_freeN(note);
  }
#endif /* If 1 (postpone disabling for in favor of message-bus), eventually. */

  /* Handle message bus. */
  {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      CTX_wm_window_set(C, win);
      WM_msgbus_handle(wm->message_bus, C);
    }
    CTX_wm_window_set(C, nullptr);
  }

  wm_event_do_refresh_wm_and_depsgraph(C);

  /* Status bar. */
  if (wm->winactive) {
    wmWindow *win = wm->winactive;
    CTX_wm_window_set(C, win);
    WM_window_cursor_keymap_status_refresh(C, win);
    CTX_wm_window_set(C, nullptr);
  }

  /* Auto-run warning. */
  wm_test_autorun_warning(C);
}

static int wm_event_always_pass(const wmEvent *event)
{
  /* Some events we always pass on, to ensure proper communication. */
  return ISTIMER(event->type) || (event->type == WINDEACTIVATE);
}

/**
 * Debug only sanity check for the return value of event handlers. Checks that "always pass" events
 * don't cause non-passing handler return values, and thus actually pass.
 *
 * Can't be executed if the handler just loaded a file (typically identified by `CTX_wm_window(C)`
 * returning `nullptr`), because the event will have been freed then.
 */
BLI_INLINE void wm_event_handler_return_value_check(const wmEvent *event, const int action)
{
  BLI_assert_msg(!wm_event_always_pass(event) || (action != WM_HANDLER_BREAK),
                 "Return value for events that should always pass should never be BREAK.");
  UNUSED_VARS_NDEBUG(event, action);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Handling
 * \{ */

static int wm_handler_ui_call(bContext *C,
                              wmEventHandler_UI *handler,
                              const wmEvent *event,
                              int always_pass)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  ARegion *menu = CTX_wm_menu(C);
  static bool do_wheel_ui = true;
  const bool is_wheel = ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE, MOUSEPAN);

  /* UI code doesn't handle return values - it just always returns break.
   * to make the #DBL_CLICK conversion work, we just don't send this to UI, except mouse clicks. */
  if (((handler->head.flag & WM_HANDLER_ACCEPT_DBL_CLICK) == 0) && !ISMOUSE_BUTTON(event->type) &&
      (event->val == KM_DBL_CLICK)) {
    return WM_HANDLER_CONTINUE;
  }

  /* UI is quite aggressive with swallowing events, like scroll-wheel. */
  /* I realize this is not extremely nice code... when UI gets key-maps it can be maybe smarter. */
  if (do_wheel_ui == false) {
    if (is_wheel) {
      return WM_HANDLER_CONTINUE;
    }
    if (wm_event_always_pass(event) == 0) {
      do_wheel_ui = true;
    }
  }

  /* Don't block file-select events. Those are triggered by a separate file browser window.
   * See T75292. */
  if (event->type == EVT_FILESELECT) {
    return WM_UI_HANDLER_CONTINUE;
  }

  /* We set context to where UI handler came from. */
  if (handler->context.area) {
    CTX_wm_area_set(C, handler->context.area);
  }
  if (handler->context.region) {
    CTX_wm_region_set(C, handler->context.region);
  }
  if (handler->context.menu) {
    CTX_wm_menu_set(C, handler->context.menu);
  }

  int retval = handler->handle_fn(C, event, handler->user_data);

  /* Putting back screen context. */
  if ((retval != WM_UI_HANDLER_BREAK) || always_pass) {
    CTX_wm_area_set(C, area);
    CTX_wm_region_set(C, region);
    CTX_wm_menu_set(C, menu);
  }
  else {
    /* This special cases is for areas and regions that get removed. */
    CTX_wm_area_set(C, nullptr);
    CTX_wm_region_set(C, nullptr);
    CTX_wm_menu_set(C, nullptr);
  }

  if (retval == WM_UI_HANDLER_BREAK) {
    return WM_HANDLER_BREAK;
  }

  /* Event not handled in UI, if wheel then we temporarily disable it. */
  if (is_wheel) {
    do_wheel_ui = false;
  }

  return WM_HANDLER_CONTINUE;
}

void wm_event_handler_ui_cancel_ex(bContext *C,
                                   wmWindow *win,
                                   ARegion *region,
                                   bool reactivate_button)
{
  if (!region) {
    return;
  }

  LISTBASE_FOREACH_MUTABLE (wmEventHandler *, handler_base, &region->handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_UI) {
      wmEventHandler_UI *handler = (wmEventHandler_UI *)handler_base;
      BLI_assert(handler->handle_fn != nullptr);
      wmEvent event;
      wm_event_init_from_window(win, &event);
      event.type = EVT_BUT_CANCEL;
      event.val = reactivate_button ? 0 : 1;
      event.flag = (eWM_EventFlag)0;
      handler->handle_fn(C, &event, handler->user_data);
    }
  }
}

static void wm_event_handler_ui_cancel(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  ARegion *region = CTX_wm_region(C);
  wm_event_handler_ui_cancel_ex(C, win, region, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name WM Reports
 *
 * Access to #wmWindowManager.reports
 * \{ */

void WM_report_banner_show(void)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
  ReportList *wm_reports = &wm->reports;

  /* After adding reports to the global list, reset the report timer. */
  WM_event_remove_timer(wm, nullptr, wm_reports->reporttimer);

  /* Records time since last report was added. */
  wm_reports->reporttimer = WM_event_add_timer(wm, wm->winactive, TIMERREPORT, 0.05);

  ReportTimerInfo *rti = MEM_cnew<ReportTimerInfo>(__func__);
  wm_reports->reporttimer->customdata = rti;
}

void WM_report_banners_cancel(Main *bmain)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  BKE_reports_clear(&wm->reports);
  WM_event_remove_timer(wm, nullptr, wm->reports.reporttimer);
}

#ifdef WITH_INPUT_NDOF
void WM_ndof_deadzone_set(float deadzone)
{
  GHOST_setNDOFDeadZone(deadzone);
}
#endif

static void wm_add_reports(ReportList *reports)
{
  /* If the caller owns them, handle this. */
  if (reports->list.first && (reports->flag & RPT_OP_HOLD) == 0) {
    wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);

    /* Add reports to the global list, otherwise they are not seen. */
    BLI_movelisttolist(&wm->reports.list, &reports->list);

    WM_report_banner_show();
  }
}

void WM_report(eReportType type, const char *message)
{
  ReportList reports;
  BKE_reports_init(&reports, RPT_STORE);
  BKE_report(&reports, type, message);

  wm_add_reports(&reports);

  BKE_reports_clear(&reports);
}

void WM_reportf(eReportType type, const char *format, ...)
{
  va_list args;

  DynStr *ds = BLI_dynstr_new();
  va_start(args, format);
  BLI_dynstr_vappendf(ds, format, args);
  va_end(args);

  char *str = BLI_dynstr_get_cstring(ds);
  WM_report(type, str);
  MEM_freeN(str);

  BLI_dynstr_free(ds);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Logic
 * \{ */

bool WM_operator_poll(bContext *C, wmOperatorType *ot)
{

  LISTBASE_FOREACH (wmOperatorTypeMacro *, macro, &ot->macro) {
    wmOperatorType *ot_macro = WM_operatortype_find(macro->idname, false);

    if (!WM_operator_poll(C, ot_macro)) {
      return false;
    }
  }

  /* Python needs operator type, so we added exception for it. */
  if (ot->pyop_poll) {
    return ot->pyop_poll(C, ot);
  }
  if (ot->poll) {
    return ot->poll(C);
  }

  return true;
}

bool WM_operator_poll_context(bContext *C, wmOperatorType *ot, short context)
{
  /* Sets up the new context and calls #wm_operator_invoke() with poll_only. */
  return wm_operator_call_internal(
      C, ot, nullptr, nullptr, static_cast<wmOperatorCallContext>(context), true, nullptr);
}

bool WM_operator_check_ui_empty(wmOperatorType *ot)
{
  if (ot->macro.first != nullptr) {
    /* For macros, check all have exec() we can call. */
    LISTBASE_FOREACH (wmOperatorTypeMacro *, macro, &ot->macro) {
      wmOperatorType *otm = WM_operatortype_find(macro->idname, false);
      if (otm && !WM_operator_check_ui_empty(otm)) {
        return false;
      }
    }
    return true;
  }

  /* Assume a UI callback will draw something. */
  if (ot->ui) {
    return false;
  }

  PointerRNA ptr;
  WM_operator_properties_create_ptr(&ptr, ot);
  RNA_STRUCT_BEGIN (&ptr, prop) {
    int flag = RNA_property_flag(prop);
    if (flag & PROP_HIDDEN) {
      continue;
    }
    return false;
  }
  RNA_STRUCT_END;
  return true;
}

void WM_operator_region_active_win_set(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area) {
    ARegion *region = CTX_wm_region(C);
    if (region && region->regiontype == RGN_TYPE_WINDOW) {
      area->region_active_win = BLI_findindex(&area->regionbase, region);
    }
  }
}

/**
 * \param caller_owns_reports: True when called from Python.
 */
static void wm_operator_reports(bContext *C, wmOperator *op, int retval, bool caller_owns_reports)
{
  if (G.background == 0 && caller_owns_reports == false) { /* Popup. */
    if (op->reports->list.first) {
      /* FIXME: temp setting window, see other call to #UI_popup_menu_reports for why. */
      wmWindow *win_prev = CTX_wm_window(C);
      ScrArea *area_prev = CTX_wm_area(C);
      ARegion *region_prev = CTX_wm_region(C);

      if (win_prev == nullptr) {
        CTX_wm_window_set(C, static_cast<wmWindow *>(CTX_wm_manager(C)->windows.first));
      }

      UI_popup_menu_reports(C, op->reports);

      CTX_wm_window_set(C, win_prev);
      CTX_wm_area_set(C, area_prev);
      CTX_wm_region_set(C, region_prev);
    }
  }

  if (retval & OPERATOR_FINISHED) {
    CLOG_STR_INFO_N(WM_LOG_OPERATORS, 1, WM_operator_pystring(C, op, false, true));

    if (caller_owns_reports == false) {
      BKE_reports_print(op->reports, RPT_DEBUG); /* Print out reports to console. */
    }

    if (op->type->flag & OPTYPE_REGISTER) {
      if (G.background == 0) { /* Ends up printing these in the terminal, gets annoying. */
        /* Report the python string representation of the operator. */
        char *buf = WM_operator_pystring(C, op, false, true);
        BKE_report(CTX_wm_reports(C), RPT_OPERATOR, buf);
        MEM_freeN(buf);
      }
    }
  }

  /* Refresh Info Editor with reports immediately, even if op returned #OPERATOR_CANCELLED. */
  if ((retval & OPERATOR_CANCELLED) && !BLI_listbase_is_empty(&op->reports->list)) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO_REPORT, nullptr);
  }
  /* If the caller owns them, handle this. */
  wm_add_reports(op->reports);
}

/**
 * This function is mainly to check that the rules for freeing
 * an operator are kept in sync.
 */
static bool wm_operator_register_check(wmWindowManager *wm, wmOperatorType *ot)
{
  /* Check undo flag here since undo operators are also added to the list,
   * to support checking if the same operator is run twice. */
  return wm && (wm->op_undo_depth == 0) && (ot->flag & (OPTYPE_REGISTER | OPTYPE_UNDO));
}

static void wm_operator_finished(bContext *C, wmOperator *op, const bool repeat, const bool store)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  enum {
    NOP,
    SET,
    CLEAR,
  } hud_status = NOP;

  op->customdata = nullptr;

  if (store) {
    WM_operator_last_properties_store(op);
  }

  /* We don't want to do undo pushes for operators that are being
   * called from operators that already do an undo push. Usually
   * this will happen for python operators that call C operators. */
  if (wm->op_undo_depth == 0) {
    if (op->type->flag & OPTYPE_UNDO) {
      ED_undo_push_op(C, op);
      if (repeat == 0) {
        hud_status = CLEAR;
      }
    }
    else if (op->type->flag & OPTYPE_UNDO_GROUPED) {
      ED_undo_grouped_push_op(C, op);
      if (repeat == 0) {
        hud_status = CLEAR;
      }
    }
  }

  if (repeat == 0) {
    if (G.debug & G_DEBUG_WM) {
      char *buf = WM_operator_pystring(C, op, false, true);
      BKE_report(CTX_wm_reports(C), RPT_OPERATOR, buf);
      MEM_freeN(buf);
    }

    if (wm_operator_register_check(wm, op->type)) {
      /* Take ownership of reports (in case python provided own). */
      op->reports->flag |= RPT_FREE;

      wm_operator_register(C, op);
      WM_operator_region_active_win_set(C);

      if (WM_operator_last_redo(C) == op) {
        /* Show the redo panel. */
        hud_status = SET;
      }
    }
    else {
      WM_operator_free(op);
    }
  }

  if (hud_status != NOP) {
    if (hud_status == SET) {
      ScrArea *area = CTX_wm_area(C);
      if (area && ((area->flag & AREA_FLAG_OFFSCREEN) == 0)) {
        ED_area_type_hud_ensure(C, area);
      }
    }
    else if (hud_status == CLEAR) {
      ED_area_type_hud_clear(wm, nullptr);
    }
    else {
      BLI_assert_unreachable();
    }
  }
}

/**
 * \param repeat: When true, it doesn't register again, nor does it free.
 */
static int wm_operator_exec(bContext *C, wmOperator *op, const bool repeat, const bool store)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  int retval = OPERATOR_CANCELLED;

  CTX_wm_operator_poll_msg_clear(C);

  if (op == nullptr || op->type == nullptr) {
    return retval;
  }

  if (0 == WM_operator_poll(C, op->type)) {
    return retval;
  }

  if (op->type->exec) {
    if (op->type->flag & OPTYPE_UNDO) {
      wm->op_undo_depth++;
    }

    retval = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(retval);

    if (op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm) {
      wm->op_undo_depth--;
    }
  }

  /* XXX(@mont29): Disabled the repeat check to address part 2 of T31840.
   * Carefully checked all calls to wm_operator_exec and WM_operator_repeat, don't see any reason
   * why this was needed, but worth to note it in case something turns bad. */
  if (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED) /* && repeat == 0 */) {
    wm_operator_reports(C, op, retval, false);
  }

  if (retval & OPERATOR_FINISHED) {
    wm_operator_finished(C, op, repeat, store && wm->op_undo_depth == 0);
  }
  else if (repeat == 0) {
    /* WARNING: modal from exec is bad practice, but avoid crashing. */
    if (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED)) {
      WM_operator_free(op);
    }
  }

  return retval | OPERATOR_HANDLED;
}

/**
 * Simply calls exec with basic checks.
 */
static int wm_operator_exec_notest(bContext *C, wmOperator *op)
{
  int retval = OPERATOR_CANCELLED;

  if (op == nullptr || op->type == nullptr || op->type->exec == nullptr) {
    return retval;
  }

  retval = op->type->exec(C, op);
  OPERATOR_RETVAL_CHECK(retval);

  return retval;
}

int WM_operator_call_ex(bContext *C, wmOperator *op, const bool store)
{
  return wm_operator_exec(C, op, false, store);
}

int WM_operator_call(bContext *C, wmOperator *op)
{
  return WM_operator_call_ex(C, op, false);
}

int WM_operator_call_notest(bContext *C, wmOperator *op)
{
  return wm_operator_exec_notest(C, op);
}

int WM_operator_repeat(bContext *C, wmOperator *op)
{
  const int op_flag = OP_IS_REPEAT;
  op->flag |= op_flag;
  const int ret = wm_operator_exec(C, op, true, true);
  op->flag &= ~op_flag;
  return ret;
}
int WM_operator_repeat_last(bContext *C, wmOperator *op)
{
  const int op_flag = OP_IS_REPEAT_LAST;
  op->flag |= op_flag;
  const int ret = wm_operator_exec(C, op, true, true);
  op->flag &= ~op_flag;
  return ret;
}
bool WM_operator_repeat_check(const bContext *UNUSED(C), wmOperator *op)
{
  if (op->type->exec != nullptr) {
    return true;
  }
  if (op->opm) {
    /* For macros, check all have exec() we can call. */
    LISTBASE_FOREACH (wmOperatorTypeMacro *, macro, &op->opm->type->macro) {
      wmOperatorType *otm = WM_operatortype_find(macro->idname, false);
      if (otm && otm->exec == nullptr) {
        return false;
      }
    }
    return true;
  }

  return false;
}

bool WM_operator_is_repeat(const bContext *C, const wmOperator *op)
{
  /* May be in the operators list or not. */
  wmOperator *op_prev;
  if (op->prev == nullptr && op->next == nullptr) {
    wmWindowManager *wm = CTX_wm_manager(C);
    op_prev = static_cast<wmOperator *>(wm->operators.last);
  }
  else {
    op_prev = op->prev;
  }
  return (op_prev && (op->type == op_prev->type));
}

static wmOperator *wm_operator_create(wmWindowManager *wm,
                                      wmOperatorType *ot,
                                      PointerRNA *properties,
                                      ReportList *reports)
{
  /* Operator-type names are static still. pass to allocation name for debugging. */
  wmOperator *op = MEM_cnew<wmOperator>(ot->idname);

  /* Adding new operator could be function, only happens here now. */
  op->type = ot;
  BLI_strncpy(op->idname, ot->idname, OP_MAX_TYPENAME);

  /* Initialize properties, either copy or create. */
  op->ptr = MEM_cnew<PointerRNA>("wmOperatorPtrRNA");
  if (properties && properties->data) {
    op->properties = IDP_CopyProperty(static_cast<const IDProperty *>(properties->data));
  }
  else {
    IDPropertyTemplate val = {0};
    op->properties = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
  }
  RNA_pointer_create(&wm->id, ot->srna, op->properties, op->ptr);

  /* Initialize error reports. */
  if (reports) {
    op->reports = reports; /* Must be initialized already. */
  }
  else {
    op->reports = MEM_cnew<ReportList>("wmOperatorReportList");
    BKE_reports_init(op->reports, RPT_STORE | RPT_FREE);
  }

  /* Recursive filling of operator macro list. */
  if (ot->macro.first) {
    static wmOperator *motherop = nullptr;
    int root = 0;

    /* Ensure all ops are in execution order in 1 list. */
    if (motherop == nullptr) {
      motherop = op;
      root = 1;
    }

    /* If properties exist, it will contain everything needed. */
    if (properties) {
      wmOperatorTypeMacro *otmacro = static_cast<wmOperatorTypeMacro *>(ot->macro.first);

      RNA_STRUCT_BEGIN (properties, prop) {

        if (otmacro == nullptr) {
          break;
        }

        /* Skip invalid properties. */
        if (STREQ(RNA_property_identifier(prop), otmacro->idname)) {
          wmOperatorType *otm = WM_operatortype_find(otmacro->idname, false);
          PointerRNA someptr = RNA_property_pointer_get(properties, prop);
          wmOperator *opm = wm_operator_create(wm, otm, &someptr, nullptr);

          IDP_ReplaceGroupInGroup(opm->properties, otmacro->properties);

          BLI_addtail(&motherop->macro, opm);
          opm->opm = motherop; /* Pointer to mom, for modal(). */

          otmacro = otmacro->next;
        }
      }
      RNA_STRUCT_END;
    }
    else {
      LISTBASE_FOREACH (wmOperatorTypeMacro *, macro, &ot->macro) {
        wmOperatorType *otm = WM_operatortype_find(macro->idname, false);
        wmOperator *opm = wm_operator_create(wm, otm, macro->ptr, nullptr);

        BLI_addtail(&motherop->macro, opm);
        opm->opm = motherop; /* Pointer to mom, for modal(). */
      }
    }

    if (root) {
      motherop = nullptr;
    }
  }

  WM_operator_properties_sanitize(op->ptr, false);

  return op;
}

/**
 * This isn't very nice but needed to redraw gizmos which are hidden while tweaking,
 * See #WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK for details.
 */
static void wm_region_tag_draw_on_gizmo_delay_refresh_for_tweak(wmWindow *win)
{

  bScreen *screen = WM_window_get_active_screen(win);
  /* Unlikely but not impossible as this runs after events have been handled. */
  if (UNLIKELY(screen == nullptr)) {
    return;
  }
  ED_screen_areas_iter (win, screen, area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (region->gizmo_map != nullptr) {
        if (WM_gizmomap_tag_delay_refresh_for_tweak_check(region->gizmo_map)) {
          ED_region_tag_redraw(region);
        }
      }
    }
  }
}

static void wm_region_mouse_co(bContext *C, wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  if (region) {
    /* Compatibility convention. */
    event->mval[0] = event->xy[0] - region->winrct.xmin;
    event->mval[1] = event->xy[1] - region->winrct.ymin;
  }
  else {
    /* These values are invalid (avoid odd behavior by relying on old #wmEvent.mval values). */
    event->mval[0] = -1;
    event->mval[1] = -1;
  }
}

/**
 * Also used for exec when 'event' is nullptr.
 */
static int wm_operator_invoke(bContext *C,
                              wmOperatorType *ot,
                              const wmEvent *event,
                              PointerRNA *properties,
                              ReportList *reports,
                              const bool poll_only,
                              bool use_last_properties)
{
  int retval = OPERATOR_PASS_THROUGH;

  /* This is done because complicated setup is done to call this function
   * that is better not duplicated. */
  if (poll_only) {
    return WM_operator_poll(C, ot);
  }

  if (WM_operator_poll(C, ot)) {
    wmWindowManager *wm = CTX_wm_manager(C);

    /* If `reports == nullptr`, they'll be initialized. */
    wmOperator *op = wm_operator_create(wm, ot, properties, reports);

    const bool is_nested_call = (wm->op_undo_depth != 0);

    if (event != nullptr) {
      op->flag |= OP_IS_INVOKE;
    }

    /* Initialize setting from previous run. */
    if (!is_nested_call && use_last_properties) { /* Not called by a Python script. */
      WM_operator_last_properties_init(op);
    }

    if ((event == nullptr) || (event->type != MOUSEMOVE)) {
      CLOG_INFO(WM_LOG_HANDLERS,
                2,
                "handle evt %d win %p op %s",
                event ? event->type : 0,
                CTX_wm_screen(C)->active_region,
                ot->idname);
    }

    if (op->type->invoke && event) {
      /* Temporarily write into `mval` (not technically `const` correct) but this is restored. */
      const int mval_prev[2] = {UNPACK2(event->mval)};
      wm_region_mouse_co(C, (wmEvent *)event);

      if (op->type->flag & OPTYPE_UNDO) {
        wm->op_undo_depth++;
      }

      retval = op->type->invoke(C, op, event);
      OPERATOR_RETVAL_CHECK(retval);

      if (op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm) {
        wm->op_undo_depth--;
      }

      copy_v2_v2_int(((wmEvent *)event)->mval, mval_prev);
    }
    else if (op->type->exec) {
      if (op->type->flag & OPTYPE_UNDO) {
        wm->op_undo_depth++;
      }

      retval = op->type->exec(C, op);
      OPERATOR_RETVAL_CHECK(retval);

      if (op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm) {
        wm->op_undo_depth--;
      }
    }
    else {
      /* Debug, important to leave a while, should never happen. */
      CLOG_ERROR(WM_LOG_OPERATORS, "invalid operator call '%s'", op->idname);
    }

    /* NOTE: if the report is given as an argument then assume the caller will deal with displaying
     * them currently Python only uses this. */
    if (!(retval & OPERATOR_HANDLED) && (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED))) {
      /* Only show the report if the report list was not given in the function. */
      wm_operator_reports(C, op, retval, (reports != nullptr));
    }

    if (retval & OPERATOR_HANDLED) {
      /* Do nothing, #wm_operator_exec() has been called somewhere. */
    }
    else if (retval & OPERATOR_FINISHED) {
      const bool store = !is_nested_call && use_last_properties;
      wm_operator_finished(C, op, false, store);
    }
    else if (retval & OPERATOR_RUNNING_MODAL) {
      /* Take ownership of reports (in case python provided own). */
      op->reports->flag |= RPT_FREE;

      /* Grab cursor during blocking modal operators (X11)
       * Also check for macro. */
      if (ot->flag & OPTYPE_BLOCKING || (op->opm && op->opm->type->flag & OPTYPE_BLOCKING)) {
        int bounds[4] = {-1, -1, -1, -1};
        int wrap = WM_CURSOR_WRAP_NONE;

        if (event && (U.uiflag & USER_CONTINUOUS_MOUSE)) {
          const wmOperator *op_test = op->opm ? op->opm : op;
          const wmOperatorType *ot_test = op_test->type;
          if ((ot_test->flag & OPTYPE_GRAB_CURSOR_XY) ||
              (op_test->flag & OP_IS_MODAL_GRAB_CURSOR)) {
            wrap = WM_CURSOR_WRAP_XY;
          }
          else if (ot_test->flag & OPTYPE_GRAB_CURSOR_X) {
            wrap = WM_CURSOR_WRAP_X;
          }
          else if (ot_test->flag & OPTYPE_GRAB_CURSOR_Y) {
            wrap = WM_CURSOR_WRAP_Y;
          }
        }

        if (wrap) {
          const rcti *winrect = nullptr;
          ARegion *region = CTX_wm_region(C);
          ScrArea *area = CTX_wm_area(C);

          /* Wrap only in X for header. */
          if (region && RGN_TYPE_IS_HEADER_ANY(region->regiontype)) {
            wrap = WM_CURSOR_WRAP_X;
          }

          if (region && region->regiontype == RGN_TYPE_WINDOW &&
              BLI_rcti_isect_pt_v(&region->winrct, event->xy)) {
            winrect = &region->winrct;
          }
          else if (area && BLI_rcti_isect_pt_v(&area->totrct, event->xy)) {
            winrect = &area->totrct;
          }

          if (winrect) {
            bounds[0] = winrect->xmin;
            bounds[1] = winrect->ymax;
            bounds[2] = winrect->xmax;
            bounds[3] = winrect->ymin;
          }
        }

        WM_cursor_grab_enable(CTX_wm_window(C), wrap, false, bounds);
      }

      /* Cancel UI handlers, typically tool-tips that can hang around
       * while dragging the view or worse, that stay there permanently
       * after the modal operator has swallowed all events and passed
       * none to the UI handler. */
      wm_event_handler_ui_cancel(C);
    }
    else {
      WM_operator_free(op);
    }
  }

  return retval;
}

/**
 * #WM_operator_name_call is the main accessor function
 * This is for python to access since its done the operator lookup
 * invokes operator in context.
 */
static int wm_operator_call_internal(bContext *C,
                                     wmOperatorType *ot,
                                     PointerRNA *properties,
                                     ReportList *reports,
                                     const wmOperatorCallContext context,
                                     const bool poll_only,
                                     const wmEvent *event)
{
  int retval;

  CTX_wm_operator_poll_msg_clear(C);

  /* Dummy test. */
  if (ot) {
    wmWindow *window = CTX_wm_window(C);

    if (event == nullptr) {
      switch (context) {
        case WM_OP_INVOKE_DEFAULT:
        case WM_OP_INVOKE_REGION_WIN:
        case WM_OP_INVOKE_REGION_PREVIEW:
        case WM_OP_INVOKE_REGION_CHANNELS:
        case WM_OP_INVOKE_AREA:
        case WM_OP_INVOKE_SCREEN:
          /* Window is needed for invoke and cancel operators. */
          if (window == nullptr) {
            if (poll_only) {
              CTX_wm_operator_poll_msg_set(C, "Missing 'window' in context");
            }
            return 0;
          }
          else {
            event = window->eventstate;
          }
          break;
        default:
          event = nullptr;
          break;
      }
    }
    else {
      switch (context) {
        case WM_OP_EXEC_DEFAULT:
        case WM_OP_EXEC_REGION_WIN:
        case WM_OP_EXEC_REGION_PREVIEW:
        case WM_OP_EXEC_REGION_CHANNELS:
        case WM_OP_EXEC_AREA:
        case WM_OP_EXEC_SCREEN:
          event = nullptr;
        default:
          break;
      }
    }

    switch (context) {
      case WM_OP_EXEC_REGION_WIN:
      case WM_OP_INVOKE_REGION_WIN:
      case WM_OP_EXEC_REGION_CHANNELS:
      case WM_OP_INVOKE_REGION_CHANNELS:
      case WM_OP_EXEC_REGION_PREVIEW:
      case WM_OP_INVOKE_REGION_PREVIEW: {
        /* Forces operator to go to the region window/channels/preview, for header menus,
         * but we stay in the same region if we are already in one. */
        ARegion *region = CTX_wm_region(C);
        ScrArea *area = CTX_wm_area(C);
        int type = RGN_TYPE_WINDOW;

        switch (context) {
          case WM_OP_EXEC_REGION_CHANNELS:
          case WM_OP_INVOKE_REGION_CHANNELS:
            type = RGN_TYPE_CHANNELS;
            break;

          case WM_OP_EXEC_REGION_PREVIEW:
          case WM_OP_INVOKE_REGION_PREVIEW:
            type = RGN_TYPE_PREVIEW;
            break;

          case WM_OP_EXEC_REGION_WIN:
          case WM_OP_INVOKE_REGION_WIN:
          default:
            type = RGN_TYPE_WINDOW;
            break;
        }

        if (!(region && region->regiontype == type) && area) {
          ARegion *region_other = (type == RGN_TYPE_WINDOW) ?
                                      BKE_area_find_region_active_win(area) :
                                      BKE_area_find_region_type(area, type);
          if (region_other) {
            CTX_wm_region_set(C, region_other);
          }
        }

        retval = wm_operator_invoke(C, ot, event, properties, reports, poll_only, true);

        /* Set region back. */
        CTX_wm_region_set(C, region);

        return retval;
      }
      case WM_OP_EXEC_AREA:
      case WM_OP_INVOKE_AREA: {
        /* Remove region from context. */
        ARegion *region = CTX_wm_region(C);

        CTX_wm_region_set(C, nullptr);
        retval = wm_operator_invoke(C, ot, event, properties, reports, poll_only, true);
        CTX_wm_region_set(C, region);

        return retval;
      }
      case WM_OP_EXEC_SCREEN:
      case WM_OP_INVOKE_SCREEN: {
        /* Remove region + area from context. */
        ARegion *region = CTX_wm_region(C);
        ScrArea *area = CTX_wm_area(C);

        CTX_wm_region_set(C, nullptr);
        CTX_wm_area_set(C, nullptr);
        retval = wm_operator_invoke(C, ot, event, properties, reports, poll_only, true);
        CTX_wm_area_set(C, area);
        CTX_wm_region_set(C, region);

        return retval;
      }
      case WM_OP_EXEC_DEFAULT:
      case WM_OP_INVOKE_DEFAULT:
        return wm_operator_invoke(C, ot, event, properties, reports, poll_only, true);
    }
  }

  return 0;
}

int WM_operator_name_call_ptr(bContext *C,
                              wmOperatorType *ot,
                              wmOperatorCallContext context,
                              PointerRNA *properties,
                              const wmEvent *event)
{
  BLI_assert(ot == WM_operatortype_find(ot->idname, true));
  return wm_operator_call_internal(C, ot, properties, nullptr, context, false, event);
}
int WM_operator_name_call(bContext *C,
                          const char *opstring,
                          wmOperatorCallContext context,
                          PointerRNA *properties,
                          const wmEvent *event)
{
  wmOperatorType *ot = WM_operatortype_find(opstring, false);
  if (ot) {
    return WM_operator_name_call_ptr(C, ot, context, properties, event);
  }

  return 0;
}

bool WM_operator_name_poll(bContext *C, const char *opstring)
{
  wmOperatorType *ot = WM_operatortype_find(opstring, false);
  if (!ot) {
    return false;
  }

  return WM_operator_poll(C, ot);
}

int WM_operator_name_call_with_properties(bContext *C,
                                          const char *opstring,
                                          wmOperatorCallContext context,
                                          IDProperty *properties,
                                          const wmEvent *event)
{
  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find(opstring, false);
  RNA_pointer_create(
      &static_cast<wmWindowManager *>(G_MAIN->wm.first)->id, ot->srna, properties, &props_ptr);
  return WM_operator_name_call_ptr(C, ot, context, &props_ptr, event);
}

void WM_menu_name_call(bContext *C, const char *menu_name, short context)
{
  wmOperatorType *ot = WM_operatortype_find("WM_OT_call_menu", false);
  PointerRNA ptr;
  WM_operator_properties_create_ptr(&ptr, ot);
  RNA_string_set(&ptr, "name", menu_name);
  WM_operator_name_call_ptr(C, ot, static_cast<wmOperatorCallContext>(context), &ptr, nullptr);
  WM_operator_properties_free(&ptr);
}

int WM_operator_call_py(bContext *C,
                        wmOperatorType *ot,
                        wmOperatorCallContext context,
                        PointerRNA *properties,
                        ReportList *reports,
                        const bool is_undo)
{
  int retval = OPERATOR_CANCELLED;
  /* Not especially nice using undo depth here. It's used so Python never
   * triggers undo or stores an operator's last used state. */
  wmWindowManager *wm = CTX_wm_manager(C);
  if (!is_undo && wm) {
    wm->op_undo_depth++;
  }

  retval = wm_operator_call_internal(C, ot, properties, reports, context, false, nullptr);

  if (!is_undo && wm && (wm == CTX_wm_manager(C))) {
    wm->op_undo_depth--;
  }

  return retval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Wait For Input
 *
 * Delay executing operators that depend on cursor location.
 *
 * See: #OPTYPE_DEPENDS_ON_CURSOR doc-string for more information.
 * \{ */

struct uiOperatorWaitForInput {
  ScrArea *area;
  wmOperatorCallParams optype_params;
  bContextStore *context;
};

static void ui_handler_wait_for_input_remove(bContext *C, void *userdata)
{
  uiOperatorWaitForInput *opwait = static_cast<uiOperatorWaitForInput *>(userdata);
  if (opwait->optype_params.opptr) {
    if (opwait->optype_params.opptr->data) {
      IDP_FreeProperty(static_cast<IDProperty *>(opwait->optype_params.opptr->data));
    }
    MEM_freeN(opwait->optype_params.opptr);
  }
  if (opwait->context) {
    CTX_store_free(opwait->context);
  }

  if (opwait->area != nullptr) {
    ED_area_status_text(opwait->area, nullptr);
  }
  else {
    ED_workspace_status_text(C, nullptr);
  }

  MEM_freeN(opwait);
}

static int ui_handler_wait_for_input(bContext *C, const wmEvent *event, void *userdata)
{
  uiOperatorWaitForInput *opwait = static_cast<uiOperatorWaitForInput *>(userdata);
  enum { CONTINUE = 0, EXECUTE, CANCEL } state = CONTINUE;
  state = CONTINUE;

  switch (event->type) {
    case LEFTMOUSE: {
      if (event->val == KM_PRESS) {
        state = EXECUTE;
      }
      break;
    }
      /* Useful if the operator isn't convenient to access while the mouse button is held.
       * If it takes numeric input for example. */
    case EVT_SPACEKEY:
    case EVT_RETKEY: {
      if (event->val == KM_PRESS) {
        state = EXECUTE;
      }
      break;
    }
    case RIGHTMOUSE: {
      if (event->val == KM_PRESS) {
        state = CANCEL;
      }
      break;
    }
    case EVT_ESCKEY: {
      if (event->val == KM_PRESS) {
        state = CANCEL;
      }
      break;
    }
  }

  if (state != CONTINUE) {
    wmWindow *win = CTX_wm_window(C);
    WM_cursor_modal_restore(win);

    if (state == EXECUTE) {
      CTX_store_set(C, opwait->context);
      WM_operator_name_call_ptr(C,
                                opwait->optype_params.optype,
                                opwait->optype_params.opcontext,
                                opwait->optype_params.opptr,
                                event);
      CTX_store_set(C, nullptr);
    }

    WM_event_remove_ui_handler(&win->modalhandlers,
                               ui_handler_wait_for_input,
                               ui_handler_wait_for_input_remove,
                               opwait,
                               false);

    ui_handler_wait_for_input_remove(C, opwait);

    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

void WM_operator_name_call_ptr_with_depends_on_cursor(bContext *C,
                                                      wmOperatorType *ot,
                                                      wmOperatorCallContext opcontext,
                                                      PointerRNA *properties,
                                                      const wmEvent *event,
                                                      const char *drawstr)
{
  int flag = ot->flag;

  LISTBASE_FOREACH (wmOperatorTypeMacro *, macro, &ot->macro) {
    wmOperatorType *otm = WM_operatortype_find(macro->idname, false);
    if (otm != nullptr) {
      flag |= otm->flag;
    }
  }

  if ((flag & OPTYPE_DEPENDS_ON_CURSOR) == 0) {
    WM_operator_name_call_ptr(C, ot, opcontext, properties, event);
    return;
  }

  wmWindow *win = CTX_wm_window(C);
  /* The operator context is applied when the operator is called,
   * the check for the area needs to be explicitly limited here.
   * Useful so it's possible to screen-shot an area without drawing into it's header. */
  ScrArea *area = WM_OP_CONTEXT_HAS_AREA(opcontext) ? CTX_wm_area(C) : nullptr;

  {
    char header_text[UI_MAX_DRAW_STR];
    SNPRINTF(header_text,
             "%s %s",
             IFACE_("Input pending "),
             (drawstr && drawstr[0]) ? drawstr : CTX_IFACE_(ot->translation_context, ot->name));
    if (area != nullptr) {
      ED_area_status_text(area, header_text);
    }
    else {
      ED_workspace_status_text(C, header_text);
    }
  }

  WM_cursor_modal_set(win, ot->cursor_pending);

  uiOperatorWaitForInput *opwait = MEM_cnew<uiOperatorWaitForInput>(__func__);
  opwait->optype_params.optype = ot;
  opwait->optype_params.opcontext = opcontext;
  opwait->optype_params.opptr = properties;

  opwait->area = area;

  if (properties) {
    opwait->optype_params.opptr = MEM_cnew<PointerRNA>(__func__);
    *opwait->optype_params.opptr = *properties;
    if (properties->data != nullptr) {
      opwait->optype_params.opptr->data = IDP_CopyProperty(
          static_cast<IDProperty *>(properties->data));
    }
  }

  bContextStore *store = CTX_store_get(C);
  if (store) {
    opwait->context = CTX_store_copy(store);
  }

  WM_event_add_ui_handler(C,
                          &win->modalhandlers,
                          ui_handler_wait_for_input,
                          ui_handler_wait_for_input_remove,
                          opwait,
                          WM_HANDLER_BLOCKING);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Handler Types
 *
 * General API for different handler types.
 * \{ */

void wm_event_free_handler(wmEventHandler *handler)
{
  /* Future extra custom-data free? */
  MEM_freeN(handler);
}

/**
 * Check if the handler's area and/or region are actually part of the screen, and return them if
 * so.
 */
static void wm_handler_op_context_get_if_valid(bContext *C,
                                               wmEventHandler_Op *handler,
                                               const wmEvent *event,
                                               ScrArea **r_area,
                                               ARegion **r_region)
{
  wmWindow *win = handler->context.win ? handler->context.win : CTX_wm_window(C);
  /* It's probably fine to always use #WM_window_get_active_screen() to get the screen. But this
   * code has been getting it through context since forever, so play safe and stick to that when
   * possible. */
  bScreen *screen = handler->context.win ? WM_window_get_active_screen(win) : CTX_wm_screen(C);

  *r_area = nullptr;
  *r_region = nullptr;

  if (screen == nullptr || handler->op == nullptr) {
    return;
  }

  if (handler->context.area == nullptr) {
    /* Pass */
  }
  else {
    ScrArea *area = nullptr;

    ED_screen_areas_iter (win, screen, area_iter) {
      if (area_iter == handler->context.area) {
        area = area_iter;
        break;
      }
    }

    if (area == nullptr) {
      /* When changing screen layouts with running modal handlers (like render display), this
       * is not an error to print. */
      if (handler->op == nullptr) {
        CLOG_ERROR(WM_LOG_HANDLERS,
                   "internal error: handler (%s) has invalid area",
                   handler->op->type->idname);
      }
    }
    else {
      ARegion *region;
      wmOperator *op = handler->op ? (handler->op->opm ? handler->op->opm : handler->op) : nullptr;
      *r_area = area;

      if (op && (op->flag & OP_IS_MODAL_CURSOR_REGION)) {
        region = BKE_area_find_region_xy(area, handler->context.region_type, event->xy);
        if (region) {
          handler->context.region = region;
        }
      }
      else {
        region = nullptr;
      }

      if ((region == nullptr) && handler->context.region) {
        if (BLI_findindex(&area->regionbase, handler->context.region) != -1) {
          region = handler->context.region;
        }
      }

      /* No warning print here, after full-area and back regions are remade. */
      if (region) {
        *r_region = region;
      }
    }
  }
}

static void wm_handler_op_context(bContext *C, wmEventHandler_Op *handler, const wmEvent *event)
{
  ScrArea *area = nullptr;
  ARegion *region = nullptr;
  wm_handler_op_context_get_if_valid(C, handler, event, &area, &region);
  CTX_wm_area_set(C, area);
  CTX_wm_region_set(C, region);
}

void WM_event_remove_handlers(bContext *C, ListBase *handlers)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* C is zero on freeing database, modal handlers then already were freed. */
  wmEventHandler *handler_base;
  while ((handler_base = static_cast<wmEventHandler *>(BLI_pophead(handlers)))) {
    BLI_assert(handler_base->type != 0);
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;

      if (handler->op) {
        wmWindow *win = CTX_wm_window(C);

        if (handler->is_fileselect) {
          /* Exit File Browsers referring to this handler/operator. */
          LISTBASE_FOREACH (wmWindow *, temp_win, &wm->windows) {
            ScrArea *file_area = ED_fileselect_handler_area_find(temp_win, handler->op);
            if (!file_area) {
              continue;
            }
            ED_area_exit(C, file_area);
          }
        }

        if (handler->op->type->cancel) {
          ScrArea *area = CTX_wm_area(C);
          ARegion *region = CTX_wm_region(C);

          wm_handler_op_context(C, handler, win->eventstate);

          if (handler->op->type->flag & OPTYPE_UNDO) {
            wm->op_undo_depth++;
          }

          handler->op->type->cancel(C, handler->op);

          if (handler->op->type->flag & OPTYPE_UNDO) {
            wm->op_undo_depth--;
          }

          CTX_wm_area_set(C, area);
          CTX_wm_region_set(C, region);
        }

        WM_cursor_grab_disable(win, nullptr);

        if (handler->is_fileselect) {
          wm_operator_free_for_fileselect(handler->op);
        }
        else {
          WM_operator_free(handler->op);
        }
      }
    }
    else if (handler_base->type == WM_HANDLER_TYPE_UI) {
      wmEventHandler_UI *handler = (wmEventHandler_UI *)handler_base;

      if (handler->remove_fn) {
        ScrArea *area = CTX_wm_area(C);
        ARegion *region = CTX_wm_region(C);
        ARegion *menu = CTX_wm_menu(C);

        if (handler->context.area) {
          CTX_wm_area_set(C, handler->context.area);
        }
        if (handler->context.region) {
          CTX_wm_region_set(C, handler->context.region);
        }
        if (handler->context.menu) {
          CTX_wm_menu_set(C, handler->context.menu);
        }

        handler->remove_fn(C, handler->user_data);

        CTX_wm_area_set(C, area);
        CTX_wm_region_set(C, region);
        CTX_wm_menu_set(C, menu);
      }
    }

    wm_event_free_handler(handler_base);
  }
}

static bool wm_eventmatch(const wmEvent *winevent, const wmKeyMapItem *kmi)
{
  if (kmi->flag & KMI_INACTIVE) {
    return false;
  }

  if (winevent->flag & WM_EVENT_IS_REPEAT) {
    if (kmi->flag & KMI_REPEAT_IGNORE) {
      return false;
    }
  }

  const int kmitype = WM_userdef_event_map(kmi->type);

  /* The matching rules. */
  if (kmitype == KM_TEXTINPUT) {
    if (winevent->val == KM_PRESS) { /* Prevent double clicks. */
      /* Not using #ISTEXTINPUT anymore because (at least on Windows) some key codes above 255
       * could have printable ascii keys, See T30479. */
      if (ISKEYBOARD(winevent->type) && (winevent->ascii || winevent->utf8_buf[0])) {
        return true;
      }
    }
  }

  if (kmitype != KM_ANY) {
    if (ELEM(kmitype, TABLET_STYLUS, TABLET_ERASER)) {
      const wmTabletData *wmtab = &winevent->tablet;

      if (winevent->type != LEFTMOUSE) {
        /* Tablet events can occur on hover + key-press. */
        return false;
      }
      if ((kmitype == TABLET_STYLUS) && (wmtab->active != EVT_TABLET_STYLUS)) {
        return false;
      }
      if ((kmitype == TABLET_ERASER) && (wmtab->active != EVT_TABLET_ERASER)) {
        return false;
      }
    }
    else {
      if (winevent->type != kmitype) {
        return false;
      }
    }
  }

  if (kmi->val != KM_ANY) {
    if (winevent->val != kmi->val) {
      return false;
    }
  }

  if (kmi->val == KM_CLICK_DRAG) {
    if (kmi->direction != KM_ANY) {
      if (kmi->direction != winevent->direction) {
        return false;
      }
    }
  }

  /* Account for rare case of when these keys are used as the 'type' not as modifiers. */
  if (kmi->shift != KM_ANY) {
    const bool shift = (winevent->modifier & KM_SHIFT) != 0;
    if ((shift != (bool)kmi->shift) &&
        !ELEM(winevent->type, EVT_LEFTSHIFTKEY, EVT_RIGHTSHIFTKEY)) {
      return false;
    }
  }
  if (kmi->ctrl != KM_ANY) {
    const bool ctrl = (winevent->modifier & KM_CTRL) != 0;
    if (ctrl != (bool)kmi->ctrl && !ELEM(winevent->type, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY)) {
      return false;
    }
  }
  if (kmi->alt != KM_ANY) {
    const bool alt = (winevent->modifier & KM_ALT) != 0;
    if (alt != (bool)kmi->alt && !ELEM(winevent->type, EVT_LEFTALTKEY, EVT_RIGHTALTKEY)) {
      return false;
    }
  }
  if (kmi->oskey != KM_ANY) {
    const bool oskey = (winevent->modifier & KM_OSKEY) != 0;
    if ((oskey != (bool)kmi->oskey) && (winevent->type != EVT_OSKEY)) {
      return false;
    }
  }

  /* Only key-map entry with key-modifier is checked,
   * means all keys without modifier get handled too. */
  /* That is currently needed to make overlapping events work (when you press A - G fast or so). */
  if (kmi->keymodifier) {
    if (winevent->keymodifier != kmi->keymodifier) {
      return false;
    }
  }

  return true;
}

static wmKeyMapItem *wm_eventmatch_modal_keymap_items(const wmKeyMap *keymap,
                                                      wmOperator *op,
                                                      const wmEvent *event)
{
  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
    /* Should already be handled by #wm_user_modal_keymap_set_items. */
    BLI_assert(kmi->propvalue_str[0] == '\0');
    if (wm_eventmatch(event, kmi)) {
      if ((keymap->poll_modal_item == nullptr) || (keymap->poll_modal_item(op, kmi->propvalue))) {
        return kmi;
      }
    }
  }
  return nullptr;
}

struct wmEvent_ModalMapStore {
  short prev_type;
  short prev_val;

  bool dbl_click_disabled;
};

/**
 * This function prepares events for use with #wmOperatorType.modal by:
 *
 * - Matching key-map items with the operators modal key-map.
 * - Converting double click events into press events,
 *   allowing them to be restored when the events aren't handled.
 *
 *   This is done since we only want to use double click events to match key-map items,
 *   allowing modal functions to check for press/release events without having to interpret them.
 */
static void wm_event_modalkeymap_begin(const bContext *C,
                                       wmOperator *op,
                                       wmEvent *event,
                                       wmEvent_ModalMapStore *event_backup)
{
  BLI_assert(event->type != EVT_MODAL_MAP);

  /* Support for modal key-map in macros. */
  if (op->opm) {
    op = op->opm;
  }

  event_backup->dbl_click_disabled = false;

  if (op->type->modalkeymap) {
    wmKeyMap *keymap = WM_keymap_active(CTX_wm_manager(C), op->type->modalkeymap);
    wmKeyMapItem *kmi = nullptr;

    const wmEvent *event_match = nullptr;
    wmEvent event_no_dbl_click;

    if ((kmi = wm_eventmatch_modal_keymap_items(keymap, op, event))) {
      event_match = event;
    }
    else if (event->val == KM_DBL_CLICK) {
      event_no_dbl_click = *event;
      event_no_dbl_click.val = KM_PRESS;
      if ((kmi = wm_eventmatch_modal_keymap_items(keymap, op, &event_no_dbl_click))) {
        event_match = &event_no_dbl_click;
      }
    }

    if (event_match != nullptr) {
      event_backup->prev_type = event->prev_type;
      event_backup->prev_val = event->prev_val;

      event->prev_type = event_match->type;
      event->prev_val = event_match->val;
      event->type = EVT_MODAL_MAP;
      event->val = kmi->propvalue;

      /* Avoid double-click events even in the case of #EVT_MODAL_MAP,
       * since it's possible users configure double-click key-map items
       * which would break when modal functions expect press/release. */
      if (event->prev_type == KM_DBL_CLICK) {
        event->prev_type = KM_PRESS;
        event_backup->dbl_click_disabled = true;
      }
    }
  }

  if (event->type != EVT_MODAL_MAP) {
    /* This bypass just disables support for double-click in modal handlers. */
    if (event->val == KM_DBL_CLICK) {
      event->val = KM_PRESS;
      event_backup->dbl_click_disabled = true;
    }
  }
}

/**
 * Restore changes from #wm_event_modalkeymap_begin
 *
 * \warning bad hacking event system...
 * better restore event type for checking of #KM_CLICK for example.
 * Modal maps could use different method (ton).
 */
static void wm_event_modalkeymap_end(wmEvent *event, const wmEvent_ModalMapStore *event_backup)
{
  if (event->type == EVT_MODAL_MAP) {
    event->type = event->prev_type;
    event->val = event->prev_val;

    event->prev_type = event_backup->prev_type;
    event->prev_val = event_backup->prev_val;
  }

  if (event_backup->dbl_click_disabled) {
    event->val = KM_DBL_CLICK;
  }
}

/**
 * \warning this function removes a modal handler, when finished.
 */
static int wm_handler_operator_call(bContext *C,
                                    ListBase *handlers,
                                    wmEventHandler *handler_base,
                                    wmEvent *event,
                                    PointerRNA *properties,
                                    const char *kmi_idname)
{
  int retval = OPERATOR_PASS_THROUGH;

  /* Derived, modal or blocking operator. */
  if ((handler_base->type == WM_HANDLER_TYPE_OP) &&
      (((wmEventHandler_Op *)handler_base)->op != nullptr)) {
    wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
    wmOperator *op = handler->op;
    wmOperatorType *ot = op->type;

    if (!wm_operator_check_locked_interface(C, ot)) {
      /* Interface is locked and operator is not allowed to run,
       * nothing to do in this case. */
    }
    else if (ot->modal) {
      /* We set context to where modal handler came from. */
      wmWindowManager *wm = CTX_wm_manager(C);
      wmWindow *win = CTX_wm_window(C);
      ScrArea *area = CTX_wm_area(C);
      ARegion *region = CTX_wm_region(C);

      wm_handler_op_context(C, handler, event);
      wm_region_mouse_co(C, event);

      wmEvent_ModalMapStore event_backup;
      wm_event_modalkeymap_begin(C, op, event, &event_backup);

      if (ot->flag & OPTYPE_UNDO) {
        wm->op_undo_depth++;
      }

      /* Warning, after this call all context data and 'event' may be freed. see check below. */
      retval = ot->modal(C, op, event);
      OPERATOR_RETVAL_CHECK(retval);

      if (ot->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm) {
        wm->op_undo_depth--;
      }

      /* When the window changes the modal modifier may have loaded a new blend file
       * (the `system_demo_mode` add-on does this), so we have to assume the event,
       * operator, area, region etc have all been freed. */
      if ((CTX_wm_window(C) == win)) {

        wm_event_modalkeymap_end(event, &event_backup);

        if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED)) {
          wm_operator_reports(C, op, retval, false);

          if (op->type->modalkeymap) {
            WM_window_status_area_tag_redraw(win);
          }
        }
        else {
          /* Not very common, but modal operators may report before finishing. */
          if (!BLI_listbase_is_empty(&op->reports->list)) {
            wm_add_reports(op->reports);
          }
        }

        /* Important to run 'wm_operator_finished' before nullptr-ing the context members. */
        if (retval & OPERATOR_FINISHED) {
          wm_operator_finished(C, op, false, true);
          handler->op = nullptr;
        }
        else if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED)) {
          WM_operator_free(op);
          handler->op = nullptr;
        }

        /* Putting back screen context, `reval` can pass through after modal failures! */
        if ((retval & OPERATOR_PASS_THROUGH) || wm_event_always_pass(event)) {
          CTX_wm_area_set(C, area);
          CTX_wm_region_set(C, region);
        }
        else {
          /* This special cases is for areas and regions that get removed. */
          CTX_wm_area_set(C, nullptr);
          CTX_wm_region_set(C, nullptr);
        }

        /* Update gizmos during modal handlers. */
        wm_gizmomaps_handled_modal_update(C, event, handler);

        /* Remove modal handler, operator itself should have been canceled and freed. */
        if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED)) {
          WM_cursor_grab_disable(CTX_wm_window(C), nullptr);

          BLI_remlink(handlers, handler);
          wm_event_free_handler(&handler->head);

          /* Prevent silly errors from operator users. */
          // retval &= ~OPERATOR_PASS_THROUGH;
        }
      }
    }
    else {
      CLOG_ERROR(WM_LOG_HANDLERS, "missing modal '%s'", op->idname);
    }
  }
  else {
    wmOperatorType *ot = WM_operatortype_find(kmi_idname, false);

    if (ot && wm_operator_check_locked_interface(C, ot)) {
      bool use_last_properties = true;
      PointerRNA tool_properties = {nullptr};

      bToolRef *keymap_tool = nullptr;
      if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
        keymap_tool = ((wmEventHandler_Keymap *)handler_base)->keymap_tool;
      }
      else if (handler_base->type == WM_HANDLER_TYPE_GIZMO) {
        wmGizmoMap *gizmo_map = ((wmEventHandler_Gizmo *)handler_base)->gizmo_map;
        wmGizmo *gz = wm_gizmomap_highlight_get(gizmo_map);
        if (gz && (gz->flag & WM_GIZMO_OPERATOR_TOOL_INIT)) {
          keymap_tool = WM_toolsystem_ref_from_context(C);
        }
      }

      const bool is_tool = (keymap_tool != nullptr);
      const bool use_tool_properties = is_tool;

      if (use_tool_properties) {
        WM_toolsystem_ref_properties_init_for_keymap(
            keymap_tool, &tool_properties, properties, ot);
        properties = &tool_properties;
        use_last_properties = false;
      }

      retval = wm_operator_invoke(C, ot, event, properties, nullptr, false, use_last_properties);

      if (use_tool_properties) {
        WM_operator_properties_free(&tool_properties);
      }

      /* Link gizmo if #WM_GIZMOGROUPTYPE_TOOL_INIT is set. */
      if (retval & OPERATOR_FINISHED) {
        if (is_tool) {
          bToolRef_Runtime *tref_rt = keymap_tool->runtime;
          if (tref_rt->gizmo_group[0]) {
            const char *idname = tref_rt->gizmo_group;
            wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
            if (gzgt != nullptr) {
              if ((gzgt->flag & WM_GIZMOGROUPTYPE_TOOL_INIT) != 0) {
                ARegion *region = CTX_wm_region(C);
                if (region != nullptr) {
                  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
                  WM_gizmo_group_type_ensure_ptr_ex(gzgt, gzmap_type);
                  wmGizmoGroup *gzgroup = WM_gizmomaptype_group_init_runtime_with_region(
                      gzmap_type, gzgt, region);
                  /* We can't rely on drawing to initialize gizmo's since disabling
                   * overlays/gizmos will prevent pre-drawing setup calls. (see T60905) */
                  WM_gizmogroup_ensure_init(C, gzgroup);
                }
              }
            }
          }
        }
      }
      /* Done linking gizmo. */
    }
  }

  /* Finished and pass through flag as handled. */
  if (retval == (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH)) {
    return WM_HANDLER_HANDLED;
  }

  /* Modal unhandled, break. */
  if (retval == (OPERATOR_PASS_THROUGH | OPERATOR_RUNNING_MODAL)) {
    return (WM_HANDLER_BREAK | WM_HANDLER_MODAL);
  }

  if (retval & OPERATOR_PASS_THROUGH) {
    return WM_HANDLER_CONTINUE;
  }

  return WM_HANDLER_BREAK;
}

static void wm_operator_free_for_fileselect(wmOperator *file_operator)
{
  LISTBASE_FOREACH (bScreen *, screen, &G_MAIN->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacetype == SPACE_FILE) {
        SpaceFile *sfile = static_cast<SpaceFile *>(area->spacedata.first);
        if (sfile->op == file_operator) {
          sfile->op = nullptr;
        }
      }
    }
  }

  WM_operator_free(file_operator);
}

/**
 * File-select handlers are only in the window queue,
 * so it's safe to switch screens or area types.
 */
static int wm_handler_fileselect_do(bContext *C,
                                    ListBase *handlers,
                                    wmEventHandler_Op *handler,
                                    int val)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  int action = WM_HANDLER_CONTINUE;

  switch (val) {
    case EVT_FILESELECT_FULL_OPEN: {
      wmWindow *win = CTX_wm_window(C);
      ScrArea *area;

      if ((area = ED_screen_temp_space_open(C,
                                            IFACE_("Blender File View"),
                                            WM_window_pixels_x(win) / 2,
                                            WM_window_pixels_y(win) / 2,
                                            U.file_space_data.temp_win_sizex * UI_DPI_FAC,
                                            U.file_space_data.temp_win_sizey * UI_DPI_FAC,
                                            SPACE_FILE,
                                            U.filebrowser_display_type,
                                            true))) {
        ARegion *region_header = BKE_area_find_region_type(area, RGN_TYPE_HEADER);

        BLI_assert(area->spacetype == SPACE_FILE);

        region_header->flag |= RGN_FLAG_HIDDEN;
        /* Header on bottom, #AZone triangle to toggle header looks misplaced at the top. */
        region_header->alignment = RGN_ALIGN_BOTTOM;

        /* Settings for file-browser, #sfile is not operator owner but sends events. */
        SpaceFile *sfile = (SpaceFile *)area->spacedata.first;
        sfile->op = handler->op;

        ED_fileselect_set_params_from_userdef(sfile);
      }
      else {
        BKE_report(&wm->reports, RPT_ERROR, "Failed to open window!");
        return OPERATOR_CANCELLED;
      }

      action = WM_HANDLER_BREAK;
      break;
    }

    case EVT_FILESELECT_EXEC:
    case EVT_FILESELECT_CANCEL:
    case EVT_FILESELECT_EXTERNAL_CANCEL: {
      wmWindow *ctx_win = CTX_wm_window(C);
      wmEvent *eventstate = ctx_win->eventstate;
      /* The root window of the operation as determined in #WM_event_add_fileselect(). */
      wmWindow *root_win = handler->context.win;

      /* Remove link now, for load file case before removing. */
      BLI_remlink(handlers, handler);

      if (val == EVT_FILESELECT_EXTERNAL_CANCEL) {
        /* The window might have been freed already. */
        if (BLI_findindex(&wm->windows, handler->context.win) == -1) {
          handler->context.win = nullptr;
        }
      }
      else {
        ScrArea *ctx_area = CTX_wm_area(C);

        wmWindow *temp_win = nullptr;
        LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
          bScreen *screen = WM_window_get_active_screen(win);
          ScrArea *file_area = static_cast<ScrArea *>(screen->areabase.first);

          if ((file_area->spacetype != SPACE_FILE) || !WM_window_is_temp_screen(win)) {
            continue;
          }

          if (file_area->full) {
            /* Users should not be able to maximize/full-screen an area in a temporary screen.
             * So if there's a maximized file browser in a temporary screen,
             * it was likely opened by #EVT_FILESELECT_FULL_OPEN. */
            continue;
          }

          int win_size[2];
          bool is_maximized;
          ED_fileselect_window_params_get(win, win_size, &is_maximized);
          ED_fileselect_params_to_userdef(
              static_cast<SpaceFile *>(file_area->spacedata.first), win_size, is_maximized);

          if (BLI_listbase_is_single(&file_area->spacedata)) {
            BLI_assert(root_win != win);

            wm_window_close(C, wm, win);

            CTX_wm_window_set(C, root_win); /* #wm_window_close() nullptrs. */
            /* Some operators expect a drawable context (for #EVT_FILESELECT_EXEC). */
            wm_window_make_drawable(wm, root_win);
            /* Ensure correct cursor position, otherwise, popups may close immediately after
             * opening (#UI_BLOCK_MOVEMOUSE_QUIT). */
            wm_cursor_position_get(root_win, &eventstate->xy[0], &eventstate->xy[1]);
            wm->winactive = root_win; /* Reports use this... */
          }
          else if (file_area->full) {
            ED_screen_full_prevspace(C, file_area);
          }
          else {
            ED_area_prevspace(C, file_area);
          }

          temp_win = win;
          break;
        }

        if (!temp_win && ctx_area->full) {
          ED_fileselect_params_to_userdef(
              static_cast<SpaceFile *>(ctx_area->spacedata.first), nullptr, false);
          ED_screen_full_prevspace(C, ctx_area);
        }
      }

      CTX_wm_window_set(C, root_win);
      wm_handler_op_context(C, handler, eventstate);
      /* At this point context is supposed to match the root context determined by
       * #WM_event_add_fileselect(). */
      BLI_assert(!CTX_wm_area(C) || (CTX_wm_area(C) == handler->context.area));
      BLI_assert(!CTX_wm_region(C) || (CTX_wm_region(C) == handler->context.region));

      ScrArea *handler_area = CTX_wm_area(C);
      /* Make sure new context area is ready, the operator callback may operate on it. */
      if (handler_area) {
        ED_area_do_refresh(C, handler_area);
      }

      /* Needed for #UI_popup_menu_reports. */

      if (val == EVT_FILESELECT_EXEC) {
        int retval;

        if (handler->op->type->flag & OPTYPE_UNDO) {
          wm->op_undo_depth++;
        }

        retval = handler->op->type->exec(C, handler->op);

        /* XXX check this carefully, `CTX_wm_manager(C) == wm` is a bit hackish. */
        if (handler->op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm) {
          wm->op_undo_depth--;
        }

        /* XXX check this carefully, `CTX_wm_manager(C) == wm` is a bit hackish. */
        if (CTX_wm_manager(C) == wm && wm->op_undo_depth == 0) {
          if (handler->op->type->flag & OPTYPE_UNDO) {
            ED_undo_push_op(C, handler->op);
          }
          else if (handler->op->type->flag & OPTYPE_UNDO_GROUPED) {
            ED_undo_grouped_push_op(C, handler->op);
          }
        }

        if (handler->op->reports->list.first) {

          /* FIXME(@campbellbarton): temp setting window, this is really bad!
           * only have because lib linking errors need to be seen by users :(
           * it can be removed without breaking anything but then no linking errors. */
          wmWindow *win_prev = CTX_wm_window(C);
          ScrArea *area_prev = CTX_wm_area(C);
          ARegion *region_prev = CTX_wm_region(C);

          if (win_prev == nullptr) {
            CTX_wm_window_set(C, static_cast<wmWindow *>(CTX_wm_manager(C)->windows.first));
          }

          BKE_report_print_level_set(handler->op->reports, RPT_WARNING);
          UI_popup_menu_reports(C, handler->op->reports);

          /* XXX: copied from #wm_operator_finished(). */
          /* Add reports to the global list, otherwise they are not seen. */
          BLI_movelisttolist(&CTX_wm_reports(C)->list, &handler->op->reports->list);

          /* More hacks, since we meddle with reports, banner display doesn't happen automatic. */
          WM_report_banner_show();

          CTX_wm_window_set(C, win_prev);
          CTX_wm_area_set(C, area_prev);
          CTX_wm_region_set(C, region_prev);
        }

        /* For #WM_operator_pystring only, custom report handling is done above. */
        wm_operator_reports(C, handler->op, retval, true);

        if (retval & OPERATOR_FINISHED) {
          WM_operator_last_properties_store(handler->op);
        }

        if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED)) {
          wm_operator_free_for_fileselect(handler->op);
        }
      }
      else {
        if (handler->op->type->cancel) {
          if (handler->op->type->flag & OPTYPE_UNDO) {
            wm->op_undo_depth++;
          }

          handler->op->type->cancel(C, handler->op);

          if (handler->op->type->flag & OPTYPE_UNDO) {
            wm->op_undo_depth--;
          }
        }
        wm_operator_free_for_fileselect(handler->op);
      }

      CTX_wm_area_set(C, nullptr);

      wm_event_free_handler(&handler->head);

      action = WM_HANDLER_BREAK;
      break;
    }
  }

  return action;
}

static int wm_handler_fileselect_call(bContext *C,
                                      ListBase *handlers,
                                      wmEventHandler_Op *handler,
                                      const wmEvent *event)
{
  int action = WM_HANDLER_CONTINUE;

  if (event->type != EVT_FILESELECT) {
    return action;
  }
  if (handler->op != (wmOperator *)event->customdata) {
    return action;
  }

  return wm_handler_fileselect_do(C, handlers, handler, event->val);
}

static int wm_action_not_handled(int action)
{
  return action == WM_HANDLER_CONTINUE || action == (WM_HANDLER_BREAK | WM_HANDLER_MODAL);
}

static const char *keymap_handler_log_action_str(const int action)
{
  if (action & WM_HANDLER_BREAK) {
    return "handled";
  }
  if (action & WM_HANDLER_HANDLED) {
    return "handled (and pass on)";
  }
  return "un-handled";
}

static const char *keymap_handler_log_kmi_event_str(const wmKeyMapItem *kmi,
                                                    char *buf,
                                                    size_t buf_maxlen)
{
  /* Short representation of the key that was pressed,
   * include this since it may differ from the event in minor details
   * which can help looking up the key-map definition. */
  WM_keymap_item_to_string(kmi, false, buf, buf_maxlen);
  return buf;
}

static const char *keymap_handler_log_kmi_op_str(bContext *C,
                                                 const wmKeyMapItem *kmi,
                                                 char *buf,
                                                 size_t buf_maxlen)
{
  /* The key-map item properties can further help distinguish this item from others. */
  char *kmi_props = nullptr;
  if (kmi->properties != nullptr) {
    wmOperatorType *ot = WM_operatortype_find(kmi->idname, false);
    if (ot) {
      kmi_props = RNA_pointer_as_string_keywords(C, kmi->ptr, false, false, true, 512);
    }
    else { /* Fallback. */
      kmi_props = IDP_reprN(kmi->properties, nullptr);
    }
  }
  BLI_snprintf(buf, buf_maxlen, "%s(%s)", kmi->idname, kmi_props ? kmi_props : "");
  if (kmi_props != nullptr) {
    MEM_freeN(kmi_props);
  }
  return buf;
}

#define PRINT \
  if (do_debug_handler) \
  printf

static int wm_handlers_do_keymap_with_keymap_handler(
    /* From 'wm_handlers_do_intern'. */
    bContext *C,
    wmEvent *event,
    ListBase *handlers,
    wmEventHandler_Keymap *handler,
    /* Additional. */
    wmKeyMap *keymap,
    const bool do_debug_handler)
{
  int action = WM_HANDLER_CONTINUE;

  if (keymap == nullptr) {
    /* Only callback is allowed to have nullptr key-maps. */
    BLI_assert(handler->dynamic.keymap_fn);
  }
  else {
    PRINT("%s:   checking '%s' ...", __func__, keymap->idname);

    if (WM_keymap_poll(C, keymap)) {

      PRINT("pass\n");

      LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
        if (wm_eventmatch(event, kmi)) {
          wmEventHandler_KeymapPost keymap_post = handler->post;

          action |= wm_handler_operator_call(
              C, handlers, &handler->head, event, kmi->ptr, kmi->idname);

          char op_buf[512];
          char kmi_buf[128];
          CLOG_INFO(WM_LOG_HANDLERS,
                    2,
                    "keymap '%s', %s, %s, event: %s",
                    keymap->idname,
                    keymap_handler_log_kmi_op_str(C, kmi, op_buf, sizeof(op_buf)),
                    keymap_handler_log_action_str(action),
                    keymap_handler_log_kmi_event_str(kmi, kmi_buf, sizeof(kmi_buf)));

          if (action & WM_HANDLER_BREAK) {
            /* Not always_pass here, it denotes removed handler_base. */
            if (keymap_post.post_fn != nullptr) {
              keymap_post.post_fn(keymap, kmi, keymap_post.user_data);
            }
            break;
          }
        }
      }
    }
    else {
      PRINT("fail\n");
    }
  }

  return action;
}

static int wm_handlers_do_keymap_with_gizmo_handler(
    /* From 'wm_handlers_do_intern' */
    bContext *C,
    wmEvent *event,
    ListBase *handlers,
    wmEventHandler_Gizmo *handler,
    /* Additional. */
    wmGizmoGroup *gzgroup,
    wmKeyMap *keymap,
    const bool do_debug_handler,
    bool *r_keymap_poll)
{
  int action = WM_HANDLER_CONTINUE;
  bool keymap_poll = false;

  PRINT("%s:   checking '%s' ...", __func__, keymap->idname);

  if (WM_keymap_poll(C, keymap)) {
    keymap_poll = true;
    PRINT("pass\n");
    LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
      if (wm_eventmatch(event, kmi)) {
        PRINT("%s:     item matched '%s'\n", __func__, kmi->idname);

        CTX_wm_gizmo_group_set(C, gzgroup);

        /* `handler->op` is called later, we want key-map op to be triggered here. */
        action |= wm_handler_operator_call(
            C, handlers, &handler->head, event, kmi->ptr, kmi->idname);

        CTX_wm_gizmo_group_set(C, nullptr);

        if (action & WM_HANDLER_BREAK) {
          if (G.debug & (G_DEBUG_EVENTS | G_DEBUG_HANDLERS)) {
            printf("%s:       handled - and pass on! '%s'\n", __func__, kmi->idname);
          }
          break;
        }
        if (action & WM_HANDLER_HANDLED) {
          if (G.debug & (G_DEBUG_EVENTS | G_DEBUG_HANDLERS)) {
            printf("%s:       handled - and pass on! '%s'\n", __func__, kmi->idname);
          }
        }
        else {
          PRINT("%s:       un-handled '%s'\n", __func__, kmi->idname);
        }
      }
    }
  }
  else {
    PRINT("fail\n");
  }

  if (r_keymap_poll) {
    *r_keymap_poll = keymap_poll;
  }

  return action;
}

static int wm_handlers_do_gizmo_handler(bContext *C,
                                        wmWindowManager *wm,
                                        wmEventHandler_Gizmo *handler,
                                        wmEvent *event,
                                        ListBase *handlers,
                                        const bool do_debug_handler)
{
  /* Drag events use the previous click location to highlight the gizmos,
   * Get the highlight again in case the user dragged off the gizmo. */
  const bool is_event_drag = (event->val == KM_CLICK_DRAG);
  const bool is_event_modifier = ISKEYMODIFIER(event->type);
  /* Only keep the highlight if the gizmo becomes modal as result of event handling.
   * Without this check, even un-handled drag events will set the highlight if the drag
   * was initiated over a gizmo. */
  const bool restore_highlight_unless_activated = is_event_drag;

  int action = WM_HANDLER_CONTINUE;
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  wmGizmoMap *gzmap = handler->gizmo_map;
  BLI_assert(gzmap != nullptr);
  wmGizmo *gz = wm_gizmomap_highlight_get(gzmap);

  /* Needed so UI blocks over gizmos don't let events fall through to the gizmos,
   * noticeable for the node editor - where dragging on a node should move it, see: T73212.
   * note we still allow for starting the gizmo drag outside, then travel 'inside' the node. */
  if (region->type->clip_gizmo_events_by_ui) {
    if (UI_region_block_find_mouse_over(region, event->xy, true)) {
      if (gz != nullptr && event->type != EVT_GIZMO_UPDATE) {
        if (restore_highlight_unless_activated == false) {
          WM_tooltip_clear(C, CTX_wm_window(C));
          wm_gizmomap_highlight_set(gzmap, C, nullptr, 0);
        }
      }
      return action;
    }
  }

  struct PrevGizmoData {
    wmGizmo *gz_modal;
    wmGizmo *gz;
    int part;
  };
  PrevGizmoData prev{};
  prev.gz_modal = wm_gizmomap_modal_get(gzmap);
  prev.gz = gz;
  prev.part = gz ? gz->highlight_part : 0;

  if (region->gizmo_map != handler->gizmo_map) {
    WM_gizmomap_tag_refresh(handler->gizmo_map);
  }

  wm_gizmomap_handler_context_gizmo(C, handler);
  wm_region_mouse_co(C, event);

  bool handle_highlight = false;
  bool handle_keymap = false;

  /* Handle gizmo highlighting. */
  if ((prev.gz_modal == nullptr) &&
      ((event->type == MOUSEMOVE) || is_event_modifier || is_event_drag)) {
    handle_highlight = true;
    if (is_event_modifier || is_event_drag) {
      handle_keymap = true;
    }
  }
  else {
    handle_keymap = true;
  }

  /* There is no need to handle this event when the key-map isn't being applied
   * since any change to the highlight will be restored to the previous value. */
  if (restore_highlight_unless_activated) {
    if ((handle_highlight == true) && (handle_keymap == false)) {
      return action;
    }
  }

  if (handle_highlight) {
    int part = -1;
    gz = wm_gizmomap_highlight_find(gzmap, C, event, &part);

    /* If no gizmos are/were active, don't clear tool-tips. */
    if (gz || prev.gz) {
      if ((prev.gz != gz) || (prev.part != part)) {
        WM_tooltip_clear(C, CTX_wm_window(C));
      }
    }

    if (wm_gizmomap_highlight_set(gzmap, C, gz, part)) {
      if (gz != nullptr) {
        if ((U.flag & USER_TOOLTIPS) && (gz->flag & WM_GIZMO_NO_TOOLTIP) == 0) {
          WM_tooltip_timer_init(C, CTX_wm_window(C), area, region, WM_gizmomap_tooltip_init);
        }
      }
    }
  }

  /* Don't use from now on. */
  bool is_event_handle_all = gz && (gz->flag & WM_GIZMO_EVENT_HANDLE_ALL);

  if (handle_keymap) {
    /* Handle highlight gizmo. */
    if ((gz != nullptr) && (gz->flag & WM_GIZMO_HIDDEN_KEYMAP) == 0) {
      bool keymap_poll = false;
      wmGizmoGroup *gzgroup = gz->parent_gzgroup;
      wmKeyMap *keymap = WM_keymap_active(wm, gz->keymap ? gz->keymap : gzgroup->type->keymap);
      action |= wm_handlers_do_keymap_with_gizmo_handler(
          C, event, handlers, handler, gzgroup, keymap, do_debug_handler, &keymap_poll);

#ifdef USE_GIZMO_MOUSE_PRIORITY_HACK
      if (((action & WM_HANDLER_BREAK) == 0) && !is_event_handle_all && keymap_poll) {
        if ((event->val == KM_PRESS) && ELEM(event->type, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE)) {

          wmEvent event_test_click = *event;
          event_test_click.val = KM_CLICK;

          wmEvent event_test_click_drag = *event;
          event_test_click_drag.val = KM_CLICK_DRAG;

          LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
            if ((kmi->flag & KMI_INACTIVE) == 0) {
              if (wm_eventmatch(&event_test_click, kmi) ||
                  wm_eventmatch(&event_test_click_drag, kmi)) {
                wmOperatorType *ot = WM_operatortype_find(kmi->idname, false);
                if (WM_operator_poll_context(C, ot, WM_OP_INVOKE_DEFAULT)) {
                  is_event_handle_all = true;
                  break;
                }
              }
            }
          }
        }
      }
#endif /* `USE_GIZMO_MOUSE_PRIORITY_HACK` */
    }

    /* Don't use from now on. */
    gz = nullptr;

    /* Fallback to selected gizmo (when un-handled). */
    if ((action & WM_HANDLER_BREAK) == 0) {
      if (WM_gizmomap_is_any_selected(gzmap)) {
        const ListBase *groups = WM_gizmomap_group_list(gzmap);
        LISTBASE_FOREACH (wmGizmoGroup *, gzgroup, groups) {
          if (wm_gizmogroup_is_any_selected(gzgroup)) {
            wmKeyMap *keymap = WM_keymap_active(wm, gzgroup->type->keymap);
            action |= wm_handlers_do_keymap_with_gizmo_handler(
                C, event, handlers, handler, gzgroup, keymap, do_debug_handler, nullptr);
            if (action & WM_HANDLER_BREAK) {
              break;
            }
          }
        }
      }
    }
  }

  if (handle_highlight) {
    if (restore_highlight_unless_activated) {
      /* Check handling the key-map didn't activate a gizmo. */
      wmGizmo *gz_modal = wm_gizmomap_modal_get(gzmap);
      if (!(gz_modal && (gz_modal != prev.gz_modal))) {
        wm_gizmomap_highlight_set(gzmap, C, prev.gz, prev.part);
      }
    }
  }

  if (is_event_handle_all) {
    if (action == WM_HANDLER_CONTINUE) {
      action |= WM_HANDLER_BREAK | WM_HANDLER_MODAL;
    }
  }

  /* Restore the area. */
  CTX_wm_area_set(C, area);
  CTX_wm_region_set(C, region);

  return action;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Handle Single Event (All Handler Types)
 * \{ */

static int wm_handlers_do_intern(bContext *C, wmWindow *win, wmEvent *event, ListBase *handlers)
{
  const bool do_debug_handler =
      (G.debug & G_DEBUG_HANDLERS) &&
      /* Comment this out to flood the console! (if you really want to test). */
      !ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE);

  wmWindowManager *wm = CTX_wm_manager(C);
  int action = WM_HANDLER_CONTINUE;

  if (handlers == nullptr) {
    wm_event_handler_return_value_check(event, action);
    return action;
  }

  /* Modal handlers can get removed in this loop, we keep the loop this way.
   *
   * NOTE: check 'handlers->first' because in rare cases the handlers can be cleared
   * by the event that's called, for eg:
   *
   * Calling a python script which changes the area.type, see T32232. */
  for (wmEventHandler *handler_base = static_cast<wmEventHandler *>(handlers->first),
                      *handler_base_next;
       handler_base && handlers->first;
       handler_base = handler_base_next) {
    handler_base_next = handler_base->next;

    /* During this loop, UI handlers for nested menus can tag multiple handlers free. */
    if (handler_base->flag & WM_HANDLER_DO_FREE) {
      /* Pass. */
    }
    else if (handler_base->poll == nullptr || handler_base->poll(CTX_wm_region(C), event)) {
      /* In advance to avoid access to freed event on window close. */
      const int always_pass = wm_event_always_pass(event);

      /* Modal+blocking handler_base. */
      if (handler_base->flag & WM_HANDLER_BLOCKING) {
        action |= WM_HANDLER_BREAK;
      }

      /* Handle all types here. */
      if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
        wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
        wmEventHandler_KeymapResult km_result;
        WM_event_get_keymaps_from_handler(wm, win, handler, &km_result);
        int action_iter = WM_HANDLER_CONTINUE;
        for (int km_index = 0; km_index < km_result.keymaps_len; km_index++) {
          wmKeyMap *keymap = km_result.keymaps[km_index];
          action_iter |= wm_handlers_do_keymap_with_keymap_handler(
              C, event, handlers, handler, keymap, do_debug_handler);
          if (action_iter & WM_HANDLER_BREAK) {
            break;
          }
        }
        action |= action_iter;

        /* Clear the tool-tip whenever a key binding is handled, without this tool-tips
         * are kept when a modal operators starts (annoying but otherwise harmless). */
        if (action & WM_HANDLER_BREAK) {
          /* Window may be gone after file read. */
          if (CTX_wm_window(C) != nullptr) {
            WM_tooltip_clear(C, CTX_wm_window(C));
          }
        }
      }
      else if (handler_base->type == WM_HANDLER_TYPE_UI) {
        wmEventHandler_UI *handler = (wmEventHandler_UI *)handler_base;
        BLI_assert(handler->handle_fn != nullptr);
        if (!wm->is_interface_locked) {
          action |= wm_handler_ui_call(C, handler, event, always_pass);
        }
      }
      else if (handler_base->type == WM_HANDLER_TYPE_DROPBOX) {
        wmEventHandler_Dropbox *handler = (wmEventHandler_Dropbox *)handler_base;
        if (!wm->is_interface_locked && event->type == EVT_DROP) {
          LISTBASE_FOREACH (wmDropBox *, drop, handler->dropboxes) {
            /* Other drop custom types allowed. */
            if (event->custom == EVT_DATA_DRAGDROP) {
              ListBase *lb = (ListBase *)event->customdata;
              LISTBASE_FOREACH_MUTABLE (wmDrag *, drag, lb) {
                if (drop->poll(C, drag, event)) {
                  wm_drop_prepare(C, drag, drop);

                  /* Pass single matched #wmDrag onto the operator. */
                  BLI_remlink(lb, drag);
                  ListBase single_lb = {nullptr};
                  BLI_addtail(&single_lb, drag);
                  event->customdata = &single_lb;

                  const wmOperatorCallContext opcontext = wm_drop_operator_context_get(drop);
                  int op_retval = wm_operator_call_internal(
                      C, drop->ot, drop->ptr, nullptr, opcontext, false, event);
                  OPERATOR_RETVAL_CHECK(op_retval);

                  if ((op_retval & OPERATOR_CANCELLED) && drop->cancel) {
                    drop->cancel(CTX_data_main(C), drag, drop);
                  }

                  action |= WM_HANDLER_BREAK;

                  /* Free the drags. */
                  WM_drag_free_list(lb);
                  WM_drag_free_list(&single_lb);

                  wm_event_custom_clear(event);

                  wm_drop_end(C, drag, drop);

                  /* XXX file-read case. */
                  if (CTX_wm_window(C) == nullptr) {
                    return action;
                  }

                  /* Escape from drag loop, got freed. */
                  break;
                }
              }
              /* Always exit all drags on a drop event, even if poll didn't succeed. */
              wm_drags_exit(wm, win);
            }
          }
        }
      }
      else if (handler_base->type == WM_HANDLER_TYPE_GIZMO) {
        wmEventHandler_Gizmo *handler = (wmEventHandler_Gizmo *)handler_base;
        action |= wm_handlers_do_gizmo_handler(C, wm, handler, event, handlers, do_debug_handler);
      }
      else if (handler_base->type == WM_HANDLER_TYPE_OP) {
        wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
        if (handler->is_fileselect) {
          if (!wm->is_interface_locked) {
            /* Screen context changes here. */
            action |= wm_handler_fileselect_call(C, handlers, handler, event);
          }
        }
        else {
          action |= wm_handler_operator_call(C, handlers, handler_base, event, nullptr, nullptr);
        }
      }
      else {
        /* Unreachable (handle all types above). */
        BLI_assert_unreachable();
      }

      if (action & WM_HANDLER_BREAK) {
        if (always_pass) {
          action &= ~WM_HANDLER_BREAK;
        }
        else {
          break;
        }
      }
    }

    /* File-read case, if the wm is freed then the handler's
     * will have been too so the code below need not run. */
    if (CTX_wm_window(C) == nullptr) {
      return action;
    }

    /* Code this for all modal ops, and ensure free only happens here. */

    /* The handler Could be freed already by regular modal ops. */
    if (BLI_findindex(handlers, handler_base) != -1) {
      /* Modal UI handler can be tagged to be freed. */
      if (handler_base->flag & WM_HANDLER_DO_FREE) {
        BLI_remlink(handlers, handler_base);
        wm_event_free_handler(handler_base);
      }
    }
  }

  if (action == (WM_HANDLER_BREAK | WM_HANDLER_MODAL)) {
    wm_cursor_arrow_move(CTX_wm_window(C), event);
  }

  /* Do some extra sanity checking before returning the action. */
  if (CTX_wm_window(C) != nullptr) {
    wm_event_handler_return_value_check(event, action);
  }
  return action;
}

#undef PRINT

/* This calls handlers twice - to solve (double-)click events. */
static int wm_handlers_do(bContext *C, wmEvent *event, ListBase *handlers)
{
  int action = wm_handlers_do_intern(C, CTX_wm_window(C), event, handlers);

  /* Will be nullptr in the file read case. */
  wmWindow *win = CTX_wm_window(C);
  if (win == nullptr) {
    return action;
  }

  if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
    /* Test for #KM_CLICK_DRAG events. */

    /* NOTE(@campbellbarton): Needed so drag can be used for editors that support both click
     * selection and passing through the drag action to box select. See #WM_generic_select_modal.
     * Unlike click, accept `action` when break isn't set.
     * Operators can return `OPERATOR_FINISHED | OPERATOR_PASS_THROUGH` which results
     * in `action` setting #WM_HANDLER_HANDLED, but not #WM_HANDLER_BREAK. */
    if ((action & WM_HANDLER_BREAK) == 0 || wm_action_not_handled(action)) {
      if (win->event_queue_check_drag) {
        if ((event->flag & WM_EVENT_FORCE_DRAG_THRESHOLD) ||
            WM_event_drag_test(event, event->prev_press_xy)) {
          win->event_queue_check_drag_handled = true;
          const int direction = WM_event_drag_direction(event);

          /* Intentionally leave `event->xy` as-is, event users are expected to use
           * `event->prev_press_xy` if they need to access the drag start location. */
          const short prev_val = event->val;
          const short prev_type = event->type;
          const uint8_t prev_modifier = event->modifier;
          const short prev_keymodifier = event->keymodifier;

          event->val = KM_CLICK_DRAG;
          event->type = event->prev_press_type;
          event->modifier = event->prev_press_modifier;
          event->keymodifier = event->prev_press_keymodifier;
          event->direction = direction;

          CLOG_INFO(WM_LOG_HANDLERS, 1, "handling CLICK_DRAG");

          action |= wm_handlers_do_intern(C, win, event, handlers);

          event->direction = 0;
          event->keymodifier = prev_keymodifier;
          event->modifier = prev_modifier;
          event->val = prev_val;
          event->type = prev_type;

          win->event_queue_check_click = false;
          if (!((action & WM_HANDLER_BREAK) == 0 || wm_action_not_handled(action))) {
            /* Only disable when handled as other handlers may use this drag event. */
            CLOG_INFO(WM_LOG_HANDLERS, 3, "canceling CLICK_DRAG: drag was generated & handled");
            win->event_queue_check_drag = false;
          }
        }
      }
    }
    else {
      if (win->event_queue_check_drag) {
        CLOG_INFO(WM_LOG_HANDLERS, 3, "canceling CLICK_DRAG: motion event was handled");
        win->event_queue_check_drag = false;
      }
    }
  }
  else if (ISKEYBOARD_OR_BUTTON(event->type)) {
    /* All events that don't set #wmEvent.prev_type must be ignored. */

    /* Test for CLICK events. */
    if (wm_action_not_handled(action)) {
      /* #wmWindow.eventstate stores if previous event was a #KM_PRESS, in case that
       * wasn't handled, the #KM_RELEASE will become a #KM_CLICK. */

      if (event->val == KM_PRESS) {
        if ((event->flag & WM_EVENT_IS_REPEAT) == 0) {
          win->event_queue_check_click = true;

          CLOG_INFO(WM_LOG_HANDLERS, 3, "detecting CLICK_DRAG: press event detected");
          win->event_queue_check_drag = true;

          win->event_queue_check_drag_handled = false;
        }
      }
      else if (event->val == KM_RELEASE) {
        if (win->event_queue_check_drag) {
          if ((event->prev_press_type != event->type) &&
              (ISKEYMODIFIER(event->type) || (event->type == event->prev_press_keymodifier))) {
            /* Support releasing modifier keys without canceling the drag event, see T89989. */
          }
          else {
            CLOG_INFO(
                WM_LOG_HANDLERS, 3, "CLICK_DRAG: canceling (release event didn't match press)");
            win->event_queue_check_drag = false;
          }
        }
      }

      if (event->prev_press_type == event->type) {

        if (event->val == KM_RELEASE) {
          if (event->prev_val == KM_PRESS) {
            if (win->event_queue_check_click) {
              if (WM_event_drag_test(event, event->prev_press_xy)) {
                win->event_queue_check_click = false;
                if (win->event_queue_check_drag) {
                  CLOG_INFO(WM_LOG_HANDLERS,
                            3,
                            "CLICK_DRAG: canceling (key-release exceeds drag threshold)");
                  win->event_queue_check_drag = false;
                }
              }
              else {
                /* Position is where the actual click happens, for more
                 * accurate selecting in case the mouse drifts a little. */
                const int xy[2] = {UNPACK2(event->xy)};

                copy_v2_v2_int(event->xy, event->prev_press_xy);
                event->val = KM_CLICK;

                CLOG_INFO(WM_LOG_HANDLERS, 1, "CLICK: handling");

                action |= wm_handlers_do_intern(C, win, event, handlers);

                event->val = KM_RELEASE;
                copy_v2_v2_int(event->xy, xy);
              }
            }
          }
        }
        else if (event->val == KM_DBL_CLICK) {
          /* The underlying event is a press, so try and handle this. */
          event->val = KM_PRESS;
          action |= wm_handlers_do_intern(C, win, event, handlers);

          /* Revert value if not handled. */
          if (wm_action_not_handled(action)) {
            event->val = KM_DBL_CLICK;
          }
        }
      }
    }
    else {
      win->event_queue_check_click = false;

      if (win->event_queue_check_drag) {
        CLOG_INFO(WM_LOG_HANDLERS,
                  3,
                  "CLICK_DRAG: canceling (button event was handled: value=%d)",
                  event->val);
        win->event_queue_check_drag = false;
      }
    }
  }
  else if (ISMOUSE_WHEEL(event->type) || ISMOUSE_GESTURE(event->type)) {
    /* Modifiers which can trigger click event's,
     * however we don't want this if the mouse wheel has been used, see T74607. */
    if (wm_action_not_handled(action)) {
      /* Pass. */
    }
    else {
      if (ISKEYMODIFIER(event->prev_type)) {
        win->event_queue_check_click = false;
      }
    }
  }

  wm_event_handler_return_value_check(event, action);
  return action;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Queue Utilities
 *
 * Utilities used by #wm_event_do_handlers.
 * \{ */

static bool wm_event_inside_rect(const wmEvent *event, const rcti *rect)
{
  if (wm_event_always_pass(event)) {
    return true;
  }
  if (BLI_rcti_isect_pt_v(rect, event->xy)) {
    return true;
  }
  return false;
}

static ScrArea *area_event_inside(bContext *C, const int xy[2])
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);

  if (screen) {
    ED_screen_areas_iter (win, screen, area) {
      if (BLI_rcti_isect_pt_v(&area->totrct, xy)) {
        return area;
      }
    }
  }
  return nullptr;
}

static ARegion *region_event_inside(bContext *C, const int xy[2])
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);

  if (screen && area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (BLI_rcti_isect_pt_v(&region->winrct, xy)) {
        return region;
      }
    }
  }
  return nullptr;
}

static void wm_paintcursor_tag(bContext *C, wmPaintCursor *pc, ARegion *region)
{
  if (region) {
    for (; pc; pc = pc->next) {
      if (pc->poll == nullptr || pc->poll(C)) {
        wmWindow *win = CTX_wm_window(C);
        WM_paint_cursor_tag_redraw(win, region);
      }
    }
  }
}

/**
 * Called on mouse-move, check updates for `wm->paintcursors`.
 *
 * \note Context was set on active area and region.
 */
static void wm_paintcursor_test(bContext *C, const wmEvent *event)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  if (wm->paintcursors.first) {
    ARegion *region = CTX_wm_region(C);

    if (region) {
      wm_paintcursor_tag(C, static_cast<wmPaintCursor *>(wm->paintcursors.first), region);
    }

    /* If previous position was not in current region, we have to set a temp new context. */
    if (region == nullptr || !BLI_rcti_isect_pt_v(&region->winrct, event->prev_xy)) {
      ScrArea *area = CTX_wm_area(C);

      CTX_wm_area_set(C, area_event_inside(C, event->prev_xy));
      CTX_wm_region_set(C, region_event_inside(C, event->prev_xy));

      wm_paintcursor_tag(
          C, static_cast<wmPaintCursor *>(wm->paintcursors.first), CTX_wm_region(C));

      CTX_wm_area_set(C, area);
      CTX_wm_region_set(C, region);
    }
  }
}

static void wm_event_drag_and_drop_test(wmWindowManager *wm, wmWindow *win, wmEvent *event)
{
  bScreen *screen = WM_window_get_active_screen(win);

  if (BLI_listbase_is_empty(&wm->drags)) {
    return;
  }

  if (event->type == MOUSEMOVE || ISKEYMODIFIER(event->type)) {
    screen->do_draw_drag = true;
  }
  else if (event->type == EVT_ESCKEY) {
    wm_drags_exit(wm, win);
    WM_drag_free_list(&wm->drags);

    screen->do_draw_drag = true;
  }
  else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    event->type = EVT_DROP;

    /* Create custom-data, first free existing. */
    wm_event_custom_free(event);
    wm_event_custom_clear(event);

    event->custom = EVT_DATA_DRAGDROP;
    event->customdata = &wm->drags;
    event->customdata_free = true;

    /* Clear drop icon. */
    screen->do_draw_drag = true;

    /* Restore cursor (disabled, see `wm_dragdrop.c`) */
    // WM_cursor_modal_restore(win);
  }
}

/**
 * Filter out all events of the pie that spawned the last pie unless it's a release event.
 */
static bool wm_event_pie_filter(wmWindow *win, const wmEvent *event)
{
  if (win->pie_event_type_lock && win->pie_event_type_lock == event->type) {
    if (event->val == KM_RELEASE) {
      win->pie_event_type_lock = EVENT_NONE;
      return false;
    }
    return true;
  }
  return false;
}

/**
 * Account for the special case when events are being handled and a file is loaded.
 * In this case event handling exits early, however when "Load UI" is disabled
 * the even will still be in #wmWindow.event_queue.
 *
 * Without this it's possible to continuously handle the same event, see: T76484.
 */
static void wm_event_free_and_remove_from_queue_if_valid(wmEvent *event)
{
  LISTBASE_FOREACH (wmWindowManager *, wm, &G_MAIN->wm) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      if (BLI_remlink_safe(&win->event_queue, event)) {
        wm_event_free(event);
        return;
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Event Queue (Every Window)
 *
 * Handle events for all windows, run from the #WM_main event loop.
 * \{ */

#ifdef WITH_XR_OPENXR
/**
 * Special handling for XR events.
 *
 * Although XR events are added to regular window queues, they are handled in an "off-screen area"
 * context that is owned entirely by XR runtime data and not tied to a window.
 */
static void wm_event_handle_xrevent(bContext *C,
                                    wmWindowManager *wm,
                                    wmWindow *win,
                                    wmEvent *event)
{
  ScrArea *area = WM_xr_session_area_get(&wm->xr);
  if (!area) {
    return;
  }
  BLI_assert(area->spacetype == SPACE_VIEW3D && area->spacedata.first);

  /* Find a valid region for XR operator execution and modal handling. */
  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  if (!region) {
    return;
  }
  BLI_assert(WM_region_use_viewport(area, region)); /* For operators using GPU-based selection. */

  CTX_wm_area_set(C, area);
  CTX_wm_region_set(C, region);

  int action = wm_handlers_do(C, event, &win->modalhandlers);

  if ((action & WM_HANDLER_BREAK) == 0) {
    wmXrActionData *actiondata = static_cast<wmXrActionData *>(event->customdata);
    if (actiondata->ot->modal && event->val == KM_RELEASE) {
      /* Don't execute modal operators on release. */
    }
    else {
      PointerRNA properties{};
      properties.type = actiondata->ot->srna;
      properties.data = actiondata->op_properties;
      if (actiondata->ot->invoke) {
        /* Invoke operator, either executing operator or transferring responsibility to window
         * modal handlers. */
        wm_operator_invoke(C,
                           actiondata->ot,
                           event,
                           actiondata->op_properties ? &properties : nullptr,
                           nullptr,
                           false,
                           false);
      }
      else {
        /* Execute operator. */
        wmOperator *op = wm_operator_create(
            wm, actiondata->ot, actiondata->op_properties ? &properties : nullptr, nullptr);
        if ((WM_operator_call(C, op) & OPERATOR_HANDLED) == 0) {
          WM_operator_free(op);
        }
      }
    }
  }

  CTX_wm_region_set(C, nullptr);
  CTX_wm_area_set(C, nullptr);
}
#endif /* WITH_XR_OPENXR */

static int wm_event_do_region_handlers(bContext *C, wmEvent *event, ARegion *region)
{
  CTX_wm_region_set(C, region);

  /* Call even on non mouse events, since the. */
  wm_region_mouse_co(C, event);

  const wmWindowManager *wm = CTX_wm_manager(C);
  if (!BLI_listbase_is_empty(&wm->drags)) {
    /* Does polls for drop regions and checks #uiButs. */
    /* Need to be here to make sure region context is true. */
    if (ELEM(event->type, MOUSEMOVE, EVT_DROP) || ISKEYMODIFIER(event->type)) {
      wm_drags_check_ops(C, event);
    }
  }

  return wm_handlers_do(C, event, &region->handlers);
}

/**
 * Send event to region handlers in \a area.
 *
 * Two cases:
 * 1) Always pass events (#wm_event_always_pass()) are sent to all regions.
 * 2) Event is passed to the region visually under the cursor (#ED_area_find_region_xy_visual()).
 */
static int wm_event_do_handlers_area_regions(bContext *C, wmEvent *event, ScrArea *area)
{
  /* Case 1. */
  if (wm_event_always_pass(event)) {
    int action = WM_HANDLER_CONTINUE;

    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      action |= wm_event_do_region_handlers(C, event, region);
    }

    wm_event_handler_return_value_check(event, action);
    return action;
  }

  /* Case 2. */
  ARegion *region_hovered = ED_area_find_region_xy_visual(area, RGN_TYPE_ANY, event->xy);
  if (!region_hovered) {
    return WM_HANDLER_CONTINUE;
  }

  return wm_event_do_region_handlers(C, event, region_hovered);
}

void wm_event_do_handlers(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  BLI_assert(ED_undo_is_state_valid(C));

  /* Update key configuration before handling events. */
  WM_keyconfig_update(wm);
  WM_gizmoconfig_update(CTX_data_main(C));

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);

    /* Some safety checks - these should always be set! */
    BLI_assert(WM_window_get_active_scene(win));
    BLI_assert(WM_window_get_active_screen(win));
    BLI_assert(WM_window_get_active_workspace(win));

    if (screen == nullptr) {
      wm_event_free_all(win);
    }
    else {
      Scene *scene = WM_window_get_active_scene(win);
      ViewLayer *view_layer = WM_window_get_active_view_layer(win);
      Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer);
      Scene *scene_eval = (depsgraph != nullptr) ? DEG_get_evaluated_scene(depsgraph) : nullptr;

      if (scene_eval != nullptr) {
        const int is_playing_sound = BKE_sound_scene_playing(scene_eval);

        if (scene_eval->id.recalc & ID_RECALC_FRAME_CHANGE) {
          /* Ignore seek here, the audio will be updated to the scene frame after jump during next
           * dependency graph update. */
        }
        else if (is_playing_sound != -1) {
          bool is_playing_screen;

          is_playing_screen = (ED_screen_animation_playing(wm) != nullptr);

          if (((is_playing_sound == 1) && (is_playing_screen == 0)) ||
              ((is_playing_sound == 0) && (is_playing_screen == 1))) {
            wmWindow *win_ctx = CTX_wm_window(C);
            bScreen *screen_stx = CTX_wm_screen(C);
            Scene *scene_ctx = CTX_data_scene(C);

            CTX_wm_window_set(C, win);
            CTX_wm_screen_set(C, screen);
            CTX_data_scene_set(C, scene);

            ED_screen_animation_play(C, -1, 1);

            CTX_data_scene_set(C, scene_ctx);
            CTX_wm_screen_set(C, screen_stx);
            CTX_wm_window_set(C, win_ctx);
          }

          if (is_playing_sound == 0) {
            const double time = BKE_sound_sync_scene(scene_eval);
            if (isfinite(time)) {
              int ncfra = round(time * FPS);
              if (ncfra != scene->r.cfra) {
                scene->r.cfra = ncfra;
                ED_update_for_newframe(CTX_data_main(C), depsgraph);
                WM_event_add_notifier(C, NC_WINDOW, nullptr);
              }
            }
          }
        }
      }
    }

    wmEvent *event;
    while ((event = static_cast<wmEvent *>(win->event_queue.first))) {
      int action = WM_HANDLER_CONTINUE;

      /* Force handling drag if a key is pressed even if the drag threshold has not been met.
       * Needed so tablet actions (which typically use a larger threshold) can click-drag
       * then press keys - activating the drag action early.
       * Limit to mouse-buttons drag actions interrupted by pressing any non-mouse button.
       * Otherwise pressing two keys on the keyboard will interpret this as a drag action. */
      if (win->event_queue_check_drag) {
        if ((event->val == KM_PRESS) && ((event->flag & WM_EVENT_IS_REPEAT) == 0) &&
            ISKEYBOARD_OR_BUTTON(event->type) && ISMOUSE_BUTTON(event->prev_press_type)) {
          event = wm_event_add_mousemove_to_head(win);
          event->flag |= WM_EVENT_FORCE_DRAG_THRESHOLD;
        }
      }
      const bool event_queue_check_drag_prev = win->event_queue_check_drag;

      /* Active screen might change during handlers, update pointer. */
      screen = WM_window_get_active_screen(win);

      if (G.debug & (G_DEBUG_HANDLERS | G_DEBUG_EVENTS) &&
          !ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
        printf("\n%s: Handling event\n", __func__);
        WM_event_print(event);
      }

      /* Take care of pie event filter. */
      if (wm_event_pie_filter(win, event)) {
        if (!ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
          CLOG_INFO(WM_LOG_HANDLERS, 1, "event filtered due to pie button pressed");
        }
        BLI_remlink(&win->event_queue, event);
        wm_event_free_last_handled(win, event);
        continue;
      }

      CTX_wm_window_set(C, win);

#ifdef WITH_XR_OPENXR
      if (event->type == EVT_XR_ACTION) {
        wm_event_handle_xrevent(C, wm, win, event);
        BLI_remlink(&win->event_queue, event);
        wm_event_free_last_handled(win, event);
        /* Skip mouse event handling below, which is unnecessary for XR events. */
        continue;
      }
#endif

      /* Clear tool-tip on mouse move. */
      if (screen->tool_tip && screen->tool_tip->exit_on_event) {
        if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
          if (len_manhattan_v2v2_int(screen->tool_tip->event_xy, event->xy) >
              WM_EVENT_CURSOR_MOTION_THRESHOLD) {
            WM_tooltip_clear(C, win);
          }
        }
      }

      /* We let modal handlers get active area/region, also wm_paintcursor_test needs it. */
      CTX_wm_area_set(C, area_event_inside(C, event->xy));
      CTX_wm_region_set(C, region_event_inside(C, event->xy));

      /* MVC demands to not draw in event handlers...
       * but we need to leave it for GPU selecting etc. */
      wm_window_make_drawable(wm, win);

      wm_region_mouse_co(C, event);

      /* First we do priority handlers, modal + some limited key-maps. */
      action |= wm_handlers_do(C, event, &win->modalhandlers);

      /* File-read case. */
      if (CTX_wm_window(C) == nullptr) {
        wm_event_free_and_remove_from_queue_if_valid(event);
        return;
      }

      /* Check for a tool-tip. */
      if (screen == WM_window_get_active_screen(win)) {
        if (screen->tool_tip && screen->tool_tip->timer) {
          if ((event->type == TIMER) && (event->customdata == screen->tool_tip->timer)) {
            WM_tooltip_init(C, win);
          }
        }
      }

      /* Check dragging, creates new event or frees, adds draw tag. */
      wm_event_drag_and_drop_test(wm, win, event);

      if ((action & WM_HANDLER_BREAK) == 0) {
        /* NOTE: setting sub-window active should be done here,
         * after modal handlers have been done. */
        if (event->type == MOUSEMOVE) {
          /* State variables in screen, cursors.
           * Also used in `wm_draw.c`, fails for modal handlers though. */
          ED_screen_set_active_region(C, win, event->xy);
          /* For regions having custom cursors. */
          wm_paintcursor_test(C, event);
        }
#ifdef WITH_INPUT_NDOF
        else if (event->type == NDOF_MOTION) {
          win->addmousemove = true;
        }
#endif

        ED_screen_areas_iter (win, screen, area) {
          /* After restoring a screen from SCREENMAXIMIZED we have to wait
           * with the screen handling till the region coordinates are updated. */
          if (screen->skip_handling) {
            /* Restore for the next iteration of wm_event_do_handlers. */
            screen->skip_handling = false;
            break;
          }

          /* Update action-zones if needed,
           * done here because it needs to be independent from redraws. */
          if (area->flag & AREA_FLAG_ACTIONZONES_UPDATE) {
            ED_area_azones_update(area, event->xy);
          }

          if (wm_event_inside_rect(event, &area->totrct)) {
            CTX_wm_area_set(C, area);

            action |= wm_event_do_handlers_area_regions(C, event, area);

            /* File-read case (Python), T29489. */
            if (CTX_wm_window(C) == nullptr) {
              wm_event_free_and_remove_from_queue_if_valid(event);
              return;
            }

            CTX_wm_region_set(C, nullptr);

            if ((action & WM_HANDLER_BREAK) == 0) {
              wm_region_mouse_co(C, event); /* Only invalidates `event->mval` in this case. */
              action |= wm_handlers_do(C, event, &area->handlers);
            }
            CTX_wm_area_set(C, nullptr);

            /* NOTE: do not escape on #WM_HANDLER_BREAK,
             * mouse-move needs handled for previous area. */
          }
        }

        if ((action & WM_HANDLER_BREAK) == 0) {
          /* Also some non-modal handlers need active area/region. */
          CTX_wm_area_set(C, area_event_inside(C, event->xy));
          CTX_wm_region_set(C, region_event_inside(C, event->xy));

          wm_region_mouse_co(C, event);

          action |= wm_handlers_do(C, event, &win->handlers);

          /* File-read case. */
          if (CTX_wm_window(C) == nullptr) {
            wm_event_free_and_remove_from_queue_if_valid(event);
            return;
          }
        }
      }

      /* If press was handled, we don't want to do click. This way
       * press in tool key-map can override click in editor key-map. */
      if (ISMOUSE_BUTTON(event->type) && event->val == KM_PRESS &&
          !wm_action_not_handled(action)) {
        win->event_queue_check_click = false;
      }

      /* If the drag even was handled, don't attempt to keep re-handing the same
       * drag event on every cursor motion, see: T87511. */
      if (win->event_queue_check_drag_handled) {
        win->event_queue_check_drag = false;
        win->event_queue_check_drag_handled = false;
      }

      if (event_queue_check_drag_prev && (win->event_queue_check_drag == false)) {
        wm_region_tag_draw_on_gizmo_delay_refresh_for_tweak(win);
      }

      /* Update previous mouse position for following events to use. */
      copy_v2_v2_int(win->eventstate->prev_xy, event->xy);

      /* Un-link and free here, Blender-quit then frees all. */
      BLI_remlink(&win->event_queue, event);
      wm_event_free_last_handled(win, event);
    }

    /* Only add mouse-move when the event queue was read entirely. */
    if (win->addmousemove && win->eventstate) {
      wmEvent tevent = *(win->eventstate);
      // printf("adding MOUSEMOVE %d %d\n", tevent.xy[0], tevent.xy[1]);
      tevent.type = MOUSEMOVE;
      tevent.val = KM_NOTHING;
      tevent.prev_xy[0] = tevent.xy[0];
      tevent.prev_xy[1] = tevent.xy[1];
      tevent.flag = (eWM_EventFlag)0;
      wm_event_add(win, &tevent);
      win->addmousemove = 0;
    }

    CTX_wm_window_set(C, nullptr);
  }

  /* Update key configuration after handling events. */
  WM_keyconfig_update(wm);
  WM_gizmoconfig_update(CTX_data_main(C));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Selector Handling
 * \{ */

void WM_event_fileselect_event(wmWindowManager *wm, void *ophandle, int eventval)
{
  /* Add to all windows! */
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    wmEvent event = *win->eventstate;

    event.type = EVT_FILESELECT;
    event.val = eventval;
    event.flag = (eWM_EventFlag)0;
    event.customdata = ophandle; /* Only as void pointer type check. */

    wm_event_add(win, &event);
  }
}

/**
 * From the context window, try to find a window that is appropriate for use as root window of a
 * modal File Browser (modal means: there is a #SpaceFile.op to execute). The root window will
 * become the parent of the File Browser and provides a context to execute the file operator in,
 * even after closing the File Browser.
 *
 * An appropriate window is either of the following:
 * * A parent window that does not yet contain a modal File Browser. This is determined using
 *   #ED_fileselect_handler_area_find_any_with_op().
 * * A parent window containing a modal File Browser, but in a maximized/fullscreen state. Users
 *   shouldn't be able to put a temporary screen like the modal File Browser into
 *   maximized/fullscreen state themselves. So this setup indicates that the File Browser was
 *   opened using #USER_TEMP_SPACE_DISPLAY_FULLSCREEN.
 *
 * If no appropriate parent window can be found from the context window, return the first
 * registered window (which can be assumed to be a regular window, e.g. no modal File Browser; this
 * is asserted).
 */
static wmWindow *wm_event_find_fileselect_root_window_from_context(const bContext *C)
{
  wmWindow *ctx_win = CTX_wm_window(C);

  for (wmWindow *ctx_win_or_parent = ctx_win; ctx_win_or_parent;
       ctx_win_or_parent = ctx_win_or_parent->parent) {
    ScrArea *file_area = ED_fileselect_handler_area_find_any_with_op(ctx_win_or_parent);

    if (!file_area) {
      return ctx_win_or_parent;
    }

    if (file_area->full) {
      return ctx_win_or_parent;
    }
  }

  /* Fallback to the first window. */
  const wmWindowManager *wm = CTX_wm_manager(C);
  BLI_assert(!ED_fileselect_handler_area_find_any_with_op(
      static_cast<const wmWindow *>(wm->windows.first)));
  return static_cast<wmWindow *>(wm->windows.first);
}

/* Operator is supposed to have a filled "path" property. */
/* Optional property: file-type (XXX enum?) */

void WM_event_add_fileselect(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *ctx_win = CTX_wm_window(C);

  /* The following vars define the root context. That is essentially the "parent" context of the
   * File Browser operation, to be restored for eventually executing the file operation. */
  wmWindow *root_win = wm_event_find_fileselect_root_window_from_context(C);
  /* Determined later. */
  ScrArea *root_area = nullptr;
  ARegion *root_region = nullptr;

  /* Close any popups, like when opening a file browser from the splash. */
  UI_popup_handlers_remove_all(C, &root_win->modalhandlers);

  /* Setting the context window unsets the context area & screen. Avoid doing that, so operators
   * calling the file browser can operate in the context the browser was opened in. */
  if (ctx_win != root_win) {
    CTX_wm_window_set(C, root_win);
  }

  /* The root window may already have a File Browser open. Cancel it if so, only 1 should be open
   * per window. The root context of this operation is also used for the new operation. */
  LISTBASE_FOREACH_MUTABLE (wmEventHandler *, handler_base, &root_win->modalhandlers) {
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
      if (handler->is_fileselect == false) {
        continue;
      }

      wm_handler_op_context_get_if_valid(
          C, handler, ctx_win->eventstate, &root_area, &root_region);

      ScrArea *file_area = ED_fileselect_handler_area_find(root_win, handler->op);

      if (file_area) {
        CTX_wm_area_set(C, file_area);
        wm_handler_fileselect_do(C, &root_win->modalhandlers, handler, EVT_FILESELECT_CANCEL);
      }
      /* If not found we stop the handler without changing the screen. */
      else {
        wm_handler_fileselect_do(
            C, &root_win->modalhandlers, handler, EVT_FILESELECT_EXTERNAL_CANCEL);
      }
    }
  }

  BLI_assert(root_win != nullptr);
  /* When not reusing the root context from a previous file browsing operation, use the current
   * area & region, if they are inside the root window. */
  if (!root_area && ctx_win == root_win) {
    root_area = CTX_wm_area(C);
    root_region = CTX_wm_region(C);
  }

  wmEventHandler_Op *handler = MEM_cnew<wmEventHandler_Op>(__func__);
  handler->head.type = WM_HANDLER_TYPE_OP;

  handler->is_fileselect = true;
  handler->op = op;
  handler->context.win = root_win;
  handler->context.area = root_area;
  handler->context.region = root_region;

  BLI_addhead(&root_win->modalhandlers, handler);

  /* Check props once before invoking if check is available
   * ensures initial properties are valid. */
  if (op->type->check) {
    op->type->check(C, op); /* Ignore return value. */
  }

  WM_event_fileselect_event(wm, op, EVT_FILESELECT_FULL_OPEN);

  if (ctx_win != root_win) {
    CTX_wm_window_set(C, ctx_win);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modal Operator Handling
 * \{ */

#if 0
/* Lets not expose struct outside wm? */
static void WM_event_set_handler_flag(wmEventHandler *handler, int flag)
{
  handler->flag = flag;
}
#endif

wmEventHandler_Op *WM_event_add_modal_handler(bContext *C, wmOperator *op)
{
  wmEventHandler_Op *handler = MEM_cnew<wmEventHandler_Op>(__func__);
  handler->head.type = WM_HANDLER_TYPE_OP;
  wmWindow *win = CTX_wm_window(C);

  /* Operator was part of macro. */
  if (op->opm) {
    /* Give the mother macro to the handler. */
    handler->op = op->opm;
    /* Mother macro `opm` becomes the macro element. */
    handler->op->opm = op;
  }
  else {
    handler->op = op;
  }

  handler->context.area = CTX_wm_area(C); /* Means frozen screen context for modal handlers! */
  handler->context.region = CTX_wm_region(C);
  handler->context.region_type = handler->context.region ? handler->context.region->regiontype :
                                                           -1;

  BLI_addhead(&win->modalhandlers, handler);

  if (op->type->modalkeymap) {
    WM_window_status_area_tag_redraw(win);
  }

  return handler;
}

void WM_event_modal_handler_area_replace(wmWindow *win, const ScrArea *old_area, ScrArea *new_area)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
      /* File-select handler is quite special.
       * it needs to keep old area stored in handler, so don't change it. */
      if ((handler->context.area == old_area) && (handler->is_fileselect == false)) {
        handler->context.area = new_area;
      }
    }
  }
}

void WM_event_modal_handler_region_replace(wmWindow *win,
                                           const ARegion *old_region,
                                           ARegion *new_region)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
      /* File-select handler is quite special.
       * it needs to keep old region stored in handler, so don't change it. */
      if ((handler->context.region == old_region) && (handler->is_fileselect == false)) {
        handler->context.region = new_region;
        handler->context.region_type = new_region ? new_region->regiontype : (int)RGN_TYPE_WINDOW;
      }
    }
  }
}

wmEventHandler_Keymap *WM_event_add_keymap_handler(ListBase *handlers, wmKeyMap *keymap)
{
  if (!keymap) {
    CLOG_WARN(WM_LOG_HANDLERS, "called with nullptr key-map");
    return nullptr;
  }

  /* Only allow same key-map once. */
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
      wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
      if (handler->keymap == keymap) {
        return handler;
      }
    }
  }

  wmEventHandler_Keymap *handler = MEM_cnew<wmEventHandler_Keymap>(__func__);
  handler->head.type = WM_HANDLER_TYPE_KEYMAP;
  BLI_addtail(handlers, handler);
  handler->keymap = keymap;

  return handler;
}

/**
 * Implements fallback tool when enabled by:
 * #SCE_WORKSPACE_TOOL_FALLBACK, #WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP.
 *
 * This runs before #WM_event_get_keymap_from_toolsystem,
 * allowing both the fallback-tool and active-tool to be activated
 * providing the key-map is configured so the keys don't conflict.
 * For example one mouse button can run the active-tool, another button for the fallback-tool.
 * See T72567.
 *
 * Follow #wmEventHandler_KeymapDynamicFn signature.
 */
static void wm_event_get_keymap_from_toolsystem_ex(wmWindowManager *wm,
                                                   wmWindow *win,
                                                   wmEventHandler_Keymap *handler,
                                                   wmEventHandler_KeymapResult *km_result,
                                                   /* Extra arguments. */
                                                   const bool with_gizmos)
{
  memset(km_result, 0x0, sizeof(*km_result));

  const char *keymap_id_list[ARRAY_SIZE(km_result->keymaps)];
  int keymap_id_list_len = 0;

  /* NOTE(@campbellbarton): If `win` is nullptr, this function may not behave as expected.
   * Assert since this should not happen in practice.
   * If it does, the window could be looked up in `wm` using the `area`.
   * Keep nullptr checks in run-time code since any crashes here are difficult to redo. */
  BLI_assert_msg(win != nullptr, "The window should always be set for tool interactions!");
  const Scene *scene = win ? win->scene : nullptr;

  ScrArea *area = static_cast<ScrArea *>(handler->dynamic.user_data);
  handler->keymap_tool = nullptr;
  bToolRef_Runtime *tref_rt = area->runtime.tool ? area->runtime.tool->runtime : nullptr;

  if (tref_rt && tref_rt->keymap[0]) {
    keymap_id_list[keymap_id_list_len++] = tref_rt->keymap;
  }

  bool is_gizmo_visible = false;
  bool is_gizmo_highlight = false;

  if ((tref_rt && tref_rt->keymap_fallback[0]) &&
      (scene && (scene->toolsettings->workspace_tool_type == SCE_WORKSPACE_TOOL_FALLBACK))) {
    bool add_keymap = false;
    /* Support for the gizmo owning the tool key-map. */

    if (tref_rt->flag & TOOLREF_FLAG_FALLBACK_KEYMAP) {
      add_keymap = true;
    }

    if (with_gizmos && (tref_rt->gizmo_group[0] != '\0')) {
      wmGizmoMap *gzmap = nullptr;
      wmGizmoGroup *gzgroup = nullptr;
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        if (region->gizmo_map != nullptr) {
          gzmap = region->gizmo_map;
          gzgroup = WM_gizmomap_group_find(gzmap, tref_rt->gizmo_group);
          if (gzgroup != nullptr) {
            break;
          }
        }
      }
      if (gzgroup != nullptr) {
        if (gzgroup->type->flag & WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP) {
          /* If all are hidden, don't override. */
          is_gizmo_visible = true;
          wmGizmo *highlight = wm_gizmomap_highlight_get(gzmap);
          if (highlight) {
            is_gizmo_highlight = true;
          }
          add_keymap = true;
        }
      }
    }

    if (add_keymap) {
      keymap_id_list[keymap_id_list_len++] = tref_rt->keymap_fallback;
    }
  }

  if (is_gizmo_visible && !is_gizmo_highlight) {
    if (keymap_id_list_len == 2) {
      SWAP(const char *, keymap_id_list[0], keymap_id_list[1]);
    }
  }

  for (int i = 0; i < keymap_id_list_len; i++) {
    const char *keymap_id = keymap_id_list[i];
    BLI_assert(keymap_id && keymap_id[0]);

    wmKeyMap *km = WM_keymap_list_find_spaceid_or_empty(
        &wm->userconf->keymaps, keymap_id, area->spacetype, RGN_TYPE_WINDOW);
    /* We shouldn't use key-maps from unrelated spaces. */
    if (km == nullptr) {
      printf("Key-map: '%s' not found for tool '%s'\n", keymap_id, area->runtime.tool->idname);
      continue;
    }
    handler->keymap_tool = area->runtime.tool;
    km_result->keymaps[km_result->keymaps_len++] = km;
  }
}

void WM_event_get_keymap_from_toolsystem_with_gizmos(wmWindowManager *wm,
                                                     wmWindow *win,
                                                     wmEventHandler_Keymap *handler,
                                                     wmEventHandler_KeymapResult *km_result)
{
  wm_event_get_keymap_from_toolsystem_ex(wm, win, handler, km_result, true);
}

void WM_event_get_keymap_from_toolsystem(wmWindowManager *wm,
                                         wmWindow *win,
                                         wmEventHandler_Keymap *handler,
                                         wmEventHandler_KeymapResult *km_result)
{
  wm_event_get_keymap_from_toolsystem_ex(wm, win, handler, km_result, false);
}

wmEventHandler_Keymap *WM_event_add_keymap_handler_dynamic(
    ListBase *handlers, wmEventHandler_KeymapDynamicFn *keymap_fn, void *user_data)
{
  if (!keymap_fn) {
    CLOG_WARN(WM_LOG_HANDLERS, "called with nullptr keymap_fn");
    return nullptr;
  }

  /* Only allow same key-map once. */
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
      wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
      if (handler->dynamic.keymap_fn == keymap_fn) {
        /* Maximizing the view needs to update the area. */
        handler->dynamic.user_data = user_data;
        return handler;
      }
    }
  }

  wmEventHandler_Keymap *handler = MEM_cnew<wmEventHandler_Keymap>(__func__);
  handler->head.type = WM_HANDLER_TYPE_KEYMAP;
  BLI_addtail(handlers, handler);
  handler->dynamic.keymap_fn = keymap_fn;
  handler->dynamic.user_data = user_data;

  return handler;
}

wmEventHandler_Keymap *WM_event_add_keymap_handler_priority(ListBase *handlers,
                                                            wmKeyMap *keymap,
                                                            int UNUSED(priority))
{
  WM_event_remove_keymap_handler(handlers, keymap);

  wmEventHandler_Keymap *handler = MEM_cnew<wmEventHandler_Keymap>("event key-map handler");
  handler->head.type = WM_HANDLER_TYPE_KEYMAP;

  BLI_addhead(handlers, handler);
  handler->keymap = keymap;

  return handler;
}

static bool event_or_prev_in_rect(const wmEvent *event, const rcti *rect)
{
  if (BLI_rcti_isect_pt_v(rect, event->xy)) {
    return true;
  }
  if (event->type == MOUSEMOVE && BLI_rcti_isect_pt_v(rect, event->prev_xy)) {
    return true;
  }
  return false;
}

static bool handler_region_v2d_mask_test(const ARegion *region, const wmEvent *event)
{
  rcti rect = region->v2d.mask;
  BLI_rcti_translate(&rect, region->winrct.xmin, region->winrct.ymin);
  return event_or_prev_in_rect(event, &rect);
}

wmEventHandler_Keymap *WM_event_add_keymap_handler_poll(ListBase *handlers,
                                                        wmKeyMap *keymap,
                                                        EventHandlerPoll poll)
{
  wmEventHandler_Keymap *handler = WM_event_add_keymap_handler(handlers, keymap);
  if (handler == nullptr) {
    return nullptr;
  }

  handler->head.poll = poll;
  return handler;
}

wmEventHandler_Keymap *WM_event_add_keymap_handler_v2d_mask(ListBase *handlers, wmKeyMap *keymap)
{
  return WM_event_add_keymap_handler_poll(handlers, keymap, handler_region_v2d_mask_test);
}

void WM_event_remove_keymap_handler(ListBase *handlers, wmKeyMap *keymap)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
      wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
      if (handler->keymap == keymap) {
        BLI_remlink(handlers, handler);
        wm_event_free_handler(&handler->head);
        break;
      }
    }
  }
}

void WM_event_set_keymap_handler_post_callback(wmEventHandler_Keymap *handler,
                                               void(keymap_tag)(wmKeyMap *keymap,
                                                                wmKeyMapItem *kmi,
                                                                void *user_data),
                                               void *user_data)
{
  handler->post.post_fn = keymap_tag;
  handler->post.user_data = user_data;
}

wmEventHandler_UI *WM_event_add_ui_handler(const bContext *C,
                                           ListBase *handlers,
                                           wmUIHandlerFunc handle_fn,
                                           wmUIHandlerRemoveFunc remove_fn,
                                           void *user_data,
                                           const char flag)
{
  wmEventHandler_UI *handler = MEM_cnew<wmEventHandler_UI>(__func__);
  handler->head.type = WM_HANDLER_TYPE_UI;
  handler->handle_fn = handle_fn;
  handler->remove_fn = remove_fn;
  handler->user_data = user_data;
  if (C) {
    handler->context.area = CTX_wm_area(C);
    handler->context.region = CTX_wm_region(C);
    handler->context.menu = CTX_wm_menu(C);
  }
  else {
    handler->context.area = nullptr;
    handler->context.region = nullptr;
    handler->context.menu = nullptr;
  }

  BLI_assert((flag & WM_HANDLER_DO_FREE) == 0);
  handler->head.flag = flag;

  BLI_addhead(handlers, handler);

  return handler;
}

void WM_event_remove_ui_handler(ListBase *handlers,
                                wmUIHandlerFunc handle_fn,
                                wmUIHandlerRemoveFunc remove_fn,
                                void *user_data,
                                const bool postpone)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_UI) {
      wmEventHandler_UI *handler = (wmEventHandler_UI *)handler_base;
      if ((handler->handle_fn == handle_fn) && (handler->remove_fn == remove_fn) &&
          (handler->user_data == user_data)) {
        /* Handlers will be freed in #wm_handlers_do(). */
        if (postpone) {
          handler->head.flag |= WM_HANDLER_DO_FREE;
        }
        else {
          BLI_remlink(handlers, handler);
          wm_event_free_handler(&handler->head);
        }
        break;
      }
    }
  }
}

void WM_event_free_ui_handler_all(bContext *C,
                                  ListBase *handlers,
                                  wmUIHandlerFunc handle_fn,
                                  wmUIHandlerRemoveFunc remove_fn)
{
  LISTBASE_FOREACH_MUTABLE (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_UI) {
      wmEventHandler_UI *handler = (wmEventHandler_UI *)handler_base;
      if ((handler->handle_fn == handle_fn) && (handler->remove_fn == remove_fn)) {
        remove_fn(C, handler->user_data);
        BLI_remlink(handlers, handler);
        wm_event_free_handler(&handler->head);
      }
    }
  }
}

wmEventHandler_Dropbox *WM_event_add_dropbox_handler(ListBase *handlers, ListBase *dropboxes)
{
  /* Only allow same dropbox once. */
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_DROPBOX) {
      wmEventHandler_Dropbox *handler = (wmEventHandler_Dropbox *)handler_base;
      if (handler->dropboxes == dropboxes) {
        return handler;
      }
    }
  }

  wmEventHandler_Dropbox *handler = MEM_cnew<wmEventHandler_Dropbox>(__func__);
  handler->head.type = WM_HANDLER_TYPE_DROPBOX;

  /* Dropbox stored static, no free or copy. */
  handler->dropboxes = dropboxes;
  BLI_addhead(handlers, handler);

  return handler;
}

void WM_event_remove_area_handler(ListBase *handlers, void *area)
{
  /* XXX(@ton): solution works, still better check the real cause. */

  LISTBASE_FOREACH_MUTABLE (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_UI) {
      wmEventHandler_UI *handler = (wmEventHandler_UI *)handler_base;
      if (handler->context.area == area) {
        BLI_remlink(handlers, handler);
        wm_event_free_handler(handler_base);
      }
    }
  }
}

wmOperator *WM_operator_find_modal_by_type(wmWindow *win, const wmOperatorType *ot)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
    if (handler_base->type != WM_HANDLER_TYPE_OP) {
      continue;
    }
    wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
    if (handler->op && handler->op->type == ot) {
      return handler->op;
    }
  }
  return nullptr;
}

#if 0
static void WM_event_remove_handler(ListBase *handlers, wmEventHandler *handler)
{
  BLI_remlink(handlers, handler);
  wm_event_free_handler(handler);
}
#endif

void WM_event_add_mousemove(wmWindow *win)
{
  win->addmousemove = 1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ghost Event Conversion
 * \{ */

/**
 * \return The WM enum for key or #EVENT_NONE (which should be ignored).
 */
static int convert_key(GHOST_TKey key)
{
  if (key >= GHOST_kKeyA && key <= GHOST_kKeyZ) {
    return (EVT_AKEY + ((int)key - GHOST_kKeyA));
  }
  if (key >= GHOST_kKey0 && key <= GHOST_kKey9) {
    return (EVT_ZEROKEY + ((int)key - GHOST_kKey0));
  }
  if (key >= GHOST_kKeyNumpad0 && key <= GHOST_kKeyNumpad9) {
    return (EVT_PAD0 + ((int)key - GHOST_kKeyNumpad0));
  }
  if (key >= GHOST_kKeyF1 && key <= GHOST_kKeyF24) {
    return (EVT_F1KEY + ((int)key - GHOST_kKeyF1));
  }

  switch (key) {
    case GHOST_kKeyBackSpace:
      return EVT_BACKSPACEKEY;
    case GHOST_kKeyTab:
      return EVT_TABKEY;
    case GHOST_kKeyLinefeed:
      return EVT_LINEFEEDKEY;
    case GHOST_kKeyClear:
      return EVENT_NONE;
    case GHOST_kKeyEnter:
      return EVT_RETKEY;

    case GHOST_kKeyEsc:
      return EVT_ESCKEY;
    case GHOST_kKeySpace:
      return EVT_SPACEKEY;
    case GHOST_kKeyQuote:
      return EVT_QUOTEKEY;
    case GHOST_kKeyComma:
      return EVT_COMMAKEY;
    case GHOST_kKeyMinus:
      return EVT_MINUSKEY;
    case GHOST_kKeyPlus:
      return EVT_PLUSKEY;
    case GHOST_kKeyPeriod:
      return EVT_PERIODKEY;
    case GHOST_kKeySlash:
      return EVT_SLASHKEY;

    case GHOST_kKeySemicolon:
      return EVT_SEMICOLONKEY;
    case GHOST_kKeyEqual:
      return EVT_EQUALKEY;

    case GHOST_kKeyLeftBracket:
      return EVT_LEFTBRACKETKEY;
    case GHOST_kKeyRightBracket:
      return EVT_RIGHTBRACKETKEY;
    case GHOST_kKeyBackslash:
      return EVT_BACKSLASHKEY;
    case GHOST_kKeyAccentGrave:
      return EVT_ACCENTGRAVEKEY;

    case GHOST_kKeyLeftShift:
      return EVT_LEFTSHIFTKEY;
    case GHOST_kKeyRightShift:
      return EVT_RIGHTSHIFTKEY;
    case GHOST_kKeyLeftControl:
      return EVT_LEFTCTRLKEY;
    case GHOST_kKeyRightControl:
      return EVT_RIGHTCTRLKEY;
    case GHOST_kKeyOS:
      return EVT_OSKEY;
    case GHOST_kKeyLeftAlt:
      return EVT_LEFTALTKEY;
    case GHOST_kKeyRightAlt:
      return EVT_RIGHTALTKEY;
    case GHOST_kKeyApp:
      return EVT_APPKEY;

    case GHOST_kKeyCapsLock:
      return EVT_CAPSLOCKKEY;
    case GHOST_kKeyNumLock:
      return EVENT_NONE;
    case GHOST_kKeyScrollLock:
      return EVENT_NONE;

    case GHOST_kKeyLeftArrow:
      return EVT_LEFTARROWKEY;
    case GHOST_kKeyRightArrow:
      return EVT_RIGHTARROWKEY;
    case GHOST_kKeyUpArrow:
      return EVT_UPARROWKEY;
    case GHOST_kKeyDownArrow:
      return EVT_DOWNARROWKEY;

    case GHOST_kKeyPrintScreen:
      return EVENT_NONE;
    case GHOST_kKeyPause:
      return EVT_PAUSEKEY;

    case GHOST_kKeyInsert:
      return EVT_INSERTKEY;
    case GHOST_kKeyDelete:
      return EVT_DELKEY;
    case GHOST_kKeyHome:
      return EVT_HOMEKEY;
    case GHOST_kKeyEnd:
      return EVT_ENDKEY;
    case GHOST_kKeyUpPage:
      return EVT_PAGEUPKEY;
    case GHOST_kKeyDownPage:
      return EVT_PAGEDOWNKEY;

    case GHOST_kKeyNumpadPeriod:
      return EVT_PADPERIOD;
    case GHOST_kKeyNumpadEnter:
      return EVT_PADENTER;
    case GHOST_kKeyNumpadPlus:
      return EVT_PADPLUSKEY;
    case GHOST_kKeyNumpadMinus:
      return EVT_PADMINUS;
    case GHOST_kKeyNumpadAsterisk:
      return EVT_PADASTERKEY;
    case GHOST_kKeyNumpadSlash:
      return EVT_PADSLASHKEY;

    case GHOST_kKeyGrLess:
      return EVT_GRLESSKEY;

    case GHOST_kKeyMediaPlay:
      return EVT_MEDIAPLAY;
    case GHOST_kKeyMediaStop:
      return EVT_MEDIASTOP;
    case GHOST_kKeyMediaFirst:
      return EVT_MEDIAFIRST;
    case GHOST_kKeyMediaLast:
      return EVT_MEDIALAST;

    case GHOST_kKeyUnknown:
      return EVT_UNKNOWNKEY;

#if defined(__GNUC__) || defined(__clang__)
      /* Ensure all members of this enum are handled, otherwise generate a compiler warning.
       * Note that these members have been handled, these ranges are to satisfy the compiler. */
    case GHOST_kKeyF1 ... GHOST_kKeyF24:
    case GHOST_kKeyA ... GHOST_kKeyZ:
    case GHOST_kKeyNumpad0 ... GHOST_kKeyNumpad9:
    case GHOST_kKey0 ... GHOST_kKey9: {
      BLI_assert_unreachable();
      break;
    }
#else
    default: {
      break;
    }
#endif
  }

  CLOG_WARN(WM_LOG_EVENTS, "unknown event type %d from ghost", (int)key);
  return EVENT_NONE;
}

static void wm_eventemulation(wmEvent *event, bool test_only)
{
  /* Store last middle-mouse event value to make emulation work
   * when modifier keys are released first.
   * This really should be in a data structure somewhere. */
  static int emulating_event = EVENT_NONE;

  /* Middle-mouse emulation. */
  if (U.flag & USER_TWOBUTTONMOUSE) {

    if (event->type == LEFTMOUSE) {
      const uint8_t mod_test = (
#if !defined(WIN32)
          (U.mouse_emulate_3_button_modifier == USER_EMU_MMB_MOD_OSKEY) ? KM_OSKEY : KM_ALT
#else
          /* Disable for WIN32 for now because it accesses the start menu. */
          KM_ALT
#endif
      );

      if (event->val == KM_PRESS) {
        if (event->modifier & mod_test) {
          event->modifier &= ~mod_test;
          event->type = MIDDLEMOUSE;

          if (!test_only) {
            emulating_event = MIDDLEMOUSE;
          }
        }
      }
      else if (event->val == KM_RELEASE) {
        /* Only send middle-mouse release if emulated. */
        if (emulating_event == MIDDLEMOUSE) {
          event->type = MIDDLEMOUSE;
          event->modifier &= ~mod_test;
        }

        if (!test_only) {
          emulating_event = EVENT_NONE;
        }
      }
    }
  }

  /* Numeric-pad emulation. */
  if (U.flag & USER_NONUMPAD) {
    switch (event->type) {
      case EVT_ZEROKEY:
        event->type = EVT_PAD0;
        break;
      case EVT_ONEKEY:
        event->type = EVT_PAD1;
        break;
      case EVT_TWOKEY:
        event->type = EVT_PAD2;
        break;
      case EVT_THREEKEY:
        event->type = EVT_PAD3;
        break;
      case EVT_FOURKEY:
        event->type = EVT_PAD4;
        break;
      case EVT_FIVEKEY:
        event->type = EVT_PAD5;
        break;
      case EVT_SIXKEY:
        event->type = EVT_PAD6;
        break;
      case EVT_SEVENKEY:
        event->type = EVT_PAD7;
        break;
      case EVT_EIGHTKEY:
        event->type = EVT_PAD8;
        break;
      case EVT_NINEKEY:
        event->type = EVT_PAD9;
        break;
      case EVT_MINUSKEY:
        event->type = EVT_PADMINUS;
        break;
      case EVT_EQUALKEY:
        event->type = EVT_PADPLUSKEY;
        break;
      case EVT_BACKSLASHKEY:
        event->type = EVT_PADSLASHKEY;
        break;
    }
  }
}

constexpr wmTabletData wm_event_tablet_data_default()
{
  wmTabletData tablet_data{};
  tablet_data.active = EVT_TABLET_NONE;
  tablet_data.pressure = 1.0f;
  tablet_data.x_tilt = 0.0f;
  tablet_data.y_tilt = 0.0f;
  tablet_data.is_motion_absolute = false;
  return tablet_data;
}

void WM_event_tablet_data_default_set(wmTabletData *tablet_data)
{
  *tablet_data = wm_event_tablet_data_default();
}

void wm_tablet_data_from_ghost(const GHOST_TabletData *tablet_data, wmTabletData *wmtab)
{
  if ((tablet_data != nullptr) && tablet_data->Active != GHOST_kTabletModeNone) {
    wmtab->active = (int)tablet_data->Active;
    wmtab->pressure = wm_pressure_curve(tablet_data->Pressure);
    wmtab->x_tilt = tablet_data->Xtilt;
    wmtab->y_tilt = tablet_data->Ytilt;
    /* We could have a preference to support relative tablet motion (we can't detect that). */
    wmtab->is_motion_absolute = true;
    // printf("%s: using tablet %.5f\n", __func__, wmtab->pressure);
  }
  else {
    *wmtab = wm_event_tablet_data_default();
    // printf("%s: not using tablet\n", __func__);
  }
}

#ifdef WITH_INPUT_NDOF
/* Adds custom-data to event. */
static void attach_ndof_data(wmEvent *event, const GHOST_TEventNDOFMotionData *ghost)
{
  wmNDOFMotionData *data = MEM_cnew<wmNDOFMotionData>("Custom-data NDOF");

  const float ts = U.ndof_sensitivity;
  const float rs = U.ndof_orbit_sensitivity;

  mul_v3_v3fl(data->tvec, &ghost->tx, ts);
  mul_v3_v3fl(data->rvec, &ghost->rx, rs);

  if (U.ndof_flag & NDOF_PAN_YZ_SWAP_AXIS) {
    float t;
    t = data->tvec[1];
    data->tvec[1] = -data->tvec[2];
    data->tvec[2] = t;
  }

  data->dt = ghost->dt;

  data->progress = (wmProgress)ghost->progress;

  event->custom = EVT_DATA_NDOF_MOTION;
  event->customdata = data;
  event->customdata_free = true;
}
#endif /* WITH_INPUT_NDOF */

/* Imperfect but probably usable... draw/enable drags to other windows. */
static wmWindow *wm_event_cursor_other_windows(wmWindowManager *wm, wmWindow *win, wmEvent *event)
{
  /* If GHOST doesn't support window positioning, don't use this feature at all. */
  const static int8_t supports_window_position = GHOST_SupportsWindowPosition();
  if (!supports_window_position) {
    return nullptr;
  }

  if (wm->windows.first == wm->windows.last) {
    return nullptr;
  }

  /* In order to use window size and mouse position (pixels), we have to use a WM function. */

  /* Check if outside, include top window bar. */
  int event_xy[2] = {UNPACK2(event->xy)};
  if (event_xy[0] < 0 || event_xy[1] < 0 || event_xy[0] > WM_window_pixels_x(win) ||
      event_xy[1] > WM_window_pixels_y(win) + 30) {
    /* Let's skip windows having modal handlers now. */
    /* Potential XXX ugly... I wouldn't have added a `modalhandlers` list
     * (introduced in rev 23331, ton). */
    LISTBASE_FOREACH (wmEventHandler *, handler, &win->modalhandlers) {
      if (ELEM(handler->type, WM_HANDLER_TYPE_UI, WM_HANDLER_TYPE_OP)) {
        return nullptr;
      }
    }

    wmWindow *win_other = WM_window_find_under_cursor(win, event_xy, event_xy);
    if (win_other && win_other != win) {
      copy_v2_v2_int(event->xy, event_xy);
      return win_other;
    }
  }
  return nullptr;
}

static bool wm_event_is_double_click(const wmEvent *event)
{
  if ((event->type == event->prev_type) && (event->prev_val == KM_RELEASE) &&
      (event->val == KM_PRESS)) {
    if (ISMOUSE(event->type) && WM_event_drag_test(event, event->prev_press_xy)) {
      /* Pass. */
    }
    else {
      if ((PIL_check_seconds_timer() - event->prev_press_time) * 1000 < U.dbl_click_time) {
        return true;
      }
    }
  }

  return false;
}

/**
 * Copy the current state to the previous event state.
 */
static void wm_event_prev_values_set(wmEvent *event, wmEvent *event_state)
{
  event->prev_val = event_state->prev_val = event_state->val;
  event->prev_type = event_state->prev_type = event_state->type;
}

static void wm_event_prev_click_set(wmEvent *event_state)
{
  event_state->prev_press_time = PIL_check_seconds_timer();
  event_state->prev_press_type = event_state->type;
  event_state->prev_press_modifier = event_state->modifier;
  event_state->prev_press_keymodifier = event_state->keymodifier;
  event_state->prev_press_xy[0] = event_state->xy[0];
  event_state->prev_press_xy[1] = event_state->xy[1];
}

static wmEvent *wm_event_add_mousemove(wmWindow *win, const wmEvent *event)
{
  wmEvent *event_last = static_cast<wmEvent *>(win->event_queue.last);

  /* Some painting operators want accurate mouse events, they can
   * handle in between mouse move moves, others can happily ignore
   * them for better performance. */
  if (event_last && event_last->type == MOUSEMOVE) {
    event_last->type = INBETWEEN_MOUSEMOVE;
    event_last->flag = (eWM_EventFlag)0;
  }

  wmEvent *event_new = wm_event_add(win, event);
  if (event_last == nullptr) {
    event_last = win->eventstate;
  }

  copy_v2_v2_int(event_new->prev_xy, event_last->xy);
  return event_new;
}

static wmEvent *wm_event_add_mousemove_to_head(wmWindow *win)
{
  /* Use the last handled event instead of `win->eventstate` because the state of the modifiers
   * and previous values should be set based on the last state, not using values from the future.
   * So this gives an accurate simulation of mouse motion before the next event is handled. */
  const wmEvent *event_last = win->event_last_handled;

  wmEvent tevent;
  if (event_last) {
    tevent = *event_last;

    tevent.flag = (eWM_EventFlag)0;
    tevent.ascii = '\0';
    tevent.utf8_buf[0] = '\0';

    wm_event_custom_clear(&tevent);
  }
  else {
    memset(&tevent, 0x0, sizeof(tevent));
  }

  tevent.type = MOUSEMOVE;
  tevent.val = KM_NOTHING;
  copy_v2_v2_int(tevent.prev_xy, tevent.xy);

  wmEvent *event_new = wm_event_add(win, &tevent);
  BLI_remlink(&win->event_queue, event_new);
  BLI_addhead(&win->event_queue, event_new);

  copy_v2_v2_int(event_new->prev_xy, event_last->xy);
  return event_new;
}

static wmEvent *wm_event_add_trackpad(wmWindow *win, const wmEvent *event, int deltax, int deltay)
{
  /* Ignore in between track-pad events for performance, we only need high accuracy
   * for painting with mouse moves, for navigation using the accumulated value is ok. */
  wmEvent *event_last = static_cast<wmEvent *>(win->event_queue.last);
  if (event_last && event_last->type == event->type) {
    deltax += event_last->xy[0] - event_last->prev_xy[0];
    deltay += event_last->xy[1] - event_last->prev_xy[1];

    wm_event_free_last(win);
  }

  /* Set prev_xy, the delta is computed from this in operators. */
  wmEvent *event_new = wm_event_add(win, event);
  event_new->prev_xy[0] = event_new->xy[0] - deltax;
  event_new->prev_xy[1] = event_new->xy[1] - deltay;

  return event_new;
}

/**
 * Update the event-state for any kind of event that supports #KM_PRESS / #KM_RELEASE.
 *
 * \param check_double_click: Optionally skip checking for double-click events.
 * Needed for event simulation where the time of click events is not so predictable.
 */
static void wm_event_state_update_and_click_set_ex(wmEvent *event,
                                                   wmEvent *event_state,
                                                   const bool is_keyboard,
                                                   const bool check_double_click)
{
  BLI_assert(ISKEYBOARD_OR_BUTTON(event->type));
  BLI_assert(ELEM(event->val, KM_PRESS, KM_RELEASE));

  /* Only copy these flags into the `event_state`. */
  const eWM_EventFlag event_state_flag_mask = WM_EVENT_IS_REPEAT;

  wm_event_prev_values_set(event, event_state);

  /* Copy to event state. */
  event_state->val = event->val;
  event_state->type = event->type;
  /* It's important only to write into the `event_state` modifier for keyboard
   * events because emulate MMB clears one of the modifiers in `event->modifier`,
   * making the second press not behave as if the modifier is pressed, see T96279. */
  if (is_keyboard) {
    event_state->modifier = event->modifier;
  }
  event_state->flag = (event->flag & event_state_flag_mask);
  /* NOTE: It's important that `keymodifier` is handled in the keyboard event handling logic
   * since the `event_state` and the `event` are not kept in sync. */

  /* Double click test. */
  if (check_double_click && wm_event_is_double_click(event)) {
    CLOG_INFO(WM_LOG_HANDLERS, 1, "DBL_CLICK: detected");
    event->val = KM_DBL_CLICK;
  }
  else if (event->val == KM_PRESS) {
    if ((event->flag & WM_EVENT_IS_REPEAT) == 0) {
      wm_event_prev_click_set(event_state);
    }
  }
}

static void wm_event_state_update_and_click_set(wmEvent *event,
                                                wmEvent *event_state,
                                                const GHOST_TEventType type)
{
  const bool is_keyboard = ELEM(type, GHOST_kEventKeyDown, GHOST_kEventKeyUp);
  const bool check_double_click = true;
  wm_event_state_update_and_click_set_ex(event, event_state, is_keyboard, check_double_click);
}

void wm_event_add_ghostevent(wmWindowManager *wm, wmWindow *win, int type, void *customdata)
{
  if (UNLIKELY(G.f & G_FLAG_EVENT_SIMULATE)) {
    return;
  }

  /**
   * Having both, \a event and \a event_state, can be highly confusing to work with,
   * but is necessary for our current event system, so let's clear things up a bit:
   *
   * - Data added to event only will be handled immediately,
   *   but will not be copied to the next event.
   * - Data added to \a event_state only stays,
   *   but is handled with the next event -> execution delay.
   * - Data added to event and \a event_state stays and is handled immediately.
   */
  wmEvent event, *event_state = win->eventstate;

  /* Initialize and copy state (only mouse x y and modifiers). */
  event = *event_state;
  event.flag = (eWM_EventFlag)0;

  /**
   * Always support accessing the last key press/release. This is set from `win->eventstate`,
   * so it will always be a valid event type to store in the previous state.
   *
   * Note that these values are intentionally _not_ set in the `win->eventstate`,
   * as copying these values only makes sense when `win->eventstate->{val/type}` would be
   * written to (which only happens for some kinds of events).
   * If this was done it could leave `win->eventstate` previous and current value
   * set to the same key press/release state which doesn't make sense.
   */
  event.prev_type = event.type;
  event.prev_val = event.val;

  /* Ensure the event state is correct, any deviation from this may cause bugs.
   *
   * NOTE: #EVENT_NONE is set when unknown keys are pressed,
   * while not common, avoid a false alarm. */
#ifndef NDEBUG
  if ((event_state->type || event_state->val) && /* Ignore cleared event state. */
      !(ISKEYBOARD_OR_BUTTON(event_state->type) || (event_state->type == EVENT_NONE))) {
    CLOG_WARN(WM_LOG_HANDLERS,
              "Non-keyboard/mouse button found in 'win->eventstate->type = %d'",
              event_state->type);
  }
  if ((event_state->prev_type || event_state->prev_val) && /* Ignore cleared event state. */
      !(ISKEYBOARD_OR_BUTTON(event_state->prev_type) || (event_state->type == EVENT_NONE))) {
    CLOG_WARN(WM_LOG_HANDLERS,
              "Non-keyboard/mouse button found in 'win->eventstate->prev_type = %d'",
              event_state->prev_type);
  }
#endif

  switch (type) {
    /* Mouse move, also to inactive window (X11 does this). */
    case GHOST_kEventCursorMove: {
      GHOST_TEventCursorData *cd = static_cast<GHOST_TEventCursorData *>(customdata);

      copy_v2_v2_int(event.xy, &cd->x);
      wm_stereo3d_mouse_offset_apply(win, event.xy);
      wm_tablet_data_from_ghost(&cd->tablet, &event.tablet);

      event.type = MOUSEMOVE;
      event.val = KM_NOTHING;
      {
        wmEvent *event_new = wm_event_add_mousemove(win, &event);
        copy_v2_v2_int(event_state->xy, event_new->xy);
        event_state->tablet.is_motion_absolute = event_new->tablet.is_motion_absolute;
      }

      /* Also add to other window if event is there, this makes overdraws disappear nicely. */
      /* It remaps mouse-coord to other window in event. */
      wmWindow *win_other = wm_event_cursor_other_windows(wm, win, &event);
      if (win_other) {
        wmEvent event_other = *win_other->eventstate;

        /* See comment for this operation on `event` for details. */
        event_other.prev_type = event_other.type;
        event_other.prev_val = event_other.val;

        copy_v2_v2_int(event_other.xy, event.xy);
        event_other.type = MOUSEMOVE;
        event_other.val = KM_NOTHING;
        {
          wmEvent *event_new = wm_event_add_mousemove(win_other, &event_other);
          copy_v2_v2_int(win_other->eventstate->xy, event_new->xy);
          win_other->eventstate->tablet.is_motion_absolute = event_new->tablet.is_motion_absolute;
        }
      }

      break;
    }
    case GHOST_kEventTrackpad: {
      GHOST_TEventTrackpadData *pd = static_cast<GHOST_TEventTrackpadData *>(customdata);
      switch (pd->subtype) {
        case GHOST_kTrackpadEventMagnify:
          event.type = MOUSEZOOM;
          pd->deltaX = -pd->deltaX;
          pd->deltaY = -pd->deltaY;
          break;
        case GHOST_kTrackpadEventSmartMagnify:
          event.type = MOUSESMARTZOOM;
          break;
        case GHOST_kTrackpadEventRotate:
          event.type = MOUSEROTATE;
          break;
        case GHOST_kTrackpadEventScroll:
        default:
          event.type = MOUSEPAN;
          break;
      }

      event.xy[0] = event_state->xy[0] = pd->x;
      event.xy[1] = event_state->xy[1] = pd->y;
      event.val = KM_NOTHING;

      /* The direction is inverted from the device due to system preferences. */
      if (pd->isDirectionInverted) {
        event.flag |= WM_EVENT_SCROLL_INVERT;
      }

      wm_event_add_trackpad(win, &event, pd->deltaX, -pd->deltaY);
      break;
    }
    /* Mouse button. */
    case GHOST_kEventButtonDown:
    case GHOST_kEventButtonUp: {
      GHOST_TEventButtonData *bd = static_cast<GHOST_TEventButtonData *>(customdata);

      /* Get value and type from Ghost. */
      event.val = (type == GHOST_kEventButtonDown) ? KM_PRESS : KM_RELEASE;

      if (bd->button == GHOST_kButtonMaskLeft) {
        event.type = LEFTMOUSE;
      }
      else if (bd->button == GHOST_kButtonMaskRight) {
        event.type = RIGHTMOUSE;
      }
      else if (bd->button == GHOST_kButtonMaskButton4) {
        event.type = BUTTON4MOUSE;
      }
      else if (bd->button == GHOST_kButtonMaskButton5) {
        event.type = BUTTON5MOUSE;
      }
      else if (bd->button == GHOST_kButtonMaskButton6) {
        event.type = BUTTON6MOUSE;
      }
      else if (bd->button == GHOST_kButtonMaskButton7) {
        event.type = BUTTON7MOUSE;
      }
      else {
        event.type = MIDDLEMOUSE;
      }

      /* Get tablet data. */
      wm_tablet_data_from_ghost(&bd->tablet, &event.tablet);

      wm_eventemulation(&event, false);
      wm_event_state_update_and_click_set(&event, event_state, (GHOST_TEventType)type);

      /* Add to other window if event is there (not to both!). */
      wmWindow *win_other = wm_event_cursor_other_windows(wm, win, &event);
      if (win_other) {
        wmEvent event_other = *win_other->eventstate;

        /* See comment for this operation on `event` for details. */
        event_other.prev_type = event_other.type;
        event_other.prev_val = event_other.val;

        copy_v2_v2_int(event_other.xy, event.xy);

        event_other.type = event.type;
        event_other.val = event.val;
        event_other.tablet = event.tablet;

        wm_event_add(win_other, &event_other);
      }
      else {
        wm_event_add(win, &event);
      }

      break;
    }
    /* Keyboard. */
    case GHOST_kEventKeyDown:
    case GHOST_kEventKeyUp: {
      GHOST_TEventKeyData *kd = static_cast<GHOST_TEventKeyData *>(customdata);
      event.type = convert_key(kd->key);
      if (UNLIKELY(event.type == EVENT_NONE)) {
        break;
      }

      event.ascii = kd->ascii;
      /* Might be not nullptr terminated. */
      memcpy(event.utf8_buf, kd->utf8_buf, sizeof(event.utf8_buf));
      if (kd->is_repeat) {
        event.flag |= WM_EVENT_IS_REPEAT;
      }
      event.val = (type == GHOST_kEventKeyDown) ? KM_PRESS : KM_RELEASE;

      wm_eventemulation(&event, false);

      /* Exclude arrow keys, escape, etc from text input. */
      if (type == GHOST_kEventKeyUp) {
        event.ascii = '\0';

        /* Ghost should do this already for key up. */
        if (event.utf8_buf[0]) {
          CLOG_ERROR(WM_LOG_EVENTS,
                     "ghost on your platform is misbehaving, utf8 events on key up!");
        }
        event.utf8_buf[0] = '\0';
      }
      else {
        if (event.ascii < 32 && event.ascii > 0) {
          event.ascii = '\0';
        }
        if (event.utf8_buf[0] < 32 && event.utf8_buf[0] > 0) {
          event.utf8_buf[0] = '\0';
        }
      }

      if (event.utf8_buf[0]) {
        if (BLI_str_utf8_size(event.utf8_buf) == -1) {
          CLOG_ERROR(WM_LOG_EVENTS,
                     "ghost detected an invalid unicode character '%d'",
                     (int)(unsigned char)event.utf8_buf[0]);
          event.utf8_buf[0] = '\0';
        }
      }

      switch (event.type) {
        case EVT_LEFTSHIFTKEY:
        case EVT_RIGHTSHIFTKEY: {
          SET_FLAG_FROM_TEST(event.modifier, (event.val == KM_PRESS), KM_SHIFT);
          break;
        }
        case EVT_LEFTCTRLKEY:
        case EVT_RIGHTCTRLKEY: {
          SET_FLAG_FROM_TEST(event.modifier, (event.val == KM_PRESS), KM_CTRL);
          break;
        }
        case EVT_LEFTALTKEY:
        case EVT_RIGHTALTKEY: {
          SET_FLAG_FROM_TEST(event.modifier, (event.val == KM_PRESS), KM_ALT);
          break;
        }
        case EVT_OSKEY: {
          SET_FLAG_FROM_TEST(event.modifier, (event.val == KM_PRESS), KM_OSKEY);
          break;
        }
        default: {
          if (event.val == KM_PRESS) {
            if (event.keymodifier == 0) {
              /* Only set in `eventstate`, for next event. */
              event_state->keymodifier = event.type;
            }
          }
          else {
            BLI_assert(event.val == KM_RELEASE);
            if (event.keymodifier == event.type) {
              event.keymodifier = event_state->keymodifier = 0;
            }
          }

          /* This case happens on holding a key pressed,
           * it should not generate press events with the same key as modifier. */
          if (event.keymodifier == event.type) {
            event.keymodifier = 0;
          }
          else if (event.keymodifier == EVT_UNKNOWNKEY) {
            /* This case happens with an external number-pad, and also when using 'dead keys'
             * (to compose complex latin characters e.g.), it's not really clear why.
             * Since it's impossible to map a key modifier to an unknown key,
             * it shouldn't harm to clear it. */
            event_state->keymodifier = event.keymodifier = 0;
          }
          break;
        }
      }

      /* It's important `event.modifier` has been initialized first. */
      wm_event_state_update_and_click_set(&event, event_state, (GHOST_TEventType)type);

      /* If test_break set, it catches this. Do not set with modifier presses.
       * Exclude modifiers because MS-Windows uses these to bring up the task manager.
       *
       * NOTE: in general handling events here isn't great design as
       * event handling should be managed by the event handling loop.
       * Make an exception for `G.is_break` as it ensures we can always cancel operations
       * such as rendering or baking no matter which operation is currently handling events. */
      if ((event.type == EVT_ESCKEY) && (event.val == KM_PRESS) && (event.modifier == 0)) {
        G.is_break = true;
      }

      wm_event_add(win, &event);

      break;
    }

    case GHOST_kEventWheel: {
      GHOST_TEventWheelData *wheelData = static_cast<GHOST_TEventWheelData *>(customdata);

      if (wheelData->z > 0) {
        event.type = WHEELUPMOUSE;
      }
      else {
        event.type = WHEELDOWNMOUSE;
      }

      event.val = KM_PRESS;
      wm_event_add(win, &event);

      break;
    }
    case GHOST_kEventTimer: {
      event.type = TIMER;
      event.custom = EVT_DATA_TIMER;
      event.customdata = customdata;
      event.val = KM_NOTHING;
      event.keymodifier = 0;
      wm_event_add(win, &event);

      break;
    }

#ifdef WITH_INPUT_NDOF
    case GHOST_kEventNDOFMotion: {
      event.type = NDOF_MOTION;
      event.val = KM_NOTHING;
      attach_ndof_data(&event, static_cast<const GHOST_TEventNDOFMotionData *>(customdata));
      wm_event_add(win, &event);

      CLOG_INFO(WM_LOG_HANDLERS, 1, "sending NDOF_MOTION, prev = %d %d", event.xy[0], event.xy[1]);
      break;
    }

    case GHOST_kEventNDOFButton: {
      GHOST_TEventNDOFButtonData *e = static_cast<GHOST_TEventNDOFButtonData *>(customdata);

      event.type = NDOF_BUTTON_NONE + e->button;

      switch (e->action) {
        case GHOST_kPress:
          event.val = KM_PRESS;
          break;
        case GHOST_kRelease:
          event.val = KM_RELEASE;
          break;
        default:
          BLI_assert_unreachable();
      }

      event.custom = 0;
      event.customdata = nullptr;

      wm_event_state_update_and_click_set(&event, event_state, (GHOST_TEventType)type);

      wm_event_add(win, &event);

      break;
    }
#endif /* WITH_INPUT_NDOF */

    case GHOST_kEventUnknown:
    case GHOST_kNumEventTypes:
      break;

    case GHOST_kEventWindowDeactivate: {
      event.type = WINDEACTIVATE;
      wm_event_add(win, &event);

      break;
    }

#ifdef WITH_INPUT_IME
    case GHOST_kEventImeCompositionStart: {
      event.val = KM_PRESS;
      win->ime_data = static_cast<wmIMEData *>(customdata);
      win->ime_data->is_ime_composing = true;
      event.type = WM_IME_COMPOSITE_START;
      wm_event_add(win, &event);
      break;
    }
    case GHOST_kEventImeComposition: {
      event.val = KM_PRESS;
      event.type = WM_IME_COMPOSITE_EVENT;
      wm_event_add(win, &event);
      break;
    }
    case GHOST_kEventImeCompositionEnd: {
      event.val = KM_PRESS;
      if (win->ime_data) {
        win->ime_data->is_ime_composing = false;
      }
      event.type = WM_IME_COMPOSITE_END;
      wm_event_add(win, &event);
      break;
    }
#endif /* WITH_INPUT_IME */
  }

#if 0
  WM_event_print(&event);
#endif
}

#ifdef WITH_XR_OPENXR
void wm_event_add_xrevent(wmWindow *win, wmXrActionData *actiondata, short val)
{
  BLI_assert(ELEM(val, KM_PRESS, KM_RELEASE));

  wmEvent event{};
  event.type = EVT_XR_ACTION;
  event.val = val;
  event.flag = (eWM_EventFlag)0;
  event.custom = EVT_DATA_XR;
  event.customdata = actiondata;
  event.customdata_free = true;

  wm_event_add(win, &event);
}
#endif /* WITH_XR_OPENXR */

/** \} */

/* -------------------------------------------------------------------- */
/** \name WM Interface Locking
 * \{ */

/**
 * Check whether operator is allowed to run in case interface is locked,
 * If interface is unlocked, will always return truth.
 */
static bool wm_operator_check_locked_interface(bContext *C, wmOperatorType *ot)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  if (wm->is_interface_locked) {
    if ((ot->flag & OPTYPE_LOCK_BYPASS) == 0) {
      return false;
    }
  }

  return true;
}

void WM_set_locked_interface(wmWindowManager *wm, bool lock)
{
  /* This will prevent events from being handled while interface is locked
   *
   * Use a "local" flag for now, because currently no other areas could
   * benefit of locked interface anyway (aka using G.is_interface_locked
   * wouldn't be useful anywhere outside of window manager, so let's not
   * pollute global context with such an information for now).
   */
  wm->is_interface_locked = lock ? 1 : 0;

  /* This will prevent drawing regions which uses non-thread-safe data.
   * Currently it'll be just a 3D viewport.
   *
   * TODO(sergey): Make it different locked states, so different jobs
   *               could lock different areas of blender and allow
   *               interaction with others?
   */
  BKE_spacedata_draw_locks(lock);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event / Keymap Matching API
 * \{ */

void WM_event_get_keymaps_from_handler(wmWindowManager *wm,
                                       wmWindow *win,
                                       wmEventHandler_Keymap *handler,
                                       wmEventHandler_KeymapResult *km_result)
{
  if (handler->dynamic.keymap_fn != nullptr) {
    handler->dynamic.keymap_fn(wm, win, handler, km_result);
    BLI_assert(handler->keymap == nullptr);
  }
  else {
    memset(km_result, 0x0, sizeof(*km_result));
    wmKeyMap *keymap = WM_keymap_active(wm, handler->keymap);
    BLI_assert(keymap != nullptr);
    if (keymap != nullptr) {
      km_result->keymaps[km_result->keymaps_len++] = keymap;
    }
  }
}

wmKeyMapItem *WM_event_match_keymap_item(bContext *C, wmKeyMap *keymap, const wmEvent *event)
{
  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
    if (wm_eventmatch(event, kmi)) {
      wmOperatorType *ot = WM_operatortype_find(kmi->idname, false);
      if (WM_operator_poll_context(C, ot, WM_OP_INVOKE_DEFAULT)) {
        return kmi;
      }
    }
  }
  return nullptr;
}

wmKeyMapItem *WM_event_match_keymap_item_from_handlers(
    bContext *C, wmWindowManager *wm, wmWindow *win, ListBase *handlers, const wmEvent *event)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    /* During this loop, UI handlers for nested menus can tag multiple handlers free. */
    if (handler_base->flag & WM_HANDLER_DO_FREE) {
      /* Pass. */
    }
    else if (handler_base->poll == nullptr || handler_base->poll(CTX_wm_region(C), event)) {
      if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
        wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
        wmEventHandler_KeymapResult km_result;
        WM_event_get_keymaps_from_handler(wm, win, handler, &km_result);
        for (int km_index = 0; km_index < km_result.keymaps_len; km_index++) {
          wmKeyMap *keymap = km_result.keymaps[km_index];
          if (WM_keymap_poll(C, keymap)) {
            wmKeyMapItem *kmi = WM_event_match_keymap_item(C, keymap, event);
            if (kmi != nullptr) {
              return kmi;
            }
          }
        }
      }
    }
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Keymap Status
 *
 * Show cursor keys in the status bar.
 * This is done by detecting changes to the state - full key-map lookups are expensive
 * so only perform this on changing tools, space types, pressing different modifier keys... etc.
 * \{ */

/** State storage to detect changes between calls to refresh the information. */
struct CursorKeymapInfo_State {
  uint8_t modifier;
  short space_type;
  short region_type;
  /** Never use, just compare memory for changes. */
  bToolRef tref;
};

struct CursorKeymapInfo {
  /**
   * 0: Mouse button index.
   * 1: Event type (click/press, drag).
   * 2: Text.
   */
  char text[3][2][128];
  wmEvent state_event;
  CursorKeymapInfo_State state;
};

static void wm_event_cursor_store(CursorKeymapInfo_State *state,
                                  const wmEvent *event,
                                  short space_type,
                                  short region_type,
                                  const bToolRef *tref)
{
  state->modifier = event->modifier;
  state->space_type = space_type;
  state->region_type = region_type;
  state->tref = tref ? *tref : bToolRef{};
}

const char *WM_window_cursor_keymap_status_get(const wmWindow *win,
                                               int button_index,
                                               int type_index)
{
  if (win->cursor_keymap_status != nullptr) {
    CursorKeymapInfo *cd = static_cast<CursorKeymapInfo *>(win->cursor_keymap_status);
    const char *msg = cd->text[button_index][type_index];
    if (*msg) {
      return msg;
    }
  }
  return nullptr;
}

ScrArea *WM_window_status_area_find(wmWindow *win, bScreen *screen)
{
  if (screen->state == SCREENFULL) {
    return nullptr;
  }
  ScrArea *area_statusbar = nullptr;
  LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
    if (area->spacetype == SPACE_STATUSBAR) {
      area_statusbar = area;
      break;
    }
  }
  return area_statusbar;
}

void WM_window_status_area_tag_redraw(wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  ScrArea *area = WM_window_status_area_find(win, screen);
  if (area != nullptr) {
    ED_area_tag_redraw(area);
  }
}

void WM_window_cursor_keymap_status_refresh(bContext *C, wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  ScrArea *area_statusbar = WM_window_status_area_find(win, screen);
  if (area_statusbar == nullptr) {
    MEM_SAFE_FREE(win->cursor_keymap_status);
    return;
  }

  CursorKeymapInfo *cd;
  if (UNLIKELY(win->cursor_keymap_status == nullptr)) {
    win->cursor_keymap_status = MEM_callocN(sizeof(CursorKeymapInfo), __func__);
  }
  cd = static_cast<CursorKeymapInfo *>(win->cursor_keymap_status);

  /* Detect unchanged state (early exit). */
  if (memcmp(&cd->state_event, win->eventstate, sizeof(wmEvent)) == 0) {
    return;
  }

  /* Now perform more comprehensive check,
   * still keep this fast since it happens on mouse-move. */
  CursorKeymapInfo cd_prev = *((CursorKeymapInfo *)win->cursor_keymap_status);
  cd->state_event = *win->eventstate;

  /* Find active region and associated area. */
  ARegion *region = screen->active_region;
  if (region == nullptr) {
    return;
  }

  ScrArea *area = nullptr;
  ED_screen_areas_iter (win, screen, area_iter) {
    if (BLI_findindex(&area_iter->regionbase, region) != -1) {
      area = area_iter;
      break;
    }
  }
  if (area == nullptr) {
    return;
  }

  /* Keep as-is. */
  if (ELEM(area->spacetype, SPACE_STATUSBAR, SPACE_TOPBAR)) {
    return;
  }
  if (ELEM(region->regiontype,
           RGN_TYPE_HEADER,
           RGN_TYPE_TOOL_HEADER,
           RGN_TYPE_FOOTER,
           RGN_TYPE_TEMPORARY,
           RGN_TYPE_HUD)) {
    return;
  }
  /* Fallback to window. */
  if (ELEM(region->regiontype, RGN_TYPE_TOOLS, RGN_TYPE_TOOL_PROPS)) {
    region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  }

  /* Detect changes to the state. */
  {
    bToolRef *tref = nullptr;
    if ((region->regiontype == RGN_TYPE_WINDOW) &&
        ((1 << area->spacetype) & WM_TOOLSYSTEM_SPACE_MASK)) {
      ViewLayer *view_layer = WM_window_get_active_view_layer(win);
      WorkSpace *workspace = WM_window_get_active_workspace(win);
      bToolKey tkey{};
      tkey.space_type = area->spacetype;
      tkey.mode = WM_toolsystem_mode_from_spacetype(view_layer, area, area->spacetype);
      tref = WM_toolsystem_ref_find(workspace, &tkey);
    }
    wm_event_cursor_store(&cd->state, win->eventstate, area->spacetype, region->regiontype, tref);
    if (memcmp(&cd->state, &cd_prev.state, sizeof(cd->state)) == 0) {
      return;
    }
  }

  /* Changed context found, detect changes to key-map and refresh the status bar. */
  const struct {
    int button_index;
    int type_index; /* 0: press or click, 1: drag. */
    int event_type;
    int event_value;
  } event_data[] = {
      {0, 0, LEFTMOUSE, KM_PRESS},
      {0, 0, LEFTMOUSE, KM_CLICK},
      {0, 0, LEFTMOUSE, KM_CLICK_DRAG},

      {1, 0, MIDDLEMOUSE, KM_PRESS},
      {1, 0, MIDDLEMOUSE, KM_CLICK},
      {1, 0, MIDDLEMOUSE, KM_CLICK_DRAG},

      {2, 0, RIGHTMOUSE, KM_PRESS},
      {2, 0, RIGHTMOUSE, KM_CLICK},
      {2, 0, RIGHTMOUSE, KM_CLICK_DRAG},
  };

  for (int button_index = 0; button_index < 3; button_index++) {
    cd->text[button_index][0][0] = '\0';
    cd->text[button_index][1][0] = '\0';
  }

  CTX_wm_window_set(C, win);
  CTX_wm_area_set(C, area);
  CTX_wm_region_set(C, region);

  ListBase *handlers[] = {
      &region->handlers,
      &area->handlers,
      &win->handlers,
  };

  wmWindowManager *wm = CTX_wm_manager(C);
  for (int data_index = 0; data_index < ARRAY_SIZE(event_data); data_index++) {
    const int button_index = event_data[data_index].button_index;
    const int type_index = event_data[data_index].type_index;
    if (cd->text[button_index][type_index][0] != 0) {
      continue;
    }
    wmEvent test_event = *win->eventstate;
    test_event.type = event_data[data_index].event_type;
    test_event.val = event_data[data_index].event_value;
    test_event.flag = (eWM_EventFlag)0;
    wm_eventemulation(&test_event, true);
    wmKeyMapItem *kmi = nullptr;
    for (int handler_index = 0; handler_index < ARRAY_SIZE(handlers); handler_index++) {
      kmi = WM_event_match_keymap_item_from_handlers(
          C, wm, win, handlers[handler_index], &test_event);
      if (kmi) {
        break;
      }
    }
    if (kmi) {
      wmOperatorType *ot = WM_operatortype_find(kmi->idname, false);
      const char *name = (ot) ? WM_operatortype_name(ot, kmi->ptr) : kmi->idname;
      STRNCPY(cd->text[button_index][type_index], name);
    }
  }

  if (memcmp(&cd_prev.text, &cd->text, sizeof(cd_prev.text)) != 0) {
    ED_area_tag_redraw(area_statusbar);
  }

  CTX_wm_window_set(C, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modal Keymap Status
 * \{ */

bool WM_window_modal_keymap_status_draw(bContext *C, wmWindow *win, uiLayout *layout)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmKeyMap *keymap = nullptr;
  wmOperator *op = nullptr;
  LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
      if (handler->op != nullptr) {
        /* 'handler->keymap' could be checked too, seems not to be used. */
        wmKeyMap *keymap_test = WM_keymap_active(wm, handler->op->type->modalkeymap);
        if (keymap_test && keymap_test->modal_items) {
          keymap = keymap_test;
          op = handler->op;
          break;
        }
      }
    }
  }
  if (keymap == nullptr || keymap->modal_items == nullptr) {
    return false;
  }
  const EnumPropertyItem *items = static_cast<const EnumPropertyItem *>(keymap->modal_items);

  uiLayout *row = uiLayoutRow(layout, true);
  for (int i = 0; items[i].identifier; i++) {
    if (!items[i].identifier[0]) {
      continue;
    }
    if ((keymap->poll_modal_item != nullptr) &&
        (keymap->poll_modal_item(op, items[i].value) == false)) {
      continue;
    }

    bool show_text = true;

    {
      /* WARNING: O(n^2). */
      wmKeyMapItem *kmi = nullptr;
      for (kmi = static_cast<wmKeyMapItem *>(keymap->items.first); kmi; kmi = kmi->next) {
        if (kmi->propvalue == items[i].value) {
          break;
        }
      }
      if (kmi != nullptr) {
        if (kmi->val == KM_RELEASE) {
          /* Assume release events just disable something which was toggled on. */
          continue;
        }
        if (uiTemplateEventFromKeymapItem(row, items[i].name, kmi, false)) {
          show_text = false;
        }
      }
    }
    if (show_text) {
      char buf[UI_MAX_DRAW_STR];
      int available_len = sizeof(buf);
      char *p = buf;
      WM_modalkeymap_operator_items_to_string_buf(
          op->type, items[i].value, true, UI_MAX_SHORTCUT_STR, &available_len, &p);
      p -= 1;
      if (p > buf) {
        BLI_snprintf(p, available_len, ": %s", items[i].name);
        uiItemL(row, buf, 0);
      }
    }
  }
  return true;
}

/** \} */
