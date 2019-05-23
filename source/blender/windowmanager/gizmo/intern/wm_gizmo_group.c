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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 *
 * \name Gizmo-Group
 *
 * Gizmo-groups store and manage groups of gizmos. They can be
 * attached to modal handlers and have own keymaps.
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_buffer.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_workspace.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"

#include "ED_screen.h"
#include "ED_undo.h"

/* own includes */
#include "wm_gizmo_wmapi.h"
#include "wm_gizmo_intern.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

/* Allow gizmo part's to be single click only,
 * dragging falls back to activating their 'drag_part' action. */
#define USE_DRAG_DETECT

/* -------------------------------------------------------------------- */
/** \name wmGizmoGroup
 *
 * \{ */

/**
 * Create a new gizmo-group from \a gzgt.
 */
wmGizmoGroup *wm_gizmogroup_new_from_type(wmGizmoMap *gzmap, wmGizmoGroupType *gzgt)
{
  wmGizmoGroup *gzgroup = MEM_callocN(sizeof(*gzgroup), "gizmo-group");
  gzgroup->type = gzgt;

  /* keep back-link */
  gzgroup->parent_gzmap = gzmap;

  BLI_addtail(&gzmap->groups, gzgroup);

  return gzgroup;
}

void wm_gizmogroup_free(bContext *C, wmGizmoGroup *gzgroup)
{
  wmGizmoMap *gzmap = gzgroup->parent_gzmap;

  /* Similar to WM_gizmo_unlink, but only to keep gzmap state correct,
   * we don't want to run callbacks. */
  if (gzmap->gzmap_context.highlight &&
      gzmap->gzmap_context.highlight->parent_gzgroup == gzgroup) {
    wm_gizmomap_highlight_set(gzmap, C, NULL, 0);
  }
  if (gzmap->gzmap_context.modal && gzmap->gzmap_context.modal->parent_gzgroup == gzgroup) {
    wm_gizmomap_modal_set(gzmap, C, gzmap->gzmap_context.modal, NULL, false);
  }

  for (wmGizmo *gz = gzgroup->gizmos.first, *gz_next; gz; gz = gz_next) {
    gz_next = gz->next;
    if (gzmap->gzmap_context.select.len) {
      WM_gizmo_select_unlink(gzmap, gz);
    }
    WM_gizmo_free(gz);
  }
  BLI_listbase_clear(&gzgroup->gizmos);

#ifdef WITH_PYTHON
  if (gzgroup->py_instance) {
    /* do this first in case there are any __del__ functions or
     * similar that use properties */
    BPY_DECREF_RNA_INVALIDATE(gzgroup->py_instance);
  }
#endif

  if (gzgroup->reports && (gzgroup->reports->flag & RPT_FREE)) {
    BKE_reports_clear(gzgroup->reports);
    MEM_freeN(gzgroup->reports);
  }

  if (gzgroup->customdata_free) {
    gzgroup->customdata_free(gzgroup->customdata);
  }
  else {
    MEM_SAFE_FREE(gzgroup->customdata);
  }

  BLI_remlink(&gzmap->groups, gzgroup);

  MEM_freeN(gzgroup);
}

/**
 * Add \a gizmo to \a gzgroup and make sure its name is unique within the group.
 */
void wm_gizmogroup_gizmo_register(wmGizmoGroup *gzgroup, wmGizmo *gz)
{
  BLI_assert(BLI_findindex(&gzgroup->gizmos, gz) == -1);
  BLI_addtail(&gzgroup->gizmos, gz);
  gz->parent_gzgroup = gzgroup;
}

int WM_gizmo_cmp_temp_fl(const void *gz_a_ptr, const void *gz_b_ptr)
{
  const wmGizmo *gz_a = gz_a_ptr;
  const wmGizmo *gz_b = gz_b_ptr;
  if (gz_a->temp.f < gz_b->temp.f) {
    return -1;
  }
  else if (gz_a->temp.f > gz_b->temp.f) {
    return 1;
  }
  else {
    return 0;
  }
}

int WM_gizmo_cmp_temp_fl_reverse(const void *gz_a_ptr, const void *gz_b_ptr)
{
  const wmGizmo *gz_a = gz_a_ptr;
  const wmGizmo *gz_b = gz_b_ptr;
  if (gz_a->temp.f < gz_b->temp.f) {
    return 1;
  }
  else if (gz_a->temp.f > gz_b->temp.f) {
    return -1;
  }
  else {
    return 0;
  }
}

wmGizmo *wm_gizmogroup_find_intersected_gizmo(const wmGizmoGroup *gzgroup,
                                              bContext *C,
                                              const wmEvent *event,
                                              int *r_part)
{
  for (wmGizmo *gz = gzgroup->gizmos.first; gz; gz = gz->next) {
    if (gz->type->test_select && (gz->flag & (WM_GIZMO_HIDDEN | WM_GIZMO_HIDDEN_SELECT)) == 0) {
      if ((*r_part = gz->type->test_select(C, gz, event->mval)) != -1) {
        return gz;
      }
    }
  }

  return NULL;
}

/**
 * Adds all gizmos of \a gzgroup that can be selected to the head of \a listbase.
 * Added items need freeing!
 */
void wm_gizmogroup_intersectable_gizmos_to_list(const wmGizmoGroup *gzgroup,
                                                BLI_Buffer *visible_gizmos)
{
  for (wmGizmo *gz = gzgroup->gizmos.last; gz; gz = gz->prev) {
    if ((gz->flag & (WM_GIZMO_HIDDEN | WM_GIZMO_HIDDEN_SELECT)) == 0) {
      if (((gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) &&
           (gz->type->draw_select || gz->type->test_select)) ||
          ((gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) == 0 && gz->type->test_select)) {
        BLI_buffer_append(visible_gizmos, wmGizmo *, gz);
      }
    }
  }
}

void WM_gizmogroup_ensure_init(const bContext *C, wmGizmoGroup *gzgroup)
{
  /* prepare for first draw */
  if (UNLIKELY((gzgroup->init_flag & WM_GIZMOGROUP_INIT_SETUP) == 0)) {
    gzgroup->type->setup(C, gzgroup);

    /* Not ideal, initialize keymap here, needed for RNA runtime generated gizmos. */
    wmGizmoGroupType *gzgt = gzgroup->type;
    if (gzgt->keymap == NULL) {
      wmWindowManager *wm = CTX_wm_manager(C);
      wm_gizmogrouptype_setup_keymap(gzgt, wm->defaultconf);
      BLI_assert(gzgt->keymap != NULL);
    }
    gzgroup->init_flag |= WM_GIZMOGROUP_INIT_SETUP;
  }

  /* Refresh may be called multiple times,
   * this just ensures its called at least once before we draw. */
  if (UNLIKELY((gzgroup->init_flag & WM_GIZMOGROUP_INIT_REFRESH) == 0)) {
    if (gzgroup->type->refresh) {
      gzgroup->type->refresh(C, gzgroup);
    }
    gzgroup->init_flag |= WM_GIZMOGROUP_INIT_REFRESH;
  }
}

bool WM_gizmo_group_type_poll(const bContext *C, const struct wmGizmoGroupType *gzgt)
{
  /* If we're tagged, only use compatible. */
  if (gzgt->owner_id[0] != '\0') {
    const WorkSpace *workspace = CTX_wm_workspace(C);
    if (BKE_workspace_owner_id_check(workspace, gzgt->owner_id) == false) {
      return false;
    }
  }
  /* Check for poll function, if gizmo-group belongs to an operator,
   * also check if the operator is running. */
  return (!gzgt->poll || gzgt->poll(C, (wmGizmoGroupType *)gzgt));
}

bool wm_gizmogroup_is_visible_in_drawstep(const wmGizmoGroup *gzgroup,
                                          const eWM_GizmoFlagMapDrawStep drawstep)
{
  switch (drawstep) {
    case WM_GIZMOMAP_DRAWSTEP_2D:
      return (gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) == 0;
    case WM_GIZMOMAP_DRAWSTEP_3D:
      return (gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D);
    default:
      BLI_assert(0);
      return false;
  }
}

bool wm_gizmogroup_is_any_selected(const wmGizmoGroup *gzgroup)
{
  if (gzgroup->type->flag & WM_GIZMOGROUPTYPE_SELECT) {
    for (const wmGizmo *gz = gzgroup->gizmos.first; gz; gz = gz->next) {
      if (gz->state & WM_GIZMO_STATE_SELECT) {
        return true;
      }
    }
  }
  return false;
}

/** \} */

/** \name Gizmo operators
 *
 * Basic operators for gizmo interaction with user configurable keymaps.
 *
 * \{ */

static int gizmo_select_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  ARegion *ar = CTX_wm_region(C);
  wmGizmoMap *gzmap = ar->gizmo_map;
  wmGizmoMapSelectState *msel = &gzmap->gzmap_context.select;
  wmGizmo *highlight = gzmap->gzmap_context.highlight;

  bool extend = RNA_boolean_get(op->ptr, "extend");
  bool deselect = RNA_boolean_get(op->ptr, "deselect");
  bool toggle = RNA_boolean_get(op->ptr, "toggle");

  /* deselect all first */
  if (extend == false && deselect == false && toggle == false) {
    wm_gizmomap_deselect_all(gzmap);
    BLI_assert(msel->items == NULL && msel->len == 0);
    UNUSED_VARS_NDEBUG(msel);
  }

  if (highlight) {
    const bool is_selected = (highlight->state & WM_GIZMO_STATE_SELECT);
    bool redraw = false;

    if (toggle) {
      /* toggle: deselect if already selected, else select */
      deselect = is_selected;
    }

    if (deselect) {
      if (is_selected && WM_gizmo_select_set(gzmap, highlight, false)) {
        redraw = true;
      }
    }
    else if (wm_gizmo_select_and_highlight(C, gzmap, highlight)) {
      redraw = true;
    }

    if (redraw) {
      ED_region_tag_redraw(ar);
    }

    return OPERATOR_FINISHED;
  }
  else {
    BLI_assert(0);
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }
}

void GIZMOGROUP_OT_gizmo_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Gizmo Select";
  ot->description = "Select the currently highlighted gizmo";
  ot->idname = "GIZMOGROUP_OT_gizmo_select";

  /* api callbacks */
  ot->invoke = gizmo_select_invoke;

  ot->flag = OPTYPE_UNDO;

  WM_operator_properties_mouse_select(ot);
}

typedef struct GizmoTweakData {
  wmGizmoMap *gzmap;
  wmGizmoGroup *gzgroup;
  wmGizmo *gz_modal;

  int init_event; /* initial event type */
  int flag;       /* tweak flags */

#ifdef USE_DRAG_DETECT
  /* True until the mouse is moved (only use when the operator has no modal).
   * this allows some gizmos to be click-only. */
  enum {
    /* Don't detect dragging. */
    DRAG_NOP = 0,
    /* Detect dragging (wait until a drag or click is detected). */
    DRAG_DETECT,
    /* Drag has started, idle until there is no active modal operator.
     * This is needed because finishing the modal operator also exits
     * the modal gizmo state (un-grabbs the cursor).
     * Ideally this workaround could be removed later. */
    DRAG_IDLE,
  } drag_state;
#endif

} GizmoTweakData;

static bool gizmo_tweak_start(bContext *C, wmGizmoMap *gzmap, wmGizmo *gz, const wmEvent *event)
{
  /* activate highlighted gizmo */
  wm_gizmomap_modal_set(gzmap, C, gz, event, true);

  return (gz->state & WM_GIZMO_STATE_MODAL);
}

static bool gizmo_tweak_start_and_finish(
    bContext *C, wmGizmoMap *gzmap, wmGizmo *gz, const wmEvent *event, bool *r_is_modal)
{
  wmGizmoOpElem *gzop = WM_gizmo_operator_get(gz, gz->highlight_part);
  if (r_is_modal) {
    *r_is_modal = false;
  }
  if (gzop && gzop->type) {

    /* Undo/Redo */
    if (gzop->is_redo) {
      wmWindowManager *wm = CTX_wm_manager(C);
      wmOperator *op = WM_operator_last_redo(C);

      /* We may want to enable this, for now the gizmo can manage it's own properties. */
#if 0
      IDP_MergeGroup(gzop->ptr.data, op->properties, false);
#endif

      WM_operator_free_all_after(wm, op);
      ED_undo_pop_op(C, op);
    }

    /* XXX temporary workaround for modal gizmo operator
     * conflicting with modal operator attached to gizmo */
    if (gzop->type->modal) {
      /* activate highlighted gizmo */
      wm_gizmomap_modal_set(gzmap, C, gz, event, true);
      if (r_is_modal) {
        *r_is_modal = true;
      }
    }
    else {
      if (gz->parent_gzgroup->type->invoke_prepare) {
        gz->parent_gzgroup->type->invoke_prepare(C, gz->parent_gzgroup, gz);
      }
      /* Allow for 'button' gizmos, single click to run an action. */
      WM_gizmo_operator_invoke(C, gz, gzop);
    }
    return true;
  }
  else {
    return false;
  }
}

static void gizmo_tweak_finish(bContext *C, wmOperator *op, const bool cancel, bool clear_modal)
{
  GizmoTweakData *mtweak = op->customdata;
  if (mtweak->gz_modal->type->exit) {
    mtweak->gz_modal->type->exit(C, mtweak->gz_modal, cancel);
  }
  if (clear_modal) {
    /* The gizmo may have been removed. */
    if ((BLI_findindex(&mtweak->gzmap->groups, mtweak->gzgroup) != -1) &&
        (BLI_findindex(&mtweak->gzgroup->gizmos, mtweak->gz_modal) != -1)) {
      wm_gizmomap_modal_set(mtweak->gzmap, C, mtweak->gz_modal, NULL, false);
    }
  }
  MEM_freeN(mtweak);
}

static int gizmo_tweak_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  GizmoTweakData *mtweak = op->customdata;
  wmGizmo *gz = mtweak->gz_modal;
  int retval = OPERATOR_PASS_THROUGH;
  bool clear_modal = true;

  if (gz == NULL) {
    BLI_assert(0);
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

#ifdef USE_DRAG_DETECT
  wmGizmoMap *gzmap = mtweak->gzmap;
  if (mtweak->drag_state == DRAG_DETECT) {
    if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
      if (len_manhattan_v2v2_int(&event->x, gzmap->gzmap_context.event_xy) >=
          WM_EVENT_CURSOR_CLICK_DRAG_THRESHOLD) {
        mtweak->drag_state = DRAG_IDLE;
        gz->highlight_part = gz->drag_part;
      }
    }
    else if (event->type == mtweak->init_event && event->val == KM_RELEASE) {
      mtweak->drag_state = DRAG_NOP;
      retval = OPERATOR_FINISHED;
    }

    if (mtweak->drag_state != DRAG_DETECT) {
      /* Follow logic in 'gizmo_tweak_invoke' */
      bool is_modal = false;
      if (gizmo_tweak_start_and_finish(C, gzmap, gz, event, &is_modal)) {
        if (is_modal) {
          clear_modal = false;
        }
      }
      else {
        if (!gizmo_tweak_start(C, gzmap, gz, event)) {
          retval = OPERATOR_FINISHED;
        }
      }
    }
  }
  if (mtweak->drag_state == DRAG_IDLE) {
    if (gzmap->gzmap_context.modal != NULL) {
      return OPERATOR_PASS_THROUGH;
    }
    else {
      gizmo_tweak_finish(C, op, false, false);
      return OPERATOR_FINISHED;
    }
  }
#endif /* USE_DRAG_DETECT */

  if (retval == OPERATOR_FINISHED) {
    /* pass */
  }
  else if (event->type == mtweak->init_event && event->val == KM_RELEASE) {
    retval = OPERATOR_FINISHED;
  }
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case TWEAK_MODAL_CANCEL:
        retval = OPERATOR_CANCELLED;
        break;
      case TWEAK_MODAL_CONFIRM:
        retval = OPERATOR_FINISHED;
        break;
      case TWEAK_MODAL_PRECISION_ON:
        mtweak->flag |= WM_GIZMO_TWEAK_PRECISE;
        break;
      case TWEAK_MODAL_PRECISION_OFF:
        mtweak->flag &= ~WM_GIZMO_TWEAK_PRECISE;
        break;

      case TWEAK_MODAL_SNAP_ON:
        mtweak->flag |= WM_GIZMO_TWEAK_SNAP;
        break;
      case TWEAK_MODAL_SNAP_OFF:
        mtweak->flag &= ~WM_GIZMO_TWEAK_SNAP;
        break;
    }
  }

  if (retval != OPERATOR_PASS_THROUGH) {
    gizmo_tweak_finish(C, op, retval != OPERATOR_FINISHED, clear_modal);
    return retval;
  }

  /* handle gizmo */
  wmGizmoFnModal modal_fn = gz->custom_modal ? gz->custom_modal : gz->type->modal;
  if (modal_fn) {
    int modal_retval = modal_fn(C, gz, event, mtweak->flag);

    if ((modal_retval & OPERATOR_RUNNING_MODAL) == 0) {
      gizmo_tweak_finish(C, op, (modal_retval & OPERATOR_CANCELLED) != 0, true);
      return OPERATOR_FINISHED;
    }

    /* Ugly hack to send gizmo events */
    ((wmEvent *)event)->type = EVT_GIZMO_UPDATE;
  }

  /* always return PASS_THROUGH so modal handlers
   * with gizmos attached can update */
  BLI_assert(retval == OPERATOR_PASS_THROUGH);
  return OPERATOR_PASS_THROUGH;
}

static int gizmo_tweak_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  wmGizmoMap *gzmap = ar->gizmo_map;
  wmGizmo *gz = gzmap->gzmap_context.highlight;

  /* Needed for single click actions which don't enter modal state. */
  WM_tooltip_clear(C, CTX_wm_window(C));

  if (!gz) {
    /* wm_handlers_do_intern shouldn't let this happen */
    BLI_assert(0);
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  bool use_drag_fallback = false;

#ifdef USE_DRAG_DETECT
  use_drag_fallback = !ELEM(gz->drag_part, -1, gz->highlight_part);
#endif

  if (use_drag_fallback == false) {
    if (gizmo_tweak_start_and_finish(C, gzmap, gz, event, NULL)) {
      return OPERATOR_FINISHED;
    }
  }

  bool use_drag_detect = false;
#ifdef USE_DRAG_DETECT
  if (use_drag_fallback) {
    wmGizmoOpElem *gzop = WM_gizmo_operator_get(gz, gz->highlight_part);
    if (gzop && gzop->type) {
      if (gzop->type->modal == NULL) {
        use_drag_detect = true;
      }
    }
  }
#endif

  if (use_drag_detect == false) {
    if (!gizmo_tweak_start(C, gzmap, gz, event)) {
      /* failed to start */
      return OPERATOR_PASS_THROUGH;
    }
  }

  GizmoTweakData *mtweak = MEM_mallocN(sizeof(GizmoTweakData), __func__);

  mtweak->init_event = WM_userdef_event_type_from_keymap_type(event->type);
  mtweak->gz_modal = gzmap->gzmap_context.highlight;
  mtweak->gzgroup = mtweak->gz_modal->parent_gzgroup;
  mtweak->gzmap = gzmap;
  mtweak->flag = 0;

#ifdef USE_DRAG_DETECT
  mtweak->drag_state = use_drag_detect ? DRAG_DETECT : DRAG_NOP;
#endif

  op->customdata = mtweak;

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void GIZMOGROUP_OT_gizmo_tweak(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Gizmo Tweak";
  ot->description = "Tweak the active gizmo";
  ot->idname = "GIZMOGROUP_OT_gizmo_tweak";

  /* api callbacks */
  ot->invoke = gizmo_tweak_invoke;
  ot->modal = gizmo_tweak_modal;

  /* TODO(campbell) This causes problems tweaking settings for operators,
   * need to find a way to support this. */
#if 0
  ot->flag = OPTYPE_UNDO;
#endif
}

/** \} */

static wmKeyMap *gizmogroup_tweak_modal_keymap(wmKeyConfig *keyconf, const char *gzgroupname)
{
  wmKeyMap *keymap;
  char name[KMAP_MAX_NAME];

  static EnumPropertyItem modal_items[] = {
      {TWEAK_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {TWEAK_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {TWEAK_MODAL_PRECISION_ON, "PRECISION_ON", 0, "Enable Precision", ""},
      {TWEAK_MODAL_PRECISION_OFF, "PRECISION_OFF", 0, "Disable Precision", ""},
      {TWEAK_MODAL_SNAP_ON, "SNAP_ON", 0, "Enable Snap", ""},
      {TWEAK_MODAL_SNAP_OFF, "SNAP_OFF", 0, "Disable Snap", ""},
      {0, NULL, 0, NULL, NULL},
  };

  BLI_snprintf(name, sizeof(name), "%s Tweak Modal Map", gzgroupname);
  keymap = WM_modalkeymap_get(keyconf, name);

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return NULL;
  }

  keymap = WM_modalkeymap_add(keyconf, name, modal_items);

  /* items for modal map */
  WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CANCEL);
  WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CANCEL);

  WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CONFIRM);
  WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CONFIRM);

  WM_modalkeymap_add_item(keymap, RIGHTSHIFTKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_PRECISION_ON);
  WM_modalkeymap_add_item(keymap, RIGHTSHIFTKEY, KM_RELEASE, KM_ANY, 0, TWEAK_MODAL_PRECISION_OFF);
  WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_PRECISION_ON);
  WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_RELEASE, KM_ANY, 0, TWEAK_MODAL_PRECISION_OFF);

  WM_modalkeymap_add_item(keymap, RIGHTCTRLKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_SNAP_ON);
  WM_modalkeymap_add_item(keymap, RIGHTCTRLKEY, KM_RELEASE, KM_ANY, 0, TWEAK_MODAL_SNAP_OFF);
  WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_SNAP_ON);
  WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_RELEASE, KM_ANY, 0, TWEAK_MODAL_SNAP_OFF);

  WM_modalkeymap_assign(keymap, "GIZMOGROUP_OT_gizmo_tweak");

  return keymap;
}

/**
 * Common default keymap for gizmo groups
 */
wmKeyMap *WM_gizmogroup_keymap_common(const wmGizmoGroupType *gzgt, wmKeyConfig *config)
{
  /* Use area and region id since we might have multiple gizmos
   * with the same name in different areas/regions. */
  wmKeyMap *km = WM_keymap_ensure(
      config, gzgt->name, gzgt->gzmap_params.spaceid, gzgt->gzmap_params.regionid);

  WM_keymap_add_item(km, "GIZMOGROUP_OT_gizmo_tweak", LEFTMOUSE, KM_PRESS, KM_ANY, 0);
  gizmogroup_tweak_modal_keymap(config, gzgt->name);

  return km;
}

/**
 * Variation of #WM_gizmogroup_keymap_common but with keymap items for selection
 */
wmKeyMap *WM_gizmogroup_keymap_common_select(const wmGizmoGroupType *gzgt, wmKeyConfig *config)
{
  /* Use area and region id since we might have multiple gizmos
   * with the same name in different areas/regions. */
  wmKeyMap *km = WM_keymap_ensure(
      config, gzgt->name, gzgt->gzmap_params.spaceid, gzgt->gzmap_params.regionid);
  /* FIXME(campbell) */
#if 0
  const int select_mouse = (U.flag & USER_LMOUSESELECT) ? LEFTMOUSE : RIGHTMOUSE;
  const int select_tweak = (U.flag & USER_LMOUSESELECT) ? EVT_TWEAK_L : EVT_TWEAK_R;
  const int action_mouse = (U.flag & USER_LMOUSESELECT) ? RIGHTMOUSE : LEFTMOUSE;
#else
  const int select_mouse = RIGHTMOUSE;
  const int select_tweak = EVT_TWEAK_R;
  const int action_mouse = LEFTMOUSE;
#endif

  WM_keymap_add_item(km, "GIZMOGROUP_OT_gizmo_tweak", action_mouse, KM_PRESS, KM_ANY, 0);
  WM_keymap_add_item(km, "GIZMOGROUP_OT_gizmo_tweak", select_tweak, KM_ANY, 0, 0);
  gizmogroup_tweak_modal_keymap(config, gzgt->name);

  wmKeyMapItem *kmi = WM_keymap_add_item(
      km, "GIZMOGROUP_OT_gizmo_select", select_mouse, KM_PRESS, 0, 0);
  RNA_boolean_set(kmi->ptr, "extend", false);
  RNA_boolean_set(kmi->ptr, "deselect", false);
  RNA_boolean_set(kmi->ptr, "toggle", false);
  kmi = WM_keymap_add_item(km, "GIZMOGROUP_OT_gizmo_select", select_mouse, KM_PRESS, KM_SHIFT, 0);
  RNA_boolean_set(kmi->ptr, "extend", false);
  RNA_boolean_set(kmi->ptr, "deselect", false);
  RNA_boolean_set(kmi->ptr, "toggle", true);

  return km;
}

/** \} */ /* wmGizmoGroup */

/* -------------------------------------------------------------------- */
/** \name wmGizmoGroupType
 *
 * \{ */

struct wmGizmoGroupTypeRef *WM_gizmomaptype_group_find_ptr(struct wmGizmoMapType *gzmap_type,
                                                           const wmGizmoGroupType *gzgt)
{
  /* could use hash lookups as operator types do, for now simple search. */
  for (wmGizmoGroupTypeRef *gzgt_ref = gzmap_type->grouptype_refs.first; gzgt_ref;
       gzgt_ref = gzgt_ref->next) {
    if (gzgt_ref->type == gzgt) {
      return gzgt_ref;
    }
  }
  return NULL;
}

struct wmGizmoGroupTypeRef *WM_gizmomaptype_group_find(struct wmGizmoMapType *gzmap_type,
                                                       const char *idname)
{
  /* could use hash lookups as operator types do, for now simple search. */
  for (wmGizmoGroupTypeRef *gzgt_ref = gzmap_type->grouptype_refs.first; gzgt_ref;
       gzgt_ref = gzgt_ref->next) {
    if (STREQ(idname, gzgt_ref->type->idname)) {
      return gzgt_ref;
    }
  }
  return NULL;
}

/**
 * Use this for registering gizmos on startup.
 * For runtime, use #WM_gizmomaptype_group_link_runtime.
 */
wmGizmoGroupTypeRef *WM_gizmomaptype_group_link(wmGizmoMapType *gzmap_type, const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  BLI_assert(gzgt != NULL);
  return WM_gizmomaptype_group_link_ptr(gzmap_type, gzgt);
}

wmGizmoGroupTypeRef *WM_gizmomaptype_group_link_ptr(wmGizmoMapType *gzmap_type,
                                                    wmGizmoGroupType *gzgt)
{
  wmGizmoGroupTypeRef *gzgt_ref = MEM_callocN(sizeof(wmGizmoGroupTypeRef), "gizmo-group-ref");
  gzgt_ref->type = gzgt;
  BLI_addtail(&gzmap_type->grouptype_refs, gzgt_ref);
  return gzgt_ref;
}

void WM_gizmomaptype_group_init_runtime_keymap(const Main *bmain, wmGizmoGroupType *gzgt)
{
  /* init keymap - on startup there's an extra call to init keymaps for 'permanent' gizmo-groups */
  wm_gizmogrouptype_setup_keymap(gzgt, ((wmWindowManager *)bmain->wm.first)->defaultconf);
}

void WM_gizmomaptype_group_init_runtime(const Main *bmain,
                                        wmGizmoMapType *gzmap_type,
                                        wmGizmoGroupType *gzgt)
{
  /* Tools add themselves. */
  if (gzgt->flag & WM_GIZMOGROUPTYPE_TOOL_INIT) {
    return;
  }

  /* now create a gizmo for all existing areas */
  for (bScreen *sc = bmain->screens.first; sc; sc = sc->id.next) {
    for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
      for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
        ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
        for (ARegion *ar = lb->first; ar; ar = ar->next) {
          wmGizmoMap *gzmap = ar->gizmo_map;
          if (gzmap && gzmap->type == gzmap_type) {
            WM_gizmomaptype_group_init_runtime_with_region(gzmap_type, gzgt, ar);
          }
        }
      }
    }
  }
}

wmGizmoGroup *WM_gizmomaptype_group_init_runtime_with_region(wmGizmoMapType *gzmap_type,
                                                             wmGizmoGroupType *gzgt,
                                                             ARegion *ar)
{
  wmGizmoMap *gzmap = ar->gizmo_map;
  BLI_assert(gzmap && gzmap->type == gzmap_type);
  UNUSED_VARS_NDEBUG(gzmap_type);

  wmGizmoGroup *gzgroup = wm_gizmogroup_new_from_type(gzmap, gzgt);

  wm_gizmomap_highlight_set(gzmap, NULL, NULL, 0);

  ED_region_tag_redraw(ar);

  return gzgroup;
}

/**
 * Unlike #WM_gizmomaptype_group_unlink this doesn't maintain correct state, simply free.
 */
void WM_gizmomaptype_group_free(wmGizmoGroupTypeRef *gzgt_ref)
{
  MEM_freeN(gzgt_ref);
}

void WM_gizmomaptype_group_unlink(bContext *C,
                                  Main *bmain,
                                  wmGizmoMapType *gzmap_type,
                                  const wmGizmoGroupType *gzgt)
{
  /* Free instances. */
  for (bScreen *sc = bmain->screens.first; sc; sc = sc->id.next) {
    for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
      for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
        ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
        for (ARegion *ar = lb->first; ar; ar = ar->next) {
          wmGizmoMap *gzmap = ar->gizmo_map;
          if (gzmap && gzmap->type == gzmap_type) {
            wmGizmoGroup *gzgroup, *gzgroup_next;
            for (gzgroup = gzmap->groups.first; gzgroup; gzgroup = gzgroup_next) {
              gzgroup_next = gzgroup->next;
              if (gzgroup->type == gzgt) {
                BLI_assert(gzgroup->parent_gzmap == gzmap);
                wm_gizmogroup_free(C, gzgroup);
                ED_region_tag_redraw(ar);
              }
            }
          }
        }
      }
    }
  }

  /* Free types. */
  wmGizmoGroupTypeRef *gzgt_ref = WM_gizmomaptype_group_find_ptr(gzmap_type, gzgt);
  if (gzgt_ref) {
    BLI_remlink(&gzmap_type->grouptype_refs, gzgt_ref);
    WM_gizmomaptype_group_free(gzgt_ref);
  }

  /* Note, we may want to keep this keymap for editing */
  WM_keymap_remove(gzgt->keyconf, gzgt->keymap);

  BLI_assert(WM_gizmomaptype_group_find_ptr(gzmap_type, gzgt) == NULL);
}

void wm_gizmogrouptype_setup_keymap(wmGizmoGroupType *gzgt, wmKeyConfig *keyconf)
{
  /* Use flag since setup_keymap may return NULL,
   * in that case we better not keep calling it. */
  if (gzgt->type_update_flag & WM_GIZMOMAPTYPE_KEYMAP_INIT) {
    gzgt->keymap = gzgt->setup_keymap(gzgt, keyconf);
    gzgt->keyconf = keyconf;
    gzgt->type_update_flag &= ~WM_GIZMOMAPTYPE_KEYMAP_INIT;
  }
}

/** \} */ /* wmGizmoGroupType */

/* -------------------------------------------------------------------- */
/** \name High Level Add/Remove API
 *
 * For use directly from operators & RNA registration.
 *
 * \note In context of gizmo API these names are a bit misleading,
 * but for general use terms its OK.
 * `WM_gizmo_group_type_add` would be more correctly called:
 * `WM_gizmomaptype_grouptype_reference_link`
 * but for general purpose API this is too detailed & annoying.
 *
 * \note We may want to return a value if there is nothing to remove.
 *
 * \{ */

void WM_gizmo_group_type_add_ptr_ex(wmGizmoGroupType *gzgt, wmGizmoMapType *gzmap_type)
{
  WM_gizmomaptype_group_link_ptr(gzmap_type, gzgt);

  WM_gizmoconfig_update_tag_init(gzmap_type, gzgt);
}
void WM_gizmo_group_type_add_ptr(wmGizmoGroupType *gzgt)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
  WM_gizmo_group_type_add_ptr_ex(gzgt, gzmap_type);
}
void WM_gizmo_group_type_add(const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  BLI_assert(gzgt != NULL);
  WM_gizmo_group_type_add_ptr(gzgt);
}

bool WM_gizmo_group_type_ensure_ptr_ex(wmGizmoGroupType *gzgt, wmGizmoMapType *gzmap_type)
{
  wmGizmoGroupTypeRef *gzgt_ref = WM_gizmomaptype_group_find_ptr(gzmap_type, gzgt);
  if (gzgt_ref == NULL) {
    WM_gizmo_group_type_add_ptr_ex(gzgt, gzmap_type);
    return true;
  }
  return false;
}
bool WM_gizmo_group_type_ensure_ptr(wmGizmoGroupType *gzgt)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
  return WM_gizmo_group_type_ensure_ptr_ex(gzgt, gzmap_type);
}
bool WM_gizmo_group_type_ensure(const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  BLI_assert(gzgt != NULL);
  return WM_gizmo_group_type_ensure_ptr(gzgt);
}

void WM_gizmo_group_type_remove_ptr_ex(struct Main *bmain,
                                       wmGizmoGroupType *gzgt,
                                       wmGizmoMapType *gzmap_type)
{
  WM_gizmomaptype_group_unlink(NULL, bmain, gzmap_type, gzgt);
  WM_gizmogrouptype_free_ptr(gzgt);
}
void WM_gizmo_group_type_remove_ptr(struct Main *bmain, wmGizmoGroupType *gzgt)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
  WM_gizmo_group_type_remove_ptr_ex(bmain, gzgt, gzmap_type);
}
void WM_gizmo_group_type_remove(struct Main *bmain, const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  BLI_assert(gzgt != NULL);
  WM_gizmo_group_type_remove_ptr(bmain, gzgt);
}

void WM_gizmo_group_type_reinit_ptr_ex(struct Main *bmain,
                                       wmGizmoGroupType *gzgt,
                                       wmGizmoMapType *gzmap_type)
{
  wmGizmoGroupTypeRef *gzgt_ref = WM_gizmomaptype_group_find_ptr(gzmap_type, gzgt);
  BLI_assert(gzgt_ref != NULL);
  UNUSED_VARS_NDEBUG(gzgt_ref);
  WM_gizmomaptype_group_unlink(NULL, bmain, gzmap_type, gzgt);
  WM_gizmo_group_type_add_ptr_ex(gzgt, gzmap_type);
}
void WM_gizmo_group_type_reinit_ptr(struct Main *bmain, wmGizmoGroupType *gzgt)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
  WM_gizmo_group_type_reinit_ptr_ex(bmain, gzgt, gzmap_type);
}
void WM_gizmo_group_type_reinit(struct Main *bmain, const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  BLI_assert(gzgt != NULL);
  WM_gizmo_group_type_reinit_ptr(bmain, gzgt);
}

/* delayed versions */

void WM_gizmo_group_type_unlink_delayed_ptr_ex(wmGizmoGroupType *gzgt, wmGizmoMapType *gzmap_type)
{
  WM_gizmoconfig_update_tag_remove(gzmap_type, gzgt);
}

void WM_gizmo_group_type_unlink_delayed_ptr(wmGizmoGroupType *gzgt)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
  WM_gizmo_group_type_unlink_delayed_ptr_ex(gzgt, gzmap_type);
}

void WM_gizmo_group_type_unlink_delayed(const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  BLI_assert(gzgt != NULL);
  WM_gizmo_group_type_unlink_delayed_ptr(gzgt);
}

/** \} */
