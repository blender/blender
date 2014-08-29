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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_text/text_ops.c
 *  \ingroup sptext
 */


#include <string.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "DNA_text_types.h"

#include "BLI_blenlib.h"

#include "BLF_translation.h"

#include "PIL_time.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_text.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_text.h"
#include "ED_curve.h"
#include "ED_screen.h"
#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#include "text_intern.h"
#include "text_format.h"

static void txt_screen_clamp(SpaceText *st, ARegion *ar);

/************************ poll ***************************/


BLI_INLINE int text_pixel_x_to_column(SpaceText *st, const int x)
{
	/* add half the char width so mouse cursor selection is inbetween letters */
	return (x + (st->cwidth / 2)) / st->cwidth;
}

static int text_new_poll(bContext *UNUSED(C))
{
	return 1;
}

static int text_edit_poll(bContext *C)
{
	Text *text = CTX_data_edit_text(C);

	if (!text)
		return 0;

	if (text->id.lib) {
		// BKE_report(op->reports, RPT_ERROR, "Cannot edit external libdata");
		return 0;
	}

	return 1;
}

int text_space_edit_poll(bContext *C)
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);

	if (!st || !text)
		return 0;

	if (text->id.lib) {
		// BKE_report(op->reports, RPT_ERROR, "Cannot edit external libdata");
		return 0;
	}

	return 1;
}

static int text_region_edit_poll(bContext *C)
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);
	ARegion *ar = CTX_wm_region(C);

	if (!st || !text)
		return 0;
	
	if (!ar || ar->regiontype != RGN_TYPE_WINDOW)
		return 0;

	if (text->id.lib) {
		// BKE_report(op->reports, RPT_ERROR, "Cannot edit external libdata");
		return 0;
	}

	return 1;
}

/********************** updates *********************/

void text_update_line_edited(TextLine *line)
{
	if (!line)
		return;

	/* we just free format here, and let it rebuild during draw */
	if (line->format) {
		MEM_freeN(line->format);
		line->format = NULL;
	}
}

void text_update_edited(Text *text)
{
	TextLine *line;

	for (line = text->lines.first; line; line = line->next)
		text_update_line_edited(line);
}

/******************* new operator *********************/

static int text_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceText *st = CTX_wm_space_text(C);
	Main *bmain = CTX_data_main(C);
	Text *text;
	PointerRNA ptr, idptr;
	PropertyRNA *prop;

	text = BKE_text_add(bmain, "Text");

	/* hook into UI */
	uiIDContextProperty(C, &ptr, &prop);

	if (prop) {
		RNA_id_pointer_create(&text->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}
	else if (st) {
		st->text = text;
		st->left = 0;
		st->top = 0;
		st->scroll_accum[0] = 0.0f;
		st->scroll_accum[1] = 0.0f;
		text_drawcache_tag_update(st, 1);
	}

	WM_event_add_notifier(C, NC_TEXT | NA_ADDED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Create Text Block";
	ot->idname = "TEXT_OT_new";
	ot->description = "Create a new text data block";
	
	/* api callbacks */
	ot->exec = text_new_exec;
	ot->poll = text_new_poll;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
}

/******************* open operator *********************/

static void text_open_init(bContext *C, wmOperator *op)
{
	PropertyPointerRNA *pprop;

	op->customdata = pprop = MEM_callocN(sizeof(PropertyPointerRNA), "OpenPropertyPointerRNA");
	uiIDContextProperty(C, &pprop->ptr, &pprop->prop);
}

static void text_open_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
}

static int text_open_exec(bContext *C, wmOperator *op)
{
	SpaceText *st = CTX_wm_space_text(C);
	Main *bmain = CTX_data_main(C);
	Text *text;
	PropertyPointerRNA *pprop;
	PointerRNA idptr;
	char str[FILE_MAX];
	const bool internal = RNA_boolean_get(op->ptr, "internal");

	RNA_string_get(op->ptr, "filepath", str);

	text = BKE_text_load_ex(bmain, str, G.main->name, internal);

	if (!text) {
		if (op->customdata) MEM_freeN(op->customdata);
		return OPERATOR_CANCELLED;
	}

	if (!op->customdata)
		text_open_init(C, op);

	/* hook into UI */
	pprop = op->customdata;

	if (pprop->prop) {
		RNA_id_pointer_create(&text->id, &idptr);
		RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr);
		RNA_property_update(C, &pprop->ptr, pprop->prop);
	}
	else if (st) {
		st->text = text;
		st->left = 0;
		st->top = 0;
		st->scroll_accum[0] = 0.0f;
		st->scroll_accum[1] = 0.0f;
	}

	text_drawcache_tag_update(st, 1);
	WM_event_add_notifier(C, NC_TEXT | NA_ADDED, text);

	MEM_freeN(op->customdata);

	return OPERATOR_FINISHED;
}

static int text_open_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Text *text = CTX_data_edit_text(C);
	const char *path = (text && text->name) ? text->name : G.main->name;

	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return text_open_exec(C, op);
	
	text_open_init(C, op);
	RNA_string_set(op->ptr, "filepath", path);
	WM_event_add_fileselect(C, op); 

	return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_open(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Open Text Block";
	ot->idname = "TEXT_OT_open";
	ot->description = "Open a new text data block";

	/* api callbacks */
	ot->exec = text_open_exec;
	ot->invoke = text_open_invoke;
	ot->cancel = text_open_cancel;
	ot->poll = text_new_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE | TEXTFILE | PYSCRIPTFILE, FILE_SPECIAL, FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);  //XXX TODO, relative_path
	RNA_def_boolean(ot->srna, "internal", 0, "Make internal", "Make text file internal after loading");
}

/******************* reload operator *********************/

static int text_reload_exec(bContext *C, wmOperator *op)
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);
	ARegion *ar = CTX_wm_region(C);

	/* store view & cursor state */
	const int orig_top = st->top;
	const int orig_curl = BLI_findindex(&text->lines, text->curl);
	const int orig_curc = text->curc;

	if (!BKE_text_reload(text)) {
		BKE_report(op->reports, RPT_ERROR, "Could not reopen file");
		return OPERATOR_CANCELLED;
	}

#ifdef WITH_PYTHON
	if (text->compiled)
		BPY_text_free_code(text);
#endif

	text_update_edited(text);
	text_update_cursor_moved(C);
	text_drawcache_tag_update(CTX_wm_space_text(C), 1);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	/* return to scroll position */
	st->top = orig_top;
	txt_screen_clamp(st, ar);
	/* return cursor */
	txt_move_to(text, orig_curl, orig_curc, false);

	return OPERATOR_FINISHED;
}

void TEXT_OT_reload(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reload";
	ot->idname = "TEXT_OT_reload";
	ot->description = "Reload active text data block from its file";
	
	/* api callbacks */
	ot->exec = text_reload_exec;
	ot->invoke = WM_operator_confirm;
	ot->poll = text_edit_poll;
}

/******************* delete operator *********************/

static int text_unlink_poll(bContext *C)
{
	/* it should be possible to unlink texts if they're lib-linked in... */
	return CTX_data_edit_text(C) != NULL;
}

static int text_unlink_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);

	/* make the previous text active, if its not there make the next text active */
	if (st) {
		if (text->id.prev) {
			st->text = text->id.prev;
			text_update_cursor_moved(C);
			WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);
		}
		else if (text->id.next) {
			st->text = text->id.next;
			text_update_cursor_moved(C);
			WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);
		}
	}

	BKE_text_unlink(bmain, text);
	BKE_libblock_free(bmain, text);

	text_drawcache_tag_update(st, 1);
	WM_event_add_notifier(C, NC_TEXT | NA_REMOVED, NULL);

	return OPERATOR_FINISHED;
}

void TEXT_OT_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unlink";
	ot->idname = "TEXT_OT_unlink";
	ot->description = "Unlink active text data block";
	
	/* api callbacks */
	ot->exec = text_unlink_exec;
	ot->invoke = WM_operator_confirm;
	ot->poll = text_unlink_poll;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
}

/******************* make internal operator *********************/

static int text_make_internal_exec(bContext *C, wmOperator *UNUSED(op))
{
	Text *text = CTX_data_edit_text(C);

	text->flags |= TXT_ISMEM | TXT_ISDIRTY;

	if (text->name) {
		MEM_freeN(text->name);
		text->name = NULL;
	}

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_make_internal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Internal";
	ot->idname = "TEXT_OT_make_internal";
	ot->description = "Make active text file internal";

	/* api callbacks */
	ot->exec = text_make_internal_exec;
	ot->poll = text_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
}

/******************* save operator *********************/

static int text_save_poll(bContext *C)
{
	Text *text = CTX_data_edit_text(C);

	if (!text_edit_poll(C))
		return 0;
	
	return (text->name != NULL && !(text->flags & TXT_ISMEM));
}

static void txt_write_file(Text *text, ReportList *reports) 
{
	FILE *fp;
	TextLine *tmp;
	BLI_stat_t st;
	char filepath[FILE_MAX];
	
	BLI_strncpy(filepath, text->name, FILE_MAX);
	BLI_path_abs(filepath, G.main->name);
	
	fp = BLI_fopen(filepath, "w");
	if (fp == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Unable to save '%s': %s",
		            filepath, errno ? strerror(errno) : TIP_("unknown error writing file"));
		return;
	}

	for (tmp = text->lines.first; tmp; tmp = tmp->next) {
		fputs(tmp->line, fp);
		fputc('\n', fp);
	}
	
	fclose(fp);

	if (BLI_stat(filepath, &st) == 0) {
		text->mtime = st.st_mtime;

		/* report since this can be called from key-shortcuts */
		BKE_reportf(reports, RPT_INFO, "Saved Text '%s'", filepath);
	}
	else {
		text->mtime = 0;
		BKE_reportf(reports, RPT_WARNING, "Unable to stat '%s': %s",
		            filepath, errno ? strerror(errno) : TIP_("unknown error stating file"));
	}
	
	text->flags &= ~TXT_ISDIRTY;
}

static int text_save_exec(bContext *C, wmOperator *op)
{
	Text *text = CTX_data_edit_text(C);

	txt_write_file(text, op->reports);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_save(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Save";
	ot->idname = "TEXT_OT_save";
	ot->description = "Save active text data block";

	/* api callbacks */
	ot->exec = text_save_exec;
	ot->poll = text_save_poll;
}

/******************* save as operator *********************/

static int text_save_as_exec(bContext *C, wmOperator *op)
{
	Text *text = CTX_data_edit_text(C);
	char str[FILE_MAX];

	if (!text)
		return OPERATOR_CANCELLED;

	RNA_string_get(op->ptr, "filepath", str);

	if (text->name) MEM_freeN(text->name);
	text->name = BLI_strdup(str);
	text->flags &= ~TXT_ISMEM;

	txt_write_file(text, op->reports);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	return OPERATOR_FINISHED;
}

static int text_save_as_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Text *text = CTX_data_edit_text(C);
	const char *str;

	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return text_save_as_exec(C, op);

	if (text->name)
		str = text->name;
	else if (text->flags & TXT_ISMEM)
		str = text->id.name + 2;
	else
		str = G.main->name;
	
	RNA_string_set(op->ptr, "filepath", str);
	WM_event_add_fileselect(C, op); 

	return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_save_as(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Save As";
	ot->idname = "TEXT_OT_save_as";
	ot->description = "Save active text file with options";
	
	/* api callbacks */
	ot->exec = text_save_as_exec;
	ot->invoke = text_save_as_invoke;
	ot->poll = text_edit_poll;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE | TEXTFILE | PYSCRIPTFILE, FILE_SPECIAL, FILE_SAVE,
	                               WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);  //XXX TODO, relative_path
}

/******************* run script operator *********************/

static int text_run_script_poll(bContext *C)
{
	return (CTX_data_edit_text(C) != NULL);
}

static int text_run_script(bContext *C, ReportList *reports)
{
#ifdef WITH_PYTHON
	Text *text = CTX_data_edit_text(C);
	const bool is_live = (reports == NULL);

	/* only for comparison */
	void *curl_prev = text->curl;
	int curc_prev = text->curc;

	if (BPY_text_exec(C, text, reports, !is_live)) {
		if (is_live) {
			/* for nice live updates */
			WM_event_add_notifier(C, NC_WINDOW | NA_EDITED, NULL);
		}
		return OPERATOR_FINISHED;
	}

	/* Don't report error messages while live editing */
	if (!is_live) {
		/* text may have freed its self */
		if (CTX_data_edit_text(C) == text) {
			if (text->curl != curl_prev || curc_prev != text->curc) {
				text_update_cursor_moved(C);
				WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);
			}
		}

		BKE_report(reports, RPT_ERROR, "Python script fail, look in the console for now...");

		return OPERATOR_FINISHED;
	}
#else
	(void)C;
	(void)reports;
#endif /* !WITH_PYTHON */
	return OPERATOR_CANCELLED;
}

static int text_run_script_exec(bContext *C, wmOperator *op)
{
#ifndef WITH_PYTHON
	(void)C; /* unused */

	BKE_report(op->reports, RPT_ERROR, "Python disabled in this build");

	return OPERATOR_CANCELLED;
#else
	return text_run_script(C, op->reports);
#endif
}

void TEXT_OT_run_script(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Run Script";
	ot->idname = "TEXT_OT_run_script";
	ot->description = "Run active script";
	
	/* api callbacks */
	ot->poll = text_run_script_poll;
	ot->exec = text_run_script_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************* refresh pyconstraints operator *********************/

static int text_refresh_pyconstraints_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
#ifdef WITH_PYTHON
#if 0
	Text *text = CTX_data_edit_text(C);
	Object *ob;
	bConstraint *con;
	short update;
	
	/* check all pyconstraints */
	for (ob = CTX_data_main(C)->object.first; ob; ob = ob->id.next) {
		update = 0;
		if (ob->type == OB_ARMATURE && ob->pose) {
			bPoseChannel *pchan;
			for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
				for (con = pchan->constraints.first; con; con = con->next) {
					if (con->type == CONSTRAINT_TYPE_PYTHON) {
						bPythonConstraint *data = con->data;
						if (data->text == text) BPY_pyconstraint_update(ob, con);
						update = 1;
						
					}
				}
			}
		}
		for (con = ob->constraints.first; con; con = con->next) {
			if (con->type == CONSTRAINT_TYPE_PYTHON) {
				bPythonConstraint *data = con->data;
				if (data->text == text) BPY_pyconstraint_update(ob, con);
				update = 1;
			}
		}
		
		if (update) {
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		}
	}
#endif
#endif

	return OPERATOR_FINISHED;
}

void TEXT_OT_refresh_pyconstraints(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Refresh PyConstraints";
	ot->idname = "TEXT_OT_refresh_pyconstraints";
	ot->description = "Refresh all pyconstraints";
	
	/* api callbacks */
	ot->exec = text_refresh_pyconstraints_exec;
	ot->poll = text_edit_poll;
}

/******************* paste operator *********************/

static char *txt_copy_selected(Text *text)
{
	TextLine *tmp, *linef, *linel;
	char *buf = NULL;
	int charf, charl, length = 0;
	
	if (!text) return NULL;
	if (!text->curl) return NULL;
	if (!text->sell) return NULL;

	if (!txt_has_sel(text)) return NULL;

	if (text->curl == text->sell) {
		linef = linel = text->curl;
		
		if (text->curc < text->selc) {
			charf = text->curc;
			charl = text->selc;
		}
		else {
			charf = text->selc;
			charl = text->curc;
		}
	}
	else if (txt_get_span(text->curl, text->sell) < 0) {
		linef = text->sell;
		linel = text->curl;

		charf = text->selc;
		charl = text->curc;
	}
	else {
		linef = text->curl;
		linel = text->sell;
		
		charf = text->curc;
		charl = text->selc;
	}

	if (linef == linel) {
		length = charl - charf;

		buf = MEM_callocN(length + 1, "cut buffera");
		
		BLI_strncpy(buf, linef->line + charf, length + 1);
	}
	else {
		length += linef->len - charf;
		length += charl;
		length++; /* For the '\n' */
		
		tmp = linef->next;
		while (tmp && tmp != linel) {
			length += tmp->len + 1;
			tmp = tmp->next;
		}
		
		buf = MEM_callocN(length + 1, "cut bufferb");
		
		strncpy(buf, linef->line + charf, linef->len - charf);
		length = linef->len - charf;
		
		buf[length++] = '\n';
		
		tmp = linef->next;
		while (tmp && tmp != linel) {
			strncpy(buf + length, tmp->line, tmp->len);
			length += tmp->len;
			
			buf[length++] = '\n';
			
			tmp = tmp->next;
		}
		strncpy(buf + length, linel->line, charl);
		length += charl;
		
		buf[length] = 0;
	}

	return buf;
}

static int text_paste_exec(bContext *C, wmOperator *op)
{
	const bool selection = RNA_boolean_get(op->ptr, "selection");
	Text *text = CTX_data_edit_text(C);
	char *buf;
	int buf_len;

	buf = WM_clipboard_text_get(selection, &buf_len);

	if (!buf)
		return OPERATOR_CANCELLED;

	text_drawcache_tag_update(CTX_wm_space_text(C), 0);

	txt_insert_buf(text, buf);
	text_update_edited(text);

	MEM_freeN(buf);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	/* run the script while editing, evil but useful */
	if (CTX_wm_space_text(C)->live_edit)
		text_run_script(C, NULL);

	return OPERATOR_FINISHED;
}

void TEXT_OT_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste";
	ot->idname = "TEXT_OT_paste";
	ot->description = "Paste text from clipboard";
	
	/* api callbacks */
	ot->exec = text_paste_exec;
	ot->poll = text_edit_poll;
	
	/* properties */
	RNA_def_boolean(ot->srna, "selection", 0, "Selection", "Paste text selected elsewhere rather than copied (X11 only)");
}

/**************** duplicate operator *******************/

static int text_duplicate_line_exec(bContext *C, wmOperator *UNUSED(op))
{
	Text *text = CTX_data_edit_text(C);
	
	txt_duplicate_line(text);
	
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	/* run the script while editing, evil but useful */
	if (CTX_wm_space_text(C)->live_edit) {
		text_run_script(C, NULL);
	}

	return OPERATOR_FINISHED;
}

void TEXT_OT_duplicate_line(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Line";
	ot->idname = "TEXT_OT_duplicate_line";
	ot->description = "Duplicate the current line";
	
	/* api callbacks */
	ot->exec = text_duplicate_line_exec;
	ot->poll = text_edit_poll;
}

/******************* copy operator *********************/

static void txt_copy_clipboard(Text *text)
{
	char *buf;

	buf = txt_copy_selected(text);

	if (buf) {
		WM_clipboard_text_set(buf, 0);
		MEM_freeN(buf);
	}
}

static int text_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
	Text *text = CTX_data_edit_text(C);

	txt_copy_clipboard(text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy";
	ot->idname = "TEXT_OT_copy";
	ot->description = "Copy selected text to clipboard";

	/* api callbacks */
	ot->exec = text_copy_exec;
	ot->poll = text_edit_poll;
}

/******************* cut operator *********************/

static int text_cut_exec(bContext *C, wmOperator *UNUSED(op))
{
	Text *text = CTX_data_edit_text(C);

	text_drawcache_tag_update(CTX_wm_space_text(C), 0);

	txt_copy_clipboard(text);
	txt_delete_selected(text);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	/* run the script while editing, evil but useful */
	if (CTX_wm_space_text(C)->live_edit)
		text_run_script(C, NULL);

	return OPERATOR_FINISHED;
}

void TEXT_OT_cut(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Cut";
	ot->idname = "TEXT_OT_cut";
	ot->description = "Cut selected text to clipboard";
	
	/* api callbacks */
	ot->exec = text_cut_exec;
	ot->poll = text_edit_poll;
}

/******************* indent operator *********************/

static int text_indent_exec(bContext *C, wmOperator *UNUSED(op))
{
	Text *text = CTX_data_edit_text(C);

	text_drawcache_tag_update(CTX_wm_space_text(C), 0);

	if (txt_has_sel(text)) {
		txt_order_cursors(text, false);
		txt_indent(text);
	}
	else
		txt_add_char(text, '\t');

	text_update_edited(text);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_indent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Indent";
	ot->idname = "TEXT_OT_indent";
	ot->description = "Indent selected text";
	
	/* api callbacks */
	ot->exec = text_indent_exec;
	ot->poll = text_edit_poll;
}

/******************* unindent operator *********************/

static int text_unindent_exec(bContext *C, wmOperator *UNUSED(op))
{
	Text *text = CTX_data_edit_text(C);

	text_drawcache_tag_update(CTX_wm_space_text(C), 0);

	txt_order_cursors(text, false);
	txt_unindent(text);

	text_update_edited(text);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_unindent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unindent";
	ot->idname = "TEXT_OT_unindent";
	ot->description = "Unindent selected text";
	
	/* api callbacks */
	ot->exec = text_unindent_exec;
	ot->poll = text_edit_poll;
}

/******************* line break operator *********************/

static int text_line_break_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);
	int a, curts;
	int space = (text->flags & TXT_TABSTOSPACES) ? st->tabnumber : 1;

	text_drawcache_tag_update(st, 0);

	// double check tabs/spaces before splitting the line
	curts = txt_setcurr_tab_spaces(text, space);
	txt_split_curline(text);

	for (a = 0; a < curts; a++) {
		if (text->flags & TXT_TABSTOSPACES) {
			txt_add_char(text, ' ');
		}
		else {
			txt_add_char(text, '\t');
		}
	}

	if (text->curl) {
		if (text->curl->prev)
			text_update_line_edited(text->curl->prev);
		text_update_line_edited(text->curl);
	}

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	return OPERATOR_CANCELLED;
}

void TEXT_OT_line_break(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Line Break";
	ot->idname = "TEXT_OT_line_break";
	ot->description = "Insert line break at cursor position";
	
	/* api callbacks */
	ot->exec = text_line_break_exec;
	ot->poll = text_edit_poll;
}

/******************* comment operator *********************/

static int text_comment_exec(bContext *C, wmOperator *UNUSED(op))
{
	Text *text = CTX_data_edit_text(C);

	if (txt_has_sel(text)) {
		text_drawcache_tag_update(CTX_wm_space_text(C), 0);

		txt_order_cursors(text, false);
		txt_comment(text);
		text_update_edited(text);

		text_update_cursor_moved(C);
		WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void TEXT_OT_comment(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Comment";
	ot->idname = "TEXT_OT_comment";
	ot->description = "Convert selected text to comment";
	
	/* api callbacks */
	ot->exec = text_comment_exec;
	ot->poll = text_edit_poll;
}

/******************* uncomment operator *********************/

static int text_uncomment_exec(bContext *C, wmOperator *UNUSED(op))
{
	Text *text = CTX_data_edit_text(C);

	if (txt_has_sel(text)) {
		text_drawcache_tag_update(CTX_wm_space_text(C), 0);

		txt_order_cursors(text, false);
		txt_uncomment(text);
		text_update_edited(text);

		text_update_cursor_moved(C);
		WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void TEXT_OT_uncomment(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Uncomment";
	ot->idname = "TEXT_OT_uncomment";
	ot->description = "Convert selected comment to text";
	
	/* api callbacks */
	ot->exec = text_uncomment_exec;
	ot->poll = text_edit_poll;
}

/******************* convert whitespace operator *********************/

enum { TO_SPACES, TO_TABS };
static EnumPropertyItem whitespace_type_items[] = {
	{TO_SPACES, "SPACES", 0, "To Spaces", NULL},
	{TO_TABS, "TABS", 0, "To Tabs", NULL},
	{0, NULL, 0, NULL, NULL}};

static int text_convert_whitespace_exec(bContext *C, wmOperator *op)
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);
	TextLine *tmp;
	FlattenString fs;
	size_t a, j;
	char *new_line;
	int extra, number; //unknown for now
	int type = RNA_enum_get(op->ptr, "type");

	/* first convert to all space, this make it a lot easier to convert to tabs
	 * because there is no mixtures of ' ' && '\t' */
	for (tmp = text->lines.first; tmp; tmp = tmp->next) {
		const char *text_check_line     = tmp->line;
		const int   text_check_line_len = tmp->len;
		number = flatten_string(st, &fs, text_check_line) + 1;
		flatten_string_free(&fs);
		new_line = MEM_callocN(number, "Converted_Line");
		j = 0;
		for (a = 0; a < text_check_line_len; a++) { //foreach char in line
			if (text_check_line[a] == '\t') { //checking for tabs
				//get the number of spaces this tabs is showing
				//i don't like doing it this way but will look into it later
				new_line[j] = '\0';
				number = flatten_string(st, &fs, new_line);
				flatten_string_free(&fs);
				new_line[j] = '\t';
				new_line[j + 1] = '\0';
				number = flatten_string(st, &fs, new_line) - number;
				flatten_string_free(&fs);

				for (extra = 0; extra < number; extra++) {
					new_line[j] = ' ';
					j++;
				}
			}
			else {
				new_line[j] = text_check_line[a];
				j++;
			}
		}
		new_line[j] = '\0';
		// put new_line in the tmp->line spot still need to try and set the curc correctly
		if (tmp->line) MEM_freeN(tmp->line);
		if (tmp->format) MEM_freeN(tmp->format);
		
		tmp->line = new_line;
		tmp->len = strlen(new_line);
		tmp->format = NULL;
	}
	
	if (type == TO_TABS) { // Converting to tabs
		//start over from the beginning
		
		for (tmp = text->lines.first; tmp; tmp = tmp->next) {
			const char *text_check_line     = tmp->line;
			const int   text_check_line_len = tmp->len;
			extra = 0;
			for (a = 0; a < text_check_line_len; a++) {
				number = 0;
				for (j = 0; j < (size_t)st->tabnumber; j++) {
					if ((a + j) <= text_check_line_len) { //check to make sure we are not pass the end of the line
						if (text_check_line[a + j] != ' ') {
							number = 1;
						}
					}
				}
				if (!number) { //found all number of space to equal a tab
					a = a + (st->tabnumber - 1);
					extra = extra + 1;
				}
			}
			
			if (extra > 0) {   //got tabs make malloc and do what you have to do
				new_line = MEM_callocN(text_check_line_len - (((st->tabnumber * extra) - extra) - 1), "Converted_Line");
				extra = 0; //reuse vars
				for (a = 0; a < text_check_line_len; a++) {
					number = 0;
					for (j = 0; j < (size_t)st->tabnumber; j++) {
						if ((a + j) <= text_check_line_len) { //check to make sure we are not pass the end of the line
							if (text_check_line[a + j] != ' ') {
								number = 1;
							}
						}
					}

					if (!number) { //found all number of space to equal a tab
						new_line[extra] = '\t';
						a = a + (st->tabnumber - 1);
						extra++;
						
					}
					else { //not adding a tab
						new_line[extra] = text_check_line[a];
						extra++;
					}
				}
				new_line[extra] = '\0';
				// put new_line in the tmp->line spot still need to try and set the curc correctly
				if (tmp->line) MEM_freeN(tmp->line);
				if (tmp->format) MEM_freeN(tmp->format);
				
				tmp->line = new_line;
				tmp->len = strlen(new_line);
				tmp->format = NULL;
			}
		}
	}

	text_update_edited(text);
	text_update_cursor_moved(C);
	text_drawcache_tag_update(st, 1);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_convert_whitespace(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Convert Whitespace";
	ot->idname = "TEXT_OT_convert_whitespace";
	ot->description = "Convert whitespaces by type";
	
	/* api callbacks */
	ot->exec = text_convert_whitespace_exec;
	ot->poll = text_edit_poll;

	/* properties */
	RNA_def_enum(ot->srna, "type", whitespace_type_items, TO_SPACES, "Type", "Type of whitespace to convert to");
}

/******************* select all operator *********************/

static int text_select_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	Text *text = CTX_data_edit_text(C);

	txt_sel_all(text);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select All";
	ot->idname = "TEXT_OT_select_all";
	ot->description = "Select all text";
	
	/* api callbacks */
	ot->exec = text_select_all_exec;
	ot->poll = text_edit_poll;
}

/******************* select line operator *********************/

static int text_select_line_exec(bContext *C, wmOperator *UNUSED(op))
{
	Text *text = CTX_data_edit_text(C);

	txt_sel_line(text);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_select_line(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Line";
	ot->idname = "TEXT_OT_select_line";
	ot->description = "Select text by line";
	
	/* api callbacks */
	ot->exec = text_select_line_exec;
	ot->poll = text_edit_poll;
}

/******************* select word operator *********************/

static int text_select_word_exec(bContext *C, wmOperator *UNUSED(op))
{
	Text *text = CTX_data_edit_text(C);
	/* don't advance cursor before stepping */
	const bool use_init_step = false;

	txt_jump_left(text, false, use_init_step);
	txt_jump_right(text, true, use_init_step);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_select_word(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Word";
	ot->idname = "TEXT_OT_select_word";
	ot->description = "Select word under cursor";

	/* api callbacks */
	ot->exec = text_select_word_exec;
	ot->poll = text_edit_poll;
}

/********************* move lines operators ***********************/

static int move_lines_exec(bContext *C, wmOperator *op)
{
	Text *text = CTX_data_edit_text(C);
	const int direction = RNA_enum_get(op->ptr, "direction");
	
	txt_move_lines(text, direction);
	
	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	/* run the script while editing, evil but useful */
	if (CTX_wm_space_text(C)->live_edit)
		text_run_script(C, NULL);
	
	return OPERATOR_FINISHED;
}

void TEXT_OT_move_lines(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[] = {
		{TXT_MOVE_LINE_UP, "UP", 0, "Up", ""},
		{TXT_MOVE_LINE_DOWN, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Move Lines";
	ot->idname = "TEXT_OT_move_lines";
	ot->description = "Move the currently selected line(s) up/down";
	
	/* api callbacks */
	ot->exec = move_lines_exec;
	ot->poll = text_edit_poll;

	/* properties */
	RNA_def_enum(ot->srna, "direction", direction_items, 1, "Direction", "");
}

/************************ move operator ************************/

static EnumPropertyItem move_type_items[] = {
	{LINE_BEGIN, "LINE_BEGIN", 0, "Line Begin", ""},
	{LINE_END, "LINE_END", 0, "Line End", ""},
	{FILE_TOP, "FILE_TOP", 0, "File Top", ""},
	{FILE_BOTTOM, "FILE_BOTTOM", 0, "File Bottom", ""},
	{PREV_CHAR, "PREVIOUS_CHARACTER", 0, "Previous Character", ""},
	{NEXT_CHAR, "NEXT_CHARACTER", 0, "Next Character", ""},
	{PREV_WORD, "PREVIOUS_WORD", 0, "Previous Word", ""},
	{NEXT_WORD, "NEXT_WORD", 0, "Next Word", ""},
	{PREV_LINE, "PREVIOUS_LINE", 0, "Previous Line", ""},
	{NEXT_LINE, "NEXT_LINE", 0, "Next Line", ""},
	{PREV_PAGE, "PREVIOUS_PAGE", 0, "Previous Page", ""},
	{NEXT_PAGE, "NEXT_PAGE", 0, "Next Page", ""},
	{0, NULL, 0, NULL, NULL}};

/* get cursor position in line by relative wrapped line and column positions */
static int text_get_cursor_rel(SpaceText *st, ARegion *ar, TextLine *linein, int rell, int relc)
{
	int i, j, start, end, max, chop, curs, loop, endj, found, selc;
	char ch;

	max = wrap_width(st, ar);

	selc = start = endj = curs = found = 0;
	end = max;
	chop = loop = 1;

	for (i = 0, j = 0; loop; j += BLI_str_utf8_size_safe(linein->line + j)) {
		int chars;
		int columns = BLI_str_utf8_char_width_safe(linein->line + j); /* = 1 for tab */

		/* Mimic replacement of tabs */
		ch = linein->line[j];
		if (ch == '\t') {
			chars = st->tabnumber - i % st->tabnumber;
			ch = ' ';
		}
		else {
			chars = 1;
		}

		while (chars--) {
			if (rell == 0 && i - start <= relc && i + columns - start > relc) {
				/* current position could be wrapped to next line */
				/* this should be checked when end of current line would be reached */
				selc = j;
				found = 1;
			}
			else if (i - end <= relc && i + columns - end > relc) {
				curs = j;
			}
			if (i + columns - start > max) {
				end = MIN2(end, i);

				if (found) {
					/* exact cursor position was found, check if it's */
					/* still on needed line (hasn't been wrapped) */
					if (selc > endj && !chop) selc = endj;
					loop = 0;
					break;
				}

				if (chop) endj = j;

				start = end;
				end += max;
				chop = 1;
				rell--;

				if (rell == 0 && i + columns - start > relc) {
					selc = curs;
					loop = 0;
					break;
				}
			}
			else if (ch == '\0') {
				if (!found) selc = linein->len;
				loop = 0;
				break;
			}
			else if (ch == ' ' || ch == '-') {
				if (found) {
					loop = 0;
					break;
				}

				if (rell == 0 && i + columns - start > relc) {
					selc = curs;
					loop = 0;
					break;
				}
				end = i + 1;
				endj = j;
				chop = 0;
			}
			i += columns;
		}
	}

	return selc;
}

static int cursor_skip_find_line(SpaceText *st, ARegion *ar,
                                 int lines, TextLine **linep, int *charp, int *rell, int *relc)
{
	int offl, offc, visible_lines;

	wrap_offset_in_line(st, ar, *linep, *charp, &offl, &offc);
	*relc = text_get_char_pos(st, (*linep)->line, *charp) + offc;
	*rell = lines;

	/* handle current line */
	if (lines > 0) {
		visible_lines = text_get_visible_lines(st, ar, (*linep)->line);

		if (*rell - visible_lines + offl >= 0) {
			if (!(*linep)->next) {
				if (offl < visible_lines - 1) {
					*rell = visible_lines - 1;
					return 1;
				}

				*charp = (*linep)->len;
				return 0;
			}

			*rell -= visible_lines - offl;
			*linep = (*linep)->next;
		}
		else {
			*rell += offl;
			return 1;
		}
	}
	else {
		if (*rell + offl <= 0) {
			if (!(*linep)->prev) {
				if (offl) {
					*rell = 0;
					return 1;
				}

				*charp = 0;
				return 0;
			}

			*rell += offl;
			*linep = (*linep)->prev;
		}
		else {
			*rell += offl;
			return 1;
		}
	}

	/* skip lines and find destination line and offsets */
	while (*linep) {
		visible_lines = text_get_visible_lines(st, ar, (*linep)->line);

		if (lines < 0) { /* moving top */
			if (*rell + visible_lines >= 0) {
				*rell += visible_lines;
				break;
			}

			if (!(*linep)->prev) {
				*rell = 0;
				break;
			}

			*rell += visible_lines;
			*linep = (*linep)->prev;
		}
		else { /* moving bottom */
			if (*rell - visible_lines < 0) break;

			if (!(*linep)->next) {
				*rell = visible_lines - 1;
				break;
			}

			*rell -= visible_lines;
			*linep = (*linep)->next;
		}
	}

	return 1;
}

static void txt_wrap_move_bol(SpaceText *st, ARegion *ar, const bool sel)
{
	Text *text = st->text;
	TextLine **linep;
	int *charp;
	int oldc, i, j, max, start, end, endj, chop, loop;
	char ch;

	text_update_character_width(st);

	if (sel) { linep = &text->sell; charp = &text->selc; }
	else     { linep = &text->curl; charp = &text->curc; }

	oldc = *charp;

	max = wrap_width(st, ar);

	start = endj = 0;
	end = max;
	chop = loop = 1;
	*charp = 0;

	for (i = 0, j = 0; loop; j += BLI_str_utf8_size_safe((*linep)->line + j)) {
		int chars;
		int columns = BLI_str_utf8_char_width_safe((*linep)->line + j); /* = 1 for tab */

		/* Mimic replacement of tabs */
		ch = (*linep)->line[j];
		if (ch == '\t') {
			chars = st->tabnumber - i % st->tabnumber;
			ch = ' ';
		}
		else {
			chars = 1;
		}

		while (chars--) {
			if (i + columns - start > max) {
				end = MIN2(end, i);

				*charp = endj;

				if (j >= oldc) {
					if (ch == '\0') *charp = txt_utf8_column_to_offset((*linep)->line, start);
					loop = 0;
					break;
				}

				if (chop) endj = j;

				start = end;
				end += max;
				chop = 1;
			}
			else if (ch == ' ' || ch == '-' || ch == '\0') {
				if (j >= oldc) {
					*charp = txt_utf8_column_to_offset((*linep)->line, start);
					loop = 0;
					break;
				}

				end = i + 1;
				endj = j + 1;
				chop = 0;
			}
			i += columns;
		}
	}

	if (!sel) txt_pop_sel(text);
}

static void txt_wrap_move_eol(SpaceText *st, ARegion *ar, const bool sel)
{
	Text *text = st->text;
	TextLine **linep;
	int *charp;
	int oldc, i, j, max, start, end, endj, chop, loop;
	char ch;

	text_update_character_width(st);

	if (sel) { linep = &text->sell; charp = &text->selc; }
	else     { linep = &text->curl; charp = &text->curc; }

	oldc = *charp;

	max = wrap_width(st, ar);

	start = endj = 0;
	end = max;
	chop = loop = 1;
	*charp = 0;

	for (i = 0, j = 0; loop; j += BLI_str_utf8_size_safe((*linep)->line + j)) {
		int chars;
		int columns = BLI_str_utf8_char_width_safe((*linep)->line + j); /* = 1 for tab */

		/* Mimic replacement of tabs */
		ch = (*linep)->line[j];
		if (ch == '\t') {
			chars = st->tabnumber - i % st->tabnumber;
			ch = ' ';
		}
		else {
			chars = 1;
		}

		while (chars--) {
			if (i + columns - start > max) {
				end = MIN2(end, i);

				if (chop) endj = BLI_str_prev_char_utf8((*linep)->line + j) - (*linep)->line;

				if (endj >= oldc) {
					if (ch == '\0') *charp = (*linep)->len;
					else *charp = endj;
					loop = 0;
					break;
				}

				start = end;
				end += max;
				chop = 1;
			}
			else if (ch == '\0') {
				*charp = (*linep)->len;
				loop = 0;
				break;
			}
			else if (ch == ' ' || ch == '-') {
				end = i + 1;
				endj = j;
				chop = 0;
			}
			i += columns;
		}
	}

	if (!sel) txt_pop_sel(text);
}

static void txt_wrap_move_up(SpaceText *st, ARegion *ar, const bool sel)
{
	Text *text = st->text;
	TextLine **linep;
	int *charp;
	int offl, offc, col;

	text_update_character_width(st);

	if (sel) { linep = &text->sell; charp = &text->selc; }
	else     { linep = &text->curl; charp = &text->curc; }

	wrap_offset_in_line(st, ar, *linep, *charp, &offl, &offc);
	col = text_get_char_pos(st, (*linep)->line, *charp) + offc;
	if (offl) {
		*charp = text_get_cursor_rel(st, ar, *linep, offl - 1, col);
	}
	else {
		if ((*linep)->prev) {
			int visible_lines;

			*linep = (*linep)->prev;
			visible_lines = text_get_visible_lines(st, ar, (*linep)->line);
			*charp = text_get_cursor_rel(st, ar, *linep, visible_lines - 1, col);
		}
		else {
			*charp = 0;
		}
	}

	if (!sel) txt_pop_sel(text);
}

static void txt_wrap_move_down(SpaceText *st, ARegion *ar, const bool sel)
{
	Text *text = st->text;
	TextLine **linep;
	int *charp;
	int offl, offc, col, visible_lines;

	text_update_character_width(st);

	if (sel) { linep = &text->sell; charp = &text->selc; }
	else     { linep = &text->curl; charp = &text->curc; }

	wrap_offset_in_line(st, ar, *linep, *charp, &offl, &offc);
	col = text_get_char_pos(st, (*linep)->line, *charp) + offc;
	visible_lines = text_get_visible_lines(st, ar, (*linep)->line);
	if (offl < visible_lines - 1) {
		*charp = text_get_cursor_rel(st, ar, *linep, offl + 1, col);
	}
	else {
		if ((*linep)->next) {
			*linep = (*linep)->next;
			*charp = text_get_cursor_rel(st, ar, *linep, 0, col);
		}
		else {
			*charp = (*linep)->len;
		}
	}

	if (!sel) txt_pop_sel(text);
}

/* Moves the cursor vertically by the specified number of lines.
 * If the destination line is shorter than the current cursor position, the
 * cursor will be positioned at the end of this line.
 *
 * This is to replace screen_skip for PageUp/Down operations.
 */
static void cursor_skip(SpaceText *st, ARegion *ar, Text *text, int lines, const bool sel)
{
	TextLine **linep;
	int *charp;
	
	if (sel) { linep = &text->sell; charp = &text->selc; }
	else     { linep = &text->curl; charp = &text->curc; }

	if (st && ar && st->wordwrap) {
		int rell, relc;

		/* find line and offsets inside it needed to set cursor position */
		if (cursor_skip_find_line(st, ar, lines, linep, charp, &rell, &relc))
			*charp = text_get_cursor_rel(st, ar, *linep, rell, relc);
	}
	else {
		while (lines > 0 && (*linep)->next) {
			*linep = (*linep)->next;
			lines--;
		}
		while (lines < 0 && (*linep)->prev) {
			*linep = (*linep)->prev;
			lines++;
		}
	}

	if (*charp > (*linep)->len) *charp = (*linep)->len;

	if (!sel) txt_pop_sel(text);
}

static int text_move_cursor(bContext *C, int type, bool select)
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);
	ARegion *ar = CTX_wm_region(C);

	/* ensure we have the right region, it's optional */
	if (ar && ar->regiontype != RGN_TYPE_WINDOW)
		ar = NULL;

	switch (type) {
		case LINE_BEGIN:
			if (!select) {
				txt_sel_clear(text);
			}
			if (st && st->wordwrap && ar) txt_wrap_move_bol(st, ar, select);
			else txt_move_bol(text, select);
			break;
			
		case LINE_END:
			if (!select) {
				txt_sel_clear(text);
			}
			if (st && st->wordwrap && ar) txt_wrap_move_eol(st, ar, select);
			else txt_move_eol(text, select);
			break;

		case FILE_TOP:
			txt_move_bof(text, select);
			break;
			
		case FILE_BOTTOM:
			txt_move_eof(text, select);
			break;

		case PREV_WORD:
			if (txt_cursor_is_line_start(text)) {
				txt_move_left(text, select);
			}
			txt_jump_left(text, select, true);
			break;

		case NEXT_WORD:
			if (txt_cursor_is_line_end(text)) {
				txt_move_right(text, select);
			}
			txt_jump_right(text, select, true);
			break;

		case PREV_CHAR:
			if (txt_has_sel(text) && !select) {
				txt_order_cursors(text, false);
				txt_pop_sel(text);
			}
			else {
				txt_move_left(text, select);
			}
			break;

		case NEXT_CHAR:
			if (txt_has_sel(text) && !select) {
				txt_order_cursors(text, true);
				txt_pop_sel(text);
			}
			else {
				txt_move_right(text, select);
			}
			break;

		case PREV_LINE:
			if (st && st->wordwrap && ar) txt_wrap_move_up(st, ar, select);
			else txt_move_up(text, select);
			break;
			
		case NEXT_LINE:
			if (st && st->wordwrap && ar) txt_wrap_move_down(st, ar, select);
			else txt_move_down(text, select);
			break;

		case PREV_PAGE:
			if (st) cursor_skip(st, ar, st->text, -st->viewlines, select);
			else cursor_skip(NULL, NULL, text, -10, select);
			break;

		case NEXT_PAGE:
			if (st) cursor_skip(st, ar, st->text, st->viewlines, select);
			else cursor_skip(NULL, NULL, text, 10, select);
			break;
	}

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);

	return OPERATOR_FINISHED;
}

static int text_move_exec(bContext *C, wmOperator *op)
{
	int type = RNA_enum_get(op->ptr, "type");

	return text_move_cursor(C, type, 0);
}

void TEXT_OT_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Cursor";
	ot->idname = "TEXT_OT_move";
	ot->description = "Move cursor to position type";
	
	/* api callbacks */
	ot->exec = text_move_exec;
	ot->poll = text_edit_poll;

	/* properties */
	RNA_def_enum(ot->srna, "type", move_type_items, LINE_BEGIN, "Type", "Where to move cursor to");
}

/******************* move select operator ********************/

static int text_move_select_exec(bContext *C, wmOperator *op)
{
	int type = RNA_enum_get(op->ptr, "type");

	return text_move_cursor(C, type, 1);
}

void TEXT_OT_move_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Select";
	ot->idname = "TEXT_OT_move_select";
	ot->description = "Move the cursor while selecting";
	
	/* api callbacks */
	ot->exec = text_move_select_exec;
	ot->poll = text_space_edit_poll;

	/* properties */
	RNA_def_enum(ot->srna, "type", move_type_items, LINE_BEGIN, "Type", "Where to move cursor to, to make a selection");
}

/******************* jump operator *********************/

static int text_jump_exec(bContext *C, wmOperator *op)
{
	Text *text = CTX_data_edit_text(C);
	int line = RNA_int_get(op->ptr, "line");
	short nlines = txt_get_span(text->lines.first, text->lines.last) + 1;

	if (line < 1)
		txt_move_toline(text, 1, 0);
	else if (line > nlines)
		txt_move_toline(text, nlines - 1, 0);
	else
		txt_move_toline(text, line - 1, 0);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);

	return OPERATOR_FINISHED;
}

static int text_jump_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	return WM_operator_props_dialog_popup(C, op, 10 * UI_UNIT_X, 5 * UI_UNIT_Y);

}

void TEXT_OT_jump(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Jump";
	ot->idname = "TEXT_OT_jump";
	ot->description = "Jump cursor to line";
	
	/* api callbacks */
	ot->invoke = text_jump_invoke;
	ot->exec = text_jump_exec;
	ot->poll = text_edit_poll;

	/* properties */
	prop = RNA_def_int(ot->srna, "line", 1, 1, INT_MAX, "Line", "Line number to jump to", 1, 10000);
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_TEXT);
}

/******************* delete operator **********************/

static EnumPropertyItem delete_type_items[] = {
	{DEL_NEXT_CHAR, "NEXT_CHARACTER", 0, "Next Character", ""},
	{DEL_PREV_CHAR, "PREVIOUS_CHARACTER", 0, "Previous Character", ""},
	{DEL_NEXT_WORD, "NEXT_WORD", 0, "Next Word", ""},
	{DEL_PREV_WORD, "PREVIOUS_WORD", 0, "Previous Word", ""},
	{0, NULL, 0, NULL, NULL}};

static int text_delete_exec(bContext *C, wmOperator *op)
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);
	int type = RNA_enum_get(op->ptr, "type");

	text_drawcache_tag_update(st, 0);

	/* behavior could be changed here,
	 * but for now just don't jump words when we have a selection */
	if (txt_has_sel(text)) {
		if      (type == DEL_PREV_WORD) type = DEL_PREV_CHAR;
		else if (type == DEL_NEXT_WORD) type = DEL_NEXT_CHAR;
	}

	if (type == DEL_PREV_WORD) {
		if (txt_cursor_is_line_start(text)) {
			txt_backspace_char(text);
		}
		txt_backspace_word(text);
	}
	else if (type == DEL_PREV_CHAR) {
		txt_backspace_char(text);
	}
	else if (type == DEL_NEXT_WORD) {
		if (txt_cursor_is_line_end(text)) {
			txt_delete_char(text);
		}
		txt_delete_word(text);
	}
	else if (type == DEL_NEXT_CHAR) {
		txt_delete_char(text);
	}

	text_update_line_edited(text->curl);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	/* run the script while editing, evil but useful */
	if (st->live_edit)
		text_run_script(C, NULL);
	
	return OPERATOR_FINISHED;
}

void TEXT_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete";
	ot->idname = "TEXT_OT_delete";
	ot->description = "Delete text by cursor position";
	
	/* api callbacks */
	ot->exec = text_delete_exec;
	ot->poll = text_edit_poll;

	/* properties */
	RNA_def_enum(ot->srna, "type", delete_type_items, DEL_NEXT_CHAR, "Type", "Which part of the text to delete");
}

/******************* toggle overwrite operator **********************/

static int text_toggle_overwrite_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceText *st = CTX_wm_space_text(C);

	st->overwrite = !st->overwrite;

	WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_overwrite_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Overwrite";
	ot->idname = "TEXT_OT_overwrite_toggle";
	ot->description = "Toggle overwrite while typing";
	
	/* api callbacks */
	ot->exec = text_toggle_overwrite_exec;
	ot->poll = text_space_edit_poll;
}

/******************* scroll operator **********************/

static void txt_screen_clamp(SpaceText *st, ARegion *ar)
{
	if (st->top <= 0) {
		st->top = 0;
	}
	else {
		int last;
		last = text_get_total_lines(st, ar);
		last = last - (st->viewlines / 2);
		if (last > 0 && st->top > last) {
			st->top = last;
		}
	}
}

/* Moves the view vertically by the specified number of lines */
static void txt_screen_skip(SpaceText *st, ARegion *ar, int lines)
{
	st->top += lines;
	txt_screen_clamp(st, ar);
}

/* quick enum for tsc->zone (scroller handles) */
enum {
	SCROLLHANDLE_BAR,
	SCROLLHANDLE_MIN_OUTSIDE,
	SCROLLHANDLE_MAX_OUTSIDE
};

typedef struct TextScroll {
	int old[2];
	int delta[2];

	int first;
	int scrollbar;

	int zone;
} TextScroll;

static int text_scroll_poll(bContext *C)
{
	/* it should be possible to still scroll linked texts to read them, even if they can't be edited... */
	return CTX_data_edit_text(C) != NULL;
}

static int text_scroll_exec(bContext *C, wmOperator *op)
{
	SpaceText *st = CTX_wm_space_text(C);
	ARegion *ar = CTX_wm_region(C);

	int lines = RNA_int_get(op->ptr, "lines");

	if (lines == 0)
		return OPERATOR_CANCELLED;

	txt_screen_skip(st, ar, lines * U.wheellinescroll);

	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
}

static void text_scroll_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceText *st = CTX_wm_space_text(C);
	ARegion *ar = CTX_wm_region(C);
	TextScroll *tsc = op->customdata;
	int mval[2] = {event->x, event->y};
	int scroll_steps[2] = {0, 0};

	text_update_character_width(st);

	/* compute mouse move distance */
	if (tsc->first) {
		tsc->old[0] = mval[0];
		tsc->old[1] = mval[1];
		tsc->first = 0;
	}

	if (event->type != MOUSEPAN) {
		tsc->delta[0] = mval[0] - tsc->old[0];
		tsc->delta[1] = mval[1] - tsc->old[1];
	}

	/* accumulate scroll, in float values for events that give less than one
	 * line offset but taken together should still scroll */
	if (!tsc->scrollbar) {
		st->scroll_accum[0] += -tsc->delta[0] / (float)st->cwidth;
		st->scroll_accum[1] += tsc->delta[1] / (float)(st->lheight_dpi + TXT_LINE_SPACING);
	}
	else {
		st->scroll_accum[1] += -tsc->delta[1] * st->pix_per_line;
	}

	/* round to number of lines to scroll */
	scroll_steps[0] = (int)st->scroll_accum[0];
	scroll_steps[1] = (int)st->scroll_accum[1];

	st->scroll_accum[0] -= scroll_steps[0];
	st->scroll_accum[1] -= scroll_steps[1];

	/* perform vertical and/or horizontal scroll */
	if (scroll_steps[0] || scroll_steps[1]) {
		txt_screen_skip(st, ar, scroll_steps[1]);

		if (st->wordwrap) {
			st->left = 0;
		}
		else {
			st->left += scroll_steps[0];
			if (st->left < 0) st->left = 0;
		}

		ED_area_tag_redraw(CTX_wm_area(C));
	}

	tsc->old[0] = mval[0];
	tsc->old[1] = mval[1];
}

static void scroll_exit(bContext *C, wmOperator *op)
{
	SpaceText *st = CTX_wm_space_text(C);

	st->flags &= ~ST_SCROLL_SELECT;
	MEM_freeN(op->customdata);
}

static int text_scroll_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	TextScroll *tsc = op->customdata;
	SpaceText *st = CTX_wm_space_text(C);
	ARegion *ar = CTX_wm_region(C);

	switch (event->type) {
		case MOUSEMOVE:
			if (tsc->zone == SCROLLHANDLE_BAR)
				text_scroll_apply(C, op, event);
			break;
		case LEFTMOUSE:
		case RIGHTMOUSE:
		case MIDDLEMOUSE:
			if (ELEM(tsc->zone, SCROLLHANDLE_MIN_OUTSIDE, SCROLLHANDLE_MAX_OUTSIDE)) {
				txt_screen_skip(st, ar, st->viewlines * (tsc->zone == SCROLLHANDLE_MIN_OUTSIDE ? 1 : -1));

				ED_area_tag_redraw(CTX_wm_area(C));
			}
			scroll_exit(C, op);
			return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static void text_scroll_cancel(bContext *C, wmOperator *op)
{
	scroll_exit(C, op);
}

static int text_scroll_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceText *st = CTX_wm_space_text(C);
	TextScroll *tsc;
	
	if (RNA_struct_property_is_set(op->ptr, "lines"))
		return text_scroll_exec(C, op);
	
	tsc = MEM_callocN(sizeof(TextScroll), "TextScroll");
	tsc->first = 1;
	tsc->zone = SCROLLHANDLE_BAR;
	op->customdata = tsc;
	
	st->flags |= ST_SCROLL_SELECT;
	
	if (event->type == MOUSEPAN) {
		text_update_character_width(st);
		
		tsc->old[0] = event->x;
		tsc->old[1] = event->y;
		/* Sensitivity of scroll set to 4pix per line/char */
		tsc->delta[0] = (event->x - event->prevx) * st->cwidth / 4;
		tsc->delta[1] = (event->y - event->prevy) * st->lheight_dpi / 4;
		tsc->first = 0;
		tsc->scrollbar = 0;
		text_scroll_apply(C, op, event);
		scroll_exit(C, op);
		return OPERATOR_FINISHED;
	}

	WM_event_add_modal_handler(C, op);
	
	return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_scroll(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Scroll";
	/* don't really see the difference between this and
	 * scroll_bar. Both do basically the same thing (aside 
	 * from keymaps).*/
	ot->idname = "TEXT_OT_scroll";
	ot->description = "";
	
	/* api callbacks */
	ot->exec = text_scroll_exec;
	ot->invoke = text_scroll_invoke;
	ot->modal = text_scroll_modal;
	ot->cancel = text_scroll_cancel;
	ot->poll = text_scroll_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_POINTER | OPTYPE_INTERNAL;

	/* properties */
	RNA_def_int(ot->srna, "lines", 1, INT_MIN, INT_MAX, "Lines", "Number of lines to scroll", -100, 100);
}

/******************** scroll bar operator *******************/

static int text_region_scroll_poll(bContext *C)
{
	/* same as text_region_edit_poll except it works on libdata too */
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);
	ARegion *ar = CTX_wm_region(C);

	if (!st || !text)
		return 0;
	
	if (!ar || ar->regiontype != RGN_TYPE_WINDOW)
		return 0;
	
	return 1;
}

static int text_scroll_bar_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceText *st = CTX_wm_space_text(C);
	ARegion *ar = CTX_wm_region(C);
	TextScroll *tsc;
	const int *mval = event->mval;
	int zone = -1;

	if (RNA_struct_property_is_set(op->ptr, "lines"))
		return text_scroll_exec(C, op);
	
	/* verify we are in the right zone */
	if (mval[0] > st->txtbar.xmin && mval[0] < st->txtbar.xmax) {
		if (mval[1] >= st->txtbar.ymin && mval[1] <= st->txtbar.ymax) {
			/* mouse inside scroll handle */
			zone = SCROLLHANDLE_BAR;
		}
		else if (mval[1] > TXT_SCROLL_SPACE && mval[1] < ar->winy - TXT_SCROLL_SPACE) {
			if (mval[1] < st->txtbar.ymin) zone = SCROLLHANDLE_MIN_OUTSIDE;
			else zone = SCROLLHANDLE_MAX_OUTSIDE;
		}
	}

	if (zone == -1) {
		/* we are outside slider - nothing to do */
		return OPERATOR_PASS_THROUGH;
	}

	tsc = MEM_callocN(sizeof(TextScroll), "TextScroll");
	tsc->first = 1;
	tsc->scrollbar = 1;
	tsc->zone = zone;
	op->customdata = tsc;
	st->flags |= ST_SCROLL_SELECT;

	/* jump scroll, works in v2d but needs to be added here too :S */
	if (event->type == MIDDLEMOUSE) {
		tsc->old[0] = ar->winrct.xmin + BLI_rcti_cent_x(&st->txtbar);
		tsc->old[1] = ar->winrct.ymin + BLI_rcti_cent_y(&st->txtbar);

		tsc->first = 0;
		tsc->zone = SCROLLHANDLE_BAR;
		text_scroll_apply(C, op, event);
	}

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_scroll_bar(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Scrollbar";
	/* don't really see the difference between this and
	 * scroll. Both do basically the same thing (aside 
	 * from keymaps).*/
	ot->idname = "TEXT_OT_scroll_bar";
	ot->description = "";
	
	/* api callbacks */
	ot->invoke = text_scroll_bar_invoke;
	ot->modal = text_scroll_modal;
	ot->cancel = text_scroll_cancel;
	ot->poll = text_region_scroll_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL;

	/* properties */
	RNA_def_int(ot->srna, "lines", 1, INT_MIN, INT_MAX, "Lines", "Number of lines to scroll", -100, 100);
}

/******************* set selection operator **********************/

typedef struct SetSelection {
	int selecting;
	int selc, sell;
	short old[2];
} SetSelection;

static int flatten_width(SpaceText *st, const char *str)
{
	int i, total = 0;

	for (i = 0; str[i]; i += BLI_str_utf8_size_safe(str + i)) {
		if (str[i] == '\t') {
			total += st->tabnumber - total % st->tabnumber;
		}
		else {
			total += BLI_str_utf8_char_width_safe(str + i);
		}
	}
	
	return total;
}

static int flatten_column_to_offset(SpaceText *st, const char *str, int index)
{
	int i = 0, j = 0, col;

	while (*(str + j)) {
		if (str[j] == '\t')
			col = st->tabnumber - i % st->tabnumber;
		else
			col = BLI_str_utf8_char_width_safe(str + j);
		
		if (i + col > index)
			break;
		
		i += col;
		j += BLI_str_utf8_size_safe(str + j);
	}
	
	return j;
}

static TextLine *get_first_visible_line(SpaceText *st, ARegion *ar, int *y)
{
	TextLine *linep = st->text->lines.first;
	int i;
	for (i = st->top; i > 0 && linep; ) {
		int lines = text_get_visible_lines(st, ar, linep->line);
		
		if (i - lines < 0) {
			*y += i;
			break;
		}
		else {
			linep = linep->next;
			i -= lines;
		}
	}
	return linep;
}

static void text_cursor_set_to_pos_wrapped(SpaceText *st, ARegion *ar, int x, int y, const bool sel)
{
	Text *text = st->text;
	int max = wrap_width(st, ar); /* column */
	int charp = -1;               /* mem */
	int loop = 1, found = 0;      /* flags */
	char ch;
	
	/* Point to first visible line */
	TextLine *linep = get_first_visible_line(st, ar, &y);
	
	while (loop && linep) {
		int i = 0, start = 0, end = max; /* column */
		int j = 0, curs = 0, endj = 0;   /* mem */
		int chop = 1;                    /* flags */
		
		for (; loop; j += BLI_str_utf8_size_safe(linep->line + j)) {
			int chars;
			int columns = BLI_str_utf8_char_width_safe(linep->line + j); /* = 1 for tab */
			
			/* Mimic replacement of tabs */
			ch = linep->line[j];
			if (ch == '\t') {
				chars = st->tabnumber - i % st->tabnumber;
				ch = ' ';
			}
			else {
				chars = 1;
			}
			
			while (chars--) {
				/* Gone too far, go back to last wrap point */
				if (y < 0) {
					charp = endj;
					loop = 0;
					break;
					/* Exactly at the cursor */
				}
				else if (y == 0 && i - start <= x && i + columns - start > x) {
					/* current position could be wrapped to next line */
					/* this should be checked when end of current line would be reached */
					charp = curs = j;
					found = 1;
					/* Prepare curs for next wrap */
				}
				else if (i - end <= x && i + columns - end > x) {
					curs = j;
				}
				if (i + columns - start > max) {
					end = MIN2(end, i);
					
					if (found) {
						/* exact cursor position was found, check if it's */
						/* still on needed line (hasn't been wrapped) */
						if (charp > endj && !chop && ch != '\0') charp = endj;
						loop = 0;
						break;
					}
					
					if (chop) endj = j;
					start = end;
					end += max;
					
					if (j < linep->len)
						y--;
					
					chop = 1;
					if (y == 0 && i + columns - start > x) {
						charp = curs;
						loop = 0;
						break;
					}
				}
				else if (ch == ' ' || ch == '-' || ch == '\0') {
					if (found) {
						loop = 0;
						break;
					}
					
					if (y == 0 && i + columns - start > x) {
						charp = curs;
						loop = 0;
						break;
					}
					end = i + 1;
					endj = j;
					chop = 0;
				}
				i += columns;
			}
			
			if (ch == '\0') break;
		}
		
		if (!loop || found) break;
		
		if (!linep->next) {
			charp = linep->len;
			break;
		}
		
		/* On correct line but didn't meet cursor, must be at end */
		if (y == 0) {
			charp = linep->len;
			break;
		}
		linep = linep->next;
		
		y--;
	}

	if (linep && charp != -1) {
		if (sel) { text->sell = linep; text->selc = charp; }
		else     { text->curl = linep; text->curc = charp; }
	}
}

static void text_cursor_set_to_pos(SpaceText *st, ARegion *ar, int x, int y, const bool sel)
{
	Text *text = st->text;
	text_update_character_width(st);
	y = (ar->winy - 2 - y) / (st->lheight_dpi + TXT_LINE_SPACING);

	if (st->showlinenrs) x -= TXT_OFFSET + TEXTXLOC;
	else x -= TXT_OFFSET;

	if (x < 0) x = 0;
	x = text_pixel_x_to_column(st, x) + st->left;
	
	if (st->wordwrap) {
		text_cursor_set_to_pos_wrapped(st, ar, x, y, sel);
	}
	else {
		TextLine **linep;
		int *charp;
		int w;
		
		if (sel) { linep = &text->sell; charp = &text->selc; }
		else     { linep = &text->curl; charp = &text->curc; }
		
		y -= txt_get_span(text->lines.first, *linep) - st->top;
		
		if (y > 0) {
			while (y-- != 0) if ((*linep)->next) *linep = (*linep)->next;
		}
		else if (y < 0) {
			while (y++ != 0) if ((*linep)->prev) *linep = (*linep)->prev;
		}

		
		w = flatten_width(st, (*linep)->line);
		if (x < w) *charp = flatten_column_to_offset(st, (*linep)->line, x);
		else *charp = (*linep)->len;
	}
	if (!sel) txt_pop_sel(text);
}

static void text_cursor_set_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceText *st = CTX_wm_space_text(C);
	ARegion *ar = CTX_wm_region(C);
	SetSelection *ssel = op->customdata;

	if (event->mval[1] < 0 || event->mval[1] > ar->winy) {
		int d = (ssel->old[1] - event->mval[1]) * st->pix_per_line;
		if (d) txt_screen_skip(st, ar, d);

		text_cursor_set_to_pos(st, ar, event->mval[0], event->mval[1] < 0 ? 0 : ar->winy, 1);

		text_update_cursor_moved(C);
		WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);
	}
	else if (!st->wordwrap && (event->mval[0] < 0 || event->mval[0] > ar->winx)) {
		if (event->mval[0] > ar->winx) st->left++;
		else if (event->mval[0] < 0 && st->left > 0) st->left--;
		
		text_cursor_set_to_pos(st, ar, event->mval[0], event->mval[1], 1);
		
		text_update_cursor_moved(C);
		WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);
		// XXX PIL_sleep_ms(10);
	}
	else {
		text_cursor_set_to_pos(st, ar, event->mval[0], event->mval[1], 1);

		text_update_cursor_moved(C);
		WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);

		ssel->old[0] = event->mval[0];
		ssel->old[1] = event->mval[1];
	}
}

static void text_cursor_set_exit(bContext *C, wmOperator *op)
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = st->text;
	SetSelection *ssel = op->customdata;
	char *buffer;

	if (txt_has_sel(text)) {
		buffer = txt_sel_to_buf(text);
		WM_clipboard_text_set(buffer, 1);
		MEM_freeN(buffer);
	}

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);

	MEM_freeN(ssel);
}

static int text_set_selection_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceText *st = CTX_wm_space_text(C);
	SetSelection *ssel;

	if (event->mval[0] >= st->txtbar.xmin)
		return OPERATOR_PASS_THROUGH;

	op->customdata = MEM_callocN(sizeof(SetSelection), "SetCursor");
	ssel = op->customdata;
	ssel->selecting = RNA_boolean_get(op->ptr, "select");

	ssel->old[0] = event->mval[0];
	ssel->old[1] = event->mval[1];

	ssel->sell = txt_get_span(st->text->lines.first, st->text->sell);
	ssel->selc = st->text->selc;

	WM_event_add_modal_handler(C, op);

	text_cursor_set_apply(C, op, event);

	return OPERATOR_RUNNING_MODAL;
}

static int text_set_selection_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	switch (event->type) {
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			text_cursor_set_exit(C, op);
			return OPERATOR_FINISHED;
		case MOUSEMOVE:
			text_cursor_set_apply(C, op, event);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static void text_set_selection_cancel(bContext *C, wmOperator *op)
{
	text_cursor_set_exit(C, op);
}

void TEXT_OT_selection_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Selection";
	ot->idname = "TEXT_OT_selection_set";
	ot->description = "Set cursor selection";

	/* api callbacks */
	ot->invoke = text_set_selection_invoke;
	ot->modal = text_set_selection_modal;
	ot->cancel = text_set_selection_cancel;
	ot->poll = text_region_edit_poll;

	/* properties */
	RNA_def_boolean(ot->srna, "select", 0, "Select", "Set selection end rather than cursor");
}

/******************* set cursor operator **********************/

static int text_cursor_set_exec(bContext *C, wmOperator *op)
{
	SpaceText *st = CTX_wm_space_text(C);
	ARegion *ar = CTX_wm_region(C);
	int x = RNA_int_get(op->ptr, "x");
	int y = RNA_int_get(op->ptr, "y");

	text_cursor_set_to_pos(st, ar, x, y, 0);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);

	return OPERATOR_PASS_THROUGH;
}

static int text_cursor_set_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceText *st = CTX_wm_space_text(C);

	if (event->mval[0] >= st->txtbar.xmin)
		return OPERATOR_PASS_THROUGH;

	RNA_int_set(op->ptr, "x", event->mval[0]);
	RNA_int_set(op->ptr, "y", event->mval[1]);

	return text_cursor_set_exec(C, op);
}

void TEXT_OT_cursor_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Cursor";
	ot->idname = "TEXT_OT_cursor_set";
	ot->description = "Set cursor position";

	/* api callbacks */
	ot->invoke = text_cursor_set_invoke;
	ot->exec = text_cursor_set_exec;
	ot->poll = text_region_edit_poll;

	/* properties */
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
}

/******************* line number operator **********************/

static int text_line_number_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);
	ARegion *ar = CTX_wm_region(C);
	const int *mval = event->mval;
	double time;
	static int jump_to = 0;
	static double last_jump = 0;

	text_update_character_width(st);

	if (!st->showlinenrs)
		return OPERATOR_PASS_THROUGH;

	if (!(mval[0] > 2 && mval[0] < (TXT_OFFSET + TEXTXLOC) && mval[1] > 2 && mval[1] < ar->winy - 2))
		return OPERATOR_PASS_THROUGH;

	if (!(event->ascii >= '0' && event->ascii <= '9'))
		return OPERATOR_PASS_THROUGH;

	time = PIL_check_seconds_timer();
	if (last_jump < time - 1)
		jump_to = 0;

	jump_to *= 10;
	jump_to += (int)(event->ascii - '0');

	txt_move_toline(text, jump_to - 1, 0);
	last_jump = time;

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_line_number(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Line Number";
	ot->idname = "TEXT_OT_line_number";
	ot->description = "The current line number";
	
	/* api callbacks */
	ot->invoke = text_line_number_invoke;
	ot->poll = text_region_edit_poll;
}

/******************* insert operator **********************/

static int text_insert_exec(bContext *C, wmOperator *op)
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);
	char *str;
	bool done = false;
	size_t i = 0;
	unsigned int code;

	text_drawcache_tag_update(st, 0);

	str = RNA_string_get_alloc(op->ptr, "text", NULL, 0);

	if (st && st->overwrite) {
		while (str[i]) {
			code = BLI_str_utf8_as_unicode_step(str, &i);
			done |= txt_replace_char(text, code);
		}
	}
	else {
		while (str[i]) {
			code = BLI_str_utf8_as_unicode_step(str, &i);
			done |= txt_add_char(text, code);
		}
	}

	MEM_freeN(str);
	
	if (!done)
		return OPERATOR_CANCELLED;

	text_update_line_edited(text->curl);

	text_update_cursor_moved(C);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);

	return OPERATOR_FINISHED;
}

static int text_insert_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int ret;

	// if (!RNA_struct_property_is_set(op->ptr, "text")) { /* always set from keymap XXX */
	if (!RNA_string_length(op->ptr, "text")) {
		/* if alt/ctrl/super are pressed pass through except for utf8 character event
		 * (when input method are used for utf8 inputs, the user may assign key event
		 * including alt/ctrl/super like ctrl+m to commit utf8 string.  in such case,
		 * the modifiers in the utf8 character event make no sense.) */
		if ((event->ctrl || event->oskey) && !event->utf8_buf[0]) {
			return OPERATOR_PASS_THROUGH;
		}
		else {
			char str[BLI_UTF8_MAX + 1];
			size_t len;
			
			if (event->utf8_buf[0]) {
				len = BLI_str_utf8_size_safe(event->utf8_buf);
				memcpy(str, event->utf8_buf, len);
			}
			else {
				/* in theory, ghost can set value to extended ascii here */
				len = BLI_str_utf8_from_unicode(event->ascii, str);
			}
			str[len] = '\0';
			RNA_string_set(op->ptr, "text", str);
		}
	}

	ret = text_insert_exec(C, op);
	
	/* run the script while editing, evil but useful */
	if (ret == OPERATOR_FINISHED && CTX_wm_space_text(C)->live_edit)
		text_run_script(C, NULL);

	return ret;
}

void TEXT_OT_insert(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Insert";
	ot->idname = "TEXT_OT_insert";
	ot->description = "Insert text at cursor position";
	
	/* api callbacks */
	ot->exec = text_insert_exec;
	ot->invoke = text_insert_invoke;
	ot->poll = text_edit_poll;

	/* properties */
	prop = RNA_def_string(ot->srna, "text", NULL, 0, "Text", "Text to insert at the cursor position");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/******************* find operator *********************/

/* mode */
#define TEXT_FIND       0
#define TEXT_REPLACE    1

static int text_find_and_replace(bContext *C, wmOperator *op, short mode)
{
	Main *bmain = CTX_data_main(C);
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = st->text;
	int flags;
	int found = 0;
	char *tmp;

	if (!st->findstr[0])
		return OPERATOR_CANCELLED;

	flags = st->flags;
	if (flags & ST_FIND_ALL)
		flags &= ~ST_FIND_WRAP;

	/* Replace current */
	if (mode != TEXT_FIND && txt_has_sel(text)) {
		tmp = txt_sel_to_buf(text);

		if (flags & ST_MATCH_CASE) found = strcmp(st->findstr, tmp) == 0;
		else found = BLI_strcasecmp(st->findstr, tmp) == 0;

		if (found) {
			if (mode == TEXT_REPLACE) {
				txt_insert_buf(text, st->replacestr);
				if (text->curl && text->curl->format) {
					MEM_freeN(text->curl->format);
					text->curl->format = NULL;
				}
				text_update_cursor_moved(C);
				WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);
				text_drawcache_tag_update(CTX_wm_space_text(C), 1);
			}
		}
		MEM_freeN(tmp);
		tmp = NULL;
	}

	/* Find next */
	if (txt_find_string(text, st->findstr, flags & ST_FIND_WRAP, flags & ST_MATCH_CASE)) {
		text_update_cursor_moved(C);
		WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);
	}
	else if (flags & ST_FIND_ALL) {
		if (text->id.next)
			text = st->text = text->id.next;
		else
			text = st->text = bmain->text.first;
		txt_move_toline(text, 0, 0);
		text_update_cursor_moved(C);
		WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);
	}
	else {
		if (!found) BKE_reportf(op->reports, RPT_ERROR, "Text not found: %s", st->findstr);
	}

	return OPERATOR_FINISHED;
}

static int text_find_exec(bContext *C, wmOperator *op)
{
	return text_find_and_replace(C, op, TEXT_FIND);
}

void TEXT_OT_find(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Find Next";
	ot->idname = "TEXT_OT_find";
	ot->description = "Find specified text";
	
	/* api callbacks */
	ot->exec = text_find_exec;
	ot->poll = text_space_edit_poll;
}

/******************* replace operator *********************/

static int text_replace_exec(bContext *C, wmOperator *op)
{
	return text_find_and_replace(C, op, TEXT_REPLACE);
}

void TEXT_OT_replace(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Replace";
	ot->idname = "TEXT_OT_replace";
	ot->description = "Replace text with the specified text";

	/* api callbacks */
	ot->exec = text_replace_exec;
	ot->poll = text_space_edit_poll;
}

/******************* find set selected *********************/

static int text_find_set_selected_exec(bContext *C, wmOperator *op)
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);
	char *tmp;

	tmp = txt_sel_to_buf(text);
	BLI_strncpy(st->findstr, tmp, ST_MAX_FIND_STR);
	MEM_freeN(tmp);

	if (!st->findstr[0])
		return OPERATOR_FINISHED;

	return text_find_and_replace(C, op, TEXT_FIND);
}

void TEXT_OT_find_set_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Find Set Selected";
	ot->idname = "TEXT_OT_find_set_selected";
	ot->description = "Find specified text and set as selected";
	
	/* api callbacks */
	ot->exec = text_find_set_selected_exec;
	ot->poll = text_space_edit_poll;
}

/******************* replace set selected *********************/

static int text_replace_set_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceText *st = CTX_wm_space_text(C);
	Text *text = CTX_data_edit_text(C);
	char *tmp;

	tmp = txt_sel_to_buf(text);
	BLI_strncpy(st->replacestr, tmp, ST_MAX_FIND_STR);
	MEM_freeN(tmp);

	return OPERATOR_FINISHED;
}

void TEXT_OT_replace_set_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Replace Set Selected";
	ot->idname = "TEXT_OT_replace_set_selected";
	ot->description = "Replace text with specified text and set as selected";
	
	/* api callbacks */
	ot->exec = text_replace_set_selected_exec;
	ot->poll = text_space_edit_poll;
}

/****************** resolve conflict operator ******************/

enum { RESOLVE_IGNORE, RESOLVE_RELOAD, RESOLVE_SAVE, RESOLVE_MAKE_INTERNAL };
static EnumPropertyItem resolution_items[] = {
	{RESOLVE_IGNORE, "IGNORE", 0, "Ignore", ""},
	{RESOLVE_RELOAD, "RELOAD", 0, "Reload", ""},
	{RESOLVE_SAVE, "SAVE", 0, "Save", ""},
	{RESOLVE_MAKE_INTERNAL, "MAKE_INTERNAL", 0, "Make Internal", ""},
	{0, NULL, 0, NULL, NULL}
};

static int text_resolve_conflict_exec(bContext *C, wmOperator *op)
{
	Text *text = CTX_data_edit_text(C);
	int resolution = RNA_enum_get(op->ptr, "resolution");

	switch (resolution) {
		case RESOLVE_RELOAD:
			return text_reload_exec(C, op);
		case RESOLVE_SAVE:
			return text_save_exec(C, op);
		case RESOLVE_MAKE_INTERNAL:
			return text_make_internal_exec(C, op);
		case RESOLVE_IGNORE:
			BKE_text_file_modified_ignore(text);
			return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

static int text_resolve_conflict_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Text *text = CTX_data_edit_text(C);
	uiPopupMenu *pup;
	uiLayout *layout;

	switch (BKE_text_file_modified_check(text)) {
		case 1:
			if (text->flags & TXT_ISDIRTY) {
				/* modified locally and externally, ahhh. offer more possibilites. */
				pup = uiPupMenuBegin(C, IFACE_("File Modified Outside and Inside Blender"), ICON_NONE);
				layout = uiPupMenuLayout(pup);
				uiItemEnumO_ptr(layout, op->type, IFACE_("Reload from disk (ignore local changes)"),
				                0, "resolution", RESOLVE_RELOAD);
				uiItemEnumO_ptr(layout, op->type, IFACE_("Save to disk (ignore outside changes)"),
				                0, "resolution", RESOLVE_SAVE);
				uiItemEnumO_ptr(layout, op->type, IFACE_("Make text internal (separate copy)"),
				                0, "resolution", RESOLVE_MAKE_INTERNAL);
				uiPupMenuEnd(C, pup);
			}
			else {
				pup = uiPupMenuBegin(C, IFACE_("File Modified Outside Blender"), ICON_NONE);
				layout = uiPupMenuLayout(pup);
				uiItemEnumO_ptr(layout, op->type, IFACE_("Reload from disk"), 0, "resolution", RESOLVE_RELOAD);
				uiItemEnumO_ptr(layout, op->type, IFACE_("Make text internal (separate copy)"),
				                0, "resolution", RESOLVE_MAKE_INTERNAL);
				uiItemEnumO_ptr(layout, op->type, IFACE_("Ignore"), 0, "resolution", RESOLVE_IGNORE);
				uiPupMenuEnd(C, pup);
			}
			break;
		case 2:
			pup = uiPupMenuBegin(C, IFACE_("File Deleted Outside Blender"), ICON_NONE);
			layout = uiPupMenuLayout(pup);
			uiItemEnumO_ptr(layout, op->type, IFACE_("Make text internal"), 0, "resolution", RESOLVE_MAKE_INTERNAL);
			uiItemEnumO_ptr(layout, op->type, IFACE_("Recreate file"), 0, "resolution", RESOLVE_SAVE);
			uiPupMenuEnd(C, pup);
			break;
	}

	return OPERATOR_CANCELLED;
}

void TEXT_OT_resolve_conflict(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Resolve Conflict";
	ot->idname = "TEXT_OT_resolve_conflict";
	ot->description = "When external text is out of sync, resolve the conflict";

	/* api callbacks */
	ot->exec = text_resolve_conflict_exec;
	ot->invoke = text_resolve_conflict_invoke;
	ot->poll = text_save_poll;

	/* properties */
	RNA_def_enum(ot->srna, "resolution", resolution_items, RESOLVE_IGNORE, "Resolution", "How to solve conflict due to differences in internal and external text");
}

/********************** to 3d object operator *****************/

static int text_to_3d_object_exec(bContext *C, wmOperator *op)
{
	Text *text = CTX_data_edit_text(C);
	const bool split_lines = RNA_boolean_get(op->ptr, "split_lines");

	ED_text_to_object(C, text, split_lines);

	return OPERATOR_FINISHED;
}

void TEXT_OT_to_3d_object(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "To 3D Object";
	ot->idname = "TEXT_OT_to_3d_object";
	ot->description = "Create 3D text object from active text data block";
	
	/* api callbacks */
	ot->exec = text_to_3d_object_exec;
	ot->poll = text_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "split_lines", 0, "Split Lines", "Create one object per line in the text");
}


/************************ undo ******************************/

void ED_text_undo_step(bContext *C, int step)
{
	Text *text = CTX_data_edit_text(C);

	if (!text)
		return;

	if (step == 1)
		txt_do_undo(text);
	else if (step == -1)
		txt_do_redo(text);

	text_update_edited(text);

	text_update_cursor_moved(C);
	text_drawcache_tag_update(CTX_wm_space_text(C), 1);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);
}

