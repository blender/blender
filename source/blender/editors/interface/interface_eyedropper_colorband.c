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
 * Eyedropper (Color Band).
 *
 * Operates by either:
 * - Dragging a straight line, sampling pixels formed by the line to extract a gradient.
 * - Clicking on points, adding each color to the end of the color-band.
 *
 * Defines:
 * - #UI_OT_eyedropper_colorramp
 * - #UI_OT_eyedropper_colorramp_point
 */

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"

#include "BLI_bitmap_draw_2d.h"
#include "BLI_math_vector.h"

#include "BKE_context.h"
#include "BKE_colorband.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

#include "interface_eyedropper_intern.h"

typedef struct Colorband_RNAUpdateCb {
  PointerRNA ptr;
  PropertyRNA *prop;
} Colorband_RNAUpdateCb;

typedef struct EyedropperColorband {
  int last_x, last_y;
  /* Alpha is currently fixed at 1.0, may support in future. */
  float (*color_buffer)[4];
  int color_buffer_alloc;
  int color_buffer_len;
  bool sample_start;
  ColorBand init_color_band;
  ColorBand *color_band;
  PointerRNA ptr;
  PropertyRNA *prop;
  bool is_undo;
  bool is_set;
} EyedropperColorband;

/* For user-data only. */
struct EyedropperColorband_Context {
  bContext *context;
  EyedropperColorband *eye;
};

static bool eyedropper_colorband_init(bContext *C, wmOperator *op)
{
  ColorBand *band = NULL;

  uiBut *but = UI_context_active_but_get(C);

  PointerRNA rna_update_ptr = PointerRNA_NULL;
  PropertyRNA *rna_update_prop = NULL;
  bool is_undo = true;

  if (but == NULL) {
    /* pass */
  }
  else {
    if (but->type == UI_BTYPE_COLORBAND) {
      /* When invoked with a hotkey, we can find the band in 'but->poin'. */
      band = (ColorBand *)but->poin;
    }
    else {
      /* When invoked from a button it's in custom_data field. */
      band = (ColorBand *)but->custom_data;
    }

    if (band) {
      rna_update_ptr = ((Colorband_RNAUpdateCb *)but->func_argN)->ptr;
      rna_update_prop = ((Colorband_RNAUpdateCb *)but->func_argN)->prop;
      is_undo = UI_but_flag_is_set(but, UI_BUT_UNDO);
    }
  }

  if (!band) {
    PointerRNA ptr = CTX_data_pointer_get_type(C, "color_ramp", &RNA_ColorRamp);
    if (ptr.data != NULL) {
      band = ptr.data;

      /* Set this to a sub-member of the property to trigger an update. */
      extern PropertyRNA rna_ColorRamp_color_mode;
      rna_update_ptr = ptr;
      rna_update_prop = &rna_ColorRamp_color_mode;
      is_undo = RNA_struct_undo_check(ptr.type);
    }
  }

  if (!band) {
    return false;
  }

  EyedropperColorband *eye = MEM_callocN(sizeof(EyedropperColorband), __func__);
  eye->color_buffer_alloc = 16;
  eye->color_buffer = MEM_mallocN(sizeof(*eye->color_buffer) * eye->color_buffer_alloc, __func__);
  eye->color_buffer_len = 0;
  eye->color_band = band;
  eye->init_color_band = *eye->color_band;
  eye->ptr = rna_update_ptr;
  eye->prop = rna_update_prop;
  eye->is_undo = is_undo;

  op->customdata = eye;

  return true;
}

static void eyedropper_colorband_sample_point(bContext *C,
                                              EyedropperColorband *eye,
                                              int mx,
                                              int my)
{
  if (eye->last_x != mx || eye->last_y != my) {
    float col[4];
    col[3] = 1.0f; /* TODO: sample alpha */
    eyedropper_color_sample_fl(C, mx, my, col);
    if (eye->color_buffer_len + 1 == eye->color_buffer_alloc) {
      eye->color_buffer_alloc *= 2;
      eye->color_buffer = MEM_reallocN(eye->color_buffer,
                                       sizeof(*eye->color_buffer) * eye->color_buffer_alloc);
    }
    copy_v4_v4(eye->color_buffer[eye->color_buffer_len], col);
    eye->color_buffer_len += 1;
    eye->last_x = mx;
    eye->last_y = my;
    eye->is_set = true;
  }
}

static bool eyedropper_colorband_sample_callback(int mx, int my, void *userdata)
{
  struct EyedropperColorband_Context *data = userdata;
  bContext *C = data->context;
  EyedropperColorband *eye = data->eye;
  eyedropper_colorband_sample_point(C, eye, mx, my);
  return true;
}

static void eyedropper_colorband_sample_segment(bContext *C,
                                                EyedropperColorband *eye,
                                                int mx,
                                                int my)
{
  /* Since the mouse tends to move rather rapidly we use #BLI_bitmap_draw_2d_line_v2v2i
   * to interpolate between the reported coordinates */
  struct EyedropperColorband_Context userdata = {C, eye};
  int p1[2] = {eye->last_x, eye->last_y};
  int p2[2] = {mx, my};
  BLI_bitmap_draw_2d_line_v2v2i(p1, p2, eyedropper_colorband_sample_callback, &userdata);
}

static void eyedropper_colorband_exit(bContext *C, wmOperator *op)
{
  WM_cursor_modal_restore(CTX_wm_window(C));

  if (op->customdata) {
    EyedropperColorband *eye = op->customdata;
    MEM_freeN(eye->color_buffer);
    MEM_freeN(eye);
    op->customdata = NULL;
  }
}

static void eyedropper_colorband_apply(bContext *C, wmOperator *op)
{
  EyedropperColorband *eye = op->customdata;
  /* Always filter, avoids noise in resulting color-band. */
  bool filter_samples = true;
  BKE_colorband_init_from_table_rgba(
      eye->color_band, eye->color_buffer, eye->color_buffer_len, filter_samples);
  eye->is_set = true;
  if (eye->prop) {
    RNA_property_update(C, &eye->ptr, eye->prop);
  }
}

static void eyedropper_colorband_cancel(bContext *C, wmOperator *op)
{
  EyedropperColorband *eye = op->customdata;
  if (eye->is_set) {
    *eye->color_band = eye->init_color_band;
    if (eye->prop) {
      RNA_property_update(C, &eye->ptr, eye->prop);
    }
  }
  eyedropper_colorband_exit(C, op);
}

/* main modal status check */
static int eyedropper_colorband_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  EyedropperColorband *eye = op->customdata;
  /* handle modal keymap */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case EYE_MODAL_CANCEL:
        eyedropper_colorband_cancel(C, op);
        return OPERATOR_CANCELLED;
      case EYE_MODAL_SAMPLE_CONFIRM: {
        const bool is_undo = eye->is_undo;
        eyedropper_colorband_sample_segment(C, eye, event->x, event->y);
        eyedropper_colorband_apply(C, op);
        eyedropper_colorband_exit(C, op);
        /* Could support finished & undo-skip. */
        return is_undo ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
      }
      case EYE_MODAL_SAMPLE_BEGIN:
        /* enable accum and make first sample */
        eye->sample_start = true;
        eyedropper_colorband_sample_point(C, eye, event->x, event->y);
        eyedropper_colorband_apply(C, op);
        eye->last_x = event->x;
        eye->last_y = event->y;
        break;
      case EYE_MODAL_SAMPLE_RESET:
        break;
    }
  }
  else if (event->type == MOUSEMOVE) {
    if (eye->sample_start) {
      eyedropper_colorband_sample_segment(C, eye, event->x, event->y);
      eyedropper_colorband_apply(C, op);
    }
  }
  return OPERATOR_RUNNING_MODAL;
}

static int eyedropper_colorband_point_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  EyedropperColorband *eye = op->customdata;
  /* handle modal keymap */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case EYE_MODAL_POINT_CANCEL:
        eyedropper_colorband_cancel(C, op);
        return OPERATOR_CANCELLED;
      case EYE_MODAL_POINT_CONFIRM:
        eyedropper_colorband_apply(C, op);
        eyedropper_colorband_exit(C, op);
        return OPERATOR_FINISHED;
      case EYE_MODAL_POINT_REMOVE_LAST:
        if (eye->color_buffer_len > 0) {
          eye->color_buffer_len -= 1;
          eyedropper_colorband_apply(C, op);
        }
        break;
      case EYE_MODAL_POINT_SAMPLE:
        eyedropper_colorband_sample_point(C, eye, event->x, event->y);
        eyedropper_colorband_apply(C, op);
        if (eye->color_buffer_len == MAXCOLORBAND) {
          eyedropper_colorband_exit(C, op);
          return OPERATOR_FINISHED;
        }
        break;
      case EYE_MODAL_SAMPLE_RESET:
        *eye->color_band = eye->init_color_band;
        if (eye->prop) {
          RNA_property_update(C, &eye->ptr, eye->prop);
        }
        eye->color_buffer_len = 0;
        break;
    }
  }
  return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int eyedropper_colorband_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  /* init */
  if (eyedropper_colorband_init(C, op)) {
    WM_cursor_modal_set(CTX_wm_window(C), BC_EYEDROPPER_CURSOR);

    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

/* Repeat operator */
static int eyedropper_colorband_exec(bContext *C, wmOperator *op)
{
  /* init */
  if (eyedropper_colorband_init(C, op)) {

    /* do something */

    /* cleanup */
    eyedropper_colorband_exit(C, op);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

static bool eyedropper_colorband_poll(bContext *C)
{
  uiBut *but = UI_context_active_but_get(C);
  if (but && but->type == UI_BTYPE_COLORBAND) {
    return true;
  }
  PointerRNA ptr = CTX_data_pointer_get_type(C, "color_ramp", &RNA_ColorRamp);
  if (ptr.data != NULL) {
    return true;
  }
  return false;
}

void UI_OT_eyedropper_colorramp(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Eyedropper colorband";
  ot->idname = "UI_OT_eyedropper_colorramp";
  ot->description = "Sample a color band";

  /* api callbacks */
  ot->invoke = eyedropper_colorband_invoke;
  ot->modal = eyedropper_colorband_modal;
  ot->cancel = eyedropper_colorband_cancel;
  ot->exec = eyedropper_colorband_exec;
  ot->poll = eyedropper_colorband_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* properties */
}

void UI_OT_eyedropper_colorramp_point(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Eyedropper colorband (points)";
  ot->idname = "UI_OT_eyedropper_colorramp_point";
  ot->description = "Point-sample a color band";

  /* api callbacks */
  ot->invoke = eyedropper_colorband_invoke;
  ot->modal = eyedropper_colorband_point_modal;
  ot->cancel = eyedropper_colorband_cancel;
  ot->exec = eyedropper_colorband_exec;
  ot->poll = eyedropper_colorband_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* properties */
}
