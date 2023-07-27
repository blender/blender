/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_math.h"

#include "BKE_context.h"

#include "WM_api.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph_query.h"

#include "ED_screen.h"

#include "view3d_intern.h"
#include "view3d_navigate.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Roll Operator
 * \{ */

/**
 * \param use_axis_view: When true, keep axis-aligned orthographic views
 * (when rotating in 90 degree increments). While this may seem obscure some NDOF
 * devices have key shortcuts to do this (see #NDOF_BUTTON_ROLL_CW & #NDOF_BUTTON_ROLL_CCW).
 */
static void view_roll_angle(ARegion *region,
                            float quat[4],
                            const float orig_quat[4],
                            const float dvec[3],
                            float angle,
                            bool use_axis_view)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  float quat_mul[4];

  /* camera axis */
  axis_angle_normalized_to_quat(quat_mul, dvec, angle);

  mul_qt_qtqt(quat, orig_quat, quat_mul);

  /* avoid precision loss over time */
  normalize_qt(quat);

  if (use_axis_view && RV3D_VIEW_IS_AXIS(rv3d->view) && (fabsf(angle) == float(M_PI_2))) {
    ED_view3d_quat_to_axis_view_and_reset_quat(quat, 0.01f, &rv3d->view, &rv3d->view_axis_roll);
  }
  else {
    rv3d->view = RV3D_VIEW_USER;
  }
}

static void viewroll_apply(ViewOpsData *vod, int x, int y)
{
  const float current_position[2] = {float(x), float(y)};
  float angle = BLI_dial_angle(vod->init.dial, current_position);

  if (angle != 0.0f) {
    view_roll_angle(
        vod->region, vod->rv3d->viewquat, vod->init.quat, vod->init.mousevec, angle, false);
  }

  if (vod->use_dyn_ofs) {
    view3d_orbit_apply_dyn_ofs(
        vod->rv3d->ofs, vod->init.ofs, vod->init.quat, vod->rv3d->viewquat, vod->dyn_ofs);
  }

  if (RV3D_LOCK_FLAGS(vod->rv3d) & RV3D_BOXVIEW) {
    view3d_boxview_sync(vod->area, vod->region);
  }

  ED_view3d_camera_lock_sync(vod->depsgraph, vod->v3d, vod->rv3d);

  ED_region_tag_redraw(vod->region);
}

static int viewroll_modal(bContext *C, wmOperator *op, const wmEvent *event)
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
      case VIEW_MODAL_CANCEL:
        event_code = VIEW_CANCEL;
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
      /* Check `vod->init.event_type` first in case RMB was used to invoke.
       * in this case confirming takes precedence over canceling, see: #102937. */
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
      viewroll_apply(vod, event->xy[0], event->xy[1]);
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
    ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, true, false);
  }

  if ((ret & OPERATOR_RUNNING_MODAL) == 0) {
    viewops_data_free(C, static_cast<ViewOpsData *>(op->customdata));
    op->customdata = nullptr;
  }

  return ret;
}

enum {
  V3D_VIEW_STEPLEFT = 1,
  V3D_VIEW_STEPRIGHT,
};

static const EnumPropertyItem prop_view_roll_items[] = {
    {0, "ANGLE", 0, "Roll Angle", "Roll the view using an angle value"},
    {V3D_VIEW_STEPLEFT, "LEFT", 0, "Roll Left", "Roll the view around to the left"},
    {V3D_VIEW_STEPRIGHT, "RIGHT", 0, "Roll Right", "Roll the view around to the right"},
    {0, nullptr, 0, nullptr, nullptr},
};

static int viewroll_exec(bContext *C, wmOperator *op)
{
  ViewOpsData *vod;
  if (op->customdata) {
    vod = static_cast<ViewOpsData *>(op->customdata);
  }
  else {
    vod = new ViewOpsData();
    ED_view3d_context_user_region(C, &vod->v3d, &vod->region);
    vod->rv3d = static_cast<RegionView3D *>(vod->region->regiondata);
  }

  const bool is_camera_lock = ED_view3d_camera_lock_check(vod->v3d, vod->rv3d);
  if (vod->rv3d->persp == RV3D_CAMOB && !is_camera_lock) {
    viewops_data_free(C, vod);
    op->customdata = nullptr;
    return OPERATOR_CANCELLED;
  }

  if (vod->depsgraph == nullptr) {
    vod->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    vod->init_navigation(C, nullptr, &ViewOpsType_roll, false);
  }

  int type = RNA_enum_get(op->ptr, "type");
  float angle = (type == 0) ? RNA_float_get(op->ptr, "angle") : DEG2RADF(U.pad_rot_angle);
  float mousevec[3];
  float quat_new[4];

  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  if (type == V3D_VIEW_STEPLEFT) {
    angle = -angle;
  }

  normalize_v3_v3(mousevec, vod->rv3d->viewinv[2]);
  negate_v3(mousevec);
  view_roll_angle(vod->region, quat_new, vod->rv3d->viewquat, mousevec, angle, true);

  V3D_SmoothParams sview_params = {};
  sview_params.quat = quat_new;
  /* Group as successive roll may run by holding a key. */
  sview_params.undo_str = op->type->name;
  sview_params.undo_grouped = true;

  if (vod->use_dyn_ofs) {
    sview_params.dyn_ofs = vod->dyn_ofs;
  }

  ED_view3d_smooth_view(C, vod->v3d, vod->region, smooth_viewtx, &sview_params);

  viewops_data_free(C, vod);
  op->customdata = nullptr;
  return OPERATOR_FINISHED;
}

static int viewroll_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod;

  bool use_angle = RNA_enum_get(op->ptr, "type") != 0;

  if (use_angle || RNA_struct_property_is_set(op->ptr, "angle")) {
    viewroll_exec(C, op);
  }
  else {
    /* makes op->customdata */
    vod = viewops_data_create(C, event, &ViewOpsType_roll, false);
    const float start_position[2] = {float(BLI_rcti_cent_x(&vod->region->winrct)),
                                     float(BLI_rcti_cent_y(&vod->region->winrct))};
    vod->init.dial = BLI_dial_init(start_position, FLT_EPSILON);
    op->customdata = vod;

    ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->region);

    /* overwrite the mouse vector with the view direction */
    normalize_v3_v3(vod->init.mousevec, vod->rv3d->viewinv[2]);
    negate_v3(vod->init.mousevec);

    if (event->type == MOUSEROTATE) {
      vod->init.event_xy[0] = vod->prev.event_xy[0] = event->xy[0];
      viewroll_apply(vod, event->prev_xy[0], event->prev_xy[1]);

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

void VIEW3D_OT_view_roll(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "View Roll";
  ot->description = "Roll the view";
  ot->idname = ViewOpsType_roll.idname;

  /* api callbacks */
  ot->invoke = viewroll_invoke;
  ot->exec = viewroll_exec;
  ot->modal = viewroll_modal;
  ot->poll = ED_operator_rv3d_user_region_poll;
  ot->cancel = view3d_navigate_cancel_fn;

  /* flags */
  ot->flag = 0;

  /* properties */
  ot->prop = prop = RNA_def_float(
      ot->srna, "angle", 0, -FLT_MAX, FLT_MAX, "Roll", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_enum(ot->srna,
                      "type",
                      prop_view_roll_items,
                      0,
                      "Roll Angle Source",
                      "How roll angle is calculated");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

const ViewOpsType ViewOpsType_roll = {
    /*flag*/ (VIEWOPS_FLAG_ORBIT_SELECT),
    /*idname*/ "VIEW3D_OT_view_roll",
    /*poll_fn*/ nullptr,
    /*init_fn*/ nullptr,
    /*apply_fn*/ nullptr,
};
