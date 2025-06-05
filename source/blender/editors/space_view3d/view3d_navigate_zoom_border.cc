/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_camera_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "BKE_context.hh"
#include "BKE_report.hh"

#include "WM_api.hh"

#include "RNA_access.hh"

#include "view3d_intern.hh"
#include "view3d_navigate.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Border Zoom Operator
 * \{ */

static wmOperatorStatus view3d_zoom_border_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* Zooms in on a border drawn by the user */
  rcti rect;
  float dvec[3], vb[2], xscale, yscale;

  /* SMOOTHVIEW */
  float dist_new;
  float ofs_new[3];

  /* ZBuffer depth vars */
  float depth_close = FLT_MAX;
  float cent[2], p[3];

  /* NOTE: otherwise opengl won't work. */
  view3d_operator_needs_gpu(C);

  /* get box select values using rna */
  WM_operator_properties_border_to_rcti(op, &rect);

  /* check if zooming in/out view */
  const bool zoom_in = !RNA_boolean_get(op->ptr, "zoom_out");

  const blender::Bounds<float> dist_range = ED_view3d_dist_soft_range_get(v3d, rv3d->is_persp);

  ED_view3d_depth_override(CTX_data_ensure_evaluated_depsgraph(C),
                           region,
                           v3d,
                           nullptr,
                           V3D_DEPTH_NO_GPENCIL,
                           true,
                           nullptr);
  {
    /* avoid allocating the whole depth buffer */
    ViewDepths depth_temp = {0};

    /* avoid view3d_update_depths() for speed. */
    view3d_depths_rect_create(region, &rect, &depth_temp);

    /* find the closest Z pixel */
    depth_close = view3d_depth_near(&depth_temp);

    MEM_SAFE_FREE(depth_temp.depths);
  }

  /* Resize border to the same ratio as the window. */
  {
    const float region_aspect = float(region->winx) / float(region->winy);
    if ((float(BLI_rcti_size_x(&rect)) / float(BLI_rcti_size_y(&rect))) < region_aspect) {
      BLI_rcti_resize_x(&rect, int(BLI_rcti_size_y(&rect) * region_aspect));
    }
    else {
      BLI_rcti_resize_y(&rect, int(BLI_rcti_size_x(&rect) / region_aspect));
    }
  }

  cent[0] = (float(rect.xmin) + float(rect.xmax)) / 2;
  cent[1] = (float(rect.ymin) + float(rect.ymax)) / 2;

  if (rv3d->is_persp) {
    float p_corner[3];

    /* no depths to use, we can't do anything! */
    if (depth_close == FLT_MAX) {
      BKE_report(op->reports, RPT_ERROR, "Depth too large");
      return OPERATOR_CANCELLED;
    }
    /* convert border to 3d coordinates */
    if (!ED_view3d_unproject_v3(region, cent[0], cent[1], depth_close, p) ||
        !ED_view3d_unproject_v3(region, rect.xmin, rect.ymin, depth_close, p_corner))
    {
      return OPERATOR_CANCELLED;
    }

    sub_v3_v3v3(dvec, p, p_corner);
    negate_v3_v3(ofs_new, p);

    dist_new = len_v3(dvec);

    /* Account for the lens, without this a narrow lens zooms in too close. */
    dist_new *= (v3d->lens / DEFAULT_SENSOR_WIDTH);
  }
  else { /* orthographic */
    /* find the current window width and height */
    vb[0] = region->winx;
    vb[1] = region->winy;

    dist_new = rv3d->dist;

    /* convert the drawn rectangle into 3d space */
    if (depth_close != FLT_MAX && ED_view3d_unproject_v3(region, cent[0], cent[1], depth_close, p))
    {
      negate_v3_v3(ofs_new, p);
    }
    else {
      float xy_delta[2];
      float zfac;

      /* We can't use the depth, fall back to the old way that doesn't set the center depth */
      copy_v3_v3(ofs_new, rv3d->ofs);

      {
        float tvec[3];
        negate_v3_v3(tvec, ofs_new);
        zfac = ED_view3d_calc_zfac(rv3d, tvec);
      }

      xy_delta[0] = (rect.xmin + rect.xmax - vb[0]) / 2.0f;
      xy_delta[1] = (rect.ymin + rect.ymax - vb[1]) / 2.0f;
      ED_view3d_win_to_delta(region, xy_delta, zfac, dvec);
      /* center the view to the center of the rectangle */
      sub_v3_v3(ofs_new, dvec);
    }

    /* work out the ratios, so that everything selected fits when we zoom */
    xscale = (BLI_rcti_size_x(&rect) / vb[0]);
    yscale = (BLI_rcti_size_y(&rect) / vb[1]);
    dist_new *= max_ff(xscale, yscale);
  }

  if (!zoom_in) {
    sub_v3_v3v3(dvec, ofs_new, rv3d->ofs);
    dist_new = rv3d->dist * (rv3d->dist / dist_new);
    add_v3_v3v3(ofs_new, rv3d->ofs, dvec);
  }

  /* clamp after because we may have been zooming out */
  CLAMP(dist_new, dist_range.min, dist_range.max);

  const bool is_camera_lock = ED_view3d_camera_lock_check(v3d, rv3d);
  if (rv3d->persp == RV3D_CAMOB) {
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    if (is_camera_lock) {
      ED_view3d_camera_lock_init(depsgraph, v3d, rv3d);
    }
    else {
      ED_view3d_persp_switch_from_camera(depsgraph, v3d, rv3d, RV3D_PERSP);
    }
  }
  V3D_SmoothParams sview_params = {};
  sview_params.ofs = ofs_new;
  sview_params.dist = &dist_new;
  sview_params.undo_str = op->type->name;

  ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview_params);

  if (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXVIEW) {
    view3d_boxview_sync(CTX_wm_area(C), region);
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_zoom_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Zoom to Border";
  ot->description = "Zoom in the view to the nearest object contained in the border";
  ot->idname = "VIEW3D_OT_zoom_border";

  /* API callbacks. */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = view3d_zoom_border_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = view3d_zoom_or_dolly_poll;

  /* flags */
  ot->flag = 0;

  /* properties */
  WM_operator_properties_gesture_box_zoom(ot);
}

/** \} */
