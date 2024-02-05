/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_time.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"

#include "RNA_access.hh"

#include "ED_screen.hh"

#include "view3d_intern.h"
#include "view3d_navigate.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Zoom Operator
 * \{ */

/* #viewdolly_modal_keymap has an exact copy of this, apply fixes to both. */
void viewzoom_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {VIEW_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {VIEW_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},

      {VIEWROT_MODAL_SWITCH_ROTATE, "SWITCH_TO_ROTATE", 0, "Switch to Rotate"},
      {VIEWROT_MODAL_SWITCH_MOVE, "SWITCH_TO_MOVE", 0, "Switch to Move"},

      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "View3D Zoom Modal");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "View3D Zoom Modal", modal_items);

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_zoom");
}

/**
 * \param zoom_xy: Optionally zoom to window location
 * (coords compatible w/ #wmEvent.xy). Use when not nullptr.
 */
static void view_zoom_to_window_xy_camera(Scene *scene,
                                          Depsgraph *depsgraph,
                                          View3D *v3d,
                                          ARegion *region,
                                          float dfac,
                                          const int zoom_xy[2])
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  const float zoomfac = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom);
  const float zoomfac_new = clamp_f(
      zoomfac * (1.0f / dfac), RV3D_CAMZOOM_MIN_FACTOR, RV3D_CAMZOOM_MAX_FACTOR);
  const float camzoom_new = BKE_screen_view3d_zoom_from_fac(zoomfac_new);

  if (zoom_xy != nullptr) {
    float zoomfac_px;
    rctf camera_frame_old;
    rctf camera_frame_new;

    const float pt_src[2] = {float(zoom_xy[0]), float(zoom_xy[1])};
    float pt_dst[2];
    float delta_px[2];

    ED_view3d_calc_camera_border(scene, depsgraph, region, v3d, rv3d, &camera_frame_old, false);
    BLI_rctf_translate(&camera_frame_old, region->winrct.xmin, region->winrct.ymin);

    rv3d->camzoom = camzoom_new;
    CLAMP(rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);

    ED_view3d_calc_camera_border(scene, depsgraph, region, v3d, rv3d, &camera_frame_new, false);
    BLI_rctf_translate(&camera_frame_new, region->winrct.xmin, region->winrct.ymin);

    BLI_rctf_transform_pt_v(&camera_frame_new, &camera_frame_old, pt_dst, pt_src);
    sub_v2_v2v2(delta_px, pt_dst, pt_src);

    /* translate the camera offset using pixel space delta
     * mapped back to the camera (same logic as panning in camera view) */
    zoomfac_px = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom) * 2.0f;

    rv3d->camdx += delta_px[0] / (region->winx * zoomfac_px);
    rv3d->camdy += delta_px[1] / (region->winy * zoomfac_px);
    CLAMP(rv3d->camdx, -1.0f, 1.0f);
    CLAMP(rv3d->camdy, -1.0f, 1.0f);
  }
  else {
    rv3d->camzoom = camzoom_new;
    CLAMP(rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);
  }
}

/**
 * \param zoom_xy: Optionally zoom to window location
 * (coords compatible w/ #wmEvent.xy). Use when not nullptr.
 */
static void view_zoom_to_window_xy_3d(ARegion *region, float dfac, const int zoom_xy[2])
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  const float dist_new = rv3d->dist * dfac;

  if (zoom_xy != nullptr) {
    float dvec[3];
    float tvec[3];
    float tpos[3];
    float xy_delta[2];

    float zfac;

    negate_v3_v3(tpos, rv3d->ofs);

    xy_delta[0] = float(((zoom_xy[0] - region->winrct.xmin) * 2) - region->winx) / 2.0f;
    xy_delta[1] = float(((zoom_xy[1] - region->winrct.ymin) * 2) - region->winy) / 2.0f;

    /* Project cursor position into 3D space */
    zfac = ED_view3d_calc_zfac(rv3d, tpos);
    ED_view3d_win_to_delta(region, xy_delta, zfac, dvec);

    /* Calculate view target position for dolly */
    add_v3_v3v3(tvec, tpos, dvec);
    negate_v3(tvec);

    /* Offset to target position and dolly */
    copy_v3_v3(rv3d->ofs, tvec);
    rv3d->dist = dist_new;

    /* Calculate final offset */
    madd_v3_v3v3fl(rv3d->ofs, tvec, dvec, dfac);
  }
  else {
    rv3d->dist = dist_new;
  }
}

static float viewzoom_scale_value(const rcti *winrct,
                                  const eViewZoom_Style viewzoom,
                                  const bool zoom_invert,
                                  const bool zoom_invert_force,
                                  const int xy_curr[2],
                                  const int xy_init[2],
                                  const float val,
                                  const float val_orig,
                                  double *r_timer_lastdraw)
{
  float zfac;

  if (viewzoom == USER_ZOOM_CONTINUE) {
    double time = BLI_check_seconds_timer();
    float time_step = float(time - *r_timer_lastdraw);
    float fac;

    if (U.uiflag & USER_ZOOM_HORIZ) {
      fac = float(xy_init[0] - xy_curr[0]);
    }
    else {
      fac = float(xy_init[1] - xy_curr[1]);
    }

    fac /= UI_SCALE_FAC;

    if (zoom_invert != zoom_invert_force) {
      fac = -fac;
    }

    zfac = 1.0f + ((fac / 20.0f) * time_step);
    *r_timer_lastdraw = time;
  }
  else if (viewzoom == USER_ZOOM_SCALE) {
    /* method which zooms based on how far you move the mouse */

    const int ctr[2] = {
        BLI_rcti_cent_x(winrct),
        BLI_rcti_cent_y(winrct),
    };
    float len_new = (5 * UI_SCALE_FAC) + (float(len_v2v2_int(ctr, xy_curr)) / UI_SCALE_FAC);
    float len_old = (5 * UI_SCALE_FAC) + (float(len_v2v2_int(ctr, xy_init)) / UI_SCALE_FAC);

    /* intentionally ignore 'zoom_invert' for scale */
    if (zoom_invert_force) {
      std::swap(len_new, len_old);
    }

    zfac = val_orig * (len_old / max_ff(len_new, 1.0f)) / val;
  }
  else { /* USER_ZOOM_DOLLY */
    float len_new = 5 * UI_SCALE_FAC;
    float len_old = 5 * UI_SCALE_FAC;

    if (U.uiflag & USER_ZOOM_HORIZ) {
      len_new += (winrct->xmax - (xy_curr[0])) / UI_SCALE_FAC;
      len_old += (winrct->xmax - (xy_init[0])) / UI_SCALE_FAC;
    }
    else {
      len_new += (winrct->ymax - (xy_curr[1])) / UI_SCALE_FAC;
      len_old += (winrct->ymax - (xy_init[1])) / UI_SCALE_FAC;
    }

    if (zoom_invert != zoom_invert_force) {
      std::swap(len_new, len_old);
    }

    zfac = val_orig * (2.0f * ((len_new / max_ff(len_old, 1.0f)) - 1.0f) + 1.0f) / val;
  }

  return zfac;
}

static float viewzoom_scale_value_offset(const rcti *winrct,
                                         const eViewZoom_Style viewzoom,
                                         const bool zoom_invert,
                                         const bool zoom_invert_force,
                                         const int xy_curr[2],
                                         const int xy_init[2],
                                         const int xy_offset[2],
                                         const float val,
                                         const float val_orig,
                                         double *r_timer_lastdraw)
{
  const int xy_curr_offset[2] = {
      xy_curr[0] + xy_offset[0],
      xy_curr[1] + xy_offset[1],
  };
  const int xy_init_offset[2] = {
      xy_init[0] + xy_offset[0],
      xy_init[1] + xy_offset[1],
  };
  return viewzoom_scale_value(winrct,
                              viewzoom,
                              zoom_invert,
                              zoom_invert_force,
                              xy_curr_offset,
                              xy_init_offset,
                              val,
                              val_orig,
                              r_timer_lastdraw);
}

static void viewzoom_apply_camera(ViewOpsData *vod,
                                  const int xy[2],
                                  const eViewZoom_Style viewzoom,
                                  const bool zoom_invert,
                                  const bool zoom_to_pos)
{
  float zfac;
  float zoomfac_prev = BKE_screen_view3d_zoom_to_fac(vod->init.camzoom) * 2.0f;
  float zoomfac = BKE_screen_view3d_zoom_to_fac(vod->rv3d->camzoom) * 2.0f;

  zfac = viewzoom_scale_value_offset(&vod->region->winrct,
                                     viewzoom,
                                     zoom_invert,
                                     true,
                                     xy,
                                     vod->init.event_xy,
                                     vod->init.event_xy_offset,
                                     zoomfac,
                                     zoomfac_prev,
                                     &vod->prev.time);

  if (!ELEM(zfac, 1.0f, 0.0f)) {
    /* calculate inverted, then invert again (needed because of camera zoom scaling) */
    zfac = 1.0f / zfac;
    view_zoom_to_window_xy_camera(vod->scene,
                                  vod->depsgraph,
                                  vod->v3d,
                                  vod->region,
                                  zfac,
                                  zoom_to_pos ? vod->prev.event_xy : nullptr);
  }

  ED_region_tag_redraw(vod->region);
}

static void viewzoom_apply_3d(ViewOpsData *vod,
                              const int xy[2],
                              const eViewZoom_Style viewzoom,
                              const bool zoom_invert,
                              const bool zoom_to_pos)
{
  float zfac;
  float dist_range[2];

  ED_view3d_dist_range_get(vod->v3d, dist_range);

  zfac = viewzoom_scale_value_offset(&vod->region->winrct,
                                     viewzoom,
                                     zoom_invert,
                                     false,
                                     xy,
                                     vod->init.event_xy,
                                     vod->init.event_xy_offset,
                                     vod->rv3d->dist,
                                     vod->init.dist,
                                     &vod->prev.time);

  if (zfac != 1.0f) {
    const float zfac_min = dist_range[0] / vod->rv3d->dist;
    const float zfac_max = dist_range[1] / vod->rv3d->dist;
    CLAMP(zfac, zfac_min, zfac_max);

    view_zoom_to_window_xy_3d(vod->region, zfac, zoom_to_pos ? vod->prev.event_xy : nullptr);
  }

  /* these limits were in old code too */
  CLAMP(vod->rv3d->dist, dist_range[0], dist_range[1]);

  if (RV3D_LOCK_FLAGS(vod->rv3d) & RV3D_BOXVIEW) {
    view3d_boxview_sync(vod->area, vod->region);
  }

  ED_view3d_camera_lock_sync(vod->depsgraph, vod->v3d, vod->rv3d);

  ED_region_tag_redraw(vod->region);
}

static void viewzoom_apply(ViewOpsData *vod,
                           const int xy[2],
                           const eViewZoom_Style viewzoom,
                           const bool zoom_invert)
{
  const bool zoom_to_pos = (vod->viewops_flag & VIEWOPS_FLAG_ZOOM_TO_MOUSE) != 0;

  if ((vod->rv3d->persp == RV3D_CAMOB) &&
      (vod->rv3d->is_persp && ED_view3d_camera_lock_check(vod->v3d, vod->rv3d)) == 0)
  {
    viewzoom_apply_camera(vod, xy, viewzoom, zoom_invert, zoom_to_pos);
  }
  else {
    viewzoom_apply_3d(vod, xy, viewzoom, zoom_invert, zoom_to_pos);
  }
}

static int viewzoom_modal_impl(bContext *C,
                               ViewOpsData *vod,
                               const eV3D_OpEvent event_code,
                               const int xy[2])
{
  bool use_autokey = false;
  int ret = OPERATOR_RUNNING_MODAL;

  switch (event_code) {
    case VIEW_APPLY: {
      viewzoom_apply(vod, xy, (eViewZoom_Style)U.viewzoom, (U.uiflag & USER_ZOOM_INVERT) != 0);
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
    case VIEW_PASS:
      break;
  }

  if (use_autokey) {
    ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);
  }

  return ret;
}

static void view_zoom_apply_step(bContext *C,
                                 Depsgraph *depsgraph,
                                 Scene *scene,
                                 ScrArea *area,
                                 ARegion *region,
                                 const int delta,
                                 const int zoom_xy[2])
{
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  bool use_cam_zoom;
  float dist_range[2];

  use_cam_zoom = (rv3d->persp == RV3D_CAMOB) &&
                 !(rv3d->is_persp && ED_view3d_camera_lock_check(v3d, rv3d));

  ED_view3d_dist_range_get(v3d, dist_range);

  if (delta < 0) {
    const float step = 1.2f;
    if (use_cam_zoom) {
      view_zoom_to_window_xy_camera(scene, depsgraph, v3d, region, step, zoom_xy);
    }
    else {
      if (rv3d->dist < dist_range[1]) {
        view_zoom_to_window_xy_3d(region, step, zoom_xy);
      }
    }
  }
  else {
    const float step = 1.0f / 1.2f;
    if (use_cam_zoom) {
      view_zoom_to_window_xy_camera(scene, depsgraph, v3d, region, step, zoom_xy);
    }
    else {
      if (rv3d->dist > dist_range[0]) {
        view_zoom_to_window_xy_3d(region, step, zoom_xy);
      }
    }
  }

  if (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXVIEW) {
    view3d_boxview_sync(area, region);
  }

  ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d);
  ED_view3d_camera_lock_autokey(v3d, rv3d, C, false, true);

  ED_region_tag_redraw(region);
}

static int viewzoom_exec(bContext *C, wmOperator *op)
{
  BLI_assert(op->customdata == nullptr);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  const int delta = RNA_int_get(op->ptr, "delta");
  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  int zoom_xy_buf[2];
  const int *zoom_xy = nullptr;
  const bool do_zoom_to_mouse_pos = (use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS));
  if (do_zoom_to_mouse_pos) {
    zoom_xy_buf[0] = RNA_struct_property_is_set(op->ptr, "mx") ? RNA_int_get(op->ptr, "mx") :
                                                                 region->winx / 2;
    zoom_xy_buf[1] = RNA_struct_property_is_set(op->ptr, "my") ? RNA_int_get(op->ptr, "my") :
                                                                 region->winy / 2;
    zoom_xy = zoom_xy_buf;
  }

  view_zoom_apply_step(C, depsgraph, scene, area, region, delta, zoom_xy);
  ED_view3d_camera_lock_undo_grouped_push(op->type->name, v3d, rv3d, C);

  return OPERATOR_FINISHED;
}

static int viewzoom_invoke_impl(bContext *C,
                                ViewOpsData *vod,
                                const wmEvent *event,
                                PointerRNA *ptr)
{
  int xy[2];

  PropertyRNA *prop;
  prop = RNA_struct_find_property(ptr, "mx");
  xy[0] = RNA_property_is_set(ptr, prop) ? RNA_property_int_get(ptr, prop) : event->xy[0];

  prop = RNA_struct_find_property(ptr, "my");
  xy[1] = RNA_property_is_set(ptr, prop) ? RNA_property_int_get(ptr, prop) : event->xy[1];

  prop = RNA_struct_find_property(ptr, "delta");
  const int delta = RNA_property_is_set(ptr, prop) ? RNA_property_int_get(ptr, prop) : 0;

  if (delta) {
    const bool do_zoom_to_mouse_pos = (vod->viewops_flag & VIEWOPS_FLAG_ZOOM_TO_MOUSE) != 0;
    view_zoom_apply_step(C,
                         vod->depsgraph,
                         vod->scene,
                         vod->area,
                         vod->region,
                         delta,
                         do_zoom_to_mouse_pos ? xy : nullptr);

    return OPERATOR_FINISHED;
  }
  else {
    eV3D_OpEvent event_code = ELEM(event->type, MOUSEZOOM, MOUSEPAN) ? VIEW_CONFIRM : VIEW_PASS;
    if (event_code == VIEW_CONFIRM) {
      if (U.uiflag & USER_ZOOM_HORIZ) {
        vod->init.event_xy[0] = vod->prev.event_xy[0] = xy[0];
      }
      else {
        /* Set y move = x move as MOUSEZOOM uses only x axis to pass magnification value */
        vod->init.event_xy[1] = vod->prev.event_xy[1] = vod->init.event_xy[1] + xy[0] -
                                                        event->prev_xy[0];
      }
      viewzoom_apply(vod, event->prev_xy, USER_ZOOM_DOLLY, (U.uiflag & USER_ZOOM_INVERT) != 0);
      ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);

      return OPERATOR_FINISHED;
    }
  }

  if (U.viewzoom == USER_ZOOM_CONTINUE) {
    /* needs a timer to continue redrawing */
    vod->timer = WM_event_timer_add(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);
    vod->prev.time = BLI_check_seconds_timer();
  }

  return OPERATOR_RUNNING_MODAL;
}

/* viewdolly_invoke() copied this function, changes here may apply there */
static int viewzoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  return view3d_navigate_invoke_impl(C, op, event, &ViewOpsType_zoom);
}

void VIEW3D_OT_zoom(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Zoom View";
  ot->description = "Zoom in/out in the view";
  ot->idname = ViewOpsType_zoom.idname;

  /* api callbacks */
  ot->invoke = viewzoom_invoke;
  ot->exec = viewzoom_exec;
  ot->modal = view3d_navigate_modal_fn;
  ot->poll = view3d_zoom_or_dolly_poll;
  ot->cancel = view3d_navigate_cancel_fn;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* properties */
  view3d_operator_properties_common(
      ot, V3D_OP_PROP_DELTA | V3D_OP_PROP_MOUSE_CO | V3D_OP_PROP_USE_MOUSE_INIT);
}

/** \} */

const ViewOpsType ViewOpsType_zoom = {
    /*flag*/ (VIEWOPS_FLAG_DEPTH_NAVIGATE | VIEWOPS_FLAG_ZOOM_TO_MOUSE),
    /*idname*/ "VIEW3D_OT_zoom",
    /*poll_fn*/ view3d_zoom_or_dolly_poll,
    /*init_fn*/ viewzoom_invoke_impl,
    /*apply_fn*/ viewzoom_modal_impl,
};
