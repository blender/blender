/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Utilities for Implementing Operators
 */

#include <cmath>

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_layer.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.hh" /* Own include. */
#include "WM_types.hh"

#include "MEM_guardedalloc.h"

#include "ED_object.hh"
#include "ED_screen.hh"

/* -------------------------------------------------------------------- */
/** \name Generic Utilities
 * \{ */

int WM_operator_flag_only_pass_through_on_press(int retval, const wmEvent *event)
{
  if (event->val != KM_PRESS) {
    if (retval & OPERATOR_PASS_THROUGH) {
      /* Operators that use this function should either finish or cancel,
       * otherwise non-press events will be passed through to other key-map items. */
      BLI_assert((retval & ~OPERATOR_PASS_THROUGH) != 0);
      if (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED)) {
        retval &= ~OPERATOR_PASS_THROUGH;
      }
    }
  }
  return retval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Value Interaction Helper
 *
 * Possible additions (add as needed).
 * - Int support.
 * - Configurable motion (x/y).
 *
 * \{ */

struct ValueInteraction {
  struct {
    float mval[2];
    float prop_value;
  } init;
  struct {
    float prop_value;
    bool is_snap;
    bool is_precise;
  } prev;
  float range[2];

  struct {
    ScrArea *area;
    ARegion *region;
  } context_vars;
};

static void interactive_value_init(bContext *C,
                                   ValueInteraction *inter,
                                   const wmEvent *event,
                                   const float value_final,
                                   const float range[2])
{

  inter->context_vars.area = CTX_wm_area(C);
  inter->context_vars.region = CTX_wm_region(C);

  inter->init.mval[0] = event->mval[0];
  inter->init.mval[1] = event->mval[1];
  inter->init.prop_value = value_final;
  inter->prev.prop_value = value_final;
  inter->range[0] = range[0];
  inter->range[1] = range[1];
}

static void interactive_value_init_from_property(
    bContext *C, ValueInteraction *inter, const wmEvent *event, PointerRNA *ptr, PropertyRNA *prop)
{
  float range[2];
  float step, precision;
  RNA_property_float_ui_range(ptr, prop, &range[0], &range[1], &step, &precision);
  const float value_final = RNA_property_float_get(ptr, prop);
  interactive_value_init(C, inter, event, value_final, range);
}

static void interactive_value_exit(ValueInteraction *inter)
{
  ED_area_status_text(inter->context_vars.area, nullptr);
}

static bool interactive_value_update(ValueInteraction *inter,
                                     const wmEvent *event,
                                     float *r_value_final)
{
  const int mval_axis = 0;

  const float value_scale = 4.0f; /* Could be option. */
  const float value_range = inter->range[1] - inter->range[0];
  const int mval_curr = event->mval[mval_axis];
  const int mval_init = inter->init.mval[mval_axis];
  float value_delta = (inter->init.prop_value +
                       ((float(mval_curr - mval_init) / inter->context_vars.region->winx) *
                        value_range)) *
                      value_scale;
  if (event->modifier & KM_CTRL) {
    const double snap = 0.1;
    value_delta = float(roundf(double(value_delta) / snap)) * snap;
  }
  if (event->modifier & KM_SHIFT) {
    value_delta *= 0.1f;
  }
  const float value_final = inter->init.prop_value + value_delta;

  const bool changed = value_final != inter->prev.prop_value;
  if (changed) {
    /* set the property for the operator and call its modal function */
    char str[64];
    SNPRINTF(str, "%.4f", value_final);
    ED_area_status_text(inter->context_vars.area, str);
  }

  inter->prev.prop_value = value_final;
  inter->prev.is_snap = (event->modifier & KM_CTRL) != 0;
  inter->prev.is_precise = (event->modifier & KM_SHIFT) != 0;

  *r_value_final = value_final;
  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Edit Mode Coords (Modal Callbacks)
 *
 * \note We could support object mode coords too, it's just not needed at the moment.
 * \{ */

struct ObCustomData_ForEditMode {
  int launch_event;
  bool wait_for_input;
  bool is_active;
  bool is_first;

  ValueInteraction inter;

  /** This could be split into a sub-type if we support different kinds of data. */
  Object **objects;
  uint objects_len;
  XFormObjectData **objects_xform;
};

/* Internal callback to free. */
static void op_generic_value_exit(wmOperator *op)
{
  ObCustomData_ForEditMode *cd = static_cast<ObCustomData_ForEditMode *>(op->customdata);
  if (cd) {
    interactive_value_exit(&cd->inter);

    for (uint ob_index = 0; ob_index < cd->objects_len; ob_index++) {
      XFormObjectData *xod = cd->objects_xform[ob_index];
      if (xod != nullptr) {
        ED_object_data_xform_destroy(xod);
      }
    }
    MEM_freeN(cd->objects);
    MEM_freeN(cd->objects_xform);
    MEM_freeN(cd);
  }

  G.moving &= ~G_TRANSFORM_EDIT;
}

static void op_generic_value_restore(wmOperator *op)
{
  ObCustomData_ForEditMode *cd = static_cast<ObCustomData_ForEditMode *>(op->customdata);
  for (uint ob_index = 0; ob_index < cd->objects_len; ob_index++) {
    ED_object_data_xform_restore(cd->objects_xform[ob_index]);
    ED_object_data_xform_tag_update(cd->objects_xform[ob_index]);
  }
}

static void op_generic_value_cancel(bContext * /*C*/, wmOperator *op)
{
  op_generic_value_exit(op);
}

static int op_generic_value_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (RNA_property_is_set(op->ptr, op->type->prop)) {
    return WM_operator_call_notest(C, op);
  }

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C), &objects_len);
  if (objects_len == 0) {
    MEM_freeN(objects);
    return OPERATOR_CANCELLED;
  }

  ObCustomData_ForEditMode *cd = static_cast<ObCustomData_ForEditMode *>(
      MEM_callocN(sizeof(*cd), __func__));
  cd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);
  cd->wait_for_input = RNA_boolean_get(op->ptr, "wait_for_input");
  cd->is_active = !cd->wait_for_input;
  cd->is_first = true;
  cd->objects = objects;
  cd->objects_len = objects_len;

  if (cd->wait_for_input == false) {
    interactive_value_init_from_property(C, &cd->inter, event, op->ptr, op->type->prop);
  }

  cd->objects_xform = static_cast<XFormObjectData **>(
      MEM_callocN(sizeof(*cd->objects_xform) * objects_len, __func__));

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    cd->objects_xform[ob_index] = ED_object_data_xform_create_from_edit_mode(
        static_cast<ID *>(obedit->data));
  }

  op->customdata = cd;

  WM_event_add_modal_handler(C, op);
  G.moving |= G_TRANSFORM_EDIT;

  return OPERATOR_RUNNING_MODAL;
}

static int op_generic_value_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ObCustomData_ForEditMode *cd = static_cast<ObCustomData_ForEditMode *>(op->customdata);

  /* Special case, check if we release the event that activated this operator. */
  if ((event->type == cd->launch_event) && (event->val == KM_RELEASE)) {
    if (cd->wait_for_input == false) {
      op_generic_value_exit(op);
      return OPERATOR_FINISHED;
    }
  }

  switch (event->type) {
    case MOUSEMOVE:
    case EVT_LEFTCTRLKEY:
    case EVT_RIGHTCTRLKEY:
    case EVT_LEFTSHIFTKEY:
    case EVT_RIGHTSHIFTKEY: {
      float value_final;
      if (cd->is_active && interactive_value_update(&cd->inter, event, &value_final)) {
        wmWindowManager *wm = CTX_wm_manager(C);

        RNA_property_float_set(op->ptr, op->type->prop, value_final);
        if (cd->is_first == false) {
          op_generic_value_restore(op);
        }

        wm->op_undo_depth++;
        int retval = op->type->exec(C, op);
        OPERATOR_RETVAL_CHECK(retval);
        wm->op_undo_depth--;

        cd->is_first = false;

        if ((retval & OPERATOR_FINISHED) == 0) {
          op_generic_value_exit(op);
          return OPERATOR_CANCELLED;
        }
      }
      break;
    }
    case EVT_RETKEY:
    case EVT_PADENTER:
    case LEFTMOUSE: {
      if (cd->wait_for_input) {
        if (event->val == KM_PRESS) {
          if (cd->is_active == false) {
            cd->is_active = true;
            interactive_value_init_from_property(C, &cd->inter, event, op->ptr, op->type->prop);
          }
        }
        else if (event->val == KM_RELEASE) {
          if (cd->is_active == true) {
            op_generic_value_exit(op);
            return OPERATOR_FINISHED;
          }
        }
      }
      else {
        if (event->val == KM_RELEASE) {
          op_generic_value_exit(op);
          return OPERATOR_FINISHED;
        }
      }
      break;
    }
    case EVT_ESCKEY:
    case RIGHTMOUSE: {
      if (event->val == KM_PRESS) {
        if (cd->is_active == true) {
          op_generic_value_restore(op);
        }
        op_generic_value_exit(op);
        return OPERATOR_CANCELLED;
      }
      break;
    }
  }
  return OPERATOR_RUNNING_MODAL;
}

void WM_operator_type_modal_from_exec_for_object_edit_coords(wmOperatorType *ot)
{
  PropertyRNA *prop;

  BLI_assert(ot->modal == nullptr);
  BLI_assert(ot->invoke == nullptr);
  BLI_assert(ot->cancel == nullptr);
  BLI_assert(ot->prop != nullptr);

  ot->invoke = op_generic_value_invoke;
  ot->modal = op_generic_value_modal;
  ot->cancel = op_generic_value_cancel;

  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */
