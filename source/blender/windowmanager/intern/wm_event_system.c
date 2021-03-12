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

/** \file
 * \ingroup wm
 *
 * Handle events and notifiers from GHOST input (mouse, keyboard, tablet, ndof).
 *
 * Also some operator reports utility functions.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

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
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "BKE_sound.h"

#include "BLT_translation.h"

#include "ED_fileselect.h"
#include "ED_info.h"
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
 * This is not a fool proof solution since since it's possible the gizmo operators would pass
 * through these events when called, see: T65479.
 */
#define USE_GIZMO_MOUSE_PRIORITY_HACK

static void wm_notifier_clear(wmNotifier *note);

static int wm_operator_call_internal(bContext *C,
                                     wmOperatorType *ot,
                                     PointerRNA *properties,
                                     ReportList *reports,
                                     const short context,
                                     const bool poll_only,
                                     wmEvent *event);

static bool wm_operator_check_locked_interface(bContext *C, wmOperatorType *ot);

/* -------------------------------------------------------------------- */
/** \name Event Management
 * \{ */

wmEvent *wm_event_add_ex(wmWindow *win,
                         const wmEvent *event_to_add,
                         const wmEvent *event_to_add_after)
{
  wmEvent *event = MEM_mallocN(sizeof(wmEvent), "wmEvent");

  *event = *event_to_add;

  if (event_to_add_after == NULL) {
    BLI_addtail(&win->event_queue, event);
  }
  else {
    /* Note: strictly speaking this breaks const-correctness,
     * however we're only changing 'next' member. */
    BLI_insertlinkafter(&win->event_queue, (void *)event_to_add_after, event);
  }
  return event;
}

wmEvent *wm_event_add(wmWindow *win, const wmEvent *event_to_add)
{
  return wm_event_add_ex(win, event_to_add, NULL);
}

wmEvent *WM_event_add_simulate(wmWindow *win, const wmEvent *event_to_add)
{
  if ((G.f & G_FLAG_EVENT_SIMULATE) == 0) {
    BLI_assert(0);
    return NULL;
  }
  wmEvent *event = wm_event_add(win, event_to_add);

  /* Logic for setting previous value is documented on the #wmEvent struct,
   * see #wm_event_add_ghostevent for the implementation of logic this follows. */

  win->eventstate->x = event->x;
  win->eventstate->y = event->y;

  if (event->type == MOUSEMOVE) {
    win->eventstate->prevx = event->prevx = win->eventstate->x;
    win->eventstate->prevy = event->prevy = win->eventstate->y;
  }
  else if (ISMOUSE_BUTTON(event->type) || ISKEYBOARD(event->type)) {
    win->eventstate->prevval = event->prevval = win->eventstate->val;
    win->eventstate->prevtype = event->prevtype = win->eventstate->type;

    win->eventstate->val = event->val;
    win->eventstate->type = event->type;

    if (event->val == KM_PRESS) {
      if (event->is_repeat == false) {
        win->eventstate->prevclickx = event->x;
        win->eventstate->prevclicky = event->y;
      }
    }
  }
  return event;
}

void wm_event_free(wmEvent *event)
{
#ifndef NDEBUG
  /* Don't use assert here because it's fairly harmless in most cases,
   * more an issue of correctness, something we should avoid in general. */
  if (event->is_repeat && !ISKEYBOARD(event->type)) {
    printf("%s: 'is_repeat=true' for non-keyboard event, this should not happen.\n", __func__);
    WM_event_print(event);
  }
#endif

  if (event->customdata) {
    if (event->customdatafree) {
      /* Note: pointer to listbase struct elsewhere. */
      if (event->custom == EVT_DATA_DRAGDROP) {
        ListBase *lb = event->customdata;
        WM_drag_free_list(lb);
      }
      else {
        MEM_freeN(event->customdata);
      }
    }
  }

  MEM_freeN(event);
}

static void wm_event_free_last(wmWindow *win)
{
  wmEvent *event = BLI_poptail(&win->event_queue);
  if (event != NULL) {
    wm_event_free(event);
  }
}

void wm_event_free_all(wmWindow *win)
{
  wmEvent *event;
  while ((event = BLI_pophead(&win->event_queue))) {
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

  wmNotifier *note = MEM_callocN(sizeof(wmNotifier), "notifier");

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
  wmWindowManager *wm = bmain->wm.first;

  if (!wm || wm_test_duplicate_notifier(wm, type, reference)) {
    return;
  }

  wmNotifier *note = MEM_callocN(sizeof(wmNotifier), "notifier");

  BLI_addtail(&wm->notifier_queue, note);

  note->category = type & NOTE_CATEGORY;
  note->data = type & NOTE_DATA;
  note->subtype = type & NOTE_SUBTYPE;
  note->action = type & NOTE_ACTION;

  note->reference = reference;
}

/**
 * Clear notifiers by reference, Used so listeners don't act on freed data.
 */
void WM_main_remove_notifier_reference(const void *reference)
{
  Main *bmain = G_MAIN;
  wmWindowManager *wm = bmain->wm.first;

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

void WM_main_remap_editor_id_reference(ID *old_id, ID *new_id)
{
  Main *bmain = G_MAIN;

  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        ED_spacedata_id_remap(area, sl, old_id, new_id);
      }
    }
  }

  wmWindowManager *wm = bmain->wm.first;
  if (wm && wm->message_bus) {
    struct wmMsgBus *mbus = wm->message_bus;
    if (new_id != NULL) {
      WM_msg_id_update(mbus, old_id, new_id);
    }
    else {
      WM_msg_id_remove(mbus, old_id);
    }
  }
}

static void wm_notifier_clear(wmNotifier *note)
{
  /* NULL the entire notifier, only leaving (next, prev) members intact. */
  memset(((char *)note) + sizeof(Link), 0, sizeof(*note) - sizeof(Link));
}

void wm_event_do_depsgraph(bContext *C, bool is_after_open_file)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  /* The whole idea of locked interface is to prevent viewport and whatever
   * thread from modifying the same data. Because of this, we can not perform
   * dependency graph update.
   */
  if (wm->is_interface_locked) {
    return;
  }
  /* Combine datamasks so one window doesn't disable UV's in another T26448. */
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
    /* XXX, hack so operators can enforce datamasks T26482, gl render */
    CustomData_MeshMasks_update(&scene->customdata_mask, &scene->customdata_mask_modal);
    /* TODO(sergey): For now all dependency graphs which are evaluated from
     * workspace are considered active. This will work all fine with "locked"
     * view layer and time across windows. This is to be granted separately,
     * and for until then we have to accept ambiguities when object is shared
     * across visible view layers and has overrides on it.
     */
    Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);
    if (is_after_open_file) {
      DEG_graph_relations_update(depsgraph);
      DEG_graph_on_visible_update(bmain, depsgraph, true);
    }
    DEG_make_active(depsgraph);
    BKE_scene_graph_update_tagged(depsgraph, bmain);
  }
}

/**
 * Was part of #wm_event_do_notifiers,
 * split out so it can be called once before entering the #WM_main loop.
 * This ensures operators don't run before the UI and depsgraph are initialized.
 */
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

  CTX_wm_window_set(C, NULL);
}

static void wm_event_execute_timers(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (UNLIKELY(wm == NULL)) {
    return;
  }

  /* Set the first window as context, so that there is some minimal context. This avoids crashes
   * when calling code that assumes that there is always a window in the context (which many
   * operators do). */
  CTX_wm_window_set(C, wm->windows.first);
  BLI_timer_execute();
  CTX_wm_window_set(C, NULL);
}

/* Called in mainloop. */
void wm_event_do_notifiers(bContext *C)
{
  /* Run the timer before assigning 'wm' in the unlikely case a timer loads a file, see T80028. */
  wm_event_execute_timers(C);

  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == NULL) {
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
      }
      if (note->window == win) {
        if (note->category == NC_SCREEN) {
          if (note->data == ND_WORKSPACE_SET) {
            WorkSpace *ref_ws = note->reference;

            UI_popup_handlers_remove_all(C, &win->modalhandlers);

            WM_window_set_active_workspace(C, win, ref_ws);
            if (G.debug & G_DEBUG_EVENTS) {
              printf("%s: Workspace set %p\n", __func__, note->reference);
            }
          }
          else if (note->data == ND_WORKSPACE_DELETE) {
            WorkSpace *workspace = note->reference;

            ED_workspace_delete(
                workspace, CTX_data_main(C), C, wm); /* XXX hrms, think this over! */
            if (G.debug & G_DEBUG_EVENTS) {
              printf("%s: Workspace delete %p\n", __func__, workspace);
            }
          }
          else if (note->data == ND_LAYOUTBROWSE) {
            bScreen *ref_screen = BKE_workspace_layout_screen_get(note->reference);

            /* free popup handlers only T35434. */
            UI_popup_handlers_remove_all(C, &win->modalhandlers);

            ED_screen_change(C, ref_screen); /* XXX hrms, think this over! */
            if (G.debug & G_DEBUG_EVENTS) {
              printf("%s: screen set %p\n", __func__, note->reference);
            }
          }
          else if (note->data == ND_LAYOUTDELETE) {
            WorkSpace *workspace = WM_window_get_active_workspace(win);
            WorkSpaceLayout *layout = note->reference;

            ED_workspace_layout_delete(workspace, layout, C); /* XXX hrms, think this over! */
            if (G.debug & G_DEBUG_EVENTS) {
              printf("%s: screen delete %p\n", __func__, note->reference);
            }
          }
        }
      }

      if (note->window == win ||
          (note->window == NULL && (note->reference == NULL || note->reference == scene))) {
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
      ED_info_stats_clear(view_layer);
      WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO, NULL);
    }

    if (do_anim) {

      /* XXX, quick frame changes can cause a crash if framechange and rendering
       * collide (happens on slow scenes), BKE_scene_graph_update_for_newframe can be called
       * twice which can depgraph update the same object at once */
      if (G.is_rendering == false) {
        /* Depsgraph gets called, might send more notifiers. */
        Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
        ED_update_for_newframe(CTX_data_main(C), depsgraph);
      }
    }
  }

  /* The notifiers are sent without context, to keep it clean. */
  wmNotifier *note;
  while ((note = BLI_pophead(&wm->notifier_queue))) {
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
          wmRegionListenerParams region_params = {
              .window = win,
              .area = NULL,
              .region = region,
              .scene = scene,
              .notifier = note,
          };
          ED_region_do_listen(&region_params);
        }

        ED_screen_areas_iter (win, screen, area) {
          if ((note->category == NC_SPACE) && note->reference) {
            /* Filter out notifiers sent to other spaces. RNA sets the reference to the owning ID
             * though, the screen, so let notifiers through that reference the entire screen. */
            if ((note->reference != area->spacedata.first) && (note->reference != screen)) {
              continue;
            }
          }
          wmSpaceTypeListenerParams area_params = {
              .window = win,
              .area = area,
              .notifier = note,
              .scene = scene,
          };
          ED_area_do_listen(&area_params);
          LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
            wmRegionListenerParams region_params = {
                .window = win,
                .area = area,
                .region = region,
                .scene = scene,
                .notifier = note,
            };
            ED_region_do_listen(&region_params);
          }
        }
      }
    }

    MEM_freeN(note);
  }
#endif /* if 1 (postpone disabling for in favor of message-bus), eventually. */

  /* Handle message bus. */
  {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      CTX_wm_window_set(C, win);
      WM_msgbus_handle(wm->message_bus, C);
    }
    CTX_wm_window_set(C, NULL);
  }

  wm_event_do_refresh_wm_and_depsgraph(C);

  /* Status bar */
  if (wm->winactive) {
    wmWindow *win = wm->winactive;
    CTX_wm_window_set(C, win);
    WM_window_cursor_keymap_status_refresh(C, win);
    CTX_wm_window_set(C, NULL);
  }

  /* Autorun warning */
  wm_test_autorun_warning(C);
}

static int wm_event_always_pass(const wmEvent *event)
{
  /* Some events we always pass on, to ensure proper communication. */
  return ISTIMER(event->type) || (event->type == WINDEACTIVATE);
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
   * to make the DBL_CLICK conversion work, we just don't send this to UI, except mouse clicks. */
  if (((handler->head.flag & WM_HANDLER_ACCEPT_DBL_CLICK) == 0) && !ISMOUSE_BUTTON(event->type) &&
      (event->val == KM_DBL_CLICK)) {
    return WM_HANDLER_CONTINUE;
  }

  /* UI is quite aggressive with swallowing events, like scroll-wheel. */
  /* I realize this is not extremely nice code... when UI gets keymaps it can be maybe smarter. */
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

  /* putting back screen context */
  if ((retval != WM_UI_HANDLER_BREAK) || always_pass) {
    CTX_wm_area_set(C, area);
    CTX_wm_region_set(C, region);
    CTX_wm_menu_set(C, menu);
  }
  else {
    /* This special cases is for areas and regions that get removed. */
    CTX_wm_area_set(C, NULL);
    CTX_wm_region_set(C, NULL);
    CTX_wm_menu_set(C, NULL);
  }

  if (retval == WM_UI_HANDLER_BREAK) {
    return WM_HANDLER_BREAK;
  }

  /* event not handled in UI, if wheel then we temporarily disable it */
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
      BLI_assert(handler->handle_fn != NULL);
      wmEvent event;
      wm_event_init_from_window(win, &event);
      event.type = EVT_BUT_CANCEL;
      event.val = reactivate_button ? 0 : 1;
      event.is_repeat = false;
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

/**
 * Show the report in the info header.
 */
void WM_report_banner_show(void)
{
  wmWindowManager *wm = G_MAIN->wm.first;
  ReportList *wm_reports = &wm->reports;

  /* After adding reports to the global list, reset the report timer. */
  WM_event_remove_timer(wm, NULL, wm_reports->reporttimer);

  /* Records time since last report was added */
  wm_reports->reporttimer = WM_event_add_timer(wm, wm->winactive, TIMERREPORT, 0.05);

  ReportTimerInfo *rti = MEM_callocN(sizeof(ReportTimerInfo), "ReportTimerInfo");
  wm_reports->reporttimer->customdata = rti;
}

/**
 * Hide all currently displayed banners and abort their timer.
 */
void WM_report_banners_cancel(Main *bmain)
{
  wmWindowManager *wm = bmain->wm.first;
  BKE_reports_clear(&wm->reports);
  WM_event_remove_timer(wm, NULL, wm->reports.reporttimer);
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
    wmWindowManager *wm = G_MAIN->wm.first;

    /* add reports to the global list, otherwise they are not seen */
    BLI_movelisttolist(&wm->reports.list, &reports->list);

    WM_report_banner_show();
  }
}

void WM_report(ReportType type, const char *message)
{
  ReportList reports;
  BKE_reports_init(&reports, RPT_STORE);
  BKE_report(&reports, type, message);

  wm_add_reports(&reports);

  BKE_reports_clear(&reports);
}

void WM_reportf(ReportType type, const char *format, ...)
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
    wmOperatorType *ot_macro = WM_operatortype_find(macro->idname, 0);

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

/* sets up the new context and calls 'wm_operator_invoke()' with poll_only */
bool WM_operator_poll_context(bContext *C, wmOperatorType *ot, short context)
{
  return wm_operator_call_internal(C, ot, NULL, NULL, context, true, NULL);
}

bool WM_operator_check_ui_empty(wmOperatorType *ot)
{
  if (ot->macro.first != NULL) {
    /* For macros, check all have exec() we can call. */
    LISTBASE_FOREACH (wmOperatorTypeMacro *, macro, &ot->macro) {
      wmOperatorType *otm = WM_operatortype_find(macro->idname, 0);
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

/**
 * Sets the active region for this space from the context.
 *
 * \see #BKE_area_find_region_active_win
 */
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

/* (caller_owns_reports == true) when called from python. */
static void wm_operator_reports(bContext *C, wmOperator *op, int retval, bool caller_owns_reports)
{
  if (G.background == 0 && caller_owns_reports == false) { /* popup */
    if (op->reports->list.first) {
      /* FIXME, temp setting window, see other call to UI_popup_menu_reports for why. */
      wmWindow *win_prev = CTX_wm_window(C);
      ScrArea *area_prev = CTX_wm_area(C);
      ARegion *region_prev = CTX_wm_region(C);

      if (win_prev == NULL) {
        CTX_wm_window_set(C, CTX_wm_manager(C)->windows.first);
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

  /* Refresh Info Editor with reports immediately, even if op returned OPERATOR_CANCELLED. */
  if ((retval & OPERATOR_CANCELLED) && !BLI_listbase_is_empty(&op->reports->list)) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO_REPORT, NULL);
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

  op->customdata = NULL;

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
      /* take ownership of reports (in case python provided own) */
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
      if (area) {
        ED_area_type_hud_ensure(C, area);
      }
    }
    else if (hud_status == CLEAR) {
      ED_area_type_hud_clear(wm, NULL);
    }
    else {
      BLI_assert(0);
    }
  }
}

/* If repeat is true, it doesn't register again, nor does it free. */
static int wm_operator_exec(bContext *C, wmOperator *op, const bool repeat, const bool store)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  int retval = OPERATOR_CANCELLED;

  CTX_wm_operator_poll_msg_set(C, NULL);

  if (op == NULL || op->type == NULL) {
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

  /* XXX(mont29) Disabled the repeat check to address part 2 of T31840.
   * Carefully checked all calls to wm_operator_exec and WM_operator_repeat, don't see any reason
   * why this was needed, but worth to note it in case something turns bad. */
  if (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED) /* && repeat == 0 */) {
    wm_operator_reports(C, op, retval, false);
  }

  if (retval & OPERATOR_FINISHED) {
    wm_operator_finished(C, op, repeat, store && wm->op_undo_depth == 0);
  }
  else if (repeat == 0) {
    /* warning: modal from exec is bad practice, but avoid crashing. */
    if (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED)) {
      WM_operator_free(op);
    }
  }

  return retval | OPERATOR_HANDLED;
}

/* Simply calls exec with basic checks. */
static int wm_operator_exec_notest(bContext *C, wmOperator *op)
{
  int retval = OPERATOR_CANCELLED;

  if (op == NULL || op->type == NULL || op->type->exec == NULL) {
    return retval;
  }

  retval = op->type->exec(C, op);
  OPERATOR_RETVAL_CHECK(retval);

  return retval;
}

/**
 * For running operators with frozen context (modal handlers, menus).
 *
 * \param store: Store settings for re-use.
 *
 * \warning do not use this within an operator to call its self! T29537.
 */
int WM_operator_call_ex(bContext *C, wmOperator *op, const bool store)
{
  return wm_operator_exec(C, op, false, store);
}

int WM_operator_call(bContext *C, wmOperator *op)
{
  return WM_operator_call_ex(C, op, false);
}

/**
 * This is intended to be used when an invoke operator wants to call exec on its self
 * and is basically like running op->type->exec() directly, no poll checks no freeing,
 * since we assume whoever called invoke will take care of that
 */
int WM_operator_call_notest(bContext *C, wmOperator *op)
{
  return wm_operator_exec_notest(C, op);
}

/**
 * Execute this operator again, put here so it can share above code
 */
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
/**
 * \return true if #WM_operator_repeat can run.
 * Simple check for now but may become more involved.
 * To be sure the operator can run call `WM_operator_poll(C, op->type)` also, since this call
 * checks if #WM_operator_repeat() can run at all, not that it WILL run at any time.
 */
bool WM_operator_repeat_check(const bContext *UNUSED(C), wmOperator *op)
{
  if (op->type->exec != NULL) {
    return true;
  }
  if (op->opm) {
    /* for macros, check all have exec() we can call */
    LISTBASE_FOREACH (wmOperatorTypeMacro *, macro, &op->opm->type->macro) {
      wmOperatorType *otm = WM_operatortype_find(macro->idname, 0);
      if (otm && otm->exec == NULL) {
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
  if (op->prev == NULL && op->next == NULL) {
    wmWindowManager *wm = CTX_wm_manager(C);
    op_prev = wm->operators.last;
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
  /* XXX operatortype names are static still. for debug */
  wmOperator *op = MEM_callocN(sizeof(wmOperator), ot->idname);

  /* XXX adding new operator could be function, only happens here now */
  op->type = ot;
  BLI_strncpy(op->idname, ot->idname, OP_MAX_TYPENAME);

  /* Initialize properties, either copy or create. */
  op->ptr = MEM_callocN(sizeof(PointerRNA), "wmOperatorPtrRNA");
  if (properties && properties->data) {
    op->properties = IDP_CopyProperty(properties->data);
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
    op->reports = MEM_mallocN(sizeof(ReportList), "wmOperatorReportList");
    BKE_reports_init(op->reports, RPT_STORE | RPT_FREE);
  }

  /* Recursive filling of operator macro list. */
  if (ot->macro.first) {
    static wmOperator *motherop = NULL;
    int root = 0;

    /* Ensure all ops are in execution order in 1 list. */
    if (motherop == NULL) {
      motherop = op;
      root = 1;
    }

    /* If properties exist, it will contain everything needed. */
    if (properties) {
      wmOperatorTypeMacro *otmacro = ot->macro.first;

      RNA_STRUCT_BEGIN (properties, prop) {

        if (otmacro == NULL) {
          break;
        }

        /* Skip invalid properties. */
        if (STREQ(RNA_property_identifier(prop), otmacro->idname)) {
          wmOperatorType *otm = WM_operatortype_find(otmacro->idname, 0);
          PointerRNA someptr = RNA_property_pointer_get(properties, prop);
          wmOperator *opm = wm_operator_create(wm, otm, &someptr, NULL);

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
        wmOperatorType *otm = WM_operatortype_find(macro->idname, 0);
        wmOperator *opm = wm_operator_create(wm, otm, macro->ptr, NULL);

        BLI_addtail(&motherop->macro, opm);
        opm->opm = motherop; /* Pointer to mom, for modal(). */
      }
    }

    if (root) {
      motherop = NULL;
    }
  }

  WM_operator_properties_sanitize(op->ptr, 0);

  return op;
}

static void wm_region_mouse_co(bContext *C, wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  if (region) {
    /* Compatibility convention. */
    event->mval[0] = event->x - region->winrct.xmin;
    event->mval[1] = event->y - region->winrct.ymin;
  }
  else {
    /* These values are invalid (avoid odd behavior by relying on old mval values). */
    event->mval[0] = -1;
    event->mval[1] = -1;
  }
}

/**
 * Also used for exec when 'event' is NULL.
 */
static int wm_operator_invoke(bContext *C,
                              wmOperatorType *ot,
                              wmEvent *event,
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

    /* If reports == NULL, they'll be initialized. */
    wmOperator *op = wm_operator_create(wm, ot, properties, reports);

    const bool is_nested_call = (wm->op_undo_depth != 0);

    if (event != NULL) {
      op->flag |= OP_IS_INVOKE;
    }

    /* Initialize setting from previous run. */
    if (!is_nested_call && use_last_properties) { /* Not called by py script. */
      WM_operator_last_properties_init(op);
    }

    if ((event == NULL) || (event->type != MOUSEMOVE)) {
      CLOG_INFO(WM_LOG_HANDLERS,
                2,
                "handle evt %d win %p op %s",
                event ? event->type : 0,
                CTX_wm_screen(C)->active_region,
                ot->idname);
    }

    if (op->type->invoke && event) {
      wm_region_mouse_co(C, event);

      if (op->type->flag & OPTYPE_UNDO) {
        wm->op_undo_depth++;
      }

      retval = op->type->invoke(C, op, event);
      OPERATOR_RETVAL_CHECK(retval);

      if (op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm) {
        wm->op_undo_depth--;
      }
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

    /* Note, if the report is given as an argument then assume the caller will deal with displaying
     * them currently Python only uses this. */
    if (!(retval & OPERATOR_HANDLED) && (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED))) {
      /* Only show the report if the report list was not given in the function. */
      wm_operator_reports(C, op, retval, (reports != NULL));
    }

    if (retval & OPERATOR_HANDLED) {
      /* Do nothing, wm_operator_exec() has been called somewhere. */
    }
    else if (retval & OPERATOR_FINISHED) {
      const bool store = !is_nested_call && use_last_properties;
      wm_operator_finished(C, op, false, store);
    }
    else if (retval & OPERATOR_RUNNING_MODAL) {
      /* Take ownership of reports (in case python provided own). */
      op->reports->flag |= RPT_FREE;

      /* Grab cursor during blocking modal ops (X11)
       * Also check for macro.
       */
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
          const rcti *winrect = NULL;
          ARegion *region = CTX_wm_region(C);
          ScrArea *area = CTX_wm_area(C);

          /* Wrap only in X for header. */
          if (region && RGN_TYPE_IS_HEADER_ANY(region->regiontype)) {
            wrap = WM_CURSOR_WRAP_X;
          }

          if (region && region->regiontype == RGN_TYPE_WINDOW &&
              BLI_rcti_isect_pt_v(&region->winrct, &event->x)) {
            winrect = &region->winrct;
          }
          else if (area && BLI_rcti_isect_pt_v(&area->totrct, &event->x)) {
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

      /* Cancel UI handlers, typically tooltips that can hang around
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
                                     const short context,
                                     const bool poll_only,
                                     wmEvent *event)
{
  int retval;

  CTX_wm_operator_poll_msg_set(C, NULL);

  /* Dummy test. */
  if (ot) {
    wmWindow *window = CTX_wm_window(C);

    if (event == NULL) {
      switch (context) {
        case WM_OP_INVOKE_DEFAULT:
        case WM_OP_INVOKE_REGION_WIN:
        case WM_OP_INVOKE_REGION_PREVIEW:
        case WM_OP_INVOKE_REGION_CHANNELS:
        case WM_OP_INVOKE_AREA:
        case WM_OP_INVOKE_SCREEN:
          /* Window is needed for invoke and cancel operators. */
          if (window == NULL) {
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
          event = NULL;
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
          event = NULL;
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
         * but we stay in the same region if we are already in one.
         */
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

        CTX_wm_region_set(C, NULL);
        retval = wm_operator_invoke(C, ot, event, properties, reports, poll_only, true);
        CTX_wm_region_set(C, region);

        return retval;
      }
      case WM_OP_EXEC_SCREEN:
      case WM_OP_INVOKE_SCREEN: {
        /* Remove region + area from context. */
        ARegion *region = CTX_wm_region(C);
        ScrArea *area = CTX_wm_area(C);

        CTX_wm_region_set(C, NULL);
        CTX_wm_area_set(C, NULL);
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

/* Invokes operator in context. */
int WM_operator_name_call_ptr(bContext *C,
                              wmOperatorType *ot,
                              short context,
                              PointerRNA *properties)
{
  BLI_assert(ot == WM_operatortype_find(ot->idname, true));
  return wm_operator_call_internal(C, ot, properties, NULL, context, false, NULL);
}
int WM_operator_name_call(bContext *C, const char *opstring, short context, PointerRNA *properties)
{
  wmOperatorType *ot = WM_operatortype_find(opstring, 0);
  if (ot) {
    return WM_operator_name_call_ptr(C, ot, context, properties);
  }

  return 0;
}

int WM_operator_name_call_with_properties(struct bContext *C,
                                          const char *opstring,
                                          short context,
                                          struct IDProperty *properties)
{
  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find(opstring, false);
  RNA_pointer_create(NULL, ot->srna, properties, &props_ptr);
  return WM_operator_name_call_ptr(C, ot, context, &props_ptr);
}

/**
 * Call an existent menu. The menu can be created in C or Python.
 */
void WM_menu_name_call(bContext *C, const char *menu_name, short context)
{
  wmOperatorType *ot = WM_operatortype_find("WM_OT_call_menu", false);
  PointerRNA ptr;
  WM_operator_properties_create_ptr(&ptr, ot);
  RNA_string_set(&ptr, "name", menu_name);
  WM_operator_name_call_ptr(C, ot, context, &ptr);
  WM_operator_properties_free(&ptr);
}

/**
 * Similar to #WM_operator_name_call called with #WM_OP_EXEC_DEFAULT context.
 *
 * - #wmOperatorType is used instead of operator name since python already has the operator type.
 * - `poll()` must be called by python before this runs.
 * - reports can be passed to this function (so python can report them as exceptions).
 */
int WM_operator_call_py(bContext *C,
                        wmOperatorType *ot,
                        short context,
                        PointerRNA *properties,
                        ReportList *reports,
                        const bool is_undo)
{
  int retval = OPERATOR_CANCELLED;

#if 0
  wmOperator *op;
  op = wm_operator_create(wm, ot, properties, reports);

  if (op->type->exec) {
    if (is_undo && op->type->flag & OPTYPE_UNDO) {
      wm->op_undo_depth++;
    }

    retval = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(retval);

    if (is_undo && op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm) {
      wm->op_undo_depth--;
    }
  }
  else {
    CLOG_WARN(WM_LOG_OPERATORS,
              "\"%s\" operator has no exec function, Python cannot call it",
              op->type->name);
  }

#endif

  /* Not especially nice using undo depth here. It's used so Python never
   * triggers undo or stores an operator's last used state.
   *
   * We could have some more obvious way of doing this like passing a flag.
   */
  wmWindowManager *wm = CTX_wm_manager(C);
  if (!is_undo && wm) {
    wm->op_undo_depth++;
  }

  retval = wm_operator_call_internal(C, ot, properties, reports, context, false, NULL);

  if (!is_undo && wm && (wm == CTX_wm_manager(C))) {
    wm->op_undo_depth--;
  }

  return retval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Handler Types
 *
 * General API for different handler types.
 * \{ */

/* Future extra customadata free? */
void wm_event_free_handler(wmEventHandler *handler)
{
  MEM_freeN(handler);
}

/* Only set context when area/region is part of screen. */
static void wm_handler_op_context(bContext *C, wmEventHandler_Op *handler, const wmEvent *event)
{
  wmWindow *win = handler->context.win ? handler->context.win : CTX_wm_window(C);
  /* It's probably fine to always use WM_window_get_active_screen() to get the screen. But this
   * code has been getting it through context since forever, so play safe and stick to that when
   * possible. */
  bScreen *screen = handler->context.win ? WM_window_get_active_screen(win) : CTX_wm_screen(C);

  if (screen == NULL || handler->op == NULL) {
    return;
  }

  if (handler->context.area == NULL) {
    CTX_wm_area_set(C, NULL);
  }
  else {
    ScrArea *area = NULL;

    ED_screen_areas_iter (win, screen, area_iter) {
      if (area_iter == handler->context.area) {
        area = area_iter;
        break;
      }
    }

    if (area == NULL) {
      /* When changing screen layouts with running modal handlers (like render display), this
       * is not an error to print. */
      if (handler->op == NULL) {
        CLOG_ERROR(WM_LOG_HANDLERS,
                   "internal error: handler (%s) has invalid area",
                   handler->op->type->idname);
      }
    }
    else {
      ARegion *region;
      wmOperator *op = handler->op ? (handler->op->opm ? handler->op->opm : handler->op) : NULL;
      CTX_wm_area_set(C, area);

      if (op && (op->flag & OP_IS_MODAL_CURSOR_REGION)) {
        region = BKE_area_find_region_xy(area, handler->context.region_type, event->x, event->y);
        if (region) {
          handler->context.region = region;
        }
      }
      else {
        region = NULL;
      }

      if (region == NULL) {
        LISTBASE_FOREACH (ARegion *, region_iter, &area->regionbase) {
          region = region_iter;
          if (region == handler->context.region) {
            break;
          }
        }
      }

      /* XXX no warning print here, after full-area and back regions are remade. */
      if (region) {
        CTX_wm_region_set(C, region);
      }
    }
  }
}

/* Called on exit or remove area, only here call cancel callback. */
void WM_event_remove_handlers(bContext *C, ListBase *handlers)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* C is zero on freeing database, modal handlers then already were freed */
  wmEventHandler *handler_base;
  while ((handler_base = BLI_pophead(handlers))) {
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

        WM_cursor_grab_disable(win, NULL);
        WM_operator_free(handler->op);
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

  if (winevent->is_repeat) {
    if (kmi->flag & KMI_REPEAT_IGNORE) {
      return false;
    }
  }

  const int kmitype = WM_userdef_event_map(kmi->type);

  /* The matching rules. */
  if (kmitype == KM_TEXTINPUT) {
    if (winevent->val == KM_PRESS) { /* Prevent double clicks. */
      /* NOT using ISTEXTINPUT anymore because (at least on Windows) some key codes above 255
       * could have printable ascii keys - BUG T30479. */
      if (ISKEYBOARD(winevent->type) && (winevent->ascii || winevent->utf8_buf[0])) {
        return true;
      }
    }
  }

  if (kmitype != KM_ANY) {
    if (ELEM(kmitype, TABLET_STYLUS, TABLET_ERASER)) {
      const wmTabletData *wmtab = &winevent->tablet;

      if (winevent->type != LEFTMOUSE) {
        /* Tablet events can occur on hover + keypress. */
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

  /* Modifiers also check bits, so it allows modifier order.
   * Account for rare case of when these keys are used as the 'type' not as modifiers. */
  if (kmi->shift != KM_ANY) {
    if ((winevent->shift != kmi->shift) && !(winevent->shift & kmi->shift) &&
        !ELEM(winevent->type, EVT_LEFTSHIFTKEY, EVT_RIGHTSHIFTKEY)) {
      return false;
    }
  }
  if (kmi->ctrl != KM_ANY) {
    if (winevent->ctrl != kmi->ctrl && !(winevent->ctrl & kmi->ctrl) &&
        !ELEM(winevent->type, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY)) {
      return false;
    }
  }
  if (kmi->alt != KM_ANY) {
    if (winevent->alt != kmi->alt && !(winevent->alt & kmi->alt) &&
        !ELEM(winevent->type, EVT_LEFTALTKEY, EVT_RIGHTALTKEY)) {
      return false;
    }
  }
  if (kmi->oskey != KM_ANY) {
    if (winevent->oskey != kmi->oskey && !(winevent->oskey & kmi->oskey) &&
        (winevent->type != EVT_OSKEY)) {
      return false;
    }
  }

  /* Only keymap entry with keymodifier is checked,
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
      if ((keymap->poll_modal_item == NULL) || (keymap->poll_modal_item(op, kmi->propvalue))) {
        return kmi;
      }
    }
  }
  return NULL;
}

struct wmEvent_ModalMapStore {
  short prevtype;
  short prevval;

  bool dbl_click_disabled;
};

/**
 * This function prepares events for use with #wmOperatorType.modal by:
 *
 * - Matching keymap items with the operators modal keymap.
 * - Converting double click events into press events,
 *   allowing them to be restored when the events aren't handled.
 *
 *   This is done since we only want to use double click events to match key-map items,
 *   allowing modal functions to check for press/release events without having to interpret them.
 */
static void wm_event_modalkeymap_begin(const bContext *C,
                                       wmOperator *op,
                                       wmEvent *event,
                                       struct wmEvent_ModalMapStore *event_backup)
{
  BLI_assert(event->type != EVT_MODAL_MAP);

  /* Support for modal keymap in macros. */
  if (op->opm) {
    op = op->opm;
  }

  event_backup->dbl_click_disabled = false;

  if (op->type->modalkeymap) {
    wmKeyMap *keymap = WM_keymap_active(CTX_wm_manager(C), op->type->modalkeymap);
    wmKeyMapItem *kmi = NULL;

    const wmEvent *event_match = NULL;
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

    if (event_match != NULL) {
      event_backup->prevtype = event->prevtype;
      event_backup->prevval = event->prevval;

      event->prevtype = event_match->type;
      event->prevval = event_match->val;
      event->type = EVT_MODAL_MAP;
      event->val = kmi->propvalue;

      /* Avoid double-click events even in the case of 'EVT_MODAL_MAP',
       * since it's possible users configure double-click keymap items
       * which would break when modal functions expect press/release. */
      if (event->prevtype == KM_DBL_CLICK) {
        event->prevtype = KM_PRESS;
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
static void wm_event_modalkeymap_end(wmEvent *event,
                                     const struct wmEvent_ModalMapStore *event_backup)
{
  if (event->type == EVT_MODAL_MAP) {
    event->type = event->prevtype;
    event->val = event->prevval;

    event->prevtype = event_backup->prevtype;
    event->prevval = event_backup->prevval;
  }

  if (event_backup->dbl_click_disabled) {
    event->val = KM_DBL_CLICK;
  }
}

/* Warning: this function removes a modal handler, when finished */
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
      (((wmEventHandler_Op *)handler_base)->op != NULL)) {
    wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
    wmOperator *op = handler->op;
    wmOperatorType *ot = op->type;

    if (!wm_operator_check_locked_interface(C, ot)) {
      /* Interface is locked and operator is not allowed to run,
       * nothing to do in this case.
       */
    }
    else if (ot->modal) {
      /* We set context to where modal handler came from. */
      wmWindowManager *wm = CTX_wm_manager(C);
      ScrArea *area = CTX_wm_area(C);
      ARegion *region = CTX_wm_region(C);

      wm_handler_op_context(C, handler, event);
      wm_region_mouse_co(C, event);

      struct wmEvent_ModalMapStore event_backup;
      wm_event_modalkeymap_begin(C, op, event, &event_backup);

      if (ot->flag & OPTYPE_UNDO) {
        wm->op_undo_depth++;
      }

      /* Warning, after this call all context data and 'event' may be freed. see check below. */
      retval = ot->modal(C, op, event);
      OPERATOR_RETVAL_CHECK(retval);

      /* When this is _not_ the case the modal modifier may have loaded
       * a new blend file (demo mode does this), so we have to assume
       * the event, operator etc have all been freed. - campbell */
      if (CTX_wm_manager(C) == wm) {

        wm_event_modalkeymap_end(event, &event_backup);

        if (ot->flag & OPTYPE_UNDO) {
          wm->op_undo_depth--;
        }

        if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED)) {
          wm_operator_reports(C, op, retval, false);

          if (op->type->modalkeymap) {
            wmWindow *win = CTX_wm_window(C);
            WM_window_status_area_tag_redraw(win);
          }
        }
        else {
          /* Not very common, but modal operators may report before finishing. */
          if (!BLI_listbase_is_empty(&op->reports->list)) {
            wm_add_reports(op->reports);
          }
        }

        /* Important to run 'wm_operator_finished' before NULLing the context members. */
        if (retval & OPERATOR_FINISHED) {
          wm_operator_finished(C, op, false, true);
          handler->op = NULL;
        }
        else if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED)) {
          WM_operator_free(op);
          handler->op = NULL;
        }

        /* Putting back screen context, reval can pass through after modal failures! */
        if ((retval & OPERATOR_PASS_THROUGH) || wm_event_always_pass(event)) {
          CTX_wm_area_set(C, area);
          CTX_wm_region_set(C, region);
        }
        else {
          /* This special cases is for areas and regions that get removed. */
          CTX_wm_area_set(C, NULL);
          CTX_wm_region_set(C, NULL);
        }

        /* /update gizmos during modal handlers. */
        wm_gizmomaps_handled_modal_update(C, event, handler);

        /* Remove modal handler, operator itself should have been canceled and freed. */
        if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED)) {
          WM_cursor_grab_disable(CTX_wm_window(C), NULL);

          BLI_remlink(handlers, handler);
          wm_event_free_handler(&handler->head);

          /* prevent silly errors from operator users */
          // retval &= ~OPERATOR_PASS_THROUGH;
        }
      }
    }
    else {
      CLOG_ERROR(WM_LOG_HANDLERS, "missing modal '%s'", op->idname);
    }
  }
  else {
    wmOperatorType *ot = WM_operatortype_find(kmi_idname, 0);

    if (ot && wm_operator_check_locked_interface(C, ot)) {
      bool use_last_properties = true;
      PointerRNA tool_properties = {0};

      bToolRef *keymap_tool = NULL;
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

      const bool is_tool = (keymap_tool != NULL);
      const bool use_tool_properties = is_tool;

      if (use_tool_properties) {
        WM_toolsystem_ref_properties_init_for_keymap(
            keymap_tool, &tool_properties, properties, ot);
        properties = &tool_properties;
        use_last_properties = false;
      }

      retval = wm_operator_invoke(C, ot, event, properties, NULL, false, use_last_properties);

      if (use_tool_properties) {
        WM_operator_properties_free(&tool_properties);
      }

      /* Link gizmo if 'WM_GIZMOGROUPTYPE_TOOL_INIT' is set. */
      if (retval & OPERATOR_FINISHED) {
        if (is_tool) {
          bToolRef_Runtime *tref_rt = keymap_tool->runtime;
          if (tref_rt->gizmo_group[0]) {
            const char *idname = tref_rt->gizmo_group;
            wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
            if (gzgt != NULL) {
              if ((gzgt->flag & WM_GIZMOGROUPTYPE_TOOL_INIT) != 0) {
                ARegion *region = CTX_wm_region(C);
                if (region != NULL) {
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

/* Fileselect handlers are only in the window queue,
 * so it's safe to switch screens or area types. */
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
        /* Header on bottom, AZone triangle to toggle header looks misplaced at the top. */
        region_header->alignment = RGN_ALIGN_BOTTOM;

        /* Settings for filebrowser, #sfile is not operator owner but sends events. */
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

      /* Remove link now, for load file case before removing. */
      BLI_remlink(handlers, handler);

      if (val == EVT_FILESELECT_EXTERNAL_CANCEL) {
        /* The window might have been freed already. */
        if (BLI_findindex(&wm->windows, handler->context.win) == -1) {
          handler->context.win = NULL;
        }
      }
      else {
        ScrArea *ctx_area = CTX_wm_area(C);

        wmWindow *temp_win = NULL;
        LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
          bScreen *screen = WM_window_get_active_screen(win);
          ScrArea *file_area = screen->areabase.first;

          if ((file_area->spacetype != SPACE_FILE) || !WM_window_is_temp_screen(win)) {
            continue;
          }

          if (file_area->full) {
            /* Users should not be able to maximize/fullscreen an area in a temporary screen. So if
             * there's a maximized file browser in a temporary screen, it was likely opened by
             * #EVT_FILESELECT_FULL_OPEN. */
            continue;
          }

          int win_size[2];
          bool is_maximized;
          ED_fileselect_window_params_get(win, win_size, &is_maximized);
          ED_fileselect_params_to_userdef(file_area->spacedata.first, win_size, is_maximized);

          if (BLI_listbase_is_single(&file_area->spacedata)) {
            BLI_assert(ctx_win != win);

            wm_window_close(C, wm, win);

            CTX_wm_window_set(C, ctx_win); /* #wm_window_close() NULLs. */
            /* Some operators expect a drawable context (for EVT_FILESELECT_EXEC). */
            wm_window_make_drawable(wm, ctx_win);
            /* Ensure correct cursor position, otherwise, popups may close immediately after
             * opening (UI_BLOCK_MOVEMOUSE_QUIT). */
            wm_cursor_position_get(ctx_win, &ctx_win->eventstate->x, &ctx_win->eventstate->y);
            wm->winactive = ctx_win; /* Reports use this... */
            if (handler->context.win == win) {
              handler->context.win = NULL;
            }
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
          ED_fileselect_params_to_userdef(ctx_area->spacedata.first, NULL, false);
          ED_screen_full_prevspace(C, ctx_area);
        }
      }

      wm_handler_op_context(C, handler, ctx_win->eventstate);
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

        /* XXX check this carefully, CTX_wm_manager(C) == wm is a bit hackish */
        if (handler->op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm) {
          wm->op_undo_depth--;
        }

        /* XXX check this carefully, CTX_wm_manager(C) == wm is a bit hackish */
        if (CTX_wm_manager(C) == wm && wm->op_undo_depth == 0) {
          if (handler->op->type->flag & OPTYPE_UNDO) {
            ED_undo_push_op(C, handler->op);
          }
          else if (handler->op->type->flag & OPTYPE_UNDO_GROUPED) {
            ED_undo_grouped_push_op(C, handler->op);
          }
        }

        if (handler->op->reports->list.first) {

          /* FIXME, temp setting window, this is really bad!
           * only have because lib linking errors need to be seen by users :(
           * it can be removed without breaking anything but then no linking errors - campbell */
          wmWindow *win_prev = CTX_wm_window(C);
          ScrArea *area_prev = CTX_wm_area(C);
          ARegion *region_prev = CTX_wm_region(C);

          if (win_prev == NULL) {
            CTX_wm_window_set(C, CTX_wm_manager(C)->windows.first);
          }

          BKE_report_print_level_set(handler->op->reports, RPT_WARNING);
          UI_popup_menu_reports(C, handler->op->reports);

          /* XXX - copied from 'wm_operator_finished()' */
          /* add reports to the global list, otherwise they are not seen */
          BLI_movelisttolist(&CTX_wm_reports(C)->list, &handler->op->reports->list);

          /* More hacks, since we meddle with reports, banner display doesn't happen automaticM */
          WM_report_banner_show();

          CTX_wm_window_set(C, win_prev);
          CTX_wm_area_set(C, area_prev);
          CTX_wm_region_set(C, region_prev);
        }

        /* For WM_operator_pystring only, custom report handling is done above. */
        wm_operator_reports(C, handler->op, retval, true);

        if (retval & OPERATOR_FINISHED) {
          WM_operator_last_properties_store(handler->op);
        }

        if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED)) {
          WM_operator_free(handler->op);
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

        WM_operator_free(handler->op);
      }

      CTX_wm_area_set(C, NULL);

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

  if (keymap == NULL) {
    /* Only callback is allowed to have NULL keymaps. */
    BLI_assert(handler->dynamic.keymap_fn);
  }
  else {
    PRINT("%s:   checking '%s' ...", __func__, keymap->idname);

    if (WM_keymap_poll(C, keymap)) {

      PRINT("pass\n");

      LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
        if (wm_eventmatch(event, kmi)) {
          struct wmEventHandler_KeymapPost keymap_post = handler->post;

          PRINT("%s:     item matched '%s'\n", __func__, kmi->idname);

          action |= wm_handler_operator_call(
              C, handlers, &handler->head, event, kmi->ptr, kmi->idname);

          if (action & WM_HANDLER_BREAK) {
            /* Not always_pass here, it denotes removed handler_base. */
            CLOG_INFO(WM_LOG_HANDLERS, 2, "handled! '%s'", kmi->idname);
            if (keymap_post.post_fn != NULL) {
              keymap_post.post_fn(keymap, kmi, keymap_post.user_data);
            }
            break;
          }
          if (action & WM_HANDLER_HANDLED) {
            CLOG_INFO(WM_LOG_HANDLERS, 2, "handled - and pass on! '%s'", kmi->idname);
          }
          else {
            CLOG_INFO(WM_LOG_HANDLERS, 2, "un-handled '%s'", kmi->idname);
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

        /* handler->op is called later, we want keymap op to be triggered here. */
        action |= wm_handler_operator_call(
            C, handlers, &handler->head, event, kmi->ptr, kmi->idname);

        CTX_wm_gizmo_group_set(C, NULL);

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
  int action = WM_HANDLER_CONTINUE;
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  wmGizmoMap *gzmap = handler->gizmo_map;
  BLI_assert(gzmap != NULL);
  wmGizmo *gz = wm_gizmomap_highlight_get(gzmap);

  /* Needed so UI blocks over gizmos don't let events fall through to the gizmos,
   * noticeable for the node editor - where dragging on a node should move it, see: T73212.
   * note we still allow for starting the gizmo drag outside, then travel 'inside' the node. */
  if (region->type->clip_gizmo_events_by_ui) {
    if (UI_region_block_find_mouse_over(region, &event->x, true)) {
      if (gz != NULL && event->type != EVT_GIZMO_UPDATE) {
        WM_tooltip_clear(C, CTX_wm_window(C));
        wm_gizmomap_highlight_set(gzmap, C, NULL, 0);
      }
      return action;
    }
  }

  if (region->gizmo_map != handler->gizmo_map) {
    WM_gizmomap_tag_refresh(handler->gizmo_map);
  }

  wm_gizmomap_handler_context_gizmo(C, handler);
  wm_region_mouse_co(C, event);

  /* Drag events use the previous click location to highlight the gizmos,
   * Get the highlight again in case the user dragged off the gizmo. */
  const bool is_event_drag = ISTWEAK(event->type) || (event->val == KM_CLICK_DRAG);
  const bool is_event_modifier = ISKEYMODIFIER(event->type);

  bool handle_highlight = false;
  bool handle_keymap = false;

  /* Handle gizmo highlighting. */
  if (!wm_gizmomap_modal_get(gzmap) &&
      ((event->type == MOUSEMOVE) || is_event_modifier || is_event_drag)) {
    handle_highlight = true;
    if (is_event_modifier || is_event_drag) {
      handle_keymap = true;
    }
  }
  else {
    handle_keymap = true;
  }

  if (handle_highlight) {
    struct {
      wmGizmo *gz;
      int part;
    } prev = {
        .gz = gz,
        .part = gz ? gz->highlight_part : 0,
    };
    int part = -1;
    gz = wm_gizmomap_highlight_find(gzmap, C, event, &part);

    /* If no gizmos are/were active, don't clear tool-tips. */
    if (gz || prev.gz) {
      if ((prev.gz != gz) || (prev.part != part)) {
        WM_tooltip_clear(C, CTX_wm_window(C));
      }
    }

    if (wm_gizmomap_highlight_set(gzmap, C, gz, part)) {
      if (gz != NULL) {
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
    if ((gz != NULL) && (gz->flag & WM_GIZMO_HIDDEN_KEYMAP) == 0) {
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

          wmEvent event_test_tweak = *event;
          event_test_tweak.type = EVT_TWEAK_L + (event->type - LEFTMOUSE);
          event_test_tweak.val = KM_ANY;

          LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
            if ((kmi->flag & KMI_INACTIVE) == 0) {
              if (wm_eventmatch(&event_test_click, kmi) ||
                  wm_eventmatch(&event_test_click_drag, kmi) ||
                  wm_eventmatch(&event_test_tweak, kmi)) {
                wmOperatorType *ot = WM_operatortype_find(kmi->idname, 0);
                if (WM_operator_poll_context(C, ot, WM_OP_INVOKE_DEFAULT)) {
                  is_event_handle_all = true;
                  break;
                }
              }
            }
          }
        }
      }
#endif /* USE_GIZMO_MOUSE_PRIORITY_HACK */
    }

    /* Don't use from now on. */
    gz = NULL;

    /* Fallback to selected gizmo (when un-handled). */
    if ((action & WM_HANDLER_BREAK) == 0) {
      if (WM_gizmomap_is_any_selected(gzmap)) {
        const ListBase *groups = WM_gizmomap_group_list(gzmap);
        LISTBASE_FOREACH (wmGizmoGroup *, gzgroup, groups) {
          if (wm_gizmogroup_is_any_selected(gzgroup)) {
            wmKeyMap *keymap = WM_keymap_active(wm, gzgroup->type->keymap);
            action |= wm_handlers_do_keymap_with_gizmo_handler(
                C, event, handlers, handler, gzgroup, keymap, do_debug_handler, NULL);
            if (action & WM_HANDLER_BREAK) {
              break;
            }
          }
        }
      }
    }
  }

  if (is_event_handle_all) {
    if (action == WM_HANDLER_CONTINUE) {
      action |= WM_HANDLER_BREAK | WM_HANDLER_MODAL;
    }
  }

  /* restore the area */
  CTX_wm_area_set(C, area);
  CTX_wm_region_set(C, region);

  return action;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Handle Single Event (All Handler Types)
 * \{ */

static int wm_handlers_do_intern(bContext *C, wmEvent *event, ListBase *handlers)
{
  const bool do_debug_handler =
      (G.debug & G_DEBUG_HANDLERS) &&
      /* Comment this out to flood the console! (if you really want to test). */
      !ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE);

  wmWindowManager *wm = CTX_wm_manager(C);
  int action = WM_HANDLER_CONTINUE;
  int always_pass;

  if (handlers == NULL) {
    return action;
  }

  /* Modal handlers can get removed in this loop, we keep the loop this way.
   *
   * Note: check 'handlers->first' because in rare cases the handlers can be cleared
   * by the event that's called, for eg:
   *
   * Calling a python script which changes the area.type, see T32232. */
  for (wmEventHandler *handler_base = handlers->first, *handler_base_next;
       handler_base && handlers->first;
       handler_base = handler_base_next) {
    handler_base_next = handler_base->next;

    /* During this loop, UI handlers for nested menus can tag multiple handlers free. */
    if (handler_base->flag & WM_HANDLER_DO_FREE) {
      /* Pass. */
    }
    else if (handler_base->poll == NULL || handler_base->poll(CTX_wm_region(C), event)) {
      /* In advance to avoid access to freed event on window close. */
      always_pass = wm_event_always_pass(event);

      /* Modal+blocking handler_base. */
      if (handler_base->flag & WM_HANDLER_BLOCKING) {
        action |= WM_HANDLER_BREAK;
      }

      /* Handle all types here. */
      if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
        wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
        wmKeyMap *keymap = WM_event_get_keymap_from_handler(wm, handler);
        action |= wm_handlers_do_keymap_with_keymap_handler(
            C, event, handlers, handler, keymap, do_debug_handler);

        /* Clear the tool-tip whenever a key binding is handled, without this tool-tips
         * are kept when a modal operators starts (annoying but otherwise harmless). */
        if (action & WM_HANDLER_BREAK) {
          /* Window may be gone after file read. */
          if (CTX_wm_window(C) != NULL) {
            WM_tooltip_clear(C, CTX_wm_window(C));
          }
        }
      }
      else if (handler_base->type == WM_HANDLER_TYPE_UI) {
        wmEventHandler_UI *handler = (wmEventHandler_UI *)handler_base;
        BLI_assert(handler->handle_fn != NULL);
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
              LISTBASE_FOREACH (wmDrag *, drag, lb) {
                const char *tooltip = NULL;
                if (drop->poll(C, drag, event, &tooltip)) {
                  /* Optionally copy drag information to operator properties. Don't call it if the
                   * operator fails anyway, it might do more than just set properties (e.g.
                   * typically import an asset). */
                  if (drop->copy && WM_operator_poll_context(C, drop->ot, drop->opcontext)) {
                    drop->copy(drag, drop);
                  }

                  /* Pass single matched wmDrag onto the operator. */
                  BLI_remlink(lb, drag);
                  ListBase single_lb = {drag, drag};
                  event->customdata = &single_lb;

                  int op_retval = wm_operator_call_internal(
                      C, drop->ot, drop->ptr, NULL, drop->opcontext, false, event);
                  OPERATOR_RETVAL_CHECK(op_retval);

                  if ((op_retval & OPERATOR_CANCELLED) && drop->cancel) {
                    drop->cancel(CTX_data_main(C), drag, drop);
                  }

                  action |= WM_HANDLER_BREAK;

                  /* Free the drags. */
                  WM_drag_free_list(lb);
                  WM_drag_free_list(&single_lb);

                  event->customdata = NULL;
                  event->custom = 0;

                  /* XXX fileread case. */
                  if (CTX_wm_window(C) == NULL) {
                    return action;
                  }

                  /* Escape from drag loop, got freed. */
                  break;
                }
              }
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
          action |= wm_handler_operator_call(C, handlers, handler_base, event, NULL, NULL);
        }
      }
      else {
        /* Unreachable (handle all types above). */
        BLI_assert(0);
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

    /* XXX fileread case, if the wm is freed then the handler's
     * will have been too so the code below need not run. */
    if (CTX_wm_window(C) == NULL) {
      return action;
    }

    /* XXX code this for all modal ops, and ensure free only happens here. */

    /* Modal UI handler can be tagged to be freed. */
    if (BLI_findindex(handlers, handler_base) !=
        -1) { /* Could be freed already by regular modal ops. */
      if (handler_base->flag & WM_HANDLER_DO_FREE) {
        BLI_remlink(handlers, handler_base);
        wm_event_free_handler(handler_base);
      }
    }
  }

  if (action == (WM_HANDLER_BREAK | WM_HANDLER_MODAL)) {
    wm_cursor_arrow_move(CTX_wm_window(C), event);
  }

  return action;
}

#undef PRINT

/* This calls handlers twice - to solve (double-)click events. */
static int wm_handlers_do(bContext *C, wmEvent *event, ListBase *handlers)
{
  int action = wm_handlers_do_intern(C, event, handlers);

  /* Will be NULL in the file read case. */
  wmWindow *win = CTX_wm_window(C);
  if (win == NULL) {
    return action;
  }

  if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
    /* Test for CLICK_DRAG events. */
    if (wm_action_not_handled(action)) {
      if (win->event_queue_check_drag) {
        if (WM_event_drag_test(event, &event->prevclickx)) {
          int x = event->x;
          int y = event->y;
          short val = event->val;
          short type = event->type;

          event->x = event->prevclickx;
          event->y = event->prevclicky;
          event->val = KM_CLICK_DRAG;
          event->type = event->prevtype;

          CLOG_INFO(WM_LOG_HANDLERS, 1, "handling PRESS_DRAG");

          action |= wm_handlers_do_intern(C, event, handlers);

          event->val = val;
          event->type = type;
          event->x = x;
          event->y = y;

          win->event_queue_check_click = false;
          if (!wm_action_not_handled(action)) {
            /* Only disable when handled as other handlers may use this drag event. */
            win->event_queue_check_drag = false;
          }
        }
      }
    }
    else {
      win->event_queue_check_drag = false;
    }
  }
  else if (ISMOUSE_BUTTON(event->type) || ISKEYBOARD(event->type)) {
    /* All events that don't set wmEvent.prevtype must be ignored. */

    /* Test for CLICK events. */
    if (wm_action_not_handled(action)) {
      /* eventstate stores if previous event was a KM_PRESS, in case that
       * wasn't handled, the KM_RELEASE will become a KM_CLICK */

      if (event->val == KM_PRESS) {
        if (event->is_repeat == false) {
          win->event_queue_check_click = true;
          win->event_queue_check_drag = true;
        }
      }
      else if (event->val == KM_RELEASE) {
        win->event_queue_check_drag = false;
      }

      if (event->prevtype == event->type) {

        if (event->val == KM_RELEASE) {
          if (event->prevval == KM_PRESS) {
            if (win->event_queue_check_click == true) {
              if (WM_event_drag_test(event, &event->prevclickx)) {
                win->event_queue_check_click = false;
                win->event_queue_check_drag = false;
              }
              else {
                /* Position is where the actual click happens, for more
                 * accurate selecting in case the mouse drifts a little. */
                int x = event->x;
                int y = event->y;

                event->x = event->prevclickx;
                event->y = event->prevclicky;
                event->val = KM_CLICK;

                CLOG_INFO(WM_LOG_HANDLERS, 1, "handling CLICK");

                action |= wm_handlers_do_intern(C, event, handlers);

                event->val = KM_RELEASE;
                event->x = x;
                event->y = y;
              }
            }
          }
        }
        else if (event->val == KM_DBL_CLICK) {
          /* The underlying event is a press, so try and handle this. */
          event->val = KM_PRESS;
          action |= wm_handlers_do_intern(C, event, handlers);

          /* revert value if not handled */
          if (wm_action_not_handled(action)) {
            event->val = KM_DBL_CLICK;
          }
        }
      }
    }
    else {
      win->event_queue_check_click = false;
      win->event_queue_check_drag = false;
    }
  }
  else if (ISMOUSE_WHEEL(event->type) || ISMOUSE_GESTURE(event->type)) {
    /* Modifiers which can trigger click event's,
     * however we don't want this if the mouse wheel has been used, see T74607. */
    if (wm_action_not_handled(action)) {
      /* pass */
    }
    else {
      if (ISKEYMODIFIER(event->prevtype)) {
        win->event_queue_check_click = false;
      }
    }
  }

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
  if (BLI_rcti_isect_pt_v(rect, &event->x)) {
    return true;
  }
  return false;
}

static bool wm_event_inside_region(const wmEvent *event, const ARegion *region)
{
  if (wm_event_always_pass(event)) {
    return true;
  }
  return ED_region_contains_xy(region, &event->x);
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
  return NULL;
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
  return NULL;
}

static void wm_paintcursor_tag(bContext *C, wmPaintCursor *pc, ARegion *region)
{
  if (region) {
    for (; pc; pc = pc->next) {
      if (pc->poll == NULL || pc->poll(C)) {
        wmWindow *win = CTX_wm_window(C);
        WM_paint_cursor_tag_redraw(win, region);
      }
    }
  }
}

/* Called on mousemove, check updates for paintcursors. */
/* Context was set on active area and region. */
static void wm_paintcursor_test(bContext *C, const wmEvent *event)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  if (wm->paintcursors.first) {
    ARegion *region = CTX_wm_region(C);

    if (region) {
      wm_paintcursor_tag(C, wm->paintcursors.first, region);
    }

    /* If previous position was not in current region, we have to set a temp new context. */
    if (region == NULL || !BLI_rcti_isect_pt_v(&region->winrct, &event->prevx)) {
      ScrArea *area = CTX_wm_area(C);

      CTX_wm_area_set(C, area_event_inside(C, &event->prevx));
      CTX_wm_region_set(C, region_event_inside(C, &event->prevx));

      wm_paintcursor_tag(C, wm->paintcursors.first, CTX_wm_region(C));

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
    WM_drag_free_list(&wm->drags);

    screen->do_draw_drag = true;
  }
  else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    event->type = EVT_DROP;

    /* Create customdata, first free existing. */
    if (event->customdata) {
      if (event->customdatafree) {
        MEM_freeN(event->customdata);
      }
    }

    event->custom = EVT_DATA_DRAGDROP;
    event->customdata = &wm->drags;
    event->customdatafree = 1;

    /* Clear drop icon. */
    screen->do_draw_drag = true;

    /* restore cursor (disabled, see wm_dragdrop.c) */
    // WM_cursor_modal_restore(win);
  }
}

/* Filter out all events of the pie that spawned the last pie unless it's a release event. */
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

/* Called in main loop. */
/* Goes over entire hierarchy:  events -> window -> screen -> area -> region. */
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

    if (screen == NULL) {
      wm_event_free_all(win);
    }
    else {
      Scene *scene = WM_window_get_active_scene(win);
      ViewLayer *view_layer = WM_window_get_active_view_layer(win);
      Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer);
      Scene *scene_eval = (depsgraph != NULL) ? DEG_get_evaluated_scene(depsgraph) : NULL;

      if (scene_eval != NULL) {
        const int is_playing_sound = BKE_sound_scene_playing(scene_eval);

        if (scene_eval->id.recalc & ID_RECALC_AUDIO_SEEK) {
          /* Ignore seek here, the audio will be updated to the scene frame after jump during next
           * dependency graph update. */
        }
        else if (is_playing_sound != -1) {
          bool is_playing_screen;

          is_playing_screen = (ED_screen_animation_playing(wm) != NULL);

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
                WM_event_add_notifier(C, NC_WINDOW, NULL);
              }
            }
          }
        }
      }
    }

    wmEvent *event;
    while ((event = win->event_queue.first)) {
      int action = WM_HANDLER_CONTINUE;

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
        wm_event_free(event);
        continue;
      }

      CTX_wm_window_set(C, win);

      /* Clear tool-tip on mouse move. */
      if (screen->tool_tip && screen->tool_tip->exit_on_event) {
        if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
          if (len_manhattan_v2v2_int(screen->tool_tip->event_xy, &event->x) > U.move_threshold) {
            WM_tooltip_clear(C, win);
          }
        }
      }

      /* We let modal handlers get active area/region, also wm_paintcursor_test needs it. */
      CTX_wm_area_set(C, area_event_inside(C, &event->x));
      CTX_wm_region_set(C, region_event_inside(C, &event->x));

      /* MVC demands to not draw in event handlers...
       * but we need to leave it for ogl selecting etc. */
      wm_window_make_drawable(wm, win);

      wm_region_mouse_co(C, event);

      /* First we do priority handlers, modal + some limited keymaps. */
      action |= wm_handlers_do(C, event, &win->modalhandlers);

      /* Fileread case. */
      if (CTX_wm_window(C) == NULL) {
        wm_event_free_and_remove_from_queue_if_valid(event);
        return;
      }

      /* Check for a tooltip. */
      if (screen == WM_window_get_active_screen(win)) {
        if (screen->tool_tip && screen->tool_tip->timer) {
          if ((event->type == TIMER) && (event->customdata == screen->tool_tip->timer)) {
            WM_tooltip_init(C, win);
          }
        }
      }

      /* Check dragging, creates new event or frees, adds draw tag. */
      wm_event_drag_and_drop_test(wm, win, event);

      /* Builtin tweak, if action is break it removes tweak. */
      wm_tweakevent_test(C, event, action);

      if ((action & WM_HANDLER_BREAK) == 0) {
        /* Note: setting subwin active should be done here, after modal handlers have been done */
        if (event->type == MOUSEMOVE) {
          /* State variables in screen, cursors.
           * Also used in wm_draw.c, fails for modal handlers though. */
          ED_screen_set_active_region(C, win, &event->x);
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
          if (screen->skip_handling == true) {
            /* Restore for the next iteration of wm_event_do_handlers. */
            screen->skip_handling = false;
            break;
          }

          /* Update azones if needed - done here because it needs to be independent from redraws.
           */
          if (area->flag & AREA_FLAG_ACTIONZONES_UPDATE) {
            ED_area_azones_update(area, &event->x);
          }

          if (wm_event_inside_rect(event, &area->totrct)) {
            CTX_wm_area_set(C, area);

            if ((action & WM_HANDLER_BREAK) == 0) {
              LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
                if (wm_event_inside_region(event, region)) {

                  CTX_wm_region_set(C, region);

                  /* Call even on non mouse events, since the */
                  wm_region_mouse_co(C, event);

                  if (!BLI_listbase_is_empty(&wm->drags)) {
                    /* Does polls for drop regions and checks #uiButs. */
                    /* Need to be here to make sure region context is true. */
                    if (ELEM(event->type, MOUSEMOVE, EVT_DROP) || ISKEYMODIFIER(event->type)) {
                      wm_drags_check_ops(C, event);
                    }
                  }

                  action |= wm_handlers_do(C, event, &region->handlers);

                  /* Fileread case (python), T29489. */
                  if (CTX_wm_window(C) == NULL) {
                    wm_event_free_and_remove_from_queue_if_valid(event);
                    return;
                  }

                  if (action & WM_HANDLER_BREAK) {
                    break;
                  }
                }
              }
            }

            CTX_wm_region_set(C, NULL);

            if ((action & WM_HANDLER_BREAK) == 0) {
              wm_region_mouse_co(C, event); /* Only invalidates event->mval in this case. */
              action |= wm_handlers_do(C, event, &area->handlers);
            }
            CTX_wm_area_set(C, NULL);

            /* NOTE: do not escape on WM_HANDLER_BREAK,
             * mousemove needs handled for previous area. */
          }
        }

        if ((action & WM_HANDLER_BREAK) == 0) {
          /* Also some non-modal handlers need active area/region. */
          CTX_wm_area_set(C, area_event_inside(C, &event->x));
          CTX_wm_region_set(C, region_event_inside(C, &event->x));

          wm_region_mouse_co(C, event);

          action |= wm_handlers_do(C, event, &win->handlers);

          /* Fileread case. */
          if (CTX_wm_window(C) == NULL) {
            wm_event_free_and_remove_from_queue_if_valid(event);
            return;
          }
        }
      }

      /* If press was handled, we don't want to do click. This way
       * press in tool keymap can override click in editor keymap.*/
      if (ISMOUSE_BUTTON(event->type) && event->val == KM_PRESS &&
          !wm_action_not_handled(action)) {
        win->event_queue_check_click = false;
      }

      /* Update previous mouse position for following events to use. */
      win->eventstate->prevx = event->x;
      win->eventstate->prevy = event->y;

      /* Unlink and free here, blender-quit then frees all. */
      BLI_remlink(&win->event_queue, event);
      wm_event_free(event);
    }

    /* Only add mouse-move when the event queue was read entirely. */
    if (win->addmousemove && win->eventstate) {
      wmEvent tevent = *(win->eventstate);
      // printf("adding MOUSEMOVE %d %d\n", tevent.x, tevent.y);
      tevent.type = MOUSEMOVE;
      tevent.prevx = tevent.x;
      tevent.prevy = tevent.y;
      tevent.is_repeat = false;
      wm_event_add(win, &tevent);
      win->addmousemove = 0;
    }

    CTX_wm_window_set(C, NULL);
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
    event.customdata = ophandle; /* Only as void pointer type check. */

    wm_event_add(win, &event);
  }
}

/* Operator is supposed to have a filled "path" property. */
/* Optional property: filetype (XXX enum?) */

/**
 * The idea here is to keep a handler alive on window queue, owning the operator.
 * The file window can send event to make it execute, thus ensuring
 * executing happens outside of lower level queues, with UI refreshed.
 * Should also allow multiwin solutions.
 */
void WM_event_add_fileselect(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  const bool is_temp_screen = WM_window_is_temp_screen(win);

  /* Close any popups, like when opening a file browser from the splash. */
  UI_popup_handlers_remove_all(C, &win->modalhandlers);

  if (!is_temp_screen) {
    /* Only allow 1 file selector open per window. */
    LISTBASE_FOREACH_MUTABLE (wmEventHandler *, handler_base, &win->modalhandlers) {
      if (handler_base->type == WM_HANDLER_TYPE_OP) {
        wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
        if (handler->is_fileselect == false) {
          continue;
        }

        ScrArea *file_area = ED_fileselect_handler_area_find(win, handler->op);

        if (file_area) {
          CTX_wm_area_set(C, file_area);
          wm_handler_fileselect_do(C, &win->modalhandlers, handler, EVT_FILESELECT_CANCEL);
        }
        /* If not found we stop the handler without changing the screen. */
        else {
          wm_handler_fileselect_do(
              C, &win->modalhandlers, handler, EVT_FILESELECT_EXTERNAL_CANCEL);
        }
      }
    }
  }

  wmEventHandler_Op *handler = MEM_callocN(sizeof(*handler), __func__);
  handler->head.type = WM_HANDLER_TYPE_OP;

  handler->is_fileselect = true;
  handler->op = op;
  handler->context.win = CTX_wm_window(C);
  handler->context.area = CTX_wm_area(C);
  handler->context.region = CTX_wm_region(C);

  BLI_addhead(&win->modalhandlers, handler);

  /* Check props once before invoking if check is available
   * ensures initial properties are valid. */
  if (op->type->check) {
    op->type->check(C, op); /* Ignore return value. */
  }

  WM_event_fileselect_event(wm, op, EVT_FILESELECT_FULL_OPEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modal Operator Handling
 * \{ */

#if 0
/* lets not expose struct outside wm? */
static void WM_event_set_handler_flag(wmEventHandler *handler, int flag)
{
  handler->flag = flag;
}
#endif

wmEventHandler_Op *WM_event_add_modal_handler(bContext *C, wmOperator *op)
{
  wmEventHandler_Op *handler = MEM_callocN(sizeof(*handler), __func__);
  handler->head.type = WM_HANDLER_TYPE_OP;
  wmWindow *win = CTX_wm_window(C);

  /* Operator was part of macro. */
  if (op->opm) {
    /* Give the mother macro to the handler. */
    handler->op = op->opm;
    /* Mother macro opm becomes the macro element. */
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

/**
 * Modal handlers store a pointer to an area which might be freed while the handler runs.
 * Use this function to NULL all handler pointers to \a old_area.
 */
void WM_event_modal_handler_area_replace(wmWindow *win, const ScrArea *old_area, ScrArea *new_area)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
      /* Fileselect handler is quite special...
       * it needs to keep old area stored in handler, so don't change it. */
      if ((handler->context.area == old_area) && (handler->is_fileselect == false)) {
        handler->context.area = new_area;
      }
    }
  }
}

/**
 * Modal handlers store a pointer to a region which might be freed while the handler runs.
 * Use this function to NULL all handler pointers to \a old_region.
 */
void WM_event_modal_handler_region_replace(wmWindow *win,
                                           const ARegion *old_region,
                                           ARegion *new_region)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
      /* Fileselect handler is quite special...
       * it needs to keep old region stored in handler, so don't change it. */
      if ((handler->context.region == old_region) && (handler->is_fileselect == false)) {
        handler->context.region = new_region;
        handler->context.region_type = new_region ? new_region->regiontype : RGN_TYPE_WINDOW;
      }
    }
  }
}

wmEventHandler_Keymap *WM_event_add_keymap_handler(ListBase *handlers, wmKeyMap *keymap)
{
  if (!keymap) {
    CLOG_WARN(WM_LOG_HANDLERS, "called with NULL keymap");
    return NULL;
  }

  /* Only allow same keymap once. */
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
      wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
      if (handler->keymap == keymap) {
        return handler;
      }
    }
  }

  wmEventHandler_Keymap *handler = MEM_callocN(sizeof(*handler), __func__);
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
wmKeyMap *WM_event_get_keymap_from_toolsystem_fallback(wmWindowManager *wm,
                                                       wmEventHandler_Keymap *handler)
{
  ScrArea *area = handler->dynamic.user_data;
  handler->keymap_tool = NULL;
  bToolRef_Runtime *tref_rt = area->runtime.tool ? area->runtime.tool->runtime : NULL;
  if (tref_rt && tref_rt->keymap_fallback[0]) {
    const char *keymap_id = NULL;

    /* Support for the gizmo owning the tool keymap. */
    if (tref_rt->gizmo_group[0] != '\0' && tref_rt->keymap_fallback[0] != '\n') {
      wmGizmoMap *gzmap = NULL;
      wmGizmoGroup *gzgroup = NULL;
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        if (region->gizmo_map != NULL) {
          gzmap = region->gizmo_map;
          gzgroup = WM_gizmomap_group_find(gzmap, tref_rt->gizmo_group);
          if (gzgroup != NULL) {
            break;
          }
        }
      }
      if (gzgroup != NULL) {
        if (gzgroup->type->flag & WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP) {
          /* If all are hidden, don't override. */
          if (gzgroup->use_fallback_keymap) {
            wmGizmo *highlight = wm_gizmomap_highlight_get(gzmap);
            if (highlight == NULL) {
              keymap_id = tref_rt->keymap_fallback;
            }
          }
        }
      }
    }

    if (keymap_id && keymap_id[0]) {
      wmKeyMap *km = WM_keymap_list_find_spaceid_or_empty(
          &wm->userconf->keymaps, keymap_id, area->spacetype, RGN_TYPE_WINDOW);
      /* We shouldn't use keymaps from unrelated spaces. */
      if (km != NULL) {
        handler->keymap_tool = area->runtime.tool;
        return km;
      }
      printf(
          "Keymap: '%s' not found for tool '%s'\n", tref_rt->keymap, area->runtime.tool->idname);
    }
  }
  return NULL;
}

wmKeyMap *WM_event_get_keymap_from_toolsystem(wmWindowManager *wm, wmEventHandler_Keymap *handler)
{
  ScrArea *area = handler->dynamic.user_data;
  handler->keymap_tool = NULL;
  bToolRef_Runtime *tref_rt = area->runtime.tool ? area->runtime.tool->runtime : NULL;
  if (tref_rt && tref_rt->keymap[0]) {
    const char *keymap_id = tref_rt->keymap;
    {
      wmKeyMap *km = WM_keymap_list_find_spaceid_or_empty(
          &wm->userconf->keymaps, keymap_id, area->spacetype, RGN_TYPE_WINDOW);
      /* We shouldn't use keymaps from unrelated spaces. */
      if (km != NULL) {
        handler->keymap_tool = area->runtime.tool;
        return km;
      }
      printf(
          "Keymap: '%s' not found for tool '%s'\n", tref_rt->keymap, area->runtime.tool->idname);
    }
  }
  return NULL;
}

struct wmEventHandler_Keymap *WM_event_add_keymap_handler_dynamic(
    ListBase *handlers, wmEventHandler_KeymapDynamicFn *keymap_fn, void *user_data)
{
  if (!keymap_fn) {
    CLOG_WARN(WM_LOG_HANDLERS, "called with NULL keymap_fn");
    return NULL;
  }

  /* Only allow same keymap once. */
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

  wmEventHandler_Keymap *handler = MEM_callocN(sizeof(*handler), __func__);
  handler->head.type = WM_HANDLER_TYPE_KEYMAP;
  BLI_addtail(handlers, handler);
  handler->dynamic.keymap_fn = keymap_fn;
  handler->dynamic.user_data = user_data;

  return handler;
}

/* Priorities not implemented yet, for time being just insert in begin of list. */
wmEventHandler_Keymap *WM_event_add_keymap_handler_priority(ListBase *handlers,
                                                            wmKeyMap *keymap,
                                                            int UNUSED(priority))
{
  WM_event_remove_keymap_handler(handlers, keymap);

  wmEventHandler_Keymap *handler = MEM_callocN(sizeof(*handler), "event keymap handler");
  handler->head.type = WM_HANDLER_TYPE_KEYMAP;

  BLI_addhead(handlers, handler);
  handler->keymap = keymap;

  return handler;
}

static bool event_or_prev_in_rect(const wmEvent *event, const rcti *rect)
{
  if (BLI_rcti_isect_pt(rect, event->x, event->y)) {
    return true;
  }
  if (event->type == MOUSEMOVE && BLI_rcti_isect_pt(rect, event->prevx, event->prevy)) {
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
  if (handler == NULL) {
    return NULL;
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
  wmEventHandler_UI *handler = MEM_callocN(sizeof(*handler), __func__);
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
    handler->context.area = NULL;
    handler->context.region = NULL;
    handler->context.menu = NULL;
  }

  BLI_assert((flag & WM_HANDLER_DO_FREE) == 0);
  handler->head.flag = flag;

  BLI_addhead(handlers, handler);

  return handler;
}

/* Set "postpone" for win->modalhandlers, this is in a running for () loop in wm_handlers_do(). */
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
        /* Handlers will be freed in wm_handlers_do(). */
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

  wmEventHandler_Dropbox *handler = MEM_callocN(sizeof(*handler), __func__);
  handler->head.type = WM_HANDLER_TYPE_DROPBOX;

  /* Dropbox stored static, no free or copy. */
  handler->dropboxes = dropboxes;
  BLI_addhead(handlers, handler);

  return handler;
}

/* XXX solution works, still better check the real cause (ton) */
void WM_event_remove_area_handler(ListBase *handlers, void *area)
{
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
      return 0;
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
      return 0;
    case GHOST_kKeyScrollLock:
      return 0;

    case GHOST_kKeyLeftArrow:
      return EVT_LEFTARROWKEY;
    case GHOST_kKeyRightArrow:
      return EVT_RIGHTARROWKEY;
    case GHOST_kKeyUpArrow:
      return EVT_UPARROWKEY;
    case GHOST_kKeyDownArrow:
      return EVT_DOWNARROWKEY;

    case GHOST_kKeyPrintScreen:
      return 0;
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

    default:
      return EVT_UNKNOWNKEY; /* GHOST_kKeyUnknown */
  }
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
      short *mod = (
#if !defined(WIN32)
          (U.mouse_emulate_3_button_modifier == USER_EMU_MMB_MOD_OSKEY) ? &event->oskey :
                                                                          &event->alt
#else
          /* Disable for WIN32 for now because it accesses the start menu. */
          &event->alt
#endif
      );

      if (event->val == KM_PRESS) {
        if (*mod) {
          *mod = 0;
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
          *mod = 0;
        }

        if (!test_only) {
          emulating_event = EVENT_NONE;
        }
      }
    }
  }

  /* Numpad emulation. */
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

static const wmTabletData wm_event_tablet_data_default = {
    .active = EVT_TABLET_NONE,
    .pressure = 1.0f,
    .x_tilt = 0.0f,
    .y_tilt = 0.0f,
    .is_motion_absolute = false,
};

void WM_event_tablet_data_default_set(wmTabletData *tablet_data)
{
  *tablet_data = wm_event_tablet_data_default;
}

void wm_tablet_data_from_ghost(const GHOST_TabletData *tablet_data, wmTabletData *wmtab)
{
  if ((tablet_data != NULL) && tablet_data->Active != GHOST_kTabletModeNone) {
    wmtab->active = (int)tablet_data->Active;
    wmtab->pressure = wm_pressure_curve(tablet_data->Pressure);
    wmtab->x_tilt = tablet_data->Xtilt;
    wmtab->y_tilt = tablet_data->Ytilt;
    /* We could have a preference to support relative tablet motion (we can't detect that). */
    wmtab->is_motion_absolute = true;
    // printf("%s: using tablet %.5f\n", __func__, wmtab->pressure);
  }
  else {
    *wmtab = wm_event_tablet_data_default;
    // printf("%s: not using tablet\n", __func__);
  }
}

#ifdef WITH_INPUT_NDOF
/* Adds customdata to event. */
static void attach_ndof_data(wmEvent *event, const GHOST_TEventNDOFMotionData *ghost)
{
  wmNDOFMotionData *data = MEM_mallocN(sizeof(wmNDOFMotionData), "customdata NDOF");

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
  event->customdatafree = 1;
}
#endif /* WITH_INPUT_NDOF */

/* Imperfect but probably usable... draw/enable drags to other windows. */
static wmWindow *wm_event_cursor_other_windows(wmWindowManager *wm, wmWindow *win, wmEvent *event)
{
  int mval[2] = {event->x, event->y};

  if (wm->windows.first == wm->windows.last) {
    return NULL;
  }

  /* In order to use window size and mouse position (pixels), we have to use a WM function. */

  /* check if outside, include top window bar... */
  if (mval[0] < 0 || mval[1] < 0 || mval[0] > WM_window_pixels_x(win) ||
      mval[1] > WM_window_pixels_y(win) + 30) {
    /* Let's skip windows having modal handlers now */
    /* potential XXX ugly... I wouldn't have added a modalhandlers list
     * (introduced in rev 23331, ton). */
    LISTBASE_FOREACH (wmEventHandler *, handler, &win->modalhandlers) {
      if (ELEM(handler->type, WM_HANDLER_TYPE_UI, WM_HANDLER_TYPE_OP)) {
        return NULL;
      }
    }

    wmWindow *win_other;
    if (WM_window_find_under_cursor(wm, win, win, mval, &win_other, mval)) {
      event->x = mval[0];
      event->y = mval[1];
      return win_other;
    }
  }
  return NULL;
}

static bool wm_event_is_double_click(const wmEvent *event)
{
  if ((event->type == event->prevtype) && (event->prevval == KM_RELEASE) &&
      (event->val == KM_PRESS)) {
    if (ISMOUSE(event->type) && WM_event_drag_test(event, &event->prevclickx)) {
      /* Pass. */
    }
    else {
      if ((PIL_check_seconds_timer() - event->prevclicktime) * 1000 < U.dbl_click_time) {
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
  event->prevval = event_state->prevval = event_state->val;
  event->prevtype = event_state->prevtype = event_state->type;
}

static void wm_event_prev_click_set(wmEvent *event, wmEvent *event_state)
{
  event->prevclicktime = event_state->prevclicktime = PIL_check_seconds_timer();
  event->prevclickx = event_state->prevclickx = event_state->x;
  event->prevclicky = event_state->prevclicky = event_state->y;
}

static wmEvent *wm_event_add_mousemove(wmWindow *win, const wmEvent *event)
{
  wmEvent *event_last = win->event_queue.last;

  /* Some painting operators want accurate mouse events, they can
   * handle in between mouse move moves, others can happily ignore
   * them for better performance. */
  if (event_last && event_last->type == MOUSEMOVE) {
    event_last->type = INBETWEEN_MOUSEMOVE;
  }

  wmEvent *event_new = wm_event_add(win, event);
  if (event_last == NULL) {
    event_last = win->eventstate;
  }

  copy_v2_v2_int(&event_new->prevx, &event_last->x);
  return event_new;
}

static wmEvent *wm_event_add_trackpad(wmWindow *win, const wmEvent *event, int deltax, int deltay)
{
  /* Ignore in between trackpad events for performance, we only need high accuracy
   * for painting with mouse moves, for navigation using the accumulated value is ok. */
  wmEvent *event_last = win->event_queue.last;
  if (event_last && event_last->type == event->type) {
    deltax += event_last->x - event_last->prevx;
    deltay += event_last->y - event_last->prevy;

    wm_event_free_last(win);
  }

  /* Set prevx/prevy, the delta is computed from this in operators. */
  wmEvent *event_new = wm_event_add(win, event);
  event_new->prevx = event_new->x - deltax;
  event_new->prevy = event_new->y - deltay;

  return event_new;
}

/**
 * Windows store own event queues #wmWindow.event_queue (no #bContext here).
 */
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
  event.is_repeat = false;

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
  event.prevtype = event.type;
  event.prevval = event.val;

  /* Ensure the event state is correct, any deviation from this may cause bugs. */
#ifndef NDEBUG
  if ((event_state->type || event_state->val) && /* Ignore cleared event state. */
      !(ISMOUSE_BUTTON(event_state->type) || ISKEYBOARD(event_state->type))) {
    CLOG_WARN(WM_LOG_HANDLERS,
              "Non-keyboard/mouse button found in 'win->eventstate->type = %d'",
              event_state->type);
  }
  if ((event_state->prevtype || event_state->prevval) && /* Ignore cleared event state. */
      !(ISMOUSE_BUTTON(event_state->prevtype) || ISKEYBOARD(event_state->prevtype))) {
    CLOG_WARN(WM_LOG_HANDLERS,
              "Non-keyboard/mouse button found in 'win->eventstate->prevtype = %d'",
              event_state->prevtype);
  }
#endif

  switch (type) {
    /* Mouse move, also to inactive window (X11 does this). */
    case GHOST_kEventCursorMove: {
      GHOST_TEventCursorData *cd = customdata;

      copy_v2_v2_int(&event.x, &cd->x);
      wm_stereo3d_mouse_offset_apply(win, &event.x);
      wm_tablet_data_from_ghost(&cd->tablet, &event.tablet);

      event.type = MOUSEMOVE;
      {
        wmEvent *event_new = wm_event_add_mousemove(win, &event);
        copy_v2_v2_int(&event_state->x, &event_new->x);
        event_state->tablet.is_motion_absolute = event_new->tablet.is_motion_absolute;
      }

      /* Also add to other window if event is there, this makes overdraws disappear nicely. */
      /* It remaps mousecoord to other window in event. */
      wmWindow *win_other = wm_event_cursor_other_windows(wm, win, &event);
      if (win_other) {
        wmEvent event_other = *win_other->eventstate;

        /* See comment for this operation on `event` for details. */
        event_other.prevtype = event_other.type;
        event_other.prevval = event_other.val;

        copy_v2_v2_int(&event_other.x, &event.x);
        event_other.type = MOUSEMOVE;
        {
          wmEvent *event_new = wm_event_add_mousemove(win_other, &event_other);
          copy_v2_v2_int(&win_other->eventstate->x, &event_new->x);
          win_other->eventstate->tablet.is_motion_absolute = event_new->tablet.is_motion_absolute;
        }
      }

      break;
    }
    case GHOST_kEventTrackpad: {
      GHOST_TEventTrackpadData *pd = customdata;
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

      event.x = event_state->x = pd->x;
      event.y = event_state->y = pd->y;
      event.val = KM_NOTHING;

      /* The direction is inverted from the device due to system preferences. */
      event.is_direction_inverted = pd->isDirectionInverted;

      wm_event_add_trackpad(win, &event, pd->deltaX, -pd->deltaY);
      break;
    }
    /* Mouse button. */
    case GHOST_kEventButtonDown:
    case GHOST_kEventButtonUp: {
      GHOST_TEventButtonData *bd = customdata;

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
      wm_event_prev_values_set(&event, event_state);

      /* Copy to event state. */
      event_state->val = event.val;
      event_state->type = event.type;

      /* Double click test. */
      if (wm_event_is_double_click(&event)) {
        CLOG_INFO(WM_LOG_HANDLERS, 1, "Send double click");
        event.val = KM_DBL_CLICK;
      }
      if (event.val == KM_PRESS) {
        wm_event_prev_click_set(&event, event_state);
      }

      /* Add to other window if event is there (not to both!). */
      wmWindow *win_other = wm_event_cursor_other_windows(wm, win, &event);
      if (win_other) {
        wmEvent event_other = *win_other->eventstate;

        /* See comment for this operation on `event` for details. */
        event_other.prevtype = event_other.type;
        event_other.prevval = event_other.val;

        copy_v2_v2_int(&event_other.x, &event.x);

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
      GHOST_TEventKeyData *kd = customdata;
      short keymodifier = KM_NOTHING;
      event.type = convert_key(kd->key);
      event.ascii = kd->ascii;
      /* Might be not NULL terminated. */
      memcpy(event.utf8_buf, kd->utf8_buf, sizeof(event.utf8_buf));
      event.is_repeat = kd->is_repeat;
      event.val = (type == GHOST_kEventKeyDown) ? KM_PRESS : KM_RELEASE;

      wm_eventemulation(&event, false);
      wm_event_prev_values_set(&event, event_state);

      /* Copy to event state. */
      event_state->val = event.val;
      event_state->type = event.type;
      event_state->is_repeat = event.is_repeat;

      /* Exclude arrow keys, esc, etc from text input. */
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

      /* Assigning both first and second is strange. - campbell */
      switch (event.type) {
        case EVT_LEFTSHIFTKEY:
        case EVT_RIGHTSHIFTKEY:
          if (event.val == KM_PRESS) {
            if (event_state->ctrl || event_state->alt || event_state->oskey) {
              keymodifier = (KM_MOD_FIRST | KM_MOD_SECOND);
            }
            else {
              keymodifier = KM_MOD_FIRST;
            }
          }
          event.shift = event_state->shift = keymodifier;
          break;
        case EVT_LEFTCTRLKEY:
        case EVT_RIGHTCTRLKEY:
          if (event.val == KM_PRESS) {
            if (event_state->shift || event_state->alt || event_state->oskey) {
              keymodifier = (KM_MOD_FIRST | KM_MOD_SECOND);
            }
            else {
              keymodifier = KM_MOD_FIRST;
            }
          }
          event.ctrl = event_state->ctrl = keymodifier;
          break;
        case EVT_LEFTALTKEY:
        case EVT_RIGHTALTKEY:
          if (event.val == KM_PRESS) {
            if (event_state->ctrl || event_state->shift || event_state->oskey) {
              keymodifier = (KM_MOD_FIRST | KM_MOD_SECOND);
            }
            else {
              keymodifier = KM_MOD_FIRST;
            }
          }
          event.alt = event_state->alt = keymodifier;
          break;
        case EVT_OSKEY:
          if (event.val == KM_PRESS) {
            if (event_state->ctrl || event_state->alt || event_state->shift) {
              keymodifier = (KM_MOD_FIRST | KM_MOD_SECOND);
            }
            else {
              keymodifier = KM_MOD_FIRST;
            }
          }
          event.oskey = event_state->oskey = keymodifier;
          break;
        default:
          if (event.val == KM_PRESS && event.keymodifier == 0) {
            /* Only set in eventstate, for next event. */
            event_state->keymodifier = event.type;
          }
          else if (event.val == KM_RELEASE && event.keymodifier == event.type) {
            event.keymodifier = event_state->keymodifier = 0;
          }
          break;
      }

      /* Double click test. */
      /* If previous event was same type, and previous was release, and now it presses... */
      if (wm_event_is_double_click(&event)) {
        CLOG_INFO(WM_LOG_HANDLERS, 1, "Send double click");
        event.val = KM_DBL_CLICK;
      }

      /* This case happens on holding a key pressed, it should not generate
       * press events events with the same key as modifier. */
      if (event.keymodifier == event.type) {
        event.keymodifier = 0;
      }

      /* This case happens with an external numpad, and also when using 'dead keys'
       * (to compose complex latin characters e.g.), it's not really clear why.
       * Since it's impossible to map a key modifier to an unknown key,
       * it shouldn't harm to clear it. */
      if (event.keymodifier == EVT_UNKNOWNKEY) {
        event_state->keymodifier = event.keymodifier = 0;
      }

      /* If test_break set, it catches this. Do not set with modifier presses.
       * XXX Keep global for now? */
      if ((event.type == EVT_ESCKEY && event.val == KM_PRESS) &&
          /* Check other modifiers because ms-windows uses these to bring up the task manager. */
          (event.shift == 0 && event.ctrl == 0 && event.alt == 0)) {
        G.is_break = true;
      }

      /* Double click test - only for press. */
      if (event.val == KM_PRESS) {
        /* Don't reset timer & location when holding the key generates repeat events. */
        if (event.is_repeat == false) {
          wm_event_prev_click_set(&event, event_state);
        }
      }

      wm_event_add(win, &event);

      break;
    }

    case GHOST_kEventWheel: {
      GHOST_TEventWheelData *wheelData = customdata;

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
      attach_ndof_data(&event, customdata);
      wm_event_add(win, &event);

      CLOG_INFO(WM_LOG_HANDLERS, 1, "sending NDOF_MOTION, prev = %d %d", event.x, event.y);
      break;
    }

    case GHOST_kEventNDOFButton: {
      GHOST_TEventNDOFButtonData *e = customdata;

      event.type = NDOF_BUTTON_NONE + e->button;

      switch (e->action) {
        case GHOST_kPress:
          event.val = KM_PRESS;
          break;
        case GHOST_kRelease:
          event.val = KM_RELEASE;
          break;
      }

      event.custom = 0;
      event.customdata = NULL;

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
      win->ime_data = customdata;
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

  /* This will prevent drawing regions which uses non-threadsafe data.
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

wmKeyMap *WM_event_get_keymap_from_handler(wmWindowManager *wm, wmEventHandler_Keymap *handler)
{
  wmKeyMap *keymap;
  if (handler->dynamic.keymap_fn != NULL) {
    keymap = handler->dynamic.keymap_fn(wm, handler);
    BLI_assert(handler->keymap == NULL);
  }
  else {
    keymap = WM_keymap_active(wm, handler->keymap);
    BLI_assert(keymap != NULL);
  }
  return keymap;
}

wmKeyMapItem *WM_event_match_keymap_item(bContext *C, wmKeyMap *keymap, const wmEvent *event)
{
  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
    if (wm_eventmatch(event, kmi)) {
      wmOperatorType *ot = WM_operatortype_find(kmi->idname, 0);
      if (WM_operator_poll_context(C, ot, WM_OP_INVOKE_DEFAULT)) {
        return kmi;
      }
    }
  }
  return NULL;
}

wmKeyMapItem *WM_event_match_keymap_item_from_handlers(bContext *C,
                                                       wmWindowManager *wm,
                                                       ListBase *handlers,
                                                       const wmEvent *event)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    /* During this loop, UI handlers for nested menus can tag multiple handlers free. */
    if (handler_base->flag & WM_HANDLER_DO_FREE) {
      /* Pass. */
    }
    else if (handler_base->poll == NULL || handler_base->poll(CTX_wm_region(C), event)) {
      if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
        wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
        wmKeyMap *keymap = WM_event_get_keymap_from_handler(wm, handler);
        if (keymap && WM_keymap_poll(C, keymap)) {
          wmKeyMapItem *kmi = WM_event_match_keymap_item(C, keymap, event);
          if (kmi != NULL) {
            return kmi;
          }
        }
      }
    }
  }
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Keymap Status
 *
 * Show cursor keys in the status bar.
 * This is done by detecting changes to the state - full keymap lookups are expensive
 * so only perform this on changing tools, space types, pressing different modifier keys... etc.
 * \{ */

/** State storage to detect changes between calls to refresh the information. */
struct CursorKeymapInfo_State {
  struct {
    short shift, ctrl, alt, oskey;
  } modifiers;
  short space_type;
  short region_type;
  /* Never use, just compare memory for changes. */
  bToolRef tref;
};

struct CursorKeymapInfo {
  /* 0: mouse button index
   * 1: event type (click/press, drag)
   * 2: text.
   */
  char text[3][2][128];
  wmEvent state_event;
  struct CursorKeymapInfo_State state;
};

static void wm_event_cursor_store(struct CursorKeymapInfo_State *state,
                                  const wmEvent *event,
                                  short space_type,
                                  short region_type,
                                  const bToolRef *tref)
{
  state->modifiers.shift = event->shift;
  state->modifiers.ctrl = event->ctrl;
  state->modifiers.alt = event->alt;
  state->modifiers.oskey = event->oskey;
  state->space_type = space_type;
  state->region_type = region_type;
  state->tref = tref ? *tref : (bToolRef){0};
}

const char *WM_window_cursor_keymap_status_get(const wmWindow *win,
                                               int button_index,
                                               int type_index)
{
  if (win->cursor_keymap_status != NULL) {
    struct CursorKeymapInfo *cd = win->cursor_keymap_status;
    const char *msg = cd->text[button_index][type_index];
    if (*msg) {
      return msg;
    }
  }
  return NULL;
}

/**
 * Similar to #BKE_screen_area_map_find_area_xy and related functions,
 * use here since the area is stored in the window manager.
 */
ScrArea *WM_window_status_area_find(wmWindow *win, bScreen *screen)
{
  if (screen->state == SCREENFULL) {
    return NULL;
  }
  ScrArea *area_statusbar = NULL;
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
  if (area != NULL) {
    ED_area_tag_redraw(area);
  }
}

void WM_window_cursor_keymap_status_refresh(bContext *C, wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  ScrArea *area_statusbar = WM_window_status_area_find(win, screen);
  if (area_statusbar == NULL) {
    MEM_SAFE_FREE(win->cursor_keymap_status);
    return;
  }

  struct CursorKeymapInfo *cd;
  if (UNLIKELY(win->cursor_keymap_status == NULL)) {
    win->cursor_keymap_status = MEM_callocN(sizeof(struct CursorKeymapInfo), __func__);
  }
  cd = win->cursor_keymap_status;

  /* Detect unchanged state (early exit). */
  if (memcmp(&cd->state_event, win->eventstate, sizeof(wmEvent)) == 0) {
    return;
  }

  /* Now perform more comprehensive check,
   * still keep this fast since it happens on mouse-move. */
  struct CursorKeymapInfo cd_prev = *((struct CursorKeymapInfo *)win->cursor_keymap_status);
  cd->state_event = *win->eventstate;

  /* Find active region and associated area. */
  ARegion *region = screen->active_region;
  if (region == NULL) {
    return;
  }

  ScrArea *area = NULL;
  ED_screen_areas_iter (win, screen, area_iter) {
    if (BLI_findindex(&area_iter->regionbase, region) != -1) {
      area = area_iter;
      break;
    }
  }
  if (area == NULL) {
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
    bToolRef *tref = NULL;
    if ((region->regiontype == RGN_TYPE_WINDOW) &&
        ((1 << area->spacetype) & WM_TOOLSYSTEM_SPACE_MASK)) {
      ViewLayer *view_layer = WM_window_get_active_view_layer(win);
      WorkSpace *workspace = WM_window_get_active_workspace(win);
      const bToolKey tkey = {
          .space_type = area->spacetype,
          .mode = WM_toolsystem_mode_from_spacetype(view_layer, area, area->spacetype),
      };
      tref = WM_toolsystem_ref_find(workspace, &tkey);
    }
    wm_event_cursor_store(&cd->state, win->eventstate, area->spacetype, region->regiontype, tref);
    if (memcmp(&cd->state, &cd_prev.state, sizeof(cd->state)) == 0) {
      return;
    }
  }

  /* Changed context found, detect changes to keymap and refresh the status bar. */
  const struct {
    int button_index;
    int type_index; /* 0: press or click, 1: drag. */
    int event_type;
    int event_value;
  } event_data[] = {
      {0, 0, LEFTMOUSE, KM_PRESS},
      {0, 0, LEFTMOUSE, KM_CLICK},
      {0, 1, EVT_TWEAK_L, KM_ANY},

      {1, 0, MIDDLEMOUSE, KM_PRESS},
      {1, 0, MIDDLEMOUSE, KM_CLICK},
      {1, 1, EVT_TWEAK_M, KM_ANY},

      {2, 0, RIGHTMOUSE, KM_PRESS},
      {2, 0, RIGHTMOUSE, KM_CLICK},
      {2, 1, EVT_TWEAK_R, KM_ANY},
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
    wm_eventemulation(&test_event, true);
    wmKeyMapItem *kmi = NULL;
    for (int handler_index = 0; handler_index < ARRAY_SIZE(handlers); handler_index++) {
      kmi = WM_event_match_keymap_item_from_handlers(C, wm, handlers[handler_index], &test_event);
      if (kmi) {
        break;
      }
    }
    if (kmi) {
      wmOperatorType *ot = WM_operatortype_find(kmi->idname, 0);
      const char *name = (ot) ? WM_operatortype_name(ot, kmi->ptr) : kmi->idname;
      STRNCPY(cd->text[button_index][type_index], name);
    }
  }

  if (memcmp(&cd_prev.text, &cd->text, sizeof(cd_prev.text)) != 0) {
    ED_area_tag_redraw(area_statusbar);
  }

  CTX_wm_window_set(C, NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modal Keymap Status
 * \{ */

bool WM_window_modal_keymap_status_draw(bContext *C, wmWindow *win, uiLayout *layout)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmKeyMap *keymap = NULL;
  wmOperator *op = NULL;
  LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
      if (handler->op != NULL) {
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
  if (keymap == NULL || keymap->modal_items == NULL) {
    return false;
  }
  const EnumPropertyItem *items = keymap->modal_items;

  uiLayout *row = uiLayoutRow(layout, true);
  for (int i = 0; items[i].identifier; i++) {
    if (!items[i].identifier[0]) {
      continue;
    }
    if ((keymap->poll_modal_item != NULL) &&
        (keymap->poll_modal_item(op, items[i].value) == false)) {
      continue;
    }

    bool show_text = true;

    {
      /* Warning: O(n^2). */
      wmKeyMapItem *kmi = NULL;
      for (kmi = keymap->items.first; kmi; kmi = kmi->next) {
        if (kmi->propvalue == items[i].value) {
          break;
        }
      }
      if (kmi != NULL) {
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
