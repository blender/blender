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

#ifndef INTERFACE_H
#define INTERFACE_H

#include "UI_resources.h"
#include "RNA_types.h"

struct ARegion;
struct bContext;
struct IDProperty;
struct uiHandleButtonData;
struct wmEvent;
struct wmOperatorType;
struct wmWindow;
struct uiStyle;
struct uiWidgetColors;
struct uiLayout;
struct bContextStore;
struct Scene;
struct ID;

/* ****************** general defines ************** */

/* visual types for drawing */
/* for time being separated from functional types */
typedef enum {
	/* default */
	UI_WTYPE_REGULAR,

	/* standard set */
	UI_WTYPE_LABEL,
	UI_WTYPE_TOGGLE,
	UI_WTYPE_OPTION,
	UI_WTYPE_RADIO,
	UI_WTYPE_NUMBER,
	UI_WTYPE_SLIDER,
	UI_WTYPE_EXEC,
	
	/* strings */
	UI_WTYPE_NAME,
	UI_WTYPE_NAME_LINK,
	UI_WTYPE_POINTER_LINK,
	UI_WTYPE_FILENAME,
	
	/* menus */
	UI_WTYPE_MENU_RADIO,
	UI_WTYPE_MENU_ICON_RADIO,
	UI_WTYPE_MENU_POINTER_LINK,
	
	UI_WTYPE_PULLDOWN,
	UI_WTYPE_MENU_ITEM,
	UI_WTYPE_MENU_BACK,
	
	/* specials */
	UI_WTYPE_ICON,
	UI_WTYPE_SWATCH,
	UI_WTYPE_RGB_PICKER,
	UI_WTYPE_NORMAL,
	UI_WTYPE_BOX,
	UI_WTYPE_SCROLL,
	UI_WTYPE_LISTITEM
	
} uiWidgetTypeEnum;



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
#define UI_TEXTINPUT	16
#define UI_HIDDEN		32
/* warn: rest of uiBut->flag in UI_interface.h */

/* internal panel drawing defines */
#define PNL_GRID	4
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
	short alignnr;
	int flag;
	
	char *str;
	char strdata[UI_MAX_NAME_STR];
	char drawstr[UI_MAX_DRAW_STR];
	
	float x1, y1, x2, y2;

	char *poin;
	float hardmin, hardmax, softmin, softmax;
	float a1, a2, hsv[3];	// hsv is temp memory for hsv buttons
	float aspect;

	uiButHandleFunc func;
	void *func_arg1;
	void *func_arg2;
	void *func_arg3;

	uiButHandleNFunc funcN;
	void *func_argN;

	struct bContextStore *context;

	void (*embossfunc)(int , int , float, float, float, float, float, int);
	void (*sliderfunc)(int , float, float, float, float, float, float, int);

	uiButCompleteFunc autocomplete_func;
	void *autofunc_arg;
	
	uiButSearchFunc search_func;
	void *search_arg;

	uiButHandleRenameFunc rename_func;
	void *rename_arg1;
	void *rename_orig;

	uiLink *link;
	short linkto[2];
	
	char *tip, *lockstr;

	BIFIconID icon;
	short but_align;	/* aligning buttons, horiz/vertical */
	short lock;
	short modifier_key;
	short iconadd, dt;

	/* IDPOIN data */
	uiIDPoinFuncFP idpoin_func;
	ID **idpoin_idpp;

	/* BLOCK data */
	uiBlockCreateFunc block_create_func;

	/* PULLDOWN/MENU data */
	uiMenuCreateFunc menu_create_func;

	/* RNA data */
	struct PointerRNA rnapoin;
	struct PropertyRNA *rnaprop;
	int rnaindex;

	struct PointerRNA rnasearchpoin;
	struct PropertyRNA *rnasearchprop;

	/* Operator data */
	struct wmOperatorType *optype;
	int opcontext;
	struct IDProperty *opproperties;
	struct PointerRNA *opptr;

		/* active button data */
	struct uiHandleButtonData *active;

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

	ListBase layouts;
	struct uiLayout *curlayout;

	ListBase contexts;
	
	char name[UI_MAX_NAME_STR];
	
	float winmat[4][4];
	
	float minx, miny, maxx, maxy;
	float aspect;

	short alignnr;

	uiButHandleFunc func;
	void *func_arg1;
	void *func_arg2;

	uiButHandleNFunc funcN;
	void *func_argN;

	uiMenuHandleFunc butm_func;
	void *butm_func_arg;

	uiBlockHandleFunc handle_func;
	void *handle_func_arg;
	
	/* custom extra handling */
	int (*block_event_func)(const struct bContext *C, struct uiBlock *, struct wmEvent *);
	
	/* extra draw function for custom blocks */
	void (*drawextra)(const struct bContext *C, void *idv, void *arg1, void *arg2, rcti *rect);
	void *drawextra_arg1;
	void *drawextra_arg2;

	int afterval, flag;
	
	short direction, dt;
	short auto_open, in_use;
	double auto_open_last;

	int lock;
	char *lockstr;
	
	float xofs, yofs;				// offset to parent button
	int dobounds, mx, my;			// for doing delayed
	int bounds, minbounds;			// for doing delayed
	int endblock;					// uiEndBlock done?

	rctf safety;				// pulldowns, to detect outside, can differ per case how it is created
	ListBase saferct;			// uiSafetyRct list

	uiPopupBlockHandle *handle;	// handle
	int tooltipdisabled;		// to avoid tooltip after click

	int active;					// to keep blocks while drawing and free them afterwards
	int puphash;				// popup menu hash for memory
	
	int color_profile;				// color profile for correcting linear colors for display

	void *evil_C;				// XXX hack for dynamic operator enums
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

void ui_fontscale(short *points, float aspect);

extern void ui_block_to_window_fl(const struct ARegion *ar, uiBlock *block, float *x, float *y);
extern void ui_block_to_window(const struct ARegion *ar, uiBlock *block, int *x, int *y);
extern void ui_block_to_window_rct(const struct ARegion *ar, uiBlock *block, rctf *graph, rcti *winr);
extern void ui_window_to_block_fl(const struct ARegion *ar, uiBlock *block, float *x, float *y);
extern void ui_window_to_block(const struct ARegion *ar, uiBlock *block, int *x, int *y);
extern void ui_window_to_region(const ARegion *ar, int *x, int *y);

extern double ui_get_but_val(uiBut *but);
extern void ui_set_but_val(uiBut *but, double value);
extern void ui_set_but_hsv(uiBut *but);
extern void ui_get_but_vectorf(uiBut *but, float *vec);
extern void ui_set_but_vectorf(uiBut *but, float *vec);

extern void ui_hsvcircle_vals_from_pos(float *valrad, float *valdist, rcti *rect, float mx, float my);

extern void ui_get_but_string(uiBut *but, char *str, int maxlen);
extern int ui_set_but_string(struct bContext *C, uiBut *but, const char *str);
extern int ui_get_but_string_max_length(uiBut *but);

extern void ui_set_but_soft_range(uiBut *but, double value);

extern void ui_check_but(uiBut *but);
extern int  ui_is_but_float(uiBut *but);
extern int  ui_is_but_unit(uiBut *but);
extern void ui_update_block_buts_hsv(uiBlock *block, float *hsv);

extern void ui_bounds_block(uiBlock *block);
extern void ui_block_translate(uiBlock *block, int x, int y);
extern void ui_block_do_align(uiBlock *block);

/* interface_regions.c */

struct uiPopupBlockHandle {
	/* internal */
	struct ARegion *region;
	int towardsx, towardsy;
	double towardstime;
	int dotowards;

	int popup;
	void (*popup_func)(struct bContext *C, void *arg, int event);
	void (*cancel_func)(void *arg);
	void *popup_arg;

	/* for operator popups */
	struct wmOperatorType *optype;
	int opcontext;
	ScrArea *ctx_area;
	ARegion *ctx_region;
	
	/* return values */
	int butretval;
	int menuretval;
	float retvalue;
	float retvec[3];
};

uiBlock *ui_block_func_COL(struct bContext *C, uiPopupBlockHandle *handle, void *arg_but);
void ui_block_func_ICONROW(struct bContext *C, uiLayout *layout, void *arg_but);
void ui_block_func_ICONTEXTROW(struct bContext *C, uiLayout *layout, void *arg_but);

struct ARegion *ui_tooltip_create(struct bContext *C, struct ARegion *butregion, uiBut *but);
void ui_tooltip_free(struct bContext *C, struct ARegion *ar);

uiBut *ui_popup_menu_memory(uiBlock *block, uiBut *but);

/* searchbox for string button */
ARegion *ui_searchbox_create(struct bContext *C, struct ARegion *butregion, uiBut *but);
int ui_searchbox_inside(struct ARegion *ar, int x, int y);
void ui_searchbox_update(struct bContext *C, struct ARegion *ar, uiBut *but, int reset);
void ui_searchbox_autocomplete(struct bContext *C, struct ARegion *ar, uiBut *but, char *str);
void ui_searchbox_event(struct bContext *C, struct ARegion *ar, uiBut *but, struct wmEvent *event);
void ui_searchbox_apply(uiBut *but, struct ARegion *ar);
void ui_searchbox_free(struct bContext *C, struct ARegion *ar);

typedef uiBlock* (*uiBlockHandleCreateFunc)(struct bContext *C, struct uiPopupBlockHandle *handle, void *arg1);

uiPopupBlockHandle *ui_popup_block_create(struct bContext *C, struct ARegion *butregion, uiBut *but,
	uiBlockCreateFunc create_func, uiBlockHandleCreateFunc handle_create_func, void *arg);
uiPopupBlockHandle *ui_popup_menu_create(struct bContext *C, struct ARegion *butregion, uiBut *but,
	uiMenuCreateFunc create_func, void *arg, char *str);

void ui_popup_block_free(struct bContext *C, uiPopupBlockHandle *handle);

void ui_set_name_menu(uiBut *but, int value);
int ui_step_name_menu(uiBut *but, int step);

struct AutoComplete;
struct AutoComplete *autocomplete_begin(char *startname, int maxlen);
void autocomplete_do_name(struct AutoComplete *autocpl, const char *name);
void autocomplete_end(struct AutoComplete *autocpl, char *autoname);

/* interface_panel.c */
extern int ui_handler_panel_region(struct bContext *C, struct wmEvent *event);
extern void ui_draw_aligned_panel(struct ARegion *ar, struct uiStyle *style, uiBlock *block, rcti *rect);

/* interface_draw.c */
extern void ui_dropshadow(rctf *rct, float radius, float aspect, int select);

extern void gl_round_box(int mode, float minx, float miny, float maxx, float maxy, float rad);
extern void gl_round_box_shade(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadetop, float shadedown);
extern void gl_round_box_vertical_shade(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadeLeft, float shadeRight);

void ui_draw_but_COLORBAND(uiBut *but, struct uiWidgetColors *wcol, rcti *rect);
void ui_draw_but_NORMAL(uiBut *but, struct uiWidgetColors *wcol, rcti *rect);
void ui_draw_but_CURVE(ARegion *ar, uiBut *but, struct uiWidgetColors *wcol, rcti *rect);
void ui_draw_but_IMAGE(ARegion *ar, uiBut *but, struct uiWidgetColors *wcol, rcti *rect);


/* interface_handlers.c */
extern void ui_button_activate_do(struct bContext *C, struct ARegion *ar, uiBut *but);
extern void ui_button_active_cancel(const struct bContext *C, uiBut *but);
extern int ui_button_is_active(struct ARegion *ar);

/* interface_widgets.c */
void ui_draw_anti_tria(float x1, float y1, float x2, float y2, float x3, float y3);
void ui_draw_menu_back(struct uiStyle *style, uiBlock *block, rcti *rect);
void ui_draw_search_back(struct uiStyle *style, uiBlock *block, rcti *rect);
void ui_draw_link_bezier(rcti *rect);

extern void ui_draw_but(const struct bContext *C, ARegion *ar, struct uiStyle *style, uiBut *but, rcti *rect);
		/* theme color init */
struct ThemeUI;
void ui_widget_color_init(struct ThemeUI *tui);

void ui_draw_menu_item(struct uiFontStyle *fstyle, rcti *rect, char *name, int iconid, int state);
void ui_draw_preview_item(struct uiFontStyle *fstyle, rcti *rect, char *name, int iconid, int state);

/* interface_style.c */
void uiStyleInit(void);

/* interface_icons.c */
void ui_id_icon_render(struct bContext *C, struct ID *id, int preview);
int ui_id_icon_get(struct bContext *C, struct ID *id, int preview);

/* resources.c */
void init_userdef_do_versions(void);
void ui_theme_init_userdef(void);
void ui_resources_init(void);
void ui_resources_free(void);

/* interface_layout.c */
void ui_layout_add_but(uiLayout *layout, uiBut *but);
int ui_but_can_align(uiBut *but);
void ui_but_add_search(uiBut *but, PointerRNA *ptr, PropertyRNA *prop, PointerRNA *searchptr, PropertyRNA *searchprop);

/* interface_anim.c */
void ui_but_anim_flag(uiBut *but, float cfra);
void ui_but_anim_insert_keyframe(struct bContext *C);
void ui_but_anim_delete_keyframe(struct bContext *C);
void ui_but_anim_add_driver(struct bContext *C);
void ui_but_anim_remove_driver(struct bContext *C);
void ui_but_anim_copy_driver(struct bContext *C);
void ui_but_anim_paste_driver(struct bContext *C);
void ui_but_anim_add_keyingset(struct bContext *C);
void ui_but_anim_remove_keyingset(struct bContext *C);
int ui_but_anim_expression_get(uiBut *but, char *str, int maxlen);
int ui_but_anim_expression_set(uiBut *but, const char *str);
void ui_but_anim_autokey(uiBut *but, struct Scene *scene, float cfra);

#endif

