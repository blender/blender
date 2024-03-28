/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Gizmo-Group
 *
 * Gizmo-groups store and manage groups of gizmos. They can be
 * attached to modal handlers and have their own keymaps.
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_buffer.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_workspace.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"
#include "wm_event_system.hh"

#include "ED_screen.hh"
#include "ED_undo.hh"

/* Own includes. */
#include "wm_gizmo_intern.hh"
#include "wm_gizmo_wmapi.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

/* -------------------------------------------------------------------- */
/** \name wmGizmoGroup
 * \{ */

wmGizmoGroup *wm_gizmogroup_new_from_type(wmGizmoMap *gzmap, wmGizmoGroupType *gzgt)
{
  wmGizmoGroup *gzgroup = static_cast<wmGizmoGroup *>(
      MEM_callocN(sizeof(*gzgroup), "gizmo-group"));

  gzgroup->type = gzgt;
  gzgroup->type->users += 1;

  /* Keep back-link. */
  gzgroup->parent_gzmap = gzmap;

  BLI_addtail(&gzmap->groups, gzgroup);

  return gzgroup;
}

wmGizmoGroup *wm_gizmogroup_find_by_type(const wmGizmoMap *gzmap, const wmGizmoGroupType *gzgt)
{
  return static_cast<wmGizmoGroup *>(
      BLI_findptr(&gzmap->groups, gzgt, offsetof(wmGizmoGroup, type)));
}

void wm_gizmogroup_free(bContext *C, wmGizmoGroup *gzgroup)
{
  wmGizmoMap *gzmap = gzgroup->parent_gzmap;

  /* Similar to WM_gizmo_unlink, but only to keep gzmap state correct,
   * we don't want to run callbacks. */
  if (gzmap->gzmap_context.highlight && gzmap->gzmap_context.highlight->parent_gzgroup == gzgroup)
  {
    wm_gizmomap_highlight_set(gzmap, C, nullptr, 0);
  }
  if (gzmap->gzmap_context.modal && gzmap->gzmap_context.modal->parent_gzgroup == gzgroup) {
    wm_gizmomap_modal_set(gzmap, C, gzmap->gzmap_context.modal, nullptr, false);
  }

  for (wmGizmo *gz = static_cast<wmGizmo *>(gzgroup->gizmos.first), *gz_next; gz; gz = gz_next) {
    gz_next = gz->next;
    if (gzmap->gzmap_context.select.len) {
      WM_gizmo_select_unlink(gzmap, gz);
    }
    WM_gizmo_free(gz);
  }
  BLI_listbase_clear(&gzgroup->gizmos);

#ifdef WITH_PYTHON
  if (gzgroup->py_instance) {
    /* Do this first in case there are any `__del__` functions or
     * similar that use properties. */
    BPY_DECREF_RNA_INVALIDATE(gzgroup->py_instance);
  }
#endif

  if (gzgroup->reports && (gzgroup->reports->flag & RPT_FREE)) {
    BKE_reports_free(gzgroup->reports);
    MEM_freeN(gzgroup->reports);
  }

  if (gzgroup->customdata_free) {
    gzgroup->customdata_free(gzgroup->customdata);
  }
  else {
    MEM_SAFE_FREE(gzgroup->customdata);
  }

  BLI_remlink(&gzmap->groups, gzgroup);

  if (gzgroup->tag_remove == false) {
    gzgroup->type->users -= 1;
  }

  MEM_freeN(gzgroup);
}

void WM_gizmo_group_tag_remove(wmGizmoGroup *gzgroup)
{
  if (gzgroup->tag_remove == false) {
    gzgroup->tag_remove = true;
    gzgroup->type->users -= 1;
    BLI_assert(gzgroup->type->users >= 0);
    WM_gizmoconfig_update_tag_group_remove(gzgroup->parent_gzmap);
  }
}

void wm_gizmogroup_gizmo_register(wmGizmoGroup *gzgroup, wmGizmo *gz)
{
  BLI_assert(BLI_findindex(&gzgroup->gizmos, gz) == -1);
  BLI_addtail(&gzgroup->gizmos, gz);
  gz->parent_gzgroup = gzgroup;
}

int WM_gizmo_cmp_temp_fl(const void *gz_a_ptr, const void *gz_b_ptr)
{
  const wmGizmo *gz_a = static_cast<const wmGizmo *>(gz_a_ptr);
  const wmGizmo *gz_b = static_cast<const wmGizmo *>(gz_b_ptr);
  if (gz_a->temp.f < gz_b->temp.f) {
    return -1;
  }
  if (gz_a->temp.f > gz_b->temp.f) {
    return 1;
  }
  return 0;
}

int WM_gizmo_cmp_temp_fl_reverse(const void *gz_a_ptr, const void *gz_b_ptr)
{
  const wmGizmo *gz_a = static_cast<const wmGizmo *>(gz_a_ptr);
  const wmGizmo *gz_b = static_cast<const wmGizmo *>(gz_b_ptr);
  if (gz_a->temp.f < gz_b->temp.f) {
    return 1;
  }
  if (gz_a->temp.f > gz_b->temp.f) {
    return -1;
  }
  return 0;
}

static bool wm_gizmo_keymap_uses_event_modifier(wmWindowManager *wm,
                                                const wmGizmoGroup *gzgroup,
                                                wmGizmo *gz,
                                                const int event_modifier,
                                                int *r_gzgroup_keymap_uses_modifier)
{
  if (gz->keymap) {
    wmKeyMap *keymap = WM_keymap_active(wm, gz->keymap);
    if (!WM_keymap_uses_event_modifier(keymap, event_modifier)) {
      return false;
    }
  }
  else if (gzgroup->type->keymap) {
    if (*r_gzgroup_keymap_uses_modifier == -1) {
      wmKeyMap *keymap = WM_keymap_active(wm, gzgroup->type->keymap);
      *r_gzgroup_keymap_uses_modifier = WM_keymap_uses_event_modifier(keymap, event_modifier);
    }
    if (*r_gzgroup_keymap_uses_modifier == 0) {
      return false;
    }
  }
  return true;
}

wmGizmo *wm_gizmogroup_find_intersected_gizmo(wmWindowManager *wm,
                                              const wmGizmoGroup *gzgroup,
                                              bContext *C,
                                              const int event_modifier,
                                              const int mval[2],
                                              int *r_part)
{
  int gzgroup_keymap_uses_modifier = -1;

  LISTBASE_FOREACH (wmGizmo *, gz, &gzgroup->gizmos) {
    if (gz->type->test_select && (gz->flag & (WM_GIZMO_HIDDEN | WM_GIZMO_HIDDEN_SELECT)) == 0) {

      if (!wm_gizmo_keymap_uses_event_modifier(
              wm, gzgroup, gz, event_modifier, &gzgroup_keymap_uses_modifier))
      {
        continue;
      }

      if ((*r_part = gz->type->test_select(C, gz, mval)) != -1) {
        return gz;
      }
    }
  }

  return nullptr;
}

void wm_gizmogroup_intersectable_gizmos_to_list(wmWindowManager *wm,
                                                const wmGizmoGroup *gzgroup,
                                                const int event_modifier,
                                                BLI_Buffer *visible_gizmos)
{
  int gzgroup_keymap_uses_modifier = -1;
  LISTBASE_FOREACH_BACKWARD (wmGizmo *, gz, &gzgroup->gizmos) {
    if ((gz->flag & (WM_GIZMO_HIDDEN | WM_GIZMO_HIDDEN_SELECT)) == 0) {
      if (((gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) &&
           (gz->type->draw_select || gz->type->test_select)) ||
          ((gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) == 0 && gz->type->test_select))
      {

        if (!wm_gizmo_keymap_uses_event_modifier(
                wm, gzgroup, gz, event_modifier, &gzgroup_keymap_uses_modifier))
        {
          continue;
        }

        BLI_buffer_append(visible_gizmos, wmGizmo *, gz);
      }
    }
  }
}

void WM_gizmogroup_ensure_init(const bContext *C, wmGizmoGroup *gzgroup)
{
  /* Prepare for first draw. */
  if (UNLIKELY((gzgroup->init_flag & WM_GIZMOGROUP_INIT_SETUP) == 0)) {

    gzgroup->type->setup(C, gzgroup);

    /* Not ideal, initialize keymap here, needed for RNA runtime generated gizmos. */
    wmGizmoGroupType *gzgt = gzgroup->type;
    if (gzgt->keymap == nullptr) {
      wmWindowManager *wm = CTX_wm_manager(C);
      wm_gizmogrouptype_setup_keymap(gzgt, wm->defaultconf);
      BLI_assert(gzgt->keymap != nullptr);
    }
    gzgroup->init_flag |= WM_GIZMOGROUP_INIT_SETUP;
  }

  /* Refresh may be called multiple times,
   * this just ensures its called at least once before we draw. */
  if (UNLIKELY((gzgroup->init_flag & WM_GIZMOGROUP_INIT_REFRESH) == 0)) {
    /* Clear the flag before calling refresh so the callback
     * can postpone the refresh by clearing this flag. */
    gzgroup->init_flag |= WM_GIZMOGROUP_INIT_REFRESH;
    WM_gizmo_group_refresh(C, gzgroup);
  }
}

void WM_gizmo_group_remove_by_tool(bContext *C,
                                   Main *bmain,
                                   const wmGizmoGroupType *gzgt,
                                   const bToolRef *tref)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_find(&gzgt->gzmap_params);
  for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->runtime.tool == tref) {
        LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
          wmGizmoMap *gzmap = region->gizmo_map;
          if (gzmap && gzmap->type == gzmap_type) {
            wmGizmoGroup *gzgroup, *gzgroup_next;
            for (gzgroup = static_cast<wmGizmoGroup *>(gzmap->groups.first); gzgroup;
                 gzgroup = gzgroup_next)
            {
              gzgroup_next = gzgroup->next;
              if (gzgroup->type == gzgt) {
                BLI_assert(gzgroup->parent_gzmap == gzmap);
                wm_gizmogroup_free(C, gzgroup);
                ED_region_tag_redraw_editor_overlays(region);
              }
            }
          }
        }
      }
    }
  }
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
      BLI_assert_unreachable();
      return false;
  }
}

bool wm_gizmogroup_is_any_selected(const wmGizmoGroup *gzgroup)
{
  if (gzgroup->type->flag & WM_GIZMOGROUPTYPE_SELECT) {
    LISTBASE_FOREACH (const wmGizmo *, gz, &gzgroup->gizmos) {
      if (gz->state & WM_GIZMO_STATE_SELECT) {
        return true;
      }
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmo Operators
 *
 * Basic operators for gizmo interaction with user configurable keymaps.
 * \{ */

static int gizmo_select_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  ARegion *region = CTX_wm_region(C);
  wmGizmoMap *gzmap = region->gizmo_map;
  wmGizmoMapSelectState *msel = &gzmap->gzmap_context.select;
  wmGizmo *highlight = gzmap->gzmap_context.highlight;

  bool extend = RNA_boolean_get(op->ptr, "extend");
  bool deselect = RNA_boolean_get(op->ptr, "deselect");
  bool toggle = RNA_boolean_get(op->ptr, "toggle");

  /* Deselect all first. */
  if (extend == false && deselect == false && toggle == false) {
    wm_gizmomap_deselect_all(gzmap);
    BLI_assert(msel->items == nullptr && msel->len == 0);
    UNUSED_VARS_NDEBUG(msel);
  }

  if (highlight) {
    const bool is_selected = (highlight->state & WM_GIZMO_STATE_SELECT);
    bool redraw = false;

    if (toggle) {
      /* Toggle: deselect if already selected, else select. */
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
      ED_region_tag_redraw_editor_overlays(region);
    }

    return OPERATOR_FINISHED;
  }

  BLI_assert_unreachable();
  return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
}

void GIZMOGROUP_OT_gizmo_select(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Gizmo Select";
  ot->description = "Select the currently highlighted gizmo";
  ot->idname = "GIZMOGROUP_OT_gizmo_select";

  /* API callbacks. */
  ot->invoke = gizmo_select_invoke;
  ot->poll = ED_operator_region_gizmo_active;

  ot->flag = OPTYPE_UNDO;

  WM_operator_properties_mouse_select(ot);
}

struct GizmoTweakData {
  wmGizmoMap *gzmap;
  wmGizmoGroup *gzgroup;
  wmGizmo *gz_modal;

  int init_event; /* Initial event type. */
  int flag;       /* Tweak flags. */
};

static bool gizmo_tweak_start(bContext *C, wmGizmoMap *gzmap, wmGizmo *gz, const wmEvent *event)
{
  /* Activate highlighted gizmo. */
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

    /* Undo/Redo. */
    if (gzop->is_redo) {
      wmWindowManager *wm = CTX_wm_manager(C);
      wmOperator *op = WM_operator_last_redo(C);

/* We may want to enable this, for now the gizmo can manage its own properties. */
#if 0
      IDP_MergeGroup(gzop->ptr.data, op->properties, false);
#endif

      WM_operator_free_all_after(wm, op);
      ED_undo_pop_op(C, op);
    }

    /* XXX temporary workaround for modal gizmo operator
     * conflicting with modal operator attached to gizmo. */
    if (gzop->type->modal) {
      /* Activate highlighted gizmo. */
      wm_gizmomap_modal_set(gzmap, C, gz, event, true);
      if (r_is_modal) {
        *r_is_modal = true;
      }
    }
    else {
      if (gz->parent_gzgroup->type->invoke_prepare) {
        gz->parent_gzgroup->type->invoke_prepare(C, gz->parent_gzgroup, gz, event);
      }
      /* Allow for 'button' gizmos, single click to run an action. */
      WM_gizmo_operator_invoke(C, gz, gzop, event);
    }
    return true;
  }
  return false;
}

static void gizmo_tweak_finish(bContext *C, wmOperator *op, const bool cancel, bool clear_modal)
{
  GizmoTweakData *mtweak = static_cast<GizmoTweakData *>(op->customdata);
  if (mtweak->gz_modal->type->exit) {
    mtweak->gz_modal->type->exit(C, mtweak->gz_modal, cancel);
  }
  if (clear_modal) {
    /* The gizmo may have been removed. */
    if ((BLI_findindex(&mtweak->gzmap->groups, mtweak->gzgroup) != -1) &&
        (BLI_findindex(&mtweak->gzgroup->gizmos, mtweak->gz_modal) != -1))
    {
      wm_gizmomap_modal_set(mtweak->gzmap, C, mtweak->gz_modal, nullptr, false);
    }
  }
  MEM_freeN(mtweak);
}

static int gizmo_tweak_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  GizmoTweakData *mtweak = static_cast<GizmoTweakData *>(op->customdata);
  wmGizmo *gz = mtweak->gz_modal;
  int retval = OPERATOR_PASS_THROUGH;
  bool clear_modal = true;

  if (gz == nullptr) {
    BLI_assert_unreachable();
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  if (retval == OPERATOR_FINISHED) {
    /* Pass. */
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

  /* Handle gizmo. */
  wmGizmoFnModal modal_fn = gz->custom_modal ? gz->custom_modal : gz->type->modal;
  if (modal_fn) {
    /* Ugly hack to ensure Python won't get 'EVT_MODAL_MAP' which isn't supported, see #73727.
     * note that we could move away from wrapping modal gizmos in a modal operator,
     * since it's causing the need for code like this. */
    wmEvent *evil_event = (wmEvent *)event;
    short event_modal_val = 0;

    if (event->type == EVT_MODAL_MAP) {
      event_modal_val = evil_event->val;
      evil_event->type = evil_event->prev_type;
      evil_event->val = evil_event->prev_val;
    }

    int modal_retval = modal_fn(C, gz, event, eWM_GizmoFlagTweak(mtweak->flag));

    if (event_modal_val != 0) {
      evil_event->type = EVT_MODAL_MAP;
      evil_event->val = event_modal_val;
    }

    if ((modal_retval & OPERATOR_RUNNING_MODAL) == 0) {
      gizmo_tweak_finish(C, op, (modal_retval & OPERATOR_CANCELLED) != 0, true);
      return OPERATOR_FINISHED;
    }

    /* Ugly hack to send gizmo events. */
    evil_event->type = EVT_GIZMO_UPDATE;
  }

  /* Always return PASS_THROUGH so modal handlers
   * with gizmos attached can update. */
  BLI_assert(retval == OPERATOR_PASS_THROUGH);
  return OPERATOR_PASS_THROUGH;
}

static int gizmo_tweak_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  wmGizmoMap *gzmap = region->gizmo_map;
  wmGizmo *gz = gzmap->gzmap_context.highlight;

  /* Needed for single click actions which don't enter modal state. */
  WM_tooltip_clear(C, CTX_wm_window(C));

  if (!gz) {
    /* #wm_handlers_do_intern shouldn't let this happen. */
    BLI_assert_unreachable();
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  const int highlight_part_init = gz->highlight_part;

  if (gz->drag_part != -1) {
    if (WM_event_is_mouse_drag(event)) {
      gz->highlight_part = gz->drag_part;
    }
  }

  if (gizmo_tweak_start_and_finish(C, gzmap, gz, event, nullptr)) {
    return OPERATOR_FINISHED;
  }

  if (!gizmo_tweak_start(C, gzmap, gz, event)) {
    /* Failed to start. */
    gz->highlight_part = highlight_part_init;
    return OPERATOR_PASS_THROUGH;
  }

  GizmoTweakData *mtweak = static_cast<GizmoTweakData *>(
      MEM_mallocN(sizeof(GizmoTweakData), __func__));

  mtweak->init_event = WM_userdef_event_type_from_keymap_type(event->type);
  mtweak->gz_modal = gzmap->gzmap_context.highlight;
  mtweak->gzgroup = mtweak->gz_modal->parent_gzgroup;
  mtweak->gzmap = gzmap;
  mtweak->flag = 0;

  op->customdata = mtweak;

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void GIZMOGROUP_OT_gizmo_tweak(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Gizmo Tweak";
  ot->description = "Tweak the active gizmo";
  ot->idname = "GIZMOGROUP_OT_gizmo_tweak";

  /* API callbacks. */
  ot->invoke = gizmo_tweak_invoke;
  ot->modal = gizmo_tweak_modal;
  ot->poll = ED_operator_region_gizmo_active;

/* TODO(@ideasman42): This causes problems tweaking settings for operators,
 * need to find a way to support this. */
#if 0
  ot->flag = OPTYPE_UNDO;
#endif
}

wmKeyMap *wm_gizmogroup_tweak_modal_keymap(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap;
  char name[KMAP_MAX_NAME];

  static const EnumPropertyItem modal_items[] = {
      {TWEAK_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {TWEAK_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {TWEAK_MODAL_PRECISION_ON, "PRECISION_ON", 0, "Enable Precision", ""},
      {TWEAK_MODAL_PRECISION_OFF, "PRECISION_OFF", 0, "Disable Precision", ""},
      {TWEAK_MODAL_SNAP_ON, "SNAP_ON", 0, "Enable Snap", ""},
      {TWEAK_MODAL_SNAP_OFF, "SNAP_OFF", 0, "Disable Snap", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  STRNCPY(name, "Generic Gizmo Tweak Modal Map");
  keymap = WM_modalkeymap_find(keyconf, name);

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return nullptr;
  }

  keymap = WM_modalkeymap_ensure(keyconf, name, modal_items);

  /* Items for modal map. */
  {
    KeyMapItem_Params params{};
    params.type = EVT_ESCKEY;
    params.value = KM_PRESS;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_CANCEL);
  }
  {
    KeyMapItem_Params params{};
    params.type = RIGHTMOUSE;
    params.value = KM_PRESS;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_CANCEL);
  }
  {
    KeyMapItem_Params params{};
    params.type = EVT_RETKEY;
    params.value = KM_PRESS;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_CONFIRM);
  }
  {
    KeyMapItem_Params params{};
    params.type = EVT_PADENTER;
    params.value = KM_PRESS;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_CONFIRM);
  }
  {
    KeyMapItem_Params params{};
    params.type = EVT_RIGHTSHIFTKEY;
    params.value = KM_PRESS;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_PRECISION_ON);
  }
  {
    KeyMapItem_Params params{};
    params.type = EVT_RIGHTSHIFTKEY;
    params.value = KM_RELEASE;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_PRECISION_OFF);
  }
  {
    KeyMapItem_Params params{};
    params.type = EVT_LEFTSHIFTKEY;
    params.value = KM_PRESS;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_PRECISION_ON);
  }
  {
    KeyMapItem_Params params{};
    params.type = EVT_LEFTSHIFTKEY;
    params.value = KM_RELEASE;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_PRECISION_OFF);
  }
  {
    KeyMapItem_Params params{};
    params.type = EVT_RIGHTCTRLKEY;
    params.value = KM_PRESS;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_SNAP_ON);
  }
  {
    KeyMapItem_Params params{};
    params.type = EVT_RIGHTCTRLKEY;
    params.value = KM_RELEASE;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_SNAP_OFF);
  }
  {
    KeyMapItem_Params params{};
    params.type = EVT_LEFTCTRLKEY;
    params.value = KM_PRESS;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_SNAP_ON);
  }
  {
    KeyMapItem_Params params{};
    params.type = EVT_LEFTCTRLKEY;
    params.value = KM_RELEASE;
    params.modifier = KM_ANY;
    params.direction = KM_ANY;
    WM_modalkeymap_add_item(keymap, &params, TWEAK_MODAL_SNAP_OFF);
  }

  WM_modalkeymap_assign(keymap, "GIZMOGROUP_OT_gizmo_tweak");

  return keymap;
}

/** \} */ /* #wmGizmoGroup. */

/* -------------------------------------------------------------------- */
/** \name wmGizmoGroup (Key-map callbacks)
 * \{ */

wmKeyMap *WM_gizmogroup_setup_keymap_generic(const wmGizmoGroupType * /*gzgt*/, wmKeyConfig *kc)
{
  return WM_gizmo_keymap_generic_with_keyconfig(kc);
}

wmKeyMap *WM_gizmogroup_setup_keymap_generic_drag(const wmGizmoGroupType * /*gzgt*/,
                                                  wmKeyConfig *kc)
{
  return WM_gizmo_keymap_generic_drag_with_keyconfig(kc);
}

wmKeyMap *WM_gizmogroup_setup_keymap_generic_maybe_drag(const wmGizmoGroupType * /*gzgt*/,
                                                        wmKeyConfig *kc)
{
  return WM_gizmo_keymap_generic_maybe_drag_with_keyconfig(kc);
}

/**
 * Variation of #WM_gizmogroup_keymap_common but with keymap items for selection
 *
 * TODO(@ideasman42): move to Python.
 *
 * \param name: Typically #wmGizmoGroupType.name
 * \param params: Typically #wmGizmoGroupType.gzmap_params
 */
static wmKeyMap *WM_gizmogroup_keymap_template_select_ex(wmKeyConfig *kc,
                                                         const char *name,
                                                         const wmGizmoMapType_Params *params)
{
  /* Use area and region id since we might have multiple gizmos
   * with the same name in different areas/regions. */
  wmKeyMap *km = WM_keymap_ensure(kc, name, params->spaceid, params->regionid);
  const bool do_init = BLI_listbase_is_empty(&km->items);

/* FIXME(@ideasman42): Currently hard coded. */
#if 0
  const int select_mouse = (U.flag & USER_LMOUSESELECT) ? LEFTMOUSE : RIGHTMOUSE;
  const int select_tweak = (U.flag & USER_LMOUSESELECT) ? EVT_TWEAK_L : EVT_TWEAK_R;
  const int action_mouse = (U.flag & USER_LMOUSESELECT) ? RIGHTMOUSE : LEFTMOUSE;
#else
  const int select_mouse = RIGHTMOUSE, select_mouse_val = KM_PRESS;
  const int select_tweak = RIGHTMOUSE, select_tweak_val = KM_CLICK_DRAG;
  const int action_mouse = LEFTMOUSE, action_mouse_val = KM_PRESS;
#endif

  if (do_init) {
    {
      KeyMapItem_Params params{};
      params.type = action_mouse;
      params.value = action_mouse_val;
      params.modifier = KM_ANY;
      params.direction = KM_ANY;
      WM_keymap_add_item(km, "GIZMOGROUP_OT_gizmo_tweak", &params);
    }
    {
      KeyMapItem_Params params{};
      params.type = select_tweak;
      params.value = select_tweak_val;
      params.modifier = 0;
      params.direction = KM_ANY;
      WM_keymap_add_item(km, "GIZMOGROUP_OT_gizmo_tweak", &params);
    }
  }

  if (do_init) {
    {
      KeyMapItem_Params params{};
      params.type = select_mouse;
      params.value = select_mouse_val;
      params.modifier = 0;
      params.direction = KM_ANY;
      wmKeyMapItem *kmi = WM_keymap_add_item(km, "GIZMOGROUP_OT_gizmo_select", &params);
      RNA_boolean_set(kmi->ptr, "extend", false);
      RNA_boolean_set(kmi->ptr, "deselect", false);
      RNA_boolean_set(kmi->ptr, "toggle", false);
    }
    {
      KeyMapItem_Params params{};
      params.type = select_mouse;
      params.value = select_mouse_val;
      params.modifier = KM_SHIFT;
      params.direction = KM_ANY;
      wmKeyMapItem *kmi = WM_keymap_add_item(km, "GIZMOGROUP_OT_gizmo_select", &params);
      RNA_boolean_set(kmi->ptr, "extend", false);
      RNA_boolean_set(kmi->ptr, "deselect", false);
      RNA_boolean_set(kmi->ptr, "toggle", true);
    }
  }

  return km;
}

wmKeyMap *WM_gizmogroup_setup_keymap_generic_select(const wmGizmoGroupType * /*gzgt*/,
                                                    wmKeyConfig *kc)
{
  wmGizmoMapType_Params params{};
  params.spaceid = SPACE_EMPTY;
  params.regionid = RGN_TYPE_WINDOW;
  return WM_gizmogroup_keymap_template_select_ex(kc, "Generic Gizmo Select", &params);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name wmGizmo (Key-map access)
 *
 * Key config version so these can be called from #wmGizmoGroupFnSetupKeymap.
 * \{ */

wmKeyMap *WM_gizmo_keymap_generic_with_keyconfig(wmKeyConfig *kc)
{
  const char *idname = "Generic Gizmo";
  return WM_keymap_ensure(kc, idname, SPACE_EMPTY, RGN_TYPE_WINDOW);
}
wmKeyMap *WM_gizmo_keymap_generic(wmWindowManager *wm)
{
  return WM_gizmo_keymap_generic_with_keyconfig(wm->defaultconf);
}

wmKeyMap *WM_gizmo_keymap_generic_select_with_keyconfig(wmKeyConfig *kc)
{
  const char *idname = "Generic Gizmo Select";
  return WM_keymap_ensure(kc, idname, SPACE_EMPTY, RGN_TYPE_WINDOW);
}
wmKeyMap *WM_gizmo_keymap_generic_select(wmWindowManager *wm)
{
  return WM_gizmo_keymap_generic_select_with_keyconfig(wm->defaultconf);
}

wmKeyMap *WM_gizmo_keymap_generic_drag_with_keyconfig(wmKeyConfig *kc)
{
  const char *idname = "Generic Gizmo Drag";
  return WM_keymap_ensure(kc, idname, SPACE_EMPTY, RGN_TYPE_WINDOW);
}
wmKeyMap *WM_gizmo_keymap_generic_drag(wmWindowManager *wm)
{
  return WM_gizmo_keymap_generic_drag_with_keyconfig(wm->defaultconf);
}

wmKeyMap *WM_gizmo_keymap_generic_click_drag_with_keyconfig(wmKeyConfig *kc)
{
  const char *idname = "Generic Gizmo Click Drag";
  return WM_keymap_ensure(kc, idname, SPACE_EMPTY, RGN_TYPE_WINDOW);
}
wmKeyMap *WM_gizmo_keymap_generic_click_drag(wmWindowManager *wm)
{
  return WM_gizmo_keymap_generic_click_drag_with_keyconfig(wm->defaultconf);
}

wmKeyMap *WM_gizmo_keymap_generic_maybe_drag_with_keyconfig(wmKeyConfig *kc)
{
  const char *idname = "Generic Gizmo Maybe Drag";
  return WM_keymap_ensure(kc, idname, SPACE_EMPTY, RGN_TYPE_WINDOW);
}
wmKeyMap *WM_gizmo_keymap_generic_maybe_drag(wmWindowManager *wm)
{
  return WM_gizmo_keymap_generic_maybe_drag_with_keyconfig(wm->defaultconf);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name wmGizmoGroupType
 * \{ */

wmGizmoGroupTypeRef *WM_gizmomaptype_group_find_ptr(wmGizmoMapType *gzmap_type,
                                                    const wmGizmoGroupType *gzgt)
{
  /* Could use hash lookups as operator types do, for now simple search. */
  LISTBASE_FOREACH (wmGizmoGroupTypeRef *, gzgt_ref, &gzmap_type->grouptype_refs) {
    if (gzgt_ref->type == gzgt) {
      return gzgt_ref;
    }
  }
  return nullptr;
}

wmGizmoGroupTypeRef *WM_gizmomaptype_group_find(wmGizmoMapType *gzmap_type, const char *idname)
{
  /* Could use hash lookups as operator types do, for now simple search. */
  LISTBASE_FOREACH (wmGizmoGroupTypeRef *, gzgt_ref, &gzmap_type->grouptype_refs) {
    if (STREQ(idname, gzgt_ref->type->idname)) {
      return gzgt_ref;
    }
  }
  return nullptr;
}

wmGizmoGroupTypeRef *WM_gizmomaptype_group_link(wmGizmoMapType *gzmap_type, const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  BLI_assert(gzgt != nullptr);
  return WM_gizmomaptype_group_link_ptr(gzmap_type, gzgt);
}

wmGizmoGroupTypeRef *WM_gizmomaptype_group_link_ptr(wmGizmoMapType *gzmap_type,
                                                    wmGizmoGroupType *gzgt)
{
  wmGizmoGroupTypeRef *gzgt_ref = static_cast<wmGizmoGroupTypeRef *>(
      MEM_callocN(sizeof(wmGizmoGroupTypeRef), "gizmo-group-ref"));
  gzgt_ref->type = gzgt;
  BLI_addtail(&gzmap_type->grouptype_refs, gzgt_ref);
  return gzgt_ref;
}

void WM_gizmomaptype_group_init_runtime_keymap(const Main *bmain, wmGizmoGroupType *gzgt)
{
  /* Initialize key-map.
   * On startup there's an extra call to initialize keymaps for 'permanent' gizmo-groups. */
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

  /* Now create a gizmo for all existing areas. */
  for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        ListBase *lb = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;
        LISTBASE_FOREACH (ARegion *, region, lb) {
          wmGizmoMap *gzmap = region->gizmo_map;
          if (gzmap && gzmap->type == gzmap_type) {
            WM_gizmomaptype_group_init_runtime_with_region(gzmap_type, gzgt, region);
          }
        }
      }
    }
  }
}

wmGizmoGroup *WM_gizmomaptype_group_init_runtime_with_region(wmGizmoMapType *gzmap_type,
                                                             wmGizmoGroupType *gzgt,
                                                             ARegion *region)
{
  wmGizmoMap *gzmap = region->gizmo_map;
  BLI_assert(gzmap && gzmap->type == gzmap_type);
  UNUSED_VARS_NDEBUG(gzmap_type);

  wmGizmoGroup *gzgroup = wm_gizmogroup_new_from_type(gzmap, gzgt);

  /* Don't allow duplicates when switching modes for e.g. see: #66229. */
  LISTBASE_FOREACH (wmGizmoGroup *, gzgroup_iter, &gzmap->groups) {
    if (gzgroup_iter->type == gzgt) {
      if (gzgroup_iter != gzgroup) {
        WM_gizmo_group_tag_remove(gzgroup_iter);
      }
    }
  }

  wm_gizmomap_highlight_set(gzmap, nullptr, nullptr, 0);

  ED_region_tag_redraw_editor_overlays(region);

  return gzgroup;
}

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
  for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        ListBase *lb = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;
        LISTBASE_FOREACH (ARegion *, region, lb) {
          wmGizmoMap *gzmap = region->gizmo_map;
          if (gzmap && gzmap->type == gzmap_type) {
            wmGizmoGroup *gzgroup, *gzgroup_next;
            for (gzgroup = static_cast<wmGizmoGroup *>(gzmap->groups.first); gzgroup;
                 gzgroup = gzgroup_next)
            {
              gzgroup_next = gzgroup->next;
              if (gzgroup->type == gzgt) {
                BLI_assert(gzgroup->parent_gzmap == gzmap);
                wm_gizmogroup_free(C, gzgroup);
                ED_region_tag_redraw_editor_overlays(region);
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

/* TODO(@ideasman42): Gizmos may share key-maps, for now don't
 * remove however we could flag them as temporary/owned by the gizmo. */
#if 0
  /* NOTE: we may want to keep this key-map for editing. */
  WM_keymap_remove(gzgt->keyconf, gzgt->keymap);
#endif

  BLI_assert(WM_gizmomaptype_group_find_ptr(gzmap_type, gzgt) == nullptr);
}

void wm_gizmogrouptype_setup_keymap(wmGizmoGroupType *gzgt, wmKeyConfig *keyconf)
{
  /* Use flag since setup_keymap may return nullptr,
   * in that case we better not keep calling it. */
  if (gzgt->type_update_flag & WM_GIZMOMAPTYPE_KEYMAP_INIT) {
    gzgt->keymap = gzgt->setup_keymap(gzgt, keyconf);
    gzgt->keyconf = keyconf;
    gzgt->type_update_flag &= ~WM_GIZMOMAPTYPE_KEYMAP_INIT;
  }
}

/** \} */ /* #wmGizmoGroupType. */

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
 * \{ */

void WM_gizmo_group_type_add_ptr_ex(wmGizmoGroupType *gzgt, wmGizmoMapType *gzmap_type)
{
  WM_gizmomaptype_group_link_ptr(gzmap_type, gzgt);

  WM_gizmoconfig_update_tag_group_type_init(gzmap_type, gzgt);
}
void WM_gizmo_group_type_add_ptr(wmGizmoGroupType *gzgt)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
  WM_gizmo_group_type_add_ptr_ex(gzgt, gzmap_type);
}
void WM_gizmo_group_type_add(const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  BLI_assert(gzgt != nullptr);
  WM_gizmo_group_type_add_ptr(gzgt);
}

bool WM_gizmo_group_type_ensure_ptr_ex(wmGizmoGroupType *gzgt, wmGizmoMapType *gzmap_type)
{
  wmGizmoGroupTypeRef *gzgt_ref = WM_gizmomaptype_group_find_ptr(gzmap_type, gzgt);
  if (gzgt_ref == nullptr) {
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
  BLI_assert(gzgt != nullptr);
  return WM_gizmo_group_type_ensure_ptr(gzgt);
}

void WM_gizmo_group_type_remove_ptr_ex(Main *bmain,
                                       wmGizmoGroupType *gzgt,
                                       wmGizmoMapType *gzmap_type)
{
  WM_gizmomaptype_group_unlink(nullptr, bmain, gzmap_type, gzgt);
}
void WM_gizmo_group_type_remove_ptr(Main *bmain, wmGizmoGroupType *gzgt)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
  WM_gizmo_group_type_remove_ptr_ex(bmain, gzgt, gzmap_type);
}
void WM_gizmo_group_type_remove(Main *bmain, const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  BLI_assert(gzgt != nullptr);
  WM_gizmo_group_type_remove_ptr(bmain, gzgt);
}

void WM_gizmo_group_type_reinit_ptr_ex(Main *bmain,
                                       wmGizmoGroupType *gzgt,
                                       wmGizmoMapType *gzmap_type)
{
  wmGizmoGroupTypeRef *gzgt_ref = WM_gizmomaptype_group_find_ptr(gzmap_type, gzgt);
  BLI_assert(gzgt_ref != nullptr);
  UNUSED_VARS_NDEBUG(gzgt_ref);
  WM_gizmomaptype_group_unlink(nullptr, bmain, gzmap_type, gzgt);
  WM_gizmo_group_type_add_ptr_ex(gzgt, gzmap_type);
}
void WM_gizmo_group_type_reinit_ptr(Main *bmain, wmGizmoGroupType *gzgt)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
  WM_gizmo_group_type_reinit_ptr_ex(bmain, gzgt, gzmap_type);
}
void WM_gizmo_group_type_reinit(Main *bmain, const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  BLI_assert(gzgt != nullptr);
  WM_gizmo_group_type_reinit_ptr(bmain, gzgt);
}

/* Delayed versions. */

void WM_gizmo_group_type_unlink_delayed_ptr_ex(wmGizmoGroupType *gzgt, wmGizmoMapType *gzmap_type)
{
  WM_gizmoconfig_update_tag_group_type_remove(gzmap_type, gzgt);
}

void WM_gizmo_group_type_unlink_delayed_ptr(wmGizmoGroupType *gzgt)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
  WM_gizmo_group_type_unlink_delayed_ptr_ex(gzgt, gzmap_type);
}

void WM_gizmo_group_type_unlink_delayed(const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  BLI_assert(gzgt != nullptr);
  WM_gizmo_group_type_unlink_delayed_ptr(gzgt);
}

void WM_gizmo_group_unlink_delayed_ptr_from_space(wmGizmoGroupType *gzgt,
                                                  wmGizmoMapType *gzmap_type,
                                                  ScrArea *area)
{
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    wmGizmoMap *gzmap = region->gizmo_map;
    if (gzmap && gzmap->type == gzmap_type) {
      LISTBASE_FOREACH (wmGizmoGroup *, gzgroup, &gzmap->groups) {
        if (gzgroup->type == gzgt) {
          WM_gizmo_group_tag_remove(gzgroup);
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmo Group Type Callback Wrappers
 * \{ */

bool WM_gizmo_group_type_poll(const bContext *C, const wmGizmoGroupType *gzgt)
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

void WM_gizmo_group_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  const wmGizmoGroupType *gzgt = gzgroup->type;
  if (gzgt->flag & WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK) {
    wmGizmoMap *gzmap = gzgroup->parent_gzmap;
    wmGizmo *gz = nullptr;
    /* Without the check for refresh, any highlighted gizmo will prevent hiding
     * when selecting with RMB when the cursor happens to be over a gizmo. */
    if ((gzgroup->init_flag & WM_GIZMOGROUP_INIT_REFRESH) == 0) {
      gz = wm_gizmomap_highlight_get(gzmap);
    }
    if (!gz || gz->parent_gzgroup != gzgroup) {
      wmWindow *win = CTX_wm_window(C);
      ARegion *region = CTX_wm_region(C);
      BLI_assert(region->gizmo_map == gzmap);
      /* Check if the tweak event originated from this region. */
      if ((win->eventstate != nullptr) && (win->event_queue_check_drag) &&
          BLI_rcti_isect_pt_v(&region->winrct, win->eventstate->prev_press_xy))
      {
        /* We need to run refresh again. */
        gzgroup->init_flag &= ~WM_GIZMOGROUP_INIT_REFRESH;
        WM_gizmomap_tag_refresh_drawstep(gzmap, WM_gizmomap_drawstep_from_gizmo_group(gzgroup));
        gzgroup->hide.delay_refresh_for_tweak = true;
        return;
      }
    }
    gzgroup->hide.delay_refresh_for_tweak = false;
  }

  if (gzgroup->hide.any) {
    return;
  }

  if (gzgt->refresh) {
    gzgt->refresh(C, gzgroup);
  }
}

/** \} */
