/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "BKE_colorband.hh"
#include "BKE_context.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "interface_intern.hh"

#include "eyedropper_intern.hh"

namespace blender::ui {

struct EyedropperColorband {
  int event_xy_last[2] = {};
  /* Alpha is currently fixed at 1.0, may support in future. */
  Vector<float4> color_buffer = {};
  bool sample_start = false;
  ColorBand init_color_band = {};
  ColorBand *color_band = nullptr;
  PointerRNA ptr = {};
  PropertyRNA *prop = nullptr;
  bool is_undo = false;
  bool is_set = false;
};

/* For user-data only. */
struct EyedropperColorband_Context {
  bContext *context;
  EyedropperColorband *eye;
};

static bool eyedropper_colorband_init(bContext *C, wmOperator *op)
{
  ColorBand *band = nullptr;

  uiBut *but = UI_context_active_but_get(C);

  PointerRNA rna_update_ptr = PointerRNA_NULL;
  PropertyRNA *rna_update_prop = nullptr;
  bool is_undo = true;

  if (but == nullptr) {
    /* pass */
  }
  else {
    if (but->type == ButType::ColorBand) {
      /* When invoked with a hotkey, we can find the band in 'but->poin'. */
      band = (ColorBand *)but->poin;
    }
    else {
      /* When invoked from a button it's in custom_data field. */
      band = (ColorBand *)but->custom_data;
    }

    if (band) {
      rna_update_ptr = but->rnapoin;
      rna_update_prop = but->rnaprop;
      is_undo = UI_but_flag_is_set(but, UI_BUT_UNDO);
    }
  }

  if (!band) {
    const PointerRNA ptr = CTX_data_pointer_get_type(C, "color_ramp", &RNA_ColorRamp);
    if (ptr.data != nullptr) {
      band = static_cast<ColorBand *>(ptr.data);

      /* Set this to a sub-member of the property to trigger an update. */
      rna_update_ptr = ptr;
      rna_update_prop = &rna_ColorRamp_color_mode;
      is_undo = RNA_struct_undo_check(ptr.type);
    }
  }

  if (!band) {
    return false;
  }

  EyedropperColorband *eye = MEM_new<EyedropperColorband>(__func__);
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
                                              const int m_xy[2])
{
  if (eye->event_xy_last[0] != m_xy[0] || eye->event_xy_last[1] != m_xy[1]) {
    float4 col;
    col[3] = 1.0f; /* TODO: sample alpha */
    eyedropper_color_sample_fl(C, nullptr, m_xy, col);
    eye->color_buffer.append(col);
    copy_v2_v2_int(eye->event_xy_last, m_xy);
    eye->is_set = true;
  }
}

static bool eyedropper_colorband_sample_callback(int mx, int my, void *userdata)
{
  EyedropperColorband_Context *data = static_cast<EyedropperColorband_Context *>(userdata);
  bContext *C = data->context;
  EyedropperColorband *eye = data->eye;
  const int cursor[2] = {mx, my};
  eyedropper_colorband_sample_point(C, eye, cursor);
  return true;
}

static void eyedropper_colorband_sample_segment(bContext *C,
                                                EyedropperColorband *eye,
                                                const int m_xy[2])
{
  /* Since the mouse tends to move rather rapidly we use #BLI_bitmap_draw_2d_line_v2v2i
   * to interpolate between the reported coordinates */
  EyedropperColorband_Context userdata = {C, eye};
  BLI_bitmap_draw_2d_line_v2v2i(
      eye->event_xy_last, m_xy, eyedropper_colorband_sample_callback, &userdata);
}

static void eyedropper_colorband_exit(bContext *C, wmOperator *op)
{
  WM_cursor_modal_restore(CTX_wm_window(C));

  if (op->customdata) {
    EyedropperColorband *eye = static_cast<EyedropperColorband *>(op->customdata);
    MEM_delete(eye);
    op->customdata = nullptr;
  }
}

static void eyedropper_colorband_apply(bContext *C, wmOperator *op)
{
  EyedropperColorband *eye = static_cast<EyedropperColorband *>(op->customdata);
  /* Always filter, avoids noise in resulting color-band. */
  const bool filter_samples = true;
  BKE_colorband_init_from_table_rgba(
      eye->color_band,
      reinterpret_cast<const float (*)[4]>(eye->color_buffer.data()),
      eye->color_buffer.size(),
      filter_samples);
  eye->is_set = true;
  if (eye->prop) {
    RNA_property_update(C, &eye->ptr, eye->prop);
  }
}

static void eyedropper_colorband_cancel(bContext *C, wmOperator *op)
{
  EyedropperColorband *eye = static_cast<EyedropperColorband *>(op->customdata);
  if (eye->is_set) {
    *eye->color_band = eye->init_color_band;
    if (eye->prop) {
      RNA_property_update(C, &eye->ptr, eye->prop);
    }
  }
  eyedropper_colorband_exit(C, op);
}

/* main modal status check */
static wmOperatorStatus eyedropper_colorband_modal(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  EyedropperColorband *eye = static_cast<EyedropperColorband *>(op->customdata);
  /* handle modal keymap */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case EYE_MODAL_CANCEL:
        eyedropper_colorband_cancel(C, op);
        return OPERATOR_CANCELLED;
      case EYE_MODAL_SAMPLE_CONFIRM: {
        const bool is_undo = eye->is_undo;
        eyedropper_colorband_sample_segment(C, eye, event->xy);
        eyedropper_colorband_apply(C, op);
        eyedropper_colorband_exit(C, op);
        /* Could support finished & undo-skip. */
        return is_undo ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
      }
      case EYE_MODAL_SAMPLE_BEGIN:
        /* Enable accumulate and make first sample. */
        eye->sample_start = true;
        eyedropper_colorband_sample_point(C, eye, event->xy);
        eyedropper_colorband_apply(C, op);
        copy_v2_v2_int(eye->event_xy_last, event->xy);
        break;
      case EYE_MODAL_SAMPLE_RESET:
        break;
    }
  }
  else if (event->type == MOUSEMOVE) {
    if (eye->sample_start) {
      eyedropper_colorband_sample_segment(C, eye, event->xy);
      eyedropper_colorband_apply(C, op);
    }
  }
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus eyedropper_colorband_point_modal(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  EyedropperColorband *eye = static_cast<EyedropperColorband *>(op->customdata);
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
        if (!eye->color_buffer.is_empty()) {
          eye->color_buffer.pop_last();
          eyedropper_colorband_apply(C, op);
        }
        break;
      case EYE_MODAL_POINT_SAMPLE:
        eyedropper_colorband_sample_point(C, eye, event->xy);
        eyedropper_colorband_apply(C, op);
        if (eye->color_buffer.size() == MAXCOLORBAND) {
          eyedropper_colorband_exit(C, op);
          return OPERATOR_FINISHED;
        }
        break;
      case EYE_MODAL_SAMPLE_RESET:
        *eye->color_band = eye->init_color_band;
        if (eye->prop) {
          RNA_property_update(C, &eye->ptr, eye->prop);
        }
        eye->color_buffer.clear();
        break;
    }
  }
  return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static wmOperatorStatus eyedropper_colorband_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent * /*event*/)
{
  /* init */
  if (eyedropper_colorband_init(C, op)) {
    wmWindow *win = CTX_wm_window(C);
    /* Workaround for de-activating the button clearing the cursor, see #76794 */
    UI_context_active_but_clear(C, win, CTX_wm_region(C));
    WM_cursor_modal_set(win, WM_CURSOR_EYEDROPPER);

    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_CANCELLED;
}

/* Repeat operator */
static wmOperatorStatus eyedropper_colorband_exec(bContext *C, wmOperator *op)
{
  /* init */
  if (eyedropper_colorband_init(C, op)) {

    /* do something */

    /* cleanup */
    eyedropper_colorband_exit(C, op);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static bool eyedropper_colorband_poll(bContext *C)
{
  uiBut *but = UI_context_active_but_get(C);
  if (but && but->type == ButType::ColorBand) {
    return true;
  }
  const PointerRNA ptr = CTX_data_pointer_get_type(C, "color_ramp", &RNA_ColorRamp);
  if (ptr.data != nullptr) {
    return true;
  }
  return false;
}

void UI_OT_eyedropper_colorramp(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Eyedropper Colorband";
  ot->idname = "UI_OT_eyedropper_colorramp";
  ot->description = "Sample a color band";

  /* API callbacks. */
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
  ot->name = "Eyedropper Colorband (Points)";
  ot->idname = "UI_OT_eyedropper_colorramp_point";
  ot->description = "Point-sample a color band";

  /* API callbacks. */
  ot->invoke = eyedropper_colorband_invoke;
  ot->modal = eyedropper_colorband_point_modal;
  ot->cancel = eyedropper_colorband_cancel;
  ot->exec = eyedropper_colorband_exec;
  ot->poll = eyedropper_colorband_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* properties */
}

}  // namespace blender::ui
