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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 */

#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#include "BKE_context.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "UI_view2d.h"
#include "UI_interface.h"

#include "PIL_time.h" /* USER_ZOOM_CONT */

static bool view2d_poll(bContext *C)
{
  ARegion *ar = CTX_wm_region(C);

  return (ar != NULL) && (ar->v2d.flag & V2D_IS_INITIALISED);
}

/* ********************************************************* */
/* VIEW PANNING OPERATOR                                 */

/**
 * This group of operators come in several forms:
 * -# Modal 'dragging' with MMB - where movement of mouse dictates amount to pan view by
 * -# Scrollwheel 'steps' - rolling mousewheel by one step moves view by predefined amount
 *
 * In order to make sure this works, each operator must define the following RNA-Operator Props:
 * - `deltax, deltay` - define how much to move view by (relative to zoom-correction factor)
 */

/* ------------------ Shared 'core' stuff ---------------------- */

/* temp customdata for operator */
typedef struct v2dViewPanData {
  /** screen where view pan was initiated */
  bScreen *sc;
  /** area where view pan was initiated */
  ScrArea *sa;
  /** region where view pan was initiated */
  ARegion *ar;
  /** view2d we're operating in */
  View2D *v2d;

  /** amount to move view relative to zoom */
  float facx, facy;

  /* options for version 1 */
  /** mouse x/y values in window when operator was initiated */
  int startx, starty;
  /** previous x/y values of mouse in window */
  int lastx, lasty;
  /** event starting pan, for modal exit */
  int invoke_event;

  /** for MMB in scrollers (old feature in past, but now not that useful) */
  short in_scroller;
} v2dViewPanData;

/* initialize panning customdata */
static int view_pan_init(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  v2dViewPanData *vpd;
  View2D *v2d;
  float winx, winy;

  /* regions now have v2d-data by default, so check for region */
  if (ar == NULL) {
    return 0;
  }

  /* check if panning is allowed at all */
  v2d = &ar->v2d;
  if ((v2d->keepofs & V2D_LOCKOFS_X) && (v2d->keepofs & V2D_LOCKOFS_Y)) {
    return 0;
  }

  /* set custom-data for operator */
  vpd = MEM_callocN(sizeof(v2dViewPanData), "v2dViewPanData");
  op->customdata = vpd;

  /* set pointers to owners */
  vpd->sc = CTX_wm_screen(C);
  vpd->sa = CTX_wm_area(C);
  vpd->v2d = v2d;
  vpd->ar = ar;

  /* calculate translation factor - based on size of view */
  winx = (float)(BLI_rcti_size_x(&ar->winrct) + 1);
  winy = (float)(BLI_rcti_size_y(&ar->winrct) + 1);
  vpd->facx = (BLI_rctf_size_x(&v2d->cur)) / winx;
  vpd->facy = (BLI_rctf_size_y(&v2d->cur)) / winy;

  return 1;
}

#ifdef WITH_INPUT_NDOF
static bool view_pan_poll(bContext *C)
{
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d;

  /* check if there's a region in context to work with */
  if (ar == NULL) {
    return 0;
  }
  v2d = &ar->v2d;

  /* check that 2d-view can pan */
  if ((v2d->keepofs & V2D_LOCKOFS_X) && (v2d->keepofs & V2D_LOCKOFS_Y)) {
    return 0;
  }

  /* view can pan */
  return 1;
}
#endif

/* apply transform to view (i.e. adjust 'cur' rect) */
static void view_pan_apply_ex(bContext *C, v2dViewPanData *vpd, float dx, float dy)
{
  View2D *v2d = vpd->v2d;

  /* calculate amount to move view by */
  dx *= vpd->facx;
  dy *= vpd->facy;

  /* only move view on an axis if change is allowed */
  if ((v2d->keepofs & V2D_LOCKOFS_X) == 0) {
    v2d->cur.xmin += dx;
    v2d->cur.xmax += dx;
  }
  if ((v2d->keepofs & V2D_LOCKOFS_Y) == 0) {
    v2d->cur.ymin += dy;
    v2d->cur.ymax += dy;
  }

  /* validate that view is in valid configuration after this operation */
  UI_view2d_curRect_validate(v2d);

  /* don't rebuild full tree in outliner, since we're just changing our view */
  ED_region_tag_redraw_no_rebuild(vpd->ar);

  /* request updates to be done... */
  WM_event_add_mousemove(C);

  UI_view2d_sync(vpd->sc, vpd->sa, v2d, V2D_LOCK_COPY);
}

static void view_pan_apply(bContext *C, wmOperator *op)
{
  v2dViewPanData *vpd = op->customdata;

  view_pan_apply_ex(C, vpd, RNA_int_get(op->ptr, "deltax"), RNA_int_get(op->ptr, "deltay"));
}

/* cleanup temp customdata  */
static void view_pan_exit(wmOperator *op)
{
  if (op->customdata) {
    MEM_freeN(op->customdata);
    op->customdata = NULL;
  }
}

/* ------------------ Modal Drag Version (1) ---------------------- */

/* for 'redo' only, with no user input */
static int view_pan_exec(bContext *C, wmOperator *op)
{
  if (!view_pan_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  view_pan_apply(C, op);
  view_pan_exit(op);
  return OPERATOR_FINISHED;
}

/* set up modal operator and relevant settings */
static int view_pan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *window = CTX_wm_window(C);
  v2dViewPanData *vpd;
  View2D *v2d;

  /* set up customdata */
  if (!view_pan_init(C, op)) {
    return OPERATOR_PASS_THROUGH;
  }

  vpd = op->customdata;
  v2d = vpd->v2d;

  /* set initial settings */
  vpd->startx = vpd->lastx = event->x;
  vpd->starty = vpd->lasty = event->y;
  vpd->invoke_event = event->type;

  if (event->type == MOUSEPAN) {
    RNA_int_set(op->ptr, "deltax", event->prevx - event->x);
    RNA_int_set(op->ptr, "deltay", event->prevy - event->y);

    view_pan_apply(C, op);
    view_pan_exit(op);
    return OPERATOR_FINISHED;
  }

  RNA_int_set(op->ptr, "deltax", 0);
  RNA_int_set(op->ptr, "deltay", 0);

  if (v2d->keepofs & V2D_LOCKOFS_X) {
    WM_cursor_modal_set(window, BC_NS_SCROLLCURSOR);
  }
  else if (v2d->keepofs & V2D_LOCKOFS_Y) {
    WM_cursor_modal_set(window, BC_EW_SCROLLCURSOR);
  }
  else {
    WM_cursor_modal_set(window, BC_NSEW_SCROLLCURSOR);
  }

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* handle user input - calculations of mouse-movement
 * need to be done here, not in the apply callback! */
static int view_pan_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  v2dViewPanData *vpd = op->customdata;

  /* execute the events */
  switch (event->type) {
    case MOUSEMOVE: {
      /* calculate new delta transform, then store mouse-coordinates for next-time */
      RNA_int_set(op->ptr, "deltax", (vpd->lastx - event->x));
      RNA_int_set(op->ptr, "deltay", (vpd->lasty - event->y));

      vpd->lastx = event->x;
      vpd->lasty = event->y;

      view_pan_apply(C, op);
      break;
    }
    /* XXX - Mode switching isn't implemented. See comments in 36818.
     * switch to zoom */
#if 0
    case LEFTMOUSE:
      if (event->val == KM_PRESS) {
        /* calculate overall delta mouse-movement for redo */
        RNA_int_set(op->ptr, "deltax", (vpd->startx - vpd->lastx));
        RNA_int_set(op->ptr, "deltay", (vpd->starty - vpd->lasty));

        view_pan_exit(op);
        WM_cursor_modal_restore(CTX_wm_window(C));
        WM_operator_name_call(C, "VIEW2D_OT_zoom", WM_OP_INVOKE_DEFAULT, NULL);
        return OPERATOR_FINISHED;
      }
#endif
    default:
      if (event->type == vpd->invoke_event || event->type == ESCKEY) {
        if (event->val == KM_RELEASE) {
          /* calculate overall delta mouse-movement for redo */
          RNA_int_set(op->ptr, "deltax", (vpd->startx - vpd->lastx));
          RNA_int_set(op->ptr, "deltay", (vpd->starty - vpd->lasty));

          view_pan_exit(op);
          WM_cursor_modal_restore(CTX_wm_window(C));

          return OPERATOR_FINISHED;
        }
      }
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void view_pan_cancel(bContext *UNUSED(C), wmOperator *op)
{
  view_pan_exit(op);
}

static void VIEW2D_OT_pan(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pan View";
  ot->description = "Pan the view";
  ot->idname = "VIEW2D_OT_pan";

  /* api callbacks */
  ot->exec = view_pan_exec;
  ot->invoke = view_pan_invoke;
  ot->modal = view_pan_modal;
  ot->cancel = view_pan_cancel;

  /* operator is modal */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* rna - must keep these in sync with the other operators */
  RNA_def_int(ot->srna, "deltax", 0, INT_MIN, INT_MAX, "Delta X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "deltay", 0, INT_MIN, INT_MAX, "Delta Y", "", INT_MIN, INT_MAX);
}

/* ------------------ Scrollwheel Versions (2) ---------------------- */

/* this operator only needs this single callback, where it calls the view_pan_*() methods */
static int view_scrollright_exec(bContext *C, wmOperator *op)
{
  v2dViewPanData *vpd;

  /* initialize default settings (and validate if ok to run) */
  if (!view_pan_init(C, op)) {
    return OPERATOR_PASS_THROUGH;
  }

  /* also, check if can pan in horizontal axis */
  vpd = op->customdata;
  if (vpd->v2d->keepofs & V2D_LOCKOFS_X) {
    view_pan_exit(op);
    return OPERATOR_PASS_THROUGH;
  }

  /* set RNA-Props - only movement in positive x-direction */
  RNA_int_set(op->ptr, "deltax", 40);
  RNA_int_set(op->ptr, "deltay", 0);

  /* apply movement, then we're done */
  view_pan_apply(C, op);
  view_pan_exit(op);

  return OPERATOR_FINISHED;
}

static void VIEW2D_OT_scroll_right(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Scroll Right";
  ot->description = "Scroll the view right";
  ot->idname = "VIEW2D_OT_scroll_right";

  /* api callbacks */
  ot->exec = view_scrollright_exec;

  /* rna - must keep these in sync with the other operators */
  RNA_def_int(ot->srna, "deltax", 0, INT_MIN, INT_MAX, "Delta X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "deltay", 0, INT_MIN, INT_MAX, "Delta Y", "", INT_MIN, INT_MAX);
}

/* this operator only needs this single callback, where it calls the view_pan_*() methods */
static int view_scrollleft_exec(bContext *C, wmOperator *op)
{
  v2dViewPanData *vpd;

  /* initialize default settings (and validate if ok to run) */
  if (!view_pan_init(C, op)) {
    return OPERATOR_PASS_THROUGH;
  }

  /* also, check if can pan in horizontal axis */
  vpd = op->customdata;
  if (vpd->v2d->keepofs & V2D_LOCKOFS_X) {
    view_pan_exit(op);
    return OPERATOR_PASS_THROUGH;
  }

  /* set RNA-Props - only movement in negative x-direction */
  RNA_int_set(op->ptr, "deltax", -40);
  RNA_int_set(op->ptr, "deltay", 0);

  /* apply movement, then we're done */
  view_pan_apply(C, op);
  view_pan_exit(op);

  return OPERATOR_FINISHED;
}

static void VIEW2D_OT_scroll_left(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Scroll Left";
  ot->description = "Scroll the view left";
  ot->idname = "VIEW2D_OT_scroll_left";

  /* api callbacks */
  ot->exec = view_scrollleft_exec;

  /* rna - must keep these in sync with the other operators */
  RNA_def_int(ot->srna, "deltax", 0, INT_MIN, INT_MAX, "Delta X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "deltay", 0, INT_MIN, INT_MAX, "Delta Y", "", INT_MIN, INT_MAX);
}

/* this operator only needs this single callback, where it calls the view_pan_*() methods */
static int view_scrolldown_exec(bContext *C, wmOperator *op)
{
  v2dViewPanData *vpd;

  /* initialize default settings (and validate if ok to run) */
  if (!view_pan_init(C, op)) {
    return OPERATOR_PASS_THROUGH;
  }

  /* also, check if can pan in vertical axis */
  vpd = op->customdata;
  if (vpd->v2d->keepofs & V2D_LOCKOFS_Y) {
    view_pan_exit(op);
    return OPERATOR_PASS_THROUGH;
  }

  /* set RNA-Props */
  RNA_int_set(op->ptr, "deltax", 0);
  RNA_int_set(op->ptr, "deltay", -40);

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "page");
  if (RNA_property_is_set(op->ptr, prop) && RNA_property_boolean_get(op->ptr, prop)) {
    ARegion *ar = CTX_wm_region(C);
    RNA_int_set(op->ptr, "deltay", ar->v2d.mask.ymin - ar->v2d.mask.ymax);
  }

  /* apply movement, then we're done */
  view_pan_apply(C, op);
  view_pan_exit(op);

  return OPERATOR_FINISHED;
}

static void VIEW2D_OT_scroll_down(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Scroll Down";
  ot->description = "Scroll the view down";
  ot->idname = "VIEW2D_OT_scroll_down";

  /* api callbacks */
  ot->exec = view_scrolldown_exec;

  /* rna - must keep these in sync with the other operators */
  RNA_def_int(ot->srna, "deltax", 0, INT_MIN, INT_MAX, "Delta X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "deltay", 0, INT_MIN, INT_MAX, "Delta Y", "", INT_MIN, INT_MAX);
  RNA_def_boolean(ot->srna, "page", 0, "Page", "Scroll down one page");
}

/* this operator only needs this single callback, where it calls the view_pan_*() methods */
static int view_scrollup_exec(bContext *C, wmOperator *op)
{
  v2dViewPanData *vpd;

  /* initialize default settings (and validate if ok to run) */
  if (!view_pan_init(C, op)) {
    return OPERATOR_PASS_THROUGH;
  }

  /* also, check if can pan in vertical axis */
  vpd = op->customdata;
  if (vpd->v2d->keepofs & V2D_LOCKOFS_Y) {
    view_pan_exit(op);
    return OPERATOR_PASS_THROUGH;
  }

  /* set RNA-Props */
  RNA_int_set(op->ptr, "deltax", 0);
  RNA_int_set(op->ptr, "deltay", 40);

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "page");
  if (RNA_property_is_set(op->ptr, prop) && RNA_property_boolean_get(op->ptr, prop)) {
    ARegion *ar = CTX_wm_region(C);
    RNA_int_set(op->ptr, "deltay", BLI_rcti_size_y(&ar->v2d.mask));
  }

  /* apply movement, then we're done */
  view_pan_apply(C, op);
  view_pan_exit(op);

  return OPERATOR_FINISHED;
}

static void VIEW2D_OT_scroll_up(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Scroll Up";
  ot->description = "Scroll the view up";
  ot->idname = "VIEW2D_OT_scroll_up";

  /* api callbacks */
  ot->exec = view_scrollup_exec;

  /* rna - must keep these in sync with the other operators */
  RNA_def_int(ot->srna, "deltax", 0, INT_MIN, INT_MAX, "Delta X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "deltay", 0, INT_MIN, INT_MAX, "Delta Y", "", INT_MIN, INT_MAX);
  RNA_def_boolean(ot->srna, "page", 0, "Page", "Scroll up one page");
}

/* ********************************************************* */
/* SINGLE-STEP VIEW ZOOMING OPERATOR                         */

/**
 * This group of operators come in several forms:
 * -# Scrollwheel 'steps' - rolling mousewheel by one step zooms view by predefined amount.
 * -# Scrollwheel 'steps' + alt + ctrl/shift - zooms view on one axis only (ctrl=x, shift=y).
 *    XXX this could be implemented...
 * -# Pad +/- Keys - pressing each key moves the zooms the view by a predefined amount.
 *
 * In order to make sure this works, each operator must define the following RNA-Operator Props:
 *
 * - zoomfacx, zoomfacy - These two zoom factors allow for non-uniform scaling.
 *   It is safe to scale by 0, as these factors are used to determine.
 *   amount to enlarge 'cur' by.
 */

/* ------------------ 'Shared' stuff ------------------------ */

/* temp customdata for operator */
typedef struct v2dViewZoomData {
  View2D *v2d; /* view2d we're operating in */
  ARegion *ar;

  /* needed for continuous zoom */
  wmTimer *timer;
  double timer_lastdraw;

  int lastx, lasty;   /* previous x/y values of mouse in window */
  int invoke_event;   /* event type that invoked, for modal exits */
  float dx, dy;       /* running tally of previous delta values (for obtaining final zoom) */
  float mx_2d, my_2d; /* initial mouse location in v2d coords */
} v2dViewZoomData;

/**
 * Clamp by convention rather then locking flags,
 * for ndof and +/- keys
 */
static void view_zoom_axis_lock_defaults(bContext *C, bool r_do_zoom_xy[2])
{
  ScrArea *sa = CTX_wm_area(C);

  r_do_zoom_xy[0] = true;
  r_do_zoom_xy[1] = true;

  /* default not to zoom the sequencer vertically */
  if (sa && sa->spacetype == SPACE_SEQ) {
    ARegion *ar = CTX_wm_region(C);

    if (ar && ar->regiontype == RGN_TYPE_WINDOW) {
      r_do_zoom_xy[1] = false;
    }
  }
}

/* initialize panning customdata */
static int view_zoomdrag_init(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  v2dViewZoomData *vzd;
  View2D *v2d;

  /* regions now have v2d-data by default, so check for region */
  if (ar == NULL) {
    return 0;
  }
  v2d = &ar->v2d;

  /* check that 2d-view is zoomable */
  if ((v2d->keepzoom & V2D_LOCKZOOM_X) && (v2d->keepzoom & V2D_LOCKZOOM_Y)) {
    return 0;
  }

  /* set custom-data for operator */
  vzd = MEM_callocN(sizeof(v2dViewZoomData), "v2dViewZoomData");
  op->customdata = vzd;

  /* set pointers to owners */
  vzd->v2d = v2d;
  vzd->ar = ar;

  return 1;
}

/* check if step-zoom can be applied */
static bool view_zoom_poll(bContext *C)
{
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d;

  /* check if there's a region in context to work with */
  if (ar == NULL) {
    return false;
  }

  /* Do not show that in 3DView context. */
  if (CTX_wm_region_view3d(C)) {
    return false;
  }

  v2d = &ar->v2d;

  /* check that 2d-view is zoomable */
  if ((v2d->keepzoom & V2D_LOCKZOOM_X) && (v2d->keepzoom & V2D_LOCKZOOM_Y)) {
    return false;
  }

  /* view is zoomable */
  return true;
}

/* apply transform to view (i.e. adjust 'cur' rect) */
static void view_zoomstep_apply_ex(
    bContext *C, v2dViewZoomData *vzd, const bool zoom_to_pos, const float facx, const float facy)
{
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = &ar->v2d;
  const rctf cur_old = v2d->cur;
  float dx, dy;
  const int snap_test = ED_region_snap_size_test(ar);

  /* calculate amount to move view by, ensuring symmetry so the
   * old zoom level is restored after zooming back the same amount
   */
  if (facx >= 0.0f) {
    dx = BLI_rctf_size_x(&v2d->cur) * facx;
    dy = BLI_rctf_size_y(&v2d->cur) * facy;
  }
  else {
    dx = (BLI_rctf_size_x(&v2d->cur) / (1.0f + 2.0f * facx)) * facx;
    dy = (BLI_rctf_size_y(&v2d->cur) / (1.0f + 2.0f * facy)) * facy;
  }

  /* only resize view on an axis if change is allowed */
  if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0) {
    if (v2d->keepofs & V2D_LOCKOFS_X) {
      v2d->cur.xmax -= 2 * dx;
    }
    else if (v2d->keepofs & V2D_KEEPOFS_X) {
      if (v2d->align & V2D_ALIGN_NO_POS_X) {
        v2d->cur.xmin += 2 * dx;
      }
      else {
        v2d->cur.xmax -= 2 * dx;
      }
    }
    else {

      v2d->cur.xmin += dx;
      v2d->cur.xmax -= dx;

      if (zoom_to_pos) {
        /* get zoom fac the same way as in
         * ui_view2d_curRect_validate_resize - better keep in sync! */
        const float zoomx = (float)(BLI_rcti_size_x(&v2d->mask) + 1) / BLI_rctf_size_x(&v2d->cur);

        /* only move view to mouse if zoom fac is inside minzoom/maxzoom */
        if (((v2d->keepzoom & V2D_LIMITZOOM) == 0) ||
            IN_RANGE_INCL(zoomx, v2d->minzoom, v2d->maxzoom)) {
          float mval_fac = (vzd->mx_2d - cur_old.xmin) / BLI_rctf_size_x(&cur_old);
          float mval_faci = 1.0f - mval_fac;
          float ofs = (mval_fac * dx) - (mval_faci * dx);

          v2d->cur.xmin += ofs;
          v2d->cur.xmax += ofs;
        }
      }
    }
  }
  if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0) {
    if (v2d->keepofs & V2D_LOCKOFS_Y) {
      v2d->cur.ymax -= 2 * dy;
    }
    else if (v2d->keepofs & V2D_KEEPOFS_Y) {
      if (v2d->align & V2D_ALIGN_NO_POS_Y) {
        v2d->cur.ymin += 2 * dy;
      }
      else {
        v2d->cur.ymax -= 2 * dy;
      }
    }
    else {

      v2d->cur.ymin += dy;
      v2d->cur.ymax -= dy;

      if (zoom_to_pos) {
        /* get zoom fac the same way as in
         * ui_view2d_curRect_validate_resize - better keep in sync! */
        const float zoomy = (float)(BLI_rcti_size_y(&v2d->mask) + 1) / BLI_rctf_size_y(&v2d->cur);

        /* only move view to mouse if zoom fac is inside minzoom/maxzoom */
        if (((v2d->keepzoom & V2D_LIMITZOOM) == 0) ||
            IN_RANGE_INCL(zoomy, v2d->minzoom, v2d->maxzoom)) {
          float mval_fac = (vzd->my_2d - cur_old.ymin) / BLI_rctf_size_y(&cur_old);
          float mval_faci = 1.0f - mval_fac;
          float ofs = (mval_fac * dy) - (mval_faci * dy);

          v2d->cur.ymin += ofs;
          v2d->cur.ymax += ofs;
        }
      }
    }
  }

  /* validate that view is in valid configuration after this operation */
  UI_view2d_curRect_validate(v2d);

  if (ED_region_snap_size_apply(ar, snap_test)) {
    ScrArea *sa = CTX_wm_area(C);
    ED_area_tag_redraw(sa);
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
  }

  /* request updates to be done... */
  ED_region_tag_redraw_no_rebuild(vzd->ar);
  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
}

static void view_zoomstep_apply(bContext *C, wmOperator *op)
{
  v2dViewZoomData *vzd = op->customdata;
  const bool zoom_to_pos = U.uiflag & USER_ZOOM_TO_MOUSEPOS;
  view_zoomstep_apply_ex(
      C, vzd, zoom_to_pos, RNA_float_get(op->ptr, "zoomfacx"), RNA_float_get(op->ptr, "zoomfacy"));
}

/* --------------- Individual Operators ------------------- */

/* cleanup temp customdata  */
static void view_zoomstep_exit(wmOperator *op)
{
  UI_view2d_zoom_cache_reset();

  if (op->customdata) {
    MEM_freeN(op->customdata);
    op->customdata = NULL;
  }
}

/* this operator only needs this single callback, where it calls the view_zoom_*() methods */
static int view_zoomin_exec(bContext *C, wmOperator *op)
{
  bool do_zoom_xy[2];

  /* check that there's an active region, as View2D data resides there */
  if (!view_zoom_poll(C)) {
    return OPERATOR_PASS_THROUGH;
  }

  view_zoom_axis_lock_defaults(C, do_zoom_xy);

  /* set RNA-Props - zooming in by uniform factor */
  RNA_float_set(op->ptr, "zoomfacx", do_zoom_xy[0] ? 0.0375f : 0.0f);
  RNA_float_set(op->ptr, "zoomfacy", do_zoom_xy[1] ? 0.0375f : 0.0f);

  /* apply movement, then we're done */
  view_zoomstep_apply(C, op);

  view_zoomstep_exit(op);

  return OPERATOR_FINISHED;
}

static int view_zoomin_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  v2dViewZoomData *vzd;

  if (!view_zoomdrag_init(C, op)) {
    return OPERATOR_PASS_THROUGH;
  }

  vzd = op->customdata;

  if (U.uiflag & USER_ZOOM_TO_MOUSEPOS) {
    ARegion *ar = CTX_wm_region(C);

    /* store initial mouse position (in view space) */
    UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &vzd->mx_2d, &vzd->my_2d);
  }

  return view_zoomin_exec(C, op);
}

static void VIEW2D_OT_zoom_in(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Zoom In";
  ot->description = "Zoom in the view";
  ot->idname = "VIEW2D_OT_zoom_in";

  /* api callbacks */
  ot->invoke = view_zoomin_invoke;
  ot->exec = view_zoomin_exec;  // XXX, needs view_zoomdrag_init called first.
  ot->poll = view_zoom_poll;

  /* rna - must keep these in sync with the other operators */
  prop = RNA_def_float(
      ot->srna, "zoomfacx", 0, -FLT_MAX, FLT_MAX, "Zoom Factor X", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_float(
      ot->srna, "zoomfacy", 0, -FLT_MAX, FLT_MAX, "Zoom Factor Y", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* this operator only needs this single callback, where it calls the view_zoom_*() methods */
static int view_zoomout_exec(bContext *C, wmOperator *op)
{
  bool do_zoom_xy[2];

  /* check that there's an active region, as View2D data resides there */
  if (!view_zoom_poll(C)) {
    return OPERATOR_PASS_THROUGH;
  }

  view_zoom_axis_lock_defaults(C, do_zoom_xy);

  /* set RNA-Props - zooming in by uniform factor */
  RNA_float_set(op->ptr, "zoomfacx", do_zoom_xy[0] ? -0.0375f : 0.0f);
  RNA_float_set(op->ptr, "zoomfacy", do_zoom_xy[1] ? -0.0375f : 0.0f);

  /* apply movement, then we're done */
  view_zoomstep_apply(C, op);

  view_zoomstep_exit(op);

  return OPERATOR_FINISHED;
}

static int view_zoomout_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  v2dViewZoomData *vzd;

  if (!view_zoomdrag_init(C, op)) {
    return OPERATOR_PASS_THROUGH;
  }

  vzd = op->customdata;

  if (U.uiflag & USER_ZOOM_TO_MOUSEPOS) {
    ARegion *ar = CTX_wm_region(C);

    /* store initial mouse position (in view space) */
    UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &vzd->mx_2d, &vzd->my_2d);
  }

  return view_zoomout_exec(C, op);
}

static void VIEW2D_OT_zoom_out(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Zoom Out";
  ot->description = "Zoom out the view";
  ot->idname = "VIEW2D_OT_zoom_out";

  /* api callbacks */
  ot->invoke = view_zoomout_invoke;
  //  ot->exec = view_zoomout_exec; // XXX, needs view_zoomdrag_init called first.
  ot->poll = view_zoom_poll;

  /* rna - must keep these in sync with the other operators */
  prop = RNA_def_float(
      ot->srna, "zoomfacx", 0, -FLT_MAX, FLT_MAX, "Zoom Factor X", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_float(
      ot->srna, "zoomfacy", 0, -FLT_MAX, FLT_MAX, "Zoom Factor Y", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* ********************************************************* */
/* DRAG-ZOOM OPERATOR                                    */

/**
 * MMB Drag - allows non-uniform scaling by dragging mouse
 *
 * In order to make sure this works, each operator must define the following RNA-Operator Props:
 * - `deltax, deltay` - amounts to add to each side of the 'cur' rect
 */

/* apply transform to view (i.e. adjust 'cur' rect) */
static void view_zoomdrag_apply(bContext *C, wmOperator *op)
{
  v2dViewZoomData *vzd = op->customdata;
  View2D *v2d = vzd->v2d;
  float dx, dy;
  const int snap_test = ED_region_snap_size_test(vzd->ar);

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");
  const bool zoom_to_pos = use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS);

  /* get amount to move view by */
  dx = RNA_float_get(op->ptr, "deltax");
  dy = RNA_float_get(op->ptr, "deltay");

  if (U.uiflag & USER_ZOOM_INVERT) {
    dx *= -1;
    dy *= -1;
  }

  /* continuous zoom shouldn't move that fast... */
  if (U.viewzoom == USER_ZOOM_CONT) {  // XXX store this setting as RNA prop?
    double time = PIL_check_seconds_timer();
    float time_step = (float)(time - vzd->timer_lastdraw);

    dx *= time_step * 0.5f;
    dy *= time_step * 0.5f;

    vzd->timer_lastdraw = time;
  }

  /* only move view on an axis if change is allowed */
  if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0) {
    if (v2d->keepofs & V2D_LOCKOFS_X) {
      v2d->cur.xmax -= 2 * dx;
    }
    else {
      if (zoom_to_pos) {
        float mval_fac = (vzd->mx_2d - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur);
        float mval_faci = 1.0f - mval_fac;
        float ofs = (mval_fac * dx) - (mval_faci * dx);

        v2d->cur.xmin += ofs + dx;
        v2d->cur.xmax += ofs - dx;
      }
      else {
        v2d->cur.xmin += dx;
        v2d->cur.xmax -= dx;
      }
    }
  }
  if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0) {
    if (v2d->keepofs & V2D_LOCKOFS_Y) {
      v2d->cur.ymax -= 2 * dy;
    }
    else {
      if (zoom_to_pos) {
        float mval_fac = (vzd->my_2d - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur);
        float mval_faci = 1.0f - mval_fac;
        float ofs = (mval_fac * dy) - (mval_faci * dy);

        v2d->cur.ymin += ofs + dy;
        v2d->cur.ymax += ofs - dy;
      }
      else {
        v2d->cur.ymin += dy;
        v2d->cur.ymax -= dy;
      }
    }
  }

  /* validate that view is in valid configuration after this operation */
  UI_view2d_curRect_validate(v2d);

  if (ED_region_snap_size_apply(vzd->ar, snap_test)) {
    ScrArea *sa = CTX_wm_area(C);
    ED_area_tag_redraw(sa);
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
  }

  /* request updates to be done... */
  ED_region_tag_redraw_no_rebuild(vzd->ar);
  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
}

/* cleanup temp customdata  */
static void view_zoomdrag_exit(bContext *C, wmOperator *op)
{
  UI_view2d_zoom_cache_reset();

  if (op->customdata) {
    v2dViewZoomData *vzd = op->customdata;

    if (vzd->timer) {
      WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), vzd->timer);
    }

    MEM_freeN(op->customdata);
    op->customdata = NULL;
  }
}

static void view_zoomdrag_cancel(bContext *C, wmOperator *op)
{
  view_zoomdrag_exit(C, op);
}

/* for 'redo' only, with no user input */
static int view_zoomdrag_exec(bContext *C, wmOperator *op)
{
  if (!view_zoomdrag_init(C, op)) {
    return OPERATOR_PASS_THROUGH;
  }

  view_zoomdrag_apply(C, op);
  view_zoomdrag_exit(C, op);
  return OPERATOR_FINISHED;
}

/* set up modal operator and relevant settings */
static int view_zoomdrag_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *window = CTX_wm_window(C);
  v2dViewZoomData *vzd;
  View2D *v2d;

  /* set up customdata */
  if (!view_zoomdrag_init(C, op)) {
    return OPERATOR_PASS_THROUGH;
  }

  vzd = op->customdata;
  v2d = vzd->v2d;

  if (event->type == MOUSEZOOM || event->type == MOUSEPAN) {
    float dx, dy, fac;

    vzd->lastx = event->prevx;
    vzd->lasty = event->prevy;

    /* As we have only 1D information (magnify value), feed both axes
     * with magnify information that is stored in x axis
     */
    fac = 0.01f * (event->prevx - event->x);
    dx = fac * BLI_rctf_size_x(&v2d->cur) / 10.0f;
    if (event->type == MOUSEPAN) {
      fac = 0.01f * (event->prevy - event->y);
    }
    dy = fac * BLI_rctf_size_y(&v2d->cur) / 10.0f;

    /* support trackpad zoom to always zoom entirely - the v2d code uses portrait or
     * landscape exceptions */
    if (v2d->keepzoom & V2D_KEEPASPECT) {
      if (fabsf(dx) > fabsf(dy)) {
        dy = dx;
      }
      else {
        dx = dy;
      }
    }
    RNA_float_set(op->ptr, "deltax", dx);
    RNA_float_set(op->ptr, "deltay", dy);

    view_zoomdrag_apply(C, op);
    view_zoomdrag_exit(C, op);
    return OPERATOR_FINISHED;
  }

  /* set initial settings */
  vzd->lastx = event->x;
  vzd->lasty = event->y;
  RNA_float_set(op->ptr, "deltax", 0);
  RNA_float_set(op->ptr, "deltay", 0);

  /* for modal exit test */
  vzd->invoke_event = event->type;

  if (U.uiflag & USER_ZOOM_TO_MOUSEPOS) {
    ARegion *ar = CTX_wm_region(C);

    /* store initial mouse position (in view space) */
    UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &vzd->mx_2d, &vzd->my_2d);
  }

  if (v2d->keepofs & V2D_LOCKOFS_X) {
    WM_cursor_modal_set(window, BC_NS_SCROLLCURSOR);
  }
  else if (v2d->keepofs & V2D_LOCKOFS_Y) {
    WM_cursor_modal_set(window, BC_EW_SCROLLCURSOR);
  }
  else {
    WM_cursor_modal_set(window, BC_NSEW_SCROLLCURSOR);
  }

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  if (U.viewzoom == USER_ZOOM_CONT) {
    /* needs a timer to continue redrawing */
    vzd->timer = WM_event_add_timer(CTX_wm_manager(C), window, TIMER, 0.01f);
    vzd->timer_lastdraw = PIL_check_seconds_timer();
  }

  return OPERATOR_RUNNING_MODAL;
}

/* handle user input - calculations of mouse-movement need to be done here,
 * not in the apply callback! */
static int view_zoomdrag_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  v2dViewZoomData *vzd = op->customdata;
  View2D *v2d = vzd->v2d;

  /* execute the events */
  if (event->type == TIMER && event->customdata == vzd->timer) {
    view_zoomdrag_apply(C, op);
  }
  else if (event->type == MOUSEMOVE) {
    float dx, dy;

    /* calculate new delta transform, based on zooming mode */
    if (U.viewzoom == USER_ZOOM_SCALE) {
      /* 'scale' zooming */
      float dist;

      /* x-axis transform */
      dist = BLI_rcti_size_x(&v2d->mask) / 2.0f;
      dx = 1.0f - (fabsf(vzd->lastx - vzd->ar->winrct.xmin - dist) + 2.0f) /
                      (fabsf(event->mval[0] - dist) + 2.0f);
      dx *= 0.5f * BLI_rctf_size_x(&v2d->cur);

      /* y-axis transform */
      dist = BLI_rcti_size_y(&v2d->mask) / 2.0f;
      dy = 1.0f - (fabsf(vzd->lasty - vzd->ar->winrct.ymin - dist) + 2.0f) /
                      (fabsf(event->mval[1] - dist) + 2.0f);
      dy *= 0.5f * BLI_rctf_size_y(&v2d->cur);
    }
    else {
      /* 'continuous' or 'dolly' */
      float fac, zoomfac = 0.01f;

      /* some view2d's (graph) don't have min/max zoom, or extreme ones */
      if (v2d->maxzoom > 0.0f) {
        zoomfac = clamp_f(0.001f * v2d->maxzoom, 0.001f, 0.01f);
      }

      /* x-axis transform */
      fac = zoomfac * (event->x - vzd->lastx);
      dx = fac * BLI_rctf_size_x(&v2d->cur);

      /* y-axis transform */
      fac = zoomfac * (event->y - vzd->lasty);
      dy = fac * BLI_rctf_size_y(&v2d->cur);
    }

    /* support zoom to always zoom entirely - the v2d code uses portrait or
     * landscape exceptions */
    if (v2d->keepzoom & V2D_KEEPASPECT) {
      if (fabsf(dx) > fabsf(dy)) {
        dy = dx;
      }
      else {
        dx = dy;
      }
    }

    /* set transform amount, and add current deltas to stored total delta (for redo) */
    RNA_float_set(op->ptr, "deltax", dx);
    RNA_float_set(op->ptr, "deltay", dy);

    vzd->dx += dx;
    vzd->dy += dy;

    /* Store mouse coordinates for next time, if not doing continuous zoom:
     * - Continuous zoom only depends on distance of mouse
     *   to starting point to determine rate of change.
     */
    if (U.viewzoom != USER_ZOOM_CONT) {  // XXX store this setting as RNA prop?
      vzd->lastx = event->x;
      vzd->lasty = event->y;
    }

    /* apply zooming */
    view_zoomdrag_apply(C, op);
  }
  else if (event->type == vzd->invoke_event || event->type == ESCKEY) {
    if (event->val == KM_RELEASE) {

      /* for redo, store the overall deltas - need to respect zoom-locks here... */
      if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0) {
        RNA_float_set(op->ptr, "deltax", vzd->dx);
      }
      else {
        RNA_float_set(op->ptr, "deltax", 0);
      }

      if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0) {
        RNA_float_set(op->ptr, "deltay", vzd->dy);
      }
      else {
        RNA_float_set(op->ptr, "deltay", 0);
      }

      /* free customdata */
      view_zoomdrag_exit(C, op);
      WM_cursor_modal_restore(CTX_wm_window(C));

      return OPERATOR_FINISHED;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void VIEW2D_OT_zoom(wmOperatorType *ot)
{
  PropertyRNA *prop;
  /* identifiers */
  ot->name = "Zoom 2D View";
  ot->description = "Zoom in/out the view";
  ot->idname = "VIEW2D_OT_zoom";

  /* api callbacks */
  ot->exec = view_zoomdrag_exec;
  ot->invoke = view_zoomdrag_invoke;
  ot->modal = view_zoomdrag_modal;
  ot->cancel = view_zoomdrag_cancel;

  ot->poll = view_zoom_poll;

  /* operator is repeatable */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* rna - must keep these in sync with the other operators */
  prop = RNA_def_float(ot->srna, "deltax", 0, -FLT_MAX, FLT_MAX, "Delta X", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_float(ot->srna, "deltay", 0, -FLT_MAX, FLT_MAX, "Delta Y", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  WM_operator_properties_use_cursor_init(ot);
}

/* ********************************************************* */
/* BORDER-ZOOM */

/**
 * The user defines a rect using standard box select tools, and we use this rect to
 * define the new zoom-level of the view in the following ways:
 *
 * -# LEFTMOUSE - zoom in to view
 * -# RIGHTMOUSE - zoom out of view
 *
 * Currently, these key mappings are hardcoded, but it shouldn't be too important to
 * have custom keymappings for this...
 */

static int view_borderzoom_exec(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = &ar->v2d;
  rctf rect;
  rctf cur_new = v2d->cur;
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* convert coordinates of rect to 'tot' rect coordinates */
  WM_operator_properties_border_to_rctf(op, &rect);
  UI_view2d_region_to_view_rctf(v2d, &rect, &rect);

  /* check if zooming in/out view */
  const bool zoom_in = !RNA_boolean_get(op->ptr, "zoom_out");

  if (zoom_in) {
    /* zoom in:
     * - 'cur' rect will be defined by the coordinates of the border region
     * - just set the 'cur' rect to have the same coordinates as the border region
     *   if zoom is allowed to be changed
     */
    if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0) {
      cur_new.xmin = rect.xmin;
      cur_new.xmax = rect.xmax;
    }
    if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0) {
      cur_new.ymin = rect.ymin;
      cur_new.ymax = rect.ymax;
    }
  }
  else {
    /* zoom out:
     * - the current 'cur' rect coordinates are going to end up where the 'rect' ones are,
     *   but the 'cur' rect coordinates will need to be adjusted to take in more of the view
     * - calculate zoom factor, and adjust using center-point
     */
    float zoom, center, size;

    /* TODO: is this zoom factor calculation valid?
     * It seems to produce same results every time... */
    if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0) {
      size = BLI_rctf_size_x(&cur_new);
      zoom = size / BLI_rctf_size_x(&rect);
      center = BLI_rctf_cent_x(&cur_new);

      cur_new.xmin = center - (size * zoom);
      cur_new.xmax = center + (size * zoom);
    }
    if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0) {
      size = BLI_rctf_size_y(&cur_new);
      zoom = size / BLI_rctf_size_y(&rect);
      center = BLI_rctf_cent_y(&cur_new);

      cur_new.ymin = center - (size * zoom);
      cur_new.ymax = center + (size * zoom);
    }
  }

  UI_view2d_smooth_view(C, ar, &cur_new, smooth_viewtx);

  return OPERATOR_FINISHED;
}

static void VIEW2D_OT_zoom_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Zoom to Border";
  ot->description = "Zoom in the view to the nearest item contained in the border";
  ot->idname = "VIEW2D_OT_zoom_border";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = view_borderzoom_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = view_zoom_poll;

  /* rna */
  WM_operator_properties_gesture_box_zoom(ot);
}

#ifdef WITH_INPUT_NDOF
static int view2d_ndof_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }
  else {
    const wmNDOFMotionData *ndof = event->customdata;

    /* tune these until it feels right */
    const float zoom_sensitivity = 0.5f;
    const float speed = 10.0f; /* match view3d ortho */
    const bool has_translate = (ndof->tvec[0] && ndof->tvec[1]) && view_pan_poll(C);
    const bool has_zoom = (ndof->tvec[2] != 0.0f) && view_zoom_poll(C);

    if (has_translate) {
      if (view_pan_init(C, op)) {
        v2dViewPanData *vpd;
        float pan_vec[3];

        WM_event_ndof_pan_get(ndof, pan_vec, false);

        pan_vec[0] *= speed;
        pan_vec[1] *= speed;

        vpd = op->customdata;

        view_pan_apply_ex(C, vpd, pan_vec[0], pan_vec[1]);

        view_pan_exit(op);
      }
    }

    if (has_zoom) {
      if (view_zoomdrag_init(C, op)) {
        v2dViewZoomData *vzd;
        float zoom_factor = zoom_sensitivity * ndof->dt * -ndof->tvec[2];

        bool do_zoom_xy[2];

        if (U.ndof_flag & NDOF_ZOOM_INVERT) {
          zoom_factor = -zoom_factor;
        }

        view_zoom_axis_lock_defaults(C, do_zoom_xy);

        vzd = op->customdata;

        view_zoomstep_apply_ex(
            C, vzd, false, do_zoom_xy[0] ? zoom_factor : 0.0f, do_zoom_xy[1] ? zoom_factor : 0.0f);

        view_zoomstep_exit(op);
      }
    }

    return OPERATOR_FINISHED;
  }
}

static void VIEW2D_OT_ndof(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Pan/Zoom";
  ot->idname = "VIEW2D_OT_ndof";
  ot->description = "Use a 3D mouse device to pan/zoom the view";

  /* api callbacks */
  ot->invoke = view2d_ndof_invoke;
  ot->poll = view2d_poll;

  /* flags */
  ot->flag = OPTYPE_LOCK_BYPASS;
}
#endif /* WITH_INPUT_NDOF */

/* ********************************************************* */
/* SMOOTH VIEW */

struct SmoothView2DStore {
  rctf orig_cur, new_cur;

  double time_allowed;
};

/**
 * function to get a factor out of a rectangle
 *
 * note: this doesn't always work as well as it might because the target size
 *       may not be reached because of clamping the desired rect, we _could_
 *       attempt to clamp the rect before working out the zoom factor but its
 *       not really worthwhile for the few cases this happens.
 */
static float smooth_view_rect_to_fac(const rctf *rect_a, const rctf *rect_b)
{
  const float size_a[2] = {BLI_rctf_size_x(rect_a), BLI_rctf_size_y(rect_a)};
  const float size_b[2] = {BLI_rctf_size_x(rect_b), BLI_rctf_size_y(rect_b)};
  const float cent_a[2] = {BLI_rctf_cent_x(rect_a), BLI_rctf_cent_y(rect_a)};
  const float cent_b[2] = {BLI_rctf_cent_x(rect_b), BLI_rctf_cent_y(rect_b)};

  float fac_max = 0.0f;
  float tfac;

  int i;

  for (i = 0; i < 2; i++) {
    /* axis translation normalized to scale */
    tfac = fabsf(cent_a[i] - cent_b[i]) / min_ff(size_a[i], size_b[i]);
    fac_max = max_ff(fac_max, tfac);
    if (fac_max >= 1.0f) {
      break;
    }

    /* axis scale difference, x2 so doubling or half gives 1.0f */
    tfac = (1.0f - (min_ff(size_a[i], size_b[i]) / max_ff(size_a[i], size_b[i]))) * 2.0f;
    fac_max = max_ff(fac_max, tfac);
    if (fac_max >= 1.0f) {
      break;
    }
  }
  return min_ff(fac_max, 1.0f);
}

/* will start timer if appropriate */
/* the arguments are the desired situation */
void UI_view2d_smooth_view(bContext *C, ARegion *ar, const rctf *cur, const int smooth_viewtx)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);

  View2D *v2d = &ar->v2d;
  struct SmoothView2DStore sms = {{0}};
  bool ok = false;
  float fac = 1.0f;

  /* initialize sms */
  sms.new_cur = v2d->cur;

  /* store the options we want to end with */
  if (cur) {
    sms.new_cur = *cur;
  }

  if (cur) {
    fac = smooth_view_rect_to_fac(&v2d->cur, cur);
  }

  if (smooth_viewtx && fac > FLT_EPSILON) {
    bool changed = false;

    if (BLI_rctf_compare(&sms.new_cur, &v2d->cur, FLT_EPSILON) == false) {
      changed = true;
    }

    /* The new view is different from the old one
     * so animate the view */
    if (changed) {
      sms.orig_cur = v2d->cur;

      sms.time_allowed = (double)smooth_viewtx / 1000.0;

      /* scale the time allowed the change in view */
      sms.time_allowed *= (double)fac;

      /* keep track of running timer! */
      if (v2d->sms == NULL) {
        v2d->sms = MEM_mallocN(sizeof(struct SmoothView2DStore), "smoothview v2d");
      }
      *v2d->sms = sms;
      if (v2d->smooth_timer) {
        WM_event_remove_timer(wm, win, v2d->smooth_timer);
      }
      /* TIMER1 is hardcoded in keymap */
      /* max 30 frs/sec */
      v2d->smooth_timer = WM_event_add_timer(wm, win, TIMER1, 1.0 / 100.0);

      ok = true;
    }
  }

  /* if we get here nothing happens */
  if (ok == false) {
    v2d->cur = sms.new_cur;

    UI_view2d_curRect_validate(v2d);
    ED_region_tag_redraw_no_rebuild(ar);
    UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
  }
}

/* only meant for timer usage */
static int view2d_smoothview_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = &ar->v2d;
  struct SmoothView2DStore *sms = v2d->sms;
  float step;

  /* escape if not our timer */
  if (v2d->smooth_timer == NULL || v2d->smooth_timer != event->customdata) {
    return OPERATOR_PASS_THROUGH;
  }

  if (sms->time_allowed != 0.0) {
    step = (float)((v2d->smooth_timer->duration) / sms->time_allowed);
  }
  else {
    step = 1.0f;
  }

  /* end timer */
  if (step >= 1.0f) {
    v2d->cur = sms->new_cur;

    MEM_freeN(v2d->sms);
    v2d->sms = NULL;

    WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), v2d->smooth_timer);
    v2d->smooth_timer = NULL;

    /* Event handling won't know if a UI item has been moved under the pointer. */
    WM_event_add_mousemove(C);
  }
  else {
    /* ease in/out */
    step = (3.0f * step * step - 2.0f * step * step * step);

    BLI_rctf_interp(&v2d->cur, &sms->orig_cur, &sms->new_cur, step);
  }

  UI_view2d_curRect_validate(v2d);
  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
  ED_region_tag_redraw_no_rebuild(ar);

  if (v2d->sms == NULL) {
    UI_view2d_zoom_cache_reset();
  }

  return OPERATOR_FINISHED;
}

static void VIEW2D_OT_smoothview(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Smooth View 2D";
  ot->idname = "VIEW2D_OT_smoothview";

  /* api callbacks */
  ot->invoke = view2d_smoothview_invoke;
  ot->poll = view2d_poll;

  /* flags */
  ot->flag = OPTYPE_INTERNAL;

  /* rna */
  WM_operator_properties_gesture_box(ot);
}

/* ********************************************************* */
/* SCROLLERS */

/**
 * Scrollers should behave in the following ways, when clicked on with LMB (and dragged):
 * -# 'Handles' on end of 'bubble' - when the axis that the scroller represents is zoomable,
 *    enlarge 'cur' rect on the relevant side.
 * -# 'Bubble'/'bar' - just drag, and bar should move with mouse (view pans opposite).
 *
 * In order to make sure this works, each operator must define the following RNA-Operator Props:
 * - `deltax, deltay` - define how much to move view by (relative to zoom-correction factor)
 */

/* customdata for scroller-invoke data */
typedef struct v2dScrollerMove {
  /** View2D data that this operation affects */
  View2D *v2d;
  /** region that the scroller is in */
  ARegion *ar;

  /** scroller that mouse is in ('h' or 'v') */
  char scroller;

  /* XXX find some way to provide visual feedback of this (active color?) */
  /** -1 is min zoomer, 0 is bar, 1 is max zoomer */
  short zone;

  /** view adjustment factor, based on size of region */
  float fac;
  /** for pixel rounding (avoid visible UI jitter) */
  float fac_round;
  /** amount moved by mouse on axis of interest */
  float delta;

  /** width of the scrollbar itself, used for page up/down clicks */
  float scrollbarwidth;
  /** initial location of scrollbar x/y, mouse relative */
  int scrollbar_orig;

  /** previous mouse coordinates (in screen coordinates) for determining movement */
  int lastx, lasty;
} v2dScrollerMove;

/**
 * #View2DScrollers is typedef'd in UI_view2d.h
 * This is a CUT DOWN VERSION of the 'real' version, which is defined in view2d.c,
 * as we only need focus bubble info.
 *
 * \warning: The start of this struct must not change,
 * so that it stays in sync with the 'real' version.
 * For now, we don't need to have a separate (internal) header for structs like this...
 */
struct View2DScrollers {
  /* focus bubbles */
  int vert_min, vert_max; /* vertical scrollbar */
  int hor_min, hor_max;   /* horizontal scrollbar */
};

/* quick enum for vsm->zone (scroller handles) */
enum {
  SCROLLHANDLE_MIN = -1,
  SCROLLHANDLE_BAR,
  SCROLLHANDLE_MAX,
  SCROLLHANDLE_MIN_OUTSIDE,
  SCROLLHANDLE_MAX_OUTSIDE,
} /*eV2DScrollerHandle_Zone*/;

/* ------------------------ */

/**
 * Check if mouse is within scroller handle.
 *
 * \param mouse: relevant mouse coordinate in region space.
 * \param sc_min, sc_max: extents of scroller 'groove' (potential available space for scroller).
 * \param sh_min, sh_max: positions of scrollbar handles.
 */
static short mouse_in_scroller_handle(int mouse, int sc_min, int sc_max, int sh_min, int sh_max)
{
  bool in_min, in_max, in_bar, out_min, out_max, in_view = 1;

  /* firstly, check if
   * - 'bubble' fills entire scroller
   * - 'bubble' completely out of view on either side
   */
  if ((sh_min <= sc_min) && (sh_max >= sc_max)) {
    in_view = 0;
  }
  if (sh_min == sh_max) {
    if (sh_min <= sc_min) {
      in_view = 0;
    }
    if (sh_max >= sc_max) {
      in_view = 0;
    }
  }
  else {
    if (sh_max <= sc_min) {
      in_view = 0;
    }
    if (sh_min >= sc_max) {
      in_view = 0;
    }
  }

  if (in_view == 0) {
    return SCROLLHANDLE_BAR;
  }

  /* check if mouse is in or past either handle */
  /* TODO: check if these extents are still valid or not */
  in_max = ((mouse >= (sh_max - V2D_SCROLL_HANDLE_SIZE_HOTSPOT)) &&
            (mouse <= (sh_max + V2D_SCROLL_HANDLE_SIZE_HOTSPOT)));
  in_min = ((mouse <= (sh_min + V2D_SCROLL_HANDLE_SIZE_HOTSPOT)) &&
            (mouse >= (sh_min - V2D_SCROLL_HANDLE_SIZE_HOTSPOT)));
  in_bar = ((mouse < (sh_max - V2D_SCROLL_HANDLE_SIZE_HOTSPOT)) &&
            (mouse > (sh_min + V2D_SCROLL_HANDLE_SIZE_HOTSPOT)));
  out_min = mouse < (sh_min - V2D_SCROLL_HANDLE_SIZE_HOTSPOT);
  out_max = mouse > (sh_max + V2D_SCROLL_HANDLE_SIZE_HOTSPOT);

  if (in_bar) {
    return SCROLLHANDLE_BAR;
  }
  else if (in_max) {
    return SCROLLHANDLE_MAX;
  }
  else if (in_min) {
    return SCROLLHANDLE_MIN;
  }
  else if (out_min) {
    return SCROLLHANDLE_MIN_OUTSIDE;
  }
  else if (out_max) {
    return SCROLLHANDLE_MAX_OUTSIDE;
  }

  /* unlikely to happen, though we just cover it in case */
  return SCROLLHANDLE_BAR;
}

static bool scroller_activate_poll(bContext *C)
{
  if (!view2d_poll(C)) {
    return false;
  }

  wmWindow *win = CTX_wm_window(C);
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = &ar->v2d;
  wmEvent *event = win->eventstate;

  /* check if mouse in scrollbars, if they're enabled */
  return (UI_view2d_mouse_in_scrollers(ar, v2d, event->x, event->y) != 0);
}

/* initialize customdata for scroller manipulation operator */
static void scroller_activate_init(bContext *C,
                                   wmOperator *op,
                                   const wmEvent *event,
                                   const char in_scroller)
{
  v2dScrollerMove *vsm;
  View2DScrollers *scrollers;
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = &ar->v2d;
  rctf tot_cur_union;
  float mask_size;

  /* set custom-data for operator */
  vsm = MEM_callocN(sizeof(v2dScrollerMove), "v2dScrollerMove");
  op->customdata = vsm;

  /* set general data */
  vsm->v2d = v2d;
  vsm->ar = ar;
  vsm->scroller = in_scroller;

  /* store mouse-coordinates, and convert mouse/screen coordinates to region coordinates */
  vsm->lastx = event->x;
  vsm->lasty = event->y;
  /* 'zone' depends on where mouse is relative to bubble
   * - zooming must be allowed on this axis, otherwise, default to pan
   */
  scrollers = UI_view2d_scrollers_calc(v2d, NULL);

  /* use a union of 'cur' & 'tot' incase the current view is far outside 'tot'. In this cases
   * moving the scroll bars has far too little effect and the view can get stuck T31476. */
  tot_cur_union = v2d->tot;
  BLI_rctf_union(&tot_cur_union, &v2d->cur);

  if (in_scroller == 'h') {
    /* horizontal scroller - calculate adjustment factor first */
    mask_size = (float)BLI_rcti_size_x(&v2d->hor);
    vsm->fac = BLI_rctf_size_x(&tot_cur_union) / mask_size;

    /* pixel rounding */
    vsm->fac_round = (BLI_rctf_size_x(&v2d->cur)) / (float)(BLI_rcti_size_x(&ar->winrct) + 1);

    /* get 'zone' (i.e. which part of scroller is activated) */
    vsm->zone = mouse_in_scroller_handle(
        event->mval[0], v2d->hor.xmin, v2d->hor.xmax, scrollers->hor_min, scrollers->hor_max);

    if ((v2d->keepzoom & V2D_LOCKZOOM_X) && ELEM(vsm->zone, SCROLLHANDLE_MIN, SCROLLHANDLE_MAX)) {
      /* default to scroll, as handles not usable */
      vsm->zone = SCROLLHANDLE_BAR;
    }

    vsm->scrollbarwidth = scrollers->hor_max - scrollers->hor_min;
    vsm->scrollbar_orig = ((scrollers->hor_max + scrollers->hor_min) / 2) + ar->winrct.xmin;
  }
  else {
    /* vertical scroller - calculate adjustment factor first */
    mask_size = (float)BLI_rcti_size_y(&v2d->vert);
    vsm->fac = BLI_rctf_size_y(&tot_cur_union) / mask_size;

    /* pixel rounding */
    vsm->fac_round = (BLI_rctf_size_y(&v2d->cur)) / (float)(BLI_rcti_size_y(&ar->winrct) + 1);

    /* get 'zone' (i.e. which part of scroller is activated) */
    vsm->zone = mouse_in_scroller_handle(
        event->mval[1], v2d->vert.ymin, v2d->vert.ymax, scrollers->vert_min, scrollers->vert_max);

    if ((v2d->keepzoom & V2D_LOCKZOOM_Y) && ELEM(vsm->zone, SCROLLHANDLE_MIN, SCROLLHANDLE_MAX)) {
      /* default to scroll, as handles not usable */
      vsm->zone = SCROLLHANDLE_BAR;
    }

    vsm->scrollbarwidth = scrollers->vert_max - scrollers->vert_min;
    vsm->scrollbar_orig = ((scrollers->vert_max + scrollers->vert_min) / 2) + ar->winrct.ymin;
  }

  UI_view2d_scrollers_free(scrollers);
  ED_region_tag_redraw_no_rebuild(ar);
}

/* cleanup temp customdata  */
static void scroller_activate_exit(bContext *C, wmOperator *op)
{
  if (op->customdata) {
    v2dScrollerMove *vsm = op->customdata;

    vsm->v2d->scroll_ui &= ~(V2D_SCROLL_H_ACTIVE | V2D_SCROLL_V_ACTIVE);

    MEM_freeN(op->customdata);
    op->customdata = NULL;

    ED_region_tag_redraw_no_rebuild(CTX_wm_region(C));
  }
}

static void scroller_activate_cancel(bContext *C, wmOperator *op)
{
  scroller_activate_exit(C, op);
}

/* apply transform to view (i.e. adjust 'cur' rect) */
static void scroller_activate_apply(bContext *C, wmOperator *op)
{
  v2dScrollerMove *vsm = op->customdata;
  View2D *v2d = vsm->v2d;
  float temp;

  /* calculate amount to move view by */
  temp = vsm->fac * vsm->delta;

  /* round to pixel */
  temp = roundf(temp / vsm->fac_round) * vsm->fac_round;

  /* type of movement */
  switch (vsm->zone) {
    case SCROLLHANDLE_MIN:
      /* only expand view on axis if zoom is allowed */
      if ((vsm->scroller == 'h') && !(v2d->keepzoom & V2D_LOCKZOOM_X)) {
        v2d->cur.xmin -= temp;
      }
      if ((vsm->scroller == 'v') && !(v2d->keepzoom & V2D_LOCKZOOM_Y)) {
        v2d->cur.ymin -= temp;
      }
      break;

    case SCROLLHANDLE_MAX:

      /* only expand view on axis if zoom is allowed */
      if ((vsm->scroller == 'h') && !(v2d->keepzoom & V2D_LOCKZOOM_X)) {
        v2d->cur.xmax += temp;
      }
      if ((vsm->scroller == 'v') && !(v2d->keepzoom & V2D_LOCKZOOM_Y)) {
        v2d->cur.ymax += temp;
      }
      break;

    case SCROLLHANDLE_MIN_OUTSIDE:
    case SCROLLHANDLE_MAX_OUTSIDE:
    case SCROLLHANDLE_BAR:
    default:
      /* only move view on an axis if panning is allowed */
      if ((vsm->scroller == 'h') && !(v2d->keepofs & V2D_LOCKOFS_X)) {
        v2d->cur.xmin += temp;
        v2d->cur.xmax += temp;
      }
      if ((vsm->scroller == 'v') && !(v2d->keepofs & V2D_LOCKOFS_Y)) {
        v2d->cur.ymin += temp;
        v2d->cur.ymax += temp;
      }
      break;
  }

  /* validate that view is in valid configuration after this operation */
  UI_view2d_curRect_validate(v2d);

  /* request updates to be done... */
  ED_region_tag_redraw_no_rebuild(vsm->ar);
  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
}

/**
 * Handle user input for scrollers - calculations of mouse-movement need to be done here,
 * not in the apply callback!
 */
static int scroller_activate_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  v2dScrollerMove *vsm = op->customdata;

  /* execute the events */
  switch (event->type) {
    case MOUSEMOVE: {
      /* calculate new delta transform, then store mouse-coordinates for next-time */
      if (ELEM(vsm->zone, SCROLLHANDLE_BAR, SCROLLHANDLE_MAX)) {
        /* if using bar (i.e. 'panning') or 'max' zoom widget */
        switch (vsm->scroller) {
          case 'h': /* horizontal scroller - so only horizontal movement
                     * ('cur' moves opposite to mouse) */
            vsm->delta = (float)(event->x - vsm->lastx);
            break;
          case 'v': /* vertical scroller - so only vertical movement
                     * ('cur' moves opposite to mouse) */
            vsm->delta = (float)(event->y - vsm->lasty);
            break;
        }
      }
      else if (vsm->zone == SCROLLHANDLE_MIN) {
        /* using 'min' zoom widget */
        switch (vsm->scroller) {
          case 'h': /* horizontal scroller - so only horizontal movement
                     * ('cur' moves with mouse) */
            vsm->delta = (float)(vsm->lastx - event->x);
            break;
          case 'v': /* vertical scroller - so only vertical movement
                     * ('cur' moves with to mouse) */
            vsm->delta = (float)(vsm->lasty - event->y);
            break;
        }
      }

      /* store previous coordinates */
      vsm->lastx = event->x;
      vsm->lasty = event->y;

      scroller_activate_apply(C, op);
      break;
    }
    case LEFTMOUSE:
    case MIDDLEMOUSE:
      if (event->val == KM_RELEASE) {
        /* single-click was in empty space outside bubble, so scroll by 1 'page' */
        if (ELEM(vsm->zone, SCROLLHANDLE_MIN_OUTSIDE, SCROLLHANDLE_MAX_OUTSIDE)) {
          if (vsm->zone == SCROLLHANDLE_MIN_OUTSIDE) {
            vsm->delta = -vsm->scrollbarwidth * 0.8f;
          }
          else if (vsm->zone == SCROLLHANDLE_MAX_OUTSIDE) {
            vsm->delta = vsm->scrollbarwidth * 0.8f;
          }

          scroller_activate_apply(C, op);
          scroller_activate_exit(C, op);
          return OPERATOR_FINISHED;
        }

        /* otherwise, end the drag action  */
        if (vsm->lastx || vsm->lasty) {
          scroller_activate_exit(C, op);
          return OPERATOR_FINISHED;
        }
      }
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

/* a click (or click drag in progress)
 * should have occurred, so check if it happened in scrollbar */
static int scroller_activate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = &ar->v2d;

  /* check if mouse in scrollbars, if they're enabled */
  const char in_scroller = UI_view2d_mouse_in_scrollers(ar, v2d, event->x, event->y);

  /* if in a scroller, init customdata then set modal handler which will
   * catch mousedown to start doing useful stuff */
  if (in_scroller) {
    v2dScrollerMove *vsm;

    /* initialize customdata */
    scroller_activate_init(C, op, event, in_scroller);
    vsm = (v2dScrollerMove *)op->customdata;

    /* support for quick jump to location - gtk and qt do this on linux */
    if (event->type == MIDDLEMOUSE) {
      switch (vsm->scroller) {
        case 'h': /* horizontal scroller - so only horizontal movement
                   * ('cur' moves opposite to mouse) */
          vsm->delta = (float)(event->x - vsm->scrollbar_orig);
          break;
        case 'v': /* vertical scroller - so only vertical movement
                   * ('cur' moves opposite to mouse) */
          vsm->delta = (float)(event->y - vsm->scrollbar_orig);
          break;
      }
      scroller_activate_apply(C, op);

      vsm->zone = SCROLLHANDLE_BAR;
    }

    /* check if zoom zones are inappropriate (i.e. zoom widgets not shown), so cannot continue
     * NOTE: see view2d.c for latest conditions, and keep this in sync with that
     */
    if (ELEM(vsm->zone, SCROLLHANDLE_MIN, SCROLLHANDLE_MAX)) {
      if (((vsm->scroller == 'h') && (v2d->scroll & V2D_SCROLL_HORIZONTAL_HANDLES) == 0) ||
          ((vsm->scroller == 'v') && (v2d->scroll & V2D_SCROLL_VERTICAL_HANDLES) == 0)) {
        /* switch to bar (i.e. no scaling gets handled) */
        vsm->zone = SCROLLHANDLE_BAR;
      }
    }

    /* check if zone is inappropriate (i.e. 'bar' but panning is banned), so cannot continue */
    if (vsm->zone == SCROLLHANDLE_BAR) {
      if (((vsm->scroller == 'h') && (v2d->keepofs & V2D_LOCKOFS_X)) ||
          ((vsm->scroller == 'v') && (v2d->keepofs & V2D_LOCKOFS_Y))) {
        /* free customdata initialized */
        scroller_activate_exit(C, op);

        /* can't catch this event for ourselves, so let it go to someone else? */
        return OPERATOR_PASS_THROUGH;
      }
    }

    /* zone is also inappropriate if scroller is not visible... */
    if (((vsm->scroller == 'h') && (v2d->scroll & (V2D_SCROLL_HORIZONTAL_FULLR))) ||
        ((vsm->scroller == 'v') && (v2d->scroll & (V2D_SCROLL_VERTICAL_FULLR)))) {
      /* free customdata initialized */
      scroller_activate_exit(C, op);

      /* can't catch this event for ourselves, so let it go to someone else? */
      /* XXX note: if handlers use mask rect to clip input, input will fail for this case */
      return OPERATOR_PASS_THROUGH;
    }

    /* activate the scroller */
    if (vsm->scroller == 'h') {
      v2d->scroll_ui |= V2D_SCROLL_H_ACTIVE;
    }
    else {
      v2d->scroll_ui |= V2D_SCROLL_V_ACTIVE;
    }

    /* still ok, so can add */
    WM_event_add_modal_handler(C, op);
    return OPERATOR_RUNNING_MODAL;
  }
  else {
    /* not in scroller, so nothing happened...
     * (pass through let's something else catch event) */
    return OPERATOR_PASS_THROUGH;
  }
}

/* LMB-Drag in Scrollers - not repeatable operator! */
static void VIEW2D_OT_scroller_activate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Scroller Activate";
  ot->description = "Scroll view by mouse click and drag";
  ot->idname = "VIEW2D_OT_scroller_activate";

  /* flags */
  ot->flag = OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = scroller_activate_invoke;
  ot->modal = scroller_activate_modal;
  ot->cancel = scroller_activate_cancel;

  ot->poll = scroller_activate_poll;
}

/* ********************************************************* */
/* RESET */

static int reset_exec(bContext *C, wmOperator *UNUSED(op))
{
  uiStyle *style = UI_style_get();
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = &ar->v2d;
  int winx, winy;
  const int snap_test = ED_region_snap_size_test(ar);

  /* zoom 1.0 */
  winx = (float)(BLI_rcti_size_x(&v2d->mask) + 1);
  winy = (float)(BLI_rcti_size_y(&v2d->mask) + 1);

  v2d->cur.xmax = v2d->cur.xmin + winx;
  v2d->cur.ymax = v2d->cur.ymin + winy;

  /* align */
  if (v2d->align) {
    /* posx and negx flags are mutually exclusive, so watch out */
    if ((v2d->align & V2D_ALIGN_NO_POS_X) && !(v2d->align & V2D_ALIGN_NO_NEG_X)) {
      v2d->cur.xmax = 0.0f;
      v2d->cur.xmin = -winx * style->panelzoom;
    }
    else if ((v2d->align & V2D_ALIGN_NO_NEG_X) && !(v2d->align & V2D_ALIGN_NO_POS_X)) {
      v2d->cur.xmax = winx * style->panelzoom;
      v2d->cur.xmin = 0.0f;
    }

    /* - posx and negx flags are mutually exclusive, so watch out */
    if ((v2d->align & V2D_ALIGN_NO_POS_Y) && !(v2d->align & V2D_ALIGN_NO_NEG_Y)) {
      v2d->cur.ymax = 0.0f;
      v2d->cur.ymin = -winy * style->panelzoom;
    }
    else if ((v2d->align & V2D_ALIGN_NO_NEG_Y) && !(v2d->align & V2D_ALIGN_NO_POS_Y)) {
      v2d->cur.ymax = winy * style->panelzoom;
      v2d->cur.ymin = 0.0f;
    }
  }

  /* validate that view is in valid configuration after this operation */
  UI_view2d_curRect_validate(v2d);

  if (ED_region_snap_size_apply(ar, snap_test)) {
    ScrArea *sa = CTX_wm_area(C);
    ED_area_tag_redraw(sa);
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
  }

  /* request updates to be done... */
  ED_region_tag_redraw(ar);
  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);

  UI_view2d_zoom_cache_reset();

  return OPERATOR_FINISHED;
}

static void VIEW2D_OT_reset(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset View";
  ot->description = "Reset the view";
  ot->idname = "VIEW2D_OT_reset";

  /* api callbacks */
  ot->exec = reset_exec;
  ot->poll = view2d_poll;
}

/* ********************************************************* */
/* Registration */

void ED_operatortypes_view2d(void)
{
  WM_operatortype_append(VIEW2D_OT_pan);

  WM_operatortype_append(VIEW2D_OT_scroll_left);
  WM_operatortype_append(VIEW2D_OT_scroll_right);
  WM_operatortype_append(VIEW2D_OT_scroll_up);
  WM_operatortype_append(VIEW2D_OT_scroll_down);

  WM_operatortype_append(VIEW2D_OT_zoom_in);
  WM_operatortype_append(VIEW2D_OT_zoom_out);

  WM_operatortype_append(VIEW2D_OT_zoom);
  WM_operatortype_append(VIEW2D_OT_zoom_border);

#ifdef WITH_INPUT_NDOF
  WM_operatortype_append(VIEW2D_OT_ndof);
#endif

  WM_operatortype_append(VIEW2D_OT_smoothview);

  WM_operatortype_append(VIEW2D_OT_scroller_activate);

  WM_operatortype_append(VIEW2D_OT_reset);
}

void ED_keymap_view2d(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "View2D", 0, 0);
}
