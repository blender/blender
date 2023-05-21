/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation */

/** \file
 * \ingroup edmask
 */

#include "MEM_guardedalloc.h"

#include "BLI_lasso_2d.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_mask.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DNA_mask_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_clip.h"
#include "ED_mask.h" /* own include */
#include "ED_select_utils.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "mask_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Public Mask Selection API
 * \{ */

bool ED_mask_spline_select_check(const MaskSpline *spline)
{
  for (int i = 0; i < spline->tot_point; i++) {
    MaskSplinePoint *point = &spline->points[i];

    if (MASKPOINT_ISSEL_ANY(point)) {
      return true;
    }
  }

  return false;
}

bool ED_mask_layer_select_check(const MaskLayer *mask_layer)
{
  if (mask_layer->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
    return false;
  }

  LISTBASE_FOREACH (const MaskSpline *, spline, &mask_layer->splines) {
    if (ED_mask_spline_select_check(spline)) {
      return true;
    }
  }

  return false;
}

bool ED_mask_select_check(const Mask *mask)
{
  LISTBASE_FOREACH (const MaskLayer *, mask_layer, &mask->masklayers) {
    if (ED_mask_layer_select_check(mask_layer)) {
      return true;
    }
  }

  return false;
}

void ED_mask_spline_select_set(MaskSpline *spline, const bool do_select)
{
  if (do_select) {
    spline->flag |= SELECT;
  }
  else {
    spline->flag &= ~SELECT;
  }

  for (int i = 0; i < spline->tot_point; i++) {
    MaskSplinePoint *point = &spline->points[i];

    BKE_mask_point_select_set(point, do_select);
  }
}

void ED_mask_layer_select_set(MaskLayer *mask_layer, const bool do_select)
{
  if (mask_layer->visibility_flag & MASK_HIDE_SELECT) {
    if (do_select == true) {
      return;
    }
  }

  LISTBASE_FOREACH (MaskSpline *, spline, &mask_layer->splines) {
    ED_mask_spline_select_set(spline, do_select);
  }
}

void ED_mask_select_toggle_all(Mask *mask, int action)
{
  if (action == SEL_TOGGLE) {
    if (ED_mask_select_check(mask)) {
      action = SEL_DESELECT;
    }
    else {
      action = SEL_SELECT;
    }
  }

  LISTBASE_FOREACH (MaskLayer *, mask_layer, &mask->masklayers) {

    if (mask_layer->visibility_flag & MASK_HIDE_VIEW) {
      continue;
    }

    if (action == SEL_INVERT) {
      /* we don't have generic functions for this, its restricted to this operator
       * if one day we need to re-use such functionality, they can be split out */

      if (mask_layer->visibility_flag & MASK_HIDE_SELECT) {
        continue;
      }
      LISTBASE_FOREACH (MaskSpline *, spline, &mask_layer->splines) {
        for (int i = 0; i < spline->tot_point; i++) {
          MaskSplinePoint *point = &spline->points[i];
          BKE_mask_point_select_set(point, !MASKPOINT_ISSEL_ANY(point));
        }
      }
    }
    else {
      ED_mask_layer_select_set(mask_layer, (action == SEL_SELECT) ? true : false);
    }
  }
}

void ED_mask_select_flush_all(Mask *mask)
{
  LISTBASE_FOREACH (MaskLayer *, mask_layer, &mask->masklayers) {
    LISTBASE_FOREACH (MaskSpline *, spline, &mask_layer->splines) {
      spline->flag &= ~SELECT;

      /* Intentionally *don't* do this in the mask layer loop
       * so we clear flags on all splines. */
      if (mask_layer->visibility_flag & MASK_HIDE_VIEW) {
        continue;
      }

      for (int i = 0; i < spline->tot_point; i++) {
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

  MaskViewLockState lock_state;
  ED_mask_view_lock_state_store(C, &lock_state);

  ED_mask_select_toggle_all(mask, action);
  ED_mask_select_flush_all(mask);

  DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

  ED_mask_view_lock_state_restore_no_jump(C, &lock_state);

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
  ot->poll = ED_maskedit_mask_visible_splines_poll;

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
  MaskLayer *mask_layer;
  MaskSpline *spline;
  MaskSplinePoint *point = nullptr;
  float co[2];
  bool extend = RNA_boolean_get(op->ptr, "extend");
  bool deselect = RNA_boolean_get(op->ptr, "deselect");
  bool toggle = RNA_boolean_get(op->ptr, "toggle");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  eMaskWhichHandle which_handle;
  const float threshold = 19;

  MaskViewLockState lock_state;
  ED_mask_view_lock_state_store(C, &lock_state);

  RNA_float_get_array(op->ptr, "location", co);

  point = ED_mask_point_find_nearest(
      C, mask, co, threshold, &mask_layer, &spline, &which_handle, nullptr);

  if (extend == false && deselect == false && toggle == false) {
    ED_mask_select_toggle_all(mask, SEL_DESELECT);
  }

  if (point) {
    if (which_handle != MASK_WHICH_HANDLE_NONE) {
      if (extend) {
        mask_layer->act_spline = spline;
        mask_layer->act_point = point;

        BKE_mask_point_select_set_handle(point, which_handle, true);
      }
      else if (deselect) {
        BKE_mask_point_select_set_handle(point, which_handle, false);
      }
      else {
        mask_layer->act_spline = spline;
        mask_layer->act_point = point;

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
        mask_layer->act_spline = spline;
        mask_layer->act_point = point;

        BKE_mask_point_select_set(point, true);
      }
      else if (deselect) {
        BKE_mask_point_select_set(point, false);
      }
      else {
        mask_layer->act_spline = spline;
        mask_layer->act_point = point;

        if (!MASKPOINT_ISSEL_ANY(point)) {
          BKE_mask_point_select_set(point, true);
        }
        else if (toggle) {
          BKE_mask_point_select_set(point, false);
        }
      }
    }

    mask_layer->act_spline = spline;
    mask_layer->act_point = point;

    ED_mask_select_flush_all(mask);

    DEG_id_tag_update(&mask->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

    ED_mask_view_lock_state_restore_no_jump(C, &lock_state);

    return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
  }

  MaskSplinePointUW *uw;

  if (ED_mask_feather_find_nearest(
          C, mask, co, threshold, &mask_layer, &spline, &point, &uw, nullptr))
  {

    if (extend) {
      mask_layer->act_spline = spline;
      mask_layer->act_point = point;

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
      mask_layer->act_spline = spline;
      mask_layer->act_point = point;

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

    ED_mask_view_lock_state_restore_no_jump(C, &lock_state);

    return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
  }
  if (deselect_all) {
    ED_mask_deselect_all(C);
    ED_mask_view_lock_state_restore_no_jump(C, &lock_state);
    return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
  }

  return OPERATOR_PASS_THROUGH;
}

static int select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  float co[2];

  ED_mask_mouse_pos(area, region, event->mval, co);

  RNA_float_set_array(op->ptr, "location", co);

  const int retval = select_exec(C, op);

  return WM_operator_flag_only_pass_through_on_press(retval, event);
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
  ot->poll = ED_maskedit_mask_visible_splines_poll;
  ot->get_name = ED_select_pick_get_name;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_mouse_select(ot);

  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       nullptr,
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
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  Mask *mask_orig = CTX_data_edit_mask(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Mask *mask_eval = (Mask *)DEG_get_evaluated_id(depsgraph, &mask_orig->id);

  rcti rect;
  rctf rectf;
  bool changed = false;

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_mask_select_toggle_all(mask_orig, SEL_DESELECT);
    changed = true;
  }

  /* get rectangle from operator */
  WM_operator_properties_border_to_rcti(op, &rect);

  ED_mask_point_pos(area, region, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
  ED_mask_point_pos(area, region, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);

  /* do actual selection */
  for (MaskLayer *mask_layer_orig = static_cast<MaskLayer *>(mask_orig->masklayers.first),
                 *mask_layer_eval = static_cast<MaskLayer *>(mask_eval->masklayers.first);
       mask_layer_orig != nullptr;
       mask_layer_orig = mask_layer_orig->next, mask_layer_eval = mask_layer_eval->next)
  {
    if (mask_layer_orig->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }
    for (MaskSpline *spline_orig = static_cast<MaskSpline *>(mask_layer_orig->splines.first),
                    *spline_eval = static_cast<MaskSpline *>(mask_layer_eval->splines.first);
         spline_orig != nullptr;
         spline_orig = spline_orig->next, spline_eval = spline_eval->next)
    {

      MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline_eval);

      for (int i = 0; i < spline_orig->tot_point; i++) {
        MaskSplinePoint *point = &spline_orig->points[i];
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
    ED_mask_select_flush_all(mask_orig);

    DEG_id_tag_update(&mask_orig->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask_orig);

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
  ot->poll = ED_maskedit_mask_visible_splines_poll;

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
                                 const int mcoords[][2],
                                 const int mcoords_len,
                                 const eSelectOp sel_op)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  Mask *mask_orig = CTX_data_edit_mask(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Mask *mask_eval = (Mask *)DEG_get_evaluated_id(depsgraph, &mask_orig->id);

  rcti rect;
  bool changed = false;

  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_mask_select_toggle_all(mask_orig, SEL_DESELECT);
    changed = true;
  }

  /* get rectangle from operator */
  BLI_lasso_boundbox(&rect, mcoords, mcoords_len);

  /* do actual selection */
  for (MaskLayer *mask_layer_orig = static_cast<MaskLayer *>(mask_orig->masklayers.first),
                 *mask_layer_eval = static_cast<MaskLayer *>(mask_eval->masklayers.first);
       mask_layer_orig != nullptr;
       mask_layer_orig = mask_layer_orig->next, mask_layer_eval = mask_layer_eval->next)
  {
    if (mask_layer_orig->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }
    for (MaskSpline *spline_orig = static_cast<MaskSpline *>(mask_layer_orig->splines.first),
                    *spline_eval = static_cast<MaskSpline *>(mask_layer_eval->splines.first);
         spline_orig != nullptr;
         spline_orig = spline_orig->next, spline_eval = spline_eval->next)
    {

      MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline_eval);

      for (int i = 0; i < spline_orig->tot_point; i++) {
        MaskSplinePoint *point = &spline_orig->points[i];
        MaskSplinePoint *point_deform = &points_array[i];

        /* TODO: handles? */
        /* TODO: uw? */

        if (MASKPOINT_ISSEL_ANY(point) && select) {
          continue;
        }

        float screen_co[2];

        /* point in screen coords */
        ED_mask_point_pos__reverse(area,
                                   region,
                                   point_deform->bezt.vec[1][0],
                                   point_deform->bezt.vec[1][1],
                                   &screen_co[0],
                                   &screen_co[1]);

        if (BLI_rcti_isect_pt(&rect, screen_co[0], screen_co[1]) &&
            BLI_lasso_is_point_inside(mcoords, mcoords_len, screen_co[0], screen_co[1], INT_MAX))
        {
          BKE_mask_point_select_set(point, select);
          BKE_mask_point_select_set_handle(point, MASK_WHICH_HANDLE_BOTH, select);
          changed = true;
        }
      }
    }
  }

  if (changed) {
    ED_mask_select_flush_all(mask_orig);

    DEG_id_tag_update(&mask_orig->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask_orig);
  }

  return changed;
}

static int clip_lasso_select_exec(bContext *C, wmOperator *op)
{
  int mcoords_len;
  const int(*mcoords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcoords_len);

  if (mcoords) {
    const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
    do_lasso_select_mask(C, mcoords, mcoords_len, sel_op);

    MEM_freeN((void *)mcoords);

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
  ot->poll = ED_maskedit_mask_visible_splines_poll;
  ot->cancel = WM_gesture_lasso_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

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
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  Mask *mask_orig = CTX_data_edit_mask(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Mask *mask_eval = (Mask *)DEG_get_evaluated_id(depsgraph, &mask_orig->id);

  float zoomx, zoomy, offset[2], ellipse[2];
  int width, height;
  bool changed = false;

  /* get operator properties */
  const int x = RNA_int_get(op->ptr, "x");
  const int y = RNA_int_get(op->ptr, "y");
  const int radius = RNA_int_get(op->ptr, "radius");

  /* compute ellipse and position in unified coordinates */
  ED_mask_get_size(area, &width, &height);
  ED_mask_zoom(area, region, &zoomx, &zoomy);
  width = height = max_ii(width, height);

  ellipse[0] = width * zoomx / radius;
  ellipse[1] = height * zoomy / radius;

  ED_mask_point_pos(area, region, x, y, &offset[0], &offset[1]);

  const eSelectOp sel_op = ED_select_op_modal(
      eSelectOp(RNA_enum_get(op->ptr, "mode")),
      WM_gesture_is_modal_first(static_cast<wmGesture *>(op->customdata)));
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_mask_select_toggle_all(mask_orig, SEL_DESELECT);
    changed = true;
  }

  /* do actual selection */
  for (MaskLayer *mask_layer_orig = static_cast<MaskLayer *>(mask_orig->masklayers.first),
                 *mask_layer_eval = static_cast<MaskLayer *>(mask_eval->masklayers.first);
       mask_layer_orig != nullptr;
       mask_layer_orig = mask_layer_orig->next, mask_layer_eval = mask_layer_eval->next)
  {
    if (mask_layer_orig->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }
    for (MaskSpline *spline_orig = static_cast<MaskSpline *>(mask_layer_orig->splines.first),
                    *spline_eval = static_cast<MaskSpline *>(mask_layer_eval->splines.first);
         spline_orig != nullptr;
         spline_orig = spline_orig->next, spline_eval = spline_eval->next)
    {

      MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline_eval);

      for (int i = 0; i < spline_orig->tot_point; i++) {
        MaskSplinePoint *point = &spline_orig->points[i];
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
    ED_mask_select_flush_all(mask_orig);

    DEG_id_tag_update(&mask_orig->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask_orig);

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
  ot->poll = ED_maskedit_mask_visible_splines_poll;
  ot->get_name = ED_select_circle_get_name;

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
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *mask_layer;
  MaskSpline *spline;
  MaskSplinePoint *point = nullptr;
  float co[2];
  bool do_select = !RNA_boolean_get(op->ptr, "deselect");
  const float threshold = 19;
  bool changed = false;

  ED_mask_mouse_pos(area, region, event->mval, co);

  point = ED_mask_point_find_nearest(
      C, mask, co, threshold, &mask_layer, &spline, nullptr, nullptr);

  if (point) {
    ED_mask_spline_select_set(spline, do_select);
    mask_layer->act_spline = spline;
    mask_layer->act_point = point;

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
  ot->poll = ED_maskedit_mask_visible_splines_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static int mask_select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  Mask *mask = CTX_data_edit_mask(C);

  bool changed = false;

  /* do actual selection */
  LISTBASE_FOREACH (MaskLayer *, mask_layer, &mask->masklayers) {
    if (mask_layer->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }

    LISTBASE_FOREACH (MaskSpline *, spline, &mask_layer->splines) {
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
  ot->poll = ED_maskedit_mask_visible_splines_poll;

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

  LISTBASE_FOREACH (MaskLayer *, mask_layer, &mask->masklayers) {
    if (mask_layer->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }

    LISTBASE_FOREACH (MaskSpline *, spline, &mask_layer->splines) {
      const bool cyclic = (spline->flag & MASK_SPLINE_CYCLIC) != 0;
      bool start_sel, end_sel, prev_sel, cur_sel;

      /* Re-select point if any handle is selected to make the result more predictable. */
      for (int i = 0; i < spline->tot_point; i++) {
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

      for (int i = 0; i < spline->tot_point; i++) {
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

      for (int i = spline->tot_point - 1; i >= 0; i--) {
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

static int mask_select_more_exec(bContext *C, wmOperator * /*op*/)
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
  ot->poll = ED_maskedit_mask_visible_splines_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int mask_select_less_exec(bContext *C, wmOperator * /*op*/)
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
  ot->poll = ED_maskedit_mask_visible_splines_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
