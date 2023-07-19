/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "BLT_translation.h"

#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_vfont.h"

#include "DEG_depsgraph_query.h"

#include "ED_mesh.h"
#include "ED_particle.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "WM_api.h"
#include "WM_message.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_resources.h"

#include "view3d_intern.h"

#include "view3d_navigate.hh" /* own include */

static eViewOpsFlag viewops_flag_from_prefs()
{
  const bool use_select = (U.uiflag & USER_ORBIT_SELECTION) != 0;
  const bool use_depth = (U.uiflag & USER_DEPTH_NAVIGATE) != 0;
  const bool use_zoom_to_mouse = (U.uiflag & USER_ZOOM_TO_MOUSEPOS) != 0;
  const bool use_auto_persp = (U.uiflag & USER_AUTOPERSP) != 0;

  enum eViewOpsFlag flag = VIEWOPS_FLAG_NONE;
  if (use_select) {
    flag |= VIEWOPS_FLAG_ORBIT_SELECT;
  }
  if (use_depth) {
    flag |= VIEWOPS_FLAG_DEPTH_NAVIGATE;
  }
  if (use_zoom_to_mouse) {
    flag |= VIEWOPS_FLAG_ZOOM_TO_MOUSE;
  }
  if (use_auto_persp) {
    flag |= VIEWOPS_FLAG_PERSP_ENSURE;
  }

  return flag;
}

/* -------------------------------------------------------------------- */
/** \name ViewOpsData definition
 * \{ */

void ViewOpsData::init_context(bContext *C)
{
  /* Store data. */
  this->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  this->scene = CTX_data_scene(C);
  this->area = CTX_wm_area(C);
  this->region = CTX_wm_region(C);
  this->v3d = static_cast<View3D *>(this->area->spacedata.first);
  this->rv3d = static_cast<RegionView3D *>(this->region->regiondata);
}

void ViewOpsData::state_backup()
{
  copy_v3_v3(this->init.ofs, rv3d->ofs);
  copy_v3_v3(this->init.ofs_lock, rv3d->ofs_lock);
  this->init.camdx = rv3d->camdx;
  this->init.camdy = rv3d->camdy;
  this->init.camzoom = rv3d->camzoom;
  this->init.dist = rv3d->dist;
  copy_qt_qt(this->init.quat, rv3d->viewquat);

  this->init.persp = rv3d->persp;
  this->init.view = rv3d->view;
  this->init.view_axis_roll = rv3d->view_axis_roll;
}

void ViewOpsData::state_restore()
{
  /* DOLLY, MOVE, ROTATE and ZOOM. */
  {
    /* For Move this only changes when offset is not locked. */
    /* For Rotate this only changes when rotating around objects or last-brush. */
    /* For Zoom this only changes when zooming to mouse position. */
    /* Note this does not remove auto-keys on locked cameras. */
    copy_v3_v3(this->rv3d->ofs, this->init.ofs);
  }

  /* MOVE and ZOOM. */
  {
    /* For Move this only changes when offset is not locked. */
    /* For Zoom this only changes when zooming to mouse position in camera view. */
    this->rv3d->camdx = this->init.camdx;
    this->rv3d->camdy = this->init.camdy;
  }

  /* MOVE. */
  {
    if ((this->rv3d->persp == RV3D_CAMOB) && !ED_view3d_camera_lock_check(this->v3d, this->rv3d)) {
      // this->rv3d->camdx = this->init.camdx;
      // this->rv3d->camdy = this->init.camdy;
    }
    else if (ED_view3d_offset_lock_check(this->v3d, this->rv3d)) {
      copy_v2_v2(this->rv3d->ofs_lock, this->init.ofs_lock);
    }
    else {
      // copy_v3_v3(vod->rv3d->ofs, vod->init.ofs);
      if (RV3D_LOCK_FLAGS(this->rv3d) & RV3D_BOXVIEW) {
        view3d_boxview_sync(this->area, this->region);
      }
    }
  }

  /* ZOOM. */
  {
    this->rv3d->camzoom = this->init.camzoom;
  }

  /* ROTATE and ZOOM. */
  {
    /**
     * For Rotate this only changes when orbiting from a camera view.
     * In this case the `dist` is calculated based on the camera relative to the `ofs`.
     */
    /* Note this does not remove auto-keys on locked cameras. */
    this->rv3d->dist = this->init.dist;
  }

  /* ROLL and ROTATE. */
  {
    /* Note this does not remove auto-keys on locked cameras. */
    copy_qt_qt(this->rv3d->viewquat, this->init.quat);
  }

  /* ROTATE. */
  {
    this->rv3d->persp = this->init.persp;
    this->rv3d->view = this->init.view;
    this->rv3d->view_axis_roll = this->init.view_axis_roll;
  }

  /* NOTE: there is no need to restore "last" values (as set by #ED_view3d_lastview_store). */

  ED_view3d_camera_lock_sync(this->depsgraph, this->v3d, this->rv3d);
}

static eViewOpsFlag navigate_pivot_get(bContext *C,
                                       Depsgraph *depsgraph,
                                       ARegion *region,
                                       View3D *v3d,
                                       const wmEvent *event,
                                       eViewOpsFlag viewops_flag,
                                       float r_pivot[3])
{
  if ((viewops_flag & VIEWOPS_FLAG_ORBIT_SELECT) && view3d_orbit_calc_center(C, r_pivot)) {
    return VIEWOPS_FLAG_ORBIT_SELECT;
  }

  wmWindow *win = CTX_wm_window(C);

  if (!(viewops_flag & VIEWOPS_FLAG_DEPTH_NAVIGATE)) {
    ED_view3d_autodist_last_clear(win);

    /* Uses the `lastofs` in #view3d_orbit_calc_center. */
    BLI_assert(viewops_flag & VIEWOPS_FLAG_ORBIT_SELECT);
    return VIEWOPS_FLAG_ORBIT_SELECT;
  }

  const bool use_depth_last = ED_view3d_autodist_last_check(win, event);

  if (use_depth_last) {
    ED_view3d_autodist_last_get(win, r_pivot);
  }
  else {
    float fallback_depth_pt[3];
    negate_v3_v3(fallback_depth_pt, static_cast<RegionView3D *>(region->regiondata)->ofs);

    const bool is_set = ED_view3d_autodist(
        depsgraph, region, v3d, event->mval, r_pivot, true, fallback_depth_pt);

    ED_view3d_autodist_last_set(win, event, r_pivot, is_set);
  }

  return VIEWOPS_FLAG_DEPTH_NAVIGATE;
}

void ViewOpsData::init_navigation(bContext *C,
                                  const wmEvent *event,
                                  const ViewOpsType *nav_type,
                                  const bool use_cursor_init)
{
  this->nav_type = nav_type;
  eViewOpsFlag viewops_flag = nav_type->flag & viewops_flag_from_prefs();

  if (!use_cursor_init) {
    viewops_flag &= ~(VIEWOPS_FLAG_USE_MOUSE_INIT | VIEWOPS_FLAG_DEPTH_NAVIGATE |
                      VIEWOPS_FLAG_ZOOM_TO_MOUSE);
  }

  bool calc_rv3d_dist = true;
#ifdef WITH_INPUT_NDOF
  if (ELEM(nav_type,
           &ViewOpsType_ndof_orbit,
           &ViewOpsType_ndof_orbit_zoom,
           &ViewOpsType_ndof_pan,
           &ViewOpsType_ndof_all))
  {
    calc_rv3d_dist = false;
  }
#endif

  /* Set the view from the camera, if view locking is enabled.
   * we may want to make this optional but for now its needed always. */
  ED_view3d_camera_lock_init_ex(depsgraph, v3d, rv3d, calc_rv3d_dist);

  this->state_backup();

  if (viewops_flag & VIEWOPS_FLAG_PERSP_ENSURE) {
    if (ED_view3d_persp_ensure(depsgraph, this->v3d, this->region)) {
      /* If we're switching from camera view to the perspective one,
       * need to tag viewport update, so camera view and borders are properly updated. */
      ED_region_tag_redraw(this->region);
    }
  }

  if (viewops_flag & (VIEWOPS_FLAG_DEPTH_NAVIGATE | VIEWOPS_FLAG_ORBIT_SELECT)) {
    float pivot_new[3];
    eViewOpsFlag pivot_type = navigate_pivot_get(
        C, depsgraph, region, v3d, event, viewops_flag, pivot_new);
    viewops_flag &= ~(VIEWOPS_FLAG_DEPTH_NAVIGATE | VIEWOPS_FLAG_ORBIT_SELECT);
    viewops_flag |= pivot_type;

    negate_v3_v3(this->dyn_ofs, pivot_new);
    this->use_dyn_ofs = true;

    if (!(nav_type->flag & VIEWOPS_FLAG_ORBIT_SELECT)) {
      /* Calculate new #RegionView3D::ofs and #RegionView3D::dist. */

      if (rv3d->is_persp) {
        float my_origin[3]; /* Original #RegionView3D.ofs. */
        float my_pivot[3];  /* View pivot. */
        float dvec[3];

        /* locals for dist correction */
        float mat[3][3];
        float upvec[3];

        negate_v3_v3(my_origin, rv3d->ofs); /* ofs is flipped */

        /* Set the dist value to be the distance from this 3d point this means you'll
         * always be able to zoom into it and panning won't go bad when dist was zero. */

        /* remove dist value */
        upvec[0] = upvec[1] = 0;
        upvec[2] = rv3d->dist;
        copy_m3_m4(mat, rv3d->viewinv);

        mul_m3_v3(mat, upvec);
        add_v3_v3v3(my_pivot, my_origin, upvec);

        /* find a new ofs value that is along the view axis
         * (rather than the mouse location) */
        closest_to_line_v3(dvec, pivot_new, my_pivot, my_origin);

        negate_v3_v3(rv3d->ofs, dvec);
        rv3d->dist = len_v3v3(my_pivot, dvec);
      }
      else {
        const float mval_region_mid[2] = {float(region->winx) / 2.0f, float(region->winy) / 2.0f};
        ED_view3d_win_to_3d(v3d, region, pivot_new, mval_region_mid, rv3d->ofs);
        negate_v3(rv3d->ofs);
      }

      /* XXX: The initial state captured by #ViewOpsData::state_backup is being modified here.
       * This causes the state when canceling a navigation operation to not be fully restored. */
      this->init.dist = rv3d->dist;
      copy_v3_v3(this->init.ofs, rv3d->ofs);
    }
  }

  this->init.persp_with_auto_persp_applied = rv3d->persp;
  if (event) {
    this->init.event_type = event->type;
    copy_v2_v2_int(this->init.event_xy, event->xy);
    copy_v2_v2_int(this->prev.event_xy, event->xy);

    if (viewops_flag & VIEWOPS_FLAG_USE_MOUSE_INIT) {
      zero_v2_int(this->init.event_xy_offset);
    }
    else {
      /* Simulate the event starting in the middle of the region. */
      this->init.event_xy_offset[0] = BLI_rcti_cent_x(&this->region->winrct) - event->xy[0];
      this->init.event_xy_offset[1] = BLI_rcti_cent_y(&this->region->winrct) - event->xy[1];
    }

    /* For dolly */
    const float mval[2] = {float(event->mval[0]), float(event->mval[1])};
    ED_view3d_win_to_vector(region, mval, this->init.mousevec);

    {
      int event_xy_offset[2];
      add_v2_v2v2_int(event_xy_offset, event->xy, this->init.event_xy_offset);

      /* For rotation with trackball rotation. */
      calctrackballvec(&region->winrct, event_xy_offset, this->init.trackvec);
    }
  }

  {
    float tvec[3];
    negate_v3_v3(tvec, rv3d->ofs);
    this->init.zfac = ED_view3d_calc_zfac(rv3d, tvec);
  }

  copy_qt_qt(this->curr.viewquat, rv3d->viewquat);

  this->reverse = 1.0f;
  if (rv3d->persmat[2][1] < 0.0f) {
    this->reverse = -1.0f;
  }

  this->viewops_flag = viewops_flag;

  /* Default. */
  this->use_dyn_ofs_ortho_correction = false;

  rv3d->rflag |= RV3D_NAVIGATING;
}

void ViewOpsData::end_navigation(bContext *C)
{
  this->rv3d->rflag &= ~RV3D_NAVIGATING;

  if (this->timer) {
    WM_event_timer_remove(CTX_wm_manager(C), this->timer->win, this->timer);
  }

  MEM_SAFE_FREE(this->init.dial);

  /* Need to redraw because drawing code uses RV3D_NAVIGATING to draw
   * faster while navigation operator runs. */
  ED_region_tag_redraw(this->region);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Operator Callback Utils
 * \{ */

static bool view3d_navigation_poll_impl(bContext *C, const char viewlock)
{
  if (!ED_operator_region_view3d_active(C)) {
    return false;
  }

  const RegionView3D *rv3d = CTX_wm_region_view3d(C);
  return !(RV3D_LOCK_FLAGS(rv3d) & viewlock);
}

static eV3D_OpEvent view3d_navigate_event(ViewOpsData *vod, const wmEvent *event)
{
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case VIEW_MODAL_CANCEL:
        return VIEW_CANCEL;
      case VIEW_MODAL_CONFIRM:
        return VIEW_CONFIRM;
      case VIEWROT_MODAL_AXIS_SNAP_ENABLE:
        vod->axis_snap = true;
        return VIEW_APPLY;
      case VIEWROT_MODAL_AXIS_SNAP_DISABLE:
        vod->rv3d->persp = vod->init.persp_with_auto_persp_applied;
        vod->axis_snap = false;
        return VIEW_APPLY;
      case VIEWROT_MODAL_SWITCH_ZOOM:
      case VIEWROT_MODAL_SWITCH_MOVE:
      case VIEWROT_MODAL_SWITCH_ROTATE: {
        const ViewOpsType *nav_type_new = (event->val == VIEWROT_MODAL_SWITCH_ZOOM) ?
                                              &ViewOpsType_zoom :
                                          (event->val == VIEWROT_MODAL_SWITCH_MOVE) ?
                                              &ViewOpsType_move :
                                              &ViewOpsType_rotate;
        if (nav_type_new == vod->nav_type) {
          break;
        }
        vod->nav_type = nav_type_new;
        return VIEW_APPLY;
      }
    }
  }
  else {
    if (event->type == TIMER && event->customdata == vod->timer) {
      /* Zoom uses timer for continuous zoom. */
      return VIEW_APPLY;
    }
    if (event->type == MOUSEMOVE) {
      return VIEW_APPLY;
    }
    if (event->type == vod->init.event_type && event->val == KM_RELEASE) {
      return VIEW_CONFIRM;
    }
    if (event->type == EVT_ESCKEY && event->val == KM_PRESS) {
      return VIEW_CANCEL;
    }
  }

  return VIEW_PASS;
}

static int view3d_navigation_invoke_generic(bContext *C,
                                            ViewOpsData *vod,
                                            const wmEvent *event,
                                            PointerRNA *ptr,
                                            const ViewOpsType *nav_type)
{
  if (!nav_type->init_fn) {
    return OPERATOR_CANCELLED;
  }

  bool use_cursor_init = false;
  if (PropertyRNA *prop = RNA_struct_find_property(ptr, "use_cursor_init")) {
    use_cursor_init = RNA_property_boolean_get(ptr, prop);
  }

  vod->init_navigation(C, event, nav_type, use_cursor_init);
  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->region);

  return nav_type->init_fn(C, vod, event, ptr);
}

int view3d_navigate_invoke_impl(bContext *C,
                                wmOperator *op,
                                const wmEvent *event,
                                const ViewOpsType *nav_type)
{
  ViewOpsData *vod = new ViewOpsData();
  vod->init_context(C);
  int ret = view3d_navigation_invoke_generic(C, vod, event, op->ptr, nav_type);
  op->customdata = (void *)vod;

  if (ret == OPERATOR_RUNNING_MODAL) {
    WM_event_add_modal_handler(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  viewops_data_free(C, vod);
  op->customdata = nullptr;
  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Callbacks
 * \{ */

bool view3d_location_poll(bContext *C)
{
  return view3d_navigation_poll_impl(C, RV3D_LOCK_LOCATION);
}

bool view3d_rotation_poll(bContext *C)
{
  return view3d_navigation_poll_impl(C, RV3D_LOCK_ROTATION);
}

bool view3d_zoom_or_dolly_poll(bContext *C)
{
  return view3d_navigation_poll_impl(C, RV3D_LOCK_ZOOM_AND_DOLLY);
}

int view3d_navigate_modal_fn(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod = static_cast<ViewOpsData *>(op->customdata);

  const ViewOpsType *nav_type_prev = vod->nav_type;
  const eV3D_OpEvent event_code = view3d_navigate_event(vod, event);
  if (nav_type_prev != vod->nav_type) {
    wmOperatorType *ot_new = WM_operatortype_find(vod->nav_type->idname, false);
    WM_operator_type_set(op, ot_new);
    vod->end_navigation(C);
    return view3d_navigation_invoke_generic(C, vod, event, op->ptr, vod->nav_type);
  }

  int ret = vod->nav_type->apply_fn(C, vod, event_code, event->xy);

  if ((ret & OPERATOR_RUNNING_MODAL) == 0) {
    if (ret & OPERATOR_FINISHED) {
      ED_view3d_camera_lock_undo_push(op->type->name, vod->v3d, vod->rv3d, C);
    }
    viewops_data_free(C, vod);
    op->customdata = nullptr;
  }

  return ret;
}

void view3d_navigate_cancel_fn(bContext *C, wmOperator *op)
{
  viewops_data_free(C, static_cast<ViewOpsData *>(op->customdata));
  op->customdata = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic View Operator Properties
 * \{ */

void view3d_operator_properties_common(wmOperatorType *ot, const enum eV3D_OpPropFlag flag)
{
  if (flag & V3D_OP_PROP_MOUSE_CO) {
    PropertyRNA *prop;
    prop = RNA_def_int(ot->srna, "mx", 0, 0, INT_MAX, "Region Position X", "", 0, INT_MAX);
    RNA_def_property_flag(prop, PROP_HIDDEN);
    prop = RNA_def_int(ot->srna, "my", 0, 0, INT_MAX, "Region Position Y", "", 0, INT_MAX);
    RNA_def_property_flag(prop, PROP_HIDDEN);
  }
  if (flag & V3D_OP_PROP_DELTA) {
    RNA_def_int(ot->srna, "delta", 0, INT_MIN, INT_MAX, "Delta", "", INT_MIN, INT_MAX);
  }
  if (flag & V3D_OP_PROP_USE_ALL_REGIONS) {
    PropertyRNA *prop;
    prop = RNA_def_boolean(
        ot->srna, "use_all_regions", false, "All Regions", "View selected for all regions");
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }
  if (flag & V3D_OP_PROP_USE_MOUSE_INIT) {
    WM_operator_properties_use_cursor_init(ot);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic View Operator Custom-Data
 * \{ */

void calctrackballvec(const rcti *rect, const int event_xy[2], float r_dir[3])
{
  const float radius = V3D_OP_TRACKBALLSIZE;
  const float t = radius / float(M_SQRT2);
  const float size[2] = {float(BLI_rcti_size_x(rect)), float(BLI_rcti_size_y(rect))};
  /* Aspect correct so dragging in a non-square view doesn't squash the direction.
   * So diagonal motion rotates the same direction the cursor is moving. */
  const float size_min = min_ff(size[0], size[1]);
  const float aspect[2] = {size_min / size[0], size_min / size[1]};

  /* Normalize x and y. */
  r_dir[0] = (event_xy[0] - BLI_rcti_cent_x(rect)) / ((size[0] * aspect[0]) / 2.0);
  r_dir[1] = (event_xy[1] - BLI_rcti_cent_y(rect)) / ((size[1] * aspect[1]) / 2.0);
  const float d = len_v2(r_dir);
  if (d < t) {
    /* Inside sphere. */
    r_dir[2] = sqrtf(square_f(radius) - square_f(d));
  }
  else {
    /* On hyperbola. */
    r_dir[2] = square_f(t) / d;
  }
}

void view3d_orbit_apply_dyn_ofs(float r_ofs[3],
                                const float ofs_old[3],
                                const float viewquat_old[4],
                                const float viewquat_new[4],
                                const float dyn_ofs[3])
{
  float q[4];
  invert_qt_qt_normalized(q, viewquat_old);
  mul_qt_qtqt(q, q, viewquat_new);

  invert_qt_normalized(q);

  sub_v3_v3v3(r_ofs, ofs_old, dyn_ofs);
  mul_qt_v3(q, r_ofs);
  add_v3_v3(r_ofs, dyn_ofs);
}

static void view3d_orbit_apply_dyn_ofs_ortho_correction(float ofs[3],
                                                        const float viewquat_old[4],
                                                        const float viewquat_new[4],
                                                        const float dyn_ofs[3])
{
  /* NOTE(@ideasman42): While orbiting in orthographic mode the "depth" of the offset
   * (position along the views Z-axis) is only noticeable when the view contents is clipped.
   * The likelihood of clipping depends on the clipping range & size of the scene.
   * In practice some users might not run into this, however using dynamic-offset in
   * orthographic views can cause the depth of the offset to drift while navigating the view,
   * causing unexpected clipping that seems like a bug from the user perspective, see: #104385.
   *
   * Imagine a camera is focused on a distant object. Now imagine a closer object in front of
   * the camera is used as a pivot, the camera is rotated to view it from the side (~90d rotation).
   * The outcome is the camera is now focused on a distant region to the left/right.
   * The new focal point is unlikely to point to anything useful (unless by accident).
   * Instead of a focal point - the `rv3d->ofs` is being manipulated in this case.
   *
   * Resolve by moving #RegionView3D::ofs so it is depth-aligned to `dyn_ofs`,
   * this is interpolated by the amount of rotation so minor rotations don't cause
   * the view-clipping to suddenly jump.
   *
   * Perspective Views
   * =================
   *
   * This logic could also be applied to perspective views because the issue of the `ofs`
   * being a location which isn't useful exists there too, however the problem where this location
   * impacts the clipping does *not* exist, as the clipping range starts from the view-point
   * (`ofs` + `dist` along the view Z-axis) unlike orthographic views which center around `ofs`.
   * Nevertheless there will be cases when having `ofs` and a large `dist` pointing nowhere doesn't
   * give ideal behavior (zooming may jump in larger than expected steps and panning the view may
   * move too much in relation to nearby objects - for e.g.). So it's worth investigating but
   * should be done with extra care as changing `ofs` in perspective view also requires changing
   * the `dist` which could cause unexpected results if the calculated `dist` happens to be small.
   * So disable this workaround in perspective view unless there are clear benefits to enabling. */

  float q_inv[4];

  float view_z_init[3] = {0.0f, 0.0f, 1.0f};
  invert_qt_qt_normalized(q_inv, viewquat_old);
  mul_qt_v3(q_inv, view_z_init);

  float view_z_curr[3] = {0.0f, 0.0f, 1.0f};
  invert_qt_qt_normalized(q_inv, viewquat_new);
  mul_qt_v3(q_inv, view_z_curr);

  const float angle_cos = max_ff(0.0f, dot_v3v3(view_z_init, view_z_curr));
  /* 1.0 or more means no rotation, there is nothing to do in that case. */
  if (LIKELY(angle_cos < 1.0f)) {
    const float dot_ofs_curr = dot_v3v3(view_z_curr, ofs);
    const float dot_ofs_next = dot_v3v3(view_z_curr, dyn_ofs);
    const float ofs_delta = dot_ofs_next - dot_ofs_curr;
    if (LIKELY(ofs_delta != 0.0f)) {
      /* Calculate a factor where 0.0 represents no rotation and 1.0 represents 90d or more.
       * NOTE: Without applying the factor, the distances immediately changes
       * (useful for testing), but not good for the users experience as minor rotations
       * should not immediately adjust the depth. */
      const float factor = acosf(angle_cos) / M_PI_2;
      madd_v3_v3fl(ofs, view_z_curr, ofs_delta * factor);
    }
  }
}

void viewrotate_apply_dyn_ofs(ViewOpsData *vod, const float viewquat_new[4])
{
  if (vod->use_dyn_ofs) {
    RegionView3D *rv3d = vod->rv3d;
    view3d_orbit_apply_dyn_ofs(
        rv3d->ofs, vod->init.ofs, vod->init.quat, viewquat_new, vod->dyn_ofs);

    if (vod->use_dyn_ofs_ortho_correction) {
      view3d_orbit_apply_dyn_ofs_ortho_correction(
          rv3d->ofs, vod->init.quat, viewquat_new, vod->dyn_ofs);
    }
  }
}

bool view3d_orbit_calc_center(bContext *C, float r_dyn_ofs[3])
{
  static float lastofs[3] = {0, 0, 0};
  bool is_set = false;

  const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);
  View3D *v3d = CTX_wm_view3d(C);
  BKE_view_layer_synced_ensure(scene_eval, view_layer_eval);
  Object *ob_act_eval = BKE_view_layer_active_object_get(view_layer_eval);
  Object *ob_act = DEG_get_original_object(ob_act_eval);

  if (ob_act && (ob_act->mode & OB_MODE_ALL_PAINT) &&
      /* with weight-paint + pose-mode, fall through to using calculateTransformCenter */
      ((ob_act->mode & OB_MODE_WEIGHT_PAINT) && BKE_object_pose_armature_get(ob_act)) == 0)
  {
    BKE_paint_stroke_get_average(scene, ob_act_eval, lastofs);
    is_set = true;
  }
  else if (ob_act && (ob_act->mode & OB_MODE_EDIT) && (ob_act->type == OB_FONT)) {
    Curve *cu = static_cast<Curve *>(ob_act_eval->data);
    EditFont *ef = cu->editfont;

    zero_v3(lastofs);
    for (int i = 0; i < 4; i++) {
      add_v2_v2(lastofs, ef->textcurs[i]);
    }
    mul_v2_fl(lastofs, 1.0f / 4.0f);

    mul_m4_v3(ob_act_eval->object_to_world, lastofs);

    is_set = true;
  }
  else if (ob_act == nullptr || ob_act->mode == OB_MODE_OBJECT) {
    /* object mode use boundbox centers */
    uint tot = 0;
    float select_center[3];

    zero_v3(select_center);
    LISTBASE_FOREACH (Base *, base_eval, BKE_view_layer_object_bases_get(view_layer_eval)) {
      if (BASE_SELECTED(v3d, base_eval)) {
        /* use the boundbox if we can */
        Object *ob_eval = base_eval->object;

        if (ob_eval->runtime.bb && !(ob_eval->runtime.bb->flag & BOUNDBOX_DIRTY)) {
          float cent[3];

          BKE_boundbox_calc_center_aabb(ob_eval->runtime.bb, cent);

          mul_m4_v3(ob_eval->object_to_world, cent);
          add_v3_v3(select_center, cent);
        }
        else {
          add_v3_v3(select_center, ob_eval->object_to_world[3]);
        }
        tot++;
      }
    }
    if (tot) {
      mul_v3_fl(select_center, 1.0f / float(tot));
      copy_v3_v3(lastofs, select_center);
      is_set = true;
    }
  }
  else {
    /* If there's no selection, `lastofs` is unmodified and last value since static. */
    is_set = ED_transform_calc_pivot_pos(C, V3D_AROUND_CENTER_MEDIAN, lastofs);
  }

  copy_v3_v3(r_dyn_ofs, lastofs);

  return is_set;
}

ViewOpsData *viewops_data_create(bContext *C,
                                 const wmEvent *event,
                                 const ViewOpsType *nav_type,
                                 const bool use_cursor_init)
{
  ViewOpsData *vod = new ViewOpsData();
  vod->init_context(C);
  vod->init_navigation(C, event, nav_type, use_cursor_init);
  return vod;
}

void viewops_data_free(bContext *C, ViewOpsData *vod)
{
  if (!vod) {
    return;
  }
  vod->end_navigation(C);
  delete vod;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic View Operator Utilities
 * \{ */

/**
 * \param align_to_quat: When not nullptr, set the axis relative to this rotation.
 */
void axis_set_view(bContext *C,
                   View3D *v3d,
                   ARegion *region,
                   const float quat_[4],
                   char view,
                   char view_axis_roll,
                   int perspo,
                   const float *align_to_quat,
                   const int smooth_viewtx)
{
  /* no nullptr check is needed, poll checks */
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  float quat[4];
  const short orig_persp = rv3d->persp;

  normalize_qt_qt(quat, quat_);

  if (align_to_quat) {
    mul_qt_qtqt(quat, quat, align_to_quat);
    rv3d->view = view = RV3D_VIEW_USER;
    rv3d->view_axis_roll = RV3D_VIEW_AXIS_ROLL_0;
  }

  if (align_to_quat == nullptr) {
    rv3d->view = view;
    rv3d->view_axis_roll = view_axis_roll;
  }

  if (RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) {
    ED_region_tag_redraw(region);
    return;
  }

  if (U.uiflag & USER_AUTOPERSP) {
    rv3d->persp = RV3D_VIEW_IS_AXIS(view) ? RV3D_ORTHO : perspo;
  }
  else if (rv3d->persp == RV3D_CAMOB) {
    rv3d->persp = perspo;
  }

  if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
    /* to camera */
    V3D_SmoothParams sview = {nullptr};
    sview.camera_old = v3d->camera;
    sview.ofs = rv3d->ofs;
    sview.quat = quat;
    /* No undo because this switches to/from camera. */
    sview.undo_str = nullptr;

    ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);
  }
  else if (orig_persp == RV3D_CAMOB && v3d->camera) {
    /* from camera */
    float ofs[3], dist;

    copy_v3_v3(ofs, rv3d->ofs);
    dist = rv3d->dist;

    /* so we animate _from_ the camera location */
    Object *camera_eval = DEG_get_evaluated_object(CTX_data_ensure_evaluated_depsgraph(C),
                                                   v3d->camera);
    ED_view3d_from_object(camera_eval, rv3d->ofs, nullptr, &rv3d->dist, nullptr);

    V3D_SmoothParams sview = {nullptr};
    sview.camera_old = camera_eval;
    sview.ofs = ofs;
    sview.quat = quat;
    sview.dist = &dist;
    /* No undo because this switches to/from camera. */
    sview.undo_str = nullptr;

    ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);
  }
  else {
    /* rotate around selection */
    const float *dyn_ofs_pt = nullptr;
    float dyn_ofs[3];

    if (U.uiflag & USER_ORBIT_SELECTION) {
      if (view3d_orbit_calc_center(C, dyn_ofs)) {
        negate_v3(dyn_ofs);
        dyn_ofs_pt = dyn_ofs;
      }
    }

    /* no camera involved */
    V3D_SmoothParams sview = {nullptr};
    sview.quat = quat;
    sview.dyn_ofs = dyn_ofs_pt;
    /* No undo because this switches to/from camera. */
    sview.undo_str = nullptr;

    ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);
  }
}

void viewmove_apply(ViewOpsData *vod, int x, int y)
{
  const float event_ofs[2] = {
      float(vod->prev.event_xy[0] - x),
      float(vod->prev.event_xy[1] - y),
  };

  if ((vod->rv3d->persp == RV3D_CAMOB) && !ED_view3d_camera_lock_check(vod->v3d, vod->rv3d)) {
    ED_view3d_camera_view_pan(vod->region, event_ofs);
  }
  else if (ED_view3d_offset_lock_check(vod->v3d, vod->rv3d)) {
    vod->rv3d->ofs_lock[0] -= (event_ofs[0] * 2.0f) / float(vod->region->winx);
    vod->rv3d->ofs_lock[1] -= (event_ofs[1] * 2.0f) / float(vod->region->winy);
  }
  else {
    float dvec[3];

    ED_view3d_win_to_delta(vod->region, event_ofs, vod->init.zfac, dvec);

    sub_v3_v3(vod->rv3d->ofs, dvec);

    if (RV3D_LOCK_FLAGS(vod->rv3d) & RV3D_BOXVIEW) {
      view3d_boxview_sync(vod->area, vod->region);
    }
  }

  vod->prev.event_xy[0] = x;
  vod->prev.event_xy[1] = y;

  ED_view3d_camera_lock_sync(vod->depsgraph, vod->v3d, vod->rv3d);

  ED_region_tag_redraw(vod->region);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Navigation Utilities
 * \{ */

/* Detect the navigation operation, by the name of the navigation operator (obtained by
 * `wmKeyMapItem::idname`) */
static const ViewOpsType *view3d_navigation_type_from_idname(const char *idname)
{
  const blender::Array<const ViewOpsType *> nav_types = {
      &ViewOpsType_zoom,
      &ViewOpsType_rotate,
      &ViewOpsType_move,
      &ViewOpsType_pan,
//    &ViewOpsType_orbit,
//    &ViewOpsType_roll,
//    &ViewOpsType_dolly,
#ifdef WITH_INPUT_NDOF
      &ViewOpsType_ndof_orbit,
      &ViewOpsType_ndof_orbit_zoom,
      &ViewOpsType_ndof_pan,
      &ViewOpsType_ndof_all,
#endif
  };

  const char *op_name = idname + sizeof("VIEW3D_OT_");
  for (const ViewOpsType *nav_type : nav_types) {
    if (STREQ(op_name, nav_type->idname + sizeof("VIEW3D_OT_"))) {
      return nav_type;
    }
  }
  return nullptr;
}

/* Unlike `viewops_data_create`, `ED_view3d_navigation_init` creates a navigation context along
 * with an array of `wmKeyMapItem`s used for navigation. */
ViewOpsData *ED_view3d_navigation_init(bContext *C)
{
  if (!CTX_wm_region_view3d(C)) {
    return nullptr;
  }

  ViewOpsData *vod = MEM_cnew<ViewOpsData>(__func__);
  vod->init_context(C);

  vod->keymap = WM_keymap_find_all(CTX_wm_manager(C), "3D View", SPACE_VIEW3D, 0);
  return vod;
}

/* Checks and initializes the navigation modal operation. */
static int view3d_navigation_invoke(bContext *C,
                                    ViewOpsData *vod,
                                    const wmEvent *event,
                                    wmKeyMapItem *kmi,
                                    const ViewOpsType *nav_type)
{
  if (nav_type->poll_fn && !nav_type->poll_fn(C)) {
    return OPERATOR_CANCELLED;
  }

  return view3d_navigation_invoke_generic(C, vod, event, kmi->ptr, nav_type);
}

bool ED_view3d_navigation_do(bContext *C, ViewOpsData *vod, const wmEvent *event)
{
  if (!vod) {
    return false;
  }

  wmEvent event_tmp;
  if (event->type == EVT_MODAL_MAP) {
    /* Workaround to use the original event values. */
    event_tmp = *event;
    event_tmp.type = event->prev_type;
    event_tmp.val = event->prev_val;
    event = &event_tmp;
  }

  int op_return = OPERATOR_CANCELLED;

  if (vod->is_modal_event) {
    const eV3D_OpEvent event_code = view3d_navigate_event(vod, event);
    op_return = vod->nav_type->apply_fn(C, vod, event_code, event->xy);
    if (op_return != OPERATOR_RUNNING_MODAL) {
      vod->end_navigation(C);
      vod->is_modal_event = false;
    }
  }
  else {
    const ViewOpsType *nav_type;
    LISTBASE_FOREACH (wmKeyMapItem *, kmi, &vod->keymap->items) {
      if (!STRPREFIX(kmi->idname, "VIEW3D")) {
        continue;
      }
      if (kmi->flag & KMI_INACTIVE) {
        continue;
      }
      if ((nav_type = view3d_navigation_type_from_idname(kmi->idname)) == nullptr) {
        continue;
      }
      if (!WM_event_match(event, kmi)) {
        continue;
      }

      op_return = view3d_navigation_invoke(C, vod, event, kmi, nav_type);
      if (op_return == OPERATOR_RUNNING_MODAL) {
        vod->is_modal_event = true;
      }
      else {
        vod->end_navigation(C);
        /* Postpone the navigation confirmation to the next call.
         * This avoids constant updating of the transform operation for example. */
        vod->rv3d->rflag |= RV3D_NAVIGATING;
      }
      break;
    }
  }

  if (op_return != OPERATOR_CANCELLED) {
    /* Although #ED_view3d_update_viewmat is already called when redrawing the 3D View, do it here
     * as well, so the updated matrix values can be accessed by the operator. */
    ED_view3d_update_viewmat(
        vod->depsgraph, vod->scene, vod->v3d, vod->region, nullptr, nullptr, nullptr, false);

    return true;
  }
  else if (vod->rv3d->rflag & RV3D_NAVIGATING) {
    /* Add a fake confirmation. */
    vod->rv3d->rflag &= ~RV3D_NAVIGATING;
    return true;
  }

  return false;
}

void ED_view3d_navigation_free(bContext *C, ViewOpsData *vod)
{
  viewops_data_free(C, vod);
}

/** \} */
