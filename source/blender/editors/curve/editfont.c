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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/curve/editfont.c
 *  \ingroup edcurve
 */


#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <wchar.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_string_cursor_utf8.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_vfont_types.h"
#include "DNA_scene_types.h"
#include "DNA_text_types.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_font.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_curve.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "UI_interface.h"

#include "curve_intern.h"

#define MAXTEXT 32766

static int kill_selection(Object *obedit, int ins);

/************************* utilities ******************************/

static char findaccent(char char1, unsigned int code)
{
	char new = 0;
	
	if (char1 == 'a') {
		if (code == '`') new = 224;
		else if (code == 39) new = 225;
		else if (code == '^') new = 226;
		else if (code == '~') new = 227;
		else if (code == '"') new = 228;
		else if (code == 'o') new = 229;
		else if (code == 'e') new = 230;
		else if (code == '-') new = 170;
	}
	else if (char1 == 'c') {
		if (code == ',') new = 231;
		else if (code == '|') new = 162;
		else if (code == 'o') new = 169;
	}
	else if (char1 == 'e') {
		if (code == '`') new = 232;
		else if (code == 39) new = 233;
		else if (code == '^') new = 234;
		else if (code == '"') new = 235;
	}
	else if (char1 == 'i') {
		if (code == '`') new = 236;
		else if (code == 39) new = 237;
		else if (code == '^') new = 238;
		else if (code == '"') new = 239;
	}
	else if (char1 == 'n') {
		if (code == '~') new = 241;
	}
	else if (char1 == 'o') {
		if (code == '`') new = 242;
		else if (code == 39) new = 243;
		else if (code == '^') new = 244;
		else if (code == '~') new = 245;
		else if (code == '"') new = 246;
		else if (code == '/') new = 248;
		else if (code == '-') new = 186;
		else if (code == 'e') new = 143;
		else if (code == 'c') new = 169;
		else if (code == 'r') new = 174;
	}
	else if (char1 == 'r') {
		if (code == 'o') new = 174;
	}
	else if (char1 == 's') {
		if (code == 's') new = 167;
	}
	else if (char1 == 't') {
		if (code == 'm') new = 153;
	}
	else if (char1 == 'u') {
		if (code == '`') new = 249;
		else if (code == 39) new = 250;
		else if (code == '^') new = 251;
		else if (code == '"') new = 252;
	}
	else if (char1 == 'y') {
		if (code == 39) new = 253;
		else if (code == '"') new = 255;
	}
	else if (char1 == 'A') {
		if (code == '`') new = 192;
		else if (code == 39) new = 193;
		else if (code == '^') new = 194;
		else if (code == '~') new = 195;
		else if (code == '"') new = 196;
		else if (code == 'o') new = 197;
		else if (code == 'e') new = 198;
	}
	else if (char1 == 'C') {
		if (code == ',') new = 199;
	}
	else if (char1 == 'E') {
		if (code == '`') new = 200;
		else if (code == 39) new = 201;
		else if (code == '^') new = 202;
		else if (code == '"') new = 203;
	}
	else if (char1 == 'I') {
		if (code == '`') new = 204;
		else if (code == 39) new = 205;
		else if (code == '^') new = 206;
		else if (code == '"') new = 207;
	}
	else if (char1 == 'N') {
		if (code == '~') new = 209;
	}
	else if (char1 == 'O') {
		if (code == '`') new = 210;
		else if (code == 39) new = 211;
		else if (code == '^') new = 212;
		else if (code == '~') new = 213;
		else if (code == '"') new = 214;
		else if (code == '/') new = 216;
		else if (code == 'e') new = 141;
	}
	else if (char1 == 'U') {
		if (code == '`') new = 217;
		else if (code == 39) new = 218;
		else if (code == '^') new = 219;
		else if (code == '"') new = 220;
	}
	else if (char1 == 'Y') {
		if (code == 39) new = 221;
	}
	else if (char1 == '1') {
		if (code == '4') new = 188;
		if (code == '2') new = 189;
	}
	else if (char1 == '3') {
		if (code == '4') new = 190;
	}
	else if (char1 == ':') {
		if (code == '-') new = 247;
	}
	else if (char1 == '-') {
		if (code == ':') new = 247;
		if (code == '|') new = 135;
		if (code == '+') new = 177;
	}
	else if (char1 == '|') {
		if (code == '-') new = 135;
		if (code == '=') new = 136;
	}
	else if (char1 == '=') {
		if (code == '|') new = 136;
	}
	else if (char1 == '+') {
		if (code == '-') new = 177;
	}
	
	if (new) return new;
	else return char1;
}

static int insert_into_textbuf(Object *obedit, uintptr_t c)
{
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	
	if (ef->len < MAXTEXT - 1) {
		int x;

		for (x = ef->len; x > ef->pos; x--) ef->textbuf[x] = ef->textbuf[x - 1];
		for (x = ef->len; x > ef->pos; x--) ef->textbufinfo[x] = ef->textbufinfo[x - 1];
		ef->textbuf[ef->pos] = c;
		ef->textbufinfo[ef->pos] = cu->curinfo;
		ef->textbufinfo[ef->pos].kern = 0;
		ef->textbufinfo[ef->pos].mat_nr = obedit->actcol;
					
		ef->pos++;
		ef->len++;
		ef->textbuf[ef->len] = '\0';

		return 1;
	}
	else
		return 0;
}

static void text_update_edited(bContext *C, Object *obedit, int mode)
{
	struct Main *bmain = CTX_data_main(C);
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	cu->curinfo = ef->textbufinfo[ef->pos ? ef->pos - 1 : 0];
	
	if (obedit->totcol > 0) {
		obedit->actcol = ef->textbufinfo[ef->pos ? ef->pos - 1 : 0].mat_nr;

		/* since this array is calloc'd, it can be 0 even though we try ensure
		 * (mat_nr > 0) almost everywhere */
		if (obedit->actcol < 1) {
			obedit->actcol = 1;
		}
	}

	if (mode == FO_EDIT) {
		/* re-tesselllate */
		DAG_id_tag_update(obedit->data, 0);
	}
	else {
		/* depsgraph runs above, but since we're not tagging for update, call direct */
		BKE_vfont_to_curve(bmain, obedit, mode);
	}

	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
}

/********************** insert lorem operator *********************/

static int insert_lorem_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	const char *p, *p2;
	int i;
	static const char *lastlorem = NULL;
	
	if (lastlorem)
		p = lastlorem;
	else
		p = ED_lorem;
	
	i = rand() / (RAND_MAX / 6) + 4;
		
	for (p2 = p; *p2 && i; p2++) {
		insert_into_textbuf(obedit, *p2);

		if (*p2 == '.')
			i--;
	}

	lastlorem = p2 + 1;
	if (strlen(lastlorem) < 5)
		lastlorem = ED_lorem;
	
	insert_into_textbuf(obedit, '\n');
	insert_into_textbuf(obedit, '\n');

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void FONT_OT_insert_lorem(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Insert Lorem";
	ot->description = "Insert placeholder text";
	ot->idname = "FONT_OT_insert_lorem";
	
	/* api callbacks */
	ot->exec = insert_lorem_exec;
	ot->poll = ED_operator_editfont;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------------------------------------- */
/* Generic Paste Functions */

/* text_update_edited(C, scene, obedit, 1, FO_EDIT); */
static bool font_paste_wchar(Object *obedit, const wchar_t *str, const size_t str_len,
                             /* optional */
                             struct CharInfo *str_info)
{
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	int selend, selstart;

	if (BKE_vfont_select_get(obedit, &selstart, &selend) == 0) {
		selstart = selend = 0;
	}

	/* Verify that the copy buffer => [copy buffer len] + ef->len < MAXTEXT */
	if ((ef->len + str_len) - (selend - selstart) <= MAXTEXT) {

		kill_selection(obedit, 0);

		if (str_len) {
			int size = (ef->len * sizeof(wchar_t)) - (ef->pos * sizeof(wchar_t)) + sizeof(wchar_t);
			memmove(ef->textbuf + ef->pos + str_len, ef->textbuf + ef->pos, size);
			memcpy(ef->textbuf + ef->pos, str, str_len * sizeof(wchar_t));

			memmove(ef->textbufinfo + ef->pos + str_len, ef->textbufinfo + ef->pos, (ef->len - ef->pos + 1) * sizeof(CharInfo));
			if (str_info) {
				memcpy(ef->textbufinfo + ef->pos, str_info, str_len * sizeof(CharInfo));
			}
			else {
				memset(ef->textbufinfo + ef->pos, '\0', str_len * sizeof(CharInfo));
			}

			ef->len += str_len;
			ef->pos += str_len;
		}

		return true;
	}

	return false;
}

static bool font_paste_utf8(bContext *C, const char *str, const size_t str_len)
{
	Object *obedit = CTX_data_edit_object(C);
	bool retval;

	int tmplen;

	wchar_t *mem = MEM_mallocN((sizeof(wchar_t) * (str_len + 1)), __func__);

	tmplen = BLI_strncpy_wchar_from_utf8(mem, str, str_len + 1);

	retval = font_paste_wchar(obedit, mem, tmplen, NULL);

	MEM_freeN(mem);

	return retval;
}


/* -------------------------------------------------------------------- */
/* Paste From File*/

static int paste_from_file(bContext *C, ReportList *reports, const char *filename)
{
	Object *obedit = CTX_data_edit_object(C);
	FILE *fp;
	char *strp;
	int filelen;
	int retval;

	fp = BLI_fopen(filename, "r");

	if (!fp) {
		BKE_reportf(reports, RPT_ERROR, "Failed to open file '%s'", filename);
		return OPERATOR_CANCELLED;
	}

	fseek(fp, 0L, SEEK_END);

	errno = 0;
	filelen = ftell(fp);
	if (filelen == -1) {
		goto fail;
	}

	if (filelen <= MAXTEXT) {
		strp = MEM_mallocN(filelen + 4, "tempstr");

		fseek(fp, 0L, SEEK_SET);

		/* fread() instead of read(), because windows read() converts text
		 * to DOS \r\n linebreaks, causing double linebreaks in the 3d text */
		errno = 0;
		filelen = fread(strp, 1, filelen, fp);
		if (filelen == -1) {
			MEM_freeN(strp);
			goto fail;
		}

		strp[filelen] = 0;
	}
	else {
		strp = NULL;
	}

	fclose(fp);


	if (strp && font_paste_utf8(C, strp, filelen)) {
		text_update_edited(C, obedit, FO_EDIT);
		retval = OPERATOR_FINISHED;

	}
	else {
		BKE_reportf(reports, RPT_ERROR, "File too long %s", filename);
		retval = OPERATOR_CANCELLED;
	}

	if (strp) {
		MEM_freeN(strp);
	}

	return retval;


	/* failed to seek or read */
fail:
	BKE_reportf(reports, RPT_ERROR, "Failed to read file '%s', %s", filename, strerror(errno));
	fclose(fp);
	return OPERATOR_CANCELLED;
}

static int paste_from_file_exec(bContext *C, wmOperator *op)
{
	char *path;
	int retval;
	
	path = RNA_string_get_alloc(op->ptr, "filepath", NULL, 0);
	retval = paste_from_file(C, op->reports, path);
	MEM_freeN(path);

	return retval;
}

static int paste_from_file_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return paste_from_file_exec(C, op);

	WM_event_add_fileselect(C, op); 

	return OPERATOR_RUNNING_MODAL;
}

void FONT_OT_text_paste_from_file(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste File";
	ot->description = "Paste contents from file";
	ot->idname = "FONT_OT_text_paste_from_file";
	
	/* api callbacks */
	ot->exec = paste_from_file_exec;
	ot->invoke = paste_from_file_invoke;
	ot->poll = ED_operator_editfont;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE | TEXTFILE, FILE_SPECIAL, FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
}


/* -------------------------------------------------------------------- */
/* Paste From Clipboard */

static int paste_from_clipboard(bContext *C, ReportList *reports)
{
	Object *obedit = CTX_data_edit_object(C);
	char *strp;
	int filelen;
	int retval;

	strp = WM_clipboard_text_get(false, &filelen);
	if (strp == NULL) {
		BKE_report(reports, RPT_ERROR, "Clipboard empty");
		return OPERATOR_CANCELLED;
	}

	if ((filelen <= MAXTEXT) && font_paste_utf8(C, strp, filelen)) {
		text_update_edited(C, obedit, FO_EDIT);
		retval = OPERATOR_FINISHED;
	}
	else {
		BKE_report(reports, RPT_ERROR, "Clipboard too long");
		retval = OPERATOR_CANCELLED;
	}
	MEM_freeN(strp);

	return retval;
}

static int paste_from_clipboard_exec(bContext *C, wmOperator *op)
{
	int retval;

	retval = paste_from_clipboard(C, op->reports);

	return retval;
}

void FONT_OT_text_paste_from_clipboard(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste Clipboard";
	ot->description = "Paste contents from system clipboard";
	ot->idname = "FONT_OT_text_paste_from_clipboard";

	/* api callbacks */
	ot->exec = paste_from_clipboard_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE | TEXTFILE, FILE_SPECIAL, FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
}

/******************* text to object operator ********************/

static void txt_add_object(bContext *C, TextLine *firstline, int totline, const float offset[3])
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Curve *cu;
	Object *obedit;
	Base *base;
	struct TextLine *tmp;
	int nchars = 0, nbytes = 0;
	char *s;
	int a;
	float rot[3] = {0.f, 0.f, 0.f};
	
	obedit = BKE_object_add(bmain, scene, OB_FONT);
	base = scene->basact;

	/* seems to assume view align ? TODO - look into this, could be an operator option */
	ED_object_base_init_transform(C, base, NULL, rot);

	BKE_object_where_is_calc(scene, obedit);

	add_v3_v3(obedit->loc, offset);

	cu = obedit->data;
	cu->vfont = BKE_vfont_builtin_get();
	cu->vfont->id.us++;

	for (tmp = firstline, a = 0; nbytes < MAXTEXT && a < totline; tmp = tmp->next, a++) {
		size_t nchars_line, nbytes_line;
		nchars_line = BLI_strlen_utf8_ex(tmp->line, &nbytes_line);
		nchars += nchars_line + 1;
		nbytes += nbytes_line + 1;
	}

	if (cu->str) MEM_freeN(cu->str);
	if (cu->strinfo) MEM_freeN(cu->strinfo);

	cu->str = MEM_mallocN(nbytes + 4, "str");
	cu->strinfo = MEM_callocN((nchars + 4) * sizeof(CharInfo), "strinfo");

	cu->len = 0;
	cu->len_wchar = nchars - 1;
	cu->pos = 0;

	s = cu->str;

	for (tmp = firstline, a = 0; cu->len < MAXTEXT && a < totline; tmp = tmp->next, a++) {
		size_t nbytes_line;

		nbytes_line = BLI_strcpy_rlen(s, tmp->line);

		s += nbytes_line;
		cu->len += nbytes_line;

		if (tmp->next) {
			nbytes_line = BLI_strcpy_rlen(s, "\n");

			s += nbytes_line;
			cu->len += nbytes_line;
		}

	}

	cu->pos = cu->len_wchar;
	*s = '\0';

	WM_event_add_notifier(C, NC_OBJECT | NA_ADDED, obedit);
}

void ED_text_to_object(bContext *C, Text *text, const bool split_lines)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	TextLine *line;
	float offset[3];
	int linenum = 0;

	if (!text || !text->lines.first) return;

	if (split_lines) {
		for (line = text->lines.first; line; line = line->next) {
			/* skip lines with no text, but still make space for them */
			if (line->line[0] == '\0') {
				linenum++;
				continue;
			}
	
			/* do the translation */
			offset[0] = 0;
			offset[1] = -linenum;
			offset[2] = 0;
	
			if (rv3d)
				mul_mat3_m4_v3(rv3d->viewinv, offset);

			txt_add_object(C, line, 1, offset);

			linenum++;
		}
	}
	else {
		offset[0] = 0.0f;
		offset[1] = 0.0f;
		offset[2] = 0.0f;

		txt_add_object(C, text->lines.first, BLI_countlist(&text->lines), offset);
	}
}

/********************** utilities ***************************/

static int kill_selection(Object *obedit, int ins)  /* 1 == new character */
{
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	int selend, selstart, direction;
	int offset = 0;
	int getfrom;

	direction = BKE_vfont_select_get(obedit, &selstart, &selend);
	if (direction) {
		int size;
		if (ins) offset = 1;
		if (ef->pos >= selstart) ef->pos = selstart + offset;
		if ((direction == -1) && ins) {
			selstart++;
			selend++;
		}
		getfrom = selend + offset;
		if (ins == 0) getfrom++;
		size = (ef->len * sizeof(wchar_t)) - (selstart * sizeof(wchar_t)) + (offset * sizeof(wchar_t));
		memmove(ef->textbuf + selstart, ef->textbuf + getfrom, size);
		memmove(ef->textbufinfo + selstart, ef->textbufinfo + getfrom, ((ef->len - selstart) + offset) * sizeof(CharInfo));
		ef->len -= ((selend - selstart) + 1);
		ef->selstart = ef->selend = 0;
	}

	return(direction);
}

/******************* set style operator ********************/

static EnumPropertyItem style_items[] = {
	{CU_CHINFO_BOLD, "BOLD", 0, "Bold", ""},
	{CU_CHINFO_ITALIC, "ITALIC", 0, "Italic", ""},
	{CU_CHINFO_UNDERLINE, "UNDERLINE", 0, "Underline", ""},
	{CU_CHINFO_SMALLCAPS, "SMALL_CAPS", 0, "Small Caps", ""},
	{0, NULL, 0, NULL, NULL}
};

static int set_style(bContext *C, const int style, const bool clear)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	int i, selstart, selend;

	if (!BKE_vfont_select_get(obedit, &selstart, &selend))
		return OPERATOR_CANCELLED;

	for (i = selstart; i <= selend; i++) {
		if (clear)
			ef->textbufinfo[i].flag &= ~style;
		else
			ef->textbufinfo[i].flag |= style;
	}

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

static int set_style_exec(bContext *C, wmOperator *op)
{
	const int style = RNA_enum_get(op->ptr, "style");
	const bool clear = RNA_boolean_get(op->ptr, "clear");

	return set_style(C, style, clear);
}

void FONT_OT_style_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Style";
	ot->description = "Set font style";
	ot->idname = "FONT_OT_style_set";
	
	/* api callbacks */
	ot->exec = set_style_exec;
	ot->poll = ED_operator_editfont;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "style", style_items, CU_CHINFO_BOLD, "Style", "Style to set selection to");
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "Clear style rather than setting it");
}

/******************* toggle style operator ********************/

static int toggle_style_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	int style, clear, selstart, selend;

	if (!BKE_vfont_select_get(obedit, &selstart, &selend))
		return OPERATOR_CANCELLED;
	
	style = RNA_enum_get(op->ptr, "style");

	cu->curinfo.flag ^= style;
	clear = (cu->curinfo.flag & style) == 0;

	return set_style(C, style, clear);
}

void FONT_OT_style_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Style";
	ot->description = "Toggle font style";
	ot->idname = "FONT_OT_style_toggle";
	
	/* api callbacks */
	ot->exec = toggle_style_exec;
	ot->poll = ED_operator_editfont;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "style", style_items, CU_CHINFO_BOLD, "Style", "Style to set selection to");
}


/* -------------------------------------------------------------------- */
/* Select All */

static int font_select_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;

	if (ef->len) {
		ef->selstart = 1;
		ef->selend = ef->len;
		ef->pos = ef->len;

		text_update_edited(C, obedit, FO_SELCHANGE);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}

}

void FONT_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select All";
	ot->description = "Select all text";
	ot->idname = "FONT_OT_select_all";

	/* api callbacks */
	ot->exec = font_select_all_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/******************* copy text operator ********************/

static void copy_selection(Object *obedit)
{
	int selstart, selend;
	
	if (BKE_vfont_select_get(obedit, &selstart, &selend)) {
		Curve *cu = obedit->data;
		EditFont *ef = cu->editfont;
		
		memcpy(ef->copybuf, ef->textbuf + selstart, ((selend - selstart) + 1) * sizeof(wchar_t));
		ef->copybuf[(selend - selstart) + 1] = 0;
		memcpy(ef->copybufinfo, ef->textbufinfo + selstart, ((selend - selstart) + 1) * sizeof(CharInfo));
	}
}

static int copy_text_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);

	copy_selection(obedit);

	return OPERATOR_FINISHED;
}

void FONT_OT_text_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Text";
	ot->description = "Copy selected text to clipboard";
	ot->idname = "FONT_OT_text_copy";
	
	/* api callbacks */
	ot->exec = copy_text_exec;
	ot->poll = ED_operator_editfont;
}

/******************* cut text operator ********************/

static int cut_text_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	int selstart, selend;

	if (!BKE_vfont_select_get(obedit, &selstart, &selend))
		return OPERATOR_CANCELLED;

	copy_selection(obedit);
	kill_selection(obedit, 0);

	text_update_edited(C, obedit, FO_EDIT);

	return OPERATOR_FINISHED;
}

void FONT_OT_text_cut(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Cut Text";
	ot->description = "Cut selected text to clipboard";
	ot->idname = "FONT_OT_text_cut";
	
	/* api callbacks */
	ot->exec = cut_text_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************* paste text operator ********************/

static bool paste_selection(Object *obedit, ReportList *reports)
{
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	int len = wcslen(ef->copybuf);

	if (font_paste_wchar(obedit, ef->copybuf, len, ef->copybufinfo)) {
		return true;
	}
	else {
		BKE_report(reports, RPT_WARNING, "Text too long");
		return false;
	}
}

static int paste_text_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);

	if (!paste_selection(obedit, op->reports))
		return OPERATOR_CANCELLED;

	text_update_edited(C, obedit, FO_EDIT);

	return OPERATOR_FINISHED;
}

void FONT_OT_text_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste Text";
	ot->description = "Paste text from clipboard";
	ot->idname = "FONT_OT_text_paste";
	
	/* api callbacks */
	ot->exec = paste_text_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************ move operator ************************/

static EnumPropertyItem move_type_items[] = {
	{LINE_BEGIN, "LINE_BEGIN", 0, "Line Begin", ""},
	{LINE_END, "LINE_END", 0, "Line End", ""},
	{PREV_CHAR, "PREVIOUS_CHARACTER", 0, "Previous Character", ""},
	{NEXT_CHAR, "NEXT_CHARACTER", 0, "Next Character", ""},
	{PREV_WORD, "PREVIOUS_WORD", 0, "Previous Word", ""},
	{NEXT_WORD, "NEXT_WORD", 0, "Next Word", ""},
	{PREV_LINE, "PREVIOUS_LINE", 0, "Previous Line", ""},
	{NEXT_LINE, "NEXT_LINE", 0, "Next Line", ""},
	{PREV_PAGE, "PREVIOUS_PAGE", 0, "Previous Page", ""},
	{NEXT_PAGE, "NEXT_PAGE", 0, "Next Page", ""},
	{0, NULL, 0, NULL, NULL}};

static int move_cursor(bContext *C, int type, const bool select)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	int cursmove = -1;

	switch (type) {
		case LINE_BEGIN:
			if ((select) && (ef->selstart == 0)) ef->selstart = ef->selend = ef->pos + 1;
			while (ef->pos > 0) {
				if (ef->textbuf[ef->pos - 1] == '\n') break;
				if (ef->textbufinfo[ef->pos - 1].flag & CU_CHINFO_WRAP) break;
				ef->pos--;
			}
			cursmove = FO_CURS;
			break;
			
		case LINE_END:
			if ((select) && (ef->selstart == 0)) ef->selstart = ef->selend = ef->pos + 1;
			while (ef->pos < ef->len) {
				if (ef->textbuf[ef->pos] == 0) break;
				if (ef->textbuf[ef->pos] == '\n') break;
				if (ef->textbufinfo[ef->pos].flag & CU_CHINFO_WRAP) break;
				ef->pos++;
			}
			cursmove = FO_CURS;
			break;

		case PREV_WORD:
		{
			int pos = ef->pos;
			if ((select) && (ef->selstart == 0)) ef->selstart = ef->selend = ef->pos + 1;
			BLI_str_cursor_step_wchar(ef->textbuf, ef->len, &pos, STRCUR_DIR_PREV, STRCUR_JUMP_DELIM, true);
			ef->pos = pos;
			cursmove = FO_CURS;
			break;
		}

		case NEXT_WORD:
		{
			int pos = ef->pos;
			if ((select) && (ef->selstart == 0)) ef->selstart = ef->selend = ef->pos + 1;
			BLI_str_cursor_step_wchar(ef->textbuf, ef->len, &pos, STRCUR_DIR_NEXT, STRCUR_JUMP_DELIM, true);
			ef->pos = pos;
			cursmove = FO_CURS;
			break;
		}

		case PREV_CHAR:
			if ((select) && (ef->selstart == 0)) ef->selstart = ef->selend = ef->pos + 1;
			ef->pos--;
			cursmove = FO_CURS;
			break;

		case NEXT_CHAR:
			if ((select) && (ef->selstart == 0)) ef->selstart = ef->selend = ef->pos + 1;
			ef->pos++;
			cursmove = FO_CURS;

			break;

		case PREV_LINE:
			if ((select) && (ef->selstart == 0)) ef->selstart = ef->selend = ef->pos + 1;
			cursmove = FO_CURSUP;
			break;
			
		case NEXT_LINE:
			if ((select) && (ef->selstart == 0)) ef->selstart = ef->selend = ef->pos + 1;
			cursmove = FO_CURSDOWN;
			break;

		case PREV_PAGE:
			if ((select) && (ef->selstart == 0)) ef->selstart = ef->selend = ef->pos + 1;
			cursmove = FO_PAGEUP;
			break;

		case NEXT_PAGE:
			if ((select) && (ef->selstart == 0)) ef->selstart = ef->selend = ef->pos + 1;
			cursmove = FO_PAGEDOWN;
			break;
	}
		
	if (cursmove == -1)
		return OPERATOR_CANCELLED;

	if      (ef->pos > ef->len)  ef->pos = ef->len;
	else if (ef->pos >= MAXTEXT) ef->pos = MAXTEXT;
	else if (ef->pos < 0)        ef->pos = 0;

	if (select == 0) {
		if (ef->selstart) {
			struct Main *bmain = CTX_data_main(C);
			ef->selstart = ef->selend = 0;
			BKE_vfont_to_curve(bmain, obedit, FO_SELCHANGE);
		}
	}

	if (select)
		ef->selend = ef->pos;

	text_update_edited(C, obedit, cursmove);

	return OPERATOR_FINISHED;
}

static int move_exec(bContext *C, wmOperator *op)
{
	int type = RNA_enum_get(op->ptr, "type");

	return move_cursor(C, type, false);
}

void FONT_OT_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Cursor";
	ot->description = "Move cursor to position type";
	ot->idname = "FONT_OT_move";
	
	/* api callbacks */
	ot->exec = move_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "type", move_type_items, LINE_BEGIN, "Type", "Where to move cursor to");
}

/******************* move select operator ********************/

static int move_select_exec(bContext *C, wmOperator *op)
{
	int type = RNA_enum_get(op->ptr, "type");

	return move_cursor(C, type, true);
}

void FONT_OT_move_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Select";
	ot->description = "Move the cursor while selecting";
	ot->idname = "FONT_OT_move_select";
	
	/* api callbacks */
	ot->exec = move_select_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "type", move_type_items, LINE_BEGIN, "Type", "Where to move cursor to, to make a selection");
}

/************************* change spacing **********************/

static int change_spacing_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	int kern, delta = RNA_int_get(op->ptr, "delta");

	kern = ef->textbufinfo[ef->pos - 1].kern;
	kern += delta;
	CLAMP(kern, -20, 20);

	if (ef->textbufinfo[ef->pos - 1].kern == kern)
		return OPERATOR_CANCELLED;

	ef->textbufinfo[ef->pos - 1].kern = kern;

	text_update_edited(C, obedit, FO_EDIT);

	return OPERATOR_FINISHED;
}

void FONT_OT_change_spacing(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Spacing";
	ot->description = "Change font spacing";
	ot->idname = "FONT_OT_change_spacing";
	
	/* api callbacks */
	ot->exec = change_spacing_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "delta", 1, -20, 20, "Delta", "Amount to decrease or increase character spacing with", -20, 20);
}

/************************* change character **********************/

static int change_character_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	int character, delta = RNA_int_get(op->ptr, "delta");

	if (ef->pos <= 0)
		return OPERATOR_CANCELLED;

	character = ef->textbuf[ef->pos - 1];
	character += delta;
	CLAMP(character, 0, 255);

	if (character == ef->textbuf[ef->pos - 1])
		return OPERATOR_CANCELLED;

	ef->textbuf[ef->pos - 1] = character;

	text_update_edited(C, obedit, FO_EDIT);

	return OPERATOR_FINISHED;
}

void FONT_OT_change_character(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Character";
	ot->description = "Change font character code";
	ot->idname = "FONT_OT_change_character";
	
	/* api callbacks */
	ot->exec = change_character_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "delta", 1, -255, 255, "Delta", "Number to increase or decrease character code with", -255, 255);
}

/******************* line break operator ********************/

static int line_break_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;

	insert_into_textbuf(obedit, '\n');

	ef->selstart = ef->selend = 0;

	text_update_edited(C, obedit, FO_EDIT);

	return OPERATOR_FINISHED;
}

void FONT_OT_line_break(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Line Break";
	ot->description = "Insert line break at cursor position";
	ot->idname = "FONT_OT_line_break";
	
	/* api callbacks */
	ot->exec = line_break_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************* delete operator **********************/

static EnumPropertyItem delete_type_items[] = {
	{DEL_ALL, "ALL", 0, "All", ""},
	{DEL_NEXT_CHAR, "NEXT_CHARACTER", 0, "Next Character", ""},
	{DEL_PREV_CHAR, "PREVIOUS_CHARACTER", 0, "Previous Character", ""},
	{DEL_SELECTION, "SELECTION", 0, "Selection", ""},
	{DEL_NEXT_SEL, "NEXT_OR_SELECTION", 0, "Next or Selection", ""},
	{DEL_PREV_SEL, "PREVIOUS_OR_SELECTION", 0, "Previous or Selection", ""},
	{0, NULL, 0, NULL, NULL}};

static int delete_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	int x, selstart, selend, type = RNA_enum_get(op->ptr, "type");

	if (ef->len == 0)
		return OPERATOR_CANCELLED;

	if (BKE_vfont_select_get(obedit, &selstart, &selend)) {
		if (type == DEL_NEXT_SEL) type = DEL_SELECTION;
		else if (type == DEL_PREV_SEL) type = DEL_SELECTION;
	}
	else {
		if (type == DEL_NEXT_SEL) type = DEL_NEXT_CHAR;
		else if (type == DEL_PREV_SEL) type = DEL_PREV_CHAR;
	}

	switch (type) {
		case DEL_ALL:
			ef->len = ef->pos = 0;
			ef->textbuf[0] = 0;
			break;
		case DEL_SELECTION:
			if (!kill_selection(obedit, 0))
				return OPERATOR_CANCELLED;
			break;
		case DEL_PREV_CHAR:
			if (ef->pos <= 0)
				return OPERATOR_CANCELLED;

			for (x = ef->pos; x <= ef->len; x++)
				ef->textbuf[x - 1] = ef->textbuf[x];
			for (x = ef->pos; x <= ef->len; x++)
				ef->textbufinfo[x - 1] = ef->textbufinfo[x];

			ef->pos--;
			ef->textbuf[--ef->len] = '\0';
			break;
		case DEL_NEXT_CHAR:
			if (ef->pos >= ef->len)
				return OPERATOR_CANCELLED;

			for (x = ef->pos; x < ef->len; x++)
				ef->textbuf[x] = ef->textbuf[x + 1];
			for (x = ef->pos; x < ef->len; x++)
				ef->textbufinfo[x] = ef->textbufinfo[x + 1];

			ef->textbuf[--ef->len] = '\0';
			break;
		default:
			return OPERATOR_CANCELLED;
	}

	text_update_edited(C, obedit, FO_EDIT);

	return OPERATOR_FINISHED;
}

void FONT_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete";
	ot->description = "Delete text by cursor position";
	ot->idname = "FONT_OT_delete";
	
	/* api callbacks */
	ot->exec = delete_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "type", delete_type_items, DEL_ALL, "Type", "Which part of the text to delete");
}

/*********************** insert text operator *************************/

static int insert_text_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	char *inserted_utf8;
	wchar_t *inserted_text;
	int a, len;

	if (!RNA_struct_property_is_set(op->ptr, "text"))
		return OPERATOR_CANCELLED;
	
	inserted_utf8 = RNA_string_get_alloc(op->ptr, "text", NULL, 0);
	len = BLI_strlen_utf8(inserted_utf8);

	inserted_text = MEM_callocN(sizeof(wchar_t) * (len + 1), "FONT_insert_text");
	BLI_strncpy_wchar_from_utf8(inserted_text, inserted_utf8, len + 1);

	for (a = 0; a < len; a++)
		insert_into_textbuf(obedit, inserted_text[a]);

	MEM_freeN(inserted_text);
	MEM_freeN(inserted_utf8);

	kill_selection(obedit, 1);
	text_update_edited(C, obedit, FO_EDIT);

	return OPERATOR_FINISHED;
}

static int insert_text_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	static int accentcode = 0;
	uintptr_t ascii = event->ascii;
	int alt = event->alt, shift = event->shift, ctrl = event->ctrl;
	int event_type = event->type, event_val = event->val;
	wchar_t inserted_text[2] = {0};

	if (RNA_struct_property_is_set(op->ptr, "text"))
		return insert_text_exec(C, op);

	if (RNA_struct_property_is_set(op->ptr, "accent")) {
		if (ef->len != 0 && ef->pos > 0)
			accentcode = 1;
		return OPERATOR_FINISHED;
	}
	
	/* tab should exit editmode, but we allow it to be typed using modifier keys */
	if (event_type == TABKEY) {
		if ((alt || ctrl || shift) == 0)
			return OPERATOR_PASS_THROUGH;
		else
			ascii = 9;
	}
	
	if (event_type == BACKSPACEKEY) {
		if (alt && ef->len != 0 && ef->pos > 0)
			accentcode = 1;
		return OPERATOR_PASS_THROUGH;
	}

	if (event_val && (ascii || event->utf8_buf[0])) {
		/* handle case like TAB (== 9) */
		if (     (ascii > 31 && ascii < 254 && ascii != 127) ||
		         (ascii == 13) ||
		         (ascii == 10) ||
		         (ascii == 8)  ||
		         (event->utf8_buf[0]))
		{

			if (accentcode) {
				if (ef->pos > 0) {
					inserted_text[0] = findaccent(ef->textbuf[ef->pos - 1], ascii);
					ef->textbuf[ef->pos - 1] = inserted_text[0];
				}
				accentcode = 0;
			}
			else if (event->utf8_buf[0]) {
				BLI_strncpy_wchar_from_utf8(inserted_text, event->utf8_buf, 2);
				ascii = inserted_text[0];
				insert_into_textbuf(obedit, ascii);
				accentcode = 0;
			}
			else if (ascii) {
				insert_into_textbuf(obedit, ascii);
				accentcode = 0;
			}
			else {
				BLI_assert(0);
			}
			
			kill_selection(obedit, 1);
			text_update_edited(C, obedit, FO_EDIT);
		}
		else {
			inserted_text[0] = ascii;
			insert_into_textbuf(obedit, ascii);
			text_update_edited(C, obedit, FO_EDIT);
		}
	}
	else
		return OPERATOR_PASS_THROUGH;

	if (inserted_text[0]) {
		/* store as utf8 in RNA string */
		char inserted_utf8[8] = {0};

		BLI_strncpy_wchar_as_utf8(inserted_utf8, inserted_text, sizeof(inserted_utf8));
		RNA_string_set(op->ptr, "text", inserted_utf8);
	}

	/* reset property? */
	if (event_val == 0)
		accentcode = 0;
	
	return OPERATOR_FINISHED;
}

void FONT_OT_text_insert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Insert Text";
	ot->description = "Insert text at cursor position";
	ot->idname = "FONT_OT_text_insert";
	
	/* api callbacks */
	ot->exec = insert_text_exec;
	ot->invoke = insert_text_invoke;
	ot->poll = ED_operator_editfont;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "text", NULL, 0, "Text", "Text to insert at the cursor position");
	RNA_def_boolean(ot->srna, "accent", 0, "Accent mode", "Next typed character will strike through previous, for special character input");
}


/*********************** textbox add operator *************************/
static int textbox_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_active_object(C);
	Curve *cu = obedit->data;
	int i;
	
	if (cu->totbox < 256) {
		for (i = cu->totbox; i > cu->actbox; i--) cu->tb[i] = cu->tb[i - 1];
		cu->tb[cu->actbox] = cu->tb[cu->actbox - 1];
		cu->actbox++;
		cu->totbox++;
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	return OPERATOR_FINISHED;
}

void FONT_OT_textbox_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Textbox";
	ot->description = "Add a new text box";
	ot->idname = "FONT_OT_textbox_add";
	
	/* api callbacks */
	ot->exec = textbox_add_exec;
	ot->poll = ED_operator_object_active_editable_font;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	
}



/*********************** textbox remove operator *************************/



static int textbox_remove_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_active_object(C);
	Curve *cu = obedit->data;
	int i;
	int index = RNA_int_get(op->ptr, "index");
	
	
	if (cu->totbox > 1) {
		for (i = index; i < cu->totbox; i++) cu->tb[i] = cu->tb[i + 1];
		cu->totbox--;
		if (cu->actbox >= index)
			cu->actbox--;
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	
	return OPERATOR_FINISHED;
}

void FONT_OT_textbox_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Textbox";
	ot->description = "Remove the textbox";
	ot->idname = "FONT_OT_textbox_remove";
	
	/* api callbacks */
	ot->exec = textbox_remove_exec;
	ot->poll = ED_operator_object_active_editable_font;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "The current text box", 0, INT_MAX);
}



/***************** editmode enter/exit ********************/

void make_editText(Object *obedit)
{
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	int len_wchar;
	
	if (ef == NULL) {
		ef = cu->editfont = MEM_callocN(sizeof(EditFont), "editfont");
	
		ef->textbuf = MEM_callocN((MAXTEXT + 4) * sizeof(wchar_t), "texteditbuf");
		ef->textbufinfo = MEM_callocN((MAXTEXT + 4) * sizeof(CharInfo), "texteditbufinfo");
		ef->copybuf = MEM_callocN((MAXTEXT + 4) * sizeof(wchar_t), "texteditcopybuf");
		ef->copybufinfo = MEM_callocN((MAXTEXT + 4) * sizeof(CharInfo), "texteditcopybufinfo");
	}
	
	/* Convert the original text to wchar_t */
	len_wchar = BLI_strncpy_wchar_from_utf8(ef->textbuf, cu->str, MAXTEXT + 4);
	BLI_assert(len_wchar == cu->len_wchar);
	ef->len = len_wchar;

	memcpy(ef->textbufinfo, cu->strinfo, ef->len * sizeof(CharInfo));

	if (ef->pos > ef->len) ef->pos = ef->len;

	cu->curinfo = ef->textbufinfo[ef->pos ? ef->pos - 1 : 0];

	/* Other vars */
	ef->pos = cu->pos;
	ef->selstart = cu->selstart;
	ef->selend = cu->selend;
}

void load_editText(Object *obedit)
{
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;

	/* Free the old curve string */
	MEM_freeN(cu->str);

	/* Calculate the actual string length in UTF-8 variable characters */
	cu->len_wchar = ef->len;
	cu->len = BLI_wstrlen_utf8(ef->textbuf);

	/* Alloc memory for UTF-8 variable char length string */
	cu->str = MEM_mallocN(cu->len + sizeof(wchar_t), "str");

	/* Copy the wchar to UTF-8 */
	BLI_strncpy_wchar_as_utf8(cu->str, ef->textbuf, cu->len + 1);
	
	if (cu->strinfo)
		MEM_freeN(cu->strinfo);
	cu->strinfo = MEM_callocN((cu->len_wchar + 4) * sizeof(CharInfo), "texteditinfo");
	memcpy(cu->strinfo, ef->textbufinfo, cu->len_wchar * sizeof(CharInfo));

	/* Other vars */
	cu->pos = ef->pos;
	cu->selstart = ef->selstart;
	cu->selend = ef->selend;
}

void free_editText(Object *obedit)
{
	BKE_curve_editfont_free((Curve *)obedit->data);
}

/********************** set case operator *********************/

static EnumPropertyItem case_items[] = {
	{CASE_LOWER, "LOWER", 0, "Lower", ""},
	{CASE_UPPER, "UPPER", 0, "Upper", ""},
	{0, NULL, 0, NULL, NULL}};

static int set_case(bContext *C, int ccase)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	wchar_t *str;
	int len;
	int selstart, selend;
	
	if (BKE_vfont_select_get(obedit, &selstart, &selend)) {
		len = (selend - selstart) + 1;
		str = &ef->textbuf[selstart];
		while (len) {
			if (*str >= 'a' && *str <= 'z')
				*str -= 32;
			len--;
			str++;
		}

		if (ccase == CASE_LOWER) {
			len = (selend - selstart) + 1;
			str = &ef->textbuf[selstart];
			while (len) {
				if (*str >= 'A' && *str <= 'Z') {
					*str += 32;
				}
				len--;
				str++;
			}
		}

		text_update_edited(C, obedit, FO_EDIT);
	}

	return OPERATOR_FINISHED;
}

static int set_case_exec(bContext *C, wmOperator *op)
{
	return set_case(C, RNA_enum_get(op->ptr, "case"));
}

void FONT_OT_case_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Case";
	ot->description = "Set font case";
	ot->idname = "FONT_OT_case_set";
	
	/* api callbacks */
	ot->exec = set_case_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "case", case_items, CASE_LOWER, "Case", "Lower or upper case");
}

/********************** toggle case operator *********************/

static int toggle_case_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	EditFont *ef = cu->editfont;
	wchar_t *str;
	int len, ccase = CASE_UPPER;
	
	len = wcslen(ef->textbuf);
	str = ef->textbuf;
	while (len) {
		if (*str >= 'a' && *str <= 'z') {
			ccase = CASE_LOWER;
			break;
		}

		len--;
		str++;
	}
	
	return set_case(C, ccase);
}

void FONT_OT_case_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Case";
	ot->description = "Toggle font case";
	ot->idname = "FONT_OT_case_toggle";
	
	/* api callbacks */
	ot->exec = toggle_case_exec;
	ot->poll = ED_operator_editfont;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* **************** Open Font ************** */

static void font_ui_template_init(bContext *C, wmOperator *op)
{
	PropertyPointerRNA *pprop;
	
	op->customdata = pprop = MEM_callocN(sizeof(PropertyPointerRNA), "OpenPropertyPointerRNA");
	uiIDContextProperty(C, &pprop->ptr, &pprop->prop);
}

static void font_open_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata = NULL;
}

static int font_open_exec(bContext *C, wmOperator *op)
{
	struct Main *bmain = CTX_data_main(C);
	VFont *font;
	PropertyPointerRNA *pprop;
	PointerRNA idptr;
	char filepath[FILE_MAX];
	RNA_string_get(op->ptr, "filepath", filepath);

	font = BKE_vfont_load(bmain, filepath);

	if (!font) {
		if (op->customdata) MEM_freeN(op->customdata);
		return OPERATOR_CANCELLED;
	}

	if (!op->customdata)
		font_ui_template_init(C, op);
	
	/* hook into UI */
	pprop = op->customdata;

	if (pprop->prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		font->id.us--;
	
		RNA_id_pointer_create(&font->id, &idptr);
		RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr);
		RNA_property_update(C, &pprop->ptr, pprop->prop);
	}

	MEM_freeN(op->customdata);

	return OPERATOR_FINISHED;
}

static int open_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	VFont *vfont = NULL;
	const char *path;

	PointerRNA idptr;
	PropertyPointerRNA *pprop;

	font_ui_template_init(C, op);

	/* hook into UI */
	pprop = op->customdata;

	if (pprop->prop) {
		idptr = RNA_property_pointer_get((PointerRNA *)pprop, pprop->prop);
		vfont = idptr.id.data;
	}

	path = (vfont && !BKE_vfont_is_builtin(vfont)) ? vfont->name : U.fontdir;

	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return font_open_exec(C, op);

	RNA_string_set(op->ptr, "filepath", path);
	WM_event_add_fileselect(C, op); 

	return OPERATOR_RUNNING_MODAL;
}

void FONT_OT_open(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Open Font";
	ot->idname = "FONT_OT_open";
	ot->description = "Load a new font from a file";
	
	/* api callbacks */
	ot->exec = font_open_exec;
	ot->invoke = open_invoke;
	ot->cancel = font_open_cancel;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE | FTFONTFILE, FILE_SPECIAL, FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY);
}

/******************* delete operator *********************/

static int font_unlink_exec(bContext *C, wmOperator *op)
{
	VFont *builtin_font;

	PointerRNA idptr;
	PropertyPointerRNA pprop;

	uiIDContextProperty(C, &pprop.ptr, &pprop.prop);
	
	if (pprop.prop == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Incorrect context for running font unlink");
		return OPERATOR_CANCELLED;
	}

	builtin_font = BKE_vfont_builtin_get();

	RNA_id_pointer_create(&builtin_font->id, &idptr);
	RNA_property_pointer_set(&pprop.ptr, pprop.prop, idptr);
	RNA_property_update(C, &pprop.ptr, pprop.prop);

	return OPERATOR_FINISHED;
}

void FONT_OT_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unlink";
	ot->idname = "FONT_OT_unlink";
	ot->description = "Unlink active font data block";
	
	/* api callbacks */
	ot->exec = font_unlink_exec;
}


/* **************** undo for font object ************** */

static void undoFont_to_editFont(void *strv, void *ecu, void *UNUSED(obdata))
{
	Curve *cu = (Curve *)ecu;
	EditFont *ef = cu->editfont;
	const char *str = strv;

	ef->pos = *((short *)str);
	ef->len = *((short *)(str + 2));

	memcpy(ef->textbuf, str + 4, (ef->len + 1) * sizeof(wchar_t));
	memcpy(ef->textbufinfo, str + 4 + (ef->len + 1) * sizeof(wchar_t), ef->len * sizeof(CharInfo));
	
	ef->selstart = ef->selend = 0;

}

static void *editFont_to_undoFont(void *ecu, void *UNUSED(obdata))
{
	Curve *cu = (Curve *)ecu;
	EditFont *ef = cu->editfont;
	char *str;
	
	/* The undo buffer includes [MAXTEXT+6]=actual string and [MAXTEXT+4]*sizeof(CharInfo)=charinfo */
	str = MEM_callocN((MAXTEXT + 6) * sizeof(wchar_t) + (MAXTEXT + 4) * sizeof(CharInfo), "string undo");

	/* Copy the string and string information */
	memcpy(str + 4, ef->textbuf, (ef->len + 1) * sizeof(wchar_t));
	memcpy(str + 4 + (ef->len + 1) * sizeof(wchar_t), ef->textbufinfo, ef->len * sizeof(CharInfo));

	*((short *)(str + 0)) = ef->pos;
	*((short *)(str + 2)) = ef->len;
	
	return str;
}

static void free_undoFont(void *strv)
{
	MEM_freeN(strv);
}

static void *get_undoFont(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_FONT) {
		return obedit->data;
	}
	return NULL;
}

/* and this is all the undo system needs to know */
void undo_push_font(bContext *C, const char *name)
{
	undo_editmode_push(C, name, get_undoFont, free_undoFont, undoFont_to_editFont, editFont_to_undoFont, NULL);
}

/**
 * TextBox selection
 */
bool mouse_font(bContext *C, const int mval[2], bool extend, bool deselect, bool toggle)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ViewContext vc;
	/* bias against the active, in pixels, allows cycling */
	const float active_bias_px = 4.0f;
	const float mval_fl[2] = {UNPACK2(mval)};
	const int i_actbox = max_ii(0, cu->actbox - 1);
	int i_iter, actbox_select = -1;
	const float dist = ED_view3d_select_dist_px();
	float dist_sq_best = dist * dist;

	view3d_set_viewcontext(C, &vc);

	ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

	/* currently only select active */
	(void)extend;
	(void)deselect;
	(void)toggle;

	for (i_iter = 0; i_iter < cu->totbox; i_iter++) {
		int i = (i_iter + i_actbox) % cu->totbox;
		float dist_sq_min;
		int j, j_prev;

		float obedit_co[4][3];
		float screen_co[4][2];
		rctf rect;
		int project_ok = 0;


		BKE_curve_rect_from_textbox(cu, &cu->tb[i], &rect);

		copy_v3_fl3(obedit_co[0], rect.xmin, rect.ymin, 0.0f);
		copy_v3_fl3(obedit_co[1], rect.xmin, rect.ymax, 0.0f);
		copy_v3_fl3(obedit_co[2], rect.xmax, rect.ymax, 0.0f);
		copy_v3_fl3(obedit_co[3], rect.xmax, rect.ymin, 0.0f);

		for (j = 0; j < 4; j++) {
			if (ED_view3d_project_float_object(vc.ar, obedit_co[j], screen_co[j],
			                                   V3D_PROJ_TEST_CLIP_BB) == V3D_PROJ_RET_OK)
			{
				project_ok |= (1 << j);
			}
		}

		dist_sq_min = dist_sq_best;
		for (j = 0, j_prev = 3; j < 4; j_prev = j++) {
			if ((project_ok & (1 << j)) &&
			    (project_ok & (1 << j_prev)))
			{
				const float dist_test_sq = dist_squared_to_line_segment_v2(mval_fl, screen_co[j_prev], screen_co[j]);
				if (dist_sq_min > dist_test_sq) {
					dist_sq_min = dist_test_sq;
				}
			}
		}

		/* bias in pixels to cycle seletion */
		if (i_iter == 0) {
			dist_sq_min += active_bias_px;
		}

		if (dist_sq_min < dist_sq_best) {
			dist_sq_best = dist_sq_min;
			actbox_select = i + 1;
		}
	}

	if (actbox_select != -1) {
		if (cu->actbox != actbox_select) {
			cu->actbox = actbox_select;
			WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
		}
		return true;
	}
	else {
		return false;
	}
}
