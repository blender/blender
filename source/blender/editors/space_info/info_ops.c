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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_info/info_ops.c
 *  \ingroup spinfo
 */


#include <string.h>
#include <stdio.h>

#include "DNA_packedFile_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_bpath.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_report.h"
#include "BKE_screen.h"


#include "WM_api.h"
#include "WM_types.h"


#include "UI_interface.h"
#include "UI_resources.h"

#include "IMB_imbuf_types.h"

#include "RNA_access.h"
#include "RNA_define.h"


#include "info_intern.h"

/********************* pack blend file libraries operator *********************/

static int pack_libraries_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);

	packLibraries(bmain, op->reports);

	return OPERATOR_FINISHED;
}

void FILE_OT_pack_libraries(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Pack Blender Libraries";
	ot->idname = "FILE_OT_pack_libraries";
	ot->description = "Pack all used Blender library files into the current .blend";

	/* api callbacks */
	ot->exec = pack_libraries_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int unpack_libraries_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);

	unpackLibraries(bmain, op->reports);

	return OPERATOR_FINISHED;
}

static int unpack_libraries_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	return WM_operator_confirm_message(C, op, "Unpack Blender Libraries - creates directories, all new paths should work");
}

void FILE_OT_unpack_libraries(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unpack Blender Libraries";
	ot->idname = "FILE_OT_unpack_libraries";
	ot->description = "Unpack all used Blender library files from this .blend file";

	/* api callbacks */
	ot->invoke = unpack_libraries_invoke;
	ot->exec = unpack_libraries_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************* toggle auto-pack operator *********************/

static int autopack_toggle_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);

	if (G.fileflags & G_AUTOPACK) {
		G.fileflags &= ~G_AUTOPACK;
	}
	else {
		packAll(bmain, op->reports, true);
		G.fileflags |= G_AUTOPACK;
	}

	return OPERATOR_FINISHED;
}

void FILE_OT_autopack_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Automatically Pack Into .blend";
	ot->idname = "FILE_OT_autopack_toggle";
	ot->description = "Automatically pack all external files into the .blend file";

	/* api callbacks */
	ot->exec = autopack_toggle_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************* pack all operator *********************/

static int pack_all_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);

	packAll(bmain, op->reports, true);

	return OPERATOR_FINISHED;
}

static int pack_all_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Main *bmain = CTX_data_main(C);
	Image *ima;
	ImBuf *ibuf;

	// first check for dirty images
	for (ima = bmain->image.first; ima; ima = ima->id.next) {
		if (BKE_image_has_loaded_ibuf(ima)) { /* XXX FIX */
			ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);

			if (ibuf && (ibuf->userflags & IB_BITMAPDIRTY)) {
				BKE_image_release_ibuf(ima, ibuf, NULL);
				break;
			}

			BKE_image_release_ibuf(ima, ibuf, NULL);
		}
	}

	if (ima) {
		return WM_operator_confirm_message(C, op, "Some images are painted on. These changes will be lost. Continue?");
	}

	return pack_all_exec(C, op);
}

void FILE_OT_pack_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Pack All Into .blend";
	ot->idname = "FILE_OT_pack_all";
	ot->description = "Pack all used external files into the .blend";

	/* api callbacks */
	ot->exec = pack_all_exec;
	ot->invoke = pack_all_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/********************* unpack all operator *********************/

static const EnumPropertyItem unpack_all_method_items[] = {
	{PF_USE_LOCAL, "USE_LOCAL", 0, "Use files in current directory (create when necessary)", ""},
	{PF_WRITE_LOCAL, "WRITE_LOCAL", 0, "Write files to current directory (overwrite existing files)", ""},
	{PF_USE_ORIGINAL, "USE_ORIGINAL", 0, "Use files in original location (create when necessary)", ""},
	{PF_WRITE_ORIGINAL, "WRITE_ORIGINAL", 0, "Write files to original location (overwrite existing files)", ""},
	{PF_KEEP, "KEEP", 0, "Disable Auto-pack, keep all packed files", ""},
	/* {PF_ASK, "ASK", 0, "Ask for each file", ""}, */
	{0, NULL, 0, NULL, NULL}};

static int unpack_all_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	int method = RNA_enum_get(op->ptr, "method");

	if (method != PF_KEEP) unpackAll(bmain, op->reports, method);  /* XXX PF_ASK can't work here */
	G.fileflags &= ~G_AUTOPACK;

	return OPERATOR_FINISHED;
}

static int unpack_all_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Main *bmain = CTX_data_main(C);
	uiPopupMenu *pup;
	uiLayout *layout;
	char title[64];
	int count = 0;

	count = countPackedFiles(bmain);

	if (!count) {
		BKE_report(op->reports, RPT_WARNING, "No packed files to unpack");
		G.fileflags &= ~G_AUTOPACK;
		return OPERATOR_CANCELLED;
	}

	if (count == 1)
		BLI_strncpy(title, IFACE_("Unpack 1 File"), sizeof(title));
	else
		BLI_snprintf(title, sizeof(title), IFACE_("Unpack %d Files"), count);

	pup = UI_popup_menu_begin(C, title, ICON_NONE);
	layout = UI_popup_menu_layout(pup);

	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemsEnumO(layout, "FILE_OT_unpack_all", "method");

	UI_popup_menu_end(C, pup);

	return OPERATOR_INTERFACE;
}

void FILE_OT_unpack_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unpack All Into Files";
	ot->idname = "FILE_OT_unpack_all";
	ot->description = "Unpack all files packed into this .blend to external ones";

	/* api callbacks */
	ot->exec = unpack_all_exec;
	ot->invoke = unpack_all_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "method", unpack_all_method_items, PF_USE_LOCAL, "Method", "How to unpack");
}

/********************* unpack single item operator *********************/

static const EnumPropertyItem unpack_item_method_items[] = {
	{PF_USE_LOCAL, "USE_LOCAL", 0, "Use file from current directory (create when necessary)", ""},
	{PF_WRITE_LOCAL, "WRITE_LOCAL", 0, "Write file to current directory (overwrite existing file)", ""},
	{PF_USE_ORIGINAL, "USE_ORIGINAL", 0, "Use file in original location (create when necessary)", ""},
	{PF_WRITE_ORIGINAL, "WRITE_ORIGINAL", 0, "Write file to original location (overwrite existing file)", ""},
	/* {PF_ASK, "ASK", 0, "Ask for each file", ""}, */
	{0, NULL, 0, NULL, NULL}};


static int unpack_item_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	ID *id;
	char idname[MAX_ID_NAME - 2];
	int type = RNA_int_get(op->ptr, "id_type");
	int method = RNA_enum_get(op->ptr, "method");

	RNA_string_get(op->ptr, "id_name", idname);
	id = BKE_libblock_find_name(bmain, type, idname);

	if (id == NULL) {
		BKE_report(op->reports, RPT_WARNING, "No packed file");
		return OPERATOR_CANCELLED;
	}

	if (method != PF_KEEP)
		BKE_unpack_id(bmain, id, op->reports, method);  /* XXX PF_ASK can't work here */

	G.fileflags &= ~G_AUTOPACK;

	return OPERATOR_FINISHED;
}

static int unpack_item_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup = UI_popup_menu_begin(C, IFACE_("Unpack"), ICON_NONE);
	layout = UI_popup_menu_layout(pup);

	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemsFullEnumO(layout, op->type->idname, "method", op->ptr->data, WM_OP_EXEC_REGION_WIN, 0);

	UI_popup_menu_end(C, pup);

	return OPERATOR_INTERFACE;
}

void FILE_OT_unpack_item(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unpack Item";
	ot->idname = "FILE_OT_unpack_item";
	ot->description = "Unpack this file to an external file";

	/* api callbacks */
	ot->exec = unpack_item_exec;
	ot->invoke = unpack_item_invoke;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "method", unpack_item_method_items, PF_USE_LOCAL, "Method", "How to unpack");
	RNA_def_string(ot->srna, "id_name", NULL, BKE_ST_MAXNAME, "ID name", "Name of ID block to unpack");
	RNA_def_int(ot->srna, "id_type", ID_IM, 0, INT_MAX, "ID Type", "Identifier type of ID block", 0, INT_MAX);
}


/********************* make paths relative operator *********************/

static int make_paths_relative_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);

	if (!G.relbase_valid) {
		BKE_report(op->reports, RPT_WARNING, "Cannot set relative paths with an unsaved blend file");
		return OPERATOR_CANCELLED;
	}

	BKE_bpath_relative_convert(bmain, BKE_main_blendfile_path(bmain), op->reports);

	/* redraw everything so any changed paths register */
	WM_main_add_notifier(NC_WINDOW, NULL);

	return OPERATOR_FINISHED;
}

void FILE_OT_make_paths_relative(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make All Paths Relative";
	ot->idname = "FILE_OT_make_paths_relative";
	ot->description = "Make all paths to external files relative to current .blend";

	/* api callbacks */
	ot->exec = make_paths_relative_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************* make paths absolute operator *********************/

static int make_paths_absolute_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);

	if (!G.relbase_valid) {
		BKE_report(op->reports, RPT_WARNING, "Cannot set absolute paths with an unsaved blend file");
		return OPERATOR_CANCELLED;
	}

	BKE_bpath_absolute_convert(bmain, BKE_main_blendfile_path(bmain), op->reports);

	/* redraw everything so any changed paths register */
	WM_main_add_notifier(NC_WINDOW, NULL);

	return OPERATOR_FINISHED;
}

void FILE_OT_make_paths_absolute(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make All Paths Absolute";
	ot->idname = "FILE_OT_make_paths_absolute";
	ot->description = "Make all paths to external files absolute";

	/* api callbacks */
	ot->exec = make_paths_absolute_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************* report missing files operator *********************/

static int report_missing_files_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);

	/* run the missing file check */
	BKE_bpath_missing_files_check(bmain, op->reports);

	return OPERATOR_FINISHED;
}

void FILE_OT_report_missing_files(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Report Missing Files";
	ot->idname = "FILE_OT_report_missing_files";
	ot->description = "Report all missing external files";

	/* api callbacks */
	ot->exec = report_missing_files_exec;

	/* flags */
	ot->flag = 0; /* only reports so no need to undo/register */
}

/********************* find missing files operator *********************/

static int find_missing_files_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	const char *searchpath = RNA_string_get_alloc(op->ptr, "directory", NULL, 0);
	const bool find_all = RNA_boolean_get(op->ptr, "find_all");

	BKE_bpath_missing_files_find(bmain, searchpath, op->reports, find_all);
	MEM_freeN((void *)searchpath);

	return OPERATOR_FINISHED;
}

static int find_missing_files_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	/* XXX file open button text "Find Missing Files" */
	WM_event_add_fileselect(C, op);
	return OPERATOR_RUNNING_MODAL;
}

void FILE_OT_find_missing_files(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Find Missing Files";
	ot->idname = "FILE_OT_find_missing_files";
	ot->description = "Try to find missing external files";

	/* api callbacks */
	ot->exec = find_missing_files_exec;
	ot->invoke = find_missing_files_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "find_all", false, "Find All", "Find all files in the search path (not just missing)");

	WM_operator_properties_filesel(
	        ot, 0, FILE_SPECIAL, FILE_OPENFILE,
	        WM_FILESEL_DIRECTORY, FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);
}

/********************* report box operator *********************/

/* Hard to decide whether to keep this as an operator,
 * or turn it into a hardcoded ui control feature,
 * handling TIMER events for all regions in interface_handlers.c
 * Not sure how good that is to be accessing UI data from
 * inactive regions, so use this for now. --matt
 */

#define INFO_TIMEOUT        5.0f
#define INFO_COLOR_TIMEOUT  3.0f
#define ERROR_TIMEOUT       10.0f
#define ERROR_COLOR_TIMEOUT 6.0f
#define COLLAPSE_TIMEOUT    0.25f
static int update_reports_display_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	ReportList *reports = CTX_wm_reports(C);
	Report *report;
	ReportTimerInfo *rti;
	float progress = 0.0, color_progress = 0.0;
	float neutral_col[3] = {0.35, 0.35, 0.35};
	float neutral_gray = 0.6;
	float timeout = 0.0, color_timeout = 0.0;
	int send_note = 0;

	/* escape if not our timer */
	if ((reports->reporttimer == NULL) ||
	    (reports->reporttimer != event->customdata) ||
	    ((report = BKE_reports_last_displayable(reports)) == NULL) /* may have been deleted */
	    )
	{
		return OPERATOR_PASS_THROUGH;
	}

	rti = (ReportTimerInfo *)reports->reporttimer->customdata;

	timeout = (report->type & RPT_ERROR_ALL) ? ERROR_TIMEOUT : INFO_TIMEOUT;
	color_timeout = (report->type & RPT_ERROR_ALL) ? ERROR_COLOR_TIMEOUT : INFO_COLOR_TIMEOUT;

	/* clear the report display after timeout */
	if ((float)reports->reporttimer->duration > timeout) {
		WM_event_remove_timer(wm, NULL, reports->reporttimer);
		reports->reporttimer = NULL;

		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO, NULL);

		return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
	}

	if (rti->widthfac == 0.0f) {
		/* initialize colors based on report type */
		if (report->type & RPT_ERROR_ALL) {
			rti->col[0] = 1.0;
			rti->col[1] = 0.2;
			rti->col[2] = 0.0;
		}
		else if (report->type & RPT_WARNING_ALL) {
			rti->col[0] = 1.0;
			rti->col[1] = 1.0;
			rti->col[2] = 0.0;
		}
		else if (report->type & RPT_INFO_ALL) {
			rti->col[0] = 0.3;
			rti->col[1] = 0.45;
			rti->col[2] = 0.7;
		}
		rti->grayscale = 0.75;
		rti->widthfac = 1.0;
	}

	progress = (float)reports->reporttimer->duration / timeout;
	color_progress = (float)reports->reporttimer->duration / color_timeout;

	/* save us from too many draws */
	if (color_progress <= 1.0f) {
		send_note = 1;

		/* fade colors out sharply according to progress through fade-out duration */
		interp_v3_v3v3(rti->col, rti->col, neutral_col, color_progress);
		rti->grayscale = interpf(neutral_gray, rti->grayscale, color_progress);
	}

	/* collapse report at end of timeout */
	if (progress * timeout > timeout - COLLAPSE_TIMEOUT) {
		rti->widthfac = (progress * timeout - (timeout - COLLAPSE_TIMEOUT)) / COLLAPSE_TIMEOUT;
		rti->widthfac = 1.0f - rti->widthfac;
		send_note = 1;
	}

	if (send_note) {
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO, NULL);
	}

	return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
}

void INFO_OT_reports_display_update(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Update Reports Display";
	ot->idname = "INFO_OT_reports_display_update";
	ot->description = "Update the display of reports in Blender UI (internal use)";

	/* api callbacks */
	ot->invoke = update_reports_display_invoke;

	/* flags */
	ot->flag = 0;

	/* properties */
}

/* report operators */
