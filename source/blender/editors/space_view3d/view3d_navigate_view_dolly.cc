/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BKE_context.hh"
#include "BKE_report.h"

#include "BLI_math_vector.h"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"

#include "RNA_access.hh"

#include "ED_screen.hh"

#include "view3d_intern.h"
#include "view3d_navigate.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Dolly Operator
 *
 * Like zoom but translates the view offset along the view direction
 * which avoids #RegionView3D.dist approaching zero.
 * \{ */

/* This is an exact copy of #viewzoom_modal_keymap. */
void viewdolly_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {VIEW_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {VIEW_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},

      {VIEWROT_MODAL_SWITCH_ROTATE, "SWITCH_TO_ROTATE", 0, "Switch to Rotate"},
      {VIEWROT_MODAL_SWITCH_MOVE, "SWITCH_TO_MOVE", 0, "Switch to Move"},

      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "View3D Dolly Modal");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "View3D Dolly Modal", modal_items);

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_dolly");
}

static bool viewdolly_offset_lock_check(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  if (ED_view3d_offset_lock_check(v3d, rv3d)) {
    BKE_report(op->reports, RPT_WARNING, "Cannot dolly when the view offset is locked");
    return true;
  }
  return false;
}

static void view_dolly_to_vector_3d(ARegion *region,
                                    const float orig_ofs[3],
                                    const float dvec[3],
                                    float dfac)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  madd_v3_v3v3fl(rv3d->ofs, orig_ofs, dvec, -(1.0f - dfac));
}

static void viewdolly_apply(ViewOpsData *vod, const int xy[2], const bool zoom_invert)
{
  float zfac = 1.0;

  {
    float len1, len2;

    if (U.uiflag & USER_ZOOM_HORIZ) {
      len1 = (vod->region->winrct.xmax - xy[0]) + 5;
      len2 = (vod->region->winrct.xmax - vod->init.event_xy[0]) + 5;
    }
    else {
      len1 = (vod->region->winrct.ymax - xy[1]) + 5;
      len2 = (vod->region->winrct.ymax - vod->init.event_xy[1]) + 5;
    }
    if (zoom_invert) {
      std::swap(len1, len2);
    }

    zfac = 1.0f + ((len1 - len2) * 0.01f * vod->rv3d->dist);
  }

  if (zfac != 1.0f) {
    view_dolly_to_vector_3d(vod->region, vod->init.ofs, vod->init.mousevec, zfac);
  }

  if (RV3D_LOCK_FLAGS(vod->rv3d) & RV3D_BOXVIEW) {
    view3d_boxview_sync(vod->area, vod->region);
  }

  ED_view3d_camera_lock_sync(vod->depsgraph, vod->v3d, vod->rv3d);

  ED_region_tag_redraw(vod->region);
}

static int viewdolly_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod = static_cast<ViewOpsData *>(op->customdata);
  short event_code = VIEW_PASS;
  bool use_autokey = false;
  int ret = OPERATOR_RUNNING_MODAL;

  /* Execute the events. */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case VIEW_MODAL_CONFIRM:
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_MOVE:
        WM_operator_name_call(C, "VIEW3D_OT_move", WM_OP_INVOKE_DEFAULT, nullptr, event);
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_ROTATE:
        WM_operator_name_call(C, "VIEW3D_OT_rotate", WM_OP_INVOKE_DEFAULT, nullptr, event);
        event_code = VIEW_CONFIRM;
        break;
    }
  }
  else {
    if (event->type == MOUSEMOVE) {
      event_code = VIEW_APPLY;
    }
    else if (event->type == vod->init.event_type) {
      if (event->val == KM_RELEASE) {
        event_code = VIEW_CONFIRM;
      }
    }
    else if (event->type == EVT_ESCKEY) {
      if (event->val == KM_PRESS) {
        event_code = VIEW_CANCEL;
      }
    }
  }

  switch (event_code) {
    case VIEW_APPLY: {
      viewdolly_apply(vod, event->xy, (U.uiflag & USER_ZOOM_INVERT) != 0);
      if (ED_screen_animation_playing(CTX_wm_manager(C))) {
        use_autokey = true;
      }
      break;
    }
    case VIEW_CONFIRM: {
      use_autokey = true;
      ret = OPERATOR_FINISHED;
      break;
    }
    case VIEW_CANCEL: {
      vod->state_restore();
      ret = OPERATOR_CANCELLED;
      break;
    }
  }

  if (use_autokey) {
    ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);
  }

  if ((ret & OPERATOR_RUNNING_MODAL) == 0) {
    if (ret & OPERATOR_FINISHED) {
      ED_view3d_camera_lock_undo_push(op->type->name, vod->v3d, vod->rv3d, C);
    }
    viewops_data_free(C, vod);
    op->customdata = nullptr;
  }

  return ret;
}

static int viewdolly_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  RegionView3D *rv3d;
  ScrArea *area;
  ARegion *region;
  float mousevec[3];

  const int delta = RNA_int_get(op->ptr, "delta");

  if (op->customdata) {
    ViewOpsData *vod = static_cast<ViewOpsData *>(op->customdata);

    area = vod->area;
    region = vod->region;
    copy_v3_v3(mousevec, vod->init.mousevec);
  }
  else {
    area = CTX_wm_area(C);
    region = CTX_wm_region(C);
    negate_v3_v3(mousevec, static_cast<RegionView3D *>(region->regiondata)->viewinv[2]);
    normalize_v3(mousevec);
  }

  v3d = static_cast<View3D *>(area->spacedata.first);
  rv3d = static_cast<RegionView3D *>(region->regiondata);

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  /* overwrite the mouse vector with the view direction (zoom into the center) */
  if ((use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)) == 0) {
    normalize_v3_v3(mousevec, rv3d->viewinv[2]);
    negate_v3(mousevec);
  }

  view_dolly_to_vector_3d(region, rv3d->ofs, mousevec, delta < 0 ? 1.8f : 0.2f);

  if (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXVIEW) {
    view3d_boxview_sync(area, region);
  }

  ED_view3d_camera_lock_sync(CTX_data_ensure_evaluated_depsgraph(C), v3d, rv3d);

  ED_region_tag_redraw(region);

  viewops_data_free(C, static_cast<ViewOpsData *>(op->customdata));
  op->customdata = nullptr;

  return OPERATOR_FINISHED;
}

/* copied from viewzoom_invoke(), changes here may apply there */
static int viewdolly_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod;

  if (viewdolly_offset_lock_check(C, op)) {
    return OPERATOR_CANCELLED;
  }

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  vod = viewops_data_create(C, event, &ViewOpsType_dolly, use_cursor_init);
  op->customdata = vod;

  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->region);

  /* needs to run before 'viewops_data_create' so the backup 'rv3d->ofs' is correct */
  /* switch from camera view when: */
  if (vod->rv3d->persp != RV3D_PERSP) {
    if (vod->rv3d->persp == RV3D_CAMOB) {
      /* ignore rv3d->lpersp because dolly only makes sense in perspective mode */
      const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
      ED_view3d_persp_switch_from_camera(depsgraph, vod->v3d, vod->rv3d, RV3D_PERSP);
    }
    else {
      vod->rv3d->persp = RV3D_PERSP;
    }
    ED_region_tag_redraw(vod->region);
  }

  /* if one or the other zoom position aren't set, set from event */
  if (!RNA_struct_property_is_set(op->ptr, "mx") || !RNA_struct_property_is_set(op->ptr, "my")) {
    RNA_int_set(op->ptr, "mx", event->xy[0]);
    RNA_int_set(op->ptr, "my", event->xy[1]);
  }

  if (RNA_struct_property_is_set(op->ptr, "delta")) {
    viewdolly_exec(C, op);
  }
  else {
    /* overwrite the mouse vector with the view direction (zoom into the center) */
    if ((use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)) == 0) {
      negate_v3_v3(vod->init.mousevec, vod->rv3d->viewinv[2]);
      normalize_v3(vod->init.mousevec);
    }

    if (event->type == MOUSEZOOM) {
      /* Bypass Zoom invert flag for track pads (pass false always) */

      if (U.uiflag & USER_ZOOM_HORIZ) {
        vod->init.event_xy[0] = vod->prev.event_xy[0] = event->xy[0];
      }
      else {
        /* Set y move = x move as MOUSEZOOM uses only x axis to pass magnification value */
        vod->init.event_xy[1] = vod->prev.event_xy[1] = vod->init.event_xy[1] + event->xy[0] -
                                                        event->prev_xy[0];
      }
      viewdolly_apply(vod, event->prev_xy, (U.uiflag & USER_ZOOM_INVERT) == 0);

      viewops_data_free(C, static_cast<ViewOpsData *>(op->customdata));
      op->customdata = nullptr;
      return OPERATOR_FINISHED;
    }

    /* add temp handler */
    WM_event_add_modal_handler(C, op);
    return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_FINISHED;
}

void VIEW3D_OT_dolly(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Dolly View";
  ot->description = "Dolly in/out in the view";
  ot->idname = ViewOpsType_dolly.idname;

  /* api callbacks */
  ot->invoke = viewdolly_invoke;
  ot->exec = viewdolly_exec;
  ot->modal = viewdolly_modal;
  ot->poll = view3d_rotation_poll;
  ot->cancel = view3d_navigate_cancel_fn;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  view3d_operator_properties_common(
      ot, V3D_OP_PROP_DELTA | V3D_OP_PROP_MOUSE_CO | V3D_OP_PROP_USE_MOUSE_INIT);
}

/** \} */

ViewOpsType ViewOpsType_dolly = {
    /*flag*/ (VIEWOPS_FLAG_DEPTH_NAVIGATE | VIEWOPS_FLAG_ZOOM_TO_MOUSE),
    /*idname*/ "VIEW3D_OT_dolly",
    /*poll_fn*/ nullptr,
    /*init_fn*/ nullptr,
    /*apply_fn*/ nullptr,
};
