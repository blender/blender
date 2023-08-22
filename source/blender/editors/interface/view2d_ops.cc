/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <cmath>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "PIL_time.h" /* USER_ZOOM_CONTINUE */

#include "view2d_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

static bool view2d_poll(bContext *C)
{
  ARegion *region = CTX_wm_region(C);

  return (region != nullptr) && (region->v2d.flag & V2D_IS_INIT);
}

/**
 * Calculate a scrolling delta that is the closest to a multiple of the page size (as returned by
 * #view2d_page_size_y). So when scrolling for more than half a page size, a delta to the next page
 * is returned. No scrolling change should be applied when this returns 0.
 */
static float view2d_scroll_delta_y_snap_page_size(const View2D &v2d, const float delta_y)
{
  const float page_size = view2d_page_size_y(v2d);
  const int delta_pages = int((delta_y - page_size * 0.5f) / page_size);

  /* Apply no change, don't update last coordinates. */
  if (abs(delta_pages) < 1) {
    return 0.0f;
  }

  /* Snap the delta to a multiple of a page size. */
  return delta_pages * page_size;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Pan Shared Utilities
 * \{ */

/**
 * This group of operators come in several forms:
 * -# Modal 'dragging' with MMB - where movement of mouse dictates amount to pan view by
 * -# Scroll-wheel 'steps' - rolling mouse-wheel by one step moves view by predefined amount
 *
 * In order to make sure this works, each operator must define the following RNA-Operator Props:
 * - `deltax, deltay` - define how much to move view by (relative to zoom-correction factor)
 */

/**
 * Temporary custom-data for operator.
 */
struct v2dViewPanData {
  /** screen where view pan was initiated */
  bScreen *screen;
  /** area where view pan was initiated */
  ScrArea *area;
  /** region where view pan was initiated */
  ARegion *region;
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

  /** Tag if the scroll is done in the category tab. */
  bool do_category_scroll;

  /** for MMB in scrollers (old feature in past, but now not that useful) */
  short in_scroller;

  /* View2D Edge Panning */
  double edge_pan_last_time;
  double edge_pan_start_time_x, edge_pan_start_time_y;
};

static bool view_pan_poll(bContext *C)
{
  ARegion *region = CTX_wm_region(C);

  /* check if there's a region in context to work with */
  if (region == nullptr) {
    return false;
  }

  View2D *v2d = &region->v2d;

  /* check that 2d-view can pan */
  if ((v2d->keepofs & V2D_LOCKOFS_X) && (v2d->keepofs & V2D_LOCKOFS_Y)) {
    return false;
  }

  /* view can pan */
  return true;
}

/* initialize panning customdata */
static void view_pan_init(bContext *C, wmOperator *op)
{
  /* Should've been checked before. */
  BLI_assert(view_pan_poll(C));

  /* set custom-data for operator */
  v2dViewPanData *vpd = MEM_cnew<v2dViewPanData>(__func__);
  op->customdata = vpd;

  /* set pointers to owners */
  vpd->screen = CTX_wm_screen(C);
  vpd->area = CTX_wm_area(C);
  vpd->region = CTX_wm_region(C);
  vpd->v2d = &vpd->region->v2d;

  /* calculate translation factor - based on size of view */
  const float winx = float(BLI_rcti_size_x(&vpd->region->winrct) + 1);
  const float winy = float(BLI_rcti_size_y(&vpd->region->winrct) + 1);
  vpd->facx = BLI_rctf_size_x(&vpd->v2d->cur) / winx;
  vpd->facy = BLI_rctf_size_y(&vpd->v2d->cur) / winy;

  vpd->v2d->flag |= V2D_IS_NAVIGATING;

  vpd->do_category_scroll = false;
}

/* apply transform to view (i.e. adjust 'cur' rect) */
static void view_pan_apply_ex(bContext *C, v2dViewPanData *vpd, float dx, float dy)
{
  View2D *v2d = vpd->v2d;

  /* calculate amount to move view by */
  dx *= vpd->facx;
  dy *= vpd->facy;

  if (!vpd->do_category_scroll) {
    /* only move view on an axis if change is allowed */
    if ((v2d->keepofs & V2D_LOCKOFS_X) == 0) {
      v2d->cur.xmin += dx;
      v2d->cur.xmax += dx;
    }
    if ((v2d->keepofs & V2D_LOCKOFS_Y) == 0) {
      v2d->cur.ymin += dy;
      v2d->cur.ymax += dy;
    }
  }
  else {
    vpd->region->category_scroll -= dy;
  }

  /* Inform v2d about changes after this operation. */
  UI_view2d_curRect_changed(C, v2d);

  /* don't rebuild full tree in outliner, since we're just changing our view */
  ED_region_tag_redraw_no_rebuild(vpd->region);

  /* request updates to be done... */
  WM_event_add_mousemove(CTX_wm_window(C));

  UI_view2d_sync(vpd->screen, vpd->area, v2d, V2D_LOCK_COPY);
}

static void view_pan_apply(bContext *C, wmOperator *op)
{
  v2dViewPanData *vpd = static_cast<v2dViewPanData *>(op->customdata);

  view_pan_apply_ex(C, vpd, RNA_int_get(op->ptr, "deltax"), RNA_int_get(op->ptr, "deltay"));
}

/* Cleanup temp custom-data. */
static void view_pan_exit(wmOperator *op)
{
  v2dViewPanData *vpd = static_cast<v2dViewPanData *>(op->customdata);
  vpd->v2d->flag &= ~V2D_IS_NAVIGATING;
  MEM_SAFE_FREE(op->customdata);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Pan Operator (modal drag-pan)
 * \{ */

/* for 'redo' only, with no user input */
static int view_pan_exec(bContext *C, wmOperator *op)
{
  view_pan_init(C, op);
  view_pan_apply(C, op);
  view_pan_exit(op);
  return OPERATOR_FINISHED;
}

/* set up modal operator and relevant settings */
static int view_pan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *window = CTX_wm_window(C);

  /* set up customdata */
  view_pan_init(C, op);

  v2dViewPanData *vpd = static_cast<v2dViewPanData *>(op->customdata);
  View2D *v2d = vpd->v2d;

  /* set initial settings */
  vpd->startx = vpd->lastx = event->xy[0];
  vpd->starty = vpd->lasty = event->xy[1];
  vpd->invoke_event = event->type;

  vpd->do_category_scroll = ED_region_panel_category_gutter_isect_xy(vpd->region, event->xy);

  if (event->type == MOUSEPAN) {
    RNA_int_set(op->ptr, "deltax", event->prev_xy[0] - event->xy[0]);
    RNA_int_set(op->ptr, "deltay", event->prev_xy[1] - event->xy[1]);

    view_pan_apply(C, op);
    view_pan_exit(op);
    return OPERATOR_FINISHED;
  }

  RNA_int_set(op->ptr, "deltax", 0);
  RNA_int_set(op->ptr, "deltay", 0);

  if (v2d->keepofs & V2D_LOCKOFS_X) {
    WM_cursor_modal_set(window, WM_CURSOR_NS_SCROLL);
  }
  else if (v2d->keepofs & V2D_LOCKOFS_Y) {
    WM_cursor_modal_set(window, WM_CURSOR_EW_SCROLL);
  }
  else {
    WM_cursor_modal_set(window, WM_CURSOR_NSEW_SCROLL);
  }

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* handle user input - calculations of mouse-movement
 * need to be done here, not in the apply callback! */
static int view_pan_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  v2dViewPanData *vpd = static_cast<v2dViewPanData *>(op->customdata);
  View2D *v2d = vpd->v2d;

  /* execute the events */
  switch (event->type) {
    case MOUSEMOVE: {
      /* calculate new delta transform, then store mouse-coordinates for next-time */
      int deltax = vpd->lastx - event->xy[0];
      int deltay = vpd->lasty - event->xy[1];

      /* Page snapping: When panning for more than half a page size, snap to the next page. */
      if (v2d->flag & V2D_SNAP_TO_PAGESIZE_Y) {
        deltay = view2d_scroll_delta_y_snap_page_size(*v2d, deltay);
      }

      if (deltax != 0) {
        RNA_int_set(op->ptr, "deltax", deltax);
        vpd->lastx = event->xy[0];
      }
      if (deltay != 0) {
        RNA_int_set(op->ptr, "deltay", deltay);
        vpd->lasty = event->xy[1];
      }

      if (deltax || deltay) {
        view_pan_apply(C, op);
      }
      break;
    }
    /* XXX: Mode switching isn't implemented. See comments in 36818.
     * switch to zoom */
#if 0
    case LEFTMOUSE:
      if (event->val == KM_PRESS) {
        /* calculate overall delta mouse-movement for redo */
        RNA_int_set(op->ptr, "deltax", (vpd->startx - vpd->lastx));
        RNA_int_set(op->ptr, "deltay", (vpd->starty - vpd->lasty));

        view_pan_exit(op);
        WM_cursor_modal_restore(CTX_wm_window(C));
        WM_operator_name_call(C, "VIEW2D_OT_zoom", WM_OP_INVOKE_DEFAULT, nullptr, event);
        return OPERATOR_FINISHED;
      }
#endif
    default:
      if (ELEM(event->type, vpd->invoke_event, EVT_ESCKEY)) {
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

static void view_pan_cancel(bContext * /*C*/, wmOperator *op)
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
  ot->poll = view_pan_poll;

  /* operator is modal */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* rna - must keep these in sync with the other operators */
  RNA_def_int(ot->srna, "deltax", 0, INT_MIN, INT_MAX, "Delta X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "deltay", 0, INT_MIN, INT_MAX, "Delta Y", "", INT_MIN, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Edge Pan Operator (modal)
 *
 * Scroll the region if the mouse is dragged to an edge. "Invisible" operator that always
 * passes through.
 * \{ */

/* set up modal operator and relevant settings */
static int view_edge_pan_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  op->customdata = MEM_callocN(sizeof(View2DEdgePanData), "View2DEdgePanData");
  View2DEdgePanData *vpd = static_cast<View2DEdgePanData *>(op->customdata);
  UI_view2d_edge_pan_operator_init(C, vpd, op);

  WM_event_add_modal_handler(C, op);

  return (OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH);
}

static int view_edge_pan_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  View2DEdgePanData *vpd = static_cast<View2DEdgePanData *>(op->customdata);

  wmWindow *source_win = CTX_wm_window(C);
  int r_mval[2];
  wmWindow *target_win = WM_window_find_under_cursor(source_win, event->xy, &r_mval[0]);

  /* Exit if we release the mouse button, hit escape, or enter a different window. */
  if (event->val == KM_RELEASE || event->type == EVT_ESCKEY || source_win != target_win) {
    vpd->v2d->flag &= ~V2D_IS_NAVIGATING;
    MEM_SAFE_FREE(op->customdata);
    return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
  }

  UI_view2d_edge_pan_apply_event(C, vpd, event);

  /* This operator is supposed to run together with some drag action.
   * On successful handling, always pass events on to other handlers. */
  return OPERATOR_PASS_THROUGH;
}

static void view_edge_pan_cancel(bContext * /*C*/, wmOperator *op)
{
  v2dViewPanData *vpd = static_cast<v2dViewPanData *>(op->customdata);
  vpd->v2d->flag &= ~V2D_IS_NAVIGATING;
  MEM_SAFE_FREE(op->customdata);
}

static void VIEW2D_OT_edge_pan(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Edge Pan";
  ot->description = "Pan the view when the mouse is held at an edge";
  ot->idname = "VIEW2D_OT_edge_pan";

  /* api callbacks */
  ot->invoke = view_edge_pan_invoke;
  ot->modal = view_edge_pan_modal;
  ot->cancel = view_edge_pan_cancel;
  ot->poll = view2d_edge_pan_poll;

  /* operator is modal */
  ot->flag = OPTYPE_INTERNAL;
  UI_view2d_edge_pan_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Pan Operator (single step)
 * \{ */

/* this operator only needs this single callback, where it calls the view_pan_*() methods */
static int view_scrollright_exec(bContext *C, wmOperator *op)
{
  /* initialize default settings (and validate if ok to run) */
  view_pan_init(C, op);

  /* also, check if can pan in horizontal axis */
  v2dViewPanData *vpd = static_cast<v2dViewPanData *>(op->customdata);
  if (vpd->v2d->keepofs & V2D_LOCKOFS_X) {
    view_pan_exit(op);
    return OPERATOR_PASS_THROUGH;
  }

  /* set RNA-Props - only movement in positive x-direction */
  RNA_int_set(op->ptr, "deltax", 40 * UI_SCALE_FAC);
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
  ot->poll = view_pan_poll;

  /* rna - must keep these in sync with the other operators */
  RNA_def_int(ot->srna, "deltax", 0, INT_MIN, INT_MAX, "Delta X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "deltay", 0, INT_MIN, INT_MAX, "Delta Y", "", INT_MIN, INT_MAX);
}

/* this operator only needs this single callback, where it calls the view_pan_*() methods */
static int view_scrollleft_exec(bContext *C, wmOperator *op)
{
  /* initialize default settings (and validate if ok to run) */
  view_pan_init(C, op);

  /* also, check if can pan in horizontal axis */
  v2dViewPanData *vpd = static_cast<v2dViewPanData *>(op->customdata);
  if (vpd->v2d->keepofs & V2D_LOCKOFS_X) {
    view_pan_exit(op);
    return OPERATOR_PASS_THROUGH;
  }

  /* set RNA-Props - only movement in negative x-direction */
  RNA_int_set(op->ptr, "deltax", -40 * UI_SCALE_FAC);
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
  ot->poll = view_pan_poll;

  /* rna - must keep these in sync with the other operators */
  RNA_def_int(ot->srna, "deltax", 0, INT_MIN, INT_MAX, "Delta X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "deltay", 0, INT_MIN, INT_MAX, "Delta Y", "", INT_MIN, INT_MAX);
}

/* this operator only needs this single callback, where it calls the view_pan_*() methods */
static int view_scrolldown_exec(bContext *C, wmOperator *op)
{
  /* initialize default settings (and validate if ok to run) */
  view_pan_init(C, op);

  /* also, check if can pan in vertical axis */
  v2dViewPanData *vpd = static_cast<v2dViewPanData *>(op->customdata);
  if (vpd->v2d->keepofs & V2D_LOCKOFS_Y) {
    view_pan_exit(op);
    return OPERATOR_PASS_THROUGH;
  }

  const wmWindow *win = CTX_wm_window(C);
  vpd->do_category_scroll = ED_region_panel_category_gutter_isect_xy(vpd->region,
                                                                     win->eventstate->xy);

  /* set RNA-Props */
  RNA_int_set(op->ptr, "deltax", 0);
  RNA_int_set(op->ptr, "deltay", -40 * UI_SCALE_FAC);

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "page");
  const bool use_page_size = (vpd->v2d->flag & V2D_SNAP_TO_PAGESIZE_Y) ||
                             (RNA_property_is_set(op->ptr, prop) &&
                              RNA_property_boolean_get(op->ptr, prop));
  if (use_page_size) {
    const ARegion *region = CTX_wm_region(C);
    const int page_size = view2d_page_size_y(region->v2d);
    RNA_int_set(op->ptr, "deltay", -page_size);
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
  ot->poll = view_pan_poll;

  /* rna - must keep these in sync with the other operators */
  RNA_def_int(ot->srna, "deltax", 0, INT_MIN, INT_MAX, "Delta X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "deltay", 0, INT_MIN, INT_MAX, "Delta Y", "", INT_MIN, INT_MAX);
  RNA_def_boolean(ot->srna, "page", false, "Page", "Scroll down one page");
}

/* this operator only needs this single callback, where it calls the view_pan_*() methods */
static int view_scrollup_exec(bContext *C, wmOperator *op)
{
  /* initialize default settings (and validate if ok to run) */
  view_pan_init(C, op);

  /* also, check if can pan in vertical axis */
  v2dViewPanData *vpd = static_cast<v2dViewPanData *>(op->customdata);
  if (vpd->v2d->keepofs & V2D_LOCKOFS_Y) {
    view_pan_exit(op);
    return OPERATOR_PASS_THROUGH;
  }

  const wmWindow *win = CTX_wm_window(C);
  vpd->do_category_scroll = ED_region_panel_category_gutter_isect_xy(vpd->region,
                                                                     win->eventstate->xy);

  /* set RNA-Props */
  RNA_int_set(op->ptr, "deltax", 0);
  RNA_int_set(op->ptr, "deltay", 40 * UI_SCALE_FAC);

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "page");
  const bool use_page_size = (vpd->v2d->flag & V2D_SNAP_TO_PAGESIZE_Y) ||
                             (RNA_property_is_set(op->ptr, prop) &&
                              RNA_property_boolean_get(op->ptr, prop));
  if (use_page_size) {
    const ARegion *region = CTX_wm_region(C);
    const int page_size = view2d_page_size_y(region->v2d);
    RNA_int_set(op->ptr, "deltay", page_size);
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
  ot->poll = view_pan_poll;

  /* rna - must keep these in sync with the other operators */
  RNA_def_int(ot->srna, "deltax", 0, INT_MIN, INT_MAX, "Delta X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "deltay", 0, INT_MIN, INT_MAX, "Delta Y", "", INT_MIN, INT_MAX);
  RNA_def_boolean(ot->srna, "page", false, "Page", "Scroll up one page");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom Shared Utilities
 * \{ */

/**
 * This group of operators come in several forms:
 * -# Scroll-wheel 'steps' - rolling mouse-wheel by one step zooms view by predefined amount.
 * -# Scroll-wheel 'steps' + alt + ctrl/shift - zooms view on one axis only (ctrl=x, shift=y).
 *    XXX this could be implemented...
 * -# Pad +/- Keys - pressing each key moves the zooms the view by a predefined amount.
 *
 * In order to make sure this works, each operator must define the following RNA-Operator Props:
 *
 * - zoomfacx, zoomfacy - These two zoom factors allow for non-uniform scaling.
 *   It is safe to scale by 0, as these factors are used to determine.
 *   amount to enlarge 'cur' by.
 */

/**
 * Temporary custom-data for operator.
 */
struct v2dViewZoomData {
  View2D *v2d; /* view2d we're operating in */
  ARegion *region;

  /* needed for continuous zoom */
  wmTimer *timer;
  double timer_lastdraw;

  int lastx, lasty;   /* previous x/y values of mouse in window */
  int invoke_event;   /* event type that invoked, for modal exits */
  float dx, dy;       /* running tally of previous delta values (for obtaining final zoom) */
  float mx_2d, my_2d; /* initial mouse location in v2d coords */
  bool zoom_to_mouse_pos;
};

/**
 * Clamp by convention rather than locking flags,
 * for ndof and +/- keys
 */
static void view_zoom_axis_lock_defaults(bContext *C, bool r_do_zoom_xy[2])
{
  ScrArea *area = CTX_wm_area(C);

  r_do_zoom_xy[0] = true;
  r_do_zoom_xy[1] = true;

  /* default not to zoom the sequencer vertically */
  if (area && area->spacetype == SPACE_SEQ) {
    ARegion *region = CTX_wm_region(C);

    if (region && region->regiontype == RGN_TYPE_WINDOW) {
      r_do_zoom_xy[1] = false;
    }
  }
}

/* check if step-zoom can be applied */
static bool view_zoom_poll(bContext *C)
{
  ARegion *region = CTX_wm_region(C);

  /* check if there's a region in context to work with */
  if (region == nullptr) {
    return false;
  }

  /* Do not show that in 3DView context. */
  if (CTX_wm_region_view3d(C)) {
    return false;
  }

  View2D *v2d = &region->v2d;

  /* check that 2d-view is zoomable */
  if ((v2d->keepzoom & V2D_LOCKZOOM_X) && (v2d->keepzoom & V2D_LOCKZOOM_Y)) {
    return false;
  }

  /* view is zoomable */
  return true;
}

/* initialize panning customdata */
static void view_zoomdrag_init(bContext *C, wmOperator *op)
{
  /* Should've been checked before. */
  BLI_assert(view_zoom_poll(C));

  /* set custom-data for operator */
  v2dViewZoomData *vzd = MEM_cnew<v2dViewZoomData>(__func__);
  op->customdata = vzd;

  /* set pointers to owners */
  vzd->region = CTX_wm_region(C);
  vzd->v2d = &vzd->region->v2d;
  /* False by default. Interactive callbacks (ie invoke()) can set it to true. */
  vzd->zoom_to_mouse_pos = false;

  vzd->v2d->flag |= V2D_IS_NAVIGATING;
}

/* apply transform to view (i.e. adjust 'cur' rect) */
static void view_zoomstep_apply_ex(bContext *C,
                                   v2dViewZoomData *vzd,
                                   const float facx,
                                   const float facy)
{
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;
  const rctf cur_old = v2d->cur;
  const int snap_test = ED_region_snap_size_test(region);

  /* calculate amount to move view by, ensuring symmetry so the
   * old zoom level is restored after zooming back the same amount
   */
  float dx, dy;
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

      if (vzd->zoom_to_mouse_pos) {
        /* get zoom fac the same way as in
         * ui_view2d_curRect_validate_resize - better keep in sync! */
        const float zoomx = float(BLI_rcti_size_x(&v2d->mask) + 1) / BLI_rctf_size_x(&v2d->cur);

        /* only move view to mouse if zoom fac is inside minzoom/maxzoom */
        if (((v2d->keepzoom & V2D_LIMITZOOM) == 0) ||
            IN_RANGE_INCL(zoomx, v2d->minzoom, v2d->maxzoom)) {
          const float mval_fac = (vzd->mx_2d - cur_old.xmin) / BLI_rctf_size_x(&cur_old);
          const float mval_faci = 1.0f - mval_fac;
          const float ofs = (mval_fac * dx) - (mval_faci * dx);

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

      if (vzd->zoom_to_mouse_pos) {
        /* get zoom fac the same way as in
         * ui_view2d_curRect_validate_resize - better keep in sync! */
        const float zoomy = float(BLI_rcti_size_y(&v2d->mask) + 1) / BLI_rctf_size_y(&v2d->cur);

        /* only move view to mouse if zoom fac is inside minzoom/maxzoom */
        if (((v2d->keepzoom & V2D_LIMITZOOM) == 0) ||
            IN_RANGE_INCL(zoomy, v2d->minzoom, v2d->maxzoom)) {
          const float mval_fac = (vzd->my_2d - cur_old.ymin) / BLI_rctf_size_y(&cur_old);
          const float mval_faci = 1.0f - mval_fac;
          const float ofs = (mval_fac * dy) - (mval_faci * dy);

          v2d->cur.ymin += ofs;
          v2d->cur.ymax += ofs;
        }
      }
    }
  }

  /* Inform v2d about changes after this operation. */
  UI_view2d_curRect_changed(C, v2d);

  if (ED_region_snap_size_apply(region, snap_test)) {
    ScrArea *area = CTX_wm_area(C);
    ED_area_tag_redraw(area);
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
  }

  /* request updates to be done... */
  ED_region_tag_redraw_no_rebuild(vzd->region);
  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
}

static void view_zoomstep_apply(bContext *C, wmOperator *op)
{
  v2dViewZoomData *vzd = static_cast<v2dViewZoomData *>(op->customdata);
  view_zoomstep_apply_ex(
      C, vzd, RNA_float_get(op->ptr, "zoomfacx"), RNA_float_get(op->ptr, "zoomfacy"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom Operator (single step)
 * \{ */

/* Cleanup temp custom-data. */
static void view_zoomstep_exit(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  /* Some areas change font sizes when zooming, so clear glyph cache. */
  if (area && !ELEM(area->spacetype, SPACE_GRAPH, SPACE_ACTION)) {
    UI_view2d_zoom_cache_reset();
  }

  v2dViewZoomData *vzd = static_cast<v2dViewZoomData *>(op->customdata);
  vzd->v2d->flag &= ~V2D_IS_NAVIGATING;
  MEM_SAFE_FREE(op->customdata);
}

/* this operator only needs this single callback, where it calls the view_zoom_*() methods */
static int view_zoomin_exec(bContext *C, wmOperator *op)
{
  if (op->customdata == nullptr) { /* Might have been setup in _invoke() already. */
    view_zoomdrag_init(C, op);
  }

  bool do_zoom_xy[2];
  view_zoom_axis_lock_defaults(C, do_zoom_xy);

  /* set RNA-Props - zooming in by uniform factor */
  RNA_float_set(op->ptr, "zoomfacx", do_zoom_xy[0] ? 0.0375f : 0.0f);
  RNA_float_set(op->ptr, "zoomfacy", do_zoom_xy[1] ? 0.0375f : 0.0f);

  /* apply movement, then we're done */
  view_zoomstep_apply(C, op);

  view_zoomstep_exit(C, op);

  return OPERATOR_FINISHED;
}

static int view_zoomin_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  view_zoomdrag_init(C, op);

  v2dViewZoomData *vzd = static_cast<v2dViewZoomData *>(op->customdata);

  if (U.uiflag & USER_ZOOM_TO_MOUSEPOS) {
    ARegion *region = CTX_wm_region(C);

    /* store initial mouse position (in view space) */
    UI_view2d_region_to_view(
        &region->v2d, event->mval[0], event->mval[1], &vzd->mx_2d, &vzd->my_2d);
    vzd->zoom_to_mouse_pos = true;
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
  ot->exec = view_zoomin_exec;
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

  if (op->customdata == nullptr) { /* Might have been setup in _invoke() already. */
    view_zoomdrag_init(C, op);
  }

  view_zoom_axis_lock_defaults(C, do_zoom_xy);

  /* set RNA-Props - zooming in by uniform factor */
  RNA_float_set(op->ptr, "zoomfacx", do_zoom_xy[0] ? -0.0375f : 0.0f);
  RNA_float_set(op->ptr, "zoomfacy", do_zoom_xy[1] ? -0.0375f : 0.0f);

  /* apply movement, then we're done */
  view_zoomstep_apply(C, op);

  view_zoomstep_exit(C, op);

  return OPERATOR_FINISHED;
}

static int view_zoomout_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  view_zoomdrag_init(C, op);

  v2dViewZoomData *vzd = static_cast<v2dViewZoomData *>(op->customdata);

  if (U.uiflag & USER_ZOOM_TO_MOUSEPOS) {
    ARegion *region = CTX_wm_region(C);

    /* store initial mouse position (in view space) */
    UI_view2d_region_to_view(
        &region->v2d, event->mval[0], event->mval[1], &vzd->mx_2d, &vzd->my_2d);
    vzd->zoom_to_mouse_pos = true;
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
  ot->exec = view_zoomout_exec;

  ot->poll = view_zoom_poll;

  /* rna - must keep these in sync with the other operators */
  prop = RNA_def_float(
      ot->srna, "zoomfacx", 0, -FLT_MAX, FLT_MAX, "Zoom Factor X", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_float(
      ot->srna, "zoomfacy", 0, -FLT_MAX, FLT_MAX, "Zoom Factor Y", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom Operator (modal drag-zoom)
 * \{ */

/**
 * MMB Drag - allows non-uniform scaling by dragging mouse
 *
 * In order to make sure this works, each operator must define the following RNA-Operator Props:
 * - `deltax, deltay` - amounts to add to each side of the 'cur' rect
 */

/* apply transform to view (i.e. adjust 'cur' rect) */
static void view_zoomdrag_apply(bContext *C, wmOperator *op)
{
  v2dViewZoomData *vzd = static_cast<v2dViewZoomData *>(op->customdata);
  View2D *v2d = vzd->v2d;
  const int snap_test = ED_region_snap_size_test(vzd->region);

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");
  const bool zoom_to_pos = use_cursor_init && vzd->zoom_to_mouse_pos;

  /* get amount to move view by */
  float dx = RNA_float_get(op->ptr, "deltax") / UI_SCALE_FAC;
  float dy = RNA_float_get(op->ptr, "deltay") / UI_SCALE_FAC;

  /* Check if the 'timer' is initialized, as zooming with the trackpad
   * never uses the "Continuous" zoom method, and the 'timer' is not initialized. */
  if ((U.viewzoom == USER_ZOOM_CONTINUE) && vzd->timer) { /* XXX store this setting as RNA prop? */
    const double time = PIL_check_seconds_timer();
    const float time_step = float(time - vzd->timer_lastdraw);

    dx *= time_step * 5.0f;
    dy *= time_step * 5.0f;

    vzd->timer_lastdraw = time;
  }

  /* only move view on an axis if change is allowed */
  if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0) {
    if (v2d->keepofs & V2D_LOCKOFS_X) {
      v2d->cur.xmax -= 2 * dx;
    }
    else {
      if (zoom_to_pos) {
        const float mval_fac = (vzd->mx_2d - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur);
        const float mval_faci = 1.0f - mval_fac;
        const float ofs = (mval_fac * dx) - (mval_faci * dx);

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
        const float mval_fac = (vzd->my_2d - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur);
        const float mval_faci = 1.0f - mval_fac;
        const float ofs = (mval_fac * dy) - (mval_faci * dy);

        v2d->cur.ymin += ofs + dy;
        v2d->cur.ymax += ofs - dy;
      }
      else {
        v2d->cur.ymin += dy;
        v2d->cur.ymax -= dy;
      }
    }
  }

  /* Inform v2d about changes after this operation. */
  UI_view2d_curRect_changed(C, v2d);

  if (ED_region_snap_size_apply(vzd->region, snap_test)) {
    ScrArea *area = CTX_wm_area(C);
    ED_area_tag_redraw(area);
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
  }

  /* request updates to be done... */
  ED_region_tag_redraw_no_rebuild(vzd->region);
  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
}

/* Cleanup temp custom-data. */
static void view_zoomdrag_exit(bContext *C, wmOperator *op)
{
  UI_view2d_zoom_cache_reset();

  if (op->customdata) {
    v2dViewZoomData *vzd = static_cast<v2dViewZoomData *>(op->customdata);
    vzd->v2d->flag &= ~V2D_IS_NAVIGATING;

    if (vzd->timer) {
      WM_event_timer_remove(CTX_wm_manager(C), CTX_wm_window(C), vzd->timer);
    }

    MEM_freeN(op->customdata);
    op->customdata = nullptr;
  }
}

static void view_zoomdrag_cancel(bContext *C, wmOperator *op)
{
  view_zoomdrag_exit(C, op);
}

/* for 'redo' only, with no user input */
static int view_zoomdrag_exec(bContext *C, wmOperator *op)
{
  view_zoomdrag_init(C, op);
  view_zoomdrag_apply(C, op);
  view_zoomdrag_exit(C, op);
  return OPERATOR_FINISHED;
}

/* set up modal operator and relevant settings */
static int view_zoomdrag_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *window = CTX_wm_window(C);

  /* set up customdata */
  view_zoomdrag_init(C, op);

  v2dViewZoomData *vzd = static_cast<v2dViewZoomData *>(op->customdata);
  View2D *v2d = vzd->v2d;

  if (U.uiflag & USER_ZOOM_TO_MOUSEPOS) {
    ARegion *region = CTX_wm_region(C);

    /* Store initial mouse position (in view space). */
    UI_view2d_region_to_view(
        &region->v2d, event->mval[0], event->mval[1], &vzd->mx_2d, &vzd->my_2d);
    vzd->zoom_to_mouse_pos = true;
  }

  if (ELEM(event->type, MOUSEZOOM, MOUSEPAN)) {
    vzd->lastx = event->prev_xy[0];
    vzd->lasty = event->prev_xy[1];

    float facx, facy;
    float zoomfac = 0.01f;

    /* Some view2d's (graph) don't have min/max zoom, or extreme ones. */
    if (v2d->maxzoom > 0.0f) {
      zoomfac = clamp_f(0.001f * v2d->maxzoom, 0.001f, 0.01f);
    }

    if (event->type == MOUSEPAN) {
      facx = zoomfac * WM_event_absolute_delta_x(event);
      facy = zoomfac * WM_event_absolute_delta_y(event);

      if (U.uiflag & USER_ZOOM_INVERT) {
        facx *= -1.0f;
        facy *= -1.0f;
      }
    }
    else { /* MOUSEZOOM */
      facx = facy = zoomfac * WM_event_absolute_delta_x(event);
    }

    /* Only respect user setting zoom axis if the view does not have any zoom restrictions
     * any will be scaled uniformly. */
    if (((v2d->keepzoom & (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y)) == 0) &&
        (v2d->keepzoom & V2D_KEEPASPECT)) {
      if (U.uiflag & USER_ZOOM_HORIZ) {
        facy = 0.0f;
      }
      else {
        facx = 0.0f;
      }
    }

    /* support trackpad zoom to always zoom entirely - the v2d code uses portrait or
     * landscape exceptions */
    if (v2d->keepzoom & V2D_KEEPASPECT) {
      if (fabsf(facx) > fabsf(facy)) {
        facy = facx;
      }
      else {
        facx = facy;
      }
    }

    const float dx = facx * BLI_rctf_size_x(&v2d->cur);
    const float dy = facy * BLI_rctf_size_y(&v2d->cur);

    RNA_float_set(op->ptr, "deltax", dx);
    RNA_float_set(op->ptr, "deltay", dy);

    view_zoomdrag_apply(C, op);
    view_zoomdrag_exit(C, op);
    return OPERATOR_FINISHED;
  }

  /* set initial settings */
  vzd->lastx = event->xy[0];
  vzd->lasty = event->xy[1];
  RNA_float_set(op->ptr, "deltax", 0);
  RNA_float_set(op->ptr, "deltay", 0);

  /* for modal exit test */
  vzd->invoke_event = event->type;

  if (v2d->keepofs & V2D_LOCKOFS_X) {
    WM_cursor_modal_set(window, WM_CURSOR_NS_SCROLL);
  }
  else if (v2d->keepofs & V2D_LOCKOFS_Y) {
    WM_cursor_modal_set(window, WM_CURSOR_EW_SCROLL);
  }
  else {
    WM_cursor_modal_set(window, WM_CURSOR_NSEW_SCROLL);
  }

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  if (U.viewzoom == USER_ZOOM_CONTINUE) {
    /* needs a timer to continue redrawing */
    vzd->timer = WM_event_timer_add(CTX_wm_manager(C), window, TIMER, 0.01f);
    vzd->timer_lastdraw = PIL_check_seconds_timer();
  }

  return OPERATOR_RUNNING_MODAL;
}

/* handle user input - calculations of mouse-movement need to be done here,
 * not in the apply callback! */
static int view_zoomdrag_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  v2dViewZoomData *vzd = static_cast<v2dViewZoomData *>(op->customdata);
  View2D *v2d = vzd->v2d;

  /* execute the events */
  if (event->type == TIMER && event->customdata == vzd->timer) {
    view_zoomdrag_apply(C, op);
  }
  else if (event->type == MOUSEMOVE) {
    float dx, dy;
    float zoomfac = 0.01f;

    /* some view2d's (graph) don't have min/max zoom, or extreme ones */
    if (v2d->maxzoom > 0.0f) {
      zoomfac = clamp_f(0.001f * v2d->maxzoom, 0.001f, 0.01f);
    }

    /* calculate new delta transform, based on zooming mode */
    if (U.viewzoom == USER_ZOOM_SCALE) {
      /* 'scale' zooming */
      float dist;
      float len_old[2];
      float len_new[2];

      /* x-axis transform */
      dist = BLI_rcti_size_x(&v2d->mask) / 2.0f;
      len_old[0] = zoomfac * fabsf(vzd->lastx - vzd->region->winrct.xmin - dist);
      len_new[0] = zoomfac * fabsf(event->xy[0] - vzd->region->winrct.xmin - dist);

      /* y-axis transform */
      dist = BLI_rcti_size_y(&v2d->mask) / 2.0f;
      len_old[1] = zoomfac * fabsf(vzd->lasty - vzd->region->winrct.ymin - dist);
      len_new[1] = zoomfac * fabsf(event->xy[1] - vzd->region->winrct.ymin - dist);

      /* Calculate distance */
      if (v2d->keepzoom & V2D_KEEPASPECT) {
        dist = len_v2(len_new) - len_v2(len_old);
        dx = dy = dist;
      }
      else {
        dx = len_new[0] - len_old[0];
        dy = len_new[1] - len_old[1];
      }

      dx *= BLI_rctf_size_x(&v2d->cur);
      dy *= BLI_rctf_size_y(&v2d->cur);
    }
    else { /* USER_ZOOM_CONTINUE or USER_ZOOM_DOLLY */
      float facx = zoomfac * (event->xy[0] - vzd->lastx);
      float facy = zoomfac * (event->xy[1] - vzd->lasty);

      /* Only respect user setting zoom axis if the view does not have any zoom restrictions
       * any will be scaled uniformly */
      if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0 && (v2d->keepzoom & V2D_LOCKZOOM_Y) == 0 &&
          (v2d->keepzoom & V2D_KEEPASPECT))
      {
        if (U.uiflag & USER_ZOOM_HORIZ) {
          facy = 0.0f;
        }
        else {
          facx = 0.0f;
        }
      }

      /* support zoom to always zoom entirely - the v2d code uses portrait or
       * landscape exceptions */
      if (v2d->keepzoom & V2D_KEEPASPECT) {
        if (fabsf(facx) > fabsf(facy)) {
          facy = facx;
        }
        else {
          facx = facy;
        }
      }

      dx = facx * BLI_rctf_size_x(&v2d->cur);
      dy = facy * BLI_rctf_size_y(&v2d->cur);
    }

    if (U.uiflag & USER_ZOOM_INVERT) {
      dx *= -1.0f;
      dy *= -1.0f;
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
    if (U.viewzoom != USER_ZOOM_CONTINUE) { /* XXX store this setting as RNA prop? */
      vzd->lastx = event->xy[0];
      vzd->lasty = event->xy[1];
    }

    /* apply zooming */
    view_zoomdrag_apply(C, op);
  }
  else if (ELEM(event->type, vzd->invoke_event, EVT_ESCKEY)) {
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Border Zoom Operator
 *
 * The user defines a rect using standard box select tools, and we use this rect to
 * define the new zoom-level of the view in the following ways:
 *
 * -# LEFTMOUSE - zoom in to view
 * -# RIGHTMOUSE - zoom out of view
 *
 * Currently, these key mappings are hardcoded, but it shouldn't be too important to
 * have custom keymappings for this.
 * \{ */

static int view_borderzoom_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;
  rctf cur_new = v2d->cur;
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* convert coordinates of rect to 'tot' rect coordinates */
  rctf rect;
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

  UI_view2d_smooth_view(C, region, &cur_new, smooth_viewtx);

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Pan/Zoom Operator
 * \{ */

#ifdef WITH_INPUT_NDOF
static int view2d_ndof_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  const wmNDOFMotionData *ndof = static_cast<const wmNDOFMotionData *>(event->customdata);

  /* tune these until it feels right */
  const float zoom_sensitivity = 0.5f;
  const float speed = 10.0f; /* match view3d ortho */
  const bool has_translate = !is_zero_v2(ndof->tvec) && view_pan_poll(C);
  const bool has_zoom = (ndof->tvec[2] != 0.0f) && view_zoom_poll(C);

  if (has_translate) {
    float pan_vec[3];

    WM_event_ndof_pan_get(ndof, pan_vec, false);

    pan_vec[0] *= speed;
    pan_vec[1] *= speed;

    view_pan_init(C, op);

    v2dViewPanData *vpd = static_cast<v2dViewPanData *>(op->customdata);
    view_pan_apply_ex(C, vpd, pan_vec[0], pan_vec[1]);

    view_pan_exit(op);
  }

  if (has_zoom) {
    float zoom_factor = zoom_sensitivity * ndof->dt * -ndof->tvec[2];

    bool do_zoom_xy[2];

    if (U.ndof_flag & NDOF_ZOOM_INVERT) {
      zoom_factor = -zoom_factor;
    }

    view_zoom_axis_lock_defaults(C, do_zoom_xy);

    view_zoomdrag_init(C, op);

    v2dViewZoomData *vzd = static_cast<v2dViewZoomData *>(op->customdata);
    view_zoomstep_apply_ex(
        C, vzd, do_zoom_xy[0] ? zoom_factor : 0.0f, do_zoom_xy[1] ? zoom_factor : 0.0f);

    view_zoomstep_exit(C, op);
  }

  return OPERATOR_FINISHED;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smooth View Operator
 * \{ */

struct SmoothView2DStore {
  rctf orig_cur, new_cur;

  double time_allowed;
};

/**
 * function to get a factor out of a rectangle
 *
 * NOTE: this doesn't always work as well as it might because the target size
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

  for (int i = 0; i < 2; i++) {
    /* axis translation normalized to scale */
    float tfac = fabsf(cent_a[i] - cent_b[i]) / min_ff(size_a[i], size_b[i]);
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

void UI_view2d_smooth_view(const bContext *C,
                           ARegion *region,
                           const rctf *cur,
                           const int smooth_viewtx)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);

  View2D *v2d = &region->v2d;
  SmoothView2DStore sms = {{0}};
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

      sms.time_allowed = double(smooth_viewtx) / 1000.0;

      /* scale the time allowed the change in view */
      sms.time_allowed *= double(fac);

      /* keep track of running timer! */
      if (v2d->sms == nullptr) {
        v2d->sms = MEM_new<SmoothView2DStore>(__func__);
      }
      *v2d->sms = sms;
      if (v2d->smooth_timer) {
        WM_event_timer_remove(wm, win, v2d->smooth_timer);
      }
      /* TIMER1 is hard-coded in key-map. */
      v2d->smooth_timer = WM_event_timer_add(wm, win, TIMER1, 1.0 / 100.0);

      ok = true;
    }
  }

  /* if we get here nothing happens */
  if (ok == false) {
    v2d->cur = sms.new_cur;

    UI_view2d_curRect_changed(C, v2d);
    ED_region_tag_redraw_no_rebuild(region);
    UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
  }
}

/* only meant for timer usage */
static int view2d_smoothview_invoke(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;
  SmoothView2DStore *sms = v2d->sms;

  /* escape if not our timer */
  if (v2d->smooth_timer == nullptr || v2d->smooth_timer != event->customdata) {
    return OPERATOR_PASS_THROUGH;
  }

  float step;
  if (sms->time_allowed != 0.0) {
    step = float((v2d->smooth_timer->duration) / sms->time_allowed);
  }
  else {
    step = 1.0f;
  }

  /* end timer */
  if (step >= 1.0f) {
    v2d->cur = sms->new_cur;

    MEM_freeN(v2d->sms);
    v2d->sms = nullptr;

    WM_event_timer_remove(CTX_wm_manager(C), win, v2d->smooth_timer);
    v2d->smooth_timer = nullptr;

    /* Event handling won't know if a UI item has been moved under the pointer. */
    WM_event_add_mousemove(win);
  }
  else {
    /* ease in/out */
    step = (3.0f * step * step - 2.0f * step * step * step);

    BLI_rctf_interp(&v2d->cur, &sms->orig_cur, &sms->new_cur, step);
  }

  UI_view2d_curRect_changed(C, v2d);
  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
  ED_region_tag_redraw_no_rebuild(region);

  if (v2d->sms == nullptr) {
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scroll Bar Move Operator
 * \{ */

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
struct v2dScrollerMove {
  /** View2D data that this operation affects */
  View2D *v2d;
  /** region that the scroller is in */
  ARegion *region;

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
};

/* quick enum for vsm->zone (scroller handles) */
enum {
  SCROLLHANDLE_MIN = -1,
  SCROLLHANDLE_BAR,
  SCROLLHANDLE_MAX,
  SCROLLHANDLE_MIN_OUTSIDE,
  SCROLLHANDLE_MAX_OUTSIDE,
} /*eV2DScrollerHandle_Zone*/;

/**
 * Check if mouse is within scroller handle.
 *
 * \param mouse: relevant mouse coordinate in region space.
 * \param sc_min, sc_max: extents of scroller 'groove' (potential available space for scroller).
 * \param sh_min, sh_max: positions of scrollbar handles.
 */
static short mouse_in_scroller_handle(int mouse, int sc_min, int sc_max, int sh_min, int sh_max)
{
  /* firstly, check if
   * - 'bubble' fills entire scroller
   * - 'bubble' completely out of view on either side
   */
  bool in_view = true;
  if (sh_min <= sc_min && sc_max <= sh_max) {
    in_view = false;
  }
  else if (sh_max <= sc_min || sc_max <= sh_min) {
    in_view = false;
  }

  if (!in_view) {
    return SCROLLHANDLE_BAR;
  }

  /* check if mouse is in or past either handle */
  /* TODO: check if these extents are still valid or not */
  bool in_max = ((mouse >= (sh_max - V2D_SCROLL_HANDLE_SIZE_HOTSPOT)) &&
                 (mouse <= (sh_max + V2D_SCROLL_HANDLE_SIZE_HOTSPOT)));
  bool in_min = ((mouse <= (sh_min + V2D_SCROLL_HANDLE_SIZE_HOTSPOT)) &&
                 (mouse >= (sh_min - V2D_SCROLL_HANDLE_SIZE_HOTSPOT)));
  bool in_bar = ((mouse < (sh_max - V2D_SCROLL_HANDLE_SIZE_HOTSPOT)) &&
                 (mouse > (sh_min + V2D_SCROLL_HANDLE_SIZE_HOTSPOT)));
  const bool out_min = mouse < (sh_min - V2D_SCROLL_HANDLE_SIZE_HOTSPOT);
  const bool out_max = mouse > (sh_max + V2D_SCROLL_HANDLE_SIZE_HOTSPOT);

  if (in_bar) {
    return SCROLLHANDLE_BAR;
  }
  if (in_max) {
    return SCROLLHANDLE_MAX;
  }
  if (in_min) {
    return SCROLLHANDLE_MIN;
  }
  if (out_min) {
    return SCROLLHANDLE_MIN_OUTSIDE;
  }
  if (out_max) {
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
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;
  wmEvent *event = win->eventstate;

  /* Check if mouse in scroll-bars, if they're enabled. */
  return (UI_view2d_mouse_in_scrollers(region, v2d, event->xy) != 0);
}

/* Initialize #wmOperator.customdata for scroller manipulation operator. */
static void scroller_activate_init(bContext *C,
                                   wmOperator *op,
                                   const wmEvent *event,
                                   const char in_scroller)
{
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;

  /* set custom-data for operator */
  v2dScrollerMove *vsm = MEM_cnew<v2dScrollerMove>(__func__);
  op->customdata = vsm;

  /* set general data */
  vsm->v2d = v2d;
  vsm->region = region;
  vsm->scroller = in_scroller;

  /* store mouse-coordinates, and convert mouse/screen coordinates to region coordinates */
  vsm->lastx = event->xy[0];
  vsm->lasty = event->xy[1];
  /* 'zone' depends on where mouse is relative to bubble
   * - zooming must be allowed on this axis, otherwise, default to pan
   */
  View2DScrollers scrollers;
  view2d_scrollers_calc(v2d, nullptr, &scrollers);

  /* Use a union of 'cur' & 'tot' in case the current view is far outside 'tot'. In this cases
   * moving the scroll bars has far too little effect and the view can get stuck #31476. */
  rctf tot_cur_union = v2d->tot;
  BLI_rctf_union(&tot_cur_union, &v2d->cur);

  if (in_scroller == 'h') {
    /* horizontal scroller - calculate adjustment factor first */
    const float mask_size = float(BLI_rcti_size_x(&v2d->hor));
    vsm->fac = BLI_rctf_size_x(&tot_cur_union) / mask_size;

    /* pixel rounding */
    vsm->fac_round = BLI_rctf_size_x(&v2d->cur) / float(BLI_rcti_size_x(&region->winrct) + 1);

    /* get 'zone' (i.e. which part of scroller is activated) */
    vsm->zone = mouse_in_scroller_handle(
        event->mval[0], v2d->hor.xmin, v2d->hor.xmax, scrollers.hor_min, scrollers.hor_max);

    if ((v2d->keepzoom & V2D_LOCKZOOM_X) && ELEM(vsm->zone, SCROLLHANDLE_MIN, SCROLLHANDLE_MAX)) {
      /* default to scroll, as handles not usable */
      vsm->zone = SCROLLHANDLE_BAR;
    }

    vsm->scrollbarwidth = scrollers.hor_max - scrollers.hor_min;
    vsm->scrollbar_orig = ((scrollers.hor_max + scrollers.hor_min) / 2) + region->winrct.xmin;
  }
  else {
    /* vertical scroller - calculate adjustment factor first */
    const float mask_size = float(BLI_rcti_size_y(&v2d->vert));
    vsm->fac = BLI_rctf_size_y(&tot_cur_union) / mask_size;

    /* pixel rounding */
    vsm->fac_round = BLI_rctf_size_y(&v2d->cur) / float(BLI_rcti_size_y(&region->winrct) + 1);

    /* get 'zone' (i.e. which part of scroller is activated) */
    vsm->zone = mouse_in_scroller_handle(
        event->mval[1], v2d->vert.ymin, v2d->vert.ymax, scrollers.vert_min, scrollers.vert_max);

    if ((v2d->keepzoom & V2D_LOCKZOOM_Y) && ELEM(vsm->zone, SCROLLHANDLE_MIN, SCROLLHANDLE_MAX)) {
      /* default to scroll, as handles not usable */
      vsm->zone = SCROLLHANDLE_BAR;
    }

    vsm->scrollbarwidth = scrollers.vert_max - scrollers.vert_min;
    vsm->scrollbar_orig = ((scrollers.vert_max + scrollers.vert_min) / 2) + region->winrct.ymin;
  }

  vsm->v2d->flag |= V2D_IS_NAVIGATING;

  ED_region_tag_redraw_no_rebuild(region);
}

/* Cleanup temp custom-data. */
static void scroller_activate_exit(bContext *C, wmOperator *op)
{
  if (op->customdata) {
    v2dScrollerMove *vsm = static_cast<v2dScrollerMove *>(op->customdata);

    vsm->v2d->scroll_ui &= ~(V2D_SCROLL_H_ACTIVE | V2D_SCROLL_V_ACTIVE);
    vsm->v2d->flag &= ~V2D_IS_NAVIGATING;

    MEM_freeN(op->customdata);
    op->customdata = nullptr;

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
  v2dScrollerMove *vsm = static_cast<v2dScrollerMove *>(op->customdata);
  View2D *v2d = vsm->v2d;

  /* calculate amount to move view by */
  float temp = vsm->fac * vsm->delta;

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

  /* Inform v2d about changes after this operation. */
  UI_view2d_curRect_changed(C, v2d);

  /* request updates to be done... */
  ED_region_tag_redraw_no_rebuild(vsm->region);
  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
}

/**
 * Handle user input for scrollers - calculations of mouse-movement need to be done here,
 * not in the apply callback!
 */
static int scroller_activate_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  v2dScrollerMove *vsm = static_cast<v2dScrollerMove *>(op->customdata);
  const bool use_page_size_y = vsm->v2d->flag & V2D_SNAP_TO_PAGESIZE_Y;

  /* execute the events */
  switch (event->type) {
    case MOUSEMOVE: {
      float delta = 0.0f;

      /* calculate new delta transform, then store mouse-coordinates for next-time */
      if (ELEM(vsm->zone, SCROLLHANDLE_BAR, SCROLLHANDLE_MAX)) {
        /* if using bar (i.e. 'panning') or 'max' zoom widget */
        switch (vsm->scroller) {
          case 'h': /* horizontal scroller - so only horizontal movement
                     * ('cur' moves opposite to mouse) */
            delta = float(event->xy[0] - vsm->lastx);
            break;
          case 'v': /* vertical scroller - so only vertical movement
                     * ('cur' moves opposite to mouse) */
            delta = float(event->xy[1] - vsm->lasty);
            break;
        }
      }
      else if (vsm->zone == SCROLLHANDLE_MIN) {
        /* using 'min' zoom widget */
        switch (vsm->scroller) {
          case 'h': /* horizontal scroller - so only horizontal movement
                     * ('cur' moves with mouse) */
            delta = float(vsm->lastx - event->xy[0]);
            break;
          case 'v': /* vertical scroller - so only vertical movement
                     * ('cur' moves with to mouse) */
            delta = float(vsm->lasty - event->xy[1]);
            break;
        }
      }

      /* Page snapping: When panning for more than half a page size, snap to the next page. */
      if (use_page_size_y && (vsm->scroller == 'v')) {
        delta = view2d_scroll_delta_y_snap_page_size(*vsm->v2d, delta * vsm->fac) / vsm->fac;
      }

      if (IS_EQF(delta, 0.0f)) {
        break;
      }

      vsm->delta = delta;
      /* store previous coordinates */
      vsm->lastx = event->xy[0];
      vsm->lasty = event->xy[1];

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

        /* Otherwise, end the drag action. */
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
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;

  /* check if mouse in scroll-bars, if they're enabled */
  const char in_scroller = UI_view2d_mouse_in_scrollers(region, v2d, event->xy);

  /* if in a scroller, init customdata then set modal handler which will
   * catch mouse-down to start doing useful stuff */
  if (in_scroller) {
    /* initialize customdata */
    scroller_activate_init(C, op, event, in_scroller);
    v2dScrollerMove *vsm = (v2dScrollerMove *)op->customdata;

    /* Support for quick jump to location - GTK and QT do this on Linux. */
    if (event->type == MIDDLEMOUSE) {
      switch (vsm->scroller) {
        case 'h': /* horizontal scroller - so only horizontal movement
                   * ('cur' moves opposite to mouse) */
          vsm->delta = float(event->xy[0] - vsm->scrollbar_orig);
          break;
        case 'v': /* vertical scroller - so only vertical movement
                   * ('cur' moves opposite to mouse) */
          vsm->delta = float(event->xy[1] - vsm->scrollbar_orig);
          break;
      }
      scroller_activate_apply(C, op);

      vsm->zone = SCROLLHANDLE_BAR;
    }

    /* Check if zoom zones are inappropriate (i.e. zoom widgets not shown), so cannot continue
     * NOTE: see `view2d.cc` for latest conditions, and keep this in sync with that. */
    if (ELEM(vsm->zone, SCROLLHANDLE_MIN, SCROLLHANDLE_MAX)) {
      if (((vsm->scroller == 'h') && (v2d->scroll & V2D_SCROLL_HORIZONTAL_HANDLES) == 0) ||
          ((vsm->scroller == 'v') && (v2d->scroll & V2D_SCROLL_VERTICAL_HANDLES) == 0))
      {
        /* switch to bar (i.e. no scaling gets handled) */
        vsm->zone = SCROLLHANDLE_BAR;
      }
    }

    /* check if zone is inappropriate (i.e. 'bar' but panning is banned), so cannot continue */
    if (vsm->zone == SCROLLHANDLE_BAR) {
      if (((vsm->scroller == 'h') && (v2d->keepofs & V2D_LOCKOFS_X)) ||
          ((vsm->scroller == 'v') && (v2d->keepofs & V2D_LOCKOFS_Y)))
      {
        /* free customdata initialized */
        scroller_activate_exit(C, op);

        /* can't catch this event for ourselves, so let it go to someone else? */
        return OPERATOR_PASS_THROUGH;
      }
    }

    /* zone is also inappropriate if scroller is not visible... */
    if (((vsm->scroller == 'h') && (v2d->scroll & V2D_SCROLL_HORIZONTAL_FULLR)) ||
        ((vsm->scroller == 'v') && (v2d->scroll & V2D_SCROLL_VERTICAL_FULLR)))
    {
      /* free customdata initialized */
      scroller_activate_exit(C, op);

      /* can't catch this event for ourselves, so let it go to someone else? */
      /* XXX NOTE: if handlers use mask rect to clip input, input will fail for this case. */
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

  /* not in scroller, so nothing happened...
   * (pass through let's something else catch event) */
  return OPERATOR_PASS_THROUGH;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Reset Operator
 * \{ */

static int reset_exec(bContext *C, wmOperator * /*op*/)
{
  const uiStyle *style = UI_style_get();
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;
  const int snap_test = ED_region_snap_size_test(region);

  region->category_scroll = 0;

  /* zoom 1.0 */
  const int winx = float(BLI_rcti_size_x(&v2d->mask) + 1);
  const int winy = float(BLI_rcti_size_y(&v2d->mask) + 1);

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

  /* Inform v2d about changes after this operation. */
  UI_view2d_curRect_changed(C, v2d);

  if (ED_region_snap_size_apply(region, snap_test)) {
    ScrArea *area = CTX_wm_area(C);
    ED_area_tag_redraw(area);
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
  }

  /* request updates to be done... */
  ED_region_tag_redraw(region);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_operatortypes_view2d()
{
  WM_operatortype_append(VIEW2D_OT_pan);
  WM_operatortype_append(VIEW2D_OT_edge_pan);

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

/** \} */
