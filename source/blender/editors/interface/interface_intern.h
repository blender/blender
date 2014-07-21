/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file blender/editors/interface/interface_intern.h
 *  \ingroup edinterface
 */


#ifndef __INTERFACE_INTERN_H__
#define __INTERFACE_INTERN_H__

#include "BLI_compiler_attrs.h"
#include "UI_resources.h"
#include "RNA_types.h"

struct ARegion;
struct bContext;
struct IDProperty;
struct uiHandleButtonData;
struct wmEvent;
struct wmOperatorType;
struct wmWindow;
struct wmTimer;
struct uiStyle;
struct uiWidgetColors;
struct uiLayout;
struct bContextStore;
struct Scene;
struct ID;
struct ImBuf;

/* ****************** general defines ************** */

#define RNA_NO_INDEX    -1
#define RNA_ENUM_VALUE  -2

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
	UI_WTYPE_TOOLTIP,
	
	/* strings */
	UI_WTYPE_NAME,
	UI_WTYPE_NAME_LINK,
	UI_WTYPE_POINTER_LINK,
	UI_WTYPE_FILENAME,
	
	/* menus */
	UI_WTYPE_MENU_RADIO,
	UI_WTYPE_MENU_ICON_RADIO,
	UI_WTYPE_MENU_POINTER_LINK,
	UI_WTYPE_MENU_NODE_LINK,
	
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
	UI_WTYPE_LISTITEM,
	UI_WTYPE_PROGRESSBAR,
} uiWidgetTypeEnum;

/* menu scrolling */
#define UI_MENU_SCROLL_ARROW	12
#define UI_MENU_SCROLL_MOUSE	(UI_MENU_SCROLL_ARROW + 2)
#define UI_MENU_SCROLL_PAD		4

/* panel limits */
#define UI_PANEL_MINX   100
#define UI_PANEL_MINY   70

/* uiBut->flag */
enum {
	UI_SELECT       = (1 << 0),  /* use when the button is pressed */
	UI_SCROLLED     = (1 << 1),  /* temp hidden, scrolled away */
	UI_ACTIVE       = (1 << 2),
	UI_HAS_ICON     = (1 << 3),
	UI_TEXTINPUT    = (1 << 4),
	UI_HIDDEN       = (1 << 5),
	/* warn: rest of uiBut->flag in UI_interface.h */
};

/* internal panel drawing defines */
#define PNL_GRID    (UI_UNIT_Y / 5) /* 4 default */
#define PNL_HEADER  (UI_UNIT_Y + 4) /* 24 default */

/* Button text selection:
 * extension direction, selextend, inside ui_do_but_TEX */
#define EXTEND_LEFT     1
#define EXTEND_RIGHT    2

/* for scope resize zone */
#define SCOPE_RESIZE_PAD    9

/* bit button defines */
/* Bit operations */
#define UI_BITBUT_TEST(a, b)    ( ( (a) & 1 << (b) ) != 0)
#define UI_BITBUT_SET(a, b)     ( (a) | 1 << (b) )
#define UI_BITBUT_CLR(a, b)     ( (a) & ~(1 << (b)) )
/* bit-row */
#define UI_BITBUT_ROW(min, max)  (((max) >= 31 ? 0xFFFFFFFF : (1 << (max + 1)) - 1) - ((min) ? ((1 << (min)) - 1) : 0) )

/* split numbuts by ':' and align l/r */
#define USE_NUMBUTS_LR_ALIGN

typedef struct uiLinkLine {  /* only for draw/edit */
	struct uiLinkLine *next, *prev;
	struct uiBut *from, *to;
	short flag, deactive;
} uiLinkLine;

typedef struct {
	void **poin;        /* pointer to original pointer */
	void ***ppoin;      /* pointer to original pointer-array */
	short *totlink;     /* if pointer-array, here is the total */
	
	short maxlink, pad;
	short fromcode, tocode;
	
	ListBase lines;
} uiLink;

struct uiBut {
	struct uiBut *next, *prev;
	int flag, drawflag;
	eButType         type;
	eButPointerType  pointype;
	short bit, bitnr, retval, strwidth, alignnr;
	short ofs, pos, selsta, selend;

	char *str;
	char strdata[UI_MAX_NAME_STR];
	char drawstr[UI_MAX_DRAW_STR];

	rctf rect;  /* block relative coords */

	char *poin;
	float hardmin, hardmax, softmin, softmax;

	/* both these values use depends on the button type
	 * (polymorphic struct or union would be nicer for this stuff) */

	/* (type == HSVCUBE),      Use UI_GRAD_* values.
	 * (type == NUM),        Use to store RNA 'step' value, for dragging and click-step.
	 * (type == LABEL),      Use (a1 == 1.0f) to use a2 as a blending factor (wow, this is imaginative!).
	 * (type == SCROLL)      Use as scroll size.
	 * (type == SEARCH_MENU) Use as number or rows.
	 * (type == COLOR)       Use as indication of color palette
	 */
	float a1;

	/* (type == HSVCIRCLE ), Use to store the luminosity.
	 * (type == NUM),        Use to store RNA 'precision' value, for dragging and click-step.
	 * (type == LABEL),      If (a1 == 1.0f) use a2 as a blending factor.
	 * (type == SEARCH_MENU) Use as number or columns.
	 * (type == COLOR)       Use as indication of active palette color
	 */
	float a2;

	unsigned char col[4];

	uiButHandleFunc func;
	void *func_arg1;
	void *func_arg2;

	uiButHandleNFunc funcN;
	void *func_argN;

	struct bContextStore *context;

	uiButCompleteFunc autocomplete_func;
	void *autofunc_arg;
	
	uiButSearchFunc search_func;
	void *search_arg;

	uiButHandleRenameFunc rename_func;
	void *rename_arg1;
	void *rename_orig;

	uiLink *link;
	short linkto[2];  /* region relative coords */
	
	const char *tip, *lockstr;

	BIFIconID icon;
	bool lock;
	char dt; /* drawtype: UI_EMBOSS, UI_EMBOSSN ... etc, copied from the block */
	char changed; /* could be made into a single flag */
	unsigned char unit_type; /* so buttons can support unit systems which are not RNA */
	short modifier_key;
	short iconadd;

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
	struct PointerRNA *opptr;
	short opcontext;
	unsigned char menu_key; /* 'a'-'z', always lower case */

	/* Draggable data, type is WM_DRAG_... */
	char dragtype;
	void *dragpoin;
	struct ImBuf *imb;
	float imb_scale;
	
	/* active button data */
	struct uiHandleButtonData *active;

	/* Custom button data. */
	void *custom_data;

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

	ListBase butstore;  /* UI_butstore_* runtime function */

	ListBase layouts;
	struct uiLayout *curlayout;

	ListBase contexts;
	
	char name[UI_MAX_NAME_STR];
	
	float winmat[4][4];

	rctf rect;
	float aspect;

	unsigned int puphash;  /* popup menu hash for memory */

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
	int (*block_event_func)(const struct bContext *C, struct uiBlock *, const struct wmEvent *);
	
	/* extra draw function for custom blocks */
	void (*drawextra)(const struct bContext *C, void *idv, void *arg1, void *arg2, rcti *rect);
	void *drawextra_arg1;
	void *drawextra_arg2;

	int flag;
	short alignnr;

	char direction;
	char dt; /* drawtype: UI_EMBOSS, UI_EMBOSSN ... etc, copied to buttons */
	bool auto_open;
	char _pad[7];
	double auto_open_last;

	const char *lockstr;

	char lock;
	char active;                /* to keep blocks while drawing and free them afterwards */
	char tooltipdisabled;       /* to avoid tooltip after click */
	char endblock;              /* uiEndBlock done? */

	eBlockBoundsCalc bounds_type;  /* for doing delayed */
	int mx, my;
	int bounds, minbounds;      /* for doing delayed */

	rctf safety;                /* pulldowns, to detect outside, can differ per case how it is created */
	ListBase saferct;           /* uiSafetyRct list */

	uiPopupBlockHandle *handle; /* handle */

	struct wmOperator *ui_operator; /* use so presets can find the operator, */
	                                /* across menus and from nested popups which fail for operator context. */

	void *evil_C;               /* XXX hack for dynamic operator enums */

	struct UnitSettings *unit;  /* unit system, used a lot for numeric buttons so include here rather then fetching through the scene every time. */
	float _hsv[3];              /* XXX, only access via ui_block_hsv_get() */

	bool color_profile;         /* color profile for correcting linear colors for display */

	char display_device[64]; /* display device name used to display this block,
	                          * used by color widgets to transform colors from/to scene linear
	                          */
};

typedef struct uiSafetyRct {
	struct uiSafetyRct *next, *prev;
	rctf parent;
	rctf safety;
} uiSafetyRct;

/* interface.c */

extern void ui_delete_linkline(uiLinkLine *line, uiBut *but);

void ui_fontscale(short *points, float aspect);

extern bool ui_block_is_menu(const uiBlock *block) ATTR_WARN_UNUSED_RESULT;
extern void ui_block_to_window_fl(const struct ARegion *ar, uiBlock *block, float *x, float *y);
extern void ui_block_to_window(const struct ARegion *ar, uiBlock *block, int *x, int *y);
extern void ui_block_to_window_rctf(const struct ARegion *ar, uiBlock *block, rctf *rct_dst, const rctf *rct_src);
extern void ui_window_to_block_fl(const struct ARegion *ar, uiBlock *block, float *x, float *y);
extern void ui_window_to_block(const struct ARegion *ar, uiBlock *block, int *x, int *y);
extern void ui_window_to_region(const ARegion *ar, int *x, int *y);

extern double ui_get_but_val(uiBut *but);
extern void ui_set_but_val(uiBut *but, double value);
extern void ui_set_but_hsv(uiBut *but);
extern void ui_get_but_vectorf(uiBut *but, float vec[3]);
extern void ui_set_but_vectorf(uiBut *but, const float vec[3]);

extern void ui_hsvcircle_vals_from_pos(float *val_rad, float *val_dist, const rcti *rect,
                                       const float mx, const float my);
extern void ui_hsvcircle_pos_from_vals(struct uiBut *but, const rcti *rect, float *hsv, float *xpos, float *ypos);
extern void ui_hsvcube_pos_from_vals(struct uiBut *but, const rcti *rect, float *hsv, float *xp, float *yp);
bool ui_color_picker_use_display_colorspace(struct uiBut *but);

extern void ui_get_but_string_ex(uiBut *but, char *str, const size_t maxlen, const int float_precision) ATTR_NONNULL();
extern void ui_get_but_string(uiBut *but, char *str, const size_t maxlen) ATTR_NONNULL();
extern void ui_convert_to_unit_alt_name(uiBut *but, char *str, size_t maxlen) ATTR_NONNULL();
extern bool ui_set_but_string(struct bContext *C, uiBut *but, const char *str) ATTR_NONNULL();
extern bool ui_set_but_string_eval_num(struct bContext *C, uiBut *but, const char *str, double *value) ATTR_NONNULL();
extern int  ui_get_but_string_max_length(uiBut *but);
extern uiBut *ui_get_but_drag_multi_edit(uiBut *but);

extern void ui_set_but_default(struct bContext *C, const bool all, const bool use_afterfunc);

extern void ui_check_but(uiBut *but);
extern bool ui_is_but_float(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
extern bool ui_is_but_bool(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
extern bool ui_is_but_unit(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
extern bool ui_is_but_compatible(const uiBut *but_a, const uiBut *but_b) ATTR_WARN_UNUSED_RESULT;
extern bool ui_is_but_rna_valid(uiBut *but) ATTR_WARN_UNUSED_RESULT;
extern bool ui_is_but_utf8(const uiBut *but) ATTR_WARN_UNUSED_RESULT;
extern bool ui_is_but_search_unlink_visible(const uiBut *but) ATTR_WARN_UNUSED_RESULT;

extern int  ui_is_but_push_ex(uiBut *but, double *value) ATTR_WARN_UNUSED_RESULT;
extern int  ui_is_but_push(uiBut *but) ATTR_WARN_UNUSED_RESULT;


extern void ui_bounds_block(uiBlock *block);
extern void ui_block_translate(uiBlock *block, int x, int y);
extern void ui_block_do_align(uiBlock *block);

extern struct ColorManagedDisplay *ui_block_display_get(uiBlock *block);
void ui_block_to_display_space_v3(uiBlock *block, float pixel[3]);
void ui_block_to_scene_linear_v3(uiBlock *block, float pixel[3]);

/* interface_regions.c */

struct uiKeyNavLock {
	/* set when we're using keyinput */
	bool is_keynav;
	/* only used to check if we've moved the cursor */
	int event_xy[2];
};

typedef uiBlock * (*uiBlockHandleCreateFunc)(struct bContext *C, struct uiPopupBlockHandle *handle, void *arg1);

struct uiPopupBlockCreate {
	uiBlockCreateFunc              create_func;
	uiBlockHandleCreateFunc handle_create_func;
	void *arg;

	int event_xy[2];

	/* when popup is initialized from a button */
	ARegion *butregion;
};

struct uiPopupBlockHandle {
	/* internal */
	struct ARegion *region;

	/* use only for 'UI_BLOCK_MOVEMOUSE_QUIT' popups */
	float towards_xy[2];
	double towardstime;
	bool dotowards;

	bool popup;
	void (*popup_func)(struct bContext *C, void *arg, int event);
	void (*cancel_func)(struct bContext *C, void *arg);
	void *popup_arg;
	
	/* store data for refreshing popups */
	struct uiPopupBlockCreate popup_create_vars;

	struct wmTimer *scrolltimer;

	struct uiKeyNavLock keynav_state;

	/* for operator popups */
	struct wmOperatorType *optype;
	ScrArea *ctx_area;
	ARegion *ctx_region;
	int opcontext;
	
	/* return values */
	int butretval;
	int menuretval;
	int   retvalue;
	float retvec[4];

	/* menu direction */
	int direction;

/* #ifdef USE_DRAG_POPUP */
	bool is_grab;
	int     grab_xy_prev[2];
/* #endif */
};

uiBlock *ui_block_func_COLOR(struct bContext *C, uiPopupBlockHandle *handle, void *arg_but);

struct ARegion *ui_tooltip_create(struct bContext *C, struct ARegion *butregion, uiBut *but);
void ui_tooltip_free(struct bContext *C, struct ARegion *ar);

uiBut *ui_popup_menu_memory_get(struct uiBlock *block);
void   ui_popup_menu_memory_set(uiBlock *block, struct uiBut *but);

void   ui_popup_translate(struct bContext *C, struct ARegion *ar, const int mdiff[2]);

float *ui_block_hsv_get(struct uiBlock *block);
void ui_popup_block_scrolltest(struct uiBlock *block);

void ui_rgb_to_color_picker_compat_v(const float rgb[3], float r_cp[3]);
void ui_rgb_to_color_picker_v(const float rgb[3], float r_cp[3]);
void ui_color_picker_to_rgb_v(const float r_cp[3], float rgb[3]);
void ui_color_picker_to_rgb(float r_cp0, float r_cp1, float r_cp2, float *r, float *g, float *b);

/* searchbox for string button */
ARegion *ui_searchbox_create(struct bContext *C, struct ARegion *butregion, uiBut *but);
bool ui_searchbox_inside(struct ARegion *ar, int x, int y);
int  ui_searchbox_find_index(struct ARegion *ar, const char *name);
void ui_searchbox_update(struct bContext *C, struct ARegion *ar, uiBut *but, const bool reset);
int ui_searchbox_autocomplete(struct bContext *C, struct ARegion *ar, uiBut *but, char *str);
void ui_searchbox_event(struct bContext *C, struct ARegion *ar, uiBut *but, const struct wmEvent *event);
bool ui_searchbox_apply(uiBut *but, struct ARegion *ar);
void ui_searchbox_free(struct bContext *C, struct ARegion *ar);
void ui_but_search_test(uiBut *but);

uiBlock *ui_popup_block_refresh(struct bContext *C, uiPopupBlockHandle *handle,
                                ARegion *butregion, uiBut *but);

uiPopupBlockHandle *ui_popup_block_create(struct bContext *C, struct ARegion *butregion, uiBut *but,
                                          uiBlockCreateFunc create_func, uiBlockHandleCreateFunc handle_create_func,
                                          void *arg);
uiPopupBlockHandle *ui_popup_menu_create(struct bContext *C, struct ARegion *butregion, uiBut *but,
                                         uiMenuCreateFunc create_func, void *arg);

void ui_popup_block_free(struct bContext *C, uiPopupBlockHandle *handle);

int ui_step_name_menu(uiBut *but, int step);

struct AutoComplete;

/* interface_panel.c */
extern int ui_handler_panel_region(struct bContext *C, const struct wmEvent *event, struct ARegion *ar);
extern void ui_draw_aligned_panel(struct uiStyle *style, uiBlock *block, const rcti *rect, const bool show_pin);

/* interface_draw.c */
extern void ui_dropshadow(const rctf *rct, float radius, float aspect, float alpha, int select);

void ui_draw_gradient(const rcti *rect, const float hsv[3], const int type, const float alpha);

void ui_draw_but_HISTOGRAM(ARegion *ar, uiBut *but, struct uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_WAVEFORM(ARegion *ar, uiBut *but, struct uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_VECTORSCOPE(ARegion *ar, uiBut *but, struct uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_COLORBAND(uiBut *but, struct uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_NORMAL(uiBut *but, struct uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_CURVE(ARegion *ar, uiBut *but, struct uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_IMAGE(ARegion *ar, uiBut *but, struct uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_TRACKPREVIEW(ARegion *ar, uiBut *but, struct uiWidgetColors *wcol, const rcti *rect);
void ui_draw_but_NODESOCKET(ARegion *ar, uiBut *but, struct uiWidgetColors *wcol, const rcti *rect);

/* interface_handlers.c */
PointerRNA *ui_handle_afterfunc_add_operator(struct wmOperatorType *ot, int opcontext, bool create_props);
extern void ui_pan_to_scroll(const struct wmEvent *event, int *type, int *val);
extern void ui_button_activate_do(struct bContext *C, struct ARegion *ar, uiBut *but);
extern void ui_button_execute_begin(struct bContext *C, struct ARegion *ar, uiBut *but, void **active_back);
extern void ui_button_execute_end(struct bContext *C, struct ARegion *ar, uiBut *but, void *active_back);
extern void ui_button_active_free(const struct bContext *C, uiBut *but);
extern bool ui_button_is_active(struct ARegion *ar) ATTR_WARN_UNUSED_RESULT;
extern int ui_button_open_menu_direction(uiBut *but);
extern void ui_button_text_password_hide(char password_str[UI_MAX_DRAW_STR], uiBut *but, const bool restore);
extern uiBut *ui_but_find_activated(struct ARegion *ar);

void ui_button_clipboard_free(void);
void ui_panel_menu(struct bContext *C, ARegion *ar, Panel *pa);
uiBut *ui_but_find_old(uiBlock *block_old, const uiBut *but_new);
uiBut *ui_but_find_new(uiBlock *block_old, const uiBut *but_new);

/* interface_widgets.c */
void ui_draw_anti_tria(float x1, float y1, float x2, float y2, float x3, float y3);
void ui_draw_anti_roundbox(int mode, float minx, float miny, float maxx, float maxy, float rad, bool use_alpha);
void ui_draw_menu_back(struct uiStyle *style, uiBlock *block, rcti *rect);
uiWidgetColors *ui_tooltip_get_theme(void);
void ui_draw_tooltip_background(uiStyle *UNUSED(style), uiBlock *block, rcti *rect);
void ui_draw_search_back(struct uiStyle *style, uiBlock *block, rcti *rect);
bool ui_link_bezier_points(const rcti *rect, float coord_array[][2], int resol);
void ui_draw_link_bezier(const rcti *rect);

extern void ui_draw_but(const struct bContext *C, ARegion *ar, struct uiStyle *style, uiBut *but, rcti *rect);
/* theme color init */
struct ThemeUI;
void ui_widget_color_init(struct ThemeUI *tui);

void ui_draw_menu_item(struct uiFontStyle *fstyle, rcti *rect, const char *name, int iconid, int state, bool use_sep);
void ui_draw_preview_item(struct uiFontStyle *fstyle, rcti *rect, const char *name, int iconid, int state);

#define UI_TEXT_MARGIN_X 0.4f

/* interface_style.c */
void uiStyleInit(void);

/* interface_icons.c */
int ui_id_icon_get(struct bContext *C, struct ID *id, const bool big);

/* resources.c */
void init_userdef_do_versions(void);
void init_userdef_factory(void);
void ui_theme_init_default(void);
void ui_style_init_default(void);
void ui_resources_init(void);
void ui_resources_free(void);

/* interface_layout.c */
void ui_layout_add_but(uiLayout *layout, uiBut *but);
bool ui_but_can_align(uiBut *but) ATTR_WARN_UNUSED_RESULT;
void ui_but_add_search(uiBut *but, PointerRNA *ptr, PropertyRNA *prop, PointerRNA *searchptr, PropertyRNA *searchprop);
void ui_but_add_shortcut(uiBut *but, const char *key_str, const bool do_strip);
void ui_layout_list_set_labels_active(uiLayout *layout);

/* interface_anim.c */
void ui_but_anim_flag(uiBut *but, float cfra);
void ui_but_anim_insert_keyframe(struct bContext *C);
void ui_but_anim_delete_keyframe(struct bContext *C);
void ui_but_anim_clear_keyframe(struct bContext *C);
void ui_but_anim_add_driver(struct bContext *C);
void ui_but_anim_remove_driver(struct bContext *C);
void ui_but_anim_copy_driver(struct bContext *C);
void ui_but_anim_paste_driver(struct bContext *C);
void ui_but_anim_add_keyingset(struct bContext *C);
void ui_but_anim_remove_keyingset(struct bContext *C);
bool ui_but_anim_expression_get(uiBut *but, char *str, size_t maxlen);
bool ui_but_anim_expression_set(uiBut *but, const char *str);
bool ui_but_anim_expression_create(uiBut *but, const char *str);
void ui_but_anim_autokey(struct bContext *C, uiBut *but, struct Scene *scene, float cfra);

/* interface_eyedropper.c */
void UI_OT_eyedropper_color(struct wmOperatorType *ot);
void UI_OT_eyedropper_id(struct wmOperatorType *ot);

#endif  /* __INTERFACE_INTERN_H__ */
