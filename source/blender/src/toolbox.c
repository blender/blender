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

#define PY_TOOLBOX 1

#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   

#include <fcntl.h>
#include "MEM_guardedalloc.h"

#include "BMF_Api.h"
#include "BIF_language.h"
#include "BIF_resources.h"

#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_camera_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_main.h"
#include "BKE_plugin_types.h"
#include "BKE_utildefines.h"

#include "BIF_editnla.h"
#include "BIF_editarmature.h"
#include "BIF_editdeform.h"
#include "BIF_editfont.h"
#include "BIF_editmesh.h"
#include "BIF_editseq.h"
#include "BIF_editlattice.h"
#include "BIF_editsima.h"
#include "BIF_editoops.h"
#include "BIF_editview.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_imasel.h"
#include "BIF_interface.h"
#include "BIF_mainqueue.h"
#include "BIF_mywindow.h"
#include "BIF_renderwin.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_tbcallback.h"
#include "BIF_transform.h"

#include "BDR_editobject.h"
#include "BDR_editcurve.h"
#include "BDR_editmball.h"

#include "BSE_drawipo.h"
#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_node.h"

#include "IMB_imbuf.h"

#include "blendef.h"
#include "butspace.h"
#include "mydevice.h"

/* bpymenu */
#include "BPY_extern.h"
#include "BPY_menus.h"

void asciitoraw(int ch, unsigned short *event, unsigned short *qual)
{
	if( isalpha(ch)==0 ) return;
	
	if( isupper(ch) ) {
		*qual= LEFTSHIFTKEY;
		ch= tolower(ch);
	}
	
	switch(ch) {
	case 'a': *event= AKEY; break;
	case 'b': *event= BKEY; break;
	case 'c': *event= CKEY; break;
	case 'd': *event= DKEY; break;
	case 'e': *event= EKEY; break;
	case 'f': *event= FKEY; break;
	case 'g': *event= GKEY; break;
	case 'h': *event= HKEY; break;
	case 'i': *event= IKEY; break;
	case 'j': *event= JKEY; break;
	case 'k': *event= KKEY; break;
	case 'l': *event= LKEY; break;
	case 'm': *event= MKEY; break;
	case 'n': *event= NKEY; break;
	case 'o': *event= OKEY; break;
	case 'p': *event= PKEY; break;
	case 'q': *event= QKEY; break;
	case 'r': *event= RKEY; break;
	case 's': *event= SKEY; break;
	case 't': *event= TKEY; break;
	case 'u': *event= UKEY; break;
	case 'v': *event= VKEY; break;
	case 'w': *event= WKEY; break;
	case 'x': *event= XKEY; break;
	case 'y': *event= YKEY; break;
	case 'z': *event= ZKEY; break;
	}
}

/* ************************************  */

/* this va_ stuff allows printf() style codes in these menus */

static int vconfirm(char *title, char *itemfmt, va_list ap)
{
	char *s, buf[512];

	s= buf;
	if (title) s+= sprintf(s, "%s%%t|", title);
	vsprintf(s, itemfmt, ap);
	
	return (pupmenu(buf)>=0);
}

static int confirm(char *title, char *itemfmt, ...)
{
	va_list ap;
	int ret;
	
	va_start(ap, itemfmt);
	ret= vconfirm(title, itemfmt, ap);
	va_end(ap);
	
	return ret;
}

int okee(char *str, ...)
{
	va_list ap;
	int ret;
	char titlestr[256];
	
	sprintf(titlestr, "OK? %%i%d", ICON_HELP);
	
	va_start(ap, str);
	ret= vconfirm(titlestr, str, ap);
	va_end(ap);
	
	return ret;
}

void notice(char *str, ...)
{
	va_list ap;
	
	va_start(ap, str);
	vconfirm(NULL, str, ap);
	va_end(ap);
}

void error(char *fmt, ...)
{
	va_list ap;
	char nfmt[256];
	char titlestr[256];
	
	sprintf(titlestr, "Error %%i%d", ICON_ERROR);
	
	sprintf(nfmt, "%s", fmt);
	
	va_start(ap, fmt);
	if (G.background || !G.curscreen) {
		vprintf(nfmt, ap);
		printf("\n");
	} else {
		vconfirm(titlestr, nfmt, ap);
	}
	va_end(ap);
}

void error_libdata(void)
{
	error(ERROR_LIBDATA_MESSAGE);
}

int saveover(char *file)
{
	int len= strlen(file);
	
	if(len==0) 
		return 0;
	
	if(BLI_exists(file)==0)
		return 1;
	
	if( file[len-1]=='/' || file[len-1]=='\\' ) {
		error("Cannot overwrite a directory");
		return 0;
	}
		
	return confirm("Save over", file);
}

/* ****************** EXTRA STUFF **************** */

short button(short *var, short min, short max, char *str)
{
	uiBlock *block;
	ListBase listb={0, 0};
	short x1,y1;
	short mval[2], ret=0;

	if(min>max) min= max;

	getmouseco_sc(mval);
	
	if(mval[0]<150) mval[0]=150;
	if(mval[1]<30) mval[1]=30;
	if(mval[0]>G.curscreen->sizex) mval[0]= G.curscreen->sizex-10;
	if(mval[1]>G.curscreen->sizey) mval[1]= G.curscreen->sizey-10;

	block= uiNewBlock(&listb, "button", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_ENTER_OK);

	x1=mval[0]-150; 
	y1=mval[1]-20; 
	
	uiDefButS(block, NUM, 0, str,	(short)(x1+5),(short)(y1+10),125,20, var,(float)min,(float)max, 0, 0, "");
	uiDefBut(block, BUT, 32767, "OK",	(short)(x1+136),(short)(y1+10),25,20, NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 5);

	ret= uiDoBlocks(&listb, 0, 0);

	if(ret==UI_RETURN_OK) return 1;
	return 0;
}

short sbutton(char *var, short min, short max, char *str)
{
	uiBlock *block;
	ListBase listb={0, 0};
	short x1,y1;
	short mval[2], ret=0;
	char *editvar = NULL; /* dont edit the original text, incase we cancel the popup */
	
	if(min>max) min= max;

	getmouseco_sc(mval);
	
	if(mval[0]<250) mval[0]=250;
	if(mval[1]<30) mval[1]=30;
	if(mval[0]>G.curscreen->sizex) mval[0]= G.curscreen->sizex-10;
	if(mval[1]>G.curscreen->sizey) mval[1]= G.curscreen->sizey-10;

	block= uiNewBlock(&listb, "button", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_ENTER_OK);

	x1=mval[0]-250; 
	y1=mval[1]-20; 
	
	editvar = MEM_callocN(max, "sbutton");
	BLI_strncpy(editvar, var, max);
	
	uiDefButC(block, TEX, 32766, str,	x1+5,y1+10,225,20, editvar,(float)min,(float)max, 0, 0, "");
	uiDefBut(block, BUT, 32767, "OK",	x1+236,y1+10,25,20, NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 5);
	
	mainqenter_ext(BUT_ACTIVATE, 32766, 0);	/* note, button id '32766' is asking for errors some day! */
	ret= uiDoBlocks(&listb, 0, 0);

	if(ret==UI_RETURN_OK) {
		BLI_strncpy(var, editvar, max);
		MEM_freeN(editvar);
		return 1;
	}
	MEM_freeN(editvar);
	return 0;
	
}

short fbutton(float *var, float min, float max, float a1, float a2, char *str)
{
	uiBlock *block;
	ListBase listb={0, 0};
	short x1,y1;
	short mval[2], ret=0;

	if(min>max) min= max;

	getmouseco_sc(mval);
	
	if(mval[0]<150) mval[0]=150;
	if(mval[1]<30) mval[1]=30;
	if(mval[0]>G.curscreen->sizex) mval[0]= G.curscreen->sizex-10;
	if(mval[1]>G.curscreen->sizey) mval[1]= G.curscreen->sizey-10;

	block= uiNewBlock(&listb, "button", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1);

	x1=mval[0]-150; 
	y1=mval[1]-20; 
	
	uiDefButF(block, NUM, 0, str,(short)(x1+5),(short)(y1+10),125,20, var, min, max, a1, a2, "");
	uiDefBut(block, BUT, 32767, "OK",(short)(x1+136),(short)(y1+10), 35, 20, NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 2);

	ret= uiDoBlocks(&listb, 0, 0);

	if(ret==UI_RETURN_OK) return 1;
	return 0;
}

int movetolayer_buts(unsigned int *lay, char *title)
{
	uiBlock *block;
	ListBase listb={0, 0};
	int dx, dy, a, x1, y1, sizex=160, sizey=30;
	short pivot[2], mval[2], ret=0;
	
	if(G.vd->localview) {
		error("Not in localview ");
		return ret;
	}

	getmouseco_sc(mval);

	pivot[0]= CLAMPIS(mval[0], (sizex+10), G.curscreen->sizex-30);
	pivot[1]= CLAMPIS(mval[1], (sizey/2)+10, G.curscreen->sizey-(sizey/2)-10);
	
	if (pivot[0]!=mval[0] || pivot[1]!=mval[1])
		warp_pointer(pivot[0], pivot[1]);

	mywinset(G.curscreen->mainwin);
	
	x1= pivot[0]-sizex+10; 
	y1= pivot[1]-sizey/2; 

	block= uiNewBlock(&listb, "button", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT|UI_BLOCK_ENTER_OK);
	
	dx= (sizex-5)/12;
	dy= sizey/2;
	
	if(title)
		uiDefBut(block, LABEL, 0, title, (short)(x1), (short)y1+30, sizex, 20, NULL, 1, 0, 0, 0, "");
	
	/* buttons have 0 as return event, to prevent menu to close on hotkeys */
	uiBlockBeginAlign(block);
	for(a=0; a<5; a++) 
		uiDefButBitI(block, TOGR, 1<<a, 0, "",(short)(x1+a*dx),(short)(y1+dy),(short)dx,(short)dy, (int *)lay, 0, 0, 0, 0, "");
	for(a=0; a<5; a++) 
		uiDefButBitI(block, TOGR, 1<<(a+10), 0, "",(short)(x1+a*dx),(short)y1,(short)dx,(short)dy, (int *)lay, 0, 0, 0, 0, "");
	x1+= 5;
	
	uiBlockBeginAlign(block);
	for(a=5; a<10; a++) 
		uiDefButBitI(block, TOGR, 1<<a, 0, "",(short)(x1+a*dx),(short)(y1+dy),(short)dx,(short)dy, (int *)lay, 0, 0, 0, 0, "");
	for(a=5; a<10; a++) 
		uiDefButBitI(block, TOGR, 1<<(a+10), 0, "",(short)(x1+a*dx),(short)y1,(short)dx,(short)dy, (int *)lay, 0, 0, 0, 0, "");
	uiBlockEndAlign(block);

	x1-= 5;
	uiDefBut(block, BUT, 32767, "OK", (short)(x1+10*dx+10), (short)y1, (short)(3*dx), (short)(2*dy), NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 2);

	ret= uiDoBlocks(&listb, 0, 0);

	if(ret==UI_RETURN_OK) return 1;
	return 0;
}

/* armature or bone */
int movetolayer_short_buts(short *lay, char *title)
{
	uiBlock *block;
	ListBase listb={0, 0};
	int dx, dy, a, x1, y1, sizex=120, sizey=30;
	short pivot[2], mval[2], ret=0;
	
	getmouseco_sc(mval);
	
	pivot[0]= CLAMPIS(mval[0], (sizex+10), G.curscreen->sizex-30);
	pivot[1]= CLAMPIS(mval[1], (sizey/2)+10, G.curscreen->sizey-(sizey/2)-10);
	
	if (pivot[0]!=mval[0] || pivot[1]!=mval[1])
		warp_pointer(pivot[0], pivot[1]);
	
	mywinset(G.curscreen->mainwin);
	
	x1= pivot[0]-sizex+10; 
	y1= pivot[1]-sizey/2; 
	
	block= uiNewBlock(&listb, "button", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT|UI_BLOCK_ENTER_OK);
	
	dx= (sizex-5)/10;
	dy= sizey/2;
	
	if(title)
		uiDefBut(block, LABEL, 0, title, (short)(x1), (short)y1+30, sizex, 20, NULL, 1, 0, 0, 0, "");

	/* buttons have 0 as return event, to prevent menu to close on hotkeys */
	uiBlockBeginAlign(block);
	for(a=0; a<8; a++) 
		uiDefButBitS(block, TOGR, 1<<a, 0, "",(short)(x1+a*dx),(short)(y1+dy),(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	for(a=0; a<8; a++) 
		uiDefButBitS(block, TOGR, 1<<(a+8), 0, "",(short)(x1+a*dx),(short)y1,(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	
	uiBlockEndAlign(block);
	
	x1-= 5;
	uiDefBut(block, BUT, 32767, "OK", (short)(x1+8*dx+10), (short)y1, (short)(3*dx), (short)(2*dy), NULL, 0, 0, 0, 0, "");
	
	uiBoundsBlock(block, 2);
	
	ret= uiDoBlocks(&listb, 0, 0);
	
	if(ret==UI_RETURN_OK) return 1;
	return 0;
}


/* ********************** CLEVER_NUMBUTS ******************** */

#define MAXNUMBUTS	120
#define MAXNUMBUTROWS	8

VarStruct numbuts[MAXNUMBUTS];
void *numbpoin[MAXNUMBUTS];
int numbdata[MAXNUMBUTS];

void draw_numbuts_tip(char *str, int x1, int y1, int x2, int y2)
{
	static char *last=0;	/* avoid ugly updates! */
	int temp;
	
	if(str==last) return;
	last= str;
	if(str==0) return;

	glColor3ub(160, 160, 160); /* MGREY */
	glRecti(x1+4,  y2-36,  x2-4,  y2-16);

	cpack(0x0);

	temp= 0;
	while( BIF_GetStringWidth(G.fonts, str+temp, (U.transopts & USER_TR_BUTTONS))>(x2 - x1-24)) temp++;
	glRasterPos2i(x1+16, y2-30);
	BIF_DrawString(G.fonts, str+temp, (U.transopts & USER_TR_BUTTONS));
}

int do_clever_numbuts(char *name, int tot, int winevent)
{
	ListBase listb= {NULL, NULL};
	uiBlock *block;
	VarStruct *varstr;
	int a, sizex, sizey, x1, y2, width, colunms=1, xi=0, yi=0;
	short mval[2], event;
	
	/* Clear all events so tooltips work, this is not ideal and
	only needed because calls from the menu still have some events
	left over when do_clever_numbuts is called.
	Calls from keyshortcuts do not have this problem.*/
	ScrArea *sa;
	BWinEvent temp_bevt;
	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->win) {
			while( bwin_qread( sa->win, &temp_bevt ) ) {}
		}
		if(sa->headwin) {
			while( bwin_qread( sa->headwin, &temp_bevt ) ) {}
		}
	}
	/* Done clearing events */
	
	if(tot<=0 || tot>MAXNUMBUTS) return 0;
	
	/* if we have too many buttons then have more then 1 column */
	colunms= (int)ceil((double)tot / (double)MAXNUMBUTROWS);
	
	getmouseco_sc(mval);

	/* size */
	sizex= 175;
	sizey= 30+20*(MIN2(MAXNUMBUTROWS, tot)+1);
	width= (sizex*colunms)+60;
	
	/* center */
	if(mval[0]<width/2) mval[0]=width/2;
	if(mval[1]<sizey/2) mval[1]=sizey/2;
	if(mval[0]>G.curscreen->sizex -width/2) mval[0]= G.curscreen->sizex -width/2;
	if(mval[1]>G.curscreen->sizey -sizey/2) mval[1]= G.curscreen->sizey -sizey/2;

	mywinset(G.curscreen->mainwin);
	
	x1= mval[0]-width/2; 
	y2= mval[1]+sizey/2;
	
	block= uiNewBlock(&listb, "numbuts", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_ENTER_OK);
	
	/* WATCH IT: TEX BUTTON EXCEPTION */
	/* WARNING: ONLY A SINGLE BIT-BUTTON POSSIBLE: WE WORK AT COPIED DATA! */
	
	BIF_ThemeColor(TH_MENU_TEXT); /* makes text readable on dark theme */
	
	uiDefBut(block, LABEL, 0, name,	(short)(x1+15), (short)(y2-35), (short)(width-60), 19, 0, 1.0, 0.0, 0, 0, ""); 
	
	/*
	if(name[0]=='A' && name[7]=='O') {
		y2 -= 20;
		uiDefBut(block, LABEL, 0, "Rotations in degrees!",	(short)(x1+15), (short)(y2-35), (short)(sizex-60), 19, 0, 0.0, 0.0, 0, 0, "");
	}*/
	
	uiBlockBeginAlign(block);
	varstr= &numbuts[0];
	for(a=0; a<tot; a++, varstr++) {
		
		if(varstr->type==TEX) {
			uiDefBut(block, TEX, 0,	varstr->name,(short)((x1+15) + (sizex*xi)),(short)(y2-55- 20*yi),(short)(sizex), 19, numbpoin[a], varstr->min, varstr->max, 0, 0, varstr->tip);
		} else if(varstr->type==COL) {
			uiDefButF(block, COL, 0, "",(short)((x1+15) + (sizex*xi)),(short)(y2-55- 20*yi),(short)(sizex), 19, numbpoin[a], varstr->min, varstr->max, 0, 0, "");
		} else  {
			if(varstr->type==LABEL) {/* dont include the label when rounding the buttons */
				uiBlockEndAlign(block);
			
				/* using the tip for the name, this is incorrect lets us get around the 16 char limit of name */
				/* Changed from the line below to use the tip since the tip isnt used for a label */
				uiDefBut(block, varstr->type, 0, varstr->tip,(short)((x1+15) + (sizex*xi)),(short)(y2-55-20*yi), (short)(sizex), 19, &(numbdata[a]), varstr->min, varstr->max, 100, 0, "");
			} else {
				uiDefBut(block, varstr->type, 0, varstr->name,(short)((x1+15) + (sizex*xi)),(short)(y2-55-20*yi), (short)(sizex), 19, &(numbdata[a]), varstr->min, varstr->max, 100, 0, varstr->tip);
			}
			
			if(varstr->type==LABEL)
				uiBlockBeginAlign(block);
		}
		
		/* move to the next column */
		yi++;
		if (yi>=MAXNUMBUTROWS) {
			yi=0;
			xi++;
			uiBlockEndAlign(block);
			uiBlockBeginAlign(block);
		}
	}
	uiBlockEndAlign(block);

	uiDefBut(block, BUT, 4000, "OK", (short)(x1+width-40),(short)(y2-35-20*MIN2(MAXNUMBUTROWS,a)), 25, (short)(sizey-50), 0, 0, 0, 0, 0, "OK: Assign Values");
	
	uiBoundsBlock(block, 5);

	event= uiDoBlocks(&listb, 0, 0);

	areawinset(curarea->win);
	
	if(event & UI_RETURN_OK) {
		
		varstr= &numbuts[0];
		for(a=0; a<tot; a++, varstr++) {
			if(varstr->type==TEX);
			else if ELEM( (varstr->type & BUTPOIN), FLO, INT ) memcpy(numbpoin[a], numbdata+a, 4);
			else if((varstr->type & BUTPOIN)==SHO ) *((short *)(numbpoin[a]))= *( (short *)(numbdata+a));
		}
		
		if(winevent) {
			ScrArea *sa;
		
			sa= G.curscreen->areabase.first;
			while(sa) {
				if(sa->spacetype==curarea->spacetype) addqueue(sa->win, winevent, 1);
				sa= sa->next;
			}
		}
		
		return 1;
	}
	return 0;
}

void add_numbut(int nr, int type, char *str, float min, float max, void *poin, char *tip)
{
	int tip_max = sizeof(numbuts[nr].tip);
	int name_max = sizeof(numbuts[nr].name);

	if(nr >= MAXNUMBUTS || (nr < 0)) return;

	numbuts[nr].type= type;
	
	numbuts[nr].min= min;
	numbuts[nr].max= max;
	
	if (type==LABEL) {
		/* evil use it tooltip for the label string to get around the 16 char limit of "name" */
		if (str) {
			strncpy(numbuts[nr].tip, str, tip_max);
			numbuts[nr].tip[tip_max-1] = 0;
		} else {
			strcpy(numbuts[nr].tip, "");
		}
	} else {
		/* for all other types */
		if (str) {
			strncpy(numbuts[nr].name, str, name_max);
			numbuts[nr].name[name_max-1] = 0;
		} else {
			strcpy(numbuts[nr].name, "");
		}
		if (tip) {
			strncpy(numbuts[nr].tip, tip, tip_max);
			numbuts[nr].tip[tip_max-1] = 0;
		} else {
			strcpy(numbuts[nr].tip, "");
		}
	}
	
	/*WATCH: TEX BUTTON EXCEPTION */
	
	numbpoin[nr]= poin;
	
	if ELEM( (type & BUTPOIN), FLO, INT ) memcpy(numbdata+nr, poin, 4);
	if((type & BUTPOIN)==SHO ) *((short *)(numbdata+nr))= *( (short *)poin);
	
	/* if( strncmp(numbuts[nr].name, "Rot", 3)==0 ) {
		float *fp;
		
		fp= (float *)(numbdata+nr);
		fp[0]= 180.0*fp[0]/M_PI;
	} */

}

void clever_numbuts(void)
{
	
	if(curarea->spacetype==SPACE_VIEW3D) {
		// panel now
	}
	else if(curarea->spacetype==SPACE_NLA){
		// panel now
	}
	else if(curarea->spacetype==SPACE_IPO) {
		// panel now
	}
	else if(curarea->spacetype==SPACE_SEQ) {
		// panel now
	}
	else if(curarea->spacetype==SPACE_IMAGE) {
		// panel now
	}
	else if(curarea->spacetype==SPACE_IMASEL) {
		clever_numbuts_imasel();
	}
	else if(curarea->spacetype==SPACE_OOPS) {
		clever_numbuts_oops();
	}
	else if(curarea->spacetype==SPACE_ACTION){
		// in its own queue
	}
	else if(curarea->spacetype==SPACE_FILE) {
		clever_numbuts_filesel();
	}
}


void replace_names_but(void)
{
	Image *ima= G.main->image.first;
	short len, tot=0;
	char old[64], new[64], temp[80];
	
	strcpy(old, "/");
	strcpy(new, "/");
	
	add_numbut(0, TEX, "Old:", 0, 63, old, 0);
	add_numbut(1, TEX, "New:", 0, 63, new, 0);

	if (do_clever_numbuts("Replace image name", 2, REDRAW) ) {
		
		len= strlen(old);
		
		while(ima) {
			
			if(strncmp(old, ima->name, len)==0) {
				
				strcpy(temp, new);
				strcat(temp, ima->name+len);
				BLI_strncpy(ima->name, temp, sizeof(ima->name));
				
				BKE_image_signal(ima, NULL, IMA_SIGNAL_FREE);
				
				tot++;
			}
			
			ima= ima->id.next;
		}

		notice("Replaced %d names", tot);
	}
	
}


/* ********************** NEW TOOLBOX ********************** */

ListBase tb_listb= {NULL, NULL};

#define TB_TAB	256
#define TB_ALT	512
#define TB_CTRL	1024
#define TB_PAD	2048
#define TB_SHIFT 4096

static void tb_do_hotkey(void *arg, int event)
{
	unsigned short i, key=0;
	unsigned short qual[] = { 0,0,0,0 };
	
	if(event & TB_CTRL) {
		qual[0] = LEFTCTRLKEY;
		event &= ~TB_CTRL;
	}
	if(event & TB_ALT) {
		qual[1] = LEFTALTKEY;
		event &= ~TB_ALT;
	}
	if(event & TB_SHIFT) {
		qual[2] = LEFTSHIFTKEY;
		event &= ~TB_SHIFT;
	}
	
	if(event & TB_TAB) key= TABKEY;
	else if(event & TB_PAD) {
		event &= ~TB_PAD;
		switch(event) {
		case '-': key= PADMINUS; break;
		case '+': key= PADPLUSKEY; break;
		case '0': key= PAD0; break;
		case '5': key= PAD5; break;
		case '/': key= PADSLASHKEY; break;
		case '.': key= PADPERIOD; break;
		case '*': key= PADASTERKEY; break;
		case 'h': key= HOMEKEY; break;
		case 'u': key= PAGEUPKEY; break;
		case 'd': key= PAGEDOWNKEY; break;
		}
	}
	else asciitoraw(event, &key, &qual[3]);

	for (i=0;i<4;i++)
	{
		if(qual[i]) mainqenter(qual[i], 1);
	}
	mainqenter(key, 1);
	mainqenter(key, 0);
	mainqenter(EXECUTE, 1);

	for (i=0;i<4;i++)
	{
		if(qual[i]) mainqenter(qual[i], 0);
	}
}

/* *************Select ********** */

static TBitem tb_object_select_layer1_5[]= {
{	0, "1", 	1, NULL},
{	0, "2", 	2, NULL},
{	0, "3", 	3, NULL},
{	0, "4", 	4, NULL},
{	0, "5", 	5, NULL},
{  -1, "", 		0, do_view3d_select_object_layermenu}};

static TBitem tb_object_select_layer6_10[]= {
{	0, "6", 	6, NULL},
{	0, "7", 	7, NULL},
{	0, "8", 	8, NULL},
{	0, "9", 	9, NULL},
{	0, "10", 	10, NULL},
{  -1, "", 		0, do_view3d_select_object_layermenu}};

static TBitem tb_object_select_layer11_15[]= {
{	0, "11", 	11, NULL},
{	0, "12",	12, NULL},
{	0, "13", 	13, NULL},
{	0, "14", 	14, NULL},
{	0, "15", 	15, NULL},
{  -1, "", 		0, do_view3d_select_object_layermenu}};

static TBitem tb_object_select_layer16_20[]= {
{	0, "16", 	16, NULL},
{	0, "17", 	17, NULL},
{	0, "18", 	18, NULL},
{	0, "19", 	19, NULL},
{	0, "20", 	20, NULL},
{  -1, "", 		0, do_view3d_select_object_layermenu}};

static TBitem tb_object_select_layer[]= {
{	0, "Layers 1-5", 	0, 		tb_object_select_layer1_5},
{	0, "Layers 6-10", 	0, 		tb_object_select_layer6_10},
{	0, "Layers 11-15", 	0, 		tb_object_select_layer11_15},
{	0, "Layers 16-20", 	0, 		tb_object_select_layer16_20},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_object_select_type[]= {
{	0, "Mesh", 		1, NULL},
{	0, "Curve", 	2, NULL},
{	0, "Surface", 	3, NULL},
{	0, "Meta", 		4, NULL},
{	0, "SEPR",		0, NULL},
{	0, "Armature", 	5, NULL},
{	0, "Lattice", 	6, NULL},
{	0, "Text", 		7, NULL},
{	0, "Empty", 	8, NULL},
{	0, "SEPR",		0, NULL},
{	0, "Camera", 	9, NULL},
{	0, "Lamp", 		10, NULL},
{  -1, "", 			0, do_view3d_select_object_typemenu}};

static TBitem tb_object_select_linked[]= {
{	0, "Object Ipo|Shift L, 1", 	1, NULL},
{	0, "ObData|Shift L, 2", 	2, NULL},
{	0, "Material|Shift L, 3", 	3, NULL},
{	0, "Texture|Shift L, 4", 	4, NULL},
{  -1, "", 			0, do_view3d_select_object_linkedmenu}};

static TBitem tb_object_select_grouped[]= {
{	0, "Children|Shift G, 1", 	1, NULL},
{	0, "Immediate Children|Shift G, 2", 	2, NULL},
{	0, "Parent|Shift G, 3", 	3, NULL},
{	0, "Siblings (Shared Parent)|Shift G, 4", 	4, NULL},
{	0, "Objects of Same Type|Shift G, 5", 	5, NULL},
{	0, "Objects on Shared Layers|Shift G, 6", 	6, NULL},
{	0, "Objects in Same Group|Shift G, 7", 	7, NULL},
{	0, "Object Hooks|Shift G, 8", 	8, NULL},
{	0, "Object PassIndex|Shift G, 9", 	9, NULL},
{  -1, "", 			0, do_view3d_select_object_groupedmenu}};

static TBitem tb_object_select[]= {
{	0, "Border Select|B", 	0, NULL},
{	0, "SEPR",				0, NULL},
{	0, "Select/Deselect All|A", 	1, NULL},
{	0, "Inverse",			2, NULL},
{	0, "Random",			3, NULL},
{	0, "Select All by Layer", 	0, 		tb_object_select_layer},
{	0, "Select All by Type", 	0, 		tb_object_select_type},
{	0, "SEPR",				0, NULL},
{	0, "Linked", 	0, 	tb_object_select_linked},
{	0, "Grouped", 	0, 	tb_object_select_grouped},
{  -1, "", 			0, do_view3d_select_objectmenu}};

static TBitem tb_face_select[]= {
{	0, "Border Select|B", 		0, NULL},
{	0, "SEPR",					0, NULL},
{	0, "Select/Deselect All|A", 2, NULL},
{	0, "Inverse",				3, NULL},
{	0, "Same UV",				4, NULL},
{	0, "SEPR",					0, NULL},
{	0, "Linked Faces|Ctrl L", 	5, NULL},
{  -1, "", 						0, do_view3d_select_faceselmenu}};

static TBitem tb_mesh_select[]= {
{	0, "Border Select|B",               0, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "Select/Deselect All|A",              2, NULL},
{	0, "Inverse|Ctrl I",                       3, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "Random...",			            5, NULL},
{	0, "Non-Manifold|Shift Ctrl Alt M", 9, NULL},
{	0, "Sharp Edges|Shift Ctrl Alt S", 14, NULL},
{	0, "Linked Flat Faces|Shift Ctrl Alt F", 15, NULL},
{	0, "Triangles|Shift Ctrl Alt 3",    11, NULL},
{	0, "Quads|Shift Ctrl Alt 4",        12, NULL},
{	0, "Non-Triangles/Quads|Shift Ctrl Alt 5", 13, NULL},
{	0, "Similar to Selection|Shift G", 21, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "More|Ctrl NumPad +",            7, NULL},
{	0, "Less|Ctrl NumPad -",            8, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "Linked Vertices|Ctrl L",        4, NULL},
{	0, "Vertex Path|W Alt 7",		16, NULL},
{	0, "Edge Loop|Ctrl E 6",		17, NULL},
{	0, "Edge Ring|Ctrl E 7",		18, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "Loop to Region|Ctrl E 8",		19, NULL},
{	0, "Region to Loop|Ctrl E 9",		20, NULL},
{  -1, "", 			0, do_view3d_select_meshmenu}};


static TBitem tb_curve_select[]= {
{	0, "Border Select|B", 	0, NULL},
{	0, "SEPR", 				0, NULL},
{	0, "(De)select All|A", 	2, NULL},
{	0, "Inverse", 			3, NULL},
{	0, "Random...", 		13, NULL},
{	0, "Every Nth", 		14, NULL},
{	0, "Row|Shift R", 			5, NULL}, /* shouldn't be visible in case of bezier curves*/
{	0, "SEPR",				0, NULL},
{	0, "(De)select First",	7, NULL},
{	0, "(De)select Last", 	8, NULL},
{	0, "Select Next",		11, NULL},
{	0, "Select Previous", 	12, NULL},
{	0, "SEPR",				0, NULL},
{	0, "More|Ctrl NumPad +",	9, NULL},
{	0, "Less|Ctrl NumPad -",	10, NULL},
{  -1, "", 				0, do_view3d_select_curvemenu}};

static TBitem tb_mball_select[]= {
{	0, "Border Select|B", 	0, NULL},
{	0, "SEPR", 		0, NULL},
{	0, "(De)select All|A", 	2, NULL},
{	0, "Inverse", 		3, NULL},
{	0, "SEPR",		0, NULL},
{	0, "Random...",		4, NULL},
{  -1, "",                      0, do_view3d_select_metaballmenu}};

static TBitem tb__select[]= {
{       0, "Border Select|B",   'b', NULL},
{       0, "(De)select All|A",  'a', NULL},
{  -1, "",                      0, tb_do_hotkey}};


/* *************Edit ********** */

static TBitem tb_edit[]= {
{	0, "Exit Editmode|Tab", 	TB_TAB, NULL},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_curve_edit_seg[]= {
{	0, "Subdivide|W, 1", 		0, NULL},
{	0, "Switch Direction|W, 2", 	1, NULL},
{  -1, "", 			0, do_view3d_edit_curve_segmentsmenu}};

static TBitem tb_curve_edit_cv[]= {
{	0, "Tilt|T", 	't', NULL},
{	0, "Clear Tilt|Alt T", 			TB_ALT|'t', NULL},
{	0, "Separate|P", 	'p', NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Automatic|Shift H", 		'H', NULL},
{	0, "Toggle Free/Aligned|H", 	'h', NULL},
{	0, "Vector|V", 					'v', NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Make Vertex Parent|Ctrl P", TB_CTRL|'p', NULL},
{	0, "Add Hook|Ctrl H",			TB_CTRL|'h', NULL},
{  -1, "", 			0, tb_do_hotkey}};


static TBitem tb_curve_edit[]= {
{	0, "Exit Editmode|Tab", 	TB_TAB, NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Extrude|E", 		'e', 		NULL},
{	0, "Duplicate|Shift D", 'D', 		NULL},
{	0, "Make Segment|F", 	'f', 		NULL},
{	0, "Toggle Cyclic|C", 	'c', 		NULL},
{	0, "Delete...|X", 		'x', 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Control Points", 	0, 		tb_curve_edit_cv},
{	0, "Segments", 	0, 		tb_curve_edit_seg},
{  -1, "", 			0, tb_do_hotkey}};


static TBitem tb_mesh_edit_vertex[]= {
{	0, "Merge...|Alt M", 		5, NULL},
{	0, "Rip|V",					7, NULL},
{	0, "Split|Y", 				4, 		NULL},
{	0, "Separate|P", 			3, 		NULL},
{	0, "SEPR",					0, NULL},
{	0, "Smooth|W, Alt 1", 			2, NULL},
{	0, "Remove Doubles|W, 6", 			1, NULL},
{	0, "SEPR",					0, NULL},
{	0, "Make Vertex Parent|Ctrl P", 	0, NULL},
{	0, "Add Hook|Ctrl H",		6, NULL},
{  -1, "", 			0, do_view3d_edit_mesh_verticesmenu}};

static TBitem tb_mesh_edit_edge[]= {
{	0, "Make Edge/Face|F", 			5, 		NULL},
{	0, "Collapse|Alt M", 14,			NULL},
{	0, "SEPR",						0, NULL},
{	0, "Bevel|W, Alt 2", 					6, 		NULL},
{	0, "Loop Subdivide|Ctrl R", 		4, 		NULL},
{	0, "Knife Subdivide...|Shift K", 	3, 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Subdivide|W, 1", 			2, 		NULL},
{	0, "Subdivide Fractal|W, 2", 	1, 		NULL},
{	0, "Subdivide Smooth|W, 3", 		0, 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Mark Seam|Ctrl E", 			7, 		NULL},
{	0, "Clear Seam|Ctrl E", 		8, 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Crease SubSurf|Shift E",	9, 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Rotate Edge CW|Ctrl E",	10, 		NULL},
{	0, "Rotate Edge CCW|Ctrl E",	11, 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Slide Edge|Ctrl E",	12, 		NULL},
{	0, "Delete Edge Loop|X",	13, 		NULL},
{  -1, "", 			0, do_view3d_edit_mesh_edgesmenu}};

static TBitem tb_mesh_edit_face[]= {
{	0, "Make Edge/Face|F", 			5, 		NULL},
{	0, "Fill|Shift F", 				0, 		NULL},
{	0, "Beautify Fill|Alt F", 			1, 		NULL},
{	0, "SEPR",					0, NULL},
{	0, "Convert to Triangles|Ctrl T", 	2, 		NULL},
{	0, "Convert to Quads|Alt J", 		3, 		NULL},
{	0, "Flip Triangle Edges|Ctrl Shift F", 	4, 		NULL},
{	0, "Set Smooth|Ctrl F, 3", 	6, 		NULL},
{	0, "Set Solid|Ctrl F, 4", 	7, 		NULL},
{  -1, "", 			0, do_view3d_edit_mesh_facesmenu}};


static TBitem tb_mesh_edit_normal[]= {
{	0, "Recalculate Outside|Ctrl N", 	2, 		NULL},
{	0, "Recalculate Inside|Ctrl Shift N", 	1, 		NULL},
{	0, "SEPR",					0, NULL},
{	0, "Flip|Ctrl F, 1", 				0, 		NULL},
{  -1, "", 			0, do_view3d_edit_mesh_normalsmenu}};

static TBitem tb_mesh_edit[]= {
{	0, "Exit Editmode|Tab", 	TB_TAB, NULL},
{	0, "Undo|Ctrl Z", 			'u', 		NULL},
{	0, "Redo|Ctrl Shift Z", 		'U', 		NULL},
{	0, "SEPR", 				0, 			NULL},
{	0, "Extrude|E", 		'e', 		NULL},
{	0, "Duplicate|Shift D", 'D', 		NULL},
{	0, "Delete...|X", 		'x', 		NULL},
{	0, "SEPR", 				0, 			NULL},
{	0, "Vertices", 		0, 		tb_mesh_edit_vertex},
{	0, "Edges", 		0, 		tb_mesh_edit_edge},
{	0, "Faces", 		0, 		tb_mesh_edit_face},
{	0, "Normals", 		0, 		tb_mesh_edit_normal},
{  -1, "", 			0, tb_do_hotkey}};


static TBitem tb_object_ipo[]= {
{	0, "Show/Hide", 	'k', NULL},
{	0, "Select Next", 	TB_PAD|'u', NULL},
{	0, "Select Prev", 	TB_PAD|'d', NULL},
{  -1, "", 			0, tb_do_hotkey}};


static TBitem tb_object_edit[]= {
{	0, "Enter Editmode|Tab", 	TB_TAB, NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Duplicate|Shift D", 		'D', 		NULL},
{	0, "Duplicate Linked|Alt D", 	TB_ALT|'d', NULL},
{	0, "Delete|X", 					'x', 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Object Keys", 	0, tb_object_ipo},
{  -1, "", 			0, tb_do_hotkey}};


/* ************* Type  ********** */

static TBitem tb_obdata_hide[]= {
{	0, "Show Hidden|Alt H", 		TB_ALT|'h', 		NULL},
{	0, "Hide Selected|H", 			'h', 		NULL},
{	0, "Hide Deselected|Shift H", 	'H', 		NULL},
{  -1, "", 			0, tb_do_hotkey}};

static void tb_do_mesh(void *arg, int event){
	switch(event) {
	case 1: common_insertkey(); break;
	case 2: G.f ^= G_DRAWEDGES; break;
	case 3: G.f ^= G_DRAWFACES; break;
	case 4: G.f ^= G_DRAWNORMALS; break;
	case 5: flip_subdivison(-1); break;
	}
	addqueue(curarea->win, REDRAW, 1);
}

static TBitem tb_mesh[]= {
{	0, "Insert Keyframe|I", 		1, 		NULL},
{	0, "SEPR", 						0, NULL},
{	0, "Show/Hide Edges", 			2, 		NULL},
{	0, "Show/Hide Faces", 			3, 		NULL},
{	0, "Show/Hide Normals", 		4, 		NULL},
{	0, "SEPR", 						0, 	NULL},
{	0, "Subdivision Surface", 		5, 		NULL},
{	0, "SEPR", 						0, NULL},
{	0, "Show/Hide Vertices", 	0, 		tb_obdata_hide},
{  -1, "", 			0, tb_do_mesh}};

static TBitem tb_curve_hide[]= {
{	0, "Show Hidden|Alt H", 		10, 		NULL},
{	0, "Hide Selected|Alt Ctrl H", 			11, 		NULL},
{  -1, "", 			0, do_view3d_edit_curve_showhidemenu}};


static TBitem tb_curve[]= {
{	0, "Insert Keyframe|I", 		'i', 		NULL},
{	0, "SEPR", 						0, NULL},
{	0, "Show/Hide Points", 	0, 		tb_curve_hide},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_obdata[]= {
{	0, "Duplicate|Shift D", 		'D', 		NULL},
{	0, "Delete|X", 					'x', 		NULL},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_object_parent[]= {
{	0, "Make Parent...|Ctrl P", 		TB_CTRL|'p', NULL},
{	0, "Clear Parent...|Alt P", 		TB_ALT|'p', NULL},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_object_track[]= {
{	0, "Make Track|Ctrl T", 		TB_CTRL|'t', NULL},
{	0, "Clear Track|Alt T", 		TB_ALT|'t', NULL},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_object[]= {
{	0, "Insert Keyframe|I", 		'i', 		NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Make Links...|Ctrl L", 		TB_CTRL|'l', NULL},
{	0, "Make Single User...|U", 	'u', 		NULL},
{	0, "Copy Attributes...|Ctrl C", TB_CTRL|'c', NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Parent", 	0, 		tb_object_parent},
{	0, "Track", 	0, 		tb_object_track},
{	0, "SEPR", 								0, NULL},
{	0, "Boolean Operation|W", 	'w', NULL},
{	0, "Join Objects...|Ctrl J", 	TB_CTRL|'j', NULL},
{	0, "Convert Object Type...|Alt C", 	TB_ALT|'c', NULL},
{	0, "SEPR", 								0, NULL},
{	0, "Move to Layer...|M", 		'm', NULL},
{  -1, "", 			0, tb_do_hotkey}};


/* *************VIEW ********** */

static void tb_do_view_dt(void *arg, int event){
	G.vd->drawtype= event;
	addqueue(curarea->win, REDRAW, 1);
}

static TBitem tb_view_dt[]= {
{	ICON_BBOX, "Bounding Box", 	1, NULL},
{	ICON_WIRE, "Wireframe|Z", 	2, NULL},
{	ICON_SOLID, "Solid|Z", 		3, NULL},
{	ICON_SMOOTH, "Shaded|Shift Z", 		4, NULL},
{	ICON_POTATO, "Textured|Alt Z", 	5, NULL},
{  -1, "", 			0, tb_do_view_dt}};

static TBitem tb_view_alignview[]= {
{	0, "Center View to Cursor|C", 		'c', NULL},
{	0, "Align Active Camera to View|Ctrl Alt NumPad 0",
TB_CTRL|TB_ALT|TB_PAD|'0', NULL}, 
{	0, "Align View to Selected|NumPad *", 		TB_PAD|'*', NULL},
{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_view[]= {
{	0, "Viewport Shading", 			0, tb_view_dt},
{	0, "SEPR", 						0, NULL},
{	0, "Ortho/Perspective|NumPad 5", 	TB_PAD|'5', NULL},
{	0, "Local/Global View|NumPad /", 	TB_PAD|'/', NULL},
{	0, "SEPR", 						0, NULL},
{	0, "Align View", 			0, tb_view_alignview},
{	0, "SEPR", 		0, NULL},
{	0, "View Selected|NumPad .", 	TB_PAD|'.', NULL},
{	0, "View All|Home", 		TB_PAD|'h', NULL},
{	0, "SEPR", 		0, NULL},
{	0, "Play Back Animation|Alt A", TB_ALT|'a', NULL},
{	0, "Camera Fly Mode|Shift F", TB_SHIFT|'f', NULL},
{  -1, "", 			0, tb_do_hotkey}};


/* *************TRANSFORM ********** */

static TBitem tb_transform_moveaxis[]= {
{	0, "X Global|G, X", 	0, NULL},
{	0, "Y Global|G, Y", 	1, NULL},
{	0, "Z Global|G, Z", 	2, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "X Local|G, X, X", 	3, NULL},
{	0, "Y Local|G, Y, Y", 	4, NULL},
{	0, "Z Local|G, Z, Z", 	5, NULL},
{  -1, "", 			0, do_view3d_transform_moveaxismenu}};

static TBitem tb_transform_rotateaxis[]= {
{	0, "X Global|R, X", 	0, NULL},
{	0, "Y Global|R, Y", 	1, NULL},
{	0, "Z Global|R, Z", 	2, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "X Local|R, X, X", 	3, NULL},
{	0, "Y Local|R, Y, Y", 	4, NULL},
{	0, "Z Local|R, Z, Z", 	5, NULL},
{  -1, "", 			0, do_view3d_transform_rotateaxismenu}};

static TBitem tb_transform_scaleaxis[]= {
{	0, "X Global|S, X", 	0, NULL},
{	0, "Y Global|S, Y", 	1, NULL},
{	0, "Z Global|S, Z", 	2, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "X Local|S, X, X", 	3, NULL},
{	0, "Y Local|S, Y, Y", 	4, NULL},
{	0, "Z Local|S, Z, Z", 	5, NULL},
{  -1, "", 			0, do_view3d_transform_scaleaxismenu}};

static void tb_do_transform_clearapply(void *arg, int event)
{
	Object *ob;
	ob= OBACT;
	
	switch(event)
	{
	    case 0: /* clear location */
			clear_object('g');
			break;
		case 1: /* clear rotation */
			clear_object('r');
			break;
		case 2: /* clear scale */
			clear_object('s');
			break;
		case 3: /* apply scale/rotation */
			apply_objects_locrot();
			break;
		case 4: /* apply scale/rotation */
			apply_objects_visual_tx();
			break;
		case 5: /* apply deformation */
			object_apply_deform(ob);
			break;
		case 6: /* make duplicates real */
			if (ob->transflag & OB_DUPLI) make_duplilist_real();
			else error("The active object does not have dupliverts");
			break;
	}
}

static TBitem tb_transform_clearapply[]= {
{	0, "Clear Location|Alt G", 		0, NULL},
{	0, "Clear Rotation|Alt R", 		1, NULL},
{	0, "Clear Scale|Alt S", 		2, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "Apply Scale/Rotation to ObData|Ctrl A, 1", 3, NULL},
{	0, "Apply Visual Transform|Ctrl A, 2", 4, NULL},
{	0, "Apply Deformation|Shift Ctrl A", 5, NULL},
{	0, "Make Duplicates Real|Shift Ctrl A", 6, NULL},
{  -1, "", 			0, tb_do_transform_clearapply}};

static TBitem tb_transform_snap[]= {
{	0, "Selection -> Grid|Shift S, 1", 		1, NULL},
{	0, "Selection -> Cursor|Shift S, 2", 	2, NULL},
{	0, "Selection -> Center|Shift S, 3", 3, NULL},
{	0, "Cursor -> Selection|Shift S, 4", 4, NULL},
{	0, "Cursor -> Grid|Shift S, 5", 		5, NULL},
{	0, "Cursor -> Active|Shift S, 6", 		6, NULL},
{  -1, "", 			0, do_view3d_edit_snapmenu}};

static void tb_do_transform(void *arg, int event)
{
	switch(event)
	{
		case 0: /* Grab/move */
			initTransform(TFM_TRANSLATION, CTX_NONE);
			Transform();
			break;
		case 1: /* Rotate */
			initTransform(TFM_ROTATION, CTX_NONE);
			Transform();
			break;
		case 2: /* Scale */
			initTransform(TFM_RESIZE,CTX_NONE);
			Transform();
			break;
		case 3: /* transform properties */
			add_blockhandler(curarea, VIEW3D_HANDLER_OBJECT, UI_PNL_UNSTOW);
			break;
		case 4: /* snap */
			snapmenu();
			break;
		case 5: /* Shrink/Fatten Along Normals */
			initTransform(TFM_SHRINKFATTEN, CTX_NONE);
			Transform();
			break;
		case 6: /* Shear */
			initTransform(TFM_SHEAR, CTX_NONE);
			Transform();
			break;
		case 7: /* Warp */
			initTransform(TFM_WARP, CTX_NONE);
			Transform();
			break;
		case 8: /* proportional edit (toggle) */
			if(G.scene->proportional) G.scene->proportional= 0;
			else G.scene->proportional= 1;
			break;
		case 10:
			docenter(0);
			break;
		case 11:
			docenter_new();
			break;
		case 12:
			docenter_cursor();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static TBitem tb_transform_object_mirror[]= {
{	0, "X Local|Ctrl M, 1", 	1, NULL},
{	0, "Y Local|Ctrl M, 2", 	2, NULL},
{	0, "Z Local|Ctrl M, 3", 	3, NULL},
{  -1, "", 			0, do_view3d_object_mirrormenu}};

static TBitem tb_transform[]= {
{	0, "Grab/Move|G", 	0, NULL},
{	0, "Grab/Move on Axis| ", 	0, tb_transform_moveaxis},
{	0, "Rotate|R", 		1, NULL},
{	0, "Rotate on Axis", 	0, tb_transform_rotateaxis},
{	0, "Scale|S", 		2, NULL},
{	0, "Scale on Axis", 	0, tb_transform_scaleaxis},
{	0, "SEPR", 					0, NULL},
{	0, "ObData to Center",		10, NULL},
{	0, "Center New",			11, NULL},
{	0, "Center Cursor",			12, NULL},
{	0, "SEPR", 					0, NULL},
{	ICON_MENU_PANEL, "Properties|N", 3, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "Mirror", 	0, tb_transform_object_mirror},
{	0, "SEPR", 					0, NULL},
{	0, "Snap", 		0, tb_transform_snap},
{	0, "SEPR", 					0, NULL},
{	0, "Clear/Apply", 	0, tb_transform_clearapply},
{  -1, "", 			0, tb_do_transform}};

static TBitem tb_transform_edit_mirror[]= {
{	0, "X Global|Ctrl M, 1", 	1, NULL},
{	0, "Y Global|Ctrl M, 2", 	2, NULL},
{	0, "Z Global|Ctrl M, 3", 	3, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "X Local|Ctrl M, 4", 	4, NULL},
{	0, "Y Local|Ctrl M, 5", 	5, NULL},
{	0, "Z Local|Ctrl M, 6", 	6, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "X View|Ctrl M, 7", 	7, NULL},
{	0, "Y View|Ctrl M, 8", 	8, NULL},
{	0, "Z View|Ctrl M, 9", 	9, NULL},
{  -1, "", 			0, do_view3d_edit_mirrormenu}};

static TBitem tb_transform_editmode1[]= {
{	0, "Grab/Move|G", 	0, NULL},
{	0, "Grab/Move on Axis| ", 	0, tb_transform_moveaxis},
{	0, "Rotate|R", 		1, NULL},
{	0, "Rotate on Axis", 	0, tb_transform_rotateaxis},
{	0, "Scale|S", 		2, NULL},
{	0, "Scale on Axis", 	0, tb_transform_scaleaxis},
{	0, "SEPR", 					0, NULL},
{	0, "Shrink/Fatten|Alt S", 5, NULL},
{	0, "Shear|Ctrl S", 6, NULL},
{	0, "Warp|Shift W", 	7, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "ObData to Center",		10, NULL},
{	0, "SEPR", 					0, NULL},
{	ICON_MENU_PANEL, "Properties|N", 3, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "Mirror", 	0, tb_transform_edit_mirror},
{	0, "SEPR", 					0, NULL},
{	0, "Snap", 		0, tb_transform_snap},
{	0, "SEPR", 					0, NULL},
{	0, "Proportional Edit|O", 	8, 		NULL},
{  -1, "", 			0, tb_do_transform}};


static TBitem tb_transform_editmode2[]= {
{	0, "Grab/Move|G", 	0, NULL},
{	0, "Grab/Move on Axis| ", 	0, tb_transform_moveaxis},
{	0, "Rotate|R", 		1, NULL},
{	0, "Rotate on Axis", 	0, tb_transform_rotateaxis},
{	0, "Scale|S", 		2, NULL},
{	0, "Scale on Axis", 	0, tb_transform_scaleaxis},
{	0, "SEPR", 					0, NULL},
{	ICON_MENU_PANEL, "Properties|N", 3, NULL},
{	0, "Snap", 		0, tb_transform_snap},
{  -1, "", 			0, tb_do_transform}};


/* *************ADD ********** */

static TBitem addmenu_curve[]= {
{	0, "Bezier Curve", 	0, NULL},
{	0, "Bezier Circle", 1, NULL},
{	0, "NURBS Curve", 	2, NULL},
{	0, "NURBS Circle", 	3, NULL},
{	0, "Path", 			4, NULL},
{  -1, "", 			0, do_info_add_curvemenu}};

static TBitem addmenu_surf[]= {
{	0, "NURBS Curve", 	0, NULL},
{	0, "NURBS Circle", 	1, NULL},
{	0, "NURBS Surface", 2, NULL},
{	0, "NURBS Tube", 	3, NULL},
{	0, "NURBS Sphere", 	4, NULL},
{	0, "NURBS Donut", 	5, NULL},
{  -1, "", 			0, do_info_add_surfacemenu}};

static TBitem addmenu_meta[]= {
{	0, "Meta Ball", 	0, NULL},
{	0, "Meta Tube", 	1, NULL},
{	0, "Meta Plane", 	2, NULL},
{	0, "Meta Ellipsoid", 3, NULL},
{	0, "Meta Cube", 	4, NULL},
{  -1, "", 			0, do_info_add_metamenu}};

static TBitem addmenu_lamp[]= {
{	0, "Lamp", 	0, NULL},
{	0, "Sun", 	1, NULL},
{	0, "Spot", 	2, NULL},
{	0, "Hemi", 3, NULL},
{	0, "Area", 	4, NULL},
{  -1, "", 			0, do_info_add_lampmenu}};

static TBitem addmenu_YF_lamp[]= {
{	0, "Lamp", 	0, NULL},
{	0, "Sun", 	1, NULL},
{	0, "Spot", 	2, NULL},
{	0, "Hemi", 3, NULL},
{	0, "Area", 	4, NULL},
{	0, "Photon", 	5, NULL},
{  -1, "", 			0, do_info_add_lampmenu}};


static TBitem addmenu_armature[]= {
{	0, "Bone", 	8, NULL},
{  -1, "", 			0, do_info_addmenu}};

/* dynamic items */
#define TB_ADD_MESH		0
#define TB_ADD_GROUP	7
#define TB_ADD_LAMP		10

static TBitem tb_add[]= {
{	0, "Mesh", 		0, NULL},
{	0, "Curve", 	1, addmenu_curve},
{	0, "Surface", 	2, addmenu_surf},
{	0, "Meta", 	3, addmenu_meta},
{	0, "Text", 		4, NULL},
{	0, "Empty", 	5, NULL},
{	0, "SEPR", 		0, NULL},
{	0, "Group", 	10, NULL},
{	0, "SEPR", 		0, NULL},
{	0, "Camera", 	6, NULL},
{	0, "Lamp", 		7, addmenu_lamp},
{	0, "SEPR", 		0, NULL},
{	0, "Armature", 	8, NULL},
{	0, "Lattice", 	9, NULL},
{  -1, "", 			0, do_info_addmenu}};

static TBitem tb_empty[]= {
{	0, "Nothing...", 	0, NULL},
{  -1, "", 		0, NULL}};


/* *************RENDER ********** */

static void tb_do_render(void *arg, int event){
	switch(event)
	{
		case 1: /* set render border */
			set_render_border();
			break;
		case 2: /* render */
			BIF_do_render(0);
			break;
		case 3: /* render anim */
			BIF_do_render(1);
			break;
		case 4: /* passepartout */
		{
			Camera *ca= NULL;
			if(G.vd->camera==NULL) return;
			
			if(G.vd->camera->type==OB_CAMERA)
				ca= G.vd->camera->data;
			else return;
				
			if (ca && (ca->flag & CAM_SHOWPASSEPARTOUT))
				ca->flag &= ~CAM_SHOWPASSEPARTOUT;
			else
				ca->flag |= CAM_SHOWPASSEPARTOUT;
			allqueue(REDRAWVIEW3D, 0);
		}
			break;
		case 5: /*preview render */
			toggle_blockhandler(curarea, VIEW3D_HANDLER_PREVIEW, 0);
			scrarea_queue_winredraw(curarea);
			break;
	}
}

static TBitem tb_render[]= {
	{       0, "Passepartout",                      4, NULL},
	{       0, "Set Border|Shift B",                1, NULL},
	{       0, "SEPR",                              0, NULL},
	{       0, "Render|F12",                        2, NULL},
	{       0, "Anim|Ctrl F12",                     3, NULL},
	{       0, "Preview|Shift P",                   5, NULL},
	{  -1, "",                      0, tb_do_render}};

/* ************************* NODES *********************** */


/* dynamic items */

static TBitem tb_node_addsh[]= {
	{	0, "Input",			1, NULL},
	{	0, "Output",		2, NULL},
	{	0, "Color",			3, NULL},
	{	0, "Vector",		4, NULL},
	{	0, "Convertor",	5, NULL},
	{	0, "Group",		6, NULL},
	{	0, "Dynamic",	7, NULL},
	{  -1, "", 			0, NULL}};

static TBitem tb_node_addcomp[]= {
	{	0, "Input",			1, NULL},
	{	0, "Output",		2, NULL},
	{	0, "Color",			3, NULL},
	{	0, "Vector",		4, NULL},
	{	0, "Filter",		5, NULL},
	{	0, "Convertor",	6, NULL},
	{ 	0, "Matte",		7, NULL},
	{	0, "Distort",	8, NULL},
	{	0, "Group",		9, NULL},
	{	0, "Dynamic",	10, NULL},
	{  	-1, "", 		0, NULL}};

/* do_node_addmenu() in header_node.c, prototype in BSE_headerbuttons.h */

/* dynamic toolbox sublevel */
static TBitem *node_add_sublevel(ListBase *storage, bNodeTree *ntree, int nodeclass)
{
	static TBitem _addmenu[]= { {	0, " ", 	0, NULL}, {  -1, "", 			0, NULL}};
	Link *link;
	TBitem *addmenu;
	int tot= 0, a;
	
	if(ntree) {
		if(nodeclass==NODE_CLASS_GROUP) {
			bNodeTree *ngroup= G.main->nodetree.first;
			for(; ngroup; ngroup= ngroup->id.next)
				if(ngroup->type==ntree->type)
					tot++;
		}
		else {
			bNodeType *ntype = ntree->alltypes.first;
			while(ntype) {
				if(ntype->nclass == nodeclass) {
					tot++;
				}
				ntype= ntype->next;
			}
		}
	}	
	if(tot==0) {
		return _addmenu;
	}
	
	link= MEM_callocN(sizeof(Link) + sizeof(TBitem)*(tot+1), "types menu");
	BLI_addtail(storage, link);
	addmenu= (TBitem *)(link+1);
	
	if(nodeclass==NODE_CLASS_GROUP) {
		bNodeTree *ngroup= G.main->nodetree.first;
		for(tot=0, a=0; ngroup; ngroup= ngroup->id.next, tot++) {
			if(ngroup->type==ntree->type) {
				addmenu[a].name= ngroup->id.name+2;
				addmenu[a].retval= NODE_GROUP_MENU+tot;	/* so we can use BLI_findlink() */
				a++;
			}
		}
	}
	else {
		bNodeType *type= ntree->alltypes.first;
		int script=0;
		for(a=0; type; type= type->next) {
			if( type->nclass == nodeclass ) {
				if(type->type == NODE_DYNAMIC) {
					if(type->id)
						addmenu[a].name= type->id->name+2;
					else
						addmenu[a].name= type->name;
					addmenu[a].retval= NODE_DYNAMIC_MENU+script;
					script++;
				} else {
					addmenu[a].name= type->name;
					addmenu[a].retval= type->type;
				}
				a++;
			}
		}
	}
	
	addmenu[a].icon= -1;	/* end signal */
	addmenu[a].name= "";
	addmenu[a].retval= a;
	addmenu[a].poin= do_node_addmenu;
	
	return addmenu;
}


static TBitem tb_node_node[]= {
	{	0, "Duplicate|Shift D", TB_SHIFT|'d', 		NULL},
	{	0, "Delete|X", 'x', 		NULL},
	{	0, "SEPR", 		0, NULL},
	{	0, "Make Link|F", 'f', NULL},
	{	0, "SEPR", 		0, NULL},
	{	0, "Make Group|Ctrl G", TB_CTRL|'g', 		NULL},
	{	0, "Ungroup|Alt G", TB_ALT|'g', 		NULL},
	{	0, "Edit Group|Tab", TB_TAB, NULL},
	{	0, "SEPR", 		0, NULL},
	{	0, "Hide/Unhide|H", 'h', NULL},
	{	0, "Rename|Ctrl R", TB_CTRL|'r', 		NULL},
	{	0, "SEPR", 		0, NULL},
	{	0, "Read Saved Render Results|R", 'r', NULL},
	{	0, "Show Cyclic Dependencies|C", 'c', NULL},
	{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_node_select[]= {
	{	0, "Select/Deselect All|A", 	'a', NULL},
	{	0, "Border Select|B", 	'b', NULL},
	{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_node_transform[]= {
	{	0, "Grab/Move|G", 'g', 		NULL},
	{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_node_view[]= {
	{	0, "Zoom In|NumPad +",	TB_PAD|'+', NULL},
	{	0, "Zoom Out|NumPad -",	TB_PAD|'-', NULL},
	{	0, "View All|Home",	TB_PAD|'h', NULL},
	{  -1, "", 			0, tb_do_hotkey}};


/* *********************************************** */

static uiBlock *tb_makemenu(void *arg)
{
	static int counter=0;
	TBitem *item= arg, *itemt;
	uiBlock *block;
	int xco= 0, yco= 0;
	char str[10];
	
	if(arg==NULL) return NULL;
	
	sprintf(str, "tb %d", counter++);
	block= uiNewBlock(&tb_listb, str, UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetCol(block, TH_MENU_ITEM);

	// last item has do_menu func, has to be stored in each button
	itemt= item;
	while(itemt->icon != -1) itemt++;
	uiBlockSetButmFunc(block, itemt->poin, NULL);

	// now make the buttons
	while(item->icon != -1) {

		if(strcmp(item->name, "SEPR")==0) {
			uiDefBut(block, SEPR, 0, "", xco, yco-=6, 50, 6, NULL, 0.0, 0.0, 0, 0, "");
		}
		else if(item->icon) {
			uiDefIconTextBut(block, BUTM, 1, item->icon, item->name, xco, yco-=20, 80, 19, NULL, 0.0, 0.0, 0, item->retval, "");
		}
		else if(item->poin) {
			uiDefIconTextBlockBut(block, tb_makemenu, item->poin, ICON_RIGHTARROW_THIN, item->name, 0, yco-=20, 80, 19, "");
		}
		else {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, item->name, xco, yco-=20, 80, 19, NULL, 0.0, 0.0, 0, item->retval, "");
		}
		
		if(yco <= -600) {
			yco = 0;
			xco += 80;
		}
		
		item++;
	}
	
	uiTextBoundsBlock(block, 60);
	
	/* direction is also set in the function that calls this */
	if(U.uiflag & USER_PLAINMENUS)
		uiBlockSetDirection(block, UI_RIGHT);
	else
		uiBlockSetDirection(block, UI_RIGHT|UI_CENTER);

	return block;
}

static int tb_mainx= 1234, tb_mainy= 0;
static void store_main(void *arg1, void *arg2)
{
	tb_mainx= (long)arg1;
	tb_mainy= (long)arg2;
}

static void do_group_addmenu(void *arg, int event)
{
	Object *ob;
	
	if(event<0) return;
	
	add_object_draw(OB_EMPTY);
	ob= OBACT;
	
	ob->dup_group= BLI_findlink(&G.main->group, event);
	if(ob->dup_group) {
		rename_id(&ob->id, ob->dup_group->id.name+2);

		id_us_plus((ID *)ob->dup_group);
		ob->transflag |= OB_DUPLIGROUP;
		DAG_scene_sort(G.scene);
	}
}

/* helper for create group menu */
static void tag_groups_for_toolbox(void)
{
	Group *group;
	GroupObject *go;
	
	for(group= G.main->group.first; group; group= group->id.next)
		group->id.flag |= LIB_DOIT;
	
	for(group= G.main->group.first; group; group= group->id.next) {
		if(group->id.flag & LIB_DOIT)
			for(go= group->gobject.first; go; go= go->next) 
				if(go->ob && go->ob->dup_group)
					go->ob->dup_group->id.flag &= ~LIB_DOIT;
	}
}

/* helper for create group menu */
/* note that group id.flag was set */
static int count_group_libs(void)
{
	Group *group;
	Library *lib;
	int tot= 0;
	
	for(lib= G.main->library.first; lib; lib= lib->id.next)
		lib->id.flag |= LIB_DOIT;
	
	for(group= G.main->group.first; group; group= group->id.next) {
		if(group->id.flag & LIB_DOIT) {
			if(group->id.lib && (group->id.lib->id.flag & LIB_DOIT)) {
				group->id.lib->id.flag &= ~LIB_DOIT;
				tot++;
			}
		}
	}
	return tot;
}

/* dynamic toolbox sublevel */
static TBitem *create_group_sublevel(ListBase *storage, Library *lib)
{
	static TBitem addmenu[]= { {	0, "No Groups", 	0, NULL}, {  -1, "", 			0, NULL}};
	Link *link;
	TBitem *groupmenu, *gm;
	Group *group;
	int a;
	int tot= BLI_countlist(&G.main->group);
	
	if(tot==0) {
		return addmenu;
	}
	
	/* build menu, we insert a Link before the array of TBitems */
	link= MEM_callocN(sizeof(Link) + sizeof(TBitem)*(tot+1), "group menu lib");
	BLI_addtail(storage, link);
	gm= groupmenu= (TBitem *)(link+1);
	for(a=0, group= G.main->group.first; group; group= group->id.next, a++) {
		if(group->id.lib==lib && (group->id.flag & LIB_DOIT)) {
			gm->name= group->id.name+2;
			gm->retval= a;
			gm++;
		}
	}
	gm->icon= -1;	/* end signal */
	gm->name= "";
	gm->retval= a;
	gm->poin= do_group_addmenu;
	
	return groupmenu;
}

static TBitem *create_group_all_sublevels(ListBase *storage)
{
	Library *lib;
	Group *group;
	Link *link;
	TBitem *groupmenu, *gm;
	int a;
	int totlevel= 0;
	int totlocal= 0;
	
	/* we add totlevel + local groups entries */
	
	/* let's skip group-in-group */
	tag_groups_for_toolbox();
	
	/* this call checks for skipped group-in-groups */
	totlevel= count_group_libs();
	
	for(group= G.main->group.first; group; group= group->id.next)
		if(group->id.flag & LIB_DOIT)
			if(group->id.lib==NULL)
				totlocal++;
	
	if(totlocal+totlevel==0)
		return create_group_sublevel(storage, NULL);
	
	/* build menu, we insert a Link before the array of TBitems */
	link= MEM_callocN(sizeof(Link) + sizeof(TBitem)*(totlocal+totlevel+1), "group menu");
	BLI_addtail(storage, link);
	gm= groupmenu= (TBitem *)(link+1);
	
	/* first all levels. libs with groups are not tagged */
	for(lib= G.main->library.first; lib; lib= lib->id.next) {
		if(!(lib->id.flag & LIB_DOIT)) {
			char *str;
			/* do some tricks to get .blend file name without extension */
			link= MEM_callocN(sizeof(Link) + 128, "string");
			BLI_addtail(storage, link);
			str= (char *)(link+1);
			BLI_strncpy(str, BLI_last_slash(lib->filename)+1, 128);
			if(strlen(str)>6) str[strlen(str)-6]= 0;
			gm->name= str;
			gm->retval= -1;
			gm->poin= create_group_sublevel(storage, lib);
			gm++;
		}
	}
	/* remaining groups */
	for(a=0, group= G.main->group.first; group; group= group->id.next, a++) {
		if(group->id.lib==NULL && (group->id.flag & LIB_DOIT)) {
			gm->name= group->id.name+2;
			gm->retval= a;
			gm++;
		}
	}
	gm->icon= -1;	/* end signal */
	gm->name= "";
	gm->retval= a;
	gm->poin= do_group_addmenu;
	
	return groupmenu;
}

static TBitem *create_mesh_sublevel(ListBase *storage)
{
	Link *link;
	TBitem *meshmenu, *mm;
	int totmenu= 10, totpymenu=0, a=0;
	
	/* Python Menu */
	BPyMenu *pym;
	
	/* count the python menu items*/
	for (pym = BPyMenuTable[PYMENU_ADDMESH]; pym; pym = pym->next, totpymenu++) {}
	if (totpymenu) totmenu += totpymenu+1; /* add 1 for the seperator */
	
	link= MEM_callocN(sizeof(Link) + sizeof(TBitem)*(totmenu+1), "mesh menu");
	BLI_addtail(storage, link);
	mm= meshmenu= (TBitem *)(link+1);
	
	mm->icon = 0; mm->retval= a; mm->name = "Plane"; 		mm++; a++;
	mm->icon = 0; mm->retval= a; mm->name = "Cube"; 		mm++; a++;
	mm->icon = 0; mm->retval= a; mm->name = "Circle"; 		mm++; a++;
	mm->icon = 0; mm->retval= a; mm->name = "UVsphere"; 	mm++; a++;
	mm->icon = 0; mm->retval= a; mm->name = "Icosphere"; 	mm++; a++;
	mm->icon = 0; mm->retval= a; mm->name = "Cylinder"; 	mm++; a++; a++;
	mm->icon = 0; mm->retval= a; mm->name = "Cone"; 		mm++; a++; 
	mm->icon = 0; mm->retval= 0; mm->name = "SEPR"; 		mm++;
	mm->icon = 0; mm->retval= a; mm->name = "Grid"; 		mm++; a++;
	mm->icon = 0; mm->retval= a; mm->name = "Monkey"; 		mm++; a++;
	/* a == 10 */
	
	if (totpymenu) {
		int i=0;
		mm->icon = 0; mm->retval= 0; mm->name = "SEPR"; 	mm++;
		
		/* note that we account for the 10 previous entries with i+4: */
		for (pym = BPyMenuTable[PYMENU_ADDMESH]; pym; pym = pym->next, i++) {
			mm->icon = ICON_PYTHON;
			mm->retval= i+20;
			mm->name = pym->name;	
			mm++; a++;
		}
	}
	
	/* terminate the menu */
	mm->icon= -1; mm->retval= a; mm->name= ""; mm->poin= do_info_add_meshmenu;
	
	return meshmenu;
}



void toolbox_n(void)
{
	uiBlock *block;
	uiBut *but;
	ListBase storage= {NULL, NULL};
	TBitem *menu1=NULL, *menu2=NULL, *menu3=NULL; 
	TBitem *menu4=NULL, *menu5=NULL, *menu6=NULL;
	TBitem *menu7=NULL;
	int dx=0;
	short event, mval[2], tot=0;
	char *str1=NULL, *str2=NULL, *str3=NULL, *str4=NULL, *str5=NULL, *str6=NULL, *str7=NULL;
	
	/* temporal too... when this flag is (was) saved, it should initialize OK */
	if(tb_mainx==1234) {
		if(U.uiflag & USER_PLAINMENUS) {
			tb_mainx= -32;
			tb_mainy= -5;
		} else {
			tb_mainx= 0;
			tb_mainy= -5;
		}
	}
	
	/* save present mouse position */
	toolbox_mousepos(mval, 1);

	mywinset(G.curscreen->mainwin); // we go to screenspace
	
	block= uiNewBlock(&tb_listb, "toolbox", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1);
	uiBlockSetCol(block, TH_MENU_ITEM);
	
	/* select context for main items */
	if(curarea->spacetype==SPACE_VIEW3D) {

		/* dynamic menu entries */
		tb_add[TB_ADD_GROUP].poin= create_group_all_sublevels(&storage);
		tb_add[TB_ADD_MESH].poin= create_mesh_sublevel(&storage);
		
		/* static */
		if (G.scene->r.renderer==R_YAFRAY)
			tb_add[TB_ADD_LAMP].poin= addmenu_YF_lamp;
		else
			tb_add[TB_ADD_LAMP].poin= addmenu_lamp;
		
		if(U.uiflag & USER_PLAINMENUS) {
			menu1= tb_add; str1= "Add";
			menu2= tb_object_edit; str2= "Edit";
			menu3= tb_object_select; str3= "Select";
			menu4= tb_transform; str4= "Transform";
			menu5= tb_object; str5= "Object";
			menu6= tb_view; str6= "View";
			menu7= tb_render; str7= "Render";

			dx= 96;
			tot= 7;
		} else {
			/* 3x2 layout menu */
			menu1= tb_object; str1= "Object";
			menu2= tb_add; str2= "Add";
			menu3= tb_object_select; str3= "Select";
			menu4= tb_object_edit; str4= "Edit";
			menu5= tb_transform; str5= "Transform";
			menu6= tb_view; str6= "View";

			dx= 64;
			tot= 6;
		}
		
		if(G.obedit) {
			if(U.uiflag & USER_PLAINMENUS) {
				switch(G.obedit->type){
				case OB_MESH:
					menu1= create_mesh_sublevel(&storage);
					menu2= tb_mesh_edit;
					menu3= tb_mesh_select;
					menu4= tb_transform_editmode1;
					menu5= tb_mesh; str5= "Mesh";
				break;
				case OB_CURVE:
					menu1= addmenu_curve;
					menu2= tb_curve_edit;
					menu3= tb_curve_select;
					menu4= tb_transform_editmode1;
					menu5= tb_curve; str5= "Curve";
				break;
				case OB_SURF:
					menu1= addmenu_surf;
					menu2= tb_curve_edit;
					menu3= tb_curve_select;
					menu4= tb_transform_editmode1;
					menu5= tb_curve; str5= "Surface";
				break;
				case OB_MBALL:
					menu1= addmenu_meta;
					menu2= tb_edit;
					menu3= tb_mball_select;
					menu4= tb_transform_editmode2;
					menu5= tb_obdata; str5= "Meta";
				break;
				case OB_ARMATURE:
					menu1= addmenu_armature;
					menu2= tb_edit;
					menu3= tb__select;
					menu4= tb_transform_editmode2;
					menu5= tb_obdata;str5= "Armature";
				break;
				case OB_LATTICE:
					menu1= tb_empty;
					menu2= tb_edit;
					menu3= tb__select;
					menu4= tb_transform_editmode1;
					menu5= tb_empty;str5= "Lattice";
				break;
				}
			} else {
				if(G.obedit->type==OB_MESH) {
					menu1= tb_mesh; str1= "Mesh";
					menu2= create_mesh_sublevel(&storage); 
					menu3= tb_mesh_select;
					menu4= tb_mesh_edit; 
					menu5= tb_transform_editmode1;
				}
				else if(G.obedit->type==OB_CURVE) {
					menu1= tb_curve; str1= "Curve";
					menu2= addmenu_curve;
					menu3= tb_curve_select;
					menu4= tb_curve_edit;
					menu5= tb_transform_editmode1;
				}
				else if(G.obedit->type==OB_SURF) {
					menu1= tb_curve; str1= "Surface";
					menu2= addmenu_surf; 
					menu3= tb_curve_select;
					menu4= tb_curve_edit;
					menu5= tb_transform_editmode1;
				}
				else if(G.obedit->type==OB_MBALL) {
					menu1= tb_obdata; str1= "Meta";
					menu2= addmenu_meta;
					menu3= tb__select;
					menu4= tb_edit;
					menu5= tb_transform_editmode2;
				}
				else if(G.obedit->type==OB_ARMATURE) {
					menu1= tb_obdata;str1= "Armature";
					menu2= addmenu_armature;
					menu3= tb__select;
					menu4= tb_edit;
					menu5= tb_transform_editmode2;
				}
				else if(G.obedit->type==OB_LATTICE) {
					menu1= tb_empty;str1= "Lattice";
					menu2= tb_empty;
					menu3= tb__select;
					menu4= tb_edit;
					menu5= tb_transform_editmode1;
				}
			}
		}
		else if (FACESEL_PAINT_TEST) {
			menu3 = tb_face_select;
		}
	}
	else if(curarea->spacetype==SPACE_NODE) {
		SpaceNode *snode= curarea->spacedata.first;
		
		if(snode->treetype==NTREE_COMPOSIT)
			menu1= tb_node_addcomp; 
		else
			menu1= tb_node_addsh; 
		str1= "Add";
		menu2= tb_node_node; str2= "Node";
		menu3= tb_node_select; str3= "Select";
		menu4= tb_node_transform; str4= "Transform";
		menu5= tb_node_view; str5= "View";
		
		if(snode->treetype==NTREE_SHADER) {
			menu1[0].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_INPUT);
			menu1[1].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_OUTPUT);
			menu1[2].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_OP_COLOR);
			menu1[3].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_OP_VECTOR);
			menu1[4].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_CONVERTOR);
			menu1[5].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_GROUP);
			menu1[6].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_OP_DYNAMIC);
		}
		else if(snode->treetype==NTREE_COMPOSIT) {
			menu1[0].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_INPUT);
			menu1[1].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_OUTPUT);
			menu1[2].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_OP_COLOR);
			menu1[3].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_OP_VECTOR);
			menu1[4].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_OP_FILTER);
			menu1[5].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_CONVERTOR);
			menu1[6].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_MATTE);
			menu1[7].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_DISTORT);
			menu1[8].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_GROUP);
			menu1[9].poin= node_add_sublevel(&storage, snode->nodetree, NODE_CLASS_OP_DYNAMIC);

		}
		
		dx= 96;
		tot= 5;
		
	}
	
	getmouseco_sc(mval);
	
	/* create the main buttons menu */
	if(tot==6) {
	
		/* check if it fits */
		if(mval[0]-1.5*dx+tb_mainx < 6) mval[0]= 6 + 1.5*dx -tb_mainx;
		else if(mval[0]+1.5*dx+tb_mainx > G.curscreen->sizex-6) 
			mval[0]= G.curscreen->sizex-6-1.5*dx-tb_mainx;

		if(mval[1]-20+tb_mainy < 6) mval[1]= 6+20 -tb_mainy;
		else if(mval[1]+20+tb_mainy > G.curscreen->sizey-6) 
			mval[1]= G.curscreen->sizey-6-20-tb_mainy;
	
		but=uiDefBlockBut(block, tb_makemenu, menu1, str1,	mval[0]-(1.5*dx)+tb_mainx,mval[1]+tb_mainy, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_TOP|UI_MAKE_RIGHT);
		uiButSetFunc(but, store_main, (void *)(long)dx, (void *)(long)-5);

		but=uiDefBlockBut(block, tb_makemenu, menu2, str2,	mval[0]-(0.5*dx)+tb_mainx,mval[1]+tb_mainy, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_TOP);
		uiButSetFunc(but, store_main, (void *)(long)0, (void *)(long)-5);

		but=uiDefBlockBut(block, tb_makemenu, menu3, str3,	mval[0]+(0.5*dx)+tb_mainx,mval[1]+tb_mainy, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_TOP|UI_MAKE_LEFT);
		uiButSetFunc(but, store_main, (void *)(long)-dx, (void *)(long)-5);

		but=uiDefBlockBut(block, tb_makemenu, menu4, str4,	mval[0]-(1.5*dx)+tb_mainx,mval[1]+tb_mainy-20, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_DOWN|UI_MAKE_RIGHT);
		uiButSetFunc(but, store_main, (void *)(long)dx, (void *)(long)5);

		but=uiDefBlockBut(block, tb_makemenu, menu5, str5,	mval[0]-(0.5*dx)+tb_mainx,mval[1]+tb_mainy-20, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_DOWN);
		uiButSetFunc(but, store_main, (void *)(long)0, (void *)(long)5);

		but=uiDefBlockBut(block, tb_makemenu, menu6, str6,	mval[0]+(0.5*dx)+tb_mainx,mval[1]+tb_mainy-20, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_DOWN|UI_MAKE_LEFT);
		uiButSetFunc(but, store_main, (void *)(long)-dx, (void *)(long)5);
	} else if (tot==5 || tot==7) {
                /* check if it fits, dubious */
		if(mval[0]-0.25*dx+tb_mainx < 6) mval[0]= 6 + 0.25*dx -tb_mainx;
		else if(mval[0]+0.25*dx+tb_mainx > G.curscreen->sizex-6)
		mval[0]= G.curscreen->sizex-6-0.25*dx-tb_mainx;

		if(mval[1]-20+tb_mainy < 6) mval[1]= 6+20 -tb_mainy;
		else if(mval[1]+20+tb_mainy > G.curscreen->sizey-6)
			mval[1]= G.curscreen->sizey-6-20-tb_mainy;

		but=uiDefIconTextBlockBut(block, tb_makemenu, menu1, ICON_RIGHTARROW_THIN, str1, mval[0]+tb_mainx,mval[1]+tb_mainy, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_RIGHT);
		uiButSetFunc(but, store_main, (void *)-32, (void *)-5);

		but=uiDefIconTextBlockBut(block, tb_makemenu, menu2, ICON_RIGHTARROW_THIN, str2, mval[0]+tb_mainx,mval[1]+tb_mainy-20, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_RIGHT);
		uiButSetFunc(but, store_main, (void *)-32, (void *)15);

		but=uiDefIconTextBlockBut(block, tb_makemenu, menu3, ICON_RIGHTARROW_THIN, str3, mval[0]+tb_mainx,mval[1]+tb_mainy-40, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_RIGHT);
		uiButSetFunc(but, store_main, (void *)-32, (void *)35);

		but=uiDefIconTextBlockBut(block, tb_makemenu, menu4, ICON_RIGHTARROW_THIN, str4, mval[0]+tb_mainx,mval[1]+tb_mainy-60, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_RIGHT);
		uiButSetFunc(but, store_main, (void *)-32, (void *)55);

		but=uiDefIconTextBlockBut(block, tb_makemenu, menu5, ICON_RIGHTARROW_THIN, str5, mval[0]+tb_mainx,mval[1]+tb_mainy-80, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_RIGHT);
		uiButSetFunc(but, store_main, (void *)-32, (void *)75);
		
		if(tot>5) {
			but=uiDefIconTextBlockBut(block, tb_makemenu, menu6, ICON_RIGHTARROW_THIN, str6, mval[0]+tb_mainx,mval[1]+tb_mainy-100, dx, 19, "");
			uiButSetFlag(but, UI_MAKE_RIGHT);
			uiButSetFunc(but, store_main, (void *)-32, (void *)95);
		}
		if(tot>6) {
			but=uiDefIconTextBlockBut(block, tb_makemenu, menu7, ICON_RIGHTARROW_THIN, str7, mval[0]+tb_mainx,mval[1]+tb_mainy-120, dx, 19, "");
			uiButSetFlag(but, UI_MAKE_RIGHT);
			uiButSetFunc(but, store_main, (void *)-32, (void *)105);
		}
	}
	
	uiBoundsBlock(block, 2);
	event= uiDoBlocks(&tb_listb, 0, 1);
	
	/* free all dynamic entries... */
	BLI_freelistN(&storage);
	
	mywinset(curarea->win);
}

void toolbox_n_add(void)
{
	reset_toolbox();
	toolbox_n();
}

void reset_toolbox(void)
{
	if(U.uiflag & USER_PLAINMENUS) {
		tb_mainx= -32;
		tb_mainy= -5;
	} else {
		tb_mainx= 0;
		tb_mainy= -5;
	}
}

/* general toolbox for python access */
void toolbox_generic( TBitem *generic_menu )
{
	uiBlock *block;
	uiBut *but;
	TBitem *menu;
	int dx=96;
	short event, mval[2];
	long ypos = -5;
	
	tb_mainx= -32;
	tb_mainy= -5;
	
	mywinset(G.curscreen->mainwin); // we go to screenspace
	
	block= uiNewBlock(&tb_listb, "toolbox", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1);
	uiBlockSetCol(block, TH_MENU_ITEM);
	
	getmouseco_sc(mval);
	
	menu= generic_menu;
	while(menu->icon != -1) menu++;
	uiBlockSetButmFunc(block, menu->poin, NULL);
	
	/* Add the menu */
	for (menu = generic_menu; menu->icon != -1; menu++) {
		if (menu->poin) {
			but=uiDefIconTextBlockBut(block, tb_makemenu, menu->poin, ICON_RIGHTARROW_THIN, menu->name, mval[0]+tb_mainx,mval[1]+tb_mainy+ypos+5, dx, 19, "");
			uiButSetFlag(but, UI_MAKE_RIGHT);
			
			uiButSetFunc(but, store_main, (void *)+32, (void *)ypos);
		} else {
			/* TODO - add icon support */
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, menu->name, mval[0]+tb_mainx,mval[1]+tb_mainy+ypos+5, dx, 19, NULL, 0.0, 0.0, 0, menu->retval, "");
		}
		ypos-=20;
	}
	
	uiBlockSetButmFunc(block, menu->poin, NULL);
	
	uiBoundsBlock(block, 2);
	event= uiDoBlocks(&tb_listb, 0, 1);
	
	mywinset(curarea->win);
	
	reset_toolbox();
}

/* save or restore mouse position when entering/exiting menus */
void toolbox_mousepos( short *mpos, int save )
{
	static short initpos[2];
	static int tog;
	
	if (save) {
		getmouseco_areawin(mpos);
		initpos[0]= mpos[0];
		initpos[1]= mpos[1];
		tog=1;
	} else {
		if (tog) {
			mpos[0]= initpos[0];
			mpos[1]= initpos[1];
		} else {
			getmouseco_areawin(mpos);
		}
		tog= 0;
	}
}
