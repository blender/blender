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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include "BLI_winstuff.h"
#endif   

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BMF_Api.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"

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
#include "BIF_interface.h"

#include "BSE_view.h"

#include "mydevice.h"
#include "interface.h"
#include "blendef.h"

/* naming conventions:
 * 
 * uiBlahBlah()		external function
 * ui_blah_blah()	internal function
 */

/***/

typedef struct {
	short xim, yim;
	unsigned int *rect;
	short xofs, yofs;
} uiIconImage;

typedef struct {
	short mval[2];
	short qual, val;
	int event;
} uiEvent;

typedef struct {
	void *xl, *large, *medium, *small;
} uiFont;

typedef struct uiLinkLine uiLinkLine;
struct uiLinkLine {				/* only for draw/edit */
	uiLinkLine *next, *prev;

	short flag, pad;
	
	uiBut *from, *to;	
};

typedef struct {
	void **poin;		/* pointer to original pointer */
	void ***ppoin;		/* pointer to original pointer-array */
	short *totlink;		/* if pointer-array, here is the total */
	
	short maxlink, pad;
	short fromcode, tocode;
	
	ListBase lines;
} uiLink;

struct uiBut {
	uiBut *next, *prev;
	short type, pointype, bit, bitnr, retval, flag, strwidth, ofs, pos;

	char *str;
	char strdata[UI_MAX_NAME_STR];
	char drawstr[UI_MAX_DRAW_STR];
	
	float x1, y1, x2, y2;

	char *poin;
	float min, max;
	float a1, a2, rt[4];
	float aspect;

	void (*func)(void *, void *);
	void *func_arg1;
	void *func_arg2;

	void (*embossfunc)(BIFColorID, float, float, float, float, float, int);

	uiLink *link;
	
	char *tip, *lockstr;

	BIFColorID col;
	void *font;

	BIFIconID icon;
	short lock, win;
	short iconadd;

		/* IDPOIN data */
	uiIDPoinFuncFP idpoin_func;
	ID **idpoin_idpp;

		/* BLOCK data */
	uiBlockFuncFP block_func;

		/* BUTM data */
	void (*butm_func)(void *arg, int event);
	void *butm_func_arg;
	
		/* pointer back */
	uiBlock *block;
};

struct uiBlock {
	uiBlock *next, *prev;
	
	ListBase buttons;
	
	char name[UI_MAX_NAME_STR];
	
	float winmat[4][4];
	
	float minx, miny, maxx, maxy;
	float aspect;

	void (*butm_func)(void *arg, int event);
	void *butm_func_arg;

	void (*func)(void *arg1, void *arg2);
	void *func_arg1;
	void *func_arg2;
	
	BIFColorID col;
	short font;	/* indices */
	int afterval;
	void *curfont;
	
	short autofill, flag, win, winq, direction, dt, frontbuf;  //frontbuf see below
	void *saveunder;
	
	float xofs, yofs;  // offset to parent button
};

/* block->frontbuf: (only internal here), to nice localize the old global var uiFrontBuf */
#define UI_NEED_DRAW_FRONT 		1
#define UI_HAS_DRAW_FRONT 		2


/* ************ GLOBALS ************* */

static float UIwinmat[4][4];
static int UIlock= 0, UIafterval;
static char *UIlockstr=NULL;
static void (*UIafterfunc)(void *arg, int event);
static void *UIafterfunc_arg;

static uiFont UIfont[UI_ARRAY];  // no init needed
static uiBut *UIbuttip;

/* ****************************** */

static void ui_check_but(uiBut *but);
static void ui_set_but_val(uiBut *but, double value);
static double ui_get_but_val(uiBut *but);

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

/* ************* SAVE UNDER ************ */

typedef struct {
	short x, y, sx, sy, oldwin;
	int oldcursor;
	unsigned int *rect;
} uiSaveUnder;


static void ui_paste_under(uiSaveUnder *su)
{
	
	if(su) {
		glDisable(GL_DITHER);
		glRasterPos2f( su->x-0.5,  su->y-0.5 );
		glDrawPixels(su->sx, su->sy, GL_RGBA, GL_UNSIGNED_BYTE, su->rect);
		glEnable(GL_DITHER);

		if(su->oldwin) {
			mywinset(su->oldwin);
			if (su->oldcursor) {
				set_cursor(su->oldcursor);
			}
		}
		
		MEM_freeN(su->rect);
		MEM_freeN(su);
	}
}


static uiSaveUnder *ui_save_under(int x, int y, int sx, int sy)
{
	uiSaveUnder *su=NULL;
	
	if(sx>1 && sy>1) {
		
		su= MEM_callocN(sizeof(uiSaveUnder), "save under");	
		
		su->rect= MEM_mallocN(sx*sy*4, "temp_frontbuffer_image");
		su->x= x;
		su->y= y;
		su->sx= sx;
		su->sy= sy;
		glReadPixels(x, y, sx, sy, GL_RGBA, GL_UNSIGNED_BYTE, su->rect);
	}
	
	return su;
}





/* ************* DRAW ************** */


static void ui_graphics_to_window(int win, float *x, float *y)	/* for rectwrite  */
{
	float gx, gy;
	int sx, sy;
	int getsizex, getsizey;

	bwin_getsize(win, &getsizex, &getsizey);
	bwin_getsuborigin(win, &sx, &sy);

	gx= *x;
	gy= *y;
	*x= sx + getsizex*(0.5+ 0.5*(gx*UIwinmat[0][0]+ gy*UIwinmat[1][0]+ UIwinmat[3][0]));
	*y= sy + getsizey*(0.5+ 0.5*(gx*UIwinmat[0][1]+ gy*UIwinmat[1][1]+ UIwinmat[3][1]));
}



static void ui_window_to_graphics(int win, float *x, float *y)	/* for mouse cursor */
{
	float a, b, c, d, e, f, px, py;
	int getsizex, getsizey;
		
	bwin_getsize(win, &getsizex, &getsizey);

	a= .5*getsizex*UIwinmat[0][0];
	b= .5*getsizex*UIwinmat[1][0];
	c= .5*getsizex*(1.0+UIwinmat[3][0]);

	d= .5*getsizey*UIwinmat[0][1];
	e= .5*getsizey*UIwinmat[1][1];
	f= .5*getsizey*(1.0+UIwinmat[3][1]);
	
	px= *x;
	py= *y;
	
	*y=  (a*(py-f) + d*(c-px))/(a*e-d*b);
	*x= (px- b*(*y)- c)/a;
	
}

static uiSaveUnder *ui_bgnpupdraw(int startx, int starty, int endx, int endy, int cursor)
{
	uiSaveUnder *su;
	short oldwin;
	
	#if defined(__sgi) || defined(__sun)
	/* this is a dirty patch: gets sometimes the backbuffer */
	my_get_frontbuffer_image(0, 0, 1, 1);
	my_put_frontbuffer_image();
	#endif

	oldwin= mywinget();

	mywinset(G.curscreen->mainwin);
	
	/* tinsy bit larger, 1 pixel on the edge */
	
	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);
	
	/* for geforce and other cards */
	glFinish();

	su= ui_save_under(startx-1, starty-1, endx-startx+2, endy-starty+6);
	if(su) su->oldwin= oldwin;
	
	if(su && cursor) {
		su->oldcursor= get_cursor();
		set_cursor(CURSOR_STD);
	}
	
	return su;
}

static void ui_endpupdraw(uiSaveUnder *su)
{

	/* for geforce and other cards */

	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);
	
	glFinish();

	if(su) {
		ui_paste_under(su);
	}
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_BACK);
}

static void ui_draw_icon(uiBut *but, BIFIconID icon)
{
	float xs, ys;
	
	/* check for left aligned icons (in case of IconTextBut) */
	if (but->type == ICONTEXTROW) {
		xs= (but->x1+but->x2- BIF_get_icon_width(icon))/2.0;
		ys= (but->y1+but->y2- BIF_get_icon_height(icon))/2.0;
	}
	else if(but->flag & UI_ICON_LEFT) {
	        if (but->type==BUTM) {
	         	xs= but->x1+1.0;
	        }
		else {
			xs= but->x1+6.0;
		}
		ys= (but->y1+but->y2- BIF_get_icon_height(icon))/2.0;
	}
	else {
		xs= (but->x1+but->x2- BIF_get_icon_width(icon))/2.0;
		ys= (but->y1+but->y2- BIF_get_icon_height(icon))/2.0;
	}
	/* END check for left aligned icons (in case of IconTextBut) */

	glRasterPos2f(xs, ys);

	if(but->aspect>1.1) glPixelZoom(1.0/but->aspect, 1.0/but->aspect);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if(but->flag & UI_SELECT) {
		if(but->flag & UI_ACTIVE) {
			BIF_draw_icon_blended(icon, but->col, COLORSHADE_DARK);
		} else {
			BIF_draw_icon_blended(icon, but->col, COLORSHADE_GREY);
		}
	}
	else {
		if ((but->flag & UI_ACTIVE) && but->type==BUTM) {
			BIF_draw_icon_blended(icon, BUTMACTIVE, COLORSHADE_MEDIUM);
		} else if (but->flag & UI_ACTIVE) {
			BIF_draw_icon_blended(icon, but->col, COLORSHADE_HILITE);
		} else {
			BIF_draw_icon_blended(icon, but->col, COLORSHADE_MEDIUM);
		}
	}

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	glPixelZoom(1.0, 1.0);
}

static void ui_draw_outlineX(float x1, float y1, float x2, float y2, float asp1)
{
	float vec[2];
	
	glBegin(GL_LINE_LOOP);
	vec[0]= x1+asp1; vec[1]= y1-asp1;
	glVertex2fv(vec);
	vec[0]= x2-asp1; 
	glVertex2fv(vec);
	vec[0]= x2+asp1; vec[1]= y1+asp1;
	glVertex2fv(vec);
	vec[1]= y2-asp1;
	glVertex2fv(vec);
	vec[0]= x2-asp1; vec[1]= y2+asp1;
	glVertex2fv(vec);
	vec[0]= x1+asp1;
	glVertex2fv(vec);
	vec[0]= x1-asp1; vec[1]= y2-asp1;
	glVertex2fv(vec);
	vec[1]= y1+asp1;
	glVertex2fv(vec);
	glEnd();		
	
}

static void ui_emboss_X(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{
	/* paper */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_DARK);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_HILITE);
		else BIF_set_color(bc, COLORSHADE_MEDIUM);
	}
	
	glRectf(x1+1, y1+1, x2-1, y2-1);

	x1+= asp;
	x2-= asp;
	y1+= asp;
	y2-= asp;

	/* below */
	if(flag & UI_SELECT) BIF_set_color(bc, COLORSHADE_MEDIUM);
	else BIF_set_color(bc, COLORSHADE_DARK);
	fdrawline(x1, y1, x2, y1);

	/* right */
	fdrawline(x2, y1, x2, y2);
	
	/* top */
	if(flag & UI_SELECT) BIF_set_color(bc, COLORSHADE_DARK);
	else BIF_set_color(bc, COLORSHADE_WHITE);
	fdrawline(x1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1, x1, y2);
	
	/* outline */
	glColor3ub(0,0,0);
	ui_draw_outlineX(x1, y1, x2, y2, asp);
}

static void ui_emboss_A(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{
	short a;

	/* paper */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_DARK);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_HILITE);
		else BIF_set_color(bc, COLORSHADE_MEDIUM);
	}
	
	glRectf(x1+1, y1+1, x2-1, y2-1);

	x1+= asp;
	x2-= asp;
	y1+= asp;
	y2-= asp;

	/* below */
	if(flag & UI_SELECT) BIF_set_color(bc, COLORSHADE_MEDIUM);
	else BIF_set_color(bc, COLORSHADE_DARK);
	fdrawline(x1, y1, x2, y1);

	/* right */
	fdrawline(x2, y1, x2, y2);
	
	/* top */
	if(flag & UI_SELECT) BIF_set_color(bc, COLORSHADE_DARK);
	else BIF_set_color(bc, COLORSHADE_WHITE);
	fdrawline(x1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1, x1, y2);
	
	/* outline */
	glColor3ub(0,0,0);
	ui_draw_outlineX(x1, y1, x2, y2, asp);

	
	/* code to draw side arrows as in iconrow */
	/* teken pijltjes, icon is standaard RGB */
	a= (y1+y2)/2;

	glColor3ub(0,0,0);
	sdrawline((short)(x1-1), (short)(a-2), (short)(x1-1), (short)(a+2));
	sdrawline((short)(x1-2), (short)(a-1), (short)(x1-2), (short)(a+1));
	sdrawline((short)(x1-3), a, (short)(x1-3), a);
	glColor3ub(255,255,255);
	sdrawline((short)(x1-3), (short)(a-1), (short)(x1-1), (short)(a-3));

	x2+=1;
	
	glColor3ub(0,0,0);
	sdrawline((short)(x2+1), (short)(a-2), (short)(x2+1), (short)(a+2));
	sdrawline((short)(x2+2), (short)(a-1), (short)(x2+2), (short)(a+1));
	sdrawline((short)(x2+3), a, (short)(x2+3), a);
	glColor3ub(255,255,255);
	sdrawline((short)(x2+3), (short)(a-1), (short)(x2+1), (short)(a-3));
}

void uiEmboss(float x1, float y1, float x2, float y2, int sel)
{
	
	/* below */
	if(sel) glColor3ub(255,255,255);
	else glColor3ub(0,0,0);
	fdrawline(x1, y1, x2, y1);

	/* right */
	fdrawline(x2, y1, x2, y2);
	
	/* top */
	if(sel) glColor3ub(0,0,0);
	else glColor3ub(255,255,255);
	fdrawline(x1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1, x1, y2);
	
}

/* super minimal button as used in logic menu */
static void ui_emboss_W(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{
	
	x1+= asp;
	x2-= asp;
	y1+= asp;
	y2-= asp;

	/* paper */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_DARK);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_HILITE);
		else BIF_set_color(bc, COLORSHADE_MEDIUM);
	}
	
	glRectf(x1, y1, x2, y2);

	if(flag & UI_SELECT) {
		BIF_set_color(bc, COLORSHADE_LIGHT);
		
		/* below */
		fdrawline(x1, y1, x2, y1);

		/* right */
		fdrawline(x2, y1, x2, y2);
	}
	else if(flag & UI_ACTIVE) {
		BIF_set_color(bc, COLORSHADE_WHITE);

		/* top */
		fdrawline(x1, y2, x2, y2);
	
		/* left */
		fdrawline(x1, y1, x1, y2);
	}
}

/* minimal button with small black outline */
static void ui_emboss_F(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{
	float asp1;
	
	/* paper */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_DARK);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_HILITE);
		else BIF_set_color(bc, COLORSHADE_MEDIUM);
	}
	
	glRectf(x1+1, y1+1, x2-1, y2-1);

	asp1= asp;

	x1+= asp1;
	x2-= asp1;
	y1+= asp1;
	y2-= asp1;

	/* below */
	if(flag & UI_SELECT) BIF_set_color(bc, COLORSHADE_WHITE);
	else BIF_set_color(bc, COLORSHADE_DARK);
	fdrawline(x1, y1, x2, y1);

	/* right */
	fdrawline(x2, y1, x2, y2);
	
	/* top */
	if(flag & UI_SELECT) BIF_set_color(bc, COLORSHADE_DARK);
	else BIF_set_color(bc, COLORSHADE_WHITE);
	fdrawline(x1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1, x1, y2);
	
	glColor3ub(0,0,0);
	fdrawbox(x1-asp1, y1-asp1, x2+asp1, y2+asp1);
}

/* minimal for menu's */
static void ui_emboss_M(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{
	x1+= 1.0;
	y1+= 1.0;
	x2-= 1.0+asp;
	y2-= 1.0+asp;
	
	
	if(flag & UI_SELECT) {
		BIF_set_color(bc, COLORSHADE_LIGHT);
		
		/* below */
		fdrawline(x1, y1, x2, y1);

		/* right */
		fdrawline(x2, y1, x2, y2);
	}
	else if(flag & UI_ACTIVE) {
		BIF_set_color(bc, COLORSHADE_WHITE);

		/* top */
		fdrawline(x1, y2, x2, y2);
	
		/* left */
		fdrawline(x1, y1, x1, y2);
	}
	else {
		BIF_set_color(bc, COLORSHADE_MEDIUM);
		
		fdrawbox(x1, y1, x2, y2);
	}
}


/* nothing! */	
static void ui_emboss_N(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int sel)
{
}

/* pulldown menu */
static void ui_emboss_P(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{
	
	BIF_set_color(bc, COLORSHADE_MEDIUM);
	glRectf(x1, y1, x2, y2);
	
	if(flag & UI_ACTIVE) {
		BIF_set_color(BUTMACTIVE, COLORSHADE_MEDIUM);
		glRectf(x1, y1, x2, y2);
	}
	
}

static void ui_emboss_slider(uiBut *but, float fac)
{
	float h;

	h= (but->y2-but->y1);

	BIF_set_color(but->col, COLORSHADE_DARK);
	glRectf(but->x1, but->y1, but->x2, but->y2);
	glColor3ub(0,0,0);
	ui_draw_outlineX(but->x1+1, but->y1+1, but->x2-1, but->y2-1, but->aspect);

	/* the box */
	if(but->flag & UI_SELECT) BIF_set_color(but->col, COLORSHADE_LIGHT);
	else BIF_set_color(but->col, COLORSHADE_GREY);
	glRects(but->x1+fac, but->y1+1, but->x1+fac+h, but->y2-1);

	BIF_set_color(but->col, COLORSHADE_WHITE);	
	fdrawline(but->x1+fac, but->y2-1, but->x1+fac+h, but->y2-1);
	fdrawline(but->x1+fac, but->y1+1, but->x1+fac, but->y2-1);

	glColor3ub(0,0,0);
	fdrawline(but->x1+fac, but->y1+1, but->x1+fac+h, but->y1+1);
	fdrawline(but->x1+fac+h, but->y1+1, but->x1+fac+h, but->y2-1);
}

static void ui_draw_but_BUT(uiBut *but)
{
	float x;
	
	but->embossfunc(but->col, but->aspect, but->x1, but->y1, but->x2, but->y2, but->flag);

	/* check for button text label */
	if (but->type == ICONTEXTROW) {
		ui_draw_icon(but, (BIFIconID) (but->icon+but->iconadd));
	}
	else if(but->drawstr[0]!=0) {
		
		/* If there's an icon too (made with uiDefIconTextBut) then draw the icon
		and offset the text label to accomodate it */
	        if ( (but->flag & UI_HAS_ICON) && (but->flag & UI_ICON_LEFT) ) {
			ui_draw_icon(but, but->icon);

			if(but->flag & UI_TEXT_LEFT) x= but->x1+24.0;
			else x= (but->x1+but->x2-but->strwidth+1)/2.0;
		}
		else {
		        if(but->flag & UI_TEXT_LEFT) x= but->x1+4.0;
			else x= (but->x1+but->x2-but->strwidth+1)/2.0;
		}
		
		if(but->flag & UI_SELECT) {
			glColor3ub(255,255,255);
		} else {
			glColor3ub(0,0,0);
		}

		glRasterPos2f( x, (but->y1+but->y2- 9.0)/2.0);

#ifdef INTERNATIONAL
		if(G.ui_international == TRUE)
			if(U.transopts & TR_BUTTONS)	// BUTTON TEXTS
				FTF_DrawString(but->drawstr+but->ofs, FTF_USE_GETTEXT | FTF_INPUT_UTF8, but->flag & UI_SELECT);
			else
				FTF_DrawString(but->drawstr+but->ofs, FTF_NO_TRANSCONV | FTF_INPUT_UTF8, but->flag & UI_SELECT);
		else
			BMF_DrawString(but->font, but->drawstr+but->ofs);
#else
		BMF_DrawString(but->font, but->drawstr+but->ofs);
#endif
	}
	/* if there's no text label, then check to see if there's an icon only and draw it */
	else if( but->flag & UI_HAS_ICON ) {
		ui_draw_icon(but, (BIFIconID) (but->icon+but->iconadd));
	}
}

static void ui_draw_but_TOG3(uiBut *but)
{
	float x, r, g, b;

	but->embossfunc(but->col, but->aspect, but->x1, but->y1, but->x2, but->y2, but->flag);
	
	if( but->flag & UI_HAS_ICON ) {
		ui_draw_icon(but, but->icon);
	}
	else if(but->drawstr[0]!=0) {
		if(but->flag & UI_SELECT) {
			int ok= 0;
			
			if( but->pointype==CHA ) {
				if( BTST( *(but->poin+2), but->bitnr )) ok= 1;
			}
			else if( but->pointype ==SHO ) {
				short *sp= (short *)but->poin;
				if( BTST( sp[1], but->bitnr )) ok= 1;
			}
			
			if (ok) {
				glColor3ub(255, 255, 0);
				r= g= 1.0;
				b= 0.0;
			} else {
				glColor3ub(255, 255, 255);
				r= g= b= 1.0;
			}
		} else {
			glColor3ub(0, 0, 0);
			r= g= b= 0.0;
		}

		if(but->flag & UI_TEXT_LEFT) x= but->x1+4.0;
		else x= (but->x1+but->x2-but->strwidth+1)/2.0;
		
		glRasterPos2f( x, (but->y1+but->y2- 9.0)/2.0);
		
#ifdef INTERNATIONAL
		if(G.ui_international == TRUE)
			if(U.transopts & TR_BUTTONS)	// BUTTON TEXTS
				FTF_DrawStringRGB(but->drawstr+but->ofs, FTF_USE_GETTEXT | FTF_INPUT_UTF8, r, g, b);
			else
				FTF_DrawStringRGB(but->drawstr+but->ofs, FTF_NO_TRANSCONV | FTF_INPUT_UTF8, r, g, b);
		else
			BMF_DrawString(but->font, but->drawstr+but->ofs);
#else
		BMF_DrawString(but->font, but->drawstr+but->ofs);
#endif
	}
}

static void ui_draw_but_TEX(uiBut *but)
{
	float x;
	short pos, sel, t;
	char ch;
	
	/* exception for text buttons using embossF */
	sel= but->flag;
	if(but->embossfunc==ui_emboss_F) sel |= UI_SELECT;
	
	but->embossfunc(but->col, but->aspect, but->x1, but->y1, but->x2, but->y2, sel);
	
	sel= but->flag & UI_SELECT;

	/* draw cursor */
	if(but->pos != -1) {
		
		pos= but->pos+strlen(but->str);
		if(pos >= but->ofs) {
			ch= but->drawstr[pos];
			but->drawstr[pos]= 0;
#ifdef INTERNATIONAL
			if(G.ui_international == TRUE)
				if(U.transopts & TR_BUTTONS)	// BUTTON TEXTS
					t= but->aspect*FTF_GetStringWidth(but->drawstr+but->ofs, FTF_USE_GETTEXT | FTF_INPUT_UTF8) + 3;
				else
					t= but->aspect*FTF_GetStringWidth(but->drawstr+but->ofs, FTF_NO_TRANSCONV | FTF_INPUT_UTF8) + 3;
			else
				t= but->aspect*BMF_GetStringWidth(but->font, but->drawstr+but->ofs) + 3;
#else
			t= but->aspect*BMF_GetStringWidth(but->font, but->drawstr+but->ofs) + 3;
#endif

			but->drawstr[pos]= ch;
			glColor3ub(255,0,0);
	
			glRects(but->x1+t, but->y1+2, but->x1+t+3, but->y2-2);
		}	
	}
	if(but->drawstr[0]!=0) {
		if(sel) glColor3ub(255,255,255);
		else glColor3ub(0,0,0);

		if(but->flag & UI_TEXT_LEFT) x= but->x1+4.0;
		else x= (but->x1+but->x2-but->strwidth+1)/2.0;
		
		glRasterPos2f( x, (but->y1+but->y2- 9.0)/2.0);
		
#ifdef INTERNATIONAL
		if(G.ui_international == TRUE)
			if(U.transopts & TR_BUTTONS)	// BUTTON TEXTS
				FTF_DrawString(but->drawstr+but->ofs, FTF_USE_GETTEXT | FTF_INPUT_UTF8, sel);
			else
				FTF_DrawString(but->drawstr+but->ofs, FTF_NO_TRANSCONV | FTF_INPUT_UTF8, sel);
		else
			BMF_DrawString(but->font, but->drawstr+but->ofs);
#else
		BMF_DrawString(but->font, but->drawstr+but->ofs);
#endif
	}
}

static void ui_draw_but_BUTM(uiBut *but)
{
	float x;
	short len;
	char *cpoin;
	int sel;
	
	but->embossfunc(but->col, but->aspect, but->x1, but->y1, but->x2, but->y2, but->flag);
	
	/* check for button text label */
	if(but->drawstr[0]!=0) {
		
		cpoin= strchr(but->drawstr, '|');
		if(cpoin) *cpoin= 0;

		if(but->embossfunc==ui_emboss_P) {
			if(but->flag & UI_ACTIVE) {
				glColor3ub(255,255,255);
				sel = 1;
			} else {
				glColor3ub(0,0,0);
				sel = 0;
			}
		}
		else {
			glColor3ub(0,0,0);
			sel = 0;
		}

		/* If there's an icon too (made with uiDefIconTextBut) then draw the icon
		and offset the text label to accomodate it */
		if ( (but->flag & UI_HAS_ICON) && (but->flag & UI_ICON_LEFT) ) {
			ui_draw_icon(but, but->icon);

			x= but->x1+24.0;
		}
		else {
		        x= but->x1+4.0;
		}

		glRasterPos2f( x, (but->y1+but->y2- 9.0)/2.0);

#ifdef INTERNATIONAL
		if(G.ui_international == TRUE) {
			if(U.transopts & TR_BUTTONS) {	// BUTTON TEXTS
				FTF_DrawString(but->drawstr, FTF_USE_GETTEXT | FTF_INPUT_UTF8, sel);
			}
			else {
				FTF_DrawString(but->drawstr, FTF_NO_TRANSCONV | FTF_INPUT_UTF8, sel);
			}
		}
		else {
			BMF_DrawString(but->font, but->drawstr);
		}
#else
		BMF_DrawString(but->font, but->drawstr);
#endif
		
		if(cpoin) {
#ifdef INTERNATIONAL
			if(G.ui_international == TRUE) {
				if(U.transopts & TR_BUTTONS) {	// BUTTON TEXTS
					len= FTF_GetStringWidth(cpoin+1, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
					glRasterPos2f( but->x2 - len*but->aspect-3, (but->y1+but->y2- 9.0)/2.0);
					FTF_DrawString(cpoin+1, FTF_USE_GETTEXT | FTF_INPUT_UTF8, but->flag & UI_ACTIVE);
					*cpoin= '|';
				}
				else {
					len= FTF_GetStringWidth(cpoin+1, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
					glRasterPos2f( but->x2 - len*but->aspect-3, (but->y1+but->y2- 9.0)/2.0);
					FTF_DrawString(cpoin+1, FTF_NO_TRANSCONV | FTF_INPUT_UTF8, but->flag & UI_ACTIVE);
					*cpoin= '|';
				}
			}
			else {
				len= BMF_GetStringWidth(but->font, cpoin+1);
				glRasterPos2f( but->x2 - len*but->aspect-3, (but->y1+but->y2- 9.0)/2.0);
				BMF_DrawString(but->font, cpoin+1);
				*cpoin= '|';
			}
#else
			len= BMF_GetStringWidth(but->font, cpoin+1);
			glRasterPos2f( but->x2 - len*but->aspect-3, (but->y1+but->y2- 9.0)/2.0);
			BMF_DrawString(but->font, cpoin+1);
			*cpoin= '|';
#endif
		}
	}
	/* if there's no text label, then check to see if there's an icon only and draw it */
	else if( but->flag & UI_HAS_ICON ) {
		ui_draw_icon(but, but->icon);
	}
}

static void ui_draw_but_LABEL(uiBut *but)
{
	float x;
	int sel;

	sel= but->min!=0.0;

	if(sel) glColor3ub(255,255,255);
	else glColor3ub(0,0,0);
	
	/* check for button text label */
	if(but->drawstr[0]!=0) {

	        /* If there's an icon too (made with uiDefIconTextBut) then draw the icon
		and offset the text label to accomodate it */
		if ( (but->flag & UI_HAS_ICON) && (but->flag & UI_ICON_LEFT) ) {
			ui_draw_icon(but, but->icon);

			if(but->flag & UI_TEXT_LEFT) x= but->x1+24.0;
			else x= (but->x1+but->x2-but->strwidth+1)/2.0;
		}
		else {
		        if(but->flag & UI_TEXT_LEFT) x= but->x1+4.0;
			else x= (but->x1+but->x2-but->strwidth+1)/2.0;
		}

		glRasterPos2f( x, (but->y1+but->y2- 9.0)/2.0);

#ifdef INTERNATIONAL
		if(G.ui_international == TRUE) {
			if(U.transopts & TR_BUTTONS) {	// BUTTON TEXTS
				FTF_DrawString(but->drawstr+but->ofs, FTF_USE_GETTEXT | FTF_INPUT_UTF8, sel);
			}
			else {
				FTF_DrawString(but->drawstr+but->ofs, FTF_NO_TRANSCONV | FTF_INPUT_UTF8, sel);
			}
		}
		else {
			BMF_DrawString(but->font, but->drawstr+but->ofs);
		}
#else
		BMF_DrawString(but->font, but->drawstr+but->ofs);
#endif
	}
	/* if there's no text label, then check to see if there's an icon only and draw it */
	else if( but->flag & UI_HAS_ICON ) {
		ui_draw_icon(but, but->icon);
	}
}

static void ui_draw_but_SEPR(uiBut *but)
{
	float y= (but->y1+but->y2)/2.0;
	
	glColor3ub(0,0,0);
	fdrawline(but->x1, y+but->aspect, but->x2, y+but->aspect);
	glColor3ub(255,255,255);
	fdrawline(but->x1, y, but->x2, y);
}

static void ui_draw_but_LINK(uiBut *but)
{
	ui_draw_icon(but, but->icon);
}


static void ui_draw_but(uiBut *but)
{
	double value;
	float fac, x1, y1, x2, y2, *fp;
	short a;
	char colr, colg, colb;
	
	if(but==0) return;

	if(but->block->frontbuf==UI_NEED_DRAW_FRONT) {
		but->block->frontbuf= UI_HAS_DRAW_FRONT;
	
		glDrawBuffer(GL_FRONT);
		if(but->win==curarea->headwin) curarea->head_swap= WIN_FRONT_OK;
		else curarea->win_swap= WIN_FRONT_OK;
	}
	
	switch (but->type) {

	case BUT: 
	case ROW: 
	case TOG: 
	case TOGR: 
	case TOGN:
	case ICONTOG: 
	case NUM:
	case KEYEVT:
	case IDPOIN:
		ui_draw_but_BUT(but);
		break;
	
	case TEX:
		ui_draw_but_TEX(but);
		break;

	case BUTM:
	case BLOCK:
		ui_draw_but_BUTM(but);
		break;
		
	case ICONROW:
		ui_draw_but_BUT(but);
		
		/* draw arriws, icon is standard RGB */
		a= (but->y1+but->y2)/2;
		
		glColor3ub(0,0,0);
		sdrawline((short)(but->x1-1), (short)(a-2), (short)(but->x1-1), (short)(a+2));
		sdrawline((short)(but->x1-2), (short)(a-1), (short)(but->x1-2), (short)(a+1));
		sdrawline((short)(but->x1-3), a, (short)(but->x1-3), a);
		glColor3ub(255,255,255);
		sdrawline((short)(but->x1-3), (short)(a-1), (short)(but->x1-1), (short)(a-3));
		
		glColor3ub(0,0,0);
		sdrawline((short)(but->x2+1), (short)(a-2), (short)(but->x2+1), (short)(a+2));
		sdrawline((short)(but->x2+2), (short)(a-1), (short)(but->x2+2), (short)(a+1));
		sdrawline((short)(but->x2+3), a, (short)(but->x2+3), a);
		glColor3ub(255,255,255);
		sdrawline((short)(but->x2+3), (short)(a-1), (short)(but->x2+1), (short)(a-3));

		break;
		
	case ICONTEXTROW:
		ui_draw_but_BUT(but);
		
		/* teken pijltjes, icon is standaard RGB */
		a= (but->y1+but->y2)/2;
		
		glColor3ub(0,0,0);
		sdrawline((short)(but->x1-1), (short)(a-2), (short)(but->x1-1), (short)(a+2));
		sdrawline((short)(but->x1-2), (short)(a-1), (short)(but->x1-2), (short)(a+1));
		sdrawline((short)(but->x1-3), a, (short)(but->x1-3), a);
		glColor3ub(255,255,255);
		sdrawline((short)(but->x1-3), (short)(a-1), (short)(but->x1-1), (short)(a-3));

		glColor3ub(0,0,0);
		sdrawline((short)(but->x2+1), (short)(a-2), (short)(but->x2+1), (short)(a+2));
		sdrawline((short)(but->x2+2), (short)(a-1), (short)(but->x2+2), (short)(a+1));
		sdrawline((short)(but->x2+3), a, (short)(but->x2+3), a);
		glColor3ub(255,255,255);
		sdrawline((short)(but->x2+3), (short)(a-1), (short)(but->x2+1), (short)(a-3));

		break;

	case MENU:
	
		ui_draw_but_BUT(but);

		/* when sufficient space: darw symbols */
		if(but->strwidth+10 < but->x2-but->x1) {
			int h;
			
			h= but->y2- but->y1;
			x1= but->x2-0.66*h; x2= x1+.33*h;
			y1= but->y1+.42*h; y2= y1+.16*h;
		
			glColor3ub(0,0,0);
			glRecti(x1,  y1,  x2,  y2);
			glColor3ub(255,255,255);
			glRecti(x1-1,  y1+1,  x2-1,  y2+1);
		}
		break;
		
	case NUMSLI:
	case HSVSLI:
	
		ui_draw_but_BUT(but);
		
		/* the slider */

		x1= but->x1; x2= but->x2;
		y1= but->y1; y2= but->y2;
		
		but->x1= (but->x1+but->x2)/2;
		but->x2-= 9;
		but->y1= -2+(but->y1+but->y2)/2;
		but->y2= but->y1+6;
		
		value= ui_get_but_val(but);
		fac= (value-but->min)*(but->x2-but->x1-but->y2+but->y1)/(but->max - but->min);
		ui_emboss_slider(but, fac);
		
		but->x1= x1; but->x2= x2;
		but->y1= y1; but->y2= y2;
		
		break;
		
	case TOG3:
		ui_draw_but_TOG3(but);
		break;

	case LABEL:
		ui_draw_but_LABEL(but);
		break;

	case SLI:
		break;

	case SCROLL:
		break;
		
	case SEPR:
		ui_draw_but_SEPR(but);
		break;
		
	case COL:
		ui_draw_but_BUT(but);
		
		if( but->pointype==FLO ) {
			fp= (float *)but->poin;
			colr= floor(255.0*fp[0]+0.5);
			colg= floor(255.0*fp[1]+0.5);
			colb= floor(255.0*fp[2]+0.5);
		}
		else {
			char *cp= (char *)but->poin;
			colr= cp[0];
			colg= cp[1];
			colb= cp[2];
		}
		glColor3ub(colr,  colg,  colb);
		glRects((short)(but->x1+2), (short)(but->y1+2), (short)(but->x2-2), (short)(but->y2-2));
		break;

	case LINK:
		ui_draw_but_LINK(but);
		break;

	case INLINK:
		ui_draw_but_LINK(but);
		break;
	}
}

void uiDrawMenuBox(float minx, float miny, float maxx, float maxy)
{
	glRectf(minx, miny, maxx, maxy);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glColor4ub(0, 0, 0, 100);
	fdrawline(minx+4, miny-1, maxx+1, miny-1);
	fdrawline(maxx+1, miny-1, maxx+1, maxy-4);

	glColor4ub(0, 0, 0, 75);
	fdrawline(minx+4, miny-2, maxx+2, miny-2);
	fdrawline(maxx+2, miny-2, maxx+2, maxy-4);

	glColor4ub(0, 0, 0, 50);
	fdrawline(minx+4, miny-3, maxx+3, miny-3);
	fdrawline(maxx+3, miny-3, maxx+3, maxy-4);

	glDisable(GL_BLEND);
	
	/* below */
	glColor3ub(0,0,0);
	fdrawline(minx, miny, maxx, miny);

	/* right */
	fdrawline(maxx, miny, maxx, maxy);
	
	/* top */
	glColor3ub(255,255,255);
	fdrawline(minx, maxy, maxx, maxy);

	/* left */
	fdrawline(minx, miny, minx, maxy);
}

static void ui_draw_linkline(uiBut *but, uiLinkLine *line)
{
	float vec1[2], vec2[2];

	if(line->from==NULL || line->to==NULL) return;
	
	if(but->block->frontbuf==UI_NEED_DRAW_FRONT) {
		but->block->frontbuf= UI_HAS_DRAW_FRONT;
	
		glDrawBuffer(GL_FRONT);
		if(but->win==curarea->headwin) curarea->head_swap= WIN_FRONT_OK;
		else curarea->win_swap= WIN_FRONT_OK;
	}

	vec1[0]= (line->from->x1+line->from->x2)/2.0;
	vec1[1]= (line->from->y1+line->from->y2)/2.0;
	vec2[0]= (line->to->x1+line->to->x2)/2.0;
	vec2[1]= (line->to->y1+line->to->y2)/2.0;
	
	if(line->flag & UI_SELECT) BIF_set_color(but->col, COLORSHADE_LIGHT);
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



/* ******************* block calc ************************* */

void uiTextBoundsBlock(uiBlock *block, int addval)
{
	uiBut *bt;
	int i = 0, j;
	
	bt= block->buttons.first;
	while(bt) {
		if(bt->type!=SEPR) {
#ifdef INTERNATIONAL
		if(G.ui_international == TRUE)
			if(U.transopts & TR_BUTTONS)
				j= FTF_GetStringWidth(bt->drawstr, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
			else
				j= FTF_GetStringWidth(bt->drawstr, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
		else
			j= BMF_GetStringWidth(bt->font, bt->drawstr);
#else
		j= BMF_GetStringWidth(bt->font, bt->drawstr);
#endif
		if(j > i) i = j;
		}
		bt= bt->next;
	}

	
	bt= block->buttons.first;
	while(bt) {
		bt->x2 = i + addval;
		bt= bt->next;
	}
}


void uiBoundsBlock(uiBlock *block, int addval)
{
	uiBut *bt;
	
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

static void ui_positionblock(uiBlock *block, uiBut *but)
{
	/* position block relative to but */
	uiBut *bt;
	int xsize, ysize, xof=0, yof=0;
	
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
	
	block->minx-= 2.0; block->miny-= 2.0;
	block->maxx+= 2.0; block->maxy+= 2.0;
	
	xsize= block->maxx - block->minx;
	ysize= block->maxy - block->miny;
	
	if(but) {
		rctf butrct;
		short left=0, right=0, top=0, down=0;
		short dir1, dir2 = 0;

		butrct.xmin= but->x1; butrct.xmax= but->x2;
		butrct.ymin= but->y1; butrct.ymax= but->y2;

		/* added this for submenu's... */
		Mat4CpyMat4(UIwinmat, block->winmat);

		ui_graphics_to_window(block->win, &butrct.xmin, &butrct.ymin);
		ui_graphics_to_window(block->win, &butrct.xmax, &butrct.ymax);

		if( butrct.xmin-xsize > 0.0) left= 1;
		if( butrct.xmax+xsize < G.curscreen->sizex) right= 1;
		if( butrct.ymin-ysize > 0.0) down= 1;
		if( butrct.ymax+ysize < G.curscreen->sizey) top= 1;
		
		dir1= block->direction;
		if(dir1==UI_LEFT || dir1==UI_RIGHT) dir2= UI_DOWN;
		if(dir1==UI_TOP || dir1==UI_DOWN) dir2= UI_LEFT;
		
		if(dir1==UI_LEFT && left==0) dir1= UI_RIGHT;
		if(dir1==UI_RIGHT && right==0) dir1= UI_LEFT;
			/* this is aligning, not append! */
		if(dir2==UI_LEFT && right==0) dir2= UI_RIGHT;
		if(dir2==UI_RIGHT && left==0) dir2= UI_LEFT;
		
		if(dir1==UI_TOP && top==0) dir1= UI_DOWN;
		if(dir1==UI_DOWN && down==0) dir1= UI_TOP;
		if(dir2==UI_TOP && top==0) dir2= UI_DOWN;
		if(dir2==UI_DOWN && down==0) dir2= UI_TOP;

		if(dir1==UI_LEFT) {
			xof= but->x1 - block->maxx;
			if(dir2==UI_TOP) yof= but->y1 - block->miny;
			else yof= but->y2 - block->maxy;
		}
		else if(dir1==UI_RIGHT) {
			xof= but->x2 - block->minx;
			if(dir2==UI_TOP) yof= but->y1 - block->miny;
			else yof= but->y2 - block->maxy;
		}
		else if(dir1==UI_TOP) {
			yof= but->y2 - block->miny+1;
			if(dir2==UI_RIGHT) xof= but->x2 - block->maxx;
			else xof= but->x1 - block->minx;
		}
		else if(dir1==UI_DOWN) {
			yof= but->y1 - block->maxy-1;
			if(dir2==UI_RIGHT) xof= but->x2 - block->maxx;
			else xof= but->x1 - block->minx;
		}



		// apply requested offset in the block

		xof += block->xofs;

		yof += block->yofs;
		
	}
	
	/* apply */
	bt= block->buttons.first;
	while(bt) {
		bt->x1 += xof;
		bt->x2 += xof;
		bt->y1 += yof;
		bt->y2 += yof;
		
		ui_graphics_to_window(block->win, &bt->x1, &bt->y1);
		ui_graphics_to_window(block->win, &bt->x2, &bt->y2);

		bt->aspect= 1.0;
		
		bt= bt->next;
	}
	
	block->minx += xof;
	block->miny += yof;
	block->maxx += xof;
	block->maxy += yof;
	
	ui_graphics_to_window(block->win, &block->minx, &block->miny);
	ui_graphics_to_window(block->win, &block->maxx, &block->maxy);

}


static void ui_autofill(uiBlock *block)
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
	
	MEM_freeN(maxw); MEM_freeN(maxh);	
	block->autofill= 0;
}

static void ui_drawblock_int(uiBlock *block)
{
	uiBut *but;

	if(block->autofill) ui_autofill(block);
	if(block->minx==0.0 && block->maxx==0.0) uiBoundsBlock(block, 0);

	if(block->flag & UI_BLOCK_LOOP) {
		BIF_set_color(block->col, COLORSHADE_HILITE);
		uiDrawMenuBox(block->minx, block->miny, block->maxx, block->maxy);
	}
	
	for (but= block->buttons.first; but; but= but->next) {
		ui_draw_but(but);
	}

	ui_draw_links(block);
	
}

void uiDrawBlock(uiBlock *block)
{
	ui_drawblock_int(block);
}

/* ************* MENUBUTS *********** */

typedef struct {
	char *str;
	int retval;
} MenuEntry;

typedef struct {
	char *instr;
	char *title;
	
	MenuEntry *items;
	int nitems, itemssize;
} MenuData;

static MenuData *menudata_new(char *instr) {
	MenuData *md= MEM_mallocN(sizeof(*md), "MenuData");

	md->instr= instr;
	md->title= NULL;
	md->items= NULL;
	md->nitems= md->itemssize= 0;
	
	return md;
}

static void menudata_set_title(MenuData *md, char *title) {
	if (!md->title)
		md->title= title;
}

static void menudata_add_item(MenuData *md, char *str, int retval) {
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
	int nretval= 1, nitem_is_title= 0;
	
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
			}
		} else if (c=='|' || c=='\0') {
			if (nitem) {
				*s= '\0';

				if (nitem_is_title) {
					menudata_set_title(md, nitem);
					nitem_is_title= 0;
				} else {
					menudata_add_item(md, nitem, nretval);
					nretval= md->nitems+1;
				} 
				
				nitem= NULL;
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


static int ui_do_but_MENU(uiBut *but)
{
	uiBlock *block;
	ListBase listb={NULL, NULL};
	double fvalue;
	int width, height, a, xmax, ymax, starty, endx, endy;
	short startx;
	int columns=1, rows=0, boxh, event;
	short  x1, y1;
	short mval[2], mousemove[2];
	MenuData *md;

	but->flag |= UI_SELECT;
	ui_draw_but(but);

	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;

	md= decompose_menu_string(but->str);

	/* columns and row calculation */
	columns= (md->nitems+20)/20;
	if (columns<1) columns= 1;
	
	rows= (int) md->nitems/columns;
	if (rows<1) rows= 1;
	
	while (rows*columns<md->nitems) rows++;
		
	/* size and location */
#ifdef INTERNATIONAL
	if(G.ui_international == TRUE) {
		if(md->title)
			if(U.transopts & TR_MENUS)
				width= 2*strlen(md->title)+FTF_GetStringWidth(md->title, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
			else
				width= 2*strlen(md->title)+FTF_GetStringWidth(md->title, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
		else
			width= 0;
	} else {
		if(md->title)
			width= 2*strlen(md->title)+BMF_GetStringWidth(block->curfont, md->title);
		else
			width= 0;
	}
#else
	if(md->title)
		width= 2*strlen(md->title)+BMF_GetStringWidth(block->curfont, md->title);
	else
		width= 0;
#endif
	for(a=0; a<md->nitems; a++) {
#ifdef INTERNATIONAL
		if(G.ui_international == TRUE)
			if(U.transopts & TR_MENUS)
				xmax= FTF_GetStringWidth(md->items[a].str, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
			else
				xmax= FTF_GetStringWidth(md->items[a].str, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
		else
			xmax= BMF_GetStringWidth(block->curfont, md->items[a].str);
#else
		xmax= BMF_GetStringWidth(block->curfont, md->items[a].str);
#endif
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
	fvalue= ui_get_but_val(but);
	for(a=0; a<md->nitems; a++) {
		if( md->items[a].retval== (int)fvalue ) break;
	}
	/* no active item? */
	if(a==md->nitems) {
		if(md->title) a= -1;
		else a= 0;
	}

	if(a>0) startx = mval[0]-width/2 - ((int)(a)/rows)*width;
	else startx= mval[0]-width/2;
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

	warp_pointer(mval[0]+mousemove[0], mval[1]+mousemove[1]);

	mousemove[0]= mval[0];
	mousemove[1]= mval[1];

	/* here we go! */

	if(md->title) {
		uiBut *bt;
		uiSetCurFont(block, block->font+1);
		bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+rows*boxh), (short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
		uiSetCurFont(block, block->font);
		bt->flag= UI_TEXT_LEFT;
	}

	for(a=0; a<md->nitems; a++) {

		x1= startx + width*((int)a/rows);
		y1= starty - boxh*(a%rows) + (rows-1)*boxh; 

		if (strcmp(md->items[a].str, "%l")==0) {
			uiDefBut(block, SEPR, B_NOP, "", x1, y1,(short)(width-(rows>1)), (short)(boxh-1), NULL, 0.0, 0.0, 0, 0, "");
		}
		else {
			uiDefBut(block, BUTM|but->pointype, but->retval, md->items[a].str, x1, y1,(short)(width-(rows>1)), (short)(boxh-1), but->poin, (float) md->items[a].retval, 0.0, 0, 0, "");
		}
	}
	
	uiBoundsBlock(block, 3);

	event= uiDoBlocks(&listb, 0);
	
	menudata_free(md);
	
	if((event & UI_RETURN_OUT)==0) warp_pointer(mousemove[0], mousemove[1]);
	
	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);
	
	uibut_do_func(but);

	return event;	
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

	if( but->type==TOGN ) true= 0;

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
		case ICONTOG:
			if(value!=0.0) push= 1;
			break;
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

		if (but->flag != oflag)
			ui_draw_but(but);
			
		PIL_sleep_ms(1);
	} while (get_mbut() & L_MOUSE);

	activated= (but->flag & UI_SELECT);

	if(activated) {
		uibut_do_func(but);
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

	do {
		event= extern_qread(&val);
	} while (!event || !val || ELEM(event, MOUSEX, MOUSEY));

	if (!key_event_to_string(event)[0]) event= 0;

	ui_set_but_val(but, (double) event);
	ui_check_but(but);
	ui_draw_but(but);
	
	return (event!=0);
}

static int ui_do_but_TOG(uiBlock *block, uiBut *but)
{
	uiBut *bt;
	double value;
	int w, lvalue, push;
	
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
		if(but->type==ICONTOG) ui_check_but(but);
		ui_draw_but(but);
	}
	else {
		
		if(value==0.0) push= 1; 
		else push= 0;
		
		if(but->type==TOGN) push= !push;
		ui_set_but_val(but, (double)push);
		if(but->type==ICONTOG) ui_check_but(but);		
		ui_draw_but(but);
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

static int ui_do_but_TEX(uiBut *but)
{
	unsigned short dev;
	short x, mval[2], len=0, dodraw;
	char *str, backstr[UI_MAX_DRAW_STR];
	
	str= (char *)but->poin;
	
	but->flag |= UI_SELECT;

	uiGetMouse(mywinget(), mval);

	/* calculate cursor pos with current mousecoords */
	BLI_strncpy(backstr, but->drawstr, UI_MAX_DRAW_STR);
	but->pos= strlen(backstr)-but->ofs;
#ifdef INTERNATIONAL
	if(G.ui_international == TRUE)
		if(U.transopts & TR_BUTTONS)
			while((but->aspect*FTF_GetStringWidth(backstr+but->ofs, FTF_USE_GETTEXT | FTF_INPUT_UTF8) + but->x1) > mval[0]) {
				if (but->pos <= 0) break;
				but->pos--;
				backstr[but->pos+but->ofs] = 0;
			}
		else
			while((but->aspect*FTF_GetStringWidth(backstr+but->ofs, FTF_NO_TRANSCONV | FTF_INPUT_UTF8) + but->x1) > mval[0]) {
				if (but->pos <= 0) break;
				but->pos--;
				backstr[but->pos+but->ofs] = 0;
			}
	else
		while((but->aspect*BMF_GetStringWidth(but->font, backstr+but->ofs) + but->x1) > mval[0]) {
			if (but->pos <= 0) break;
			but->pos--;
			backstr[but->pos+but->ofs] = 0;
		}
#else
	while((but->aspect*BMF_GetStringWidth(but->font, backstr+but->ofs) + but->x1) > mval[0]) {
		if (but->pos <= 0) break;
		but->pos--;
		backstr[but->pos+but->ofs] = 0;
	}
#endif
	
	but->pos -= strlen(but->str);
	but->pos += but->ofs;
	if(but->pos<0) but->pos= 0;

	/* backup */
	BLI_strncpy(backstr, but->poin, UI_MAX_DRAW_STR);

	ui_draw_but(but);

	while (get_mbut() & L_MOUSE) BIF_wait_for_statechange();
	len= strlen(str);
	but->min= 0.0;
	
	while(TRUE) {
		char ascii;
		short val;

		dodraw= 0;
		dev = extern_qread_ext(&val, &ascii);

		if(dev==INPUTCHANGE) break;
		else if(get_mbut() & L_MOUSE) break;
		else if(get_mbut() & R_MOUSE) break;
		else if(dev==ESCKEY) break;
		else if(dev==MOUSEX) val= 0;
		else if(dev==MOUSEY) val= 0;

		if(ascii) {
			if( ascii>31 && ascii<127) {
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
		
			if(dev==RIGHTARROWKEY) {
				if(G.qual & LR_SHIFTKEY) but->pos= strlen(str);
				else but->pos++;
				if(but->pos>strlen(str)) but->pos= strlen(str);
				dodraw= 1;
			}
			else if(dev==LEFTARROWKEY) {
				if(G.qual & LR_SHIFTKEY) but->pos= 0;
				else if(but->pos>0) but->pos--;
				dodraw= 1;
			}
			else if(dev==PADENTER || dev==RETKEY) {
				break;
			}
			else if(dev==DELKEY) {
					if(but->pos>=0 && but->pos<strlen(str)) {
							for(x=but->pos; x<=strlen(str); x++)
									str[x]= str[x+1];
							str[--len]='\0';
							dodraw= 1;
					}
			}
			else if(dev==BACKSPACEKEY) {
				if(len!=0) {
					if(get_qual() & LR_SHIFTKEY) {
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
			}
		}
		if(dodraw) {
			ui_check_but(but);
			ui_draw_but(but);
			glFinish(); // flush display in subloops
		}
	}
	
	if(dev==ESCKEY) strcpy(but->poin, backstr);
	but->pos= -1;
	but->flag &= ~UI_SELECT;

	uibut_do_func(but);

	ui_check_but(but);
	ui_draw_but(but);
	
	if(dev!=ESCKEY) return but->retval;
	else return 0;
}


static int uiActAsTextBut(uiBut *but)
{
	double value;
	float min, max;
	int temp, retval, textleft;
	char str[UI_MAX_DRAW_STR], *point;
	
	
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
	point= but->poin;
	but->poin= str;
	min= but->min;
	max= but->max;
	but->min= 0.0;
	but->max= 15.0;
	temp= but->type;
	but->type= TEX;
	textleft= but->flag & UI_TEXT_LEFT;
	but->flag |= UI_TEXT_LEFT;
	ui_check_but(but);
	
	retval= ui_do_but_TEX(but);
	
	but->type= temp;
	but->poin= point;
	but->min= min;
	but->max= max;
	if(textleft==0) but->flag &= ~UI_TEXT_LEFT;

	if( but->pointype==FLO ) value= atof(str);
	else value= atoi(str);

	if(value<min) value= min;
	if(value>max) value= max;

	ui_set_but_val(but, value);
	ui_check_but(but);
	ui_draw_but(but);
	
	return retval;
}

static int ui_do_but_NUM(uiBut *but)
{
	double value;
	float deler, fstart, f, tempf;
	int lvalue, temp; /*  , firsttime=1; */
	short qual, sx, mval[2], pos=0;
	

	but->flag |= UI_SELECT;
	ui_draw_but(but);
	
	uiGetMouse(mywinget(), mval);
	value= ui_get_but_val(but);
	
	sx= mval[0];
	fstart= (value - but->min)/(but->max-but->min);
	f= fstart;
	
	temp= (int)value;
	tempf= value;
	
	if(get_qual() & LR_SHIFTKEY) {	/* make it textbut */
		if( uiActAsTextBut(but) ) return but->retval;
		else return 0;
	}
	
	/* firsttime: this button can be approached with enter as well */
	while (get_mbut() & L_MOUSE) {
		qual= get_qual();
		
		deler= 500;
		if( but->pointype!=FLO ) {

			if( (but->max-but->min)<100 ) deler= 200.0;
			if( (but->max-but->min)<25 ) deler= 50.0;

		}
		if(qual & LR_SHIFTKEY) deler*= 10.0;
		if(qual & LR_ALTKEY) deler*= 20.0;

		uiGetMouse(mywinget(), mval);
		
		if(mval[0] != sx) {
		
			f+= ((float)(mval[0]-sx))/deler;
			if(f>1.0) f= 1.0;
			if(f<0.0) f= 0.0;
			sx= mval[0];
			tempf= ( but->min + f*(but->max-but->min));
			
			if( but->pointype!=FLO ) {
				
				temp= floor(tempf+.5);
				
				if(tempf==but->min || tempf==but->max);
				else if(qual & LR_CTRLKEY) temp= 10*(temp/10);
				
				if( temp>=but->min && temp<=but->max) {
				
					value= ui_get_but_val(but);
					lvalue= (int)value;
					
					if(temp != lvalue ) {
						pos= 1;
						ui_set_but_val(but, (double)temp);
						ui_check_but(but);
						ui_draw_but(but);
						glFinish(); // flush display in subloops
						
						uibut_do_func(but);
					}
				}

			}
			else {
				temp= 0;
				if(qual & LR_CTRLKEY) {
					if(tempf==but->min || tempf==but->max);
					else if(but->max-but->min < 2.10) tempf= 0.1*floor(10*tempf);
					else if(but->max-but->min < 21.0) tempf= floor(tempf);
					else tempf= 10.0*floor(tempf/10.0);
				}

				if( tempf>=but->min && tempf<=but->max) {
					value= ui_get_but_val(but);
					
					if(tempf != value ) {
						pos= 1;
						ui_set_but_val(but, tempf);
						ui_check_but(but);
						ui_draw_but(but);
						glFinish(); // flush display in subloops
					}
				}

			}
		}
		BIF_wait_for_statechange();
	}
	
	if(pos==0) {  /* plus 1 or minus 1 */
		if( but->pointype!=FLO ) {

			if(sx<(but->x1+but->x2)/2) temp--;
			else temp++;
			
			if( temp>=but->min && temp<=but->max)
				ui_set_but_val(but, (double)temp);

		}
		else {
		
			if(sx<(but->x1+but->x2)/2) tempf-= 0.01*but->a1;
				else tempf+= 0.01*but->a1;

			if (tempf < but->min) tempf = but->min;
			if (tempf > but->max) tempf = but->max;

			ui_set_but_val(but, tempf);

		}
	}
	
	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);	
	glFinish(); // flush display in subloops
	
	return but->retval;
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
	
	but->flag |= UI_SELECT;
	ui_draw_but(but);
	
	/* here we go! */
	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, but->win);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;

	for(a=(int)but->min; a<=(int)but->max; a++) {
		uiDefIconBut(block, BUTM|but->pointype, but->retval, but->icon+(a-but->min), 0, (short)(18*a), (short)(but->x2-but->x1-4), 18, but->poin, (float)a, 0.0, 0, 0, "");
	}
	block->direction= UI_TOP;	
	ui_positionblock(block, but);
	
	/* the block is made with but-win, but is handled in mainwin space...
	   this is needs better implementation */
	block->win= G.curscreen->mainwin;
	
	uiDoBlocks(&listb, 0);

	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);	
	
	return but->retval;
}

static int ui_do_but_ICONTEXTROW(uiBut *but)
{
	uiBlock *block;
	ListBase listb={NULL, NULL};
	int width, a, xmax, ypos;
	MenuData *md;

	but->flag |= UI_SELECT;
	ui_draw_but(but);

	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, but->win);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;

	md= decompose_menu_string(but->str);

	/* size and location */
	/* expand menu width to fit labels */
#ifdef INTERNATIONAL
	if(G.ui_international == TRUE) {
		if(md->title)
			if(U.transopts & TR_MENUS)
				width= 2*strlen(md->title)+FTF_GetStringWidth(md->title, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
			else
				width= 2*strlen(md->title)+FTF_GetStringWidth(md->title, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
		else
			width= 0;
	} else {
		if(md->title)
			width= 2*strlen(md->title)+BMF_GetStringWidth(block->curfont, md->title);
		else
			width= 0;
	}
#else
	if(md->title)
		width= 2*strlen(md->title)+BMF_GetStringWidth(block->curfont, md->title);
	else
		width= 0;
#endif
	for(a=0; a<md->nitems; a++) {
#ifdef INTERNATIONAL
		if(G.ui_international == TRUE)
			if(U.transopts & TR_MENUS)
				xmax= FTF_GetStringWidth(md->items[a].str, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
			else
				xmax= FTF_GetStringWidth(md->items[a].str, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
		else
			xmax= BMF_GetStringWidth(block->curfont, md->items[a].str);
#else
		xmax= BMF_GetStringWidth(block->curfont, md->items[a].str);
#endif
		if(xmax>width) width= xmax;
	}

	width+= 30;
	if (width<50) width=50;

	ypos = 0;

	/* loop through the menu options and draw them out with icons & text labels */
	for(a=0; a<md->nitems; a++) {

		/* add a space if there's a separator (%l) */
	        if (strcmp(md->items[a].str, "%l")==0) {
			ypos +=3;
		}
		else {
			uiDefIconTextBut(block, BUTM|but->pointype, but->retval, (short)(md->items[a].retval-but->min), md->items[a].str, 0, ypos,(short)width, 19, but->poin, (float) md->items[a].retval, 0.0, 0, 0, "");
			ypos += 20;
		}
	}

	block->direction= UI_TOP;
	ui_positionblock(block, but);

	/* the block is made with but-win, but is handled in mainwin space...
	   this is needs better implementation */
	block->win= G.curscreen->mainwin;

	uiBoundsBlock(block, 3);

	uiDoBlocks(&listb, 0);
	
	menudata_free(md);

	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);

	uibut_do_func(but);

	return but->retval;

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
	float curmatrix[4][4];

	value= ui_get_but_val(but);
	uiGetMouse(mywinget(), mval);

	sx= mval[0];
	h= but->y2-but->y1;
	fstart= but->max-but->min;
	fstart= (value - but->min)/fstart;
	temp= 32767;

	if( but->type==NUMSLI) deler= ( (but->x2-but->x1)/2 - h);
	else if( but->type==HSVSLI) deler= ( (but->x2-but->x1)/2 - h);
	else deler= (but->x2-but->x1-h);
	

	while (get_mbut() & L_MOUSE) {
	
		qual= get_qual();
		uiGetMouse(mywinget(), mval);
		
		f= (float)(mval[0]-sx)/deler +fstart;
		
		if(qual & LR_CTRLKEY) {
			if(qual & LR_SHIFTKEY) f= floor(f*100.0)/100.0;
			else f= floor(f*10.0)/10.0;
		} 
		else if (qual & LR_SHIFTKEY) {
			f= (f-fstart)/10.0 + fstart;
		}
		
		CLAMP(f, 0.0, 1.0);
		tempf= but->min+f*(but->max-but->min);
		
		temp= floor(tempf+.5);
		
		value= ui_get_but_val(but);
		lvalue= (int) value;
		
		if( but->pointype!=FLO )
			redraw= (temp != lvalue);
		else
			redraw= (tempf != value);

		if (redraw) {
			pos= 1;
			
			ui_set_but_val(but, tempf);
			ui_check_but(but);
			ui_draw_but(but);
			glFinish(); // flush display in subloops
			
			if(but->a1) {	/* color number */
				uiBut *bt= but->prev;
				while(bt) {
					if(bt->retval == but->a1) ui_draw_but(bt);
					bt= bt->prev;
				}
				bt= but->next;
				while(bt) {
					if(bt->retval == but->a1) ui_draw_but(bt);
					bt= bt->next;
				}
			}
			/* save current window matrix (global UIwinmat)
			   because button callback function MIGHT change it
			   - which has until now occured through the Python API
			*/
			Mat4CpyMat4(curmatrix, UIwinmat);
			uibut_do_func(but);
			Mat4CpyMat4(UIwinmat, curmatrix);
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
	glFinish(); // flush display in subloops
	
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
		uiActAsTextBut(but);
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

static int ui_do_but_BLOCK(uiBut *but)
{
	uiBlock *block;
	
	but->flag |= UI_SELECT;
	ui_draw_but(but);	

	block= but->block_func(0);

	ui_positionblock(block, but);
	block->flag |= UI_BLOCK_LOOP;
	block->win= G.curscreen->mainwin;
		
	/* postpone draw, this will cause a new window matrix, first finish all other buttons */
	block->flag |= UI_BLOCK_REDRAW;
	
	but->flag &= ~UI_SELECT;

	return 0;
}

static int ui_do_but_BUTM(uiBut *but)
{

	ui_set_but_val(but, but->min);
	UIafterfunc= but->butm_func;
	UIafterfunc_arg= but->butm_func_arg;
	UIafterval= but->a2;
	
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

/* ********************** NEXT/PREV for arrowkeys etc ************** */

static uiBut *ui_but_prev(uiBut *but)
{
	while(but->prev) {
		but= but->prev;
		if(but->type!=LABEL && but->type!=SEPR) return but;
	}
	return NULL;
}

static uiBut *ui_but_next(uiBut *but)
{
	while(but->next) {
		but= but->next;
		if(but->type!=LABEL && but->type!=SEPR) return but;
	}
	return NULL;
}

static uiBut *ui_but_first(uiBlock *block)
{
	uiBut *but;
	
	but= block->buttons.first;
	while(but) {
		if(but->type!=LABEL && but->type!=SEPR) return but;
		but= but->next;
	}
	return NULL;
}

static uiBut *ui_but_last(uiBlock *block)
{
	uiBut *but;
	
	but= block->buttons.last;
	while(but) {
		if(but->type!=LABEL && but->type!=SEPR) return but;
		but= but->prev;
	}
	return NULL;
}

/* *************************************************************** */


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
		if( but->pointype ) {		/* er there a pointer needed */
			if(but->poin==0 ) {
				printf("DoButton pointer error: %s\n",but->str);
				return 0;
			}
		}
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
	case TOGN:
		if(uevent->val) {
			retval= ui_do_but_TOG(block, but);
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
		if(uevent->val) retval= ui_do_but_NUM(but);
		break;
		
	case SLI:
	case NUMSLI:
	case HSVSLI:
		if(uevent->val) retval= ui_do_but_NUMSLI(but);
		break;
		
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
		if(uevent->val) retval= ui_do_but_BLOCK(but);
		break;

	case BUTM:
		retval= ui_do_but_BUTM(but);
		break;

	case LINK:
	case INLINK:
		retval= ui_do_but_LINK(block, but);
		break;
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
	allqueue(REDRAWBUTSGAME, 0);
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
	}
}

/* return: 
 * UI_NOTHING	pass event to other ui's
 * UI_CONT		don't pass event to other ui's
 * UI_RETURN	something happened, return, swallow event
 */
static int ui_do_block(uiBlock *block, uiEvent *uevent)
{
	uiBut *but, *bt;
	int butevent, event, retval=UI_NOTHING, count, act=0;
	int inside= 0, active=0;
	
	if(block->win != mywinget()) return UI_NOTHING;

	/* filter some unwanted events */
	if(uevent->event==LEFTSHIFTKEY || uevent->event==RIGHTSHIFTKEY) return UI_NOTHING;

	if(block->flag & UI_BLOCK_ENTER_OK) {
		if(uevent->event == RETKEY && uevent->val) {
			// printf("qual: %d %d %d\n", uevent->qual, get_qual(), G.qual);
			if ((G.qual & LR_SHIFTKEY) == 0) {
				return UI_RETURN_OK;
			}
		}
	}		

	Mat4CpyMat4(UIwinmat, block->winmat);
	uiGetMouse(mywinget(), uevent->mval);	/* transformed mouseco */

	/* check boundbox */
	if( block->minx <= uevent->mval[0] && block->maxx >= uevent->mval[0] ) {
		if( block->miny <= uevent->mval[1] && block->maxy >= uevent->mval[1] ) {
			inside= 1;
		}
	}

	switch(uevent->event) {
	case PAD8: case PAD2:
	case UPARROWKEY:
	case DOWNARROWKEY:
		if(inside || (block->flag & UI_BLOCK_LOOP)) {
			/* arrowkeys: only handle for block_loop blocks */
			event= 0;
			if(block->flag & UI_BLOCK_LOOP) {
				event= uevent->event;
				if(event==PAD8) event= UPARROWKEY;
				if(event==PAD2) event= DOWNARROWKEY;
			}
			else {
				if(uevent->event==PAD8) event= UPARROWKEY;
				if(uevent->event==PAD2) event= DOWNARROWKEY;
			}
			if(event && uevent->val) {
	
				but= block->buttons.first;
				while(but) {
					
					but->flag &= ~UI_MOUSE_OVER;
		
					if(but->flag & UI_ACTIVE) {
						but->flag &= ~UI_ACTIVE;
						ui_draw_but(but);
	
						bt= ui_but_prev(but);
						if(bt && event==UPARROWKEY) {
							bt->flag |= UI_ACTIVE;
							ui_draw_but(bt);
							break;
						}
						bt= ui_but_next(but);
						if(bt && event==DOWNARROWKEY) {
							bt->flag |= UI_ACTIVE;
							ui_draw_but(bt);
							break;
						}
					}
					but= but->next;
				}
	
				/* nothing done */
				if(but==NULL) {
				
					if(event==UPARROWKEY) but= ui_but_last(block);
					else but= ui_but_first(block);
					
					if(but) {
						but->flag |= UI_ACTIVE;
						ui_draw_but(but);
					}
				}
				retval= UI_CONT;
			}
		}
		break;
	
	case ONEKEY: act= 1;
	case TWOKEY: if(act==0) act= 2;
	case THREEKEY: if(act==0) act= 3;
	case FOURKEY: if(act==0) act= 4;
	case FIVEKEY: if(act==0) act= 5;
	case SIXKEY: if(act==0) act= 6;
	case SEVENKEY: if(act==0) act= 7;
	case EIGHTKEY: if(act==0) act= 8;
	case NINEKEY: if(act==0) act= 9;
	case ZEROKEY: if(act==0) act= 10;
	
		if( block->flag & UI_BLOCK_NUMSELECT ) {
			
			if(get_qual() & LR_ALTKEY) act+= 10;
			
			but= block->buttons.first;
			count= 0;
			while(but) {
				if( but->type!=LABEL && but->type!=SEPR) count++;
				if(count==act) {
					but->flag |= UI_ACTIVE;
					if(uevent->val==1) ui_draw_but(but);
					else {
						uevent->event= RETKEY;
						uevent->val= 1;			/* patch: to avoid UI_BLOCK_RET_1 type not working */
						addqueue(block->winq, RIGHTARROWKEY, 1);
					}
				}
				else if(but->flag & UI_ACTIVE) {
					but->flag &= ~UI_ACTIVE;
					ui_draw_but(but);
				}
				but= but->next;
			}
		}
		
		break;
		
	default:
		if (uevent->event!=RETKEY) {	/* when previous command was arrow */
			but= block->buttons.first;
			while(but) {
			
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
						if(but->type != LABEL &&
							but->embossfunc != ui_emboss_N) ui_draw_but(but);
					}
				}
				/* hilite case 2 */
				if(but->flag & UI_ACTIVE) {
					if( (but->flag & UI_MOUSE_OVER)==0) {
						but->flag &= ~UI_ACTIVE;
						if(but->type != LABEL &&
							but->embossfunc != ui_emboss_N) ui_draw_but(but);
					}
					if(but->flag & UI_ACTIVE) active= 1;
				}
				
				but= but->next;
			}
			
			/* if there are no active buttons... otherwise clear lines */
			if(active) ui_do_active_linklines(block, 0);
			else ui_do_active_linklines(block, uevent->mval);			

		}
	}

	/* middlemouse exception, not for regular blocks */
	if( (block->flag & UI_BLOCK_LOOP) && uevent->event==MIDDLEMOUSE) uevent->event= LEFTMOUSE;

	/* the final dobutton */
	but= block->buttons.first;
	while(but) {
		if(but->flag & UI_ACTIVE) {
			
			/* UI_BLOCK_RET_1: not return when val==0 */
			
			if(uevent->val || (block->flag & UI_BLOCK_RET_1)==0) {
				if ELEM3(uevent->event, LEFTMOUSE, PADENTER, RETKEY) {
					butevent= ui_do_button(block, but, uevent);
					if(butevent) addqueue(block->winq, UI_BUT_EVENT, (short)butevent);

					/* i doubt about the next line! */
					/* if(but->func) mywinset(block->win); */
			
					if( (block->flag & UI_BLOCK_LOOP) && but->type==BLOCK);
					else	
						if(/*but->func ||*/ butevent) retval= UI_RETURN_OK;
				}
			}
		}
		
		but= but->next;
	}

	/* the linkines... why not make buttons from it? Speed? Memory? */
	if(uevent->val && (uevent->event==XKEY || uevent->event==DELKEY)) 
		ui_delete_active_linkline(block);

	if(block->flag & UI_BLOCK_LOOP) {

		if(inside==0 && uevent->val==1) {
			if ELEM3(uevent->event, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE)
				return UI_RETURN_OUT;
		}

		if(uevent->event==ESCKEY && uevent->val==1) return UI_RETURN_CANCEL;

		if((uevent->event==RETKEY || uevent->event==PADENTER) && uevent->val==1) return UI_RETURN_OK;
		
		/* check outside */
		if(block->direction==UI_RIGHT) count= 140; else count= 40;
		if(uevent->mval[0]<block->minx-count) return UI_RETURN_OUT;
		
		if(uevent->mval[1]<block->miny-40) return UI_RETURN_OUT;

		if(block->direction==UI_LEFT) count= 140; else count= 40;
		if(uevent->mval[0]>block->maxx+count) return UI_RETURN_OUT;

		if(uevent->mval[1]>block->maxy+40) return UI_RETURN_OUT;
		
	}

	return retval;
}

static uiSaveUnder *ui_draw_but_tip(uiBut *but)
{
	uiSaveUnder *su;
	float x1, x2, y1, y2;
	
#ifdef INTERNATIONAL
	if(G.ui_international == TRUE) {
		float llx,lly,llz,urx,ury,urz;  //for FTF_GetBoundingBox()

		if(U.transopts & TR_TOOLTIPS) {
			FTF_GetBoundingBox(but->tip, &llx,&lly,&llz,&urx,&ury,&urz, FTF_USE_GETTEXT | FTF_INPUT_UTF8);

			x1= (but->x1+but->x2)/2; x2= 10+x1+ but->aspect*FTF_GetStringWidth(but->tip, FTF_USE_GETTEXT | FTF_INPUT_UTF8);  //BMF_GetStringWidth(but->font, but->tip);
			y1= but->y1-(ury+FTF_GetSize()); y2= but->y1;
		} else {
			FTF_GetBoundingBox(but->tip, &llx,&lly,&llz,&urx,&ury,&urz, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);

			x1= (but->x1+but->x2)/2; x2= 10+x1+ but->aspect*FTF_GetStringWidth(but->tip, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);  //BMF_GetStringWidth(but->font, but->tip);
			y1= but->y1-(ury+FTF_GetSize()); y2= but->y1;
		}
	} else {
  		x1= (but->x1+but->x2)/2; x2= 10+x1+ but->aspect*BMF_GetStringWidth(but->font, but->tip);
  		y1= but->y1-19; y2= but->y1-2;
	}
#else
  	x1= (but->x1+but->x2)/2; x2= 10+x1+ but->aspect*BMF_GetStringWidth(but->font, but->tip);
  	y1= but->y1-19; y2= but->y1-2;
#endif

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

	// adjust tooltip heights
	if(mywinget()==G.curscreen->mainwin)
		y2 -= G.ui_international ? 4:1;		//tip is from pulldownmenu
	else if(curarea->win != mywinget())
		y2 -= G.ui_international ? 5:1;		//tip is from a windowheader
//	else y2 += 1;							//tip is from button area

	su= ui_bgnpupdraw((int)(x1-1), (int)(y1-1), (int)(x2+4), (int)(y2+4), 0);

	glColor3ub(0xD0, 0xD0, 0xC0);
	glRectf(x1, y1, x2, y2);
	
	/* bottom */
	glColor3ub(0,0,0);
	fdrawline(x1, y1, x2, y1);
	/* right */
	fdrawline(x2, y1, x2, y2);
	/* top */
	glColor3ub(255,255,255);
	fdrawline(x1, y2, x2, y2);
	/* left */
	fdrawline(x1, y1, x1, y2);
	
	glColor3ub(0,0,0);
	glRasterPos2f( x1+3, y1+4);
#ifdef INTERNATIONAL
	if(G.ui_international == TRUE)
		if(U.transopts & TR_TOOLTIPS)
			FTF_DrawString(but->tip, FTF_USE_GETTEXT | FTF_INPUT_UTF8, 0);
		else
			FTF_DrawString(but->tip, FTF_NO_TRANSCONV | FTF_INPUT_UTF8, 0);
	else
		BMF_DrawString(but->font, but->tip);
#else
	BMF_DrawString(but->font, but->tip);
#endif
	
	glFinish();		/* to show it in the frontbuffer */
	return su;
}

static void ui_do_but_tip(void)
{
	uiSaveUnder *su;
	int time;
	
	if (UIbuttip && UIbuttip->tip && UIbuttip->tip[0]) {
			/* Pause for a moment to see if we
			 * should really display the tip
			 * or if the user will keep moving
			 * the pointer.
			 */
		for (time= 0; time<10; time++) {
			if (anyqtest())
				return;
			else
				PIL_sleep_ms(30);
		}
			
			/* Display the tip, and keep it displayed
			 * as long as the mouse remains on top
			 * of the button that owns it.
			 */
		su= ui_draw_but_tip(UIbuttip);
		
		while (1) {
			char ascii;
			short val;
			unsigned short evt= extern_qread_ext(&val, &ascii);

			if (evt==MOUSEX || evt==MOUSEY) {
				short mouse[2];
				uiGetMouse(su->oldwin, mouse);
				
				if (!uibut_contains_pt(UIbuttip, mouse))
					break;
			} else {
				mainqpushback(evt, val, ascii);
				break;
			}
		}
		
		ui_endpupdraw(su);
		UIbuttip= NULL;
	}
}

/* returns UI_NOTHING, if nothing happened */
int uiDoBlocks(ListBase *lb, int event)
{
	/* return when:  firstblock != BLOCK_LOOP
	 * The mainloop is constructed in such a way
	 * that the last mouse event from a sub-block
	 * is passed on to the next block.
	 * 
	 * 'cont' is used to make sure you can press a menu button while another
	 * is active. otherwise you have to press twice...
	 */

	uiBlock *block;
	uiEvent uevent;
	int retval= UI_NOTHING, cont= 1;

	if(lb->first==0) return UI_NOTHING;
		
	UIbuttip= NULL;
	UIafterfunc= NULL;	/* to prevent infinite loops, this shouldnt be a global! */
	
	uevent.qual= G.qual;
	uevent.event= event;
	uevent.val= 1;

	/* this is a caching mechanism, to prevent too many calls to glFrontBuffer and glFinish, which slows down interface */
	block= lb->first;
	while(block) {
		block->frontbuf= UI_NEED_DRAW_FRONT;	// signal
		block= block->next;
	}

	/* main loop, we stay here for pulldown menus or temporal blocks (UI_BLOCK_LOOP type) */
	while(cont) {
		block= lb->first;
		while(block) {
			
			/* this here, to make sure it also draws when event==0 */
			if(block->flag & UI_BLOCK_REDRAW) {
				if( block->flag & UI_BLOCK_LOOP) {
					block->saveunder= ui_bgnpupdraw((int)block->minx-1, (int)block->miny-4, (int)block->maxx+4, (int)block->maxy+1, 1);
					block->frontbuf= UI_HAS_DRAW_FRONT;
				}
				uiDrawBlock(block);
				block->flag &= ~UI_BLOCK_REDRAW;
			}

			retval= ui_do_block(block, &uevent);
			
			if(block->frontbuf == UI_HAS_DRAW_FRONT) {
				glFinish();
				glDrawBuffer(GL_BACK);
				block->frontbuf= UI_NEED_DRAW_FRONT;
			}
			
			if(retval==UI_CONT || retval & UI_RETURN) break;

			block= block->next;
		}
	
		/* this is here, to allow closed loop-blocks (menu's) to return to the previous block */
		block= lb->first;
		if(block==NULL || (block->flag & UI_BLOCK_LOOP)==0) cont= 0;

		while( (block= lb->first) && (block->flag & UI_BLOCK_LOOP)) {
				
			/* this here, for menu buts */
			if(block->flag & UI_BLOCK_REDRAW) {

				if( block->flag & UI_BLOCK_LOOP) {
					block->saveunder= ui_bgnpupdraw((int)block->minx-1, (int)block->miny-4, (int)block->maxx+4, (int)block->maxy+1, 1);
					block->frontbuf= UI_HAS_DRAW_FRONT;
				}
				uiDrawBlock(block);
				block->flag &= ~UI_BLOCK_REDRAW;
			}

			/* need to reveil drawing? (not in end of loop, because of free block */
			if(block->frontbuf == UI_HAS_DRAW_FRONT) {
				glFinish();
				block->frontbuf= UI_NEED_DRAW_FRONT;
			}

			uevent.event= extern_qread(&uevent.val);
			
			if(uevent.event) {
			
				retval= ui_do_block(block, &uevent);
			
				if(retval & UI_RETURN) {
					/* free this block */
					ui_endpupdraw(block->saveunder);
					
					BLI_remlink(lb, block);
					uiFreeBlock(block);
				}
				if(retval==UI_RETURN_OK) {
					/* free other menus */
					while( (block= lb->first) && (block->flag & UI_BLOCK_LOOP)) {
						ui_endpupdraw(block->saveunder);
						BLI_remlink(lb, block);
						uiFreeBlock(block);
					}
				}
			}
			
			/* tooltip */	
			if(retval==UI_NOTHING && (uevent.event==MOUSEX || uevent.event==MOUSEY)) {
				if(U.flag & TOOLTIPS) ui_do_but_tip();
			}
		}

		if(retval==UI_CONT || (retval & UI_RETURN_OK)) cont= 0;
	}
	
	if(retval & UI_RETURN_OK) {
		if(UIafterfunc) UIafterfunc(UIafterfunc_arg, UIafterval);
		UIafterfunc= NULL;
	}

	/* tooltip */	
	if(retval==UI_NOTHING && (uevent.event==MOUSEX || uevent.event==MOUSEY)) {
		if(U.flag & TOOLTIPS) ui_do_but_tip();
	}


	/* cleanup frontbuffer & flags */
	block= lb->first;
	while(block) {
		if(block->frontbuf==UI_HAS_DRAW_FRONT) glFinish();
		block->frontbuf= 0;
		block= block->next;
	}
	
	/* doesnt harm :-) */
	glDrawBuffer(GL_BACK);

	return retval;
}

/* ************** DATA *************** */


static double ui_get_but_val(uiBut *but)
{
	void *poin;
	double value = 0.0;

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
		*((char *)poin)= (char)value;
	else if( but->pointype==SHO ) {
		/* gcc 3.2.1 seems to have problems 
		 * casting a double like 32772.0 to
		 * a short so we cast to an int, then 
		 to a short */
		int gcckludge;
		gcckludge = (int) value;
		*((short *)poin)= (short) gcckludge;
	}
	else if( but->pointype==INT )
		*((int *)poin)= (int)value;
	else if( but->pointype==FLO )
		*((float *)poin)= value;
	
	/* update select flag */
	ui_is_but_sel(but);

}

void uiSetCurFont(uiBlock *block, int index)
{

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

	if(block->flag & UI_BLOCK_BUSY) printf("var1: %x\n", block);	

	while( (but= block->buttons.first) ) {
		BLI_remlink(&block->buttons, but);	
		ui_free_but(but);
	}

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
	if(lb) BLI_addhead(lb, block);		/* at the beginning of the list! */

	strcpy(block->name, name);
	/* draw win */
	block->win= win;
	/* window where queue event should be added, pretty weak this way!
	   this is because the 'mainwin' pup menu's */
	block->winq= mywinget();
	block->dt= dt;
	block->col= BUTGREY;

		/* aspect */
	bwin_getsinglematrix(win, block->winmat);

	if (win==G.curscreen->mainwin) {
		block->aspect= 1.0;
	} else {
		int getsizex, getsizey;

		bwin_getsize(win, &getsizex, &getsizey);
		block->aspect= 2.0/( (getsizex)*block->winmat[0][0]);
	}

	uiSetCurFont(block, font);

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

static void ui_check_but(uiBut *but)
{
	/* if something changed in the button */
	ID *id;
	double value;
	short pos;
	
	ui_is_but_sel(but);

	/* name: */
	switch( but->type ) {
	
	case MENU:
		
		if(but->x2 - but->x1 > 24) {
			value= ui_get_but_val(but);
			ui_set_name_menu(but, (int)value);
		}
		break;
	
	case NUM:
	case NUMSLI:
	case HSVSLI:

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

	default:
		strcpy(but->drawstr, but->str);
		
	}
	
#ifdef INTERNATIONAL
	if(G.ui_international == TRUE) {
		if(U.transopts & TR_BUTTONS) {
			if(but->drawstr[0]) {
				but->strwidth= but->aspect*FTF_GetStringWidth(but->drawstr, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
			} else {
				but->strwidth= 0;
			}
		} else {
			if(but->drawstr[0]) {
				but->strwidth= but->aspect*FTF_GetStringWidth(but->drawstr, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
			} else {
				but->strwidth= 0;
			}
		}
	} else {
		if(but->drawstr[0]) {
			but->strwidth= but->aspect*BMF_GetStringWidth(but->font, but->drawstr);
		} else {
			but->strwidth= 0;
		}
	}
#else
		if(but->drawstr[0])
			but->strwidth= but->aspect*BMF_GetStringWidth(but->font, but->drawstr);
		else
			but->strwidth= 0;
#endif
	/* automatic width */
	if(but->x2==0.0) {
		but->x2= (but->x1+but->strwidth+6); 
	}

	/* calc but->ofs, to draw the string shorter if too long */
	but->ofs= 0;
	while(but->strwidth > (int)(but->x2-but->x1-7) ) {
		but->ofs++;

		if(but->drawstr[but->ofs]) 
#ifdef INTERNATIONAL
			if(G.ui_international == TRUE) {
				if(U.transopts & TR_BUTTONS) {
					but->strwidth= but->aspect*FTF_GetStringWidth(but->drawstr+but->ofs, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
				} else {
					but->strwidth= but->aspect*FTF_GetStringWidth(but->drawstr+but->ofs, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
				}
			} else {
				but->strwidth= but->aspect*BMF_GetStringWidth(but->font, but->drawstr+but->ofs);
			}
#else
			but->strwidth= but->aspect*BMF_GetStringWidth(but->font, but->drawstr+but->ofs);
#endif
		else but->strwidth= 0;

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
		
		if(but->strwidth < 10) break;
	}

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
		
	case ICONTOG: 
		if(but->flag & UI_SELECT) but->iconadd= 1;
		else but->iconadd= 0;
		break;
		
	case ICONROW:
		value= ui_get_but_val(but);
		but->iconadd= (int)value- (int)(but->min);
		break;

	case ICONTEXTROW:
		value= ui_get_but_val(but);
		ui_set_name_menu(but, (int)value);
		but->iconadd= (int)value- (int)(but->min);
		break;
	}
}

static uiBut *ui_def_but(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but;
	short slen;
	
	if(type & BUTPOIN) {		/* a pointer is required */
		if(poin==0) {
				/* if pointer is zero, button is removed and not drawn */
			BIF_set_color(block->col, COLORSHADE_MEDIUM);
			glRects(x1,  y1,  x1+x2,  y1+y2);
			return NULL;
		}
	}

	but= MEM_callocN(sizeof(uiBut), "uiBut");

	but->type= type & BUTTYPE;
	but->pointype= type & BUTPOIN;
	but->bit= type & BIT;
	but->bitnr= type & 31;

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
	but->col= block->col;

	but->lock= UIlock;
	but->lockstr= UIlockstr;

	but->aspect= block->aspect;
	but->win= block->win;
	but->block= block;		// pointer back, used for frontbuffer status

	if (but->type==BUTM) {
		but->butm_func= block->butm_func;
		but->butm_func_arg= block->butm_func_arg;
	} else {
		but->func= block->func;
		but->func_arg1= block->func_arg1;
		but->func_arg2= block->func_arg2;
	}

	if(block->dt==UI_EMBOSSX) but->embossfunc= ui_emboss_X;
	else if(block->dt==UI_EMBOSSW) but->embossfunc= ui_emboss_W;
	else if(block->dt==UI_EMBOSSF) but->embossfunc= ui_emboss_F;
	else if(block->dt==UI_EMBOSSM) but->embossfunc= ui_emboss_M;
	else if(block->dt==UI_EMBOSSP) but->embossfunc= ui_emboss_P;
	else if(block->dt==UI_EMBOSSA) but->embossfunc= ui_emboss_A;
	else but->embossfunc= ui_emboss_N;
	
	but->pos= -1;	/* cursor invisible */

	if(but->type==NUM) {	/* add a space to name */
		slen= strlen(but->str);
		if(slen>0 && slen<UI_MAX_NAME_STR-2) {
			if(but->str[slen-1]!=' ') {
				but->str[slen]= ' ';
				but->str[slen+1]= 0;
			}
		}
	}
	
	if ELEM6(but->type, HSVSLI , NUMSLI, TEX, LABEL, IDPOIN, BLOCK) {
		but->flag |= UI_TEXT_LEFT;
	}
	
	return but;
}

uiBut *uiDefBut(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but= ui_def_but(block, type, retval, str, x1, y1, x2, y2, poin, min, max, a1, a2, tip);

	ui_check_but(but);
	
	return but;
}
uiBut *uiDefButF(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|FLO, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButI(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|INT, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButS(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|SHO, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButC(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|CHA, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}

uiBut *uiDefIconBut(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but= ui_def_but(block, type, retval, "", x1, y1, x2, y2, poin, min, max, a1, a2, tip);
	
	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;

	ui_check_but(but);
	
	return but;
}

uiBut *uiDefIconButF(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|FLO, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButI(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|INT, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButS(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|SHO, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButC(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|CHA, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
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

uiBut *uiDefIconTextButF(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|FLO, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButI(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|INT, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButS(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|SHO, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButC(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|CHA, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
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
	return block->col;
}
void uiBlockSetCol(uiBlock *block, int col)
{
	block->col= col;
}
void uiBlockSetEmboss(uiBlock *block, int emboss)
{
	block->dt= emboss;
}
void uiBlockSetDirection(uiBlock *block, int direction)
{
	block->direction= direction;
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

int uiButGetRetVal(uiBut *but)
{
	return but->retval;
}


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
void uiButSetFunc(uiBut *but, void (*func)(void *arg1, void *arg2), void *arg1, void *arg2)
{
	but->func= func;
	but->func_arg1= arg1;
	but->func_arg2= arg2;
}

void uiDefIDPoinBut(uiBlock *block, uiIDPoinFuncFP func, int retval, char *str, short x1, short y1, short x2, short y2, void *idpp, char *tip)
{
	uiBut *but= ui_def_but(block, IDPOIN, retval, str, x1, y1, x2, y2, NULL, 0.0, 0.0, 0.0, 0.0, tip);
	but->idpoin_func= func;
	but->idpoin_idpp= (ID**) idpp;
	ui_check_but(but);
}

void uiDefBlockBut(uiBlock *block, uiBlockFuncFP func, void *arg, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, BLOCK, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	but->block_func= func;
	ui_check_but(but);
}

void uiDefKeyevtButS(uiBlock *block, int retval, char *str, short x1, short y1, short x2, short y2, short *spoin, char *tip)
{
	uiBut *but= ui_def_but(block, KEYEVT|SHO, retval, str, x1, y1, x2, y2, spoin, 0.0, 0.0, 0.0, 0.0, tip);
	ui_check_but(but);
}

/* ******************** PUPmenu ****************** */

short pupmenu(char *instr)
{
	uiBlock *block;
	ListBase listb= {NULL, NULL};
	int event;
	static int lastselected= 0;
	short width, height, mousexmove = 0, mouseymove, xmax, ymax, mval[2], val= -1;
	short a, startx, starty, endx, endy, boxh=TBOXH, x1, y1;
	static char laststring[UI_MAX_NAME_STR];
	MenuData *md;
	
	/* block stuff first, need to know the font */
	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_NUMSELECT);
	
	md= decompose_menu_string(instr);

	/* size and location, title slightly bigger for bold */
#ifdef INTERNATIONAL
	if(G.ui_international == TRUE) {
		if(U.transopts && TR_BUTTONS) {
			if(md->title) width= 2*strlen(md->title)+FTF_GetStringWidth(md->title, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
			else width= 0;
			for(a=0; a<md->nitems; a++) {
				xmax= FTF_GetStringWidth( md->items[a].str, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
				if(xmax>width) width= xmax;
			}
		} else {
			if(md->title) width= 2*strlen(md->title)+FTF_GetStringWidth(md->title, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
			else width= 0;
			for(a=0; a<md->nitems; a++) {
				xmax= FTF_GetStringWidth( md->items[a].str, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
				if(xmax>width) width= xmax;
			}
		}
	} else {
		if(md->title) width= 2*strlen(md->title)+BMF_GetStringWidth(uiBlockGetCurFont(block), md->title);
		else width= 0;
		for(a=0; a<md->nitems; a++) {
			xmax= BMF_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str);
			if(xmax>width) width= xmax;
		}
	}
#else
	if(md->title) width= 2*strlen(md->title)+BMF_GetStringWidth(uiBlockGetCurFont(block), md->title);
	else width= 0;
	for(a=0; a<md->nitems; a++) {
		xmax= BMF_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str);
		if(xmax>width) width= xmax;
	}
#endif

	width+= 10;
	
	height= boxh*md->nitems;
	
	xmax = G.curscreen->sizex;
	ymax = G.curscreen->sizey;

	getmouseco_sc(mval);
	
	if(strncmp(laststring, instr, UI_MAX_NAME_STR-1)!=0) lastselected= 0;
	BLI_strncpy(laststring, instr, UI_MAX_NAME_STR);
	
	startx= mval[0]-width/2;
	if(lastselected>=0 && lastselected<md->nitems) {
		starty= mval[1]-height+boxh/2+lastselected*boxh;
	}
	else starty= mval[1]-height/2;
	
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
		warp_pointer(mval[0], mouseymove+mval[1]);
		mousexmove= mval[0];
		mouseymove= mval[1];
	}
	
	/* here we go! */	
	if(md->title) {
		uiBut *bt;
		uiSetCurFont(block, UI_HELVB);
		bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+md->nitems*boxh), width, boxh, NULL, 0.0, 0.0, 0, 0, "");
		bt->flag= UI_TEXT_LEFT;
		uiSetCurFont(block, UI_HELV);
	}

	y1= starty + boxh*(md->nitems-1);
	x1= startx;
	for(a=0; a<md->nitems; a++, y1-=boxh) {
		char *name= md->items[a].str;
		
		if( strcmp(name, "%l")==0) {
			uiDefBut(block, SEPR, B_NOP, "", x1, y1, width, boxh, NULL, 0, 0.0, 0, 0, "");
		}
		else {
			uiDefButS(block, BUTM, B_NOP, name, x1, y1, width, boxh-1, &val, (float) md->items[a].retval, 0.0, 0, 0, "");
		}
	}
	
	uiBoundsBlock(block, 2);

	event= uiDoBlocks(&listb, 0);

	/* calculate last selected */
	lastselected= 0;
	for(a=0; a<md->nitems; a++) {
		if(val==md->items[a].retval) lastselected= a;
	}

	menudata_free(md);
	
	if(mouseymove && (event & UI_RETURN_OUT)==0) warp_pointer(mousexmove, mouseymove);
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
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT);

	md= decompose_menu_string(instr);

	/* collumns and row calculation */
	columns= (md->nitems+maxrow)/maxrow;
	if (columns<1) columns= 1;
	
	rows= (int) md->nitems/columns;
	if (rows<1) rows= 1;
	
	while (rows*columns<md->nitems) rows++;
		
	/* size and location */
#ifdef INTERNATIONAL
	if(G.ui_international == TRUE) {
		if(U.transopts & TR_BUTTONS) {
			if(md->title) width= 2*strlen(md->title)+FTF_GetStringWidth(md->title, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
			else width= 0;
			for(a=0; a<md->nitems; a++) {
				xmax= FTF_GetStringWidth( md->items[a].str, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
				if(xmax>width) width= xmax;
			}
		} else {
			if(md->title) width= 2*strlen(md->title)+FTF_GetStringWidth(md->title, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
			else width= 0;
			for(a=0; a<md->nitems; a++) {
				xmax= FTF_GetStringWidth( md->items[a].str, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
				if(xmax>width) width= xmax;
			}
		}
	} else {
		if(md->title) width= 2*strlen(md->title)+BMF_GetStringWidth(uiBlockGetCurFont(block), md->title);
		else width= 0;
		for(a=0; a<md->nitems; a++) {
			xmax= BMF_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str);
			if(xmax>width) width= xmax;
		}
	}
#else
	if(md->title) width= 2*strlen(md->title)+BMF_GetStringWidth(uiBlockGetCurFont(block), md->title);
	else width= 0;
	for(a=0; a<md->nitems; a++) {
		xmax= BMF_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str);
		if(xmax>width) width= xmax;
	}
#endif

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

	warp_pointer(mval[0]+mousemove[0], mval[1]+mousemove[1]);

	mousemove[0]= mval[0];
	mousemove[1]= mval[1];

	/* here we go! */

	if(md->title) {
		uiBut *bt;
		uiSetCurFont(block, UI_HELVB);
		bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+rows*boxh), (short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
		uiSetCurFont(block, UI_HELV);
		bt->flag= UI_TEXT_LEFT;
	}

	for(a=0; a<md->nitems; a++) {

		x1= startx + width*((int)a/rows);
		y1= starty - boxh*(a%rows) + (rows-1)*boxh; 

		uiDefButI(block, BUTM, B_NOP, md->items[a].str, x1, y1, (short)(width-(rows>1)), (short)(boxh-1), &val, (float)md->items[a].retval, 0.0, 0, 0, "");
	}
	
	uiBoundsBlock(block, 3);

	event= uiDoBlocks(&listb, 0);
	
	menudata_free(md);
	
	if((event & UI_RETURN_OUT)==0) warp_pointer(mousemove[0], mousemove[1]);
	
	return val;	
}

