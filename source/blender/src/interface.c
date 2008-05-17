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

/* 
     a full doc with API notes can be found in bf-blender/blender/doc/interface_API.txt

 */
 

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif // INTERNATIONAL

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_color_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_vfont_types.h"

#include "BKE_blender.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BIF_cursors.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_keyval.h"
#include "BIF_mainqueue.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_glutil.h"
#include "BIF_editfont.h"
#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_butspace.h"
#include "BIF_previewrender.h"

#include "BSE_view.h"

#include "BPY_extern.h" /* for BPY_button_eval */

#include "GHOST_Types.h" /* for tablet data */

#include "mydevice.h"
#include "interface.h"
#include "blendef.h"
#include "winlay.h"

#define INSIDE_BLOCK		1
#define INSIDE_PANEL_HEADER	2
#define INSIDE_PANEL_SCALE	3

/* naming conventions:
 * 
 * uiBlahBlah()		external function
 * ui_blah_blah()	internal function
 */

/***/
/* ************ GLOBALS ************* */

float UIwinmat[4][4];
static int UIlock= 0, UIafterval;
static char *UIlockstr=NULL;

static void (*UIafterfunc_butm)(void *arg, int event);
static void (*UIafterfunc_but)(void *arg1, void *arg2);
static void *UIafterfunc_arg1, *UIafterfunc_arg2;

static uiFont UIfont[UI_ARRAY];  // no init needed
uiBut *UIbuttip;

static char but_copypaste_str[256]="";
static double but_copypaste_val=0.0;
static float but_copypaste_rgb[3];
static ColorBand but_copypaste_coba;

/* ************* PROTOTYPES ***************** */

static void ui_set_but_val(uiBut *but, double value);
static void ui_do_but_tip(uiBut *buttip);

/* ****************************** */

static int uibut_contains_pt(uiBut *but, short *pt)
{
	return ((but->x1<pt[0] && but->x2>=pt[0]) && 
			(but->y1<pt[1] && but->y2>=pt[1]));
}

static void uibut_do_func(uiBut *but)
{
	if (but->func) {
		but->func(but->func_arg1, but->func_arg2);
	}
}

/* ************* window matrix ************** */


void ui_graphics_to_window(int win, float *x, float *y)	/* for rectwrite  */
{
	float gx, gy;
	int sx, sy;
	int getsizex, getsizey;

	bwin_getsize(win, &getsizex, &getsizey);
	bwin_getsuborigin(win, &sx, &sy);

	gx= *x;
	gy= *y;
	*x= ((float)sx) + ((float)getsizex)*(0.5+ 0.5*(gx*UIwinmat[0][0]+ gy*UIwinmat[1][0]+ UIwinmat[3][0]));
	*y= ((float)sy) + ((float)getsizey)*(0.5+ 0.5*(gx*UIwinmat[0][1]+ gy*UIwinmat[1][1]+ UIwinmat[3][1]));
}

void ui_graphics_to_window_rct(int win, rctf *graph, rcti *winr)
{
	float gx, gy;
	int sx, sy;
	int getsizex, getsizey;
	
	bwin_getsize(win, &getsizex, &getsizey);
	bwin_getsuborigin(win, &sx, &sy);
	
	gx= graph->xmin;
	gy= graph->ymin;
	winr->xmin= (int)((float)sx) + ((float)getsizex)*(0.5+ 0.5*(gx*UIwinmat[0][0]+ gy*UIwinmat[1][0]+ UIwinmat[3][0]));
	winr->ymin= (int)((float)sy) + ((float)getsizey)*(0.5+ 0.5*(gx*UIwinmat[0][1]+ gy*UIwinmat[1][1]+ UIwinmat[3][1]));
	gx= graph->xmax;
	gy= graph->ymax;
	winr->xmax= (int)((float)sx) + ((float)getsizex)*(0.5+ 0.5*(gx*UIwinmat[0][0]+ gy*UIwinmat[1][0]+ UIwinmat[3][0]));
	winr->ymax= (int)((float)sy) + ((float)getsizey)*(0.5+ 0.5*(gx*UIwinmat[0][1]+ gy*UIwinmat[1][1]+ UIwinmat[3][1]));
}


void ui_window_to_graphics(int win, float *x, float *y)	/* for mouse cursor */
{
	float a, b, c, d, e, f, px, py;
	int getsizex, getsizey;
		
	bwin_getsize(win, &getsizex, &getsizey);

	a= .5*((float)getsizex)*UIwinmat[0][0];
	b= .5*((float)getsizex)*UIwinmat[1][0];
	c= .5*((float)getsizex)*(1.0+UIwinmat[3][0]);

	d= .5*((float)getsizey)*UIwinmat[0][1];
	e= .5*((float)getsizey)*UIwinmat[1][1];
	f= .5*((float)getsizey)*(1.0+UIwinmat[3][1]);
	
	px= *x;
	py= *y;
	
	*y=  (a*(py-f) + d*(c-px))/(a*e-d*b);
	*x= (px- b*(*y)- c)/a;
	
}


/* ************* SAVE UNDER ************ */

/* new method: 

OverDraw *ui_begin_overdraw(int minx, int miny, int maxx, int maxy);
- enforces mainwindow to become active
- grabs copy from frontbuffer, pastes in back

void ui_flush_overdraw(OverDraw *od);
- copies backbuffer to front

void ui_refresh_overdraw(Overdraw *od);
- pastes in back copy of frontbuffer again for fresh drawing

void ui_end_overdraw(OverDraw *od);
- puts back on frontbuffer saved image
- frees copy
- sets back active blender area
- signals backbuffer to be corrupt (sel buffer!)

*/

/* frontbuffer updates now glCopyPixels too, with block->flush rect */

/* new idea for frontbuffer updates:

- hilites: with blended poly?

- full updates... thats harder, but:
  - copy original
  - before draw, always paste to backbuf
  - flush
  - always end with redraw event for full update

*/

static void myglCopyPixels(int a, int b, int c, int d, int e)
{
	if(G.rt==2) {
		unsigned int *buf= MEM_mallocN(4*c*d, "temp glcopypixels");
		glReadPixels(a, b, c, d, GL_RGBA, GL_UNSIGNED_BYTE, buf);
		glDrawPixels(c, d, GL_RGBA, GL_UNSIGNED_BYTE, buf);
		MEM_freeN(buf);
	}
	else glCopyPixels(a, b, c, d, e);
}

typedef struct {
	short x, y, sx, sy, oldwin;
	unsigned int *rect;
} uiOverDraw;


static uiOverDraw *ui_begin_overdraw(int minx, int miny, int maxx, int maxy)
{
	uiOverDraw *od=NULL;
	
	// dirty patch removed for sun and sgi to mywindow.c commented out
	
	/* clip with actual window size */
	if(minx < 0) minx= 0;
	if(miny < 0) miny= 0;
	if(maxx >= G.curscreen->sizex) maxx= G.curscreen->sizex-1;
	if(maxy >= G.curscreen->sizey) maxy= G.curscreen->sizey-1;

	if(minx<maxx && miny<maxy) {
		od= MEM_callocN(sizeof(uiOverDraw), "overdraw");	
		
		od->x= minx;
		od->y= miny;
		od->sx= maxx-minx;
		od->sy= maxy-miny;
		od->rect= MEM_mallocN(od->sx*od->sy*4, "temp_frontbuffer_image");

		od->oldwin= mywinget();
		mywinset(G.curscreen->mainwin);
		/* grab front */
		glReadBuffer(GL_FRONT);
		glReadPixels(od->x, od->y, od->sx, od->sy, GL_RGBA, GL_UNSIGNED_BYTE, od->rect);
		glReadBuffer(GL_BACK);
		/* paste in back */
		glDisable(GL_DITHER);
		glRasterPos2f(od->x, od->y);
		glDrawPixels(od->sx, od->sy, GL_RGBA, GL_UNSIGNED_BYTE, od->rect);
		glEnable(GL_DITHER);
	}
	
	return od;
}

static void ui_flush_overdraw(uiOverDraw *od)
{

	if(od==NULL) return;
	glDisable(GL_DITHER);
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_FRONT);
	glRasterPos2s(od->x, od->y);
	myglCopyPixels(od->x, od->y, od->sx, od->sy, GL_COLOR);
	glEnable(GL_DITHER);
	bglFlush();
	glDrawBuffer(GL_BACK);
}

/* special flush version to enable transparent menus */
static void ui_block_flush_overdraw(uiBlock *block)
{
	
	if(block->flag & UI_BLOCK_LOOP) {
		char col[4];
		
		BIF_GetThemeColor4ubv(TH_MENU_BACK, col);
		if(col[3]!=255) {
			uiBut *bt;
			uiOverDraw *od= block->overdraw;

			/* completely draw all! */
			glRasterPos2s(od->x, od->y);
			glDrawPixels(od->sx, od->sy, GL_RGBA, GL_UNSIGNED_BYTE, od->rect);
			
			uiDrawMenuBox(block->minx, block->miny, block->maxx, block->maxy, block->flag);
			for (bt= block->buttons.first; bt; bt= bt->next) {
				ui_draw_but(bt);
			}
		}
	}
	
	ui_flush_overdraw(block->overdraw);
}

static void ui_end_overdraw(uiOverDraw *od)
{
	if(od==NULL) return;
	
	glDisable(GL_DITHER);

	// clear in back
	glRasterPos2s(od->x, od->y);
	glDrawPixels(od->sx, od->sy, GL_RGBA, GL_UNSIGNED_BYTE, od->rect);

	// clear in front
	glDrawBuffer(GL_FRONT);
	glRasterPos2s(od->x, od->y);
	glDrawPixels(od->sx, od->sy, GL_RGBA, GL_UNSIGNED_BYTE, od->rect);

	bglFlush();
	glDrawBuffer(GL_BACK);
	glEnable(GL_DITHER);
	
	if(od->oldwin) mywinset(od->oldwin);
	
	MEM_freeN(od->rect);
	MEM_freeN(od);

	markdirty_all_back();	// sets flags only
}

/* ****************** live updates for hilites and button presses *********** */

void ui_block_flush_back(uiBlock *block)
{
	int minx, miny, sizex, sizey;
	
	/* note; this routine also has to work for block loop */
	if(block->needflush==0) return;

	/* exception, when we cannot use backbuffer for draw... */
	if(block->flag & UI_BLOCK_FRONTBUFFER) {
		bglFlush();
		glDrawBuffer(GL_BACK);
		block->needflush= 0;
		return;
	}
	
	/* copy pixels works on window coords, so we move to window space */

	ui_graphics_to_window(block->win, &block->flush.xmin, &block->flush.ymin);
	ui_graphics_to_window(block->win, &block->flush.xmax, &block->flush.ymax);
	minx= floor(block->flush.xmin);
	miny= floor(block->flush.ymin);
	sizex= ceil(block->flush.xmax-block->flush.xmin);
	sizey= ceil(block->flush.ymax-block->flush.ymin);

	if(sizex>0 && sizey>0) {
		glPushMatrix();
		mywinset(G.curscreen->mainwin);
		
		glDisable(GL_DITHER);
		glReadBuffer(GL_BACK);
		glDrawBuffer(GL_FRONT);
		glRasterPos2i(minx, miny);
#ifdef __sun__		
		myglCopyPixels(minx, miny+1, sizex, sizey, GL_COLOR);
#else
		myglCopyPixels(minx, miny, sizex, sizey, GL_COLOR);
#endif
		glEnable(GL_DITHER);
		bglFlush();
		glDrawBuffer(GL_BACK);

		mywinset(block->win);
		glPopMatrix();
		
		markdirty_win_back(block->win);
	}

	block->needflush= 0; 
}

/* merge info for live updates in frontbuf */
void ui_block_set_flush(uiBlock *block, uiBut *but)
{
	/* clear signal */
	if(but==NULL) {
		block->needflush= 0; 

		block->flush.xmin= 0.0;
		block->flush.xmax= 0.0;
	}
	else {
		/* exception, when we cannot use backbuffer for draw... */
		if(block->flag & UI_BLOCK_FRONTBUFFER) {
			glDrawBuffer(GL_FRONT);
		}
		else if(block->needflush==0) {
			/* first rect */
			block->flush.xmin= but->x1;
			block->flush.xmax= but->x2;
			block->flush.ymin= but->y1;
			block->flush.ymax= but->y2;
			
		}
		else {
			/* union of rects */
			if(block->flush.xmin > but->x1) block->flush.xmin= but->x1;
			if(block->flush.xmax < but->x2) block->flush.xmax= but->x2;
			if(block->flush.ymin > but->y1) block->flush.ymin= but->y1;
			if(block->flush.ymax < but->y2) block->flush.ymax= but->y2;
		}
		
		block->needflush= 1;
		
	}
}

/* ******************* copy and paste ********************  */

/* c = copy, v = paste */
/* return 1 when something changed */
static int ui_but_copy_paste(uiBut *but, char mode)
{
	void *poin;
	
	if(mode=='v' && but->lock) return 0;
	poin= but->poin;
		
	if ELEM4(but->type, NUM, NUMABS, NUMSLI, HSVSLI) {
		
		if(poin==NULL);
		else if(mode=='c') {
			but_copypaste_val= ui_get_but_val(but);
		}
		else {
			ui_set_but_val(but, but_copypaste_val);
			uibut_do_func(but);
			ui_check_but(but);
			return 1;
		}
	}
	else if(but->type==COL) {
		
		if(poin==NULL);
		else if(mode=='c') {
			if(but->pointype==FLO) {
				float *fp= (float *) poin;
				but_copypaste_rgb[0]= fp[0];
				but_copypaste_rgb[1]= fp[1];
				but_copypaste_rgb[2]= fp[2];	
			}	
			else if (but->pointype==CHA) {
				char *cp= (char *) poin;
				but_copypaste_rgb[0]= (float)(cp[0]/255.0);
				but_copypaste_rgb[1]= (float)(cp[1]/255.0);
				but_copypaste_rgb[2]= (float)(cp[2]/255.0);
			}
			
		}
		else {
			if(but->pointype==FLO) {
				float *fp= (float *) poin;
				fp[0] = but_copypaste_rgb[0];
				fp[1] = but_copypaste_rgb[1];
				fp[2] = but_copypaste_rgb[2];
				return 1;
			}
			else if (but->pointype==CHA) {
				char *cp= (char *) poin;
				cp[0] = (char)(but_copypaste_rgb[0]*255.0);
				cp[1] = (char)(but_copypaste_rgb[1]*255.0);
				cp[2] = (char)(but_copypaste_rgb[2]*255.0);
				
				return 1;
			}
			
		}
	}
	else if(but->type==TEX) {
		if(poin==NULL);
		else if(mode=='c') {
			strncpy(but_copypaste_str, but->poin, but->max);
		}
		else {
			char backstr[UI_MAX_DRAW_STR];
			/* give butfunc the original text too */
			/* feature used for bone renaming, channels, etc */
			if(but->func_arg2==NULL) {
				strncpy(backstr, but->drawstr, UI_MAX_DRAW_STR);
				but->func_arg2= backstr;
			}
			strncpy(but->poin, but_copypaste_str, but->max);
			uibut_do_func(but);
			ui_check_but(but);
			return 1;
		}
	}
	else if(but->type==IDPOIN) {
		if(mode=='c') {
			ID *id= *but->idpoin_idpp;
			if(id) strncpy(but_copypaste_str, id->name+2, 22);
		}
		else {
			but->idpoin_func(but_copypaste_str, but->idpoin_idpp);
			ui_check_but(but);
			return 1;
		}
	}
	else if(but->type==BUT_COLORBAND) {
		if(mode=='c') {
			if (!but->poin) {
				return 0;
			}
			memcpy( &but_copypaste_coba, but->poin, sizeof(ColorBand) );
		} else {
			if (but_copypaste_coba.tot==0) {
				return 0;
			}
			if (!but->poin) {
				but->poin= MEM_callocN( sizeof(ColorBand), "colorband");
			}
			memcpy( but->poin, &but_copypaste_coba, sizeof(ColorBand) );
			return 1;
		}
	}
	
	return 0;
}

/* ******************* block calc ************************* */

/* only for pulldowns */
void uiTextBoundsBlock(uiBlock *block, int addval)
{
	uiBut *bt;
	int i = 0, j, x1addval= 0, nextcol;
	
	bt= block->buttons.first;
	while(bt) {
		if(bt->type!=SEPR) {
			int transopts= (U.transopts & USER_TR_BUTTONS);
			if(bt->type==TEX || bt->type==IDPOIN) transopts= 0;
			j= BIF_GetStringWidth(bt->font, bt->drawstr, transopts);

			if(j > i) i = j;
		}
		bt= bt->next;
	}

	/* cope with multi collumns */
	bt= block->buttons.first;
	while(bt) {
		if(bt->next && bt->x1 < bt->next->x1)
			nextcol= 1;
		else nextcol= 0;
		
		bt->x1 = x1addval;
		bt->x2 = bt->x1 + i + addval;
		
		ui_check_but(bt);	// clips text again
		
		if(nextcol)
			x1addval+= i + addval;
		
		bt= bt->next;
	}
}


void uiBoundsBlock(uiBlock *block, int addval)
{
	uiBut *bt;
	int xof;
	
	if(block->buttons.first==NULL) {
		if(block->panel) {
			block->minx= 0.0; block->maxx= block->panel->sizex;
			block->miny= 0.0; block->maxy= block->panel->sizey;
		}
	}
	else {
	
		block->minx= block->miny= 10000;
		block->maxx= block->maxy= -10000;
		
		bt= block->buttons.first;
		while(bt) {
			if(bt->x1 < block->minx) block->minx= bt->x1;
			if(bt->y1 < block->miny) block->miny= bt->y1;
	
  			if(bt->x2 > block->maxx) block->maxx= bt->x2;
			if(bt->y2 > block->maxy) block->maxy= bt->y2;
			
			bt= bt->next;
		}
		
		block->minx -= addval;
		block->miny -= addval;
		block->maxx += addval;
		block->maxy += addval;
	}

	/* hardcoded exception... but that one is annoying with larger safety */ 
	bt= block->buttons.first;
	if(bt && strncmp(bt->str, "ERROR", 5)==0) xof= 10;
	else xof= 40;
	
	block->safety.xmin= block->minx-xof;
	block->safety.ymin= block->miny-xof;
	block->safety.xmax= block->maxx+xof;
	block->safety.ymax= block->maxy+xof;
}

static void ui_positionblock(uiBlock *block, uiBut *but)
{
	/* position block relative to but */
	uiBut *bt;
	rctf butrct;
	float aspect;
	int xsize, ysize, xof=0, yof=0, center;
	short dir1= 0, dir2=0;
	
	/* first transform to screen coords, assuming matrix is stil OK */
	/* the UIwinmat is in panelspace */

	butrct.xmin= but->x1; butrct.xmax= but->x2;
	butrct.ymin= but->y1; butrct.ymax= but->y2;

	ui_graphics_to_window(block->win, &butrct.xmin, &butrct.ymin);
	ui_graphics_to_window(block->win, &butrct.xmax, &butrct.ymax);
	block->parentrct= butrct;	// will use that for pulldowns later

	/* calc block rect */
	if(block->buttons.first) {
		block->minx= block->miny= 10000;
		block->maxx= block->maxy= -10000;
		
		bt= block->buttons.first;
		while(bt) {
			if(bt->x1 < block->minx) block->minx= bt->x1;
			if(bt->y1 < block->miny) block->miny= bt->y1;

			if(bt->x2 > block->maxx) block->maxx= bt->x2;
			if(bt->y2 > block->maxy) block->maxy= bt->y2;
			
			bt= bt->next;
		}
	}
	else {
		/* we're nice and allow empty blocks too */
		block->minx= block->miny= 0;
		block->maxx= block->maxy= 20;
	}
	
	aspect= (float)(block->maxx - block->minx + 4);
	ui_graphics_to_window(block->win, &block->minx, &block->miny);
	ui_graphics_to_window(block->win, &block->maxx, &block->maxy);

	//block->minx-= 2.0; block->miny-= 2.0;
	//block->maxx+= 2.0; block->maxy+= 2.0;
	
	xsize= block->maxx - block->minx+4; // 4 for shadow
	ysize= block->maxy - block->miny+4;
	aspect/= (float)xsize;

	if(but) {
		short left=0, right=0, top=0, down=0;

		if(block->direction & UI_CENTER) center= ysize/2;
		else center= 0;

		if( butrct.xmin-xsize > 0.0) left= 1;
		if( butrct.xmax+xsize < G.curscreen->sizex) right= 1;
		if( butrct.ymin-ysize+center > 0.0) down= 1;
		if( butrct.ymax+ysize-center < G.curscreen->sizey) top= 1;
		
		dir1= block->direction & UI_DIRECTION;

		/* secundary directions */
		if(dir1 & (UI_TOP|UI_DOWN)) {
			if(dir1 & UI_LEFT) dir2= UI_LEFT;
			else if(dir1 & UI_RIGHT) dir2= UI_RIGHT;
			dir1 &= (UI_TOP|UI_DOWN);
		}

		if(dir2==0) if(dir1==UI_LEFT || dir1==UI_RIGHT) dir2= UI_DOWN;
		if(dir2==0) if(dir1==UI_TOP || dir1==UI_DOWN) dir2= UI_LEFT;
		
		/* no space at all? dont change */
		if(left || right) {
			if(dir1==UI_LEFT && left==0) dir1= UI_RIGHT;
			if(dir1==UI_RIGHT && right==0) dir1= UI_LEFT;
			/* this is aligning, not append! */
			if(dir2==UI_LEFT && right==0) dir2= UI_RIGHT;
			if(dir2==UI_RIGHT && left==0) dir2= UI_LEFT;
		}
		if(down || top) {
			if(dir1==UI_TOP && top==0) dir1= UI_DOWN;
			if(dir1==UI_DOWN && down==0) dir1= UI_TOP;
			if(dir2==UI_TOP && top==0) dir2= UI_DOWN;
			if(dir2==UI_DOWN && down==0) dir2= UI_TOP;
		}
		
		if(dir1==UI_LEFT) {
			xof= butrct.xmin - block->maxx;
			if(dir2==UI_TOP) yof= butrct.ymin - block->miny-center;
			else yof= butrct.ymax - block->maxy+center;
		}
		else if(dir1==UI_RIGHT) {
			xof= butrct.xmax - block->minx;
			if(dir2==UI_TOP) yof= butrct.ymin - block->miny-center;
			else yof= butrct.ymax - block->maxy+center;
		}
		else if(dir1==UI_TOP) {
			yof= butrct.ymax - block->miny;
			if(dir2==UI_RIGHT) xof= butrct.xmax - block->maxx;
			else xof= butrct.xmin - block->minx;
			// changed direction? 
			if((dir1 & block->direction)==0) {
				if(block->direction & UI_SHIFT_FLIPPED)
					xof+= dir2==UI_LEFT?25:-25;
				uiBlockFlipOrder(block);
			}
		}
		else if(dir1==UI_DOWN) {
			yof= butrct.ymin - block->maxy;
			if(dir2==UI_RIGHT) xof= butrct.xmax - block->maxx;
			else xof= butrct.xmin - block->minx;
			// changed direction?
			if((dir1 & block->direction)==0) {
				if(block->direction & UI_SHIFT_FLIPPED)
					xof+= dir2==UI_LEFT?25:-25;
				uiBlockFlipOrder(block);
			}
		}

		/* and now we handle the exception; no space below or to top */
		if(top==0 && down==0) {
			if(dir1==UI_LEFT || dir1==UI_RIGHT) {
				// align with bottom of screen 
				yof= ysize;
			}
		}
		
		/* or no space left or right */
		if(left==0 && right==0) {
			if(dir1==UI_TOP || dir1==UI_DOWN) {
				// align with left size of screen 
				xof= -block->minx+5;
			}
		}
		
		// apply requested offset in the block
		xof += block->xofs/block->aspect;
		yof += block->yofs/block->aspect;
		
	}
	
	/* apply */
	
	for(bt= block->buttons.first; bt; bt= bt->next) {
		
		ui_graphics_to_window(block->win, &bt->x1, &bt->y1);
		ui_graphics_to_window(block->win, &bt->x2, &bt->y2);

		bt->x1 += xof;
		bt->x2 += xof;
		bt->y1 += yof;
		bt->y2 += yof;

		bt->aspect= 1.0;
		// ui_check_but recalculates drawstring size in pixels
		ui_check_but(bt);
	}
	
	block->minx += xof;
	block->miny += yof;
	block->maxx += xof;
	block->maxy += yof;
	
	/* safety calculus */
	if(but) {
		float midx= (block->parentrct.xmin+block->parentrct.xmax)/2.0;
		float midy= (block->parentrct.ymin+block->parentrct.ymax)/2.0;
		
		/* when you are outside parent button, safety there should be smaller */
		
		// parent button to left
		if( midx < block->minx ) block->safety.xmin= block->minx-3; 
		else block->safety.xmin= block->minx-40;
		// parent button to right
		if( midx > block->maxx ) block->safety.xmax= block->maxx+3; 
		else block->safety.xmax= block->maxx+40;
		
		// parent button on bottom
		if( midy < block->miny ) block->safety.ymin= block->miny-3; 
		else block->safety.ymin= block->miny-40;
		// parent button on top
		if( midy > block->maxy ) block->safety.ymax= block->maxy+3; 
		else block->safety.ymax= block->maxy+40;
		
		// exception for switched pulldowns...
		if(dir1 && (dir1 & block->direction)==0) {
			if(dir2==UI_RIGHT) block->safety.xmax= block->maxx+3; 
			if(dir2==UI_LEFT) block->safety.xmin= block->minx-3; 
		}
		block->direction= dir1;
	}
	else {
		block->safety.xmin= block->minx-40;
		block->safety.ymin= block->miny-40;
		block->safety.xmax= block->maxx+40;
		block->safety.ymax= block->maxy+40;
	}

}


void ui_autofill(uiBlock *block)
{
	uiBut *but;
	float *maxw, *maxh, startx = 0, starty, height = 0;
	float totmaxh;
	int rows=0, /*  cols=0, */ i, lasti;
	
	/* first count rows */
	but= block->buttons.last;
	rows= but->x1+1;

	/* calculate max width / height for each row */
	maxw= MEM_callocN(sizeof(float)*rows, "maxw");
	maxh= MEM_callocN(sizeof(float)*rows, "maxh");
	but= block->buttons.first;
	while(but) {
		i= but->x1;
		if( maxh[i] < but->y2) maxh[i]= but->y2;
		maxw[i] += but->x2;
		but= but->next;
	}
	
	totmaxh= 0.0;
	for(i=0; i<rows; i++) totmaxh+= maxh[i];
	
	/* apply widths/heights */
	starty= block->maxy;
	but= block->buttons.first;
	lasti= -1;
	while(but) {
		// signal for aligning code
		but->flag |= UI_BUT_ALIGN_DOWN;
		
		i= but->x1;

		if(i!=lasti) {
			startx= block->minx;
			height= (maxh[i]*(block->maxy-block->miny))/totmaxh;
			starty-= height;
			lasti= i;
		}
		
		but->y1= starty+but->aspect;
		but->y2= but->y1+height-but->aspect;
		
		but->x2= (but->x2*(block->maxx-block->minx))/maxw[i];
		but->x1= startx+but->aspect;
		
		startx+= but->x2;
		but->x2+= but->x1-but->aspect;
		
		ui_check_but(but);
		
		but= but->next;
	}
	
	uiBlockEndAlign(block);
	
	MEM_freeN(maxw); MEM_freeN(maxh);	
	block->autofill= 0;
}

/* ************** LINK LINE DRAWING  ************* */

/* link line drawing is not part of buttons or theme.. so we stick with it here */

static void ui_draw_linkline(uiBut *but, uiLinkLine *line)
{
	float vec1[2], vec2[2];

	if(line->from==NULL || line->to==NULL) return;
	
	vec1[0]= (line->from->x1+line->from->x2)/2.0;
	vec1[1]= (line->from->y1+line->from->y2)/2.0;
	vec2[0]= (line->to->x1+line->to->x2)/2.0;
	vec2[1]= (line->to->y1+line->to->y2)/2.0;
	
	if(line->flag & UI_SELECT) BIF_ThemeColorShade(but->themecol, 80);
	else glColor3ub(0,0,0);
	fdrawline(vec1[0], vec1[1], vec2[0], vec2[1]);
}

static void ui_draw_links(uiBlock *block)
{
	uiBut *but;
	uiLinkLine *line;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==LINK && but->link) {
			line= but->link->lines.first;
			while(line) {
				ui_draw_linkline(but, line);
				line= line->next;
			}
		}
		but= but->next;
	}	
}

/* ************** BLOCK DRAWING FUNCTION ************* */


void uiDrawBlock(uiBlock *block)
{
	uiBut *but;
	short testmouse=0, mouse[2];
	
	/* we set this only once */
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	/* handle pending stuff */
	if(block->autofill) ui_autofill(block);
	if(block->minx==0.0 && block->maxx==0.0) uiBoundsBlock(block, 0);
	if(block->flag & UI_BUT_ALIGN) uiBlockEndAlign(block);
	
	/* we set active flag on a redraw again */
	if((block->flag & UI_BLOCK_LOOP)==0) {
		testmouse= 1;  
		Mat4CpyMat4(UIwinmat, block->winmat);
	}
	
	uiPanelPush(block); // panel matrix
	
	if(block->flag & UI_BLOCK_LOOP) {
		uiDrawMenuBox(block->minx, block->miny, block->maxx, block->maxy, block->flag);
	}
	else {
		if(block->panel) ui_draw_panel(block);
	}		

	if(block->drawextra) block->drawextra(curarea, block);
	
	if(testmouse)	/* do it after panel push, otherwise coords are wrong */
		uiGetMouse(block->win, mouse);

	for (but= block->buttons.first; but; but= but->next) {
		
		if(testmouse && uibut_contains_pt(but, mouse))
			but->flag |= UI_ACTIVE;
		
		ui_draw_but(but);
	}

	ui_draw_links(block);

	uiPanelPop(block); // matrix restored
}

/* ************* MENUBUTS *********** */

typedef struct {
	char *str;
	int retval;
	int icon;
} MenuEntry;

typedef struct {
	char *instr;
	char *title;
	int titleicon;
	
	MenuEntry *items;
	int nitems, itemssize;
} MenuData;

static MenuData *menudata_new(char *instr) {
	MenuData *md= MEM_mallocN(sizeof(*md), "MenuData");

	md->instr= instr;
	md->title= NULL;
	md->titleicon= 0;
	md->items= NULL;
	md->nitems= md->itemssize= 0;
	
	return md;
}

static void menudata_set_title(MenuData *md, char *title, int titleicon) {
	if (!md->title)
		md->title= title;
	if (!md->titleicon)
		md->titleicon= titleicon;
}

static void menudata_add_item(MenuData *md, char *str, int retval, int icon) {
	if (md->nitems==md->itemssize) {
		int nsize= md->itemssize?(md->itemssize<<1):1;
		MenuEntry *oitems= md->items;
		
		md->items= MEM_mallocN(nsize*sizeof(*md->items), "md->items");
		if (oitems) {
			memcpy(md->items, oitems, md->nitems*sizeof(*md->items));
			MEM_freeN(oitems);
		}
		
		md->itemssize= nsize;
	}
	
	md->items[md->nitems].str= str;
	md->items[md->nitems].retval= retval;
	md->items[md->nitems].icon= icon;
	md->nitems++;
}

static void menudata_free(MenuData *md) {
	MEM_freeN(md->instr);
	if (md->items)
		MEM_freeN(md->items);
	MEM_freeN(md);
}

	/**
	 * Parse menu description strings, string is of the
	 * form "[sss%t|]{(sss[%xNN]|), (%l|)}", ssss%t indicates the
	 * menu title, sss or sss%xNN indicates an option, 
	 * if %xNN is given then NN is the return value if
	 * that option is selected otherwise the return value
	 * is the index of the option (starting with 1). %l
	 * indicates a seperator.
	 * 
	 * @param str String to be parsed.
	 * @retval new menudata structure, free with menudata_free()
	 */
static MenuData *decompose_menu_string(char *str) 
{
	char *instr= BLI_strdup(str);
	MenuData *md= menudata_new(instr);
	char *nitem= NULL, *s= instr;
	int nicon=0, nretval= 1, nitem_is_title= 0;
	
	while (1) {
		char c= *s;

		if (c=='%') {
			if (s[1]=='x') {
				nretval= atoi(s+2);

				*s= '\0';
				s++;
			} else if (s[1]=='t') {
				nitem_is_title= 1;

				*s= '\0';
				s++;
			} else if (s[1]=='l') {
				nitem= "%l";
				s++;
			} else if (s[1]=='i') {
				nicon= atoi(s+2);
				
				*s= '\0';
				s++;
			}
		} else if (c=='|' || c=='\0') {
			if (nitem) {
				*s= '\0';

				if (nitem_is_title) {
					menudata_set_title(md, nitem, nicon);
					nitem_is_title= 0;
				} else {
					/* prevent separator to get a value */
					if(nitem[0]=='%' && nitem[1]=='l')
						menudata_add_item(md, nitem, -1, nicon);
					else
						menudata_add_item(md, nitem, nretval, nicon);
					nretval= md->nitems+1;
				} 
				
				nitem= NULL;
				nicon= 0;
			}
			
			if (c=='\0')
				break;
		} else if (!nitem)
			nitem= s;
		
		s++;
	}
	
	return md;
}

static void ui_set_name_menu(uiBut *but, int value)
{
	MenuData *md;
	int i;
	
	md= decompose_menu_string(but->str);
	for (i=0; i<md->nitems; i++)
		if (md->items[i].retval==value)
			strcpy(but->drawstr, md->items[i].str);
	
	menudata_free(md);
}

static void ui_warp_pointer(short x, short y)
{
	/* OSX has very poor mousewarp support, it sends events;
	   this causes a menu being pressed immediately ... */
	#ifndef __APPLE__
	warp_pointer(x, y);
	#endif
}

#define TBOXH 20
static int ui_do_but_MENU(uiBut *but)
{
	uiBlock *block;
	uiBut *bt;
	ListBase listb={NULL, NULL}, lb;
	double fvalue;
	int width, height=0, a, xmax, starty;
	short startx;
	int columns=1, rows=0, boxh, event;
	short  x1, y1, active= -1;
	short mval[2];
	MenuData *md;

	but->flag |= UI_SELECT;
	ui_draw_but(but);
	ui_block_flush_back(but->block);	// flush because this button creates own blocks loop

	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, but->win);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;
	block->themecol= TH_MENU_ITEM;
	
	md= decompose_menu_string(but->str);

	/* columns and row calculation */
	columns= (md->nitems+20)/20;
	if (columns<1) columns= 1;
	
	if(columns>8) columns= (md->nitems+25)/25;
	
	rows= (int) md->nitems/columns;
	if (rows<1) rows= 1;
	
	while (rows*columns<md->nitems) rows++;
		
	/* prevent scaling up of pupmenu */
	if (but->aspect < 1.0f) but->aspect = 1.0f;

	/* size and location */
	if(md->title)
		width= 1.5*but->aspect*strlen(md->title)+BIF_GetStringWidth(block->curfont, md->title, (U.transopts & USER_TR_MENUS));
	else
		width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= but->aspect*BIF_GetStringWidth(block->curfont, md->items[a].str, (U.transopts & USER_TR_MENUS));
		if ( md->items[a].icon) xmax += 20*but->aspect;
		if(xmax>width) width= xmax;
	}

	width+= 10;
	if (width < (but->x2 - but->x1)) width = (but->x2 - but->x1);
	if (width<50) width=50;

	boxh= TBOXH;
	
	height= rows*boxh;
	if (md->title) height+= boxh;
	
	getmouseco_sc(mval);
	
	/* find active item */
	fvalue= ui_get_but_val(but);
	for(active=0; active<md->nitems; active++) {
		if( md->items[active].retval== (int)fvalue ) break;
	}
	/* no active item? */
	if(active==md->nitems) {
		if(md->title) active= -1;
		else active= 0;
	}

	/* for now disabled... works confusing because you think it's a title or so.... */
	active= -1;

	/* here we go! */
	startx= but->x1;
	starty= but->y1;
	
	if(md->title) {
		uiBut *bt;
		uiSetCurFont(block, block->font+1);
		if (md->titleicon) {
			uiDefIconTextBut(block, LABEL, 0, md->titleicon, md->title, startx, (short)(starty+rows*boxh), (short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
		} else {
			bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+rows*boxh), (short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
			bt->flag= UI_TEXT_LEFT;
		}
		uiSetCurFont(block, block->font);
		
	}

	for(a=0; a<md->nitems; a++) {
		
		x1= but->x1 + width*((int)(md->nitems-a-1)/rows);
		y1= but->y1 - boxh*(rows - ((md->nitems - a - 1)%rows)) + (rows*boxh);

		if (strcmp(md->items[md->nitems-a-1].str, "%l")==0) {
			uiDefBut(block, SEPR, B_NOP, "", x1, y1,(short)(width-(rows>1)), (short)(boxh-1), NULL, 0.0, 0.0, 0, 0, "");
		}
		else if(md->items[md->nitems-a-1].icon) {
			uiBut *bt= uiDefIconTextBut(block, BUTM|but->pointype, but->retval, md->items[md->nitems-a-1].icon ,md->items[md->nitems-a-1].str, x1, y1,(short)(width-(rows>1)), (short)(boxh-1), but->poin, (float) md->items[md->nitems-a-1].retval, 0.0, 0, 0, "");
			if(active==a) bt->flag |= UI_ACTIVE;			
		}
		else {
			uiBut *bt= uiDefBut(block, BUTM|but->pointype, but->retval, md->items[md->nitems-a-1].str, x1, y1,(short)(width-(rows>1)), (short)(boxh-1), but->poin, (float) md->items[md->nitems-a-1].retval, 0.0, 0, 0, "");
			if(active==a) bt->flag |= UI_ACTIVE;
		}
	}
	
	/* the code up here has flipped locations, because of change of preferred order */
	/* thats why we have to switch list order too, to make arrowkeys work */
	
	lb.first= lb.last= NULL;
	bt= block->buttons.first;
	while(bt) {
		uiBut *next= bt->next;
		BLI_remlink(&block->buttons, bt);
		BLI_addhead(&lb, bt);
		bt= next;
	}
	block->buttons= lb;

	/* and lets go */
	block->direction= UI_TOP;
	ui_positionblock(block, but);
	
	/* blocks can come (and get scaled) from a normal window, now we go to screenspace */
	block->win= G.curscreen->mainwin;
	for(bt= block->buttons.first; bt; bt= bt->next) bt->win= block->win;
	bwin_getsinglematrix(block->win, block->winmat);

	event= uiDoBlocks(&listb, 0, 1);
	
	menudata_free(md);
	
	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	/* no draw of button now, for floating panels the matrix now is invalid...
	   the button still is active, and will be redrawn in main loop to de-activate it */
	/* but, if no hilites, we send redraw to queue */
	if(but->flag & UI_NO_HILITE)
		addqueue(but->block->winq, REDRAW, 1);
	
	uibut_do_func(but);
	
	/* return no existing event, because the menu sends events instead */
	return -1;
}

/* ********************** NEXT/PREV for arrowkeys etc ************** */

static uiBut *ui_but_prev(uiBut *but)
{
	while(but->prev) {
		but= but->prev;
		if(but->type!=LABEL && but->type!=SEPR && but->type!=ROUNDBOX) return but;
	}
	return NULL;
}

static uiBut *ui_but_next(uiBut *but)
{
	while(but->next) {
		but= but->next;
		if(but->type!=LABEL && but->type!=SEPR && but->type!=ROUNDBOX) return but;
	}
	return NULL;
}

static uiBut *ui_but_first(uiBlock *block)
{
	uiBut *but;
	
	but= block->buttons.first;
	while(but) {
		if(but->type!=LABEL && but->type!=SEPR && but->type!=ROUNDBOX) return but;
		but= but->next;
	}
	return NULL;
}

static uiBut *ui_but_last(uiBlock *block)
{
	uiBut *but;
	
	but= block->buttons.last;
	while(but) {
		if(but->type!=LABEL && but->type!=SEPR && but->type!=ROUNDBOX) return but;
		but= but->prev;
	}
	return NULL;
}


/* ************* IN-BUTTON TEXT SELECTION/EDITING ************* */

static short ui_delete_selection_edittext(uiBut *but)
{
	int x;
	short deletedwidth=0;
	char *str;
	
	str= (char *)but->poin;
	
	deletedwidth = (but->selend - but->selsta);
	
	for(x=0; x< strlen(str); x++) {
		if (but->selend + x <= strlen(str) ) {
			str[but->selsta + x]= str[but->selend + x];
		} else {
			str[but->selsta + x]= '\0';
			break;
		}
	}
	but->pos = but->selend = but->selsta;
	
	return deletedwidth;
}

static void ui_set_cursor_pos_edittext(uiBut *but, short sx)
{
	char backstr[UI_MAX_DRAW_STR];
	
	BLI_strncpy(backstr, but->drawstr, UI_MAX_DRAW_STR);
	but->pos= strlen(backstr)-but->ofs;
	
	while((but->aspect*BIF_GetStringWidth(but->font, backstr+but->ofs, 0) + but->x1) > sx) {
		if (but->pos <= 0) break;
		but->pos--;
		backstr[but->pos+but->ofs] = 0;
	}
	
	but->pos -= strlen(but->str);
	but->pos += but->ofs;
	if(but->pos<0) but->pos= 0;
}


/* ************* EVENTS ************* */

void uiGetMouse(int win, short *adr)
{
	int x, y;
	float xwin, ywin;
	
	getmouseco_sc(adr);
	if (win == G.curscreen->mainwin) return;
	
	bwin_getsuborigin(win, &x, &y);

	adr[0]-= x;
	adr[1]-= y;

	xwin= adr[0];
	ywin= adr[1];

	ui_window_to_graphics(win, &xwin, &ywin);

	adr[0]= (short)(xwin+0.5);
	adr[1]= (short)(ywin+0.5);
}

static void ui_is_but_sel(uiBut *but)
{
	double value;
	int lvalue;
	short push=0, true=1;

	value= ui_get_but_val(but);

	if( but->type==TOGN  || but->type==ICONTOGN) true= 0;

	if( but->bit ) {
		lvalue= (int)value;
		if( BTST(lvalue, (but->bitnr)) ) push= true;
		else push= !true;
	}
	else {
		switch(but->type) {
		case BUT:
			push= 0;
			break;
		case KEYEVT:
			if (value==-1) push= 1;
			break;
		case TOG:
		case TOGR:
		case TOG3:
		case BUT_TOGDUAL:
		case ICONTOG:
			if(value!=but->min) push= 1;
			break;
		case ICONTOGN:
		case TOGN:
			if(value==0.0) push= 1;
			break;
		case ROW:
			if(value == but->max) push= 1;
			break;
		case COL:
			push= 1;
			break;
		default:
			push= 2;
			break;
		}
	}
	
	if(push==2);
	else if(push==1) but->flag |= UI_SELECT;
	else but->flag &= ~UI_SELECT;
}

static int ui_do_but_BUT(uiBut *but)
{
	int activated;
	
	do {
		int oflag= but->flag;
		short mval[2];
			
		uiGetMouse(mywinget(), mval);

		if (uibut_contains_pt(but, mval))
			but->flag |= UI_SELECT;
		else
			but->flag &= ~UI_SELECT;

		if (but->flag != oflag) {
			ui_draw_but(but);
			ui_block_flush_back(but->block);
		}
		
		PIL_sleep_ms(10);
	} while (get_mbut() & L_MOUSE);

	activated= (but->flag & UI_SELECT);

	if(activated) {
		UIafterfunc_but= but->func;
		UIafterfunc_arg1= but->func_arg1;
		UIafterfunc_arg2= but->func_arg2;
		/* no more uibut_do_func(but); this button calls fileselecting windows */
	}
	
	but->flag &= ~UI_SELECT;
	ui_draw_but(but);

	return activated?but->retval:0;
}

static int ui_do_but_KEYEVT(uiBut *but)
{
	unsigned short event= 0;
	short val;

		/* flag for ui_check_but */
	ui_set_but_val(but, -1);
	ui_check_but(but);
	ui_draw_but(but);
	ui_block_flush_back(but->block);

	do {
		event= extern_qread(&val);
	} while (!event || !val || ELEM(event, MOUSEX, MOUSEY));

	if (!key_event_to_string(event)[0]) event= 0;

	ui_set_but_val(but, (double) event);
	ui_check_but(but);
	ui_draw_but(but);
	
	return (event!=0);
}

static int ui_do_but_TOG(uiBlock *block, uiBut *but, int qual)
{
	uiBut *bt;
	double value;
	int w, lvalue, push;
	
	/* local hack... */
	if(but->type==BUT_TOGDUAL && qual==LR_CTRLKEY) {
		if(but->pointype==SHO)
			but->poin += 2;
		else if(but->pointype==INT)
			but->poin += 4;
	}
	
	value= ui_get_but_val(but);
	lvalue= (int)value;
	
	if(but->bit) {
		w= BTST(lvalue, but->bitnr);
		if(w) lvalue = BCLR(lvalue, but->bitnr);
		else lvalue = BSET(lvalue, but->bitnr);
		
		if(but->type==TOGR) {
			if( (get_qual() & LR_SHIFTKEY)==0 ) {
				lvalue= 1<<(but->bitnr);
	
				ui_set_but_val(but, (double)lvalue);

				bt= block->buttons.first;
				while(bt) {
					if( bt!=but && bt->poin==but->poin ) {
						ui_is_but_sel(bt);
						ui_draw_but(bt);
					}
					bt= bt->next;
				}
			}
			else {
				if(lvalue==0) lvalue= 1<<(but->bitnr);
			}
		}
		
		ui_set_but_val(but, (double)lvalue);
		if(but->type==ICONTOG || but->type==ICONTOGN) ui_check_but(but);
		// no frontbuffer draw for this one
		if(but->type==BUT_TOGDUAL);
		else if((but->flag & UI_NO_HILITE)==0) ui_draw_but(but);
	}
	else {
		
		if(value==0.0) push= 1; 
		else push= 0;
		
		if(but->type==TOGN || but->type==ICONTOGN) push= !push;
		ui_set_but_val(but, (double)push);
		if(but->type==ICONTOG || but->type==ICONTOGN) ui_check_but(but);		
		// no frontbuffer draw for this one
		if((but->flag & UI_NO_HILITE)==0) ui_draw_but(but);
	}
	
	/* end local hack... */
	if(but->type==BUT_TOGDUAL && qual==LR_CTRLKEY) {
		if(but->pointype==SHO)
			but->poin -= 2;
		else if(but->pointype==INT)
			but->poin -= 4;
	}
	
	/* no while loop...this button is used for viewmove */

	uibut_do_func(but);

	return but->retval;
}

static int ui_do_but_ROW(uiBlock *block, uiBut *but)
{
	uiBut *bt;
	
	ui_set_but_val(but, but->max);
	ui_draw_but(but);

	bt= block->buttons.first;
	while(bt) {
		if( bt!=but && bt->type==ROW ) {
			if(bt->min==but->min) {
				ui_is_but_sel(bt);
				ui_draw_but(bt);
			}
		}
		bt= bt->next;
	}
	return but->retval;
}

/* return 1 if char ch is special character otherwise
 * it returns 0 */
static short test_special_char(char ch)
{
	switch(ch) {
		case '\\':
		case '/':
		case '~':
		case '!':
		case '@':
		case '#':
		case '$':
		case '%':
		case '^':
		case '&':
		case '*':
		case '(':
		case ')':
		case '+':
		case '=':
		case '{':
		case '}':
		case '[':
		case ']':
		case ':':
		case ';':
		case '\'':
		case '\"':
		case '<':
		case '>':
		case ',':
		case '.':
		case '?':
		case '_':
		case '-':
		case ' ':
			return 1;
			break;
		default:
			break;
	}
	return 0;
}

static int ui_do_but_TEX(uiBut *but)
{
	unsigned short dev;
	short x, y, mval[2], len=0, dodraw, selextend=0;
	char *str, backstr[UI_MAX_DRAW_STR];
	short capturing, sx, sy, prevx;
	
	str= (char *)but->poin;
	
	but->flag |= UI_SELECT;

	uiGetMouse(mywinget(), mval);

	/* set cursor pos to the end of the text */
	but->pos = strlen(str);
	but->selsta = 0;
	but->selend = strlen(but->drawstr) - strlen(but->str);

	/* backup */
	BLI_strncpy(backstr, but->poin, UI_MAX_DRAW_STR);

	ui_draw_but(but);
	ui_block_flush_back(but->block);
	
	while (get_mbut() & L_MOUSE) BIF_wait_for_statechange();
	len= strlen(str);

	but->min= 0.0;
	
	capturing = TRUE;
	while(capturing) {
		char ascii;
		short val;

		dodraw= 0;
		dev = extern_qread_ext(&val, &ascii);
		
		if(dev==INPUTCHANGE) break;
		else if(get_mbut() & R_MOUSE) break;
		else if(get_mbut() & L_MOUSE) {
			uiGetMouse(mywinget(), mval);
			sx = mval[0]; sy = mval[1];
			
			if ((but->y1 <= sy) && (sy <= but->y2) && (but->x1 <= sx) && (sx <= but->x2)) {
				ui_set_cursor_pos_edittext(but, mval[0]);
				
				but->selsta = but->selend = but->pos;
				
				/* drag text select */
				prevx= mval[0];
				while (get_mbut() & L_MOUSE) {
					uiGetMouse(mywinget(), mval);
					
					if(prevx!=mval[0]) {
						
						if (mval[0] > sx) selextend = EXTEND_RIGHT;
						else if (mval[0] < sx) selextend = EXTEND_LEFT;
						
						ui_set_cursor_pos_edittext(but, mval[0]);
						
						if (selextend == EXTEND_RIGHT) but->selend = but->pos;
						if (selextend == EXTEND_LEFT) but->selsta = but->pos;
						
						ui_check_but(but);
						ui_draw_but(but);
						ui_block_flush_back(but->block);
					}					
					PIL_sleep_ms(10);
				}
				dodraw= 1;
			} else break;
		}
		else if(dev==ESCKEY) break;
		else if(dev==MOUSEX) val= 0;
		else if(dev==MOUSEY) val= 0;
		
		/* cut, copy, paste selected text */
		/* mainqread discards ascii values < 32, so can't do this cleanly within the if(ascii) block*/
		else if ( (val) && 
			 ((G.qual & LR_COMMANDKEY) || (G.qual & LR_CTRLKEY)) && 
			 ((dev==XKEY) || (dev==CKEY) || (dev==VKEY)) ) {
				 
			
			/* paste */
			if (dev==VKEY) {
				/* paste over the current selection */
				if ((but->selend - but->selsta) > 0) {	
					len -= ui_delete_selection_edittext(but);
				}
				
				for (y=0; y<strlen(but_copypaste_str); y++)
				{
					/* add contents of buffer */
					if(len < but->max) {
						for(x= but->max; x>but->pos; x--)
							str[x]= str[x-1];
						str[but->pos]= but_copypaste_str[y];
						but->pos++; 
						len++;
						str[len]= '\0';
					}
				}
				if (strlen(but_copypaste_str) > 0) dodraw= 1;
			}
			/* cut & copy */
			else if ( (dev==XKEY) || (dev==CKEY) ) {
				/* copy the contents to the copypaste buffer */
				for(x= but->selsta; x <= but->selend; x++) {
					if (x==but->selend)
						but_copypaste_str[x] = '\0';
					else
						but_copypaste_str[(x - but->selsta)] = str[x];
				}
				
				/* for cut only, delete the selection afterwards */
				if (dev==XKEY) {
					if ((but->selend - but->selsta) > 0) {	
						len -= ui_delete_selection_edittext(but);
						
						if (len < 0) len = 0;
						dodraw=1;
					}
				}
			} 
		}
		else if((ascii)){
			
			if(len-(but->selend - but->selsta)+1 <= but->max) {
				
				/* type over the current selection */
				if ((but->selend - but->selsta) > 0) {	
					len -= ui_delete_selection_edittext(but);
				}

				if(len < but->max) {
					for(x= but->max; x>but->pos; x--)
						str[x]= str[x-1];
					str[but->pos]= ascii;
					but->pos++; 
					len++;
					str[len]= '\0';
					dodraw= 1;
				}
			}
		}
		else if(val) {
		
			switch (dev) {
				
			case RIGHTARROWKEY:
				/* if there's a selection */
				if ((but->selend - but->selsta) > 0) {
					/* extend the selection based on the first direction taken */
					if(G.qual & LR_SHIFTKEY) {
						if (!selextend) {
							selextend = EXTEND_RIGHT;
						}
						if (selextend == EXTEND_RIGHT) {
							but->selend++;
							if (but->selend > len) but->selend = len;
						} else if (selextend == EXTEND_LEFT) {
							but->selsta++;
							/* if the selection start has gone past the end,
							* flip them so they're in sync again */
							if (but->selsta == but->selend) {
								but->pos = but->selsta;
								selextend = EXTEND_RIGHT;
							}
						}
					} else {
						but->selsta = but->pos = but->selend;
						selextend = 0;
					}
				} else {
					if(G.qual & LR_SHIFTKEY) {
						/* make a selection, starting from the cursor position */
						but->selsta = but->pos;
						
						but->pos++;
						if(but->pos>strlen(str)) but->pos= strlen(str);
						
						but->selend = but->pos;
					} else if(G.qual & LR_CTRLKEY) {
						/* jump betweenn special characters (/,\,_,-, etc.),
						 * look at function test_special_char() for complete
						 * list of special character, ctr -> */
						while(but->pos < len) {
							but->pos++;
							if(test_special_char(str[but->pos])) break;
						}
					} else {
						but->pos++;
						if(but->pos>strlen(str)) but->pos= strlen(str);
					}
				}
				dodraw= 1;
				break;
				
			case LEFTARROWKEY:
				/* if there's a selection */
				if ((but->selend - but->selsta) > 0) {
					/* extend the selection based on the first direction taken */
					if(G.qual & LR_SHIFTKEY) {
						if (!selextend) {
							selextend = EXTEND_LEFT;
						}
						if (selextend == EXTEND_LEFT) {
							but->selsta--;
							if (but->selsta < 0) but->selsta = 0;
						} else if (selextend == EXTEND_RIGHT) {
							but->selend--;
							/* if the selection start has gone past the end,
							* flip them so they're in sync again */
							if (but->selsta == but->selend) {
								but->pos = but->selsta;
								selextend = EXTEND_LEFT;
							}
						}
					} else {
						but->pos = but->selend = but->selsta;
						selextend = 0;
					}
				} else {
					if(G.qual & LR_SHIFTKEY) {
						/* make a selection, starting from the cursor position */
						but->selend = but->pos;
						
						but->pos--;
						if(but->pos<0) but->pos= 0;
						
						but->selsta = but->pos;
					} else if(G.qual & LR_CTRLKEY) {
						/* jump betweenn special characters (/,\,_,-, etc.),
						 * look at function test_special_char() for complete
						 * list of special character, ctr -> */
						while(but->pos > 0){
							but->pos--;
							if(test_special_char(str[but->pos])) break;
						}
					} else {
						if(but->pos>0) but->pos--;
					}
				}
				dodraw= 1;
				break;

			case DOWNARROWKEY:
			case ENDKEY:
				if(G.qual & LR_SHIFTKEY) {
					but->selsta = but->pos;
					but->selend = strlen(str);
					selextend = EXTEND_RIGHT;
				} else {
					but->selsta = but->selend = but->pos= strlen(str);
				}
				dodraw= 1;
				break;
				
			case UPARROWKEY:
			case HOMEKEY:
				if(G.qual & LR_SHIFTKEY) {
					but->selend = but->pos;
					but->selsta = 0;
					selextend = EXTEND_LEFT;
				} else {
					but->selsta = but->selend = but->pos= 0;
				}
				dodraw= 1;
				break;
				
			case PADENTER:
			case RETKEY:
				capturing = FALSE;
				break;
				
			case DELKEY:
				if ((but->selend - but->selsta) > 0) {
					len -= ui_delete_selection_edittext(but);
					
					if (len < 0) len = 0;
					dodraw=1;
				}
				else if(but->pos>=0 && but->pos<strlen(str)) {
					for(x=but->pos; x<=strlen(str); x++)
						str[x]= str[x+1];
					str[--len]='\0';
					dodraw= 1;
				}
				break;

			case BACKSPACEKEY:
				if(len!=0) {
					if ((but->selend - but->selsta) > 0) {
						len -= ui_delete_selection_edittext(but);
						
						if (len < 0) len = 0;
						dodraw=1;
					}
					else if(get_qual() & LR_SHIFTKEY) {
						str[0]= 0;
						but->pos= 0;
						len= 0;
						dodraw= 1;
					}
					else if(but->pos>0) {
						for(x=but->pos; x<=strlen(str); x++)
							str[x-1]= str[x];
						but->pos--;
						str[--len]='\0';
						dodraw= 1;
					}
				} 
				break;
				
			case TABKEY:
				if(but->autocomplete_func) {
					but->autocomplete_func(str, but->autofunc_arg);
					but->pos= strlen(str);
					len= but->pos;
					dodraw= 1;
				}
				else capturing= FALSE;
				
				break;
			}
		}

		
		if(dodraw) {
			ui_check_but(but);
			ui_draw_but(but);
			ui_block_flush_back(but->block);
		}
	}
	
	if(dev==ESCKEY) strcpy(but->poin, backstr);
	but->pos= -1;
	but->flag &= ~UI_SELECT;

	if(dev!=ESCKEY) {
		/* give butfunc the original text too */
		/* feature used for bone renaming, channels, etc */
		if(but->func_arg2==NULL) but->func_arg2= backstr;
		uibut_do_func(but);
	}
	
	ui_check_but(but);
	ui_draw_but(but);
	
	if(dev==TABKEY) addqueue(but->win, G.qual?BUT_PREV:BUT_NEXT, 1);
	
	if(dev!=ESCKEY) return but->retval;
	else return B_NOP;	// prevent event to be passed on
}


static int ui_act_as_text_but(uiBut *but)
{
	void *but_func;
	double value;
	float min, max;
	int temp, retval, textleft;
	char str[UI_MAX_DRAW_STR], *point;
	
	/* this function is abused for tab-cycling */
	if(but->type==TEX)
		return ui_do_but_TEX(but);
	
	value= ui_get_but_val(but);
	if( but->pointype==FLO ) {
		if(but->a2) { /* amount of digits defined */
			if(but->a2==1) sprintf(str, "%.1f", value);
			else if(but->a2==2) sprintf(str, "%.2f", value);
			else if(but->a2==3) sprintf(str, "%.3f", value);
			else sprintf(str, "%.4f", value);
		}
		else sprintf(str, "%.3f", value);
	}
	else {
		sprintf(str, "%d", (int)value);
	}
	/* store values before calling as text button */
	point= but->poin;
	but->poin= str;
	but_func= but->func;
	but->func= NULL;
	min= but->min;
	max= but->max;
	but->min= 0.0;
	but->max= UI_MAX_DRAW_STR - 1; /* for py strings evaluation */
	temp= but->type;
	but->type= TEX;
	textleft= but->flag & UI_TEXT_LEFT;
	but->flag |= UI_TEXT_LEFT;
	ui_check_but(but);
	
	retval= ui_do_but_TEX(but);
	
	/* restore values */
	but->type= temp;
	but->poin= point;
	but->func= but_func;
	but->min= min;
	but->max= max;
	if(textleft==0) but->flag &= ~UI_TEXT_LEFT;

	if(BPY_button_eval(str, &value)) {
		/* Uncomment this if you want to see an error message (and annoy users) */
		/* error("Invalid Python expression, check console");*/
		value = 0.0f; /* Zero out value on error */
		
		if(str[0]) 
			retval = 0;  /* invalidate return value if eval failed, except when string was null */
	}
	
	if(but->pointype!=FLO) value= (int)value;
	
	if(but->type==NUMABS) value= fabs(value);
	if(value<min) value= min;
	if(value>max) value= max;

	ui_set_but_val(but, value);
	ui_check_but(but);
	ui_draw_but(but);
	
	return retval;
}

static int ui_do_but_NUM(uiBut *but)
{
	double value, butrange;
	float deler, fstart, f, tempf, pressure;
	int lvalue, temp, orig_x; /*  , firsttime=1; */
	short retval=0, qual, sx, mval[2], pos=0;

	but->flag |= UI_SELECT;
	ui_draw_but(but);
	ui_block_flush_back(but->block);
	
	uiGetMouse(mywinget(), mval);
	value= ui_get_but_val(but);
	
	sx= mval[0];
	orig_x = sx; /* Store so we can scale the rate of change by the dist the mouse is from its original xlocation */
	butrange= (but->max - but->min);
	fstart= (butrange == 0.0)? 0.0f: (value - but->min)/butrange;
	f= fstart;
	
	temp= (int)value;
	tempf= value;
	
	if(get_qual() & LR_SHIFTKEY) {	/* make it textbut */
		if( ui_act_as_text_but(but) ) retval= but->retval;
	}
	else {
		retval= but->retval;
		/* firsttime: this button can be approached with enter as well */
		
		/* drag-lock - prevent unwanted scroll adjustments */
		/* change last value (now 3) to adjust threshold in pixels */
		while (get_mbut() & L_MOUSE & ( abs(mval[0]-sx) <= 3) ) {
			uiGetMouse(mywinget(), mval);
		}
		sx = mval[0]; /* ignore mouse movement within drag-lock */

		while (get_mbut() & L_MOUSE) {
			qual= get_qual();
			pressure = get_pressure();
			
			uiGetMouse(mywinget(), mval);
			
			deler= 500;
			if( but->pointype!=FLO ) {

				if( (but->max-but->min)<100 ) deler= 200.0;
				if( (but->max-but->min)<25 ) deler= 50.0;
			}
			
			if(qual & LR_SHIFTKEY) deler*= 10.0;
			if(qual & LR_ALTKEY) deler*= 20.0;

			/* de-sensitise based on tablet pressure */
			if (ELEM(get_activedevice(), DEV_STYLUS, DEV_ERASER)) deler /= pressure;
						
			if(mval[0] != sx) {
				if( but->pointype==FLO && but->max-but->min > 11) {
					/* non linear change in mouse input- good for high precicsion */
					f+= (((float)(mval[0]-sx))/deler) * (fabs(orig_x-mval[0])*0.002);
				} else if ( but->pointype!=FLO && but->max-but->min > 129) { /* only scale large int buttons */
					/* non linear change in mouse input- good for high precicsionm ints need less fine tuning */
					f+= (((float)(mval[0]-sx))/deler) * (fabs(orig_x-mval[0])*0.004);
				} else {
					/*no scaling */
					f+= ((float)(mval[0]-sx))/deler ;
				}
				
				if(f>1.0) f= 1.0;
				if(f<0.0) f= 0.0;
				sx= mval[0];
				tempf= ( but->min + f*(but->max-but->min));
				
				if( but->pointype!=FLO ) {
					
					temp= floor(tempf+.5);
					
					if(tempf==but->min || tempf==but->max);
					else if(qual & LR_CTRLKEY) {
						if(qual & LR_SHIFTKEY) temp= 100*(temp/100);
						else temp= 10*(temp/10);
					}
					if( temp>=but->min && temp<=but->max) {
					
						value= ui_get_but_val(but);
						lvalue= (int)value;
						
						if(temp != lvalue ) {
							pos= 1;
							ui_set_but_val(but, (double)temp);
							ui_check_but(but);
							ui_draw_but(but);
							ui_block_flush_back(but->block);

							uibut_do_func(but);
						}
					}
	
				}
				else {
					temp= 0;
					if(qual & LR_CTRLKEY) {
						if(qual & LR_SHIFTKEY) {
							if(tempf==but->min || tempf==but->max);
							else if(but->max-but->min < 2.10) tempf= 0.01*floor(100.0*tempf);
							else if(but->max-but->min < 21.0) tempf= 0.1*floor(10.0*tempf);
							else tempf= floor(tempf);
						}
						else {
							if(tempf==but->min || tempf==but->max);
							else if(but->max-but->min < 2.10) tempf= 0.1*floor(10*tempf);
							else if(but->max-but->min < 21.0) tempf= floor(tempf);
							else tempf= 10.0*floor(tempf/10.0);
						}
					}
	
					if( tempf>=but->min && tempf<=but->max) {
						value= ui_get_but_val(but);
						
						if(tempf != value ) {
							pos= 1;
							ui_set_but_val(but, tempf);
							ui_check_but(but);
							ui_draw_but(but);
							ui_block_flush_back(but->block);
						}
					}
	
				}
			}
			BIF_wait_for_statechange();
		}
		
		/* click on the side arrows to increment/decrement, click inside
		* to edit the value directly */
		if(pos==0) {  /* plus 1 or minus 1 */
			if( but->pointype!=FLO ) {
	
				if(sx < (but->x1 + (but->x2 - but->x1)/3 - 3)) {
					temp--;
					if( temp>=but->min && temp<=but->max) ui_set_but_val(but, (double)temp);
				}
				else if(sx > (but->x1 + (2*(but->x2 - but->x1)/3) + 3)) {
					temp++;
					if( temp>=but->min && temp<=but->max) ui_set_but_val(but, (double)temp);
				}
				else {
					if( ui_act_as_text_but(but) ); else retval= 0;
				}
			}
			else {
			
				if(sx < (but->x1 + (but->x2 - but->x1)/3 - 3)) {
					tempf-= 0.01*but->a1;
					if (tempf < but->min) tempf = but->min;
					ui_set_but_val(but, tempf);
				}
				else if(sx > but->x1 + (2*((but->x2 - but->x1)/3) + 3)) {
					tempf+= 0.01*but->a1;
					if (tempf < but->min) tempf = but->min;
					ui_set_but_val(but, tempf);
				}
				else {
					if( ui_act_as_text_but(but) ); else retval= 0;
				}
			}
		}
	}

	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);	
	ui_block_flush_back(but->block);
	
	uibut_do_func(but);
	
	return retval;
}

static int ui_do_but_TOG3(uiBut *but)
{ 

	if( but->pointype==SHO ) {
		short *sp= (short *)but->poin;
		
		if( BTST(sp[1], but->bitnr)) {
			sp[1]= BCLR(sp[1], but->bitnr);
			sp[0]= BCLR(sp[0], but->bitnr);
		}
		else if( BTST(sp[0], but->bitnr)) {
			sp[1]= BSET(sp[1], but->bitnr);
		} else {
			sp[0]= BSET(sp[0], but->bitnr);
		}
	}
	else {
		if( BTST(*(but->poin+2), but->bitnr)) {
			*(but->poin+2)= BCLR(*(but->poin+2), but->bitnr);
			*(but->poin)= BCLR(*(but->poin), but->bitnr);
		}
		else if( BTST(*(but->poin), but->bitnr)) {
			*(but->poin+2)= BSET(*(but->poin+2), but->bitnr);
		} else {
			*(but->poin)= BSET(*(but->poin), but->bitnr);
		}
	}
	
	ui_is_but_sel(but);
	ui_draw_but(but);
	
	return but->retval;
}

static int ui_do_but_ICONROW(uiBut *but)
{
	ListBase listb= {NULL, NULL};
	uiBlock *block;
	int a;
	short event;
	
	but->flag |= UI_SELECT;
	ui_draw_but(but);
	ui_block_flush_back(but->block);	// flush because this button creates own blocks loop
	
	/* here we go! */
	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, but->win);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;
	block->themecol= TH_MENU_ITEM;
	
	for(a=(int)but->min; a<=(int)but->max; a++) {
		uiDefIconBut(block, BUTM|but->pointype, but->retval, but->icon+(a-but->min), 0, (short)(18*a), (short)(but->x2-but->x1-4), 18, but->poin, (float)a, 0.0, 0, 0, "");
	}
	block->direction= UI_TOP;	
	ui_positionblock(block, but);
	
	/* the block is made with but-win, but is handled in mainwin space...
	   this is needs better implementation */
	block->win= G.curscreen->mainwin;
	
	event= uiDoBlocks(&listb, 0, 1);

	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);	

	if (event & UI_RETURN_OK) {
		return but->retval;
	} else {
		return 0;
	}
}

static int ui_do_but_ICONTEXTROW(uiBut *but)
{
	uiBlock *block;
	ListBase listb={NULL, NULL};
	int width, a, xmax, ypos;
	MenuData *md;
	short event;
	but->flag |= UI_SELECT;
	ui_draw_but(but);
	ui_block_flush_back(but->block);	// flush because this button creates own blocks loop

	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, but->win);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;
	block->themecol= TH_MENU_ITEM;

	md= decompose_menu_string(but->str);

	/* size and location */
	/* expand menu width to fit labels */
	if(md->title)
		width= 2*strlen(md->title)+BIF_GetStringWidth(block->curfont, md->title, (U.transopts & USER_TR_MENUS));
	else
		width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= BIF_GetStringWidth(block->curfont, md->items[a].str, (U.transopts & USER_TR_MENUS));
		if(xmax>width) width= xmax;
	}

	width+= 30;
	if (width<50) width=50;

	ypos = 1;

	/* loop through the menu options and draw them out with icons & text labels */
	for(a=0; a<md->nitems; a++) {

		/* add a space if there's a separator (%l) */
	        if (strcmp(md->items[a].str, "%l")==0) {
			ypos +=3;
		}
		else {
			uiDefIconTextBut(block, BUTM|but->pointype, but->retval, (short)((but->icon)+(md->items[a].retval-but->min)), md->items[a].str, 0, ypos,(short)width, 19, but->poin, (float) md->items[a].retval, 0.0, 0, 0, "");
			ypos += 20;
		}
	}
	
	if(md->title) {
		uiBut *bt;
		uiSetCurFont(block, block->font+1);
		bt= uiDefBut(block, LABEL, 0, md->title, 0, ypos, (short)width, 19, NULL, 0.0, 0.0, 0, 0, "");
		uiSetCurFont(block, block->font);
		bt->flag= UI_TEXT_LEFT;
	}
	
	block->direction= UI_TOP;
	ui_positionblock(block, but);

	/* the block is made with but-win, but is handled in mainwin space...
	   this is needs better implementation */
	block->win= G.curscreen->mainwin;

	uiBoundsBlock(block, 3);

	event = uiDoBlocks(&listb, 0, 1);
	
	menudata_free(md);

	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);

	if (event & UI_RETURN_OK) {
		uibut_do_func(but);
		return but->retval;
	} else {
		return 0;
	}
}

static int ui_do_but_IDPOIN(uiBut *but)
{
	char str[UI_MAX_DRAW_STR];
	ID *id;
	
	id= *but->idpoin_idpp;
	if(id) strcpy(str, id->name+2);
	else str[0]= 0;
	
	but->type= TEX;
	but->poin= str;
	but->min= 0.0;
	but->max= 22.0;
	ui_check_but(but);
	ui_do_but_TEX(but);
	but->poin= NULL;
	but->type= IDPOIN;
	
	but->idpoin_func(str, but->idpoin_idpp);
	ui_check_but(but);
	ui_draw_but(but);
	
	return but->retval;
}

static int ui_do_but_SLI(uiBut *but)
{
	float f, fstart, tempf = 0.0, deler, value;
	int sx, h, temp, pos=0, lvalue, redraw;
	short mval[2], qual;

	value= ui_get_but_val(but);
	uiGetMouse(mywinget(), mval);

	sx= mval[0];
	h= but->y2-but->y1;
	fstart= but->max-but->min;
	fstart= (value - but->min)/fstart;
	temp= 32767;

	if( but->type==NUMSLI) deler= ( (but->x2-but->x1)/2 - 5.0*but->aspect);
	else if( but->type==HSVSLI) deler= ( (but->x2-but->x1)/2 - 5.0*but->aspect);
	else deler= (but->x2-but->x1- 5.0*but->aspect);
	

	while (get_mbut() & L_MOUSE) {
	
		qual= get_qual();
		uiGetMouse(mywinget(), mval);
		
		f= (float)(mval[0]-sx)/deler +fstart;
		
		if (qual & LR_SHIFTKEY) {
			f= (f-fstart)/10.0 + fstart;
		}

		CLAMP(f, 0.0, 1.0);
		tempf= but->min+f*(but->max-but->min);		
		temp= floor(tempf+.5);

		if(qual & LR_CTRLKEY) {
			if(tempf==but->min || tempf==but->max);
			else if( but->pointype==FLO ) {

				if(qual & LR_SHIFTKEY) {
					if(tempf==but->min || tempf==but->max);
					else if(but->max-but->min < 2.10) tempf= 0.01*floor(100.0*tempf);
					else if(but->max-but->min < 21.0) tempf= 0.1*floor(10.0*tempf);
					else tempf= floor(tempf);
				}
				else {
					if(but->max-but->min < 2.10) tempf= 0.1*floor(10*tempf);
					else if(but->max-but->min < 21.0) tempf= floor(tempf);
					else tempf= 10.0*floor(tempf/10.0);
				}
			}
			else {
				temp= 10*(temp/10);
				tempf= temp;
			}
		} 
	
		value= ui_get_but_val(but);
		lvalue= floor(value+0.5);
		
		if( but->pointype!=FLO )
			redraw= (temp != lvalue);
		else
			redraw= (tempf != value);

		if (redraw) {
			pos= 1;
			ui_set_but_val(but, tempf);
			ui_check_but(but);
			ui_draw_but(but);
			ui_block_flush_back(but->block);
			
			if(but->a1) {	/* color number */
				uiBut *bt= but->prev;
				while(bt) {
					if(bt->a2 == but->a1) ui_draw_but(bt);
					bt= bt->prev;
				}
				bt= but->next;
				while(bt) {
					if(bt->a2 == but->a1) ui_draw_but(bt);
					bt= bt->next;
				}
			}
			/* save current window matrix (global UIwinmat)
			   because button callback function MIGHT change it
			   - which has until now occured through the Python API
			*/
			/* This is really not possible atm... nothing in Blender
			   supports such functionality even now. Calling function 
			   callbacks while using a button screws up the UI (ton) 
			*/
			/* Mat4CpyMat4(curmatrix, UIwinmat);
			uibut_do_func(but);
			Mat4CpyMat4(UIwinmat, curmatrix); */
		} 
		else BIF_wait_for_statechange();
	}

	
	if(temp!=32767 && pos==0) {  /* plus 1 or minus 1 */
		
		if( but->type==SLI) f= (float)(mval[0]-but->x1)/(but->x2-but->x1-h);
		else f= (float)(mval[0]- (but->x1+but->x2)/2)/( (but->x2-but->x1)/2 - h);
		
		f= but->min+f*(but->max-but->min);
		
		if( but->pointype!=FLO ) {

			if(f<temp) temp--;
			else temp++;
			if( temp>=but->min && temp<=but->max)
				ui_set_but_val(but, (float)temp);
		
		} 
		else {

			if(f<tempf) tempf-=.01;
			else tempf+=.01;
			if( tempf>=but->min && tempf<=but->max)
				ui_set_but_val(but, tempf);

		}
	}
	ui_check_but(but);
	ui_draw_but(but);
	uibut_do_func(but);
	ui_block_flush_back(but->block);
	
	return but->retval;
}

static int ui_do_but_NUMSLI(uiBut *but)
{
	short mval[2];

	/* first define if it's a slider or textbut */
	uiGetMouse(mywinget(), mval);
	
	if(mval[0]>= -6+(but->x1+but->x2)/2 ) {	/* slider */
		but->flag |= UI_SELECT;
		ui_draw_but(but);
		ui_do_but_SLI(but);
		but->flag &= ~UI_SELECT;
	}
	else {
		ui_act_as_text_but(but);
		uibut_do_func(but);	// this is done in ui_do_but_SLI() not in ui_act_as_text_but()
	}

	while(get_mbut() & L_MOUSE) BIF_wait_for_statechange();
	
	ui_draw_but(but);
	
	/* hsv patch */
	if(but->type==HSVSLI) {
	
		if(but->str[0]=='H') {
			ui_draw_but(but->next);
			ui_draw_but(but->next->next);
		} 
		else if(but->str[0]=='S') {
			ui_draw_but(but->next);
			ui_draw_but(but->prev);
		} 
		else if(but->str[0]=='V') {
			ui_draw_but(but->prev);
			ui_draw_but(but->prev->prev);
		}
	}
	
	return but->retval;
}

/* event denotes if we make first item active or not */
static uiBlock *ui_do_but_BLOCK(uiBut *but, int event)
{
	uiBlock *block;
	uiBut *bt;
	
	but->flag |= UI_SELECT;
	ui_draw_but(but);	

	block= but->block_func(but->poin);
	block->parent= but->block;	/* allows checking for nested pulldowns */

	block->xofs = -2;	/* for proper alignment */
	
	/* only used for automatic toolbox, so can set the shift flag */
	if(but->flag & UI_MAKE_TOP) {
		block->direction= UI_TOP|UI_SHIFT_FLIPPED;
		uiBlockFlipOrder(block);
	}
	if(but->flag & UI_MAKE_DOWN) block->direction= UI_DOWN|UI_SHIFT_FLIPPED;
	if(but->flag & UI_MAKE_LEFT) block->direction |= UI_LEFT;
	if(but->flag & UI_MAKE_RIGHT) block->direction |= UI_RIGHT;
	
	ui_positionblock(block, but);
	block->flag |= UI_BLOCK_LOOP;
	
	/* blocks can come (and get scaled) from a normal window, now we go to screenspace */
	block->win= G.curscreen->mainwin;
	for(bt= block->buttons.first; bt; bt= bt->next) bt->win= block->win;
	bwin_getsinglematrix(block->win, block->winmat);
	
	/* postpone draw, this will cause a new window matrix, first finish all other buttons */
	block->flag |= UI_BLOCK_REDRAW;
	
	if(event!=MOUSEX && event!=MOUSEY && event!=LEFTMOUSE && but->type==BLOCK) {
		bt= ui_but_first(block);
		if(bt) bt->flag |= UI_ACTIVE;
	}
	
	but->flag &= ~UI_SELECT;
	uibut_do_func(but);
	
	if(but->retval)
		addqueue(curarea->win, UI_BUT_EVENT, (short)but->retval);
	
	return block;
}

static int ui_do_but_BUTM(uiBut *but)
{
	/* draw 'pushing-in' when clicked on for use as a normal button in a panel */
	do {
		int oflag= but->flag;
		short mval[2];
			
		uiGetMouse(mywinget(), mval);
		
		if (uibut_contains_pt(but, mval))
			but->flag |= UI_SELECT;
		else
			but->flag &= ~UI_SELECT;
		
		if (but->flag != oflag) {
			ui_draw_but(but);
			ui_block_flush_back(but->block);
		}
		
		PIL_sleep_ms(10);
	} while (get_mbut() & L_MOUSE);
	
	ui_set_but_val(but, but->min);
	UIafterfunc_butm= but->butm_func;
	UIafterfunc_arg1= but->butm_func_arg;
	UIafterval= but->a2;
	
	uibut_do_func(but);
	
	but->flag &= ~UI_SELECT;
	ui_draw_but(but);

	return but->retval;
}

static int ui_do_but_LABEL(uiBut *but)
{
	uibut_do_func(but);
	return but->retval;
}

static uiBut *ui_get_valid_link_button(uiBlock *block, uiBut *but, short *mval)
{
	uiBut *bt;
	
		/* find button to link to */
	for (bt= block->buttons.first; bt; bt= bt->next)
		if(bt!=but && uibut_contains_pt(bt, mval))
			break;

	if (bt) {
		if (but->type==LINK && bt->type==INLINK) {
			if( but->link->tocode == (int)bt->min ) {
				return bt;
			}
		}
		else if(but->type==INLINK && bt->type==LINK) {
			if( bt->link->tocode == (int)but->min ) {
				return bt;
			}
		}
	}

	return NULL;
}

static int ui_is_a_link(uiBut *from, uiBut *to)
{
	uiLinkLine *line;
	uiLink *link;
	
	link= from->link;
	if(link) {
		line= link->lines.first;
		while(line) {
			if(line->from==from && line->to==to) return 1;
			line= line->next;
		}
	}
	return 0;
}

static uiBut *ui_find_inlink(uiBlock *block, void *poin)
{
	uiBut *but;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==INLINK) {
			if(but->poin == poin) return but;
		}
		but= but->next;
	}
	return NULL;
}

static void ui_add_link_line(ListBase *listb, uiBut *but, uiBut *bt)
{
	uiLinkLine *line;
	
	line= MEM_callocN(sizeof(uiLinkLine), "linkline");
	BLI_addtail(listb, line);
	line->from= but;
	line->to= bt;
}


void uiComposeLinks(uiBlock *block)
{
	uiBut *but, *bt;
	uiLink *link;
	void ***ppoin;
	int a;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==LINK) {
			link= but->link;
			
			/* for all pointers in the array */
			if(link) {
				if(link->ppoin) {
					ppoin= link->ppoin;
					for(a=0; a < *(link->totlink); a++) {
						bt= ui_find_inlink(block, (*ppoin)[a] );
						if(bt) {
							ui_add_link_line(&link->lines, but, bt);
						}
					}
				}
				else if(link->poin) {
					bt= ui_find_inlink(block, *(link->poin) );
					if(bt) {
						ui_add_link_line(&link->lines, but, bt);
					}
				}
			}
		}
		but= but->next;
	}
}

static void ui_add_link(uiBut *from, uiBut *to)
{
	/* in 'from' we have to add a link to 'to' */
	uiLink *link;
	void **oldppoin;
	int a;
	
	if(ui_is_a_link(from, to)) {
		printf("already exists\n");
		return;
	}
	
	link= from->link;

	/* are there more pointers allowed? */
	if(link->ppoin) {
		oldppoin= *(link->ppoin);
		
		(*(link->totlink))++;
		*(link->ppoin)= MEM_callocN( *(link->totlink)*sizeof(void *), "new link");

		for(a=0; a< (*(link->totlink))-1; a++) {
			(*(link->ppoin))[a]= oldppoin[a];
		}
		(*(link->ppoin))[a]= to->poin;
		
		if(oldppoin) MEM_freeN(oldppoin);
	}
	else {
		*(link->poin)= to->poin;
	}
	
}

static int ui_do_but_LINK(uiBlock *block, uiBut *but)
{
	/* 
	 * This button only visualizes, the dobutton mode
	 * can add a new link, but then the whole system
	 * should be redrawn/initialized. 
	 * 
	 */
	uiBut *bt=0, *bto=NULL;
	short sval[2], mval[2], mvalo[2], first= 1;

	uiGetMouse(curarea->win, sval);
	mvalo[0]= sval[0];
	mvalo[1]= sval[1];
	
	while (get_mbut() & L_MOUSE) {
		uiGetMouse(curarea->win, mval);

		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || first) {			
				/* clear completely, because of drawbuttons */
			bt= ui_get_valid_link_button(block, but, mval);
			if(bt) {
				bt->flag |= UI_ACTIVE;
				ui_draw_but(bt);
			}
			if(bto && bto!=bt) {
				bto->flag &= ~UI_ACTIVE;
				ui_draw_but(bto);
			}
			bto= bt;

			if (!first) {
				glutil_draw_front_xor_line(sval[0], sval[1], mvalo[0], mvalo[1]);
			}
			glutil_draw_front_xor_line(sval[0], sval[1], mval[0], mval[1]);

			mvalo[0]= mval[0];
			mvalo[1]= mval[1];

			first= 0;
		}
		else BIF_wait_for_statechange();		
	}
	
	if (!first) {
		glutil_draw_front_xor_line(sval[0], sval[1], mvalo[0], mvalo[1]);
	}

	if(bt) {
		if(but->type==LINK) ui_add_link(but, bt);
		else ui_add_link(bt, but);

		scrarea_queue_winredraw(curarea);
	}

	return 0;
}

/* picker sizes S hsize, F full size, D spacer, B button/pallette height  */
#define SPICK	110.0
#define FPICK	180.0
#define DPICK	6.0
#define BPICK	24.0

#define UI_PALETTE_TOT 16
/* note; in tot+1 the old color is stored */
static float palette[UI_PALETTE_TOT+1][3]= {
{0.93, 0.83, 0.81}, {0.88, 0.89, 0.73}, {0.69, 0.81, 0.57}, {0.51, 0.76, 0.64}, 
{0.37, 0.56, 0.61}, {0.33, 0.29, 0.55}, {0.46, 0.21, 0.51}, {0.40, 0.12, 0.18}, 
{1.0, 1.0, 1.0}, {0.85, 0.85, 0.85}, {0.7, 0.7, 0.7}, {0.56, 0.56, 0.56}, 
{0.42, 0.42, 0.42}, {0.28, 0.28, 0.28}, {0.14, 0.14, 0.14}, {0.0, 0.0, 0.0}
};  

/* for picker, while editing hsv */
static void ui_set_but_hsv(uiBut *but)
{
	float col[3];
	
	hsv_to_rgb(but->hsv[0], but->hsv[1], but->hsv[2], col, col+1, col+2);
	ui_set_but_vectorf(but, col);
}

static void update_picker_hex(uiBlock *block, float *rgb)
{
	uiBut *bt;
	char col[16];
	
	sprintf(col, "%02X%02X%02X", (unsigned int)(rgb[0]*255.0), (unsigned int)(rgb[1]*255.0), (unsigned int)(rgb[2]*255.0));
	
	// this updates button strings, is hackish... but button pointers are on stack of caller function

	for(bt= block->buttons.first; bt; bt= bt->next) {
		if(strcmp(bt->str, "Hex: ")==0) {
			strcpy(bt->poin, col);
			ui_check_but(bt);
			break;
		}
	}
}

static void update_picker_buts_hsv(uiBlock *block, float *hsv, char *poin)
{
	uiBut *bt;
	float r, g, b;
	float rgb[3];
	
	// this updates button strings, is hackish... but button pointers are on stack of caller function
	hsv_to_rgb(hsv[0], hsv[1], hsv[2], &r, &g, &b);
	
	rgb[0] = r; rgb[1] = g; rgb[2] = b;
	update_picker_hex(block, rgb);

	for(bt= block->buttons.first; bt; bt= bt->next) {
		if(bt->type==HSVCUBE) {
			VECCOPY(bt->hsv, hsv);
			ui_set_but_hsv(bt);
		}
		else if(bt->str[1]==' ') {
			if(bt->str[0]=='R') {
				ui_set_but_val(bt, r);
			}
			else if(bt->str[0]=='G') {
				ui_set_but_val(bt, g);
			}
			else if(bt->str[0]=='B') {
				ui_set_but_val(bt, b);
			}
			else if(bt->str[0]=='H') {
				ui_set_but_val(bt, hsv[0]);
			}
			else if(bt->str[0]=='S') {
				ui_set_but_val(bt, hsv[1]);
			}
			else if(bt->str[0]=='V') {
				ui_set_but_val(bt, hsv[2]);
			}
		}		
	}
}

static void update_picker_buts_hex(uiBlock *block, char *hexcol)
{
	uiBut *bt;
	float r=0, g=0, b=0;
	float h, s, v;
	
	
	// this updates button strings, is hackish... but button pointers are on stack of caller function
	hex_to_rgb(hexcol, &r, &g, &b);
	rgb_to_hsv(r, g, b, &h, &s, &v);

	for(bt= block->buttons.first; bt; bt= bt->next) {
		if(bt->type==HSVCUBE) {
			bt->hsv[0] = h;
			bt->hsv[1] = s;			
			bt->hsv[2] = v;
			ui_set_but_hsv(bt);
		}
		else if(bt->str[1]==' ') {
			if(bt->str[0]=='R') {
				ui_set_but_val(bt, r);
			}
			else if(bt->str[0]=='G') {
				ui_set_but_val(bt, g);
			}
			else if(bt->str[0]=='B') {
				ui_set_but_val(bt, b);
			}
			else if(bt->str[0]=='H') {
				ui_set_but_val(bt, h);
			}
			else if(bt->str[0]=='S') {
				ui_set_but_val(bt, s);
			}
			else if(bt->str[0]=='V') {
				ui_set_but_val(bt, v);
			}
		}
	}
}



/* bt1 is palette but, col1 is original color */
/* callback to copy from/to palette */
static void do_palette_cb(void *bt1, void *col1)
{
	uiBut *but1= (uiBut *)bt1;
	uiBut *but;
	float *col= (float *)col1;
	float *fp, hsv[3];
	
	fp= (float *)but1->poin;
	
	if( (get_qual() & LR_CTRLKEY) ) {
		VECCOPY(fp, col);
	}
	else {
		VECCOPY(col, fp);
	}
	
	rgb_to_hsv(col[0], col[1], col[2], hsv, hsv+1, hsv+2);
	update_picker_buts_hsv(but1->block, hsv, but1->poin);
	update_picker_hex(but1->block, col);
	
	for (but= but1->block->buttons.first; but; but= but->next) {
		ui_check_but(but);
		ui_draw_but(but);
	}
	but= but1->block->buttons.first;
	ui_block_flush_back(but->block);
}

/* bt1 is num but, hsv1 is pointer to original color in hsv space*/
/* callback to handle changes in num-buts in picker */
static void do_palette1_cb(void *bt1, void *hsv1)
{
	uiBut *but1= (uiBut *)bt1;
	uiBut *but;
	float *hsv= (float *)hsv1;
	float *fp= NULL;
	
	if(but1->str[1]==' ') {
		if(but1->str[0]=='R') fp= (float *)but1->poin;
		else if(but1->str[0]=='G') fp= ((float *)but1->poin)-1;
		else if(but1->str[0]=='B') fp= ((float *)but1->poin)-2;
	}
	if(fp) {
		rgb_to_hsv(fp[0], fp[1], fp[2], hsv, hsv+1, hsv+2);
	} 
	update_picker_buts_hsv(but1->block, hsv, but1->poin);
	
	for (but= but1->block->buttons.first; but; but= but->next) {
		ui_check_but(but);
		ui_draw_but(but);
	}
	
	but= but1->block->buttons.first;
	ui_block_flush_back(but->block);

}

/* bt1 is num but, col1 is pointer to original color */
/* callback to handle changes in num-buts in picker */
static void do_palette2_cb(void *bt1, void *col1)
{
	uiBut *but1= (uiBut *)bt1;
	uiBut *but;
	float *rgb= (float *)col1;
	float *fp= NULL;
	
	if(but1->str[1]==' ') {
		if(but1->str[0]=='H') fp= (float *)but1->poin;
		else if(but1->str[0]=='S') fp= ((float *)but1->poin)-1;
		else if(but1->str[0]=='V') fp= ((float *)but1->poin)-2;
	}
	if(fp) {
		hsv_to_rgb(fp[0], fp[1], fp[2], rgb, rgb+1, rgb+2);
	} 
	update_picker_buts_hsv(but1->block, fp, but1->poin);

	for (but= but1->block->buttons.first; but; but= but->next) {
		ui_check_but(but);
		ui_draw_but(but);
	}
	
	but= but1->block->buttons.first;
	ui_block_flush_back(but->block);

}

static void do_palette_hex_cb(void *bt1, void *hexcl)
{
	uiBut *but1= (uiBut *)bt1;
	uiBut *but;
	char *hexcol= (char *)hexcl;
	
	update_picker_buts_hex(but1->block, hexcol);	
	
	for (but= but1->block->buttons.first; but; but= but->next) {
		ui_check_but(but);
		ui_draw_but(but);
	}
	
	but= but1->block->buttons.first;
	ui_block_flush_back(but->block);
}


/* used for both 3d view and image window */
static void do_palette_sample_cb(void *bt1, void *col1)	/* frontbuf */
{
	uiBut *but1= (uiBut *)bt1;
	uiBut *but;
	float tempcol[4];
	int x=0, y=0;
	short mval[2];
	float hsv[3];
	short capturing;
	int oldcursor;
	Window *win;
	unsigned short dev;
	
	oldcursor=get_cursor();
	win=winlay_get_active_window();
	
	while (get_mbut() & L_MOUSE) BIF_wait_for_statechange();
	
	SetBlenderCursor(BC_EYEDROPPER_CURSOR);
	
	/* loop and wait for a mouse click */
	capturing = TRUE;
	while(capturing) {
		char ascii;
		short val;
		
		dev = extern_qread_ext(&val, &ascii);
		
		if(dev==INPUTCHANGE) break;
		if(get_mbut() & R_MOUSE) break;
		else if(get_mbut() & L_MOUSE) {
			uiGetMouse(mywinget(), mval);
			x= mval[0]; y= mval[1];
			
			capturing = FALSE;
			break;
		}
		else if(dev==ESCKEY) break;
	}
	window_set_cursor(win, oldcursor);
	
	if(capturing) return;
	
	if(x<0 || y<0) return;
	
	/* if we've got a glick, use OpenGL to sample the color under the mouse pointer */
	glReadBuffer(GL_FRONT);
	glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, tempcol);
	glReadBuffer(GL_BACK);
	
	/* and send that color back to the picker */
	rgb_to_hsv(tempcol[0], tempcol[1], tempcol[2], hsv, hsv+1, hsv+2);
	update_picker_buts_hsv(but1->block, hsv, but1->poin);
	update_picker_hex(but1->block, tempcol);
	
	for (but= but1->block->buttons.first; but; but= but->next) {
		ui_check_but(but);
		ui_draw_but(but);
	}
	
	but= but1->block->buttons.first;
	ui_block_flush_back(but->block);
}


/* color picker, Gimp version. mode: 'f' = floating panel, 'p' =  popup */
/* col = read/write to, hsv/old/hexcol = memory for temporal use */
void uiBlockPickerButtons(uiBlock *block, float *col, float *hsv, float *old, char *hexcol, char mode, short retval)
{
	uiBut *bt;
	float h, offs;
	int a;

	VECCOPY(old, col);	// old color stored there, for palette_cb to work
	
	// the cube intersection
	bt= uiDefButF(block, HSVCUBE, retval, "",	0,DPICK+BPICK,FPICK,FPICK, col, 0.0, 0.0, 2, 0, "");
	uiButSetFlag(bt, UI_NO_HILITE);

	bt= uiDefButF(block, HSVCUBE, retval, "",	0,0,FPICK,BPICK, col, 0.0, 0.0, 3, 0, "");
	uiButSetFlag(bt, UI_NO_HILITE);

	// palette
	
	uiBlockSetEmboss(block, UI_EMBOSSP);
	
	bt=uiDefButF(block, COL, retval, "",		FPICK+DPICK, 0, BPICK,BPICK, old, 0.0, 0.0, -1, 0, "Old color, click to restore");
	uiButSetFunc(bt, do_palette_cb, bt, col);
	uiDefButF(block, COL, retval, "",		FPICK+DPICK, BPICK+DPICK, BPICK,60-BPICK-DPICK, col, 0.0, 0.0, -1, 0, "Active color");

	h= (DPICK+BPICK+FPICK-64)/(UI_PALETTE_TOT/2.0);
	uiBlockBeginAlign(block);
	for(a= -1+UI_PALETTE_TOT/2; a>=0; a--) {
		bt= uiDefButF(block, COL, retval, "",	FPICK+DPICK, 65.0+(float)a*h, BPICK/2, h, palette[a+UI_PALETTE_TOT/2], 0.0, 0.0, -1, 0, "Click to choose, hold CTRL to store in palette");
		uiButSetFunc(bt, do_palette_cb, bt, col);
		bt= uiDefButF(block, COL, retval, "",	FPICK+DPICK+BPICK/2, 65.0+(float)a*h, BPICK/2, h, palette[a], 0.0, 0.0, -1, 0, "Click to choose, hold CTRL to store in palette");		
		uiButSetFunc(bt, do_palette_cb, bt, col);
	}
	uiBlockEndAlign(block);
	
	uiBlockSetEmboss(block, UI_EMBOSS);

	// buttons
	rgb_to_hsv(col[0], col[1], col[2], hsv, hsv+1, hsv+2);
	sprintf(hexcol, "%02X%02X%02X", (unsigned int)(col[0]*255.0), (unsigned int)(col[1]*255.0), (unsigned int)(col[2]*255.0));	

	offs= FPICK+2*DPICK+BPICK;

	/* note; made this a TOG now, with NULL pointer. Is because BUT now gets handled with a afterfunc */
	bt= uiDefIconTextBut(block, TOG, UI_RETURN_OK, ICON_EYEDROPPER, "Sample", offs+55, 170, 85, 20, NULL, 0, 0, 0, 0, "Sample the color underneath the following mouse click (ESC or RMB to cancel)");
	uiButSetFunc(bt, do_palette_sample_cb, bt, col);
	uiButSetFlag(bt, UI_TEXT_LEFT);
	
	bt= uiDefBut(block, TEX, retval, "Hex: ", offs, 140, 140, 20, hexcol, 0, 8, 0, 0, "Hex triplet for color (#RRGGBB)");
	uiButSetFunc(bt, do_palette_hex_cb, bt, hexcol);

	uiBlockBeginAlign(block);
	bt= uiDefButF(block, NUMSLI, retval, "R ",	offs, 110, 140,20, col, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette1_cb, bt, hsv);
	bt= uiDefButF(block, NUMSLI, retval, "G ",	offs, 90, 140,20, col+1, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette1_cb, bt, hsv);
	bt= uiDefButF(block, NUMSLI, retval, "B ",	offs, 70, 140,20, col+2, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette1_cb, bt, hsv);
	
	uiBlockBeginAlign(block);
	bt= uiDefButF(block, NUMSLI, retval, "H ",	offs, 40, 140,20, hsv, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette2_cb, bt, col);
	bt= uiDefButF(block, NUMSLI, retval, "S ",	offs, 20, 140,20, hsv+1, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette2_cb, bt, col);
	bt= uiDefButF(block, NUMSLI, retval, "V ",	offs, 0, 140,20, hsv+2, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette2_cb, bt, col);
	uiBlockEndAlign(block);
}

static int ui_do_but_COL(uiBut *but)
{
	uiBlock *block;
	uiBut *bt;
	ListBase listb={NULL, NULL};
	float hsv[3], old[3], *poin= NULL, colstore[3];
	static char hexcol[128];
	short event;
	
	// signal to prevent calling up color picker
	if(but->a1 == -1) {
		uibut_do_func(but);
		return but->retval;
	}
	
	// enable char button too, use temporal colstore for color
	if(but->pointype!=FLO) {
		if(but->pointype==CHA) {
			ui_get_but_vectorf(but, colstore);
			poin= colstore;
		}
		else return but->retval;
	}
	else poin= (float *)but->poin;
	
	block= uiNewBlock(&listb, "colorpicker", UI_EMBOSS, UI_HELV, but->win);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW;
	block->themecol= TH_BUT_NUM;
	
	uiBlockPickerButtons(block, poin, hsv, old, hexcol, 'p', 0);

	/* and lets go */
	block->direction= UI_TOP;
	ui_positionblock(block, but);
	uiBoundsBlock(block, 3);
	
	/* blocks can come from a normal window, but we go to screenspace */
	block->win= G.curscreen->mainwin;
	for(bt= block->buttons.first; bt; bt= bt->next) bt->win= block->win;
	bwin_getsinglematrix(block->win, block->winmat);

	event= uiDoBlocks(&listb, 0, 1);
	
	if(but->pointype==CHA) ui_set_but_vectorf(but, colstore);
	
	uibut_do_func(but);
	return but->retval;

}

static int ui_do_but_HSVCUBE(uiBut *but)
{
	uiBut *bt;
	float x, y;
	short mval[2], mvalo[2];
	
	mvalo[0]= mvalo[1]= -32000;
	
	/* we work on persistant hsv, to prevent it being converted back and forth all the time */
			   
	while (get_mbut() & L_MOUSE) {
		
		uiGetMouse(mywinget(), mval);

		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {			
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			/* relative position within box */
			x= ((float)mval[0]-but->x1)/(but->x2-but->x1);
			y= ((float)mval[1]-but->y1)/(but->y2-but->y1);
			CLAMP(x, 0.0, 1.0);
			CLAMP(y, 0.0, 1.0);
			
			if(but->a1==0) {
				but->hsv[0]= x; 
				but->hsv[2]= y; 
				// hsv_to_rgb(x, s, y, col, col+1, col+2);
			}
			else if(but->a1==1) {
				but->hsv[0]= x; 				
				but->hsv[1]= y; 				
				// hsv_to_rgb(x, y, v, col, col+1, col+2);
			}
			else if(but->a1==2) {
				but->hsv[2]= x; 
				but->hsv[1]= y; 
				// hsv_to_rgb(h, y, x, col, col+1, col+2);
			}
			else {
				but->hsv[0]= x; 
				// hsv_to_rgb(x, s, v, col, col+1, col+2);
			}
	
			ui_set_but_hsv(but);	// converts to rgb
			
			// update button values and strings
			update_picker_buts_hsv(but->block, but->hsv, but->poin);
//			update_picker_buts_hex(but->block, but->hsv);			

			/* we redraw the entire block */
			for (bt= but->block->buttons.first; bt; bt= bt->next) {
				if(but->poin == bt->poin) VECCOPY(bt->hsv, but->hsv);
				ui_draw_but(bt);
			}
			ui_block_flush_back(but->block);
		}
		else BIF_wait_for_statechange();
	}

	return but->retval;
}

#ifdef INTERNATIONAL

static int ui_do_but_CHARTAB(uiBut *but)
{
	/* Variables */
	short mval[2];
	float sx, sy, ex, ey;
	float width, height;
	float butw, buth;
	int x, y, cs, che;

	/* Check the position */
	uiGetMouse(mywinget(), mval);

	/* Calculate the size of the button */
	width = abs(but->x2 - but->x1);
	height = abs(but->y2 - but->y1);

	butw = floor(width / 12);
	buth = floor(height / 6);

	/* Initialize variables */
	sx = but->x1;
	ex = but->x1 + butw;
	sy = but->y1 + height - buth;
	ey = but->y1 + height;

	cs = G.charstart;

	/* And the character is */
	x = (int) ((mval[0] / butw) - 0.5);
	y = (int) (6 - ((mval[1] / buth) - 0.5));

	che = cs + (y*12) + x;

	if(che > G.charmax)
		che = 0;

	if(G.obedit)
	{
		do_textedit(0,0,che);
	}

	return but->retval;
}

#endif

static int vergcband(const void *a1, const void *a2)
{
	const CBData *x1=a1, *x2=a2;
	
	if( x1->pos > x2->pos ) return 1;
	else if( x1->pos < x2->pos) return -1;
	return 0;
}


static void do_colorband_evt(ColorBand *coba)
{
	int a;
	
	if(coba==NULL) return;
	
	if(coba->tot<2) return;
	
	for(a=0; a<coba->tot; a++) coba->data[a].cur= a;
		qsort(coba->data, coba->tot, sizeof(CBData), vergcband);
	for(a=0; a<coba->tot; a++) {
		if(coba->data[a].cur==coba->cur) {
			if(coba->cur!=a) addqueue(curarea->win, REDRAW, 0);	/* button cur */
			coba->cur= a;
			break;
		}
	}
}

static int ui_do_but_COLORBAND(uiBut *but)
{	
	ColorBand *coba= (ColorBand *)but->poin;
	CBData *cbd;
	float dx, width= but->x2-but->x1;
	int a;
	int mindist= 12, xco;
	short mval[2], mvalo[2];
	
	uiGetMouse(mywinget(), mvalo);
	
	if(G.qual & LR_CTRLKEY) {
		/* insert new key on mouse location */
		if(coba->tot < MAXCOLORBAND-1) {
			float pos= ((float)(mvalo[0] - but->x1))/width;
			float col[4];
			
			do_colorband(coba, pos, col);	/* executes it */
			
			coba->tot++;
			coba->cur= coba->tot-1;
			
			coba->data[coba->cur].r= col[0];
			coba->data[coba->cur].g= col[1];
			coba->data[coba->cur].b= col[2];
			coba->data[coba->cur].a= col[3];
			coba->data[coba->cur].pos= pos;
			
			do_colorband_evt(coba);
		}
	}
	else {
		
		/* first, activate new key when mouse is close */
		for(a=0, cbd= coba->data; a<coba->tot; a++, cbd++) {
			xco= but->x1 + (cbd->pos*width);
			xco= ABS(xco-mvalo[0]);
			if(a==coba->cur) xco+= 5; // selected one disadvantage
			if(xco<mindist) {
				coba->cur= a;
				mindist= xco;
			}
		}
		
		cbd= coba->data + coba->cur;
		
		while(get_mbut() & L_MOUSE) {
			uiGetMouse(mywinget(), mval);
			if(mval[0]!=mvalo[0]) {
				dx= mval[0]-mvalo[0];
				dx/= width;
				cbd->pos+= dx;
				CLAMP(cbd->pos, 0.0, 1.0);
				
				ui_draw_but(but);
				ui_block_flush_back(but->block);
				
				do_colorband_evt(coba);
				cbd= coba->data + coba->cur;	/* because qsort */
				
				mvalo[0]= mval[0];
			}
			BIF_wait_for_statechange();
		}
	}
	
	return but->retval;
}

/* button is presumed square */
/* if mouse moves outside of sphere, it does negative normal */
static int ui_do_but_NORMAL(uiBut *but)
{
	float dx, dy, rad, radsq, mrad, *fp= (float *)but->poin;
	int firsttime=1;
	short mval[2], mvalo[2], mvals[2], mvaldx, mvaldy;
	
	rad= (but->x2 - but->x1);
	radsq= rad*rad;
	
	if(fp[2]>0.0f) {
		mvaldx= (rad*fp[0]);
		mvaldy= (rad*fp[1]);
	}
	else if(fp[2]> -1.0f) {
		mrad= rad/sqrt(fp[0]*fp[0] + fp[1]*fp[1]);
		
		mvaldx= 2.0f*mrad*fp[0] - (rad*fp[0]);
		mvaldy= 2.0f*mrad*fp[1] - (rad*fp[1]);
	}
	else mvaldx= mvaldy= 0;
	
	uiGetMouse(mywinget(), mvalo);
	mvals[0]= mvalo[0];
	mvals[1]= mvalo[1];
	
	while(get_mbut() & L_MOUSE) {
		
		uiGetMouse(mywinget(), mval);
		
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || firsttime) {
			firsttime= 0;
			
			dx= (float)(mval[0]+mvaldx-mvals[0]);
			dy= (float)(mval[1]+mvaldy-mvals[1]);

			mrad= dx*dx+dy*dy;
			if(mrad < radsq) {	/* inner circle */
				fp[0]= dx;
				fp[1]= dy;
				fp[2]= sqrt( radsq-dx*dx-dy*dy );
			}
			else {	/* outer circle */
				
				mrad= rad/sqrt(mrad);	// veclen
				
				dx*= (2.0f*mrad - 1.0f);
				dy*= (2.0f*mrad - 1.0f);
				
				mrad= dx*dx+dy*dy;
				if(mrad < radsq) {
					fp[0]= dx;
					fp[1]= dy;
					fp[2]= -sqrt( radsq-dx*dx-dy*dy );
				}
			}
			Normalize(fp);
				
			ui_draw_but(but);
			ui_block_flush_back(but->block);

			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
		}
		BIF_wait_for_statechange();
	}
			
	return but->retval;
}

static int ui_do_but_CURVE(uiBut *but)
{
	CurveMapping *cumap= (CurveMapping *)but->poin;
	CurveMap *cuma= cumap->cm+cumap->cur;
	CurveMapPoint *cmp= cuma->curve;
	float fx, fy, zoomx, zoomy, offsx, offsy;
	float dist, mindist= 200.0f;	// 14 pixels radius
	int a, sel= -1, retval= but->retval;
	short mval[2], mvalo[2];
	
	uiGetMouse(mywinget(), mval);
	
	/* calculate offset and zoom */
	zoomx= (but->x2-but->x1)/(cumap->curr.xmax-cumap->curr.xmin);
	zoomy= (but->y2-but->y1)/(cumap->curr.ymax-cumap->curr.ymin);
	offsx= cumap->curr.xmin;
	offsy= cumap->curr.ymin;
	
	if(G.qual & LR_CTRLKEY) {
		
		fx= ((float)mval[0] - but->x1)/zoomx + offsx;
		fy= ((float)mval[1] - but->y1)/zoomy + offsy;
		
		curvemap_insert(cuma, fx, fy);
		curvemapping_changed(cumap, 0);

		ui_draw_but(but);
		ui_block_flush_back(but->block);
	}
	
	
	/* check for selecting of a point */
	cmp= cuma->curve;	/* ctrl adds point, new malloc */
	for(a=0; a<cuma->totpoint; a++) {
		fx= but->x1 + zoomx*(cmp[a].x-offsx);
		fy= but->y1 + zoomy*(cmp[a].y-offsy);
		dist= (fx-mval[0])*(fx-mval[0]) + (fy-mval[1])*(fy-mval[1]);
		if(dist < mindist) {
			sel= a;
			mindist= dist;
		}
	}
	
	if (sel == -1) {
		/* if the click didn't select anything, check if it's clicked on the 
		 * curve itself, and if so, add a point */
		fx= ((float)mval[0] - but->x1)/zoomx + offsx;
		fy= ((float)mval[1] - but->y1)/zoomy + offsy;
		
		cmp= cuma->table;

		/* loop through the curve segment table and find what's near the mouse.
		 * 0.05 is kinda arbitrary, but seems to be what works nicely. */
		for(a=0; a<=CM_TABLE; a++) {			
			if ( ( fabs(fx - cmp[a].x) < (0.05) ) && ( fabs(fy - cmp[a].y) < (0.05) ) ) {
			
				curvemap_insert(cuma, fx, fy);
				curvemapping_changed(cumap, 0);
				
				ui_draw_but(but);
				ui_block_flush_back(but->block);
				
				/* reset cmp back to the curve points again, rather than drawing segments */		
				cmp= cuma->curve;
				
				/* find newly added point and make it 'sel' */
				for(a=0; a<cuma->totpoint; a++) {
					if (cmp[a].x == fx) sel = a;
				}
					
				break;
			}
		}
	}
	
	/* ok, we move a point */
	if(sel!= -1) {
		int moved_point;
		int moved_mouse= 0;

		/* deselect all if this one is deselect. except if we hold shift */
		if((G.qual & LR_SHIFTKEY)==0 && (cmp[sel].flag & SELECT)==0)
			for(a=0; a<cuma->totpoint; a++)
				cmp[a].flag &= ~SELECT;
		cmp[sel].flag |= SELECT;
		
		/* draw to show select updates */
		ui_draw_but(but);
		ui_block_flush_back(but->block);
		
		/* while move mouse, do move points around */
		while(get_mbut() & L_MOUSE) {
			
			uiGetMouse(mywinget(), mvalo);
			
			if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {
				moved_mouse= 1;		/* for selection */
				moved_point= 0;		/* for ctrl grid, can't use orig coords because of sorting */
				
				fx= (mvalo[0]-mval[0])/zoomx;
				fy= (mvalo[1]-mval[1])/zoomy;
				for(a=0; a<cuma->totpoint; a++) {
					if(cmp[a].flag & SELECT) {
						float origx= cmp[a].x, origy= cmp[a].y;
						cmp[a].x+= fx;
						cmp[a].y+= fy;
						if( (get_qual() & LR_SHIFTKEY) ) {
							cmp[a].x= 0.125f*floor(0.5f + 8.0f*cmp[a].x);
							cmp[a].y= 0.125f*floor(0.5f + 8.0f*cmp[a].y);
						}
						if(cmp[a].x!=origx || cmp[a].y!=origy)
							moved_point= 1;
					}
				}
				curvemapping_changed(cumap, 0);	/* no remove doubles */
				
				ui_draw_but(but);
				ui_block_flush_back(but->block);
				
				if(moved_point) {
					mval[0]= mvalo[0];
					mval[1]= mvalo[1];
				}
			}
			BIF_wait_for_statechange();
		}
		
		if(moved_mouse==0) {
			/* deselect all, select one */
			if((G.qual & LR_SHIFTKEY)==0) {
				for(a=0; a<cuma->totpoint; a++)
					cmp[a].flag &= ~SELECT;
				cmp[sel].flag |= SELECT;
			}
		}
		else 
			curvemapping_changed(cumap, 1);	/* remove doubles */
		
		ui_draw_but(but);
		ui_block_flush_back(but->block);
	}
	else {
		/* we move the view */
		retval= B_NOP;
		
		while(get_mbut() & L_MOUSE) {
			
			uiGetMouse(mywinget(), mvalo);
			
			if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {
				fx= (mvalo[0]-mval[0])/zoomx;
				fy= (mvalo[1]-mval[1])/zoomy;
				
				/* clamp for clip */
				if(cumap->flag & CUMA_DO_CLIP) {
					if(cumap->curr.xmin-fx < cumap->clipr.xmin)
						fx= cumap->curr.xmin - cumap->clipr.xmin;
					else if(cumap->curr.xmax-fx > cumap->clipr.xmax)
						fx= cumap->curr.xmax - cumap->clipr.xmax;
					if(cumap->curr.ymin-fy < cumap->clipr.ymin)
						fy= cumap->curr.ymin - cumap->clipr.ymin;
					else if(cumap->curr.ymax-fy > cumap->clipr.ymax)
						fy= cumap->curr.ymax - cumap->clipr.ymax;
				}				
				cumap->curr.xmin-=fx;
				cumap->curr.ymin-=fy;
				cumap->curr.xmax-=fx;
				cumap->curr.ymax-=fy;
				
				ui_draw_but(but);
				ui_block_flush_back(but->block);
				
				mval[0]= mvalo[0];
				mval[1]= mvalo[1];
			}
		}
		BIF_wait_for_statechange();
	}
	
	return retval;
}

/* ************************************************ */

void uiSetButLock(int val, char *lockstr)
{
	UIlock |= val;
	if (val) UIlockstr= lockstr;
}

void uiClearButLock()
{
	UIlock= 0;
	UIlockstr= NULL;
}

/* *************************************************************** */

static void setup_file(uiBlock *block)
{
	uiBut *but;
	FILE *fp;

	fp= fopen("butsetup","w");
	if(fp==NULL);
	else {
		but= block->buttons.first;
		while(but) {
			ui_check_but(but);
			fprintf(fp,"%d,%d,%d,%d   %s %s\n", (int)but->x1, (int)but->y1, (int)( but->x2-but->x1), (int)(but->y2-but->y1), but->str, but->tip);
			but= but->next;
		}
		fclose(fp);
	}
}


static void edit_but(uiBlock *block, uiBut *but, uiEvent *uevent)
{
	short dx, dy, mval[2], mvalo[2], didit=0;
	
	getmouseco_sc(mvalo);
	while(TRUE) {
		if( !(get_mbut() & L_MOUSE) ) break;	
	
		getmouseco_sc(mval);
		dx= (mval[0]-mvalo[0]);
		dy= (mval[1]-mvalo[1]);
		
		if(dx!=0 || dy!=0) {
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			cpack(0xc0c0c0);
			glRectf(but->x1-2, but->y1-2, but->x2+2, but->y2+2); 
			
			if((uevent->qual & LR_SHIFTKEY)==0) {
				but->x1 += dx;
				but->y1 += dy;
			}
			but->x2 += dx;
			but->y2 += dy;
			
			ui_draw_but(but);
			ui_block_flush_back(but->block);
			didit= 1;

		}
		/* idle for this poor code */
		else PIL_sleep_ms(30);
	}
	if(didit) setup_file(block);
}


/* is called when LEFTMOUSE is pressed or released
 * return: butval or zero
 */
static int ui_do_button(uiBlock *block, uiBut *but, uiEvent *uevent)
{
	int retval= 0;

	if(but->lock) {
		if (but->lockstr) {
			error("%s", but->lockstr);
			return 0;
		}
	} 
	else {
		if( but->pointype ) {		/* there's a pointer needed */
			if(but->poin==0 ) {
				printf("DoButton pointer error: %s\n",but->str);
				return 0;
			}
		}
	}

	if(G.rt==1 && (uevent->qual & LR_CTRLKEY)) {
		edit_but(block, but, uevent);
		return 0;
	}
	
	block->flag |= UI_BLOCK_BUSY;

	switch(but->type) {
	case BUT:
		if(uevent->val) retval= ui_do_but_BUT(but);		
		break;

	case KEYEVT:
		if(uevent->val) retval= ui_do_but_KEYEVT(but);
		break;

	case TOG: 
	case TOGR: 
	case ICONTOG:
	case ICONTOGN:
	case TOGN:
	case BUT_TOGDUAL:
		if(uevent->val) {
			retval= ui_do_but_TOG(block, but, uevent->qual);
		}
		break;
		
	case ROW:
		if(uevent->val) retval= ui_do_but_ROW(block, but);
		break;

	case SCROLL:
		/* DrawBut(b, 1); */
		/* do_scrollbut(b); */
		/* DrawBut(b,0); */
		break;

	case NUM:
	case NUMABS:
		if(uevent->val) retval= ui_do_but_NUM(but);
		break;
		
	case SLI:
	case NUMSLI:
	case HSVSLI:
		if(uevent->val) retval= ui_do_but_NUMSLI(but);
		break;
		
	case ROUNDBOX:	
	case LABEL:	
		if(uevent->val) retval= ui_do_but_LABEL(but);
		break;
		
	case TOG3:	
		if(uevent->val) retval= ui_do_but_TOG3(but);
		break;
		
	case TEX:
		if(uevent->val) retval= ui_do_but_TEX(but);
		break;
		
	case MENU:
		if(uevent->val) retval= ui_do_but_MENU(but);
		break;
		
	case ICONROW:
		if(uevent->val) retval= ui_do_but_ICONROW(but);		
		break;
		
	case ICONTEXTROW:
		if(uevent->val) retval= ui_do_but_ICONTEXTROW(but);
		break;

	case IDPOIN:
		if(uevent->val) retval= ui_do_but_IDPOIN(but);	
		break;
	
	case BLOCK:
	case PULLDOWN:
		if(uevent->val) {
			ui_do_but_BLOCK(but, uevent->event);
			retval= 0;
			if(block->auto_open==0) block->auto_open= 1;
		}
		break;

	case BUTM:
		retval= ui_do_but_BUTM(but);
		break;

	case LINK:
	case INLINK:
		retval= ui_do_but_LINK(block, but);
		break;
		
	case COL:
		if(uevent->val) retval= ui_do_but_COL(but);
		break;
		
	case HSVCUBE:
		retval= ui_do_but_HSVCUBE(but);
		break;
	case BUT_COLORBAND:
		retval= ui_do_but_COLORBAND(but);
		break;
	case BUT_NORMAL:
		retval= ui_do_but_NORMAL(but);
		break;
	case BUT_CURVE:
		retval= ui_do_but_CURVE(but);
		break;
		
#ifdef INTERNATIONAL
	case CHARTAB:
		retval= ui_do_but_CHARTAB(but);
		break;
#endif
	}
	
	block->flag &= ~UI_BLOCK_BUSY;

	return retval;
}

static void ui_delete_active_linkline(uiBlock *block)
{
	uiBut *but;
	uiLink *link;
	uiLinkLine *line, *nline;
	int a, b;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==LINK && but->link) {
			line= but->link->lines.first;
			while(line) {
				
				nline= line->next;
				
				if(line->flag & UI_SELECT) {
					BLI_remlink(&but->link->lines, line);

					link= line->from->link;

					/* are there more pointers allowed? */
					if(link->ppoin) {
						
						if(*(link->totlink)==1) {
							*(link->totlink)= 0;
							MEM_freeN(*(link->ppoin));
							*(link->ppoin)= NULL;
						}
						else {
							b= 0;
							for(a=0; a< (*(link->totlink)); a++) {
								
								if( (*(link->ppoin))[a] != line->to->poin ) {
									(*(link->ppoin))[b]= (*(link->ppoin))[a];
									b++;
								}
							}	
							(*(link->totlink))--;
						}
					}
					else {
						*(link->poin)= NULL;
					}

					MEM_freeN(line);
				}
				line= nline;
			}
		}
		but= but->next;
	}
	
	/* temporal! these buttons can be everywhere... */
	allqueue(REDRAWBUTSLOGIC, 0);
}

static void ui_do_active_linklines(uiBlock *block, short *mval)
{
	uiBut *but;
	uiLinkLine *line, *act= NULL;
	float mindist= 12.0, fac, v1[2], v2[2], v3[3];
	int foundone= 0; 
	
	if(mval) {
		v1[0]= mval[0];
		v1[1]= mval[1];
		
		/* find a line close to the mouse */
		but= block->buttons.first;
		while(but) {
			if(but->type==LINK && but->link) {
				foundone= 1;
				line= but->link->lines.first;
				while(line) {
					v2[0]= line->from->x2;
					v2[1]= (line->from->y1+line->from->y2)/2.0;
					v3[0]= line->to->x1;
					v3[1]= (line->to->y1+line->to->y2)/2.0;
					
					fac= PdistVL2Dfl(v1, v2, v3);
					if(fac < mindist) {
						mindist= fac;
						act= line;
					}
					line= line->next;
				}
			}
			but= but->next;
		}
	}

	/* check for a 'found one' to prevent going to 'frontbuffer' mode.
		this slows done gfx quite some, and at OSX the 'finish' forces a swapbuffer */
	if(foundone) {
		glDrawBuffer(GL_FRONT);
		
		/* draw */
		but= block->buttons.first;
		while(but) {
			if(but->type==LINK && but->link) {
				line= but->link->lines.first;
				while(line) {
					if(line==act) {
						if((line->flag & UI_SELECT)==0) {
							line->flag |= UI_SELECT;
							ui_draw_linkline(but, line);
						}
					}
					else if(line->flag & UI_SELECT) {
						line->flag &= ~UI_SELECT;
						ui_draw_linkline(but, line);
					}
					line= line->next;
				}
			}
			but= but->next;
		}
		bglFlush();
		glDrawBuffer(GL_BACK);
	}
}


/* only to be used to prevent an 'outside' event when using nested pulldowns */
/* only one checks:
  - while mouse moves in triangular area defined old mouse position and left/right side of new menu
  - only for 1 second
  
  return 0: check outside
*/
static int ui_mouse_motion_towards_block(uiBlock *block, uiEvent *uevent)
{
	short mvalo[2], dx, dy, domx, domy;
	int counter=0;

	if((block->direction & UI_TOP) || (block->direction & UI_DOWN)) return 0;
	if(uevent->event!= MOUSEX && uevent->event!= MOUSEY) return 0;
	
	/* calculate dominant direction */
	domx= ( -uevent->mval[0] + (block->maxx+block->minx)/2 );
	domy= ( -uevent->mval[1] + (block->maxy+block->miny)/2 );
	/* we need some accuracy */
	if( abs(domx)<4 ) return 0;
	
	uiGetMouse(mywinget(), mvalo);
	
	while(TRUE) {
		uiGetMouse(mywinget(), uevent->mval);
		
		/* check inside, if so return */
		if( block->minx <= uevent->mval[0] && block->maxx >= uevent->mval[0] ) {		
			if( block->miny <= uevent->mval[1] && block->maxy >= uevent->mval[1] ) {
				return 1;
			}
		}
		
		/* check direction */
		dx= uevent->mval[0] - mvalo[0];
		dy= uevent->mval[1] - mvalo[1];
				
		if( abs(dx)+abs(dy)>4 ) {  // threshold
			/* menu to right */
			if(domx>0) {
				int fac= (uevent->mval[0] - mvalo[0])*(mvalo[1] - (short)(block->maxy +20)) + (uevent->mval[1] - mvalo[1])*(-mvalo[0] + (short)block->minx);
				if( (fac>0)) {
					// printf("Left outside 1, Fac %d\n", fac); 
					return 0;
				}
				
				fac= (uevent->mval[0] - mvalo[0])*(mvalo[1] - (short)(block->miny-20)) + (uevent->mval[1] - mvalo[1])*(-mvalo[0] + (short)block->minx);
				if( (fac<0))  {
					//printf("Left outside 2, Fac %d\n", fac); 
					return 0;
				}
			}
			else {
				int fac= (uevent->mval[0] - mvalo[0])*(mvalo[1] - (short)(block->maxy+20)) + (uevent->mval[1] - mvalo[1])*(-mvalo[0] + (short)block->maxx);
				if( (fac<0)) {
					// printf("Left outside 1, Fac %d\n", fac); 
					return 0;
				}
				
				fac= (uevent->mval[0] - mvalo[0])*(mvalo[1] - (short)(block->miny-20)) + (uevent->mval[1] - mvalo[1])*(-mvalo[0] + (short)block->maxx);
				if( (fac>0))  {
					// printf("Left outside 2, Fac %d\n", fac); 
					return 0;
				}
			}
		}
		
		/* idle for this poor code */
		PIL_sleep_ms(10);
		counter++;
		if(counter > 100) {
			//printf("left because of timer (1 sec)\n");
			return 0;
		}
	}
	
	return 0;
}


static void ui_set_ftf_font(float aspect)
{

#ifdef INTERNATIONAL
	if(aspect<1.15) {
		FTF_SetFontSize('l');
	}
	else if(aspect<1.59) {
		FTF_SetFontSize('m');
	}
	else {
		FTF_SetFontSize('s');
	}
#endif
}

static void ui_but_next_edittext(uiBlock *block)
{
	uiBut *but, *actbut;

	for(actbut= block->buttons.first; actbut; actbut= actbut->next) {
		/* label and roundbox can overlap real buttons (backdrops...) */
		if(actbut->type!=LABEL && actbut->type!=ROUNDBOX)
			if(actbut->flag & UI_ACTIVE) break;
	}
	if(actbut) {
		/* ensure all buttons are cleared, label/roundbox overlap */
		for(but= block->buttons.first; but; but= but->next)
			but->flag &= ~(UI_ACTIVE|UI_SELECT);
		
		for(but= actbut->next; but; but= but->next) {
			if(ELEM5(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI)) {
				but->flag |= UI_ACTIVE;
				return;
			}
		}
		for(but= block->buttons.first; but!=actbut; but= but->next) {
			if(ELEM5(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI)) {
				but->flag |= UI_ACTIVE;
				return;
			}
		}
	}
}

static void ui_but_prev_edittext(uiBlock *block)
{
	uiBut *but, *actbut;
	
	for(actbut= block->buttons.first; actbut; actbut= actbut->next) {
		/* label and roundbox can overlap real buttons (backdrops...) */
		if(actbut->type!=LABEL && actbut->type!=ROUNDBOX)
			if(actbut->flag & UI_ACTIVE) break;
	}
	if(actbut) {
		/* ensure all buttons are cleared, label/roundbox overlap */
		for(but= block->buttons.first; but; but= but->next)
			but->flag &= ~(UI_ACTIVE|UI_SELECT);
		
		for(but= actbut->prev; but; but= but->prev) {
			if(ELEM5(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI)) {
				but->flag |= UI_ACTIVE;
				return;
			}
		}
		for(but= block->buttons.last; but!=actbut; but= but->prev) {
			if(ELEM5(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI)) {
				but->flag |= UI_ACTIVE;
				return;
			}
		}
	}
}

/* ******************************************************* */

/* nasty but safe way to store screendump rect */
static int scr_x=0, scr_y=0, scr_sizex=0, scr_sizey=0;

static void ui_set_screendump_bbox(uiBlock *block)
{
	if(block) {
		scr_x= block->minx;
		scr_y= block->miny;
		scr_sizex= block->maxx - block->minx;
		scr_sizey= block->maxy - block->miny;
	}
	else {
		scr_sizex= scr_sizey= 0;
	}
}

/* used for making screenshots for menus, called in screendump.c */
int uiIsMenu(int *x, int *y, int *sizex, int *sizey)
{
	if(scr_sizex!=0 && scr_sizey!=0) {
		*x= scr_x;
		*y= scr_y;
		*sizex= scr_sizex;
		*sizey= scr_sizey;
		return 1;
	}
	
	return 0;
}
/* ******************************************************* */

/* return: 
 * UI_NOTHING	pass event to other ui's
 * UI_CONT		don't pass event to other ui's
 * UI_RETURN	something happened, return, swallow event
 */
static int ui_do_block(uiBlock *block, uiEvent *uevent, int movemouse_quit)
{
	uiBut *but, *bt;
	int butevent, event, retval=UI_NOTHING, count, act=0;
	int inside= 0, active=0;
	
	if(block->win != mywinget()) return UI_NOTHING;

	/* filter some unwanted events */
	/* btw: we allow event==0 for first time in menus, draws the hilited item */
	if(uevent==0 || uevent->event==LEFTSHIFTKEY || uevent->event==RIGHTSHIFTKEY) return UI_NOTHING;
	if(uevent->event==UI_BUT_EVENT) return UI_NOTHING;
	
	if(block->flag & UI_BLOCK_ENTER_OK) {
		if((uevent->event==RETKEY || uevent->event==PADENTER) && uevent->val) {
			// printf("qual: %d %d %d\n", uevent->qual, get_qual(), G.qual);
			if ((G.qual & LR_SHIFTKEY) == 0) {
				return UI_RETURN_OK;
			}
		}
	}		

	ui_set_ftf_font(block->aspect);	// sets just a pointer in ftf lib... the button dont have ftf handles
	ui_set_screendump_bbox(block);
	
	// added this for panels in windows with buttons... 
	// maybe speed optimize should require test
	if((block->flag & UI_BLOCK_LOOP)==0) {
		glMatrixMode(GL_PROJECTION);
		bwin_load_winmatrix(block->win, block->winmat);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}

	Mat4CpyMat4(UIwinmat, block->winmat);
	uiPanelPush(block); // push matrix; no return without pop!

	uiGetMouse(mywinget(), uevent->mval);	/* transformed mouseco */

	/* check boundbox and panel events */
	if( block->minx <= uevent->mval[0] && block->maxx >= uevent->mval[0] ) {
		
		// inside block
		if( block->miny <= uevent->mval[1] && block->maxy >= uevent->mval[1] ) inside= INSIDE_BLOCK;
		
		if(block->panel && block->panel->paneltab==NULL) {
			
			/* clicked at panel header? */
			if( block->panel->flag & PNL_CLOSEDX) {
				if(block->minx <= uevent->mval[0] && block->minx+PNL_HEADER >= uevent->mval[0]) 
					inside= INSIDE_PANEL_HEADER;
			}
			else if( (block->maxy <= uevent->mval[1]) && (block->maxy+PNL_HEADER >= uevent->mval[1]) ) {
				inside= INSIDE_PANEL_HEADER;
			}
			else if( block->panel->control & UI_PNL_SCALE) {
				if( (block->maxx-PNL_HEADER <= uevent->mval[0]))
					if( (block->miny+PNL_HEADER >= uevent->mval[1]) && inside )
						inside= INSIDE_PANEL_SCALE;
			}
			
			if (inside) {	// this stuff should move to do_panel
				
				if(uevent->event==LEFTMOUSE) {
					if(ELEM(inside, INSIDE_PANEL_HEADER, INSIDE_PANEL_SCALE)) {
						uiPanelPop(block); 	// pop matrix; no return without pop!
						if(inside==INSIDE_PANEL_HEADER)
							ui_do_panel(block, uevent);
						else
							ui_scale_panel(block);
						return UI_EXIT_LOOP;	// exit loops because of moving panels
					}
				}
				else if(uevent->event==ESCKEY) {
					if(block->handler) {
						rem_blockhandler(curarea, block->handler);
						addqueue(curarea->win, REDRAW, 1);
					}
				}
				else if(uevent->event==PADPLUSKEY || uevent->event==PADMINUS) {
					int zoom=0;
				
					/* if panel is closed, only zoom if mouse is over the header */
					if ((block->panel->flag & PNL_CLOSEDX) || (block->panel->flag & PNL_CLOSEDY)) {
						if (inside == INSIDE_PANEL_HEADER)
							zoom=1;
					} else if (inside >= INSIDE_BLOCK)
						zoom=1;

					if(zoom) {
						SpaceLink *sl= curarea->spacedata.first;
						if(curarea->spacetype!=SPACE_BUTS) {
							if(!(block->panel->control & UI_PNL_SCALE)) {
								if(uevent->event==PADPLUSKEY) sl->blockscale+= 0.1;
								else sl->blockscale-= 0.1;
								CLAMP(sl->blockscale, 0.6, 1.0);
								addqueue(block->winq, REDRAW, 1);
								retval= UI_RETURN_OK;
							}						
						}
					}
					
				}
			}
		}
	}
	
	/* inside menus, scrollwheel acts as arrow */
	if(block->flag & UI_BLOCK_LOOP) {
		if(uevent->event==WHEELUPMOUSE) uevent->event= UPARROWKEY;
		if(uevent->event==WHEELDOWNMOUSE) uevent->event= DOWNARROWKEY;
	}
	
	switch(uevent->event) {
	case LEFTARROWKEY:	/* closing sublevels of pulldowns */
		if(uevent->val && (block->flag & UI_BLOCK_LOOP) && block->parent) {
			return UI_RETURN_OUT;
		}
		break;
		
	case RIGHTARROWKEY:	/* opening sublevels of pulldowns */
		if(uevent->val && (block->flag & UI_BLOCK_LOOP)) {
			for(but= block->buttons.first; but; but= but->next) {
				if(but->flag & UI_ACTIVE) {
					if(but->type==BLOCK) {
						but->flag &= ~UI_MOUSE_OVER;
						uevent->event= BUT_ACTIVATE;
					}
					break;
				}
			}
			if(but==NULL) {	/* no item active, we make first active */
				if(block->direction & UI_TOP) but= ui_but_last(block);
				else but= ui_but_first(block);
				if(but) {
					but->flag |= UI_ACTIVE;
					ui_draw_but(but);
				}
			}
		}
		break;
	
	case UPARROWKEY:
	case DOWNARROWKEY:
		if(inside || (block->flag & UI_BLOCK_LOOP)) {
			/* arrowkeys: only handle for block_loop blocks */
			event= 0;
			if(block->flag & UI_BLOCK_LOOP)
				event= uevent->event;
			if(event && uevent->val) {
	
				for(but= block->buttons.first; but; but= but->next) {
					but->flag &= ~UI_MOUSE_OVER;
		
					if(but->flag & UI_ACTIVE) {
						but->flag &= ~UI_ACTIVE;
						ui_draw_but(but);
	
						if(event==UPARROWKEY) {
							if(block->direction & UI_TOP) bt= ui_but_next(but);
							else bt= ui_but_prev(but);
						}
						else {
							if(block->direction & UI_TOP) bt= ui_but_prev(but);
							else bt= ui_but_next(but);
						}
						
						if(bt) {
							bt->flag |= UI_ACTIVE;
							ui_draw_but(bt);
							break;
						}
					}
				}
	
				/* nothing done */
				if(but==NULL) {
					if(event==UPARROWKEY) {
						if(block->direction & UI_TOP) but= ui_but_first(block);
						else but= ui_but_last(block);
					}
					else {
						if(block->direction & UI_TOP) but= ui_but_last(block);
						else but= ui_but_first(block);
					}
					if(but) {
						but->flag |= UI_ACTIVE;
						ui_draw_but(but);
					}
				}
				retval= UI_CONT;
			}
		}
		break;
	
	case ONEKEY: 	case PAD1: 
		act= 1;
	case TWOKEY: 	case PAD2: 
		if(act==0) act= 2;
	case THREEKEY: 	case PAD3: 
		if(act==0) act= 3;
	case FOURKEY: 	case PAD4: 
		if(act==0) act= 4;
	case FIVEKEY: 	case PAD5: 
		if(act==0) act= 5;
	case SIXKEY: 	case PAD6: 
		if(act==0) act= 6;
	case SEVENKEY: 	case PAD7: 
		if(act==0) act= 7;
	case EIGHTKEY: 	case PAD8: 
		if(act==0) act= 8;
	case NINEKEY: 	case PAD9: 
		if(act==0) act= 9;
	case ZEROKEY: 	case PAD0: 
		if(act==0) act= 10;
	
		if( block->flag & UI_BLOCK_NUMSELECT ) {
			
			if(get_qual() & LR_ALTKEY) act+= 10;
			
			count= 0;
			for(but= block->buttons.first; but; but= but->next) {
				int doit= 0;
				
				if(but->type!=LABEL && but->type!=SEPR) count++;
				/* exception for menus like layer buts, with button aligning they're not drawn in order */
				if(but->type==TOGR) {
					if(but->bitnr==act-1) doit= 1;
				} else if(count==act) doit=1;
				
				if(doit) {
					but->flag |= UI_ACTIVE;
					if(uevent->val==1) ui_draw_but(but);
					else if(block->flag & UI_BLOCK_RET_1) { /* to make UI_BLOCK_RET_1 working */
						uevent->event= RETKEY;
						uevent->val= 1;			
						//addqueue(block->winq, RIGHTARROWKEY, 1); (why! (ton))
					}
					else { 
						uevent->event= LEFTMOUSE;	/* to make sure the button is handled further on */
						uevent->val= 1;			
					}
				}
				else if(but->flag & UI_ACTIVE) {
					but->flag &= ~UI_ACTIVE;
					ui_draw_but(but);
				}
			}
		}
		break;
	case BUT_NEXT:
		ui_but_next_edittext(block);
		break;
	case BUT_PREV:
		ui_but_prev_edittext(block);
		break;
	case BUT_ACTIVATE:
		for(but= block->buttons.first; but; but= but->next) {
			if(but->retval==uevent->val) but->flag |= UI_ACTIVE;
		}
		break;
	case VKEY:
	case CKEY:
		if(uevent->val && (uevent->qual & (LR_CTRLKEY|LR_COMMANDKEY))) {
			for(but= block->buttons.first; but; but= but->next) {
				if(but->type!=LABEL && but->type!=ROUNDBOX) {
					if(but->flag & UI_ACTIVE) {
						int doit=0;
						
						if(uevent->event==VKEY) doit= ui_but_copy_paste(but, 'v');
						else ui_but_copy_paste(but, 'c');
						
						if(doit) {
							ui_draw_but(but);
							
							if(but->retval) addqueue(block->winq, UI_BUT_EVENT, (short)but->retval);
							if((but->type==NUMSLI && but->a1) || (but->type==COL)) addqueue(block->winq, REDRAW, 1);	// col button update

							BIF_undo_push(but->str);
						}
						// but we do return, to prevent passing event through other queues */
						if( (block->flag & UI_BLOCK_LOOP) && but->type==BLOCK);
						else if(but->retval) retval= UI_RETURN_OK;
						break;
					}
				}
			}
		}
		break;


#ifdef INTERNATIONAL
	//HACK to let the chartab button react to the mousewheel and PGUP/PGDN keys
	case WHEELUPMOUSE:
	case PAGEUPKEY:
		for(but= block->buttons.first; but; but= but->next)
		{
			if(but->type == CHARTAB && (but->flag & UI_MOUSE_OVER))
			{
				G.charstart = G.charstart - (12*6);
				if(G.charstart < 0)
					G.charstart = 0;
				if(G.charstart < G.charmin)
					G.charstart = G.charmin;
				ui_draw_but(but);

				//Really nasty... to update the num button from the same butblock
				for(bt= block->buttons.first; bt; bt= bt->next)
				{
					if(ELEM(bt->type, NUM, NUMABS)) {
						ui_check_but(bt);
						ui_draw_but(bt);
					}
				}
				retval=UI_CONT;
				break;
			}
		}
		break;
		
	case WHEELDOWNMOUSE:
	case PAGEDOWNKEY:
		for(but= block->buttons.first; but; but= but->next)
		{
			if(but->type == CHARTAB && (but->flag & UI_MOUSE_OVER))
			{
				G.charstart = G.charstart + (12*6);
				if(G.charstart > (0xffff - 12*6))
					G.charstart = 0xffff - (12*6);
				if(G.charstart > G.charmax - 12*6)
					G.charstart = G.charmax - 12*6;
				ui_draw_but(but);

				for(bt= block->buttons.first; bt; bt= bt->next)
				{
					if(ELEM(bt->type, NUM, NUMABS)) {
						ui_check_but(bt);
						ui_draw_but(bt);
					}
				}
				
				but->flag |= UI_ACTIVE;
				retval=UI_RETURN_OK;
				break;
			}
		}
		break;
#endif
				
	case PADENTER:
	case RETKEY:	// prevent treating this as mousemove. for example when you enter at popup 
		if(block->flag & UI_BLOCK_LOOP) break;
		
	default:

		for(but= block->buttons.first; but; but= but->next) {
			
			// active flag clear, it can have been set with number keys or arrows, prevents next loop from wrong selection on click
			if(uevent->event==LEFTMOUSE) but->flag &= ~UI_ACTIVE;
			
			but->flag &= ~UI_MOUSE_OVER;
			
			/* check boundbox */
			if (uibut_contains_pt(but, uevent->mval)) {
				but->flag |= UI_MOUSE_OVER;
				UIbuttip= but;
			}
			/* hilite case 1 */
			if(but->flag & UI_MOUSE_OVER) {
				if( (but->flag & UI_ACTIVE)==0) {
					but->flag |= UI_ACTIVE;
					if(but->type != LABEL && (but->flag & UI_NO_HILITE)==0) ui_draw_but(but);
				}
			}
			/* hilite case 2 */
			if(but->flag & UI_ACTIVE) {
				if( (but->flag & UI_MOUSE_OVER)==0) {
					/* we dont clear active flag until mouse move, for Menu buttons to remain showing active item when opened */
					if (uevent->event==MOUSEY) {
						but->flag &= ~UI_ACTIVE;
						if(but->type != LABEL && (but->flag & UI_NO_HILITE)==0) ui_draw_but(but);
					}
				}
				else if(but->type==BLOCK || but->type==MENU || but->type==PULLDOWN || but->type==ICONTEXTROW) {	// automatic opens block button (pulldown)
					int time;
					if(uevent->event!=LEFTMOUSE ) {
						if(block->auto_open==2) time= 1;	// test for toolbox
						else if(block->auto_open) time= 5*U.menuthreshold2;
						else if(U.uiflag & USER_MENUOPENAUTO) time= 5*U.menuthreshold1;
						else time= -1;

						for (; time>0; time--) {
							if (qtest()) break;
							else PIL_sleep_ms(20);
						}

						if(time==0) {
							uevent->val= 1;	// otherwise buttons dont react
							ui_do_button(block, but, uevent);
						}
					}
				}
				if(but->flag & UI_ACTIVE) active= 1;
			}
		}
		
		/* if there are no active buttons... otherwise clear lines */
		if(active) ui_do_active_linklines(block, 0);
		else ui_do_active_linklines(block, uevent->mval);			

	}

	/* middlemouse exception, not for regular blocks */
	if( (block->flag & UI_BLOCK_LOOP) && uevent->event==MIDDLEMOUSE) uevent->event= LEFTMOUSE;

	/* the final dobutton */
	for(but= block->buttons.first; but; but= but->next) {
		if(but->flag & UI_ACTIVE) {
			
			/* UI_BLOCK_RET_1: not return when val==0 */
			
			if(uevent->val || (block->flag & UI_BLOCK_RET_1)==0) {
				if ELEM6(uevent->event, LEFTMOUSE, PADENTER, RETKEY, BUT_ACTIVATE, BUT_NEXT, BUT_PREV) {
					/* when mouse outside, don't do button */
					if(inside || uevent->event!=LEFTMOUSE) {						
						if ELEM(uevent->event, BUT_NEXT, BUT_PREV) {
							butevent= ui_act_as_text_but(but);
							uibut_do_func(but);
						}
						else
							butevent= ui_do_button(block, but, uevent);
						
						/* add undo pushes if... */
						if( !(block->flag & UI_BLOCK_LOOP)) {
							if(!G.obedit) {
								if ELEM5(but->type, BLOCK, BUT, LABEL, PULLDOWN, ROUNDBOX); 
								else {
									/* define which string to use for undo */
									if ELEM(but->type, LINK, INLINK) screen_delayed_undo_push("Add button link");
									else if ELEM(but->type, MENU, ICONTEXTROW) screen_delayed_undo_push(but->drawstr);
									else if(but->drawstr[0]) screen_delayed_undo_push(but->drawstr);
									else screen_delayed_undo_push(but->tip);
								}
							}
						}
						
						if(butevent) addqueue(block->winq, UI_BUT_EVENT, (short)butevent);
						
						/* i doubt about the next line! */
						/* if(but->func) mywinset(block->win); */
						
						if( (block->flag & UI_BLOCK_LOOP) && but->type==BLOCK);
						else	
							if (butevent) retval= UI_RETURN_OK;
					}
				}
			}
		}
	}

	/* flush to frontbuffer */
	if((block->flag & UI_BLOCK_LOOP)==0) { // no loop, might need total flush in uidoblocks()
		ui_block_flush_back(block);
	}

	uiPanelPop(block); // pop matrix; no return without pop!


	/* the linkines... why not make buttons from it? Speed? Memory? */
	if(uevent->val && (uevent->event==XKEY || uevent->event==DELKEY)) 
		ui_delete_active_linkline(block);

	/* here we check return conditions for menus */
	if(block->flag & UI_BLOCK_LOOP) {

		if(inside==0 && uevent->val==1) {
			if ELEM3(uevent->event, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE) {
				if(BLI_in_rctf(&block->parentrct, (float)uevent->mval[0], (float)uevent->mval[1]));
				else return UI_RETURN_OUT;
			}
		}

		if(uevent->event==ESCKEY && uevent->val==1) return UI_RETURN_CANCEL;

		if((uevent->event==RETKEY || uevent->event==PADENTER) && uevent->val==1) return UI_RETURN_OK;
		
		/* check outside */
		if(inside==0 && movemouse_quit) {
			uiBlock *tblock= NULL;
			
			/* check for all parent rects, enables arrowkeys to be used */
			if(uevent->event!=MOUSEX && uevent->event!=MOUSEY) {
				for(tblock=block->parent; tblock; tblock= tblock->parent) {
					if( BLI_in_rctf(&tblock->parentrct, (float)uevent->mval[0], (float)uevent->mval[1]))
						break;
					else if( BLI_in_rctf(&tblock->safety, (float)uevent->mval[0], (float)uevent->mval[1]))
						break;
				}
			}			
			/* strict check, and include the parent rect */
			if(tblock);
			else if( BLI_in_rctf(&block->parentrct, (float)uevent->mval[0], (float)uevent->mval[1]));
			else if( ui_mouse_motion_towards_block(block, uevent));
			else if( BLI_in_rctf(&block->safety, (float)uevent->mval[0], (float)uevent->mval[1]));
			else return UI_RETURN_OUT;
		}
	}

	return retval;
}

static uiOverDraw *ui_draw_but_tip(uiBut *but)
{
	uiOverDraw *od;
	float x1, x2, y1, y2;
	rctf tip_bbox;

	BIF_GetBoundingBox(but->font, but->tip, (U.transopts & USER_TR_TOOLTIPS), &tip_bbox);
	
	x1= (but->x1+but->x2)/2;
	x2= x1+but->aspect*((tip_bbox.xmax-tip_bbox.xmin) + 8);
	y2= but->y1-10;
	y1= y2-but->aspect*((tip_bbox.ymax+(tip_bbox.ymax-tip_bbox.ymin)));


	/* for pulldown menus it doesnt work */
	if(mywinget()==G.curscreen->mainwin);
	else {
		ui_graphics_to_window(mywinget(), &x1, &y1);
		ui_graphics_to_window(mywinget(), &x2, &y2);
	}
	
	if(x2 > G.curscreen->sizex) {
		x1 -= x2-G.curscreen->sizex;
		x2= G.curscreen->sizex;
	}
	if(y1 < 0) {
		y1 += 36;
		y2 += 36;
	}

	od= ui_begin_overdraw((int)(x1-1), (int)(y1-2), (int)(x2+4), (int)(y2+4));

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glColor4ub(0, 0, 0, 20);
	
	gl_round_box(GL_POLYGON, x1+3, y1-1, x2+1, y2-2, 2.0);
	gl_round_box(GL_POLYGON, x1+3, y1-2, x2+2, y2-2, 3.0);
	
	glColor4ub(0, 0, 0, 8);
	
	gl_round_box(GL_POLYGON, x1+3, y1-3, x2+3, y2-3, 4.0);
	gl_round_box(GL_POLYGON, x1+3, y1-4, x2+4, y2-3, 5.0);

	glDisable(GL_BLEND);
	
	glColor3ub(0xFF, 0xFF, 0xDD);
	glRectf(x1, y1, x2, y2);
	
	glColor3ub(0,0,0);
	/* set the position for drawing text +4 in from the left edge, and leaving an equal gap between the top of the background box
	 * and the top of the string's tip_bbox, and the bottom of the background box, and the bottom of the string's tip_bbox
	 */
	ui_rasterpos_safe(x1+4, ((y2-tip_bbox.ymax)+(y1+tip_bbox.ymin))/2 - tip_bbox.ymin, but->aspect);
	BIF_SetScale(1.0);

	BIF_DrawString(but->font, but->tip, (U.transopts & USER_TR_TOOLTIPS));
	
	ui_flush_overdraw(od);		/* to show it in the frontbuffer */
	return od;
}

/* inside this function no global UIbuttip... qread is not safe */
static void ui_do_but_tip(uiBut *buttip)
{
	uiOverDraw *od;
	int time;
	
	if (buttip && buttip->tip && buttip->tip[0]) {
			/* Pause for a moment to see if we
			 * should really display the tip
			 * or if the user will keep moving
			 * the pointer.
			 */
		for (time= 0; time<25; time++) {
			if (anyqtest())
				return;
			else
				PIL_sleep_ms(20);
		}
			
			/* Display the tip, and keep it displayed
			 * as long as the mouse remains on top
			 * of the button that owns it.
			 */
		Mat4CpyMat4(UIwinmat, buttip->block->winmat);	// get rid of uiwinmat once...
		uiPanelPush(buttip->block); // panel matrix
		od= ui_draw_but_tip(buttip);
		
		if(od) {
			while (1) {
				char ascii;
				short val;
				unsigned short evt= extern_qread_ext(&val, &ascii);

				if (evt==MOUSEX || evt==MOUSEY) {
					short mouse[2];
					uiGetMouse(od->oldwin, mouse);
					
					if (!uibut_contains_pt(buttip, mouse))
						break;
				} else {
					mainqpushback(evt, val, ascii);
					break;
				}
			}
			
			ui_end_overdraw(od);
		}
		
		uiPanelPop(buttip->block); // panel matrix
		/* still the evil global.... */
		UIbuttip= NULL;
	}
}

/* returns UI_NOTHING, if nothing happened */
int uiDoBlocks(ListBase *lb, int event, int movemouse_quit)
{
	/* return when:  firstblock != BLOCK_LOOP
	 * 
	 * 'cont' is used to make sure you can press another button while a looping menu
	 * is active. otherwise you have to press twice...
	 */

	uiBlock *block, *first;
	uiEvent uevent;
	int retval= UI_NOTHING, cont= 1;

	if(lb->first==0) return UI_NOTHING;
	
	/* for every pixel both x and y events are generated, overloads the system! */
	if(event==MOUSEX) return UI_NOTHING;
		
	UIbuttip= NULL;
	UIafterfunc_butm= NULL;	/* to prevent infinite loops, this shouldnt be a global! */
	UIafterfunc_but= NULL;	/* to prevent infinite loops, this shouldnt be a global! */
	UIafterfunc_arg1= UIafterfunc_arg2= NULL;
	
	uevent.qual= G.qual;
	uevent.event= event;
	uevent.val= 1;

	/* this is a caching mechanism, to prevent too many calls to glFrontBuffer and glFlush, which slows down interface */
	block= lb->first;
	while(block) {
		ui_block_set_flush(block, NULL); // clears all flushing info
		block= block->next;
	}

	/* main loop, needed when you click outside a looping block (menu) then it uses that
	   event to immediately evaluate the other uiBlocks again.  */
	while(cont) {
	
		/* first loop, for the normal blocks */
		block= lb->first;
		while(block) {

			/* for pupmenus, the bgnpupdraw sets (and later restores) the active
			   window. Then mousecoords get transformed OK.
			   It looks double... but a call to ui_do_block otherwise doesnt get handled properly
			 */
			if(block->flag & UI_BLOCK_REDRAW) {
				if( block->flag & UI_BLOCK_LOOP) {
					block->overdraw= ui_begin_overdraw((int)block->minx-1, (int)block->miny-10, (int)block->maxx+10, (int)block->maxy+1);
				}
				block->in_use= 1;	// is always a menu
				uiDrawBlock(block);
				block->flag &= ~UI_BLOCK_REDRAW;
			}
			
			block->in_use= 1; // bit awkward, but now we can detect if frontbuf flush should be set
			retval |= ui_do_block(block, &uevent, movemouse_quit); /* we 'or' because 2nd loop can return to here, and we we want 'out' to return */
			block->in_use= 0;
			if(retval & UI_EXIT_LOOP) break;
			
			/* now a new block could be created for menus, this is 
			   inserted in the beginning of a list */
			
			/* is there a flush cached? */
			if(block->needflush) {
				ui_block_flush_overdraw(block);
				block->needflush= 0;
			}
			
			/* to make sure the matrix of the panel works for menus too */
			if(retval==UI_CONT || (retval & UI_RETURN)) break;
			first= lb->first; if(first->flag & UI_BLOCK_LOOP) break;
			
			block= block->next;
		}
	
		/* second loop, for menus (looping blocks). works for sub->menus too */
		block= lb->first;
		if(block==NULL || (block->flag & UI_BLOCK_LOOP)==0) cont= 0;
		
		while( (block= lb->first) && (block->flag & UI_BLOCK_LOOP)) {
			if(block->auto_open==0) block->auto_open= 1;
			
			/* this here, for menu buts */
			if(block->flag & UI_BLOCK_REDRAW) {

				if( block->flag & UI_BLOCK_LOOP) {
					block->overdraw= ui_begin_overdraw((int)block->minx-1, (int)block->miny-6, (int)block->maxx+6, (int)block->maxy+1);
				}
				uiDrawBlock(block);
				block->flag &= ~UI_BLOCK_REDRAW;
				ui_flush_overdraw(block->overdraw);
				block->needflush= 0;
			}
			
			uevent.event= extern_qread(&uevent.val);
			uevent.qual= G.qual;

			if(uevent.event) {
				block->in_use= 1; // bit awkward, but now we can detect if frontbuf flush should be set
				retval= ui_do_block(block, &uevent, movemouse_quit);
				block->in_use= 0;
			
				if(block->needflush) { // flush (old menu) now, maybe new menu was opened
					ui_block_flush_overdraw(block);
					block->needflush= 0;
				}

				if(retval & UI_RETURN) {
					ui_end_overdraw(block->overdraw);
					BLI_remlink(lb, block);
					uiFreeBlock(block);
				}
				if(retval & (UI_RETURN_OK|UI_RETURN_CANCEL)) {
					/* free other menus */
					while( (block= lb->first) && (block->flag & UI_BLOCK_LOOP)) {
						ui_end_overdraw(block->overdraw);
						BLI_remlink(lb, block);
						uiFreeBlock(block);
					}
				}
			}
			
			/* tooltip */	
			if(retval==UI_NOTHING && (uevent.event==MOUSEX || uevent.event==MOUSEY)) {
				if(U.flag & USER_TOOLTIPS) ui_do_but_tip(UIbuttip);
			}
		}
		
		/* else it does the first part of this loop again, maybe another menu needs to be opened */
		if(retval==UI_CONT || (retval & UI_RETURN_OK)) cont= 0;
	}
	
	/* clears screendump boundbox, call before afterfunc! */
	ui_set_screendump_bbox(NULL);
	
	/* afterfunc is used for fileloading too, so after this call, the blocks pointers are invalid */
	if(retval & UI_RETURN_OK) {
		if(UIafterfunc_butm) {
			mywinset(curarea->win);
			UIafterfunc_butm(UIafterfunc_arg1, UIafterval);
			UIafterfunc_butm= NULL;
		}
		if(UIafterfunc_but) {
			mywinset(curarea->win);
			UIafterfunc_but(UIafterfunc_arg1, UIafterfunc_arg2);
			UIafterfunc_but= NULL;
		}
	}
	
	/* tooltip */	
	if(retval==UI_NOTHING && (uevent.event==MOUSEX || uevent.event==MOUSEY)) {
		if(U.flag & USER_TOOLTIPS) ui_do_but_tip(UIbuttip);
	}
	
	return retval;
}

/* ************** DATA *************** */

/* for buttons pointing to color for example */
void ui_get_but_vectorf(uiBut *but, float *vec)
{
	void *poin;

	poin= but->poin;

	if( but->pointype == CHA ) {
		char *cp= (char *)poin;
		vec[0]= ((float)cp[0])/255.0;
		vec[1]= ((float)cp[1])/255.0;
		vec[2]= ((float)cp[2])/255.0;
	}
	else if( but->pointype == FLO ) {
		float *fp= (float *)poin;
		VECCOPY(vec, fp);
	}
}
/* for buttons pointing to color for example */
void ui_set_but_vectorf(uiBut *but, float *vec)
{
	void *poin;

	poin= but->poin;

	if( but->pointype == CHA ) {
		char *cp= (char *)poin;
		cp[0]= (char)(0.5 +vec[0]*255.0);
		cp[1]= (char)(0.5 +vec[1]*255.0);
		cp[2]= (char)(0.5 +vec[2]*255.0);
	}
	else if( but->pointype == FLO ) {
		float *fp= (float *)poin;
		VECCOPY(fp, vec);
	}
}

double ui_get_but_val(uiBut *but)
{
	void *poin;
	double value = 0.0;
	
	if(but->poin==NULL) return 0.0;
	poin= but->poin;

	if(but->type== HSVSLI) {
		float h, s, v, *fp= (float *) poin;
		
		rgb_to_hsv(fp[0], fp[1], fp[2], &h, &s, &v);

		switch(but->str[0]) {
			case 'H': value= h; break;
			case 'S': value= s; break;
			case 'V': value= v; break;
		}
		
	} 
	else if( but->pointype == CHA ) {
		value= *(char *)poin;
	}
	else if( but->pointype == SHO ) {
		value= *(short *)poin;
	} 
	else if( but->pointype == INT ) {
		value= *(int *)poin;		
	} 
	else if( but->pointype == FLO ) {
		value= *(float *)poin;
	}
    
	return value;
}

static void ui_set_but_val(uiBut *but, double value)
{
	void *poin;

	if(but->pointype==0) return;
	poin= but->poin;
	
	/* value is a hsv value: convert to rgb */
	if( but->type==HSVSLI ) {
		float h, s, v, *fp= (float *)but->poin;
		
		rgb_to_hsv(fp[0], fp[1], fp[2], &h, &s, &v);
		
		switch(but->str[0]) {
		case 'H': h= value; break;
		case 'S': s= value; break;
		case 'V': v= value; break;
		}
		
		hsv_to_rgb(h, s, v, fp, fp+1, fp+2);
		
	}
	else if( but->pointype==CHA )
		*((char *)poin)= (char)floor(value+0.5);
	else if( but->pointype==SHO ) {
		/* gcc 3.2.1 seems to have problems 
		 * casting a double like 32772.0 to
		 * a short so we cast to an int, then 
		 to a short */
		int gcckludge;
		gcckludge = (int) floor(value+0.5);
		*((short *)poin)= (short) gcckludge;
	}
	else if( but->pointype==INT )
		*((int *)poin)= (int)floor(value+0.5);
	else if( but->pointype==FLO ) {
		float fval= (float)value;
		if(fval>= -0.00001f && fval<= 0.00001f) fval= 0.0f;	/* prevent negative zero */
		*((float *)poin)= fval;
	}
	
	/* update select flag */
	ui_is_but_sel(but);

}

void uiSetCurFont(uiBlock *block, int index)
{
	
	ui_set_ftf_font(block->aspect);
	
	if(block->aspect<0.60) {
		block->curfont= UIfont[index].xl;
	}
	else if(block->aspect<1.15) {
		block->curfont= UIfont[index].large;
	}
	else if(block->aspect<1.59) {
		block->curfont= UIfont[index].medium;		
	}
	else {
		block->curfont= UIfont[index].small;		
	}

	if(block->curfont==NULL) block->curfont= UIfont[index].large;	
	if(block->curfont==NULL) block->curfont= UIfont[index].medium;	
	if(block->curfont==NULL) printf("error block no font %s\n", block->name);
	
}

/* called by node editor */
void *uiSetCurFont_ext(float aspect)
{
	void *curfont;
	
	ui_set_ftf_font(aspect);
	
	if(aspect<0.60) {
		curfont= UIfont[0].xl;
	}
	else if(aspect<1.15) {
		curfont= UIfont[0].large;
	}
	else if(aspect<1.59) {
		curfont= UIfont[0].medium;		
	}
	else {
		curfont= UIfont[0].small;		
	}
	
	if(curfont==NULL) curfont= UIfont[0].large;	
	if(curfont==NULL) curfont= UIfont[0].medium;	
	
	return curfont;
}

void uiDefFont(unsigned int index, void *xl, void *large, void *medium, void *small)
{
	if(index>=UI_ARRAY) return;
	
	UIfont[index].xl= xl;
	UIfont[index].large= large;
	UIfont[index].medium= medium;
	UIfont[index].small= small;
}

static void ui_free_link(uiLink *link)
{
	if(link) {	
		BLI_freelistN(&link->lines);
		MEM_freeN(link);
	}
}

static void ui_free_but(uiBut *but)
{
	if(but->str && but->str != but->strdata) MEM_freeN(but->str);
	ui_free_link(but->link);

	MEM_freeN(but);
}

void uiFreeBlock(uiBlock *block)
{
	uiBut *but;

	if(block->flag & UI_BLOCK_BUSY) printf("attempt to free busy buttonblock: %p\n", block);	

	while( (but= block->buttons.first) ) {
		BLI_remlink(&block->buttons, but);	
		ui_free_but(but);
	}

	if(block->panel) block->panel->active= 0;

	
	MEM_freeN(block);
	UIbuttip= NULL;
}

void uiFreeBlocks(ListBase *lb)
{
	uiBlock *block;
	
	while( (block= lb->first) ) {
		BLI_remlink(lb, block);
		uiFreeBlock(block);
	}
}

void uiFreeBlocksWin(ListBase *lb, int win)
{
	uiBlock *block, *blockn;
	
	block= lb->first;
	while(block) {
		blockn= block->next;
		if(block->win==win) {
			BLI_remlink(lb, block);
			uiFreeBlock(block);
		}
		block= blockn;
	}
}

uiBlock *uiNewBlock(ListBase *lb, char *name, short dt, short font, short win)
{
	uiBlock *block;
	
	/* each listbase only has one block with this name */
	if(lb) {
		for (block= lb->first; block; block= block->next)
			if (BLI_streq(block->name, name))
				break;
		if (block) {
			BLI_remlink(lb, block);
			uiFreeBlock(block);
		}
	}
	
	block= MEM_callocN(sizeof(uiBlock), "uiBlock");
	if(lb) BLI_addhead(lb, block);		/* at the beginning of the list! for dynamical menus/blocks */

	strcpy(block->name, name);
	/* draw win */
	block->win= win;
	/* window where queue event should be added, pretty weak this way!
	   this is because the 'mainwin' pup menu's */
	block->winq= mywinget();
	block->dt= dt;
	block->themecol= TH_AUTO;
	
		/* aspect */
	bwin_getsinglematrix(win, block->winmat);

	if (win==G.curscreen->mainwin) {
		block->aspect= 1.0;
		block->auto_open= 2;
	} else {
		int getsizex, getsizey;

		bwin_getsize(win, &getsizex, &getsizey);
		block->aspect= 2.0/( (getsizex)*block->winmat[0][0]);
	}

	uiSetCurFont(block, font);

	UIbuttip= NULL;	
	UIlock= 0;

	return block;
}

uiBlock *uiGetBlock(char *name, ScrArea *sa)
{
	uiBlock *block= sa->uiblocks.first;
	
	while(block) {
		if( strcmp(name, block->name)==0 ) return block;
		block= block->next;
	}
	
	return NULL;
}

void ui_check_but(uiBut *but)
{
	/* if something changed in the button */
	ID *id;
	double value;
	float okwidth;
	int transopts= (U.transopts & USER_TR_BUTTONS);
	short pos;
	
	ui_is_but_sel(but);
	
	if(but->type==TEX || but->type==IDPOIN) transopts= 0;

	/* test for min and max, icon sliders, etc */
	switch( but->type ) {
		case NUM:
		case SLI:
		case SCROLL:
		case NUMSLI:
		case HSVSLI:
			value= ui_get_but_val(but);
			if(value < but->min) value= but->min;
			if(value > but->max) value= but->max;
			ui_set_but_val(but, value);
			break;
			
		case NUMABS:
			value= fabs( ui_get_but_val(but) );
			if(value < but->min) value= but->min;
			if(value > but->max) value= but->max;
			ui_set_but_val(but, value);
			break;
			
		case ICONTOG: 
		case ICONTOGN:
			if(but->flag & UI_SELECT) but->iconadd= 1;
			else but->iconadd= 0;
			break;
			
		case ICONROW:
			value= ui_get_but_val(but);
			but->iconadd= (int)value- (int)(but->min);
			break;
			
		case ICONTEXTROW:
			value= ui_get_but_val(but);
			but->iconadd= (int)value- (int)(but->min);
			break;
	}
	
	
	/* safety is 4 to enable small number buttons (like 'users') */
	if(but->type==NUMSLI || but->type==HSVSLI) 
		okwidth= -4 + (but->x2 - but->x1)/2.0;
	else 
		okwidth= -4 + (but->x2 - but->x1); 
	
	/* name: */
	switch( but->type ) {
	
	case MENU:
	case ICONTEXTROW:
		
		if(but->x2 - but->x1 > 24) {
			value= ui_get_but_val(but);
			ui_set_name_menu(but, (int)value);
		}
		break;
	
	case NUM:
	case NUMSLI:
	case HSVSLI:
	case NUMABS:

		value= ui_get_but_val(but);

		if( but->pointype==FLO ) {
			if(but->a2) { /* amount of digits defined */
				if(but->a2==1) sprintf(but->drawstr, "%s%.1f", but->str, value);
				else if(but->a2==2) sprintf(but->drawstr, "%s%.2f", but->str, value);
				else if(but->a2==3) sprintf(but->drawstr, "%s%.3f", but->str, value);
				else sprintf(but->drawstr, "%s%.4f", but->str, value);
			}
			else {
				if(but->max<10.001) sprintf(but->drawstr, "%s%.3f", but->str, value);
				else sprintf(but->drawstr, "%s%.2f", but->str, value);
			}
		}
		else {
			sprintf(but->drawstr, "%s%d", but->str, (int)value);
		}
		break;

	case LABEL:
		if( but->pointype==FLO && but->poin) {
			value= ui_get_but_val(but);
			if(but->a2) { /* amount of digits defined */
				if(but->a2==1) sprintf(but->drawstr, "%s%.1f", but->str, value);
				else if(but->a2==2) sprintf(but->drawstr, "%s%.2f", but->str, value);
				else if(but->a2==3) sprintf(but->drawstr, "%s%.3f", but->str, value);
				else sprintf(but->drawstr, "%s%.4f", but->str, value);
			}
			else {
				sprintf(but->drawstr, "%s%.2f", but->str, value);
			}
		}
		else strcpy(but->drawstr, but->str);
		
		break;

	case IDPOIN:
		id= *(but->idpoin_idpp);
		strcpy(but->drawstr, but->str);
		if(id) strcat(but->drawstr, id->name+2);
		break;
	
	case TEX:
		strcpy(but->drawstr, but->str);
		strcat(but->drawstr, but->poin);
		break;
	
	case KEYEVT:
		strcpy(but->drawstr, but->str);
		if (but->flag & UI_SELECT) {
			strcat(but->drawstr, "Press a key");
		} else {
			strcat(but->drawstr, key_event_to_string((short) ui_get_but_val(but)));
		}
		break;
	case BUT_TOGDUAL:
		/* trying to get the dual-icon to left of text... not very nice */
		if(but->str[0]) {
			strcpy(but->drawstr, "  ");
			strcpy(but->drawstr+2, but->str);
		}
		break;
	default:
		strcpy(but->drawstr, but->str);
		
	}

	if(but->drawstr[0]) {
		but->strwidth= but->aspect*BIF_GetStringWidth(but->font, but->drawstr, transopts);
		// here should be check for less space for icon offsets...
		if(but->type==MENU) okwidth -= 15;
	}
	else
		but->strwidth= 0;

		/* automatic width */
	if(but->x2==0.0f && but->x1 > 0.0f) {
		but->x2= (but->x1+but->strwidth+6); 
	}

	if(but->strwidth==0) but->drawstr[0]= 0;
	else if(but->type==BUTM || but->type==BLOCK);	// no clip string, uiTextBoundsBlock is used (hack!)
	else {

		/* calc but->ofs, to draw the string shorter if too long */
		but->ofs= 0;
		while(but->strwidth > (int)okwidth ) {
	
			if ELEM3(but->type, NUM, NUMABS, TEX) {	// only these cut off left
				but->ofs++;
				but->strwidth= but->aspect*BIF_GetStringWidth(but->font, but->drawstr+but->ofs, transopts);
				
				/* textbut exception */
				if(but->pos != -1) {
					pos= but->pos+strlen(but->str);
					if(pos-1 < but->ofs) {
						pos= but->ofs-pos+1;
						but->ofs -= pos;
						if(but->ofs<0) {
							but->ofs= 0;
							pos--;
						}
						but->drawstr[ strlen(but->drawstr)-pos ]= 0;
					}
				}
			}
			else {
				but->drawstr[ strlen(but->drawstr)-1 ]= 0;
				but->strwidth= but->aspect*BIF_GetStringWidth(but->font, but->drawstr, transopts);
			}
			
			if(but->strwidth < 10) break;
		}
	}
}

static int ui_auto_themecol(uiBut *but)
{

	switch(but->type) {
	case BUT:
		return TH_BUT_ACTION;
	case ROW:
	case TOG:
	case TOG3:
	case TOGR:
	case TOGN:
	case BUT_TOGDUAL:
		return TH_BUT_SETTING;
	case SLI:
	case NUM:
	case NUMSLI:
	case NUMABS:
	case HSVSLI:
		return TH_BUT_NUM;
	case TEX:
		return TH_BUT_TEXTFIELD;
	case PULLDOWN:
	case BLOCK:
	case MENU:
	case BUTM:
		// (weak!) detect if it is a blockloop
		if(but->block->dt == UI_EMBOSSP) return TH_MENU_ITEM;
		return TH_BUT_POPUP;
	default:
		return TH_BUT_NEUTRAL;
	}
}

void uiBlockBeginAlign(uiBlock *block)
{
	/* if other align was active, end it */
	if(block->flag & UI_BUT_ALIGN) uiBlockEndAlign(block);

	block->flag |= UI_BUT_ALIGN_DOWN;	
	/* buttons declared after this call will this align flag */
}

static int buts_are_horiz(uiBut *but1, uiBut *but2)
{
	float dx, dy;
	
	dx= fabs( but1->x2 - but2->x1);
	dy= fabs( but1->y1 - but2->y2);
	
	if(dx > dy) return 0;
	return 1;
}

void uiBlockEndAlign(uiBlock *block)
{
	uiBut *prev, *but=NULL, *next;
	int flag= 0, cols=0, rows=0;
	int theme= BIF_GetThemeValue(TH_BUT_DRAWTYPE);
	
	if ( !(ELEM3(theme, TH_MINIMAL, TH_SHADED, TH_ROUNDED)) ) {
		block->flag &= ~UI_BUT_ALIGN;	// all 4 flags
		return;
	}
	
	/* auto align:
		- go back to first button of align start (ALIGN_DOWN)
		- compare triples, and define flags
	*/
	prev= block->buttons.last;
	while(prev) {
		if( (prev->flag & UI_BUT_ALIGN_DOWN)) but= prev;
		else break;
		
		if(but && but->next) {
			if(buts_are_horiz(but, but->next)) cols++;
			else rows++;
		}
		
		prev= prev->prev;
	}
	if(but==NULL) return;
	
	/* rows==0: 1 row, cols==0: 1 collumn */
	
	/* note;  how it uses 'flag' in loop below (either set it, or OR it) is confusing */
	prev= NULL;
	while(but) {
		next= but->next;
		
		/* clear old flag */
		but->flag &= ~UI_BUT_ALIGN_DOWN;
		
		if(flag==0) {	/* first case */
			if(next) {
				if(buts_are_horiz(but, next)) {
					if(rows==0)
						flag= UI_BUT_ALIGN_RIGHT;
					else 
						flag= UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_RIGHT;
				}
				else {
					flag= UI_BUT_ALIGN_DOWN;
				}
			}
		}
		else if(next==NULL) {	/* last case */
			if(prev) {
				if(buts_are_horiz(prev, but)) {
					if(rows==0) 
						flag= UI_BUT_ALIGN_LEFT;
					else
						flag= UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_LEFT;
				}
				else flag= UI_BUT_ALIGN_TOP;
			}
		}
		else if(buts_are_horiz(but, next)) {
			/* check if this is already second row */
			if( prev && buts_are_horiz(prev, but)==0) {
				flag |= UI_BUT_ALIGN_TOP;
				/* exception case: bottom row */
				if(rows>0) {
					uiBut *bt= but;
					while(bt) {
						if(bt->next && buts_are_horiz(bt, bt->next)==0 ) break; 
						bt= bt->next;
					}
					if(bt==0) flag= UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_RIGHT;
				}
			}
			else flag |= UI_BUT_ALIGN_LEFT;
		}
		else {
			if(cols==0) {
				flag |= UI_BUT_ALIGN_TOP;
			}
			else {	/* next button switches to new row */
				if( (flag & UI_BUT_ALIGN_TOP)==0) {	/* stil top row */
					if(prev)
						flag= UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_LEFT;
					else 
						flag |= UI_BUT_ALIGN_DOWN;
				}
				else 
					flag |= UI_BUT_ALIGN_TOP;
			}
		}
		
		but->flag |= flag;
		
		/* merge coordinates */
		if(prev) {
			// simple cases 
			if(rows==0) {
				but->x1= (prev->x2+but->x1)/2.0;
				prev->x2= but->x1;
			}
			else if(cols==0) {
				but->y2= (prev->y1+but->y2)/2.0;
				prev->y1= but->y2;
			}
			else {
				if(buts_are_horiz(prev, but)) {
					but->x1= (prev->x2+but->x1)/2.0;
					prev->x2= but->x1;
					/* copy height too */
					but->y2= prev->y2;
				}
				else if(prev->prev && buts_are_horiz(prev->prev, prev)==0) {
					/* the previous button is a single one in its row */
					but->y2= (prev->y1+but->y2)/2.0;
					prev->y1= but->y2;
				}
				else {
					/* the previous button is not a single one in its row */
					but->y2= prev->y1;
				}
			}
		}
		
		prev= but;
		but= next;
	}
	
	block->flag &= ~UI_BUT_ALIGN;	// all 4 flags
}

#if 0
static void uiBlockEndAligno(uiBlock *block)
{
	uiBut *but;
	
	/* correct last defined button */
	but= block->buttons.last;
	if(but) {
		/* vertical align case */
		if( (block->flag & UI_BUT_ALIGN) == (UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_DOWN) ) {
			but->flag &= ~UI_BUT_ALIGN_DOWN;
		}
		/* horizontal align case */
		if( (block->flag & UI_BUT_ALIGN) == (UI_BUT_ALIGN_LEFT|UI_BUT_ALIGN_RIGHT) ) {
			but->flag &= ~UI_BUT_ALIGN_RIGHT;
		}
		/* else do nothing, manually provided flags */
	}
	block->flag &= ~UI_BUT_ALIGN;	// all 4 flags
}
#endif

/*
ui_def_but is the function that draws many button types

for float buttons:
	"a1" Click Step (how much to change the value each click)
	"a2" Number of decimal point values to display. 0 defaults to 3 (0.000) 1,2,3, and a maximum of 4,
       all greater values will be clamped to 4.

*/
static uiBut *ui_def_but(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but;
	short slen;
	
	if(type & BUTPOIN) {		/* a pointer is required */
		if(poin==NULL) {
				/* if pointer is zero, button is removed and not drawn */
			BIF_ThemeColor(block->themecol);
			glRects(x1,  y1,  x1+x2,  y1+y2);
			return NULL;
		}
	}

	but= MEM_callocN(sizeof(uiBut), "uiBut");

	but->type= type & BUTTYPE;
	but->pointype= type & BUTPOIN;
	but->bit= type & BIT;
	but->bitnr= type & 31;
	but->icon = 0;

	BLI_addtail(&block->buttons, but);

	but->retval= retval;
	if( strlen(str)>=UI_MAX_NAME_STR-1 ) {
		but->str= MEM_callocN( strlen(str)+2, "uiDefBut");
		strcpy(but->str, str);
	}
	else {
		but->str= but->strdata;
		strcpy(but->str, str);
	}
	but->x1= x1; 
	but->y1= y1;
	if(block->autofill) {
		but->x2= x2; 
		but->y2= y2;
	}
	else {
		but->x2= (x1+x2); 
		but->y2= (y1+y2);
	}
	but->poin= poin;
	but->min= min; 
	but->max= max;
	but->a1= a1; 
	but->a2= a2;
	but->tip= tip;
	
	but->font= block->curfont;
	
	but->lock= UIlock;
	but->lockstr= UIlockstr;

	but->aspect= block->aspect;
	but->win= block->win;
	but->block= block;		// pointer back, used for frontbuffer status, and picker

	if(block->themecol==TH_AUTO) but->themecol= ui_auto_themecol(but);
	else but->themecol= block->themecol;
	
	if (but->type==BUTM) {
		but->butm_func= block->butm_func;
		but->butm_func_arg= block->butm_func_arg;
	} else {
		but->func= block->func;
		but->func_arg1= block->func_arg1;
		but->func_arg2= block->func_arg2;
	}

	ui_set_embossfunc(but, block->dt);
	
	but->pos= -1;	/* cursor invisible */

	if(ELEM(but->type, NUM, NUMABS)) {	/* add a space to name */
		slen= strlen(but->str);
		if(slen>0 && slen<UI_MAX_NAME_STR-2) {
			if(but->str[slen-1]!=' ') {
				but->str[slen]= ' ';
				but->str[slen+1]= 0;
			}
		}
	}
	
	if(but->type==HSVCUBE) { /* hsv buttons temp storage */
		float rgb[3];
		ui_get_but_vectorf(but, rgb);
		rgb_to_hsv(rgb[0], rgb[1], rgb[2], but->hsv, but->hsv+1, but->hsv+2);
	}

	if ELEM8(but->type, HSVSLI , NUMSLI, MENU, TEX, LABEL, IDPOIN, BLOCK, BUTM) {
		but->flag |= UI_TEXT_LEFT;
	}
	
	if(but->type==BUT_TOGDUAL) {
		but->flag |= UI_ICON_LEFT;
	}

	if(but->type==ROUNDBOX)
		but->flag |= UI_NO_HILITE;

	but->flag |= (block->flag & UI_BUT_ALIGN);
	if(block->flag & UI_BLOCK_NO_HILITE)
		but->flag |= UI_NO_HILITE;
	
	return but;
}

uiBut *uiDefBut(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but= ui_def_but(block, type, retval, str, x1, y1, x2, y2, poin, min, max, a1, a2, tip);

	ui_check_but(but);
	
	return but;
}

	/* if _x_ is a power of two (only one bit) return the power,
	 * otherwise return -1. 
	 * (1<<findBitIndex(x))==x for powers of two.
	 */
static int findBitIndex(unsigned int x) {
	if (!x || (x&(x-1))!=0) {	/* x&(x-1) strips lowest bit */
		return -1;
	} else {
		int idx= 0;

		if (x&0xFFFF0000)	idx+=16, x>>=16;
		if (x&0xFF00)		idx+=8, x>>=8;
		if (x&0xF0)			idx+=4, x>>=4;
		if (x&0xC)			idx+=2, x>>=2;
		if (x&0x2)			idx+=1;

		return idx;
	}
}

/* autocomplete helper functions */
struct AutoComplete {
	int maxlen;
	char *truncate;
	char *startname;
};

AutoComplete *autocomplete_begin(char *startname, int maxlen)
{
	AutoComplete *autocpl;
	
	autocpl= MEM_callocN(sizeof(AutoComplete), "AutoComplete");
	autocpl->maxlen= maxlen;
	autocpl->truncate= MEM_callocN(sizeof(char)*maxlen, "AutoCompleteTruncate");
	autocpl->startname= startname;

	return autocpl;
}

void autocomplete_do_name(AutoComplete *autocpl, const char *name)
{
	char *truncate= autocpl->truncate;
	char *startname= autocpl->startname;
	int a;

	for(a=0; a<autocpl->maxlen-1; a++) {
		if(startname[a]==0 || startname[a]!=name[a])
			break;
	}
	/* found a match */
	if(startname[a]==0) {
		/* first match */
		if(truncate[0]==0)
			BLI_strncpy(truncate, name, autocpl->maxlen);
		else {
			/* remove from truncate what is not in bone->name */
			for(a=0; a<autocpl->maxlen-1; a++) {
				if(truncate[a]!=name[a])
					truncate[a]= 0;
			}
		}
	}
}

void autocomplete_end(AutoComplete *autocpl, char *autoname)
{
	if(autocpl->truncate[0])
		BLI_strncpy(autoname, autocpl->truncate, autocpl->maxlen);
	else
		BLI_strncpy(autoname, autocpl->startname, autocpl->maxlen);

	MEM_freeN(autocpl->truncate);
	MEM_freeN(autocpl);
}

/* autocomplete callback for ID buttons */
static void autocomplete_id(char *str, void *arg_v)
{
	int blocktype= (long)arg_v;
	ListBase *listb= wich_libbase(G.main, blocktype);
	
	if(listb==NULL) return;
	
	/* search if str matches the beginning of an ID struct */
	if(str[0]) {
		AutoComplete *autocpl= autocomplete_begin(str, 22);
		ID *id;
		
		for(id= listb->first; id; id= id->next)
			autocomplete_do_name(autocpl, id->name+2);

		autocomplete_end(autocpl, str);
	}
}

static uiBut *uiDefButBit(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	int bitIdx= findBitIndex(bit);
	if (bitIdx==-1) {
		return NULL;
	} else {
		return uiDefBut(block, type|BIT|bitIdx, retval, str, x1, y1, x2, y2, poin, min, max, a1, a2, tip);
	}
}
uiBut *uiDefButF(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|FLO, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitF(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefButBit(block, type|FLO, bit, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButI(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|INT, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitI(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefButBit(block, type|INT, bit, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButS(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|SHO, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitS(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefButBit(block, type|SHO, bit, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButC(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|CHA, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitC(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefButBit(block, type|CHA, bit, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}

uiBut *uiDefIconBut(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but= ui_def_but(block, type, retval, "", x1, y1, x2, y2, poin, min, max, a1, a2, tip);
	
	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;

	ui_check_but(but);
	
	return but;
}
static uiBut *uiDefIconButBit(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	int bitIdx= findBitIndex(bit);
	if (bitIdx==-1) {
		return NULL;
	} else {
		return uiDefIconBut(block, type|BIT|bitIdx, retval, icon, x1, y1, x2, y2, poin, min, max, a1, a2, tip);
	}
}

uiBut *uiDefIconButF(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|FLO, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitF(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconButBit(block, type|FLO, bit, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButI(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|INT, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitI(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconButBit(block, type|INT, bit, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButS(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|SHO, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitS(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconButBit(block, type|SHO, bit, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButC(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|CHA, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitC(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconButBit(block, type|CHA, bit, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}

/* Button containing both string label and icon */
uiBut *uiDefIconTextBut(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but= ui_def_but(block, type, retval, str, x1, y1, x2, y2, poin, min, max, a1, a2, tip);

	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;

	but->flag|= UI_ICON_LEFT;

	ui_check_but(but);

	return but;
}
static uiBut *uiDefIconTextButBit(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	int bitIdx= findBitIndex(bit);
	if (bitIdx==-1) {
		return NULL;
	} else {
		return uiDefIconTextBut(block, type|BIT|bitIdx, retval, icon, str, x1, y1, x2, y2, poin, min, max, a1, a2, tip);
	}
}

uiBut *uiDefIconTextButF(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|FLO, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitF(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextButBit(block, type|FLO, bit, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButI(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|INT, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitI(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextButBit(block, type|INT, bit, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButS(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|SHO, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitS(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextButBit(block, type|SHO, bit, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButC(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|CHA, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitC(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextButBit(block, type|CHA, bit, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}

/* END Button containing both string label and icon */

void uiAutoBlock(uiBlock *block, float minx, float miny, float sizex, float sizey, int flag)
{
	block->minx= minx;
	block->maxx= minx+sizex;
	block->miny= miny;
	block->maxy= miny+sizey;
	
	block->autofill= flag;	/* also check for if it has to be done */

}

void uiSetButLink(uiBut *but, void **poin, void ***ppoin, short *tot, int from, int to)
{
	uiLink *link;
	
	link= but->link= MEM_callocN(sizeof(uiLink), "new uilink");
	
	link->poin= poin;
	link->ppoin= ppoin;
	link->totlink= tot;
	link->fromcode= from;
	link->tocode= to;
}

/* cruft to make uiBlock and uiBut private */

int uiBlocksGetYMin(ListBase *lb)
{
	uiBlock *block;
	int min= 0;
	
	for (block= lb->first; block; block= block->next)
		if (block==lb->first || block->miny<min)
			min= block->miny;
			
	return min;
}

int uiBlockGetCol(uiBlock *block)
{
	return block->themecol;
}
void uiBlockSetCol(uiBlock *block, int col)
{
	block->themecol= col;
}
void uiBlockSetEmboss(uiBlock *block, int emboss)
{
	block->dt= emboss;
}
void uiBlockSetDirection(uiBlock *block, int direction)
{
	block->direction= direction;
}

/* this call escapes if there's alignment flags */
void uiBlockFlipOrder(uiBlock *block)
{
	ListBase lb;
	uiBut *but, *next;
	float centy, miny=10000, maxy= -10000;

//      if(U.uiflag & USER_PLAINMENUS)
//		return;
	
	for(but= block->buttons.first; but; but= but->next) {
		if(but->flag & UI_BUT_ALIGN) return;
		if(but->y1 < miny) miny= but->y1;
		if(but->y2 > maxy) maxy= but->y2;
	}
	/* mirror trick */
	centy= (miny+maxy)/2.0;
	for(but= block->buttons.first; but; but= but->next) {
		but->y1 = centy-(but->y1-centy);
		but->y2 = centy-(but->y2-centy);
		SWAP(float, but->y1, but->y2);
	}
	
	/* also flip order in block itself, for example for arrowkey */
	lb.first= lb.last= NULL;
	but= block->buttons.first;
	while(but) {
		next= but->next;
		BLI_remlink(&block->buttons, but);
		BLI_addtail(&lb, but);
		but= next;
	}
	block->buttons= lb;
}


void uiBlockSetFlag(uiBlock *block, int flag)
{
	block->flag= flag;
}
void uiBlockSetXOfs(uiBlock *block, int xofs)
{
	block->xofs= xofs;
}
void* uiBlockGetCurFont(uiBlock *block)
{
	return block->curfont;
}

void uiButSetFlag(uiBut *but, int flag)
{
	but->flag|= flag;
}
void uiButClearFlag(uiBut *but, int flag)
{
	but->flag&= ~flag;
}

int uiButGetRetVal(uiBut *but)
{
	return but->retval;
}

/* Call this function BEFORE adding buttons to the block */
void uiBlockSetButmFunc(uiBlock *block, void (*menufunc)(void *arg, int event), void *arg)
{
	block->butm_func= menufunc;
	block->butm_func_arg= arg;
}

void uiBlockSetFunc(uiBlock *block, void (*func)(void *arg1, void *arg2), void *arg1, void *arg2)
{
	block->func= func;
	block->func_arg1= arg1;
	block->func_arg2= arg2;
}

void uiBlockSetDrawExtraFunc(uiBlock *block, void (*func)())
{
	block->drawextra= func;
}

void uiButSetFunc(uiBut *but, void (*func)(void *arg1, void *arg2), void *arg1, void *arg2)
{
	but->func= func;
	but->func_arg1= arg1;
	but->func_arg2= arg2;
}

void uiButSetCompleteFunc(uiBut *but, void (*func)(char *str, void *arg), void *arg)
{
	but->autocomplete_func= func;
	but->autofunc_arg= arg;
}


uiBut *uiDefIDPoinBut(uiBlock *block, uiIDPoinFuncFP func, short blocktype, int retval, char *str, short x1, short y1, short x2, short y2, void *idpp, char *tip)
{
	uiBut *but= ui_def_but(block, IDPOIN, retval, str, x1, y1, x2, y2, NULL, 0.0, 0.0, 0.0, 0.0, tip);
	but->idpoin_func= func;
	but->idpoin_idpp= (ID**) idpp;
	ui_check_but(but);
	
	if(blocktype)
		uiButSetCompleteFunc(but, autocomplete_id, (void *)(long)blocktype);

	return but;
}

uiBut *uiDefBlockBut(uiBlock *block, uiBlockFuncFP func, void *arg, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, BLOCK, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	but->block_func= func;
	ui_check_but(but);
	return but;
}

uiBut *uiDefPulldownBut(uiBlock *block, uiBlockFuncFP func, void *arg, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, PULLDOWN, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	but->block_func= func;
	ui_check_but(but);
	return but;
}

/* Block button containing both string label and icon */
uiBut *uiDefIconTextBlockBut(uiBlock *block, uiBlockFuncFP func, void *arg, int icon, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, BLOCK, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	
	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;

	but->flag|= UI_ICON_LEFT;
	but->flag|= UI_ICON_RIGHT;

	but->block_func= func;
	ui_check_but(but);
	
	return but;
}

/* Block button containing icon */
uiBut *uiDefIconBlockBut(uiBlock *block, uiBlockFuncFP func, void *arg, int retval, int icon, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, BLOCK, retval, "", x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	
	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;
	
	but->flag|= UI_ICON_LEFT;
	but->flag|= UI_ICON_RIGHT;
	
	but->block_func= func;
	ui_check_but(but);
	
	return but;
}

void uiDefKeyevtButS(uiBlock *block, int retval, char *str, short x1, short y1, short x2, short y2, short *spoin, char *tip)
{
	uiBut *but= ui_def_but(block, KEYEVT|SHO, retval, str, x1, y1, x2, y2, spoin, 0.0, 0.0, 0.0, 0.0, tip);
	ui_check_but(but);
}

/* ******************** PUPmenu ****************** */

static int pupmenu_set= 0;

void pupmenu_set_active(int val)
{
	pupmenu_set= val;
}

/* value== -1 read, otherwise set */
static int pupmenu_memory(char *str, int value)
{
	static char mem[256], first=1;
	int val=0, nr=0;
	
	if(first) {
		memset(mem, 0, 256);
		first= 0;
	}
	while(str[nr]) {
		val+= str[nr];
		nr++;
	}

	if(value >= 0) mem[ val & 255 ]= value;
	else return mem[ val & 255 ];
	
	return 0;
}

#define PUP_LABELH	6
short pupmenu(char *instr)
{
	uiBlock *block;
	ListBase listb= {NULL, NULL};
	int event;
	short lastselected, width, height=0, mousexmove = 0, mouseymove, xmax, ymax, mval[2], val= -1;
	short a, startx, starty, endx, endy, boxh=TBOXH, x1, y1;
	MenuData *md;

	/* block stuff first, need to know the font */
	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_NUMSELECT);
	block->themecol= TH_MENU_ITEM;
	
	md= decompose_menu_string(instr);

	/* size and location, title slightly bigger for bold */
	if(md->title) width= 2*strlen(md->title)+BIF_GetStringWidth(uiBlockGetCurFont(block), md->title, (U.transopts & USER_TR_BUTTONS));
	else width= 0;
	for(a=0; a<md->nitems; a++) {
		char *name= md->items[a].str;
		
		xmax= BIF_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str, (U.transopts & USER_TR_BUTTONS));
		if(xmax>width) width= xmax;

		if( strcmp(name, "%l")==0) height+= PUP_LABELH;
		else height+= boxh;
	}

	width+= 10;
	
	xmax = G.curscreen->sizex;
	ymax = G.curscreen->sizey;

	getmouseco_sc(mval);
	
	/* set first item */
	lastselected= 0;
	if(pupmenu_set) {
		lastselected= pupmenu_set-1;
		pupmenu_set= 0;
	}
	else if(md->nitems>1) {
		lastselected= pupmenu_memory(instr, -1);
	}

	startx= mval[0]-(0.8*(width));
	starty= mval[1]-height+boxh/2;
	if(lastselected>=0 && lastselected<md->nitems) {
		for(a=0; a<md->nitems; a++) {
			if(a==lastselected) break;
			if( strcmp(md->items[a].str, "%l")==0) starty+= PUP_LABELH;
			else starty+=boxh;
		}
		
		//starty= mval[1]-height+boxh/2+lastselected*boxh;
	}
	
	mouseymove= 0;
	
	if(startx<10) startx= 10;
	if(starty<10) {
		mouseymove= 10-starty;
		starty= 10;
	}
	
	endx= startx+width;
	endy= starty+height;
	if(endx>xmax) {
		endx= xmax-10;
		startx= endx-width;
	}
	if(endy>ymax-20) {
		mouseymove= ymax-endy-20;
		endy= ymax-20;
		starty= endy-height;
		
	}

	if(mouseymove) {
		ui_warp_pointer(mval[0], mouseymove+mval[1]);
		mousexmove= mval[0];
		mouseymove= mval[1];
	}
	
	/* here we go! */	
	if(md->title) {
		uiBut *bt;
		char titlestr[256];
		uiSetCurFont(block, UI_HELVB);
		
		if (md->titleicon) {
			width+= 20;
			sprintf(titlestr, " %s", md->title);
			uiDefIconTextBut(block, LABEL, 0, md->titleicon, titlestr, startx, (short)(starty+height), width, boxh, NULL, 0.0, 0.0, 0, 0, "");
		} else {
			bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+height), width, boxh, NULL, 0.0, 0.0, 0, 0, "");
			bt->flag= UI_TEXT_LEFT;
		}
		uiSetCurFont(block, UI_HELV);
	}

	y1= starty + height - boxh;
	x1= startx;
	
	for(a=0; a<md->nitems; a++) {
		char *name= md->items[a].str;
		
		if( strcmp(name, "%l")==0) {
			uiDefBut(block, SEPR, B_NOP, "", x1, y1, width, PUP_LABELH, NULL, 0, 0.0, 0, 0, "");
			y1 -= PUP_LABELH;
		}
		else {
			uiDefButS(block, BUTM, B_NOP, name, x1, y1, width, boxh-1, &val, (float) md->items[a].retval, 0.0, 0, 0, "");
			y1 -= boxh;
		}
	}
	
	uiBoundsBlock(block, 1);

	event= uiDoBlocks(&listb, 0, 1);

	/* calculate last selected */
	if(event & UI_RETURN_OK) {
		lastselected= 0;
		for(a=0; a<md->nitems; a++) {
			if(val==md->items[a].retval) lastselected= a;
		}
		
		pupmenu_memory(instr, lastselected);
	}
	menudata_free(md);
	
	if(mouseymove && (event & UI_RETURN_OUT)==0) ui_warp_pointer(mousexmove, mouseymove);
	return val;
}

short pupmenu_col(char *instr, int maxrow)
{
	uiBlock *block;
	ListBase listb= {NULL, NULL};
	int	columns, rows;
	short	mousemove[2], mval[2], event;
	int width, height, xmax, ymax, val= -1;
	int a, startx, starty, endx, endy, boxh=TBOXH, x1, y1;
	MenuData *md;
	
	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_NUMSELECT);
	block->themecol= TH_MENU_ITEM;
	
	md= decompose_menu_string(instr);

	/* collumns and row calculation */
	columns= (md->nitems+maxrow)/maxrow;
	if (columns<1) columns= 1;
	
	if(columns > 8) {
		maxrow += 5;
		columns= (md->nitems+maxrow)/maxrow;
	}
	
	rows= (int) md->nitems/columns;
	if (rows<1) rows= 1;
	
	while (rows*columns<(md->nitems+columns) ) rows++;
		
	/* size and location */
	if(md->title) {
		width= 2*strlen(md->title)+BIF_GetStringWidth(uiBlockGetCurFont(block), md->title, (U.transopts & USER_TR_BUTTONS));
		width /= columns;
	}
	else width= 0;
	
	for(a=0; a<md->nitems; a++) {
		xmax= BIF_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str, (U.transopts & USER_TR_BUTTONS));
		if(xmax>width) width= xmax;
	}

	width+= 10;
	if (width<50) width=50;

	boxh= TBOXH;
	
	height= rows*boxh;
	if (md->title) height+= boxh;
	
	xmax = G.curscreen->sizex;
	ymax = G.curscreen->sizey;

	getmouseco_sc(mval);
	
	/* find active item */
#if 0
	fvalue= ui_get_but_val(but);
	for(a=0; a<md->nitems; a++) {
		if( md->items[a].retval== (int)fvalue ) break;
	}
#endif
	/* no active item? */
	if(a==md->nitems) {
		if(md->title) a= -1;
		else a= 0;
	}

	if(a>0)
		startx = mval[0]-width/2 - ((int)(a)/rows)*width;
	else
		startx= mval[0]-width/2;
	starty = mval[1]-height + boxh/2 + ((a)%rows)*boxh;

	if (md->title) starty+= boxh;
	
	mousemove[0]= mousemove[1]= 0;
	
	if(startx<10) {
		mousemove[0]= 10-startx;
		startx= 10;
	}
	if(starty<10) {
		mousemove[1]= 10-starty;
		starty= 10;
	}
	
	endx= startx+width*columns;
	endy= starty+height;
	
	if(endx>xmax) {
		mousemove[0]= xmax-endx-10;
		endx= xmax-10;
		startx= endx-width*columns;
	}
	if(endy>ymax) {
		mousemove[1]= ymax-endy-10;
		endy= ymax-10;
		starty= endy-height;
	}

	ui_warp_pointer(mval[0]+mousemove[0], mval[1]+mousemove[1]);

	mousemove[0]= mval[0];
	mousemove[1]= mval[1];

	/* here we go! */

	if(md->title) {
		uiBut *bt;
		uiSetCurFont(block, UI_HELVB);
		bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+rows*boxh), columns*(short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
		uiSetCurFont(block, UI_HELV);
		bt->flag= UI_TEXT_LEFT;
	}

	for(a=0; a<md->nitems; a++) {
		char *name= md->items[a].str;
		
		int icon = md->items[a].icon;

		x1= startx + width*((int)a/rows);
		y1= starty - boxh*(a%rows) + (rows-1)*boxh; 
		
		if( strcmp(name, "%l")==0){
			uiDefBut(block, SEPR, B_NOP, "", x1, y1, width, PUP_LABELH, NULL, 0, 0.0, 0, 0, "");
			y1 -= PUP_LABELH;
		}
		else if (icon) {
			uiDefIconButI(block, BUTM, B_NOP, icon, x1, y1, width+16, boxh-1, &val, (float) md->items[a].retval, 0.0, 0, 0, "");
			y1 -= boxh;
		}
		else {
			uiDefButI(block, BUTM, B_NOP, name, x1, y1, width, boxh-1, &val, (float) md->items[a].retval, 0.0, 0, 0, "");
			y1 -= boxh;
		}
		//uiDefButI(block, BUTM, B_NOP, md->items[a].str, x1, y1, (short)(width-(rows>1)), (short)(boxh-1), &val, (float)md->items[a].retval, 0.0, 0, 0, "");
	}
	
	uiBoundsBlock(block, 1);

	event= uiDoBlocks(&listb, 0, 1);
	
	menudata_free(md);
	
	if((event & UI_RETURN_OUT)==0) ui_warp_pointer(mousemove[0], mousemove[1]);
	
	return val;	
}

