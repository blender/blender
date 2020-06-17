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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edcurve
 */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_heap_simple.h"
#include "BLI_kdtree.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_layer.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_curve.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_types.h"
#include "ED_view3d.h"

#include "curve_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph.h"

/* returns 1 in case (de)selection was successful */
bool select_beztriple(BezTriple *bezt, bool selstatus, short flag, eVisible_Types hidden)
{
  if ((bezt->hide == 0) || (hidden == HIDDEN)) {
    if (selstatus == SELECT) { /* selects */
      bezt->f1 |= flag;
      bezt->f2 |= flag;
      bezt->f3 |= flag;
      return true;
    }
    else { /* deselects */
      bezt->f1 &= ~flag;
      bezt->f2 &= ~flag;
      bezt->f3 &= ~flag;
      return true;
    }
  }

  return false;
}

/* returns 1 in case (de)selection was successful */
bool select_bpoint(BPoint *bp, bool selstatus, short flag, bool hidden)
{
  if ((bp->hide == 0) || (hidden == 1)) {
    if (selstatus == SELECT) {
      bp->f1 |= flag;
      return true;
    }
    else {
      bp->f1 &= ~flag;
      return true;
    }
  }

  return false;
}

static bool swap_selection_beztriple(BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    return select_beztriple(bezt, DESELECT, SELECT, VISIBLE);
  }
  else {
    return select_beztriple(bezt, SELECT, SELECT, VISIBLE);
  }
}

static bool swap_selection_bpoint(BPoint *bp)
{
  if (bp->f1 & SELECT) {
    return select_bpoint(bp, DESELECT, SELECT, VISIBLE);
  }
  else {
    return select_bpoint(bp, SELECT, SELECT, VISIBLE);
  }
}

bool ED_curve_nurb_select_check(View3D *v3d, Nurb *nu)
{
  if (nu->type == CU_BEZIER) {
    BezTriple *bezt;
    int i;

    for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
      if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
        return true;
      }
    }
  }
  else {
    BPoint *bp;
    int i;

    for (i = nu->pntsu * nu->pntsv, bp = nu->bp; i--; bp++) {
      if (bp->f1 & SELECT) {
        return true;
      }
    }
  }
  return false;
}

int ED_curve_nurb_select_count(View3D *v3d, Nurb *nu)
{
  int sel = 0;

  if (nu->type == CU_BEZIER) {
    BezTriple *bezt;
    int i;

    for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
      if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
        sel++;
      }
    }
  }
  else {
    BPoint *bp;
    int i;

    for (i = nu->pntsu * nu->pntsv, bp = nu->bp; i--; bp++) {
      if (bp->f1 & SELECT) {
        sel++;
      }
    }
  }

  return sel;
}

bool ED_curve_nurb_select_all(const Nurb *nu)
{
  bool changed = false;
  int i;
  if (nu->bezt) {
    BezTriple *bezt;
    for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
      if (bezt->hide == 0) {
        if (BEZT_ISSEL_ALL(bezt) == false) {
          BEZT_SEL_ALL(bezt);
          changed = true;
        }
      }
    }
  }
  else if (nu->bp) {
    BPoint *bp;
    for (i = nu->pntsu * nu->pntsv, bp = nu->bp; i--; bp++) {
      if (bp->hide == 0) {
        if ((bp->f1 & SELECT) == 0) {
          bp->f1 |= SELECT;
          changed = true;
        }
      }
    }
  }
  return changed;
}

bool ED_curve_select_all(EditNurb *editnurb)
{
  bool changed = false;
  LISTBASE_FOREACH (Nurb *, nu, &editnurb->nurbs) {
    changed |= ED_curve_nurb_select_all(nu);
  }
  return changed;
}

bool ED_curve_nurb_deselect_all(const Nurb *nu)
{
  bool changed = false;
  int i;
  if (nu->bezt) {
    BezTriple *bezt;
    for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
      if (BEZT_ISSEL_ANY(bezt)) {
        BEZT_DESEL_ALL(bezt);
        changed = true;
      }
    }
  }
  else if (nu->bp) {
    BPoint *bp;
    for (i = nu->pntsu * nu->pntsv, bp = nu->bp; i--; bp++) {
      if (bp->f1 & SELECT) {
        bp->f1 &= ~SELECT;
        changed = true;
      }
    }
  }
  return changed;
}

int ED_curve_select_count(View3D *v3d, struct EditNurb *editnurb)
{
  int sel = 0;
  Nurb *nu;

  for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
    sel += ED_curve_nurb_select_count(v3d, nu);
  }

  return sel;
}

bool ED_curve_select_check(View3D *v3d, struct EditNurb *editnurb)
{
  Nurb *nu;

  for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
    if (ED_curve_nurb_select_check(v3d, nu)) {
      return true;
    }
  }

  return false;
}

bool ED_curve_deselect_all(EditNurb *editnurb)
{
  bool changed = false;
  LISTBASE_FOREACH (Nurb *, nu, &editnurb->nurbs) {
    changed |= ED_curve_nurb_deselect_all(nu);
  }
  return changed;
}

bool ED_curve_deselect_all_multi_ex(Base **bases, int bases_len)
{
  bool changed_multi = false;
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Object *obedit = bases[base_index]->object;
    Curve *cu = obedit->data;
    changed_multi |= ED_curve_deselect_all(cu->editnurb);
    DEG_id_tag_update(&cu->id, ID_RECALC_SELECT);
  }
  return changed_multi;
}

bool ED_curve_deselect_all_multi(struct bContext *C)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  uint bases_len = 0;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc.view_layer, vc.v3d, &bases_len);
  bool changed_multi = ED_curve_deselect_all_multi_ex(bases, bases_len);
  MEM_freeN(bases);
  return changed_multi;
}

bool ED_curve_select_swap(EditNurb *editnurb, bool hide_handles)
{
  Nurb *nu;
  BPoint *bp;
  BezTriple *bezt;
  int a;
  bool changed = false;

  for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
    if (nu->type == CU_BEZIER) {
      bezt = nu->bezt;
      a = nu->pntsu;
      while (a--) {
        if (bezt->hide == 0) {
          bezt->f2 ^= SELECT; /* always do the center point */
          if (!hide_handles) {
            bezt->f1 ^= SELECT;
            bezt->f3 ^= SELECT;
          }
          changed = true;
        }
        bezt++;
      }
    }
    else {
      bp = nu->bp;
      a = nu->pntsu * nu->pntsv;
      while (a--) {
        if (bp->hide == 0) {
          swap_selection_bpoint(bp);
          changed = true;
        }
        bp++;
      }
    }
  }
  return changed;
}

/**
 * \param next: -1/1 for prev/next
 * \param cont: when true select continuously
 * \param selstatus: inverts behavior
 */
static void select_adjacent_cp(ListBase *editnurb,
                               short next,
                               const bool cont,
                               const bool selstatus)
{
  Nurb *nu;
  BezTriple *bezt;
  BPoint *bp;
  int a;
  bool lastsel = false;

  if (next == 0) {
    return;
  }

  for (nu = editnurb->first; nu; nu = nu->next) {
    lastsel = false;
    if (nu->type == CU_BEZIER) {
      a = nu->pntsu;
      bezt = nu->bezt;
      if (next < 0) {
        bezt = &nu->bezt[a - 1];
      }
      while (a--) {
        if (a - abs(next) < 0) {
          break;
        }
        if ((lastsel == false) && (bezt->hide == 0) &&
            ((bezt->f2 & SELECT) || (selstatus == DESELECT))) {
          bezt += next;
          if (!(bezt->f2 & SELECT) || (selstatus == DESELECT)) {
            bool sel = select_beztriple(bezt, selstatus, SELECT, VISIBLE);
            if (sel && !cont) {
              lastsel = true;
            }
          }
        }
        else {
          bezt += next;
          lastsel = false;
        }
        /* move around in zigzag way so that we go through each */
        bezt -= (next - next / abs(next));
      }
    }
    else {
      a = nu->pntsu * nu->pntsv;
      bp = nu->bp;
      if (next < 0) {
        bp = &nu->bp[a - 1];
      }
      while (a--) {
        if (a - abs(next) < 0) {
          break;
        }
        if ((lastsel == false) && (bp->hide == 0) &&
            ((bp->f1 & SELECT) || (selstatus == DESELECT))) {
          bp += next;
          if (!(bp->f1 & SELECT) || (selstatus == DESELECT)) {
            bool sel = select_bpoint(bp, selstatus, SELECT, VISIBLE);
            if (sel && !cont) {
              lastsel = true;
            }
          }
        }
        else {
          bp += next;
          lastsel = false;
        }
        /* move around in zigzag way so that we go through each */
        bp -= (next - next / abs(next));
      }
    }
  }
}

/**************** select start/end operators **************/

/* (de)selects first or last of visible part of each Nurb depending on selFirst
 * selFirst: defines the end of which to select
 * doswap: defines if selection state of each first/last control point is swapped
 * selstatus: selection status in case doswap is false
 */
static void selectend_nurb(Object *obedit, eEndPoint_Types selfirst, bool doswap, bool selstatus)
{
  ListBase *editnurb = object_editcurve_get(obedit);
  Nurb *nu;
  BPoint *bp;
  BezTriple *bezt;
  Curve *cu;
  int a;

  if (obedit == NULL) {
    return;
  }

  cu = (Curve *)obedit->data;
  cu->actvert = CU_ACT_NONE;

  for (nu = editnurb->first; nu; nu = nu->next) {
    if (nu->type == CU_BEZIER) {
      a = nu->pntsu;

      /* which point? */
      if (selfirst == LAST) { /* select last */
        bezt = &nu->bezt[a - 1];
      }
      else { /* select first */
        bezt = nu->bezt;
      }

      while (a--) {
        bool sel;
        if (doswap) {
          sel = swap_selection_beztriple(bezt);
        }
        else {
          sel = select_beztriple(bezt, selstatus, SELECT, VISIBLE);
        }

        if (sel == true) {
          break;
        }
      }
    }
    else {
      a = nu->pntsu * nu->pntsv;

      /* which point? */
      if (selfirst == LAST) { /* select last */
        bp = &nu->bp[a - 1];
      }
      else { /* select first */
        bp = nu->bp;
      }

      while (a--) {
        if (bp->hide == 0) {
          bool sel;
          if (doswap) {
            sel = swap_selection_bpoint(bp);
          }
          else {
            sel = select_bpoint(bp, selstatus, SELECT, VISIBLE);
          }

          if (sel == true) {
            break;
          }
        }
      }
    }
  }
}

static int de_select_first_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    selectend_nurb(obedit, FIRST, true, DESELECT);
    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    BKE_curve_nurb_vert_active_validate(obedit->data);
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void CURVE_OT_de_select_first(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select First";
  ot->idname = "CURVE_OT_de_select_first";
  ot->description = "(De)select first of visible part of each NURBS";

  /* api cfirstbacks */
  ot->exec = de_select_first_exec;
  ot->poll = ED_operator_editcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int de_select_last_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    selectend_nurb(obedit, LAST, true, DESELECT);
    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    BKE_curve_nurb_vert_active_validate(obedit->data);
  }

  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void CURVE_OT_de_select_last(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select Last";
  ot->idname = "CURVE_OT_de_select_last";
  ot->description = "(De)select last of visible part of each NURBS";

  /* api clastbacks */
  ot->exec = de_select_last_exec;
  ot->poll = ED_operator_editcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int de_select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");

  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
      Object *obedit = objects[ob_index];
      Curve *cu = obedit->data;

      if (ED_curve_select_check(v3d, cu->editnurb)) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    Curve *cu = obedit->data;
    bool changed = false;

    switch (action) {
      case SEL_SELECT:
        changed = ED_curve_select_all(cu->editnurb);
        break;
      case SEL_DESELECT:
        changed = ED_curve_deselect_all(cu->editnurb);
        break;
      case SEL_INVERT:
        changed = ED_curve_select_swap(cu->editnurb,
                                       v3d->overlay.handle_display == CURVE_HANDLE_NONE);
        break;
    }

    if (changed) {
      DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
      BKE_curve_nurb_vert_active_validate(cu);
    }
  }

  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void CURVE_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->idname = "CURVE_OT_select_all";
  ot->description = "(De)select all control points";

  /* api callbacks */
  ot->exec = de_select_all_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_all(ot);
}

/***************** select linked operator ******************/

static int select_linked_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    Curve *cu = obedit->data;
    EditNurb *editnurb = cu->editnurb;
    ListBase *nurbs = &editnurb->nurbs;
    Nurb *nu;
    bool changed = false;

    for (nu = nurbs->first; nu; nu = nu->next) {
      if (ED_curve_nurb_select_check(v3d, nu)) {
        changed |= ED_curve_nurb_select_all(nu);
      }
    }

    if (changed) {
      DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static int select_linked_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  return select_linked_exec(C, op);
}

void CURVE_OT_select_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked All";
  ot->idname = "CURVE_OT_select_linked";
  ot->description = "Select all control points linked to the current selection";

  /* api callbacks */
  ot->exec = select_linked_exec;
  ot->invoke = select_linked_invoke;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
}

/***************** select linked pick operator ******************/

static int select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  Nurb *nu;
  BezTriple *bezt;
  BPoint *bp;
  int a;
  const bool select = !RNA_boolean_get(op->ptr, "deselect");
  Base *basact = NULL;

  view3d_operator_needs_opengl(C);
  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  copy_v2_v2_int(vc.mval, event->mval);

  if (!ED_curve_pick_vert(&vc, 1, &nu, &bezt, &bp, NULL, &basact)) {
    return OPERATOR_CANCELLED;
  }

  if (bezt) {
    a = nu->pntsu;
    bezt = nu->bezt;
    while (a--) {
      select_beztriple(bezt, select, SELECT, VISIBLE);
      bezt++;
    }
  }
  else if (bp) {
    a = nu->pntsu * nu->pntsv;
    bp = nu->bp;
    while (a--) {
      select_bpoint(bp, select, SELECT, VISIBLE);
      bp++;
    }
  }

  Object *obedit = basact->object;

  DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

  if (!select) {
    BKE_curve_nurb_vert_active_validate(obedit->data);
  }

  return OPERATOR_FINISHED;
}

void CURVE_OT_select_linked_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked";
  ot->idname = "CURVE_OT_select_linked_pick";
  ot->description = "Select all control points linked to already selected ones";

  /* api callbacks */
  ot->invoke = select_linked_pick_invoke;
  ot->poll = ED_operator_editsurfcurve_region_view3d;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "deselect",
                  0,
                  "Deselect",
                  "Deselect linked control points rather than selecting them");
}

/***************** select row operator **********************/

static int select_row_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *obedit = CTX_data_edit_object(C);
  Curve *cu = obedit->data;
  ListBase *editnurb = object_editcurve_get(obedit);
  static BPoint *last = NULL;
  static int direction = 0;
  Nurb *nu = NULL;
  BPoint *bp = NULL;
  int u = 0, v = 0, a, b;

  if (!BKE_curve_nurb_vert_active_get(cu, &nu, (void *)&bp)) {
    return OPERATOR_CANCELLED;
  }

  if (last == bp) {
    direction = 1 - direction;
    BKE_nurbList_flag_set(editnurb, SELECT, false);
  }
  last = bp;

  u = cu->actvert % nu->pntsu;
  v = cu->actvert / nu->pntsu;
  bp = nu->bp;
  for (a = 0; a < nu->pntsv; a++) {
    for (b = 0; b < nu->pntsu; b++, bp++) {
      if (direction) {
        if (a == v) {
          select_bpoint(bp, SELECT, SELECT, VISIBLE);
        }
      }
      else {
        if (b == u) {
          select_bpoint(bp, SELECT, SELECT, VISIBLE);
        }
      }
    }
  }

  DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

  return OPERATOR_FINISHED;
}

void CURVE_OT_select_row(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Control Point Row";
  ot->idname = "CURVE_OT_select_row";
  ot->description = "Select a row of control points including active one";

  /* api callbacks */
  ot->exec = select_row_exec;
  ot->poll = ED_operator_editsurf;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** select next operator **********************/

static int select_next_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];

    ListBase *editnurb = object_editcurve_get(obedit);
    select_adjacent_cp(editnurb, 1, 0, SELECT);

    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void CURVE_OT_select_next(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Next";
  ot->idname = "CURVE_OT_select_next";
  ot->description = "Select control points following already selected ones along the curves";

  /* api callbacks */
  ot->exec = select_next_exec;
  ot->poll = ED_operator_editcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** select previous operator **********************/

static int select_previous_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];

    ListBase *editnurb = object_editcurve_get(obedit);
    select_adjacent_cp(editnurb, -1, 0, SELECT);

    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void CURVE_OT_select_previous(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Previous";
  ot->idname = "CURVE_OT_select_previous";
  ot->description = "Select control points preceding already selected ones along the curves";

  /* api callbacks */
  ot->exec = select_previous_exec;
  ot->poll = ED_operator_editcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** select more operator **********************/

static void curve_select_more(Object *obedit)
{
  ListBase *editnurb = object_editcurve_get(obedit);
  Nurb *nu;
  BPoint *bp, *tempbp;
  int a;
  short sel = 0;

  /* note that NURBS surface is a special case because we mimic */
  /* the behavior of "select more" of mesh tools.       */
  /* The algorithm is designed to work in planar cases so it    */
  /* may not be optimal always (example: end of NURBS sphere)   */
  if (obedit->type == OB_SURF) {
    for (nu = editnurb->first; nu; nu = nu->next) {
      BLI_bitmap *selbpoints;
      a = nu->pntsu * nu->pntsv;
      bp = nu->bp;
      selbpoints = BLI_BITMAP_NEW(a, "selectlist");
      while (a > 0) {
        if ((!BLI_BITMAP_TEST(selbpoints, a)) && (bp->hide == 0) && (bp->f1 & SELECT)) {
          /* upper control point */
          if (a % nu->pntsu != 0) {
            tempbp = bp - 1;
            if (!(tempbp->f1 & SELECT)) {
              select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
            }
          }

          /* left control point. select only if it is not selected already */
          if (a - nu->pntsu > 0) {
            sel = 0;
            tempbp = bp + nu->pntsu;
            if (!(tempbp->f1 & SELECT)) {
              sel = select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
            }
            /* make sure selected bpoint is discarded */
            if (sel == 1) {
              BLI_BITMAP_ENABLE(selbpoints, a - nu->pntsu);
            }
          }

          /* right control point */
          if (a + nu->pntsu < nu->pntsu * nu->pntsv) {
            tempbp = bp - nu->pntsu;
            if (!(tempbp->f1 & SELECT)) {
              select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
            }
          }

          /* lower control point. skip next bp in case selection was made */
          if (a % nu->pntsu != 1) {
            sel = 0;
            tempbp = bp + 1;
            if (!(tempbp->f1 & SELECT)) {
              sel = select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
            }
            if (sel) {
              bp++;
              a--;
            }
          }
        }

        bp++;
        a--;
      }

      MEM_freeN(selbpoints);
    }
  }
  else {
    select_adjacent_cp(editnurb, 1, 0, SELECT);
    select_adjacent_cp(editnurb, -1, 0, SELECT);
  }
}

static int curve_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    curve_select_more(obedit);
    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void CURVE_OT_select_more(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select More";
  ot->idname = "CURVE_OT_select_more";
  ot->description = "Select control points at the boundary of each selection region";

  /* api callbacks */
  ot->exec = curve_select_more_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** select less operator *****************/

/* basic method: deselect if control point doesn't have all neighbors selected */
static void curve_select_less(Object *obedit)
{
  ListBase *editnurb = object_editcurve_get(obedit);
  Nurb *nu;
  BPoint *bp;
  BezTriple *bezt;
  int a;
  int sel = 0;
  bool lastsel = false;

  if (obedit->type == OB_SURF) {
    for (nu = editnurb->first; nu; nu = nu->next) {
      BLI_bitmap *selbpoints;
      a = nu->pntsu * nu->pntsv;
      bp = nu->bp;
      selbpoints = BLI_BITMAP_NEW(a, "selectlist");
      while (a--) {
        if ((bp->hide == 0) && (bp->f1 & SELECT)) {
          sel = 0;

          /* check if neighbors have been selected */
          /* edges of surface are an exception */
          if ((a + 1) % nu->pntsu == 0) {
            sel++;
          }
          else {
            bp--;
            if (BLI_BITMAP_TEST(selbpoints, a + 1) || ((bp->hide == 0) && (bp->f1 & SELECT))) {
              sel++;
            }
            bp++;
          }

          if ((a + 1) % nu->pntsu == 1) {
            sel++;
          }
          else {
            bp++;
            if ((bp->hide == 0) && (bp->f1 & SELECT)) {
              sel++;
            }
            bp--;
          }

          if (a + 1 > nu->pntsu * nu->pntsv - nu->pntsu) {
            sel++;
          }
          else {
            bp -= nu->pntsu;
            if (BLI_BITMAP_TEST(selbpoints, a + nu->pntsu) ||
                ((bp->hide == 0) && (bp->f1 & SELECT))) {
              sel++;
            }
            bp += nu->pntsu;
          }

          if (a < nu->pntsu) {
            sel++;
          }
          else {
            bp += nu->pntsu;
            if ((bp->hide == 0) && (bp->f1 & SELECT)) {
              sel++;
            }
            bp -= nu->pntsu;
          }

          if (sel != 4) {
            select_bpoint(bp, DESELECT, SELECT, VISIBLE);
            BLI_BITMAP_ENABLE(selbpoints, a);
          }
        }
        else {
          lastsel = false;
        }

        bp++;
      }

      MEM_freeN(selbpoints);
    }
  }
  else {
    for (nu = editnurb->first; nu; nu = nu->next) {
      lastsel = false;
      /* check what type of curve/nurb it is */
      if (nu->type == CU_BEZIER) {
        a = nu->pntsu;
        bezt = nu->bezt;
        while (a--) {
          if ((bezt->hide == 0) && (bezt->f2 & SELECT)) {
            sel = (lastsel == 1);

            /* check if neighbors have been selected */
            /* first and last are exceptions */
            if (a == nu->pntsu - 1) {
              sel++;
            }
            else {
              bezt--;
              if ((bezt->hide == 0) && (bezt->f2 & SELECT)) {
                sel++;
              }
              bezt++;
            }

            if (a == 0) {
              sel++;
            }
            else {
              bezt++;
              if ((bezt->hide == 0) && (bezt->f2 & SELECT)) {
                sel++;
              }
              bezt--;
            }

            if (sel != 2) {
              select_beztriple(bezt, DESELECT, SELECT, VISIBLE);
              lastsel = true;
            }
            else {
              lastsel = false;
            }
          }
          else {
            lastsel = false;
          }

          bezt++;
        }
      }
      else {
        a = nu->pntsu * nu->pntsv;
        bp = nu->bp;
        while (a--) {
          if ((lastsel == false) && (bp->hide == 0) && (bp->f1 & SELECT)) {
            sel = 0;

            /* first and last are exceptions */
            if (a == nu->pntsu * nu->pntsv - 1) {
              sel++;
            }
            else {
              bp--;
              if ((bp->hide == 0) && (bp->f1 & SELECT)) {
                sel++;
              }
              bp++;
            }

            if (a == 0) {
              sel++;
            }
            else {
              bp++;
              if ((bp->hide == 0) && (bp->f1 & SELECT)) {
                sel++;
              }
              bp--;
            }

            if (sel != 2) {
              select_bpoint(bp, DESELECT, SELECT, VISIBLE);
              lastsel = true;
            }
            else {
              lastsel = false;
            }
          }
          else {
            lastsel = false;
          }

          bp++;
        }
      }
    }
  }
}

static int curve_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    curve_select_less(obedit);
    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void CURVE_OT_select_less(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Less";
  ot->idname = "CURVE_OT_select_less";
  ot->description = "Deselect control points at the boundary of each selection region";

  /* api callbacks */
  ot->exec = curve_select_less_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** select random *********************/

static void curve_select_random(ListBase *editnurb, float randfac, int seed, bool select)
{
  Nurb *nu;
  BezTriple *bezt;
  BPoint *bp;
  int a;

  RNG *rng = BLI_rng_new_srandom(seed);

  for (nu = editnurb->first; nu; nu = nu->next) {
    if (nu->type == CU_BEZIER) {
      bezt = nu->bezt;
      a = nu->pntsu;
      while (a--) {
        if (!bezt->hide) {
          if (BLI_rng_get_float(rng) < randfac) {
            select_beztriple(bezt, select, SELECT, VISIBLE);
          }
        }
        bezt++;
      }
    }
    else {
      bp = nu->bp;
      a = nu->pntsu * nu->pntsv;

      while (a--) {
        if (!bp->hide) {
          if (BLI_rng_get_float(rng) < randfac) {
            select_bpoint(bp, select, SELECT, VISIBLE);
          }
        }
        bp++;
      }
    }
  }

  BLI_rng_free(rng);
}

static int curve_select_random_exec(bContext *C, wmOperator *op)
{
  const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);
  const float randfac = RNA_float_get(op->ptr, "percent") / 100.0f;
  const int seed = WM_operator_properties_select_random_seed_increment_get(op);

  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    ListBase *editnurb = object_editcurve_get(obedit);
    int seed_iter = seed;

    /* This gives a consistent result regardless of object order. */
    if (ob_index) {
      seed_iter += BLI_ghashutil_strhash_p(obedit->id.name);
    }

    curve_select_random(editnurb, randfac, seed_iter, select);
    BKE_curve_nurb_vert_active_validate(obedit->data);

    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void CURVE_OT_select_random(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Random";
  ot->idname = "CURVE_OT_select_random";
  ot->description = "Randomly select some control points";

  /* api callbacks */
  ot->exec = curve_select_random_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_random(ot);
}

/********************* every nth number of point *******************/

static void select_nth_bezt(Nurb *nu, BezTriple *bezt, const struct CheckerIntervalParams *params)
{
  int a, start;

  start = bezt - nu->bezt;
  a = nu->pntsu;
  bezt = &nu->bezt[a - 1];

  while (a--) {
    const int depth = abs(start - a);
    if (!WM_operator_properties_checker_interval_test(params, depth)) {
      select_beztriple(bezt, DESELECT, SELECT, HIDDEN);
    }

    bezt--;
  }
}

static void select_nth_bp(Nurb *nu, BPoint *bp, const struct CheckerIntervalParams *params)
{
  int a, startrow, startpnt;
  int row, pnt;

  startrow = (bp - nu->bp) / nu->pntsu;
  startpnt = (bp - nu->bp) % nu->pntsu;

  a = nu->pntsu * nu->pntsv;
  bp = &nu->bp[a - 1];
  row = nu->pntsv - 1;
  pnt = nu->pntsu - 1;

  while (a--) {
    const int depth = abs(pnt - startpnt) + abs(row - startrow);
    if (!WM_operator_properties_checker_interval_test(params, depth)) {
      select_bpoint(bp, DESELECT, SELECT, HIDDEN);
    }

    pnt--;
    if (pnt < 0) {
      pnt = nu->pntsu - 1;
      row--;
    }

    bp--;
  }
}

static bool ed_curve_select_nth(Curve *cu, const struct CheckerIntervalParams *params)
{
  Nurb *nu = NULL;
  void *vert = NULL;

  if (!BKE_curve_nurb_vert_active_get(cu, &nu, &vert)) {
    return false;
  }

  if (nu->bezt) {
    select_nth_bezt(nu, vert, params);
  }
  else {
    select_nth_bp(nu, vert, params);
  }

  return true;
}

static int select_nth_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obact = CTX_data_edit_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  bool changed = false;

  struct CheckerIntervalParams op_params;
  WM_operator_properties_checker_interval_from_op(op, &op_params);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    Curve *cu = obedit->data;

    if (!ED_curve_select_check(v3d, cu->editnurb)) {
      continue;
    }

    if (ed_curve_select_nth(obedit->data, &op_params) == true) {
      changed = true;

      DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }
  MEM_freeN(objects);

  if (!changed) {
    if (obact->type == OB_SURF) {
      BKE_report(
          op->reports,
          RPT_ERROR,
          (objects_len == 1 ? "Surface has no active point" : "Surfaces have no active point"));
    }
    else {
      BKE_report(op->reports,
                 RPT_ERROR,
                 (objects_len == 1 ? "Curve has no active point" : "Curves have no active point"));
    }
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

void CURVE_OT_select_nth(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Checker Deselect";
  ot->description = "Deselect every Nth point starting from the active one";
  ot->idname = "CURVE_OT_select_nth";

  /* api callbacks */
  ot->exec = select_nth_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_checker_interval(ot, false);
}

/* -------------------------------------------------------------------- */
/* Select Similar */

/** \name Select Similar
 * \{ */

static const EnumPropertyItem curve_prop_similar_compare_types[] = {
    {SIM_CMP_EQ, "EQUAL", 0, "Equal", ""},
    {SIM_CMP_GT, "GREATER", 0, "Greater", ""},
    {SIM_CMP_LT, "LESS", 0, "Less", ""},

    {0, NULL, 0, NULL, NULL},
};

enum {
  SIMCURHAND_TYPE = 0,
  SIMCURHAND_RADIUS,
  SIMCURHAND_WEIGHT,
  SIMCURHAND_DIRECTION,
};

static const EnumPropertyItem curve_prop_similar_types[] = {
    {SIMCURHAND_TYPE, "TYPE", 0, "Type", ""},
    {SIMCURHAND_RADIUS, "RADIUS", 0, "Radius", ""},
    {SIMCURHAND_WEIGHT, "WEIGHT", 0, "Weight", ""},
    {SIMCURHAND_DIRECTION, "DIRECTION", 0, "Direction", ""},
    {0, NULL, 0, NULL, NULL},
};

static void nurb_bezt_direction_worldspace_get(Object *ob,
                                               Nurb *nu,
                                               BezTriple *bezt,
                                               float r_dir[3])
{
  float rsmat[3][3];
  BKE_nurb_bezt_calc_normal(nu, bezt, r_dir);
  copy_m3_m4(rsmat, ob->obmat);
  mul_m3_v3(rsmat, r_dir);
  normalize_v3(r_dir);
}

static void nurb_bpoint_direction_worldspace_get(Object *ob, Nurb *nu, BPoint *bp, float r_dir[3])
{
  float rsmat[3][3];
  BKE_nurb_bpoint_calc_normal(nu, bp, r_dir);
  copy_m3_m4(rsmat, ob->obmat);
  mul_m3_v3(rsmat, r_dir);
  normalize_v3(r_dir);
}

static void curve_nurb_selected_type_get(
    Object *ob, Nurb *nu, const int type, KDTree_1d *tree_1d, KDTree_3d *tree_3d)
{
  float tree_entry[3] = {0.0f, 0.0f, 0.0f};

  if (nu->type == CU_BEZIER) {
    BezTriple *bezt;
    int i;
    int tree_index = 0;

    for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
      if ((!bezt->hide) && (bezt->f1 & SELECT)) {

        switch (type) {
          case SIMCURHAND_RADIUS: {
            float radius_ref = bezt->radius;
            tree_entry[0] = radius_ref;
            break;
          }
          case SIMCURHAND_WEIGHT: {
            float weight_ref = bezt->weight;
            tree_entry[0] = weight_ref;
            break;
          }
          case SIMCURHAND_DIRECTION: {
            nurb_bezt_direction_worldspace_get(ob, nu, bezt, tree_entry);
            break;
          }
        }
        if (tree_1d) {
          BLI_kdtree_1d_insert(tree_1d, tree_index++, tree_entry);
        }
        else {
          BLI_kdtree_3d_insert(tree_3d, tree_index++, tree_entry);
        }
      }
    }
  }
  else {
    BPoint *bp;
    int i;
    int tree_index = 0;

    for (i = nu->pntsu * nu->pntsv, bp = nu->bp; i--; bp++) {
      if (!bp->hide && bp->f1 & SELECT) {
        switch (type) {
          case SIMCURHAND_RADIUS: {
            float radius_ref = bp->radius;
            tree_entry[0] = radius_ref;
            break;
          }
          case SIMCURHAND_WEIGHT: {
            float weight_ref = bp->weight;
            tree_entry[0] = weight_ref;
            break;
          }
          case SIMCURHAND_DIRECTION: {
            nurb_bpoint_direction_worldspace_get(ob, nu, bp, tree_entry);
            break;
          }
        }
        if (tree_1d) {
          BLI_kdtree_1d_insert(tree_1d, tree_index++, tree_entry);
        }
        else {
          BLI_kdtree_3d_insert(tree_3d, tree_index++, tree_entry);
        }
      }
    }
  }
}

static bool curve_nurb_select_similar_type(Object *ob,
                                           Nurb *nu,
                                           const int type,
                                           const KDTree_1d *tree_1d,
                                           const KDTree_3d *tree_3d,
                                           const float thresh,
                                           const int compare)
{
  const float thresh_cos = cosf(thresh * (float)M_PI_2);
  bool changed = false;

  if (nu->type == CU_BEZIER) {
    BezTriple *bezt;
    int i;

    for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
      if (!bezt->hide) {
        bool select = false;

        switch (type) {
          case SIMCURHAND_RADIUS: {
            float radius_ref = bezt->radius;
            if (ED_select_similar_compare_float_tree(tree_1d, radius_ref, thresh, compare)) {
              select = true;
            }
            break;
          }
          case SIMCURHAND_WEIGHT: {
            float weight_ref = bezt->weight;
            if (ED_select_similar_compare_float_tree(tree_1d, weight_ref, thresh, compare)) {
              select = true;
            }
            break;
          }
          case SIMCURHAND_DIRECTION: {
            float dir[3];
            nurb_bezt_direction_worldspace_get(ob, nu, bezt, dir);
            KDTreeNearest_3d nearest;
            if (BLI_kdtree_3d_find_nearest(tree_3d, dir, &nearest) != -1) {
              float orient = angle_normalized_v3v3(dir, nearest.co);
              float delta = thresh_cos - fabsf(cosf(orient));
              if (ED_select_similar_compare_float(delta, thresh, compare)) {
                select = true;
              }
            }
            break;
          }
        }

        if (select) {
          select_beztriple(bezt, SELECT, SELECT, VISIBLE);
          changed = true;
        }
      }
    }
  }
  else {
    BPoint *bp;
    int i;

    for (i = nu->pntsu * nu->pntsv, bp = nu->bp; i--; bp++) {
      if (!bp->hide) {
        bool select = false;

        switch (type) {
          case SIMCURHAND_RADIUS: {
            float radius_ref = bp->radius;
            if (ED_select_similar_compare_float_tree(tree_1d, radius_ref, thresh, compare)) {
              select = true;
            }
            break;
          }
          case SIMCURHAND_WEIGHT: {
            float weight_ref = bp->weight;
            if (ED_select_similar_compare_float_tree(tree_1d, weight_ref, thresh, compare)) {
              select = true;
            }
            break;
          }
          case SIMCURHAND_DIRECTION: {
            float dir[3];
            nurb_bpoint_direction_worldspace_get(ob, nu, bp, dir);
            KDTreeNearest_3d nearest;
            if (BLI_kdtree_3d_find_nearest(tree_3d, dir, &nearest) != -1) {
              float orient = angle_normalized_v3v3(dir, nearest.co);
              float delta = fabsf(cosf(orient)) - thresh_cos;
              if (ED_select_similar_compare_float(delta, thresh, compare)) {
                select = true;
              }
            }
            break;
          }
        }

        if (select) {
          select_bpoint(bp, SELECT, SELECT, VISIBLE);
          changed = true;
        }
      }
    }
  }
  return changed;
}

static int curve_select_similar_exec(bContext *C, wmOperator *op)
{
  /* Get props. */
  const int optype = RNA_enum_get(op->ptr, "type");
  const float thresh = RNA_float_get(op->ptr, "threshold");
  const int compare = RNA_enum_get(op->ptr, "compare");

  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  int tot_nurbs_selected_all = 0;
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    Curve *cu = obedit->data;
    tot_nurbs_selected_all += ED_curve_select_count(v3d, cu->editnurb);
  }

  if (tot_nurbs_selected_all == 0) {
    BKE_report(op->reports, RPT_ERROR, "No control point selected");
    MEM_freeN(objects);
    return OPERATOR_CANCELLED;
  }

  KDTree_1d *tree_1d = NULL;
  KDTree_3d *tree_3d = NULL;
  short type_ref = 0;

  switch (optype) {
    case SIMCURHAND_RADIUS:
    case SIMCURHAND_WEIGHT:
      tree_1d = BLI_kdtree_1d_new(tot_nurbs_selected_all);
      break;
    case SIMCURHAND_DIRECTION:
      tree_3d = BLI_kdtree_3d_new(tot_nurbs_selected_all);
      break;
  }

  /* Get type of selected control points. */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    Curve *cu = obedit->data;
    EditNurb *editnurb = cu->editnurb;

    Nurb *nu;
    for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
      if (!ED_curve_nurb_select_check(v3d, nu)) {
        continue;
      }
      switch (optype) {
        case SIMCURHAND_TYPE: {
          type_ref |= nu->type;
          break;
        }
        case SIMCURHAND_RADIUS:
        case SIMCURHAND_WEIGHT:
        case SIMCURHAND_DIRECTION:
          curve_nurb_selected_type_get(obedit, nu, optype, tree_1d, tree_3d);
          break;
      }
    }
  }

  if (tree_1d != NULL) {
    BLI_kdtree_1d_deduplicate(tree_1d);
    BLI_kdtree_1d_balance(tree_1d);
  }
  if (tree_3d != NULL) {
    BLI_kdtree_3d_deduplicate(tree_3d);
    BLI_kdtree_3d_balance(tree_3d);
  }

  /* Select control points with desired type. */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    Curve *cu = obedit->data;
    EditNurb *editnurb = cu->editnurb;
    bool changed = false;
    Nurb *nu;

    for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
      switch (optype) {
        case SIMCURHAND_TYPE: {
          if (nu->type & type_ref) {
            changed |= ED_curve_nurb_select_all(nu);
          }
          break;
        }
        case SIMCURHAND_RADIUS:
        case SIMCURHAND_WEIGHT:
        case SIMCURHAND_DIRECTION:
          changed = curve_nurb_select_similar_type(
              obedit, nu, optype, tree_1d, tree_3d, thresh, compare);
          break;
      }
    }

    if (changed) {
      DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }

  MEM_freeN(objects);

  if (tree_1d != NULL) {
    BLI_kdtree_1d_free(tree_1d);
  }
  if (tree_3d != NULL) {
    BLI_kdtree_3d_free(tree_3d);
  }
  return OPERATOR_FINISHED;
}

void CURVE_OT_select_similar(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Similar";
  ot->idname = "CURVE_OT_select_similar";
  ot->description = "Select similar curve points by property type";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = curve_select_similar_exec;
  ot->poll = ED_operator_editsurfcurve;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", curve_prop_similar_types, SIMCURHAND_WEIGHT, "Type", "");
  RNA_def_enum(ot->srna, "compare", curve_prop_similar_compare_types, SIM_CMP_EQ, "Compare", "");
  RNA_def_float(ot->srna, "threshold", 0.1, 0.0, FLT_MAX, "Threshold", "", 0.0, 100.0);
}

/** \} */

/* -------------------------------------------------------------------- */
/* Select Shortest Path */

/** \name Select Path
 * \{ */

static float curve_calc_dist_pair(const Nurb *nu, int a, int b)
{
  const float *a_fl, *b_fl;

  if (nu->type == CU_BEZIER) {
    a_fl = nu->bezt[a].vec[1];
    b_fl = nu->bezt[b].vec[1];
  }
  else {
    a_fl = nu->bp[a].vec;
    b_fl = nu->bp[b].vec;
  }

  return len_v3v3(a_fl, b_fl);
}

static float curve_calc_dist_span(Nurb *nu, int vert_src, int vert_dst)
{
  const int u = nu->pntsu;
  int i_prev, i;
  float dist = 0.0f;

  BLI_assert(nu->pntsv <= 1);

  i_prev = vert_src;
  i = (i_prev + 1) % u;

  while (true) {
    dist += curve_calc_dist_pair(nu, i_prev, i);

    if (i == vert_dst) {
      break;
    }
    i = (i + 1) % u;
  }
  return dist;
}

static void curve_select_shortest_path_curve(Nurb *nu, int vert_src, int vert_dst)
{
  const int u = nu->pntsu;
  int i;

  if (vert_src > vert_dst) {
    SWAP(int, vert_src, vert_dst);
  }

  if (nu->flagu & CU_NURB_CYCLIC) {
    if (curve_calc_dist_span(nu, vert_src, vert_dst) >
        curve_calc_dist_span(nu, vert_dst, vert_src)) {
      SWAP(int, vert_src, vert_dst);
    }
  }

  i = vert_src;
  while (true) {
    if (nu->type & CU_BEZIER) {
      select_beztriple(&nu->bezt[i], SELECT, SELECT, HIDDEN);
    }
    else {
      select_bpoint(&nu->bp[i], SELECT, SELECT, HIDDEN);
    }

    if (i == vert_dst) {
      break;
    }
    i = (i + 1) % u;
  }
}

static void curve_select_shortest_path_surf(Nurb *nu, int vert_src, int vert_dst)
{
  HeapSimple *heap;

  int i, vert_curr;

  int totu = nu->pntsu;
  int totv = nu->pntsv;
  int vert_num = totu * totv;

  /* custom data */
  struct PointAdj {
    int vert, vert_prev;
    float cost;
  } * data;

  /* init connectivity data */
  data = MEM_mallocN(sizeof(*data) * vert_num, __func__);
  for (i = 0; i < vert_num; i++) {
    data[i].vert = i;
    data[i].vert_prev = -1;
    data[i].cost = FLT_MAX;
  }

  /* init heap */
  heap = BLI_heapsimple_new();

  vert_curr = data[vert_src].vert;
  BLI_heapsimple_insert(heap, 0.0f, &data[vert_src].vert);
  data[vert_src].cost = 0.0f;
  data[vert_src].vert_prev = vert_src; /* nop */

  while (!BLI_heapsimple_is_empty(heap)) {
    int axis, sign;
    int u, v;

    vert_curr = *((int *)BLI_heapsimple_pop_min(heap));
    if (vert_curr == vert_dst) {
      break;
    }

    BKE_nurb_index_to_uv(nu, vert_curr, &u, &v);

    /* loop over 4 adjacent verts */
    for (sign = -1; sign != 3; sign += 2) {
      for (axis = 0; axis != 2; axis += 1) {
        int uv_other[2] = {u, v};
        int vert_other;

        uv_other[axis] += sign;

        vert_other = BKE_nurb_index_from_uv(nu, uv_other[0], uv_other[1]);
        if (vert_other != -1) {
          const float dist = data[vert_curr].cost +
                             curve_calc_dist_pair(nu, vert_curr, vert_other);

          if (data[vert_other].cost > dist) {
            data[vert_other].cost = dist;
            if (data[vert_other].vert_prev == -1) {
              BLI_heapsimple_insert(heap, data[vert_other].cost, &data[vert_other].vert);
            }
            data[vert_other].vert_prev = vert_curr;
          }
        }
      }
    }
  }

  BLI_heapsimple_free(heap, NULL);

  if (vert_curr == vert_dst) {
    i = 0;
    while (vert_curr != vert_src && i++ < vert_num) {
      if (nu->type == CU_BEZIER) {
        select_beztriple(&nu->bezt[vert_curr], SELECT, SELECT, HIDDEN);
      }
      else {
        select_bpoint(&nu->bp[vert_curr], SELECT, SELECT, HIDDEN);
      }
      vert_curr = data[vert_curr].vert_prev;
    }
  }

  MEM_freeN(data);
}

static int edcu_shortest_path_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  Nurb *nu_dst;
  BezTriple *bezt_dst;
  BPoint *bp_dst;
  int vert_dst;
  void *vert_dst_p;
  Base *basact = NULL;

  view3d_operator_needs_opengl(C);
  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  copy_v2_v2_int(vc.mval, event->mval);

  if (!ED_curve_pick_vert(&vc, 1, &nu_dst, &bezt_dst, &bp_dst, NULL, &basact)) {
    return OPERATOR_PASS_THROUGH;
  }

  ED_view3d_viewcontext_init_object(&vc, basact->object);
  Object *obedit = basact->object;
  Curve *cu = obedit->data;
  Nurb *nu_src = BKE_curve_nurb_active_get(cu);
  int vert_src = cu->actvert;

  if (vert_src == CU_ACT_NONE) {
    return OPERATOR_PASS_THROUGH;
  }

  if (nu_src != nu_dst) {
    BKE_report(op->reports, RPT_ERROR, "Control point belongs to another spline");
    return OPERATOR_CANCELLED;
  }

  vert_dst_p = bezt_dst ? (void *)bezt_dst : (void *)bp_dst;
  vert_dst = BKE_curve_nurb_vert_index_get(nu_dst, vert_dst_p);
  if (vert_src == vert_dst) {
    return OPERATOR_CANCELLED;
  }

  if ((obedit->type == OB_SURF) && (nu_src->pntsv > 1)) {
    curve_select_shortest_path_surf(nu_src, vert_src, vert_dst);
  }
  else {
    curve_select_shortest_path_curve(nu_src, vert_src, vert_dst);
  }

  BKE_curve_nurb_vert_active_set(cu, nu_dst, vert_dst_p);

  if (vc.view_layer->basact != basact) {
    ED_object_base_activate(C, basact);
  }

  DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  return OPERATOR_FINISHED;
}

void CURVE_OT_shortest_path_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pick Shortest Path";
  ot->idname = "CURVE_OT_shortest_path_pick";
  ot->description = "Select shortest path between two selections";

  /* api callbacks */
  ot->invoke = edcu_shortest_path_pick_invoke;
  ot->poll = ED_operator_editsurfcurve_region_view3d;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
