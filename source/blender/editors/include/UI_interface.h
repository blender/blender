/*
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
 */

/** \file
 * \ingroup editorui
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h" /* size_t */
#include "UI_interface_icons.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Struct Declarations */

struct ARegion;
struct AutoComplete;
struct EnumPropertyItem;
struct FileSelectParams;
struct ID;
struct IDProperty;
struct ImBuf;
struct Image;
struct ImageUser;
struct ListBase;
struct MTex;
struct Panel;
struct PanelType;
struct PointerRNA;
struct PropertyRNA;
struct ReportList;
struct ResultBLF;
struct bContext;
struct bContextStore;
struct bNode;
struct bNodeSocket;
struct bNodeTree;
struct bScreen;
struct rctf;
struct rcti;
struct uiButSearch;
struct uiFontStyle;
struct uiList;
struct uiStyle;
struct uiWidgetColors;
struct wmDrag;
struct wmDropBox;
struct wmEvent;
struct wmGizmo;
struct wmKeyConfig;
struct wmKeyMap;
struct wmKeyMapItem;
struct wmMsgBus;
struct wmOperator;
struct wmOperatorType;
struct wmWindow;

typedef struct uiBlock uiBlock;
typedef struct uiBut uiBut;
typedef struct uiLayout uiLayout;
typedef struct uiPopupBlockHandle uiPopupBlockHandle;

/* Defines */

/* char for splitting strings, aligning shortcuts in menus, users never see */
#define UI_SEP_CHAR '|'
#define UI_SEP_CHAR_S "|"

/* names */
#define UI_MAX_DRAW_STR 400
#define UI_MAX_NAME_STR 128
#define UI_MAX_SHORTCUT_STR 64

/**
 * For #ARegion.overlap regions, pass events though if they don't overlap
 * the regions contents (the usable part of the #View2D and buttons).
 *
 * The margin is needed so it's not possible to accidentally click in between buttons.
 */
#define UI_REGION_OVERLAP_MARGIN (U.widget_unit / 3)

/* use for clamping popups within the screen */
#define UI_SCREEN_MARGIN 10

/** #uiBlock.emboss and #uiBut.emboss */
typedef enum eUIEmbossType {
  UI_EMBOSS = 0,          /* use widget style for drawing */
  UI_EMBOSS_NONE = 1,     /* Nothing, only icon and/or text */
  UI_EMBOSS_PULLDOWN = 2, /* Pull-down menu style */
  UI_EMBOSS_RADIAL = 3,   /* Pie Menu */
  /**
   * The same as #UI_EMBOSS_NONE, unless the button has
   * a coloring status like an animation state or red alert.
   */
  UI_EMBOSS_NONE_OR_STATUS = 4,

  UI_EMBOSS_UNDEFINED = 255, /* For layout engine, use emboss from block. */
} eUIEmbossType;

/* uiBlock->direction */
enum {
  UI_DIR_UP = 1 << 0,
  UI_DIR_DOWN = 1 << 1,
  UI_DIR_LEFT = 1 << 2,
  UI_DIR_RIGHT = 1 << 3,
  UI_DIR_CENTER_X = 1 << 4,
  UI_DIR_CENTER_Y = 1 << 5,

  UI_DIR_ALL = UI_DIR_UP | UI_DIR_DOWN | UI_DIR_LEFT | UI_DIR_RIGHT,
};

#if 0
/* uiBlock->autofill (not yet used) */
#  define UI_BLOCK_COLLUMNS 1
#  define UI_BLOCK_ROWS 2
#endif

/** #uiBlock.flag (controls) */
enum {
  UI_BLOCK_LOOP = 1 << 0,
  UI_BLOCK_IS_FLIP = 1 << 1,
  UI_BLOCK_NO_FLIP = 1 << 2,
  UI_BLOCK_NUMSELECT = 1 << 3,
  /** Don't apply window clipping. */
  UI_BLOCK_NO_WIN_CLIP = 1 << 4,
  UI_BLOCK_CLIPBOTTOM = 1 << 5,
  UI_BLOCK_CLIPTOP = 1 << 6,
  UI_BLOCK_MOVEMOUSE_QUIT = 1 << 7,
  UI_BLOCK_KEEP_OPEN = 1 << 8,
  UI_BLOCK_POPUP = 1 << 9,
  UI_BLOCK_OUT_1 = 1 << 10,
  UI_BLOCK_SEARCH_MENU = 1 << 11,
  UI_BLOCK_POPUP_MEMORY = 1 << 12,
  /* Stop handling mouse events. */
  UI_BLOCK_CLIP_EVENTS = 1 << 13,

  /* block->flag bits 14-17 are identical to but->drawflag bits */

  UI_BLOCK_POPUP_HOLD = 1 << 18,
  UI_BLOCK_LIST_ITEM = 1 << 19,
  UI_BLOCK_RADIAL = 1 << 20,
  UI_BLOCK_POPOVER = 1 << 21,
  UI_BLOCK_POPOVER_ONCE = 1 << 22,
  /** Always show key-maps, even for non-menus. */
  UI_BLOCK_SHOW_SHORTCUT_ALWAYS = 1 << 23,
  /** The block is only used during the search process and will not be drawn.
   * Currently just for the case of a closed panel's sub-panel (and its sub-panels). */
  UI_BLOCK_SEARCH_ONLY = 1 << 25,
  /** Hack for quick setup (splash screen) to draw text centered. */
  UI_BLOCK_QUICK_SETUP = 1 << 26,
};

/** #uiPopupBlockHandle.menuretval */
enum {
  /** Cancel all menus cascading. */
  UI_RETURN_CANCEL = 1 << 0,
  /** Choice made. */
  UI_RETURN_OK = 1 << 1,
  /** Left the menu. */
  UI_RETURN_OUT = 1 << 2,
  /** Let the parent handle this event. */
  UI_RETURN_OUT_PARENT = 1 << 3,
  /** Update the button that opened. */
  UI_RETURN_UPDATE = 1 << 4,
  /** Popup is ok to be handled. */
  UI_RETURN_POPUP_OK = 1 << 5,
};

/* but->flag - general state flags. */
enum {
  /** Warning, the first 6 flags are internal. */
  UI_BUT_ICON_SUBMENU = 1 << 6,
  UI_BUT_ICON_PREVIEW = 1 << 7,

  UI_BUT_NODE_LINK = 1 << 8,
  UI_BUT_NODE_ACTIVE = 1 << 9,
  UI_BUT_DRAG_LOCK = 1 << 10,
  /** Grayed out and un-editable. */
  UI_BUT_DISABLED = 1 << 11,

  UI_BUT_ANIMATED = 1 << 13,
  UI_BUT_ANIMATED_KEY = 1 << 14,
  UI_BUT_DRIVEN = 1 << 15,
  UI_BUT_REDALERT = 1 << 16,
  /** Grayed out but still editable. */
  UI_BUT_INACTIVE = 1 << 17,
  UI_BUT_LAST_ACTIVE = 1 << 18,
  UI_BUT_UNDO = 1 << 19,
  UI_BUT_IMMEDIATE = 1 << 20,
  UI_BUT_NO_UTF8 = 1 << 21,

  /** For popups, pressing return activates this button, overriding the highlighted button.
   * For non-popups this is just used as a display hint for the user to let them
   * know the action which is activated when pressing return (file selector for eg). */
  UI_BUT_ACTIVE_DEFAULT = 1 << 23,

  /** This but is "inside" a list item (currently used to change theme colors). */
  UI_BUT_LIST_ITEM = 1 << 24,
  /** edit this button as well as the active button (not just dragging) */
  UI_BUT_DRAG_MULTI = 1 << 25,
  /** Use for popups to start editing the button on initialization. */
  UI_BUT_ACTIVATE_ON_INIT = 1 << 26,

  /** #uiBut.str contains #UI_SEP_CHAR, used for key shortcuts */
  UI_BUT_HAS_SEP_CHAR = 1 << 27,
  /** Don't run updates while dragging (needed in rare cases). */
  UI_BUT_UPDATE_DELAY = 1 << 28,
  /** When widget is in textedit mode, update value on each char stroke */
  UI_BUT_TEXTEDIT_UPDATE = 1 << 29,
  /** Show 'x' icon to clear/unlink value of text or search button. */
  UI_BUT_VALUE_CLEAR = 1 << 30,

  /** RNA property of the button is overridden from linked reference data. */
  UI_BUT_OVERRIDDEN = 1u << 31u,
};

/* Default font size for normal text. */
#define UI_DEFAULT_TEXT_POINTS 11

/* Larger size used for title text. */
#define UI_DEFAULT_TITLE_POINTS 12

#define UI_PANEL_WIDTH 340
#define UI_COMPACT_PANEL_WIDTH 160
#define UI_SIDEBAR_PANEL_WIDTH 220
#define UI_NAVIGATION_REGION_WIDTH UI_COMPACT_PANEL_WIDTH
#define UI_NARROW_NAVIGATION_REGION_WIDTH 100

#define UI_PANEL_CATEGORY_MARGIN_WIDTH (U.widget_unit * 1.0f)

#define UI_PANEL_BOX_STYLE_MARGIN (U.widget_unit * 0.2f)

/* but->drawflag - these flags should only affect how the button is drawn. */
/* Note: currently, these flags _are not passed_ to the widget's state() or draw() functions
 *       (except for the 'align' ones)!
 */
enum {
  /** Text and icon alignment (by default, they are centered). */
  UI_BUT_TEXT_LEFT = 1 << 1,
  UI_BUT_ICON_LEFT = 1 << 2,
  UI_BUT_TEXT_RIGHT = 1 << 3,
  /** Prevent the button to show any tooltip. */
  UI_BUT_NO_TOOLTIP = 1 << 4,
  /** Do not add the usual horizontal padding for text drawing. */
  UI_BUT_NO_TEXT_PADDING = 1 << 5,

  /* Button align flag, for drawing groups together.
   * Used in 'uiBlock.flag', take care! */
  UI_BUT_ALIGN_TOP = 1 << 14,
  UI_BUT_ALIGN_LEFT = 1 << 15,
  UI_BUT_ALIGN_RIGHT = 1 << 16,
  UI_BUT_ALIGN_DOWN = 1 << 17,
  UI_BUT_ALIGN = UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_LEFT | UI_BUT_ALIGN_RIGHT | UI_BUT_ALIGN_DOWN,
  /* end bits shared with 'uiBlock.flag' */

  /**
   * Warning - HACK!
   * Needed for buttons which are not TOP/LEFT aligned,
   * but have some top/left corner stitched to some other TOP/LEFT-aligned button,
   * because of 'corrective' hack in widget_roundbox_set()... */
  UI_BUT_ALIGN_STITCH_TOP = 1 << 18,
  UI_BUT_ALIGN_STITCH_LEFT = 1 << 19,
  UI_BUT_ALIGN_ALL = UI_BUT_ALIGN | UI_BUT_ALIGN_STITCH_TOP | UI_BUT_ALIGN_STITCH_LEFT,

  /** This but is "inside" a box item (currently used to change theme colors). */
  UI_BUT_BOX_ITEM = 1 << 20,

  /** Active left part of number button */
  UI_BUT_ACTIVE_LEFT = 1 << 21,
  /** Active right part of number button */
  UI_BUT_ACTIVE_RIGHT = 1 << 22,

  /* (also used by search buttons to enforce shortcut display for their items). */
  /** Button has shortcut text. */
  UI_BUT_HAS_SHORTCUT = 1 << 23,

  /** Reverse order of consecutive off/on icons */
  UI_BUT_ICON_REVERSE = 1 << 24,

  /** Value is animated, but the current value differs from the animated one. */
  UI_BUT_ANIMATED_CHANGED = 1 << 25,

  /* Draw the checkbox buttons inverted. */
  UI_BUT_CHECKBOX_INVERT = 1 << 26,
};

/* scale fixed button widths by this to account for DPI */

#define UI_DPI_FAC (U.dpi_fac)
/* 16 to copy ICON_DEFAULT_HEIGHT */
#define UI_DPI_ICON_SIZE ((float)16 * UI_DPI_FAC)

/**
 * Button types, bits stored in 1 value... and a short even!
 * - bits 0-4:  #uiBut.bitnr (0-31)
 * - bits 5-7:  pointer type
 * - bit  8:    for 'bit'
 * - bit  9-15: button type (now 6 bits, 64 types)
 */
typedef enum {
  UI_BUT_POIN_CHAR = 32,
  UI_BUT_POIN_SHORT = 64,
  UI_BUT_POIN_INT = 96,
  UI_BUT_POIN_FLOAT = 128,
  /*  UI_BUT_POIN_FUNCTION = 192, */ /*UNUSED*/
  UI_BUT_POIN_BIT = 256,             /* OR'd with a bit index*/
} eButPointerType;

/* requires (but->poin != NULL) */
#define UI_BUT_POIN_TYPES (UI_BUT_POIN_FLOAT | UI_BUT_POIN_SHORT | UI_BUT_POIN_CHAR)

/* assigned to but->type, OR'd with the flags above when passing args */
typedef enum {
  UI_BTYPE_BUT = 1 << 9,
  UI_BTYPE_ROW = 2 << 9,
  UI_BTYPE_TEXT = 3 << 9,
  /** Drop-down list. */
  UI_BTYPE_MENU = 4 << 9,
  UI_BTYPE_BUT_MENU = 5 << 9,
  /** number button */
  UI_BTYPE_NUM = 6 << 9,
  /** number slider */
  UI_BTYPE_NUM_SLIDER = 7 << 9,
  UI_BTYPE_TOGGLE = 8 << 9,
  UI_BTYPE_TOGGLE_N = 9 << 9,
  UI_BTYPE_ICON_TOGGLE = 10 << 9,
  UI_BTYPE_ICON_TOGGLE_N = 11 << 9,
  /** same as regular toggle, but no on/off state displayed */
  UI_BTYPE_BUT_TOGGLE = 12 << 9,
  /** similar to toggle, display a 'tick' */
  UI_BTYPE_CHECKBOX = 13 << 9,
  UI_BTYPE_CHECKBOX_N = 14 << 9,
  UI_BTYPE_COLOR = 15 << 9,
  UI_BTYPE_TAB = 16 << 9,
  UI_BTYPE_POPOVER = 17 << 9,
  UI_BTYPE_SCROLL = 18 << 9,
  UI_BTYPE_BLOCK = 19 << 9,
  UI_BTYPE_LABEL = 20 << 9,
  UI_BTYPE_KEY_EVENT = 24 << 9,
  UI_BTYPE_HSVCUBE = 26 << 9,
  /** menu (often used in headers), **_MENU /w different draw-type */
  UI_BTYPE_PULLDOWN = 27 << 9,
  UI_BTYPE_ROUNDBOX = 28 << 9,
  UI_BTYPE_COLORBAND = 30 << 9,
  /** sphere widget (used to input a unit-vector, aka normal) */
  UI_BTYPE_UNITVEC = 31 << 9,
  UI_BTYPE_CURVE = 32 << 9,
  /** Profile editing widget */
  UI_BTYPE_CURVEPROFILE = 33 << 9,
  UI_BTYPE_LISTBOX = 36 << 9,
  UI_BTYPE_LISTROW = 37 << 9,
  UI_BTYPE_HSVCIRCLE = 38 << 9,
  UI_BTYPE_TRACK_PREVIEW = 40 << 9,

  /** Buttons with value >= #UI_BTYPE_SEARCH_MENU don't get undo pushes. */
  UI_BTYPE_SEARCH_MENU = 41 << 9,
  UI_BTYPE_EXTRA = 42 << 9,
  UI_BTYPE_HOTKEY_EVENT = 46 << 9,
  /** Non-interactive image, used for splash screen */
  UI_BTYPE_IMAGE = 47 << 9,
  UI_BTYPE_HISTOGRAM = 48 << 9,
  UI_BTYPE_WAVEFORM = 49 << 9,
  UI_BTYPE_VECTORSCOPE = 50 << 9,
  UI_BTYPE_PROGRESS_BAR = 51 << 9,
  UI_BTYPE_NODE_SOCKET = 53 << 9,
  UI_BTYPE_SEPR = 54 << 9,
  UI_BTYPE_SEPR_LINE = 55 << 9,
  /** Dynamically fill available space. */
  UI_BTYPE_SEPR_SPACER = 56 << 9,
  /** Resize handle (resize uilist). */
  UI_BTYPE_GRIP = 57 << 9,
  UI_BTYPE_DECORATOR = 58 << 9,
} eButType;

#define BUTTYPE (63 << 9)

/** Gradient types, for color picker #UI_BTYPE_HSVCUBE etc. */
typedef enum eButGradientType {
  UI_GRAD_SV = 0,
  UI_GRAD_HV = 1,
  UI_GRAD_HS = 2,
  UI_GRAD_H = 3,
  UI_GRAD_S = 4,
  UI_GRAD_V = 5,

  UI_GRAD_V_ALT = 9,
  UI_GRAD_L_ALT = 10,
} eButGradientType;

/* Drawing
 *
 * Functions to draw various shapes, taking theme settings into account.
 * Used for code that draws its own UI style elements. */

void UI_draw_anti_tria(
    float x1, float y1, float x2, float y2, float x3, float y3, const float color[4]);
void UI_draw_anti_fan(float tri_array[][2], unsigned int length, const float color[4]);

void UI_draw_roundbox_corner_set(int type);
void UI_draw_roundbox_aa(const struct rctf *rect, bool filled, float rad, const float color[4]);
void UI_draw_roundbox_4fv(const struct rctf *rect, bool filled, float rad, const float col[4]);
void UI_draw_roundbox_3ub_alpha(const struct rctf *rect,
                                bool filled,
                                float rad,
                                const unsigned char col[3],
                                unsigned char alpha);
void UI_draw_roundbox_3fv_alpha(
    const struct rctf *rect, bool filled, float rad, const float col[3], float alpha);
void UI_draw_roundbox_shade_x(const struct rctf *rect,
                              bool filled,
                              float rad,
                              float shadetop,
                              float shadedown,
                              const float col[4]);
void UI_draw_roundbox_4fv_ex(const struct rctf *rect,
                             const float inner1[4],
                             const float inner2[4],
                             float shade_dir,
                             const float outline[4],
                             float outline_width,
                             float rad);

#if 0 /* unused */
int UI_draw_roundbox_corner_get(void);
#endif

void UI_draw_box_shadow(const struct rctf *rect, unsigned char alpha);
void UI_draw_text_underline(int pos_x, int pos_y, int len, int height, const float color[4]);

void UI_draw_safe_areas(uint pos,
                        const struct rctf *rect,
                        const float title_aspect[2],
                        const float action_aspect[2]);

/** State for scrolldrawing. */
enum {
  UI_SCROLL_PRESSED = 1 << 0,
  UI_SCROLL_ARROWS = 1 << 1,
  UI_SCROLL_NO_OUTLINE = 1 << 2,
};
void UI_draw_widget_scroll(struct uiWidgetColors *wcol,
                           const struct rcti *rect,
                           const struct rcti *slider,
                           int state);

/* Shortening string helper. */
float UI_text_clip_middle_ex(const struct uiFontStyle *fstyle,
                             char *str,
                             float okwidth,
                             const float minwidth,
                             const size_t max_len,
                             const char rpart_sep);

/**
 * Callbacks
 *
 * UI_block_func_handle_set/ButmFunc are for handling events through a callback.
 * HandleFunc gets the retval passed on, and ButmFunc gets a2. The latter is
 * mostly for compatibility with older code.
 *
 * - #UI_but_func_complete_set is for tab completion.
 *
 * - #uiButSearchFunc is for name buttons, showing a popup with matches
 *
 * - #UI_block_func_set and UI_but_func_set are callbacks run when a button is used,
 *   in case events, operators or RNA are not sufficient to handle the button.
 *
 * - #UI_but_funcN_set will free the argument with MEM_freeN. */

typedef struct uiSearchItems uiSearchItems;

typedef void (*uiButHandleFunc)(struct bContext *C, void *arg1, void *arg2);
typedef void (*uiButHandleRenameFunc)(struct bContext *C, void *arg, char *origstr);
typedef void (*uiButHandleNFunc)(struct bContext *C, void *argN, void *arg2);
typedef void (*uiButHandleHoldFunc)(struct bContext *C, struct ARegion *butregion, uiBut *but);
typedef int (*uiButCompleteFunc)(struct bContext *C, char *str, void *arg);

/* Search types. */
typedef struct ARegion *(*uiButSearchCreateFn)(struct bContext *C,
                                               struct ARegion *butregion,
                                               struct uiButSearch *search_but);
/* `is_first` is typically used to ignore search filtering when the menu is first opened in order
 * to display the full list of options. The value will be false after the button's text is edited
 * (for every call except the first). */
typedef void (*uiButSearchUpdateFn)(const struct bContext *C,
                                    void *arg,
                                    const char *str,
                                    uiSearchItems *items,
                                    const bool is_first);
typedef void (*uiButSearchArgFreeFn)(void *arg);
typedef bool (*uiButSearchContextMenuFn)(struct bContext *C,
                                         void *arg,
                                         void *active,
                                         const struct wmEvent *event);
typedef struct ARegion *(*uiButSearchTooltipFn)(struct bContext *C,
                                                struct ARegion *region,
                                                const struct rcti *item_rect,
                                                void *arg,
                                                void *active);

/* Must return allocated string. */
typedef char *(*uiButToolTipFunc)(struct bContext *C, void *argN, const char *tip);
typedef int (*uiButPushedStateFunc)(struct bContext *C, void *arg);

typedef void (*uiBlockHandleFunc)(struct bContext *C, void *arg, int event);

/* Menu Callbacks */

typedef void (*uiMenuCreateFunc)(struct bContext *C, struct uiLayout *layout, void *arg1);
typedef void (*uiMenuHandleFunc)(struct bContext *C, void *arg, int event);
/**
 * Used for cycling menu values without opening the menu (Ctrl-Wheel).
 * \param direction: forward or backwards [1 / -1].
 * \param arg1: uiBut.poin (as with #uiMenuCreateFunc).
 * \return true when the button was changed.
 */
typedef bool (*uiMenuStepFunc)(struct bContext *C, int direction, void *arg1);

/* interface_query.c */
bool UI_but_has_tooltip_label(const uiBut *but);
bool UI_but_is_tool(const uiBut *but);
bool UI_but_is_utf8(const uiBut *but);
#define UI_but_is_decorator(but) ((but)->type == UI_BTYPE_DECORATOR)

bool UI_block_is_empty_ex(const uiBlock *block, const bool skip_title);
bool UI_block_is_empty(const uiBlock *block);
bool UI_block_can_add_separator(const uiBlock *block);

/* interface_region_menu_popup.c */
/**
 * Popup Menus
 *
 * Functions used to create popup menus. For more extended menus the
 * UI_popup_menu_begin/End functions can be used to define own items with
 * the uiItem functions in between. If it is a simple confirmation menu
 * or similar, popups can be created with a single function call.
 */
typedef struct uiPopupMenu uiPopupMenu;

uiPopupMenu *UI_popup_menu_begin(struct bContext *C, const char *title, int icon) ATTR_NONNULL();
uiPopupMenu *UI_popup_menu_begin_ex(struct bContext *C,
                                    const char *title,
                                    const char *block_name,
                                    int icon) ATTR_NONNULL();
void UI_popup_menu_end(struct bContext *C, struct uiPopupMenu *pup);
bool UI_popup_menu_end_or_cancel(struct bContext *C, struct uiPopupMenu *head);
struct uiLayout *UI_popup_menu_layout(uiPopupMenu *pup);

void UI_popup_menu_reports(struct bContext *C, struct ReportList *reports) ATTR_NONNULL();
int UI_popup_menu_invoke(struct bContext *C, const char *idname, struct ReportList *reports)
    ATTR_NONNULL(1, 2);

void UI_popup_menu_retval_set(const uiBlock *block, const int retval, const bool enable);
void UI_popup_menu_but_set(uiPopupMenu *pup, struct ARegion *butregion, uiBut *but);

/* interface_region_popover.c */

typedef struct uiPopover uiPopover;

int UI_popover_panel_invoke(struct bContext *C,
                            const char *idname,
                            bool keep_open,
                            struct ReportList *reports);

uiPopover *UI_popover_begin(struct bContext *C, int menu_width, bool from_active_button)
    ATTR_NONNULL(1);
void UI_popover_end(struct bContext *C, struct uiPopover *pup, struct wmKeyMap *keymap);
struct uiLayout *UI_popover_layout(uiPopover *pup);
void UI_popover_once_clear(uiPopover *pup);

/* interface_region_menu_pie.c */
/* Pie menus */
typedef struct uiPieMenu uiPieMenu;

int UI_pie_menu_invoke(struct bContext *C, const char *idname, const struct wmEvent *event);
int UI_pie_menu_invoke_from_operator_enum(struct bContext *C,
                                          const char *title,
                                          const char *opname,
                                          const char *propname,
                                          const struct wmEvent *event);
int UI_pie_menu_invoke_from_rna_enum(struct bContext *C,
                                     const char *title,
                                     const char *path,
                                     const struct wmEvent *event);

struct uiPieMenu *UI_pie_menu_begin(struct bContext *C,
                                    const char *title,
                                    int icon,
                                    const struct wmEvent *event) ATTR_NONNULL();
void UI_pie_menu_end(struct bContext *C, uiPieMenu *pie);
struct uiLayout *UI_pie_menu_layout(struct uiPieMenu *pie);

/* interface_region_menu_popup.c */

/* Popup Blocks
 *
 * Functions used to create popup blocks. These are like popup menus
 * but allow using all button types and creating an own layout. */
typedef uiBlock *(*uiBlockCreateFunc)(struct bContext *C, struct ARegion *region, void *arg1);
typedef void (*uiBlockCancelFunc)(struct bContext *C, void *arg1);

void UI_popup_block_invoke(struct bContext *C,
                           uiBlockCreateFunc func,
                           void *arg,
                           void (*arg_free)(void *arg));
void UI_popup_block_invoke_ex(struct bContext *C,
                              uiBlockCreateFunc func,
                              void *arg,
                              void (*arg_free)(void *arg),
                              bool can_refresh);
void UI_popup_block_ex(struct bContext *C,
                       uiBlockCreateFunc func,
                       uiBlockHandleFunc popup_func,
                       uiBlockCancelFunc cancel_func,
                       void *arg,
                       struct wmOperator *op);
#if 0 /* UNUSED */
void uiPupBlockOperator(struct bContext *C,
                        uiBlockCreateFunc func,
                        struct wmOperator *op,
                        int opcontext);
#endif

void UI_popup_block_close(struct bContext *C, struct wmWindow *win, uiBlock *block);

bool UI_popup_block_name_exists(const struct bScreen *screen, const char *name);

/* Blocks
 *
 * Functions for creating, drawing and freeing blocks. A Block is a
 * container of buttons and used for various purposes.
 *
 * Begin/Define Buttons/End/Draw is the typical order in which these
 * function should be called, though for popup blocks Draw is left out.
 * Freeing blocks is done by the screen/ module automatically.
 */

uiBlock *UI_block_begin(const struct bContext *C,
                        struct ARegion *region,
                        const char *name,
                        eUIEmbossType emboss);
void UI_block_end_ex(const struct bContext *C, uiBlock *block, const int xy[2], int r_xy[2]);
void UI_block_end(const struct bContext *C, uiBlock *block);
void UI_block_draw(const struct bContext *C, struct uiBlock *block);
void UI_blocklist_update_window_matrix(const struct bContext *C, const struct ListBase *lb);
void UI_blocklist_draw(const struct bContext *C, const struct ListBase *lb);
void UI_block_update_from_old(const struct bContext *C, struct uiBlock *block);

enum {
  UI_BLOCK_THEME_STYLE_REGULAR = 0,
  UI_BLOCK_THEME_STYLE_POPUP = 1,
};
void UI_block_theme_style_set(uiBlock *block, char theme_style);
char UI_block_emboss_get(uiBlock *block);
void UI_block_emboss_set(uiBlock *block, eUIEmbossType emboss);
bool UI_block_is_search_only(const uiBlock *block);
void UI_block_set_search_only(uiBlock *block, bool search_only);

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
void UI_block_bounds_set_popup(uiBlock *block, int addval, const int bounds_offset[2]);
void UI_block_bounds_set_menu(uiBlock *block, int addval, const int bounds_offset[2]);
void UI_block_bounds_set_centered(uiBlock *block, int addval);
void UI_block_bounds_set_explicit(uiBlock *block, int minx, int miny, int maxx, int maxy);

int UI_blocklist_min_y_get(struct ListBase *lb);

void UI_block_direction_set(uiBlock *block, char direction);
void UI_block_order_flip(uiBlock *block);
void UI_block_flag_enable(uiBlock *block, int flag);
void UI_block_flag_disable(uiBlock *block, int flag);
void UI_block_translate(uiBlock *block, int x, int y);

int UI_but_return_value_get(uiBut *but);

void UI_but_drag_set_id(uiBut *but, struct ID *id);
void UI_but_drag_set_asset(uiBut *but,
                           const char *name,
                           const char *path,
                           int id_type,
                           int icon,
                           struct ImBuf *imb,
                           float scale);
void UI_but_drag_set_rna(uiBut *but, struct PointerRNA *ptr);
void UI_but_drag_set_path(uiBut *but, const char *path, const bool use_free);
void UI_but_drag_set_name(uiBut *but, const char *name);
void UI_but_drag_set_value(uiBut *but);
void UI_but_drag_set_image(
    uiBut *but, const char *path, int icon, struct ImBuf *imb, float scale, const bool use_free);

bool UI_but_active_drop_name(struct bContext *C);
bool UI_but_active_drop_color(struct bContext *C);

void UI_but_flag_enable(uiBut *but, int flag);
void UI_but_flag_disable(uiBut *but, int flag);
bool UI_but_flag_is_set(uiBut *but, int flag);

void UI_but_drawflag_enable(uiBut *but, int flag);
void UI_but_drawflag_disable(uiBut *but, int flag);

void UI_but_disable(uiBut *but, const char *disabled_hint);

void UI_but_type_set_menu_from_pulldown(uiBut *but);

/* special button case, only draw it when used actively, for outliner etc */
bool UI_but_active_only_ex(const struct bContext *C,
                           struct ARegion *region,
                           uiBlock *block,
                           uiBut *but,
                           const bool remove_on_failure);
bool UI_but_active_only(const struct bContext *C,
                        struct ARegion *region,
                        uiBlock *block,
                        uiBut *but);
bool UI_block_active_only_flagged_buttons(const struct bContext *C,
                                          struct ARegion *region,
                                          struct uiBlock *block);

void UI_but_execute(const struct bContext *C, struct ARegion *region, uiBut *but);

bool UI_but_online_manual_id(const uiBut *but,
                             char *r_str,
                             size_t maxlength) ATTR_WARN_UNUSED_RESULT;
bool UI_but_online_manual_id_from_active(const struct bContext *C,
                                         char *r_str,
                                         size_t maxlength) ATTR_WARN_UNUSED_RESULT;
bool UI_but_is_userdef(const uiBut *but);

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
                int type,
                int retval,
                const char *str,
                int x,
                int y,
                short width,
                short height,
                void *poin,
                float min,
                float max,
                float a1,
                float a2,
                const char *tip);
uiBut *uiDefButF(uiBlock *block,
                 int type,
                 int retval,
                 const char *str,
                 int x,
                 int y,
                 short width,
                 short height,
                 float *poin,
                 float min,
                 float max,
                 float a1,
                 float a2,
                 const char *tip);
uiBut *uiDefButBitF(uiBlock *block,
                    int type,
                    int bit,
                    int retval,
                    const char *str,
                    int x,
                    int y,
                    short width,
                    short height,
                    float *poin,
                    float min,
                    float max,
                    float a1,
                    float a2,
                    const char *tip);
uiBut *uiDefButI(uiBlock *block,
                 int type,
                 int retval,
                 const char *str,
                 int x,
                 int y,
                 short width,
                 short height,
                 int *poin,
                 float min,
                 float max,
                 float a1,
                 float a2,
                 const char *tip);
uiBut *uiDefButBitI(uiBlock *block,
                    int type,
                    int bit,
                    int retval,
                    const char *str,
                    int x,
                    int y,
                    short width,
                    short height,
                    int *poin,
                    float min,
                    float max,
                    float a1,
                    float a2,
                    const char *tip);
uiBut *uiDefButS(uiBlock *block,
                 int type,
                 int retval,
                 const char *str,
                 int x,
                 int y,
                 short width,
                 short height,
                 short *poin,
                 float min,
                 float max,
                 float a1,
                 float a2,
                 const char *tip);
uiBut *uiDefButBitS(uiBlock *block,
                    int type,
                    int bit,
                    int retval,
                    const char *str,
                    int x,
                    int y,
                    short width,
                    short height,
                    short *poin,
                    float min,
                    float max,
                    float a1,
                    float a2,
                    const char *tip);
uiBut *uiDefButC(uiBlock *block,
                 int type,
                 int retval,
                 const char *str,
                 int x,
                 int y,
                 short width,
                 short height,
                 char *poin,
                 float min,
                 float max,
                 float a1,
                 float a2,
                 const char *tip);
uiBut *uiDefButBitC(uiBlock *block,
                    int type,
                    int bit,
                    int retval,
                    const char *str,
                    int x,
                    int y,
                    short width,
                    short height,
                    char *poin,
                    float min,
                    float max,
                    float a1,
                    float a2,
                    const char *tip);
uiBut *uiDefButR(uiBlock *block,
                 int type,
                 int retval,
                 const char *str,
                 int x,
                 int y,
                 short width,
                 short height,
                 struct PointerRNA *ptr,
                 const char *propname,
                 int index,
                 float min,
                 float max,
                 float a1,
                 float a2,
                 const char *tip);
uiBut *uiDefButR_prop(uiBlock *block,
                      int type,
                      int retval,
                      const char *str,
                      int x,
                      int y,
                      short width,
                      short height,
                      struct PointerRNA *ptr,
                      struct PropertyRNA *prop,
                      int index,
                      float min,
                      float max,
                      float a1,
                      float a2,
                      const char *tip);
uiBut *uiDefButO(uiBlock *block,
                 int type,
                 const char *opname,
                 int opcontext,
                 const char *str,
                 int x,
                 int y,
                 short width,
                 short height,
                 const char *tip);
uiBut *uiDefButO_ptr(uiBlock *block,
                     int type,
                     struct wmOperatorType *ot,
                     int opcontext,
                     const char *str,
                     int x,
                     int y,
                     short width,
                     short height,
                     const char *tip);

uiBut *uiDefIconBut(uiBlock *block,
                    int type,
                    int retval,
                    int icon,
                    int x,
                    int y,
                    short width,
                    short height,
                    void *poin,
                    float min,
                    float max,
                    float a1,
                    float a2,
                    const char *tip);
uiBut *uiDefIconButF(uiBlock *block,
                     int type,
                     int retval,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     float *poin,
                     float min,
                     float max,
                     float a1,
                     float a2,
                     const char *tip);
uiBut *uiDefIconButBitF(uiBlock *block,
                        int type,
                        int bit,
                        int retval,
                        int icon,
                        int x,
                        int y,
                        short width,
                        short height,
                        float *poin,
                        float min,
                        float max,
                        float a1,
                        float a2,
                        const char *tip);
uiBut *uiDefIconButI(uiBlock *block,
                     int type,
                     int retval,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     int *poin,
                     float min,
                     float max,
                     float a1,
                     float a2,
                     const char *tip);
uiBut *uiDefIconButBitI(uiBlock *block,
                        int type,
                        int bit,
                        int retval,
                        int icon,
                        int x,
                        int y,
                        short width,
                        short height,
                        int *poin,
                        float min,
                        float max,
                        float a1,
                        float a2,
                        const char *tip);
uiBut *uiDefIconButS(uiBlock *block,
                     int type,
                     int retval,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     short *poin,
                     float min,
                     float max,
                     float a1,
                     float a2,
                     const char *tip);
uiBut *uiDefIconButBitS(uiBlock *block,
                        int type,
                        int bit,
                        int retval,
                        int icon,
                        int x,
                        int y,
                        short width,
                        short height,
                        short *poin,
                        float min,
                        float max,
                        float a1,
                        float a2,
                        const char *tip);
uiBut *uiDefIconButC(uiBlock *block,
                     int type,
                     int retval,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     char *poin,
                     float min,
                     float max,
                     float a1,
                     float a2,
                     const char *tip);
uiBut *uiDefIconButBitC(uiBlock *block,
                        int type,
                        int bit,
                        int retval,
                        int icon,
                        int x,
                        int y,
                        short width,
                        short height,
                        char *poin,
                        float min,
                        float max,
                        float a1,
                        float a2,
                        const char *tip);
uiBut *uiDefIconButR(uiBlock *block,
                     int type,
                     int retval,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     struct PointerRNA *ptr,
                     const char *propname,
                     int index,
                     float min,
                     float max,
                     float a1,
                     float a2,
                     const char *tip);
uiBut *uiDefIconButR_prop(uiBlock *block,
                          int type,
                          int retval,
                          int icon,
                          int x,
                          int y,
                          short width,
                          short height,
                          struct PointerRNA *ptr,
                          struct PropertyRNA *prop,
                          int index,
                          float min,
                          float max,
                          float a1,
                          float a2,
                          const char *tip);
uiBut *uiDefIconButO(uiBlock *block,
                     int type,
                     const char *opname,
                     int opcontext,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     const char *tip);
uiBut *uiDefIconButO_ptr(uiBlock *block,
                         int type,
                         struct wmOperatorType *ot,
                         int opcontext,
                         int icon,
                         int x,
                         int y,
                         short width,
                         short height,
                         const char *tip);
uiBut *uiDefButImage(
    uiBlock *block, void *imbuf, int x, int y, short width, short height, const uchar color[4]);
uiBut *uiDefButAlert(uiBlock *block, int icon, int x, int y, short width, short height);
uiBut *uiDefIconTextBut(uiBlock *block,
                        int type,
                        int retval,
                        int icon,
                        const char *str,
                        int x,
                        int y,
                        short width,
                        short height,
                        void *poin,
                        float min,
                        float max,
                        float a1,
                        float a2,
                        const char *tip);
uiBut *uiDefIconTextButF(uiBlock *block,
                         int type,
                         int retval,
                         int icon,
                         const char *str,
                         int x,
                         int y,
                         short width,
                         short height,
                         float *poin,
                         float min,
                         float max,
                         float a1,
                         float a2,
                         const char *tip);
uiBut *uiDefIconTextButBitF(uiBlock *block,
                            int type,
                            int bit,
                            int retval,
                            int icon,
                            const char *str,
                            int x,
                            int y,
                            short width,
                            short height,
                            float *poin,
                            float min,
                            float max,
                            float a1,
                            float a2,
                            const char *tip);
uiBut *uiDefIconTextButI(uiBlock *block,
                         int type,
                         int retval,
                         int icon,
                         const char *str,
                         int x,
                         int y,
                         short width,
                         short height,
                         int *poin,
                         float min,
                         float max,
                         float a1,
                         float a2,
                         const char *tip);
uiBut *uiDefIconTextButBitI(uiBlock *block,
                            int type,
                            int bit,
                            int retval,
                            int icon,
                            const char *str,
                            int x,
                            int y,
                            short width,
                            short height,
                            int *poin,
                            float min,
                            float max,
                            float a1,
                            float a2,
                            const char *tip);
uiBut *uiDefIconTextButS(uiBlock *block,
                         int type,
                         int retval,
                         int icon,
                         const char *str,
                         int x,
                         int y,
                         short width,
                         short height,
                         short *poin,
                         float min,
                         float max,
                         float a1,
                         float a2,
                         const char *tip);
uiBut *uiDefIconTextButBitS(uiBlock *block,
                            int type,
                            int bit,
                            int retval,
                            int icon,
                            const char *str,
                            int x,
                            int y,
                            short width,
                            short height,
                            short *poin,
                            float min,
                            float max,
                            float a1,
                            float a2,
                            const char *tip);
uiBut *uiDefIconTextButC(uiBlock *block,
                         int type,
                         int retval,
                         int icon,
                         const char *str,
                         int x,
                         int y,
                         short width,
                         short height,
                         char *poin,
                         float min,
                         float max,
                         float a1,
                         float a2,
                         const char *tip);
uiBut *uiDefIconTextButBitC(uiBlock *block,
                            int type,
                            int bit,
                            int retval,
                            int icon,
                            const char *str,
                            int x,
                            int y,
                            short width,
                            short height,
                            char *poin,
                            float min,
                            float max,
                            float a1,
                            float a2,
                            const char *tip);
uiBut *uiDefIconTextButR(uiBlock *block,
                         int type,
                         int retval,
                         int icon,
                         const char *str,
                         int x,
                         int y,
                         short width,
                         short height,
                         struct PointerRNA *ptr,
                         const char *propname,
                         int index,
                         float min,
                         float max,
                         float a1,
                         float a2,
                         const char *tip);
uiBut *uiDefIconTextButR_prop(uiBlock *block,
                              int type,
                              int retval,
                              int icon,
                              const char *str,
                              int x,
                              int y,
                              short width,
                              short height,
                              struct PointerRNA *ptr,
                              struct PropertyRNA *prop,
                              int index,
                              float min,
                              float max,
                              float a1,
                              float a2,
                              const char *tip);
uiBut *uiDefIconTextButO(uiBlock *block,
                         int type,
                         const char *opname,
                         int opcontext,
                         int icon,
                         const char *str,
                         int x,
                         int y,
                         short width,
                         short height,
                         const char *tip);
uiBut *uiDefIconTextButO_ptr(uiBlock *block,
                             int type,
                             struct wmOperatorType *ot,
                             int opcontext,
                             int icon,
                             const char *str,
                             int x,
                             int y,
                             short width,
                             short height,
                             const char *tip);

/* for passing inputs to ButO buttons */
struct PointerRNA *UI_but_operator_ptr_get(uiBut *but);

void UI_but_unit_type_set(uiBut *but, const int unit_type);
int UI_but_unit_type_get(const uiBut *but);

typedef enum uiStringInfoType {
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
  BUT_GET_PROP_KEYMAP,
} uiStringInfoType;

typedef struct uiStringInfo {
  uiStringInfoType type;
  char *strinfo;
} uiStringInfo;

/* Note: Expects pointers to uiStringInfo structs as parameters.
 *       Will fill them with translated strings, when possible.
 *       Strings in uiStringInfo must be MEM_freeN'ed by caller. */
void UI_but_string_info_get(struct bContext *C, uiBut *but, ...) ATTR_SENTINEL(0);

/* Edit i18n stuff. */
/* Name of the main py op from i18n addon. */
#define EDTSRC_I18N_OP_NAME "UI_OT_edittranslation"

/**
 * Special Buttons
 *
 * Buttons with a more specific purpose:
 * - MenuBut: buttons that popup a menu (in headers usually).
 * - PulldownBut: like MenuBut, but creating a uiBlock (for compatibility).
 * - BlockBut: buttons that popup a block with more buttons.
 * - KeyevtBut: buttons that can be used to turn key events into values.
 * - PickerButtons: buttons like the color picker (for code sharing).
 * - AutoButR: RNA property button with type automatically defined.
 */
enum {
  UI_ID_RENAME = 1 << 0,
  UI_ID_BROWSE = 1 << 1,
  UI_ID_ADD_NEW = 1 << 2,
  UI_ID_ALONE = 1 << 4,
  UI_ID_OPEN = 1 << 3,
  UI_ID_DELETE = 1 << 5,
  UI_ID_LOCAL = 1 << 6,
  UI_ID_AUTO_NAME = 1 << 7,
  UI_ID_FAKE_USER = 1 << 8,
  UI_ID_PIN = 1 << 9,
  UI_ID_PREVIEWS = 1 << 10,
  UI_ID_OVERRIDE = 1 << 11,
  UI_ID_FULL = UI_ID_RENAME | UI_ID_BROWSE | UI_ID_ADD_NEW | UI_ID_OPEN | UI_ID_ALONE |
               UI_ID_DELETE | UI_ID_LOCAL,
};

/**
 * Ways to limit what is displayed in ID-search popup.
 * \note We may want to add LOCAL, LIBRARY ... as needed.
 */
enum {
  UI_TEMPLATE_ID_FILTER_ALL = 0,
  UI_TEMPLATE_ID_FILTER_AVAILABLE = 1,
};

int UI_icon_from_id(const struct ID *id);
int UI_icon_from_report_type(int type);
int UI_icon_colorid_from_report_type(int type);
int UI_text_colorid_from_report_type(int type);

int UI_icon_from_event_type(short event_type, short event_value);
int UI_icon_from_keymap_item(const struct wmKeyMapItem *kmi, int r_icon_mod[4]);

uiBut *uiDefPulldownBut(uiBlock *block,
                        uiBlockCreateFunc func,
                        void *arg,
                        const char *str,
                        int x,
                        int y,
                        short width,
                        short height,
                        const char *tip);
uiBut *uiDefMenuBut(uiBlock *block,
                    uiMenuCreateFunc func,
                    void *arg,
                    const char *str,
                    int x,
                    int y,
                    short width,
                    short height,
                    const char *tip);
uiBut *uiDefIconTextMenuBut(uiBlock *block,
                            uiMenuCreateFunc func,
                            void *arg,
                            int icon,
                            const char *str,
                            int x,
                            int y,
                            short width,
                            short height,
                            const char *tip);
uiBut *uiDefIconMenuBut(uiBlock *block,
                        uiMenuCreateFunc func,
                        void *arg,
                        int icon,
                        int x,
                        int y,
                        short width,
                        short height,
                        const char *tip);

uiBut *uiDefBlockBut(uiBlock *block,
                     uiBlockCreateFunc func,
                     void *arg,
                     const char *str,
                     int x,
                     int y,
                     short width,
                     short height,
                     const char *tip);
uiBut *uiDefBlockButN(uiBlock *block,
                      uiBlockCreateFunc func,
                      void *argN,
                      const char *str,
                      int x,
                      int y,
                      short width,
                      short height,
                      const char *tip);

uiBut *uiDefIconBlockBut(uiBlock *block,
                         uiBlockCreateFunc func,
                         void *arg,
                         int retval,
                         int icon,
                         int x,
                         int y,
                         short width,
                         short height,
                         const char *tip);
uiBut *uiDefIconTextBlockBut(uiBlock *block,
                             uiBlockCreateFunc func,
                             void *arg,
                             int icon,
                             const char *str,
                             int x,
                             int y,
                             short width,
                             short height,
                             const char *tip);

uiBut *uiDefKeyevtButS(uiBlock *block,
                       int retval,
                       const char *str,
                       int x,
                       int y,
                       short width,
                       short height,
                       short *spoin,
                       const char *tip);
uiBut *uiDefHotKeyevtButS(uiBlock *block,
                          int retval,
                          const char *str,
                          int x,
                          int y,
                          short width,
                          short height,
                          short *keypoin,
                          const short *modkeypoin,
                          const char *tip);

uiBut *uiDefSearchBut(uiBlock *block,
                      void *arg,
                      int retval,
                      int icon,
                      int maxlen,
                      int x,
                      int y,
                      short width,
                      short height,
                      float a1,
                      float a2,
                      const char *tip);
uiBut *uiDefSearchButO_ptr(uiBlock *block,
                           struct wmOperatorType *ot,
                           struct IDProperty *properties,
                           void *arg,
                           int retval,
                           int icon,
                           int maxlen,
                           int x,
                           int y,
                           short width,
                           short height,
                           float a1,
                           float a2,
                           const char *tip);

/* For uiDefAutoButsRNA */
typedef enum {
  /* Keep current layout for aligning label with property button. */
  UI_BUT_LABEL_ALIGN_NONE,
  /* Align label and property button vertically. */
  UI_BUT_LABEL_ALIGN_COLUMN,
  /* Split layout into a column for the label and one for property button. */
  UI_BUT_LABEL_ALIGN_SPLIT_COLUMN,
} eButLabelAlign;

/* Return info for uiDefAutoButsRNA */
typedef enum {
  /* Returns when no buttons were added */
  UI_PROP_BUTS_NONE_ADDED = 1 << 0,
  /* Returned when any property failed the custom check callback (check_prop) */
  UI_PROP_BUTS_ANY_FAILED_CHECK = 1 << 1,
} eAutoPropButsReturn;

uiBut *uiDefAutoButR(uiBlock *block,
                     struct PointerRNA *ptr,
                     struct PropertyRNA *prop,
                     int index,
                     const char *name,
                     int icon,
                     int x1,
                     int y1,
                     int x2,
                     int y2);
eAutoPropButsReturn uiDefAutoButsRNA(uiLayout *layout,
                                     struct PointerRNA *ptr,
                                     bool (*check_prop)(struct PointerRNA *ptr,
                                                        struct PropertyRNA *prop,
                                                        void *user_data),
                                     void *user_data,
                                     struct PropertyRNA *prop_activate_init,
                                     eButLabelAlign label_align,
                                     const bool compact);

/* use inside searchfunc to add items */
bool UI_search_item_add(uiSearchItems *items,
                        const char *name,
                        void *poin,
                        int iconid,
                        int state,
                        const uint8_t name_prefix_offset);

void UI_but_func_search_set(uiBut *but,
                            uiButSearchCreateFn search_create_fn,
                            uiButSearchUpdateFn search_update_fn,
                            void *arg,
                            uiButSearchArgFreeFn search_arg_free_fn,
                            uiButHandleFunc search_exec_fn,
                            void *active);
void UI_but_func_search_set_context_menu(uiBut *but, uiButSearchContextMenuFn context_menu_fn);
void UI_but_func_search_set_tooltip(uiBut *but, uiButSearchTooltipFn tooltip_fn);
void UI_but_func_search_set_sep_string(uiBut *but, const char *search_sep_string);
void UI_but_func_search_set_results_are_suggestions(uiBut *but, const bool value);

/* height in pixels, it's using hardcoded values still */
int UI_searchbox_size_y(void);
int UI_searchbox_size_x(void);
/* check if a string is in an existing search box */
int UI_search_items_find_index(uiSearchItems *items, const char *name);

void UI_but_node_link_set(uiBut *but, struct bNodeSocket *socket, const float draw_color[4]);

void UI_but_number_step_size_set(uiBut *but, float step_size);
void UI_but_number_precision_set(uiBut *but, float precision);

void UI_block_func_handle_set(uiBlock *block, uiBlockHandleFunc func, void *arg);
void UI_block_func_butmenu_set(uiBlock *block, uiMenuHandleFunc func, void *arg);
void UI_block_func_set(uiBlock *block, uiButHandleFunc func, void *arg1, void *arg2);
void UI_block_funcN_set(uiBlock *block, uiButHandleNFunc funcN, void *argN, void *arg2);

void UI_but_func_rename_set(uiBut *but, uiButHandleRenameFunc func, void *arg1);
void UI_but_func_set(uiBut *but, uiButHandleFunc func, void *arg1, void *arg2);
void UI_but_funcN_set(uiBut *but, uiButHandleNFunc funcN, void *argN, void *arg2);

void UI_but_func_complete_set(uiBut *but, uiButCompleteFunc func, void *arg);

void UI_but_func_drawextra_set(
    uiBlock *block,
    void (*func)(const struct bContext *C, void *, void *, void *, struct rcti *rect),
    void *arg1,
    void *arg2);

void UI_but_func_menu_step_set(uiBut *but, uiMenuStepFunc func);

void UI_but_func_tooltip_set(uiBut *but, uiButToolTipFunc func, void *argN);
void UI_but_tooltip_refresh(struct bContext *C, uiBut *but);
void UI_but_tooltip_timer_remove(struct bContext *C, uiBut *but);

bool UI_textbutton_activate_rna(const struct bContext *C,
                                struct ARegion *region,
                                const void *rna_poin_data,
                                const char *rna_prop_id);
bool UI_textbutton_activate_but(const struct bContext *C, uiBut *but);

void UI_but_focus_on_enter_event(struct wmWindow *win, uiBut *but);

void UI_but_func_hold_set(uiBut *but, uiButHandleHoldFunc func, void *argN);

void UI_but_func_pushed_state_set(uiBut *but, uiButPushedStateFunc func, void *arg);

struct PointerRNA *UI_but_extra_operator_icon_add(uiBut *but,
                                                  const char *opname,
                                                  short opcontext,
                                                  int icon);

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

void UI_panels_begin(const struct bContext *C, struct ARegion *region);
void UI_panels_end(const struct bContext *C, struct ARegion *region, int *r_x, int *r_y);
void UI_panels_draw(const struct bContext *C, struct ARegion *region);

struct Panel *UI_panel_find_by_type(struct ListBase *lb, const struct PanelType *pt);
struct Panel *UI_panel_begin(struct ARegion *region,
                             struct ListBase *lb,
                             uiBlock *block,
                             struct PanelType *pt,
                             struct Panel *panel,
                             bool *r_open);
void UI_panel_header_buttons_begin(struct Panel *panel);
void UI_panel_header_buttons_end(struct Panel *panel);
void UI_panel_end(struct Panel *panel, int width, int height);

bool UI_panel_is_closed(const struct Panel *panel);
bool UI_panel_is_active(const struct Panel *panel);
void UI_panel_label_offset(const struct uiBlock *block, int *r_x, int *r_y);
int UI_panel_size_y(const struct Panel *panel);
bool UI_panel_is_dragging(const struct Panel *panel);
bool UI_panel_matches_search_filter(const struct Panel *panel);

bool UI_panel_category_is_visible(const struct ARegion *region);
void UI_panel_category_add(struct ARegion *region, const char *name);
struct PanelCategoryDyn *UI_panel_category_find(const struct ARegion *region, const char *idname);
struct PanelCategoryStack *UI_panel_category_active_find(struct ARegion *region,
                                                         const char *idname);
const char *UI_panel_category_active_get(struct ARegion *region, bool set_fallback);
void UI_panel_category_active_set(struct ARegion *region, const char *idname);
void UI_panel_category_active_set_default(struct ARegion *region, const char *idname);
void UI_panel_category_clear_all(struct ARegion *region);
void UI_panel_category_draw_all(struct ARegion *region, const char *category_id_active);

/* Panel custom data. */
struct PointerRNA *UI_panel_custom_data_get(const struct Panel *panel);
struct PointerRNA *UI_region_panel_custom_data_under_cursor(const struct bContext *C,
                                                            const struct wmEvent *event);
void UI_panel_custom_data_set(struct Panel *panel, struct PointerRNA *custom_data);

/* Polyinstantiated panels for representing a list of data. */
struct Panel *UI_panel_add_instanced(const struct bContext *C,
                                     struct ARegion *region,
                                     struct ListBase *panels,
                                     const char *panel_idname,
                                     struct PointerRNA *custom_data);
void UI_panels_free_instanced(const struct bContext *C, struct ARegion *region);

#define INSTANCED_PANEL_UNIQUE_STR_LEN 4
void UI_list_panel_unique_str(struct Panel *panel, char *r_name);

typedef void (*uiListPanelIDFromDataFunc)(void *data_link, char *r_idname);
bool UI_panel_list_matches_data(struct ARegion *region,
                                struct ListBase *data,
                                uiListPanelIDFromDataFunc panel_idname_func);

/* Handlers
 *
 * Handlers that can be registered in regions, areas and windows for
 * handling WM events. Mostly this is done automatic by modules such
 * as screen/ if ED_KEYMAP_UI is set, or internally in popup functions. */

void UI_region_handlers_add(struct ListBase *handlers);
void UI_popup_handlers_add(struct bContext *C,
                           struct ListBase *handlers,
                           uiPopupBlockHandle *popup,
                           const char flag);
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
enum {
  UI_LAYOUT_HORIZONTAL = 0,
  UI_LAYOUT_VERTICAL = 1,
};

enum {
  UI_LAYOUT_PANEL = 0,
  UI_LAYOUT_HEADER = 1,
  UI_LAYOUT_MENU = 2,
  UI_LAYOUT_TOOLBAR = 3,
  UI_LAYOUT_PIEMENU = 4,
  UI_LAYOUT_VERT_BAR = 5,
};

#define UI_UNIT_X ((void)0, U.widget_unit)
#define UI_UNIT_Y ((void)0, U.widget_unit)

enum {
  UI_LAYOUT_ALIGN_EXPAND = 0,
  UI_LAYOUT_ALIGN_LEFT = 1,
  UI_LAYOUT_ALIGN_CENTER = 2,
  UI_LAYOUT_ALIGN_RIGHT = 3,
};

enum {
  /* UI_ITEM_O_RETURN_PROPS = 1 << 0, */ /* UNUSED */
  UI_ITEM_R_EXPAND = 1 << 1,
  UI_ITEM_R_SLIDER = 1 << 2,
  /**
   * Use for booleans, causes the button to draw with an outline (emboss),
   * instead of text with a checkbox.
   * This is implied when toggle buttons have an icon
   * unless #UI_ITEM_R_ICON_NEVER flag is set.
   */
  UI_ITEM_R_TOGGLE = 1 << 3,
  /**
   * Don't attempt to use an icon when the icon is set to #ICON_NONE.
   *
   * Use for boolean's, causes the buttons to always show as a checkbox
   * even when there is an icon (which would normally show the button as a toggle).
   */
  UI_ITEM_R_ICON_NEVER = 1 << 4,
  UI_ITEM_R_ICON_ONLY = 1 << 5,
  UI_ITEM_R_EVENT = 1 << 6,
  UI_ITEM_R_FULL_EVENT = 1 << 7,
  UI_ITEM_R_NO_BG = 1 << 8,
  UI_ITEM_R_IMMEDIATE = 1 << 9,
  UI_ITEM_O_DEPRESS = 1 << 10,
  UI_ITEM_R_COMPACT = 1 << 11,
  UI_ITEM_R_CHECKBOX_INVERT = 1 << 12,
  /** Don't add a real decorator item, just blank space. */
  UI_ITEM_R_FORCE_BLANK_DECORATE = 1 << 13,
  /* Even create the property split layout if there's no name to show there. */
  UI_ITEM_R_SPLIT_EMPTY_NAME = 1 << 14,
};

#define UI_HEADER_OFFSET ((void)0, 0.4f * UI_UNIT_X)

/* uiLayoutOperatorButs flags */
enum {
  UI_TEMPLATE_OP_PROPS_SHOW_TITLE = 1 << 0,
  UI_TEMPLATE_OP_PROPS_SHOW_EMPTY = 1 << 1,
  UI_TEMPLATE_OP_PROPS_COMPACT = 1 << 2,
  UI_TEMPLATE_OP_PROPS_HIDE_ADVANCED = 1 << 3,
  /* Disable property split for the default layout (custom ui callbacks still have full control
   * over the layout and can enable it). */
  UI_TEMPLATE_OP_PROPS_NO_SPLIT_LAYOUT = 1 << 4,
};

/* used for transp checkers */
#define UI_ALPHA_CHECKER_DARK 100
#define UI_ALPHA_CHECKER_LIGHT 160

/* flags to set which corners will become rounded:
 *
 * 1------2
 * |      |
 * 8------4 */

enum {
  UI_CNR_TOP_LEFT = 1 << 0,
  UI_CNR_TOP_RIGHT = 1 << 1,
  UI_CNR_BOTTOM_RIGHT = 1 << 2,
  UI_CNR_BOTTOM_LEFT = 1 << 3,
  /* just for convenience */
  UI_CNR_NONE = 0,
  UI_CNR_ALL = (UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT),
};

uiLayout *UI_block_layout(uiBlock *block,
                          int dir,
                          int type,
                          int x,
                          int y,
                          int size,
                          int em,
                          int padding,
                          const struct uiStyle *style);
void UI_block_layout_set_current(uiBlock *block, uiLayout *layout);
void UI_block_layout_resolve(uiBlock *block, int *r_x, int *r_y);
void UI_block_layout_free(uiBlock *block);

bool UI_block_apply_search_filter(uiBlock *block, const char *search_filter);

void UI_region_message_subscribe(struct ARegion *region, struct wmMsgBus *mbus);

uiBlock *uiLayoutGetBlock(uiLayout *layout);

void uiLayoutSetFunc(uiLayout *layout, uiMenuHandleFunc handlefunc, void *argv);
void uiLayoutSetContextPointer(uiLayout *layout, const char *name, struct PointerRNA *ptr);
struct bContextStore *uiLayoutGetContextStore(uiLayout *layout);
void uiLayoutContextCopy(uiLayout *layout, struct bContextStore *context);
struct wmOperatorType *UI_but_operatortype_get_from_enum_menu(struct uiBut *but,
                                                              struct PropertyRNA **r_prop);
struct MenuType *UI_but_menutype_get(uiBut *but);
struct PanelType *UI_but_paneltype_get(uiBut *but);
void UI_menutype_draw(struct bContext *C, struct MenuType *mt, struct uiLayout *layout);
void UI_paneltype_draw(struct bContext *C, struct PanelType *pt, struct uiLayout *layout);

/* Only for convenience. */
void uiLayoutSetContextFromBut(uiLayout *layout, uiBut *but);

void uiLayoutSetOperatorContext(uiLayout *layout, int opcontext);
void uiLayoutSetActive(uiLayout *layout, bool active);
void uiLayoutSetActiveDefault(uiLayout *layout, bool active_default);
void uiLayoutSetActivateInit(uiLayout *layout, bool activate_init);
void uiLayoutSetEnabled(uiLayout *layout, bool enabled);
void uiLayoutSetRedAlert(uiLayout *layout, bool redalert);
void uiLayoutSetAlignment(uiLayout *layout, char alignment);
void uiLayoutSetFixedSize(uiLayout *layout, bool fixed_size);
void uiLayoutSetKeepAspect(uiLayout *layout, bool keepaspect);
void uiLayoutSetScaleX(uiLayout *layout, float scale);
void uiLayoutSetScaleY(uiLayout *layout, float scale);
void uiLayoutSetUnitsX(uiLayout *layout, float unit);
void uiLayoutSetUnitsY(uiLayout *layout, float unit);
void uiLayoutSetEmboss(uiLayout *layout, eUIEmbossType emboss);
void uiLayoutSetPropSep(uiLayout *layout, bool is_sep);
void uiLayoutSetPropDecorate(uiLayout *layout, bool is_sep);
int uiLayoutGetLocalDir(const uiLayout *layout);

int uiLayoutGetOperatorContext(uiLayout *layout);
bool uiLayoutGetActive(uiLayout *layout);
bool uiLayoutGetActiveDefault(uiLayout *layout);
bool uiLayoutGetActivateInit(uiLayout *layout);
bool uiLayoutGetEnabled(uiLayout *layout);
bool uiLayoutGetRedAlert(uiLayout *layout);
int uiLayoutGetAlignment(uiLayout *layout);
bool uiLayoutGetFixedSize(uiLayout *layout);
bool uiLayoutGetKeepAspect(uiLayout *layout);
int uiLayoutGetWidth(uiLayout *layout);
float uiLayoutGetScaleX(uiLayout *layout);
float uiLayoutGetScaleY(uiLayout *layout);
float uiLayoutGetUnitsX(uiLayout *layout);
float uiLayoutGetUnitsY(uiLayout *layout);
eUIEmbossType uiLayoutGetEmboss(uiLayout *layout);
bool uiLayoutGetPropSep(uiLayout *layout);
bool uiLayoutGetPropDecorate(uiLayout *layout);

/* layout specifiers */
uiLayout *uiLayoutRow(uiLayout *layout, bool align);
uiLayout *uiLayoutRowWithHeading(uiLayout *layout, bool align, const char *heading);
uiLayout *uiLayoutColumn(uiLayout *layout, bool align);
uiLayout *uiLayoutColumnWithHeading(uiLayout *layout, bool align, const char *heading);
uiLayout *uiLayoutColumnFlow(uiLayout *layout, int number, bool align);
uiLayout *uiLayoutGridFlow(uiLayout *layout,
                           bool row_major,
                           int columns_len,
                           bool even_columns,
                           bool even_rows,
                           bool align);
uiLayout *uiLayoutBox(uiLayout *layout);
uiLayout *uiLayoutListBox(uiLayout *layout,
                          struct uiList *ui_list,
                          struct PointerRNA *actptr,
                          struct PropertyRNA *actprop);
uiLayout *uiLayoutAbsolute(uiLayout *layout, bool align);
uiLayout *uiLayoutSplit(uiLayout *layout, float percentage, bool align);
uiLayout *uiLayoutOverlap(uiLayout *layout);
uiBlock *uiLayoutAbsoluteBlock(uiLayout *layout);
uiLayout *uiLayoutRadial(uiLayout *layout);

/* templates */
void uiTemplateHeader(uiLayout *layout, struct bContext *C);
void uiTemplateID(uiLayout *layout,
                  const struct bContext *C,
                  struct PointerRNA *ptr,
                  const char *propname,
                  const char *newop,
                  const char *openop,
                  const char *unlinkop,
                  int filter,
                  const bool live_icon,
                  const char *text);
void uiTemplateIDBrowse(uiLayout *layout,
                        struct bContext *C,
                        struct PointerRNA *ptr,
                        const char *propname,
                        const char *newop,
                        const char *openop,
                        const char *unlinkop,
                        int filter,
                        const char *text);
void uiTemplateIDPreview(uiLayout *layout,
                         struct bContext *C,
                         struct PointerRNA *ptr,
                         const char *propname,
                         const char *newop,
                         const char *openop,
                         const char *unlinkop,
                         int rows,
                         int cols,
                         int filter,
                         const bool hide_buttons);
void uiTemplateIDTabs(uiLayout *layout,
                      struct bContext *C,
                      struct PointerRNA *ptr,
                      const char *propname,
                      const char *newop,
                      const char *menu,
                      int filter);
void uiTemplateAnyID(uiLayout *layout,
                     struct PointerRNA *ptr,
                     const char *propname,
                     const char *proptypename,
                     const char *text);
void uiTemplateSearch(uiLayout *layout,
                      struct bContext *C,
                      struct PointerRNA *ptr,
                      const char *propname,
                      struct PointerRNA *searchptr,
                      const char *searchpropname,
                      const char *newop,
                      const char *unlinkop);
void uiTemplateSearchPreview(uiLayout *layout,
                             struct bContext *C,
                             struct PointerRNA *ptr,
                             const char *propname,
                             struct PointerRNA *searchptr,
                             const char *searchpropname,
                             const char *newop,
                             const char *unlinkop,
                             const int rows,
                             const int cols);
void uiTemplatePathBuilder(uiLayout *layout,
                           struct PointerRNA *ptr,
                           const char *propname,
                           struct PointerRNA *root_ptr,
                           const char *text);
void uiTemplateModifiers(uiLayout *layout, struct bContext *C);
void uiTemplateGpencilModifiers(uiLayout *layout, struct bContext *C);
void uiTemplateShaderFx(uiLayout *layout, struct bContext *C);
void uiTemplateConstraints(uiLayout *layout, struct bContext *C, bool use_bone_constraints);

uiLayout *uiTemplateGpencilModifier(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr);
void uiTemplateGpencilColorPreview(uiLayout *layout,
                                   struct bContext *C,
                                   struct PointerRNA *ptr,
                                   const char *propname,
                                   int rows,
                                   int cols,
                                   float scale,
                                   int filter);

void uiTemplateOperatorRedoProperties(uiLayout *layout, const struct bContext *C);

void uiTemplateConstraintHeader(uiLayout *layout, struct PointerRNA *ptr);
void uiTemplatePreview(uiLayout *layout,
                       struct bContext *C,
                       struct ID *id,
                       bool show_buttons,
                       struct ID *parent,
                       struct MTex *slot,
                       const char *preview_id);
void uiTemplateColorRamp(uiLayout *layout,
                         struct PointerRNA *ptr,
                         const char *propname,
                         bool expand);
void uiTemplateIcon(uiLayout *layout, int icon_value, float icon_scale);
void uiTemplateIconView(uiLayout *layout,
                        struct PointerRNA *ptr,
                        const char *propname,
                        bool show_labels,
                        float icon_scale,
                        float icon_scale_popup);
void uiTemplateHistogram(uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiTemplateWaveform(uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiTemplateVectorscope(uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiTemplateCurveMapping(uiLayout *layout,
                            struct PointerRNA *ptr,
                            const char *propname,
                            int type,
                            bool levels,
                            bool brush,
                            bool neg_slope,
                            bool tone);
void uiTemplateCurveProfile(uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiTemplateColorPicker(uiLayout *layout,
                           struct PointerRNA *ptr,
                           const char *propname,
                           bool value_slider,
                           bool lock,
                           bool lock_luminosity,
                           bool cubic);
void uiTemplatePalette(uiLayout *layout,
                       struct PointerRNA *ptr,
                       const char *propname,
                       bool colors);
void uiTemplateCryptoPicker(uiLayout *layout,
                            struct PointerRNA *ptr,
                            const char *propname,
                            int icon);
void uiTemplateLayers(uiLayout *layout,
                      struct PointerRNA *ptr,
                      const char *propname,
                      struct PointerRNA *used_ptr,
                      const char *used_propname,
                      int active_layer);
void uiTemplateImage(uiLayout *layout,
                     struct bContext *C,
                     struct PointerRNA *ptr,
                     const char *propname,
                     struct PointerRNA *userptr,
                     bool compact,
                     bool multiview);
void uiTemplateImageSettings(uiLayout *layout, struct PointerRNA *imfptr, bool color_management);
void uiTemplateImageStereo3d(uiLayout *layout, struct PointerRNA *stereo3d_format_ptr);
void uiTemplateImageViews(uiLayout *layout, struct PointerRNA *imaptr);
void uiTemplateImageFormatViews(uiLayout *layout,
                                struct PointerRNA *imfptr,
                                struct PointerRNA *ptr);
void uiTemplateImageLayers(uiLayout *layout,
                           struct bContext *C,
                           struct Image *ima,
                           struct ImageUser *iuser);
void uiTemplateImageInfo(uiLayout *layout,
                         struct bContext *C,
                         struct Image *ima,
                         struct ImageUser *iuser);
void uiTemplateRunningJobs(uiLayout *layout, struct bContext *C);
void UI_but_func_operator_search(uiBut *but);
void uiTemplateOperatorSearch(uiLayout *layout);

void UI_but_func_menu_search(uiBut *but);
void uiTemplateMenuSearch(uiLayout *layout);

void uiTemplateOperatorPropertyButs(const struct bContext *C,
                                    uiLayout *layout,
                                    struct wmOperator *op,
                                    eButLabelAlign label_align,
                                    short flag);
void uiTemplateHeader3D_mode(uiLayout *layout, struct bContext *C);
void uiTemplateEditModeSelection(uiLayout *layout, struct bContext *C);
void uiTemplateReportsBanner(uiLayout *layout, struct bContext *C);
void uiTemplateInputStatus(uiLayout *layout, struct bContext *C);
void uiTemplateKeymapItemProperties(uiLayout *layout, struct PointerRNA *ptr);

bool uiTemplateEventFromKeymapItem(struct uiLayout *layout,
                                   const char *text,
                                   const struct wmKeyMapItem *kmi,
                                   bool text_fallback);

void uiTemplateComponentMenu(uiLayout *layout,
                             struct PointerRNA *ptr,
                             const char *propname,
                             const char *name);
void uiTemplateNodeSocket(uiLayout *layout, struct bContext *C, float *color);
void uiTemplateCacheFile(uiLayout *layout,
                         const struct bContext *C,
                         struct PointerRNA *ptr,
                         const char *propname);

/* Default UIList class name, keep in sync with its declaration in bl_ui/__init__.py */
#define UI_UL_DEFAULT_CLASS_NAME "UI_UL_list"
void uiTemplateList(uiLayout *layout,
                    struct bContext *C,
                    const char *listtype_name,
                    const char *list_id,
                    struct PointerRNA *dataptr,
                    const char *propname,
                    struct PointerRNA *active_dataptr,
                    const char *active_propname,
                    const char *item_dyntip_propname,
                    int rows,
                    int maxrows,
                    int layout_type,
                    int columns,
                    bool sort_reverse,
                    bool sort_lock);
void uiTemplateNodeLink(uiLayout *layout,
                        struct bContext *C,
                        struct bNodeTree *ntree,
                        struct bNode *node,
                        struct bNodeSocket *input);
void uiTemplateNodeView(uiLayout *layout,
                        struct bContext *C,
                        struct bNodeTree *ntree,
                        struct bNode *node,
                        struct bNodeSocket *input);
void uiTemplateTextureUser(uiLayout *layout, struct bContext *C);
void uiTemplateTextureShow(uiLayout *layout,
                           const struct bContext *C,
                           struct PointerRNA *ptr,
                           struct PropertyRNA *prop);

void uiTemplateMovieClip(struct uiLayout *layout,
                         struct bContext *C,
                         struct PointerRNA *ptr,
                         const char *propname,
                         bool compact);
void uiTemplateTrack(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiTemplateMarker(struct uiLayout *layout,
                      struct PointerRNA *ptr,
                      const char *propname,
                      struct PointerRNA *userptr,
                      struct PointerRNA *trackptr,
                      bool compact);
void uiTemplateMovieclipInformation(struct uiLayout *layout,
                                    struct PointerRNA *ptr,
                                    const char *propname,
                                    struct PointerRNA *userptr);

void uiTemplateColorspaceSettings(struct uiLayout *layout,
                                  struct PointerRNA *ptr,
                                  const char *propname);
void uiTemplateColormanagedViewSettings(struct uiLayout *layout,
                                        struct bContext *C,
                                        struct PointerRNA *ptr,
                                        const char *propname);

int uiTemplateRecentFiles(struct uiLayout *layout, int rows);
void uiTemplateFileSelectPath(uiLayout *layout,
                              struct bContext *C,
                              struct FileSelectParams *params);

/* items */
void uiItemO(uiLayout *layout, const char *name, int icon, const char *opname);
void uiItemEnumO_ptr(uiLayout *layout,
                     struct wmOperatorType *ot,
                     const char *name,
                     int icon,
                     const char *propname,
                     int value);
void uiItemEnumO(uiLayout *layout,
                 const char *opname,
                 const char *name,
                 int icon,
                 const char *propname,
                 int value);
void uiItemEnumO_value(uiLayout *layout,
                       const char *name,
                       int icon,
                       const char *opname,
                       const char *propname,
                       int value);
void uiItemEnumO_string(uiLayout *layout,
                        const char *name,
                        int icon,
                        const char *opname,
                        const char *propname,
                        const char *value);
void uiItemsEnumO(uiLayout *layout, const char *opname, const char *propname);
void uiItemBooleanO(uiLayout *layout,
                    const char *name,
                    int icon,
                    const char *opname,
                    const char *propname,
                    int value);
void uiItemIntO(uiLayout *layout,
                const char *name,
                int icon,
                const char *opname,
                const char *propname,
                int value);
void uiItemFloatO(uiLayout *layout,
                  const char *name,
                  int icon,
                  const char *opname,
                  const char *propname,
                  float value);
void uiItemStringO(uiLayout *layout,
                   const char *name,
                   int icon,
                   const char *opname,
                   const char *propname,
                   const char *value);

void uiItemFullO_ptr(uiLayout *layout,
                     struct wmOperatorType *ot,
                     const char *name,
                     int icon,
                     struct IDProperty *properties,
                     int context,
                     int flag,
                     struct PointerRNA *r_opptr);
void uiItemFullO(uiLayout *layout,
                 const char *opname,
                 const char *name,
                 int icon,
                 struct IDProperty *properties,
                 int context,
                 int flag,
                 struct PointerRNA *r_opptr);
void uiItemFullOMenuHold_ptr(uiLayout *layout,
                             struct wmOperatorType *ot,
                             const char *name,
                             int icon,
                             struct IDProperty *properties,
                             int context,
                             int flag,
                             const char *menu_id, /* extra menu arg. */
                             struct PointerRNA *r_opptr);

void uiItemR(uiLayout *layout,
             struct PointerRNA *ptr,
             const char *propname,
             int flag,
             const char *name,
             int icon);
void uiItemFullR(uiLayout *layout,
                 struct PointerRNA *ptr,
                 struct PropertyRNA *prop,
                 int index,
                 int value,
                 int flag,
                 const char *name,
                 int icon);
void uiItemFullR_with_popover(uiLayout *layout,
                              struct PointerRNA *ptr,
                              struct PropertyRNA *prop,
                              int index,
                              int value,
                              int flag,
                              const char *name,
                              int icon,
                              const char *panel_type);
void uiItemFullR_with_menu(uiLayout *layout,
                           struct PointerRNA *ptr,
                           struct PropertyRNA *prop,
                           int index,
                           int value,
                           int flag,
                           const char *name,
                           int icon,
                           const char *menu_type);
void uiItemEnumR_prop(uiLayout *layout,
                      const char *name,
                      int icon,
                      struct PointerRNA *ptr,
                      struct PropertyRNA *prop,
                      int value);
void uiItemEnumR(uiLayout *layout,
                 const char *name,
                 int icon,
                 struct PointerRNA *ptr,
                 const char *propname,
                 int value);
void uiItemEnumR_string_prop(uiLayout *layout,
                             struct PointerRNA *ptr,
                             struct PropertyRNA *prop,
                             const char *value,
                             const char *name,
                             int icon);
void uiItemEnumR_string(uiLayout *layout,
                        struct PointerRNA *ptr,
                        const char *propname,
                        const char *value,
                        const char *name,
                        int icon);
void uiItemsEnumR(uiLayout *layout, struct PointerRNA *ptr, const char *propname);
void uiItemPointerR_prop(uiLayout *layout,
                         struct PointerRNA *ptr,
                         struct PropertyRNA *prop,
                         struct PointerRNA *searchptr,
                         struct PropertyRNA *searchprop,
                         const char *name,
                         int icon);
void uiItemPointerR(uiLayout *layout,
                    struct PointerRNA *ptr,
                    const char *propname,
                    struct PointerRNA *searchptr,
                    const char *searchpropname,
                    const char *name,
                    int icon);
void uiItemsFullEnumO(uiLayout *layout,
                      const char *opname,
                      const char *propname,
                      struct IDProperty *properties,
                      int context,
                      int flag);
void uiItemsFullEnumO_items(uiLayout *layout,
                            struct wmOperatorType *ot,
                            struct PointerRNA ptr,
                            struct PropertyRNA *prop,
                            struct IDProperty *properties,
                            int context,
                            int flag,
                            const struct EnumPropertyItem *item_array,
                            int totitem);

typedef struct uiPropertySplitWrapper {
  uiLayout *label_column;
  uiLayout *property_row;
  uiLayout *decorate_column;
} uiPropertySplitWrapper;

uiPropertySplitWrapper uiItemPropertySplitWrapperCreate(uiLayout *parent_layout);

void uiItemL(uiLayout *layout, const char *name, int icon); /* label */
void uiItemL_ex(
    uiLayout *layout, const char *name, int icon, const bool highlight, const bool redalert);
uiLayout *uiItemL_respect_property_split(uiLayout *layout, const char *text, int icon);
/* label icon for dragging */
void uiItemLDrag(uiLayout *layout, struct PointerRNA *ptr, const char *name, int icon);
/* menu */
void uiItemM_ptr(uiLayout *layout, struct MenuType *mt, const char *name, int icon);
void uiItemM(uiLayout *layout, const char *menuname, const char *name, int icon);
/* menu contents */
void uiItemMContents(uiLayout *layout, const char *menuname);
/* Decorators */
void uiItemDecoratorR_prop(uiLayout *layout,
                           struct PointerRNA *ptr,
                           struct PropertyRNA *prop,
                           int index);
void uiItemDecoratorR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, int index);
/* value */
void uiItemV(uiLayout *layout, const char *name, int icon, int argval);
/* separator */
void uiItemS(uiLayout *layout);
void uiItemS_ex(uiLayout *layout, float factor);
/* Special separator. */
void uiItemSpacer(uiLayout *layout);

void uiItemPopoverPanel_ptr(
    uiLayout *layout, struct bContext *C, struct PanelType *pt, const char *name, int icon);
void uiItemPopoverPanel(
    uiLayout *layout, struct bContext *C, const char *panel_type, const char *name, int icon);
void uiItemPopoverPanelFromGroup(uiLayout *layout,
                                 struct bContext *C,
                                 int space_id,
                                 int region_id,
                                 const char *context,
                                 const char *category);

void uiItemMenuF(uiLayout *layout, const char *name, int icon, uiMenuCreateFunc func, void *arg);
void uiItemMenuFN(uiLayout *layout, const char *name, int icon, uiMenuCreateFunc func, void *argN);
void uiItemMenuEnumO_ptr(uiLayout *layout,
                         struct bContext *C,
                         struct wmOperatorType *ot,
                         const char *propname,
                         const char *name,
                         int icon);
void uiItemMenuEnumO(uiLayout *layout,
                     struct bContext *C,
                     const char *opname,
                     const char *propname,
                     const char *name,
                     int icon);
void uiItemMenuEnumR_prop(uiLayout *layout,
                          struct PointerRNA *ptr,
                          struct PropertyRNA *prop,
                          const char *name,
                          int icon);
void uiItemMenuEnumR(
    uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *name, int icon);
void uiItemTabsEnumR_prop(uiLayout *layout,
                          struct bContext *C,
                          struct PointerRNA *ptr,
                          struct PropertyRNA *prop,
                          struct PointerRNA *ptr_highlight,
                          struct PropertyRNA *prop_highlight,
                          bool icon_only);

/* Only for testing, inspecting layouts. */
const char *UI_layout_introspect(uiLayout *layout);

/* Helper to add a big icon and create a split layout for alert boxes. */
uiLayout *uiItemsAlertBox(uiBlock *block, const int size, const eAlertIcon icon);

/* UI Operators */
typedef struct uiDragColorHandle {
  float color[3];
  bool gamma_corrected;
} uiDragColorHandle;

void ED_operatortypes_ui(void);
void ED_keymap_ui(struct wmKeyConfig *keyconf);

void UI_drop_color_copy(struct wmDrag *drag, struct wmDropBox *drop);
bool UI_drop_color_poll(struct bContext *C,
                        struct wmDrag *drag,
                        const struct wmEvent *event,
                        const char **r_tooltip);

bool UI_context_copy_to_selected_list(struct bContext *C,
                                      struct PointerRNA *ptr,
                                      struct PropertyRNA *prop,
                                      struct ListBase *r_lb,
                                      bool *r_use_path_from_id,
                                      char **r_path);

/* Helpers for Operators */
uiBut *UI_context_active_but_get(const struct bContext *C);
uiBut *UI_context_active_but_get_respect_menu(const struct bContext *C);
uiBut *UI_context_active_but_prop_get(const struct bContext *C,
                                      struct PointerRNA *r_ptr,
                                      struct PropertyRNA **r_prop,
                                      int *r_index);
void UI_context_active_but_prop_handle(struct bContext *C);
void UI_context_active_but_clear(struct bContext *C, struct wmWindow *win, struct ARegion *region);

struct wmOperator *UI_context_active_operator_get(const struct bContext *C);
void UI_context_update_anim_flag(const struct bContext *C);
void UI_context_active_but_prop_get_filebrowser(const struct bContext *C,
                                                struct PointerRNA *r_ptr,
                                                struct PropertyRNA **r_prop,
                                                bool *r_is_undo,
                                                bool *r_is_userdef);
void UI_context_active_but_prop_get_templateID(struct bContext *C,
                                               struct PointerRNA *r_ptr,
                                               struct PropertyRNA **r_prop);
struct ID *UI_context_active_but_get_tab_ID(struct bContext *C);

uiBut *UI_region_active_but_get(const struct ARegion *region);
uiBut *UI_region_but_find_rect_over(const struct ARegion *region, const struct rcti *rect_px);
uiBlock *UI_region_block_find_mouse_over(const struct ARegion *region,
                                         const int xy[2],
                                         bool only_clip);
struct ARegion *UI_region_searchbox_region_get(const struct ARegion *button_region);

/* uiFontStyle.align */
typedef enum eFontStyle_Align {
  UI_STYLE_TEXT_LEFT = 0,
  UI_STYLE_TEXT_CENTER = 1,
  UI_STYLE_TEXT_RIGHT = 2,
} eFontStyle_Align;

struct uiFontStyleDraw_Params {
  eFontStyle_Align align;
  uint word_wrap : 1;
};

/* Styled text draw */
void UI_fontstyle_set(const struct uiFontStyle *fs);
void UI_fontstyle_draw_ex(const struct uiFontStyle *fs,
                          const struct rcti *rect,
                          const char *str,
                          const uchar col[4],
                          const struct uiFontStyleDraw_Params *fs_params,
                          size_t len,
                          int *r_xofs,
                          int *r_yofs,
                          struct ResultBLF *r_info);
void UI_fontstyle_draw(const struct uiFontStyle *fs,
                       const struct rcti *rect,
                       const char *str,
                       const uchar col[4],
                       const struct uiFontStyleDraw_Params *fs_params);
void UI_fontstyle_draw_rotated(const struct uiFontStyle *fs,
                               const struct rcti *rect,
                               const char *str,
                               const uchar col[4]);
void UI_fontstyle_draw_simple(
    const struct uiFontStyle *fs, float x, float y, const char *str, const uchar col[4]);
void UI_fontstyle_draw_simple_backdrop(const struct uiFontStyle *fs,
                                       float x,
                                       float y,
                                       const char *str,
                                       const float col_fg[4],
                                       const float col_bg[4]);

int UI_fontstyle_string_width(const struct uiFontStyle *fs, const char *str);
int UI_fontstyle_height_max(const struct uiFontStyle *fs);

void UI_draw_icon_tri(float x, float y, char dir, const float[4]);

const struct uiStyle *UI_style_get(void);     /* use for fonts etc */
const struct uiStyle *UI_style_get_dpi(void); /* DPI scaled settings for drawing */

/* linker workaround ack! */
void UI_template_fix_linking(void);

/* UI_OT_editsource helpers */
bool UI_editsource_enable_check(void);
void UI_editsource_active_but_test(uiBut *but);
void UI_editsource_but_replace(const uiBut *old_but, uiBut *new_but);

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

/* ui_interface_region_tooltip.c */
struct ARegion *UI_tooltip_create_from_button(struct bContext *C,
                                              struct ARegion *butregion,
                                              uiBut *but,
                                              bool is_label);
struct ARegion *UI_tooltip_create_from_gizmo(struct bContext *C, struct wmGizmo *gz);
void UI_tooltip_free(struct bContext *C, struct bScreen *screen, struct ARegion *region);

typedef struct {
  /** A description for the item, e.g. what happens when selecting it. */
  char description[UI_MAX_DRAW_STR];
  /* The full name of the item, without prefixes or suffixes (e.g. hint with UI_SEP_CHARP). */
  const char *name;
  /** Additional info about the item (e.g. library name of a linked data-block). */
  char hint[UI_MAX_DRAW_STR];
} uiSearchItemTooltipData;

struct ARegion *UI_tooltip_create_from_search_item_generic(
    struct bContext *C,
    const struct ARegion *searchbox_region,
    const struct rcti *item_rect,
    const uiSearchItemTooltipData *item_tooltip_data);

/* How long before a tool-tip shows. */
#define UI_TOOLTIP_DELAY 0.5
#define UI_TOOLTIP_DELAY_LABEL 0.2

/* Float precision helpers */
#define UI_PRECISION_FLOAT_MAX 6
/* For float buttons the 'step' (or a1), is scaled */
#define UI_PRECISION_FLOAT_SCALE 0.01f

/* Typical UI text */
#define UI_FSTYLE_WIDGET (const uiFontStyle *)&(UI_style_get()->widget)

int UI_calc_float_precision(int prec, double value);

/* widget batched drawing */
void UI_widgetbase_draw_cache_begin(void);
void UI_widgetbase_draw_cache_flush(void);
void UI_widgetbase_draw_cache_end(void);

/* Use for resetting the theme. */
void UI_theme_init_default(void);
void UI_style_init_default(void);

void UI_interface_tag_script_reload(void);

/* Special drawing for toolbar, mainly workarounds for inflexible icon sizing. */
#define USE_UI_TOOLBAR_HACK

/* Support click-drag motion which presses the button and closes a popover (like a menu). */
#define USE_UI_POPOVER_ONCE

#ifdef __cplusplus
}
#endif
