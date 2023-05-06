/* SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "view3d_navigate.h" /* own include */

/* Prototypes. */
static void viewops_data_init_context(bContext *C, ViewOpsData *vod);
static void viewops_data_init_navigation(bContext *C,
                                         const wmEvent *event,
                                         const eV3D_OpMode nav_type,
                                         const bool use_cursor_init,
                                         ViewOpsData *vod);
static void viewops_data_end_navigation(bContext *C, ViewOpsData *vod);
static int viewpan_invoke_impl(ViewOpsData *vod, PointerRNA *ptr);

const char *viewops_operator_idname_get(eV3D_OpMode nav_type)
{
  switch (nav_type) {
    case V3D_OP_MODE_ZOOM:
      return "VIEW3D_OT_zoom";
    case V3D_OP_MODE_ROTATE:
      return "VIEW3D_OT_rotate";
    case V3D_OP_MODE_MOVE:
      return "VIEW3D_OT_move";
    case V3D_OP_MODE_VIEW_PAN:
      return "VIEW3D_OT_view_pan";
    case V3D_OP_MODE_VIEW_ROLL:
      return "VIEW3D_OT_view_roll";
    case V3D_OP_MODE_DOLLY:
      return "VIEW3D_OT_dolly";
#ifdef WITH_INPUT_NDOF
    case V3D_OP_MODE_NDOF_ORBIT:
      return "VIEW3D_OT_ndof_orbit";
    case V3D_OP_MODE_NDOF_ORBIT_ZOOM:
      return "VIEW3D_OT_ndof_orbit_zoom";
#endif
  }
  BLI_assert(false);
  return nullptr;
}

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
        const eV3D_OpMode nav_type_new = (event->val == VIEWROT_MODAL_SWITCH_ZOOM) ?
                                             V3D_OP_MODE_ZOOM :
                                         (event->val == VIEWROT_MODAL_SWITCH_MOVE) ?
                                             V3D_OP_MODE_MOVE :
                                             V3D_OP_MODE_ROTATE;
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

static int view3d_navigation_modal(bContext *C,
                                   ViewOpsData *vod,
                                   const eV3D_OpEvent event_code,
                                   const int xy[2])
{
  switch (vod->nav_type) {
    case V3D_OP_MODE_ZOOM:
      return viewzoom_modal_impl(C, vod, event_code, xy);
    case V3D_OP_MODE_ROTATE:
      return viewrotate_modal_impl(C, vod, event_code, xy);
    case V3D_OP_MODE_MOVE:
      return viewmove_modal_impl(C, vod, event_code, xy);
    default:
      break;
  }
  return OPERATOR_CANCELLED;
}

static int view3d_navigation_invoke_generic(bContext *C,
                                            ViewOpsData *vod,
                                            const wmEvent *event,
                                            PointerRNA *ptr,
                                            const eV3D_OpMode nav_type)
{
  bool use_cursor_init = false;
  if (PropertyRNA *prop = RNA_struct_find_property(ptr, "use_cursor_init")) {
    use_cursor_init = RNA_property_boolean_get(ptr, prop);
  }

  viewops_data_init_navigation(C, event, nav_type, use_cursor_init, vod);
  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->region);

  switch (nav_type) {
    case V3D_OP_MODE_ZOOM:
      return viewzoom_invoke_impl(C, vod, event, ptr);
    case V3D_OP_MODE_ROTATE:
      return viewrotate_invoke_impl(vod, event);
    case V3D_OP_MODE_MOVE:
      return viewmove_invoke_impl(vod, event);
    case V3D_OP_MODE_VIEW_PAN: {
      return viewpan_invoke_impl(vod, ptr);
    }
    default:
      break;
  }
  return OPERATOR_CANCELLED;
}

int view3d_navigate_invoke_impl(bContext *C,
                                wmOperator *op,
                                const wmEvent *event,
                                const eV3D_OpMode nav_type)
{
  ViewOpsData *vod = MEM_cnew<ViewOpsData>(__func__);
  viewops_data_init_context(C, vod);
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

  const eV3D_OpMode nav_type_prev = vod->nav_type;
  const eV3D_OpEvent event_code = view3d_navigate_event(vod, event);
  if (nav_type_prev != vod->nav_type) {
    wmOperatorType *ot_new = WM_operatortype_find(viewops_operator_idname_get(vod->nav_type),
                                                  false);
    WM_operator_type_set(op, ot_new);
    viewops_data_end_navigation(C, vod);
    return view3d_navigation_invoke_generic(C, vod, event, op->ptr, vod->nav_type);
  }

  int ret = view3d_navigation_modal(C, vod, event_code, event->xy);

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
  viewops_data_free(C, (ViewOpsData *)op->customdata);
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
        ot->srna, "use_all_regions", 0, "All Regions", "View selected for all regions");
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

static eViewOpsFlag viewops_flag_from_prefs(void)
{
  const bool use_select = (U.uiflag & USER_ORBIT_SELECTION) != 0;
  const bool use_depth = (U.uiflag & USER_DEPTH_NAVIGATE) != 0;
  const bool use_zoom_to_mouse = (U.uiflag & USER_ZOOM_TO_MOUSEPOS) != 0;

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

  return flag;
}

static void viewops_data_init_context(bContext *C, ViewOpsData *vod)
{
  /* Store data. */
  vod->bmain = CTX_data_main(C);
  vod->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  vod->scene = CTX_data_scene(C);
  vod->area = CTX_wm_area(C);
  vod->region = CTX_wm_region(C);
  vod->v3d = static_cast<View3D *>(vod->area->spacedata.first);
  vod->rv3d = static_cast<RegionView3D *>(vod->region->regiondata);
}

static void viewops_data_init_navigation(bContext *C,
                                         const wmEvent *event,
                                         const eV3D_OpMode nav_type,
                                         const bool use_cursor_init,
                                         ViewOpsData *vod)
{
  Depsgraph *depsgraph = vod->depsgraph;
  RegionView3D *rv3d = vod->rv3d;

  eViewOpsFlag viewops_flag = viewops_flag_from_prefs();

  if (use_cursor_init) {
    viewops_flag |= VIEWOPS_FLAG_USE_MOUSE_INIT;
  }

  switch (nav_type) {
    case V3D_OP_MODE_ZOOM:
    case V3D_OP_MODE_MOVE:
    case V3D_OP_MODE_VIEW_PAN:
    case V3D_OP_MODE_DOLLY:
      viewops_flag &= ~VIEWOPS_FLAG_ORBIT_SELECT;
      break;
    case V3D_OP_MODE_ROTATE:
      viewops_flag |= VIEWOPS_FLAG_PERSP_ENSURE;
      break;
#ifdef WITH_INPUT_NDOF
    case V3D_OP_MODE_NDOF_ORBIT:
    case V3D_OP_MODE_NDOF_ORBIT_ZOOM:
      viewops_flag &= ~VIEWOPS_FLAG_DEPTH_NAVIGATE;
      break;
#endif
    default:
      break;
  }

  /* Could do this more nicely. */
  if ((viewops_flag & VIEWOPS_FLAG_USE_MOUSE_INIT) == 0) {
    viewops_flag &= ~(VIEWOPS_FLAG_DEPTH_NAVIGATE | VIEWOPS_FLAG_ZOOM_TO_MOUSE);
  }

  /* we need the depth info before changing any viewport options */
  if (viewops_flag & VIEWOPS_FLAG_DEPTH_NAVIGATE) {
    wmWindow *win = CTX_wm_window(C);
    const bool use_depth_last = ED_view3d_autodist_last_check(win, event);

    if (use_depth_last) {
      vod->use_dyn_ofs = ED_view3d_autodist_last_get(win, vod->dyn_ofs);
    }
    else {
      float fallback_depth_pt[3];

      view3d_operator_needs_opengl(C); /* Needed for Z-buffer drawing. */

      negate_v3_v3(fallback_depth_pt, rv3d->ofs);

      vod->use_dyn_ofs = ED_view3d_autodist(
          depsgraph, vod->region, vod->v3d, event->mval, vod->dyn_ofs, true, fallback_depth_pt);

      ED_view3d_autodist_last_set(win, event, vod->dyn_ofs, vod->use_dyn_ofs);
    }
  }
  else {
    wmWindow *win = CTX_wm_window(C);
    ED_view3d_autodist_last_clear(win);

    vod->use_dyn_ofs = false;
  }
  vod->init.persp = rv3d->persp;

  if (viewops_flag & VIEWOPS_FLAG_PERSP_ENSURE) {
    if (ED_view3d_persp_ensure(depsgraph, vod->v3d, vod->region)) {
      /* If we're switching from camera view to the perspective one,
       * need to tag viewport update, so camera view and borders are properly updated. */
      ED_region_tag_redraw(vod->region);
    }
  }

  /* set the view from the camera, if view locking is enabled.
   * we may want to make this optional but for now its needed always */
  ED_view3d_camera_lock_init(depsgraph, vod->v3d, vod->rv3d);

  vod->init.persp_with_auto_persp_applied = rv3d->persp;
  vod->init.view = rv3d->view;
  vod->init.view_axis_roll = rv3d->view_axis_roll;
  vod->init.dist = rv3d->dist;
  vod->init.camzoom = rv3d->camzoom;
  copy_qt_qt(vod->init.quat, rv3d->viewquat);
  copy_v2_v2_int(vod->init.event_xy, event->xy);
  copy_v2_v2_int(vod->prev.event_xy, event->xy);

  if (viewops_flag & VIEWOPS_FLAG_USE_MOUSE_INIT) {
    zero_v2_int(vod->init.event_xy_offset);
  }
  else {
    /* Simulate the event starting in the middle of the region. */
    vod->init.event_xy_offset[0] = BLI_rcti_cent_x(&vod->region->winrct) - event->xy[0];
    vod->init.event_xy_offset[1] = BLI_rcti_cent_y(&vod->region->winrct) - event->xy[1];
  }

  vod->init.event_type = event->type;
  copy_v3_v3(vod->init.ofs, rv3d->ofs);

  copy_qt_qt(vod->curr.viewquat, rv3d->viewquat);

  copy_v3_v3(vod->init.ofs_lock, rv3d->ofs_lock);
  vod->init.camdx = rv3d->camdx;
  vod->init.camdy = rv3d->camdy;

  if (viewops_flag & VIEWOPS_FLAG_ORBIT_SELECT) {
    float ofs[3];
    if (view3d_orbit_calc_center(C, ofs) || (vod->use_dyn_ofs == false)) {
      vod->use_dyn_ofs = true;
      negate_v3_v3(vod->dyn_ofs, ofs);
      viewops_flag &= ~VIEWOPS_FLAG_DEPTH_NAVIGATE;
    }
  }

  if (viewops_flag & VIEWOPS_FLAG_DEPTH_NAVIGATE) {
    if (vod->use_dyn_ofs) {
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
        sub_v3_v3v3(my_pivot, rv3d->ofs, upvec);
        negate_v3(my_pivot); /* ofs is flipped */

        /* find a new ofs value that is along the view axis
         * (rather than the mouse location) */
        closest_to_line_v3(dvec, vod->dyn_ofs, my_pivot, my_origin);
        vod->init.dist = rv3d->dist = len_v3v3(my_pivot, dvec);

        negate_v3_v3(rv3d->ofs, dvec);
      }
      else {
        const float mval_region_mid[2] = {float(vod->region->winx) / 2.0f,
                                          float(vod->region->winy) / 2.0f};

        ED_view3d_win_to_3d(vod->v3d, vod->region, vod->dyn_ofs, mval_region_mid, rv3d->ofs);
        negate_v3(rv3d->ofs);
      }
      negate_v3(vod->dyn_ofs);
      copy_v3_v3(vod->init.ofs, rv3d->ofs);
    }
  }

  /* For dolly */
  const float mval[2] = {float(event->mval[0]), float(event->mval[1])};
  ED_view3d_win_to_vector(vod->region, mval, vod->init.mousevec);

  {
    int event_xy_offset[2];
    add_v2_v2v2_int(event_xy_offset, event->xy, vod->init.event_xy_offset);

    /* For rotation with trackball rotation. */
    calctrackballvec(&vod->region->winrct, event_xy_offset, vod->init.trackvec);
  }

  {
    float tvec[3];
    negate_v3_v3(tvec, rv3d->ofs);
    vod->init.zfac = ED_view3d_calc_zfac(rv3d, tvec);
  }

  vod->reverse = 1.0f;
  if (rv3d->persmat[2][1] < 0.0f) {
    vod->reverse = -1.0f;
  }

  vod->nav_type = nav_type;
  vod->viewops_flag = viewops_flag;

  /* Default. */
  vod->use_dyn_ofs_ortho_correction = false;

  rv3d->rflag |= RV3D_NAVIGATING;
}

ViewOpsData *viewops_data_create(bContext *C,
                                 const wmEvent *event,
                                 const eV3D_OpMode nav_type,
                                 const bool use_cursor_init)
{
  ViewOpsData *vod = MEM_cnew<ViewOpsData>(__func__);
  viewops_data_init_context(C, vod);
  viewops_data_init_navigation(C, event, nav_type, use_cursor_init, vod);
  return vod;
}

static void viewops_data_end_navigation(bContext *C, ViewOpsData *vod)
{
  ARegion *region;
  if (vod) {
    region = vod->region;
    vod->rv3d->rflag &= ~RV3D_NAVIGATING;

    if (vod->timer) {
      WM_event_remove_timer(CTX_wm_manager(C), vod->timer->win, vod->timer);
    }

    MEM_SAFE_FREE(vod->init.dial);
  }
  else {
    region = CTX_wm_region(C);
  }

  /* Need to redraw because drawing code uses RV3D_NAVIGATING to draw
   * faster while navigation operator runs. */
  ED_region_tag_redraw(region);
}

void viewops_data_free(bContext *C, ViewOpsData *vod)
{
  viewops_data_end_navigation(C, vod);
  if (vod) {
    MEM_freeN(vod);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic View Operator Utilities
 * \{ */

/**
 * \param align_to_quat: When not nullptr, set the axis relative to this rotation.
 */
static void axis_set_view(bContext *C,
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

void viewmove_apply_reset(ViewOpsData *vod)
{
  if ((vod->rv3d->persp == RV3D_CAMOB) && !ED_view3d_camera_lock_check(vod->v3d, vod->rv3d)) {
    vod->rv3d->camdx = vod->init.camdx;
    vod->rv3d->camdy = vod->init.camdy;
  }
  else if (ED_view3d_offset_lock_check(vod->v3d, vod->rv3d)) {
    copy_v2_v2(vod->rv3d->ofs_lock, vod->init.ofs_lock);
  }
  else {
    copy_v3_v3(vod->rv3d->ofs, vod->init.ofs);
    if (RV3D_LOCK_FLAGS(vod->rv3d) & RV3D_BOXVIEW) {
      view3d_boxview_sync(vod->area, vod->region);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View All Operator
 *
 * Move & Zoom the view to fit all of its contents.
 * \{ */

static bool view3d_object_skip_minmax(const View3D *v3d,
                                      const RegionView3D *rv3d,
                                      const Object *ob,
                                      const bool skip_camera,
                                      bool *r_only_center)
{
  BLI_assert(ob->id.orig_id == nullptr);
  *r_only_center = false;

  if (skip_camera && (ob == v3d->camera)) {
    return true;
  }

  if ((ob->type == OB_EMPTY) && (ob->empty_drawtype == OB_EMPTY_IMAGE) &&
      !BKE_object_empty_image_frame_is_visible_in_view3d(ob, rv3d))
  {
    *r_only_center = true;
    return false;
  }

  return false;
}

static void view3d_object_calc_minmax(Depsgraph *depsgraph,
                                      Scene *scene,
                                      Object *ob_eval,
                                      const bool only_center,
                                      float min[3],
                                      float max[3])
{
  /* Account for duplis. */
  if (BKE_object_minmax_dupli(depsgraph, scene, ob_eval, min, max, false) == 0) {
    /* Use if duplis aren't found. */
    if (only_center) {
      minmax_v3v3_v3(min, max, ob_eval->object_to_world[3]);
    }
    else {
      BKE_object_minmax(ob_eval, min, max, false);
    }
  }
}

static void view3d_from_minmax(bContext *C,
                               View3D *v3d,
                               ARegion *region,
                               const float min[3],
                               const float max[3],
                               bool ok_dist,
                               const int smooth_viewtx)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  float afm[3];
  float size;

  ED_view3d_smooth_view_force_finish(C, v3d, region);

  /* SMOOTHVIEW */
  float new_ofs[3];
  float new_dist;

  sub_v3_v3v3(afm, max, min);
  size = max_fff(afm[0], afm[1], afm[2]);

  if (ok_dist) {
    char persp;

    if (rv3d->is_persp) {
      if (rv3d->persp == RV3D_CAMOB && ED_view3d_camera_lock_check(v3d, rv3d)) {
        persp = RV3D_CAMOB;
      }
      else {
        persp = RV3D_PERSP;
      }
    }
    else { /* ortho */
      if (size < 0.0001f) {
        /* bounding box was a single point so do not zoom */
        ok_dist = false;
      }
      else {
        /* adjust zoom so it looks nicer */
        persp = RV3D_ORTHO;
      }
    }

    if (ok_dist) {
      Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
      new_dist = ED_view3d_radius_to_dist(
          v3d, region, depsgraph, persp, true, (size / 2) * VIEW3D_MARGIN);
      if (rv3d->is_persp) {
        /* don't zoom closer than the near clipping plane */
        new_dist = max_ff(new_dist, v3d->clip_start * 1.5f);
      }
    }
  }

  mid_v3_v3v3(new_ofs, min, max);
  negate_v3(new_ofs);

  V3D_SmoothParams sview = {nullptr};
  sview.ofs = new_ofs;
  sview.dist = ok_dist ? &new_dist : nullptr;
  /* The caller needs to use undo begin/end calls. */
  sview.undo_str = nullptr;

  if (rv3d->persp == RV3D_CAMOB && !ED_view3d_camera_lock_check(v3d, rv3d)) {
    rv3d->persp = RV3D_PERSP;
    sview.camera_old = v3d->camera;
  }

  ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);

  /* Smooth-view does view-lock #RV3D_BOXVIEW copy. */
}

/**
 * Same as #view3d_from_minmax but for all regions (except cameras).
 */
static void view3d_from_minmax_multi(bContext *C,
                                     View3D *v3d,
                                     const float min[3],
                                     const float max[3],
                                     const bool ok_dist,
                                     const int smooth_viewtx)
{
  ScrArea *area = CTX_wm_area(C);
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
      /* when using all regions, don't jump out of camera view,
       * but _do_ allow locked cameras to be moved */
      if ((rv3d->persp != RV3D_CAMOB) || ED_view3d_camera_lock_check(v3d, rv3d)) {
        view3d_from_minmax(C, v3d, region, min, max, ok_dist, smooth_viewtx);
      }
    }
  }
}

static int view3d_all_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);

  const bool use_all_regions = RNA_boolean_get(op->ptr, "use_all_regions");
  const bool skip_camera = (ED_view3d_camera_lock_check(v3d, rv3d) ||
                            /* any one of the regions may be locked */
                            (use_all_regions && v3d->flag2 & V3D_LOCK_CAMERA));
  const bool center = RNA_boolean_get(op->ptr, "center");
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  float min[3], max[3];
  bool changed = false;

  if (center) {
    /* in 2.4x this also move the cursor to (0, 0, 0) (with shift+c). */
    View3DCursor *cursor = &scene->cursor;
    zero_v3(min);
    zero_v3(max);
    zero_v3(cursor->location);
    float mat3[3][3];
    unit_m3(mat3);
    BKE_scene_cursor_mat3_to_rot(cursor, mat3, false);
  }
  else {
    INIT_MINMAX(min, max);
  }

  BKE_view_layer_synced_ensure(scene_eval, view_layer_eval);
  LISTBASE_FOREACH (Base *, base_eval, BKE_view_layer_object_bases_get(view_layer_eval)) {
    if (BASE_VISIBLE(v3d, base_eval)) {
      bool only_center = false;
      Object *ob = DEG_get_original_object(base_eval->object);
      if (view3d_object_skip_minmax(v3d, rv3d, ob, skip_camera, &only_center)) {
        continue;
      }
      view3d_object_calc_minmax(depsgraph, scene, base_eval->object, only_center, min, max);
      changed = true;
    }
  }

  if (center) {
    wmMsgBus *mbus = CTX_wm_message_bus(C);
    WM_msg_publish_rna_prop(mbus, &scene->id, &scene->cursor, View3DCursor, location);

    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  }

  if (!changed) {
    ED_region_tag_redraw(region);
    /* TODO: should this be cancel?
     * I think no, because we always move the cursor, with or without
     * object, but in this case there is no change in the scene,
     * only the cursor so I choice a ED_region_tag like
     * view3d_smooth_view do for the center_cursor.
     * See bug #22640.
     */
    return OPERATOR_FINISHED;
  }

  if (RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    /* This is an approximation, see function documentation for details. */
    ED_view3d_clipping_clamp_minmax(rv3d, min, max);
  }
  ED_view3d_smooth_view_undo_begin(C, area);

  if (use_all_regions) {
    view3d_from_minmax_multi(C, v3d, min, max, true, smooth_viewtx);
  }
  else {
    view3d_from_minmax(C, v3d, region, min, max, true, smooth_viewtx);
  }

  ED_view3d_smooth_view_undo_end(C, area, op->type->name, false);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame All";
  ot->description = "View all objects in scene";
  ot->idname = "VIEW3D_OT_view_all";

  /* api callbacks */
  ot->exec = view3d_all_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;

  /* properties */
  view3d_operator_properties_common(ot, V3D_OP_PROP_USE_ALL_REGIONS);
  RNA_def_boolean(ot->srna, "center", 0, "Center", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Selected Operator
 *
 * Move & Zoom the view to fit selected contents.
 * \{ */

static int viewselected_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);
  BKE_view_layer_synced_ensure(scene_eval, view_layer_eval);
  Object *ob_eval = BKE_view_layer_active_object_get(view_layer_eval);
  Object *obedit = CTX_data_edit_object(C);
  const bGPdata *gpd_eval = ob_eval && (ob_eval->type == OB_GPENCIL_LEGACY) ?
                                static_cast<const bGPdata *>(ob_eval->data) :
                                nullptr;
  const bool is_gp_edit = gpd_eval ? GPENCIL_ANY_MODE(gpd_eval) : false;
  const bool is_face_map = ((is_gp_edit == false) && region->gizmo_map &&
                            WM_gizmomap_is_any_selected(region->gizmo_map));
  float min[3], max[3];
  bool ok = false, ok_dist = true;
  const bool use_all_regions = RNA_boolean_get(op->ptr, "use_all_regions");
  const bool skip_camera = (ED_view3d_camera_lock_check(v3d, rv3d) ||
                            /* any one of the regions may be locked */
                            (use_all_regions && v3d->flag2 & V3D_LOCK_CAMERA));
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  INIT_MINMAX(min, max);
  if (is_face_map) {
    ob_eval = nullptr;
  }

  if (ob_eval && (ob_eval->mode & OB_MODE_WEIGHT_PAINT)) {
    /* hard-coded exception, we look for the one selected armature */
    /* this is weak code this way, we should make a generic
     * active/selection callback interface once... */
    Base *base_eval;
    for (base_eval = (Base *)BKE_view_layer_object_bases_get(view_layer_eval)->first; base_eval;
         base_eval = base_eval->next)
    {
      if (BASE_SELECTED_EDITABLE(v3d, base_eval)) {
        if (base_eval->object->type == OB_ARMATURE) {
          if (base_eval->object->mode & OB_MODE_POSE) {
            break;
          }
        }
      }
    }
    if (base_eval) {
      ob_eval = base_eval->object;
    }
  }

  if (is_gp_edit) {
    CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
      /* we're only interested in selected points here... */
      if ((gps->flag & GP_STROKE_SELECT) && (gps->flag & GP_STROKE_3DSPACE)) {
        ok |= BKE_gpencil_stroke_minmax(gps, true, min, max);
      }
      if (gps->editcurve != nullptr) {
        for (int i = 0; i < gps->editcurve->tot_curve_points; i++) {
          BezTriple *bezt = &gps->editcurve->curve_points[i].bezt;
          if (bezt->f1 & SELECT) {
            minmax_v3v3_v3(min, max, bezt->vec[0]);
            ok = true;
          }
          if (bezt->f2 & SELECT) {
            minmax_v3v3_v3(min, max, bezt->vec[1]);
            ok = true;
          }
          if (bezt->f3 & SELECT) {
            minmax_v3v3_v3(min, max, bezt->vec[2]);
            ok = true;
          }
        }
      }
    }
    CTX_DATA_END;

    if ((ob_eval) && (ok)) {
      mul_m4_v3(ob_eval->object_to_world, min);
      mul_m4_v3(ob_eval->object_to_world, max);
    }
  }
  else if (is_face_map) {
    ok = WM_gizmomap_minmax(region->gizmo_map, true, true, min, max);
  }
  else if (obedit) {
    /* only selected */
    FOREACH_OBJECT_IN_MODE_BEGIN (
        scene_eval, view_layer_eval, v3d, obedit->type, obedit->mode, ob_eval_iter)
    {
      ok |= ED_view3d_minmax_verts(ob_eval_iter, min, max);
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  else if (ob_eval && (ob_eval->mode & OB_MODE_POSE)) {
    FOREACH_OBJECT_IN_MODE_BEGIN (
        scene_eval, view_layer_eval, v3d, ob_eval->type, ob_eval->mode, ob_eval_iter)
    {
      ok |= BKE_pose_minmax(ob_eval_iter, min, max, true, true);
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  else if (BKE_paint_select_face_test(ob_eval)) {
    ok = paintface_minmax(ob_eval, min, max);
  }
  else if (ob_eval && (ob_eval->mode & OB_MODE_PARTICLE_EDIT)) {
    ok = PE_minmax(depsgraph, scene, CTX_data_view_layer(C), min, max);
  }
  else if (ob_eval && (ob_eval->mode & (OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT |
                                        OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)))
  {
    BKE_paint_stroke_get_average(scene, ob_eval, min);
    copy_v3_v3(max, min);
    ok = true;
    ok_dist = 0; /* don't zoom */
  }
  else {
    LISTBASE_FOREACH (Base *, base_eval, BKE_view_layer_object_bases_get(view_layer_eval)) {
      if (BASE_SELECTED(v3d, base_eval)) {
        bool only_center = false;
        Object *ob = DEG_get_original_object(base_eval->object);
        if (view3d_object_skip_minmax(v3d, rv3d, ob, skip_camera, &only_center)) {
          continue;
        }
        view3d_object_calc_minmax(depsgraph, scene, base_eval->object, only_center, min, max);
        ok = 1;
      }
    }
  }

  if (ok == 0) {
    return OPERATOR_FINISHED;
  }

  if (RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    /* This is an approximation, see function documentation for details. */
    ED_view3d_clipping_clamp_minmax(rv3d, min, max);
  }

  ED_view3d_smooth_view_undo_begin(C, area);

  if (use_all_regions) {
    view3d_from_minmax_multi(C, v3d, min, max, ok_dist, smooth_viewtx);
  }
  else {
    view3d_from_minmax(C, v3d, region, min, max, ok_dist, smooth_viewtx);
  }

  ED_view3d_smooth_view_undo_end(C, area, op->type->name, false);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame Selected";
  ot->description = "Move the view to the selection center";
  ot->idname = "VIEW3D_OT_view_selected";

  /* api callbacks */
  ot->exec = viewselected_exec;
  ot->poll = view3d_zoom_or_dolly_poll;

  /* flags */
  ot->flag = 0;

  /* properties */
  view3d_operator_properties_common(ot, V3D_OP_PROP_USE_ALL_REGIONS);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Center Cursor Operator
 * \{ */

static int viewcenter_cursor_exec(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Scene *scene = CTX_data_scene(C);

  if (rv3d) {
    ARegion *region = CTX_wm_region(C);
    const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

    ED_view3d_smooth_view_force_finish(C, v3d, region);

    /* non camera center */
    float new_ofs[3];
    negate_v3_v3(new_ofs, scene->cursor.location);

    V3D_SmoothParams sview = {nullptr};
    sview.ofs = new_ofs;
    sview.undo_str = op->type->name;
    ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);

    /* Smooth view does view-lock #RV3D_BOXVIEW copy. */
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_cursor(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Center View to Cursor";
  ot->description = "Center the view so that the cursor is in the middle of the view";
  ot->idname = "VIEW3D_OT_view_center_cursor";

  /* api callbacks */
  ot->exec = viewcenter_cursor_exec;
  ot->poll = view3d_location_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Center Pick Operator
 * \{ */

static int viewcenter_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  ARegion *region = CTX_wm_region(C);

  if (rv3d) {
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    float new_ofs[3];
    const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

    ED_view3d_smooth_view_force_finish(C, v3d, region);

    view3d_operator_needs_opengl(C);

    if (ED_view3d_autodist(depsgraph, region, v3d, event->mval, new_ofs, false, nullptr)) {
      /* pass */
    }
    else {
      /* fallback to simple pan */
      negate_v3_v3(new_ofs, rv3d->ofs);
      ED_view3d_win_to_3d_int(v3d, region, new_ofs, event->mval, new_ofs);
    }
    negate_v3(new_ofs);

    V3D_SmoothParams sview = {nullptr};
    sview.ofs = new_ofs;
    sview.undo_str = op->type->name;

    ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Center View to Mouse";
  ot->description = "Center the view to the Z-depth position under the mouse cursor";
  ot->idname = "VIEW3D_OT_view_center_pick";

  /* api callbacks */
  ot->invoke = viewcenter_pick_invoke;
  ot->poll = view3d_location_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Axis Operator
 * \{ */

static const EnumPropertyItem prop_view_items[] = {
    {RV3D_VIEW_LEFT, "LEFT", ICON_TRIA_LEFT, "Left", "View from the left"},
    {RV3D_VIEW_RIGHT, "RIGHT", ICON_TRIA_RIGHT, "Right", "View from the right"},
    {RV3D_VIEW_BOTTOM, "BOTTOM", ICON_TRIA_DOWN, "Bottom", "View from the bottom"},
    {RV3D_VIEW_TOP, "TOP", ICON_TRIA_UP, "Top", "View from the top"},
    {RV3D_VIEW_FRONT, "FRONT", 0, "Front", "View from the front"},
    {RV3D_VIEW_BACK, "BACK", 0, "Back", "View from the back"},
    {0, nullptr, 0, nullptr, nullptr},
};

static int view_axis_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  ARegion *region;
  RegionView3D *rv3d;
  static int perspo = RV3D_PERSP;
  int viewnum;
  int view_axis_roll = RV3D_VIEW_AXIS_ROLL_0;
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* no nullptr check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d, &region);
  rv3d = static_cast<RegionView3D *>(region->regiondata);

  ED_view3d_smooth_view_force_finish(C, v3d, region);

  viewnum = RNA_enum_get(op->ptr, "type");

  float align_quat_buf[4];
  float *align_quat = nullptr;

  if (RNA_boolean_get(op->ptr, "align_active")) {
    /* align to active object */
    Object *obact = CTX_data_active_object(C);
    if (obact != nullptr) {
      float twmat[3][3];
      const Scene *scene = CTX_data_scene(C);
      ViewLayer *view_layer = CTX_data_view_layer(C);
      Object *obedit = CTX_data_edit_object(C);
      /* same as transform gizmo when normal is set */
      ED_getTransformOrientationMatrix(
          scene, view_layer, v3d, obact, obedit, V3D_AROUND_ACTIVE, twmat);
      align_quat = align_quat_buf;
      mat3_to_quat(align_quat, twmat);
      invert_qt_normalized(align_quat);
    }
  }

  if (RNA_boolean_get(op->ptr, "relative")) {
    float quat_rotate[4];
    float quat_test[4];

    if (viewnum == RV3D_VIEW_LEFT) {
      axis_angle_to_quat(quat_rotate, rv3d->viewinv[1], -M_PI_2);
    }
    else if (viewnum == RV3D_VIEW_RIGHT) {
      axis_angle_to_quat(quat_rotate, rv3d->viewinv[1], M_PI_2);
    }
    else if (viewnum == RV3D_VIEW_TOP) {
      axis_angle_to_quat(quat_rotate, rv3d->viewinv[0], -M_PI_2);
    }
    else if (viewnum == RV3D_VIEW_BOTTOM) {
      axis_angle_to_quat(quat_rotate, rv3d->viewinv[0], M_PI_2);
    }
    else if (viewnum == RV3D_VIEW_FRONT) {
      unit_qt(quat_rotate);
    }
    else if (viewnum == RV3D_VIEW_BACK) {
      axis_angle_to_quat(quat_rotate, rv3d->viewinv[0], M_PI);
    }
    else {
      BLI_assert(0);
    }

    mul_qt_qtqt(quat_test, rv3d->viewquat, quat_rotate);

    float angle_best = FLT_MAX;
    int view_best = -1;
    int view_axis_roll_best = -1;
    for (int i = RV3D_VIEW_FRONT; i <= RV3D_VIEW_BOTTOM; i++) {
      for (int j = RV3D_VIEW_AXIS_ROLL_0; j <= RV3D_VIEW_AXIS_ROLL_270; j++) {
        float quat_axis[4];
        ED_view3d_quat_from_axis_view(i, j, quat_axis);
        if (align_quat) {
          mul_qt_qtqt(quat_axis, quat_axis, align_quat);
        }
        const float angle_test = fabsf(angle_signed_qtqt(quat_axis, quat_test));
        if (angle_best > angle_test) {
          angle_best = angle_test;
          view_best = i;
          view_axis_roll_best = j;
        }
      }
    }
    if (view_best == -1) {
      view_best = RV3D_VIEW_FRONT;
      view_axis_roll_best = RV3D_VIEW_AXIS_ROLL_0;
    }

    /* Disallow non-upright views in turn-table modes,
     * it's too difficult to navigate out of them. */
    if ((U.flag & USER_TRACKBALL) == 0) {
      if (!ELEM(view_best, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
        view_axis_roll_best = RV3D_VIEW_AXIS_ROLL_0;
      }
    }

    viewnum = view_best;
    view_axis_roll = view_axis_roll_best;
  }

  /* Use this to test if we started out with a camera */
  const int nextperspo = (rv3d->persp == RV3D_CAMOB) ? rv3d->lpersp : perspo;
  float quat[4];
  ED_view3d_quat_from_axis_view(viewnum, view_axis_roll, quat);
  axis_set_view(
      C, v3d, region, quat, viewnum, view_axis_roll, nextperspo, align_quat, smooth_viewtx);

  perspo = rv3d->persp;

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_axis(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "View Axis";
  ot->description = "Use a preset viewpoint";
  ot->idname = "VIEW3D_OT_view_axis";

  /* api callbacks */
  ot->exec = view_axis_exec;
  ot->poll = ED_operator_rv3d_user_region_poll;

  /* flags */
  ot->flag = 0;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_view_items, 0, "View", "Preset viewpoint to use");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_EDITOR_VIEW3D);

  prop = RNA_def_boolean(
      ot->srna, "align_active", 0, "Align Active", "Align to the active object's axis");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "relative", 0, "Relative", "Rotate relative to the current orientation");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Camera Operator
 * \{ */

static int view_camera_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  ARegion *region;
  RegionView3D *rv3d;
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* no nullptr check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d, &region);
  rv3d = static_cast<RegionView3D *>(region->regiondata);

  ED_view3d_smooth_view_force_finish(C, v3d, region);

  if ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ANY_TRANSFORM) == 0) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Scene *scene = CTX_data_scene(C);

    if (rv3d->persp != RV3D_CAMOB) {
      BKE_view_layer_synced_ensure(scene, view_layer);
      Object *ob = BKE_view_layer_active_object_get(view_layer);

      if (!rv3d->smooth_timer) {
        /* store settings of current view before allowing overwriting with camera view
         * only if we're not currently in a view transition */

        ED_view3d_lastview_store(rv3d);
      }

      /* first get the default camera for the view lock type */
      if (v3d->scenelock) {
        /* sets the camera view if available */
        v3d->camera = scene->camera;
      }
      else {
        /* use scene camera if one is not set (even though we're unlocked) */
        if (v3d->camera == nullptr) {
          v3d->camera = scene->camera;
        }
      }

      /* if the camera isn't found, check a number of options */
      if (v3d->camera == nullptr && ob && ob->type == OB_CAMERA) {
        v3d->camera = ob;
      }

      if (v3d->camera == nullptr) {
        v3d->camera = BKE_view_layer_camera_find(scene, view_layer);
      }

      /* couldn't find any useful camera, bail out */
      if (v3d->camera == nullptr) {
        return OPERATOR_CANCELLED;
      }

      /* important these don't get out of sync for locked scenes */
      if (v3d->scenelock && scene->camera != v3d->camera) {
        scene->camera = v3d->camera;
        DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
      }

      /* finally do snazzy view zooming */
      rv3d->persp = RV3D_CAMOB;

      V3D_SmoothParams sview = {nullptr};
      sview.camera = v3d->camera;
      sview.ofs = rv3d->ofs;
      sview.quat = rv3d->viewquat;
      sview.dist = &rv3d->dist;
      sview.lens = &v3d->lens;
      /* No undo because this changes cameras (and wont move the camera). */
      sview.undo_str = nullptr;

      ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);
    }
    else {
      /* return to settings of last view */
      /* does view3d_smooth_view too */
      axis_set_view(C,
                    v3d,
                    region,
                    rv3d->lviewquat,
                    rv3d->lview,
                    rv3d->lview_axis_roll,
                    rv3d->lpersp,
                    nullptr,
                    smooth_viewtx);
    }
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_camera(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Camera";
  ot->description = "Toggle the camera view";
  ot->idname = "VIEW3D_OT_view_camera";

  /* api callbacks */
  ot->exec = view_camera_exec;
  ot->poll = ED_operator_rv3d_user_region_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Orbit Operator
 *
 * Rotate (orbit) in incremental steps. For interactive orbit see #VIEW3D_OT_rotate.
 * \{ */

enum {
  V3D_VIEW_STEPLEFT = 1,
  V3D_VIEW_STEPRIGHT,
  V3D_VIEW_STEPDOWN,
  V3D_VIEW_STEPUP,
};

static const EnumPropertyItem prop_view_orbit_items[] = {
    {V3D_VIEW_STEPLEFT, "ORBITLEFT", 0, "Orbit Left", "Orbit the view around to the left"},
    {V3D_VIEW_STEPRIGHT, "ORBITRIGHT", 0, "Orbit Right", "Orbit the view around to the right"},
    {V3D_VIEW_STEPUP, "ORBITUP", 0, "Orbit Up", "Orbit the view up"},
    {V3D_VIEW_STEPDOWN, "ORBITDOWN", 0, "Orbit Down", "Orbit the view down"},
    {0, nullptr, 0, nullptr, nullptr},
};

static int vieworbit_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  ARegion *region;
  RegionView3D *rv3d;
  int orbitdir;
  char view_opposite;
  PropertyRNA *prop_angle = RNA_struct_find_property(op->ptr, "angle");
  float angle = RNA_property_is_set(op->ptr, prop_angle) ?
                    RNA_property_float_get(op->ptr, prop_angle) :
                    DEG2RADF(U.pad_rot_angle);

  /* no nullptr check is needed, poll checks */
  v3d = CTX_wm_view3d(C);
  region = CTX_wm_region(C);
  rv3d = static_cast<RegionView3D *>(region->regiondata);

  /* support for switching to the opposite view (even when in locked views) */
  view_opposite = (fabsf(angle) == float(M_PI)) ? ED_view3d_axis_view_opposite(rv3d->view) :
                                                  RV3D_VIEW_USER;
  orbitdir = RNA_enum_get(op->ptr, "type");

  if ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) && (view_opposite == RV3D_VIEW_USER)) {
    /* no nullptr check is needed, poll checks */
    ED_view3d_context_user_region(C, &v3d, &region);
    rv3d = static_cast<RegionView3D *>(region->regiondata);
  }

  ED_view3d_smooth_view_force_finish(C, v3d, region);

  if ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) == 0 || (view_opposite != RV3D_VIEW_USER)) {
    const bool is_camera_lock = ED_view3d_camera_lock_check(v3d, rv3d);
    if ((rv3d->persp != RV3D_CAMOB) || is_camera_lock) {
      if (is_camera_lock) {
        const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
        ED_view3d_camera_lock_init(depsgraph, v3d, rv3d);
      }
      int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
      float quat_mul[4];
      float quat_new[4];

      if (view_opposite == RV3D_VIEW_USER) {
        const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
        ED_view3d_persp_ensure(depsgraph, v3d, region);
      }

      if (ELEM(orbitdir, V3D_VIEW_STEPLEFT, V3D_VIEW_STEPRIGHT)) {
        if (orbitdir == V3D_VIEW_STEPRIGHT) {
          angle = -angle;
        }

        /* z-axis */
        axis_angle_to_quat_single(quat_mul, 'Z', angle);
      }
      else {

        if (orbitdir == V3D_VIEW_STEPDOWN) {
          angle = -angle;
        }

        /* horizontal axis */
        axis_angle_to_quat(quat_mul, rv3d->viewinv[0], angle);
      }

      mul_qt_qtqt(quat_new, rv3d->viewquat, quat_mul);

      /* avoid precision loss over time */
      normalize_qt(quat_new);

      if (view_opposite != RV3D_VIEW_USER) {
        rv3d->view = view_opposite;
        /* avoid float in-precision, just get a new orientation */
        ED_view3d_quat_from_axis_view(view_opposite, rv3d->view_axis_roll, quat_new);
      }
      else {
        rv3d->view = RV3D_VIEW_USER;
      }

      float dyn_ofs[3], *dyn_ofs_pt = nullptr;

      if (U.uiflag & USER_ORBIT_SELECTION) {
        if (view3d_orbit_calc_center(C, dyn_ofs)) {
          negate_v3(dyn_ofs);
          dyn_ofs_pt = dyn_ofs;
        }
      }

      V3D_SmoothParams sview = {nullptr};
      sview.quat = quat_new;
      sview.dyn_ofs = dyn_ofs_pt;
      sview.lens = &v3d->lens;
      /* Group as successive orbit may run by holding a key. */
      sview.undo_str = op->type->name;
      sview.undo_grouped = true;

      ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);

      return OPERATOR_FINISHED;
    }
  }

  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_view_orbit(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "View Orbit";
  ot->description = "Orbit the view";
  ot->idname = "VIEW3D_OT_view_orbit";

  /* api callbacks */
  ot->exec = vieworbit_exec;
  ot->poll = ED_operator_rv3d_user_region_poll;

  /* flags */
  ot->flag = 0;

  /* properties */
  prop = RNA_def_float(ot->srna, "angle", 0, -FLT_MAX, FLT_MAX, "Roll", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  ot->prop = RNA_def_enum(
      ot->srna, "type", prop_view_orbit_items, 0, "Orbit", "Direction of View Orbit");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Pan Operator
 *
 * Move (pan) in incremental steps. For interactive pan see #VIEW3D_OT_move.
 * \{ */

enum {
  V3D_VIEW_PANLEFT = 1,
  V3D_VIEW_PANRIGHT,
  V3D_VIEW_PANDOWN,
  V3D_VIEW_PANUP,
};

static const EnumPropertyItem prop_view_pan_items[] = {
    {V3D_VIEW_PANLEFT, "PANLEFT", 0, "Pan Left", "Pan the view to the left"},
    {V3D_VIEW_PANRIGHT, "PANRIGHT", 0, "Pan Right", "Pan the view to the right"},
    {V3D_VIEW_PANUP, "PANUP", 0, "Pan Up", "Pan the view up"},
    {V3D_VIEW_PANDOWN, "PANDOWN", 0, "Pan Down", "Pan the view down"},
    {0, nullptr, 0, nullptr, nullptr},
};

static int viewpan_invoke_impl(ViewOpsData *vod, PointerRNA *ptr)
{
  int x = 0, y = 0;
  int pandir = RNA_enum_get(ptr, "type");

  if (pandir == V3D_VIEW_PANRIGHT) {
    x = -32;
  }
  else if (pandir == V3D_VIEW_PANLEFT) {
    x = 32;
  }
  else if (pandir == V3D_VIEW_PANUP) {
    y = -25;
  }
  else if (pandir == V3D_VIEW_PANDOWN) {
    y = 25;
  }

  viewmove_apply(vod, vod->prev.event_xy[0] + x, vod->prev.event_xy[1] + y);

  return OPERATOR_FINISHED;
}

static int viewpan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  return view3d_navigate_invoke_impl(C, op, event, V3D_OP_MODE_VIEW_PAN);
}

void VIEW3D_OT_view_pan(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pan View Direction";
  ot->description = "Pan the view in a given direction";
  ot->idname = viewops_operator_idname_get(V3D_OP_MODE_VIEW_PAN);

  /* api callbacks */
  ot->invoke = viewpan_invoke;
  ot->poll = view3d_location_poll;

  /* flags */
  ot->flag = 0;

  /* Properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", prop_view_pan_items, 0, "Pan", "Direction of View Pan");
}

/** \} */
