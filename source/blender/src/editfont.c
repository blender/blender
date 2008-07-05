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
#include <fcntl.h>
#include <wchar.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32 
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MTC_matrixops.h"

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

#include "BKE_depsgraph.h"
#include "BKE_font.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"

#include "BIF_editfont.h"
#include "BIF_editmode_undo.h"
#include "BIF_toolbox.h"
#include "BIF_space.h"
#include "BIF_mywindow.h"

#include "BDR_editobject.h"

#include "mydevice.h"

#include "blendef.h"

#define MAXTEXT	32766

/* -- prototypes --------*/
VFont *get_builtin_font(void);

int textediting=0;

extern struct SelBox *selboxes;		/* from blenkernel/font.c */

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

wchar_t *copybuf=NULL;
wchar_t *copybufinfo=NULL;

static wchar_t *textbuf=NULL;
static CharInfo *textbufinfo=NULL;
static wchar_t *oldstr=NULL;
static CharInfo *oldstrinfo=NULL;

void update_string(Curve *cu)
{
	int len;

	// Free the old curve string	
	MEM_freeN(cu->str);

	// Calculate the actual string length in UTF-8 variable characters
	len = wcsleninu8(textbuf);

	// Alloc memory for UTF-8 variable char length string
	cu->str = MEM_callocN(len + sizeof(wchar_t), "str");

	// Copy the wchar to UTF-8
	wcs2utf8s(cu->str, textbuf);
}

static int insert_into_textbuf(Curve *cu, unsigned long c)
{
	if (cu->len<MAXTEXT-1) {
		int x;

		for(x= cu->len; x>cu->pos; x--) textbuf[x]= textbuf[x-1];
		for(x= cu->len; x>cu->pos; x--) textbufinfo[x]= textbufinfo[x-1];		
		textbuf[cu->pos]= c;
		textbufinfo[cu->pos] = cu->curinfo;
		textbufinfo[cu->pos].kern = 0;
		if (G.obedit->actcol>0)
			textbufinfo[cu->pos].mat_nr = G.obedit->actcol;
		else
			textbufinfo[cu->pos].mat_nr = 0;
					
		cu->pos++;
		cu->len++;
		textbuf[cu->len]='\0';

		update_string(cu);

		return 1;
	} else {
		return 0;
	}
}

void add_lorem(void)
{
	char *p, *p2;
	int i;
	Curve *cu=G.obedit->data;
	static char* lastlorem;
	
	if (lastlorem)
		p= lastlorem;
	else
		p= BIF_lorem;
	
	i= rand()/(RAND_MAX/6)+4;	
		
	for (p2=p; *p2 && i; p2++) {
		insert_into_textbuf(cu, *p2);
		if (*p2=='.') i--;
	}
	lastlorem = p2+1;
	if (strlen(lastlorem)<5) lastlorem = BIF_lorem;
	
	insert_into_textbuf(cu, '\n');
	insert_into_textbuf(cu, '\n');	
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);	
}

void load_3dtext_fs(char *file) 
{
	FILE *fp;
	int filelen;
	char *strp;
	Curve *cu=G.obedit->data;

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
		wcscat(textbuf, mem);
		MEM_freeN(mem);
		cu->len += tmplen;
		cu->pos= cu->len;
	}
	MEM_freeN(strp);

	update_string(cu);

	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);	
}

VFont *get_builtin_font(void)
{
	VFont *vf;
	
	for (vf= G.main->vfont.first; vf; vf= vf->id.next)
		if (BLI_streq(vf->name, "<builtin>"))
			return vf;
	
	return load_vfont("<builtin>");
}


void txt_export_to_object(struct Text *text)
{
	ID *id;
	Curve *cu;
	struct TextLine *tmp;
	int nchars = 0;
//	char sdir[FILE_MAXDIR];
//	char sfile[FILE_MAXFILE];

	if(!text || !text->lines.first) return;

	id = (ID *)text;

	if (G.obedit && G.obedit->type==OB_FONT) return;
	check_editmode(OB_FONT);
	
	add_object(OB_FONT);

	base_init_from_view3d(BASACT, G.vd);
	G.obedit= BASACT->object;
	where_is_object(G.obedit);

	cu= G.obedit->data;

/*	
//		renames object, careful with long filenames.

	if (text->name) {
	//ID *find_id(char *type, char *name)	
		BLI_split_dirfile(text->name, sdir, sfile);
//		rename_id((ID *)G.obedit, sfile);
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

	make_editText();
	exit_editmode(EM_FREEDATA|EM_WAITCURSOR);

	allqueue(REDRAWVIEW3D, 0);
}


void txt_export_to_objects(struct Text *text)
{
	ID *id;
	Curve *cu;
	struct TextLine *curline;
	int nchars;
	int linenum = 0;
	float offset[3] = {0.0,0.0,0.0};

	if(!text || !text->lines.first) return;

	id = (ID *)text;

	if (G.obedit && G.obedit->type==OB_FONT) return;
	check_editmode(OB_FONT);

	curline = text->lines.first;
	while(curline){	
		/*skip lines with no text, but still make space for them*/
		if(curline->line[0] == '\0'){
			linenum++;
			curline = curline->next;
			continue;
		}
			
		nchars = 0;	
		add_object(OB_FONT);
	
		base_init_from_view3d(BASACT, G.vd);
		G.obedit= BASACT->object;
		where_is_object(G.obedit);	
		
		/* Do the translation */
		offset[0] = 0;
		offset[1] = -linenum;
		offset[2] = 0;
	
		Mat4Mul3Vecfl(G.vd->viewinv,offset);
		
		G.obedit->loc[0] += offset[0];
		G.obedit->loc[1] += offset[1];
		G.obedit->loc[2] += offset[2];
		/* End Translation */
					
		cu= G.obedit->data;
		
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

		make_editText();
		exit_editmode(EM_FREEDATA|EM_WAITCURSOR);

		linenum++;
		curline = curline->next;
	}
	BIF_undo_push("Add Text as Objects");
	allqueue(REDRAWVIEW3D, 0);
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



static int killselection(int ins)	/* 1 == new character */
{
	int selend, selstart, direction;
	Curve *cu= G.obedit->data;
	int offset = 0;
	int getfrom;

	direction = getselection(&selstart, &selend);
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
		memmove(textbuf+selstart, textbuf+getfrom, size);
		memmove(textbufinfo+selstart, textbufinfo+getfrom, ((cu->len-selstart)+offset)*sizeof(CharInfo));
		cu->len -= (selend-selstart)+offset;
		cu->selstart = cu->selend = 0;
	}
	return(direction);
}

static void copyselection(void)
{
	int selstart, selend;
	
	if (getselection(&selstart, &selend)) {
		memcpy(copybuf, textbuf+selstart, ((selend-selstart)+1)*sizeof(wchar_t));
		copybuf[(selend-selstart)+1]=0;
		memcpy(copybufinfo, textbufinfo+selstart, ((selend-selstart)+1)*sizeof(CharInfo));	
	}
}

static void pasteselection(void)
{
	Curve *cu= G.obedit->data;

	int len= wcslen(copybuf);

	// Verify that the copy buffer => [copy buffer len] + cu->len < MAXTEXT
	if(cu->len + len <= MAXTEXT)
	{
		if (len) {	
			int size = (cu->len * sizeof(wchar_t)) - (cu->pos*sizeof(wchar_t)) + sizeof(wchar_t);
			memmove(textbuf+cu->pos+len, textbuf+cu->pos, size);
			memcpy(textbuf+cu->pos, copybuf, len * sizeof(wchar_t));
		
			memmove(textbufinfo+cu->pos+len, textbufinfo+cu->pos, (cu->len-cu->pos+1)*sizeof(CharInfo));
			memcpy(textbufinfo+cu->pos, copybufinfo, len*sizeof(CharInfo));	
		
			cu->len += len;
			cu->pos += len;
		}
	}
	else
	{
		error("Text too long");
	}
}

int style_to_sel(int style, int toggle) 
{
	int selstart, selend;
	int i;
	Curve *cu;
	
	if (G.obedit && (G.obedit->type == OB_FONT)) {
		cu= G.obedit->data;
		
		if (getselection(&selstart, &selend)) {
			for (i=selstart; i<=selend; i++) {
				if (toggle==0) {
					textbufinfo[i].flag &= ~style;
				} else {
					textbufinfo[i].flag |= style;
				}
			}
			return 1;
		}
	}
	return 0;
}

int mat_to_sel(void) {
	int selstart, selend;
	int i;
	Curve *cu;
	
	if (G.obedit && (G.obedit->type == OB_FONT)) {
		cu= G.obedit->data;
		
		if (getselection(&selstart, &selend)) {
			for (i=selstart; i<=selend; i++) {
				textbufinfo[i].mat_nr = G.obedit->actcol;
			}
			return 1;
		}
	}
	return 0;
}

void do_textedit(unsigned short event, short val, unsigned long _ascii)
{
	Curve *cu;
	static int accentcode= 0;
	int x, doit=0, cursmove=0;
	unsigned long ascii = _ascii;
	short kern;

	cu= G.obedit->data;

	if(ascii) {
	
		/* handle case like TAB (TAB==9) */
		if( (ascii > 31 && ascii < 254 && ascii != 127) || (ascii==13) || (ascii==10) || (ascii==8)) {
	
			if(accentcode) {
				if(cu->pos>0) textbuf[cu->pos-1]= findaccent(textbuf[cu->pos-1], ascii);
				accentcode= 0;
			}
			else if(cu->len<MAXTEXT-1) {
				if(G.qual & LR_ALTKEY ) {
				
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
							wcscat(textbuf, mem);
							MEM_freeN(mem);
							cu->len += tmplen;
							cu->pos= cu->len;
						}
						MEM_freeN(strp);
					}
				}
				else {
					insert_into_textbuf(cu, ascii);
				}
			}
			
			killselection(1);
			
			doit= 1;
		}
		else
		{
			insert_into_textbuf(cu, ascii);
			doit = 1;
		}
	}
	else if(val) {
		cursmove= 0;
		
		switch(event) {
		case ENDKEY:
			if ((G.qual & LR_SHIFTKEY) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;		
			while(cu->pos<cu->len) {
				if( textbuf[cu->pos]==0) break;
				if( textbuf[cu->pos]=='\n') break;
				if( textbufinfo[cu->pos].flag & CU_WRAP ) break;
				cu->pos++;
			}
			cursmove=FO_CURS;
			break;

		case HOMEKEY:
			if ((G.qual & LR_SHIFTKEY) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			while(cu->pos>0) {
				if( textbuf[cu->pos-1]=='\n') break;
				if( textbufinfo[cu->pos-1].flag & CU_WRAP ) break;				
				cu->pos--;
			}		
			cursmove=FO_CURS;
			break;
			
		case RETKEY:
			if(G.qual & LR_CTRLKEY) {
				insert_into_textbuf(cu, 1);
				if (textbuf[cu->pos]!='\n') insert_into_textbuf(cu, '\n');				
			}
			else {
				insert_into_textbuf(cu, '\n');
			}
			cu->selstart = cu->selend = 0;
			doit= 1;
			break;

		case RIGHTARROWKEY:	
			if ((G.qual & LR_SHIFTKEY) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			if (G.qual & LR_CTRLKEY) {
				cu->pos= next_word(cu);
				cursmove= FO_CURS;				
			} 
			else if (G.qual & LR_ALTKEY) {
				kern = textbufinfo[cu->pos-1].kern;
				kern += 1;
				if (kern>20) kern = 20;
				textbufinfo[cu->pos-1].kern = kern;
				doit = 1;
			}
			else {
				cu->pos++;
				cursmove= FO_CURS;				
			}

			break;
			
		case LEFTARROWKEY:
			if ((G.qual & LR_SHIFTKEY) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			if (G.qual & LR_CTRLKEY) {
				cu->pos= prev_word(cu);
				cursmove= FO_CURS;
			} 
			else if (G.qual & LR_ALTKEY) {
				kern = textbufinfo[cu->pos-1].kern;
				kern -= 1;
				if (kern<-20) kern = -20;
				textbufinfo[cu->pos-1].kern = kern;
				doit = 1;
			}
			else {
				cu->pos--;
				cursmove=FO_CURS;
			}
			break;

		case UPARROWKEY:
			if ((G.qual & LR_SHIFTKEY) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			if(G.qual & LR_ALTKEY) {
				if (cu->pos && textbuf[cu->pos - 1] < 255) {
					textbuf[cu->pos - 1]++;
					doit= 1;
				}
			}
			else cursmove=FO_CURSUP;
			break;
			
		case PAGEUPKEY:
			if ((G.qual & LR_SHIFTKEY) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cursmove=FO_PAGEUP;
			break;
			
		case DOWNARROWKEY:
			if ((G.qual & LR_SHIFTKEY) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			if(G.qual & LR_ALTKEY) {
				if (cu->pos && textbuf[cu->pos - 1] > 1) {
					textbuf[cu->pos - 1]--;
					doit= 1;
				}
			}
			else cursmove= FO_CURSDOWN;
			break;

		case PAGEDOWNKEY:
			if ((G.qual & LR_SHIFTKEY) && (cu->selstart==0)) cu->selstart = cu->selend = cu->pos+1;
			cursmove=FO_PAGEDOWN;
			break;
			
		case BACKSPACEKEY:
			if(cu->len!=0) {
				if(G.qual & LR_ALTKEY) {
					if(cu->pos>0) accentcode= 1;
				}
				else if (G.qual & LR_CTRLKEY) {
					cu->len = cu->pos = 0;
					textbuf[0]= 0;
					doit= 1;
				}
				else {
					if (killselection(0)==0) {
						if (cu->pos>0) {
							for(x=cu->pos;x<=cu->len;x++) textbuf[x-1]= textbuf[x];
							for(x=cu->pos;x<=cu->len;x++) textbufinfo[x-1]= textbufinfo[x];					
							cu->pos--;
							textbuf[--cu->len]='\0';
							doit=1;
						}
					} else doit=1;
				}
			}
			break;

		case DELKEY:
			if(cu->len!=0) {
				if (killselection(0)==0) {
					if(cu->pos<cu->len) {					
						for(x=cu->pos;x<cu->len;x++) textbuf[x]= textbuf[x+1];
						for(x=cu->pos;x<cu->len;x++) textbufinfo[x]= textbufinfo[x+1];					
						textbuf[--cu->len]='\0';
						doit=1;
					}
				} else doit=1;
			}
			break;
		
		case IKEY:
			if (G.qual & LR_CTRLKEY) {
				cu->curinfo.flag ^= CU_ITALIC;
				if (style_to_sel(CU_ITALIC, cu->curinfo.flag & CU_ITALIC)) doit= 1;   				
				allqueue(REDRAWBUTSEDIT, 0);
			}
			break;

		case BKEY:
			if (G.qual & LR_CTRLKEY) {
				cu->curinfo.flag ^= CU_BOLD;
				if (style_to_sel(CU_BOLD, cu->curinfo.flag & CU_BOLD)) doit= 1;
				allqueue(REDRAWBUTSEDIT, 0);
			}
			break;			
			
		case UKEY:
			if (G.qual & LR_CTRLKEY) {
				cu->curinfo.flag ^= CU_UNDERLINE;
				if (style_to_sel(CU_UNDERLINE, cu->curinfo.flag & CU_UNDERLINE)) doit= 1;
				allqueue(REDRAWBUTSEDIT, 0);
			}
			break;
			
		case XKEY:
			if (G.qual & LR_CTRLKEY) {
				copyselection();
				killselection(0);
				doit= 1;
			}
			break;
			
		case CKEY:
			if (G.qual & LR_CTRLKEY) {
				copyselection();
			}
			break;   			
			
		case VKEY:
			if (G.qual & LR_CTRLKEY) {
				pasteselection();
				doit= 1;
			}
			break;
		
		}
			
		if(cursmove) {
			if ((G.qual & LR_SHIFTKEY)==0) {
				if (cu->selstart) {
					cu->selstart = cu->selend = 0;
					update_string(cu);
					text_to_curve(G.obedit, FO_SELCHANGE);
					allqueue(REDRAWVIEW3D, 0);
				}
			}
			if(cu->pos>cu->len) cu->pos= cu->len;
			else if(cu->pos>=MAXTEXT) cu->pos= MAXTEXT;
			else if(cu->pos<0) cu->pos= 0;
		}
	}
	if(doit || cursmove) {
	
		if (cu->pos) {
			cu->curinfo = textbufinfo[cu->pos-1];
		} else cu->curinfo = textbufinfo[0];
		
		if (G.obedit->totcol>0) {
			G.obedit->actcol = textbufinfo[cu->pos-1].mat_nr;
		}
		allqueue(REDRAWBUTSEDIT, 0);
		update_string(cu);
		text_to_curve(G.obedit, cursmove);
		if (cursmove && (G.qual & LR_SHIFTKEY)) {
			cu->selend = cu->pos;
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		}
		if(cursmove==0) {
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		}			

		BIF_undo_push("Textedit");
		allqueue(REDRAWVIEW3D, 0);
	}
}

void paste_unicodeText(char *filename)
{
	Curve *cu= G.obedit->data;
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
			wcscat(textbuf, mem);
			MEM_freeN(mem);
			cu->len += tmplen;
			cu->pos= cu->len;
		}
		MEM_freeN(strp);
		doit = 1;
	}
	if(doit) {
		update_string(cu);
		text_to_curve(G.obedit, 0);
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Paste text");
	}
}

void paste_editText(void)
{
	Curve *cu= G.obedit->data;
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
			wcscat(textbuf, mem);
			MEM_freeN(mem);
			cu->len += tmplen;
			cu->pos= cu->len;
		}
		MEM_freeN(strp);
		doit = 1;
	}
	if(doit) {
		update_string(cu);
		text_to_curve(G.obedit, 0);
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Paste text");
	}
}


void make_editText(void)
{
	Curve *cu;
	cu= G.obedit->data;
	
	if(textbuf==NULL) textbuf= MEM_callocN((MAXTEXT+4)*sizeof(wchar_t), "texteditbuf");
	if(textbufinfo==NULL) textbufinfo= MEM_callocN((MAXTEXT+4)*sizeof(CharInfo), "texteditbufinfo");
	if(copybuf==NULL) copybuf= MEM_callocN((MAXTEXT+4)*sizeof(wchar_t), "texteditcopybuf");
	if(copybufinfo==NULL) copybufinfo= MEM_callocN((MAXTEXT+4)*sizeof(CharInfo), "texteditcopybufinfo");	
	if(oldstr==NULL) oldstr= MEM_callocN((MAXTEXT+4)*sizeof(wchar_t), "oldstrbuf");
	
	// Convert the original text to wchar_t
	utf8towchar(textbuf, cu->str);
	wcscpy(oldstr, textbuf);
		
	cu->len= wcslen(textbuf);
	
	memcpy(textbufinfo, cu->strinfo, (cu->len)*sizeof(CharInfo));
	
	oldstrinfo= cu->strinfo;
	cu->strinfo= textbufinfo;

	if(cu->pos>cu->len) cu->pos= cu->len;

	if (cu->pos) {
		cu->curinfo = textbufinfo[cu->pos-1];
	} else cu->curinfo = textbufinfo[0];
	
	// Convert to UTF-8
	update_string(cu);
	
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	
	textediting= 1;
	BIF_undo_push("Original");
}


void load_editText(void)
{
	Curve *cu;
	
	cu= G.obedit->data;

	MEM_freeN(oldstr);
	oldstr= NULL;
	MEM_freeN(oldstrinfo);
	oldstrinfo= NULL;
	
	update_string(cu);
	
	cu->strinfo= MEM_callocN((cu->len+4)*sizeof(CharInfo), "texteditinfo");
	memcpy(cu->strinfo, textbufinfo, (cu->len)*sizeof(CharInfo));

	cu->len= strlen(cu->str);
	
	/* this memory system is weak... */
	MEM_freeN(textbuf);
	MEM_freeN(textbufinfo);
	textbuf= NULL;
	textbufinfo= NULL;
	
	if (selboxes) {
		MEM_freeN(selboxes);
		selboxes= NULL;
	}
	
	textediting= 0;
	
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

}


void remake_editText(void)
{
	Curve *cu;
		
	if(okee("Reload original text")==0) return;
	
	// Copy the oldstr to textbuf temporary global variable
	wcscpy(textbuf, oldstr);

	// Set the object length and position	
	cu= G.obedit->data;
	cu->len= wcslen(textbuf);
	if(cu->pos>cu->len) cu->pos= cu->len;

	update_string(cu);
	
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
	allqueue(REDRAWVIEW3D, 0);
	
	BIF_undo_push("Reload");
}


void free_editText(void)
{
	if(oldstr) MEM_freeN(oldstr);
	if(oldstrinfo) MEM_freeN(oldstrinfo);
	if(textbuf) MEM_freeN(textbuf);
	textbuf = oldstr = NULL;
	textbufinfo = oldstrinfo = NULL;
	textediting= 0;
}


void add_primitiveFont(int dummy_argument)
{
	Curve *cu;

	if (G.obedit && G.obedit->type==OB_FONT) return;
	check_editmode(OB_FONT);
	
	add_object_draw(OB_FONT);
	base_init_from_view3d(BASACT, G.vd);
	
	where_is_object(BASACT->object);
	
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
	
	if (U.flag & USER_ADD_EDITMODE) 
		enter_editmode(EM_WAITCURSOR);

	allqueue(REDRAWALL, 0);
}

void to_upper(void)
{
	Curve *cu;
	int len, ok;
	wchar_t *str;
	
	if(G.obedit==0) {
		return;
	}
	
	ok= 0;
	cu= G.obedit->data;
	
	len= wcslen(textbuf);
	str= textbuf;
	while(len) {
		if( *str>=97 && *str<=122) {
			ok= 1;
			*str-= 32;
		}
		len--;
		str++;
	}
	
	if(ok==0) {
		len= wcslen(textbuf);
		str= textbuf;
		while(len) {
			if( *str>=65 && *str<=90) {
				*str+= 32;
			}
			len--;
			str++;
		}
	}
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("To upper");

	update_string(cu);
}


/* **************** undo for font object ************** */

static void undoFont_to_editFont(void *strv)
{
	Curve *cu= G.obedit->data;
	char *str= strv;

	cu->pos= *((short *)str);
	cu->len= *((short *)(str+2));

	memcpy(textbuf, str+4, (cu->len+1)*sizeof(wchar_t));
	memcpy(textbufinfo, str+4 + (cu->len+1)*sizeof(wchar_t), cu->len*sizeof(CharInfo));
	
	cu->selstart = cu->selend = 0;
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	
	update_string(cu);
	
	allqueue(REDRAWVIEW3D, 0);
}

static void *editFont_to_undoFont(void)
{
	Curve *cu= G.obedit->data;
	char *str;
	
	// The undo buffer includes [MAXTEXT+6]=actual string and [MAXTEXT+4]*sizeof(CharInfo)=charinfo
	str= MEM_callocN((MAXTEXT+6)*sizeof(wchar_t) + (MAXTEXT+4)*sizeof(CharInfo), "string undo");

	// Copy the string and string information
	memcpy(str+4, textbuf, (cu->len+1)*sizeof(wchar_t));
	memcpy(str+4 + (cu->len+1)*sizeof(wchar_t), textbufinfo, cu->len*sizeof(CharInfo));

	*((short *)str)= cu->pos;
	*((short *)(str+2))= cu->len;	
	
	return str;
}

static void free_undoFont(void *strv)
{
	MEM_freeN(strv);
}

/* and this is all the undo system needs to know */
void undo_push_font(char *name)
{
	undo_editmode_push(name, free_undoFont, undoFont_to_editFont, editFont_to_undoFont, NULL);
}



/***/
