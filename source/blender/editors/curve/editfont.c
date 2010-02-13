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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <wchar.h>

#ifndef WIN32 
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_vfont_types.h"
#include "DNA_scene_types.h"
#include "DNA_text_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_curve.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "UI_interface.h"

#include "curve_intern.h"

#define MAXTEXT	32766

/************************* utilities ******************************/

static char findaccent(char char1, unsigned int code)
{
	char new= 0;
	
	if(char1=='a') {
		if(code=='`') new= 224;
		else if(code==39) new= 225;
		else if(code=='^') new= 226;
		else if(code=='~') new= 227;
		else if(code=='"') new= 228;
		else if(code=='o') new= 229;
		else if(code=='e') new= 230;
		else if(code=='-') new= 170;
	}
	else if(char1=='c') {
		if(code==',') new= 231;
		if(code=='|') new= 162;
	}
	else if(char1=='e') {
		if(code=='`') new= 232;
		else if(code==39) new= 233;
		else if(code=='^') new= 234;
		else if(code=='"') new= 235;
	}
	else if(char1=='i') {
		if(code=='`') new= 236;
		else if(code==39) new= 237;
		else if(code=='^') new= 238;
		else if(code=='"') new= 239;
	}
	else if(char1=='n') {
		if(code=='~') new= 241;
	}
	else if(char1=='o') {
		if(code=='`') new= 242;
		else if(code==39) new= 243;
		else if(code=='^') new= 244;
		else if(code=='~') new= 245;
		else if(code=='"') new= 246;
		else if(code=='/') new= 248;
		else if(code=='-') new= 186;
		else if(code=='e') new= 143;
	}
	else if(char1=='s') {
		if(code=='s') new= 167;
	}
	else if(char1=='u') {
		if(code=='`') new= 249;
		else if(code==39) new= 250;
		else if(code=='^') new= 251;
		else if(code=='"') new= 252;
	}
	else if(char1=='y') {
		if(code==39) new= 253;
		else if(code=='"') new= 255;
	}
	else if(char1=='A') {
		if(code=='`') new= 192;
		else if(code==39) new= 193;
		else if(code=='^') new= 194;
		else if(code=='~') new= 195;
		else if(code=='"') new= 196;
		else if(code=='o') new= 197;
		else if(code=='e') new= 198;
	}
	else if(char1=='C') {
		if(code==',') new= 199;
	}
	else if(char1=='E') {
		if(code=='`') new= 200;
		else if(code==39) new= 201;
		else if(code=='^') new= 202;
		else if(code=='"') new= 203;
	}
	else if(char1=='I') {
		if(code=='`') new= 204;
		else if(code==39) new= 205;
		else if(code=='^') new= 206;
		else if(code=='"') new= 207;
	}
	else if(char1=='N') {
		if(code=='~') new= 209;
	}
	else if(char1=='O') {
		if(code=='`') new= 210;
		else if(code==39) new= 211;
		else if(code=='^') new= 212;
		else if(code=='~') new= 213;
		else if(code=='"') new= 214;
		else if(code=='/') new= 216;
		else if(code=='e') new= 141;
	}
	else if(char1=='U') {
		if(code=='`') new= 217;
		else if(code==39) new= 218;
		else if(code=='^') new= 219;
		else if(code=='"') new= 220;
	}
	else if(char1=='Y') {
		if(code==39) new= 221;
	}
	else if(char1=='1') {
		if(code=='4') new= 188;
		if(code=='2') new= 189;
	}
	else if(char1=='3') {
		if(code=='4') new= 190;
	}
	else if(char1==':') {
		if(code=='-') new= 247;
	}
	else if(char1=='-') {
		if(code==':') new= 247;
		if(code=='|') new= 135;
		if(code=='+') new= 177;
	}
	else if(char1=='|') {
		if(code=='-') new= 135;
		if(code=='=') new= 136;
	}
	else if(char1=='=') {
		if(code=='|') new= 136;
	}
	else if(char1=='+') {
		if(code=='-') new= 177;
	}
	
	if(new) return new;
	else return char1;
}


void update_string(Curve *cu)
{
	EditFont *ef= cu->editfont;
	int len;

	// Free the old curve string	
	MEM_freeN(cu->str);

	// Calculate the actual string length in UTF-8 variable characters
	len = wcsleninu8(ef->textbuf);

	// Alloc memory for UTF-8 variable char length string
	cu->str = MEM_callocN(len + sizeof(wchar_t), "str");

	// Copy the wchar to UTF-8
	wcs2utf8s(cu->str, ef->textbuf);
}

static int insert_into_textbuf(Object *obedit, uintptr_t c)
{
	Curve *cu= obedit->data;
	
	if(cu->len<MAXTEXT-1) {
		EditFont *ef= cu->editfont;
		int x;

		for(x= cu->len; x>cu->pos; x--) ef->textbuf[x]= ef->textbuf[x-1];
		for(x= cu->len; x>cu->pos; x--) ef->textbufinfo[x]= ef->textbufinfo[x-1];		
		ef->textbuf[cu->pos]= c;
		ef->textbufinfo[cu->pos] = cu->curinfo;
		ef->textbufinfo[cu->pos].kern = 0;
		if(obedit->actcol>0)
			ef->textbufinfo[cu->pos].mat_nr = obedit->actcol;
		else
			ef->textbufinfo[cu->pos].mat_nr = 0;
					
		cu->pos++;
		cu->len++;
		ef->textbuf[cu->len]='\0';

		update_string(cu);

		return 1;
	}
	else
		return 0;
}

static void text_update_edited(bContext *C, Scene *scene, Object *obedit, int recalc, int mode)
{
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;

	if(cu->pos)
		cu->curinfo = ef->textbufinfo[cu->pos-1];
	else
		cu->curinfo = ef->textbufinfo[0];
	
	if(obedit->totcol>0)
		obedit->actcol= ef->textbufinfo[cu->pos-1].mat_nr;

	update_string(cu);
	BKE_text_to_curve(scene, obedit, mode);

	if(recalc)
		DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);
}

/********************** insert lorem operator *********************/

static int insert_lorem_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	char *p, *p2;
	int i;
	static char *lastlorem;
	
	if(lastlorem)
		p= lastlorem;
	else
		p= ED_lorem;
	
	i= rand()/(RAND_MAX/6)+4;	
		
	for(p2=p; *p2 && i; p2++) {
		insert_into_textbuf(obedit, *p2);

		if(*p2=='.')
			i--;
	}

	lastlorem = p2+1;
	if(strlen(lastlorem)<5)
		lastlorem = ED_lorem;
	
	insert_into_textbuf(obedit, '\n');
	insert_into_textbuf(obedit, '\n');	

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void FONT_OT_insert_lorem(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Insert Lorem";
    ot->description= "Insert placeholder text";
	ot->idname= "FONT_OT_insert_lorem";
	
	/* api callbacks */
	ot->exec= insert_lorem_exec;
	ot->poll= ED_operator_editfont;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/******************* paste file operator ********************/

/* note this handles both ascii and utf8 unicode, previously
 * there were 3 functions that did effectively the same thing. */

static int paste_file(bContext *C, ReportList *reports, char *filename)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	FILE *fp;
	int filelen;
	char *strp;

	fp= fopen(filename, "r");

	if(!fp) {
		if(reports)
			BKE_reportf(reports, RPT_ERROR, "Failed to open file %s.", filename);
		return OPERATOR_CANCELLED;
	}

	fseek(fp, 0L, SEEK_END);
	filelen = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	strp= MEM_callocN(filelen+4, "tempstr");

	// fread() instead of read(), because windows read() converts text
	// to DOS \r\n linebreaks, causing double linebreaks in the 3d text
	filelen = fread(strp, 1, filelen, fp);
	fclose(fp);
	strp[filelen]= 0;

	if(cu->len+filelen<MAXTEXT) {
		int tmplen;
		wchar_t *mem = MEM_callocN((sizeof(wchar_t)*filelen)+(4*sizeof(wchar_t)), "temporary");
		tmplen = utf8towchar(mem, strp);
		wcscat(ef->textbuf, mem);
		MEM_freeN(mem);
		cu->len += tmplen;
		cu->pos= cu->len;
	}
	MEM_freeN(strp);

	text_update_edited(C, scene, obedit, 1, 0);

	return OPERATOR_FINISHED;
}

static int paste_file_exec(bContext *C, wmOperator *op)
{
	char *path;
	int retval;
	
	path= RNA_string_get_alloc(op->ptr, "path", NULL, 0);
	retval= paste_file(C, op->reports, path);
	MEM_freeN(path);

	return retval;
}

static int paste_file_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if(RNA_property_is_set(op->ptr, "path"))
		return paste_file_exec(C, op);

	WM_event_add_fileselect(C, op); 

	return OPERATOR_RUNNING_MODAL;
}

void FONT_OT_file_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Paste File";
    ot->description= "Paste contents from file";
	ot->idname= "FONT_OT_file_paste";
	
	/* api callbacks */
	ot->exec= paste_file_exec;
	ot->invoke= paste_file_invoke;
	ot->poll= ED_operator_editfont;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|TEXTFILE, FILE_SPECIAL, FILE_OPENFILE);
}

/******************* paste buffer operator ********************/

static int paste_buffer_exec(bContext *C, wmOperator *op)
{
	char *filename;

#ifdef WIN32
	filename= "C:\\windows\\temp\\cutbuf.txt";

//	The following is more likely to work on all Win32 installations.
//	suggested by Douglas Toltzman. Needs windows include files...
/*
	char tempFileName[MAX_PATH];
	DWORD pathlen;
	static const char cutbufname[]="cutbuf.txt";

	if((pathlen=GetTempPath(sizeof(tempFileName),tempFileName)) > 0 &&
		pathlen + sizeof(cutbufname) <= sizeof(tempFileName))
	{
		strcat(tempFileName,cutbufname);
		filename= tempFilename;
	}
*/
#else
	filename= "/tmp/.cutbuffer";
#endif

	return paste_file(C, NULL, filename);
}

void FONT_OT_buffer_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Paste Buffer";
    ot->description= "Paste text from OS buffer";
	ot->idname= "FONT_OT_buffer_paste";
	
	/* api callbacks */
	ot->exec= paste_buffer_exec;
	ot->poll= ED_operator_editfont;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/******************* text to object operator ********************/

static void txt_add_object(bContext *C, TextLine *firstline, int totline, float offset[3])
{
	Scene *scene= CTX_data_scene(C);
	Curve *cu;
	Object *obedit;
	Base *base;
	struct TextLine *tmp;
	int nchars = 0, a;
	float rot[3] = {0.f, 0.f, 0.f};
	
	obedit= add_object(scene, OB_FONT);
	base= scene->basact;

	
	ED_object_base_init_transform(C, base, NULL, rot); /* seems to assume view align ? TODO - look into this, could be an operator option */
	where_is_object(scene, obedit);

	obedit->loc[0] += offset[0];
	obedit->loc[1] += offset[1];
	obedit->loc[2] += offset[2];

	cu= obedit->data;
	cu->vfont= get_builtin_font();
	cu->vfont->id.us++;

	for(tmp=firstline, a=0; cu->len<MAXTEXT && a<totline; tmp=tmp->next, a++)
		nchars += strlen(tmp->line) + 1;

	if(cu->str) MEM_freeN(cu->str);
	if(cu->strinfo) MEM_freeN(cu->strinfo);	

	cu->str= MEM_callocN(nchars+4, "str");
	cu->strinfo= MEM_callocN((nchars+4)*sizeof(CharInfo), "strinfo");

	cu->str[0]= '\0';
	cu->len= 0;
	cu->pos= 0;
	
	for(tmp=firstline, a=0; cu->len<MAXTEXT && a<totline; tmp=tmp->next, a++) {
		strcat(cu->str, tmp->line);
		cu->len+= strlen(tmp->line);

		if(tmp->next) {
			strcat(cu->str, "\n");
			cu->len++;
		}

		cu->pos= cu->len;
	}

	WM_event_add_notifier(C, NC_OBJECT|NA_ADDED, obedit);
}

void ED_text_to_object(bContext *C, Text *text, int split_lines)
{
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	TextLine *line;
	float offset[3];
	int linenum= 0;

	if(!text || !text->lines.first) return;

	if(split_lines) {
		for(line=text->lines.first; line; line=line->next) {
			/* skip lines with no text, but still make space for them */
			if(line->line[0] == '\0') {
				linenum++;
				continue;
			}
	
			/* do the translation */
			offset[0] = 0;
			offset[1] = -linenum;
			offset[2] = 0;
	
			if(rv3d)
				mul_mat3_m4_v3(rv3d->viewinv, offset);

			txt_add_object(C, line, 1, offset);

			linenum++;
		}
	}
	else {
		offset[0]= 0.0f;
		offset[1]= 0.0f;
		offset[2]= 0.0f;

		txt_add_object(C, text->lines.first, BLI_countlist(&text->lines), offset);
	}
}

/********************** utilities ***************************/

static short next_word(Curve *cu)
{
	short s;
	for(s=cu->pos; (cu->str[s]) && (cu->str[s]!=' ') && (cu->str[s]!='\n') &&
	                (cu->str[s]!=1) && (cu->str[s]!='\r'); s++);
	if(cu->str[s]) return(s+1); else return(s);
}

static short prev_word(Curve *cu)
{
	short s;
	
	if(cu->pos==0) return(0);
	for(s=cu->pos-2; (cu->str[s]) && (cu->str[s]!=' ') && (cu->str[s]!='\n') &&
	                (cu->str[s]!=1) && (cu->str[s]!='\r'); s--);
	if(cu->str[s]) return(s+1); else return(s);
}

static int kill_selection(Object *obedit, int ins)	/* 1 == new character */
{
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int selend, selstart, direction;
	int offset = 0;
	int getfrom;

	direction = BKE_font_getselection(obedit, &selstart, &selend);
	if(direction) {
		int size;
		if(ins) offset = 1;
		if(cu->pos >= selstart) cu->pos = selstart+offset;
		if((direction == -1) && ins) {
			selstart++;
			selend++;
		}
		getfrom = selend+offset;
		if(ins==0) getfrom++;
		size = (cu->len * sizeof(wchar_t)) - (selstart * sizeof(wchar_t)) + (offset*sizeof(wchar_t));
		memmove(ef->textbuf+selstart, ef->textbuf+getfrom, size);
		memmove(ef->textbufinfo+selstart, ef->textbufinfo+getfrom, ((cu->len-selstart)+offset)*sizeof(CharInfo));
		cu->len -= (selend-selstart)+offset;
		cu->selstart = cu->selend = 0;
	}

	return(direction);
}

/******************* set style operator ********************/

static EnumPropertyItem style_items[]= {
	{CU_BOLD, "BOLD", 0, "Bold", ""},
	{CU_ITALIC, "ITALIC", 0, "Italic", ""},
	{CU_UNDERLINE, "UNDERLINE", 0, "Underline", ""},
	{0, NULL, 0, NULL, NULL}};

static int set_style(bContext *C, int style, int clear)
{
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int i, selstart, selend;

	if(!BKE_font_getselection(obedit, &selstart, &selend))
		return OPERATOR_CANCELLED;

	for(i=selstart; i<=selend; i++) {
		if(clear)
			ef->textbufinfo[i].flag &= ~style;
		else
			ef->textbufinfo[i].flag |= style;
	}

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

static int set_style_exec(bContext *C, wmOperator *op)
{
	int style, clear;

	style= RNA_enum_get(op->ptr, "style");
	clear= RNA_enum_get(op->ptr, "clear");

	return set_style(C, style, clear);
}

void FONT_OT_style_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Style";
    ot->description= "Set font style";
	ot->idname= "FONT_OT_style_set";
	
	/* api callbacks */
	ot->exec= set_style_exec;
	ot->poll= ED_operator_editfont;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "style", style_items, CU_BOLD, "Style", "Style to set selection to.");
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "Clear style rather than setting it.");
}

/******************* toggle style operator ********************/

static int toggle_style_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	int style, clear, selstart, selend;

	if(!BKE_font_getselection(obedit, &selstart, &selend))
		return OPERATOR_CANCELLED;
	
	style= RNA_enum_get(op->ptr, "style");

	cu->curinfo.flag ^= style;
	clear= (cu->curinfo.flag & style) == 0;

	return set_style(C, style, clear);
}

void FONT_OT_style_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Style";
    ot->description= "Toggle font style";
	ot->idname= "FONT_OT_style_toggle";
	
	/* api callbacks */
	ot->exec= toggle_style_exec;
	ot->poll= ED_operator_editfont;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "style", style_items, CU_BOLD, "Style", "Style to set selection to.");
}

/******************* copy text operator ********************/

static void copy_selection(Object *obedit)
{
	int selstart, selend;
	
	if(BKE_font_getselection(obedit, &selstart, &selend)) {
		Curve *cu= obedit->data;
		EditFont *ef= cu->editfont;
		
		memcpy(ef->copybuf, ef->textbuf+selstart, ((selend-selstart)+1)*sizeof(wchar_t));
		ef->copybuf[(selend-selstart)+1]=0;
		memcpy(ef->copybufinfo, ef->textbufinfo+selstart, ((selend-selstart)+1)*sizeof(CharInfo));	
	}
}

static int copy_text_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);

	copy_selection(obedit);

	return OPERATOR_FINISHED;
}

void FONT_OT_text_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy Text";
    ot->description= "Copy selected text to clipboard";
	ot->idname= "FONT_OT_text_copy";
	
	/* api callbacks */
	ot->exec= copy_text_exec;
	ot->poll= ED_operator_editfont;
}

/******************* cut text operator ********************/

static int cut_text_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	int selstart, selend;

	if(!BKE_font_getselection(obedit, &selstart, &selend))
		return OPERATOR_CANCELLED;

	copy_selection(obedit);
	kill_selection(obedit, 0);

	text_update_edited(C, scene, obedit, 1, 0);

	return OPERATOR_FINISHED;
}

void FONT_OT_text_cut(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Cut Text";
    ot->description= "Cut selected text to clipboard";
	ot->idname= "FONT_OT_text_cut";
	
	/* api callbacks */
	ot->exec= cut_text_exec;
	ot->poll= ED_operator_editfont;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/******************* paste text operator ********************/

static int paste_selection(Object *obedit, ReportList *reports)
{
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int len= wcslen(ef->copybuf);

	// Verify that the copy buffer => [copy buffer len] + cu->len < MAXTEXT
	if(cu->len + len <= MAXTEXT) {
		if(len) {	
			int size = (cu->len * sizeof(wchar_t)) - (cu->pos*sizeof(wchar_t)) + sizeof(wchar_t);
			memmove(ef->textbuf+cu->pos+len, ef->textbuf+cu->pos, size);
			memcpy(ef->textbuf+cu->pos, ef->copybuf, len * sizeof(wchar_t));
		
			memmove(ef->textbufinfo+cu->pos+len, ef->textbufinfo+cu->pos, (cu->len-cu->pos+1)*sizeof(CharInfo));
			memcpy(ef->textbufinfo+cu->pos, ef->copybufinfo, len*sizeof(CharInfo));	
		
			cu->len += len;
			cu->pos += len;

			return 1;
		}
	}
	else
		BKE_report(reports, RPT_WARNING, "Text too long.");
	
	return 0;
}

static int paste_text_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);

	if(!paste_selection(obedit, op->reports))
		return OPERATOR_CANCELLED;

	text_update_edited(C, scene, obedit, 1, 0);

	return OPERATOR_FINISHED;
}

void FONT_OT_text_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Paste Text";
    ot->description= "Paste text from clipboard";
	ot->idname= "FONT_OT_text_paste";
	
	/* api callbacks */
	ot->exec= paste_text_exec;
	ot->poll= ED_operator_editfont;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ move operator ************************/

static EnumPropertyItem move_type_items[]= {
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

static int move_cursor(bContext *C, int type, int select)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int cursmove= 0;

	switch(type) {
		case LINE_BEGIN:
			if((select) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			while(cu->pos>0) {
				if(ef->textbuf[cu->pos-1]=='\n') break;
				if(ef->textbufinfo[cu->pos-1].flag & CU_WRAP ) break;				
				cu->pos--;
			}		
			cursmove=FO_CURS;
			break;
			
		case LINE_END:
			if((select) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;		
			while(cu->pos<cu->len) {
				if(ef->textbuf[cu->pos]==0) break;
				if(ef->textbuf[cu->pos]=='\n') break;
				if(ef->textbufinfo[cu->pos].flag & CU_WRAP ) break;
				cu->pos++;
			}
			cursmove=FO_CURS;
			break;

		case PREV_WORD:
			if((select) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cu->pos= prev_word(cu);
			cursmove= FO_CURS;
			break;

		case NEXT_WORD:
			if((select) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cu->pos= next_word(cu);
			cursmove= FO_CURS;				
			break;

		case PREV_CHAR:
			if((select) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cu->pos--;
			cursmove=FO_CURS;
			break;

		case NEXT_CHAR:	
			if((select) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cu->pos++;
			cursmove= FO_CURS;				

			break;

		case PREV_LINE:
			if((select) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cursmove=FO_CURSUP;
			break;
			
		case NEXT_LINE:
			if((select) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cursmove= FO_CURSDOWN;
			break;

		case PREV_PAGE:
			if((select) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cursmove=FO_PAGEUP;
			break;

		case NEXT_PAGE:
			if((select) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cursmove=FO_PAGEDOWN;
			break;
	}
		
	if(!cursmove)
		return OPERATOR_CANCELLED;

	if(select == 0) {
		if(cu->selstart) {
			cu->selstart = cu->selend = 0;
			update_string(cu);
			BKE_text_to_curve(scene, obedit, FO_SELCHANGE);
		}
	}

	if(cu->pos>cu->len) cu->pos= cu->len;
	else if(cu->pos>=MAXTEXT) cu->pos= MAXTEXT;
	else if(cu->pos<0) cu->pos= 0;

	text_update_edited(C, scene, obedit, select, cursmove);

	if(select)
		cu->selend = cu->pos;

	return OPERATOR_FINISHED;
}

static int move_exec(bContext *C, wmOperator *op)
{
	int type= RNA_enum_get(op->ptr, "type");

	return move_cursor(C, type, 0);
}

void FONT_OT_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Cursor";
    ot->description= "Move cursor to position type";
	ot->idname= "FONT_OT_move";
	
	/* api callbacks */
	ot->exec= move_exec;
	ot->poll= ED_operator_editfont;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "type", move_type_items, LINE_BEGIN, "Type", "Where to move cursor to.");
}

/******************* move select operator ********************/

static int move_select_exec(bContext *C, wmOperator *op)
{
	int type= RNA_enum_get(op->ptr, "type");

	return move_cursor(C, type, 1);
}

void FONT_OT_move_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Select";
    ot->description= "Make selection from current cursor position to new cursor position type";
	ot->idname= "FONT_OT_move_select";
	
	/* api callbacks */
	ot->exec= move_select_exec;
	ot->poll= ED_operator_editfont;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "type", move_type_items, LINE_BEGIN, "Type", "Where to move cursor to, to make a selection.");
}

/************************* change spacing **********************/

static int change_spacing_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int kern, delta= RNA_int_get(op->ptr, "delta");

	kern = ef->textbufinfo[cu->pos-1].kern;
	kern += delta;
	CLAMP(kern, -20, 20);

	if(ef->textbufinfo[cu->pos-1].kern == kern)
		return OPERATOR_CANCELLED;

	ef->textbufinfo[cu->pos-1].kern = kern;

	text_update_edited(C, scene, obedit, 1, 0);

	return OPERATOR_FINISHED;
}

void FONT_OT_change_spacing(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Change Spacing";
    ot->description= "Change font spacing";
	ot->idname= "FONT_OT_change_spacing";
	
	/* api callbacks */
	ot->exec= change_spacing_exec;
	ot->poll= ED_operator_editfont;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "delta", 1, -20, 20, "Delta", "Amount to decrease or increasing character spacing with.", -20, 20);
}

/************************* change character **********************/

static int change_character_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int character, delta= RNA_int_get(op->ptr, "delta");

	if(cu->pos <= 0)
		return OPERATOR_CANCELLED;

	character= ef->textbuf[cu->pos - 1];
	character += delta;
	CLAMP(character, 0, 255);

	if(character == ef->textbuf[cu->pos - 1])
		return OPERATOR_CANCELLED;

	ef->textbuf[cu->pos - 1]= character;

	text_update_edited(C, scene, obedit, 1, 0);

	return OPERATOR_FINISHED;
}

void FONT_OT_change_character(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Change Character";
    ot->description= "Change font character code";
	ot->idname= "FONT_OT_change_character";
	
	/* api callbacks */
	ot->exec= change_character_exec;
	ot->poll= ED_operator_editfont;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "delta", 1, -255, 255, "Delta", "Number to increase or decrease character code with.", -255, 255);
}

/******************* line break operator ********************/

static int line_break_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int ctrl= RNA_enum_get(op->ptr, "ctrl");

	if(ctrl) {
		insert_into_textbuf(obedit, 1);
		if(ef->textbuf[cu->pos]!='\n')
			insert_into_textbuf(obedit, '\n');
	}
	else
		insert_into_textbuf(obedit, '\n');

	cu->selstart = cu->selend = 0;

	text_update_edited(C, scene, obedit, 1, 0);

	return OPERATOR_FINISHED;
}

void FONT_OT_line_break(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Line Break";
    ot->description= "Insert line break at cursor position";
	ot->idname= "FONT_OT_line_break";
	
	/* api callbacks */
	ot->exec= line_break_exec;
	ot->poll= ED_operator_editfont;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "ctrl", 0, "Ctrl", ""); // XXX what is this?
}

/******************* delete operator **********************/

static EnumPropertyItem delete_type_items[]= {
	{DEL_ALL, "ALL", 0, "All", ""},
	{DEL_NEXT_CHAR, "NEXT_CHARACTER", 0, "Next Character", ""},
	{DEL_PREV_CHAR, "PREVIOUS_CHARACTER", 0, "Previous Character", ""},
	{DEL_SELECTION, "SELECTION", 0, "Selection", ""},
	{DEL_NEXT_SEL, "NEXT_OR_SELECTION", 0, "Next or Selection", ""},
	{DEL_PREV_SEL, "PREVIOUS_OR_SELECTION", 0, "Previous or Selection", ""},
	{0, NULL, 0, NULL, NULL}};

static int delete_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int x, selstart, selend, type= RNA_enum_get(op->ptr, "type");

	if(cu->len == 0)
		return OPERATOR_CANCELLED;

	if(BKE_font_getselection(obedit, &selstart, &selend)) {
		if(type == DEL_NEXT_SEL) type= DEL_SELECTION;
		else if(type == DEL_PREV_SEL) type= DEL_SELECTION;
	}
	else {
		if(type == DEL_NEXT_SEL) type= DEL_NEXT_CHAR;
		else if(type == DEL_PREV_SEL) type= DEL_PREV_CHAR;
	}

	switch(type) {
		case DEL_ALL:
			cu->len = cu->pos = 0;
			ef->textbuf[0]= 0;
			break;
		case DEL_SELECTION:
			if(!kill_selection(obedit, 0))
				return OPERATOR_CANCELLED;
			break;
		case DEL_PREV_CHAR:
			if(cu->pos<=0)
				return OPERATOR_CANCELLED;

			for(x=cu->pos;x<=cu->len;x++)
				ef->textbuf[x-1]= ef->textbuf[x];
			for(x=cu->pos;x<=cu->len;x++)
				ef->textbufinfo[x-1]= ef->textbufinfo[x];					

			cu->pos--;
			ef->textbuf[--cu->len]='\0';
			break;
		case DEL_NEXT_CHAR:
			if(cu->pos>=cu->len)
				return OPERATOR_CANCELLED;

			for(x=cu->pos;x<cu->len;x++)
				ef->textbuf[x]= ef->textbuf[x+1];
			for(x=cu->pos;x<cu->len;x++)
				ef->textbufinfo[x]= ef->textbufinfo[x+1];					

			ef->textbuf[--cu->len]='\0';
			break;
		default:
			return OPERATOR_CANCELLED;
	}

	text_update_edited(C, scene, obedit, 1, 0);

	return OPERATOR_FINISHED;
}

void FONT_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete";
    ot->description= "Delete text by cursor position";
	ot->idname= "FONT_OT_delete";
	
	/* api callbacks */
	ot->exec= delete_exec;
	ot->poll= ED_operator_editfont;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "type", delete_type_items, DEL_ALL, "Type", "Which part of the text to delete.");
}

/*********************** insert text operator *************************/

static int insert_text_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	char *inserted_utf8;
	wchar_t *inserted_text, first;
	int len;

	if(!RNA_property_is_set(op->ptr, "text"))
		return OPERATOR_CANCELLED;
	
	inserted_utf8= RNA_string_get_alloc(op->ptr, "text", NULL, 0);
	len= strlen(inserted_utf8);

	inserted_text= MEM_callocN(sizeof(wchar_t)*(len+1), "FONT_insert_text");
	utf8towchar(inserted_text, inserted_utf8);
	first= inserted_text[0];

	MEM_freeN(inserted_text);
	MEM_freeN(inserted_utf8);

	if(!first)
		return OPERATOR_CANCELLED;

	insert_into_textbuf(obedit, first);
	kill_selection(obedit, 1);
	text_update_edited(C, scene, obedit, 1, 0);

	return OPERATOR_FINISHED;
}

static int insert_text_invoke(bContext *C, wmOperator *op, wmEvent *evt)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	static int accentcode= 0;
	uintptr_t ascii = evt->ascii;
	int alt= evt->alt, shift= evt->shift, ctrl= evt->ctrl;
	int event= evt->type, val= evt->val;
	wchar_t inserted_text[2]= {0};

	if(RNA_property_is_set(op->ptr, "text"))
		return insert_text_exec(C, op);
	
	/* tab should exit editmode, but we allow it to be typed using modifier keys */
	if(event==TABKEY) {
		if((alt||ctrl||shift) == 0)
			return OPERATOR_PASS_THROUGH;
		else
			ascii= 9;
	}
	else if(event==BACKSPACEKEY)
		ascii= 0;

	if(val && ascii) {
		/* handle case like TAB (== 9) */
		if((ascii > 31 && ascii < 254 && ascii != 127) || (ascii==13) || (ascii==10) || (ascii==8)) {
			if(accentcode) {
				if(cu->pos>0) {
					inserted_text[0]= findaccent(ef->textbuf[cu->pos-1], ascii);
					ef->textbuf[cu->pos-1]= inserted_text[0];
				}
				accentcode= 0;
			}
			else if(cu->len<MAXTEXT-1) {
				if(alt) {
					/* might become obsolete, apple has default values for this, other OS's too? */
					if(ascii=='t') ascii= 137;
					else if(ascii=='c') ascii= 169;
					else if(ascii=='f') ascii= 164;
					else if(ascii=='g') ascii= 176;
					else if(ascii=='l') ascii= 163;
					else if(ascii=='r') ascii= 174;
					else if(ascii=='s') ascii= 223;
					else if(ascii=='y') ascii= 165;
					else if(ascii=='.') ascii= 138;
					else if(ascii=='1') ascii= 185;
					else if(ascii=='2') ascii= 178;
					else if(ascii=='3') ascii= 179;
					else if(ascii=='%') ascii= 139;
					else if(ascii=='?') ascii= 191;
					else if(ascii=='!') ascii= 161;
					else if(ascii=='x') ascii= 215;
					else if(ascii=='>') ascii= 187;
					else if(ascii=='<') ascii= 171;
				}

				inserted_text[0]= ascii;
				insert_into_textbuf(obedit, ascii);
			}
			
			kill_selection(obedit, 1);
			text_update_edited(C, scene, obedit, 1, 0);
		}
		else {
			inserted_text[0]= ascii;
			insert_into_textbuf(obedit, ascii);
			text_update_edited(C, scene, obedit, 1, 0);
		}
	}
	else if(val && event == BACKSPACEKEY) {
		if(alt && cu->len!=0 && cu->pos>0)
			accentcode= 1;

		return OPERATOR_PASS_THROUGH;
	}
	else
		return OPERATOR_PASS_THROUGH;

	if(inserted_text[0]) {
		/* store as utf8 in RNA string */
		char inserted_utf8[8] = {0};

		wcs2utf8s(inserted_utf8, inserted_text);
		RNA_string_set(op->ptr, "text", inserted_utf8);
	}

	return OPERATOR_FINISHED;
}

void FONT_OT_text_insert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Insert Text";
    ot->description= "Insert text at cursor position";
	ot->idname= "FONT_OT_text_insert";
	
	/* api callbacks */
	ot->exec= insert_text_exec;
	ot->invoke= insert_text_invoke;
	ot->poll= ED_operator_editfont;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "text", "", 0, "Text", "Text to insert at the cursor position.");
}

/***************** editmode enter/exit ********************/

void make_editText(Object *obedit)
{
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	
	if(ef==NULL) {
		ef= cu->editfont= MEM_callocN(sizeof(EditFont), "editfont");
	
		ef->textbuf= MEM_callocN((MAXTEXT+4)*sizeof(wchar_t), "texteditbuf");
		ef->textbufinfo= MEM_callocN((MAXTEXT+4)*sizeof(CharInfo), "texteditbufinfo");
		ef->copybuf= MEM_callocN((MAXTEXT+4)*sizeof(wchar_t), "texteditcopybuf");
		ef->copybufinfo= MEM_callocN((MAXTEXT+4)*sizeof(CharInfo), "texteditcopybufinfo");	
		ef->oldstr= MEM_callocN((MAXTEXT+4)*sizeof(wchar_t), "oldstrbuf");
		ef->oldstrinfo= MEM_callocN((MAXTEXT+4)*sizeof(CharInfo), "oldstrbuf");
	}
	
	// Convert the original text to wchar_t
	utf8towchar(ef->textbuf, cu->str);
	wcscpy(ef->oldstr, ef->textbuf);
		
	cu->len= wcslen(ef->textbuf);
	
	memcpy(ef->textbufinfo, cu->strinfo, (cu->len)*sizeof(CharInfo));
	memcpy(ef->oldstrinfo, cu->strinfo, (cu->len)*sizeof(CharInfo));

	if(cu->pos>cu->len) cu->pos= cu->len;

	if(cu->pos)
		cu->curinfo = ef->textbufinfo[cu->pos-1];
	else
		cu->curinfo = ef->textbufinfo[0];
	
	// Convert to UTF-8
	update_string(cu);
}

void load_editText(Object *obedit)
{
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	
	MEM_freeN(ef->oldstr);
	ef->oldstr= NULL;
	MEM_freeN(ef->oldstrinfo);
	ef->oldstrinfo= NULL;
	
	update_string(cu);
	
	if(cu->strinfo)
		MEM_freeN(cu->strinfo);
	cu->strinfo= MEM_callocN((cu->len+4)*sizeof(CharInfo), "texteditinfo");
	memcpy(cu->strinfo, ef->textbufinfo, (cu->len)*sizeof(CharInfo));

	cu->len= strlen(cu->str);
	
	/* this memory system is weak... */
	
	if(cu->selboxes) {
		MEM_freeN(cu->selboxes);
		cu->selboxes= NULL;
	}
}

void free_editText(Object *obedit)
{
	BKE_free_editfont((Curve *)obedit->data);
}

/********************** set case operator *********************/

static EnumPropertyItem case_items[]= {
	{CASE_LOWER, "LOWER", 0, "Lower", ""},
	{CASE_UPPER, "UPPER", 0, "Upper", ""},
	{0, NULL, 0, NULL, NULL}};

static int set_case(bContext *C, int ccase)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	wchar_t *str;
	int len;
	
	len= wcslen(ef->textbuf);
	str= ef->textbuf;
	while(len) {
		if(*str>='a' && *str<='z')
			*str-= 32;
		len--;
		str++;
	}
	
	if(ccase == CASE_LOWER) {
		len= wcslen(ef->textbuf);
		str= ef->textbuf;
		while(len) {
			if(*str>='A' && *str<='Z') {
				*str+= 32;
			}
			len--;
			str++;
		}
	}

	text_update_edited(C, scene, obedit, 1, 0);

	return OPERATOR_FINISHED;
}

static int set_case_exec(bContext *C, wmOperator *op)
{
	return set_case(C, RNA_enum_get(op->ptr, "case"));
}

void FONT_OT_case_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Case";
    ot->description= "Set font case";
	ot->idname= "FONT_OT_case_set";
	
	/* api callbacks */
	ot->exec= set_case_exec;
	ot->poll= ED_operator_editfont;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "case", case_items, CASE_LOWER, "Case", "Lower or upper case.");
}

/********************** toggle case operator *********************/

static int toggle_case_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	wchar_t *str;
	int len, ccase= CASE_UPPER;
	
	len= wcslen(ef->textbuf);
	str= ef->textbuf;
	while(len) {
		if(*str>='a' && *str<='z') {
			ccase= CASE_LOWER;
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
	ot->name= "Toggle Case";
    ot->description= "Toggle font case";
	ot->idname= "FONT_OT_case_toggle";
	
	/* api callbacks */
	ot->exec= toggle_case_exec;
	ot->poll= ED_operator_editfont;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* **************** Open Font ************** */

static void open_init(bContext *C, wmOperator *op)
{
	PropertyPointerRNA *pprop;
	
	op->customdata= pprop= MEM_callocN(sizeof(PropertyPointerRNA), "OpenPropertyPointerRNA");
	uiIDContextProperty(C, &pprop->ptr, &pprop->prop);
}

static int open_cancel(bContext *C, wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata= NULL;
	return OPERATOR_CANCELLED;
}

static int open_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Curve *cu;
	VFont *font;
	PropertyPointerRNA *pprop;
	PointerRNA idptr;
	char str[FILE_MAX];
	
	RNA_string_get(op->ptr, "path", str);

	font = load_vfont(str);
	
	if(!font) {
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
		font->id.us--;
		
		RNA_id_pointer_create(&font->id, &idptr);
		RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr);
		RNA_property_update(C, &pprop->ptr, pprop->prop);
	} else if(ob && ob->type == OB_FONT) {
		cu = ob->data;
		id_us_min(&cu->vfont->id);
		cu->vfont = font;
	}
	
	DAG_id_flush_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA|NA_EDITED, ob->data);
	
	MEM_freeN(op->customdata);
	
	return OPERATOR_FINISHED;
}

static int open_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *ob = CTX_data_active_object(C);
	Curve *cu;
	VFont *font=NULL;
	char *path;
	if (ob && ob->type == OB_FONT) {
		cu = ob->data;
		font = cu->vfont;
	}
	path = (font && font->name)? font->name: U.fontdir;
	 
	if(RNA_property_is_set(op->ptr, "path"))
		return open_exec(C, op);
	
	open_init(C, op);
	
	RNA_string_set(op->ptr, "path", path);
	WM_event_add_fileselect(C, op); 

	return OPERATOR_RUNNING_MODAL;
}

void FONT_OT_open(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Open";
	ot->idname= "FONT_OT_open";
	
	/* api callbacks */
	ot->exec= open_exec;
	ot->invoke= open_invoke;
	ot->cancel= open_cancel;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|FTFONTFILE, FILE_SPECIAL, FILE_OPENFILE);
}

/******************* delete operator *********************/
static int font_unlink_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	Curve *cu;
	
	if (!ED_operator_object_active_editable(C) ) return 0;
	if (ob->type != OB_FONT) return 0;
	
	cu = ob->data;
	if (cu && strcmp(cu->vfont->name, "<builtin>")==0) return 0;
	return 1;
}

static int font_unlink_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Curve *cu;
	VFont *font, *builtin_font;
		
	cu = ob->data;
	font = cu->vfont;
	
	if (!font) {
		BKE_report(op->reports, RPT_ERROR, "No font datablock available to unlink.");
		return OPERATOR_CANCELLED;
	}
	
	if (strcmp(font->name, "<builtin>")==0) {
		BKE_report(op->reports, RPT_WARNING, "Can't unlink the default builtin font.");
		return OPERATOR_FINISHED;
	}

	/* revert back to builtin font */
	builtin_font = get_builtin_font();

	cu->vfont = builtin_font;
	id_us_plus(&cu->vfont->id);
	id_us_min(&font->id);
	
	DAG_id_flush_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA|NA_EDITED, ob->data);
	
	return OPERATOR_FINISHED;
}

void FONT_OT_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Unlink";
	ot->idname= "FONT_OT_unlink";
	ot->description= "Unlink active font data block";
	
	/* api callbacks */
	ot->exec= font_unlink_exec;
	ot->poll= font_unlink_poll;
}


/* **************** undo for font object ************** */

static void undoFont_to_editFont(void *strv, void *ecu)
{
	Curve *cu= (Curve *)ecu;
	EditFont *ef= cu->editfont;
	char *str= strv;

	cu->pos= *((short *)str);
	cu->len= *((short *)(str+2));

	memcpy(ef->textbuf, str+4, (cu->len+1)*sizeof(wchar_t));
	memcpy(ef->textbufinfo, str+4 + (cu->len+1)*sizeof(wchar_t), cu->len*sizeof(CharInfo));
	
	cu->selstart = cu->selend = 0;
	
	update_string(cu);
}

static void *editFont_to_undoFont(void *ecu)
{
	Curve *cu= (Curve *)ecu;
	EditFont *ef= cu->editfont;
	char *str;
	
	// The undo buffer includes [MAXTEXT+6]=actual string and [MAXTEXT+4]*sizeof(CharInfo)=charinfo
	str= MEM_callocN((MAXTEXT+6)*sizeof(wchar_t) + (MAXTEXT+4)*sizeof(CharInfo), "string undo");

	// Copy the string and string information
	memcpy(str+4, ef->textbuf, (cu->len+1)*sizeof(wchar_t));
	memcpy(str+4 + (cu->len+1)*sizeof(wchar_t), ef->textbufinfo, cu->len*sizeof(CharInfo));

	*((short *)str)= cu->pos;
	*((short *)(str+2))= cu->len;	
	
	return str;
}

static void free_undoFont(void *strv)
{
	MEM_freeN(strv);
}

static void *get_undoFont(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && obedit->type==OB_FONT) {
		return obedit->data;
	}
	return NULL;
}

/* and this is all the undo system needs to know */
void undo_push_font(bContext *C, char *name)
{
	undo_editmode_push(C, name, get_undoFont, free_undoFont, undoFont_to_editFont, editFont_to_undoFont, NULL);
}
