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

/** \file UI_interface.h
 *  \ingroup editorui
 */

#ifndef __UI_INTERFACE_H__
#define __UI_INTERFACE_H__

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h" /* size_t */
#include "RNA_types.h"
#include "DNA_userdef_types.h"

/* Struct Declarations */

struct ID;
struct Main;
struct ListBase;
struct ARegion;
struct ARegionType;
struct ScrArea;
struct wmEvent;
struct wmWindow;
struct wmWindowManager;
struct wmOperator;
struct AutoComplete;
struct bContext;
struct bContextStore;
struct Panel;
struct PanelType;
struct PointerRNA;
struct PropertyRNA;
struct ReportList;
struct rcti;
struct rctf;
struct uiList;
struct uiStyle;
struct uiFontStyle;
struct uiWidgetColors;
struct ColorBand;
struct CurveMapping;
struct Image;
struct ImageUser;
struct wmOperatorType;
struct uiWidgetColors;
struct Tex;
struct MTex;
struct ImBuf;
struct bNodeTree;
struct bNode;
struct bNodeSocket;
struct wmDropBox;
struct wmDrag;
struct wmEvent;

typedef struct uiBut uiBut;
typedef struct uiBlock uiBlock;
typedef struct uiPopupBlockHandle uiPopupBlockHandle;
typedef struct uiLayout uiLayout;

/* Defines */

/* char for splitting strings, aligning shortcuts in menus, users never see */
#define UI_SEP_CHAR   '|'
#define UI_SEP_CHAR_S "|"

/* names */
#define UI_MAX_DRAW_STR 400
#define UI_MAX_NAME_STR 128

/* use for clamping popups within the screen */
#define UI_SCREEN_MARGIN 10

/* uiBlock->dt and uiBut->dt */
enum {
	UI_EMBOSS               = 0,  /* use widget style for drawing */
	UI_EMBOSS_NONE          = 1,  /* Nothing, only icon and/or text */
	UI_EMBOSS_PULLDOWN      = 2,  /* Pulldown menu style */
	UI_EMBOSS_RADIAL        = 3,  /* Pie Menu */
};

/* uiBlock->direction */
enum {
	UI_DIR_UP           = (1 << 0),
	UI_DIR_DOWN         = (1 << 1),
	UI_DIR_LEFT         = (1 << 2),
	UI_DIR_RIGHT        = (1 << 3),
	UI_DIR_CENTER_Y     = (1 << 4),

	UI_DIR_ALL          = (UI_DIR_UP | UI_DIR_DOWN | UI_DIR_LEFT | UI_DIR_RIGHT),
};

#if 0
/* uiBlock->autofill (not yet used) */
#define UI_BLOCK_COLLUMNS  1
#define UI_BLOCK_ROWS      2
#endif

/* uiBlock->flag (controls) */
#define UI_BLOCK_LOOP           (1 << 0)
#define UI_BLOCK_IS_FLIP        (1 << 1)
#define UI_BLOCK_NO_FLIP        (1 << 2)
#define UI_BLOCK_NUMSELECT      (1 << 3)
#define UI_BLOCK_NO_WIN_CLIP    (1 << 4)   /* don't apply window clipping */ /* was UI_BLOCK_ENTER_OK */
#define UI_BLOCK_CLIPBOTTOM     (1 << 5)
#define UI_BLOCK_CLIPTOP        (1 << 6)
#define UI_BLOCK_MOVEMOUSE_QUIT (1 << 7)
#define UI_BLOCK_KEEP_OPEN      (1 << 8)
#define UI_BLOCK_POPUP          (1 << 9)
#define UI_BLOCK_OUT_1          (1 << 10)
#define UI_BLOCK_SEARCH_MENU    (1 << 11)
#define UI_BLOCK_POPUP_MEMORY   (1 << 12)
#define UI_BLOCK_CLIP_EVENTS    (1 << 13)  /* stop handling mouse events */

/* block->flag bits 14-17 are identical to but->drawflag bits */

#define UI_BLOCK_LIST_ITEM   (1 << 19)
#define UI_BLOCK_RADIAL      (1 << 20)

/* uiPopupBlockHandle->menuretval */
#define UI_RETURN_CANCEL     (1 << 0)   /* cancel all menus cascading */
#define UI_RETURN_OK         (1 << 1)   /* choice made */
#define UI_RETURN_OUT        (1 << 2)   /* left the menu */
#define UI_RETURN_OUT_PARENT (1 << 3)   /* let the parent handle this event */
#define UI_RETURN_UPDATE     (1 << 4)   /* update the button that opened */
#define UI_RETURN_POPUP_OK   (1 << 5)   /* popup is ok to be handled */

/* panel controls */
#define UI_PNL_SOLID    (1 << 1)
#define UI_PNL_CLOSE    (1 << 5)
#define UI_PNL_SCALE    (1 << 9)

/* but->flag - general state flags. */
enum {
	/* warning, the first 6 flags are internal */
	UI_BUT_ICON_SUBMENU  = (1 << 6),
	UI_BUT_ICON_PREVIEW  = (1 << 7),

	UI_BUT_NODE_LINK     = (1 << 8),
	UI_BUT_NODE_ACTIVE   = (1 << 9),
	UI_BUT_DRAG_LOCK     = (1 << 10),
	UI_BUT_DISABLED      = (1 << 11),
	UI_BUT_COLOR_LOCK    = (1 << 12),
	UI_BUT_ANIMATED      = (1 << 13),
	UI_BUT_ANIMATED_KEY  = (1 << 14),
	UI_BUT_DRIVEN        = (1 << 15),
	UI_BUT_REDALERT      = (1 << 16),
	UI_BUT_INACTIVE      = (1 << 17),
	UI_BUT_LAST_ACTIVE   = (1 << 18),
	UI_BUT_UNDO          = (1 << 19),
	UI_BUT_IMMEDIATE     = (1 << 20),
	UI_BUT_NO_UTF8       = (1 << 21),

	UI_BUT_VEC_SIZE_LOCK = (1 << 22),  /* used to flag if color hsv-circle should keep luminance */
	UI_BUT_COLOR_CUBIC   = (1 << 23),  /* cubic saturation for the color wheel */
	UI_BUT_LIST_ITEM     = (1 << 24),  /* This but is "inside" a list item (currently used to change theme colors). */
	UI_BUT_DRAG_MULTI    = (1 << 25),  /* edit this button as well as the active button (not just dragging) */
	UI_BUT_SCA_LINK_GREY = (1 << 26),  /* used to flag if sca links shoud be grey out */
	UI_BUT_HAS_SEP_CHAR  = (1 << 27),  /* but->str contains UI_SEP_CHAR, used for key shortcuts */
	UI_BUT_TIP_FORCE     = (1 << 28),  /* force show tooltips when holding option/alt if U's USER_TOOLTIPS is off */
};

#define UI_PANEL_WIDTH          340
#define UI_COMPACT_PANEL_WIDTH  160

#define UI_PANEL_CATEGORY_MARGIN_WIDTH (U.widget_unit * 1.0f)

/* but->drawflag - these flags should only affect how the button is drawn. */
/* Note: currently, these flags _are not passed_ to the widget's state() or draw() functions
 *       (except for the 'align' ones)!
 */
enum {
	/* Text and icon alignment (by default, they are centered). */
	UI_BUT_TEXT_LEFT         = (1 << 1),
	UI_BUT_ICON_LEFT         = (1 << 2),
	UI_BUT_TEXT_RIGHT        = (1 << 3),
	/* Prevent the button to show any tooltip. */
	UI_BUT_NO_TOOLTIP        = (1 << 4),
	/* button align flag, for drawing groups together (also used in uiBlock->flag!) */
	UI_BUT_ALIGN_TOP         = (1 << 14),
	UI_BUT_ALIGN_LEFT        = (1 << 15),
	UI_BUT_ALIGN_RIGHT       = (1 << 16),
	UI_BUT_ALIGN_DOWN        = (1 << 17),
	UI_BUT_ALIGN             = (UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_LEFT | UI_BUT_ALIGN_RIGHT | UI_BUT_ALIGN_DOWN),
};

/* scale fixed button widths by this to account for DPI */

#define UI_DPI_FAC ((U.pixelsize * (float)U.dpi) / 72.0f)
#define UI_DPI_WINDOW_FAC (((float)U.dpi) / 72.0f)
/* 16 to copy ICON_DEFAULT_HEIGHT */
#define UI_DPI_ICON_SIZE ((float)16 * UI_DPI_FAC)

/* Button types, bits stored in 1 value... and a short even!
 * - bits 0-4:  bitnr (0-31)
 * - bits 5-7:  pointer type
 * - bit  8:    for 'bit'
 * - bit  9-15: button type (now 6 bits, 64 types)
 * */
typedef enum {
	UI_BUT_POIN_CHAR = 32,
	UI_BUT_POIN_SHORT = 64,
	UI_BUT_POIN_INT = 96,
	UI_BUT_POIN_FLOAT = 128,
/*	UI_BUT_POIN_FUNCTION = 192, */ /*UNUSED*/
	UI_BUT_POIN_BIT = 256  /* OR'd with a bit index*/
} eButPointerType;

/* requires (but->poin != NULL) */
#define UI_BUT_POIN_TYPES (UI_BUT_POIN_FLOAT | UI_BUT_POIN_SHORT | UI_BUT_POIN_CHAR)

/* assigned to but->type, OR'd with the flags above when passing args */
typedef enum {
	UI_BTYPE_BUT                    = (1 << 9),
	UI_BTYPE_ROW                    = (2 << 9),
	UI_BTYPE_TEXT                   = (3 << 9),
	UI_BTYPE_MENU                   = (4 << 9),  /* dropdown list */
	UI_BTYPE_BUT_MENU               = (5 << 9),
	UI_BTYPE_NUM                    = (6 << 9),  /* number button */
	UI_BTYPE_NUM_SLIDER             = (7 << 9),  /* number slider */
	UI_BTYPE_TOGGLE                 = (8 << 9),
	UI_BTYPE_TOGGLE_N               = (9 << 9),
	UI_BTYPE_ICON_TOGGLE            = (10 << 9),
	UI_BTYPE_ICON_TOGGLE_N          = (11 << 9),
	UI_BTYPE_BUT_TOGGLE             = (12 << 9),  /* same as regular toggle, but no on/off state displayed */
	UI_BTYPE_CHECKBOX               = (13 << 9),  /* similar to toggle, display a 'tick' */
	UI_BTYPE_CHECKBOX_N             = (14 << 9),
	UI_BTYPE_COLOR                  = (15 << 9),
	UI_BTYPE_SCROLL                 = (18 << 9),
	UI_BTYPE_BLOCK                  = (19 << 9),
	UI_BTYPE_LABEL                  = (20 << 9),
	UI_BTYPE_LINK                   = (22 << 9),
	UI_BTYPE_INLINK                 = (23 << 9),
	UI_BTYPE_KEY_EVENT              = (24 << 9),
	UI_BTYPE_HSVCUBE                = (26 << 9),
	UI_BTYPE_PULLDOWN               = (27 << 9),  /* menu (often used in headers), **_MENU /w different draw-type */
	UI_BTYPE_ROUNDBOX               = (28 << 9),
	UI_BTYPE_COLORBAND              = (30 << 9),
	UI_BTYPE_UNITVEC                = (31 << 9),  /* sphere widget (used to input a unit-vector, aka normal) */
	UI_BTYPE_CURVE                  = (32 << 9),
	UI_BTYPE_LISTBOX                = (36 << 9),
	UI_BTYPE_LISTROW                = (37 << 9),
	UI_BTYPE_HSVCIRCLE              = (38 << 9),
	UI_BTYPE_TRACK_PREVIEW          = (40 << 9),

	/* buttons with value >= UI_BTYPE_SEARCH_MENU don't get undo pushes */
	UI_BTYPE_SEARCH_MENU            = (41 << 9),
	UI_BTYPE_EXTRA                  = (42 << 9),
	UI_BTYPE_HOTKEY_EVENT           = (46 << 9),
	UI_BTYPE_IMAGE                  = (47 << 9),  /* non-interactive image, used for splash screen */
	UI_BTYPE_HISTOGRAM              = (48 << 9),
	UI_BTYPE_WAVEFORM               = (49 << 9),
	UI_BTYPE_VECTORSCOPE            = (50 << 9),
	UI_BTYPE_PROGRESS_BAR           = (51 << 9),
	UI_BTYPE_SEARCH_MENU_UNLINK     = (52 << 9),
	UI_BTYPE_NODE_SOCKET            = (53 << 9),
	UI_BTYPE_SEPR                   = (54 << 9),
	UI_BTYPE_SEPR_LINE              = (55 << 9),
	UI_BTYPE_GRIP                   = (56 << 9),  /* resize handle (resize uilist) */
} eButType;

#define BUTTYPE     (63 << 9)

/* gradient types, for color picker UI_BTYPE_HSVCUBE etc */
#define UI_GRAD_SV      0
#define UI_GRAD_HV      1
#define UI_GRAD_HS      2
#define UI_GRAD_H       3
#define UI_GRAD_S       4
#define UI_GRAD_V       5

#define UI_GRAD_V_ALT   9
#define UI_GRAD_L_ALT   10

#define UI_PALETTE_COLOR 20

/* Drawing
 *
 * Functions to draw various shapes, taking theme settings into account.
 * Used for code that draws its own UI style elements. */

void UI_draw_roundbox(float minx, float miny, float maxx, float maxy, float rad);
void UI_draw_roundbox_corner_set(int type);
int  UI_draw_roundbox_corner_get(void);
void UI_draw_roundbox_unfilled(float minx, float miny, float maxx, float maxy, float rad);
void UI_draw_box_shadow(unsigned char alpha, float minx, float miny, float maxx, float maxy);
void UI_draw_roundbox_gl_mode(int mode, float minx, float miny, float maxx, float maxy, float rad);
void UI_draw_roundbox_shade_x(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadetop, float shadedown);
void UI_draw_roundbox_shade_y(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadeLeft, float shadeRight);

/* state for scrolldrawing */
#define UI_SCROLL_PRESSED       (1 << 0)
#define UI_SCROLL_ARROWS        (1 << 1)
#define UI_SCROLL_NO_OUTLINE    (1 << 2)
void UI_draw_widget_scroll(struct uiWidgetColors *wcol, const struct rcti *rect, const struct rcti *slider, int state);

/* Callbacks
 *
 * UI_block_func_handle_set/ButmFunc are for handling events through a callback.
 * HandleFunc gets the retval passed on, and ButmFunc gets a2. The latter is
 * mostly for compatibility with older code.
 *
 * UI_but_func_complete_set is for tab completion.
 *
 * uiButSearchFunc is for name buttons, showing a popup with matches
 *
 * UI_block_func_set and UI_but_func_set are callbacks run when a button is used,
 * in case events, operators or RNA are not sufficient to handle the button.
 *
 * UI_but_funcN_set will free the argument with MEM_freeN. */

typedef struct uiSearchItems uiSearchItems;

typedef void (*uiButHandleFunc)(struct bContext *C, void *arg1, void *arg2);
typedef void (*uiButHandleRenameFunc)(struct bContext *C, void *arg, char *origstr);
typedef void (*uiButHandleNFunc)(struct bContext *C, void *argN, void *arg2);
typedef int (*uiButCompleteFunc)(struct bContext *C, char *str, void *arg);
typedef void (*uiButSearchFunc)(const struct bContext *C, void *arg, const char *str, uiSearchItems *items);
typedef void (*uiBlockHandleFunc)(struct bContext *C, void *arg, int event);

/* Menu Callbacks */

typedef void (*uiMenuCreateFunc)(struct bContext *C, struct uiLayout *layout, void *arg1);
typedef void (*uiMenuHandleFunc)(struct bContext *C, void *arg, int event);

/* Popup Menus
 *
 * Functions used to create popup menus. For more extended menus the
 * UI_popup_menu_begin/End functions can be used to define own items with
 * the uiItem functions in between. If it is a simple confirmation menu
 * or similar, popups can be created with a single function call. */

typedef struct uiPopupMenu uiPopupMenu;

struct uiPopupMenu *UI_popup_menu_begin(struct bContext *C, const char *title, int icon) ATTR_NONNULL();
void UI_popup_menu_end(struct bContext *C, struct uiPopupMenu *head);
struct uiLayout *UI_popup_menu_layout(uiPopupMenu *head);

void UI_popup_menu_reports(struct bContext *C, struct ReportList *reports) ATTR_NONNULL();
int UI_popup_menu_invoke(struct bContext *C, const char *idname, struct ReportList *reports) ATTR_NONNULL(1, 2);

/* Pie menus */
typedef struct uiPieMenu uiPieMenu;

int UI_pie_menu_invoke(struct bContext *C, const char *idname, const struct wmEvent *event);
int UI_pie_menu_invoke_from_operator_enum(
        struct bContext *C, const char *title, const char *opname,
        const char *propname, const struct wmEvent *event);
int UI_pie_menu_invoke_from_rna_enum(
        struct bContext *C, const char *title,
        const char *path, const struct wmEvent *event);

struct uiPieMenu *UI_pie_menu_begin(
        struct bContext *C, const char *title, int icon,
        const struct wmEvent *event) ATTR_NONNULL();
void UI_pie_menu_end(struct bContext *C, uiPieMenu *pie);
struct uiLayout *UI_pie_menu_layout(struct uiPieMenu *pie);
/* Popup Blocks
 *
 * Functions used to create popup blocks. These are like popup menus
 * but allow using all button types and creating an own layout. */

typedef uiBlock * (*uiBlockCreateFunc)(struct bContext *C, struct ARegion *ar, void *arg1);
typedef void (*uiBlockCancelFunc)(struct bContext *C, void *arg1);

void UI_popup_block_invoke(struct bContext *C, uiBlockCreateFunc func, void *arg);
void UI_popup_block_invoke_ex(struct bContext *C, uiBlockCreateFunc func, void *arg, const char *opname, int opcontext);
void UI_popup_block_ex(struct bContext *C, uiBlockCreateFunc func, uiBlockHandleFunc popup_func, uiBlockCancelFunc cancel_func, void *arg);
/* void uiPupBlockOperator(struct bContext *C, uiBlockCreateFunc func, struct wmOperator *op, int opcontext); */ /* UNUSED */

void UI_popup_block_close(struct bContext *C, uiBlock *block);

/* Blocks
 *
 * Functions for creating, drawing and freeing blocks. A Block is a
 * container of buttons and used for various purposes.
 * 
 * Begin/Define Buttons/End/Draw is the typical order in which these
 * function should be called, though for popup blocks Draw is left out.
 * Freeing blocks is done by the screen/ module automatically.
 *
 * */

uiBlock *UI_block_begin(const struct bContext *C, struct ARegion *region, const char *name, short dt);
void UI_block_end_ex(const struct bContext *C, uiBlock *block, const int xy[2]);
void UI_block_end(const struct bContext *C, uiBlock *block);
void UI_block_draw(const struct bContext *C, struct uiBlock *block);
void UI_block_update_from_old(const struct bContext *C, struct uiBlock *block);

uiBlock *UI_block_find_in_region(const char *name, struct ARegion *ar);

void UI_block_emboss_set(uiBlock *block, char dt);

void UI_block_free(const struct bContext *C, uiBlock *block);
void UI_blocklist_free(const struct bContext *C, struct ListBase *lb);
void UI_blocklist_free_inactive(const struct bContext *C, struct ListBase *lb);
void UI_screen_free_active_but(const struct bContext *C, struct bScreen *screen);

void UI_block_region_set(uiBlock *block, struct ARegion *region);

void UI_block_lock_set(uiBlock *block, bool val, const char *lockstr);
void UI_block_lock_clear(uiBlock *block);

/* automatic aligning, horiz or verical */
void UI_block_align_begin(uiBlock *block);
void UI_block_align_end(uiBlock *block);

/* block bounds/position calculation */
typedef enum {
	UI_BLOCK_BOUNDS_NONE = 0,
	UI_BLOCK_BOUNDS = 1,
	UI_BLOCK_BOUNDS_TEXT,
	UI_BLOCK_BOUNDS_POPUP_MOUSE,
	UI_BLOCK_BOUNDS_POPUP_MENU,
	UI_BLOCK_BOUNDS_POPUP_CENTER,
	UI_BLOCK_BOUNDS_PIE_CENTER,
} eBlockBoundsCalc;

void UI_block_bounds_set_normal(struct uiBlock *block, int addval);
void UI_block_bounds_set_text(uiBlock *block, int addval);
void UI_block_bounds_set_popup(uiBlock *block, int addval, int mx, int my);
void UI_block_bounds_set_menu(uiBlock *block, int addvall, int mx, int my);
void UI_block_bounds_set_centered(uiBlock *block, int addval);
void UI_block_bounds_set_explicit(uiBlock *block, int minx, int miny, int maxx, int maxy);

int     UI_blocklist_min_y_get(struct ListBase *lb);

void    UI_block_direction_set(uiBlock *block, char direction);
void    UI_block_order_flip(uiBlock *block);
void    UI_block_flag_enable(uiBlock *block, int flag);
void    UI_block_flag_disable(uiBlock *block, int flag);

int     UI_but_return_value_get(uiBut *but);

void    UI_but_drag_set_id(uiBut *but, struct ID *id);
void    UI_but_drag_set_rna(uiBut *but, struct PointerRNA *ptr);
void    UI_but_drag_set_path(uiBut *but, const char *path);
void    UI_but_drag_set_name(uiBut *but, const char *name);
void    UI_but_drag_set_value(uiBut *but);
void    UI_but_drag_set_image(uiBut *but, const char *path, int icon, struct ImBuf *ima, float scale);

bool    UI_but_active_drop_name(struct bContext *C);
bool    UI_but_active_drop_color(struct bContext *C);

void    UI_but_flag_enable(uiBut *but, int flag);
void    UI_but_flag_disable(uiBut *but, int flag);

void    UI_but_drawflag_enable(uiBut *but, int flag);
void    UI_but_drawflag_disable(uiBut *but, int flag);

void    UI_but_type_set_menu_from_pulldown(uiBut *but);

/* special button case, only draw it when used actively, for outliner etc */
bool    UI_but_active_only(const struct bContext *C, struct ARegion *ar, uiBlock *block, uiBut *but);

void    UI_but_execute(const struct bContext *C, uiBut *but);


/* Buttons
 *
 * Functions to define various types of buttons in a block. Postfixes:
 * - F: float
 * - I: int
 * - S: short
 * - C: char
 * - R: RNA
 * - O: operator */

uiBut *uiDefBut(uiBlock *block, 
                int type, int retval, const char *str,
                int x1, int y1,
                short x2, short y2,
                void *poin,
                float min, float max,
                float a1, float a2, const char *tip);
uiBut *uiDefButF(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefButBitF(uiBlock *block, int type, int bit, int retval, const char *str, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefButI(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefButBitI(uiBlock *block, int type, int bit, int retval, const char *str, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefButS(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefButBitS(uiBlock *block, int type, int bit, int retval, const char *str, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefButC(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefButBitC(uiBlock *block, int type, int bit, int retval, const char *str, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefButR(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, struct PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefButR_prop(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, struct PointerRNA *ptr, struct PropertyRNA *prop, int index, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefButO(uiBlock *block, int type, const char *opname, int opcontext, const char *str, int x, int y, short width, short height, const char *tip);
uiBut *uiDefButO_ptr(uiBlock *block, int type, struct wmOperatorType *ot, int opcontext, const char *str, int x, int y, short width, short height, const char *tip);

uiBut *uiDefIconBut(uiBlock *block, 
                    int type, int retval, int icon,
                    int x1, int y1,
                    short x2, short y2,
                    void *poin,
                    float min, float max,
                    float a1, float a2,  const char *tip);
uiBut *uiDefIconButF(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconButBitF(uiBlock *block, int type, int bit, int retval, int icon, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconButI(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconButBitI(uiBlock *block, int type, int bit, int retval, int icon, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconButS(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconButBitS(uiBlock *block, int type, int bit, int retval, int icon, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconButC(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconButBitC(uiBlock *block, int type, int bit, int retval, int icon, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconButR(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, struct PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconButR_prop(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, struct PointerRNA *ptr, PropertyRNA *prop, int index, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconButO(uiBlock *block, int type, const char *opname, int opcontext, int icon, int x, int y, short width, short height, const char *tip);
uiBut *uiDefIconButO_ptr(uiBlock *block, int type, struct wmOperatorType *ot, int opcontext, int icon, int x, int y, short width, short height, const char *tip);

uiBut *uiDefIconTextBut(uiBlock *block,
                        int type, int retval, int icon, const char *str,
                        int x1, int y1,
                        short x2, short y2,
                        void *poin,
                        float min, float max,
                        float a1, float a2, const char *tip);
uiBut *uiDefIconTextButF(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconTextButBitF(uiBlock *block, int type, int bit, int retval, int icon, const char *str, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconTextButI(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconTextButBitI(uiBlock *block, int type, int bit, int retval, int icon, const char *str, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconTextButS(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconTextButBitS(uiBlock *block, int type, int bit, int retval, int icon, const char *str, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconTextButC(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconTextButBitC(uiBlock *block, int type, int bit, int retval, int icon, const char *str, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconTextButR(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, struct PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconTextButR_prop(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, struct PointerRNA *ptr, struct PropertyRNA *prop, int index, float min, float max, float a1, float a2, const char *tip);
uiBut *uiDefIconTextButO(uiBlock *block, int type, const char *opname, int opcontext, int icon, const char *str, int x, int y, short width, short height, const char *tip);
uiBut *uiDefIconTextButO_ptr(uiBlock *block, int type, struct wmOperatorType *ot, int opcontext, int icon, const char *str, int x, int y, short width, short height, const char *tip);

/* for passing inputs to ButO buttons */
struct PointerRNA *UI_but_operator_ptr_get(uiBut *but);

void UI_but_unit_type_set(uiBut *but, const int unit_type);
int UI_but_unit_type_get(const uiBut *but);

enum {
	BUT_GET_RNAPROP_IDENTIFIER = 1,
	BUT_GET_RNASTRUCT_IDENTIFIER,
	BUT_GET_RNAENUM_IDENTIFIER,
	BUT_GET_LABEL,
	BUT_GET_RNA_LABEL,
	BUT_GET_RNAENUM_LABEL,
	BUT_GET_RNA_LABEL_CONTEXT, /* Context specified in CTX_XXX_ macros are just unreachable! */
	BUT_GET_TIP,
	BUT_GET_RNA_TIP,
	BUT_GET_RNAENUM_TIP,
	BUT_GET_OP_KEYMAP,
	BUT_GET_PROP_KEYMAP
};

typedef struct uiStringInfo {
	int type;
	char *strinfo;
} uiStringInfo; 

/* Note: Expects pointers to uiStringInfo structs as parameters.
 *       Will fill them with translated strings, when possible.
 *       Strings in uiStringInfo must be MEM_freeN'ed by caller. */
void UI_but_string_info_get(struct bContext *C, uiBut *but, ...) ATTR_SENTINEL(0);

/* Edit i18n stuff. */
/* Name of the main py op from i18n addon. */
#define EDTSRC_I18N_OP_NAME "UI_OT_edittranslation"

/* Special Buttons
 *
 * Buttons with a more specific purpose:
 * - MenuBut: buttons that popup a menu (in headers usually).
 * - PulldownBut: like MenuBut, but creating a uiBlock (for compatibility).
 * - BlockBut: buttons that popup a block with more buttons.
 * - KeyevtBut: buttons that can be used to turn key events into values.
 * - PickerButtons: buttons like the color picker (for code sharing).
 * - AutoButR: RNA property button with type automatically defined. */

#define UI_ID_RENAME        (1 << 0)
#define UI_ID_BROWSE        (1 << 1)
#define UI_ID_ADD_NEW       (1 << 2)
#define UI_ID_OPEN          (1 << 3)
#define UI_ID_ALONE         (1 << 4)
#define UI_ID_DELETE        (1 << 5)
#define UI_ID_LOCAL         (1 << 6)
#define UI_ID_AUTO_NAME     (1 << 7)
#define UI_ID_FAKE_USER     (1 << 8)
#define UI_ID_PIN           (1 << 9)
#define UI_ID_BROWSE_RENDER (1 << 10)
#define UI_ID_PREVIEWS      (1 << 11)
#define UI_ID_FULL          (UI_ID_RENAME | UI_ID_BROWSE | UI_ID_ADD_NEW | UI_ID_OPEN | UI_ID_ALONE | UI_ID_DELETE | UI_ID_LOCAL)

int UI_icon_from_id(struct ID *id);
int UI_icon_from_report_type(int type);

uiBut *uiDefPulldownBut(uiBlock *block, uiBlockCreateFunc func, void *arg, const char *str, int x, int y, short width, short height, const char *tip);
uiBut *uiDefMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, const char *str, int x, int y, short width, short height, const char *tip);
uiBut *uiDefIconTextMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, int icon, const char *str, int x, int y, short width, short height, const char *tip);
uiBut *uiDefIconMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, int icon, int x, int y, short width, short height, const char *tip);

uiBut *uiDefBlockBut(uiBlock *block, uiBlockCreateFunc func, void *func_arg1, const char *str, int x, int y, short width, short height, const char *tip);
uiBut *uiDefBlockButN(uiBlock *block, uiBlockCreateFunc func, void *argN, const char *str, int x, int y, short width, short height, const char *tip);

uiBut *uiDefIconBlockBut(uiBlock *block, uiBlockCreateFunc func, void *arg, int retval, int icon, int x, int y, short width, short height, const char *tip);
uiBut *uiDefIconTextBlockBut(uiBlock *block, uiBlockCreateFunc func, void *arg, int icon, const char *str, int x, int y, short width, short height, const char *tip);

uiBut *uiDefKeyevtButS(uiBlock *block, int retval, const char *str, int x, int y, short width, short height, short *spoin, const char *tip);
uiBut *uiDefHotKeyevtButS(uiBlock *block, int retval, const char *str, int x, int y, short width, short height, short *keypoin, short *modkeypoin, const char *tip);

uiBut *uiDefSearchBut(uiBlock *block, void *arg, int retval, int icon, int maxlen, int x, int y, short width, short height, float a1, float a2, const char *tip);
uiBut *uiDefSearchButO_ptr(uiBlock *block, struct wmOperatorType *ot, IDProperty *properties,
                           void *arg, int retval, int icon, int maxlen, int x, int y,
                           short width, short height, float a1, float a2, const char *tip);

uiBut *uiDefAutoButR(uiBlock *block, struct PointerRNA *ptr, struct PropertyRNA *prop, int index, const char *name, int icon, int x1, int y1, int x2, int y2);
int uiDefAutoButsRNA(uiLayout *layout, struct PointerRNA *ptr, bool (*check_prop)(struct PointerRNA *, struct PropertyRNA *), const char label_align);

/* Links
 *
 * Game engine logic brick links. Non-functional currently in 2.5,
 * code to handle and draw these is disabled internally. */

void UI_but_link_set(struct uiBut *but,  void **poin,  void ***ppoin,  short *tot,  int from, int to);

void UI_block_links_compose(uiBlock *block);
uiBut *UI_block_links_find_inlink(uiBlock *block, void *poin);

/* use inside searchfunc to add items */
bool    UI_search_item_add(uiSearchItems *items, const char *name, void *poin, int iconid);
/* bfunc gets search item *poin as arg2, or if NULL the old string */
void    UI_but_func_search_set(uiBut *but,        uiButSearchFunc sfunc, void *arg1, uiButHandleFunc bfunc, void *active);
/* height in pixels, it's using hardcoded values still */
int     UI_searchbox_size_y(void);
int     UI_searchbox_size_x(void);
/* check if a string is in an existing search box */
int     UI_search_items_find_index(uiSearchItems *items, const char *name);

void    UI_block_func_handle_set(uiBlock *block, uiBlockHandleFunc func, void *arg);
void    UI_block_func_butmenu_set(uiBlock *block, uiMenuHandleFunc func, void *arg);
void    UI_block_func_set(uiBlock *block, uiButHandleFunc func, void *arg1, void *arg2);
void    UI_block_funcN_set(uiBlock *block, uiButHandleNFunc funcN, void *argN, void *arg2);

void    UI_but_func_rename_set(uiBut *but, uiButHandleRenameFunc func, void *arg1);
void    UI_but_func_set(uiBut *but, uiButHandleFunc func, void *arg1, void *arg2);
void    UI_but_funcN_set(uiBut *but, uiButHandleNFunc funcN, void *argN, void *arg2);

void    UI_but_func_complete_set(uiBut *but, uiButCompleteFunc func, void *arg);

void    UI_but_func_drawextra_set(
        uiBlock *block,
        void (*func)(const struct bContext *C, void *, void *, void *, struct rcti *rect),
        void *arg1, void *arg2);

bool UI_textbutton_activate_rna(const struct bContext *C, struct ARegion *ar,
                                const void *rna_poin_data, const char *rna_prop_id);
bool UI_textbutton_activate_but(const struct bContext *C, uiBut *but);

void UI_but_focus_on_enter_event(struct wmWindow *win, uiBut *but);

/* Autocomplete
 *
 * Tab complete helper functions, for use in uiButCompleteFunc callbacks.
 * Call begin once, then multiple times do_name with all possibilities,
 * and finally end to finish and get the completed name. */

typedef struct AutoComplete AutoComplete;

#define AUTOCOMPLETE_NO_MATCH 0
#define AUTOCOMPLETE_FULL_MATCH 1
#define AUTOCOMPLETE_PARTIAL_MATCH 2

AutoComplete *UI_autocomplete_begin(const char *startname, size_t maxlen);
void UI_autocomplete_update_name(AutoComplete *autocpl, const char *name);
int UI_autocomplete_end(AutoComplete *autocpl, char *autoname);

/* Panels
 *
 * Functions for creating, freeing and drawing panels. The API here
 * could use a good cleanup, though how they will function in 2.5 is
 * not clear yet so we postpone that. */

void UI_panels_begin(const struct bContext *C, struct ARegion *ar);
void UI_panels_end(const struct bContext *C, struct ARegion *ar, int *x, int *y);
void UI_panels_draw(const struct bContext *C, struct ARegion *ar);

struct Panel *UI_panel_find_by_type(struct ARegion *ar, struct PanelType *pt);
struct Panel *UI_panel_begin(struct ScrArea *sa, struct ARegion *ar, uiBlock *block,
                             struct PanelType *pt, struct Panel *pa, bool *r_open);
void UI_panel_end(uiBlock *block, int width, int height);
void UI_panels_scale(struct ARegion *ar, float new_width);

bool                       UI_panel_category_is_visible(struct ARegion *ar);
void                       UI_panel_category_add(struct ARegion *ar, const char *name);
struct PanelCategoryDyn   *UI_panel_category_find(struct ARegion *ar, const char *idname);
struct PanelCategoryStack *UI_panel_category_active_find(struct ARegion *ar, const char *idname);
const char                *UI_panel_category_active_get(struct ARegion *ar, bool set_fallback);
void                       UI_panel_category_active_set(struct ARegion *ar, const char *idname);
struct PanelCategoryDyn   *UI_panel_category_find_mouse_over_ex(struct ARegion *ar, const int x, const int y);
struct PanelCategoryDyn   *UI_panel_category_find_mouse_over(struct ARegion *ar, const struct wmEvent *event);
void                       UI_panel_category_clear_all(struct ARegion *ar);
void                       UI_panel_category_draw_all(struct ARegion *ar, const char *category_id_active);

/* Handlers
 *
 * Handlers that can be registered in regions, areas and windows for
 * handling WM events. Mostly this is done automatic by modules such
 * as screen/ if ED_KEYMAP_UI is set, or internally in popup functions. */

void UI_region_handlers_add(struct ListBase *handlers);
void UI_popup_handlers_add(struct bContext *C, struct ListBase *handlers, uiPopupBlockHandle *popup, const bool accept_dbl_click);
void UI_popup_handlers_remove(struct ListBase *handlers, uiPopupBlockHandle *popup);
void UI_popup_handlers_remove_all(struct bContext *C, struct ListBase *handlers);

/* Module
 *
 * init and exit should be called before using this module. init_userdef must
 * be used to reinitialize some internal state if user preferences change. */

void UI_init(void);
void UI_init_userdef(void);
void UI_reinit_font(void);
void UI_exit(void);

/* Layout
 *
 * More automated layout of buttons. Has three levels:
 * - Layout: contains a number templates, within a bounded width or height.
 * - Template: predefined layouts for buttons with a number of slots, each
 *   slot can contain multiple items.
 * - Item: item to put in a template slot, being either an RNA property,
 *   operator, label or menu. Also regular buttons can be used when setting
 *   uiBlockCurLayout. */

/* layout */
#define UI_LAYOUT_HORIZONTAL    0
#define UI_LAYOUT_VERTICAL      1

#define UI_LAYOUT_PANEL         0
#define UI_LAYOUT_HEADER        1
#define UI_LAYOUT_MENU          2
#define UI_LAYOUT_TOOLBAR       3
#define UI_LAYOUT_PIEMENU       4

#define UI_UNIT_X               ((void)0, U.widget_unit)
#define UI_UNIT_Y               ((void)0, U.widget_unit)

#define UI_LAYOUT_ALIGN_EXPAND  0
#define UI_LAYOUT_ALIGN_LEFT    1
#define UI_LAYOUT_ALIGN_CENTER  2
#define UI_LAYOUT_ALIGN_RIGHT   3

#define UI_ITEM_O_RETURN_PROPS  (1 << 0)
#define UI_ITEM_R_EXPAND        (1 << 1)
#define UI_ITEM_R_SLIDER        (1 << 2)
#define UI_ITEM_R_TOGGLE        (1 << 3)
#define UI_ITEM_R_ICON_ONLY     (1 << 4)
#define UI_ITEM_R_EVENT         (1 << 5)
#define UI_ITEM_R_FULL_EVENT    (1 << 6)
#define UI_ITEM_R_NO_BG         (1 << 7)
#define UI_ITEM_R_IMMEDIATE     (1 << 8)

/* uiLayoutOperatorButs flags */
#define UI_LAYOUT_OP_SHOW_TITLE 1
#define UI_LAYOUT_OP_SHOW_EMPTY 2

/* used for transp checkers */
#define UI_ALPHA_CHECKER_DARK 100
#define UI_ALPHA_CHECKER_LIGHT 160

/* flags to set which corners will become rounded:
 *
 * 1------2
 * |      |
 * 8------4 */

enum {
	UI_CNR_TOP_LEFT     = (1 << 0),
	UI_CNR_TOP_RIGHT    = (1 << 1),
	UI_CNR_BOTTOM_RIGHT = (1 << 2),
	UI_CNR_BOTTOM_LEFT  = (1 << 3),
	/* just for convenience */
	UI_CNR_NONE         = 0,
	UI_CNR_ALL          = (UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT)
};

/* not apart of the corner flags but mixed in some functions  */
#define UI_RB_ALPHA (UI_CNR_ALL + 1)

uiLayout *UI_block_layout(uiBlock *block, int dir, int type, int x, int y, int size, int em, int padding, struct uiStyle *style);
void UI_block_layout_set_current(uiBlock *block, uiLayout *layout);
void UI_block_layout_resolve(uiBlock *block, int *x, int *y);

uiBlock *uiLayoutGetBlock(uiLayout *layout);

void uiLayoutSetFunc(uiLayout *layout, uiMenuHandleFunc handlefunc, void *argv);
void uiLayoutSetContextPointer(uiLayout *layout, const char *name, struct PointerRNA *ptr);
void uiLayoutContextCopy(uiLayout *layout, struct bContextStore *context);
const char *uiLayoutIntrospect(uiLayout *layout); // XXX - testing
void uiLayoutOperatorButs(const struct bContext *C, struct uiLayout *layout, struct wmOperator *op,
                          bool (*check_prop)(struct PointerRNA *, struct PropertyRNA *),
                          const char label_align, const short flag);
struct MenuType *UI_but_menutype_get(uiBut *but);

void uiLayoutSetOperatorContext(uiLayout *layout, int opcontext);
void uiLayoutSetActive(uiLayout *layout, bool active);
void uiLayoutSetEnabled(uiLayout *layout, bool enabled);
void uiLayoutSetRedAlert(uiLayout *layout, bool redalert);
void uiLayoutSetAlignment(uiLayout *layout, char alignment);
void uiLayoutSetKeepAspect(uiLayout *layout, bool keepaspect);
void uiLayoutSetScaleX(uiLayout *layout, float scale);
void uiLayoutSetScaleY(uiLayout *layout, float scale);

int uiLayoutGetOperatorContext(uiLayout *layout);
bool uiLayoutGetActive(uiLayout *layout);
bool uiLayoutGetEnabled(uiLayout *layout);
bool uiLayoutGetRedAlert(uiLayout *layout);
int uiLayoutGetAlignment(uiLayout *layout);
bool uiLayoutGetKeepAspect(uiLayout *layout);
int uiLayoutGetWidth(uiLayout *layout);
float uiLayoutGetScaleX(uiLayout *layout);
float uiLayoutGetScaleY(uiLayout *layout);

/* layout specifiers */
uiLayout *uiLayoutRow(uiLayout *layout, int align);
uiLayout *uiLayoutColumn(uiLayout *layout, int align);
uiLayout *uiLayoutColumnFlow(uiLayout *layout, int number, int align);
uiLayout *uiLayoutBox(uiLayout *layout);
uiLayout *uiLayoutListBox(uiLayout *layout, struct uiList *ui_list, struct PointerRNA *ptr, struct PropertyRNA *prop,
                          struct PointerRNA *actptr, struct PropertyRNA *actprop);
uiLayout *uiLayoutAbsolute(uiLayout *layout, int align);
uiLayout *uiLayoutSplit(uiLayout *layout, float percentage, int align);
uiLayout *uiLayoutOverlap(uiLayout *layout);
uiBlock *uiLayoutAbsoluteBlock(uiLayout *layout);
uiLayout *uiLayoutRadial(uiLayout *layout);

/* templates */
void uiTemplateHeader(uiLayout *layout, struct bContext *C);
void uiTemplateID(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname,
                  const char *newop, const char *openop, const char *unlinkop);
void uiTemplateIDBrowse(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname,
                        const char *newop, const char *openop, const char *unlinkop);
void uiTemplateIDPreview(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname,
                         const char *newop, const char *openop, const char *unlinkop, int rows, int cols);
void uiTemplateAnyID(uiLayout *layout, struct PointerRNA *ptr, const char *propname, 
                     const char *proptypename, const char *text);
void uiTemplatePathBuilder(uiLayout *layout, struct PointerRNA *ptr, const char *propname, 
                           struct PointerRNA *root_ptr, const char *text);
uiLayout *uiTemplateModifier(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr);
uiLayout *uiTemplateConstraint(uiLayout *layout, struct PointerRNA *ptr);
void uiTemplatePreview(uiLayout *layout, struct bContext *C, struct ID *id, int show_buttons, struct ID *parent,
                       struct MTex *slot, const char *preview_id);
void uiTemplateColorRamp(uiLayout *layout, struct PointerRNA *ptr, const char *propname, int expand);
void uiTemplateIconView(uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiTemplateHistogram(uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiTemplateWaveform(uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiTemplateVectorscope(uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiTemplateCurveMapping(uiLayout *layout, struct PointerRNA *ptr, const char *propname, int type,
                            int levels, int brush, int neg_slope);
void uiTemplateColorPicker(uiLayout *layout, struct PointerRNA *ptr, const char *propname, int value_slider, int lock, int lock_luminosity, int cubic);
void uiTemplatePalette(uiLayout *layout, struct PointerRNA *ptr, const char *propname, int color);
void uiTemplateLayers(uiLayout *layout, struct PointerRNA *ptr, const char *propname,
                      PointerRNA *used_ptr, const char *used_propname, int active_layer);
void uiTemplateGameStates(uiLayout *layout, struct PointerRNA *ptr, const char *propname,
                      PointerRNA *used_ptr, const char *used_propname, int active_state);
void uiTemplateImage(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname, struct PointerRNA *userptr, int compact);
void uiTemplateImageSettings(uiLayout *layout, struct PointerRNA *imfptr, int color_management);
void uiTemplateImageLayers(uiLayout *layout, struct bContext *C, struct Image *ima, struct ImageUser *iuser);
void uiTemplateRunningJobs(uiLayout *layout, struct bContext *C);
void UI_but_func_operator_search(uiBut *but);
void uiTemplateOperatorSearch(uiLayout *layout);
void uiTemplateHeader3D(uiLayout *layout, struct bContext *C);
void uiTemplateEditModeSelection(uiLayout *layout, struct bContext *C);
void uiTemplateReportsBanner(uiLayout *layout, struct bContext *C);
void uiTemplateKeymapItemProperties(uiLayout *layout, struct PointerRNA *ptr);
void uiTemplateComponentMenu(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *name);
void uiTemplateNodeSocket(uiLayout *layout, struct bContext *C, float *color);

/* Default UIList class name, keep in sync with its declaration in bl_ui/__init__.py */
#define UI_UL_DEFAULT_CLASS_NAME "UI_UL_list"
void uiTemplateList(uiLayout *layout, struct bContext *C, const char *listtype_name, const char *list_id,
                    struct PointerRNA *dataptr, const char *propname, struct PointerRNA *active_dataptr,
                    const char *active_propname, int rows, int maxrows, int layout_type, int columns);
void uiTemplateNodeLink(uiLayout *layout, struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *input);
void uiTemplateNodeView(uiLayout *layout, struct bContext *C, struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *input);
void uiTemplateTextureUser(uiLayout *layout, struct bContext *C);
void uiTemplateTextureShow(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop);

void uiTemplateMovieClip(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname, int compact);
void uiTemplateTrack(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiTemplateMarker(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname, PointerRNA *userptr, PointerRNA *trackptr, int cmpact);
void uiTemplateMovieclipInformation(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname, struct PointerRNA *userptr);

void uiTemplateColorspaceSettings(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiTemplateColormanagedViewSettings(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname);

/* items */
void uiItemO(uiLayout *layout, const char *name, int icon, const char *opname);
void uiItemEnumO_ptr(uiLayout *layout, struct wmOperatorType *ot, const char *name, int icon, const char *propname, int value);
void uiItemEnumO(uiLayout *layout, const char *opname, const char *name, int icon, const char *propname, int value);
void uiItemEnumO_value(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, int value);
void uiItemEnumO_string(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, const char *value);
void uiItemsEnumO(uiLayout *layout, const char *opname, const char *propname);
void uiItemBooleanO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, int value);
void uiItemIntO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, int value);
void uiItemFloatO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, float value);
void uiItemStringO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, const char *value);

PointerRNA uiItemFullO_ptr(uiLayout *layout, struct wmOperatorType *ot, const char *name, int icon, IDProperty *properties, int context, int flag);
PointerRNA uiItemFullO(uiLayout *layout, const char *idname, const char *name, int icon, struct IDProperty *properties, int context, int flag);

void uiItemR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, int flag, const char *name, int icon);
void uiItemFullR(uiLayout *layout, struct PointerRNA *ptr, struct PropertyRNA *prop, int index, int value, int flag, const char *name, int icon);
void uiItemEnumR(uiLayout *layout, const char *name, int icon, struct PointerRNA *ptr, const char *propname, int value);
void uiItemEnumR_string(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *value, const char *name, int icon);
void uiItemsEnumR(uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiItemPointerR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, struct PointerRNA *searchptr, const char *searchpropname, const char *name, int icon);
void uiItemsFullEnumO(uiLayout *layout, const char *opname, const char *propname, struct IDProperty *properties, int context, int flag);

void uiItemL(uiLayout *layout, const char *name, int icon); /* label */
void uiItemLDrag(uiLayout *layout, struct PointerRNA *ptr, const char *name, int icon); /* label icon for dragging */
void uiItemM(uiLayout *layout, struct bContext *C, const char *menuname, const char *name, int icon); /* menu */
void uiItemV(uiLayout *layout, const char *name, int icon, int argval); /* value */
void uiItemS(uiLayout *layout); /* separator */

void uiItemMenuF(uiLayout *layout, const char *name, int icon, uiMenuCreateFunc func, void *arg);
void uiItemMenuEnumO(uiLayout *layout, struct bContext *C, const char *opname, const char *propname, const char *name, int icon);
void uiItemMenuEnumR_prop(uiLayout *layout, struct PointerRNA *ptr, PropertyRNA *prop, const char *name, int icon);
void uiItemMenuEnumR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *name, int icon);

/* UI Operators */
typedef struct uiDragColorHandle {
	float color[3];
	bool gamma_corrected;
} uiDragColorHandle;

void ED_button_operatortypes(void);
void UI_drop_color_copy(struct wmDrag *drag, struct wmDropBox *drop);
int UI_drop_color_poll(struct bContext *C, struct wmDrag *drag, const struct wmEvent *event);

/* Helpers for Operators */
uiBut *UI_context_active_but_get(const struct bContext *C);
void UI_context_active_but_prop_get(const struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA **prop, int *index);
void UI_context_active_but_prop_handle(struct bContext *C);
struct wmOperator *UI_context_active_operator_get(const struct bContext *C);
void UI_context_update_anim_flag(const struct bContext *C);
void UI_context_active_but_prop_get_filebrowser(const struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA **prop);
void UI_context_active_but_prop_get_templateID(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA **prop);

/* Styled text draw */
void UI_fontstyle_set(struct uiFontStyle *fs);
void UI_fontstyle_draw_ex(
        struct uiFontStyle *fs, const struct rcti *rect, const char *str,
        size_t len, float *r_xofs, float *r_yofs);
void UI_fontstyle_draw(struct uiFontStyle *fs, const struct rcti *rect, const char *str);
void UI_fontstyle_draw_rotated(struct uiFontStyle *fs, const struct rcti *rect, const char *str);

int UI_fontstyle_string_width(const char *str); // XXX temp
void UI_draw_string(float x, float y, const char *str); // XXX temp
void UI_draw_icon_tri(float x, float y, char dir);

uiStyle *UI_style_get(void);		/* use for fonts etc */
uiStyle *UI_style_get_dpi(void);	/* DPI scaled settings for drawing */

/* linker workaround ack! */
void UI_template_fix_linking(void);

/* UI_OT_editsource helpers */
bool UI_editsource_enable_check(void);
void UI_editsource_active_but_test(uiBut *but);

/* UI_butstore_ helpers */
typedef struct uiButStore uiButStore;
typedef struct uiButStoreElem uiButStoreElem;

uiButStore *UI_butstore_create(uiBlock *block);
void UI_butstore_clear(uiBlock *block);
void UI_butstore_update(uiBlock *block);
void UI_butstore_free(uiBlock *block, uiButStore *bs);
bool UI_butstore_is_valid(uiButStore *bs);
bool UI_butstore_is_registered(uiBlock *block, uiBut *but);
void UI_butstore_register(uiButStore *bs_handle, uiBut **but_p);
bool UI_butstore_register_update(uiBlock *block, uiBut *but_dst, const uiBut *but_src);
void UI_butstore_unregister(uiButStore *bs_handle, uiBut **but_p);


/* Float precision helpers */
#define UI_PRECISION_FLOAT_MAX 7

int UI_calc_float_precision(int prec, double value);

#endif  /* __UI_INTERFACE_H__ */
