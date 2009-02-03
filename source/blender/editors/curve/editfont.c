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
#include "BLI_arithb.h"

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
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_curve.h"
#include "ED_object.h"
#include "ED_util.h"

#include "curve_intern.h"

/* XXX */
static void error() {}
static int okee() {return 0;}
/* XXX */


#define MAXTEXT	32766

int textediting=0;

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

static int insert_into_textbuf(Object *obedit, unsigned long c)
{
	Curve *cu= obedit->data;
	
	if (cu->len<MAXTEXT-1) {
		EditFont *ef= cu->editfont;
		int x;

		for(x= cu->len; x>cu->pos; x--) ef->textbuf[x]= ef->textbuf[x-1];
		for(x= cu->len; x>cu->pos; x--) ef->textbufinfo[x]= ef->textbufinfo[x-1];		
		ef->textbuf[cu->pos]= c;
		ef->textbufinfo[cu->pos] = cu->curinfo;
		ef->textbufinfo[cu->pos].kern = 0;
		if (obedit->actcol>0)
			ef->textbufinfo[cu->pos].mat_nr = obedit->actcol;
		else
			ef->textbufinfo[cu->pos].mat_nr = 0;
					
		cu->pos++;
		cu->len++;
		ef->textbuf[cu->len]='\0';

		update_string(cu);

		return 1;
	} else {
		return 0;
	}
}

void add_lorem(Scene *scene)
{
	Object *obedit= scene->obedit;
	char *p, *p2;
	int i;
	static char *lastlorem;
	
	if (lastlorem)
		p= lastlorem;
	else
		p= ED_lorem;
	
	i= rand()/(RAND_MAX/6)+4;	
		
	for (p2=p; *p2 && i; p2++) {
		insert_into_textbuf(obedit, *p2);
		if (*p2=='.') i--;
	}
	lastlorem = p2+1;
	if (strlen(lastlorem)<5) lastlorem = ED_lorem;
	
	insert_into_textbuf(obedit, '\n');
	insert_into_textbuf(obedit, '\n');	
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);

}

void load_3dtext_fs(Scene *scene, char *file) 
{
	Curve *cu= scene->obedit->data;
	EditFont *ef= cu->editfont;
	FILE *fp;
	int filelen;
	char *strp;

	fp= fopen(file, "r");
	if (!fp) return;

	fseek(fp, 0L, SEEK_END);
	filelen = ftell(fp);
	fseek(fp, 0L, SEEK_SET);	

	strp = MEM_callocN(filelen+4, "tempstr");	

	filelen = fread(strp, 1, filelen, fp);
	fclose(fp);
	strp[filelen]= 0;
	
	if(cu->len+filelen<MAXTEXT)
	{
		int tmplen;
		wchar_t *mem = MEM_callocN((sizeof(wchar_t)*filelen)+(4*sizeof(wchar_t)), "temporary");
		tmplen = utf8towchar(mem, strp);
		wcscat(ef->textbuf, mem);
		MEM_freeN(mem);
		cu->len += tmplen;
		cu->pos= cu->len;
	}
	MEM_freeN(strp);

	update_string(cu);

	DAG_object_flush_update(scene, scene->obedit, OB_RECALC_DATA);
}


void txt_export_to_object(Scene *scene, struct Text *text)
{
	Object *obedit= scene->obedit; // XXX
	ID *id;
	Curve *cu;
	struct TextLine *tmp;
	int nchars = 0;
//	char sdir[FILE_MAXDIR];
//	char sfile[FILE_MAXFILE];

	if(!text || !text->lines.first) return;

	id = (ID *)text;

	if (obedit && obedit->type==OB_FONT) return;
// XXX	check_editmode(OB_FONT);
	
	add_object(scene, OB_FONT);

	ED_object_base_init_from_view(NULL, BASACT); // XXX
	obedit= BASACT->object;
	where_is_object(scene, obedit);

	cu= obedit->data;

/*	
//		renames object, careful with long filenames.

	if (text->name) {
	//ID *find_id(char *type, char *name)	
		BLI_split_dirfile(text->name, sdir, sfile);
//		rename_id((ID *)obedit, sfile);
		rename_id((ID *)cu, sfile);
		id->us++;
	}
*/	
	cu->vfont= get_builtin_font();
	cu->vfont->id.us++;

	tmp= text->lines.first;
	while(cu->len<MAXTEXT && tmp) {
		nchars += strlen(tmp->line) + 1;
		tmp = tmp->next;
	}

	if(cu->str) MEM_freeN(cu->str);
	if(cu->strinfo) MEM_freeN(cu->strinfo);	

	cu->str= MEM_mallocN(nchars+4, "str");
	cu->strinfo= MEM_callocN((nchars+4)*sizeof(CharInfo), "strinfo");
	cu->totbox= cu->actbox= 1;
	cu->tb= MEM_callocN(MAXTEXTBOX*sizeof(TextBox), "textbox");
	cu->tb[0].w = cu->tb[0].h = 0.0;
	
	tmp= text->lines.first;
	strcpy(cu->str, tmp->line);
	cu->len= strlen(tmp->line);
	cu->pos= cu->len;

	tmp= tmp->next;

	while(cu->len<MAXTEXT && tmp) {
		strcat(cu->str, "\n");
		strcat(cu->str, tmp->line);
		cu->len+= strlen(tmp->line) + 1;
		cu->pos= cu->len;
		tmp= tmp->next;
	}

	make_editText(obedit);
	ED_object_exit_editmode(NULL, EM_FREEDATA|EM_WAITCURSOR); // XXX

}


void txt_export_to_objects(struct Text *text)
{
	Scene *scene= NULL; // XXX
	Object *obedit= NULL; // XXX
	RegionView3D *rv3d= NULL; // XXX
	ID *id;
	Curve *cu;
	struct TextLine *curline;
	int nchars;
	int linenum = 0;
	float offset[3] = {0.0,0.0,0.0};

	if(!text || !text->lines.first) return;

	id = (ID *)text;

	if (obedit && obedit->type==OB_FONT) return;
// XXX	check_editmode(OB_FONT);

	curline = text->lines.first;
	while(curline){	
		/*skip lines with no text, but still make space for them*/
		if(curline->line[0] == '\0'){
			linenum++;
			curline = curline->next;
			continue;
		}
			
		nchars = 0;	
		add_object(scene, OB_FONT);
	
		ED_object_base_init_from_view(NULL, BASACT); // XXX
		obedit= BASACT->object;
		where_is_object(scene, obedit);	
		
		/* Do the translation */
		offset[0] = 0;
		offset[1] = -linenum;
		offset[2] = 0;
	
		Mat4Mul3Vecfl(rv3d->viewinv, offset);
		
		obedit->loc[0] += offset[0];
		obedit->loc[1] += offset[1];
		obedit->loc[2] += offset[2];
		/* End Translation */
					
		cu= obedit->data;
		
		cu->vfont= get_builtin_font();
		cu->vfont->id.us++;
	
		nchars = strlen(curline->line) + 1;
	
		if(cu->str) MEM_freeN(cu->str);
		if(cu->strinfo) MEM_freeN(cu->strinfo);		
	
		cu->str= MEM_mallocN(nchars+4, "str");
		cu->strinfo= MEM_callocN((nchars+4)*sizeof(CharInfo), "strinfo");
		cu->totbox= cu->actbox= 1;
		cu->tb= MEM_callocN(MAXTEXTBOX*sizeof(TextBox), "textbox");
		cu->tb[0].w = cu->tb[0].h = 0.0;
		
		strcpy(cu->str, curline->line);
		cu->len= strlen(curline->line);
		cu->pos= cu->len;

		make_editText(obedit);
		ED_object_exit_editmode(NULL, EM_FREEDATA|EM_WAITCURSOR); // XXX

		linenum++;
		curline = curline->next;
	}
}

static short next_word(Curve *cu)
{
	short s;
	for (s=cu->pos; (cu->str[s]) && (cu->str[s]!=' ') && (cu->str[s]!='\n') &&
	                (cu->str[s]!=1) && (cu->str[s]!='\r'); s++);
	if (cu->str[s]) return(s+1); else return(s);
}

static short prev_word(Curve *cu)
{
	short s;
	
	if (cu->pos==0) return(0);
	for (s=cu->pos-2; (cu->str[s]) && (cu->str[s]!=' ') && (cu->str[s]!='\n') &&
	                (cu->str[s]!=1) && (cu->str[s]!='\r'); s--);
	if (cu->str[s]) return(s+1); else return(s);
}



static int killselection(Object *obedit, int ins)	/* 1 == new character */
{
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int selend, selstart, direction;
	int offset = 0;
	int getfrom;

	direction = BKE_font_getselection(obedit, &selstart, &selend);
	if (direction) {
		int size;
		if (ins) offset = 1;
		if (cu->pos >= selstart) cu->pos = selstart+offset;
		if ((direction == -1) && ins) {
			selstart++;
			selend++;
		}
		getfrom = selend+offset;
		if (ins==0) getfrom++;
		size = (cu->len * sizeof(wchar_t)) - (selstart * sizeof(wchar_t)) + (offset*sizeof(wchar_t));
		memmove(ef->textbuf+selstart, ef->textbuf+getfrom, size);
		memmove(ef->textbufinfo+selstart, ef->textbufinfo+getfrom, ((cu->len-selstart)+offset)*sizeof(CharInfo));
		cu->len -= (selend-selstart)+offset;
		cu->selstart = cu->selend = 0;
	}
	return(direction);
}

static void copyselection(Object *obedit)
{
	int selstart, selend;
	
	if (BKE_font_getselection(obedit, &selstart, &selend)) {
		Curve *cu= obedit->data;
		EditFont *ef= cu->editfont;
		
		memcpy(ef->copybuf, ef->textbuf+selstart, ((selend-selstart)+1)*sizeof(wchar_t));
		ef->copybuf[(selend-selstart)+1]=0;
		memcpy(ef->copybufinfo, ef->textbufinfo+selstart, ((selend-selstart)+1)*sizeof(CharInfo));	
	}
}

static void pasteselection(Object *obedit)
{
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;

	int len= wcslen(ef->copybuf);

	// Verify that the copy buffer => [copy buffer len] + cu->len < MAXTEXT
	if(cu->len + len <= MAXTEXT)
	{
		if (len) {	
			int size = (cu->len * sizeof(wchar_t)) - (cu->pos*sizeof(wchar_t)) + sizeof(wchar_t);
			memmove(ef->textbuf+cu->pos+len, ef->textbuf+cu->pos, size);
			memcpy(ef->textbuf+cu->pos, ef->copybuf, len * sizeof(wchar_t));
		
			memmove(ef->textbufinfo+cu->pos+len, ef->textbufinfo+cu->pos, (cu->len-cu->pos+1)*sizeof(CharInfo));
			memcpy(ef->textbufinfo+cu->pos, ef->copybufinfo, len*sizeof(CharInfo));	
		
			cu->len += len;
			cu->pos += len;
		}
	}
	else
	{
		error("Text too long");
	}
}

int style_to_sel(Object *obedit, int style, int toggle) 
{
	int selstart, selend;
	int i;
	
	if (obedit && (obedit->type == OB_FONT)) {
		Curve *cu= obedit->data;
		EditFont *ef= cu->editfont;
		
		if (BKE_font_getselection(obedit, &selstart, &selend)) {
			for (i=selstart; i<=selend; i++) {
				if (toggle==0) {
					ef->textbufinfo[i].flag &= ~style;
				} else {
					ef->textbufinfo[i].flag |= style;
				}
			}
			return 1;
		}
	}
	return 0;
}

int mat_to_sel(Object *obedit) 
{
	int selstart, selend;
	int i;
	
	if (obedit && (obedit->type == OB_FONT)) {
		Curve *cu= obedit->data;
		EditFont *ef= cu->editfont;
		
		if (BKE_font_getselection(obedit, &selstart, &selend)) {
			for (i=selstart; i<=selend; i++) {
				ef->textbufinfo[i].mat_nr = obedit->actcol;
			}
			return 1;
		}
	}
	return 0;
}

static int do_textedit(bContext *C, wmOperator *op, wmEvent *evt)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	static int accentcode= 0;
	int x, doit=0, cursmove=0;
	unsigned long ascii = evt->ascii;
	int alt= evt->alt, shift= evt->shift, ctrl= evt->ctrl;
	int event= evt->type, val= evt->val;
	short kern;
	
	/* tab should exit editmode, but we allow it to be typed using modifier keys */
	if(event==TABKEY) {
		if((alt||ctrl||shift) == 0)
			return OPERATOR_PASS_THROUGH;
		else
			ascii= 9;
	}
	if(event==BACKSPACEKEY)
		ascii= 0;

	if(val && ascii) {
		
		/* handle case like TAB (==9) */
		if( (ascii > 31 && ascii < 254 && ascii != 127) || (ascii==13) || (ascii==10) || (ascii==8)) {
	
			if(accentcode) {
				if(cu->pos>0) ef->textbuf[cu->pos-1]= findaccent(ef->textbuf[cu->pos-1], ascii);
				accentcode= 0;
			}
			else if(cu->len<MAXTEXT-1) {
				if(alt ) {
				
					/* might become obsolete, apple has default values for this, other OS's too? */
				
					if(ascii=='t') ascii= 137;
					else if(ascii=='c') ascii= 169;
					else if(ascii=='f') ascii= 164;
					else if(ascii=='g') ascii= 176;
					else if(ascii=='l') ascii= 163;
					else if(ascii=='r') ascii= 174;
					else if(ascii=='s') ascii= 223;
					else if(ascii=='v') ascii= 1001;
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
				if(ascii==1001) {
					int file, filelen;
					char *strp;
					
/* this should be solved by clipboard support */
#ifdef __WIN32_DISABLED 
					file= open("C:\\windows\\temp\\cutbuf", O_BINARY|O_RDONLY);
#else
					file= open("/tmp/.cutbuffer", O_BINARY|O_RDONLY);
#endif
					if(file>0) {
					
						filelen = BLI_filesize(file);
					
						strp= MEM_mallocN(filelen+4, "tempstr");
						read(file, strp, filelen);
						close(file);
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
					}
				}
				else {
					insert_into_textbuf(obedit, ascii);
				}
			}
			
			killselection(obedit, 1);
			
			doit= 1;
		}
		else
		{
			insert_into_textbuf(obedit, ascii);
			doit = 1;
		}
	}
	else if(val) {
		cursmove= 0;
		
		switch(event) {
		case ENDKEY:
			if ((shift) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;		
			while(cu->pos<cu->len) {
				if( ef->textbuf[cu->pos]==0) break;
				if( ef->textbuf[cu->pos]=='\n') break;
				if( ef->textbufinfo[cu->pos].flag & CU_WRAP ) break;
				cu->pos++;
			}
			cursmove=FO_CURS;
			break;

		case HOMEKEY:
			if ((shift) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			while(cu->pos>0) {
				if( ef->textbuf[cu->pos-1]=='\n') break;
				if( ef->textbufinfo[cu->pos-1].flag & CU_WRAP ) break;				
				cu->pos--;
			}		
			cursmove=FO_CURS;
			break;
			
		case RETKEY:
			if(ctrl) {
				insert_into_textbuf(obedit, 1);
				if (ef->textbuf[cu->pos]!='\n') insert_into_textbuf(obedit, '\n');				
			}
			else {
				insert_into_textbuf(obedit, '\n');
			}
			cu->selstart = cu->selend = 0;
			doit= 1;
			break;

		case RIGHTARROWKEY:	
			if ((shift) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			if (ctrl) {
				cu->pos= next_word(cu);
				cursmove= FO_CURS;				
			} 
			else if (alt) {
				kern = ef->textbufinfo[cu->pos-1].kern;
				kern += 1;
				if (kern>20) kern = 20;
				ef->textbufinfo[cu->pos-1].kern = kern;
				doit = 1;
			}
			else {
				cu->pos++;
				cursmove= FO_CURS;				
			}

			break;
			
		case LEFTARROWKEY:
			if ((shift) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			if (ctrl) {
				cu->pos= prev_word(cu);
				cursmove= FO_CURS;
			} 
			else if (alt) {
				kern = ef->textbufinfo[cu->pos-1].kern;
				kern -= 1;
				if (kern<-20) kern = -20;
				ef->textbufinfo[cu->pos-1].kern = kern;
				doit = 1;
			}
			else {
				cu->pos--;
				cursmove=FO_CURS;
			}
			break;

		case UPARROWKEY:
			if ((shift) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			if(alt) {
				if (cu->pos && ef->textbuf[cu->pos - 1] < 255) {
					ef->textbuf[cu->pos - 1]++;
					doit= 1;
				}
			}
			else cursmove=FO_CURSUP;
			break;
			
		case PAGEUPKEY:
			if ((shift) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cursmove=FO_PAGEUP;
			break;
			
		case DOWNARROWKEY:
			if ((shift) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			if(alt) {
				if (cu->pos && ef->textbuf[cu->pos - 1] > 1) {
					ef->textbuf[cu->pos - 1]--;
					doit= 1;
				}
			}
			else cursmove= FO_CURSDOWN;
			break;

		case PAGEDOWNKEY:
			if ((shift) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cursmove=FO_PAGEDOWN;
			break;
			
		case BACKSPACEKEY:
			if(cu->len!=0) {
				if(alt) {
					if(cu->pos>0) accentcode= 1;
				}
				else if (ctrl) {
					cu->len = cu->pos = 0;
					ef->textbuf[0]= 0;
					doit= 1;
				}
				else {
					if (killselection(obedit, 0)==0) {
						if (cu->pos>0) {
							for(x=cu->pos;x<=cu->len;x++) ef->textbuf[x-1]= ef->textbuf[x];
							for(x=cu->pos;x<=cu->len;x++) ef->textbufinfo[x-1]= ef->textbufinfo[x];					
							cu->pos--;
							ef->textbuf[--cu->len]='\0';
							doit=1;
						}
					} else doit=1;
				}
			}
			break;

		case DELKEY:
			if(cu->len!=0) {
				if (killselection(obedit, 0)==0) {
					if(cu->pos<cu->len) {					
						for(x=cu->pos;x<cu->len;x++) ef->textbuf[x]= ef->textbuf[x+1];
						for(x=cu->pos;x<cu->len;x++) ef->textbufinfo[x]= ef->textbufinfo[x+1];					
						ef->textbuf[--cu->len]='\0';
						doit=1;
					}
				} else doit=1;
			}
			break;
		
		case IKEY:
			if (ctrl) {
				cu->curinfo.flag ^= CU_ITALIC;
				if (style_to_sel(obedit, CU_ITALIC, cu->curinfo.flag & CU_ITALIC)) doit= 1;   				
			}
			break;

		case BKEY:
			if (ctrl) {
				cu->curinfo.flag ^= CU_BOLD;
				if (style_to_sel(obedit, CU_BOLD, cu->curinfo.flag & CU_BOLD)) doit= 1;
			}
			break;			
			
		case UKEY:
			if (ctrl) {
				cu->curinfo.flag ^= CU_UNDERLINE;
				if (style_to_sel(obedit, CU_UNDERLINE, cu->curinfo.flag & CU_UNDERLINE)) doit= 1;
			}
			break;
			
		case XKEY:
			if (ctrl) {
				copyselection(obedit);
				killselection(obedit, 0);
				doit= 1;
			}
			break;
			
		case CKEY:
			if (ctrl) {
				copyselection(obedit);
			}
			break;   			
			
		case VKEY:
			if (ctrl) {
				pasteselection(obedit);
				doit= 1;
			}
			break;
		default:
			return OPERATOR_PASS_THROUGH;
		}
			
		if(cursmove) {
			if ((shift)==0) {
				if (cu->selstart) {
					cu->selstart = cu->selend = 0;
					update_string(cu);
					BKE_text_to_curve(scene, obedit, FO_SELCHANGE);
				}
			}
			if(cu->pos>cu->len) cu->pos= cu->len;
			else if(cu->pos>=MAXTEXT) cu->pos= MAXTEXT;
			else if(cu->pos<0) cu->pos= 0;
		}
	}
	if(doit || cursmove) {
	
		if (cu->pos) {
			cu->curinfo = ef->textbufinfo[cu->pos-1];
		} else cu->curinfo = ef->textbufinfo[0];
		
		if (obedit->totcol>0) {
			obedit->actcol = ef->textbufinfo[cu->pos-1].mat_nr;
		}
		update_string(cu);
		BKE_text_to_curve(scene, obedit, cursmove);
		if (cursmove && (shift)) {
			cu->selend = cu->pos;
			DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
		}
		if(cursmove==0) {
			DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
		}			

		WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, NULL); // XXX better note

	}
	return OPERATOR_FINISHED;
}

static int font_editmode(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && obedit->type==OB_FONT)
		return 1;
	return 0;
}

void FONT_OT_textedit(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Edit Text";
	ot->idname= "FONT_OT_textedit";
	
	/* api callbacks */
	ot->invoke= do_textedit;
	
	ot->poll= font_editmode;
	
	ot->flag = OPTYPE_UNDO;
}


void paste_unicodeText(Scene *scene, char *filename)
{
	Object *obedit= scene->obedit; // XXX
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int filelen, doit= 0;
	char *strp;
	FILE *fp = NULL;

	fp= fopen(filename, "r");

	if(fp) {

		fseek( fp, 0L, SEEK_END );
		filelen = ftell( fp );
		fseek( fp, 0L, SEEK_SET );
			
		strp= MEM_mallocN(filelen+4, "tempstr");
		//fread() instead of read(),
		//because windows read() converts text to DOS \r\n linebreaks
		//causing double linebreaks in the 3d text
		filelen = fread(strp, 1, filelen, fp);
		fclose(fp);
		strp[filelen]= 0;


		if(cu->len+filelen<MAXTEXT) 
		{
			int tmplen;
			wchar_t *mem = MEM_callocN((sizeof(wchar_t)*filelen)+(4*sizeof(wchar_t)), "temporary");
			tmplen = utf8towchar(mem, strp);
//			mem =utf8s2wc(strp);
			wcscat(ef->textbuf, mem);
			MEM_freeN(mem);
			cu->len += tmplen;
			cu->pos= cu->len;
		}
		MEM_freeN(strp);
		doit = 1;
	}
	if(doit) {
		update_string(cu);
		BKE_text_to_curve(scene, obedit, 0);
		DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	}
}

void paste_editText(Scene *scene)
{
	Object *obedit= scene->obedit; // XXX
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int filelen, doit= 0;
	char *strp;
	FILE *fp = NULL;

#ifdef WIN32
	fp= fopen("C:\\windows\\temp\\cutbuf.txt", "r");

//	The following is more likely to work on all Win32 installations.
//	suggested by Douglas Toltzman. Needs windows include files...
/*
	char tempFileName[MAX_PATH];
	DWORD pathlen;
	static const char cutbufname[]="cutbuf.txt";

	if ((pathlen=GetTempPath(sizeof(tempFileName),tempFileName)) > 0 &&
		pathlen + sizeof(cutbufname) <= sizeof(tempFileName))
	{
		strcat(tempFileName,cutbufname);
		file= open(tempFileName, O_BINARY|O_RDONLY);
	}
*/
#else
	fp= fopen("/tmp/.cutbuffer", "r");
#endif

	if(fp) {
		
		fseek(fp, 0L, SEEK_END);		
		filelen = ftell( fp );
		fseek(fp, 0L, SEEK_SET);
				
		strp= MEM_mallocN(filelen+4, "tempstr");
		// fread() instead of read(),
		// because windows read() converts text to DOS \r\n linebreaks
		// causing double linebreaks in the 3d text
		filelen = fread(strp, 1, filelen, fp);
		fclose(fp);
		strp[filelen]= 0;
		
		if(cu->len+filelen<MAXTEXT) {
			int tmplen;
			wchar_t *mem = MEM_callocN((sizeof(wchar_t) * filelen) + (4 * sizeof(wchar_t)), "temporary");
			tmplen = utf8towchar(mem, strp);
			wcscat(ef->textbuf, mem);
			MEM_freeN(mem);
			cu->len += tmplen;
			cu->pos= cu->len;
		}
		MEM_freeN(strp);
		doit = 1;
	}
	if(doit) {
		update_string(cu);
		BKE_text_to_curve(scene, obedit, 0);
		DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	}
}

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
		ef->oldstrinfo= MEM_callocN((MAXTEXT+4)*sizeof(wchar_t), "oldstrbuf");
	}
	
	// Convert the original text to wchar_t
	utf8towchar(ef->textbuf, cu->str);
	wcscpy(ef->oldstr, ef->textbuf);
		
	cu->len= wcslen(ef->textbuf);
	
	memcpy(ef->textbufinfo, cu->strinfo, (cu->len)*sizeof(CharInfo));
	memcpy(ef->oldstrinfo, cu->strinfo, (cu->len)*sizeof(CharInfo));

	if(cu->pos>cu->len) cu->pos= cu->len;

	if (cu->pos) {
		cu->curinfo = ef->textbufinfo[cu->pos-1];
	} else cu->curinfo = ef->textbufinfo[0];
	
	// Convert to UTF-8
	update_string(cu);
	
	textediting= 1;
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
	
	if (cu->selboxes) {
		MEM_freeN(cu->selboxes);
		cu->selboxes= NULL;
	}
	
	textediting= 0;

}

void remake_editText(Object *obedit)
{
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
		
	if(okee("Reload original text")==0) return;
	
	// Copy the oldstr to textbuf temporary global variable
	wcscpy(ef->textbuf, ef->oldstr);
	memcpy(ef->textbufinfo, ef->oldstrinfo, (cu->len)*sizeof(CharInfo));

	// Set the object length and position	
	cu= obedit->data;
	cu->len= wcslen(ef->textbuf);
	if(cu->pos>cu->len) cu->pos= cu->len;

	update_string(cu);
	
}


void free_editText(Object *obedit)
{
	BKE_free_editfont((Curve *)obedit->data);

	textediting= 0;
}


void add_primitiveFont(int dummy_argument)
{
	Scene *scene= NULL; // XXX
	Object *obedit= scene->obedit;
	Curve *cu;

	if (obedit && obedit->type==OB_FONT) return;
// XXX	check_editmode(OB_FONT);
	
// XXX	add_object_draw(OB_FONT);
	ED_object_base_init_from_view(NULL, BASACT); // XXX
	
	where_is_object(scene, BASACT->object);
	
	cu= BASACT->object->data;
	
	cu->vfont= cu->vfontb= cu->vfonti= cu->vfontbi= get_builtin_font();
	cu->vfont->id.us+=4;
	cu->str= MEM_mallocN(12, "str");
	strcpy(cu->str, "Text");
	cu->pos= 4;
	cu->strinfo= MEM_callocN(12*sizeof(CharInfo), "strinfo");
	cu->totbox= cu->actbox= 1;
	cu->tb= MEM_callocN(MAXTEXTBOX*sizeof(TextBox), "textbox");
	cu->tb[0].w = cu->tb[0].h = 0.0;
	
//	if (U.flag & USER_ADD_EDITMODE) 
//		enter_editmode(EM_WAITCURSOR);

}

void to_upper(Scene *scene)
{
	Object *obedit= scene->obedit; // XXX
	Curve *cu= obedit->data;
	EditFont *ef= cu->editfont;
	int len, ok;
	wchar_t *str;
	
	if(obedit==0) {
		return;
	}
	
	ok= 0;
	cu= obedit->data;
	
	len= wcslen(ef->textbuf);
	str= ef->textbuf;
	while(len) {
		if( *str>=97 && *str<=122) {
			ok= 1;
			*str-= 32;
		}
		len--;
		str++;
	}
	
	if(ok==0) {
		len= wcslen(ef->textbuf);
		str= ef->textbuf;
		while(len) {
			if( *str>=65 && *str<=90) {
				*str+= 32;
			}
			len--;
			str++;
		}
	}
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);

	update_string(cu);
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



/***/
