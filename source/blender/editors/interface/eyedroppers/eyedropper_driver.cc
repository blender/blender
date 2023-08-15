/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Eyedropper (Animation Driver Targets).
 *
 * Defines:
 * - #UI_OT_eyedropper_driver
 */

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_animsys.h"
#include "BKE_context.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_path.hh"

#include "UI_interface.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_keyframing.hh"

#include "eyedropper_intern.hh"
#include "interface_intern.hh"

struct DriverDropper {
  /* Destination property (i.e. where we'll add a driver) */
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
  bool is_undo;

  /* TODO: new target? */
};

static bool driverdropper_init(bContext *C, wmOperator *op)
{
  DriverDropper *ddr = MEM_cnew<DriverDropper>(__func__);

  uiBut *but = UI_context_active_but_prop_get(C, &ddr->ptr, &ddr->prop, &ddr->index);

  if ((ddr->ptr.data == nullptr) || (ddr->prop == nullptr) ||
      (RNA_property_editable(&ddr->ptr, ddr->prop) == false) ||
      (RNA_property_animateable(&ddr->ptr, ddr->prop) == false) || (but->flag & UI_BUT_DRIVEN))
  {
    MEM_freeN(ddr);
    return false;
  }
  op->customdata = ddr;

  ddr->is_undo = UI_but_flag_is_set(but, UI_BUT_UNDO);

  return true;
}

static void driverdropper_exit(bContext *C, wmOperator *op)
{
  WM_cursor_modal_restore(CTX_wm_window(C));

  MEM_SAFE_FREE(op->customdata);
}

static void driverdropper_sample(bContext *C, wmOperator *op, const wmEvent *event)
{
  DriverDropper *ddr = static_cast<DriverDropper *>(op->customdata);
  uiBut *but = eyedropper_get_property_button_under_mouse(C, event);

  const short mapping_type = RNA_enum_get(op->ptr, "mapping_type");
  const short flag = 0;

  /* we can only add a driver if we know what RNA property it corresponds to */
  if (but == nullptr) {
    return;
  }
  /* Get paths for the source. */
  PointerRNA *target_ptr = &but->rnapoin;
  PropertyRNA *target_prop = but->rnaprop;
  const int target_index = but->rnaindex;

  char *target_path = RNA_path_from_ID_to_property(target_ptr, target_prop);

  /* Get paths for the destination. */
  char *dst_path = RNA_path_from_ID_to_property(&ddr->ptr, ddr->prop);

  /* Now create driver(s) */
  if (target_path && dst_path) {
    int success = ANIM_add_driver_with_target(op->reports,
                                              ddr->ptr.owner_id,
                                              dst_path,
                                              ddr->index,
                                              target_ptr->owner_id,
                                              target_path,
                                              target_index,
                                              flag,
                                              DRIVER_TYPE_PYTHON,
                                              mapping_type);

    if (success) {
      /* send updates */
      UI_context_update_anim_flag(C);
      DEG_relations_tag_update(CTX_data_main(C));
      DEG_id_tag_update(ddr->ptr.owner_id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_ANIMATION | ND_FCURVES_ORDER, nullptr); /* XXX */
    }
  }

  /* cleanup */
  if (target_path) {
    MEM_freeN(target_path);
  }
  if (dst_path) {
    MEM_freeN(dst_path);
  }
}

static void driverdropper_cancel(bContext *C, wmOperator *op)
{
  driverdropper_exit(C, op);
}

/* main modal status check */
static int driverdropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  DriverDropper *ddr = static_cast<DriverDropper *>(op->customdata);

  /* handle modal keymap */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case EYE_MODAL_CANCEL: {
        driverdropper_cancel(C, op);
        return OPERATOR_CANCELLED;
      }
      case EYE_MODAL_SAMPLE_CONFIRM: {
        const bool is_undo = ddr->is_undo;
        driverdropper_sample(C, op, event);
        driverdropper_exit(C, op);
        /* Could support finished & undo-skip. */
        return is_undo ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
      }
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int driverdropper_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  /* init */
  if (driverdropper_init(C, op)) {
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
static int driverdropper_exec(bContext *C, wmOperator *op)
{
  /* init */
  if (driverdropper_init(C, op)) {
    /* cleanup */
    driverdropper_exit(C, op);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static bool driverdropper_poll(bContext *C)
{
  if (!CTX_wm_window(C)) {
    return false;
  }
  return true;
}

void UI_OT_eyedropper_driver(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Eyedropper Driver";
  ot->idname = "UI_OT_eyedropper_driver";
  ot->description = "Pick a property to use as a driver target";

  /* api callbacks */
  ot->invoke = driverdropper_invoke;
  ot->modal = driverdropper_modal;
  ot->cancel = driverdropper_cancel;
  ot->exec = driverdropper_exec;
  ot->poll = driverdropper_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* properties */
  RNA_def_enum(ot->srna,
               "mapping_type",
               prop_driver_create_mapping_types,
               0,
               "Mapping Type",
               "Method used to match target and driven properties");
}
