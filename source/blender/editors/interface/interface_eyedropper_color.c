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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 *
 * Eyedropper (RGB Color)
 *
 * Defines:
 * - #UI_OT_eyedropper_color
 */

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_math_vector.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_screen.h"

#include "RNA_access.h"

#include "GPU_glew.h"

#include "UI_interface.h"

#include "IMB_colormanagement.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

#include "interface_intern.h"

#include "ED_clip.h"
#include "ED_image.h"
#include "ED_node.h"

#include "interface_eyedropper_intern.h"

typedef struct Eyedropper {
  struct ColorManagedDisplay *display;

  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
  bool is_undo;

  bool is_set;
  float init_col[3]; /* for resetting on cancel */

  bool accum_start; /* has mouse been pressed */
  float accum_col[3];
  int accum_tot;

  bool use_accum;
} Eyedropper;

static bool eyedropper_init(bContext *C, wmOperator *op)
{
  Eyedropper *eye = MEM_callocN(sizeof(Eyedropper), __func__);
  eye->use_accum = RNA_boolean_get(op->ptr, "use_accumulate");

  uiBut *but = UI_context_active_but_prop_get(C, &eye->ptr, &eye->prop, &eye->index);
  const enum PropertySubType prop_subtype = eye->prop ? RNA_property_subtype(eye->prop) : 0;

  if ((eye->ptr.data == NULL) || (eye->prop == NULL) ||
      (RNA_property_editable(&eye->ptr, eye->prop) == false) ||
      (RNA_property_array_length(&eye->ptr, eye->prop) < 3) ||
      (RNA_property_type(eye->prop) != PROP_FLOAT) ||
      (ELEM(prop_subtype, PROP_COLOR, PROP_COLOR_GAMMA) == 0)) {
    MEM_freeN(eye);
    return false;
  }
  op->customdata = eye;

  eye->is_undo = UI_but_flag_is_set(but, UI_BUT_UNDO);

  float col[4];
  RNA_property_float_get_array(&eye->ptr, eye->prop, col);
  if (prop_subtype != PROP_COLOR) {
    Scene *scene = CTX_data_scene(C);
    const char *display_device;

    display_device = scene->display_settings.display_device;
    eye->display = IMB_colormanagement_display_get_named(display_device);

    /* store initial color */
    if (eye->display) {
      IMB_colormanagement_display_to_scene_linear_v3(col, eye->display);
    }
  }
  copy_v3_v3(eye->init_col, col);

  return true;
}

static void eyedropper_exit(bContext *C, wmOperator *op)
{
  WM_cursor_modal_restore(CTX_wm_window(C));

  if (op->customdata) {
    MEM_freeN(op->customdata);
    op->customdata = NULL;
  }
}

/* *** eyedropper_color_ helper functions *** */

/**
 * \brief get the color from the screen.
 *
 * Special check for image or nodes where we MAY have HDR pixels which don't display.
 *
 * \note Exposed by 'interface_eyedropper_intern.h' for use with color band picking.
 */
void eyedropper_color_sample_fl(bContext *C, int mx, int my, float r_col[3])
{
  /* we could use some clever */
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  const char *display_device = CTX_data_scene(C)->display_settings.display_device;
  struct ColorManagedDisplay *display = IMB_colormanagement_display_get_named(display_device);

  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, mx, my);
  if (area == NULL) {
    int mval[2] = {mx, my};
    if (WM_window_find_under_cursor(wm, NULL, win, mval, &win, mval)) {
      mx = mval[0];
      my = mval[1];
      screen = WM_window_get_active_screen(win);
      area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, mx, my);
    }
    else {
      win = NULL;
    }
  }

  if (area) {
    if (area->spacetype == SPACE_IMAGE) {
      ARegion *region = BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, mx, my);
      if (region) {
        SpaceImage *sima = area->spacedata.first;
        int mval[2] = {mx - region->winrct.xmin, my - region->winrct.ymin};

        if (ED_space_image_color_sample(sima, region, mval, r_col)) {
          return;
        }
      }
    }
    else if (area->spacetype == SPACE_NODE) {
      ARegion *region = BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, mx, my);
      if (region) {
        SpaceNode *snode = area->spacedata.first;
        int mval[2] = {mx - region->winrct.xmin, my - region->winrct.ymin};

        if (ED_space_node_color_sample(bmain, snode, region, mval, r_col)) {
          return;
        }
      }
    }
    else if (area->spacetype == SPACE_CLIP) {
      ARegion *region = BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, mx, my);
      if (region) {
        SpaceClip *sc = area->spacedata.first;
        int mval[2] = {mx - region->winrct.xmin, my - region->winrct.ymin};

        if (ED_space_clip_color_sample(sc, region, mval, r_col)) {
          return;
        }
      }
    }
  }

  if (win) {
    /* Fallback to simple opengl picker. */
    int mval[2] = {mx, my};
    WM_window_pixel_sample_read(wm, win, mval, r_col);
    IMB_colormanagement_display_to_scene_linear_v3(r_col, display);
  }
  else {
    zero_v3(r_col);
  }
}

/* sets the sample color RGB, maintaining A */
static void eyedropper_color_set(bContext *C, Eyedropper *eye, const float col[3])
{
  float col_conv[4];

  /* to maintain alpha */
  RNA_property_float_get_array(&eye->ptr, eye->prop, col_conv);

  /* convert from linear rgb space to display space */
  if (eye->display) {
    copy_v3_v3(col_conv, col);
    IMB_colormanagement_scene_linear_to_display_v3(col_conv, eye->display);
  }
  else {
    copy_v3_v3(col_conv, col);
  }

  RNA_property_float_set_array(&eye->ptr, eye->prop, col_conv);
  eye->is_set = true;

  RNA_property_update(C, &eye->ptr, eye->prop);
}

static void eyedropper_color_sample(bContext *C, Eyedropper *eye, int mx, int my)
{
  /* Accumulate color. */
  float col[3];
  eyedropper_color_sample_fl(C, mx, my, col);

  if (eye->use_accum) {
    add_v3_v3(eye->accum_col, col);
    eye->accum_tot++;
  }
  else {
    copy_v3_v3(eye->accum_col, col);
    eye->accum_tot = 1;
  }

  /* Apply to property. */
  float accum_col[3];
  if (eye->accum_tot > 1) {
    mul_v3_v3fl(accum_col, eye->accum_col, 1.0f / (float)eye->accum_tot);
  }
  else {
    copy_v3_v3(accum_col, eye->accum_col);
  }
  eyedropper_color_set(C, eye, accum_col);
}

static void eyedropper_cancel(bContext *C, wmOperator *op)
{
  Eyedropper *eye = op->customdata;
  if (eye->is_set) {
    eyedropper_color_set(C, eye, eye->init_col);
  }
  eyedropper_exit(C, op);
}

/* main modal status check */
static int eyedropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Eyedropper *eye = (Eyedropper *)op->customdata;

  /* handle modal keymap */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case EYE_MODAL_CANCEL:
        eyedropper_cancel(C, op);
        return OPERATOR_CANCELLED;
      case EYE_MODAL_SAMPLE_CONFIRM: {
        const bool is_undo = eye->is_undo;
        if (eye->accum_tot == 0) {
          eyedropper_color_sample(C, eye, event->x, event->y);
        }
        eyedropper_exit(C, op);
        /* Could support finished & undo-skip. */
        return is_undo ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
      }
      case EYE_MODAL_SAMPLE_BEGIN:
        /* enable accum and make first sample */
        eye->accum_start = true;
        eyedropper_color_sample(C, eye, event->x, event->y);
        break;
      case EYE_MODAL_SAMPLE_RESET:
        eye->accum_tot = 0;
        zero_v3(eye->accum_col);
        eyedropper_color_sample(C, eye, event->x, event->y);
        break;
    }
  }
  else if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
    if (eye->accum_start) {
      /* button is pressed so keep sampling */
      eyedropper_color_sample(C, eye, event->x, event->y);
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int eyedropper_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  /* init */
  if (eyedropper_init(C, op)) {
    wmWindow *win = CTX_wm_window(C);
    /* Workaround for de-activating the button clearing the cursor, see T76794 */
    UI_context_active_but_clear(C, win, CTX_wm_region(C));
    WM_cursor_modal_set(win, WM_CURSOR_EYEDROPPER);

    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  else {
    return OPERATOR_PASS_THROUGH;
  }
}

/* Repeat operator */
static int eyedropper_exec(bContext *C, wmOperator *op)
{
  /* init */
  if (eyedropper_init(C, op)) {

    /* do something */

    /* cleanup */
    eyedropper_exit(C, op);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_PASS_THROUGH;
  }
}

static bool eyedropper_poll(bContext *C)
{
  /* Actual test for active button happens later, since we don't
   * know which one is active until mouse over. */
  return (CTX_wm_window(C) != NULL);
}

void UI_OT_eyedropper_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Eyedropper";
  ot->idname = "UI_OT_eyedropper_color";
  ot->description = "Sample a color from the Blender window to store in a property";

  /* api callbacks */
  ot->invoke = eyedropper_invoke;
  ot->modal = eyedropper_modal;
  ot->cancel = eyedropper_cancel;
  ot->exec = eyedropper_exec;
  ot->poll = eyedropper_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* properties */
  PropertyRNA *prop;

  /* Needed for color picking with crypto-matte. */
  prop = RNA_def_boolean(ot->srna, "use_accumulate", true, "Accumulate", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}
