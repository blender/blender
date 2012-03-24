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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_info/info_report.c
 *  \ingroup spinfo
 */


#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "info_intern.h"

int info_report_mask(SpaceInfo *UNUSED(sinfo))
{
#if 0
	int report_mask = 0;

	if (sinfo->rpt_mask & INFO_RPT_DEBUG)	report_mask |= RPT_DEBUG_ALL;
	if (sinfo->rpt_mask & INFO_RPT_INFO)		report_mask |= RPT_INFO_ALL;
	if (sinfo->rpt_mask & INFO_RPT_OP)		report_mask |= RPT_OPERATOR_ALL;
	if (sinfo->rpt_mask & INFO_RPT_WARN)		report_mask |= RPT_WARNING_ALL;
	if (sinfo->rpt_mask & INFO_RPT_ERR)		report_mask |= RPT_ERROR_ALL;

	return report_mask;
#endif

	return RPT_DEBUG_ALL|RPT_INFO_ALL|RPT_OPERATOR_ALL|RPT_WARNING_ALL|RPT_ERROR_ALL;
}

// TODO, get this working again!
static int report_replay_exec(bContext *C, wmOperator *UNUSED(op))
{
//	SpaceInfo *sc= CTX_wm_space_info(C);
//	ReportList *reports= CTX_wm_reports(C);
//	int report_mask= info_report_mask(sc);
//	Report *report;

#if 0
	sc->type= CONSOLE_TYPE_PYTHON;

	for (report=reports->list.last; report; report=report->prev) {
		if ((report->type & report_mask) && (report->type & RPT_OPERATOR_ALL) && (report->flag & SELECT)) {
			console_history_add_str(sc, report->message, 0);
			WM_operator_name_call(C, "CONSOLE_OT_execute", WM_OP_EXEC_DEFAULT, NULL);

			ED_area_tag_redraw(CTX_wm_area(C));
		}
	}

	sc->type= CONSOLE_TYPE_REPORT;
#endif
	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
}

void INFO_OT_report_replay(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Replay Operators";
	ot->description = "Replay selected reports";
	ot->idname = "INFO_OT_report_replay";

	/* api callbacks */
	ot->poll = ED_operator_info_active;
	ot->exec = report_replay_exec;

	/* flags */
	/* ot->flag = OPTYPE_REGISTER; */

	/* properties */
}

static int select_report_pick_exec(bContext *C, wmOperator *op)
{
	int report_index= RNA_int_get(op->ptr, "report_index");
	Report *report= BLI_findlink(&CTX_wm_reports(C)->list, report_index);

	if (!report)
		return OPERATOR_CANCELLED;

	report->flag ^= SELECT; /* toggle */

	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
}

static int select_report_pick_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceInfo *sinfo= CTX_wm_space_info(C);
	ARegion *ar= CTX_wm_region(C);
	ReportList *reports= CTX_wm_reports(C);
	Report *report;

	/* uses opengl */
	wmSubWindowSet(CTX_wm_window(C), ar->swinid);
	
	report= info_text_pick(sinfo, ar, reports, event->mval[1]);

	RNA_int_set(op->ptr, "report_index", BLI_findindex(&reports->list, report));

	return select_report_pick_exec(C, op);
}


void INFO_OT_select_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select report";
	ot->description = "Select reports by index";
	ot->idname = "INFO_OT_select_pick";

	/* api callbacks */
	ot->poll = ED_operator_info_active;
	ot->invoke = select_report_pick_invoke;
	ot->exec = select_report_pick_exec;

	/* flags */
	/* ot->flag = OPTYPE_REGISTER; */

	/* properties */
	RNA_def_int(ot->srna, "report_index", 0, 0, INT_MAX, "Report", "Index of the report", 0, INT_MAX);
}



static int report_select_all_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceInfo *sinfo= CTX_wm_space_info(C);
	ReportList *reports= CTX_wm_reports(C);
	int report_mask= info_report_mask(sinfo);
	int deselect= 0;

	Report *report;

	for (report=reports->list.last; report; report=report->prev) {
		if ((report->type & report_mask) && (report->flag & SELECT)) {
			deselect= 1;
			break;
		}
	}


	if (deselect) {
		for (report=reports->list.last; report; report=report->prev)
			if (report->type & report_mask)
				report->flag &= ~SELECT;
	}
	else {
		for (report=reports->list.last; report; report=report->prev)
			if (report->type & report_mask)
				report->flag |= SELECT;
	}

	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
}

void INFO_OT_select_all_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->description = "Select or deselect all reports";
	ot->idname = "INFO_OT_select_all_toggle";

	/* api callbacks */
	ot->poll = ED_operator_info_active;
	ot->exec = report_select_all_toggle_exec;

	/* flags */
	/*ot->flag = OPTYPE_REGISTER;*/

	/* properties */
}

/* borderselect operator */
static int borderselect_exec(bContext *C, wmOperator *op)
{
	SpaceInfo *sinfo= CTX_wm_space_info(C);
	ARegion *ar= CTX_wm_region(C);
	ReportList *reports= CTX_wm_reports(C);
	int report_mask= info_report_mask(sinfo);
	int extend= RNA_boolean_get(op->ptr, "extend");
	Report *report_min, *report_max, *report;

	//View2D *v2d= UI_view2d_fromcontext(C);


	rcti rect;
	//rctf rectf, rq;
	short selecting= (RNA_int_get(op->ptr, "gesture_mode")==GESTURE_MODAL_SELECT);
	//int mval[2];

	rect.xmin = RNA_int_get(op->ptr, "xmin");
	rect.ymin = RNA_int_get(op->ptr, "ymin");
	rect.xmax = RNA_int_get(op->ptr, "xmax");
	rect.ymax = RNA_int_get(op->ptr, "ymax");

#if 0
	mval[0]= rect.xmin;
	mval[1]= rect.ymin;
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &rectf.xmin, &rectf.ymin);
	mval[0]= rect.xmax;
	mval[1]= rect.ymax;
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &rectf.xmax, &rectf.ymax);
#endif

	if (!extend) {
		for (report= reports->list.first; report; report= report->next) {

			if ((report->type & report_mask)==0)
				continue;

			report->flag &= ~SELECT;
		}
	}

	report_min= info_text_pick(sinfo, ar, reports, rect.ymax);
	report_max= info_text_pick(sinfo, ar, reports, rect.ymin);

	/* get the first report if none found */
	if (report_min==NULL) {
		// printf("find_min\n");
		for (report=reports->list.first; report; report=report->next) {
			if (report->type & report_mask) {
				report_min= report;
				break;
			}
		}
	}

	if (report_max==NULL) {
		// printf("find_max\n");
		for (report=reports->list.last; report; report=report->prev) {
			if (report->type & report_mask) {
				report_max= report;
				break;
			}
		}
	}

	if (report_min==NULL || report_max==NULL)
		return OPERATOR_CANCELLED;

	for (report= report_min; (report != report_max->next); report= report->next) {

		if ((report->type & report_mask)==0)
			continue;

		if (selecting)
			report->flag |= SELECT;
		else
			report->flag &= ~SELECT;
	}

	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
}


/* ****** Border Select ****** */
void INFO_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Border Select";
	ot->description = "Toggle border selection";
	ot->idname = "INFO_OT_select_border";

	/* api callbacks */
	ot->invoke = WM_border_select_invoke;
	ot->exec = borderselect_exec;
	ot->modal = WM_border_select_modal;
	ot->cancel = WM_border_select_cancel;

	ot->poll = ED_operator_info_active;

	/* flags */
	/* ot->flag = OPTYPE_REGISTER; */

	/* rna */
	WM_operator_properties_gesture_border(ot, TRUE);
}



static int report_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceInfo *sinfo= CTX_wm_space_info(C);
	ReportList *reports= CTX_wm_reports(C);
	int report_mask= info_report_mask(sinfo);


	Report *report, *report_next;

	for (report=reports->list.first; report; ) {

		report_next=report->next;

		if ((report->type & report_mask) && (report->flag & SELECT)) {
			BLI_remlink(&reports->list, report);
			MEM_freeN((void *)report->message);
			MEM_freeN(report);
		}

		report= report_next;
	}

	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
}

void INFO_OT_report_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Reports";
	ot->description = "Delete selected reports";
	ot->idname = "INFO_OT_report_delete";

	/* api callbacks */
	ot->poll = ED_operator_info_active;
	ot->exec = report_delete_exec;

	/* flags */
	/*ot->flag = OPTYPE_REGISTER;*/

	/* properties */
}


static int report_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceInfo *sinfo= CTX_wm_space_info(C);
	ReportList *reports= CTX_wm_reports(C);
	int report_mask= info_report_mask(sinfo);

	Report *report;

	DynStr *buf_dyn= BLI_dynstr_new();
	char *buf_str;

	for (report=reports->list.first; report; report= report->next) {
		if ((report->type & report_mask) && (report->flag & SELECT)) {
			BLI_dynstr_append(buf_dyn, report->message);
			BLI_dynstr_append(buf_dyn, "\n");
		}
	}

	buf_str= BLI_dynstr_get_cstring(buf_dyn);
	BLI_dynstr_free(buf_dyn);

	WM_clipboard_text_set(buf_str, 0);

	MEM_freeN(buf_str);
	return OPERATOR_FINISHED;
}

void INFO_OT_report_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Reports to Clipboard";
	ot->description = "Copy selected reports to Clipboard";
	ot->idname = "INFO_OT_report_copy";

	/* api callbacks */
	ot->poll = ED_operator_info_active;
	ot->exec = report_copy_exec;

	/* flags */
	/*ot->flag = OPTYPE_REGISTER;*/

	/* properties */
}
