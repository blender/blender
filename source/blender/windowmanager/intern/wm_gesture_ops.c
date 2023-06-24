/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Default operator callbacks for use with gestures (border/circle/lasso/straightline).
 * Operators themselves are defined elsewhere.
 *
 * - Keymaps are in `wm_operators.c`.
 * - Property definitions are in `wm_operator_props.c`.
 */

#include "MEM_guardedalloc.h"

#include "DNA_windowmanager_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_event_system.h"
#include "wm_event_types.h"

#include "ED_screen.h"
#include "ED_select_utils.h"

#include "UI_interface.h"

#include "RNA_access.h"
#include "RNA_define.h"

/* -------------------------------------------------------------------- */
/** \name Internal Gesture Utilities
 *
 * Border gesture has two types:
 * -# #WM_GESTURE_CROSS_RECT: starts a cross, on mouse click it changes to border.
 * -# #WM_GESTURE_RECT: starts immediate as a border, on mouse click or release it ends.
 *
 * It stores 4 values (xmin, xmax, ymin, ymax) and event it ended with (event_type).
 *
 * \{ */

static void gesture_modal_end(bContext *C, wmOperator *op)
{
  wmWindow *win = CTX_wm_window(C);
  wmGesture *gesture = op->customdata;

  WM_gesture_end(win, gesture); /* frees gesture itself, and unregisters from window */
  op->customdata = NULL;

  ED_area_tag_redraw(CTX_wm_area(C));

  if (RNA_struct_find_property(op->ptr, "cursor")) {
    WM_cursor_modal_restore(win);
  }
}

static void gesture_modal_state_to_operator(wmOperator *op, int modal_state)
{
  PropertyRNA *prop;

  switch (modal_state) {
    case GESTURE_MODAL_SELECT:
    case GESTURE_MODAL_DESELECT:
      if ((prop = RNA_struct_find_property(op->ptr, "deselect"))) {
        RNA_property_boolean_set(op->ptr, prop, (modal_state == GESTURE_MODAL_DESELECT));
      }
      if ((prop = RNA_struct_find_property(op->ptr, "mode"))) {
        RNA_property_enum_set(
            op->ptr, prop, (modal_state == GESTURE_MODAL_DESELECT) ? SEL_OP_SUB : SEL_OP_ADD);
      }
      break;
    case GESTURE_MODAL_IN:
    case GESTURE_MODAL_OUT:
      if ((prop = RNA_struct_find_property(op->ptr, "zoom_out"))) {
        RNA_property_boolean_set(op->ptr, prop, (modal_state == GESTURE_MODAL_OUT));
      }
      break;
  }
}

static int UNUSED_FUNCTION(gesture_modal_state_from_operator)(wmOperator *op)
{
  PropertyRNA *prop;

  if ((prop = RNA_struct_find_property(op->ptr, "deselect"))) {
    if (RNA_property_is_set(op->ptr, prop)) {
      return RNA_property_boolean_get(op->ptr, prop) ? GESTURE_MODAL_DESELECT :
                                                       GESTURE_MODAL_SELECT;
    }
  }
  if ((prop = RNA_struct_find_property(op->ptr, "mode"))) {
    if (RNA_property_is_set(op->ptr, prop)) {
      return RNA_property_enum_get(op->ptr, prop) == SEL_OP_SUB ? GESTURE_MODAL_DESELECT :
                                                                  GESTURE_MODAL_SELECT;
    }
  }
  if ((prop = RNA_struct_find_property(op->ptr, "zoom_out"))) {
    if (RNA_property_is_set(op->ptr, prop)) {
      return RNA_property_boolean_get(op->ptr, prop) ? GESTURE_MODAL_OUT : GESTURE_MODAL_IN;
    }
  }
  return GESTURE_MODAL_NOP;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Border Gesture
 *
 * Border gesture has two types:
 * -# #WM_GESTURE_CROSS_RECT: starts a cross, on mouse click it changes to border.
 * -# #WM_GESTURE_RECT: starts immediate as a border, on mouse click or release it ends.
 *
 * It stores 4 values (xmin, xmax, ymin, ymax) and event it ended with (event_type).
 *
 * \{ */

static bool gesture_box_apply_rect(wmOperator *op)
{
  wmGesture *gesture = op->customdata;
  rcti *rect = gesture->customdata;

  if (rect->xmin == rect->xmax || rect->ymin == rect->ymax) {
    return 0;
  }

  /* operator arguments and storage. */
  RNA_int_set(op->ptr, "xmin", min_ii(rect->xmin, rect->xmax));
  RNA_int_set(op->ptr, "ymin", min_ii(rect->ymin, rect->ymax));
  RNA_int_set(op->ptr, "xmax", max_ii(rect->xmin, rect->xmax));
  RNA_int_set(op->ptr, "ymax", max_ii(rect->ymin, rect->ymax));

  return 1;
}

static bool gesture_box_apply(bContext *C, wmOperator *op)
{
  wmGesture *gesture = op->customdata;

  int retval;

  if (!gesture_box_apply_rect(op)) {
    return 0;
  }

  if (gesture->wait_for_input) {
    gesture_modal_state_to_operator(op, gesture->modal_state);
  }

  retval = op->type->exec(C, op);
  OPERATOR_RETVAL_CHECK(retval);

  return (retval & OPERATOR_FINISHED) ? 1 : 0;
}

int WM_gesture_box_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  const ARegion *region = CTX_wm_region(C);
  const bool wait_for_input = !WM_event_is_mouse_drag_or_press(event) &&
                              RNA_boolean_get(op->ptr, "wait_for_input");

  if (wait_for_input) {
    op->customdata = WM_gesture_new(win, region, event, WM_GESTURE_CROSS_RECT);
  }
  else {
    op->customdata = WM_gesture_new(win, region, event, WM_GESTURE_RECT);
  }

  {
    wmGesture *gesture = op->customdata;
    gesture->wait_for_input = wait_for_input;
  }

  /* add modal handler */
  WM_event_add_modal_handler(C, op);

  wm_gesture_tag_redraw(win);

  return OPERATOR_RUNNING_MODAL;
}

int WM_gesture_box_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  wmGesture *gesture = op->customdata;
  rcti *rect = gesture->customdata;

  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case GESTURE_MODAL_MOVE: {
        gesture->move = !gesture->move;
        break;
      }
      case GESTURE_MODAL_BEGIN: {
        if (gesture->type == WM_GESTURE_CROSS_RECT && gesture->is_active == false) {
          gesture->is_active = true;
          wm_gesture_tag_redraw(win);
        }
        break;
      }
      case GESTURE_MODAL_SELECT:
      case GESTURE_MODAL_DESELECT:
      case GESTURE_MODAL_IN:
      case GESTURE_MODAL_OUT: {
        if (gesture->wait_for_input) {
          gesture->modal_state = event->val;
        }
        if (gesture_box_apply(C, op)) {
          gesture_modal_end(C, op);
          return OPERATOR_FINISHED;
        }
        gesture_modal_end(C, op);
        return OPERATOR_CANCELLED;
      }
      case GESTURE_MODAL_CANCEL: {
        gesture_modal_end(C, op);
        return OPERATOR_CANCELLED;
      }
    }
  }
  else {
    switch (event->type) {
      case MOUSEMOVE: {
        if (gesture->type == WM_GESTURE_CROSS_RECT && gesture->is_active == false) {
          rect->xmin = rect->xmax = event->xy[0] - gesture->winrct.xmin;
          rect->ymin = rect->ymax = event->xy[1] - gesture->winrct.ymin;
        }
        else if (gesture->move) {
          BLI_rcti_translate(rect,
                             (event->xy[0] - gesture->winrct.xmin) - rect->xmax,
                             (event->xy[1] - gesture->winrct.ymin) - rect->ymax);
        }
        else {
          rect->xmax = event->xy[0] - gesture->winrct.xmin;
          rect->ymax = event->xy[1] - gesture->winrct.ymin;
        }
        gesture_box_apply_rect(op);

        wm_gesture_tag_redraw(win);

        break;
      }
#ifdef WITH_INPUT_NDOF
      case NDOF_MOTION: {
        return OPERATOR_PASS_THROUGH;
      }
#endif

#if 0 /* This allows view navigation, keep disabled as it's too unpredictable. */
      default:
        return OPERATOR_PASS_THROUGH;
#endif
    }
  }

  gesture->is_active_prev = gesture->is_active;
  return OPERATOR_RUNNING_MODAL;
}

void WM_gesture_box_cancel(bContext *C, wmOperator *op)
{
  gesture_modal_end(C, op);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Gesture
 *
 * Currently only used for selection or modal paint stuff,
 * calls #wmOperatorType.exec while hold mouse, exits on release
 * (with no difference between cancel and confirm).
 *
 * \{ */

static void gesture_circle_apply(bContext *C, wmOperator *op);

int WM_gesture_circle_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  const bool wait_for_input = !WM_event_is_mouse_drag_or_press(event) &&
                              RNA_boolean_get(op->ptr, "wait_for_input");

  op->customdata = WM_gesture_new(win, CTX_wm_region(C), event, WM_GESTURE_CIRCLE);
  wmGesture *gesture = op->customdata;
  rcti *rect = gesture->customdata;

  /* Default or previously stored value. */
  rect->xmax = RNA_int_get(op->ptr, "radius");

  gesture->wait_for_input = wait_for_input;

  /* Starting with the mode starts immediately,
   * like having 'wait_for_input' disabled (some tools use this). */
  if (gesture->wait_for_input == false) {
    gesture->is_active = true;
    gesture_circle_apply(C, op);
    gesture->is_active_prev = true;
  }

  /* add modal handler */
  WM_event_add_modal_handler(C, op);

  wm_gesture_tag_redraw(win);

  return OPERATOR_RUNNING_MODAL;
}

static void gesture_circle_apply(bContext *C, wmOperator *op)
{
  wmGesture *gesture = op->customdata;
  rcti *rect = gesture->customdata;

  if (gesture->wait_for_input && (gesture->modal_state == GESTURE_MODAL_NOP)) {
    return;
  }

  /* operator arguments and storage. */
  RNA_int_set(op->ptr, "x", rect->xmin);
  RNA_int_set(op->ptr, "y", rect->ymin);
  RNA_int_set(op->ptr, "radius", rect->xmax);

  /* When 'wait_for_input' is false,
   * use properties to get the selection state (typically tool settings).
   * This is done so executing as a mode can select & de-select, see: #58594. */
  if (gesture->wait_for_input) {
    gesture_modal_state_to_operator(op, gesture->modal_state);
  }

  if (op->type->exec) {
    int retval;
    retval = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(retval);
  }
}

int WM_gesture_circle_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  wmGesture *gesture = op->customdata;
  rcti *rect = gesture->customdata;

  if (event->type == MOUSEMOVE) {

    rect->xmin = event->xy[0] - gesture->winrct.xmin;
    rect->ymin = event->xy[1] - gesture->winrct.ymin;

    wm_gesture_tag_redraw(win);

    if (gesture->is_active) {
      gesture_circle_apply(C, op);
    }
  }
  else if (event->type == EVT_MODAL_MAP) {
    bool is_circle_size = false;
    bool is_finished = false;
    float fac;

    switch (event->val) {
      case GESTURE_MODAL_CIRCLE_SIZE:
        fac = 0.3f * (event->xy[1] - event->prev_xy[1]);
        if (fac > 0) {
          rect->xmax += ceil(fac);
        }
        else {
          rect->xmax += floor(fac);
        }
        if (rect->xmax < 1) {
          rect->xmax = 1;
        }
        is_circle_size = true;
        break;
      case GESTURE_MODAL_CIRCLE_ADD:
        rect->xmax += 2 + rect->xmax / 10;
        is_circle_size = true;
        break;
      case GESTURE_MODAL_CIRCLE_SUB:
        rect->xmax -= 2 + rect->xmax / 10;
        if (rect->xmax < 1) {
          rect->xmax = 1;
        }
        is_circle_size = true;
        break;
      case GESTURE_MODAL_SELECT:
      case GESTURE_MODAL_DESELECT:
      case GESTURE_MODAL_NOP: {
        if (gesture->wait_for_input) {
          gesture->modal_state = event->val;
        }
        if (event->val == GESTURE_MODAL_NOP) {
          /* Single action, click-drag & release to exit. */
          if (gesture->wait_for_input == false) {
            is_finished = true;
          }
        }
        else {
          /* apply first click */
          gesture->is_active = true;
          gesture_circle_apply(C, op);
          wm_gesture_tag_redraw(win);
        }
        break;
      }
      case GESTURE_MODAL_CANCEL:
      case GESTURE_MODAL_CONFIRM:
        is_finished = true;
    }

    if (is_finished) {
      gesture_modal_end(C, op);
      return OPERATOR_FINISHED; /* use finish or we don't get an undo */
    }

    if (is_circle_size) {
      wm_gesture_tag_redraw(win);

      /* So next use remembers last seen size, even if we didn't apply it. */
      RNA_int_set(op->ptr, "radius", rect->xmax);
    }
  }
#ifdef WITH_INPUT_NDOF
  else if (event->type == NDOF_MOTION) {
    return OPERATOR_PASS_THROUGH;
  }
#endif

#if 0
  /* Allow view navigation??? */
  /* NOTE: this gives issues:
   * 1) other modal ops run on top (box select),
   * 2) middle-mouse is used now 3) tablet/trackpad? */
  else {
    return OPERATOR_PASS_THROUGH;
  }
#endif

  gesture->is_active_prev = gesture->is_active;
  return OPERATOR_RUNNING_MODAL;
}

void WM_gesture_circle_cancel(bContext *C, wmOperator *op)
{
  gesture_modal_end(C, op);
}

#if 0
/* template to copy from */
void WM_OT_circle_gesture(wmOperatorType *ot)
{
  ot->name = "Circle Gesture";
  ot->idname = "WM_OT_circle_gesture";
  ot->description = "Enter rotate mode with a circular gesture";

  ot->invoke = WM_gesture_circle_invoke;
  ot->modal = WM_gesture_circle_modal;
  ot->poll = WM_operator_winactive;

  /* properties */
  WM_operator_properties_gesture_circle(ot);
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lasso Gesture
 * \{ */

int WM_gesture_lasso_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  PropertyRNA *prop;

  op->customdata = WM_gesture_new(win, CTX_wm_region(C), event, WM_GESTURE_LASSO);

  /* add modal handler */
  WM_event_add_modal_handler(C, op);

  wm_gesture_tag_redraw(win);

  if ((prop = RNA_struct_find_property(op->ptr, "cursor"))) {
    WM_cursor_modal_set(win, RNA_property_int_get(op->ptr, prop));
  }

  return OPERATOR_RUNNING_MODAL;
}

int WM_gesture_lines_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  PropertyRNA *prop;

  op->customdata = WM_gesture_new(win, CTX_wm_region(C), event, WM_GESTURE_LINES);

  /* add modal handler */
  WM_event_add_modal_handler(C, op);

  wm_gesture_tag_redraw(win);

  if ((prop = RNA_struct_find_property(op->ptr, "cursor"))) {
    WM_cursor_modal_set(win, RNA_property_int_get(op->ptr, prop));
  }

  return OPERATOR_RUNNING_MODAL;
}

static int gesture_lasso_apply(bContext *C, wmOperator *op)
{
  int retval = OPERATOR_FINISHED;
  wmGesture *gesture = op->customdata;
  PointerRNA itemptr;
  float loc[2];
  int i;
  const short *lasso = gesture->customdata;

  /* operator storage as path. */

  RNA_collection_clear(op->ptr, "path");
  for (i = 0; i < gesture->points; i++, lasso += 2) {
    loc[0] = lasso[0];
    loc[1] = lasso[1];
    RNA_collection_add(op->ptr, "path", &itemptr);
    RNA_float_set_array(&itemptr, "loc", loc);
  }

  gesture_modal_end(C, op);

  if (op->type->exec) {
    retval = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(retval);
  }

  return retval;
}

int WM_gesture_lasso_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmGesture *gesture = op->customdata;

  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case GESTURE_MODAL_MOVE: {
        gesture->move = !gesture->move;
        break;
      }
    }
  }
  else {
    switch (event->type) {
      case MOUSEMOVE:
      case INBETWEEN_MOUSEMOVE: {
        wm_gesture_tag_redraw(CTX_wm_window(C));

        if (gesture->points == gesture->points_alloc) {
          gesture->points_alloc *= 2;
          gesture->customdata = MEM_reallocN(gesture->customdata,
                                             sizeof(short[2]) * gesture->points_alloc);
        }

        {
          short(*lasso)[2] = gesture->customdata;

          const int x = ((event->xy[0] - gesture->winrct.xmin) - lasso[gesture->points - 1][0]);
          const int y = ((event->xy[1] - gesture->winrct.ymin) - lasso[gesture->points - 1][1]);

          /* move the lasso */
          if (gesture->move) {
            for (int i = 0; i < gesture->points; i++) {
              lasso[i][0] += x;
              lasso[i][1] += y;
            }
          }
          /* Make a simple distance check to get a smoother lasso
           * add only when at least 2 pixels between this and previous location. */
          else if ((x * x + y * y) > pow2f(2.0f * UI_SCALE_FAC)) {
            lasso[gesture->points][0] = event->xy[0] - gesture->winrct.xmin;
            lasso[gesture->points][1] = event->xy[1] - gesture->winrct.ymin;
            gesture->points++;
          }
        }
        break;
      }
      case LEFTMOUSE:
      case MIDDLEMOUSE:
      case RIGHTMOUSE: {
        if (event->val == KM_RELEASE) { /* key release */
          return gesture_lasso_apply(C, op);
        }
        break;
      }
      case EVT_ESCKEY: {
        gesture_modal_end(C, op);
        return OPERATOR_CANCELLED;
      }
    }
  }

  gesture->is_active_prev = gesture->is_active;
  return OPERATOR_RUNNING_MODAL;
}

int WM_gesture_lines_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return WM_gesture_lasso_modal(C, op, event);
}

void WM_gesture_lasso_cancel(bContext *C, wmOperator *op)
{
  gesture_modal_end(C, op);
}

void WM_gesture_lines_cancel(bContext *C, wmOperator *op)
{
  gesture_modal_end(C, op);
}

const int (*WM_gesture_lasso_path_to_array(bContext *UNUSED(C),
                                           wmOperator *op,
                                           int *r_mcoords_len))[2]
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "path");
  int(*mcoords)[2] = NULL;
  BLI_assert(prop != NULL);

  if (prop) {
    const int len = RNA_property_collection_length(op->ptr, prop);

    if (len) {
      int i = 0;
      mcoords = MEM_mallocN(sizeof(int[2]) * len, __func__);

      RNA_PROP_BEGIN (op->ptr, itemptr, prop) {
        float loc[2];

        RNA_float_get_array(&itemptr, "loc", loc);
        mcoords[i][0] = (int)loc[0];
        mcoords[i][1] = (int)loc[1];
        i++;
      }
      RNA_PROP_END;
    }
    *r_mcoords_len = len;
  }
  else {
    *r_mcoords_len = 0;
  }

  /* cast for 'const' */
  return mcoords;
}

#if 0
/* template to copy from */

static int gesture_lasso_exec(bContext *C, wmOperator *op)
{
  RNA_BEGIN (op->ptr, itemptr, "path") {
    float loc[2];

    RNA_float_get_array(&itemptr, "loc", loc);
    printf("Location: %f %f\n", loc[0], loc[1]);
  }
  RNA_END;

  return OPERATOR_FINISHED;
}

void WM_OT_lasso_gesture(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Lasso Gesture";
  ot->idname = "WM_OT_lasso_gesture";
  ot->description = "Select objects within the lasso as you move the pointer";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = gesture_lasso_exec;

  ot->poll = WM_operator_winactive;

  ot->flag = OPTYPE_DEPENDS_ON_CURSOR;

  prop = RNA_def_property(ot->srna, "path", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_runtime(ot->srna, prop, &RNA_OperatorMousePath);
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Straight Line Gesture
 *
 * Gesture defined by the start and end points of a line that is created between the position of
 * the initial event and the position of the current event.
 *
 * Straight Line Gesture has two modal callbacks depending on the tool that is being implemented: a
 * regular modal callback intended to update the data during the execution of the gesture and a
 * one-shot callback that only updates the data once when the gesture finishes.
 *
 * It stores 4 values: `xstart, ystart, xend, yend`.
 * \{ */

static bool gesture_straightline_apply(bContext *C, wmOperator *op)
{
  wmGesture *gesture = op->customdata;
  rcti *rect = gesture->customdata;

  if (rect->xmin == rect->xmax && rect->ymin == rect->ymax) {
    return 0;
  }

  /* operator arguments and storage. */
  RNA_int_set(op->ptr, "xstart", rect->xmin);
  RNA_int_set(op->ptr, "ystart", rect->ymin);
  RNA_int_set(op->ptr, "xend", rect->xmax);
  RNA_int_set(op->ptr, "yend", rect->ymax);
  RNA_boolean_set(op->ptr, "flip", gesture->use_flip);

  if (op->type->exec) {
    int retval = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(retval);
  }

  return 1;
}

int WM_gesture_straightline_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  PropertyRNA *prop;

  op->customdata = WM_gesture_new(win, CTX_wm_region(C), event, WM_GESTURE_STRAIGHTLINE);

  if (WM_event_is_mouse_drag_or_press(event)) {
    wmGesture *gesture = op->customdata;
    gesture->is_active = true;
  }

  /* add modal handler */
  WM_event_add_modal_handler(C, op);

  wm_gesture_tag_redraw(win);

  if ((prop = RNA_struct_find_property(op->ptr, "cursor"))) {
    WM_cursor_modal_set(win, RNA_property_int_get(op->ptr, prop));
  }

  return OPERATOR_RUNNING_MODAL;
}
int WM_gesture_straightline_active_side_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  WM_gesture_straightline_invoke(C, op, event);
  wmGesture *gesture = op->customdata;
  gesture->draw_active_side = true;
  gesture->use_flip = false;
  return OPERATOR_RUNNING_MODAL;
}

#define STRAIGHTLINE_SNAP_DEG 15.0f
static void wm_gesture_straightline_do_angle_snap(rcti *rect)
{
  const float line_start[2] = {rect->xmin, rect->ymin};
  const float line_end[2] = {rect->xmax, rect->ymax};
  const float x_axis[2] = {1.0f, 0.0f};

  float line_direction[2];
  sub_v2_v2v2(line_direction, line_end, line_start);
  const float line_length = normalize_v2(line_direction);

  const float angle = angle_signed_v2v2(x_axis, line_direction);
  const float angle_deg = RAD2DEG(angle) + (STRAIGHTLINE_SNAP_DEG / 2.0f);
  const float angle_snapped_deg = -floorf(angle_deg / STRAIGHTLINE_SNAP_DEG) *
                                  STRAIGHTLINE_SNAP_DEG;
  const float angle_snapped = DEG2RAD(angle_snapped_deg);

  float line_snapped_end[2];
  rotate_v2_v2fl(line_snapped_end, x_axis, angle_snapped);
  mul_v2_fl(line_snapped_end, line_length);
  add_v2_v2(line_snapped_end, line_start);

  rect->xmax = (int)line_snapped_end[0];
  rect->ymax = (int)line_snapped_end[1];
}

int WM_gesture_straightline_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmGesture *gesture = op->customdata;
  wmWindow *win = CTX_wm_window(C);
  rcti *rect = gesture->customdata;

  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case GESTURE_MODAL_MOVE: {
        gesture->move = !gesture->move;
        break;
      }
      case GESTURE_MODAL_BEGIN: {
        if (gesture->is_active == false) {
          gesture->is_active = true;
          wm_gesture_tag_redraw(win);
        }
        break;
      }
      case GESTURE_MODAL_SNAP: {
        /* Toggle snapping on/off. */
        gesture->use_snap = !gesture->use_snap;
        break;
      }
      case GESTURE_MODAL_FLIP: {
        /* Toggle flipping on/off. */
        gesture->use_flip = !gesture->use_flip;
        gesture_straightline_apply(C, op);
        wm_gesture_tag_redraw(win);
        break;
      }
      case GESTURE_MODAL_SELECT: {
        if (gesture_straightline_apply(C, op)) {
          gesture_modal_end(C, op);
          return OPERATOR_FINISHED;
        }
        gesture_modal_end(C, op);
        return OPERATOR_CANCELLED;
      }
      case GESTURE_MODAL_CANCEL: {
        gesture_modal_end(C, op);
        return OPERATOR_CANCELLED;
      }
    }
  }
  else {
    switch (event->type) {
      case MOUSEMOVE: {
        if (gesture->is_active == false) {
          rect->xmin = rect->xmax = event->xy[0] - gesture->winrct.xmin;
          rect->ymin = rect->ymax = event->xy[1] - gesture->winrct.ymin;
        }
        else if (gesture->move) {
          BLI_rcti_translate(rect,
                             (event->xy[0] - gesture->winrct.xmin) - rect->xmax,
                             (event->xy[1] - gesture->winrct.ymin) - rect->ymax);
          gesture_straightline_apply(C, op);
        }
        else {
          rect->xmax = event->xy[0] - gesture->winrct.xmin;
          rect->ymax = event->xy[1] - gesture->winrct.ymin;
          gesture_straightline_apply(C, op);
        }

        if (gesture->use_snap) {
          wm_gesture_straightline_do_angle_snap(rect);
          gesture_straightline_apply(C, op);
        }

        wm_gesture_tag_redraw(win);

        break;
      }
    }
  }

  gesture->is_active_prev = gesture->is_active;
  return OPERATOR_RUNNING_MODAL;
}

int WM_gesture_straightline_oneshot_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmGesture *gesture = op->customdata;
  wmWindow *win = CTX_wm_window(C);
  rcti *rect = gesture->customdata;

  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case GESTURE_MODAL_MOVE: {
        gesture->move = !gesture->move;
        break;
      }
      case GESTURE_MODAL_BEGIN: {
        if (gesture->is_active == false) {
          gesture->is_active = true;
          wm_gesture_tag_redraw(win);
        }
        break;
      }
      case GESTURE_MODAL_SNAP: {
        /* Toggle snapping on/off. */
        gesture->use_snap = !gesture->use_snap;
        break;
      }
      case GESTURE_MODAL_FLIP: {
        /* Toggle flip on/off. */
        gesture->use_flip = !gesture->use_flip;
        wm_gesture_tag_redraw(win);
        break;
      }
      case GESTURE_MODAL_SELECT:
      case GESTURE_MODAL_DESELECT:
      case GESTURE_MODAL_IN:
      case GESTURE_MODAL_OUT: {
        if (gesture->wait_for_input) {
          gesture->modal_state = event->val;
        }
        if (gesture_straightline_apply(C, op)) {
          gesture_modal_end(C, op);
          return OPERATOR_FINISHED;
        }
        gesture_modal_end(C, op);
        return OPERATOR_CANCELLED;
      }
      case GESTURE_MODAL_CANCEL: {
        gesture_modal_end(C, op);
        return OPERATOR_CANCELLED;
      }
    }
  }
  else {
    switch (event->type) {
      case MOUSEMOVE: {
        if (gesture->is_active == false) {
          rect->xmin = rect->xmax = event->xy[0] - gesture->winrct.xmin;
          rect->ymin = rect->ymax = event->xy[1] - gesture->winrct.ymin;
        }
        else if (gesture->move) {
          BLI_rcti_translate(rect,
                             (event->xy[0] - gesture->winrct.xmin) - rect->xmax,
                             (event->xy[1] - gesture->winrct.ymin) - rect->ymax);
        }
        else {
          rect->xmax = event->xy[0] - gesture->winrct.xmin;
          rect->ymax = event->xy[1] - gesture->winrct.ymin;
        }

        if (gesture->use_snap) {
          wm_gesture_straightline_do_angle_snap(rect);
        }

        wm_gesture_tag_redraw(win);

        break;
      }
    }
  }

  gesture->is_active_prev = gesture->is_active;
  return OPERATOR_RUNNING_MODAL;
}

void WM_gesture_straightline_cancel(bContext *C, wmOperator *op)
{
  gesture_modal_end(C, op);
}

#if 0
/* template to copy from */
void WM_OT_straightline_gesture(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Straight Line Gesture";
  ot->idname = "WM_OT_straightline_gesture";
  ot->description = "Draw a straight line as you move the pointer";

  ot->invoke = WM_gesture_straightline_invoke;
  ot->modal = WM_gesture_straightline_modal;
  ot->exec = gesture_straightline_exec;

  ot->poll = WM_operator_winactive;

  WM_operator_properties_gesture_straightline(ot, 0);
}
#endif

/** \} */
