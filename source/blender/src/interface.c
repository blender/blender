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
#include "BLI_winstuff.h"
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
#include "BLI_editVert.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BKE_blender.h"
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
#include "BIF_butspace.h"

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
	Panel *panel;
	
	char name[UI_MAX_NAME_STR];
	
	float winmat[4][4];
	
	float minx, miny, maxx, maxy;
	float aspect;

	void (*butm_func)(void *arg, int event);
	void *butm_func_arg;

	void (*func)(void *arg1, void *arg2);
	void *func_arg1;
	void *func_arg2;
	
	/* extra draw function for custom blocks */
	void (*drawextra)();

	BIFColorID col;
	short font;	/* indices */
	int afterval;
	void *curfont;
	
	short autofill, flag, win, winq, direction, dt, frontbuf, auto_open;  //frontbuf see below
	void *saveunder;
	
	float xofs, yofs;  	// offset to parent button
	rctf parentrct;		// for pulldowns, rect the mouse is allowed outside of menu (parent button)
};

/* block->frontbuf: (only internal here), to nice localize the old global var uiFrontBuf */
#define UI_NEED_DRAW_FRONT 		1
#define UI_HAS_DRAW_FRONT 		2

/* panel drawing defines */
#define PNL_GRID	4
#define PNL_DIST	8
#define PNL_SAFETY 	8
#define PNL_HEADER  20

/* panel->flag */
#define PNL_SELECT	1
#define PNL_CLOSEDX	2
#define PNL_CLOSEDY	4
#define PNL_CLOSED	6
#define PNL_TABBED	8
#define PNL_OVERLAP	16


/* ************ GLOBALS ************* */

static float UIwinmat[4][4];
static int UIlock= 0, UIafterval;
static char *UIlockstr=NULL;
static void (*UIafterfunc)(void *arg, int event);
static void *UIafterfunc_arg;

static uiFont UIfont[UI_ARRAY];  // no init needed
static uiBut *UIbuttip;

/* ************* PROTOTYPES ***************** */

static void ui_draw_anti_tria(float x1, float y1, float x2, float y2, float x3, float y3);
static void ui_check_but(uiBut *but);
static void ui_set_but_val(uiBut *but, double value);
static double ui_get_but_val(uiBut *but);
static void ui_draw_panel(uiBlock *block);
static void ui_drag_panel(uiBlock *block);
static void ui_do_panel(uiBlock *block, uiEvent *uevent);
static int panel_has_tabs(Panel *panel);

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
	float xs=0, ys=0;
	
	if(but->flag & UI_ICON_LEFT) {
		if (but->type==BUTM) {
			xs= but->x1+1.0;
		}
		else if ((but->type==ICONROW) || (but->type==ICONTEXTROW)) {
			xs= but->x1+4.0;
		}
		else {
			xs= but->x1+6.0;
		}
		ys= (but->y1+but->y2- BIF_get_icon_height(icon))/2.0;
	}
	if(but->flag & UI_ICON_RIGHT) {
		xs= but->x2-17.0;
		ys= (but->y1+but->y2- BIF_get_icon_height(icon))/2.0;
	}
	if (!((but->flag & UI_ICON_RIGHT) || (but->flag & UI_ICON_LEFT))) {
		xs= (but->x1+but->x2- BIF_get_icon_width(icon))/2.0;
		ys= (but->y1+but->y2- BIF_get_icon_height(icon))/2.0;
	}

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

/* not used
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

static void ui_emboss_R(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_DARK);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_HILITE);
		else BIF_set_color(bc, COLORSHADE_MEDIUM);
	}
	
	uiSetRoundBox(15);
	uiRoundBox(x1, y1, x2, y2, 6);
	cpack(0x0);
	uiSetRoundBox(16+15);
	uiRoundRect(x1, y1, x2, y2, 6);
	uiSetRoundBox(15);
}

*/


static void ui_emboss_X(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{

	float asp1;
	
	asp1= asp;

	/*x1+= asp1;*/
	x2-= asp1;	
	/*y1+= asp1;*/
	y2-= asp1;

	/* SHADED BUTTON */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);
	

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_MEDIUM);
		else BIF_set_color(bc, COLORSHADE_LGREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LIGHT);
		else BIF_set_color(bc, COLORSHADE_HILITE);
	}

	glVertex2f(x1,y1);
	glVertex2f(x2,y1);

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LGREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_WHITE);
		BIF_set_color(bc, COLORSHADE_LIGHT);
	}

	glVertex2f(x2,(y2-(y2-y1)/3));
	glVertex2f(x1,(y2-(y2-y1)/3));
	glEnd();
	

	glShadeModel(GL_FLAT);
	glBegin(GL_QUADS);
	
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LGREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_WHITE);
		BIF_set_color(bc, COLORSHADE_LIGHT);
	}
	
	glVertex2f(x1,(y2-(y2-y1)/3));
	glVertex2f(x2,(y2-(y2-y1)/3));
	glVertex2f(x2,y2);
	glVertex2f(x1,y2);

	glEnd();

	/* END SHADED BUTTON */

	/* OUTER SUNKEN EFFECT */
	/* left */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	glVertex2f(x1-1,y1);
	BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	glVertex2f(x1-1,y2);
	glEnd();
	
	/* right */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	glVertex2f(x2+1,y1);
	BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	glVertex2f(x2+1,y2);
	glEnd();

	/* bottom */
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	fdrawline(x1, y1-1, x2, y1-1);
	/* END OUTER SUNKEN EFFECT */
	
	/* INNER OUTLINE */
	glShadeModel(GL_FLAT);
	
	/* top */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_GREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_WHITE);
		BIF_set_color(bc, COLORSHADE_WHITE);
	}

	fdrawline(x1, (y2-1), x2, y2-1);
	
	/* bottom */
	
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LGREY);
		else BIF_set_color(bc, COLORSHADE_LGREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LMEDIUM);
		BIF_set_color(bc, COLORSHADE_LMEDIUM);
	}
	fdrawline(x1, (y1+1), x2, y1+1);

	/* left */
	if(!(flag & UI_SELECT)) {
	                        	
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(bc, COLORSHADE_WHITE);
	glVertex2f(x1+1,y1+2);
	BIF_set_color(bc, COLORSHADE_LGREY);
	glVertex2f(x1+1,y2);
	glEnd();
	
	}
	
	/* right */
	if(!(flag & UI_SELECT)) {

	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(bc, COLORSHADE_LGREY);
	glVertex2f(x2-1,y1+2);
	BIF_set_color(bc, COLORSHADE_WHITE);
	glVertex2f(x2-1,y2);
	glEnd();
	
	}
	/* END INNER OUTLINE */
	
	/* OUTER OUTLINE */
	glShadeModel(GL_FLAT);
	
	/* underneath semi-fake-AA */
	BIF_set_color(BUTGREY, COLORSHADE_GREY);
	fdrawline(x1, y2, x2, y2);
	BIF_set_color(BUTGREY, COLORSHADE_MEDIUM);
	fdrawline(x1, y1, x2, y1);

	/* top */
	BIF_set_color(BUTGREY, COLORSHADE_DARK);
	fdrawline(x1+1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1+1, x1, y2);

	/* right */
	fdrawline(x2, y1+1, x2, y2);
	
	/* bottom */
	BIF_set_color(BUTGREY, COLORSHADE_DARK);
	fdrawline(x1+1, y1, x2, y1);
	/* END OUTER OUTLINE */
	
}

static void ui_emboss_TEX(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{

	float asp1;
	
	asp1= asp;

	/*x1+= asp1;*/
	x2-= asp1;	
	/*y1+= asp1;*/
	y2-= asp1;

	/* FLAT TEXT/NUM FIELD */
	glShadeModel(GL_FLAT);
	glBegin(GL_QUADS);

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LGREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_HILITE);
		else BIF_set_color(bc, COLORSHADE_LMEDIUM);
	}

	glVertex2f(x1,y1);
	glVertex2f(x2,y1);
	glVertex2f(x2,y2);
	glVertex2f(x1,y2);

	glEnd();
	/* END FLAT TEXT/NUM FIELD */
	
	/* OUTER SUNKEN EFFECT */
	/* left */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	glVertex2f(x1-1,y1);
	BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	glVertex2f(x1-1,y2);
	glEnd();
	
	/* right */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	glVertex2f(x2+1,y1);
	BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	glVertex2f(x2+1,y2);
	glEnd();

	/* bottom */
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	fdrawline(x1, y1-1, x2, y1-1);
	/* END OUTER SUNKEN EFFECT */

	/* OUTER OUTLINE */
	glShadeModel(GL_FLAT);
	
	/* underneath semi-fake-AA */
	BIF_set_color(BUTGREY, COLORSHADE_GREY);
	fdrawline(x1, y2, x2, y2);
	BIF_set_color(BUTGREY, COLORSHADE_MEDIUM);
	fdrawline(x1, y1, x2, y1);

	/* top */
	BIF_set_color(BUTGREY, COLORSHADE_DARK);
	fdrawline(x1+1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1+1, x1, y2);

	/* right */
	fdrawline(x2, y1+1, x2, y2);
	
	/* bottom */
	BIF_set_color(BUTGREY, COLORSHADE_DARK);
	fdrawline(x1+1, y1, x2, y1);
	/* END OUTER OUTLINE */
}

static void ui_emboss_NUM(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{

	float asp1;

	asp1= asp;

	/*x1+= asp1;*/
	x2-= asp1;	
	/*y1+= asp1;*/
	y2-= asp1;

	/* FLAT TEXT/NUM FIELD */
	glShadeModel(GL_FLAT);
	glBegin(GL_QUADS);

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LGREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_HILITE);
		else BIF_set_color(bc, COLORSHADE_LMEDIUM);
	}

	glVertex2f(x1,y1);
	glVertex2f(x2,y1);
	glVertex2f(x2,y2);
	glVertex2f(x1,y2);

	glEnd();
	/* END FLAT TEXT/NUM FIELD */
	
	/* OUTER SUNKEN EFFECT */
	/* left */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	glVertex2f(x1-1,y1);
	BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	glVertex2f(x1-1,y2);
	glEnd();
	
	/* right */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	glVertex2f(x2+1,y1);
	BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	glVertex2f(x2+1,y2);
	glEnd();

	/* bottom */
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	fdrawline(x1, y1-1, x2, y1-1);
	/* END OUTER SUNKEN EFFECT */

	/* OUTER OUTLINE */
	glShadeModel(GL_FLAT);
	
	/* underneath semi-fake-AA */
	BIF_set_color(BUTGREY, COLORSHADE_GREY);
	fdrawline(x1, y2, x2, y2);
	BIF_set_color(BUTGREY, COLORSHADE_MEDIUM);
	fdrawline(x1, y1, x2, y1);

	/* top */
	BIF_set_color(BUTGREY, COLORSHADE_DARK);
	fdrawline(x1+1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1+1, x1, y2);

	/* right */
	fdrawline(x2, y1+1, x2, y2);
	
	/* bottom */
	BIF_set_color(BUTGREY, COLORSHADE_DARK);
	fdrawline(x1+1, y1, x2, y1);
	/* END OUTER OUTLINE */

	/* SIDE ARROWS */
	/* left */
	
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_DARK);
		else BIF_set_color(bc, COLORSHADE_DARK);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_GREY);
		else BIF_set_color(bc, COLORSHADE_LGREY);
	}

	glEnable( GL_POLYGON_SMOOTH );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	glShadeModel(GL_FLAT);
	glBegin(GL_TRIANGLES);
	
	glVertex2f((short)x1+5,(short)(y2-(y2-y1)/2));
	glVertex2f((short)x1+10,(short)(y2-(y2-y1)/2)+4);
	glVertex2f((short)x1+10,(short)(y2-(y2-y1)/2)-4);
	glEnd();

	/* right */
	glShadeModel(GL_FLAT);
	glBegin(GL_TRIANGLES);

	glVertex2f((short)x2-5,(short)(y2-(y2-y1)/2));
	glVertex2f((short)x2-10,(short)(y2-(y2-y1)/2)-4);
	glVertex2f((short)x2-10,(short)(y2-(y2-y1)/2)+4);
	glEnd();
	
	glDisable( GL_BLEND );
	glDisable( GL_POLYGON_SMOOTH );
	/* END SIDE ARROWS */

}

static void ui_emboss_MENU(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{

	float asp1;
	
	asp1= asp;

	/*x1+= asp1;*/
	x2-= asp1;	
	/*y1+= asp1;*/
	y2-= asp1;

	/* SHADED BUTTON */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);
	

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LGREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LIGHT);
		else BIF_set_color(bc, COLORSHADE_HILITE);
	}

	glVertex2f(x1,y1);
	glVertex2f(x2,y1);

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_GREY);
		else BIF_set_color(bc, COLORSHADE_DARK);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_WHITE);
		BIF_set_color(bc, COLORSHADE_LIGHT);
	}

	glVertex2f(x2,(y2-(y2-y1)/3));
	glVertex2f(x1,(y2-(y2-y1)/3));
	glEnd();
	

	glShadeModel(GL_FLAT);
	glBegin(GL_QUADS);
	
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_GREY);
		else BIF_set_color(bc, COLORSHADE_DARK);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_WHITE);
		BIF_set_color(bc, COLORSHADE_LIGHT);
	}

	glVertex2f(x1,(y2-(y2-y1)/3));
	glVertex2f(x2,(y2-(y2-y1)/3));
	glVertex2f(x2,y2);
	glVertex2f(x1,y2);

	glEnd();
	/* END SHADED BUTTON */

	/* OUTER SUNKEN EFFECT */
	/* left */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	glVertex2f(x1-1,y1);
	BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	glVertex2f(x1-1,y2);
	glEnd();
	
	/* right */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	glVertex2f(x2+1,y1);
	BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	glVertex2f(x2+1,y2);
	glEnd();

	/* bottom */
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	fdrawline(x1, y1-1, x2, y1-1);
	/* END OUTER SUNKEN EFFECT */
	
	/* INNER OUTLINE */
	glShadeModel(GL_FLAT);
	
	/* top */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_GREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_WHITE);
		BIF_set_color(bc, COLORSHADE_WHITE);
	}

	fdrawline(x1, (y2-1), x2, y2-1);
	
	/* bottom */
	
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LGREY);
		else BIF_set_color(bc, COLORSHADE_LGREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LMEDIUM);
		BIF_set_color(bc, COLORSHADE_LMEDIUM);
	}
	fdrawline(x1, (y1+1), x2, y1+1);

	/* left */
	if(!(flag & UI_SELECT)) {
	                        	
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(bc, COLORSHADE_WHITE);
	glVertex2f(x1+1,y1+2);
	BIF_set_color(bc, COLORSHADE_LGREY);
	glVertex2f(x1+1,y2);
	glEnd();
	
	}
	
	/* right */
	if(!(flag & UI_SELECT)) {

	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(bc, COLORSHADE_LGREY);
	glVertex2f(x2-1,y1+2);
	BIF_set_color(bc, COLORSHADE_WHITE);
	glVertex2f(x2-1,y2);
	glEnd();
	
	}
	/* END INNER OUTLINE */
	
	/* OUTER OUTLINE */
	glShadeModel(GL_FLAT);
	
	/* underneath semi-fake-AA */
	BIF_set_color(BUTGREY, COLORSHADE_GREY);
	fdrawline(x1, y2, x2, y2);
	BIF_set_color(BUTGREY, COLORSHADE_MEDIUM);
	fdrawline(x1, y1, x2, y1);

	/* top */
	BIF_set_color(BUTGREY, COLORSHADE_DARK);
	fdrawline(x1+1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1+1, x1, y2);

	/* right */
	fdrawline(x2, y1+1, x2, y2);
	
	/* bottom */
	BIF_set_color(BUTGREY, COLORSHADE_DARK);
	fdrawline(x1+1, y1, x2, y1);
	/* END OUTER OUTLINE */

	/* DARKENED AREA */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	glColor4ub(0, 0, 0, 30);
	glRectf(x2-18, y1, x2, y2);

	glDisable(GL_BLEND);
	/* END DARKENED AREA */

	/* MENU DOUBLE-ARROW  */
	
	/* set antialias line */
	BIF_set_color(bc, COLORSHADE_DARK);
	
	glEnable( GL_POLYGON_SMOOTH );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	glShadeModel(GL_FLAT);
	glBegin(GL_TRIANGLES);
	glVertex2f((short)x2-4,(short)(y2-(y2-y1)/2)+1);
	glVertex2f((short)x2-12,(short)(y2-(y2-y1)/2)+1);
	glVertex2f((short)x2-8,(short)(y2-(y2-y1)/2)+4);
	glEnd();
		
	glBegin(GL_TRIANGLES);
	glVertex2f((short)x2-4,(short)(y2-(y2-y1)/2) -1);
	glVertex2f((short)x2-12,(short)(y2-(y2-y1)/2) -1);
	glVertex2f((short)x2-8,(short)(y2-(y2-y1)/2) -4);
	glEnd();
	
	glDisable( GL_BLEND );
	glDisable( GL_POLYGON_SMOOTH );
	/* MENU DOUBLE-ARROW */

}

static void ui_emboss_ICONROW(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{

	float asp1;
	
	asp1= asp;

	/*x1+= asp1;*/
	x2-= asp1;	
	/*y1+= asp1;*/
	y2-= asp1;

	/* SHADED BUTTON */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);
	

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_MEDIUM);
		else BIF_set_color(bc, COLORSHADE_LGREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LIGHT);
		else BIF_set_color(bc, COLORSHADE_HILITE);
	}

	glVertex2f(x1,y1);
	glVertex2f(x2,y1);

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LGREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_WHITE);
		BIF_set_color(bc, COLORSHADE_LIGHT);
	}

	glVertex2f(x2,(y2-(y2-y1)/3));
	glVertex2f(x1,(y2-(y2-y1)/3));
	glEnd();
	

	glShadeModel(GL_FLAT);
	glBegin(GL_QUADS);
	
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LGREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_WHITE);
		BIF_set_color(bc, COLORSHADE_LIGHT);
	}

	glVertex2f(x1,(y2-(y2-y1)/3));
	glVertex2f(x2,(y2-(y2-y1)/3));
	glVertex2f(x2,y2);
	glVertex2f(x1,y2);

	glEnd();
	/* END SHADED BUTTON */

	/* OUTER SUNKEN EFFECT */
	/* left */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	glVertex2f(x1-1,y1);
	BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	glVertex2f(x1-1,y2);
	glEnd();
	
	/* right */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	glVertex2f(x2+1,y1);
	BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	glVertex2f(x2+1,y2);
	glEnd();

	/* bottom */
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	fdrawline(x1, y1-1, x2, y1-1);
	/* END OUTER SUNKEN EFFECT */
	
	/* INNER OUTLINE */
	glShadeModel(GL_FLAT);
	
	/* top */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_GREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_WHITE);
		BIF_set_color(bc, COLORSHADE_WHITE);
	}

	fdrawline(x1, (y2-1), x2, y2-1);
	
	/* bottom */
	
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LGREY);
		else BIF_set_color(bc, COLORSHADE_LGREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LMEDIUM);
		BIF_set_color(bc, COLORSHADE_LMEDIUM);
	}
	fdrawline(x1, (y1+1), x2, y1+1);

	/* left */
	if(!(flag & UI_SELECT)) {
	                        	
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(bc, COLORSHADE_WHITE);
	glVertex2f(x1+1,y1+2);
	BIF_set_color(bc, COLORSHADE_LGREY);
	glVertex2f(x1+1,y2);
	glEnd();
	
	}
	
	/* right */
	if(!(flag & UI_SELECT)) {

	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(bc, COLORSHADE_LGREY);
	glVertex2f(x2-1,y1+2);
	BIF_set_color(bc, COLORSHADE_WHITE);
	glVertex2f(x2-1,y2);
	glEnd();
	
	}
	/* END INNER OUTLINE */
	
	/* OUTER OUTLINE */
	glShadeModel(GL_FLAT);
	
	/* underneath semi-fake-AA */
	BIF_set_color(BUTGREY, COLORSHADE_GREY);
	fdrawline(x1, y2, x2, y2);
	BIF_set_color(BUTGREY, COLORSHADE_MEDIUM);
	fdrawline(x1, y1, x2, y1);

	/* top */
	BIF_set_color(BUTGREY, COLORSHADE_DARK);
	fdrawline(x1+1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1+1, x1, y2);

	/* right */
	fdrawline(x2, y1+1, x2, y2);
	
	/* bottom */
	BIF_set_color(BUTGREY, COLORSHADE_DARK);
	fdrawline(x1+1, y1, x2, y1);
	/* END OUTER OUTLINE */

	/* DARKENED AREA */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	glColor4ub(0, 0, 0, 30);
	glRectf(x2-9, y1, x2, y2);

	glDisable(GL_BLEND);
	/* END DARKENED AREA */

	/* MENU DOUBLE-ARROW  */
	
	/* set antialias line */
	BIF_set_color(bc, COLORSHADE_DARK);
	
	glEnable( GL_POLYGON_SMOOTH );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	glShadeModel(GL_FLAT);
	glBegin(GL_TRIANGLES);
	glVertex2f((short)x2-2,(short)(y2-(y2-y1)/2)+1);
	glVertex2f((short)x2-6,(short)(y2-(y2-y1)/2)+1);
	glVertex2f((short)x2-4,(short)(y2-(y2-y1)/2)+4);
	glEnd();
		
	glBegin(GL_TRIANGLES);
	glVertex2f((short)x2-2,(short)(y2-(y2-y1)/2) -1);
	glVertex2f((short)x2-6,(short)(y2-(y2-y1)/2) -1);
	glVertex2f((short)x2-4,(short)(y2-(y2-y1)/2) -4);
	glEnd();
	
	glDisable( GL_BLEND );
	glDisable( GL_POLYGON_SMOOTH );
	/* MENU DOUBLE-ARROW */

}

static void ui_emboss_TABL(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{

	float asp1;
	
	asp1= asp;

	/*x1+= asp1;*/
	x2-= asp1;	
	/*y1+= asp1;*/
	y2-= asp1;

	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);
	

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LMEDIUM);
		else BIF_set_color(bc, COLORSHADE_MEDIUM);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LMEDIUM);
		else BIF_set_color(bc, COLORSHADE_MEDIUM);
	}

	
	//BIF_set_color(bc, COLORSHADE_MEDIUM);

	glVertex2f(x1,y1);
	glVertex2f(x2,y1);

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LGREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_WHITE);
		BIF_set_color(bc, COLORSHADE_LIGHT);
	}


	//BIF_set_color(bc, COLORSHADE_LIGHT);

	//glVertex2f(x2,(y1+(y2-y1)/2));
	glVertex2f(x2,(y2-(y2-y1)/3));
	glVertex2f(x1,(y2-(y2-y1)/3));
	glEnd();
	

	glShadeModel(GL_FLAT);
	glBegin(GL_QUADS);
	
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_GREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_LIGHT);
		BIF_set_color(bc, COLORSHADE_LIGHT);
	}

	//BIF_set_color(bc, COLORSHADE_LIGHT);

	glVertex2f(x1,(y2-(y2-y1)/3));
	glVertex2f(x2,(y2-(y2-y1)/3));
	glVertex2f(x2,y2);
	glVertex2f(x1,y2);

	glEnd();


	/* inner outline */
	glShadeModel(GL_FLAT);
	
	/* top */
	
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_GREY);
		else BIF_set_color(bc, COLORSHADE_GREY);
	}
	else {
		if(flag & UI_ACTIVE) BIF_set_color(bc, COLORSHADE_WHITE);
		BIF_set_color(bc, COLORSHADE_WHITE);
	}

	fdrawline(x1, (y2-1), x2, y2-1);


	/* left */
	if(!(flag & UI_SELECT)) {
	                        	
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(bc, COLORSHADE_WHITE);
	glVertex2f(x1+1,y1-1);
	BIF_set_color(bc, COLORSHADE_MEDIUM);
	glVertex2f(x1+1,y2);
	glEnd();
	
	}
	
	/* right */
	
	if(!(flag & UI_SELECT)) {

	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(bc, COLORSHADE_MEDIUM);
	glVertex2f(x2-1,y1+2);
	BIF_set_color(bc, COLORSHADE_WHITE);
	glVertex2f(x2-1,y2);
	glEnd();
	
	}

	/* outer outline */
	glShadeModel(GL_FLAT);
	
	/* underneath semi-fake-AA */
	BIF_set_color(BUTGREY, COLORSHADE_GREY);
	fdrawline(x1, y2, x2, y2);
	BIF_set_color(BUTGREY, COLORSHADE_MEDIUM);
	fdrawline(x1, y1, x2, y1);

	/* top */
	BIF_set_color(BUTGREY, COLORSHADE_DARK);
	fdrawline(x1+1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1, x1, y2);

	/* right */
	fdrawline(x2, y1, x2, y2);

	/* outer sunken effect */
	/* left */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	glVertex2f(x1-1,y1);
	BIF_set_color(BUTGREY, COLORSHADE_MEDIUM);
	glVertex2f(x1-1,y2);
	glEnd();
	

	glShadeModel(GL_FLAT);

}
static void ui_emboss_TABM(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{
}
static void ui_emboss_TABR(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{
}


void uiEmboss(float x1, float y1, float x2, float y2, int sel)
{
	
	/* below */
	if(sel) glColor3ub(200,200,200);
	else glColor3ub(50,50,50);
	fdrawline(x1, y1, x2, y1);

	/* right */
	fdrawline(x2, y1, x2, y2);
	
	/* top */
	if(sel) glColor3ub(50,50,50);
	else glColor3ub(200,200,200);
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

/* minimal for menus */
static void ui_emboss_M(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{
	x1+= 1.0;
	y1+= 1.0;
	x2-= 1.0+asp;
	y2-= 1.0+asp;
	
	
	BIF_set_color(bc, COLORSHADE_WHITE);
		
	fdrawbox(x1, y1, x2, y2);

	/* 
	if(flag & UI_SELECT) {
		BIF_set_color(bc, COLORSHADE_LIGHT);
		

		fdrawline(x1, y1, x2, y1);


		fdrawline(x2, y1, x2, y2);
	}
	else if(flag & UI_ACTIVE) {
		BIF_set_color(bc, COLORSHADE_WHITE);


		fdrawline(x1, y2, x2, y2);
	

		fdrawline(x1, y1, x1, y2);
	}
	else {
		BIF_set_color(bc, COLORSHADE_MEDIUM);
		
		fdrawbox(x1, y1, x2, y2);
	}
	*/
}


/* nothing! */
static void ui_emboss_N(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int sel)
{
}

/* pulldown menu */
static void ui_emboss_P(BIFColorID bc, float asp, float x1, float y1, float x2, float y2, int flag)
{

	if(flag & UI_ACTIVE) {
		BIF_set_color(bc, COLORSHADE_DARK);
		glRectf(x1-1, y1, x2+2, y2);

	} else {
		BIF_set_color(bc, COLORSHADE_LMEDIUM);
		glRectf(x1-1, y1, x2+2, y2);
	}

	glDisable(GL_BLEND);

}

static void ui_emboss_slider(uiBut *but, float fac)
{
	float x1, x2, y1, y2, ymid, yc;

	x1= but->x1; x2= but->x2;
	y1= but->y1; y2= but->y2;
	
	/* the slider background line */
	ymid= (y1+y2)/2.0;
	yc= 1.7*but->aspect;	// height of center line

	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);

	if(but->flag & UI_ACTIVE) 
		BIF_set_color(BUTGREY, COLORSHADE_LMEDIUM);
	else 
		BIF_set_color(BUTGREY, COLORSHADE_MEDIUM);
	
	
	glVertex2f(x1,   ymid-yc);
	glVertex2f(x2, ymid-yc);

	if(but->flag & UI_ACTIVE) 
		BIF_set_color(BUTGREY, COLORSHADE_LIGHT);
	else 
		BIF_set_color(BUTGREY, COLORSHADE_LMEDIUM);

	glVertex2f(x2, ymid+yc);
	glVertex2f(x1,   ymid+yc);

	glEnd();

	BIF_set_color(but->col, COLORSHADE_DARK);
	fdrawline(x1+1, ymid-yc, x2, ymid-yc);
	
	/* the movable slider */
	if(but->flag & UI_SELECT) BIF_set_color(but->col, COLORSHADE_WHITE);
	else BIF_set_color(but->col, COLORSHADE_GREY);

	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);

	BIF_set_color(BUTGREY, COLORSHADE_GREY);

	glVertex2f(x1,     y1+2.5);
	glVertex2f(x1+fac, y1+2.5);

	BIF_set_color(BUTGREY, COLORSHADE_MEDIUM);

	glVertex2f(x1+fac, y2-2.5);
	glVertex2f(x1,     y2-2.5);

	glEnd();
	

	/* slider handle center */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);

	BIF_set_color(BUTGREY, COLORSHADE_MEDIUM);
	glVertex2f(x1+fac-3, y1+2);
	glVertex2f(x1+fac, y1+4);
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	glVertex2f(x1+fac, y2-2);
	glVertex2f(x1+fac-3, y2-2);

	glEnd();
	
	/* slider handle left bevel */
	BIF_set_color(BUTGREY, COLORSHADE_WHITE);
	fdrawline(x1+fac-3, y2-2, x1+fac-3, y1+2);
	
	/* slider handle right bevel */
	BIF_set_color(BUTGREY, COLORSHADE_GREY);
	fdrawline(x1+fac, y2-2, x1+fac, y1+2);

	glShadeModel(GL_FLAT);
}

static void ui_draw_but_BUT(uiBut *but)
{
	float x=0.0;
	
	/* check for button text label */
	if (but->type == ICONTEXTROW) {
		but->embossfunc = ui_emboss_ICONROW;
		but->flag |= UI_ICON_LEFT;
		ui_draw_icon(but, (BIFIconID) (but->icon+but->iconadd));
	} else if (but->type == ICONROW) {
		but->flag |= UI_ICON_LEFT;
		but->embossfunc = ui_emboss_ICONROW;
	}
	
	but->embossfunc(but->col, but->aspect, but->x1, but->y1, but->x2, but->y2, but->flag);
	
	if(but->embossfunc==ui_emboss_TABL) {
		but->flag |= UI_TEXT_LEFT;
		but->flag |= UI_ICON_RIGHT;
		but->flag &= ~UI_ICON_LEFT;
	}
	
	//but->flag |= UI_TEXT_LEFT;

	/* check for button text label */
	if (but->type == ICONTEXTROW) {
		ui_draw_icon(but, (BIFIconID) (but->icon+but->iconadd));
	}
	else if(but->drawstr[0]!=0) {
  
		/* If there's an icon too (made with uiDefIconTextBut) then draw the icon
		 * and offset the text label to accomodate it 
		 */
		if ( but->flag & UI_HAS_ICON) {
			if (but->flag & UI_ICON_LEFT) {
				ui_draw_icon(but, but->icon);

				if(but->flag & UI_TEXT_LEFT) x= but->x1+24.0;
				else x= (but->x1+but->x2-but->strwidth+1)/2.0;
			} else if (but->flag & UI_ICON_RIGHT) {
				ui_draw_icon(but, but->icon);

				if(but->flag & UI_TEXT_LEFT) x= but->x1+4.0;
				else x= (but->x1+but->x2-but->strwidth+1)/2.0;
			}
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

		BIF_DrawString(but->font, but->drawstr+but->ofs, (U.transopts & TR_BUTTONS), but->flag & UI_SELECT);
	}
	/* if there's no text label, then check to see if there's an icon only and draw it */
	else if( but->flag & UI_HAS_ICON ) {
		ui_draw_icon(but, (BIFIconID) (but->icon+but->iconadd));
	}
}

static void ui_draw_but_MENU(uiBut *but)
{
	float x;

	but->embossfunc = ui_emboss_MENU;

	but->embossfunc(but->col, but->aspect, but->x1, but->y1, but->x2, but->y2, but->flag);

	but->flag |= UI_TEXT_LEFT;

	/* check for button text label */
	if (but->type == ICONTEXTROW) {
		ui_draw_icon(but, (BIFIconID) (but->icon+but->iconadd));
	}
	else if(but->drawstr[0]!=0) {
		
		/* If there's an icon too (made with uiDefIconTextBut) then draw the icon
		and offset the text label to accomodate it */
	        if ( (but->flag & UI_HAS_ICON) && (but->flag & UI_ICON_LEFT) ) {
			ui_draw_icon(but, but->icon);

			if(but->flag & UI_TEXT_LEFT) x= but->x1+28.0;
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

		BIF_DrawString(but->font, but->drawstr+but->ofs, (U.transopts & TR_BUTTONS), but->flag & UI_SELECT);
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
		
		BIF_DrawStringRGB(but->font, but->drawstr+but->ofs, (U.transopts & TR_BUTTONS), r, g, b);
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
	
	but->embossfunc = ui_emboss_TEX;

	but->embossfunc(but->col, but->aspect, but->x1, but->y1, but->x2, but->y2, sel);
	
	sel= but->flag & UI_SELECT;

	/* draw cursor */
	if(but->pos != -1) {
		
		pos= but->pos+strlen(but->str);
		if(pos >= but->ofs) {
			ch= but->drawstr[pos];
			but->drawstr[pos]= 0;

			t= but->aspect*BIF_GetStringWidth(but->font, but->drawstr+but->ofs, (U.transopts & TR_BUTTONS)) + 3;

			but->drawstr[pos]= ch;
			glColor3ub(255,0,0);

			glRects(but->x1+t, but->y1+2, but->x1+t+3, but->y2-2);
		}	
	}
	if(but->drawstr[0]!=0) {
		/* make text white if selected (editing) */
		if (but->flag & UI_SELECT) glColor3ub(255,255,255);
		else glColor3ub(0,0,0);

		if(but->flag & UI_TEXT_LEFT) x= but->x1+4.0;
		else x= (but->x1+but->x2-but->strwidth+1)/2.0;
		
		glRasterPos2f( x, (but->y1+but->y2- 9.0)/2.0);
		
		/* last arg determines text black (0) or whilte (1) */
		BIF_DrawString(but->font, but->drawstr+but->ofs, (U.transopts & TR_BUTTONS), but->flag & UI_SELECT);
	}
}

static void ui_draw_but_NUM(uiBut *but)
{
	
	float x;
	but->embossfunc = ui_emboss_NUM;

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

		BIF_DrawString(but->font, but->drawstr+but->ofs, (U.transopts & TR_BUTTONS), but->flag & UI_SELECT);
	}
	/* if there's no text label, then check to see if there's an icon only and draw it */
	else if( but->flag & UI_HAS_ICON ) {
		ui_draw_icon(but, (BIFIconID) (but->icon+but->iconadd));
	}

}

static void ui_draw_but_BUTM(uiBut *but)
{
	float x=0;
	short len;
	char *cpoin;
	int sel;

	if (but->type == MENU) {
		but->embossfunc = ui_emboss_MENU;
	}
	
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
		if ( but->flag & UI_HAS_ICON ) {
		        if (but->flag & UI_ICON_LEFT ) {
				ui_draw_icon(but, but->icon);

				x= but->x1+22.0;
			} else if (but->flag & UI_ICON_RIGHT) {
				ui_draw_icon(but, but->icon);

				x= but->x1+4.0;
			}
		}
		else {
		        x= but->x1+4.0;
		}

		glRasterPos2f( x, (but->y1+but->y2- 9.0)/2.0);

		BIF_DrawString(but->font, but->drawstr, (U.transopts & TR_BUTTONS), sel);
		
		if(cpoin) {
			len= BIF_GetStringWidth(but->font, cpoin+1, (U.transopts & TR_BUTTONS));
			glRasterPos2f( but->x2 - len*but->aspect-3, (but->y1+but->y2- 9.0)/2.0);
			BIF_DrawString(but->font, cpoin+1, (U.transopts & TR_BUTTONS), but->flag & UI_ACTIVE);
			*cpoin= '|';
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

		BIF_DrawString(but->font, but->drawstr+but->ofs, (U.transopts & TR_BUTTONS), sel);
	}
	/* if there's no text label, then check to see if there's an icon only and draw it */
	else if( but->flag & UI_HAS_ICON ) {
		ui_draw_icon(but, but->icon);
	}
}

static void ui_draw_but_SEPR(uiBut *but)
{
	//float y= (but->y1+but->y2)/2.0;

	BIF_set_color(but->col, COLORSHADE_MEDIUM);
	glRectf(but->x1-2, but->y1-1, but->x2+2, but->y2);

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
	case KEYEVT:
	case IDPOIN:
	case ICONROW:
	case ICONTEXTROW:
		ui_draw_but_BUT(but);
		break;
	
	case NUM:
		ui_draw_but_NUM(but);
		break;
	
	case TEX:
		ui_draw_but_TEX(but);
		break;
	
	case BUTM:
	case BLOCK:
		ui_draw_but_BUTM(but);
		break;
	
	case MENU:
		ui_draw_but_MENU(but);
		break;
	
	case NUMSLI:
	case HSVSLI:
	
		ui_draw_but_BUT(but);
		
		/* the slider */

		x1= but->x1; x2= but->x2;
		y1= but->y1; y2= but->y2;
		
		but->x1= (but->x1+but->x2)/2;
		but->x2-= 5.0*but->aspect;

		but->y1+= 2.0*but->aspect;
		but->y2-= 2.0*but->aspect;
		
		value= ui_get_but_val(but);
		fac= (value-but->min)*(but->x2-but->x1)/(but->max - but->min);
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
		glRectf((but->x1), (but->y1), (but->x2), (but->y2));
		glColor3ub(0,  0,  0);
		fdrawbox((but->x1), (but->y1), (but->x2), (but->y2));
		break;

	case LINK:
		ui_draw_but_LINK(but);
		break;

	case INLINK:
		ui_draw_but_LINK(but);
		break;
	}
}

/* --------- generic helper drawng calls ---------------- */

/* supposes you draw the actual box atop of this. */
void uiSoftShadow(float minx, float miny, float maxx, float maxy, float rad, int alpha)
{

	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	/* quads start left-top, clockwise */
	
	/* left */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( minx-rad, maxy-rad);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( minx+rad, maxy-rad);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( minx+rad, miny+rad);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( minx-rad, miny-rad);
	glEnd();

	/* bottom */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( minx+rad, miny+rad);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( maxx-rad, miny+rad);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( maxx+rad, miny-rad);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( minx-rad, miny-rad);
	glEnd();

	/* right */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( maxx-rad, maxy-rad);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( maxx+rad, maxy-rad);
	glColor4ub(0, 0, 0, 0);
	glVertex2f( maxx+rad, miny-rad);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f( maxx-rad, miny+rad);
	glEnd();

	glDisable(GL_BLEND);
	glShadeModel(GL_FLAT);
}


#define UI_RB_ALPHA 16
static int roundboxtype= 15;

void uiSetRoundBox(int type)
{
	roundboxtype= type;
	
	/* flags to set which corners will become rounded:

	1------2
	|      |
	8------4
	*/
	
}

void gl_round_box_topshade(float minx, float miny, float maxx, float maxy, float rad)
{
	float vec[7][2]= {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                  {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	char col[7]= {140, 165, 195, 210, 230, 245, 255};
	int a;
	char alpha=255;
	
	if(roundboxtype & UI_RB_ALPHA) alpha= 128;
	
	/* mult */
	for(a=0; a<7; a++) {
		vec[a][0]*= rad; vec[a][1]*= rad;
	}

	/* shades from grey->white->grey */
	glBegin(GL_LINE_STRIP);
	
	if(roundboxtype & 3) {
		/* corner right-top */
		glColor4ub(140, 140, 140, alpha);
		glVertex2f( maxx, maxy-rad);
		for(a=0; a<7; a++) {
			glColor4ub(col[a], col[a], col[a], alpha);
			glVertex2f( maxx-vec[a][1], maxy-rad+vec[a][0]);
		}
		glColor4ub(225, 225, 225, alpha);
		glVertex2f( maxx-rad, maxy);
	
		
		/* corner left-top */
		glVertex2f( minx+rad, maxy);
		for(a=0; a<7; a++) {
			glColor4ub(col[6-a], col[6-a], col[6-a], alpha);
			glVertex2f( minx+rad-vec[a][0], maxy-vec[a][1]);
		}
		glVertex2f( minx, maxy-rad);
	}
	else {
		glColor4ub(225, 225, 225, alpha);
		glVertex2f( minx, maxy);
		glVertex2f( maxx, maxy);
	}
	
	glEnd();
}


void gl_round_box(float minx, float miny, float maxx, float maxy, float rad)
{
	float vec[7][2]= {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                  {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	int a;
	
	/* mult */
	for(a=0; a<7; a++) {
		vec[a][0]*= rad; vec[a][1]*= rad;
	}

	/* start with corner right-bottom */
	if(roundboxtype & 4) {
		glVertex2f( maxx-rad, miny);
		for(a=0; a<7; a++) {
			glVertex2f( maxx-rad+vec[a][0], miny+vec[a][1]);
		}
		glVertex2f( maxx, miny+rad);
	}
	else glVertex2f( maxx, miny);
	
	/* corner right-top */
	if(roundboxtype & 2) {
		glVertex2f( maxx, maxy-rad);
		for(a=0; a<7; a++) {
			glVertex2f( maxx-vec[a][1], maxy-rad+vec[a][0]);
		}
		glVertex2f( maxx-rad, maxy);
	}
	else glVertex2f( maxx, maxy);
	
	/* corner left-top */
	if(roundboxtype & 1) {
		glVertex2f( minx+rad, maxy);
		for(a=0; a<7; a++) {
			glVertex2f( minx+rad-vec[a][0], maxy-vec[a][1]);
		}
		glVertex2f( minx, maxy-rad);
	}
	else glVertex2f( minx, maxy);
	
	/* corner left-bottom */
	if(roundboxtype & 8) {
		glVertex2f( minx, miny+rad);
		for(a=0; a<7; a++) {
			glVertex2f( minx+vec[a][1], miny+rad-vec[a][0]);
		}
		glVertex2f( minx+rad, miny);
	}
	else glVertex2f( minx, miny);
	
}

/* for headers and floating panels */
void uiRoundBoxEmboss(float minx, float miny, float maxx, float maxy, float rad)
{
	float color[4];
	
	if(roundboxtype & UI_RB_ALPHA) {
		glGetFloatv(GL_CURRENT_COLOR, color);
		color[3]= 0.5;
		glColor4fv(color);
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	
	/* solid part */
	glBegin(GL_POLYGON);
	gl_round_box(minx, miny, maxx, maxy, rad);
	glEnd();
	
	/* set antialias line */
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	gl_round_box_topshade(minx+1, miny+1, maxx-1, maxy-1, rad);

	if(roundboxtype & UI_RB_ALPHA) glColor4ub(0,0,0, 128); else glColor4ub(0,0,0, 255);
	glBegin(GL_LINE_LOOP);
	gl_round_box(minx, miny, maxx, maxy, rad);
	glEnd();
   
	glDisable( GL_BLEND );
	glDisable( GL_LINE_SMOOTH );

}


/* plain antialiased unfilled rectangle */
void uiRoundRect(float minx, float miny, float maxx, float maxy, float rad)
{
	float color[4];
	
	if(roundboxtype & UI_RB_ALPHA) {
		glGetFloatv(GL_CURRENT_COLOR, color);
		color[3]= 0.5;
		glColor4fv(color);
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	
	/* set antialias line */
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glBegin(GL_LINE_LOOP);
	gl_round_box(minx, miny, maxx, maxy, rad);
	glEnd();
   
	glDisable( GL_BLEND );
	glDisable( GL_LINE_SMOOTH );
}



/* plain antialiased filled box */
void uiRoundBox(float minx, float miny, float maxx, float maxy, float rad)
{
	float color[4];
	
	if(roundboxtype & UI_RB_ALPHA) {
		glGetFloatv(GL_CURRENT_COLOR, color);
		color[3]= 0.5;
		glColor4fv(color);
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	
	/* solid part */
	glBegin(GL_POLYGON);
	gl_round_box(minx, miny, maxx, maxy, rad);
	glEnd();
	
	/* set antialias line */
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glBegin(GL_LINE_LOOP);
	gl_round_box(minx, miny, maxx, maxy, rad);
	glEnd();
   
	glDisable( GL_BLEND );
	glDisable( GL_LINE_SMOOTH );
}




void uiDrawMenuBox(float minx, float miny, float maxx, float maxy)
{
	BIF_set_color(MENUCOL, COLORSHADE_MEDIUM);

	glRectf(minx, miny, maxx, maxy);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glColor4ub(0, 0, 0, 100);
	fdrawline(minx+4, miny, maxx+1, miny);
	fdrawline(maxx+1, miny, maxx+1, maxy-4);
	
	glColor4ub(0, 0, 0, 80);
	fdrawline(minx+4, miny-1, maxx+1, miny-1);
	fdrawline(maxx+1, miny-1, maxx+1, maxy-4);

	glColor4ub(0, 0, 0, 55);
	fdrawline(minx+4, miny-2, maxx+2, miny-2);
	fdrawline(maxx+2, miny-2, maxx+2, maxy-4);

	glColor4ub(0, 0, 0, 35);
	fdrawline(minx+4, miny-3, maxx+3, miny-3);
	fdrawline(maxx+3, miny-3, maxx+3, maxy-4);

	glColor4ub(0, 0, 0, 20);
	fdrawline(minx+4, miny-4, maxx+4, miny-4);
	fdrawline(maxx+4, miny-4, maxx+4, maxy-4);

	glDisable(GL_BLEND);
	
	/* below */
	//glColor3ub(0,0,0);
	//fdrawline(minx, miny, maxx, miny);

	/* right */
	//fdrawline(maxx, miny, maxx, maxy);
	
	/* top */
	//glColor3ub(255,255,255);
	//fdrawline(minx, maxy, maxx, maxy);

	/* left */
	//fdrawline(minx, miny, minx, maxy);

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
			j= BIF_GetStringWidth(bt->font, bt->drawstr, (U.transopts & TR_BUTTONS));

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
	
	xsize= block->maxx - block->minx+4; // 4 for shadow
	ysize= block->maxy - block->miny+4;
	
	if(but) {
		rctf butrct;
		short left=0, right=0, top=0, down=0;
		short dir1, dir2 = 0;

		butrct.xmin= but->x1; butrct.xmax= but->x2;
		butrct.ymin= but->y1; butrct.ymax= but->y2;
		
		if(but->block->panel) {
			butrct.xmin += but->block->panel->ofsx;
			butrct.ymin += but->block->panel->ofsy;
			butrct.xmax += but->block->panel->ofsx;
			butrct.ymax += but->block->panel->ofsy;
		}

		/* added this for submenu's... */
		Mat4CpyMat4(UIwinmat, block->winmat);

		ui_graphics_to_window(block->win, &butrct.xmin, &butrct.ymin);
		ui_graphics_to_window(block->win, &butrct.xmax, &butrct.ymax);
		block->parentrct= butrct;	// will use that for pulldowns later

		if( butrct.xmin-xsize > 0.0) left= 1;
		if( butrct.xmax+xsize < G.curscreen->sizex) right= 1;
		if( butrct.ymin-ysize > 0.0) down= 1;
		if( butrct.ymax+ysize < G.curscreen->sizey) top= 1;
		
		dir1= block->direction;
		if(dir1==UI_LEFT || dir1==UI_RIGHT) dir2= UI_DOWN;
		if(dir1==UI_TOP || dir1==UI_DOWN) dir2= UI_LEFT;
		
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
		
		if(but->block->panel) {
			xof += but->block->panel->ofsx;
			yof += but->block->panel->ofsy;
		}
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

void uiDrawBlock(uiBlock *block)
{
	uiBut *but;

	if(block->autofill) ui_autofill(block);
	if(block->minx==0.0 && block->maxx==0.0) uiBoundsBlock(block, 0);

	uiPanelPush(block); // panel matrix
	
	if(block->flag & UI_BLOCK_LOOP) {
		BIF_set_color(block->col, COLORSHADE_HILITE);
		uiDrawMenuBox(block->minx, block->miny, block->maxx, block->maxy);
	}
	else if(block->panel) ui_draw_panel(block);
	
	if(block->drawextra) block->drawextra();

	for (but= block->buttons.first; but; but= but->next) {
		ui_draw_but(but);
	}

	ui_draw_links(block);
	
	uiPanelPop(block); // matrix restored
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

static void ui_warp_pointer(short x, short y)
{
	/* OSX has very poor mousewarp support, it sends events;
	   this causes a menu being pressed immediately ... */
	#ifndef __APPLE__
	warp_pointer(x, y);
	#endif
}

#if 0
static int ui_do_but_MENUo(uiBut *but)
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
	uiBlockSetCol(block, MENUCOL);
	
	md= decompose_menu_string(but->str);

	/* columns and row calculation */
	columns= (md->nitems+20)/20;
	if (columns<1) columns= 1;
	
	rows= (int) md->nitems/columns;
	if (rows<1) rows= 1;
	
	while (rows*columns<md->nitems) rows++;
		
	/* size and location */
	if(md->title)
		width= 2*strlen(md->title)+BIF_GetStringWidth(block->curfont, md->title, (U.transopts & TR_MENUS));
	else
		width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= BIF_GetStringWidth(block->curfont, md->items[a].str, (U.transopts & TR_MENUS));
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

	ui_warp_pointer(mval[0]+mousemove[0], mval[1]+mousemove[1]);

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
	
	if((event & UI_RETURN_OUT)==0) ui_warp_pointer(mousemove[0], mousemove[1]);
	
	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);
	
	uibut_do_func(but);

	return event;	
}
#endif

static int ui_do_but_MENU(uiBut *but)
{
	uiBlock *block;
	ListBase listb={NULL, NULL};
	double fvalue;
	int width, height, a, xmax, starty;
	short startx;
	int columns=1, rows=0, boxh, event;
	short  x1, y1;
	short mval[2];
	MenuData *md;

	but->flag |= UI_SELECT;
	ui_draw_but(but);

	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, but->win);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;
	uiBlockSetCol(block, MENUCOL);
	
	md= decompose_menu_string(but->str);

	/* columns and row calculation */
	columns= (md->nitems+20)/20;
	if (columns<1) columns= 1;
	
	rows= (int) md->nitems/columns;
	if (rows<1) rows= 1;
	
	while (rows*columns<md->nitems) rows++;
		
	/* size and location */
	if(md->title)
		width= 1.5*but->aspect*strlen(md->title)+BIF_GetStringWidth(block->curfont, md->title, (U.transopts & TR_MENUS));
	else
		width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= but->aspect*BIF_GetStringWidth(block->curfont, md->items[a].str, (U.transopts & TR_MENUS));
		if(xmax>width) width= xmax;
	}

	width+= 10;
	if (width<50) width=50;

	boxh= TBOXH;
	
	height= rows*boxh;
	if (md->title) height+= boxh;
	
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

	/* here we go! */
	startx= but->x1;
	starty= but->y1;
	
	if(md->title) {
		uiBut *bt;
		uiSetCurFont(block, block->font+1);
		bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+rows*boxh), (short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
		uiSetCurFont(block, block->font);
		bt->flag= UI_TEXT_LEFT;
	}

	for(a=0; a<md->nitems; a++) {

		x1= but->x1 + width*((int)a/rows);
		y1= but->y1 - boxh*(a%rows) + (rows-1)*boxh; 

		if (strcmp(md->items[a].str, "%l")==0) {
			uiDefBut(block, SEPR, B_NOP, "", x1, y1,(short)(width-(rows>1)), (short)(boxh-1), NULL, 0.0, 0.0, 0, 0, "");
		}
		else {
			uiDefBut(block, BUTM|but->pointype, but->retval, md->items[a].str, x1, y1,(short)(width-(rows>1)), (short)(boxh-1), but->poin, (float) md->items[a].retval, 0.0, 0, 0, "");
		}
	}
	
	ui_positionblock(block, but);
	block->win= G.curscreen->mainwin;
	event= uiDoBlocks(&listb, 0);
	
	menudata_free(md);
	
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

		if (but->flag != oflag) {
			ui_draw_but(but);
			glFinish(); // flush display in subloops
		}
		
		PIL_sleep_ms(10);
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

	while((but->aspect*BIF_GetStringWidth(but->font, backstr+but->ofs, (U.transopts & TR_BUTTONS)) + but->x1) > mval[0]) {
		if (but->pos <= 0) break;
		but->pos--;
		backstr[but->pos+but->ofs] = 0;
	}
	
	but->pos -= strlen(but->str);
	but->pos += but->ofs;
	if(but->pos<0) but->pos= 0;

	/* backup */
	BLI_strncpy(backstr, but->poin, UI_MAX_DRAW_STR);

	ui_draw_but(but);
	glFinish(); // flush display in subloops
	
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
	glFinish(); // flush display before subloop
	
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
	uiBlockSetCol(block, MENUCOL);
	
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
	uiBlockSetCol(block, MENUCOL);
	md= decompose_menu_string(but->str);

	/* size and location */
	/* expand menu width to fit labels */
	if(md->title)
		width= 2*strlen(md->title)+BIF_GetStringWidth(block->curfont, md->title, (U.transopts & TR_MENUS));
	else
		width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= BIF_GetStringWidth(block->curfont, md->items[a].str, (U.transopts & TR_MENUS));
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
			uiDefIconTextBut(block, BUTM|but->pointype, but->retval, (short)((but->icon)+(md->items[a].retval-but->min)), md->items[a].str, 0, ypos,(short)width, 19, but->poin, (float) md->items[a].retval, 0.0, 0, 0, "");
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

	if( but->type==NUMSLI) deler= ( (but->x2-but->x1)/2 - 5.0*but->aspect);
	else if( but->type==HSVSLI) deler= ( (but->x2-but->x1)/2 - 5.0*but->aspect);
	else deler= (but->x2-but->x1- 5.0*but->aspect);
	

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

static void setup_file(uiBlock *block)
{
	uiBut *but;
	FILE *fp;

	fp= fopen("butsetup","w");
	if(fp==NULL);
	else {
		but= block->buttons.first;
		while(but) {
			// if(but->rt[3]==1) {
			ui_check_but(but);
			fprintf(fp,"%d,%d,%d,%d   %s %s\n", (int)but->x1, (int)but->y1, (int)( but->x2-but->x1), (int)(but->y2-but->y1), but->str, but->tip);
			// }
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
			glFinish();
			didit= 1;
			but->rt[3]= 1;
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
		if(uevent->val) {
			retval= ui_do_but_BLOCK(but);
			block->auto_open= 1;
		}
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

/* only to be used to prevent an 'outside' event when using nested pulldowns */
/* four checks:
  - while mouse moves in good x direction
  - while mouse motion x is bigger than y motion
  - while distance to center block diminishes
  - only for 1 second
  
  return 0: check outside
*/
static int ui_mouse_motion_towards_block(uiBlock *block, uiEvent *uevent)
{
	short mvalo[2], dx, dy, domx, domy, x1, y1;
	int disto, dist, counter=0;

	if(block->direction==UI_TOP || block->direction==UI_DOWN) return 0;
	if(uevent->event!= MOUSEX && uevent->event!= MOUSEY) return 0;
	
	/* calculate dominant direction */
	domx= ( -uevent->mval[0] + (block->maxx+block->minx)/2 );
	domy= ( -uevent->mval[1] + (block->maxy+block->miny)/2 );
	/* we need some accuracy */
	if( abs(domx)<4 ) return 0;
	
	/* calculte old dist */
	disto= domx*domx + domy*domy;
	
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
			if( abs(dy) > abs(dx) ) {
				//printf("left because y>x direction\n");
				return 0;
			}
			
			if( dx>0 && domx>0);
			else if(dx<0 && domx<0);
			else {
				//printf("left because dominant direction\n");
				return 0;
			}
			
		}
		
		/* check dist */
		x1= ( -uevent->mval[0] + (block->maxx+block->minx)/2 );
		y1= ( -uevent->mval[1] + (block->maxy+block->miny)/2 );
		dist= x1*x1 + y1*y1;
		if(dist > disto) {
			//printf("left because distance\n");
			return 0;
		}
		else disto= dist;

		/* idle for this poor code */
		PIL_sleep_ms(10);
		counter++;
		if(counter > 100) {
			// printf("left because of timer (1 sec)\n");
			return 0;
		}
	}
	
	return 0;
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
	if(uevent==0 || uevent->event==LEFTSHIFTKEY || uevent->event==RIGHTSHIFTKEY) return UI_NOTHING;
	
	if(block->flag & UI_BLOCK_ENTER_OK) {
		if(uevent->event == RETKEY && uevent->val) {
			// printf("qual: %d %d %d\n", uevent->qual, get_qual(), G.qual);
			if ((G.qual & LR_SHIFTKEY) == 0) {
				return UI_RETURN_OK;
			}
		}
	}		

	Mat4CpyMat4(UIwinmat, block->winmat);
	uiPanelPush(block); // push matrix; no return without pop!

	uiGetMouse(mywinget(), uevent->mval);	/* transformed mouseco */

	/* check boundbox and panel events */
	if( block->minx <= uevent->mval[0] && block->maxx >= uevent->mval[0] ) {
		
		if(block->panel==NULL) {
			if( block->miny <= uevent->mval[1] && block->maxy >= uevent->mval[1] )
				inside= 1;
		}
		else if(block->panel->paneltab==NULL) {
		
			if( block->miny <= uevent->mval[1] && block->maxy >= uevent->mval[1] ) inside= 1;

			/* clicked at panel header? */
			if(uevent->event==LEFTMOUSE) {
				if( block->panel->flag & PNL_CLOSEDX) {
					if(block->minx <= uevent->mval[0] && block->minx+PNL_HEADER >= uevent->mval[0]) 
						inside= 2;
				}
				else if( (block->maxy <= uevent->mval[1]) && (block->maxy+PNL_HEADER >= uevent->mval[1]) )
					inside= 2;
				
				if(inside==2) {
					uiPanelPop(block); 	// pop matrix; no return without pop!
					ui_do_panel(block, uevent);
					return UI_EXIT_LOOP;	// exit loops because of moving panels
				}
			}
			else if(uevent->event==PADPLUSKEY || uevent->event==PADMINUS) {
				SpaceLink *sl= curarea->spacedata.first;
				
				if(uevent->event==PADPLUSKEY) sl->blockscale+= 0.1;
				else sl->blockscale-= 0.1;
				CLAMP(sl->blockscale, 0.6, 1.0);
				addqueue(block->winq, REDRAW, 1);
				retval= UI_CONT;
			}
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
						if(but->type != LABEL && but->embossfunc != ui_emboss_N) ui_draw_but(but);
					}
				}
				/* hilite case 2 */
				if(but->flag & UI_ACTIVE) {
					if( (but->flag & UI_MOUSE_OVER)==0) {
						but->flag &= ~UI_ACTIVE;
						if(but->type != LABEL && but->embossfunc != ui_emboss_N) ui_draw_but(but);
					}
					else if(but->type==BLOCK || but->type==MENU) {	// automatic opens block button (pulldown)
						int time;
						if(uevent->event!=LEFTMOUSE ) {
							if(block->auto_open) time= 5*U.menuthreshold2;
							else if(U.uiflag & MENUOPENAUTO) time= 5*U.menuthreshold1;
							else time= -1;
							
							for (; time>0; time--) {
								if (anyqtest()) break;
								else PIL_sleep_ms(20);
							}
							if(time==0) ui_do_button(block, but, uevent);
						}
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

	uiPanelPop(block); // pop matrix; no return without pop!

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
		if(inside==0 && block->parentrct.xmax != 0.0) {
			/* strict check, and include the parent rect */
			if( BLI_in_rctf(&block->parentrct, (float)uevent->mval[0], (float)uevent->mval[1]));
			else if( ui_mouse_motion_towards_block(block, uevent));
			else {
				float midx, midy;
				int safety;
				
				midx= (block->parentrct.xmin+block->parentrct.xmax)/2.0;
				midy= (block->parentrct.ymin+block->parentrct.ymax)/2.0;
				
				if( midx < block->minx ) safety = 3; else safety= 40;
				if(uevent->mval[0]<block->minx-safety) return UI_RETURN_OUT;
				
				if( midy < block->miny ) safety = 3; else safety= 40;
				if(uevent->mval[1]<block->miny-safety) return UI_RETURN_OUT;
				
				if( midx > block->maxx ) safety = 3; else safety= 40;
				if(uevent->mval[0]>block->maxx+safety) return UI_RETURN_OUT;
				
				if( midy > block->maxy ) safety = 3; else safety= 40;
				if(uevent->mval[1]>block->maxy+safety) return UI_RETURN_OUT;
			}
		}
		else {
			/* for popups without parent button */
			if(uevent->mval[0]<block->minx-40) return UI_RETURN_OUT;
			if(uevent->mval[1]<block->miny-40) return UI_RETURN_OUT;
	
			if(uevent->mval[0]>block->maxx+40) return UI_RETURN_OUT;
			if(uevent->mval[1]>block->maxy+40) return UI_RETURN_OUT;
		}
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
	BIF_DrawString(but->font, but->tip, (U.transopts & TR_TOOLTIPS), 0);
	
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
				PIL_sleep_ms(50);
		}
			
			/* Display the tip, and keep it displayed
			 * as long as the mouse remains on top
			 * of the button that owns it.
			 */
		uiPanelPush(UIbuttip->block); // panel matrix
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
		uiPanelPop(UIbuttip->block); // panel matrix
		UIbuttip= NULL;
	}
}

/* returns UI_NOTHING, if nothing happened */
int uiDoBlocks(ListBase *lb, int event)
{
	/* return when:  firstblock != BLOCK_LOOP
	 * 
	 * 'cont' is used to make sure you can press another button while a looping menu
	 * is active. otherwise you have to press twice...
	 */

	uiBlock *block, *first;
	uiEvent uevent;
	int retval= UI_NOTHING, cont= 1, dopop=0;

	if(lb->first==0) return UI_NOTHING;
	
	/* for every pixel both x and y events are generated, overloads the system! */
	if(event==MOUSEX) return UI_NOTHING;
		
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
					block->saveunder= ui_bgnpupdraw((int)block->minx-1, (int)block->miny-6, (int)block->maxx+6, (int)block->maxy+1, 1);
					block->frontbuf= UI_HAS_DRAW_FRONT;
				}
				uiDrawBlock(block);
				block->flag &= ~UI_BLOCK_REDRAW;
			}

			retval= ui_do_block(block, &uevent);
			if(retval==UI_EXIT_LOOP) break;
			
			/* now a new block could be created for menus, this is 
			   inserted in the beginning of a list */
			
			/* is there a glfinish cached? */
			if(block->frontbuf == UI_HAS_DRAW_FRONT) {
				glFinish();
				glDrawBuffer(GL_BACK);
				block->frontbuf= UI_NEED_DRAW_FRONT;
			}
			
			/* to make sure the matrix of the panel works for menus too */
			dopop= 1;
			if(retval==UI_CONT || (retval & UI_RETURN)) break;
			first= lb->first; if(first->flag & UI_BLOCK_LOOP) break;
			
			block= block->next;
		}
	
		/* second loop, for menus (looping blocks). works for sub->menus too */
		block= lb->first;
		if(block==NULL || (block->flag & UI_BLOCK_LOOP)==0) cont= 0;
		
		while( (block= lb->first) && (block->flag & UI_BLOCK_LOOP)) {
			block->auto_open= 1;
			
			/* this here, for menu buts */
			if(block->flag & UI_BLOCK_REDRAW) {

				if( block->flag & UI_BLOCK_LOOP) {
					block->saveunder= ui_bgnpupdraw((int)block->minx-1, (int)block->miny-6, (int)block->maxx+6, (int)block->maxy+1, 1);
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
		
		/* else it does the first part of this loop again, maybe another menu needs to be opened */
		if(retval==UI_CONT || (retval & UI_RETURN_OK)) cont= 0;
	}
	
	/* cleanup frontbuffer & flags */
	block= lb->first;
	while(block) {
		if(block->frontbuf==UI_HAS_DRAW_FRONT) glFinish();
		block->frontbuf= 0;
		block= block->next;
	}
	
	/* afterfunc is used for fileloading too, so after this call, the blocks pointers are invalid */
	if(retval & UI_RETURN_OK) {
		if(UIafterfunc) UIafterfunc(UIafterfunc_arg, UIafterval);
		UIafterfunc= NULL;
	}

	/* tooltip */	
	if(retval==UI_NOTHING && (uevent.event==MOUSEX || uevent.event==MOUSEY)) {
		if(U.flag & TOOLTIPS) ui_do_but_tip();
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

static char *ui_block_cut_str(uiBlock *block, char *str, short okwidth)
{
	short width, ofs=strlen(str);
	static char str1[128];
	
	if(ofs>127) return str;
	
	width= block->aspect*BIF_GetStringWidth(block->curfont, str, (U.transopts & TR_BUTTONS));

	if(width <= okwidth) return str;
	strcpy(str1, str);
	
	while(width > okwidth && ofs>0) {
		ofs--;
		str1[ofs]= 0;
		
		width= block->aspect*BIF_GetStringWidth(block->curfont, str1, 0);
		
		if(width < 10) break;
	}
	return str1;
}

static void ui_check_but(uiBut *but)
{
	/* if something changed in the button */
	ID *id;
	double value;
	float okwidth;
	short pos;
	
	ui_is_but_sel(but);
	
	
	if(but->type==NUMSLI || but->type==HSVSLI) 
		okwidth= -7 + (but->x2 - but->x1)/2.0;
	else 
		okwidth= -7 + (but->x2 - but->x1); 
	
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

	if(but->drawstr[0]) {
		but->strwidth= but->aspect*BIF_GetStringWidth(but->font, but->drawstr, (U.transopts & TR_BUTTONS));
		// here should be check for less space for icon offsets...
		if(but->type==MENU) okwidth -= 20;
	}
	else
		but->strwidth= 0;

		/* automatic width */
	if(but->x2==0.0) {
		but->x2= (but->x1+but->strwidth+6); 
	}

	if(but->strwidth==0) but->drawstr[0]= 0;
	else if(but->type==BUTM);	// clip string
	else {

		/* calc but->ofs, to draw the string shorter if too long */
		but->ofs= 0;
		while(but->strwidth > (int)okwidth ) {
			but->ofs++;
	
			if(but->drawstr[but->ofs]) 
				but->strwidth= but->aspect*BIF_GetStringWidth(but->font, but->drawstr+but->ofs, (U.transopts & TR_BUTTONS));
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
		
		/* fix for buttons that better not have text cut off to the right */
		if(but->ofs) {
			if ELEM(but->type, NUM, TEX);	// only these cut off left
			else {
				but->drawstr[ strlen(but->drawstr)-but->ofs ]= 0;
				but->ofs= 0;
			}
		}
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
	else if(block->dt==UI_EMBOSST) but->embossfunc= ui_emboss_TABL;
	else if(block->dt==UI_EMBOSSTABL) but->embossfunc= ui_emboss_TABL;
	else if(block->dt==UI_EMBOSSTABM) but->embossfunc= ui_emboss_TABM;
	else if(block->dt==UI_EMBOSSTABR) but->embossfunc= ui_emboss_TABR;
	else if(block->dt==UI_EMBOSSMB) but->embossfunc= ui_emboss_MENU;
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
void uiBlockFlipOrder(uiBlock *block)
{
	uiBut *but;
	float centy, miny=10000, maxy= -10000;

	for(but= block->buttons.first; but; but= but->next) {
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

void uiDefIDPoinBut(uiBlock *block, uiIDPoinFuncFP func, int retval, char *str, short x1, short y1, short x2, short y2, void *idpp, char *tip)
{
	uiBut *but= ui_def_but(block, IDPOIN, retval, str, x1, y1, x2, y2, NULL, 0.0, 0.0, 0.0, 0.0, tip);
	but->idpoin_func= func;
	but->idpoin_idpp= (ID**) idpp;
	ui_check_but(but);
}

uiBut *uiDefBlockBut(uiBlock *block, uiBlockFuncFP func, void *arg, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, BLOCK, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
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
	uiBlockSetCol(block, MENUCOL);
	
	md= decompose_menu_string(instr);

	/* size and location, title slightly bigger for bold */
	if(md->title) width= 2*strlen(md->title)+BIF_GetStringWidth(uiBlockGetCurFont(block), md->title, (U.transopts && TR_BUTTONS));
	else width= 0;
	for(a=0; a<md->nitems; a++) {
		xmax= BIF_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str, (U.transopts && TR_BUTTONS));
		if(xmax>width) width= xmax;
	}

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
		ui_warp_pointer(mval[0], mouseymove+mval[1]);
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
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT);
	uiBlockSetCol(block, MENUCOL);
	
	md= decompose_menu_string(instr);

	/* collumns and row calculation */
	columns= (md->nitems+maxrow)/maxrow;
	if (columns<1) columns= 1;
	
	rows= (int) md->nitems/columns;
	if (rows<1) rows= 1;
	
	while (rows*columns<md->nitems) rows++;
		
	/* size and location */
	if(md->title) width= 2*strlen(md->title)+BIF_GetStringWidth(uiBlockGetCurFont(block), md->title, (U.transopts & TR_BUTTONS));
	else width= 0;
	for(a=0; a<md->nitems; a++) {
		xmax= BIF_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str, (U.transopts & TR_BUTTONS));
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
	
	if((event & UI_RETURN_OUT)==0) ui_warp_pointer(mousemove[0], mousemove[1]);
	
	return val;	
}

/* ************** panels ************* */

static void copy_panel_offset(Panel *pa, Panel *papar)
{
	/* with respect to sizes... papar is parent */

	pa->ofsx= papar->ofsx;
	pa->ofsy= papar->ofsy + papar->sizey-pa->sizey;
}



/* ugly global... but will be NULLed after each 'newPanel' call */
static char *panel_tabbed=NULL, *group_tabbed=NULL;

void uiNewPanelTabbed(char *panelname, char *groupname)
{
	panel_tabbed= panelname;
	group_tabbed= groupname;
}

/* another global... */
static int pnl_style= UI_PNL_TRANSP;

void uiSetPanelStyle(int style)
{
	pnl_style= style;
}


/* ofsx/ofsy only used for new panel definitions */
/* return 1 if visible (create buttons!) */
int uiNewPanel(ScrArea *sa, uiBlock *block, char *panelname, char *tabname, int ofsx, int ofsy, int sizex, int sizey)
{
	Panel *pa, *palign;
	
	/* check if Panel exists, then use that one */
	pa= sa->panels.first;
	while(pa) {
		if( strncmp(pa->panelname, panelname, UI_MAX_NAME_STR)==0) {
			if( strncmp(pa->tabname, tabname, UI_MAX_NAME_STR)==0) {
				break;
			}
		}
		pa= pa->next;
	}
	
	if(pa==NULL) {
		
		/* new panel */
		pa= MEM_callocN(sizeof(Panel), "new panel");
		BLI_addtail(&sa->panels, pa);
		strncpy(pa->panelname, panelname, UI_MAX_NAME_STR);
		strncpy(pa->tabname, tabname, UI_MAX_NAME_STR);
	
		pa->ofsx= ofsx & ~(PNL_GRID-1);
		pa->ofsy= ofsy & ~(PNL_GRID-1);
		pa->sizex= sizex;
		pa->sizey= sizey;
		pa->style= pnl_style;
		
		/* pre align, for good sorting later on */
		if(sa->spacetype==SPACE_BUTS && pa->prev) {
			SpaceButs *sbuts= sa->spacedata.first;
			
			palign= pa->prev;
			if(sbuts->align==BUT_VERTICAL) {
				pa->ofsy= palign->ofsy - pa->sizey - PNL_HEADER;
			}
			else if(sbuts->align==BUT_HORIZONTAL) {
				pa->ofsx= palign->ofsx + palign->sizex;
			}
		}
		/* make new Panel tabbed? */
		if(panel_tabbed && group_tabbed) {
			Panel *papar;
			for(papar= sa->panels.first; papar; papar= papar->next) {
				if(papar->active && papar->paneltab==NULL) {
					if( strncmp(panel_tabbed, papar->panelname, UI_MAX_NAME_STR)==0) {
						if( strncmp(group_tabbed, papar->tabname, UI_MAX_NAME_STR)==0) {
							pa->paneltab= papar;
							copy_panel_offset(pa, papar);
							break;
						}
					}
				}
			} 
		}
	}
	
	block->panel= pa;
	pa->active= 1;

	/* clear global */
	panel_tabbed= group_tabbed= NULL;
	
	if(block->panel->paneltab) return 0;
	if(block->panel->flag & PNL_CLOSED) return 0;

	return 1;
}

void uiFreePanels(ListBase *lb)
{
	Panel *panel;
	
	while( (panel= lb->first) ) {
		BLI_remlink(lb, panel);
		MEM_freeN(panel);
	}
}

void uiNewPanelHeight(uiBlock *block, int sizey)
{
	if(sizey<64) sizey= 64;
	
	if(block->panel) {
		block->panel->ofsy+= (block->panel->sizey - sizey);
		block->panel->sizey= sizey;
	}
}

static int panel_has_tabs(Panel *panel)
{
	Panel *pa= curarea->panels.first;
	
	if(panel==NULL) return 0;
	
	while(pa) {
		if(pa->paneltab==panel) return 1;
		pa= pa->next;
	}
	return 0;
}

static void ui_scale_panel_block(uiBlock *block)
{
	uiBut *but;
	float facx= 1.0, facy= 1.0;
	int centrex= 0, topy=0, tabsy=0;
	
	if(block->panel==NULL) return;

	if(block->autofill) ui_autofill(block);
	/* buttons min/max centered, offset calculated */
	uiBoundsBlock(block, 0);

	if( block->maxx-block->minx > block->panel->sizex - 2*PNL_SAFETY ) {
		facx= (block->panel->sizex - (2*PNL_SAFETY))/( block->maxx-block->minx );
	}
	else centrex= (block->panel->sizex-( block->maxx-block->minx ) - PNL_SAFETY)/2;
	
	// tabsy= PNL_HEADER*panel_has_tabs(block->panel);
	if( (block->maxy-block->miny) > block->panel->sizey - 2*PNL_SAFETY - tabsy) {
		facy= (block->panel->sizey - (2*PNL_SAFETY) - tabsy)/( block->maxy-block->miny );
	}
	else topy= (block->panel->sizey- 2*PNL_SAFETY - tabsy) - ( block->maxy-block->miny ) ;

	but= block->buttons.first;
	while(but) {
		but->x1= PNL_SAFETY+centrex+ facx*(but->x1-block->minx);
		but->y1= PNL_SAFETY+topy   + facy*(but->y1-block->miny);
		but->x2= PNL_SAFETY+centrex+ facx*(but->x2-block->minx);
		but->y2= PNL_SAFETY+topy   + facy*(but->y2-block->miny);
		if(facx!=1.0) ui_check_but(but);	/* for strlen */
		but= but->next;
	}

	block->maxx= block->panel->sizex;
	block->maxy= block->panel->sizey;
	block->minx= block->miny= 0.0;
	
}

// for 'home' key
void uiSetPanel_view2d(ScrArea *sa)
{
	Panel *pa;
	float minx=10000, maxx= -10000, miny=10000, maxy= -10000;
	int done=0;
	
	pa= sa->panels.first;
	while(pa) {
		if(pa->active) {
			done= 1;
			if(pa->ofsx < minx) minx= pa->ofsx;
			if(pa->ofsx+pa->sizex > maxx) maxx= pa->ofsx+pa->sizex;
			if(pa->ofsy < miny) miny= pa->ofsy;
			if(pa->ofsy+pa->sizey+PNL_HEADER > maxy) maxy= pa->ofsy+pa->sizey+PNL_HEADER;
		}
		pa= pa->next;
	}
	if(done) {
		G.v2d->tot.xmin= minx-PNL_DIST;
		G.v2d->tot.xmax= maxx+PNL_DIST;
		G.v2d->tot.ymin= miny-PNL_DIST;
		G.v2d->tot.ymax= maxy+PNL_DIST;
	}
	else {
		G.v2d->tot.xmin= 0;
		G.v2d->tot.xmax= 1280;
		G.v2d->tot.ymin= 0;
		G.v2d->tot.ymax= 228;
	}
	
}

// make sure the panels are not outside 'tot' area
void uiMatchPanel_view2d(ScrArea *sa)
{
	Panel *pa;
	
	pa= sa->panels.first;
	while(pa) {
		if(pa->active) {
			if(pa->ofsx < G.v2d->tot.xmin) G.v2d->tot.xmin= pa->ofsx;
			if(pa->ofsx+pa->sizex > G.v2d->tot.xmax) 
				G.v2d->tot.xmax= pa->ofsx+pa->sizex;
			if(pa->ofsy < G.v2d->tot.ymin) G.v2d->tot.ymin= pa->ofsy;
			if(pa->ofsy+pa->sizey+PNL_HEADER > G.v2d->tot.ymax) 
				G.v2d->tot.ymax= pa->ofsy+pa->sizey+PNL_HEADER;
		}
		pa= pa->next;
	}	
}

/* extern used ny previewrender */
void uiPanelPush(uiBlock *block)
{
	glPushMatrix(); 
	if(block->panel) {
		glTranslatef((float)block->panel->ofsx, (float)block->panel->ofsy, 0.0);
		i_translate((float)block->panel->ofsx, (float)block->panel->ofsy, 0.0, UIwinmat);
	}
}

void uiPanelPop(uiBlock *block)
{
	glPopMatrix();
	Mat4CpyMat4(UIwinmat, block->winmat);
}

uiBlock *uiFindOpenPanelBlockName(ListBase *lb, char *name)
{
	uiBlock *block;
	
	for(block= lb->first; block; block= block->next) {
		if(block->panel && block->panel->active && block->panel->paneltab==NULL) {
			if(block->panel->flag & PNL_CLOSED);
			else if(strncmp(name, block->panel->panelname, UI_MAX_NAME_STR)==0) break;
		}
	}
	return block;
}

static void ui_draw_anti_tria(float x1, float y1, float x2, float y2, float x3, float y3)
{

	// we draw twice, anti polygons not widely supported...

	glBegin(GL_POLYGON);
	glVertex2f(x1, y1);
	glVertex2f(x2, y2);
	glVertex2f(x3, y3);
	glEnd();

	/* set antialias line */
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glBegin(GL_LINE_LOOP);
	glVertex2f(x1, y1);
	glVertex2f(x2, y2);
	glVertex2f(x3, y3);
	glEnd();
	
	glDisable( GL_LINE_SMOOTH );
	glDisable( GL_BLEND );
	
}

/* 'icon' for panel header */
static void ui_draw_tria_icon(float x, float y, float aspect, char dir)
{

	
	glColor3ub(240, 240, 240);
	
	if(dir=='h') {
		ui_draw_anti_tria( x, y, x, y+12.0, x+10, y+6);
	}
	else {
		ui_draw_anti_tria( x, y+10.0,  x+12, y+10.0, x+6, y);	
	}
	
	
}

static void ui_set_panel_pattern(char dir)
{
	static int firsttime= 1;
	static GLubyte path[4*32], patv[4*32];
	int a,b,i=0;

	if(firsttime) {
		firsttime= 0;
		for(a=0; a<128; a++) patv[a]= 0x33;
		for(a=0; a<8; a++) {
			for(b=0; b<4; b++) path[i++]= 0xff;	/* 1 scanlines */
			for(b=0; b<12; b++) path[i++]= 0x0;	/* 3 lines */
		}
	}
	glEnable(GL_POLYGON_STIPPLE);
	if(dir=='h') glPolygonStipple(path);	
	else glPolygonStipple(patv);	
}

#define PNL_ICON 	20
#define PNL_DRAGGER	20


static void ui_draw_panel_header(uiBlock *block)
{
	Panel *pa, *panel= block->panel;
	float width;
	int a, nr= 1;
	char *str;
	
	/* count */
	pa= curarea->panels.first;
	while(pa) {
		if(pa->active) {
			if(pa->paneltab==panel) nr++;
		}
		pa= pa->next;
	}
	
	if(nr==1) {
		glColor3ub(255,255,255);
		glRasterPos2f(block->minx+40, block->maxy+5);
		BIF_DrawString(block->curfont, block->panel->panelname, (U.transopts & TR_BUTTONS), 0);
		return;
	}
	
	a= 0;
	width= (panel->sizex - 3 - 2*PNL_ICON)/nr;
	pa= curarea->panels.first;
	while(pa) {
		if(pa->active==0);
		else if(pa==panel) {
			/* active tab */
			uiSetRoundBox(15);
			glColor3ub(140, 140, 147);
			uiRoundBox(2+PNL_ICON+a*width, panel->sizey+3, PNL_ICON+(a+1)*width, panel->sizey+PNL_HEADER-3, 8);

			glColor3ub(255,255,255);
			glRasterPos2f(10+PNL_ICON+a*width, panel->sizey+5);
			str= ui_block_cut_str(block, pa->panelname, (short)(width-10));
			BIF_DrawString(block->curfont, str, (U.transopts & TR_BUTTONS), 0);

			a++;
		}
		else if(pa->paneltab==panel) {
			/* not active tab */
			
			glColor3ub(95,95,95);
			glRasterPos2f(10+PNL_ICON+a*width, panel->sizey+5);
			str= ui_block_cut_str(block, pa->panelname, (short)(width-10));
			BIF_DrawString(block->curfont, str, (U.transopts & TR_BUTTONS), 0);
			
			a++;
		}
		pa= pa->next;
	}
	
	// dragger
	uiSetRoundBox(15);
	glColor3ub(140, 140, 147);
	uiRoundBox(panel->sizex-PNL_ICON+5, panel->sizey+5, panel->sizex-5, panel->sizey+PNL_HEADER-5, 5);
	
}

static void ui_draw_panel(uiBlock *block)
{
	if(block->panel->paneltab) return;
	
	if(block->panel->flag & PNL_CLOSEDY) {
		uiSetRoundBox(15);
		glColor3ub(160, 160, 167);
		uiRoundBox(block->minx, block->maxy, block->maxx, block->maxy+PNL_HEADER, 10);
		
		// title
		glColor3ub(255,255,255);
		glRasterPos2f(block->minx+40, block->maxy+5);
		BIF_DrawString(block->curfont, block->panel->panelname, (U.transopts & TR_BUTTONS), 0);

		//  border
		if(block->panel->flag & PNL_SELECT) {
			glColor3ub(64, 64, 64);
			uiRoundRect(block->minx, block->maxy, block->maxx, block->maxy+PNL_HEADER, 10);
		}
		if(block->panel->flag & PNL_OVERLAP) {
			glColor3ub(240, 240, 240);
			uiRoundRect(block->minx, block->maxy, block->maxx, block->maxy+PNL_HEADER, 10);
		}
	
	}
	else if(block->panel->flag & PNL_CLOSEDX) {
		char str[4];
		int a, end, ofs;
		
		uiSetRoundBox(15);
		glColor3ub(160, 160, 167);
		uiRoundBox(block->minx, block->miny, block->minx+PNL_HEADER, block->maxy+PNL_HEADER, 10);
	
		// title, only capitals for now
		glColor3ub(255,255,255);
		str[1]= 0;
		end= strlen(block->panel->panelname);
		ofs= 20;
		for(a=0; a<end; a++) {
			str[0]= block->panel->panelname[a];
			if( isupper(str[0]) ) {
				glRasterPos2f(block->minx+5, block->maxy-ofs);
				BIF_DrawString(block->curfont, str, 0, 0);
				ofs+= 15;
			}
		}
		
		//  border
		if(block->panel->flag & PNL_SELECT) {
			glColor3ub(64, 64, 64);
			uiRoundRect(block->minx, block->miny, block->minx+PNL_HEADER, block->maxy+PNL_HEADER, 10);
		}
		if(block->panel->flag & PNL_OVERLAP) {
			glColor3ub(240, 240, 240);
			uiRoundRect(block->minx, block->miny, block->minx+PNL_HEADER, block->maxy+PNL_HEADER, 10);
		}
	
	}
	else {
		
		uiSetRoundBox(3);

		if(block->panel->style== UI_PNL_SOLID) {
			glColor3ub(160, 160, 167);
			uiRoundBox(block->minx, block->maxy, block->maxx, block->maxy+PNL_HEADER, 10);
			// blend now for panels in 3d window, test...
			glEnable(GL_BLEND);
			glColor4ub(198, 198, 198, 100);
			glRectf(block->minx, block->miny, block->maxx, block->maxy);

			if(G.buts->align) {
				glColor4ub(206, 206, 206, 100);
				if(G.buts->align==BUT_HORIZONTAL) ui_set_panel_pattern('h');
				else ui_set_panel_pattern('v');
	
				glRectf(block->minx, block->miny, block->maxx, block->maxy);
				glDisable(GL_POLYGON_STIPPLE);
			}
			glDisable(GL_BLEND);
		}
		else {
			glColor3ub(218, 218, 218);
			uiRoundRect(block->minx, block->miny, block->maxx, block->maxy+PNL_HEADER, 10);
		}
		
		
		ui_draw_panel_header(block);

		//  border
		uiSetRoundBox(3);
		if(block->panel->flag & PNL_SELECT) {
			glColor3ub(64, 64, 64);
			uiRoundRect(block->minx, block->miny, block->maxx, block->maxy+PNL_HEADER, 10);
		}
		if(block->panel->flag & PNL_OVERLAP) {
			glColor3ub(240, 240, 240);
			uiRoundRect(block->minx, block->miny, block->maxx, block->maxy+PNL_HEADER, 10);
		}
		
		/* and a soft shadow-line for now */
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glColor4ub(0, 0, 0, 50);
		fdrawline(block->maxx, block->miny, block->maxx, block->maxy+PNL_HEADER/2);
		fdrawline(block->minx, block->miny, block->maxx, block->miny);
		glDisable(GL_BLEND);

	}

	/* draw close icon */

	if(block->panel->flag & PNL_CLOSEDY)
		ui_draw_tria_icon(block->minx+6, block->maxy+3, block->aspect, 'h');
	else if(block->panel->flag & PNL_CLOSEDX)
		ui_draw_tria_icon(block->minx+4, block->maxy+2, block->aspect, 'h');
	else
		ui_draw_tria_icon(block->minx+6, block->maxy+3, block->aspect, 'v');


}

static void ui_redraw_select_panel(ScrArea *sa)
{
	/* only for beauty, make sure the panel thats moved is on top */
	/* better solution later? */
	uiBlock *block;
	
	for(block= sa->uiblocks.first; block; block= block->next) {
		if(block->panel && (block->panel->flag & PNL_SELECT)) {
			uiDrawBlock(block);
		}
	}

}


/* ------------ panel alignment ---------------- */


/* this function is needed because uiBlock and Panel itself dont
change sizey or location when closed */
static int get_panel_real_ofsy(Panel *pa)
{
	if(pa->flag & PNL_CLOSEDY) return pa->ofsy+pa->sizey;
	else if(pa->paneltab && (pa->paneltab->flag & PNL_CLOSEDY)) return pa->ofsy+pa->sizey;
	else return pa->ofsy;
}

static int get_panel_real_ofsx(Panel *pa)
{
	if(pa->flag & PNL_CLOSEDX) return pa->ofsx+PNL_HEADER;
	else if(pa->paneltab && (pa->paneltab->flag & PNL_CLOSEDX)) return pa->ofsx+PNL_HEADER;
	else return pa->ofsx+pa->sizex;
}


typedef struct PanelSort {
	Panel *pa, *orig;
} PanelSort;

static int find_leftmost_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1=a1, *ps2=a2;
	
	if( ps1->pa->ofsx > ps2->pa->ofsx) return 1;
	else if( ps1->pa->ofsx < ps2->pa->ofsx) return -1;

	return 0;
}


static int find_highest_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1=a1, *ps2=a2;
	
	if( ps1->pa->ofsy < ps2->pa->ofsy) return 1;
	else if( ps1->pa->ofsy > ps2->pa->ofsy) return -1;
	
	return 0;
}

/* this doesnt draw */
/* returns 1 when it did something */
int uiAlignPanelStep(ScrArea *sa, float fac)
{
	SpaceButs *sbuts= sa->spacedata.first;
	Panel *pa;
	PanelSort *ps, *panelsort, *psnext;
	int a, tot=0, done;
	
	if(sa->spacetype!=SPACE_BUTS) {
		return 0;
	}
	
	/* count active, not tabbed Panels */
	for(pa= sa->panels.first; pa; pa= pa->next) {
		if(pa->active && pa->paneltab==NULL) tot++;
	}

	if(tot==0) return 0;

	/* extra; change close direction? */
	for(pa= sa->panels.first; pa; pa= pa->next) {
		if(pa->active && pa->paneltab==NULL) {
			if( (pa->flag & PNL_CLOSEDX) && (sbuts->align==BUT_VERTICAL) )
				pa->flag ^= PNL_CLOSED;
			
			else if( (pa->flag & PNL_CLOSEDY) && (sbuts->align==BUT_HORIZONTAL) )
				pa->flag ^= PNL_CLOSED;
			
		}
	}

	panelsort= MEM_callocN( tot*sizeof(PanelSort), "panelsort");
	
	/* fill panelsort array */
	ps= panelsort;
	for(pa= sa->panels.first; pa; pa= pa->next) {
		if(pa->active && pa->paneltab==NULL) {
			ps->pa= MEM_dupallocN(pa);
			ps->orig= pa;
			ps++;
		}
	}
	
	if(sbuts->align==BUT_VERTICAL) 
		qsort(panelsort, tot, sizeof(PanelSort), find_highest_panel);
	else
		qsort(panelsort, tot, sizeof(PanelSort), find_leftmost_panel);

	
	/* no smart other default start loc! this keeps switching f5/f6/etc compatible */
	ps= panelsort;
	ps->pa->ofsx= 0;
	ps->pa->ofsy= 0;
	
	for(a=0 ; a<tot-1; a++, ps++) {
		psnext= ps+1;
		
		if(sbuts->align==BUT_VERTICAL) {
			psnext->pa->ofsx = ps->pa->ofsx;
			psnext->pa->ofsy = get_panel_real_ofsy(ps->pa) - psnext->pa->sizey-PNL_HEADER-PNL_DIST;
		}
		else {
			psnext->pa->ofsx = get_panel_real_ofsx(ps->pa)+PNL_DIST;
			psnext->pa->ofsy = ps->pa->ofsy;
		}
	}
	
	/* we interpolate */
	done= 0;
	ps= panelsort;
	for(a=0; a<tot; a++, ps++) {
		if( (ps->pa->flag & PNL_SELECT)==0) {
			if( (ps->orig->ofsx != ps->pa->ofsx) || (ps->orig->ofsy != ps->pa->ofsy)) {
				ps->orig->ofsx= floor(0.5 + fac*ps->pa->ofsx + (1.0-fac)*ps->orig->ofsx);
				ps->orig->ofsy= floor(0.5 + fac*ps->pa->ofsy + (1.0-fac)*ps->orig->ofsy);
				done= 1;
			}
		}
	}

	/* copy locations to tabs */
	for(pa= sa->panels.first; pa; pa= pa->next) {
		if(pa->paneltab && pa->active) {
			copy_panel_offset(pa, pa->paneltab);
		}
	}

	/* free panelsort array */
	ps= panelsort;
	for(a=0; a<tot; a++, ps++) {
		MEM_freeN(ps->pa);
	}
	MEM_freeN(panelsort);
	
	return done;
}


static void ui_animate_panels(ScrArea *sa)
{
	double time=0, ltime;
	float result= 0.0, fac= 0.2;
	
	ltime = PIL_check_seconds_timer();

	/* for max 1 second, interpolate positions */
	while(TRUE) {
	
		if( uiAlignPanelStep(sa, fac) ) {
			/* warn: this re-allocs uiblocks! */
			scrarea_do_windraw(curarea);
			ui_redraw_select_panel(curarea);
			screen_swapbuffers();
		}
		else {
			addqueue(curarea->win, REDRAW,1 );	// because 'Animate' is also called as redraw
			break;
		}
		
		if(result >= 1.0) break;
		
		if(result==0.0) { // firsttime
			time = PIL_check_seconds_timer()-ltime;
			if(time > 0.5) fac= 0.7;
			else if(time > 0.2) fac= 0.5;
			else if(time > 0.1) fac= 0.4;	
			else if(time > 0.05) fac= 0.3; // 11 steps
		}
		
		result= fac + (1.0-fac)*result;
		
		if(result > 0.98) {
			result= 1.0;
			fac= 1.0;
		}
	}
}

/* only draws blocks with panels */
void uiDrawBlocksPanels(ScrArea *sa, int re_align)
{
	uiBlock *block;
	Panel *panot, *panew, *patest;
	
	/* scaling contents */
	block= sa->uiblocks.first;
	while(block) {
		if(block->panel) ui_scale_panel_block(block);
		block= block->next;
	}

	/* consistancy; are panels not made, whilst they have tabs */
	for(panot= sa->panels.first; panot; panot= panot->next) {
		if(panot->active==0) { // not made

			for(panew= sa->panels.first; panew; panew= panew->next) {
				if(panew->active) {
					if(panew->paneltab==panot) { // panew is tab in notmade pa
						break;
					}
				}
			}
			/* now panew can become the new parent, check all other tabs */
			if(panew) {
				for(patest= sa->panels.first; patest; patest= patest->next) {
					if(patest->paneltab == panot) {
						patest->paneltab= panew;
					}
				}
				panot->paneltab= panew;
				panew->paneltab= NULL;
				addqueue(sa->win, REDRAW, 1);	// the buttons panew were not made
			}
		}	
	}

	/* re-align */
	if(re_align) uiAlignPanelStep(sa, 1.0);

	/* draw */
	block= sa->uiblocks.first;
	while(block) {
		if(block->panel) uiDrawBlock(block);
		block= block->next;
	}

}



/* ------------ panel merging ---------------- */

static void check_panel_overlap(ScrArea *sa, Panel *panel)
{
	Panel *pa= sa->panels.first;

	/* also called with panel==NULL for clear */
	
	while(pa) {
		pa->flag &= ~PNL_OVERLAP;
		if(panel && (pa != panel)) {
			if(pa->paneltab==NULL && pa->active) {
				float safex= 0.2, safey= 0.2;
				
				if( pa->flag & PNL_CLOSEDX) safex= 0.05;
				else if(pa->flag & PNL_CLOSEDY) safey= 0.05;
				else if( panel->flag & PNL_CLOSEDX) safex= 0.05;
				else if(panel->flag & PNL_CLOSEDY) safey= 0.05;
				
				if( pa->ofsx > panel->ofsx- safex*panel->sizex)
				if( pa->ofsx+pa->sizex < panel->ofsx+ (1.0+safex)*panel->sizex)
				if( pa->ofsy > panel->ofsy- safey*panel->sizey)
				if( pa->ofsy+pa->sizey < panel->ofsy+ (1.0+safey)*panel->sizey)
					pa->flag |= PNL_OVERLAP;
			}
		}
		
		pa= pa->next;
	}
}

static void test_add_new_tabs(ScrArea *sa)
{
	Panel *pa, *pasel=NULL, *palap=NULL;
	/* search selected and overlapped panel */
	
	pa= sa->panels.first;
	while(pa) {
		if(pa->active) {
			if(pa->flag & PNL_SELECT) pasel= pa;
			if(pa->flag & PNL_OVERLAP) palap= pa;
		}
		pa= pa->next;
	}
	
	if(pasel && palap==NULL) {

		/* copy locations */
		pa= sa->panels.first;
		while(pa) {
			if(pa->paneltab==pasel) {
				copy_panel_offset(pa, pasel);
			}
			pa= pa->next;
		}
	}
	
	if(pasel==NULL || palap==NULL) return;
	
	/* the overlapped panel becomes a tab */
	palap->paneltab= pasel;
	
	/* the selected panel gets coords of overlapped one */
	copy_panel_offset(pasel, palap);

	/* and its tabs */
	pa= sa->panels.first;
	while(pa) {
		if(pa->paneltab == pasel) {
			copy_panel_offset(pa, palap);
		}
		pa= pa->next;
	}
	
	/* but, the overlapped panel already can have tabs too! */
	pa= sa->panels.first;
	while(pa) {
		if(pa->paneltab == palap) {
			pa->paneltab = pasel;
		}
		pa= pa->next;
	}
}

/* ------------ panel drag ---------------- */


static void ui_drag_panel(uiBlock *block)
{
	Panel *panel= block->panel;
	short align=0, first=1, ofsx, ofsy, dx=0, dy=0, dxo=0, dyo=0, mval[2], mvalo[2];

	if(curarea->spacetype==SPACE_BUTS) {
		SpaceButs *sbuts= curarea->spacedata.first;
		align= sbuts->align;
	}

	uiGetMouse(block->win, mvalo);
	ofsx= block->panel->ofsx;
	ofsy= block->panel->ofsy;

	panel->flag |= PNL_SELECT;
	
	while(TRUE) {
	
		if( !(get_mbut() & L_MOUSE) ) break;	
	
		/* first clip for window, no dragging outside */
		getmouseco_areawin(mval);
		if( mval[0]>0 && mval[0]<curarea->winx && mval[1]>0 && mval[1]<curarea->winy) {
			uiGetMouse(mywinget(), mval);
			dx= (mval[0]-mvalo[0]) & ~(PNL_GRID-1);
			dy= (mval[1]-mvalo[1]) & ~(PNL_GRID-1);
		}
		
		if(dx!=dxo || dy!=dyo || first || align) {
			dxo= dx; dyo= dy;		
			first= 0;
			
			panel->ofsx = ofsx+dx;
			panel->ofsy = ofsy+dy;
			
			check_panel_overlap(curarea, panel);
			
			if(align) uiAlignPanelStep(curarea, 0.2);

			/* warn: this re-allocs blocks! */
			scrarea_do_windraw(curarea);
			ui_redraw_select_panel(curarea);
			screen_swapbuffers();
			
			/* so, we find the new block */
			block= curarea->uiblocks.first;
			while(block) {
				if(block->panel == panel) break;
				block= block->next;
			}
			// temporal debug
			if(block==NULL) {
				printf("block null while panel drag, should not happen\n");
			}
			
			/* restore */
			Mat4CpyMat4(UIwinmat, block->winmat);
			
			/* idle for align */
			if(dx==dxo && dy==dyo) PIL_sleep_ms(30);
		}
		/* idle for this poor code */
		else PIL_sleep_ms(30);
	}

	test_add_new_tabs(curarea);	 // also copies locations of tabs in dragged panel

	panel->flag &= ~PNL_SELECT;
	check_panel_overlap(curarea, NULL);	// clears
	
	if(align==0) addqueue(block->win, REDRAW, 1);
	else ui_animate_panels(curarea);
}


static void ui_panel_untab(uiBlock *block)
{
	Panel *panel= block->panel, *pa, *panew=NULL;
	short nr, mval[2], mvalo[2];
	
	/* while hold mouse, check for movement, then untab */
	
	uiGetMouse(block->win, mvalo);
	while(TRUE) {
	
		if( !(get_mbut() & L_MOUSE) ) break;	
		uiGetMouse(mywinget(), mval);
		
		if( abs(mval[0]-mvalo[0]) + abs(mval[1]-mvalo[1]) > 6 ) {
			/* find new parent panel */
			nr= 0;
			pa= curarea->panels.first;
			while(pa) {
				if(pa->paneltab==panel) {
					panew= pa;
					nr++;
				}
				pa= pa->next;
			}
			
			/* make old tabs point to panew */
			if(panew==NULL) printf("panel untab: shouldnt happen\n");
			panew->paneltab= NULL;
			
			pa= curarea->panels.first;
			while(pa) {
				if(pa->paneltab==panel) {
					pa->paneltab= panew;
				}
				pa= pa->next;
			}
			
			ui_drag_panel(block);
			break;
		
		}
		/* idle for this poor code */
		else PIL_sleep_ms(50);
		
	}

}

/* ------------ panel events ---------------- */


static void panel_clicked_tabs(uiBlock *block,  int mousex)
{
	Panel *pa, *tabsel=NULL, *panel= block->panel;
	int nr= 1, a, width;
	
	/* count */
	pa= curarea->panels.first;
	while(pa) {
		if(pa!=panel) {
			if(pa->paneltab==panel) nr++;
		}
		pa= pa->next;
	}

	if(nr==1) return;
	
	/* find clicked tab, mouse in panel coords */
	a= 0;
	width= (panel->sizex - 3- 2*PNL_ICON)/nr;
	pa= curarea->panels.first;
	while(pa) {
		if(pa==panel || pa->paneltab==panel) {
			if( (mousex > PNL_ICON+a*width) && (mousex < PNL_ICON+(a+1)*width) ) {
				tabsel= pa;
			}
			a++;
		}
		pa= pa->next;
	}

	if(tabsel) {
		
		if(tabsel == panel) {
			ui_panel_untab(block);
		}
		else {
			/* tabsel now becomes parent for all others */
			panel->paneltab= tabsel;
			tabsel->paneltab= NULL;
			
			pa= curarea->panels.first;
			while(pa) {
				if(pa->paneltab == panel) pa->paneltab = tabsel;
				pa= pa->next;
			}
			
			addqueue(curarea->win, REDRAW, 1);
		}
	}
	
}


/* this function is supposed to call general window drawing too */
/* also it supposes a block has panel, and isnt a menu */
static void ui_do_panel(uiBlock *block, uiEvent *uevent)
{
	Panel *pa;
	int align= 0;
	
	if(curarea->spacetype==SPACE_BUTS) {
		SpaceButs *sbuts= curarea->spacedata.first;
		align= sbuts->align;
	}

	/* mouse coordinates in panel space! */

	if(uevent->event==LEFTMOUSE && block->panel->paneltab==NULL) {
		int button= 0;
		
		/* check open/closed button */
		if(block->panel->flag & PNL_CLOSEDX) {
			if(uevent->mval[1] >= block->maxy) button= 1;
		}
		else if(uevent->mval[0] <= block->minx+PNL_ICON+3) button= 1;
		
		if(button) {
			if(block->panel->flag & PNL_CLOSED) block->panel->flag &= ~PNL_CLOSED;
			else if(align==BUT_HORIZONTAL) block->panel->flag |= PNL_CLOSEDX;
			else block->panel->flag |= PNL_CLOSEDY;
			
			for(pa= curarea->panels.first; pa; pa= pa->next) {
				if(pa->paneltab==block->panel) {
					if(block->panel->flag & PNL_CLOSED) pa->flag |= PNL_CLOSED;
					else pa->flag &= ~PNL_CLOSED;
				}
			}
			if(align==0) addqueue(block->win, REDRAW, 1);
			else ui_animate_panels(curarea);
			
		}
		else if(block->panel->flag & PNL_CLOSED) {
			ui_drag_panel(block);
		}
		/* check if clicked in tabbed area */
		else if(uevent->mval[0] < block->maxx-PNL_ICON-3 && panel_has_tabs(block->panel)) {
			panel_clicked_tabs(block, uevent->mval[0]);
		}
		else {
			ui_drag_panel(block);
		}
	}
}


