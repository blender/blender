/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <string.h>

#include <fcntl.h>
#ifndef WIN32 
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_vfont_types.h"
#include "DNA_scene_types.h"

#include "BKE_displist.h"
#include "BKE_font.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "BIF_editfont.h"
#include "BIF_toolbox.h"
#include "BIF_space.h"
#include "BIF_mywindow.h"

#include "BDR_editobject.h"

#include "mydevice.h"

#include "blendef.h"

#define MAXTEXT	1000

int textediting=0;

static char findaccent(char char1, char code)
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

static char *textbuf=0;
static char *oldstr;

static int insert_into_textbuf(Curve *cu, char c)
{
	if (cu->len<MAXTEXT-1) {
		int x;

		for(x= cu->len; x>cu->pos; x--) textbuf[x]= textbuf[x-1];
		textbuf[cu->pos]= c;
					
		cu->pos++;
		cu->len++;
		textbuf[cu->len]='\0';

		return 1;
	} else {
		return 0;
	}
}

void do_textedit(unsigned short event, short val, char ascii)
{
	Curve *cu;
	static int accentcode= 0;
	int x, doit=0, cursmove=0;

	cu= G.obedit->data;

	if(ascii) {
	
		/* o.a. afvangen van TAB (TAB==9) */
		if( (ascii > 31 && ascii < 200 && ascii != 127) || (ascii==13) || (ascii==10) || (ascii==8)) {
	
			if(accentcode) {
				if(cu->pos>0) textbuf[cu->pos-1]= findaccent(textbuf[cu->pos-1], ascii);
				accentcode= 0;
			}
			else if(cu->len<MAXTEXT-1) {
				if(G.qual & LR_ALTKEY ) {
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
					
						strp= MEM_mallocN(filelen+1, "tempstr");
						read(file, strp, filelen);
						close(file);
						strp[filelen]= 0;
						if(cu->len+filelen<MAXTEXT) {
							strcat( textbuf, strp);
							cu->len= strlen(textbuf);
							cu->pos= cu->len;
						}
						MEM_freeN(strp);
					}
				}
				else {
					insert_into_textbuf(cu, ascii);
				}
			}
			
			doit= 1;
		}
	}
	else if(val) {
		cursmove= 0;
		
		switch(event) {
		case RETKEY:
			insert_into_textbuf(cu, '\n');
			doit= 1;
			break;

		case RIGHTARROWKEY:	
			if(G.qual & LR_SHIFTKEY) {
				while(cu->pos<cu->len) {
					if( textbuf[cu->pos]==0) break;
					if( textbuf[cu->pos]=='\n') break;
					cu->pos++;
				}
			}
			else {
				cu->pos++;
			}
			cursmove= FO_CURS;
			break;
			
		case LEFTARROWKEY:
			
			if(G.qual & LR_SHIFTKEY) {
				while(cu->pos>0) {
					if( textbuf[cu->pos-1]=='\n') break;
					cu->pos--;
				}
			}
			else {
				cu->pos--;
			}
			cursmove=FO_CURS;
			break;

		case UPARROWKEY:
			if(G.qual & LR_SHIFTKEY) {
				cu->pos= 0;
				cursmove= FO_CURS;
			}
			else if(G.qual & LR_ALTKEY) {
				if (cu->pos && textbuf[cu->pos - 1] < 255) {
					textbuf[cu->pos - 1]++;
					doit= 1;
				}
			}
			else cursmove=FO_CURSUP;
			break;
			
		case DOWNARROWKEY:
			if(G.qual & LR_SHIFTKEY) {
				cu->pos= cu->len;
				cursmove= FO_CURS;
			}
			else if(G.qual & LR_ALTKEY) {
				if (cu->pos && textbuf[cu->pos - 1] > 1) {
					textbuf[cu->pos - 1]--;
					doit= 1;
				}
			}
			else cursmove= FO_CURSDOWN;
			break;
			
		case BACKSPACEKEY:
			if(cu->len!=0) {
				if(G.qual & LR_ALTKEY) {
					if(cu->pos>0) accentcode= 1;
				}
				else if(G.qual & LR_SHIFTKEY) {
					cu->pos= 0;
					textbuf[0]= 0;
					cu->len= 0;
				}
				else if(cu->pos>0) {
					for(x=cu->pos;x<=cu->len;x++) textbuf[x-1]= textbuf[x];
					cu->pos--;
					textbuf[--cu->len]='\0';
				}
			}
			doit= 1;
			break;
		}
			
		if(cursmove) {
			if(cu->pos>cu->len) cu->pos= cu->len;
			else if(cu->pos>=MAXTEXT) cu->pos= MAXTEXT;
			else if(cu->pos<0) cu->pos= 0;
		}
	}
	if(doit || cursmove) {
		text_to_curve(G.obedit, cursmove);
		if(cursmove==0) makeDispList(G.obedit);
		allqueue(REDRAWVIEW3D, 0);
	}
}

void make_editText(void)
{
	Curve *cu;

	cu= G.obedit->data;
	if(textbuf==0) textbuf= MEM_mallocN(MAXTEXT, "texteditbuf");
	BLI_strncpy(textbuf, cu->str, MAXTEXT);
	oldstr= cu->str;
	cu->str= textbuf;

	cu->len= strlen(textbuf);
	if(cu->pos>cu->len) cu->pos= cu->len;
	
	text_to_curve(G.obedit, 0);
	makeDispList(G.obedit);
	
	textediting= 1;
}

void load_editText(void)
{
	Curve *cu;
	
	cu= G.obedit->data;

	MEM_freeN(oldstr);
	oldstr= 0;
	
	cu->str= MEM_mallocN(cu->len+1, "tekstedit");
	strcpy(cu->str, textbuf);
	
	/* this memory system is weak... */
	MEM_freeN(textbuf);
	textbuf= 0;
	
	cu->len= strlen(cu->str);
	textediting= 0;
}

void remake_editText(void)
{
	Curve *cu;
		
	if(okee("Reload Original text")==0) return;
	
	BLI_strncpy(textbuf, oldstr, MAXTEXT);
	cu= G.obedit->data;
	cu->len= strlen(textbuf);
	if(cu->pos>cu->len) cu->pos= cu->len;
	
	text_to_curve(G.obedit, 0);
	makeDispList(G.obedit);
	
	allqueue(REDRAWVIEW3D, 0);
}

void free_editText(void)
{
	if(oldstr) MEM_freeN(oldstr);
	textbuf= oldstr= 0;
	textediting= 0;
}

static VFont *get_builtin_font(void)
{
	VFont *vf;
	
	for (vf= G.main->vfont.first; vf; vf= vf->id.next)
		if (BLI_streq(vf->name, "<builtin>"))
			return vf;
	
	return load_vfont("<builtin>");
}

void add_primitiveFont(int dummy_argument)
{
	Curve *cu;

	if (G.obedit && G.obedit->type==OB_FONT) return;
	check_editmode(OB_FONT);
	
	add_object(OB_FONT);
	base_init_from_view3d(BASACT, G.vd);
	G.obedit= BASACT->object;
	where_is_object(G.obedit);
	
	cu= G.obedit->data;
	
	cu->vfont= get_builtin_font();
	cu->vfont->id.us++;
	cu->str= MEM_mallocN(12, "str");
	strcpy(cu->str, "Text");
	cu->pos= 4;
	
	make_editText();
	allqueue(REDRAWVIEW3D, 0);
}

void to_upper(void)
{
	Curve *cu;
	int len, ok;
	char *str;
	
	if(G.obedit==0) {
		return;
	}
	
	ok= 0;
	cu= G.obedit->data;
	
	len= strlen(cu->str);
	str= cu->str;
	while(len) {
		if( *str>=97 && *str<=122) {
			ok= 1;
			*str-= 32;
		}
		len--;
		str++;
	}
	
	if(ok==0) {
		len= strlen(cu->str);
		str= cu->str;
		while(len) {
			if( *str>=65 && *str<=90) {
				*str+= 32;
			}
			len--;
			str++;
		}
	}
	text_to_curve(G.obedit, 0);
	makeDispList(G.obedit);

	allqueue(REDRAWVIEW3D, 0);

}


