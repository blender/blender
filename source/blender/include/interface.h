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

#ifndef INTERFACE_H
#define INTERFACE_H

#include "BIF_resources.h"

/* general defines */

#define UI_MAX_DRAW_STR	400
#define UI_MAX_NAME_STR	64
#define UI_ARRAY	29



/* uiBut->flag */
#define UI_SELECT		1
#define UI_MOUSE_OVER	2
#define UI_ACTIVE		4
#define UI_HAS_ICON		8
/* warn: rest of uiBut->flag in BIF_interface.c */


/* block->frontbuf: (only internal here), to nice localize the old global var uiFrontBuf */
#define UI_NEED_DRAW_FRONT 		1
#define UI_HAS_DRAW_FRONT 		2


/* internal panel drawing defines */
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
	short type, pointype, bit, bitnr, retval, strwidth, ofs, pos;
	int flag;
	
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

	void (*embossfunc)(int , int , float, float, float, float, float, int);
	void (*sliderfunc)(int , float, float, float, float, float, float, int);

	uiLink *link;
	
	char *tip, *lockstr;

	int themecol;	/* themecolor id */
	void *font;

	BIFIconID icon;
	short but_align;	/* aligning buttons, horiz/vertical */
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

	int themecol;	/* themecolor id */
	
	short font;	/* indices */
	int afterval, flag;
	void *curfont;
	
	short autofill, win, winq, direction, dt, frontbuf, auto_open;  //frontbuf see below
	void *saveunder;
	
	float xofs, yofs;  	// offset to parent button
	rctf parentrct;		// for pulldowns, rect the mouse is allowed outside of menu (parent button)
	rctf safety;		// pulldowns, to detect outside, can differ per case how it is created
	
	int handler;		// for panels in other windows than buttonswin... just event code
};

/* interface.c */
extern void ui_check_but(uiBut *but);
extern double ui_get_but_val(uiBut *but);
extern void ui_autofill(uiBlock *block);
extern void ui_graphics_to_window(int win, float *x, float *y);
extern void ui_window_to_graphics(int win, float *x, float *y);

/* interface_panel.c */
extern void ui_draw_panel(uiBlock *block);
extern void ui_do_panel(uiBlock *block, uiEvent *uevent);
extern void gl_round_box(float minx, float miny, float maxx, float maxy, float rad);

/* interface_draw.c */
extern void ui_set_embossfunc(uiBut *but, int drawtype);
extern void ui_draw_but(uiBut *but);


#endif

