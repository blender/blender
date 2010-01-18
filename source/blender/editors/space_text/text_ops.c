/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h> /* ispunct */
#include <sys/stat.h>

#include "MEM_guardedalloc.h"

#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "PIL_time.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_suggestions.h"
#include "BKE_text.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_curve.h"
#include "ED_screen.h"
#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#ifndef DISABLE_PYTHON
#include "BPY_extern.h"
#endif

#include "text_intern.h"

/************************ poll ***************************/

static int text_new_poll(bContext *C)
{
	return 1;
}

static int text_edit_poll(bContext *C)
{
	Text *text= CTX_data_edit_text(C);

	if(!text)
		return 0;

	if(text->id.lib) {
		// BKE_report(op->reports, RPT_ERROR, "Can't edit external libdata");
		return 0;
	}

	return 1;
}

static int text_space_edit_poll(bContext *C)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= CTX_data_edit_text(C);

	if(!st || !text)
		return 0;

	if(text->id.lib) {
		// BKE_report(op->reports, RPT_ERROR, "Can't edit external libdata");
		return 0;
	}

	return 1;
}

static int text_region_edit_poll(bContext *C)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= CTX_data_edit_text(C);
	ARegion *ar= CTX_wm_region(C);

	if(!st || !text)
		return 0;
	
	if(!ar || ar->regiontype != RGN_TYPE_WINDOW)
		return 0;

	if(text->id.lib) {
		// BKE_report(op->reports, RPT_ERROR, "Can't edit external libdata");
		return 0;
	}

	return 1;
}


/********************** updates *********************/

void text_update_line_edited(Text *text, TextLine *line)
{
	if(!line)
		return;

	/* we just free format here, and let it rebuild during draw */
	if(line->format) {
		MEM_freeN(line->format);
		line->format= NULL;
	}
}

void text_update_edited(Text *text)
{
	TextLine *line;

	for(line=text->lines.first; line; line=line->next)
		text_update_line_edited(text, line);
}

/******************* new operator *********************/

static int new_exec(bContext *C, wmOperator *op)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text;
	PointerRNA ptr, idptr;
	PropertyRNA *prop;

	text= add_empty_text("Text");

	/* hook into UI */
	uiIDContextProperty(C, &ptr, &prop);

	if(prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		text->id.us--;

		RNA_id_pointer_create(&text->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}
	else if(st) {
		st->text= text;
		st->top= 0;
	}

	WM_event_add_notifier(C, NC_TEXT|NA_ADDED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New";
	ot->idname= "TEXT_OT_new";
	ot->description= "Create a new text data block.";
	
	/* api callbacks */
	ot->exec= new_exec;
	ot->poll= text_new_poll;
}

/******************* open operator *********************/

static void open_init(bContext *C, wmOperator *op)
{
	PropertyPointerRNA *pprop;

	op->customdata= pprop= MEM_callocN(sizeof(PropertyPointerRNA), "OpenPropertyPointerRNA");
	uiIDContextProperty(C, &pprop->ptr, &pprop->prop);
}

static int open_cancel(bContext *C, wmOperator *op)
{
	MEM_freeN(op->customdata);
	return OPERATOR_CANCELLED;
}

static int open_exec(bContext *C, wmOperator *op)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text;
	PropertyPointerRNA *pprop;
	PointerRNA idptr;
	char str[FILE_MAX];

	RNA_string_get(op->ptr, "path", str);

	text= add_text(str, G.sce);

	if(!text) {
		if(op->customdata) MEM_freeN(op->customdata);
		return OPERATOR_CANCELLED;
	}

	if(!op->customdata)
		open_init(C, op);

	/* hook into UI */
	pprop= op->customdata;

	if(pprop->prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		text->id.us--;

		RNA_id_pointer_create(&text->id, &idptr);
		RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr);
		RNA_property_update(C, &pprop->ptr, pprop->prop);
	}
	else if(st) {
		st->text= text;
		st->top= 0;
	}

	WM_event_add_notifier(C, NC_TEXT|NA_ADDED, text);

	MEM_freeN(op->customdata);

	return OPERATOR_FINISHED;
}

static int open_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Text *text= CTX_data_edit_text(C);
	char *path= (text && text->name)? text->name: G.sce;

	if(RNA_property_is_set(op->ptr, "path"))
		return open_exec(C, op);
	
	open_init(C, op);
	RNA_string_set(op->ptr, "path", path);
	WM_event_add_fileselect(C, op); 

	return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_open(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Open";
	ot->idname= "TEXT_OT_open";
	ot->description= "Open a new text data block.";

	/* api callbacks */
	ot->exec= open_exec;
	ot->invoke= open_invoke;
	ot->cancel= open_cancel;
	ot->poll= text_new_poll;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|TEXTFILE|PYSCRIPTFILE, FILE_SPECIAL);
}

/******************* reload operator *********************/

static int reload_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	if(!reopen_text(text)) {
		BKE_report(op->reports, RPT_ERROR, "Could not reopen file");
		return OPERATOR_CANCELLED;
	}

#ifndef DISABLE_PYTHON
	if(text->compiled)
		BPY_free_compiled_text(text);
#endif

	text_update_edited(text);
	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_reload(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reload";
	ot->idname= "TEXT_OT_reload";
	ot->description= "Reload active text data block from its file.";
	
	/* api callbacks */
	ot->exec= reload_exec;
	ot->invoke= WM_operator_confirm;
	ot->poll= text_edit_poll;
}

/******************* delete operator *********************/

static int unlink_exec(bContext *C, wmOperator *op)
{
	Main *bmain= CTX_data_main(C);
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= CTX_data_edit_text(C);

	/* make the previous text active, if its not there make the next text active */
	if(st) {
		if(text->id.prev) {
			st->text = text->id.prev;
			WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, st->text);
		}
		else if(text->id.next) {
			st->text = text->id.next;
			WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, st->text);
		}
	}

	unlink_text(bmain, text);
	free_libblock(&bmain->text, text);
	WM_event_add_notifier(C, NC_TEXT|NA_REMOVED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Unlink";
	ot->idname= "TEXT_OT_unlink";
	ot->description= "Unlink active text data block.";
	
	/* api callbacks */
	ot->exec= unlink_exec;
	ot->invoke= WM_operator_confirm;
	ot->poll= text_edit_poll;
}

/******************* make internal operator *********************/

static int make_internal_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	text->flags |= TXT_ISMEM | TXT_ISDIRTY;

	if(text->name) {
		MEM_freeN(text->name);
		text->name= NULL;
	}

	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_make_internal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make Internal";
	ot->idname= "TEXT_OT_make_internal";
	ot->description= "Make active text file internal.";

	/* api callbacks */
	ot->exec= make_internal_exec;
	ot->poll= text_edit_poll;
}

/******************* save operator *********************/

static int save_poll(bContext *C)
{
	Text *text= CTX_data_edit_text(C);

	if(!text_edit_poll(C))
		return 0;
	
	return (text->name != NULL && !(text->flags & TXT_ISMEM));
}

static void txt_write_file(Text *text, ReportList *reports) 
{
	FILE *fp;
	TextLine *tmp;
	struct stat st;
	int res;
	char file[FILE_MAXDIR+FILE_MAXFILE];
	
	BLI_strncpy(file, text->name, FILE_MAXDIR+FILE_MAXFILE);
	BLI_convertstringcode(file, G.sce);
	
	fp= fopen(file, "w");
	if(fp==NULL) {
		BKE_report(reports, RPT_ERROR, "Unable to save file.");
		return;
	}

	tmp= text->lines.first;
	while(tmp) {
		if(tmp->next) fprintf(fp, "%s\n", tmp->line);
		else fprintf(fp, "%s", tmp->line);
		
		tmp= tmp->next;
	}
	
	fclose (fp);

	res= stat(file, &st);
	text->mtime= st.st_mtime;
	
	if(text->flags & TXT_ISDIRTY)
		text->flags ^= TXT_ISDIRTY;
}

static int save_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	txt_write_file(text, op->reports);

	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_save(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Save";
	ot->idname= "TEXT_OT_save";
	ot->description= "Save active text data block.";

	/* api callbacks */
	ot->exec= save_exec;
	ot->poll= save_poll;
}

/******************* save as operator *********************/

static int save_as_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);
	char str[FILE_MAX];

	if(!text)
		return OPERATOR_CANCELLED;

	RNA_string_get(op->ptr, "path", str);

	if(text->name) MEM_freeN(text->name);
	text->name= BLI_strdup(str);
	text->flags &= ~TXT_ISMEM;

	txt_write_file(text, op->reports);

	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

static int save_as_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Text *text= CTX_data_edit_text(C);
	char *str;

	if(RNA_property_is_set(op->ptr, "path"))
		return save_as_exec(C, op);

	if(text->name)
		str= text->name;
	else if(text->flags & TXT_ISMEM)
		str= text->id.name+2;
	else
		str= G.sce;
	
	RNA_string_set(op->ptr, "path", str);
	WM_event_add_fileselect(C, op); 

	return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_save_as(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Save As";
	ot->idname= "TEXT_OT_save_as";
	ot->description= "Save active text file with options.";
	
	/* api callbacks */
	ot->exec= save_as_exec;
	ot->invoke= save_as_invoke;
	ot->poll= text_edit_poll;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|TEXTFILE|PYSCRIPTFILE, FILE_SPECIAL);
}

/******************* run script operator *********************/

static int run_script_poll(bContext *C)
{
	return (CTX_data_edit_text(C) != NULL);
}

static int run_script_exec(bContext *C, wmOperator *op)
{
#ifdef DISABLE_PYTHON
	BKE_report(op->reports, RPT_ERROR, "Python disabled in this build");

	return OPERATOR_CANCELLED;
#else
	Text *text= CTX_data_edit_text(C);
	SpaceText *st= CTX_wm_space_text(C);

	if (BPY_run_python_script(C, NULL, text, op->reports))
		return OPERATOR_FINISHED;
	
	/* Dont report error messages while live editing */
	if(!(st && st->live_edit))
		BKE_report(op->reports, RPT_ERROR, "Python script fail, look in the console for now...");
	
	return OPERATOR_CANCELLED;
#endif
}

void TEXT_OT_run_script(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Run Script";
	ot->idname= "TEXT_OT_run_script";
	ot->description= "Run active script.";
	
	/* api callbacks */
	ot->poll= run_script_poll;
	ot->exec= run_script_exec;
}


/******************* refresh pyconstraints operator *********************/

static int refresh_pyconstraints_exec(bContext *C, wmOperator *op)
{
#ifndef DISABLE_PYTHON
	Text *text= CTX_data_edit_text(C);
	Object *ob;
	bConstraint *con;
	short update;
	
	/* check all pyconstraints */
	for(ob= CTX_data_main(C)->object.first; ob; ob= ob->id.next) {
		update = 0;
		if(ob->type==OB_ARMATURE && ob->pose) {
			bPoseChannel *pchan;
			for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				for(con = pchan->constraints.first; con; con= con->next) {
					if(con->type==CONSTRAINT_TYPE_PYTHON) {
						bPythonConstraint *data = con->data;
						if(data->text==text) BPY_pyconstraint_update(ob, con);
						update = 1;
						
					}
				}
			}
		}
		for(con = ob->constraints.first; con; con= con->next) {
			if(con->type==CONSTRAINT_TYPE_PYTHON) {
				bPythonConstraint *data = con->data;
				if(data->text==text) BPY_pyconstraint_update(ob, con);
				update = 1;
			}
		}
		
		if(update) {
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		}
	}
#endif

	return OPERATOR_FINISHED;
}

void TEXT_OT_refresh_pyconstraints(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Refresh PyConstraints";
	ot->idname= "TEXT_OT_refresh_pyconstraints";
	ot->description= "Refresh all pyconstraints.";
	
	/* api callbacks */
	ot->exec= refresh_pyconstraints_exec;
	ot->poll= text_edit_poll;
}

/******************* paste operator *********************/

static char *txt_copy_selected(Text *text)
{
	TextLine *tmp, *linef, *linel;
	char *buf= NULL;
	int charf, charl, length= 0;
	
	if(!text) return NULL;
	if(!text->curl) return NULL;
	if(!text->sell) return NULL;

	if(!txt_has_sel(text)) return NULL;

	if(text->curl==text->sell) {
		linef= linel= text->curl;
		
		if(text->curc < text->selc) {
			charf= text->curc;
			charl= text->selc;
		}
		else{
			charf= text->selc;
			charl= text->curc;
		}
	}
	else if(txt_get_span(text->curl, text->sell)<0) {
		linef= text->sell;
		linel= text->curl;

		charf= text->selc;		
		charl= text->curc;
	}
	else {
		linef= text->curl;
		linel= text->sell;
		
		charf= text->curc;
		charl= text->selc;
	}

	if(linef == linel) {
		length= charl-charf;

		buf= MEM_callocN(length+1, "cut buffera");
		
		BLI_strncpy(buf, linef->line + charf, length+1);
	}
	else {
		length+= linef->len - charf;
		length+= charl;
		length++; /* For the '\n' */
		
		tmp= linef->next;
		while(tmp && tmp!= linel) {
			length+= tmp->len+1;
			tmp= tmp->next;
		}
		
		buf= MEM_callocN(length+1, "cut bufferb");
		
		strncpy(buf, linef->line+ charf, linef->len-charf);
		length= linef->len-charf;
		
		buf[length++]='\n';
		
		tmp= linef->next;
		while(tmp && tmp!=linel) {
			strncpy(buf+length, tmp->line, tmp->len);
			length+= tmp->len;
			
			buf[length++]='\n';			
			
			tmp= tmp->next;
		}
		strncpy(buf+length, linel->line, charl);
		length+= charl;
		
		buf[length]=0;
	}

	return buf;
}

static int paste_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);
	char *buf;
	int selection= RNA_boolean_get(op->ptr, "selection");

	buf= WM_clipboard_text_get(selection);

	if(!buf)
		return OPERATOR_CANCELLED;

	txt_insert_buf(text, buf);
	text_update_edited(text);

	MEM_freeN(buf);

	WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	/* run the script while editing, evil but useful */
	if(CTX_wm_space_text(C)->live_edit)
		run_script_exec(C, op);
	
	return OPERATOR_FINISHED;
}

void TEXT_OT_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Paste";
	ot->idname= "TEXT_OT_paste";
	ot->description= "Paste text from clipboard.";
	
	/* api callbacks */
	ot->exec= paste_exec;
	ot->poll= text_edit_poll;

	/* properties */
	RNA_def_boolean(ot->srna, "selection", 0, "Selection", "Paste text selected elsewhere rather than copied, X11 only.");
}

/******************* copy operator *********************/

static void txt_copy_clipboard(Text *text)
{
	char *buf;

	buf= txt_copy_selected(text);

	if(buf) {
		WM_clipboard_text_set(buf, 0);
		MEM_freeN(buf);
	}
}

static int copy_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	txt_copy_clipboard(text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy";
	ot->idname= "TEXT_OT_copy";
	ot->description= "Copy selected text to clipboard.";

	/* api callbacks */
	ot->exec= copy_exec;
	ot->poll= text_edit_poll;
}

/******************* cut operator *********************/

static int cut_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	txt_copy_clipboard(text);
	txt_delete_selected(text);

	WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	/* run the script while editing, evil but useful */
	if(CTX_wm_space_text(C)->live_edit)
		run_script_exec(C, op);
	
	return OPERATOR_FINISHED;
}

void TEXT_OT_cut(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Cut";
	ot->idname= "TEXT_OT_cut";
	ot->description= "Cut selected text to clipboard.";
	
	/* api callbacks */
	ot->exec= cut_exec;
	ot->poll= text_edit_poll;
}

/******************* indent operator *********************/

static int indent_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	if(txt_has_sel(text)) {
		txt_order_cursors(text);
		indent(text);
	}
	else
		txt_add_char(text, '\t');

	text_update_edited(text);

	WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_indent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Indent";
	ot->idname= "TEXT_OT_indent";
	ot->description= "Indent selected text.";
	
	/* api callbacks */
	ot->exec= indent_exec;
	ot->poll= text_edit_poll;
}

/******************* unindent operator *********************/

static int unindent_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	if(txt_has_sel(text)) {
		txt_order_cursors(text);
		unindent(text);

		text_update_edited(text);

		WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
		WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void TEXT_OT_unindent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Unindent";
	ot->idname= "TEXT_OT_unindent";
	ot->description= "Unindent selected text.";
	
	/* api callbacks */
	ot->exec= unindent_exec;
	ot->poll= text_edit_poll;
}

/******************* line break operator *********************/

static int line_break_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);
	int a, curtab;

	// double check tabs before splitting the line
	curtab= setcurr_tab(text);
	txt_split_curline(text);

	for(a=0; a < curtab; a++)
		txt_add_char(text, '\t');

	if(text->curl) {
		if(text->curl->prev)
			text_update_line_edited(text, text->curl->prev);
		text_update_line_edited(text, text->curl);
	}

	WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_CANCELLED;
}

void TEXT_OT_line_break(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Line Break";
	ot->idname= "TEXT_OT_line_break";
	ot->description= "Insert line break at cursor position.";
	
	/* api callbacks */
	ot->exec= line_break_exec;
	ot->poll= text_edit_poll;
}

/******************* comment operator *********************/

static int comment_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	if(txt_has_sel(text)) {
		txt_order_cursors(text);
		comment(text);
		text_update_edited(text);

		WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void TEXT_OT_comment(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Comment";
	ot->idname= "TEXT_OT_comment";
	ot->description= "Convert selected text to comment.";
	
	/* api callbacks */
	ot->exec= comment_exec;
	ot->poll= text_edit_poll;
}

/******************* uncomment operator *********************/

static int uncomment_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	if(txt_has_sel(text)) {
		txt_order_cursors(text);
		uncomment(text);
		text_update_edited(text);

		WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void TEXT_OT_uncomment(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Uncomment";
	ot->idname= "TEXT_OT_uncomment";
	ot->description= "Convert selected comment to text.";
	
	/* api callbacks */
	ot->exec= uncomment_exec;
	ot->poll= text_edit_poll;
}

/******************* convert whitespace operator *********************/

enum { TO_SPACES, TO_TABS };
static EnumPropertyItem whitespace_type_items[]= {
	{TO_SPACES, "SPACES", 0, "To Spaces", NULL},
	{TO_TABS, "TABS", 0, "To Tabs", NULL},
	{0, NULL, 0, NULL, NULL}};

static int convert_whitespace_exec(bContext *C, wmOperator *op)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= CTX_data_edit_text(C);
	TextLine *tmp;
	FlattenString fs;
	size_t a, j;
	char *text_check_line, *new_line;
	int extra, number; //unknown for now
	int type= RNA_enum_get(op->ptr, "type");
	
	tmp = text->lines.first;
	
	//first convert to all space, this make it alot easier to convert to tabs because there is no mixtures of ' ' && '\t'
	while(tmp) {
		text_check_line = tmp->line;
		number = flatten_string(st, &fs, text_check_line)+1;
		flatten_string_free(&fs);
		new_line = MEM_callocN(number, "Converted_Line");
		j = 0;
		for(a=0; a < strlen(text_check_line); a++) { //foreach char in line
			if(text_check_line[a] == '\t') { //checking for tabs
				//get the number of spaces this tabs is showing
				//i dont like doing it this way but will look into it later
				new_line[j] = '\0';
				number = flatten_string(st, &fs, new_line);
				flatten_string_free(&fs);
				new_line[j] = '\t';
				new_line[j+1] = '\0';
				number = flatten_string(st, &fs, new_line)-number;
				flatten_string_free(&fs);

				for(extra = 0; extra < number; extra++) {
					new_line[j] = ' ';
					j++;
				}
			}
			else {
				new_line[j] = text_check_line[a];
				++j;
			}
		}
		new_line[j] = '\0';
		// put new_line in the tmp->line spot still need to try and set the curc correctly
		if(tmp->line) MEM_freeN(tmp->line);
		if(tmp->format) MEM_freeN(tmp->format);
		
		tmp->line = new_line;
		tmp->len = strlen(new_line);
		tmp->format = NULL;
		tmp = tmp->next;
	}
	
	if(type == TO_TABS) // Converting to tabs
	{	//start over from the begining
		tmp = text->lines.first;
		
		while(tmp) {
			text_check_line = tmp->line;
			extra = 0;
			for(a = 0; a < strlen(text_check_line); a++) {
				number = 0;
				for(j = 0; j < (size_t)st->tabnumber; j++) {
					if((a+j) <= strlen(text_check_line)) { //check to make sure we are not pass the end of the line
						if(text_check_line[a+j] != ' ') {
							number = 1;
						}
					}
				}
				if(!number) { //found all number of space to equal a tab
					a = a+(st->tabnumber-1);
					extra = extra+1;
				}
			}
			
			if( extra > 0 ) { //got tabs make malloc and do what you have to do
				new_line = MEM_callocN(strlen(text_check_line)-(((st->tabnumber*extra)-extra)-1), "Converted_Line");
				extra = 0; //reuse vars
				for(a = 0; a < strlen(text_check_line); a++) {
					number = 0;
					for(j = 0; j < (size_t)st->tabnumber; j++) {
						if((a+j) <= strlen(text_check_line)) { //check to make sure we are not pass the end of the line
							if(text_check_line[a+j] != ' ') {
								number = 1;
							}
						}
					}

					if(!number) { //found all number of space to equal a tab
						new_line[extra] = '\t';
						a = a+(st->tabnumber-1);
						++extra;
						
					}
					else { //not adding a tab
						new_line[extra] = text_check_line[a];
						++extra;
					}
				}
				new_line[extra] = '\0';
				// put new_line in the tmp->line spot still need to try and set the curc correctly
				if(tmp->line) MEM_freeN(tmp->line);
				if(tmp->format) MEM_freeN(tmp->format);
				
				tmp->line = new_line;
				tmp->len = strlen(new_line);
				tmp->format = NULL;
			}
			tmp = tmp->next;
		}
	}

	text_update_edited(text);

	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_convert_whitespace(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Convert Whitespace";
	ot->idname= "TEXT_OT_convert_whitespace";
	ot->description= "Convert whitespaces by type.";
	
	/* api callbacks */
	ot->exec= convert_whitespace_exec;
	ot->poll= text_edit_poll;

	/* properties */
	RNA_def_enum(ot->srna, "type", whitespace_type_items, TO_SPACES, "type", "Type of whitespace to convert to.");
}

/******************* select all operator *********************/

static int select_all_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	txt_sel_all(text);

	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select All";
	ot->idname= "TEXT_OT_select_all";
	ot->description= "Select all text.";
	
	/* api callbacks */
	ot->exec= select_all_exec;
	ot->poll= text_edit_poll;
}

/******************* select line operator *********************/

static int select_line_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	txt_sel_line(text);

	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_select_line(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Line";
	ot->idname= "TEXT_OT_select_line";
	ot->description= "Select text by line.";
	
	/* api clinebacks */
	ot->exec= select_line_exec;
	ot->poll= text_edit_poll;
}

/******************* previous marker operator *********************/

static int previous_marker_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);
	TextMarker *mrk;
	int lineno;

	lineno= txt_get_span(text->lines.first, text->curl);
	mrk= text->markers.last;
	while(mrk && (mrk->lineno>lineno || (mrk->lineno==lineno && mrk->end > text->curc)))
		mrk= mrk->prev;
	if(!mrk) mrk= text->markers.last;
	if(mrk) {
		txt_move_to(text, mrk->lineno, mrk->start, 0);
		txt_move_to(text, mrk->lineno, mrk->end, 1);
	}

	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_previous_marker(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Previous Marker";
	ot->idname= "TEXT_OT_previous_marker";
	ot->description= "Move to previous marker.";
	
	/* api callbacks */
	ot->exec= previous_marker_exec;
	ot->poll= text_edit_poll;
}

/******************* next marker operator *********************/

static int next_marker_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);
	TextMarker *mrk;
	int lineno;

	lineno= txt_get_span(text->lines.first, text->curl);
	mrk= text->markers.first;
	while(mrk && (mrk->lineno<lineno || (mrk->lineno==lineno && mrk->start <= text->curc)))
		mrk= mrk->next;
	if(!mrk) mrk= text->markers.first;
	if(mrk) {
		txt_move_to(text, mrk->lineno, mrk->start, 0);
		txt_move_to(text, mrk->lineno, mrk->end, 1);
	}

	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_next_marker(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Next Marker";
	ot->idname= "TEXT_OT_next_marker";
	ot->description= "Move to next marker";
	
	/* api callbacks */
	ot->exec= next_marker_exec;
	ot->poll= text_edit_poll;
}

/******************* clear all markers operator *********************/

static int clear_all_markers_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);

	txt_clear_markers(text, 0, 0);

	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_markers_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear All Markers";
	ot->idname= "TEXT_OT_markers_clear";
	ot->description= "Clear all markers.";
	
	/* api callbacks */
	ot->exec= clear_all_markers_exec;
	ot->poll= text_edit_poll;
}

/************************ move operator ************************/

static EnumPropertyItem move_type_items[]= {
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

static void wrap_move_bol(SpaceText *st, ARegion *ar, short sel)
{
	Text *text= st->text;
	int offl, offc, lin;

	text_update_character_width(st);

	lin= txt_get_span(text->lines.first, text->sell);
	wrap_offset(st, ar, text->sell, text->selc, &offl, &offc);

	if (sel) {
		txt_undo_add_toop(text, UNDO_STO, lin, text->selc, lin, -offc);
		text->selc= -offc;
	} else {
		txt_undo_add_toop(text, UNDO_CTO, lin, text->curc, lin, -offc);
		text->curc= -offc;
		txt_pop_sel(text);
	}
}

static void wrap_move_eol(SpaceText *st, ARegion *ar, short sel)
{
	Text *text= st->text;
	int offl, offc, lin, startl, c;

	text_update_character_width(st);

	lin= txt_get_span(text->lines.first, text->sell);
	wrap_offset(st, ar, text->sell, text->selc, &offl, &offc);
	startl= offl;
	c= text->selc;
	while (offl==startl && text->sell->line[c]!='\0') {
		c++;
		wrap_offset(st, ar, text->sell, c, &offl, &offc);
	} if (offl!=startl) c--;

	if (sel) {
		txt_undo_add_toop(text, UNDO_STO, lin, text->selc, lin, c);
		text->selc= c;
	} else {
		txt_undo_add_toop(text, UNDO_CTO, lin, text->curc, lin, c);
		text->curc= c;
		txt_pop_sel(text);
	}
}

static void wrap_move_up(SpaceText *st, ARegion *ar, short sel)
{
	Text *text= st->text;
	int offl, offl_1, offc, fromline, toline, c, target;

	text_update_character_width(st);

	wrap_offset(st, ar, text->sell, 0, &offl_1, &offc);
	wrap_offset(st, ar, text->sell, text->selc, &offl, &offc);
	fromline= toline= txt_get_span(text->lines.first, text->sell);
	target= text->selc + offc;

	if (offl==offl_1) {
		if (!text->sell->prev) {
			txt_move_bol(text, sel);
			return;
		}
		toline--;
		c= text->sell->prev->len; /* End of prev. line */
		wrap_offset(st, ar, text->sell->prev, c, &offl, &offc);
		c= -offc+target;
	} else {
		c= -offc-1; /* End of prev. line */
		wrap_offset(st, ar, text->sell, c, &offl, &offc);
		c= -offc+target;
	}
	if (c<0) c=0;

	if (sel) {
		txt_undo_add_toop(text, UNDO_STO, fromline, text->selc, toline, c);
		if (toline<fromline) text->sell= text->sell->prev;
		if(text->sell) {
			if (c>text->sell->len) c= text->sell->len;
			text->selc= c;
		}
	} 
	else if(text->curl) {
		txt_undo_add_toop(text, UNDO_CTO, fromline, text->curc, toline, c);
		if (toline<fromline) text->curl= text->curl->prev;
		if(text->curl) {
			if (c>text->curl->len) c= text->curl->len;
			text->curc= c;
			txt_pop_sel(text);
		}
	}
}

static void wrap_move_down(SpaceText *st, ARegion *ar, short sel)
{
	Text *text= st->text;
	int offl, startoff, offc, fromline, toline, c, target;

	text_update_character_width(st);

	wrap_offset(st, ar, text->sell, text->selc, &offl, &offc);
	fromline= toline= txt_get_span(text->lines.first, text->sell);
	target= text->selc + offc;
	startoff= offl;
	c= text->selc;
	while (offl==startoff && text->sell->line[c]!='\0') {
		c++;
		wrap_offset(st, ar, text->sell, c, &offl, &offc);
	}

	if (text->sell->line[c]=='\0') {
		if (!text->sell->next) {
			txt_move_eol(text, sel);
			return;
		}
		toline++;
		c= target;
	} else {
		c += target;
		if (c > text->sell->len) c= text->sell->len;
	}
	if (c<0) c=0;

	if (sel) {
		txt_undo_add_toop(text, UNDO_STO, fromline, text->selc, toline, c);
		if (toline>fromline) text->sell= text->sell->next;
		if(text->sell) {
			if (c>text->sell->len) c= text->sell->len;
			text->selc= c;
		}
	} 
	else if(text->curl) {
		txt_undo_add_toop(text, UNDO_CTO, fromline, text->curc, toline, c);
		if (toline>fromline) text->curl= text->curl->next;
		if(text->curl) {
			if (c > text->curl->len) c= text->curl->len;
			text->curc= c;
			txt_pop_sel(text);
		}
	}
}

/* Moves the cursor vertically by the specified number of lines.
 If the destination line is shorter than the current cursor position, the
 cursor will be positioned at the end of this line.

 This is to replace screen_skip for PageUp/Down operations.
 */
static void cursor_skip(Text *text, int lines, int sel)
{
	TextLine **linep;
	int oldl, oldc, *charp;
	
	if (sel) linep= &text->sell, charp= &text->selc;
	else linep= &text->curl, charp= &text->curc;
	oldl= txt_get_span(text->lines.first, *linep);
	oldc= *charp;

	while (lines>0 && (*linep)->next) {
		*linep= (*linep)->next;
		lines--;
	}
	while (lines<0 && (*linep)->prev) {
		*linep= (*linep)->prev;
		lines++;
	}

	if (*charp > (*linep)->len) *charp= (*linep)->len;

	if (!sel) txt_pop_sel(text);
	txt_undo_add_toop(text, sel?UNDO_STO:UNDO_CTO, oldl, oldc, txt_get_span(text->lines.first, *linep), *charp);
}

static int move_cursor(bContext *C, int type, int select)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= CTX_data_edit_text(C);
	ARegion *ar= CTX_wm_region(C);

	/* ensure we have the right region, it's optional */
	if(ar && ar->regiontype != RGN_TYPE_WINDOW)
		ar= NULL;

	switch(type) {
		case LINE_BEGIN:
			if(st && st->wordwrap && ar) wrap_move_bol(st, ar, select);
			else txt_move_bol(text, select);
			break;
			
		case LINE_END:
			if(st && st->wordwrap && ar) wrap_move_eol(st, ar, select);
			else txt_move_eol(text, select);
			break;

		case FILE_TOP:
			txt_move_bof(text, select);
			break;
			
		case FILE_BOTTOM:
			txt_move_eof(text, select);
			break;

		case PREV_WORD:
			txt_jump_left(text, select);
			break;

		case NEXT_WORD:
			txt_jump_right(text, select);
			break;

		case PREV_CHAR:
			txt_move_left(text, select);
			break;

		case NEXT_CHAR:	
			txt_move_right(text, select);
			break;

		case PREV_LINE:
			if(st && st->wordwrap && ar) wrap_move_up(st, ar, select);
			else txt_move_up(text, select);
			break;
			
		case NEXT_LINE:
			if(st && st->wordwrap && ar) wrap_move_down(st, ar, select);
			else txt_move_down(text, select);
			break;

		case PREV_PAGE:
			if(st) cursor_skip(text, -st->viewlines, select);
			else cursor_skip(text, -10, select);
			break;

		case NEXT_PAGE:
			if(st) cursor_skip(text, st->viewlines, select);
			else cursor_skip(text, 10, select);
			break;
	}

	WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

static int move_exec(bContext *C, wmOperator *op)
{
	int type= RNA_enum_get(op->ptr, "type");

	return move_cursor(C, type, 0);
}

void TEXT_OT_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Cursor";
	ot->idname= "TEXT_OT_move";
	ot->description= "Move cursor to position type.";
	
	/* api callbacks */
	ot->exec= move_exec;
	ot->poll= text_edit_poll;

	/* properties */
	RNA_def_enum(ot->srna, "type", move_type_items, LINE_BEGIN, "Type", "Where to move cursor to.");
}

/******************* move select operator ********************/

static int move_select_exec(bContext *C, wmOperator *op)
{
	int type= RNA_enum_get(op->ptr, "type");

	return move_cursor(C, type, 1);
}

void TEXT_OT_move_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Select";
	ot->idname= "TEXT_OT_move_select";
	ot->description= "Make selection from current cursor position to new cursor position type.";
	
	/* api callbacks */
	ot->exec= move_select_exec;
	ot->poll= text_space_edit_poll;

	/* properties */
	RNA_def_enum(ot->srna, "type", move_type_items, LINE_BEGIN, "Type", "Where to move cursor to, to make a selection.");
}

/******************* jump operator *********************/

static int jump_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);
	int line= RNA_int_get(op->ptr, "line");
	short nlines= txt_get_span(text->lines.first, text->lines.last)+1;

	if(line < 1 || line > nlines)
		return OPERATOR_CANCELLED;

	txt_move_toline(text, line-1, 0);

	WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_jump(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Jump";
	ot->idname= "TEXT_OT_jump";
	ot->description= "Jump cursor to line.";
	
	/* api callbacks */
	ot->invoke=  WM_operator_props_popup;
	ot->exec= jump_exec;
	ot->poll= text_edit_poll;

	/* properties */
	RNA_def_int(ot->srna, "line", 1, 1, INT_MAX, "Line", "Line number to jump to.", 1, 10000);
}

/******************* delete operator **********************/

static EnumPropertyItem delete_type_items[]= {
	{DEL_NEXT_CHAR, "NEXT_CHARACTER", 0, "Next Character", ""},
	{DEL_PREV_CHAR, "PREVIOUS_CHARACTER", 0, "Previous Character", ""},
	{DEL_NEXT_WORD, "NEXT_WORD", 0, "Next Word", ""},
	{DEL_PREV_WORD, "PREVIOUS_WORD", 0, "Previous Word", ""},
	{0, NULL, 0, NULL, NULL}};

static int delete_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);
	int type= RNA_enum_get(op->ptr, "type");

	if(type == DEL_PREV_WORD)
		txt_backspace_word(text);
	else if(type == DEL_PREV_CHAR)
		txt_backspace_char(text);
	else if(type == DEL_NEXT_WORD)
		txt_delete_word(text);
	else if(type == DEL_NEXT_CHAR)
		txt_delete_char(text);

	text_update_line_edited(text, text->curl);

	WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	/* run the script while editing, evil but useful */
	if(CTX_wm_space_text(C)->live_edit)
		run_script_exec(C, op);
	
	return OPERATOR_FINISHED;
}

void TEXT_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete";
	ot->idname= "TEXT_OT_delete";
	ot->description= "Delete text by cursor position.";
	
	/* api callbacks */
	ot->exec= delete_exec;
	ot->poll= text_edit_poll;

	/* properties */
	RNA_def_enum(ot->srna, "type", delete_type_items, DEL_NEXT_CHAR, "Type", "Which part of the text to delete.");
}

/******************* toggle overwrite operator **********************/

static int toggle_overwrite_exec(bContext *C, wmOperator *op)
{
	SpaceText *st= CTX_wm_space_text(C);

	st->overwrite= !st->overwrite;

	return OPERATOR_FINISHED;
}

void TEXT_OT_overwrite_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Overwrite";
	ot->idname= "TEXT_OT_overwrite_toggle";
	ot->description= "Toggle overwrite while typing.";
	
	/* api callbacks */
	ot->exec= toggle_overwrite_exec;
	ot->poll= text_space_edit_poll;
}

/******************* scroll operator **********************/

/* Moves the view vertically by the specified number of lines */
static void screen_skip(SpaceText *st, int lines)
{
	int last;

 	st->top += lines;

	last= txt_get_span(st->text->lines.first, st->text->lines.last);
	last= last - (st->viewlines/2);
	
	if(st->top>last) st->top= last;
	if(st->top<0) st->top= 0;
}

typedef struct TextScroll {
	short old[2];
	short hold[2];
	short delta[2];

	int first;
	int characters;
	int lines;
	int scrollbar;
} TextScroll;

static int scroll_exec(bContext *C, wmOperator *op)
{
	SpaceText *st= CTX_wm_space_text(C);
	int lines= RNA_int_get(op->ptr, "lines");

	if(lines == 0)
		return OPERATOR_CANCELLED;

	screen_skip(st, lines*U.wheellinescroll);

	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
}

static void scroll_apply(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceText *st= CTX_wm_space_text(C);
	TextScroll *tsc= op->customdata;
	short *mval= event->mval;

	text_update_character_width(st);

	if(tsc->first) {
		tsc->old[0]= mval[0];
		tsc->old[1]= mval[1];
		tsc->hold[0]= mval[0];
		tsc->hold[1]= mval[1];
		tsc->first= 0;
	}

	if(!tsc->scrollbar) {
		tsc->delta[0]= (tsc->hold[0]-mval[0])/st->cwidth;
		tsc->delta[1]= (mval[1]-tsc->hold[1])/st->lheight;
	}
	else
		tsc->delta[1]= (tsc->hold[1]-mval[1])*st->pix_per_line;
	
	if(tsc->delta[0] || tsc->delta[1]) {
		screen_skip(st, tsc->delta[1]);

		tsc->lines += tsc->delta[1];

		if(st->wordwrap) {
			st->left= 0;
		}
		else {
			st->left+= tsc->delta[0];
			if(st->left<0) st->left= 0;
		}
		
		tsc->hold[0]= mval[0];
		tsc->hold[1]= mval[1];

		ED_area_tag_redraw(CTX_wm_area(C));
	}

	tsc->old[0]= mval[0];
	tsc->old[1]= mval[1];
}

static void scroll_exit(bContext *C, wmOperator *op)
{
	SpaceText *st= CTX_wm_space_text(C);

	st->flags &= ~ST_SCROLL_SELECT;
	MEM_freeN(op->customdata);
}

static int scroll_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	switch(event->type) {
		case MOUSEMOVE:
			scroll_apply(C, op, event);
			break;
		case LEFTMOUSE:
		case RIGHTMOUSE:
		case MIDDLEMOUSE:
			scroll_exit(C, op);
			return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int scroll_cancel(bContext *C, wmOperator *op)
{
	scroll_exit(C, op);

	return OPERATOR_CANCELLED;
}

static int scroll_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceText *st= CTX_wm_space_text(C);
	TextScroll *tsc;
	
	if(RNA_property_is_set(op->ptr, "lines"))
		return scroll_exec(C, op);
	
	tsc= MEM_callocN(sizeof(TextScroll), "TextScroll");
	tsc->first= 1;
	op->customdata= tsc;
	
	st->flags|= ST_SCROLL_SELECT;
	
	if (event->type == MOUSEPAN) {
		text_update_character_width(st);
		
		tsc->hold[0] = event->prevx;
		tsc->hold[1] = event->prevy;
		/* Sensitivity of scroll set to 4pix per line/char */
		event->mval[0] = event->prevx + (event->x - event->prevx)*st->cwidth/4;
		event->mval[1] = event->prevy + (event->y - event->prevy)*st->lheight/4;
		tsc->first = 0;
		tsc->scrollbar = 0;
		scroll_apply(C, op, event);
		scroll_exit(C, op);
		return OPERATOR_FINISHED;
	}	
	
	WM_event_add_modal_handler(C, op);
	
	return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_scroll(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Scroll";
    /*don't really see the difference between this and
      scroll_bar. Both do basically the same thing (aside 
      from keymaps).*/
	ot->idname= "TEXT_OT_scroll";
	ot->description= "Scroll text screen.";
	
	/* api callbacks */
	ot->exec= scroll_exec;
	ot->invoke= scroll_invoke;
	ot->modal= scroll_modal;
	ot->cancel= scroll_cancel;
	ot->poll= text_space_edit_poll;

	/* flags */
	ot->flag= OPTYPE_BLOCKING;

	/* properties */
	RNA_def_int(ot->srna, "lines", 1, INT_MIN, INT_MAX, "Lines", "Number of lines to scroll.", -100, 100);
}

/******************** scroll bar operator *******************/

static int scroll_bar_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceText *st= CTX_wm_space_text(C);
	ARegion *ar= CTX_wm_region(C);
	TextScroll *tsc;
	short *mval= event->mval;

	if(RNA_property_is_set(op->ptr, "lines"))
		return scroll_exec(C, op);
	
	/* verify we are in the right zone */
	if(!(mval[0]>ar->winx-20 && mval[0]<ar->winx-2 && mval[1]>2 && mval[1]<ar->winy))
		return OPERATOR_PASS_THROUGH;

	tsc= MEM_callocN(sizeof(TextScroll), "TextScroll");
	tsc->first= 1;
	tsc->scrollbar= 1;
	op->customdata= tsc;
	
	st->flags|= ST_SCROLL_SELECT;

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

void TEXT_OT_scroll_bar(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Scrollbar";
    /*don't really see the difference between this and
      scroll. Both do basically the same thing (aside 
      from keymaps).*/
	ot->idname= "TEXT_OT_scroll_bar";
	ot->description= "Scroll text screen.";
	
	/* api callbacks */
	ot->invoke= scroll_bar_invoke;
	ot->modal= scroll_modal;
	ot->cancel= scroll_cancel;
	ot->poll= text_region_edit_poll;

	/* flags */
	ot->flag= OPTYPE_BLOCKING;

	/* properties */
	RNA_def_int(ot->srna, "lines", 1, INT_MIN, INT_MAX, "Lines", "Number of lines to scroll.", -100, 100);
}

/******************* set cursor operator **********************/

typedef struct SetCursor {
	int selecting;
	int selc, sell;
	short old[2];
} SetCursor;

static void set_cursor_to_pos(SpaceText *st, ARegion *ar, int x, int y, int sel) 
{
	FlattenString fs;
	Text *text= st->text;
	TextLine **linep;
	int *charp;
	int w;

	text_update_character_width(st);

	if(sel) { linep= &text->sell; charp= &text->selc; } 
	else { linep= &text->curl; charp= &text->curc; }
	
	y= (ar->winy - y)/st->lheight;

	if(st->showlinenrs)
		x-= TXT_OFFSET+TEXTXLOC;
	else
		x-= TXT_OFFSET;

	if(x<0) x= 0;
	x = (x/st->cwidth) + st->left;
	
	if(st->wordwrap) {
		int i, j, endj, curs, max, chop, start, end, chars, loop;
		char ch;

		/* Point to first visible line */
		*linep= text->lines.first;
		for(i=0; i<st->top && (*linep)->next; i++) *linep= (*linep)->next;

		max= wrap_width(st, ar);

		loop= 1;
		while(loop && *linep) {
			start= 0;
			end= max;
			chop= 1;
			chars= 0;
			curs= 0;
			endj= 0;
			for(i=0, j=0; loop; j++) {

				/* Mimic replacement of tabs */
				ch= (*linep)->line[j];
				if(ch=='\t') {
					chars= st->tabnumber-i%st->tabnumber;
					ch= ' ';
				}
				else
					chars= 1;

				while(chars--) {
					/* Gone too far, go back to last wrap point */
					if(y<0) {
						*charp= endj;
						loop= 0;
						break;
					/* Exactly at the cursor, done */
					}
					else if(y==0 && i-start==x) {
						*charp= curs= j;
						loop= 0;
						break;
					/* Prepare curs for next wrap */
					}
					else if(i-end==x) {
						curs= j;
					}
					if(i-start>=max) {
						if(chop) endj= j;
						y--;
						start= end;
						end += max;
						chop= 1;
						if(y==0 && i-start>=x) {
							*charp= curs;
							loop= 0;
							break;
						}
					}
					else if(ch==' ' || ch=='-' || ch=='\0') {
						if(y==0 && i-start>=x) {
							*charp= curs;
							loop= 0;
							break;
						}
						end = i+1;
						endj = j;
						chop= 0;
					}
					i++;
				}
				if(ch=='\0') break;
			}
			if(!loop || y<0) break;

			if(!(*linep)->next) {
				*charp= (*linep)->len;
				break;
			}
			
			/* On correct line but didn't meet cursor, must be at end */
			if(y==0) {
				*charp= (*linep)->len;
				break;
			}
			*linep= (*linep)->next;
			y--;
		}

	}
	else {
		y-= txt_get_span(text->lines.first, *linep) - st->top;
		
		if(y>0) {
			while(y-- != 0) if((*linep)->next) *linep= (*linep)->next;
		}
		else if(y<0) {
			while(y++ != 0) if((*linep)->prev) *linep= (*linep)->prev;
		}

		
		w= flatten_string(st, &fs, (*linep)->line);
		if(x<w) *charp= fs.accum[x];
		else *charp= (*linep)->len;
		flatten_string_free(&fs);
	}
	if(!sel) txt_pop_sel(text);
}

static void set_cursor_apply(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceText *st= CTX_wm_space_text(C);
	ARegion *ar= CTX_wm_region(C);
	SetCursor *scu= op->customdata;

	if(event->mval[1]<0 || event->mval[1]>ar->winy) {
		int d= (scu->old[1]-event->mval[1])*st->pix_per_line;
		if(d) screen_skip(st, d);

		set_cursor_to_pos(st, ar, event->mval[0], event->mval[1]<0?0:ar->winy, 1);

		WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, st->text);
	} 
	else if(!st->wordwrap && (event->mval[0]<0 || event->mval[0]>ar->winx)) {
		if(event->mval[0]>ar->winx) st->left++;
		else if(event->mval[0]<0 && st->left>0) st->left--;
		
		set_cursor_to_pos(st, ar, event->mval[0], event->mval[1], 1);
		
		WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, st->text);
		// XXX PIL_sleep_ms(10);
	} 
	else {
		set_cursor_to_pos(st, ar, event->mval[0], event->mval[1], 1);

		WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, st->text);

		scu->old[0]= event->mval[0];
		scu->old[1]= event->mval[1];
	} 
}

static void set_cursor_exit(bContext *C, wmOperator *op)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= st->text;
	SetCursor *scu= op->customdata;
	int linep2, charp2;
	char *buffer;

	if(txt_has_sel(text)) {
		buffer = txt_sel_to_buf(text);
		WM_clipboard_text_set(buffer, 1);
		MEM_freeN(buffer);
	}

	linep2= txt_get_span(st->text->lines.first, st->text->sell);
	charp2= st->text->selc;
		
	if(scu->sell!=linep2 || scu->selc!=charp2)
		txt_undo_add_toop(st->text, UNDO_STO, scu->sell, scu->selc, linep2, charp2);

	WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, st->text);

	MEM_freeN(scu);
}

static int set_cursor_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceText *st= CTX_wm_space_text(C);
	ARegion *ar= CTX_wm_region(C);
	SetCursor *scu;

	op->customdata= MEM_callocN(sizeof(SetCursor), "SetCursor");
	scu= op->customdata;
	scu->selecting= RNA_boolean_get(op->ptr, "select");

	scu->old[0]= event->mval[0];
	scu->old[1]= event->mval[1];

	if(!scu->selecting) {
		int curl= txt_get_span(st->text->lines.first, st->text->curl);
		int curc= st->text->curc;			
		int linep2, charp2;
					
		set_cursor_to_pos(st, ar, event->mval[0], event->mval[1], 0);

		linep2= txt_get_span(st->text->lines.first, st->text->curl);
		charp2= st->text->selc;
				
		if(curl!=linep2 || curc!=charp2)
			txt_undo_add_toop(st->text, UNDO_CTO, curl, curc, linep2, charp2);
	}

	scu->sell= txt_get_span(st->text->lines.first, st->text->sell);
	scu->selc= st->text->selc;

	WM_event_add_modal_handler(C, op);

	set_cursor_apply(C, op, event);

	return OPERATOR_RUNNING_MODAL;
}

static int set_cursor_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	switch(event->type) {
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			set_cursor_exit(C, op);
			return OPERATOR_FINISHED;
		case MOUSEMOVE:
			set_cursor_apply(C, op, event);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int set_cursor_cancel(bContext *C, wmOperator *op)
{
	set_cursor_exit(C, op);
	return OPERATOR_FINISHED;
}

void TEXT_OT_cursor_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Cursor";
	ot->idname= "TEXT_OT_cursor_set";
	ot->description= "Set cursor selection.";
	
	/* api callbacks */
	ot->invoke= set_cursor_invoke;
	ot->modal= set_cursor_modal;
	ot->cancel= set_cursor_cancel;
	ot->poll= text_region_edit_poll;

	/* properties */
	RNA_def_boolean(ot->srna, "select", 0, "Select", "Set selection end rather than cursor.");
}

/******************* line number operator **********************/

static int line_number_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= CTX_data_edit_text(C);
	ARegion *ar= CTX_wm_region(C);
	short *mval= event->mval;
	double time;
	static int jump_to= 0;
	static double last_jump= 0;

	if(!st->showlinenrs)
		return OPERATOR_PASS_THROUGH;

	if(!(mval[0]>2 && mval[0]<60 && mval[1]>2 && mval[1]<ar->winy-2))
		return OPERATOR_PASS_THROUGH;

	if(!(event->ascii>='0' && event->ascii<='9'))
		return OPERATOR_PASS_THROUGH;

	time = PIL_check_seconds_timer();
	if(last_jump < time-1)
		jump_to= 0;

	jump_to *= 10;
	jump_to += (int)(event->ascii-'0');

	txt_move_toline(text, jump_to-1, 0);
	last_jump= time;

	WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);

	return OPERATOR_FINISHED;
}

void TEXT_OT_line_number(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Line Number";
	ot->idname= "TEXT_OT_line_number";
	ot->description= "The current line number.";
	
	/* api callbacks */
	ot->invoke= line_number_invoke;
	ot->poll= text_region_edit_poll;
}

/******************* insert operator **********************/

static int insert_exec(bContext *C, wmOperator *op)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= CTX_data_edit_text(C);
	char *str;
	int done = 0, i;

	str= RNA_string_get_alloc(op->ptr, "text", NULL, 0);

	if(st && st->overwrite) {
		for(i=0; str[i]; i++) {
			done |= txt_replace_char(text, str[i]);
		}
	} else {
		for(i=0; str[i]; i++) {
			done |= txt_add_char(text, str[i]);
		}
	}

	MEM_freeN(str);
	
	if(!done)
		return OPERATOR_CANCELLED;

	text_update_line_edited(text, text->curl);

	WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);

	return OPERATOR_FINISHED;
}

static int insert_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	char str[2];
	int ret;
	/* XXX old code from winqreadtextspace, is it still needed somewhere? */
	/* smartass code to prevent the CTRL/ALT events below from not working! */
	/*if(qual & (LR_ALTKEY|LR_CTRLKEY))
		if(!ispunct(ascii)) 
			ascii= 0;*/

	str[0]= event->ascii;
	str[1]= '\0';

	RNA_string_set(op->ptr, "text", str);
	ret = insert_exec(C, op);
	
	/* run the script while editing, evil but useful */
	if(ret==OPERATOR_FINISHED && CTX_wm_space_text(C)->live_edit)
		run_script_exec(C, op);

	return ret;
}

void TEXT_OT_insert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Insert";
	ot->idname= "TEXT_OT_insert";
	ot->description= "Insert text at cursor position.";
	
	/* api callbacks */
	ot->exec= insert_exec;
	ot->invoke= insert_invoke;
	ot->poll= text_edit_poll;

	/* properties */
	RNA_def_string(ot->srna, "text", "", 0, "Text", "Text to insert at the cursor position.");
}

/******************* find operator *********************/

/* mode */
#define TEXT_FIND		0
#define TEXT_REPLACE	1
#define TEXT_MARK_ALL	2

static int find_and_replace(bContext *C, wmOperator *op, short mode)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *start= NULL, *text= st->text;
	int flags, first= 1;
	char *tmp;

	if(!st->findstr[0] || (mode == TEXT_REPLACE && !st->replacestr[0]))
		return OPERATOR_CANCELLED;

	flags= st->flags;
	if(flags & ST_FIND_ALL)
		flags ^= ST_FIND_WRAP;

	do {
		if(first)
			txt_clear_markers(text, TMARK_GRP_FINDALL, 0);

		first= 0;
		
		/* Replace current */
		if(mode!=TEXT_FIND && txt_has_sel(text)) {
			tmp= txt_sel_to_buf(text);

			if(strcmp(st->findstr, tmp)==0) {
				if(mode==TEXT_REPLACE) {
					txt_insert_buf(text, st->replacestr);
					if(text->curl && text->curl->format) {
						MEM_freeN(text->curl->format);
						text->curl->format= NULL;
					}
					WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);
				}
				else if(mode==TEXT_MARK_ALL) {
					char color[4];
					UI_GetThemeColor4ubv(TH_SHADE2, color);

					if(txt_find_marker(text, text->curl, text->selc, TMARK_GRP_FINDALL, 0)) {
						if(tmp) MEM_freeN(tmp), tmp=NULL;
						break;
					}

					txt_add_marker(text, text->curl, text->curc, text->selc, color, TMARK_GRP_FINDALL, TMARK_EDITALL);
					WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);
				}
			}
			MEM_freeN(tmp);
			tmp= NULL;
		}

		/* Find next */
		if(txt_find_string(text, st->findstr, flags & ST_FIND_WRAP)) {
			WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
		}
		else if(flags & ST_FIND_ALL) {
			if(text==start) break;
			if(!start) start= text;
			if(text->id.next)
				text= st->text= text->id.next;
			else
				text= st->text= G.main->text.first;
			txt_move_toline(text, 0, 0);
			WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
			first= 1;
		}
		else {
			BKE_reportf(op->reports, RPT_ERROR, "Text not found: %s", st->findstr);
			break;
		}
	} while(mode==TEXT_MARK_ALL);

	return OPERATOR_FINISHED;
}

static int find_exec(bContext *C, wmOperator *op)
{
	return find_and_replace(C, op, TEXT_FIND);
}

void TEXT_OT_find(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Find";
	ot->idname= "TEXT_OT_find";
	ot->description= "Find specified text.";
	
	/* api callbacks */
	ot->exec= find_exec;
	ot->poll= text_space_edit_poll;
}

/******************* replace operator *********************/

static int replace_exec(bContext *C, wmOperator *op)
{
	return find_and_replace(C, op, TEXT_REPLACE);
}

void TEXT_OT_replace(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Replace";
	ot->idname= "TEXT_OT_replace";
	ot->description= "Replace text with the specified text.";

	/* api callbacks */
	ot->exec= replace_exec;
	ot->poll= text_space_edit_poll;
}

/******************* mark all operator *********************/

static int mark_all_exec(bContext *C, wmOperator *op)
{
	return find_and_replace(C, op, TEXT_MARK_ALL);
}

void TEXT_OT_mark_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mark All";
	ot->idname= "TEXT_OT_mark_all";
	ot->description= "Mark all specified text.";
	
	/* api callbacks */
	ot->exec= mark_all_exec;
	ot->poll= text_space_edit_poll;
}

/******************* find set selected *********************/

static int find_set_selected_exec(bContext *C, wmOperator *op)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= CTX_data_edit_text(C);
	char *tmp;

	tmp= txt_sel_to_buf(text);
	BLI_strncpy(st->findstr, tmp, ST_MAX_FIND_STR);
	MEM_freeN(tmp);

	if(!st->findstr[0])
		return OPERATOR_FINISHED;

	return find_and_replace(C, op, TEXT_FIND);
}

void TEXT_OT_find_set_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Find Set Selected";
	ot->idname= "TEXT_OT_find_set_selected";
	ot->description= "Find specified text and set as selected.";
	
	/* api callbacks */
	ot->exec= find_set_selected_exec;
	ot->poll= text_space_edit_poll;
}

/******************* replace set selected *********************/

static int replace_set_selected_exec(bContext *C, wmOperator *op)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= CTX_data_edit_text(C);
	char *tmp;

	tmp= txt_sel_to_buf(text);
	BLI_strncpy(st->replacestr, tmp, ST_MAX_FIND_STR);
	MEM_freeN(tmp);

	return OPERATOR_FINISHED;
}

void TEXT_OT_replace_set_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Replace Set Selected";
	ot->idname= "TEXT_OT_replace_set_selected";
	ot->description= "Replace text with specified text and set as selected.";
	
	/* api callbacks */
	ot->exec= replace_set_selected_exec;
	ot->poll= text_space_edit_poll;
}

/****************** resolve conflict operator ******************/

enum { RESOLVE_IGNORE, RESOLVE_RELOAD, RESOLVE_SAVE, RESOLVE_MAKE_INTERNAL };
static EnumPropertyItem resolution_items[]= {
	{RESOLVE_IGNORE, "IGNORE", 0, "Ignore", ""},
	{RESOLVE_RELOAD, "RELOAD", 0, "Reload", ""},
	{RESOLVE_SAVE, "SAVE", 0, "Save", ""},
	{RESOLVE_MAKE_INTERNAL, "MAKE_INTERNAL", 0, "Make Internal", ""},
	{0, NULL, 0, NULL, NULL}};

/* returns 0 if file on disk is the same or Text is in memory only
   returns 1 if file has been modified on disk since last local edit
   returns 2 if file on disk has been deleted
   -1 is returned if an error occurs */

int text_file_modified(Text *text)
{
	struct stat st;
	int result;
	char file[FILE_MAXDIR+FILE_MAXFILE];

	if(!text || !text->name)
		return 0;

	BLI_strncpy(file, text->name, FILE_MAXDIR+FILE_MAXFILE);
	BLI_convertstringcode(file, G.sce);

	if(!BLI_exists(file))
		return 2;

	result = stat(file, &st);
	
	if(result == -1)
		return -1;

	if((st.st_mode & S_IFMT) != S_IFREG)
		return -1;

	if(st.st_mtime > text->mtime)
		return 1;

	return 0;
}

static void text_ignore_modified(Text *text)
{
	struct stat st;
	int result;
	char file[FILE_MAXDIR+FILE_MAXFILE];

	if(!text || !text->name) return;

	BLI_strncpy(file, text->name, FILE_MAXDIR+FILE_MAXFILE);
	BLI_convertstringcode(file, G.sce);

	if(!BLI_exists(file)) return;

	result = stat(file, &st);
	
	if(result == -1 || (st.st_mode & S_IFMT) != S_IFREG)
		return;

	text->mtime= st.st_mtime;
}

static int resolve_conflict_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);
	int resolution= RNA_enum_get(op->ptr, "resolution");

	switch(resolution) {
		case RESOLVE_RELOAD:
			return reload_exec(C, op);
		case RESOLVE_SAVE:
			return save_exec(C, op);
		case RESOLVE_MAKE_INTERNAL:
			return make_internal_exec(C, op);
		case RESOLVE_IGNORE:
			text_ignore_modified(text);
			return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

static int resolve_conflict_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Text *text= CTX_data_edit_text(C);
	uiPopupMenu *pup;
	uiLayout *layout;

	switch(text_file_modified(text)) {
		case 1:
			if(text->flags & TXT_ISDIRTY) {
				/* modified locally and externally, ahhh. offer more possibilites. */
				pup= uiPupMenuBegin(C, "File Modified Outside and Inside Blender", 0);
				layout= uiPupMenuLayout(pup);
				uiItemEnumO(layout, "Reload from disk (ignore local changes)", 0, op->type->idname, "resolution", RESOLVE_RELOAD);
				uiItemEnumO(layout, "Save to disk (ignore outside changes)", 0, op->type->idname, "resolution", RESOLVE_SAVE);
				uiItemEnumO(layout, "Make text internal (separate copy)", 0, op->type->idname, "resolution", RESOLVE_MAKE_INTERNAL);
				uiPupMenuEnd(C, pup);
			}
			else {
				pup= uiPupMenuBegin(C, "File Modified Outside Blender", 0);
				layout= uiPupMenuLayout(pup);
				uiItemEnumO(layout, "Reload from disk", 0, op->type->idname, "resolution", RESOLVE_RELOAD);
				uiItemEnumO(layout, "Make text internal (separate copy)", 0, op->type->idname, "resolution", RESOLVE_MAKE_INTERNAL);
				uiItemEnumO(layout, "Ignore", 0, op->type->idname, "resolution", RESOLVE_IGNORE);
				uiPupMenuEnd(C, pup);
			}
			break;
		case 2:
			pup= uiPupMenuBegin(C, "File Deleted Outside Blender", 0);
			layout= uiPupMenuLayout(pup);
			uiItemEnumO(layout, "Make text internal", 0, op->type->idname, "resolution", RESOLVE_MAKE_INTERNAL);
			uiItemEnumO(layout, "Recreate file", 0, op->type->idname, "resolution", RESOLVE_SAVE);
			uiPupMenuEnd(C, pup);
			break;
	}

	return OPERATOR_CANCELLED;
}

void TEXT_OT_resolve_conflict(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Resolve Conflict";
	ot->idname= "TEXT_OT_resolve_conflict";
	ot->description= "When external text is out of sync, resolve the conflict.";

	/* api callbacks */
	ot->exec= resolve_conflict_exec;
	ot->invoke= resolve_conflict_invoke;
	ot->poll= save_poll;

	/* properties */
	RNA_def_enum(ot->srna, "resolution", resolution_items, RESOLVE_IGNORE, "Resolution", "How to solve conflict due to different in internal and external text.");
}

/********************** to 3d object operator *****************/

static int to_3d_object_exec(bContext *C, wmOperator *op)
{
	Text *text= CTX_data_edit_text(C);
	int split_lines= RNA_boolean_get(op->ptr, "split_lines");

	ED_text_to_object(C, text, split_lines);

	return OPERATOR_FINISHED;
}

void TEXT_OT_to_3d_object(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "To 3D Object";
	ot->idname= "TEXT_OT_to_3d_object";
	ot->description= "Create 3d text object from active text data block.";
	
	/* api callbacks */
	ot->exec= to_3d_object_exec;
	ot->poll= text_edit_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "split_lines", 0, "Split Lines", "Create one object per line in the text.");
}


/************************ undo ******************************/

void ED_text_undo_step(bContext *C, int step)
{
	Text *text= CTX_data_edit_text(C);

	if(!text)
		return;

	if(step==1)
		txt_do_undo(text);
	else if(step==-1)
		txt_do_redo(text);

	text_update_edited(text);

	WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, text);
	WM_event_add_notifier(C, NC_TEXT|NA_EDITED, text);
}

