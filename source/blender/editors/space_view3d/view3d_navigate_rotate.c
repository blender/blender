/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BLI_math.h"

#include "BKE_context.h"

#include "WM_api.h"

#include "RNA_access.h"

#include "ED_screen.h"

#include "view3d_intern.h"
#include "view3d_navigate.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Rotate Operator
 * \{ */

void viewrotate_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {VIEW_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},

      {VIEWROT_MODAL_AXIS_SNAP_ENABLE, "AXIS_SNAP_ENABLE", 0, "Axis Snap", ""},
      {VIEWROT_MODAL_AXIS_SNAP_DISABLE, "AXIS_SNAP_DISABLE", 0, "Axis Snap (Off)", ""},

      {VIEWROT_MODAL_SWITCH_ZOOM, "SWITCH_TO_ZOOM", 0, "Switch to Zoom"},
      {VIEWROT_MODAL_SWITCH_MOVE, "SWITCH_TO_MOVE", 0, "Switch to Move"},

      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "View3D Rotate Modal");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "View3D Rotate Modal", modal_items);

  /* disabled mode switching for now, can re-implement better, later on */
#if 0
  WM_modalkeymap_add_item(keymap,
                          &(const KeyMapItem_Params){
                              .type = LEFTMOUSE,
                              .value = KM_PRESS,
                              .modifier = KM_ANY,
                              .direction = KM_ANY,
                          },
                          VIEWROT_MODAL_SWITCH_ZOOM);
  WM_modalkeymap_add_item(keymap,
                          &(const KeyMapItem_Params){
                              .type = EVT_LEFTCTRLKEY,
                              .value = KM_PRESS,
                              .modifier = KM_ANY,
                              .direction = KM_ANY,
                          },
                          VIEWROT_MODAL_SWITCH_ZOOM);
  WM_modalkeymap_add_item(keymap,
                          &(const KeyMapItem_Params){
                              .type = EVT_LEFTSHIFTKEY,
                              .value = KM_PRESS,
                              .modifier = KM_ANY,
                              .direction = KM_ANY,
                          },
                          VIEWROT_MODAL_SWITCH_MOVE);
#endif

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_rotate");
}

static void viewrotate_apply_snap(ViewOpsData *vod)
{
  const float axis_limit = DEG2RADF(45 / 3);

  RegionView3D *rv3d = vod->rv3d;

  float viewquat_inv[4];
  float zaxis[3] = {0, 0, 1};
  float zaxis_best[3];
  int x, y, z;
  bool found = false;

  invert_qt_qt_normalized(viewquat_inv, vod->curr.viewquat);

  mul_qt_v3(viewquat_inv, zaxis);
  normalize_v3(zaxis);

  for (x = -1; x < 2; x++) {
    for (y = -1; y < 2; y++) {
      for (z = -1; z < 2; z++) {
        if (x || y || z) {
          float zaxis_test[3] = {x, y, z};

          normalize_v3(zaxis_test);

          if (angle_normalized_v3v3(zaxis_test, zaxis) < axis_limit) {
            copy_v3_v3(zaxis_best, zaxis_test);
            found = true;
          }
        }
      }
    }
  }

  if (found) {

    /* find the best roll */
    float quat_roll[4], quat_final[4], quat_best[4], quat_snap[4];
    float viewquat_align[4];     /* viewquat aligned to zaxis_best */
    float viewquat_align_inv[4]; /* viewquat aligned to zaxis_best */
    float best_angle = axis_limit;
    int j;

    /* viewquat_align is the original viewquat aligned to the snapped axis
     * for testing roll */
    rotation_between_vecs_to_quat(viewquat_align, zaxis_best, zaxis);
    normalize_qt(viewquat_align);
    mul_qt_qtqt(viewquat_align, vod->curr.viewquat, viewquat_align);
    normalize_qt(viewquat_align);
    invert_qt_qt_normalized(viewquat_align_inv, viewquat_align);

    vec_to_quat(quat_snap, zaxis_best, OB_NEGZ, OB_POSY);
    normalize_qt(quat_snap);
    invert_qt_normalized(quat_snap);

    /* check if we can find the roll */
    found = false;

    /* find best roll */
    for (j = 0; j < 8; j++) {
      float angle;
      float xaxis1[3] = {1, 0, 0};
      float xaxis2[3] = {1, 0, 0};
      float quat_final_inv[4];

      axis_angle_to_quat(quat_roll, zaxis_best, (float)j * DEG2RADF(45.0f));
      normalize_qt(quat_roll);

      mul_qt_qtqt(quat_final, quat_snap, quat_roll);
      normalize_qt(quat_final);

      /* compare 2 vector angles to find the least roll */
      invert_qt_qt_normalized(quat_final_inv, quat_final);
      mul_qt_v3(viewquat_align_inv, xaxis1);
      mul_qt_v3(quat_final_inv, xaxis2);
      angle = angle_v3v3(xaxis1, xaxis2);

      if (angle <= best_angle) {
        found = true;
        best_angle = angle;
        copy_qt_qt(quat_best, quat_final);
      }
    }

    if (found) {
      /* lock 'quat_best' to an axis view if we can */
      ED_view3d_quat_to_axis_view(quat_best, 0.01f, &rv3d->view, &rv3d->view_axis_roll);
      if (rv3d->view != RV3D_VIEW_USER) {
        ED_view3d_quat_from_axis_view(rv3d->view, rv3d->view_axis_roll, quat_best);
      }
    }
    else {
      copy_qt_qt(quat_best, viewquat_align);
    }

    copy_qt_qt(rv3d->viewquat, quat_best);

    viewrotate_apply_dyn_ofs(vod, rv3d->viewquat);

    if (U.uiflag & USER_AUTOPERSP) {
      if (RV3D_VIEW_IS_AXIS(rv3d->view)) {
        if (rv3d->persp == RV3D_PERSP) {
          rv3d->persp = RV3D_ORTHO;
        }
      }
    }
  }
  else if (U.uiflag & USER_AUTOPERSP) {
    rv3d->persp = vod->init.persp;
  }
}

static void viewrotate_apply(ViewOpsData *vod, const int event_xy[2])
{
  RegionView3D *rv3d = vod->rv3d;

  rv3d->view = RV3D_VIEW_USER; /* need to reset every time because of view snapping */

  if (U.flag & USER_TRACKBALL) {
    float axis[3], q1[4], dvec[3], newvec[3];
    float angle;

    {
      const int event_xy_offset[2] = {
          event_xy[0] + vod->init.event_xy_offset[0],
          event_xy[1] + vod->init.event_xy_offset[1],
      };
      calctrackballvec(&vod->region->winrct, event_xy_offset, newvec);
    }

    sub_v3_v3v3(dvec, newvec, vod->init.trackvec);

    angle = (len_v3(dvec) / (2.0f * V3D_OP_TRACKBALLSIZE)) * (float)M_PI;

    /* Before applying the sensitivity this is rotating 1:1,
     * where the cursor would match the surface of a sphere in the view. */
    angle *= U.view_rotate_sensitivity_trackball;

    /* Allow for rotation beyond the interval [-pi, pi] */
    angle = angle_wrap_rad(angle);

    /* This relation is used instead of the actual angle between vectors
     * so that the angle of rotation is linearly proportional to
     * the distance that the mouse is dragged. */

    cross_v3_v3v3(axis, vod->init.trackvec, newvec);
    axis_angle_to_quat(q1, axis, angle);

    mul_qt_qtqt(vod->curr.viewquat, q1, vod->init.quat);

    viewrotate_apply_dyn_ofs(vod, vod->curr.viewquat);
  }
  else {
    float quat_local_x[4], quat_global_z[4];
    float m[3][3];
    float m_inv[3][3];
    const float zvec_global[3] = {0.0f, 0.0f, 1.0f};
    float xaxis[3];

    /* Radians per-pixel. */
    const float sensitivity = U.view_rotate_sensitivity_turntable / U.dpi_fac;

    /* Get the 3x3 matrix and its inverse from the quaternion */
    quat_to_mat3(m, vod->curr.viewquat);
    invert_m3_m3(m_inv, m);

    /* Avoid Gimbal Lock
     *
     * Even though turn-table mode is in use, this can occur when the user exits the camera view
     * or when aligning the view to a rotated object.
     *
     * We have gimbal lock when the user's view is rotated +/- 90 degrees along the view axis.
     * In this case the vertical rotation is the same as the sideways turntable motion.
     * Making it impossible to get out of the gimbal locked state without resetting the view.
     *
     * The logic below lets the user exit out of this state without any abrupt 'fix'
     * which would be disorienting.
     *
     * This works by blending two horizons:
     * - Rotated-horizon: `cross_v3_v3v3(xaxis, zvec_global, m_inv[2])`
     *   When only this is used, this turntable rotation works - but it's side-ways
     *   (as if the entire turn-table has been placed on its side)
     *   While there is no gimbal lock, it's also awkward to use.
     * - Un-rotated-horizon: `m_inv[0]`
     *   When only this is used, the turntable rotation can have gimbal lock.
     *
     * The solution used here is to blend between these two values,
     * so the severity of the gimbal lock is used to blend the rotated horizon.
     * Blending isn't essential, it just makes the transition smoother.
     *
     * This allows sideways turn-table rotation on a Z axis that isn't world-space Z,
     * While up-down turntable rotation eventually corrects gimbal lock. */
#if 1
    if (len_squared_v3v3(zvec_global, m_inv[2]) > 0.001f) {
      float fac;
      cross_v3_v3v3(xaxis, zvec_global, m_inv[2]);
      if (dot_v3v3(xaxis, m_inv[0]) < 0) {
        negate_v3(xaxis);
      }
      fac = angle_normalized_v3v3(zvec_global, m_inv[2]) / (float)M_PI;
      fac = fabsf(fac - 0.5f) * 2;
      fac = fac * fac;
      interp_v3_v3v3(xaxis, xaxis, m_inv[0], fac);
    }
    else {
      copy_v3_v3(xaxis, m_inv[0]);
    }
#else
    copy_v3_v3(xaxis, m_inv[0]);
#endif

    /* Determine the direction of the x vector (for rotating up and down) */
    /* This can likely be computed directly from the quaternion. */

    /* Perform the up/down rotation */
    axis_angle_to_quat(quat_local_x, xaxis, sensitivity * -(event_xy[1] - vod->prev.event_xy[1]));
    mul_qt_qtqt(quat_local_x, vod->curr.viewquat, quat_local_x);

    /* Perform the orbital rotation */
    axis_angle_to_quat_single(
        quat_global_z, 'Z', sensitivity * vod->reverse * (event_xy[0] - vod->prev.event_xy[0]));
    mul_qt_qtqt(vod->curr.viewquat, quat_local_x, quat_global_z);

    viewrotate_apply_dyn_ofs(vod, vod->curr.viewquat);
  }

  /* avoid precision loss over time */
  normalize_qt(vod->curr.viewquat);

  /* use a working copy so view rotation locking doesn't overwrite the locked
   * rotation back into the view we calculate with */
  copy_qt_qt(rv3d->viewquat, vod->curr.viewquat);

  /* Check for view snap,
   * NOTE: don't apply snap to `vod->viewquat` so the view won't jam up. */
  if (vod->axis_snap) {
    viewrotate_apply_snap(vod);
  }
  vod->prev.event_xy[0] = event_xy[0];
  vod->prev.event_xy[1] = event_xy[1];

  ED_view3d_camera_lock_sync(vod->depsgraph, vod->v3d, rv3d);

  ED_region_tag_redraw(vod->region);
}

static int viewrotate_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod = op->customdata;
  short event_code = VIEW_PASS;
  bool use_autokey = false;
  int ret = OPERATOR_RUNNING_MODAL;

  /* execute the events */
  if (event->type == MOUSEMOVE) {
    event_code = VIEW_APPLY;
  }
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case VIEW_MODAL_CONFIRM:
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_AXIS_SNAP_ENABLE:
        vod->axis_snap = true;
        event_code = VIEW_APPLY;
        break;
      case VIEWROT_MODAL_AXIS_SNAP_DISABLE:
        vod->rv3d->persp = vod->init.persp;
        vod->axis_snap = false;
        event_code = VIEW_APPLY;
        break;
      case VIEWROT_MODAL_SWITCH_ZOOM:
        WM_operator_name_call(C, "VIEW3D_OT_zoom", WM_OP_INVOKE_DEFAULT, NULL, event);
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_MOVE:
        WM_operator_name_call(C, "VIEW3D_OT_move", WM_OP_INVOKE_DEFAULT, NULL, event);
        event_code = VIEW_CONFIRM;
        break;
    }
  }
  else if (event->type == vod->init.event_type && event->val == KM_RELEASE) {
    event_code = VIEW_CONFIRM;
  }

  if (event_code == VIEW_APPLY) {
    viewrotate_apply(vod, event->xy);
    if (ED_screen_animation_playing(CTX_wm_manager(C))) {
      use_autokey = true;
    }
  }
  else if (event_code == VIEW_CONFIRM) {
    use_autokey = true;
    ret = OPERATOR_FINISHED;
  }

  if (use_autokey) {
    ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, true, true);
  }

  if (ret & OPERATOR_FINISHED) {
    viewops_data_free(C, op->customdata);
    op->customdata = NULL;
  }

  return ret;
}

static int viewrotate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod;

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  /* makes op->customdata */
  vod = op->customdata = viewops_data_create(
      C,
      event,
      viewops_flag_from_prefs() | VIEWOPS_FLAG_PERSP_ENSURE |
          (use_cursor_init ? VIEWOPS_FLAG_USE_MOUSE_INIT : 0));

  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->region);

  if (ELEM(event->type, MOUSEPAN, MOUSEROTATE)) {
    /* Rotate direction we keep always same */
    int event_xy[2];

    if (event->type == MOUSEPAN) {
      if (event->flag & WM_EVENT_SCROLL_INVERT) {
        event_xy[0] = 2 * event->xy[0] - event->prev_xy[0];
        event_xy[1] = 2 * event->xy[1] - event->prev_xy[1];
      }
      else {
        copy_v2_v2_int(event_xy, event->prev_xy);
      }
    }
    else {
      /* MOUSEROTATE performs orbital rotation, so y axis delta is set to 0 */
      copy_v2_v2_int(event_xy, event->prev_xy);
    }

    viewrotate_apply(vod, event_xy);

    viewops_data_free(C, op->customdata);
    op->customdata = NULL;

    return OPERATOR_FINISHED;
  }

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void viewrotate_cancel(bContext *C, wmOperator *op)
{
  viewops_data_free(C, op->customdata);
  op->customdata = NULL;
}

void VIEW3D_OT_rotate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rotate View";
  ot->description = "Rotate the view";
  ot->idname = "VIEW3D_OT_rotate";

  /* api callbacks */
  ot->invoke = viewrotate_invoke;
  ot->modal = viewrotate_modal;
  ot->poll = view3d_rotation_poll;
  ot->cancel = viewrotate_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  view3d_operator_properties_common(ot, V3D_OP_PROP_USE_MOUSE_INIT);
}

/** \} */
