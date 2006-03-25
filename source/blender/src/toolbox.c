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

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
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

int saveover(char *file)
{
	return (!BLI_exists(file) || confirm("Save over", file));
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
	uiDefBut(block, BUT, 1, "OK",	(short)(x1+136),(short)(y1+10),25,20, NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 5);

	ret= uiDoBlocks(&listb, 0);

	if(ret==UI_RETURN_OK) return 1;
	return 0;
}

short sbutton(char *var, float min, float max, char *str)
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
	
	uiDefButC(block, TEX, 0, str,	x1+5,y1+10,125,20, var,(float)min,(float)max, 0, 0, "");
	uiDefBut(block, BUT, 1, "OK",	x1+136,y1+10,25,20, NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 5);

	ret= uiDoBlocks(&listb, 0);

	if(ret==UI_RETURN_OK) return 1;
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
	uiDefBut(block, BUT, 1, "OK",(short)(x1+136),(short)(y1+10), 35, 20, NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 2);

	ret= uiDoBlocks(&listb, 0);

	if(ret==UI_RETURN_OK) return 1;
	return 0;
}

int movetolayer_buts(unsigned int *lay)
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
	
	/* buttons have 0 as return event, to prevent menu to close on hotkeys */
	
	uiBlockBeginAlign(block);
	for(a=0; a<5; a++) 
		uiDefButBitI(block, TOGR, 1<<a, 0, "",(short)(x1+a*dx),(short)(y1+dy),(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	for(a=0; a<5; a++) 
		uiDefButBitI(block, TOGR, 1<<(a+10), 0, "",(short)(x1+a*dx),(short)y1,(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	x1+= 5;
	
	uiBlockBeginAlign(block);
	for(a=5; a<10; a++) 
		uiDefButBitI(block, TOGR, 1<<a, 0, "",(short)(x1+a*dx),(short)(y1+dy),(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	for(a=5; a<10; a++) 
		uiDefButBitI(block, TOGR, 1<<(a+10), 0, "",(short)(x1+a*dx),(short)y1,(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	uiBlockEndAlign(block);

	x1-= 5;
	uiDefBut(block, BUT, 1, "OK", (short)(x1+10*dx+10), (short)y1, (short)(3*dx), (short)(2*dy), NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 2);

	ret= uiDoBlocks(&listb, 0);

	if(ret==UI_RETURN_OK) return 1;
	return 0;
}

int movetolayer_short_buts(short *lay)
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
	
	/* buttons have 0 as return event, to prevent menu to close on hotkeys */
	
	uiBlockBeginAlign(block);
	for(a=0; a<8; a++) 
		uiDefButBitS(block, TOGR, 1<<a, 0, "",(short)(x1+a*dx),(short)(y1+dy),(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	for(a=0; a<8; a++) 
		uiDefButBitS(block, TOGR, 1<<(a+8), 0, "",(short)(x1+a*dx),(short)y1,(short)dx,(short)dy, lay, 0, 0, 0, 0, "");
	
	uiBlockEndAlign(block);
	
	x1-= 5;
	uiDefBut(block, BUT, 1, "OK", (short)(x1+8*dx+10), (short)y1, (short)(3*dx), (short)(2*dy), NULL, 0, 0, 0, 0, "");
	
	uiBoundsBlock(block, 2);
	
	ret= uiDoBlocks(&listb, 0);
	
	if(ret==UI_RETURN_OK) return 1;
	return 0;
}


/* ********************** CLEVER_NUMBUTS ******************** */

#define MAXNUMBUTS	24

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
	int a, sizex, sizey, x1, y2;
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

	getmouseco_sc(mval);

	/* size */
	sizex= 235;
	sizey= 30+20*(tot+1);
	
	/* center */
	if(mval[0]<sizex/2) mval[0]=sizex/2;
	if(mval[1]<sizey/2) mval[1]=sizey/2;
	if(mval[0]>G.curscreen->sizex -sizex/2) mval[0]= G.curscreen->sizex -sizex/2;
	if(mval[1]>G.curscreen->sizey -sizey/2) mval[1]= G.curscreen->sizey -sizey/2;

	mywinset(G.curscreen->mainwin);
	
	x1= mval[0]-sizex/2; 
	y2= mval[1]+sizey/2;
	
	block= uiNewBlock(&listb, "numbuts", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_ENTER_OK);
	
	/* WATCH IT: TEX BUTTON EXCEPTION */
	/* WARNING: ONLY A SINGLE BIT-BUTTON POSSIBLE: WE WORK AT COPIED DATA! */
	
	BIF_ThemeColor(TH_MENU_TEXT); /* makes text readable on dark theme */
	
	uiDefBut(block, LABEL, 0, name,	(short)(x1+15), (short)(y2-35), (short)(sizex-60), 19, 0, 1.0, 0.0, 0, 0, ""); 
	
	/*
	if(name[0]=='A' && name[7]=='O') {
		y2 -= 20;
		uiDefBut(block, LABEL, 0, "Rotations in degrees!",	(short)(x1+15), (short)(y2-35), (short)(sizex-60), 19, 0, 0.0, 0.0, 0, 0, "");
	}*/
	
	uiBlockBeginAlign(block);
	varstr= &numbuts[0];
	for(a=0; a<tot; a++, varstr++) {
		
		if(varstr->type==TEX) {
			uiDefBut(block, TEX, 0,	varstr->name,(short)(x1+15),(short)(y2-55-20*a),(short)(sizex-60), 19, numbpoin[a], varstr->min, varstr->max, 0, 0, varstr->tip);
		}
		else  {
			
			if(varstr->type==LABEL) /* dont include the label when rounding the buttons */
				uiBlockEndAlign(block);
			
			uiDefBut(block, varstr->type, 0, varstr->name,(short)(x1+15),(short)(y2-55-20*a), (short)(sizex-60), 19, &(numbdata[a]), varstr->min, varstr->max, 100, 0, varstr->tip);
			
			if(varstr->type==LABEL)
				uiBlockBeginAlign(block);
		}

		
	}
	uiBlockEndAlign(block);

	uiDefBut(block, BUT, 4000, "OK", (short)(x1+sizex-40),(short)(y2-35-20*a), 25, (short)(sizey-50), 0, 0, 0, 0, 0, "OK: Assign Values");
	
	uiBoundsBlock(block, 5);

	event= uiDoBlocks(&listb, 0);

	areawinset(curarea->win);
	
	if(event & UI_RETURN_OK) {
		
		varstr= &numbuts[0];
		for(a=0; a<tot; a++, varstr++) {
			if(varstr->type==TEX);
			else if ELEM( (varstr->type & BUTPOIN), FLO, INT ) memcpy(numbpoin[a], numbdata+a, 4);
			else if((varstr->type & BUTPOIN)==SHO ) *((short *)(numbpoin[a]))= *( (short *)(numbdata+a));
			
			/*
			if( strncmp(varstr->name, "Rot", 3)==0 ) {
				float *fp;
				
				fp= numbpoin[a];
				fp[0]= M_PI*fp[0]/180.0;
			}*/
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
	if(nr>=MAXNUMBUTS) return;

	numbuts[nr].type= type;
	strcpy(numbuts[nr].name, str);
	numbuts[nr].min= min;
	numbuts[nr].max= max;
	if(tip) 
		strcpy(numbuts[nr].tip, tip);
	else
		strcpy(numbuts[nr].tip, "");
	
	
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
				
				if(ima->ibuf) IMB_freeImBuf(ima->ibuf);
				ima->ibuf= 0;
				ima->ok= 1;
				
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

typedef struct TBitem {
	int icon;
	char *name;
	int retval;
	void *poin;
} TBitem;

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
{	0, "Objects on Shared Layers|Shift G, 4", 	4, NULL},
{	0, "Objects in Same Group|Shift G, 5", 	5, NULL},
{  -1, "", 			0, do_view3d_select_object_groupedmenu}};

static TBitem tb_object_select[]= {
{	0, "Border Select|B", 	0, NULL},
{	0, "SEPR",				0, NULL},
{	0, "Select/Deselect All|A", 	1, NULL},
{	0, "Inverse",			2, NULL},
{	0, "Select All by Layer", 	0, 		tb_object_select_layer},
{	0, "Select All by Type", 	0, 		tb_object_select_type},
{	0, "SEPR",				0, NULL},
{	0, "Linked", 	0, 	tb_object_select_linked},
{	0, "Grouped", 	0, 	tb_object_select_grouped},
{  -1, "", 			0, do_view3d_select_objectmenu}};

static TBitem tb_mesh_select[]= {
{	0, "Border Select|B",               0, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "(De)select All|A",              2, NULL},
{	0, "Inverse",                       3, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "Random...",			            5, NULL},
{	0, "Non-Manifold|Shift Ctrl Alt M", 9, NULL},
{	0, "Sharp Edges|Shift Ctrl Alt S", 14, NULL},
{	0, "Linked Flat Faces|Shift Ctrl Alt F", 15, NULL},
{	0, "Triangles|Shift Ctrl Alt 3",    11, NULL},
{	0, "Quads|Shift Ctrl Alt 4",        12, NULL},
{	0, "Non-Triangles/Quads|Shift Ctrl Alt 5", 13, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "More|Ctrl NumPad +",            7, NULL},
{	0, "Less|Ctrl NumPad -",            8, NULL},
{	0, "SEPR",                          0, NULL},
{	0, "Linked Vertices|Ctrl L",        4, NULL},
{  -1, "", 			0, do_view3d_select_meshmenu}};


static TBitem tb_curve_select[]= {
{	0, "Border Select|B", 	0, NULL},
{	0, "SEPR", 				0, NULL},
{	0, "(De)select All|A", 	2, NULL},
{	0, "Inverse", 			3, NULL},
{	0, "Row|Shift R", 			5, NULL},
{  -1, "", 				0, do_view3d_select_curvemenu}};

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
{	0, "Remove Doubles|W, 5", 			1, NULL},
{	0, "SEPR",					0, NULL},
{	0, "Make Vertex Parent|Ctrl P", 	0, NULL},
{	0, "Add Hook|Ctrl H",		6, NULL},
{  -1, "", 			0, do_view3d_edit_mesh_verticesmenu}};

static TBitem tb_mesh_edit_edge[]= {
{	0, "Make Edge/Face|F", 			5, 		NULL},
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
{	0, "Flip Triangle Edges|Ctrl F", 	4, 		NULL},
{	0, "Set Smooth|W, Alt 3", 	6, 		NULL},
{	0, "Set Solid|W, Alt 4", 	7, 		NULL},
{  -1, "", 			0, do_view3d_edit_mesh_facesmenu}};


static TBitem tb_mesh_edit_normal[]= {
{	0, "Recalculate Outside|Ctrl N", 	2, 		NULL},
{	0, "Recalculate Inside|Ctrl Shift N", 	1, 		NULL},
{	0, "SEPR",					0, NULL},
{	0, "Flip|W, 9", 				0, 		NULL},
{  -1, "", 			0, do_view3d_edit_mesh_normalsmenu}};

static TBitem tb_mesh_edit[]= {
{	0, "Exit Editmode|Tab", 	TB_TAB, NULL},
{	0, "Undo|U", 			'u', 		NULL},
{	0, "Redo|Shift U", 		'U', 		NULL},
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
	case 5: flip_subdivison(OBACT, -1); break;
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
{	0, "Hide Selected|H", 			11, 		NULL},
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
{	0, "Centre View to Cursor|C", 		'c', NULL},
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
		case 2: /* clear size */
			clear_object('s');
			break;
		case 3: /* apply size/rotation */
			apply_object();
			break;
		case 4: /* apply deformation */
			object_apply_deform(ob);
			break;
		case 5: /* make duplicates real */
			if (ob->transflag & OB_DUPLI) make_duplilist_real();
			else error("The active object does not have dupliverts");
			break;
	}
}

static TBitem tb_transform_clearapply[]= {
{	0, "Clear Location|Alt G", 		0, NULL},
{	0, "Clear Rotation|Alt R", 		1, NULL},
{	0, "Clear Size|Alt S", 			2, NULL},
{	0, "SEPR", 					0, NULL},
{	0, "Apply Size/Rotation|Ctrl A", 3, NULL},
{	0, "Apply Deformation|Shift Ctrl A", 4, NULL},
{	0, "Make Duplicates Real|Shift Ctrl A", 5, NULL},
{  -1, "", 			0, tb_do_transform_clearapply}};

static TBitem tb_transform_snap[]= {
{	0, "Selection -> Grid|Shift S, 1", 		1, NULL},
{	0, "Selection -> Cursor|Shift S, 2", 	2, NULL},
{	0, "Cursor -> Grid|Shift S, 3", 		3, NULL},
{	0, "Cursor -> Selection|Shift S, 4", 4, NULL},
{	0, "Selection -> Center|Shift S, 5", 5, NULL},
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
			docentre(0);
			break;
		case 11:
			docentre_new();
			break;
		case 12:
			docentre_cursor();
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

static TBitem addmenu_mesh[]= {
{	0, "Plane", 	0, NULL},
{	0, "Cube", 		1, NULL},
{	0, "Circle", 	2, NULL},
{	0, "UVsphere", 	3, NULL},
{	0, "Icosphere", 4, NULL},
{	0, "Cylinder", 	5, NULL},
{	0, "Tube", 		6, NULL},
{	0, "Cone", 		7, NULL},
{	0, "SEPR",		0, NULL},
{	0, "Grid", 		8, NULL},
{	0, "Monkey", 	9, NULL},
{  -1, "", 			0, do_info_add_meshmenu}};

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
#define TB_ADD_GROUP	7
#define TB_ADD_LAMP		10

static TBitem tb_add[]= {
{	0, "Mesh", 		0, addmenu_mesh},
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
		case 4: /* render anim */
			if(G.scene->r.scemode & R_PASSEPARTOUT) G.scene->r.scemode &= ~R_PASSEPARTOUT;
			else G.scene->r.scemode |= R_PASSEPARTOUT;
			allqueue(REDRAWVIEW3D, 0);
			break;
	}
}

static TBitem tb_render[]= {
	{       0, "Passepartout",                      4, NULL},
	{       0, "Set Border",                        1, NULL},
	{       0, "SEPR",              0, NULL},
	{       0, "Render|F12",                        2, NULL},
	{       0, "Anim",                                      3, NULL},
	{  -1, "",                      0, tb_do_render}};

/* ************************* NODES *********************** */


/* dynamic items */
#define TB_SH_INPUTS		0
#define TB_SH_OUTPUTS		1
#define TB_SH_OP_COLOR		2
#define TB_SH_OP_VECTOR		3
#define TB_SH_CONVERTORS	4
#define TB_SH_GENERATORS	5
#define TB_SH_GROUPS		6

static TBitem tb_node_addsh[]= {
	{	0, "Inputs",		1, NULL},
	{	0, "Outputs",		2, NULL},
	{	0, "Color Ops",		3, NULL},
	{	0, "Vector Ops",	4, NULL},
	{	0, "Convertors",	5, NULL},
	{	0, "Generators",	6, NULL},
	{	0, "Groups",		7, NULL},
	{  -1, "", 			0, NULL}};


#define TB_CMP_INPUTS		0
#define TB_CMP_OUTPUTS		1
#define TB_CMP_OP_COLOR		2
#define TB_CMP_OP_VECTOR	3
#define TB_CMP_OP_FILTER	4
#define TB_CMP_CONVERTORS	5
#define TB_CMP_GENERATORS	6
#define TB_CMP_GROUPS		7

static TBitem tb_node_addcomp[]= {
	{	0, "Inputs",		1, NULL},
	{	0, "Outputs",		2, NULL},
	{	0, "Color Ops",		3, NULL},
	{	0, "Vector Ops",	4, NULL},
	{	0, "Filters",		5, NULL},
	{	0, "Convertors",	6, NULL},
	{	0, "Generators",	7, NULL},
	{	0, "Groups",		8, NULL},
	{  -1, "", 			0, NULL}};

static void do_node_addmenu(void *arg, int event)
{
	SpaceNode *snode= curarea->spacedata.first;
	float locx, locy;
	short mval[2];
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &locx, &locy);
	node_add_node(snode, event, locx, locy);
	
	addqueue(curarea->win, B_NODE_TREE_EXEC, 1);
	
	BIF_undo_push("Add Node");
	
}

/* dynamic toolbox sublevel */
static TBitem *node_add_sublevel(void **poin, bNodeTree *ntree, int nodeclass)
{
	static TBitem _addmenu[]= { {	0, "Empty", 	0, NULL}, {  -1, "", 			0, NULL}};
	bNodeType **typedefs;
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
			for(typedefs= ntree->alltypes; *typedefs; typedefs++)
				if( (*typedefs)->nclass == nodeclass )
					tot++;
		}
	}	
	if(tot==0) {
		*poin= _addmenu;
		return NULL;
	}
	
	addmenu= MEM_callocN(sizeof(TBitem)*(tot+1), "types menu");
	
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
		for(a=0, typedefs= ntree->alltypes; *typedefs; typedefs++) {
			if( (*typedefs)->nclass == nodeclass ) {
				addmenu[a].name= (*typedefs)->name;
				addmenu[a].retval= (*typedefs)->type;
				a++;
			}
		}
	}

	addmenu[a].icon= -1;	/* end signal */
	addmenu[a].name= "";
	addmenu[a].retval= a;
	addmenu[a].poin= do_node_addmenu;
	
	*poin= addmenu;
	
	return addmenu;
}


static TBitem tb_node_edit[]= {
	{	0, "Duplicate|Shift D", TB_SHIFT|'d', 		NULL},
	{	0, "Delete|X", 'x', 		NULL},
	{	0, "SEPR", 		0, NULL},
	{	0, "Make Group|Ctrl G", TB_CTRL|'g', 		NULL},
	{	0, "Ungroup|Alt G", TB_ALT|'g', 		NULL},
	{	0, "Edit Group", TB_TAB, NULL},
	{	0, "SEPR", 		0, NULL},
	{	0, "Hide/Unhide|H", 'h', NULL},
	{	0, "SEPR", 		0, NULL},
	{	0, "Show Cyclic Dependencies", 'c', NULL},
	{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_node_select[]= {
	{	0, "Select/Deselect All|A", 	'a', NULL},
	{	0, "Border Select|B", 	'b', NULL},
	{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_node_transform[]= {
	{	0, "Grab/Move|G", 'g', 		NULL},
	{  -1, "", 			0, tb_do_hotkey}};

static TBitem tb_node_view[]= {
	{	0, "Zoom in|NumPad +",	TB_PAD|'+', NULL},
	{	0, "Zoom out|NumPad -",	TB_PAD|'-', NULL},
	{	0, "View all|Home",	TB_PAD|'h', NULL},
	{  -1, "", 			0, tb_do_hotkey}};


/* *********************************************** */

static uiBlock *tb_makemenu(void *arg)
{
	static int counter=0;
	TBitem *item= arg, *itemt;
	uiBlock *block;
	int yco= 0;
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
			uiDefBut(block, SEPR, 0, "", 0, yco-=6, 50, 6, NULL, 0.0, 0.0, 0, 0, "");
		}
		else if(item->icon) {
			uiDefIconTextBut(block, BUTM, 1, item->icon, item->name, 0, yco-=20, 80, 19, NULL, 0.0, 0.0, 0, item->retval, "");
		}
		else if(item->poin) {
			uiDefIconTextBlockBut(block, tb_makemenu, item->poin, ICON_RIGHTARROW_THIN, item->name, 0, yco-=20, 80, 19, "");
		}
		else {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, item->name, 0, yco-=20, 80, 19, NULL, 0.0, 0.0, 0, item->retval, "");
		}
		item++;
	}
	uiTextBoundsBlock(block, 60);
	
	/* direction is also set in the function that calls this */
	if(U.uiflag & USER_PLAINMENUS)
		uiBlockSetDirection(block, UI_RIGHT);
	else
		uiBlockSetDirection(block, UI_RIGHT|UI_CENTRE);

	return block;
}

static int tb_mainx= 1234, tb_mainy= 0;
static void store_main(void *arg1, void *arg2)
{
	tb_mainx= (int)arg1;
	tb_mainy= (int)arg2;
}

static void do_group_addmenu(void *arg, int event)
{
	Object *ob;
	
	add_object_draw(OB_EMPTY);
	ob= OBACT;
	
	ob->dup_group= BLI_findlink(&G.main->group, event);
	if(ob->dup_group) {
		id_us_plus((ID *)ob->dup_group);
		ob->transflag |= OB_DUPLIGROUP;
		DAG_scene_sort(G.scene);
	}
}
							 
/* example of dynamic toolbox sublevel */
static TBitem *create_group_sublevel(void)
{
	static TBitem addmenu[]= { {	0, "No Groups", 	0, NULL}, {  -1, "", 			0, NULL}};
	TBitem *groupmenu;
	Group *group;
	int a;
	
	int tot= BLI_countlist(&G.main->group);
	
	if(tot==0) {
		tb_add[TB_ADD_GROUP].poin= addmenu;
		return NULL;
	}
	
	groupmenu= MEM_callocN(sizeof(TBitem)*(tot+1), "group menu");
	for(a=0, group= G.main->group.first; group; group= group->id.next, a++) {
		groupmenu[a].name= group->id.name+2;
		groupmenu[a].retval= a;
	}
	groupmenu[a].icon= -1;	/* end signal */
	groupmenu[a].name= "";
	groupmenu[a].retval= a;
	groupmenu[a].poin= do_group_addmenu;
	
	tb_add[TB_ADD_GROUP].poin= groupmenu;
	
	return groupmenu;
}

void toolbox_n(void)
{
	uiBlock *block;
	uiBut *but;
	TBitem *menu1=NULL, *menu2=NULL, *menu3=NULL; 
	TBitem *menu4=NULL, *menu5=NULL, *menu6=NULL;
	TBitem *menu7=NULL, *groupmenu= NULL;
	TBitem *node_add_gen= NULL, *node_add_group= NULL, *node_add_out= NULL, *node_add_in= NULL;
	TBitem *node_add_op_col= NULL, *node_add_op_filt= NULL, *node_add_op_vec= NULL, *node_add_con= NULL;
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
	
	mywinset(G.curscreen->mainwin); // we go to screenspace
	
	block= uiNewBlock(&tb_listb, "toolbox", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1);
	uiBlockSetCol(block, TH_MENU_ITEM);
	
	/* select context for main items */
	if(curarea->spacetype==SPACE_VIEW3D) {

		/* dynamic menu entries */
		groupmenu= create_group_sublevel();
		
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
					menu1= addmenu_mesh;
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
					menu3= tb__select;
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
					menu2= addmenu_mesh; 
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
		else {
		}
	}
	else if(curarea->spacetype==SPACE_NODE) {
		SpaceNode *snode= curarea->spacedata.first;
		
		if(snode->treetype==NTREE_COMPOSIT)
			menu1= tb_node_addcomp; 
		else
			menu1= tb_node_addsh; 
		str1= "Add";
		menu2= tb_node_edit; str2= "Edit";
		menu3= tb_node_select; str3= "Select";
		menu4= tb_node_transform; str4= "Transform";
		menu5= tb_node_view; str5= "View";
		
		if(snode->treetype==NTREE_SHADER) {
			node_add_in= node_add_sublevel(&menu1[TB_SH_INPUTS].poin, snode->nodetree, NODE_CLASS_INPUT);
			node_add_out= node_add_sublevel(&menu1[TB_SH_OUTPUTS].poin, snode->nodetree, NODE_CLASS_OUTPUT);
			node_add_op_col= node_add_sublevel(&menu1[TB_SH_OP_COLOR].poin, snode->nodetree, NODE_CLASS_OP_COLOR);
			node_add_op_vec= node_add_sublevel(&menu1[TB_SH_OP_VECTOR].poin, snode->nodetree, NODE_CLASS_OP_VECTOR);
			node_add_con= node_add_sublevel(&menu1[TB_SH_CONVERTORS].poin, snode->nodetree, NODE_CLASS_CONVERTOR);
			node_add_gen= node_add_sublevel(&menu1[TB_SH_GENERATORS].poin, snode->nodetree, NODE_CLASS_GENERATOR);
			node_add_group= node_add_sublevel(&menu1[TB_SH_GROUPS].poin, snode->nodetree, NODE_CLASS_GROUP);
		}
		else if(snode->treetype==NTREE_COMPOSIT) {
			node_add_in= node_add_sublevel(&menu1[TB_CMP_INPUTS].poin, snode->nodetree, NODE_CLASS_INPUT);
			node_add_out= node_add_sublevel(&menu1[TB_CMP_OUTPUTS].poin, snode->nodetree, NODE_CLASS_OUTPUT);
			node_add_op_col= node_add_sublevel(&menu1[TB_CMP_OP_COLOR].poin, snode->nodetree, NODE_CLASS_OP_COLOR);
			node_add_op_filt= node_add_sublevel(&menu1[TB_CMP_OP_FILTER].poin, snode->nodetree, NODE_CLASS_OP_FILTER);
			node_add_op_vec= node_add_sublevel(&menu1[TB_CMP_OP_VECTOR].poin, snode->nodetree, NODE_CLASS_OP_VECTOR);
			node_add_con= node_add_sublevel(&menu1[TB_CMP_CONVERTORS].poin, snode->nodetree, NODE_CLASS_CONVERTOR);
			node_add_gen= node_add_sublevel(&menu1[TB_CMP_GENERATORS].poin, snode->nodetree, NODE_CLASS_GENERATOR);
			node_add_group= node_add_sublevel(&menu1[TB_CMP_GROUPS].poin, snode->nodetree, NODE_CLASS_GROUP);
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
		uiButSetFunc(but, store_main, (void *)dx, (void *)-5);

		but=uiDefBlockBut(block, tb_makemenu, menu2, str2,	mval[0]-(0.5*dx)+tb_mainx,mval[1]+tb_mainy, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_TOP);
		uiButSetFunc(but, store_main, (void *)0, (void *)-5);

		but=uiDefBlockBut(block, tb_makemenu, menu3, str3,	mval[0]+(0.5*dx)+tb_mainx,mval[1]+tb_mainy, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_TOP|UI_MAKE_LEFT);
		uiButSetFunc(but, store_main, (void *)-dx, (void *)-5);

		but=uiDefBlockBut(block, tb_makemenu, menu4, str4,	mval[0]-(1.5*dx)+tb_mainx,mval[1]+tb_mainy-20, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_DOWN|UI_MAKE_RIGHT);
		uiButSetFunc(but, store_main, (void *)dx, (void *)5);

		but=uiDefBlockBut(block, tb_makemenu, menu5, str5,	mval[0]-(0.5*dx)+tb_mainx,mval[1]+tb_mainy-20, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_DOWN);
		uiButSetFunc(but, store_main, (void *)0, (void *)5);

		but=uiDefBlockBut(block, tb_makemenu, menu6, str6,	mval[0]+(0.5*dx)+tb_mainx,mval[1]+tb_mainy-20, dx, 19, "");
		uiButSetFlag(but, UI_MAKE_DOWN|UI_MAKE_LEFT);
		uiButSetFunc(but, store_main, (void *)-dx, (void *)5);
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
	event= uiDoBlocks(&tb_listb, 0);
	
	/* free all dynamic entries... clumsy! */
	if(groupmenu) MEM_freeN(groupmenu);
	
	if(node_add_in) MEM_freeN(node_add_in);
	if(node_add_out) MEM_freeN(node_add_out);
	if(node_add_op_col) MEM_freeN(node_add_op_col);
	if(node_add_op_filt) MEM_freeN(node_add_op_filt);
	if(node_add_op_vec) MEM_freeN(node_add_op_vec);
	if(node_add_con) MEM_freeN(node_add_con);
	if(node_add_gen) MEM_freeN(node_add_gen);
	if(node_add_group) MEM_freeN(node_add_group);
	
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
