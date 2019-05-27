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

#include <stdlib.h>
#include <string.h>

#include "DNA_listBase.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "GHOST_C-api.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_timer.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_idprop.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "BKE_sound.h"

#include "ED_fileselect.h"
#include "ED_info.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_util.h"
#include "ED_undo.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "PIL_time.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"
#include "WM_toolsystem.h"

#include "wm.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"

#include "RNA_enum_types.h"

#include "DEG_depsgraph.h"

/* Motion in pixels allowed before we don't consider single/double click,
 * or detect the start of a tweak event. */
#define WM_EVENT_CLICK_TWEAK_THRESHOLD (U.tweak_threshold * U.dpi_fac)

static void wm_notifier_clear(wmNotifier *note);
static void update_tablet_data(wmWindow *win, wmEvent *event);

static int wm_operator_call_internal(bContext *C,
                                     wmOperatorType *ot,
                                     PointerRNA *properties,
                                     ReportList *reports,
                                     const short context,
                                     const bool poll_only,
                                     wmEvent *event);

/* ************ event management ************** */

wmEvent *wm_event_add_ex(wmWindow *win,
                         const wmEvent *event_to_add,
                         const wmEvent *event_to_add_after)
{
  wmEvent *event = MEM_mallocN(sizeof(wmEvent), "wmEvent");

  *event = *event_to_add;

  update_tablet_data(win, event);

  if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
    /* We could have a preference to support relative tablet motion (we can't detect that). */
    event->is_motion_absolute = ((event->tablet_data != NULL) &&
                                 (event->tablet_data->Active != GHOST_kTabletModeNone));
  }

  if (event_to_add_after == NULL) {
    BLI_addtail(&win->queue, event);
  }
  else {
    /* Note: strictly speaking this breaks const-correctness,
     * however we're only changing 'next' member. */
    BLI_insertlinkafter(&win->queue, (void *)event_to_add_after, event);
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
  win->eventstate->x = event->x;
  win->eventstate->y = event->y;
  return event;
}

void wm_event_free(wmEvent *event)
{
  if (event->customdata) {
    if (event->customdatafree) {
      /* note: pointer to listbase struct elsewhere */
      if (event->custom == EVT_DATA_DRAGDROP) {
        ListBase *lb = event->customdata;
        WM_drag_free_list(lb);
      }
      else {
        MEM_freeN(event->customdata);
      }
    }
  }

  if (event->tablet_data) {
    MEM_freeN((void *)event->tablet_data);
  }

  MEM_freeN(event);
}

void wm_event_free_all(wmWindow *win)
{
  wmEvent *event;

  while ((event = BLI_pophead(&win->queue))) {
    wm_event_free(event);
  }
}

void wm_event_init_from_window(wmWindow *win, wmEvent *event)
{
  /* make sure we don't copy any owned pointers */
  BLI_assert(win->eventstate->tablet_data == NULL);

  *event = *(win->eventstate);
}

/* ********************* notifiers, listeners *************** */

static bool wm_test_duplicate_notifier(wmWindowManager *wm, unsigned int type, void *reference)
{
  wmNotifier *note;

  for (note = wm->queue.first; note; note = note->next) {
    if ((note->category | note->data | note->subtype | note->action) == type &&
        note->reference == reference) {
      return 1;
    }
  }

  return 0;
}

/* XXX: in future, which notifiers to send to other windows? */
void WM_event_add_notifier(const bContext *C, unsigned int type, void *reference)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmNotifier *note;

  if (wm_test_duplicate_notifier(wm, type, reference)) {
    return;
  }

  note = MEM_callocN(sizeof(wmNotifier), "notifier");

  note->wm = wm;
  BLI_addtail(&note->wm->queue, note);

  note->window = CTX_wm_window(C);

  note->category = type & NOTE_CATEGORY;
  note->data = type & NOTE_DATA;
  note->subtype = type & NOTE_SUBTYPE;
  note->action = type & NOTE_ACTION;

  note->reference = reference;
}

void WM_main_add_notifier(unsigned int type, void *reference)
{
  Main *bmain = G_MAIN;
  wmWindowManager *wm = bmain->wm.first;
  wmNotifier *note;

  if (!wm || wm_test_duplicate_notifier(wm, type, reference)) {
    return;
  }

  note = MEM_callocN(sizeof(wmNotifier), "notifier");

  note->wm = wm;
  BLI_addtail(&note->wm->queue, note);

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
    wmNotifier *note, *note_next;

    for (note = wm->queue.first; note; note = note_next) {
      note_next = note->next;

      if (note->reference == reference) {
        /* don't remove because this causes problems for #wm_event_do_notifiers
         * which may be looping on the data (deleting screens) */
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
  bScreen *sc;

  for (sc = bmain->screens.first; sc; sc = sc->id.next) {
    ScrArea *sa;

    for (sa = sc->areabase.first; sa; sa = sa->next) {
      SpaceLink *sl;

      for (sl = sa->spacedata.first; sl; sl = sl->next) {
        ED_spacedata_id_remap(sa, sl, old_id, new_id);
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
  /* NULL the entire notifier, only leaving (next, prev) members intact */
  memset(((char *)note) + sizeof(Link), 0, sizeof(*note) - sizeof(Link));
}

void wm_event_do_depsgraph(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  /* The whole idea of locked interface is to prevent viewport and whatever
   * thread to modify the same data. Because of this, we can not perform
   * dependency graph update.
   */
  if (wm->is_interface_locked) {
    return;
  }
  /* Combine datamasks so 1 win doesn't disable UV's in another [#26448]. */
  CustomData_MeshMasks win_combine_v3d_datamask = {0};
  for (wmWindow *win = wm->windows.first; win; win = win->next) {
    const Scene *scene = WM_window_get_active_scene(win);
    const bScreen *screen = WM_window_get_active_screen(win);

    ED_view3d_screen_datamask(C, scene, screen, &win_combine_v3d_datamask);
  }
  /* Update all the dependency graphs of visible view layers. */
  for (wmWindow *win = wm->windows.first; win; win = win->next) {
    Scene *scene = WM_window_get_active_scene(win);
    ViewLayer *view_layer = WM_window_get_active_view_layer(win);
    Main *bmain = CTX_data_main(C);
    /* Copied to set's in scene_update_tagged_recursive() */
    scene->customdata_mask = win_combine_v3d_datamask;
    /* XXX, hack so operators can enforce datamasks [#26482], gl render */
    CustomData_MeshMasks_update(&scene->customdata_mask, &scene->customdata_mask_modal);
    /* TODO(sergey): For now all dependency graphs which are evaluated from
     * workspace are considered active. This will work all fine with "locked"
     * view layer and time across windows. This is to be granted separately,
     * and for until then we have to accept ambiguities when object is shared
     * across visible view layers and has overrides on it.
     */
    Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
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
  /* cached: editor refresh callbacks now, they get context */
  for (wmWindow *win = wm->windows.first; win; win = win->next) {
    const bScreen *screen = WM_window_get_active_screen(win);
    ScrArea *sa;

    CTX_wm_window_set(C, win);
    for (sa = screen->areabase.first; sa; sa = sa->next) {
      if (sa->do_refresh) {
        CTX_wm_area_set(C, sa);
        ED_area_do_refresh(C, sa);
      }
    }
  }

  wm_event_do_depsgraph(C);

  CTX_wm_window_set(C, NULL);
}

/* called in mainloop */
void wm_event_do_notifiers(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmNotifier *note, *next;
  wmWindow *win;

  if (wm == NULL) {
    return;
  }

  BLI_timer_execute();

  /* disable? - keep for now since its used for window level notifiers. */
#if 1
  /* cache & catch WM level notifiers, such as frame change, scene/screen set */
  for (win = wm->windows.first; win; win = win->next) {
    Scene *scene = WM_window_get_active_scene(win);
    bool do_anim = false;

    CTX_wm_window_set(C, win);

    for (note = wm->queue.first; note; note = next) {
      next = note->next;

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

            ED_workspace_delete(workspace, CTX_data_main(C), C, wm);  // XXX hrms, think this over!
            if (G.debug & G_DEBUG_EVENTS) {
              printf("%s: Workspace delete %p\n", __func__, workspace);
            }
          }
          else if (note->data == ND_LAYOUTBROWSE) {
            bScreen *ref_screen = BKE_workspace_layout_screen_get(note->reference);

            /* free popup handlers only [#35434] */
            UI_popup_handlers_remove_all(C, &win->modalhandlers);

            ED_screen_change(C, ref_screen); /* XXX hrms, think this over! */
            if (G.debug & G_DEBUG_EVENTS) {
              printf("%s: screen set %p\n", __func__, note->reference);
            }
          }
          else if (note->data == ND_LAYOUTDELETE) {
            WorkSpace *workspace = WM_window_get_active_workspace(win);
            WorkSpaceLayout *layout = note->reference;

            ED_workspace_layout_delete(workspace, layout, C);  // XXX hrms, think this over!
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
        ViewLayer *view_layer = CTX_data_view_layer(C);
        ED_info_stats_clear(view_layer);
        WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO, NULL);
      }
    }
    if (do_anim) {

      /* XXX, quick frame changes can cause a crash if framechange and rendering
       * collide (happens on slow scenes), BKE_scene_graph_update_for_newframe can be called
       * twice which can depgraph update the same object at once */
      if (G.is_rendering == false) {
        /* depsgraph gets called, might send more notifiers */
        Depsgraph *depsgraph = CTX_data_depsgraph(C);
        ED_update_for_newframe(CTX_data_main(C), depsgraph);
      }
    }
  }

  /* the notifiers are sent without context, to keep it clean */
  while ((note = BLI_pophead(&wm->queue))) {
    for (win = wm->windows.first; win; win = win->next) {
      Scene *scene = WM_window_get_active_scene(win);
      bScreen *screen = WM_window_get_active_screen(win);
      WorkSpace *workspace = WM_window_get_active_workspace(win);

      /* filter out notifiers */
      if (note->category == NC_SCREEN && note->reference && note->reference != screen &&
          note->reference != workspace && note->reference != WM_window_get_active_layout(win)) {
        /* pass */
      }
      else if (note->category == NC_SCENE && note->reference && note->reference != scene) {
        /* pass */
      }
      else {
        ARegion *ar;

        /* XXX context in notifiers? */
        CTX_wm_window_set(C, win);

#  if 0
        printf("notifier win %d screen %s cat %x\n",
               win->winid,
               win->screen->id.name + 2,
               note->category);
#  endif
        ED_screen_do_listen(C, note);

        for (ar = screen->regionbase.first; ar; ar = ar->next) {
          ED_region_do_listen(win, NULL, ar, note, scene);
        }

        ED_screen_areas_iter(win, screen, sa)
        {
          ED_area_do_listen(win, sa, note, scene);
          for (ar = sa->regionbase.first; ar; ar = ar->next) {
            ED_region_do_listen(win, sa, ar, note, scene);
          }
        }
      }
    }

    MEM_freeN(note);
  }
#endif /* if 1 (postpone disabling for in favor of message-bus), eventually. */

  /* Handle message bus. */
  {
    for (win = wm->windows.first; win; win = win->next) {
      CTX_wm_window_set(C, win);
      WM_msgbus_handle(wm->message_bus, C);
    }
    CTX_wm_window_set(C, NULL);
  }

  wm_event_do_refresh_wm_and_depsgraph(C);

  /* Status bar */
  if (wm->winactive) {
    win = wm->winactive;
    CTX_wm_window_set(C, win);
    WM_window_cursor_keymap_status_refresh(C, win);
    CTX_wm_window_set(C, NULL);
  }

  /* Autorun warning */
  wm_test_autorun_warning(C);
}

static int wm_event_always_pass(const wmEvent *event)
{
  /* some events we always pass on, to ensure proper communication */
  return ISTIMER(event->type) || (event->type == WINDEACTIVATE);
}

/* ********************* ui handler ******************* */

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
  int retval;

  /* UI code doesn't handle return values - it just always returns break.
   * to make the DBL_CLICK conversion work, we just don't send this to UI, except mouse clicks */
  if (((handler->head.flag & WM_HANDLER_ACCEPT_DBL_CLICK) == 0) && !ISMOUSE_BUTTON(event->type) &&
      (event->val == KM_DBL_CLICK)) {
    return WM_HANDLER_CONTINUE;
  }

  /* UI is quite aggressive with swallowing events, like scrollwheel */
  /* I realize this is not extremely nice code... when UI gets keymaps it can be maybe smarter */
  if (do_wheel_ui == false) {
    if (is_wheel) {
      return WM_HANDLER_CONTINUE;
    }
    else if (wm_event_always_pass(event) == 0) {
      do_wheel_ui = true;
    }
  }

  /* we set context to where ui handler came from */
  if (handler->context.area) {
    CTX_wm_area_set(C, handler->context.area);
  }
  if (handler->context.region) {
    CTX_wm_region_set(C, handler->context.region);
  }
  if (handler->context.menu) {
    CTX_wm_menu_set(C, handler->context.menu);
  }

  retval = handler->handle_fn(C, event, handler->user_data);

  /* putting back screen context */
  if ((retval != WM_UI_HANDLER_BREAK) || always_pass) {
    CTX_wm_area_set(C, area);
    CTX_wm_region_set(C, region);
    CTX_wm_menu_set(C, menu);
  }
  else {
    /* this special cases is for areas and regions that get removed */
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

static void wm_handler_ui_cancel(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  ARegion *ar = CTX_wm_region(C);

  if (!ar) {
    return;
  }

  LISTBASE_FOREACH_MUTABLE (wmEventHandler *, handler_base, &ar->handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_UI) {
      wmEventHandler_UI *handler = (wmEventHandler_UI *)handler_base;
      BLI_assert(handler->handle_fn != NULL);
      wmEvent event;
      wm_event_init_from_window(win, &event);
      event.type = EVT_BUT_CANCEL;
      handler->handle_fn(C, &event, handler->user_data);
    }
  }
}

/* ********************* operators ******************* */

bool WM_operator_poll(bContext *C, wmOperatorType *ot)
{
  wmOperatorTypeMacro *otmacro;

  for (otmacro = ot->macro.first; otmacro; otmacro = otmacro->next) {
    wmOperatorType *ot_macro = WM_operatortype_find(otmacro->idname, 0);

    if (0 == WM_operator_poll(C, ot_macro)) {
      return 0;
    }
  }

  /* python needs operator type, so we added exception for it */
  if (ot->pyop_poll) {
    return ot->pyop_poll(C, ot);
  }
  else if (ot->poll) {
    return ot->poll(C);
  }

  return 1;
}

/* sets up the new context and calls 'wm_operator_invoke()' with poll_only */
bool WM_operator_poll_context(bContext *C, wmOperatorType *ot, short context)
{
  return wm_operator_call_internal(C, ot, NULL, NULL, context, true, NULL);
}

bool WM_operator_check_ui_empty(wmOperatorType *ot)
{
  if (ot->macro.first != NULL) {
    /* for macros, check all have exec() we can call */
    wmOperatorTypeMacro *otmacro;
    for (otmacro = ot->macro.first; otmacro; otmacro = otmacro->next) {
      wmOperatorType *otm = WM_operatortype_find(otmacro->idname, 0);
      if (otm && !WM_operator_check_ui_empty(otm)) {
        return false;
      }
    }
    return true;
  }

  /* Assume a ui callback will draw something. */
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
  ScrArea *sa = CTX_wm_area(C);
  if (sa) {
    ARegion *ar = CTX_wm_region(C);
    if (ar && ar->regiontype == RGN_TYPE_WINDOW) {
      sa->region_active_win = BLI_findindex(&sa->regionbase, ar);
    }
  }
}

/* for debugging only, getting inspecting events manually is tedious */
void WM_event_print(const wmEvent *event)
{
  if (event) {
    const char *unknown = "UNKNOWN";
    const char *type_id = unknown;
    const char *val_id = unknown;

    RNA_enum_identifier(rna_enum_event_type_items, event->type, &type_id);
    RNA_enum_identifier(rna_enum_event_value_items, event->val, &val_id);

    printf(
        "wmEvent  type:%d / %s, val:%d / %s,\n"
        "         shift:%d, ctrl:%d, alt:%d, oskey:%d, keymodifier:%d,\n"
        "         mouse:(%d,%d), ascii:'%c', utf8:'%.*s', keymap_idname:%s, pointer:%p\n",
        event->type,
        type_id,
        event->val,
        val_id,
        event->shift,
        event->ctrl,
        event->alt,
        event->oskey,
        event->keymodifier,
        event->x,
        event->y,
        event->ascii,
        BLI_str_utf8_size(event->utf8_buf),
        event->utf8_buf,
        event->keymap_idname,
        (const void *)event);

#ifdef WITH_INPUT_NDOF
    if (ISNDOF(event->type)) {
      const wmNDOFMotionData *ndof = event->customdata;
      if (event->type == NDOF_MOTION) {
        printf("   ndof: rot: (%.4f %.4f %.4f), tx: (%.4f %.4f %.4f), dt: %.4f, progress: %u\n",
               UNPACK3(ndof->rvec),
               UNPACK3(ndof->tvec),
               ndof->dt,
               ndof->progress);
      }
      else {
        /* ndof buttons printed already */
      }
    }
#endif /* WITH_INPUT_NDOF */

    if (event->tablet_data) {
      const wmTabletData *wmtab = event->tablet_data;
      printf(" tablet: active: %d, pressure %.4f, tilt: (%.4f %.4f)\n",
             wmtab->Active,
             wmtab->Pressure,
             wmtab->Xtilt,
             wmtab->Ytilt);
    }
  }
  else {
    printf("wmEvent - NULL\n");
  }
}

/**
 * Show the report in the info header.
 */
void WM_report_banner_show(void)
{
  wmWindowManager *wm = G_MAIN->wm.first;
  ReportList *wm_reports = &wm->reports;
  ReportTimerInfo *rti;

  /* After adding reports to the global list, reset the report timer. */
  WM_event_remove_timer(wm, NULL, wm_reports->reporttimer);

  /* Records time since last report was added */
  wm_reports->reporttimer = WM_event_add_timer(wm, wm->winactive, TIMERREPORT, 0.05);

  rti = MEM_callocN(sizeof(ReportTimerInfo), "ReportTimerInfo");
  wm_reports->reporttimer->customdata = rti;
}

bool WM_event_is_last_mousemove(const wmEvent *event)
{
  while ((event = event->next)) {
    if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
      return false;
    }
  }
  return true;
}

#ifdef WITH_INPUT_NDOF
void WM_ndof_deadzone_set(float deadzone)
{
  GHOST_setNDOFDeadZone(deadzone);
}
#endif

static void wm_add_reports(ReportList *reports)
{
  /* if the caller owns them, handle this */
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
  DynStr *ds;
  va_list args;

  ds = BLI_dynstr_new();
  va_start(args, format);
  BLI_dynstr_vappendf(ds, format, args);
  va_end(args);

  char *str = BLI_dynstr_get_cstring(ds);
  WM_report(type, str);
  MEM_freeN(str);

  BLI_dynstr_free(ds);
}

/* (caller_owns_reports == true) when called from python */
static void wm_operator_reports(bContext *C, wmOperator *op, int retval, bool caller_owns_reports)
{
  if (G.background == 0 && caller_owns_reports == false) { /* popup */
    if (op->reports->list.first) {
      /* FIXME, temp setting window, see other call to UI_popup_menu_reports for why */
      wmWindow *win_prev = CTX_wm_window(C);
      ScrArea *area_prev = CTX_wm_area(C);
      ARegion *ar_prev = CTX_wm_region(C);

      if (win_prev == NULL) {
        CTX_wm_window_set(C, CTX_wm_manager(C)->windows.first);
      }

      UI_popup_menu_reports(C, op->reports);

      CTX_wm_window_set(C, win_prev);
      CTX_wm_area_set(C, area_prev);
      CTX_wm_region_set(C, ar_prev);
    }
  }

  if (retval & OPERATOR_FINISHED) {
    CLOG_STR_INFO_N(WM_LOG_OPERATORS, 1, WM_operator_pystring(C, op, false, true));

    if (caller_owns_reports == false) {
      BKE_reports_print(op->reports, RPT_DEBUG); /* print out reports to console. */
    }

    if (op->type->flag & OPTYPE_REGISTER) {
      if (G.background == 0) { /* ends up printing these in the terminal, gets annoying */
        /* Report the python string representation of the operator */
        char *buf = WM_operator_pystring(C, op, false, true);
        BKE_report(CTX_wm_reports(C), RPT_OPERATOR, buf);
        MEM_freeN(buf);
      }
    }
  }

  /* if the caller owns them, handle this */
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

  /* we don't want to do undo pushes for operators that are being
   * called from operators that already do an undo push. usually
   * this will happen for python operators that call C operators */
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
      ScrArea *sa = CTX_wm_area(C);
      if (sa) {
        ED_area_type_hud_ensure(C, sa);
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

/* if repeat is true, it doesn't register again, nor does it free */
static int wm_operator_exec(bContext *C,
                            wmOperator *op,
                            const bool repeat,
                            const bool use_repeat_op_flag,
                            const bool store)
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

    if (repeat && use_repeat_op_flag) {
      op->flag |= OP_IS_REPEAT;
    }
    retval = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(retval);
    if (repeat && use_repeat_op_flag) {
      op->flag &= ~OP_IS_REPEAT;
    }

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

/* simply calls exec with basic checks */
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
 * for running operators with frozen context (modal handlers, menus)
 *
 * \param store: Store settings for re-use.
 *
 * warning: do not use this within an operator to call its self! [#29537] */
int WM_operator_call_ex(bContext *C, wmOperator *op, const bool store)
{
  return wm_operator_exec(C, op, false, false, store);
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
  return wm_operator_exec(C, op, true, true, true);
}
int WM_operator_repeat_interactive(bContext *C, wmOperator *op)
{
  return wm_operator_exec(C, op, true, false, true);
}
/**
 * \return true if #WM_operator_repeat can run
 * simple check for now but may become more involved.
 * To be sure the operator can run call `WM_operator_poll(C, op->type)` also, since this call
 * checks if #WM_operator_repeat() can run at all, not that it WILL run at any time.
 */
bool WM_operator_repeat_check(const bContext *UNUSED(C), wmOperator *op)
{
  if (op->type->exec != NULL) {
    return true;
  }
  else if (op->opm) {
    /* for macros, check all have exec() we can call */
    wmOperatorTypeMacro *otmacro;
    for (otmacro = op->opm->type->macro.first; otmacro; otmacro = otmacro->next) {
      wmOperatorType *otm = WM_operatortype_find(otmacro->idname, 0);
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
  /* may be in the operators list or not */
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

  /* initialize properties, either copy or create */
  op->ptr = MEM_callocN(sizeof(PointerRNA), "wmOperatorPtrRNA");
  if (properties && properties->data) {
    op->properties = IDP_CopyProperty(properties->data);
  }
  else {
    IDPropertyTemplate val = {0};
    op->properties = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
  }
  RNA_pointer_create(&wm->id, ot->srna, op->properties, op->ptr);

  /* initialize error reports */
  if (reports) {
    op->reports = reports; /* must be initialized already */
  }
  else {
    op->reports = MEM_mallocN(sizeof(ReportList), "wmOperatorReportList");
    BKE_reports_init(op->reports, RPT_STORE | RPT_FREE);
  }

  /* recursive filling of operator macro list */
  if (ot->macro.first) {
    static wmOperator *motherop = NULL;
    wmOperatorTypeMacro *otmacro;
    int root = 0;

    /* ensure all ops are in execution order in 1 list */
    if (motherop == NULL) {
      motherop = op;
      root = 1;
    }

    /* if properties exist, it will contain everything needed */
    if (properties) {
      otmacro = ot->macro.first;

      RNA_STRUCT_BEGIN (properties, prop) {

        if (otmacro == NULL) {
          break;
        }

        /* skip invalid properties */
        if (STREQ(RNA_property_identifier(prop), otmacro->idname)) {
          wmOperatorType *otm = WM_operatortype_find(otmacro->idname, 0);
          PointerRNA someptr = RNA_property_pointer_get(properties, prop);
          wmOperator *opm = wm_operator_create(wm, otm, &someptr, NULL);

          IDP_ReplaceGroupInGroup(opm->properties, otmacro->properties);

          BLI_addtail(&motherop->macro, opm);
          opm->opm = motherop; /* pointer to mom, for modal() */

          otmacro = otmacro->next;
        }
      }
      RNA_STRUCT_END;
    }
    else {
      for (otmacro = ot->macro.first; otmacro; otmacro = otmacro->next) {
        wmOperatorType *otm = WM_operatortype_find(otmacro->idname, 0);
        wmOperator *opm = wm_operator_create(wm, otm, otmacro->ptr, NULL);

        BLI_addtail(&motherop->macro, opm);
        opm->opm = motherop; /* pointer to mom, for modal() */
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
  ARegion *ar = CTX_wm_region(C);
  if (ar) {
    /* compatibility convention */
    event->mval[0] = event->x - ar->winrct.xmin;
    event->mval[1] = event->y - ar->winrct.ymin;
  }
  else {
    /* these values are invalid (avoid odd behavior by relying on old mval values) */
    event->mval[0] = -1;
    event->mval[1] = -1;
  }
}

#if 1 /* may want to disable operator remembering previous state for testing */

static bool operator_last_properties_init_impl(wmOperator *op, IDProperty *last_properties)
{
  bool changed = false;
  IDPropertyTemplate val = {0};
  IDProperty *replaceprops = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
  PropertyRNA *iterprop;

  CLOG_INFO(WM_LOG_OPERATORS, 1, "loading previous properties for '%s'", op->type->idname);

  iterprop = RNA_struct_iterator_property(op->type->srna);

  RNA_PROP_BEGIN (op->ptr, itemptr, iterprop) {
    PropertyRNA *prop = itemptr.data;
    if ((RNA_property_flag(prop) & PROP_SKIP_SAVE) == 0) {
      if (!RNA_property_is_set(op->ptr, prop)) { /* don't override a setting already set */
        const char *identifier = RNA_property_identifier(prop);
        IDProperty *idp_src = IDP_GetPropertyFromGroup(last_properties, identifier);
        if (idp_src) {
          IDProperty *idp_dst = IDP_CopyProperty(idp_src);

          /* note - in the future this may need to be done recursively,
           * but for now RNA doesn't access nested operators */
          idp_dst->flag |= IDP_FLAG_GHOST;

          /* add to temporary group instead of immediate replace,
           * because we are iterating over this group */
          IDP_AddToGroup(replaceprops, idp_dst);
          changed = true;
        }
      }
    }
  }
  RNA_PROP_END;

  IDP_MergeGroup(op->properties, replaceprops, true);
  IDP_FreeProperty(replaceprops);
  return changed;
}

bool WM_operator_last_properties_init(wmOperator *op)
{
  bool changed = false;
  if (op->type->last_properties) {
    changed |= operator_last_properties_init_impl(op, op->type->last_properties);
    for (wmOperator *opm = op->macro.first; opm; opm = opm->next) {
      IDProperty *idp_src = IDP_GetPropertyFromGroup(op->type->last_properties, opm->idname);
      if (idp_src) {
        changed |= operator_last_properties_init_impl(opm, idp_src);
      }
    }
  }
  return changed;
}

bool WM_operator_last_properties_store(wmOperator *op)
{
  if (op->type->last_properties) {
    IDP_FreeProperty(op->type->last_properties);
    op->type->last_properties = NULL;
  }

  if (op->properties) {
    CLOG_INFO(WM_LOG_OPERATORS, 1, "storing properties for '%s'", op->type->idname);
    op->type->last_properties = IDP_CopyProperty(op->properties);
  }

  if (op->macro.first != NULL) {
    for (wmOperator *opm = op->macro.first; opm; opm = opm->next) {
      if (opm->properties) {
        if (op->type->last_properties == NULL) {
          op->type->last_properties = IDP_New(
              IDP_GROUP, &(IDPropertyTemplate){0}, "wmOperatorProperties");
        }
        IDProperty *idp_macro = IDP_CopyProperty(opm->properties);
        STRNCPY(idp_macro->name, opm->idname);
        IDP_ReplaceInGroup(op->type->last_properties, idp_macro);
      }
    }
  }

  return (op->type->last_properties != NULL);
}

#else

bool WM_operator_last_properties_init(wmOperator *UNUSED(op))
{
  return false;
}

bool WM_operator_last_properties_store(wmOperator *UNUSED(op))
{
  return false;
}

#endif

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
    wmOperator *op = wm_operator_create(
        wm, ot, properties, reports); /* if reports == NULL, they'll be initialized */
    const bool is_nested_call = (wm->op_undo_depth != 0);

    if (event != NULL) {
      op->flag |= OP_IS_INVOKE;
    }

    /* initialize setting from previous run */
    if (!is_nested_call && use_last_properties) { /* not called by py script */
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
      /* debug, important to leave a while, should never happen */
      CLOG_ERROR(WM_LOG_OPERATORS, "invalid operator call '%s'", op->idname);
    }

    /* Note, if the report is given as an argument then assume the caller will deal with displaying
     * them currently Python only uses this. */
    if (!(retval & OPERATOR_HANDLED) && (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED))) {
      /* only show the report if the report list was not given in the function */
      wm_operator_reports(C, op, retval, (reports != NULL));
    }

    if (retval & OPERATOR_HANDLED) {
      /* do nothing, wm_operator_exec() has been called somewhere */
    }
    else if (retval & OPERATOR_FINISHED) {
      const bool store = !is_nested_call && use_last_properties;
      wm_operator_finished(C, op, false, store);
    }
    else if (retval & OPERATOR_RUNNING_MODAL) {
      /* take ownership of reports (in case python provided own) */
      op->reports->flag |= RPT_FREE;

      /* grab cursor during blocking modal ops (X11)
       * Also check for macro
       */
      if (ot->flag & OPTYPE_BLOCKING || (op->opm && op->opm->type->flag & OPTYPE_BLOCKING)) {
        int bounds[4] = {-1, -1, -1, -1};
        bool wrap;

        if (event == NULL) {
          wrap = false;
        }
        else if (op->opm) {
          wrap = (U.uiflag & USER_CONTINUOUS_MOUSE) &&
                 ((op->opm->flag & OP_IS_MODAL_GRAB_CURSOR) ||
                  (op->opm->type->flag & OPTYPE_GRAB_CURSOR));
        }
        else {
          wrap = (U.uiflag & USER_CONTINUOUS_MOUSE) &&
                 ((op->flag & OP_IS_MODAL_GRAB_CURSOR) || (ot->flag & OPTYPE_GRAB_CURSOR));
        }

        /* exception, cont. grab in header is annoying */
        if (wrap) {
          ARegion *ar = CTX_wm_region(C);
          if (ar && ELEM(ar->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER, RGN_TYPE_FOOTER)) {
            wrap = false;
          }
        }

        if (wrap) {
          const rcti *winrect = NULL;
          ARegion *ar = CTX_wm_region(C);
          ScrArea *sa = CTX_wm_area(C);

          if (ar && ar->regiontype == RGN_TYPE_WINDOW &&
              BLI_rcti_isect_pt_v(&ar->winrct, &event->x)) {
            winrect = &ar->winrct;
          }
          else if (sa && BLI_rcti_isect_pt_v(&sa->totrct, &event->x)) {
            winrect = &sa->totrct;
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

      /* cancel UI handlers, typically tooltips that can hang around
       * while dragging the view or worse, that stay there permanently
       * after the modal operator has swallowed all events and passed
       * none to the UI handler */
      wm_handler_ui_cancel(C);
    }
    else {
      WM_operator_free(op);
    }
  }

  return retval;
}

/**
 * #WM_operator_name_call is the main accessor function
 * this is for python to access since its done the operator lookup
 *
 * invokes operator in context
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

  /* dummie test */
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
          /* window is needed for invoke, cancel operator */
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
        /* forces operator to go to the region window/channels/preview, for header menus
         * but we stay in the same region if we are already in one
         */
        ARegion *ar = CTX_wm_region(C);
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

        if (!(ar && ar->regiontype == type) && area) {
          ARegion *ar1;
          if (type == RGN_TYPE_WINDOW) {
            ar1 = BKE_area_find_region_active_win(area);
          }
          else {
            ar1 = BKE_area_find_region_type(area, type);
          }

          if (ar1) {
            CTX_wm_region_set(C, ar1);
          }
        }

        retval = wm_operator_invoke(C, ot, event, properties, reports, poll_only, true);

        /* set region back */
        CTX_wm_region_set(C, ar);

        return retval;
      }
      case WM_OP_EXEC_AREA:
      case WM_OP_INVOKE_AREA: {
        /* remove region from context */
        ARegion *ar = CTX_wm_region(C);

        CTX_wm_region_set(C, NULL);
        retval = wm_operator_invoke(C, ot, event, properties, reports, poll_only, true);
        CTX_wm_region_set(C, ar);

        return retval;
      }
      case WM_OP_EXEC_SCREEN:
      case WM_OP_INVOKE_SCREEN: {
        /* remove region + area from context */
        ARegion *ar = CTX_wm_region(C);
        ScrArea *area = CTX_wm_area(C);

        CTX_wm_region_set(C, NULL);
        CTX_wm_area_set(C, NULL);
        retval = wm_operator_invoke(C, ot, event, properties, reports, poll_only, true);
        CTX_wm_area_set(C, area);
        CTX_wm_region_set(C, ar);

        return retval;
      }
      case WM_OP_EXEC_DEFAULT:
      case WM_OP_INVOKE_DEFAULT:
        return wm_operator_invoke(C, ot, event, properties, reports, poll_only, true);
    }
  }

  return 0;
}

/* invokes operator in context */
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
    if (is_undo && op->type->flag & OPTYPE_UNDO)
      wm->op_undo_depth++;

    retval = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(retval);

    if (is_undo && op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm)
      wm->op_undo_depth--;
  }
  else {
    CLOG_WARN(WM_LOG_OPERATORS,
              "\"%s\" operator has no exec function, Python cannot call it",
              op->type->name);
  }

#endif

  /* not especially nice using undo depth here, its used so py never
   * triggers undo or stores operators last used state.
   *
   * we could have some more obvious way of doing this like passing a flag.
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

/* ********************* handlers *************** */

/* future extra customadata free? */
void wm_event_free_handler(wmEventHandler *handler)
{
  MEM_freeN(handler);
}

/* only set context when area/region is part of screen */
static void wm_handler_op_context(bContext *C, wmEventHandler_Op *handler, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);

  if (screen && handler->op) {
    if (handler->context.area == NULL) {
      CTX_wm_area_set(C, NULL);
    }
    else {
      ScrArea *sa = NULL;

      ED_screen_areas_iter(win, screen, sa_iter)
      {
        if (sa_iter == handler->context.area) {
          sa = sa_iter;
          break;
        }
      }

      if (sa == NULL) {
        /* when changing screen layouts with running modal handlers (like render display), this
         * is not an error to print */
        if (handler->op == NULL) {
          CLOG_ERROR(WM_LOG_HANDLERS,
                     "internal error: handler (%s) has invalid area",
                     handler->op->type->idname);
        }
      }
      else {
        ARegion *ar;
        wmOperator *op = handler->op ? (handler->op->opm ? handler->op->opm : handler->op) : NULL;
        CTX_wm_area_set(C, sa);

        if (op && (op->flag & OP_IS_MODAL_CURSOR_REGION)) {
          ar = BKE_area_find_region_xy(sa, handler->context.region_type, event->x, event->y);
          if (ar) {
            handler->context.region = ar;
          }
        }
        else {
          ar = NULL;
        }

        if (ar == NULL) {
          for (ar = sa->regionbase.first; ar; ar = ar->next) {
            if (ar == handler->context.region) {
              break;
            }
          }
        }

        /* XXX no warning print here, after full-area and back regions are remade */
        if (ar) {
          CTX_wm_region_set(C, ar);
        }
      }
    }
  }
}

/* called on exit or remove area, only here call cancel callback */
void WM_event_remove_handlers(bContext *C, ListBase *handlers)
{
  wmEventHandler *handler_base;
  wmWindowManager *wm = CTX_wm_manager(C);

  /* C is zero on freeing database, modal handlers then already were freed */
  while ((handler_base = BLI_pophead(handlers))) {
    BLI_assert(handler_base->type != 0);
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
      if (handler->op) {
        wmWindow *win = CTX_wm_window(C);
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

/* do userdef mappings */
int WM_userdef_event_map(int kmitype)
{
  switch (kmitype) {
    case WHEELOUTMOUSE:
      return (U.uiflag & USER_WHEELZOOMDIR) ? WHEELUPMOUSE : WHEELDOWNMOUSE;
    case WHEELINMOUSE:
      return (U.uiflag & USER_WHEELZOOMDIR) ? WHEELDOWNMOUSE : WHEELUPMOUSE;
  }

  return kmitype;
}

/**
 * Use so we can check if 'wmEvent.type' is released in modal operators.
 *
 * An alternative would be to add a 'wmEvent.type_nokeymap'... or similar.
 */
int WM_userdef_event_type_from_keymap_type(int kmitype)
{
  switch (kmitype) {
    case EVT_TWEAK_L:
      return LEFTMOUSE;
    case EVT_TWEAK_M:
      return MIDDLEMOUSE;
    case EVT_TWEAK_R:
      return RIGHTMOUSE;
    case WHEELOUTMOUSE:
      return (U.uiflag & USER_WHEELZOOMDIR) ? WHEELUPMOUSE : WHEELDOWNMOUSE;
    case WHEELINMOUSE:
      return (U.uiflag & USER_WHEELZOOMDIR) ? WHEELDOWNMOUSE : WHEELUPMOUSE;
  }

  return kmitype;
}

static bool wm_eventmatch(const wmEvent *winevent, const wmKeyMapItem *kmi)
{
  if (kmi->flag & KMI_INACTIVE) {
    return false;
  }

  const int kmitype = WM_userdef_event_map(kmi->type);

  /* the matching rules */
  if (kmitype == KM_TEXTINPUT) {
    if (winevent->val == KM_PRESS) { /* prevent double clicks */
      /* NOT using ISTEXTINPUT anymore because (at least on Windows) some key codes above 255
       * could have printable ascii keys - BUG [#30479] */
      if (ISKEYBOARD(winevent->type) && (winevent->ascii || winevent->utf8_buf[0])) {
        return true;
      }
    }
  }

  if (kmitype != KM_ANY) {
    if (ELEM(kmitype, TABLET_STYLUS, TABLET_ERASER)) {
      const wmTabletData *wmtab = winevent->tablet_data;

      if (wmtab == NULL) {
        return false;
      }
      else if (winevent->type != LEFTMOUSE) {
        /* tablet events can occur on hover + keypress */
        return false;
      }
      else if ((kmitype == TABLET_STYLUS) && (wmtab->Active != EVT_TABLET_STYLUS)) {
        return false;
      }
      else if ((kmitype == TABLET_ERASER) && (wmtab->Active != EVT_TABLET_ERASER)) {
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
        !ELEM(winevent->type, LEFTSHIFTKEY, RIGHTSHIFTKEY)) {
      return false;
    }
  }
  if (kmi->ctrl != KM_ANY) {
    if (winevent->ctrl != kmi->ctrl && !(winevent->ctrl & kmi->ctrl) &&
        !ELEM(winevent->type, LEFTCTRLKEY, RIGHTCTRLKEY)) {
      return false;
    }
  }
  if (kmi->alt != KM_ANY) {
    if (winevent->alt != kmi->alt && !(winevent->alt & kmi->alt) &&
        !ELEM(winevent->type, LEFTALTKEY, RIGHTALTKEY)) {
      return false;
    }
  }
  if (kmi->oskey != KM_ANY) {
    if (winevent->oskey != kmi->oskey && !(winevent->oskey & kmi->oskey) &&
        (winevent->type != OSKEY)) {
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
  for (wmKeyMapItem *kmi = keymap->items.first; kmi; kmi = kmi->next) {
    if (wm_eventmatch(event, kmi)) {
      if ((keymap->poll_modal_item == NULL) || (keymap->poll_modal_item(op, kmi->propvalue))) {
        return kmi;
      }
    }
  }
  return NULL;
}

/* operator exists */
static void wm_event_modalkeymap(const bContext *C,
                                 wmOperator *op,
                                 wmEvent *event,
                                 bool *dbl_click_disabled)
{
  /* support for modal keymap in macros */
  if (op->opm) {
    op = op->opm;
  }

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
      event->prevtype = event_match->type;
      event->prevval = event_match->val;
      event->type = EVT_MODAL_MAP;
      event->val = kmi->propvalue;
    }
  }
  else {
    /* modal keymap checking returns handled events fine, but all hardcoded modal
     * handling typically swallows all events (OPERATOR_RUNNING_MODAL).
     * This bypass just disables support for double clicks in hardcoded modal handlers */
    if (event->val == KM_DBL_CLICK) {
      event->val = KM_PRESS;
      *dbl_click_disabled = true;
    }
  }
}

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

/* bad hacking event system... better restore event type for checking of KM_CLICK for example */
/* XXX modal maps could use different method (ton) */
static void wm_event_modalmap_end(wmEvent *event, bool dbl_click_disabled)
{
  if (event->type == EVT_MODAL_MAP) {
    event->type = event->prevtype;
    event->prevtype = 0;
    event->val = event->prevval;
    event->prevval = 0;
  }
  else if (dbl_click_disabled) {
    event->val = KM_DBL_CLICK;
  }
}

/* Warning: this function removes a modal handler, when finished */
static int wm_handler_operator_call(bContext *C,
                                    ListBase *handlers,
                                    wmEventHandler *handler_base,
                                    wmEvent *event,
                                    PointerRNA *properties)
{
  int retval = OPERATOR_PASS_THROUGH;

  /* derived, modal or blocking operator */
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
      /* we set context to where modal handler came from */
      wmWindowManager *wm = CTX_wm_manager(C);
      ScrArea *area = CTX_wm_area(C);
      ARegion *region = CTX_wm_region(C);
      bool dbl_click_disabled = false;

      wm_handler_op_context(C, handler, event);
      wm_region_mouse_co(C, event);
      wm_event_modalkeymap(C, op, event, &dbl_click_disabled);

      if (ot->flag & OPTYPE_UNDO) {
        wm->op_undo_depth++;
      }

      /* warning, after this call all context data and 'event' may be freed. see check below */
      retval = ot->modal(C, op, event);
      OPERATOR_RETVAL_CHECK(retval);

      /* when this is _not_ the case the modal modifier may have loaded
       * a new blend file (demo mode does this), so we have to assume
       * the event, operator etc have all been freed. - campbell */
      if (CTX_wm_manager(C) == wm) {

        wm_event_modalmap_end(event, dbl_click_disabled);

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
          /* not very common, but modal operators may report before finishing */
          if (!BLI_listbase_is_empty(&op->reports->list)) {
            wm_add_reports(op->reports);
          }
        }

        /* important to run 'wm_operator_finished' before NULLing the context members */
        if (retval & OPERATOR_FINISHED) {
          wm_operator_finished(C, op, false, true);
          handler->op = NULL;
        }
        else if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED)) {
          WM_operator_free(op);
          handler->op = NULL;
        }

        /* putting back screen context, reval can pass trough after modal failures! */
        if ((retval & OPERATOR_PASS_THROUGH) || wm_event_always_pass(event)) {
          CTX_wm_area_set(C, area);
          CTX_wm_region_set(C, region);
        }
        else {
          /* this special cases is for areas and regions that get removed */
          CTX_wm_area_set(C, NULL);
          CTX_wm_region_set(C, NULL);
        }

        /* update gizmos during modal handlers */
        wm_gizmomaps_handled_modal_update(C, event, handler);

        /* remove modal handler, operator itself should have been canceled and freed */
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
    wmOperatorType *ot = WM_operatortype_find(event->keymap_idname, 0);

    if (ot && wm_operator_check_locked_interface(C, ot)) {
      bool use_last_properties = true;
      PointerRNA tool_properties = {{0}};

      bToolRef *keymap_tool = ((handler_base->type == WM_HANDLER_TYPE_KEYMAP) ?
                                   ((wmEventHandler_Keymap *)handler_base)->keymap_tool :
                                   NULL);
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
                ARegion *ar = CTX_wm_region(C);
                if (ar != NULL) {
                  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
                  WM_gizmo_group_type_ensure_ptr_ex(gzgt, gzmap_type);
                  wmGizmoGroup *gzgroup = WM_gizmomaptype_group_init_runtime_with_region(
                      gzmap_type, gzgt, ar);
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
  /* Finished and pass through flag as handled */

  /* Finished and pass through flag as handled */
  if (retval == (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH)) {
    return WM_HANDLER_HANDLED;
  }

  /* Modal unhandled, break */
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
  SpaceFile *sfile;
  int action = WM_HANDLER_CONTINUE;

  switch (val) {
    case EVT_FILESELECT_FULL_OPEN: {
      ScrArea *sa;

      /* sa can be null when window A is active, but mouse is over window B
       * in this case, open file select in original window A. Also don't
       * use global areas. */
      if (handler->context.area == NULL || ED_area_is_global(handler->context.area)) {
        bScreen *screen = CTX_wm_screen(C);
        sa = (ScrArea *)screen->areabase.first;
      }
      else {
        sa = handler->context.area;
      }

      if (sa->full) {
        /* ensure the first area becomes the file browser, because the second one is the small
         * top (info-)area which might be too small (in fullscreens we have max two areas) */
        if (sa->prev) {
          sa = sa->prev;
        }
        ED_area_newspace(C, sa, SPACE_FILE, true); /* 'sa' is modified in-place */
        /* we already had a fullscreen here -> mark new space as a stacked fullscreen */
        sa->flag |= (AREA_FLAG_STACKED_FULLSCREEN | AREA_FLAG_TEMP_TYPE);
      }
      else if (sa->spacetype == SPACE_FILE) {
        sa = ED_screen_state_toggle(C, CTX_wm_window(C), sa, SCREENMAXIMIZED);
      }
      else {
        sa = ED_screen_full_newspace(C, sa, SPACE_FILE); /* sets context */
      }

      /* note, getting the 'sa' back from the context causes a nasty bug where the newly created
       * 'sa' != CTX_wm_area(C). removed the line below and set 'sa' in the 'if' above */
      /* sa = CTX_wm_area(C); */

      /* settings for filebrowser, sfile is not operator owner but sends events */
      sfile = (SpaceFile *)sa->spacedata.first;
      sfile->op = handler->op;

      ED_fileselect_set_params(sfile);

      action = WM_HANDLER_BREAK;
      break;
    }

    case EVT_FILESELECT_EXEC:
    case EVT_FILESELECT_CANCEL:
    case EVT_FILESELECT_EXTERNAL_CANCEL: {
      /* remlink now, for load file case before removing*/
      BLI_remlink(handlers, handler);

      if (val != EVT_FILESELECT_EXTERNAL_CANCEL) {
        ScrArea *sa = CTX_wm_area(C);

        if (sa->full) {
          ED_screen_full_prevspace(C, sa);
        }
        /* user may have left fullscreen */
        else {
          ED_area_prevspace(C, sa);
        }
      }

      wm_handler_op_context(C, handler, CTX_wm_window(C)->eventstate);

      /* needed for UI_popup_menu_reports */

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
          ARegion *ar_prev = CTX_wm_region(C);

          if (win_prev == NULL) {
            CTX_wm_window_set(C, CTX_wm_manager(C)->windows.first);
          }

          BKE_report_print_level_set(handler->op->reports, RPT_WARNING);
          UI_popup_menu_reports(C, handler->op->reports);

          /* XXX - copied from 'wm_operator_finished()' */
          /* add reports to the global list, otherwise they are not seen */
          BLI_movelisttolist(&CTX_wm_reports(C)->list, &handler->op->reports->list);

          /* more hacks, since we meddle with reports, banner display doesn't happen automatic */
          WM_report_banner_show();

          CTX_wm_window_set(C, win_prev);
          CTX_wm_area_set(C, area_prev);
          CTX_wm_region_set(C, ar_prev);
        }

        /* for WM_operator_pystring only, custom report handling is done above */
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
    /* From 'wm_handlers_do_intern' */
    bContext *C,
    wmEvent *event,
    ListBase *handlers,
    wmEventHandler_Keymap *handler,
    /* Additional. */
    wmKeyMap *keymap,
    const bool do_debug_handler)
{
  int action = WM_HANDLER_CONTINUE;

  PRINT("%s:   checking '%s' ...", __func__, keymap->idname);

  if (keymap == NULL) {
    /* Only callback is allowed to have NULL keymaps. */
    BLI_assert(handler->dynamic.keymap_fn);
  }
  else {
    if (WM_keymap_poll(C, keymap)) {

      PRINT("pass\n");

      for (wmKeyMapItem *kmi = keymap->items.first; kmi; kmi = kmi->next) {
        if (wm_eventmatch(event, kmi)) {
          struct wmEventHandler_KeymapPost keymap_post = handler->post;

          PRINT("%s:     item matched '%s'\n", __func__, kmi->idname);

          /* weak, but allows interactive callback to not use rawkey */
          event->keymap_idname = kmi->idname;

          action |= wm_handler_operator_call(C, handlers, &handler->head, event, kmi->ptr);

          if (action & WM_HANDLER_BREAK) {
            /* not always_pass here, it denotes removed handler_base */
            CLOG_INFO(WM_LOG_HANDLERS, 2, "handled! '%s'", kmi->idname);
            if (keymap_post.post_fn != NULL) {
              keymap_post.post_fn(keymap, kmi, keymap_post.user_data);
            }
            break;
          }
          else {
            if (action & WM_HANDLER_HANDLED) {
              CLOG_INFO(WM_LOG_HANDLERS, 2, "handled - and pass on! '%s'", kmi->idname);
            }
            else {
              CLOG_INFO(WM_LOG_HANDLERS, 2, "un-handled '%s'", kmi->idname);
            }
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

static bool wm_handlers_do_keymap_with_gizmo_handler(
    /* From 'wm_handlers_do_intern' */
    bContext *C,
    wmEvent *event,
    ListBase *handlers,
    wmEventHandler_Gizmo *handler,
    /* Additional. */
    wmGizmoGroup *gzgroup,
    wmKeyMap *keymap,
    const bool do_debug_handler)
{
  int action = WM_HANDLER_CONTINUE;
  wmKeyMapItem *kmi;

  PRINT("%s:   checking '%s' ...", __func__, keymap->idname);

  if (WM_keymap_poll(C, keymap)) {
    PRINT("pass\n");
    for (kmi = keymap->items.first; kmi; kmi = kmi->next) {
      if (wm_eventmatch(event, kmi)) {
        PRINT("%s:     item matched '%s'\n", __func__, kmi->idname);

        /* weak, but allows interactive callback to not use rawkey */
        event->keymap_idname = kmi->idname;

        CTX_wm_gizmo_group_set(C, gzgroup);

        /* handler->op is called later, we want keymap op to be triggered here */
        action |= wm_handler_operator_call(C, handlers, &handler->head, event, kmi->ptr);

        CTX_wm_gizmo_group_set(C, NULL);

        if (action & WM_HANDLER_BREAK) {
          if (G.debug & (G_DEBUG_EVENTS | G_DEBUG_HANDLERS)) {
            printf("%s:       handled - and pass on! '%s'\n", __func__, kmi->idname);
          }
          break;
        }
        else {
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
  }
  else {
    PRINT("fail\n");
  }
  return action;
}

static int wm_handlers_do_intern(bContext *C, wmEvent *event, ListBase *handlers)
{
  const bool do_debug_handler =
      (G.debug & G_DEBUG_HANDLERS) &&
      /* comment this out to flood the console! (if you really want to test) */
      !ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE);

  wmWindowManager *wm = CTX_wm_manager(C);
  int action = WM_HANDLER_CONTINUE;
  int always_pass;

  if (handlers == NULL) {
    return action;
  }

  /* modal handlers can get removed in this loop, we keep the loop this way
   *
   * note: check 'handlers->first' because in rare cases the handlers can be cleared
   * by the event that's called, for eg:
   *
   * Calling a python script which changes the area.type, see [#32232] */
  for (wmEventHandler *handler_base = handlers->first, *handler_base_next;
       handler_base && handlers->first;
       handler_base = handler_base_next) {
    handler_base_next = handler_base->next;

    /* during this loop, ui handlers for nested menus can tag multiple handlers free */
    if (handler_base->flag & WM_HANDLER_DO_FREE) {
      /* pass */
    }
    else if (handler_base->poll == NULL || handler_base->poll(CTX_wm_region(C), event)) {
      /* in advance to avoid access to freed event on window close */
      always_pass = wm_event_always_pass(event);

      /* modal+blocking handler_base */
      if (handler_base->flag & WM_HANDLER_BLOCKING) {
        action |= WM_HANDLER_BREAK;
      }

      /* Handle all types here. */
      if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
        wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
        wmKeyMap *keymap = WM_event_get_keymap_from_handler(wm, handler);
        action |= wm_handlers_do_keymap_with_keymap_handler(
            C, event, handlers, handler, keymap, do_debug_handler);
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
          wmDropBox *drop = handler->dropboxes->first;
          for (; drop; drop = drop->next) {
            /* other drop custom types allowed */
            if (event->custom == EVT_DATA_DRAGDROP) {
              ListBase *lb = (ListBase *)event->customdata;
              wmDrag *drag;

              for (drag = lb->first; drag; drag = drag->next) {
                const char *tooltip = NULL;
                if (drop->poll(C, drag, event, &tooltip)) {
                  /* Optionally copy drag information to operator properties. */
                  if (drop->copy) {
                    drop->copy(drag, drop);
                  }

                  /* Pass single matched wmDrag onto the operator. */
                  BLI_remlink(lb, drag);
                  ListBase single_lb = {drag, drag};
                  event->customdata = &single_lb;

                  wm_operator_call_internal(
                      C, drop->ot, drop->ptr, NULL, drop->opcontext, false, event);
                  action |= WM_HANDLER_BREAK;

                  /* free the drags */
                  WM_drag_free_list(lb);
                  WM_drag_free_list(&single_lb);

                  event->customdata = NULL;
                  event->custom = 0;

                  /* XXX fileread case */
                  if (CTX_wm_window(C) == NULL) {
                    return action;
                  }

                  /* escape from drag loop, got freed */
                  break;
                }
              }
            }
          }
        }
      }
      else if (handler_base->type == WM_HANDLER_TYPE_GIZMO) {
        wmEventHandler_Gizmo *handler = (wmEventHandler_Gizmo *)handler_base;
        ScrArea *area = CTX_wm_area(C);
        ARegion *region = CTX_wm_region(C);
        wmGizmoMap *gzmap = handler->gizmo_map;
        BLI_assert(gzmap != NULL);
        wmGizmo *gz = wm_gizmomap_highlight_get(gzmap);

        if (region->gizmo_map != handler->gizmo_map) {
          WM_gizmomap_tag_refresh(handler->gizmo_map);
        }

        wm_gizmomap_handler_context_gizmo(C, handler);
        wm_region_mouse_co(C, event);

        /* Drag events use the previous click location to highlight the gizmos,
         * Get the highlight again in case the user dragged off the gizmo. */
        const bool is_event_drag = ISTWEAK(event->type) || (event->val == KM_CLICK_DRAG);

        bool handle_highlight = false;
        bool handle_keymap = false;

        /* handle gizmo highlighting */
        if (!wm_gizmomap_modal_get(gzmap) && ((event->type == MOUSEMOVE) || is_event_drag)) {
          handle_highlight = true;
          if (is_event_drag) {
            handle_keymap = true;
          }
        }
        else {
          handle_keymap = true;
        }

        if (handle_highlight) {
          int part;
          gz = wm_gizmomap_highlight_find(gzmap, C, event, &part);
          if (wm_gizmomap_highlight_set(gzmap, C, gz, part) && gz != NULL) {
            if (U.flag & USER_TOOLTIPS) {
              WM_tooltip_timer_init(C, CTX_wm_window(C), region, WM_gizmomap_tooltip_init);
            }
          }
        }

        if (handle_keymap) {
          /* Handle highlight gizmo. */
          if (gz != NULL) {
            wmGizmoGroup *gzgroup = gz->parent_gzgroup;
            wmKeyMap *keymap = WM_keymap_active(wm,
                                                gz->keymap ? gz->keymap : gzgroup->type->keymap);
            action |= wm_handlers_do_keymap_with_gizmo_handler(
                C, event, handlers, handler, gzgroup, keymap, do_debug_handler);
          }

          /* Don't use from now on. */
          gz = NULL;

          /* Fallback to selected gizmo (when un-handled). */
          if ((action & WM_HANDLER_BREAK) == 0) {
            if (WM_gizmomap_is_any_selected(gzmap)) {
              const ListBase *groups = WM_gizmomap_group_list(gzmap);
              for (wmGizmoGroup *gzgroup = groups->first; gzgroup; gzgroup = gzgroup->next) {
                if (wm_gizmogroup_is_any_selected(gzgroup)) {
                  wmKeyMap *keymap = WM_keymap_active(wm, gzgroup->type->keymap);
                  action |= wm_handlers_do_keymap_with_gizmo_handler(
                      C, event, handlers, handler, gzgroup, keymap, do_debug_handler);
                  if (action & WM_HANDLER_BREAK) {
                    break;
                  }
                }
              }
            }
          }
        }

        /* restore the area */
        CTX_wm_area_set(C, area);
        CTX_wm_region_set(C, region);
      }
      else if (handler_base->type == WM_HANDLER_TYPE_OP) {
        wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
        if (handler->is_fileselect) {
          if (!wm->is_interface_locked) {
            /* screen context changes here */
            action |= wm_handler_fileselect_call(C, handlers, handler, event);
          }
        }
        else {
          action |= wm_handler_operator_call(C, handlers, handler_base, event, NULL);
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

    /* XXX code this for all modal ops, and ensure free only happens here */

    /* modal ui handler can be tagged to be freed */
    if (BLI_findindex(handlers, handler_base) !=
        -1) { /* could be freed already by regular modal ops */
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

/* this calls handlers twice - to solve (double-)click events */
static int wm_handlers_do(bContext *C, wmEvent *event, ListBase *handlers)
{
  int action = wm_handlers_do_intern(C, event, handlers);

  /* fileread case */
  if (CTX_wm_window(C) == NULL) {
    return action;
  }

  if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {

    /* Test for CLICK_DRAG events. */
    if (wm_action_not_handled(action)) {
      if (event->check_drag) {
        wmWindow *win = CTX_wm_window(C);
        if ((abs(event->x - win->eventstate->prevclickx)) >=
                WM_EVENT_CURSOR_CLICK_DRAG_THRESHOLD ||
            (abs(event->y - win->eventstate->prevclicky)) >=
                WM_EVENT_CURSOR_CLICK_DRAG_THRESHOLD) {
          int x = event->x;
          int y = event->y;
          short val = event->val;
          short type = event->type;

          event->x = win->eventstate->prevclickx;
          event->y = win->eventstate->prevclicky;
          event->val = KM_CLICK_DRAG;
          event->type = win->eventstate->type;

          CLOG_INFO(WM_LOG_HANDLERS, 1, "handling PRESS_DRAG");

          action |= wm_handlers_do_intern(C, event, handlers);

          event->val = val;
          event->type = type;
          event->x = x;
          event->y = y;

          win->eventstate->check_click = false;
          win->eventstate->check_drag = false;
        }
      }
    }
    else {
      wmWindow *win = CTX_wm_window(C);
      if (win) {
        win->eventstate->check_drag = false;
      }
    }
  }
  else if (ISMOUSE_BUTTON(event->type) || ISKEYBOARD(event->type)) {
    /* All events that don't set wmEvent.prevtype must be ignored. */

    /* Test for CLICK events. */
    if (wm_action_not_handled(action)) {
      wmWindow *win = CTX_wm_window(C);

      /* eventstate stores if previous event was a KM_PRESS, in case that
       * wasn't handled, the KM_RELEASE will become a KM_CLICK */

      if (win != NULL) {
        if (event->val == KM_PRESS) {
          win->eventstate->check_click = true;
          win->eventstate->check_drag = true;
        }
        else if (event->val == KM_RELEASE) {
          win->eventstate->check_drag = false;
        }
      }

      if (win && win->eventstate->prevtype == event->type) {

        if ((event->val == KM_RELEASE) && (win->eventstate->prevval == KM_PRESS) &&
            (win->eventstate->check_click == true)) {
          if ((abs(event->x - win->eventstate->prevclickx)) < WM_EVENT_CLICK_TWEAK_THRESHOLD &&
              (abs(event->y - win->eventstate->prevclicky)) < WM_EVENT_CLICK_TWEAK_THRESHOLD) {
            /* Position is where the actual click happens, for more
             * accurate selecting in case the mouse drifts a little. */
            int x = event->x;
            int y = event->y;

            event->x = win->eventstate->prevclickx;
            event->y = win->eventstate->prevclicky;
            event->val = KM_CLICK;

            CLOG_INFO(WM_LOG_HANDLERS, 1, "handling CLICK");

            action |= wm_handlers_do_intern(C, event, handlers);

            event->val = KM_RELEASE;
            event->x = x;
            event->y = y;
          }
          else {
            win->eventstate->check_click = 0;
            win->eventstate->check_drag = 0;
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
      wmWindow *win = CTX_wm_window(C);
      if (win) {
        win->eventstate->check_click = 0;
      }
    }
  }

  return action;
}

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

static bool wm_event_inside_region(const wmEvent *event, const ARegion *ar)
{
  if (wm_event_always_pass(event)) {
    return true;
  }
  return ED_region_contains_xy(ar, &event->x);
}

static ScrArea *area_event_inside(bContext *C, const int xy[2])
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);

  if (screen) {
    ED_screen_areas_iter(win, screen, sa)
    {
      if (BLI_rcti_isect_pt_v(&sa->totrct, xy)) {
        return sa;
      }
    }
  }
  return NULL;
}

static ARegion *region_event_inside(bContext *C, const int xy[2])
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *ar;

  if (screen && area) {
    for (ar = area->regionbase.first; ar; ar = ar->next) {
      if (BLI_rcti_isect_pt_v(&ar->winrct, xy)) {
        return ar;
      }
    }
  }
  return NULL;
}

static void wm_paintcursor_tag(bContext *C, wmPaintCursor *pc, ARegion *ar)
{
  if (ar) {
    for (; pc; pc = pc->next) {
      if (pc->poll == NULL || pc->poll(C)) {
        wmWindow *win = CTX_wm_window(C);
        WM_paint_cursor_tag_redraw(win, ar);
      }
    }
  }
}

/* called on mousemove, check updates for paintcursors */
/* context was set on active area and region */
static void wm_paintcursor_test(bContext *C, const wmEvent *event)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  if (wm->paintcursors.first) {
    ARegion *ar = CTX_wm_region(C);

    if (ar) {
      wm_paintcursor_tag(C, wm->paintcursors.first, ar);
    }

    /* if previous position was not in current region, we have to set a temp new context */
    if (ar == NULL || !BLI_rcti_isect_pt_v(&ar->winrct, &event->prevx)) {
      ScrArea *sa = CTX_wm_area(C);

      CTX_wm_area_set(C, area_event_inside(C, &event->prevx));
      CTX_wm_region_set(C, region_event_inside(C, &event->prevx));

      wm_paintcursor_tag(C, wm->paintcursors.first, CTX_wm_region(C));

      CTX_wm_area_set(C, sa);
      CTX_wm_region_set(C, ar);
    }
  }
}

static void wm_event_drag_test(wmWindowManager *wm, wmWindow *win, wmEvent *event)
{
  bScreen *screen = WM_window_get_active_screen(win);

  if (BLI_listbase_is_empty(&wm->drags)) {
    return;
  }

  if (event->type == MOUSEMOVE || ISKEYMODIFIER(event->type)) {
    screen->do_draw_drag = true;
  }
  else if (event->type == ESCKEY) {
    WM_drag_free_list(&wm->drags);

    screen->do_draw_drag = true;
  }
  else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    event->type = EVT_DROP;

    /* create customdata, first free existing */
    if (event->customdata) {
      if (event->customdatafree) {
        MEM_freeN(event->customdata);
      }
    }

    event->custom = EVT_DATA_DRAGDROP;
    event->customdata = &wm->drags;
    event->customdatafree = 1;

    /* clear drop icon */
    screen->do_draw_drag = true;

    /* restore cursor (disabled, see wm_dragdrop.c) */
    // WM_cursor_modal_restore(win);
  }
}

/* filter out all events of the pie that spawned the last pie unless it's a release event */
static bool wm_event_pie_filter(wmWindow *win, const wmEvent *event)
{
  if (win->lock_pie_event && win->lock_pie_event == event->type) {
    if (event->val == KM_RELEASE) {
      win->lock_pie_event = EVENT_NONE;
      return false;
    }
    else {
      return true;
    }
  }
  else {
    return false;
  }
}

/* called in main loop */
/* goes over entire hierarchy:  events -> window -> screen -> area -> region */
void wm_event_do_handlers(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win;

  /* update key configuration before handling events */
  WM_keyconfig_update(wm);
  WM_gizmoconfig_update(CTX_data_main(C));

  for (win = wm->windows.first; win; win = win->next) {
    bScreen *screen = WM_window_get_active_screen(win);
    wmEvent *event;

    /* some safety checks - these should always be set! */
    BLI_assert(WM_window_get_active_scene(win));
    BLI_assert(WM_window_get_active_screen(win));
    BLI_assert(WM_window_get_active_workspace(win));

    if (screen == NULL) {
      wm_event_free_all(win);
    }
    else {
      Scene *scene = WM_window_get_active_scene(win);

      if (scene) {
        int is_playing_sound = BKE_sound_scene_playing(scene);

        if (is_playing_sound != -1) {
          bool is_playing_screen;
          CTX_wm_window_set(C, win);
          CTX_data_scene_set(C, scene);

          is_playing_screen = (ED_screen_animation_playing(wm) != NULL);

          if (((is_playing_sound == 1) && (is_playing_screen == 0)) ||
              ((is_playing_sound == 0) && (is_playing_screen == 1))) {
            ED_screen_animation_play(C, -1, 1);
          }

          if (is_playing_sound == 0) {
            const float time = BKE_sound_sync_scene(scene);
            if (isfinite(time)) {
              int ncfra = time * (float)FPS + 0.5f;
              if (ncfra != scene->r.cfra) {
                scene->r.cfra = ncfra;
                Depsgraph *depsgraph = CTX_data_depsgraph(C);
                ED_update_for_newframe(CTX_data_main(C), depsgraph);
                WM_event_add_notifier(C, NC_WINDOW, NULL);
              }
            }
          }

          CTX_data_scene_set(C, NULL);
          CTX_wm_screen_set(C, NULL);
          CTX_wm_window_set(C, NULL);
        }
      }
    }

    while ((event = win->queue.first)) {
      int action = WM_HANDLER_CONTINUE;

      /* active screen might change during handlers, update pointer */
      screen = WM_window_get_active_screen(win);

      if (G.debug & (G_DEBUG_HANDLERS | G_DEBUG_EVENTS) &&
          !ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
        printf("\n%s: Handling event\n", __func__);
        WM_event_print(event);
      }

      /* take care of pie event filter */
      if (wm_event_pie_filter(win, event)) {
        if (!ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
          CLOG_INFO(WM_LOG_HANDLERS, 1, "event filtered due to pie button pressed");
        }
        BLI_remlink(&win->queue, event);
        wm_event_free(event);
        continue;
      }

      CTX_wm_window_set(C, win);

      /* Clear tool-tip on mouse move. */
      if (screen->tool_tip && screen->tool_tip->exit_on_event) {
        if (ISMOUSE(event->type)) {
          WM_tooltip_clear(C, win);
        }
      }

      /* we let modal handlers get active area/region, also wm_paintcursor_test needs it */
      CTX_wm_area_set(C, area_event_inside(C, &event->x));
      CTX_wm_region_set(C, region_event_inside(C, &event->x));

      /* MVC demands to not draw in event handlers...
       * but we need to leave it for ogl selecting etc. */
      wm_window_make_drawable(wm, win);

      wm_region_mouse_co(C, event);

      /* first we do priority handlers, modal + some limited keymaps */
      action |= wm_handlers_do(C, event, &win->modalhandlers);

      /* fileread case */
      if (CTX_wm_window(C) == NULL) {
        return;
      }

      /* check for a tooltip */
      if (screen == WM_window_get_active_screen(win)) {
        if (screen->tool_tip && screen->tool_tip->timer) {
          if ((event->type == TIMER) && (event->customdata == screen->tool_tip->timer)) {
            WM_tooltip_init(C, win);
          }
        }
      }

      /* check dragging, creates new event or frees, adds draw tag */
      wm_event_drag_test(wm, win, event);

      /* builtin tweak, if action is break it removes tweak */
      wm_tweakevent_test(C, event, action);

      if ((action & WM_HANDLER_BREAK) == 0) {
        ARegion *ar;

        /* Note: setting subwin active should be done here, after modal handlers have been done */
        if (event->type == MOUSEMOVE) {
          /* State variables in screen, cursors.
           * Also used in wm_draw.c, fails for modal handlers though. */
          ED_screen_set_active_region(C, win, &event->x);
          /* for regions having custom cursors */
          wm_paintcursor_test(C, event);
        }
#ifdef WITH_INPUT_NDOF
        else if (event->type == NDOF_MOTION) {
          win->addmousemove = true;
        }
#endif

        ED_screen_areas_iter(win, screen, sa)
        {
          /* after restoring a screen from SCREENMAXIMIZED we have to wait
           * with the screen handling till the region coordinates are updated */
          if (screen->skip_handling == true) {
            /* restore for the next iteration of wm_event_do_handlers */
            screen->skip_handling = false;
            break;
          }

          /* update azones if needed - done here because it needs to be independent from redraws */
          if (sa->flag & AREA_FLAG_ACTIONZONES_UPDATE) {
            ED_area_azones_update(sa, &event->x);
          }

          if (wm_event_inside_rect(event, &sa->totrct)) {
            CTX_wm_area_set(C, sa);

            if ((action & WM_HANDLER_BREAK) == 0) {
              for (ar = sa->regionbase.first; ar; ar = ar->next) {
                if (wm_event_inside_region(event, ar)) {

                  CTX_wm_region_set(C, ar);

                  /* call even on non mouse events, since the */
                  wm_region_mouse_co(C, event);

                  if (!BLI_listbase_is_empty(&wm->drags)) {
                    /* does polls for drop regions and checks uibuts */
                    /* need to be here to make sure region context is true */
                    if (ELEM(event->type, MOUSEMOVE, EVT_DROP) || ISKEYMODIFIER(event->type)) {
                      wm_drags_check_ops(C, event);
                    }
                  }

                  action |= wm_handlers_do(C, event, &ar->handlers);

                  /* fileread case (python), [#29489] */
                  if (CTX_wm_window(C) == NULL) {
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
              wm_region_mouse_co(C, event); /* only invalidates event->mval in this case */
              action |= wm_handlers_do(C, event, &sa->handlers);
            }
            CTX_wm_area_set(C, NULL);

            /* NOTE: do not escape on WM_HANDLER_BREAK,
             * mousemove needs handled for previous area. */
          }
        }

        if ((action & WM_HANDLER_BREAK) == 0) {
          /* also some non-modal handlers need active area/region */
          CTX_wm_area_set(C, area_event_inside(C, &event->x));
          CTX_wm_region_set(C, region_event_inside(C, &event->x));

          wm_region_mouse_co(C, event);

          action |= wm_handlers_do(C, event, &win->handlers);

          /* fileread case */
          if (CTX_wm_window(C) == NULL) {
            return;
          }
        }
      }

      /* If press was handled, we don't want to do click. This way
       * press in tool keymap can override click in editor keymap.*/
      if (ISMOUSE_BUTTON(event->type) && event->val == KM_PRESS &&
          !wm_action_not_handled(action)) {
        win->eventstate->check_click = false;
      }

      /* update previous mouse position for following events to use */
      win->eventstate->prevx = event->x;
      win->eventstate->prevy = event->y;

      /* unlink and free here, blender-quit then frees all */
      BLI_remlink(&win->queue, event);
      wm_event_free(event);
    }

    /* only add mousemove when queue was read entirely */
    if (win->addmousemove && win->eventstate) {
      wmEvent tevent = *(win->eventstate);
      // printf("adding MOUSEMOVE %d %d\n", tevent.x, tevent.y);
      tevent.type = MOUSEMOVE;
      tevent.prevx = tevent.x;
      tevent.prevy = tevent.y;
      wm_event_add(win, &tevent);
      win->addmousemove = 0;
    }

    CTX_wm_window_set(C, NULL);
  }

  /* update key configuration after handling events */
  WM_keyconfig_update(wm);
  WM_gizmoconfig_update(CTX_data_main(C));
}

/* ********** filesector handling ************ */

void WM_event_fileselect_event(wmWindowManager *wm, void *ophandle, int eventval)
{
  /* add to all windows! */
  wmWindow *win;

  for (win = wm->windows.first; win; win = win->next) {
    wmEvent event = *win->eventstate;

    event.type = EVT_FILESELECT;
    event.val = eventval;
    event.customdata = ophandle;  // only as void pointer type check

    wm_event_add(win, &event);
  }
}

/* operator is supposed to have a filled "path" property */
/* optional property: filetype (XXX enum?) */

/**
 * The idea here is to keep a handler alive on window queue, owning the operator.
 * The filewindow can send event to make it execute, thus ensuring
 * executing happens outside of lower level queues, with UI refreshed.
 * Should also allow multiwin solutions
 */
void WM_event_add_fileselect(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);

  /* Close any popups, like when opening a file browser from the splash. */
  UI_popup_handlers_remove_all(C, &win->modalhandlers);

  /* only allow 1 file selector open per window */
  LISTBASE_FOREACH_MUTABLE (wmEventHandler *, handler_base, &win->modalhandlers) {
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
      if (handler->is_fileselect == false) {
        continue;
      }
      bScreen *screen = CTX_wm_screen(C);
      bool cancel_handler = true;

      /* find the area with the file selector for this handler */
      ED_screen_areas_iter(win, screen, sa)
      {
        if (sa->spacetype == SPACE_FILE) {
          SpaceFile *sfile = sa->spacedata.first;

          if (sfile->op == handler->op) {
            CTX_wm_area_set(C, sa);
            wm_handler_fileselect_do(C, &win->modalhandlers, handler, EVT_FILESELECT_CANCEL);
            cancel_handler = false;
            break;
          }
        }
      }

      /* if not found we stop the handler without changing the screen */
      if (cancel_handler) {
        wm_handler_fileselect_do(C, &win->modalhandlers, handler, EVT_FILESELECT_EXTERNAL_CANCEL);
      }
    }
  }

  wmEventHandler_Op *handler = MEM_callocN(sizeof(*handler), __func__);
  handler->head.type = WM_HANDLER_TYPE_OP;

  handler->is_fileselect = true;
  handler->op = op;
  handler->context.area = CTX_wm_area(C);
  handler->context.region = CTX_wm_region(C);

  BLI_addhead(&win->modalhandlers, handler);

  /* check props once before invoking if check is available
   * ensures initial properties are valid */
  if (op->type->check) {
    op->type->check(C, op); /* ignore return value */
  }

  WM_event_fileselect_event(wm, op, EVT_FILESELECT_FULL_OPEN);
}

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

  /* operator was part of macro */
  if (op->opm) {
    /* give the mother macro to the handler */
    handler->op = op->opm;
    /* mother macro opm becomes the macro element */
    handler->op->opm = op;
  }
  else {
    handler->op = op;
  }

  handler->context.area = CTX_wm_area(C); /* means frozen screen context for modal handlers! */
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

  /* only allow same keymap once */
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

/** Follow #wmEventHandler_KeymapDynamicFn signiture. */
wmKeyMap *WM_event_get_keymap_from_toolsystem(wmWindowManager *wm, wmEventHandler_Keymap *handler)
{
  ScrArea *sa = handler->dynamic.user_data;
  handler->keymap_tool = NULL;
  bToolRef_Runtime *tref_rt = sa->runtime.tool ? sa->runtime.tool->runtime : NULL;
  if (tref_rt && tref_rt->keymap[0]) {
    wmKeyMap *km = WM_keymap_list_find_spaceid_or_empty(
        &wm->userconf->keymaps, tref_rt->keymap, sa->spacetype, RGN_TYPE_WINDOW);
    /* We shouldn't use keymaps from unrelated spaces. */
    if (km != NULL) {
      handler->keymap_tool = sa->runtime.tool;
      return km;
    }
    else {
      printf("Keymap: '%s' not found for tool '%s'\n", tref_rt->keymap, sa->runtime.tool->idname);
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

  /* only allow same keymap once */
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

/* priorities not implemented yet, for time being just insert in begin of list */
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
  else if (event->type == MOUSEMOVE && BLI_rcti_isect_pt(rect, event->prevx, event->prevy)) {
    return true;
  }
  else {
    return false;
  }
}

static bool handler_region_v2d_mask_test(const ARegion *ar, const wmEvent *event)
{
  rcti rect = ar->v2d.mask;
  BLI_rcti_translate(&rect, ar->winrct.xmin, ar->winrct.ymin);
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

/* set "postpone" for win->modalhandlers, this is in a running for () loop in wm_handlers_do() */
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
        /* handlers will be freed in wm_handlers_do() */
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
  /* only allow same dropbox once */
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

  /* dropbox stored static, no free or copy */
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

void WM_event_add_mousemove(const bContext *C)
{
  wmWindow *window = CTX_wm_window(C);

  window->addmousemove = 1;
}

/* for modal callbacks, check configuration for how to interpret exit with tweaks  */
bool WM_event_is_modal_tweak_exit(const wmEvent *event, int tweak_event)
{
  /* if the release-confirm userpref setting is enabled,
   * tweak events can be canceled when mouse is released
   */
  if (U.flag & USER_RELEASECONFIRM) {
    /* option on, so can exit with km-release */
    if (event->val == KM_RELEASE) {
      switch (tweak_event) {
        case EVT_TWEAK_L:
        case EVT_TWEAK_M:
        case EVT_TWEAK_R:
          return 1;
      }
    }
    else {
      /* if the initial event wasn't a tweak event then
       * ignore USER_RELEASECONFIRM setting: see [#26756] */
      if (ELEM(tweak_event, EVT_TWEAK_L, EVT_TWEAK_M, EVT_TWEAK_R) == 0) {
        return 1;
      }
    }
  }
  else {
    /* this is fine as long as not doing km-release, otherwise
     * some items (i.e. markers) being tweaked may end up getting
     * dropped all over
     */
    if (event->val != KM_RELEASE) {
      return 1;
    }
  }

  return 0;
}

bool WM_event_type_mask_test(const int event_type, const enum eEventType_Mask mask)
{
  /* Keyboard. */
  if (mask & EVT_TYPE_MASK_KEYBOARD) {
    if (ISKEYBOARD(event_type)) {
      return true;
    }
  }
  else if (mask & EVT_TYPE_MASK_KEYBOARD_MODIFIER) {
    if (ISKEYMODIFIER(event_type)) {
      return true;
    }
  }

  /* Mouse. */
  if (mask & EVT_TYPE_MASK_MOUSE) {
    if (ISMOUSE(event_type)) {
      return true;
    }
  }
  else if (mask & EVT_TYPE_MASK_MOUSE_WHEEL) {
    if (ISMOUSE_WHEEL(event_type)) {
      return true;
    }
  }
  else if (mask & EVT_TYPE_MASK_MOUSE_GESTURE) {
    if (ISMOUSE_GESTURE(event_type)) {
      return true;
    }
  }

  /* Tweak. */
  if (mask & EVT_TYPE_MASK_TWEAK) {
    if (ISTWEAK(event_type)) {
      return true;
    }
  }

  /* Action Zone. */
  if (mask & EVT_TYPE_MASK_ACTIONZONE) {
    if (IS_EVENT_ACTIONZONE(event_type)) {
      return true;
    }
  }

  return false;
}

/* ********************* ghost stuff *************** */

static int convert_key(GHOST_TKey key)
{
  if (key >= GHOST_kKeyA && key <= GHOST_kKeyZ) {
    return (AKEY + ((int)key - GHOST_kKeyA));
  }
  else if (key >= GHOST_kKey0 && key <= GHOST_kKey9) {
    return (ZEROKEY + ((int)key - GHOST_kKey0));
  }
  else if (key >= GHOST_kKeyNumpad0 && key <= GHOST_kKeyNumpad9) {
    return (PAD0 + ((int)key - GHOST_kKeyNumpad0));
  }
  else if (key >= GHOST_kKeyF1 && key <= GHOST_kKeyF19) {
    return (F1KEY + ((int)key - GHOST_kKeyF1));
  }
  else {
    switch (key) {
      case GHOST_kKeyBackSpace:
        return BACKSPACEKEY;
      case GHOST_kKeyTab:
        return TABKEY;
      case GHOST_kKeyLinefeed:
        return LINEFEEDKEY;
      case GHOST_kKeyClear:
        return 0;
      case GHOST_kKeyEnter:
        return RETKEY;

      case GHOST_kKeyEsc:
        return ESCKEY;
      case GHOST_kKeySpace:
        return SPACEKEY;
      case GHOST_kKeyQuote:
        return QUOTEKEY;
      case GHOST_kKeyComma:
        return COMMAKEY;
      case GHOST_kKeyMinus:
        return MINUSKEY;
      case GHOST_kKeyPlus:
        return PLUSKEY;
      case GHOST_kKeyPeriod:
        return PERIODKEY;
      case GHOST_kKeySlash:
        return SLASHKEY;

      case GHOST_kKeySemicolon:
        return SEMICOLONKEY;
      case GHOST_kKeyEqual:
        return EQUALKEY;

      case GHOST_kKeyLeftBracket:
        return LEFTBRACKETKEY;
      case GHOST_kKeyRightBracket:
        return RIGHTBRACKETKEY;
      case GHOST_kKeyBackslash:
        return BACKSLASHKEY;
      case GHOST_kKeyAccentGrave:
        return ACCENTGRAVEKEY;

      case GHOST_kKeyLeftShift:
        return LEFTSHIFTKEY;
      case GHOST_kKeyRightShift:
        return RIGHTSHIFTKEY;
      case GHOST_kKeyLeftControl:
        return LEFTCTRLKEY;
      case GHOST_kKeyRightControl:
        return RIGHTCTRLKEY;
      case GHOST_kKeyOS:
        return OSKEY;
      case GHOST_kKeyLeftAlt:
        return LEFTALTKEY;
      case GHOST_kKeyRightAlt:
        return RIGHTALTKEY;

      case GHOST_kKeyCapsLock:
        return CAPSLOCKKEY;
      case GHOST_kKeyNumLock:
        return 0;
      case GHOST_kKeyScrollLock:
        return 0;

      case GHOST_kKeyLeftArrow:
        return LEFTARROWKEY;
      case GHOST_kKeyRightArrow:
        return RIGHTARROWKEY;
      case GHOST_kKeyUpArrow:
        return UPARROWKEY;
      case GHOST_kKeyDownArrow:
        return DOWNARROWKEY;

      case GHOST_kKeyPrintScreen:
        return 0;
      case GHOST_kKeyPause:
        return PAUSEKEY;

      case GHOST_kKeyInsert:
        return INSERTKEY;
      case GHOST_kKeyDelete:
        return DELKEY;
      case GHOST_kKeyHome:
        return HOMEKEY;
      case GHOST_kKeyEnd:
        return ENDKEY;
      case GHOST_kKeyUpPage:
        return PAGEUPKEY;
      case GHOST_kKeyDownPage:
        return PAGEDOWNKEY;

      case GHOST_kKeyNumpadPeriod:
        return PADPERIOD;
      case GHOST_kKeyNumpadEnter:
        return PADENTER;
      case GHOST_kKeyNumpadPlus:
        return PADPLUSKEY;
      case GHOST_kKeyNumpadMinus:
        return PADMINUS;
      case GHOST_kKeyNumpadAsterisk:
        return PADASTERKEY;
      case GHOST_kKeyNumpadSlash:
        return PADSLASHKEY;

      case GHOST_kKeyGrLess:
        return GRLESSKEY;

      case GHOST_kKeyMediaPlay:
        return MEDIAPLAY;
      case GHOST_kKeyMediaStop:
        return MEDIASTOP;
      case GHOST_kKeyMediaFirst:
        return MEDIAFIRST;
      case GHOST_kKeyMediaLast:
        return MEDIALAST;

      default:
        return UNKNOWNKEY; /* GHOST_kKeyUnknown */
    }
  }
}

static void wm_eventemulation(wmEvent *event, bool test_only)
{
  /* Store last mmb/rmb event value to make emulation work when modifier keys
   * are released first. This really should be in a data structure somewhere. */
  static int emulating_event = EVENT_NONE;

  /* middlemouse and rightmouse emulation */
  if (U.flag & USER_TWOBUTTONMOUSE) {
    if (event->type == LEFTMOUSE) {

      if (event->val == KM_PRESS && event->alt) {
        event->type = MIDDLEMOUSE;
        event->alt = 0;

        if (!test_only) {
          emulating_event = MIDDLEMOUSE;
        }
      }
#ifdef __APPLE__
      else if (event->val == KM_PRESS && event->oskey) {
        event->type = RIGHTMOUSE;
        event->oskey = 0;

        if (!test_only) {
          emulating_event = RIGHTMOUSE;
        }
      }
#endif
      else if (event->val == KM_RELEASE) {
        /* only send middle-mouse release if emulated */
        if (emulating_event == MIDDLEMOUSE) {
          event->type = MIDDLEMOUSE;
          event->alt = 0;
        }
        else if (emulating_event == RIGHTMOUSE) {
          event->type = RIGHTMOUSE;
          event->oskey = 0;
        }

        if (!test_only) {
          emulating_event = EVENT_NONE;
        }
      }
    }
  }

  /* numpad emulation */
  if (U.flag & USER_NONUMPAD) {
    switch (event->type) {
      case ZEROKEY:
        event->type = PAD0;
        break;
      case ONEKEY:
        event->type = PAD1;
        break;
      case TWOKEY:
        event->type = PAD2;
        break;
      case THREEKEY:
        event->type = PAD3;
        break;
      case FOURKEY:
        event->type = PAD4;
        break;
      case FIVEKEY:
        event->type = PAD5;
        break;
      case SIXKEY:
        event->type = PAD6;
        break;
      case SEVENKEY:
        event->type = PAD7;
        break;
      case EIGHTKEY:
        event->type = PAD8;
        break;
      case NINEKEY:
        event->type = PAD9;
        break;
      case MINUSKEY:
        event->type = PADMINUS;
        break;
      case EQUALKEY:
        event->type = PADPLUSKEY;
        break;
      case BACKSLASHKEY:
        event->type = PADSLASHKEY;
        break;
    }
  }
}

/* applies the global tablet pressure correction curve */
float wm_pressure_curve(float pressure)
{
  if (U.pressure_threshold_max != 0.0f) {
    pressure /= U.pressure_threshold_max;
  }

  CLAMP(pressure, 0.0f, 1.0f);

  if (U.pressure_softness != 0.0f) {
    pressure = powf(pressure, powf(4.0f, -U.pressure_softness));
  }

  return pressure;
}

/* adds customdata to event */
static void update_tablet_data(wmWindow *win, wmEvent *event)
{
  const GHOST_TabletData *td = GHOST_GetTabletData(win->ghostwin);

  /* if there's tablet data from an active tablet device then add it */
  if ((td != NULL) && td->Active != GHOST_kTabletModeNone) {
    struct wmTabletData *wmtab = MEM_mallocN(sizeof(wmTabletData), "customdata tablet");

    wmtab->Active = (int)td->Active;
    wmtab->Pressure = wm_pressure_curve(td->Pressure);
    wmtab->Xtilt = td->Xtilt;
    wmtab->Ytilt = td->Ytilt;

    event->tablet_data = wmtab;
    // printf("%s: using tablet %.5f\n", __func__, wmtab->Pressure);
  }
  else {
    event->tablet_data = NULL;
    // printf("%s: not using tablet\n", __func__);
  }
}

#ifdef WITH_INPUT_NDOF
/* adds customdata to event */
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

/* imperfect but probably usable... draw/enable drags to other windows */
static wmWindow *wm_event_cursor_other_windows(wmWindowManager *wm, wmWindow *win, wmEvent *event)
{
  int mx = event->x, my = event->y;

  if (wm->windows.first == wm->windows.last) {
    return NULL;
  }

  /* in order to use window size and mouse position (pixels), we have to use a WM function */

  /* check if outside, include top window bar... */
  if (mx < 0 || my < 0 || mx > WM_window_pixels_x(win) || my > WM_window_pixels_y(win) + 30) {
    wmWindow *owin;
    wmEventHandler *handler;

    /* Let's skip windows having modal handlers now */
    /* potential XXX ugly... I wouldn't have added a modalhandlers list
     * (introduced in rev 23331, ton). */
    for (handler = win->modalhandlers.first; handler; handler = handler->next) {
      if (ELEM(handler->type, WM_HANDLER_TYPE_UI, WM_HANDLER_TYPE_OP)) {
        return NULL;
      }
    }

    /* to desktop space */
    mx += (int)(U.pixelsize * win->posx);
    my += (int)(U.pixelsize * win->posy);

    /* check other windows to see if it has mouse inside */
    for (owin = wm->windows.first; owin; owin = owin->next) {

      if (owin != win) {
        int posx = (int)(U.pixelsize * owin->posx);
        int posy = (int)(U.pixelsize * owin->posy);

        if (mx - posx >= 0 && owin->posy >= 0 && mx - posx <= WM_window_pixels_x(owin) &&
            my - posy <= WM_window_pixels_y(owin)) {
          event->x = mx - (int)(U.pixelsize * owin->posx);
          event->y = my - (int)(U.pixelsize * owin->posy);

          return owin;
        }
      }
    }
  }
  return NULL;
}

static bool wm_event_is_double_click(wmEvent *event, const wmEvent *event_state)
{
  if ((event->type == event_state->prevtype) && (event_state->prevval == KM_RELEASE) &&
      (event->val == KM_PRESS)) {
    if ((ISMOUSE(event->type) == false) ||
        ((abs(event->x - event_state->prevclickx)) < WM_EVENT_CLICK_TWEAK_THRESHOLD &&
         (abs(event->y - event_state->prevclicky)) < WM_EVENT_CLICK_TWEAK_THRESHOLD)) {
      if ((PIL_check_seconds_timer() - event_state->prevclicktime) * 1000 < U.dbl_click_time) {
        return true;
      }
    }
  }

  return false;
}

static wmEvent *wm_event_add_mousemove(wmWindow *win, const wmEvent *event)
{
  wmEvent *event_last = win->queue.last;

  /* some painting operators want accurate mouse events, they can
   * handle in between mouse move moves, others can happily ignore
   * them for better performance */
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

/* windows store own event queues, no bContext here */
/* time is in 1000s of seconds, from ghost */
void wm_event_add_ghostevent(
    wmWindowManager *wm, wmWindow *win, int type, int UNUSED(time), void *customdata)
{
  wmWindow *owin;

  if (UNLIKELY(G.f & G_FLAG_EVENT_SIMULATE)) {
    return;
  }

  /**
   * Having both, \a event and \a evt, can be highly confusing to work with,
   * but is necessary for our current event system, so let's clear things up a bit:
   *
   * - Data added to event only will be handled immediately,
   *   but will not be copied to the next event.
   * - Data added to \a evt only stays,
   *   but is handled with the next event -> execution delay.
   * - Data added to event and \a evt stays and is handled immediately.
   */
  wmEvent event, *evt = win->eventstate;

  /* initialize and copy state (only mouse x y and modifiers) */
  event = *evt;

  switch (type) {
    /* mouse move, also to inactive window (X11 does this) */
    case GHOST_kEventCursorMove: {
      GHOST_TEventCursorData *cd = customdata;

      copy_v2_v2_int(&event.x, &cd->x);
      wm_stereo3d_mouse_offset_apply(win, &event.x);

      event.type = MOUSEMOVE;
      {
        wmEvent *event_new = wm_event_add_mousemove(win, &event);
        copy_v2_v2_int(&evt->x, &event_new->x);
        evt->is_motion_absolute = event_new->is_motion_absolute;
      }

      /* also add to other window if event is there, this makes overdraws disappear nicely */
      /* it remaps mousecoord to other window in event */
      owin = wm_event_cursor_other_windows(wm, win, &event);
      if (owin) {
        wmEvent oevent, *oevt = owin->eventstate;

        oevent = *oevt;

        copy_v2_v2_int(&oevent.x, &event.x);
        oevent.type = MOUSEMOVE;
        {
          wmEvent *event_new = wm_event_add_mousemove(owin, &oevent);
          copy_v2_v2_int(&oevt->x, &event_new->x);
          oevt->is_motion_absolute = event_new->is_motion_absolute;
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
        case GHOST_kTrackpadEventRotate:
          event.type = MOUSEROTATE;
          break;
        case GHOST_kTrackpadEventScroll:
        default:
          event.type = MOUSEPAN;
          break;
      }

      event.x = evt->x = pd->x;
      event.y = evt->y = pd->y;
      event.val = KM_NOTHING;

      /* Use prevx/prevy so we can calculate the delta later */
      event.prevx = event.x - pd->deltaX;
      event.prevy = event.y - (-pd->deltaY);

      wm_event_add(win, &event);
      break;
    }
    /* mouse button */
    case GHOST_kEventButtonDown:
    case GHOST_kEventButtonUp: {
      GHOST_TEventButtonData *bd = customdata;

      /* get value and type from ghost */
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

      wm_eventemulation(&event, false);

      /* copy previous state to prev event state (two old!) */
      evt->prevval = evt->val;
      evt->prevtype = evt->type;

      /* copy to event state */
      evt->val = event.val;
      evt->type = event.type;

      if (win->active == 0) {
        int cx, cy;

        /* Entering window, update mouse pos.
         * (ghost sends win-activate *after* the mouseclick in window!) */
        wm_get_cursor_position(win, &cx, &cy);

        event.x = evt->x = cx;
        event.y = evt->y = cy;
      }

      /* double click test */
      if (wm_event_is_double_click(&event, evt)) {
        CLOG_INFO(WM_LOG_HANDLERS, 1, "Send double click");
        event.val = KM_DBL_CLICK;
      }
      if (event.val == KM_PRESS) {
        evt->prevclicktime = PIL_check_seconds_timer();
        evt->prevclickx = event.x;
        evt->prevclicky = event.y;
      }

      /* add to other window if event is there (not to both!) */
      owin = wm_event_cursor_other_windows(wm, win, &event);
      if (owin) {
        wmEvent oevent = *(owin->eventstate);

        oevent.x = event.x;
        oevent.y = event.y;
        oevent.type = event.type;
        oevent.val = event.val;

        wm_event_add(owin, &oevent);
      }
      else {
        wm_event_add(win, &event);
      }

      break;
    }
    /* keyboard */
    case GHOST_kEventKeyDown:
    case GHOST_kEventKeyUp: {
      GHOST_TEventKeyData *kd = customdata;
      short keymodifier = KM_NOTHING;
      event.type = convert_key(kd->key);
      event.ascii = kd->ascii;
      memcpy(
          event.utf8_buf, kd->utf8_buf, sizeof(event.utf8_buf)); /* might be not null terminated*/
      event.val = (type == GHOST_kEventKeyDown) ? KM_PRESS : KM_RELEASE;

      wm_eventemulation(&event, false);

      /* copy previous state to prev event state (two old!) */
      evt->prevval = evt->val;
      evt->prevtype = evt->type;

      /* copy to event state */
      evt->val = event.val;
      evt->type = event.type;

      /* exclude arrow keys, esc, etc from text input */
      if (type == GHOST_kEventKeyUp) {
        event.ascii = '\0';

        /* ghost should do this already for key up */
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

      /* assigning both first and second is strange - campbell */
      switch (event.type) {
        case LEFTSHIFTKEY:
        case RIGHTSHIFTKEY:
          if (event.val == KM_PRESS) {
            if (evt->ctrl || evt->alt || evt->oskey) {
              keymodifier = (KM_MOD_FIRST | KM_MOD_SECOND);
            }
            else {
              keymodifier = KM_MOD_FIRST;
            }
          }
          event.shift = evt->shift = keymodifier;
          break;
        case LEFTCTRLKEY:
        case RIGHTCTRLKEY:
          if (event.val == KM_PRESS) {
            if (evt->shift || evt->alt || evt->oskey) {
              keymodifier = (KM_MOD_FIRST | KM_MOD_SECOND);
            }
            else {
              keymodifier = KM_MOD_FIRST;
            }
          }
          event.ctrl = evt->ctrl = keymodifier;
          break;
        case LEFTALTKEY:
        case RIGHTALTKEY:
          if (event.val == KM_PRESS) {
            if (evt->ctrl || evt->shift || evt->oskey) {
              keymodifier = (KM_MOD_FIRST | KM_MOD_SECOND);
            }
            else {
              keymodifier = KM_MOD_FIRST;
            }
          }
          event.alt = evt->alt = keymodifier;
          break;
        case OSKEY:
          if (event.val == KM_PRESS) {
            if (evt->ctrl || evt->alt || evt->shift) {
              keymodifier = (KM_MOD_FIRST | KM_MOD_SECOND);
            }
            else {
              keymodifier = KM_MOD_FIRST;
            }
          }
          event.oskey = evt->oskey = keymodifier;
          break;
        default:
          if (event.val == KM_PRESS && event.keymodifier == 0) {
            /* Only set in eventstate, for next event. */
            evt->keymodifier = event.type;
          }
          else if (event.val == KM_RELEASE && event.keymodifier == event.type) {
            event.keymodifier = evt->keymodifier = 0;
          }
          break;
      }

      /* double click test */
      /* if previous event was same type, and previous was release, and now it presses... */
      if (wm_event_is_double_click(&event, evt)) {
        CLOG_INFO(WM_LOG_HANDLERS, 1, "Send double click");
        event.val = KM_DBL_CLICK;
      }

      /* this case happens on holding a key pressed, it should not generate
       * press events events with the same key as modifier */
      if (event.keymodifier == event.type) {
        event.keymodifier = 0;
      }

      /* this case happens with an external numpad, and also when using 'dead keys'
       * (to compose complex latin characters e.g.), it's not really clear why.
       * Since it's impossible to map a key modifier to an unknown key,
       * it shouldn't harm to clear it. */
      if (event.keymodifier == UNKNOWNKEY) {
        evt->keymodifier = event.keymodifier = 0;
      }

      /* if test_break set, it catches this. Do not set with modifier presses.
       * XXX Keep global for now? */
      if ((event.type == ESCKEY && event.val == KM_PRESS) &&
          /* check other modifiers because ms-windows uses these to bring up the task manager */
          (event.shift == 0 && event.ctrl == 0 && event.alt == 0)) {
        G.is_break = true;
      }

      /* double click test - only for press */
      if (event.val == KM_PRESS) {
        /* Don't reset timer & location when holding the key generates repeat events. */
        if ((evt->prevtype != event.type) || (evt->prevval != KM_PRESS)) {
          evt->prevclicktime = PIL_check_seconds_timer();
          evt->prevclickx = event.x;
          evt->prevclicky = event.y;
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

#ifdef WITH_INPUT_NDOF
/* -------------------------------------------------------------------- */
/* NDOF */

/** \name NDOF Utility Functions
 * \{ */

void WM_event_ndof_pan_get(const wmNDOFMotionData *ndof, float r_pan[3], const bool use_zoom)
{
  int z_flag = use_zoom ? NDOF_ZOOM_INVERT : NDOF_PANZ_INVERT_AXIS;
  r_pan[0] = ndof->tvec[0] * ((U.ndof_flag & NDOF_PANX_INVERT_AXIS) ? -1.0f : 1.0f);
  r_pan[1] = ndof->tvec[1] * ((U.ndof_flag & NDOF_PANY_INVERT_AXIS) ? -1.0f : 1.0f);
  r_pan[2] = ndof->tvec[2] * ((U.ndof_flag & z_flag) ? -1.0f : 1.0f);
}

void WM_event_ndof_rotate_get(const wmNDOFMotionData *ndof, float r_rot[3])
{
  r_rot[0] = ndof->rvec[0] * ((U.ndof_flag & NDOF_ROTX_INVERT_AXIS) ? -1.0f : 1.0f);
  r_rot[1] = ndof->rvec[1] * ((U.ndof_flag & NDOF_ROTY_INVERT_AXIS) ? -1.0f : 1.0f);
  r_rot[2] = ndof->rvec[2] * ((U.ndof_flag & NDOF_ROTZ_INVERT_AXIS) ? -1.0f : 1.0f);
}

float WM_event_ndof_to_axis_angle(const struct wmNDOFMotionData *ndof, float axis[3])
{
  float angle;
  angle = normalize_v3_v3(axis, ndof->rvec);

  axis[0] = axis[0] * ((U.ndof_flag & NDOF_ROTX_INVERT_AXIS) ? -1.0f : 1.0f);
  axis[1] = axis[1] * ((U.ndof_flag & NDOF_ROTY_INVERT_AXIS) ? -1.0f : 1.0f);
  axis[2] = axis[2] * ((U.ndof_flag & NDOF_ROTZ_INVERT_AXIS) ? -1.0f : 1.0f);

  return ndof->dt * angle;
}

void WM_event_ndof_to_quat(const struct wmNDOFMotionData *ndof, float q[4])
{
  float axis[3];
  float angle;

  angle = WM_event_ndof_to_axis_angle(ndof, axis);
  axis_angle_to_quat(q, axis, angle);
}
#endif /* WITH_INPUT_NDOF */

/* if this is a tablet event, return tablet pressure and set *pen_flip
 * to 1 if the eraser tool is being used, 0 otherwise */
float WM_event_tablet_data(const wmEvent *event, int *pen_flip, float tilt[2])
{
  int erasor = 0;
  float pressure = 1;

  if (tilt) {
    zero_v2(tilt);
  }

  if (event->tablet_data) {
    const wmTabletData *wmtab = event->tablet_data;

    erasor = (wmtab->Active == EVT_TABLET_ERASER);
    if (wmtab->Active != EVT_TABLET_NONE) {
      pressure = wmtab->Pressure;
      if (tilt) {
        tilt[0] = wmtab->Xtilt;
        tilt[1] = wmtab->Ytilt;
      }
    }
  }

  if (pen_flip) {
    (*pen_flip) = erasor;
  }

  return pressure;
}

bool WM_event_is_tablet(const struct wmEvent *event)
{
  return (event->tablet_data) ? true : false;
}

#ifdef WITH_INPUT_IME
/* most os using ctrl/oskey + space to switch ime, avoid added space */
bool WM_event_is_ime_switch(const struct wmEvent *event)
{
  return event->val == KM_PRESS && event->type == SPACEKEY &&
         (event->ctrl || event->oskey || event->shift || event->alt);
}
#endif

/** \} */

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

static wmKeyMapItem *wm_kmi_from_event(bContext *C,
                                       wmWindowManager *wm,
                                       ListBase *handlers,
                                       const wmEvent *event)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    /* during this loop, ui handlers for nested menus can tag multiple handlers free */
    if (handler_base->flag & WM_HANDLER_DO_FREE) {
      /* pass */
    }
    else if (handler_base->poll == NULL || handler_base->poll(CTX_wm_region(C), event)) {
      if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
        wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
        wmKeyMap *keymap = WM_event_get_keymap_from_handler(wm, handler);
        if (keymap && WM_keymap_poll(C, keymap)) {
          for (wmKeyMapItem *kmi = keymap->items.first; kmi; kmi = kmi->next) {
            if (wm_eventmatch(event, kmi)) {
              wmOperatorType *ot = WM_operatortype_find(kmi->idname, 0);
              if (WM_operator_poll_context(C, ot, WM_OP_INVOKE_DEFAULT)) {
                return kmi;
              }
            }
          }
        }
      }
    }
  }
  return NULL;
}

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
 * use here since the ara is stored in the window manager.
 */
ScrArea *WM_window_status_area_find(wmWindow *win, bScreen *screen)
{
  if (screen->state == SCREENFULL) {
    return NULL;
  }
  ScrArea *sa_statusbar = NULL;
  for (ScrArea *sa = win->global_areas.areabase.first; sa; sa = sa->next) {
    if (sa->spacetype == SPACE_STATUSBAR) {
      sa_statusbar = sa;
      break;
    }
  }
  return sa_statusbar;
}

void WM_window_status_area_tag_redraw(wmWindow *win)
{
  bScreen *sc = WM_window_get_active_screen(win);
  ScrArea *sa = WM_window_status_area_find(win, sc);
  if (sa != NULL) {
    ED_area_tag_redraw(sa);
  }
}

void WM_window_cursor_keymap_status_refresh(bContext *C, wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  ScrArea *sa_statusbar = WM_window_status_area_find(win, screen);
  if (sa_statusbar == NULL) {
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
  ARegion *ar = screen->active_region;
  if (ar == NULL) {
    return;
  }

  ScrArea *sa = NULL;
  ED_screen_areas_iter(win, screen, sa_iter)
  {
    if (BLI_findindex(&sa_iter->regionbase, ar) != -1) {
      sa = sa_iter;
      break;
    }
  }
  if (sa == NULL) {
    return;
  }

  /* Keep as-is. */
  if (ELEM(sa->spacetype, SPACE_STATUSBAR, SPACE_TOPBAR)) {
    return;
  }
  if (ELEM(ar->regiontype,
           RGN_TYPE_HEADER,
           RGN_TYPE_TOOL_HEADER,
           RGN_TYPE_FOOTER,
           RGN_TYPE_TEMPORARY,
           RGN_TYPE_HUD)) {
    return;
  }
  /* Fallback to window. */
  if (ELEM(ar->regiontype, RGN_TYPE_TOOLS, RGN_TYPE_TOOL_PROPS)) {
    ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
  }

  /* Detect changes to the state. */
  {
    bToolRef *tref = NULL;
    if ((ar->regiontype == RGN_TYPE_WINDOW) && ((1 << sa->spacetype) & WM_TOOLSYSTEM_SPACE_MASK)) {
      ViewLayer *view_layer = WM_window_get_active_view_layer(win);
      WorkSpace *workspace = WM_window_get_active_workspace(win);
      const bToolKey tkey = {
          .space_type = sa->spacetype,
          .mode = WM_toolsystem_mode_from_spacetype(view_layer, sa, sa->spacetype),
      };
      tref = WM_toolsystem_ref_find(workspace, &tkey);
    }
    wm_event_cursor_store(&cd->state, win->eventstate, sa->spacetype, ar->regiontype, tref);
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
  CTX_wm_area_set(C, sa);
  CTX_wm_region_set(C, ar);

  ListBase *handlers[] = {
      &ar->handlers,
      &sa->handlers,
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
      kmi = wm_kmi_from_event(C, wm, handlers[handler_index], &test_event);
      if (kmi) {
        break;
      }
    }
    if (kmi) {
      wmOperatorType *ot = WM_operatortype_find(kmi->idname, 0);
      STRNCPY(cd->text[button_index][type_index], ot ? ot->name : kmi->idname);
    }
  }

  if (memcmp(&cd_prev.text, &cd->text, sizeof(cd_prev.text)) != 0) {
    ED_area_tag_redraw(sa_statusbar);
  }

  CTX_wm_window_set(C, NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modal Keymap Status
 *
 * \{ */

bool WM_window_modal_keymap_status_draw(bContext *UNUSED(C), wmWindow *win, uiLayout *layout)
{
  wmKeyMap *keymap = NULL;
  wmOperator *op = NULL;
  LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
      if (handler->op != NULL) {
        /* 'handler->keymap' could be checked too, seems not to be used. */
        wmKeyMap *keymap_test = handler->op->type->modalkeymap;
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
      /* warning: O(n^2) */
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
        int icon_mod[4];
#ifdef WITH_HEADLESS
        int icon = 0;
#else
        int icon = UI_icon_from_keymap_item(kmi, icon_mod);
#endif
        if (icon != 0) {
          for (int j = 0; j < ARRAY_SIZE(icon_mod) && icon_mod[j]; j++) {
            uiItemL(row, "", icon_mod[j]);
          }
          uiItemL(row, items[i].name, icon);
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
