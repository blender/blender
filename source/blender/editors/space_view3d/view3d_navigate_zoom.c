/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "DEG_depsgraph_query.h"

#include "WM_api.h"

#include "RNA_access.h"

#include "ED_screen.h"

#include "PIL_time.h"

#include "view3d_intern.h"
#include "view3d_navigate.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Zoom Operator
 * \{ */

/* #viewdolly_modal_keymap has an exact copy of this, apply fixes to both. */
void viewzoom_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {VIEW_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},

      {VIEWROT_MODAL_SWITCH_ROTATE, "SWITCH_TO_ROTATE", 0, "Switch to Rotate"},
      {VIEWROT_MODAL_SWITCH_MOVE, "SWITCH_TO_MOVE", 0, "Switch to Move"},

      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "View3D Zoom Modal");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "View3D Zoom Modal", modal_items);

  /* disabled mode switching for now, can re-implement better, later on */
#if 0
  WM_modalkeymap_add_item(keymap,
                          &(const KeyMapItem_Params){
                              .type = LEFTMOUSE,
                              .value = KM_RELEASE,
                              .modifier = KM_ANY,
                              .direction = KM_ANY,
                          },
                          VIEWROT_MODAL_SWITCH_ROTATE);
  WM_modalkeymap_add_item(keymap,
                          &(const KeyMapItem_Params){
                              .type = EVT_LEFTCTRLKEY,
                              .value = KM_RELEASE,
                              .modifier = KM_ANY,
                              .direction = KM_ANY,
                          },
                          VIEWROT_MODAL_SWITCH_ROTATE);
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
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_zoom");
}

/**
 * \param zoom_xy: Optionally zoom to window location
 * (coords compatible w/ #wmEvent.xy). Use when not NULL.
 */
static void view_zoom_to_window_xy_camera(Scene *scene,
                                          Depsgraph *depsgraph,
                                          View3D *v3d,
                                          ARegion *region,
                                          float dfac,
                                          const int zoom_xy[2])
{
  RegionView3D *rv3d = region->regiondata;
  const float zoomfac = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom);
  const float zoomfac_new = clamp_f(
      zoomfac * (1.0f / dfac), RV3D_CAMZOOM_MIN_FACTOR, RV3D_CAMZOOM_MAX_FACTOR);
  const float camzoom_new = BKE_screen_view3d_zoom_from_fac(zoomfac_new);

  if (zoom_xy != NULL) {
    float zoomfac_px;
    rctf camera_frame_old;
    rctf camera_frame_new;

    const float pt_src[2] = {zoom_xy[0], zoom_xy[1]};
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
 * (coords compatible w/ #wmEvent.xy). Use when not NULL.
 */
static void view_zoom_to_window_xy_3d(ARegion *region, float dfac, const int zoom_xy[2])
{
  RegionView3D *rv3d = region->regiondata;
  const float dist_new = rv3d->dist * dfac;

  if (zoom_xy != NULL) {
    float dvec[3];
    float tvec[3];
    float tpos[3];
    float xy_delta[2];

    float zfac;

    negate_v3_v3(tpos, rv3d->ofs);

    xy_delta[0] = (float)(((zoom_xy[0] - region->winrct.xmin) * 2) - region->winx) / 2.0f;
    xy_delta[1] = (float)(((zoom_xy[1] - region->winrct.ymin) * 2) - region->winy) / 2.0f;

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
    double time = PIL_check_seconds_timer();
    float time_step = (float)(time - *r_timer_lastdraw);
    float fac;

    if (U.uiflag & USER_ZOOM_HORIZ) {
      fac = (float)(xy_init[0] - xy_curr[0]);
    }
    else {
      fac = (float)(xy_init[1] - xy_curr[1]);
    }

    fac /= U.dpi_fac;

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
    float len_new = (5 * U.dpi_fac) + ((float)len_v2v2_int(ctr, xy_curr) / U.dpi_fac);
    float len_old = (5 * U.dpi_fac) + ((float)len_v2v2_int(ctr, xy_init) / U.dpi_fac);

    /* intentionally ignore 'zoom_invert' for scale */
    if (zoom_invert_force) {
      SWAP(float, len_new, len_old);
    }

    zfac = val_orig * (len_old / max_ff(len_new, 1.0f)) / val;
  }
  else { /* USER_ZOOM_DOLLY */
    float len_new = 5 * U.dpi_fac;
    float len_old = 5 * U.dpi_fac;

    if (U.uiflag & USER_ZOOM_HORIZ) {
      len_new += (winrct->xmax - (xy_curr[0])) / U.dpi_fac;
      len_old += (winrct->xmax - (xy_init[0])) / U.dpi_fac;
    }
    else {
      len_new += (winrct->ymax - (xy_curr[1])) / U.dpi_fac;
      len_old += (winrct->ymax - (xy_init[1])) / U.dpi_fac;
    }

    if (zoom_invert != zoom_invert_force) {
      SWAP(float, len_new, len_old);
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
                                  zoom_to_pos ? vod->prev.event_xy : NULL);
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

    view_zoom_to_window_xy_3d(vod->region, zfac, zoom_to_pos ? vod->prev.event_xy : NULL);
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
                           const bool zoom_invert,
                           const bool zoom_to_pos)
{
  if ((vod->rv3d->persp == RV3D_CAMOB) &&
      (vod->rv3d->is_persp && ED_view3d_camera_lock_check(vod->v3d, vod->rv3d)) == 0) {
    viewzoom_apply_camera(vod, xy, viewzoom, zoom_invert, zoom_to_pos);
  }
  else {
    viewzoom_apply_3d(vod, xy, viewzoom, zoom_invert, zoom_to_pos);
  }
}

static int viewzoom_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod = op->customdata;
  short event_code = VIEW_PASS;
  bool use_autokey = false;
  int ret = OPERATOR_RUNNING_MODAL;

  /* execute the events */
  if (event->type == TIMER && event->customdata == vod->timer) {
    /* continuous zoom */
    event_code = VIEW_APPLY;
  }
  else if (event->type == MOUSEMOVE) {
    event_code = VIEW_APPLY;
  }
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case VIEW_MODAL_CONFIRM:
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_MOVE:
        WM_operator_name_call(C, "VIEW3D_OT_move", WM_OP_INVOKE_DEFAULT, NULL, event);
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_ROTATE:
        WM_operator_name_call(C, "VIEW3D_OT_rotate", WM_OP_INVOKE_DEFAULT, NULL, event);
        event_code = VIEW_CONFIRM;
        break;
    }
  }
  else if (event->type == vod->init.event_type && event->val == KM_RELEASE) {
    event_code = VIEW_CONFIRM;
  }

  if (event_code == VIEW_APPLY) {
    const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");
    viewzoom_apply(vod,
                   event->xy,
                   (eViewZoom_Style)U.viewzoom,
                   (U.uiflag & USER_ZOOM_INVERT) != 0,
                   (use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)));
    if (ED_screen_animation_playing(CTX_wm_manager(C))) {
      use_autokey = true;
    }
  }
  else if (event_code == VIEW_CONFIRM) {
    use_autokey = true;
    ret = OPERATOR_FINISHED;
  }

  if (use_autokey) {
    ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);
  }

  if (ret & OPERATOR_FINISHED) {
    viewops_data_free(C, op->customdata);
    op->customdata = NULL;
  }

  return ret;
}

static int viewzoom_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  View3D *v3d;
  RegionView3D *rv3d;
  ScrArea *area;
  ARegion *region;
  bool use_cam_zoom;
  float dist_range[2];

  const int delta = RNA_int_get(op->ptr, "delta");
  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  if (op->customdata) {
    ViewOpsData *vod = op->customdata;

    area = vod->area;
    region = vod->region;
  }
  else {
    area = CTX_wm_area(C);
    region = CTX_wm_region(C);
  }

  v3d = area->spacedata.first;
  rv3d = region->regiondata;

  use_cam_zoom = (rv3d->persp == RV3D_CAMOB) &&
                 !(rv3d->is_persp && ED_view3d_camera_lock_check(v3d, rv3d));

  int zoom_xy_buf[2];
  const int *zoom_xy = NULL;
  if (use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)) {
    zoom_xy_buf[0] = RNA_struct_property_is_set(op->ptr, "mx") ? RNA_int_get(op->ptr, "mx") :
                                                                 region->winx / 2;
    zoom_xy_buf[1] = RNA_struct_property_is_set(op->ptr, "my") ? RNA_int_get(op->ptr, "my") :
                                                                 region->winy / 2;
    zoom_xy = zoom_xy_buf;
  }

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

  viewops_data_free(C, op->customdata);
  op->customdata = NULL;

  return OPERATOR_FINISHED;
}

/* viewdolly_invoke() copied this function, changes here may apply there */
static int viewzoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod;

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  vod = op->customdata = viewops_data_create(
      C,
      event,
      (viewops_flag_from_prefs() & ~VIEWOPS_FLAG_ORBIT_SELECT) |
          (use_cursor_init ? VIEWOPS_FLAG_USE_MOUSE_INIT : 0));

  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->region);

  /* if one or the other zoom position aren't set, set from event */
  if (!RNA_struct_property_is_set(op->ptr, "mx") || !RNA_struct_property_is_set(op->ptr, "my")) {
    RNA_int_set(op->ptr, "mx", event->xy[0]);
    RNA_int_set(op->ptr, "my", event->xy[1]);
  }

  if (RNA_struct_property_is_set(op->ptr, "delta")) {
    viewzoom_exec(C, op);
  }
  else {
    if (ELEM(event->type, MOUSEZOOM, MOUSEPAN)) {

      if (U.uiflag & USER_ZOOM_HORIZ) {
        vod->init.event_xy[0] = vod->prev.event_xy[0] = event->xy[0];
      }
      else {
        /* Set y move = x move as MOUSEZOOM uses only x axis to pass magnification value */
        vod->init.event_xy[1] = vod->prev.event_xy[1] = vod->init.event_xy[1] + event->xy[0] -
                                                        event->prev_xy[0];
      }
      viewzoom_apply(vod,
                     event->prev_xy,
                     USER_ZOOM_DOLLY,
                     (U.uiflag & USER_ZOOM_INVERT) != 0,
                     (use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)));
      ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);

      viewops_data_free(C, op->customdata);
      op->customdata = NULL;
      return OPERATOR_FINISHED;
    }

    if (U.viewzoom == USER_ZOOM_CONTINUE) {
      /* needs a timer to continue redrawing */
      vod->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);
      vod->prev.time = PIL_check_seconds_timer();
    }

    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_FINISHED;
}

static void viewzoom_cancel(bContext *C, wmOperator *op)
{
  viewops_data_free(C, op->customdata);
  op->customdata = NULL;
}

void VIEW3D_OT_zoom(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Zoom View";
  ot->description = "Zoom in/out in the view";
  ot->idname = "VIEW3D_OT_zoom";

  /* api callbacks */
  ot->invoke = viewzoom_invoke;
  ot->exec = viewzoom_exec;
  ot->modal = viewzoom_modal;
  ot->poll = view3d_zoom_or_dolly_poll;
  ot->cancel = viewzoom_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* properties */
  view3d_operator_properties_common(
      ot, V3D_OP_PROP_DELTA | V3D_OP_PROP_MOUSE_CO | V3D_OP_PROP_USE_MOUSE_INIT);
}

/** \} */
