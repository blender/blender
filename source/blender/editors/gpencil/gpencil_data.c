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
 * The Original Code is Copyright (C) 2008, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Operators for dealing with GP datablocks and layers
 */

/** \file blender/editors/gpencil/gpencil_data.c
 *  \ingroup edgpencil
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_gpencil_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_gpencil.h"

#include "gpencil_intern.h"


/* ************************************************ */
/* Datablock Operators */

/* ******************* Add New Data ************************ */

/* add new datablock - wrapper around API */
static int gp_data_add_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers(C, NULL);
	
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
		return OPERATOR_CANCELLED;
	}
	else {
		/* decrement user count and add new datablock */
		bGPdata *gpd = (*gpd_ptr);
		
		id_us_min(&gpd->id);
		*gpd_ptr = gpencil_data_addnew(DATA_("GPencil"));
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_data_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Grease Pencil Add New";
	ot->idname = "GPENCIL_OT_data_add";
	ot->description = "Add new Grease Pencil datablock";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_data_add_exec;
	ot->poll = gp_add_poll;
}

/* ******************* Unlink Data ************************ */

/* poll callback for adding data/layers - special */
static int gp_data_unlink_poll(bContext *C)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers(C, NULL);
	
	/* if we have access to some active data, make sure there's a datablock before enabling this */
	return (gpd_ptr && *gpd_ptr);
}


/* unlink datablock - wrapper around API */
static int gp_data_unlink_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers(C, NULL);
	
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
		return OPERATOR_CANCELLED;
	}
	else {
		/* just unlink datablock now, decreasing its user count */
		bGPdata *gpd = (*gpd_ptr);

		id_us_min(&gpd->id);
		*gpd_ptr = NULL;
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_data_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Grease Pencil Unlink";
	ot->idname = "GPENCIL_OT_data_unlink";
	ot->description = "Unlink active Grease Pencil datablock";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_data_unlink_exec;
	ot->poll = gp_data_unlink_poll;
}


/* ************************************************ */
/* Layer Operators */

/* ******************* Add New Layer ************************ */

/* add new layer - wrapper around API */
static int gp_layer_add_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers(C, NULL);
	
	/* if there's no existing Grease-Pencil data there, add some */
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
		return OPERATOR_CANCELLED;
	}
	if (*gpd_ptr == NULL)
		*gpd_ptr = gpencil_data_addnew(DATA_("GPencil"));
	
	/* add new layer now */
	gpencil_layer_addnew(*gpd_ptr, DATA_("GP_Layer"), 1);
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add New Layer";
	ot->idname = "GPENCIL_OT_layer_add";
	ot->description = "Add new Grease Pencil layer for the active Grease Pencil datablock";
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_layer_add_exec;
	ot->poll = gp_add_poll;
}

/* ******************* Remove Active Layer ************************* */

static int gp_layer_remove_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	
	/* sanity checks */
	if (ELEM(NULL, gpd, gpl))
		return OPERATOR_CANCELLED;
	
	if (gpl->flag & GP_LAYER_LOCKED) {
		BKE_report(op->reports, RPT_ERROR, "Cannot delete locked layers");
		return OPERATOR_CANCELLED;
	}
	
	/* make the layer before this the new active layer
	 * - use the one after if this is the first
	 * - if this is the only layer, this naturally becomes NULL
	 */
	if (gpl->prev)
		gpencil_layer_setactive(gpd, gpl->prev);
	else
		gpencil_layer_setactive(gpd, gpl->next);
	
	/* delete the layer now... */
	gpencil_layer_delete(gpd, gpl);
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Layer";
	ot->idname = "GPENCIL_OT_layer_remove";
	ot->description = "Remove active Grease Pencil layer";
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_layer_remove_exec;
	ot->poll = gp_active_layer_poll;
}

/* ******************* Move Layer Up/Down ************************** */

enum {
	GP_LAYER_MOVE_UP   = -1,
	GP_LAYER_MOVE_DOWN = 1
};

static int gp_layer_move_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	
	int direction = RNA_enum_get(op->ptr, "type");
	
	/* sanity checks */
	if (ELEM(NULL, gpd, gpl))
		return OPERATOR_CANCELLED;
	
	/* up or down? */
	if (direction == GP_LAYER_MOVE_UP) {
		/* up */
		BLI_remlink(&gpd->layers, gpl);
		BLI_insertlinkbefore(&gpd->layers, gpl->prev, gpl);
	}
	else {
		/* down */
		BLI_remlink(&gpd->layers, gpl);
		BLI_insertlinkafter(&gpd->layers, gpl->next, gpl);
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_move(wmOperatorType *ot)
{
	static EnumPropertyItem slot_move[] = {
		{GP_LAYER_MOVE_UP, "UP", 0, "Up", ""},
		{GP_LAYER_MOVE_DOWN, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name = "Move Grease Pencil Layer";
	ot->idname = "GPENCIL_OT_layer_move";
	ot->description = "Move the active Grease Pencil layer up/down in the list";
	
	/* api callbacks */
	ot->exec = gp_layer_move_exec;
	ot->poll = gp_active_layer_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	ot->prop = RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

/* ********************* Duplicate Layer ************************** */

static int gp_layer_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	bGPDlayer *new_layer;
	
	/* sanity checks */
	if (ELEM(NULL, gpd, gpl))
		return OPERATOR_CANCELLED;
	
	/* make copy of layer, and add it immediately after the existing layer */
	new_layer = gpencil_layer_duplicate(gpl);
	BLI_insertlinkafter(&gpd->layers, gpl, new_layer);
	
	/* ensure new layer has a unique name, and is now the active layer */
	BLI_uniquename(&gpd->layers, new_layer, DATA_("GP_Layer"), '.', offsetof(bGPDlayer, info), sizeof(new_layer->info));
	gpencil_layer_setactive(gpd, new_layer);
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Layer";
	ot->idname = "GPENCIL_OT_layer_duplicate";
	ot->description = "Make a copy of the active Grease Pencil layer";
	
	/* callbacks */
	ot->exec = gp_layer_copy_exec;
	ot->poll = gp_active_layer_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *********************** Hide Layers ******************************** */

static int gp_hide_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *layer = gpencil_layer_getactive(gpd);
	bool unselected = RNA_boolean_get(op->ptr, "unselected");
	
	/* sanity checks */
	if (ELEM(NULL, gpd, layer))
		return OPERATOR_CANCELLED;
	
	if (unselected) {
		bGPDlayer *gpl;
		
		/* hide unselected */
		for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			if (gpl != layer) {
				gpl->flag |= GP_LAYER_HIDE;
			}
		}
	}
	else {
		/* hide selected/active */
		layer->flag |= GP_LAYER_HIDE;
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide Layer(s)";
	ot->idname = "GPENCIL_OT_hide";
	ot->description = "Hide selected/unselected Grease Pencil layers";
	
	/* callbacks */
	ot->exec = gp_hide_exec;
	ot->poll = gp_active_layer_poll; /* NOTE: we need an active layer to play with */
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected layers");
}

/* ********************** Show All Layers ***************************** */

/* poll callback for showing layers */
static int gp_reveal_poll(bContext *C)
{
	return ED_gpencil_data_get_active(C) != NULL;
}

static int gp_reveal_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl;
	
	/* sanity checks */
	if (gpd == NULL)
		return OPERATOR_CANCELLED;
	
	/* make all layers visible */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		gpl->flag &= ~GP_LAYER_HIDE;
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Show All Layers";
	ot->idname = "GPENCIL_OT_reveal";
	ot->description = "Show all Grease Pencil layers";
	
	/* callbacks */
	ot->exec = gp_reveal_exec;
	ot->poll = gp_reveal_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ***************** Lock/Unlock All Layers ************************ */

static int gp_lock_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl;
	
	/* sanity checks */
	if (gpd == NULL)
		return OPERATOR_CANCELLED;
	
	/* make all layers non-editable */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		gpl->flag |= GP_LAYER_LOCKED;
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_lock_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Lock All Layers";
	ot->idname = "GPENCIL_OT_lock_all";
	ot->description = "Lock all Grease Pencil layers to prevent them from being accidentally modified";
	
	/* callbacks */
	ot->exec = gp_lock_all_exec;
	ot->poll = gp_reveal_poll; /* XXX: could use dedicated poll later */
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------- */

static int gp_unlock_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl;
	
	/* sanity checks */
	if (gpd == NULL)
		return OPERATOR_CANCELLED;
	
	/* make all layers editable again */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		gpl->flag &= ~GP_LAYER_LOCKED;
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_unlock_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unlock All Layers";
	ot->idname = "GPENCIL_OT_unlock_all";
	ot->description = "Unlock all Grease Pencil layers so that they can be edited";
	
	/* callbacks */
	ot->exec = gp_unlock_all_exec;
	ot->poll = gp_reveal_poll; /* XXX: could use dedicated poll later */
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************** Isolate Layer **************************** */

static int gp_isolate_layer_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *layer = gpencil_layer_getactive(gpd);
	bGPDlayer *gpl;
	int flags = GP_LAYER_LOCKED;
	bool isolate = false;
	
	if (RNA_boolean_get(op->ptr, "affect_visibility"))
		flags |= GP_LAYER_HIDE;
		
	if (ELEM(NULL, gpd, layer)) {
		BKE_report(op->reports, RPT_ERROR, "No active layer to isolate");
		return OPERATOR_CANCELLED;
	}
	
	/* Test whether to isolate or clear all flags */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* Skip if this is the active layer */
		if (gpl == layer)
			continue;
		
		/* If the flags aren't set, that means that the layer is
		 * not alone, so we have some layers to isolate still
		 */
		if ((gpl->flag & flags) == 0) {
			isolate = true;
			break;
		}
	}
	
	/* Set/Clear flags as appropriate */
	/* TODO: Include onionskinning on this list? */
	if (isolate) {
		/* Set flags on all "other" layers */
		for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			if (gpl == layer)
				continue;
			else
				gpl->flag |= flags;
		}
	}
	else {
		/* Clear flags - Restore everything else */
		for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			gpl->flag &= ~flags;
		}
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_isolate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Isolate Layer";
	ot->idname = "GPENCIL_OT_layer_isolate";
	ot->description = "Toggle whether the active layer is the only one that can be edited and/or visible";
	
	/* callbacks */
	ot->exec = gp_isolate_layer_exec;
	ot->poll = gp_active_layer_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "affect_visibility", false, "Affect Visibility",
	                "In addition to toggling the editability, also affect the visibility");
}

/* ********************** Change Layer ***************************** */

static int gp_layer_change_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(evt))
{
	uiPopupMenu *pup;
	uiLayout *layout;
	
	/* call the menu, which will call this operator again, hence the canceled */
	pup = UI_popup_menu_begin(C, op->type->name, ICON_NONE);
	layout = UI_popup_menu_layout(pup);
	uiItemsEnumO(layout, "GPENCIL_OT_layer_change", "layer");
	UI_popup_menu_end(C, pup);
	
	return OPERATOR_INTERFACE;
}

static int gp_layer_change_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	bGPDlayer *gpl = NULL;
	int layer_num = RNA_enum_get(op->ptr, "layer");
	
	/* Get layer or create new one */
	if (layer_num == -1) {
		/* Create layer */
		gpl = gpencil_layer_addnew(gpd, DATA_("GP_Layer"), true);
	}
	else {
		/* Try to get layer */
		gpl = BLI_findlink(&gpd->layers, layer_num);
		
		if (gpl == NULL) {
			BKE_reportf(op->reports, RPT_ERROR, "Cannot change to non-existent layer (index = %d)", layer_num);
			return OPERATOR_CANCELLED;
		}
	}
	
	/* Set active layer */
	gpencil_layer_setactive(gpd, gpl);
	
	/* updates */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_change(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Layer";
	ot->idname = "GPENCIL_OT_layer_change";
	ot->description = "Change active Grease Pencil layer";
	
	/* callbacks */
	ot->invoke = gp_layer_change_invoke;
	ot->exec = gp_layer_change_exec;
	ot->poll = gp_active_layer_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* gp layer to use (dynamic enum) */
	ot->prop = RNA_def_enum(ot->srna, "layer", DummyRNA_DEFAULT_items, 0, "Grease Pencil Layer", "");
	RNA_def_enum_funcs(ot->prop, ED_gpencil_layers_with_new_enum_itemf);
}

/* ************************************************ */
