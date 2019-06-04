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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmask
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_rect.h"
#include "BLI_lasso_2d.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_mask.h"

#include "DNA_mask_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_clip.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_mask.h" /* own include */

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph.h"

#include "mask_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Public Mask Selection API
 * \{ */

/* 'check' select */
bool ED_mask_spline_select_check(MaskSpline *spline)
{
  int i;

  for (i = 0; i < spline->tot_point; i++) {
    MaskSplinePoint *point = &spline->points[i];

    if (MASKPOINT_ISSEL_ANY(point)) {
      return true;
    }
  }

  return false;
}

bool ED_mask_layer_select_check(MaskLayer *masklay)
{
  MaskSpline *spline;

  if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
    return false;
  }

  for (spline = masklay->splines.first; spline; spline = spline->next) {
    if (ED_mask_spline_select_check(spline)) {
      return true;
    }
  }

  return false;
}

bool ED_mask_select_check(Mask *mask)
{
  MaskLayer *masklay;

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    if (ED_mask_layer_select_check(masklay)) {
      return true;
    }
  }

  return false;
}

/* 'sel' select  */
void ED_mask_spline_select_set(MaskSpline *spline, const bool do_select)
{
  int i;

  if (do_select) {
    spline->flag |= SELECT;
  }
  else {
    spline->flag &= ~SELECT;
  }

  for (i = 0; i < spline->tot_point; i++) {
    MaskSplinePoint *point = &spline->points[i];

    BKE_mask_point_select_set(point, do_select);
  }
}

void ED_mask_layer_select_set(MaskLayer *masklay, const bool do_select)
{
  MaskSpline *spline;

  if (masklay->restrictflag & MASK_RESTRICT_SELECT) {
    if (do_select == true) {
      return;
    }
  }

  for (spline = masklay->splines.first; spline; spline = spline->next) {
    ED_mask_spline_select_set(spline, do_select);
  }
}

void ED_mask_select_toggle_all(Mask *mask, int action)
{
  MaskLayer *masklay;

  if (action == SEL_TOGGLE) {
    if (ED_mask_select_check(mask)) {
      action = SEL_DESELECT;
    }
    else {
      action = SEL_SELECT;
    }
  }

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {

    if (masklay->restrictflag & MASK_RESTRICT_VIEW) {
      continue;
    }

    if (action == SEL_INVERT) {
      /* we don't have generic functions for this, its restricted to this operator
       * if one day we need to re-use such functionality, they can be split out */

      MaskSpline *spline;
      if (masklay->restrictflag & MASK_RESTRICT_SELECT) {
        continue;
      }
      for (spline = masklay->splines.first; spline; spline = spline->next) {
        int i;
        for (i = 0; i < spline->tot_point; i++) {
          MaskSplinePoint *point = &spline->points[i];
          BKE_mask_point_select_set(point, !MASKPOINT_ISSEL_ANY(point));
        }
      }
    }
    else {
      ED_mask_layer_select_set(masklay, (action == SEL_SELECT) ? true : false);
    }
  }
}

void ED_mask_select_flush_all(Mask *mask)
{
  MaskLayer *masklay;

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      int i;

      spline->flag &= ~SELECT;

      /* intentionally _dont_ do this in the masklay loop
       * so we clear flags on all splines */
      if (masklay->restrictflag & MASK_RESTRICT_VIEW) {
        continue;
      }

      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *cur_point = &spline->points[i];

        if (MASKPOINT_ISSEL_ANY(cur_point)) {
          spline->flag |= SELECT;
        }
        else {
          int j;

          for (j = 0; j < cur_point->tot_uw; j++) {
            if (cur_point->uw[j].flag & SELECT) {
              spline->flag |= SELECT;
              break;
            }
          }
        }
      }
    }
  }
}

void ED_mask_deselect_all(const bContext *C)
{
  Mask *mask = CTX_data_edit_mask(C);
  if (mask) {
    ED_mask_select_toggle_all(mask, SEL_DESELECT);
    ED_mask_select_flush_all(mask);
    DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name (De)select All Operator
 * \{ */

static int select_all_exec(bContext *C, wmOperator *op)
{
  Mask *mask = CTX_data_edit_mask(C);
  int action = RNA_enum_get(op->ptr, "action");

  ED_mask_select_toggle_all(mask, action);
  ED_mask_select_flush_all(mask);

  DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

  return OPERATOR_FINISHED;
}

void MASK_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->description = "Change selection of all curve points";
  ot->idname = "MASK_OT_select_all";

  /* api callbacks */
  ot->exec = select_all_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select (Cursor Pick) Operator
 * \{ */

static int select_exec(bContext *C, wmOperator *op)
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  MaskSpline *spline;
  MaskSplinePoint *point = NULL;
  float co[2];
  bool extend = RNA_boolean_get(op->ptr, "extend");
  bool deselect = RNA_boolean_get(op->ptr, "deselect");
  bool toggle = RNA_boolean_get(op->ptr, "toggle");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  eMaskWhichHandle which_handle;
  const float threshold = 19;

  RNA_float_get_array(op->ptr, "location", co);

  point = ED_mask_point_find_nearest(
      C, mask, co, threshold, &masklay, &spline, &which_handle, NULL);

  if (extend == false && deselect == false && toggle == false) {
    ED_mask_select_toggle_all(mask, SEL_DESELECT);
  }

  if (point) {
    if (which_handle != MASK_WHICH_HANDLE_NONE) {
      if (extend) {
        masklay->act_spline = spline;
        masklay->act_point = point;

        BKE_mask_point_select_set_handle(point, which_handle, true);
      }
      else if (deselect) {
        BKE_mask_point_select_set_handle(point, which_handle, false);
      }
      else {
        masklay->act_spline = spline;
        masklay->act_point = point;

        if (!MASKPOINT_ISSEL_HANDLE(point, which_handle)) {
          BKE_mask_point_select_set_handle(point, which_handle, true);
        }
        else if (toggle) {
          BKE_mask_point_select_set_handle(point, which_handle, false);
        }
      }
    }
    else {
      if (extend) {
        masklay->act_spline = spline;
        masklay->act_point = point;

        BKE_mask_point_select_set(point, true);
      }
      else if (deselect) {
        BKE_mask_point_select_set(point, false);
      }
      else {
        masklay->act_spline = spline;
        masklay->act_point = point;

        if (!MASKPOINT_ISSEL_ANY(point)) {
          BKE_mask_point_select_set(point, true);
        }
        else if (toggle) {
          BKE_mask_point_select_set(point, false);
        }
      }
    }

    masklay->act_spline = spline;
    masklay->act_point = point;

    ED_mask_select_flush_all(mask);

    DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

    return OPERATOR_FINISHED;
  }
  else {
    MaskSplinePointUW *uw;

    if (ED_mask_feather_find_nearest(
            C, mask, co, threshold, &masklay, &spline, &point, &uw, NULL)) {

      if (extend) {
        masklay->act_spline = spline;
        masklay->act_point = point;

        if (uw) {
          uw->flag |= SELECT;
        }
      }
      else if (deselect) {
        if (uw) {
          uw->flag &= ~SELECT;
        }
      }
      else {
        masklay->act_spline = spline;
        masklay->act_point = point;

        if (uw) {
          if (!(uw->flag & SELECT)) {
            uw->flag |= SELECT;
          }
          else if (toggle) {
            uw->flag &= ~SELECT;
          }
        }
      }

      ED_mask_select_flush_all(mask);

      DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

      return OPERATOR_FINISHED;
    }
    else if (deselect_all) {
      /* For clip editor tracks, leave deselect all to clip editor. */
      if (!ED_clip_can_select(C)) {
        ED_mask_deselect_all(C);
        return OPERATOR_FINISHED;
      }
    }
  }

  return OPERATOR_PASS_THROUGH;
}

static int select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);

  float co[2];

  ED_mask_mouse_pos(sa, ar, event->mval, co);

  RNA_float_set_array(op->ptr, "location", co);

  return select_exec(C, op);
}

void MASK_OT_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select";
  ot->description = "Select spline points";
  ot->idname = "MASK_OT_select";

  /* api callbacks */
  ot->exec = select_exec;
  ot->invoke = select_invoke;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_mouse_select(ot);

  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       NULL,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Location of vertex in normalized space",
                       -1.0f,
                       1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

static int box_select_exec(bContext *C, wmOperator *op)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);

  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  int i;

  rcti rect;
  rctf rectf;
  bool changed = false;

  const eSelectOp sel_op = RNA_enum_get(op->ptr, "mode");
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_mask_select_toggle_all(mask, SEL_DESELECT);
    changed = true;
  }

  /* get rectangle from operator */
  WM_operator_properties_border_to_rcti(op, &rect);

  ED_mask_point_pos(sa, ar, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
  ED_mask_point_pos(sa, ar, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);

  /* do actual selection */
  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];
        MaskSplinePoint *point_deform = &points_array[i];

        /* TODO: handles? */
        /* TODO: uw? */
        if (BLI_rctf_isect_pt_v(&rectf, point_deform->bezt.vec[1])) {
          BKE_mask_point_select_set(point, select);
          BKE_mask_point_select_set_handle(point, MASK_WHICH_HANDLE_BOTH, select);
          changed = true;
        }
      }
    }
  }

  if (changed) {
    ED_mask_select_flush_all(mask);

    DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void MASK_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->description = "Select curve points using box selection";
  ot->idname = "MASK_OT_select_box";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lasso Select Operator
 * \{ */

static bool do_lasso_select_mask(bContext *C,
                                 const int mcords[][2],
                                 short moves,
                                 const eSelectOp sel_op)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);

  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  int i;

  rcti rect;
  bool changed = false;

  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_mask_select_toggle_all(mask, SEL_DESELECT);
    changed = true;
  }

  /* get rectangle from operator */
  BLI_lasso_boundbox(&rect, mcords, moves);

  /* do actual selection */
  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];
        MaskSplinePoint *point_deform = &points_array[i];

        /* TODO: handles? */
        /* TODO: uw? */

        if (MASKPOINT_ISSEL_ANY(point) && select) {
          continue;
        }

        float screen_co[2];

        /* point in screen coords */
        ED_mask_point_pos__reverse(sa,
                                   ar,
                                   point_deform->bezt.vec[1][0],
                                   point_deform->bezt.vec[1][1],
                                   &screen_co[0],
                                   &screen_co[1]);

        if (BLI_rcti_isect_pt(&rect, screen_co[0], screen_co[1]) &&
            BLI_lasso_is_point_inside(mcords, moves, screen_co[0], screen_co[1], INT_MAX)) {
          BKE_mask_point_select_set(point, select);
          BKE_mask_point_select_set_handle(point, MASK_WHICH_HANDLE_BOTH, select);
          changed = true;
        }
      }
    }
  }

  if (changed) {
    ED_mask_select_flush_all(mask);

    DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);
  }

  return changed;
}

static int clip_lasso_select_exec(bContext *C, wmOperator *op)
{
  int mcords_tot;
  const int(*mcords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcords_tot);

  if (mcords) {
    const eSelectOp sel_op = RNA_enum_get(op->ptr, "mode");
    do_lasso_select_mask(C, mcords, mcords_tot, sel_op);

    MEM_freeN((void *)mcords);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_PASS_THROUGH;
}

void MASK_OT_select_lasso(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Lasso Select";
  ot->description = "Select curve points using lasso selection";
  ot->idname = "MASK_OT_select_lasso";

  /* api callbacks */
  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = clip_lasso_select_exec;
  ot->poll = ED_maskedit_mask_poll;
  ot->cancel = WM_gesture_lasso_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_lasso(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Select Operator
 * \{ */

static int mask_spline_point_inside_ellipse(BezTriple *bezt,
                                            const float offset[2],
                                            const float ellipse[2])
{
  /* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
  float x, y;

  x = (bezt->vec[1][0] - offset[0]) * ellipse[0];
  y = (bezt->vec[1][1] - offset[1]) * ellipse[1];

  return x * x + y * y < 1.0f;
}

static int circle_select_exec(bContext *C, wmOperator *op)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);

  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  int i;

  float zoomx, zoomy, offset[2], ellipse[2];
  int width, height;
  bool changed = false;

  /* get operator properties */
  const int x = RNA_int_get(op->ptr, "x");
  const int y = RNA_int_get(op->ptr, "y");
  const int radius = RNA_int_get(op->ptr, "radius");

  /* compute ellipse and position in unified coordinates */
  ED_mask_get_size(sa, &width, &height);
  ED_mask_zoom(sa, ar, &zoomx, &zoomy);
  width = height = max_ii(width, height);

  ellipse[0] = width * zoomx / radius;
  ellipse[1] = height * zoomy / radius;

  ED_mask_point_pos(sa, ar, x, y, &offset[0], &offset[1]);

  const eSelectOp sel_op = ED_select_op_modal(RNA_enum_get(op->ptr, "mode"),
                                              WM_gesture_is_modal_first(op->customdata));
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_mask_select_toggle_all(mask, SEL_DESELECT);
    changed = true;
  }

  /* do actual selection */
  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];
        MaskSplinePoint *point_deform = &points_array[i];

        if (mask_spline_point_inside_ellipse(&point_deform->bezt, offset, ellipse)) {
          BKE_mask_point_select_set(point, select);
          BKE_mask_point_select_set_handle(point, MASK_WHICH_HANDLE_BOTH, select);

          changed = true;
        }
      }
    }
  }

  if (changed) {
    ED_mask_select_flush_all(mask);

    DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void MASK_OT_select_circle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Circle Select";
  ot->description = "Select curve points using circle selection";
  ot->idname = "MASK_OT_select_circle";

  /* api callbacks */
  ot->invoke = WM_gesture_circle_invoke;
  ot->modal = WM_gesture_circle_modal;
  ot->exec = circle_select_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_circle(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked (Cursor Pick) Operator
 * \{ */

static int mask_select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar = CTX_wm_region(C);

  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  MaskSpline *spline;
  MaskSplinePoint *point = NULL;
  float co[2];
  bool do_select = !RNA_boolean_get(op->ptr, "deselect");
  const float threshold = 19;
  bool changed = false;

  ED_mask_mouse_pos(sa, ar, event->mval, co);

  point = ED_mask_point_find_nearest(C, mask, co, threshold, &masklay, &spline, NULL, NULL);

  if (point) {
    ED_mask_spline_select_set(spline, do_select);
    masklay->act_spline = spline;
    masklay->act_point = point;

    changed = true;
  }

  if (changed) {
    ED_mask_select_flush_all(mask);

    DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void MASK_OT_select_linked_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked";
  ot->idname = "MASK_OT_select_linked_pick";
  ot->description = "(De)select all points linked to the curve under the mouse cursor";

  /* api callbacks */
  ot->invoke = mask_select_linked_pick_invoke;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static int mask_select_linked_exec(bContext *C, wmOperator *UNUSED(op))
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;

  bool changed = false;

  /* do actual selection */
  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      if (ED_mask_spline_select_check(spline)) {
        ED_mask_spline_select_set(spline, true);
        changed = true;
      }
    }
  }

  if (changed) {
    ED_mask_select_flush_all(mask);

    DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void MASK_OT_select_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked All";
  ot->idname = "MASK_OT_select_linked";
  ot->description = "Select all curve points linked to already selected ones";

  /* api callbacks */
  ot->exec = mask_select_linked_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More/Less Operators
 * \{ */

static int mask_select_more_less(bContext *C, bool more)
{
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      const bool cyclic = (spline->flag & MASK_SPLINE_CYCLIC) != 0;
      bool start_sel, end_sel, prev_sel, cur_sel;
      int i;

      /* reselect point if any handle is selected to make the result more predictable */
      for (i = 0; i < spline->tot_point; i++) {
        BKE_mask_point_select_set(spline->points + i, MASKPOINT_ISSEL_ANY(spline->points + i));
      }

      /* select more/less does not affect empty/single point splines */
      if (spline->tot_point < 2) {
        continue;
      }

      if (cyclic) {
        start_sel = !!MASKPOINT_ISSEL_KNOT(spline->points);
        end_sel = !!MASKPOINT_ISSEL_KNOT(&spline->points[spline->tot_point - 1]);
      }
      else {
        start_sel = false;
        end_sel = false;
      }

      for (i = 0; i < spline->tot_point; i++) {
        if (i == 0 && !cyclic) {
          continue;
        }

        prev_sel = (i > 0) ? !!MASKPOINT_ISSEL_KNOT(&spline->points[i - 1]) : end_sel;
        cur_sel = !!MASKPOINT_ISSEL_KNOT(&spline->points[i]);

        if (cur_sel != more) {
          if (prev_sel == more) {
            BKE_mask_point_select_set(&spline->points[i], more);
          }
          i++;
        }
      }

      for (i = spline->tot_point - 1; i >= 0; i--) {
        if (i == spline->tot_point - 1 && !cyclic) {
          continue;
        }

        prev_sel = (i < spline->tot_point - 1) ? !!MASKPOINT_ISSEL_KNOT(&spline->points[i + 1]) :
                                                 start_sel;
        cur_sel = !!MASKPOINT_ISSEL_KNOT(&spline->points[i]);

        if (cur_sel != more) {
          if (prev_sel == more) {
            BKE_mask_point_select_set(&spline->points[i], more);
          }
          i--;
        }
      }
    }
  }

  DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

  return OPERATOR_FINISHED;
}

static int mask_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
  return mask_select_more_less(C, true);
}

void MASK_OT_select_more(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select More";
  ot->idname = "MASK_OT_select_more";
  ot->description = "Select more spline points connected to initial selection";

  /* api callbacks */
  ot->exec = mask_select_more_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int mask_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
  return mask_select_more_less(C, false);
}

void MASK_OT_select_less(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Less";
  ot->idname = "MASK_OT_select_less";
  ot->description = "Deselect spline points at the boundary of each selection region";

  /* api callbacks */
  ot->exec = mask_select_less_exec;
  ot->poll = ED_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
