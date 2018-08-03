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
 * The Original Code is Copyright (C) 2014, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_select.c
 *  \ingroup edgpencil
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_lasso_2d.h"
#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_report.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

/* ********************************************** */
/* Polling callbacks */

static bool gpencil_select_poll(bContext *C)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);

	/* we just need some visible strokes, and to be in editmode or other modes only to catch event */
	if (GPENCIL_ANY_MODE(gpd)) {
		/* TODO: include a check for visible strokes? */
		if (gpd->layers.first)
			return true;
	}

	return false;
}

/* ********************************************** */
/* Select All Operator */

static int gpencil_select_all_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	int action = RNA_enum_get(op->ptr, "action");

	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}

	/* if not edit/sculpt mode, the event is catched but not processed */
	if (GPENCIL_NONE_EDIT_MODE(gpd)) {
		return OPERATOR_CANCELLED;
	}

	/* for "toggle", test for existing selected strokes */
	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;

		CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
		{
			if (gps->flag & GP_STROKE_SELECT) {
				action = SEL_DESELECT;
				break; // XXX: this only gets out of the inner loop...
			}
		}
		CTX_DATA_END;
	}

	/* if deselecting, we need to deselect strokes across all frames
	 *  - Currently, an exception is only given for deselection
	 *    Selecting and toggling should only affect what's visible,
	 *    while deselecting helps clean up unintended/forgotten
	 *    stuff on other frames
	 */
	if (action == SEL_DESELECT) {
		/* deselect strokes across editable layers
		 * NOTE: we limit ourselves to editable layers, since once a layer is "locked/hidden
		 *       nothing should be able to touch it
		 */
		CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
		{
			bGPDframe *gpf;

			/* deselect all strokes on all frames */
			for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
				bGPDstroke *gps;

				for (gps = gpf->strokes.first; gps; gps = gps->next) {
					bGPDspoint *pt;
					int i;

					/* only edit strokes that are valid in this view... */
					if (ED_gpencil_stroke_can_use(C, gps)) {
						for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
							pt->flag &= ~GP_SPOINT_SELECT;
						}

						gps->flag &= ~GP_STROKE_SELECT;
					}
				}
			}
		}
		CTX_DATA_END;
	}
	else {
		/* select or deselect all strokes */
		CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
		{
			bGPDspoint *pt;
			int i;
			bool selected = false;

			/* Change selection status of all points, then make the stroke match */
			for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
				switch (action) {
					case SEL_SELECT:
						pt->flag |= GP_SPOINT_SELECT;
						break;
					//case SEL_DESELECT:
					//	pt->flag &= ~GP_SPOINT_SELECT;
					//	break;
					case SEL_INVERT:
						pt->flag ^= GP_SPOINT_SELECT;
						break;
				}

				if (pt->flag & GP_SPOINT_SELECT)
					selected = true;
			}

			/* Change status of stroke */
			if (selected)
				gps->flag |= GP_STROKE_SELECT;
			else
				gps->flag &= ~GP_STROKE_SELECT;
		}
		CTX_DATA_END;
	}

	/* updates */
	DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

	/* copy on write tag is needed, or else no refresh happens */
	DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

	WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All Strokes";
	ot->idname = "GPENCIL_OT_select_all";
	ot->description = "Change selection of all Grease Pencil strokes currently visible";

	/* callbacks */
	ot->exec = gpencil_select_all_exec;
	ot->poll = gpencil_select_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

/* ********************************************** */
/* Select Linked */

static int gpencil_select_linked_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);

	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}

	/* if not edit/sculpt mode, the event is catched but not processed */
	if (GPENCIL_NONE_EDIT_MODE(gpd)) {
		return OPERATOR_CANCELLED;
	}

	/* select all points in selected strokes */
	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		if (gps->flag & GP_STROKE_SELECT) {
			bGPDspoint *pt;
			int i;

			for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
				pt->flag |= GP_SPOINT_SELECT;
			}
		}
	}
	CTX_DATA_END;

	/* updates */
	DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

	/* copy on write tag is needed, or else no refresh happens */
	DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

	WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->idname = "GPENCIL_OT_select_linked";
	ot->description = "Select all points in same strokes as already selected points";

	/* callbacks */
	ot->exec = gpencil_select_linked_exec;
	ot->poll = gpencil_select_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************************** */
/* Select Alternate */

static int gpencil_select_alternate_exec(bContext *C, wmOperator *op)
{
	const bool unselect_ends = RNA_boolean_get(op->ptr, "unselect_ends");
	bGPdata *gpd = ED_gpencil_data_get_active(C);

	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}

	/* if not edit/sculpt mode, the event is catched but not processed */
	if (GPENCIL_NONE_EDIT_MODE(gpd)) {
		return OPERATOR_CANCELLED;
	}

	/* select all points in selected strokes */
	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		if ((gps->flag & GP_STROKE_SELECT) && (gps->totpoints > 1)) {
			bGPDspoint *pt;
			int row = 0;
			int start = 0;
			if (unselect_ends) {
				start = 1;
			}

			for (int i = start; i < gps->totpoints; i++) {
				pt = &gps->points[i];
				if ((row % 2) == 0) {
					pt->flag |= GP_SPOINT_SELECT;
				}
				else {
					pt->flag &= ~GP_SPOINT_SELECT;
				}
				row++;
			}

			/* unselect start and end points */
			if (unselect_ends) {
				pt = &gps->points[0];
				pt->flag &= ~GP_SPOINT_SELECT;

				pt = &gps->points[gps->totpoints - 1];
				pt->flag &= ~GP_SPOINT_SELECT;
			}
		}
	}
	CTX_DATA_END;

	/* updates */
	DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

	/* copy on write tag is needed, or else no refresh happens */
	DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

	WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_alternate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Alternated";
	ot->idname = "GPENCIL_OT_select_alternate";
	ot->description = "Select alternative points in same strokes as already selected points";

	/* callbacks */
	ot->exec = gpencil_select_alternate_exec;
	ot->poll = gpencil_select_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "unselect_ends", true, "Unselect Ends", "Do not select the first and last point of the stroke");
}

/* ********************************************** */
/* Select Grouped */

typedef enum eGP_SelectGrouped {
	/* Select strokes in the same layer */
	GP_SEL_SAME_LAYER     = 0,

	/* Select strokes with the same color */
	GP_SEL_SAME_COLOR     = 1,

	/* TODO: All with same prefix - Useful for isolating all layers for a particular character for instance */
	/* TODO: All with same appearance - colour/opacity/volumetric/fills ? */
} eGP_SelectGrouped;

/* ----------------------------------- */

/* On each visible layer, check for selected strokes - if found, select all others */
static void gp_select_same_layer(bContext *C)
{
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	int cfra_eval = (int)DEG_get_ctime(depsgraph);

	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *gpf = BKE_gpencil_layer_getframe(gpl, cfra_eval, 0);
		bGPDstroke *gps;
		bool found = false;

		if (gpf == NULL)
			continue;

		/* Search for a selected stroke */
		for (gps = gpf->strokes.first; gps; gps = gps->next) {
			if (ED_gpencil_stroke_can_use(C, gps)) {
				if (gps->flag & GP_STROKE_SELECT) {
					found = true;
					break;
				}
			}
		}

		/* Select all if found */
		if (found) {
			for (gps = gpf->strokes.first; gps; gps = gps->next) {
				if (ED_gpencil_stroke_can_use(C, gps)) {
					bGPDspoint *pt;
					int i;

					for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
						pt->flag |= GP_SPOINT_SELECT;
					}

					gps->flag |= GP_STROKE_SELECT;
				}
			}
		}
	}
	CTX_DATA_END;
}

/* Select all strokes with same colors as selected ones */
static void gp_select_same_color(bContext *C)
{
	/* First, build set containing all the colors of selected strokes */
	GSet *selected_colors = BLI_gset_str_new("GP Selected Colors");

	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		if (gps->flag & GP_STROKE_SELECT) {
			/* add instead of insert here, otherwise the uniqueness check gets skipped,
			 * and we get many duplicate entries...
			 */
			BLI_gset_add(selected_colors, &gps->mat_nr);
		}
	}
	CTX_DATA_END;

	/* Second, select any visible stroke that uses these colors */
	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		if (BLI_gset_haskey(selected_colors, &gps->mat_nr)) {
			/* select this stroke */
			bGPDspoint *pt;
			int i;

			for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
				pt->flag |= GP_SPOINT_SELECT;
			}

			gps->flag |= GP_STROKE_SELECT;
		}
	}
	CTX_DATA_END;

	/* free memomy */
	if (selected_colors != NULL) {
		BLI_gset_free(selected_colors, NULL);
	}
}


/* ----------------------------------- */

static int gpencil_select_grouped_exec(bContext *C, wmOperator *op)
{
	eGP_SelectGrouped mode = RNA_enum_get(op->ptr, "type");
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	/* if not edit/sculpt mode, the event is catched but not processed */
	if (GPENCIL_NONE_EDIT_MODE(gpd)) {
		return OPERATOR_CANCELLED;
	}

	switch (mode) {
		case GP_SEL_SAME_LAYER:
			gp_select_same_layer(C);
			break;
		case GP_SEL_SAME_COLOR:
			gp_select_same_color(C);
			break;

		default:
			BLI_assert(!"unhandled select grouped gpencil mode");
			break;
	}

	/* updates */
	DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

	/* copy on write tag is needed, or else no refresh happens */
	DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

	WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_grouped(wmOperatorType *ot)
{
	static const EnumPropertyItem prop_select_grouped_types[] = {
		{GP_SEL_SAME_LAYER, "LAYER", 0, "Layer", "Shared layers"},
		{GP_SEL_SAME_COLOR, "COLOR", 0, "Color", "Shared colors"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Select Grouped";
	ot->idname = "GPENCIL_OT_select_grouped";
	ot->description = "Select all strokes with similar characteristics";

	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = gpencil_select_grouped_exec;
	ot->poll = gpencil_select_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_select_grouped_types, GP_SEL_SAME_LAYER, "Type", "");
}

/* ********************************************** */
/* Select First */

static int gpencil_select_first_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	/* if not edit/sculpt mode, the event is catched but not processed */
	if (GPENCIL_NONE_EDIT_MODE(gpd)) {
		return OPERATOR_CANCELLED;
	}

	const bool only_selected = RNA_boolean_get(op->ptr, "only_selected_strokes");
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		/* skip stroke if we're only manipulating selected strokes */
		if (only_selected && !(gps->flag & GP_STROKE_SELECT)) {
			continue;
		}

		/* select first point */
		BLI_assert(gps->totpoints >= 1);

		gps->points->flag |= GP_SPOINT_SELECT;
		gps->flag |= GP_STROKE_SELECT;

		/* deselect rest? */
		if ((extend == false) && (gps->totpoints > 1)) {
			/* start from index 1, to skip the first point that we'd just selected... */
			bGPDspoint *pt = &gps->points[1];
			int i = 1;

			for (; i < gps->totpoints; i++, pt++) {
				pt->flag &= ~GP_SPOINT_SELECT;
			}
		}
	}
	CTX_DATA_END;

	/* updates */
	DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

	/* copy on write tag is needed, or else no refresh happens */
	DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

	WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_first(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select First";
	ot->idname = "GPENCIL_OT_select_first";
	ot->description = "Select first point in Grease Pencil strokes";

	/* callbacks */
	ot->exec = gpencil_select_first_exec;
	ot->poll = gpencil_select_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "only_selected_strokes", false, "Selected Strokes Only",
	                "Only select the first point of strokes that already have points selected");

	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting all other selected points");
}

/* ********************************************** */
/* Select First */

static int gpencil_select_last_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	/* if not edit/sculpt mode, the event is catched but not processed */
	if (GPENCIL_NONE_EDIT_MODE(gpd)) {
		return OPERATOR_CANCELLED;
	}

	const bool only_selected = RNA_boolean_get(op->ptr, "only_selected_strokes");
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		/* skip stroke if we're only manipulating selected strokes */
		if (only_selected && !(gps->flag & GP_STROKE_SELECT)) {
			continue;
		}

		/* select last point */
		BLI_assert(gps->totpoints >= 1);

		gps->points[gps->totpoints - 1].flag |= GP_SPOINT_SELECT;
		gps->flag |= GP_STROKE_SELECT;

		/* deselect rest? */
		if ((extend == false) && (gps->totpoints > 1)) {
			/* don't include the last point... */
			bGPDspoint *pt = gps->points;
			int i = 1;

			for (; i < gps->totpoints - 1; i++, pt++) {
				pt->flag &= ~GP_SPOINT_SELECT;
			}
		}
	}
	CTX_DATA_END;

	/* updates */
	DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

	/* copy on write tag is needed, or else no refresh happens */
	DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

	WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_last(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Last";
	ot->idname = "GPENCIL_OT_select_last";
	ot->description = "Select last point in Grease Pencil strokes";

	/* callbacks */
	ot->exec = gpencil_select_last_exec;
	ot->poll = gpencil_select_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "only_selected_strokes", false, "Selected Strokes Only",
	                "Only select the last point of strokes that already have points selected");

	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting all other selected points");
}

/* ********************************************** */
/* Select More */

static int gpencil_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	/* if not edit/sculpt mode, the event is catched but not processed */
	if (GPENCIL_NONE_EDIT_MODE(gpd)) {
		return OPERATOR_CANCELLED;
	}

	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		if (gps->flag & GP_STROKE_SELECT) {
			bGPDspoint *pt;
			int i;
			bool prev_sel;

			/* First Pass: Go in forward order, expanding selection if previous was selected (pre changes)...
			 * - This pass covers the "after" edges of selection islands
			 */
			prev_sel = false;
			for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
				if (pt->flag & GP_SPOINT_SELECT) {
					/* selected point - just set flag for next point */
					prev_sel = true;
				}
				else {
					/* unselected point - expand selection if previous was selected... */
					if (prev_sel) {
						pt->flag |= GP_SPOINT_SELECT;
					}
					prev_sel = false;
				}
			}

			/* Second Pass: Go in reverse order, doing the same as before (except in opposite order)
			 * - This pass covers the "before" edges of selection islands
			 */
			prev_sel = false;
			for (pt -= 1; i > 0; i--, pt--) {
				if (pt->flag & GP_SPOINT_SELECT) {
					prev_sel = true;
				}
				else {
					/* unselected point - expand selection if previous was selected... */
					if (prev_sel) {
						pt->flag |= GP_SPOINT_SELECT;
					}
					prev_sel = false;
				}
			}
		}
	}
	CTX_DATA_END;

	/* updates */
	DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

	/* copy on write tag is needed, or else no refresh happens */
	DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

	WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->idname = "GPENCIL_OT_select_more";
	ot->description = "Grow sets of selected Grease Pencil points";

	/* callbacks */
	ot->exec = gpencil_select_more_exec;
	ot->poll = gpencil_select_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************************** */
/* Select Less */

static int gpencil_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	/* if not edit/sculpt mode, the event is catched but not processed */
	if (GPENCIL_NONE_EDIT_MODE(gpd)) {
		return OPERATOR_CANCELLED;
	}

	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		if (gps->flag & GP_STROKE_SELECT) {
			bGPDspoint *pt;
			int i;
			bool prev_sel;

			/* First Pass: Go in forward order, shrinking selection if previous was not selected (pre changes)...
			 * - This pass covers the "after" edges of selection islands
			 */
			prev_sel = false;
			for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
				if (pt->flag & GP_SPOINT_SELECT) {
					/* shrink if previous wasn't selected */
					if (prev_sel == false) {
						pt->flag &= ~GP_SPOINT_SELECT;
					}
					prev_sel = true;
				}
				else {
					/* mark previous as being unselected - and hence, is trigger for shrinking */
					prev_sel = false;
				}
			}

			/* Second Pass: Go in reverse order, doing the same as before (except in opposite order)
			 * - This pass covers the "before" edges of selection islands
			 */
			prev_sel = false;
			for (pt -= 1; i > 0; i--, pt--) {
				if (pt->flag & GP_SPOINT_SELECT) {
					/* shrink if previous wasn't selected */
					if (prev_sel == false) {
						pt->flag &= ~GP_SPOINT_SELECT;
					}
					prev_sel = true;
				}
				else {
					/* mark previous as being unselected - and hence, is trigger for shrinking */
					prev_sel = false;
				}
			}
		}
	}
	CTX_DATA_END;

	/* updates */
	DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

	/* copy on write tag is needed, or else no refresh happens */
	DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

	WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->idname = "GPENCIL_OT_select_less";
	ot->description = "Shrink sets of selected Grease Pencil points";

	/* callbacks */
	ot->exec = gpencil_select_less_exec;
	ot->poll = gpencil_select_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************************** */
/* Circle Select Operator */

/* Helper to check if a given stroke is within the area */
/* NOTE: Code here is adapted (i.e. copied directly) from gpencil_paint.c::gp_stroke_eraser_dostroke()
 *       It would be great to de-duplicate the logic here sometime, but that can wait...
 */
static bool gp_stroke_do_circle_sel(
        bGPDstroke *gps, GP_SpaceConversion *gsc,
        const int mx, const int my, const int radius,
        const bool select, rcti *rect, float diff_mat[4][4])
{
	bGPDspoint *pt1, *pt2;
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	int i;
	bool changed = false;

	if (gps->totpoints == 1) {
		bGPDspoint pt_temp;
		gp_point_to_parent_space(gps->points, diff_mat, &pt_temp);
		gp_point_to_xy(gsc, gps, &pt_temp, &x0, &y0);

		/* do boundbox check first */
		if ((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(rect, x0, y0)) {
			/* only check if point is inside */
			if (((x0 - mx) * (x0 - mx) + (y0 - my) * (y0 - my)) <= radius * radius) {
				/* change selection */
				if (select) {
					gps->points->flag |= GP_SPOINT_SELECT;
					gps->flag |= GP_STROKE_SELECT;
				}
				else {
					gps->points->flag &= ~GP_SPOINT_SELECT;
					gps->flag &= ~GP_STROKE_SELECT;
				}

				return true;
			}
		}
	}
	else {
		/* Loop over the points in the stroke, checking for intersections
		 *  - an intersection means that we touched the stroke
		 */
		for (i = 0; (i + 1) < gps->totpoints; i++) {
			/* get points to work with */
			pt1 = gps->points + i;
			pt2 = gps->points + i + 1;
			bGPDspoint npt;
			gp_point_to_parent_space(pt1, diff_mat, &npt);
			gp_point_to_xy(gsc, gps, &npt, &x0, &y0);

			gp_point_to_parent_space(pt2, diff_mat, &npt);
			gp_point_to_xy(gsc, gps, &npt, &x1, &y1);

			/* check that point segment of the boundbox of the selection stroke */
			if (((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(rect, x0, y0)) ||
			    ((!ELEM(V2D_IS_CLIPPED, x1, y1)) && BLI_rcti_isect_pt(rect, x1, y1)))
			{
				int mval[2]  = {mx, my};
				int mvalo[2] = {mx, my}; /* dummy - this isn't used... */

				/* check if point segment of stroke had anything to do with
				 * eraser region  (either within stroke painted, or on its lines)
				 *  - this assumes that linewidth is irrelevant
				 */
				if (gp_stroke_inside_circle(mval, mvalo, radius, x0, y0, x1, y1)) {
					/* change selection of stroke, and then of both points
					 * (as the last point otherwise wouldn't get selected
					 *  as we only do n-1 loops through)
					 */
					if (select) {
						pt1->flag |= GP_SPOINT_SELECT;
						pt2->flag |= GP_SPOINT_SELECT;

						changed = true;
					}
					else {
						pt1->flag &= ~GP_SPOINT_SELECT;
						pt2->flag &= ~GP_SPOINT_SELECT;

						changed = true;
					}
				}
			}
		}

		/* Ensure that stroke selection is in sync with its points */
		BKE_gpencil_stroke_sync_selection(gps);
	}

	return changed;
}


static int gpencil_circle_select_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	/* if not edit/sculpt mode, the event is catched but not processed */
	if (GPENCIL_NONE_EDIT_MODE(gpd)) {
		return OPERATOR_CANCELLED;
	}

	ScrArea *sa = CTX_wm_area(C);

	const int mx = RNA_int_get(op->ptr, "x");
	const int my = RNA_int_get(op->ptr, "y");
	const int radius = RNA_int_get(op->ptr, "radius");

	bool select = !RNA_boolean_get(op->ptr, "deselect");

	GP_SpaceConversion gsc = {NULL};
	rcti rect = {0};            /* for bounding rect around circle (for quicky intersection testing) */

	bool changed = false;


	/* sanity checks */
	if (sa == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active area");
		return OPERATOR_CANCELLED;
	}

	/* init space conversion stuff */
	gp_point_conversion_init(C, &gsc);


	/* rect is rectangle of selection circle */
	rect.xmin = mx - radius;
	rect.ymin = my - radius;
	rect.xmax = mx + radius;
	rect.ymax = my + radius;


	/* find visible strokes, and select if hit */
	GP_EDITABLE_STROKES_BEGIN(C, gpl, gps)
	{
		changed |= gp_stroke_do_circle_sel(
			gps, &gsc, mx, my, radius, select, &rect, diff_mat);
	}
	GP_EDITABLE_STROKES_END;

	/* updates */
	if (changed) {
		DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

		/* copy on write tag is needed, or else no refresh happens */
		DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

		WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	}

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_circle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Circle Select";
	ot->description = "Select Grease Pencil strokes using brush selection";
	ot->idname = "GPENCIL_OT_select_circle";

	/* callbacks */
	ot->invoke = WM_gesture_circle_invoke;
	ot->modal = WM_gesture_circle_modal;
	ot->exec = gpencil_circle_select_exec;
	ot->poll = gpencil_select_poll;
	ot->cancel = WM_gesture_circle_cancel;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_USE_EVAL_DATA;

	/* properties */
	WM_operator_properties_gesture_circle_select(ot);
}

/* ********************************************** */
/* Box Selection */

static int gpencil_border_select_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	ScrArea *sa = CTX_wm_area(C);

	const bool select = !RNA_boolean_get(op->ptr, "deselect");
	const bool extend = RNA_boolean_get(op->ptr, "extend") && ((gpd->flag & GP_DATA_STROKE_PAINTMODE) == 0);

	GP_SpaceConversion gsc = {NULL};
	rcti rect = {0};

	bool changed = false;


	/* sanity checks */
	if (sa == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active area");
		return OPERATOR_CANCELLED;
	}

	/* init space conversion stuff */
	gp_point_conversion_init(C, &gsc);


	/* deselect all strokes first? */
	if (select && !extend) {
		CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
		{
			bGPDspoint *pt;
			int i;

			for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
				pt->flag &= ~GP_SPOINT_SELECT;
			}

			gps->flag &= ~GP_STROKE_SELECT;
		}
		CTX_DATA_END;
	}

	/* get settings from operator */
	WM_operator_properties_border_to_rcti(op, &rect);

	/* select/deselect points */
	GP_EDITABLE_STROKES_BEGIN(C, gpl, gps)
	{

		bGPDspoint *pt;
		int i;

		for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
			int x0, y0;

			/* convert point coords to screenspace */
			bGPDspoint pt2;
			gp_point_to_parent_space(pt, diff_mat, &pt2);
			gp_point_to_xy(&gsc, gps, &pt2, &x0, &y0);

			/* test if in selection rect */
			if ((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(&rect, x0, y0)) {
				if (select) {
					pt->flag |= GP_SPOINT_SELECT;
				}
				else {
					pt->flag &= ~GP_SPOINT_SELECT;
				}

				changed = true;
			}
		}

		/* Ensure that stroke selection is in sync with its points */
		BKE_gpencil_stroke_sync_selection(gps);
	}
	GP_EDITABLE_STROKES_END;

	/* if paint mode,delete selected points */
	if (gpd->flag & GP_DATA_STROKE_PAINTMODE) {
		gp_delete_selected_point_wrap(C);
		changed = true;
		DEG_id_tag_update(&gpd->id, OB_RECALC_OB | OB_RECALC_DATA);
	}

	/* updates */
	if (changed) {
		DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

		/* copy on write tag is needed, or else no refresh happens */
		DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

		WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	}

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Border Select";
	ot->description = "Select Grease Pencil strokes within a rectangular region";
	ot->idname = "GPENCIL_OT_select_border";

	/* callbacks */
	ot->invoke = WM_gesture_border_invoke;
	ot->exec = gpencil_border_select_exec;
	ot->modal = WM_gesture_border_modal;
	ot->cancel = WM_gesture_border_cancel;

	ot->poll = gpencil_select_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_USE_EVAL_DATA;

	/* rna */
	WM_operator_properties_gesture_border_select(ot);
}

/* ********************************************** */
/* Lasso */

static int gpencil_lasso_select_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	GP_SpaceConversion gsc = {NULL};
	rcti rect = {0};

	const bool extend = RNA_boolean_get(op->ptr, "extend") && ((gpd->flag & GP_DATA_STROKE_PAINTMODE) == 0);
	const bool select = !RNA_boolean_get(op->ptr, "deselect");

	int mcords_tot;
	const int (*mcords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcords_tot);

	bool changed = false;

	/* sanity check */
	if (mcords == NULL)
		return OPERATOR_PASS_THROUGH;

	/* compute boundbox of lasso (for faster testing later) */
	BLI_lasso_boundbox(&rect, mcords, mcords_tot);

	/* init space conversion stuff */
	gp_point_conversion_init(C, &gsc);

	/* deselect all strokes first? */
	if (select && !extend) {
		CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
		{
			bGPDspoint *pt;
			int i;

			for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
				pt->flag &= ~GP_SPOINT_SELECT;
			}

			gps->flag &= ~GP_STROKE_SELECT;
		}
		CTX_DATA_END;
	}

	/* select/deselect points */
	GP_EDITABLE_STROKES_BEGIN(C, gpl, gps)
	{
		bGPDspoint *pt;
		int i;

		for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
			int x0, y0;

			/* convert point coords to screenspace */
			bGPDspoint pt2;
			gp_point_to_parent_space(pt, diff_mat, &pt2);
			gp_point_to_xy(&gsc, gps, &pt2, &x0, &y0);
			/* test if in lasso boundbox + within the lasso noose */
			if ((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(&rect, x0, y0) &&
			    BLI_lasso_is_point_inside(mcords, mcords_tot, x0, y0, INT_MAX))
			{
				if (select) {
					pt->flag |= GP_SPOINT_SELECT;
				}
				else {
					pt->flag &= ~GP_SPOINT_SELECT;
				}

				changed = true;
			}
		}

		/* Ensure that stroke selection is in sync with its points */
		BKE_gpencil_stroke_sync_selection(gps);
	}
	GP_EDITABLE_STROKES_END;

	/* cleanup */
	MEM_freeN((void *)mcords);

	/* if paint mode,delete selected points */
	if (gpd->flag & GP_DATA_STROKE_PAINTMODE) {
		gp_delete_selected_point_wrap(C);
		changed = true;
		DEG_id_tag_update(&gpd->id, OB_RECALC_OB | OB_RECALC_DATA);
	}

	/* updates */
	if (changed) {
		DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

		/* copy on write tag is needed, or else no refresh happens */
		DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

		WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	}

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_lasso(wmOperatorType *ot)
{
	ot->name = "Lasso Select Strokes";
	ot->description = "Select Grease Pencil strokes using lasso selection";
	ot->idname = "GPENCIL_OT_select_lasso";

	ot->invoke = WM_gesture_lasso_invoke;
	ot->modal = WM_gesture_lasso_modal;
	ot->exec = gpencil_lasso_select_exec;
	ot->poll = gpencil_select_poll;
	ot->cancel = WM_gesture_lasso_cancel;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_USE_EVAL_DATA;

	/* properties */
	WM_operator_properties_gesture_lasso_select(ot);
}

/* ********************************************** */
/* Mouse Click to Select */

static int gpencil_select_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa = CTX_wm_area(C);
	bGPdata *gpd = ED_gpencil_data_get_active(C);

	/* "radius" is simply a threshold (screen space) to make it easier to test with a tolerance */
	const float radius = 0.75f * U.widget_unit;
	const int radius_squared = (int)(radius * radius);

	bool extend = RNA_boolean_get(op->ptr, "extend");
	bool deselect = RNA_boolean_get(op->ptr, "deselect");
	bool toggle = RNA_boolean_get(op->ptr, "toggle");
	bool whole = RNA_boolean_get(op->ptr, "entire_strokes");

	int mval[2] = {0};

	GP_SpaceConversion gsc = {NULL};

	bGPDstroke *hit_stroke = NULL;
	bGPDspoint *hit_point = NULL;
	int hit_distance = radius_squared;

	/* sanity checks */
	if (sa == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active area");
		return OPERATOR_CANCELLED;
	}

	/* init space conversion stuff */
	gp_point_conversion_init(C, &gsc);

	/* get mouse location */
	RNA_int_get_array(op->ptr, "location", mval);

	/* First Pass: Find stroke point which gets hit */
	/* XXX: maybe we should go from the top of the stack down instead... */
	GP_EDITABLE_STROKES_BEGIN(C, gpl, gps)
	{
		bGPDspoint *pt;
		int i;

		/* firstly, check for hit-point */
		for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
			int xy[2];

			bGPDspoint pt2;
			gp_point_to_parent_space(pt, diff_mat, &pt2);
			gp_point_to_xy(&gsc, gps, &pt2, &xy[0], &xy[1]);

			/* do boundbox check first */
			if (!ELEM(V2D_IS_CLIPPED, xy[0], xy[1])) {
				const int pt_distance = len_manhattan_v2v2_int(mval, xy);

				/* check if point is inside */
				if (pt_distance <= radius_squared) {
					/* only use this point if it is a better match than the current hit - T44685 */
					if (pt_distance < hit_distance) {
						hit_stroke = gps;
						hit_point = pt;
						hit_distance = pt_distance;
					}
				}
			}
		}
	}
	GP_EDITABLE_STROKES_END;

	/* Abort if nothing hit... */
	if (ELEM(NULL, hit_stroke, hit_point)) {
		return OPERATOR_CANCELLED;
	}

	/* adjust selection behaviour - for toggle option */
	if (toggle) {
		deselect = (hit_point->flag & GP_SPOINT_SELECT) != 0;
	}

	/* If not extending selection, deselect everything else */
	if (extend == false) {
		CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
		{
			/* deselect stroke and its points if selected */
			if (gps->flag & GP_STROKE_SELECT) {
				bGPDspoint *pt;
				int i;

				/* deselect points */
				for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
					pt->flag &= ~GP_SPOINT_SELECT;
				}

				/* deselect stroke itself too */
				gps->flag &= ~GP_STROKE_SELECT;
			}
		}
		CTX_DATA_END;
	}

	/* Perform selection operations... */
	if (whole) {
		bGPDspoint *pt;
		int i;

		/* entire stroke's points */
		for (i = 0, pt = hit_stroke->points; i < hit_stroke->totpoints; i++, pt++) {
			if (deselect == false)
				pt->flag |= GP_SPOINT_SELECT;
			else
				pt->flag &= ~GP_SPOINT_SELECT;
		}

		/* stroke too... */
		if (deselect == false)
			hit_stroke->flag |= GP_STROKE_SELECT;
		else
			hit_stroke->flag &= ~GP_STROKE_SELECT;
	}
	else {
		/* just the point (and the stroke) */
		if (deselect == false) {
			/* we're adding selection, so selection must be true */
			hit_point->flag  |= GP_SPOINT_SELECT;
			hit_stroke->flag |= GP_STROKE_SELECT;
		}
		else {
			/* deselect point */
			hit_point->flag &= ~GP_SPOINT_SELECT;

			/* ensure that stroke is selected correctly */
			BKE_gpencil_stroke_sync_selection(hit_stroke);
		}
	}

	/* updates */
	if (hit_point != NULL) {
		DEG_id_tag_update(&gpd->id, OB_RECALC_DATA);

		/* copy on write tag is needed, or else no refresh happens */
		DEG_id_tag_update(&gpd->id, DEG_TAG_COPY_ON_WRITE);

		WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
	}

	return OPERATOR_FINISHED;
}

static int gpencil_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	RNA_int_set_array(op->ptr, "location", event->mval);
	return gpencil_select_exec(C, op);
}

void GPENCIL_OT_select(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select";
	ot->description = "Select Grease Pencil strokes and/or stroke points";
	ot->idname = "GPENCIL_OT_select";

	/* callbacks */
	ot->invoke = gpencil_select_invoke;
	ot->exec = gpencil_select_exec;
	ot->poll = gpencil_select_poll;

	/* flag */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_USE_EVAL_DATA;

	/* properties */
	WM_operator_properties_mouse_select(ot);

	prop = RNA_def_boolean(ot->srna, "entire_strokes", false, "Entire Strokes", "Select entire strokes instead of just the nearest stroke vertex");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_int_vector(ot->srna, "location", 2, NULL, INT_MIN, INT_MAX, "Location", "Mouse location", INT_MIN, INT_MAX);
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* ********************************************** */
