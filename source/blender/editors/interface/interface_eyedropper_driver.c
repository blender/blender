/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_eyedropper_driver.c
 *  \ingroup edinterface
 *
 * Eyedropper (Animation Driver Targets).
 *
 * Defines:
 * - #UI_OT_eyedropper_driver
 */

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"

#include "BLI_math_vector.h"

#include "BKE_context.h"
#include "BKE_animsys.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_keyframing.h"

#include "interface_intern.h"
#include "interface_eyedropper_intern.h"

typedef struct DriverDropper {
	/* Destination property (i.e. where we'll add a driver) */
	PointerRNA ptr;
	PropertyRNA *prop;
	int index;

	// TODO: new target?
} DriverDropper;

static bool driverdropper_init(bContext *C, wmOperator *op)
{
	DriverDropper *ddr;
	uiBut *but;

	op->customdata = ddr = MEM_callocN(sizeof(DriverDropper), "DriverDropper");

	but = UI_context_active_but_prop_get(C, &ddr->ptr, &ddr->prop, &ddr->index);

	if ((ddr->ptr.data == NULL) ||
	    (ddr->prop == NULL) ||
	    (RNA_property_editable(&ddr->ptr, ddr->prop) == false) ||
	    (RNA_property_animateable(&ddr->ptr, ddr->prop) == false) ||
	    (but->flag & UI_BUT_DRIVEN))
	{
		return false;
	}

	return true;
}

static void driverdropper_exit(bContext *C, wmOperator *op)
{
	WM_cursor_modal_restore(CTX_wm_window(C));

	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata = NULL;
	}
}

static void driverdropper_sample(bContext *C, wmOperator *op, const wmEvent *event)
{
	DriverDropper *ddr = (DriverDropper *)op->customdata;
	uiBut *but = eyedropper_get_property_button_under_mouse(C, event);

	short mapping_type = RNA_enum_get(op->ptr, "mapping_type");
	short flag = 0;

	/* we can only add a driver if we know what RNA property it corresponds to */
	if (but == NULL) {
		return;
	}
	else {
		/* Get paths for src... */
		PointerRNA *target_ptr = &but->rnapoin;
		PropertyRNA *target_prop = but->rnaprop;
		int target_index = but->rnaindex;

		char *target_path = RNA_path_from_ID_to_property(target_ptr, target_prop);

		/* ... and destination */
		char *dst_path    = BKE_animdata_driver_path_hack(C, &ddr->ptr, ddr->prop, NULL);

		/* Now create driver(s) */
		if (target_path && dst_path) {
			int success = ANIM_add_driver_with_target(op->reports,
			                                          ddr->ptr.id.data, dst_path, ddr->index,
			                                          target_ptr->id.data, target_path, target_index,
			                                          flag, DRIVER_TYPE_PYTHON, mapping_type);

			if (success) {
				/* send updates */
				UI_context_update_anim_flag(C);
				DEG_relations_tag_update(CTX_data_main(C));
				DEG_id_tag_update(ddr->ptr.id.data, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
				WM_event_add_notifier(C, NC_ANIMATION | ND_FCURVES_ORDER, NULL);  // XXX
			}
		}

		/* cleanup */
		if (target_path)
			MEM_freeN(target_path);
		if (dst_path)
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
	/* handle modal keymap */
	if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case EYE_MODAL_CANCEL:
				driverdropper_cancel(C, op);
				return OPERATOR_CANCELLED;

			case EYE_MODAL_SAMPLE_CONFIRM:
				driverdropper_sample(C, op, event);
				driverdropper_exit(C, op);

				return OPERATOR_FINISHED;
		}
	}

	return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int driverdropper_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	/* init */
	if (driverdropper_init(C, op)) {
		WM_cursor_modal_set(CTX_wm_window(C), BC_EYEDROPPER_CURSOR);

		/* add temp handler */
		WM_event_add_modal_handler(C, op);

		return OPERATOR_RUNNING_MODAL;
	}
	else {
		driverdropper_exit(C, op);
		return OPERATOR_CANCELLED;
	}
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
	else {
		return OPERATOR_CANCELLED;
	}
}

static bool driverdropper_poll(bContext *C)
{
	if (!CTX_wm_window(C)) return 0;
	else return 1;
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
	ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "mapping_type", prop_driver_create_mapping_types, 0,
	             "Mapping Type", "Method used to match target and driven properties");
}
