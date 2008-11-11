/**
 * $Id: interface.h 14444 2008-04-16 22:40:48Z hos $
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

#ifndef INTERFACE_H
#define INTERFACE_H

#include "UI_resources.h"
#include "RNA_types.h"

struct uiActivateBut;
struct wmWindow;
struct ARegion;

/* general defines */

#define UI_MAX_DRAW_STR	400
#define UI_MAX_NAME_STR	64
#define UI_ARRAY	29

/* panel limits */
#define UI_PANEL_MINX	100
#define UI_PANEL_MINY	70

/* uiBut->flag */
#define UI_SELECT		1
#define UI_MOUSE_OVER	2
#define UI_ACTIVE		4
#define UI_HAS_ICON		8
/* warn: rest of uiBut->flag in BIF_interface.c */

/* uiBut->activateflag */
#define UI_ACTIVATE					1
#define UI_ACTIVATE_APPLY			2
#define UI_ACTIVATE_TEXT_EDITING	4
#define UI_ACTIVATE_OPEN			8

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

/* Button text selection:
 * extension direction, selextend, inside ui_do_but_TEX */
#define EXTEND_LEFT		1
#define EXTEND_RIGHT	2

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

typedef struct uiLinkLine {				/* only for draw/edit */
	struct uiLinkLine *next, *prev;

	short flag, pad;
	
	struct uiBut *from, *to;	
} uiLinkLine;

typedef struct {
	void **poin;		/* pointer to original pointer */
	void ***ppoin;		/* pointer to original pointer-array */
	short *totlink;		/* if pointer-array, here is the total */
	
	short maxlink, pad;
	short fromcode, tocode;
	
	ListBase lines;
} uiLink;

struct uiBut {
	struct uiBut *next, *prev;
	short type, pointype, bit, bitnr, retval, strwidth, ofs, pos, selsta, selend;
	int flag;
	
	char *str;
	char strdata[UI_MAX_NAME_STR];
	char drawstr[UI_MAX_DRAW_STR];
	
	float x1, y1, x2, y2;

	char *poin;
	float min, max;
	float a1, a2, hsv[3];	// hsv is temp memory for hsv buttons
	float aspect;

	void (*func)(void *, void *);
	void (*func3)(void *, void *, void *); /* XXX remove */
	void *func_arg1;
	void *func_arg2;
	void *func_arg3; /* XXX remove */

	void (*embossfunc)(int , int , float, float, float, float, float, int);
	void (*sliderfunc)(int , float, float, float, float, float, float, int);

	void (*autocomplete_func)(char *, void *);
	void *autofunc_arg;
	
	uiLink *link;
	
	char *tip, *lockstr;

	int themecol;	/* themecolor id */
	void *font;

	BIFIconID icon;
	short but_align;	/* aligning buttons, horiz/vertical */
	short lock, win;
	short iconadd, dt;

		/* IDPOIN data */
	uiIDPoinFuncFP idpoin_func;
	ID **idpoin_idpp;

		/* BLOCK data */
	uiBlockFuncFP block_func;

		/* BUTM data */
	void (*butm_func)(void *arg, int event);
	void *butm_func_arg;

		/* RNA */
	struct PointerRNA rnapoin;
	struct PropertyRNA *rnaprop;
	int rnaindex;

		/* Activation data */
	struct uiActivateBut *activate;
	int activateflag;

	char *editstr;
	double *editval;
	float *editvec;
	void *editcoba;
	void *editcumap;
	
		/* pointer back */
	uiBlock *block;
};

struct uiBlock {
	uiBlock *next, *prev;
	
	ListBase buttons;
	Panel *panel;
	uiBlock *oldblock;
	
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
	
	short autofill, win, winq, direction, dt;
	short auto_open, in_use, pad;
	double auto_open_last;

	int lock;
	char *lockstr;
	
	float xofs, yofs;			// offset to parent button
	rctf safety;				// pulldowns, to detect outside, can differ per case how it is created
	ListBase saferct;			// uiSafetyRct list
	uiMenuBlockHandle *handle;	// handle
};

typedef struct uiSafetyRct {
	struct uiSafetyRct *next, *prev;
	rctf parent;
	rctf safety;
} uiSafetyRct;

/* interface.c */

extern int ui_translate_buttons(void);
extern int ui_translate_menus(void);
extern int ui_translate_tooltips(void);

extern void ui_block_to_window_fl(const struct ARegion *ar, uiBlock *block, float *x, float *y);
extern void ui_block_to_window(const struct ARegion *ar, uiBlock *block, int *x, int *y);
extern void ui_block_to_window_rct(const struct ARegion *ar, uiBlock *block, rctf *graph, rcti *winr);
extern void ui_window_to_block_fl(const struct ARegion *ar, uiBlock *block, float *x, float *y);
extern void ui_window_to_block(const struct ARegion *ar, uiBlock *block, int *x, int *y);

extern double ui_get_but_val(uiBut *but);
extern void ui_set_but_val(uiBut *but, double value);
extern void ui_set_but_hsv(uiBut *but);
extern void ui_get_but_vectorf(uiBut *but, float *vec);
extern void ui_set_but_vectorf(uiBut *but, float *vec);
extern void ui_get_but_string(uiBut *but, char *str, int maxlen);
extern void ui_set_but_string(uiBut *but, const char *str);

extern void ui_check_but(uiBut *but);
extern void ui_autofill(uiBlock *block);
extern int  ui_is_but_float(uiBut *but);
extern void ui_update_block_buts_hsv(uiBlock *block, float *hsv);

/* interface_regions.c */
uiBlock *ui_block_func_MENU(struct wmWindow *window, uiMenuBlockHandle *handle, void *arg_but);
uiBlock *ui_block_func_ICONROW(struct wmWindow *window, uiMenuBlockHandle *handle, void *arg_but);
uiBlock *ui_block_func_ICONTEXTROW(struct wmWindow *window, uiMenuBlockHandle *handle, void *arg_but);
uiBlock *ui_block_func_COL(struct wmWindow *window, uiMenuBlockHandle *handle, void *arg_but);

struct ARegion *ui_tooltip_create(struct bContext *C, struct ARegion *butregion, uiBut *but);
void ui_tooltip_free(struct bContext *C, struct ARegion *ar);

uiMenuBlockHandle *ui_menu_block_create(struct bContext *C, struct ARegion *butregion, uiBut *but,
	uiBlockFuncFP block_func, void *arg);
void ui_menu_block_free(struct bContext *C, uiMenuBlockHandle *handle);

void ui_set_name_menu(uiBut *but, int value);

/* interface_panel.c */
extern void ui_draw_panel(uiBlock *block);
extern void ui_do_panel(uiBlock *block, uiEvent *uevent);
extern void ui_scale_panel(uiBlock *block);
extern void gl_round_box(int mode, float minx, float miny, float maxx, float maxy, float rad);
extern void gl_round_box_shade(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadetop, float shadedown);

/* interface_draw.c */
extern void ui_set_embossfunc(uiBut *but, int drawtype);
extern void ui_draw_but(uiBut *but);
extern void ui_rasterpos_safe(float x, float y, float aspect);
extern void ui_draw_tria_icon(float x, float y, float aspect, char dir);
extern void ui_draw_anti_x(float x1, float y1, float x2, float y2);
extern void ui_dropshadow(rctf *rct, float radius, float aspect, int select);

#endif

